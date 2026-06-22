
#include "Ringlogger.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <system_error>

#include <windows.h>

namespace Tunnel {

// ================================================================
//  Internal helpers
// ================================================================

namespace {

int64_t unixNanosNow() noexcept {
    FILETIME ft;
    ::GetSystemTimePreciseAsFileTime(&ft);
    // FILETIME: 100-ns ticks since 1601-01-01
    constexpr uint64_t kEpochDiff = 116444736000000000ULL; // 100-ns ticks to Unix epoch
    uint64_t ticks = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    if (ticks < kEpochDiff) return 0;
    // Convert 100-ns ticks to nanoseconds
    return static_cast<int64_t>((ticks - kEpochDiff) * 100);
}

/// Throw a std::system_error for the current Win32 GetLastError().
[[noreturn]] void throwLastError(const char* ctx) {
    throw std::system_error(
        std::error_code(static_cast<int>(::GetLastError()),
                        std::system_category()),
        ctx);
}

} // anonymous namespace

std::string Ringlogger::formatTimestamp(int64_t ns) {
    const time_t secs = static_cast<time_t>(ns / 1'000'000'000LL);
    const long   sub_ns = static_cast<long>(ns % 1'000'000'000LL);
    // sub-second digits:  shows 6 digits (microseconds) — same as "(ns%1e9).ToString()+"00000")[0..5]"
    const long   micros = sub_ns / 1'000;

    struct tm lt{};
    ::localtime_s(&lt, &secs);

    char buf[64]{};
    std::snprintf(buf, sizeof(buf),
                  "%04d-%02d-%02d %02d:%02d:%02d.%06ld",
                  lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                  lt.tm_hour, lt.tm_min, lt.tm_sec,
                  micros);
    return buf;
}

std::string Ringlogger::lineToString(const RawLine& l) {
    if (l.timestampNs == 0)
        return {};
    // text is null-terminated; strnlen for safety
    const std::size_t len = ::strnlen(l.text, kMaxLineBytes);
    if (len == 0)
        return {};
    return formatTimestamp(l.timestampNs) + ": " + std::string(l.text, len);
}

void Ringlogger::initMapping(const wchar_t* path, bool readOnly) {
    m_file = ::CreateFileW(
        path,
        readOnly ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE),
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        readOnly ? OPEN_EXISTING : OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (m_file == INVALID_HANDLE_VALUE)
        throwLastError("Ringlogger: CreateFileW");

    if (readOnly) {
        LARGE_INTEGER size{};
        if (!::GetFileSizeEx(m_file, &size))
            throwLastError("Ringlogger: GetFileSizeEx");
        if (size.QuadPart != static_cast<LONGLONG>(kTotalBytes))
            throw std::runtime_error("Ringlogger: invalid file size");
    }
    else {
        LARGE_INTEGER size{};
        size.QuadPart = static_cast<LONGLONG>(kTotalBytes);
        if (!::SetFilePointerEx(m_file, size, nullptr, FILE_BEGIN) ||
            !::SetEndOfFile(m_file)) {
            throwLastError("Ringlogger: SetFileLength");
        }
    }

    m_mapping = ::CreateFileMappingW(
        m_file,
        nullptr,
        readOnly ? PAGE_READONLY : PAGE_READWRITE,
        0,
        static_cast<DWORD>(kTotalBytes),
        nullptr);

    if (!m_mapping)
        throwLastError("Ringlogger: CreateFileMappingW");

    m_view = static_cast<uint8_t*>(
        ::MapViewOfFile(m_mapping, readOnly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS,
                        0, 0, kTotalBytes));

    if (!m_view)
        throwLastError("Ringlogger: MapViewOfFile");

    if (header()->magic != kMagic) {
        if (readOnly)
            throw std::runtime_error("Ringlogger: invalid file magic");
        ::memset(m_view, 0, kTotalBytes);
        header()->magic = kMagic;
    }
}

// ================================================================
//  Ringlogger::closeMapping
// ================================================================

void Ringlogger::closeMapping() noexcept {
    if (m_view) {
        ::UnmapViewOfFile(m_view);
        m_view = nullptr;
    }
    if (m_mapping) {
        ::CloseHandle(m_mapping);
        m_mapping = nullptr;
    }
    if (m_file != INVALID_HANDLE_VALUE) {
        ::CloseHandle(m_file);
        m_file = INVALID_HANDLE_VALUE;
    }
}

// ================================================================
//  Constructor / destructor
// ================================================================

Ringlogger::Ringlogger(std::wstring_view filename, std::string_view tag, bool readOnly)
    : m_tag(tag),
      m_readOnly(readOnly)
{
    const std::wstring path(filename);
    try {
        initMapping(path.c_str(), readOnly);
    }
    catch (...) {
        closeMapping();
        throw;
    }
}

Ringlogger::~Ringlogger() {
    closeMapping();
}

void Ringlogger::write(std::string_view line) const {
    if (m_readOnly)
        throw std::logic_error("Ringlogger: cannot write through a read-only mapping");

    const int64_t now = unixNanosNow();

    // Build "[tag] <trimmed line>"
    std::string_view trimmed = line;
    while (!trimmed.empty() &&
           (trimmed.back() == ' ' || trimmed.back() == '\t' ||
            trimmed.back() == '\r' || trimmed.back() == '\n'))
        trimmed.remove_suffix(1);

    // "[tag] text" — truncate to kMaxLineBytes-1 to leave room for '\0'
    std::string text;
    text.reserve(m_tag.size() + 3 + trimmed.size());
    text = '[';
    text += m_tag;
    text += "] ";
    text += trimmed;

    const uint32_t slot = insertNextIndex();
    RawLine* entry = lineAt(slot);

    // Signal "being written": clear timestamp first
    // Use a compiler barrier via std::atomic_thread_fence to prevent
    // the stores from being reordered above the fence.
    entry->timestampNs = 0;
    std::atomic_thread_fence(std::memory_order_release);

    // Write text — copy at most kMaxLineBytes-1 bytes, then null-terminate
    const std::size_t copyLen = std::min(text.size(),
                                         static_cast<std::size_t>(kMaxLineBytes - 1));
    std::memcpy(entry->text, text.data(), copyLen);
    entry->text[copyLen] = '\0';

    // Zero any leftover bytes from a previously longer entry
    if (copyLen < kMaxLineBytes - 1)
        std::memset(entry->text + copyLen + 1, 0, kMaxLineBytes - 1 - copyLen);

    // Commit: write timestamp last so readers see a consistent entry
    std::atomic_thread_fence(std::memory_order_release);
    entry->timestampNs = now;
}

void Ringlogger::writeTo(std::function<void(std::string_view)> sink) const {
    const uint32_t start = header()->nextIndex;
    for (uint32_t i = 0; i < kMaxLines; ++i) {
        const RawLine& entry = *lineAt(i + start);
        if (entry.timestampNs == 0)
            continue;
        std::string s = lineToString(entry);
        if (!s.empty())
            sink(s);
    }
}

std::vector<std::string> Ringlogger::followFromCursor(uint32_t& cursor) const {
    std::vector<std::string> lines;
    lines.reserve(kMaxLines);

    const bool all = (cursor == CursorAll);
    uint32_t   i   = all ? header()->nextIndex : cursor;

    for (uint32_t l = 0; l < kMaxLines; ++l, ++i) {
        // Stop when we've wrapped back to the write head (non-CursorAll mode)
        if (!all && (i % kMaxLines) == (header()->nextIndex % kMaxLines))
            break;

        const RawLine& entry = *lineAt(i);

        if (entry.timestampNs == 0) {
            if (all)
                continue;   // sparse ring — keep going
            break;          // hit an empty slot, done
        }

        // Update cursor to point past this entry
        cursor = (i + 1) % kMaxLines;

        std::string s = lineToString(entry);
        if (!s.empty())
            lines.push_back(std::move(s));
    }

    return lines;
}

} // namespace Tunnel
