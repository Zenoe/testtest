#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <windows.h>

namespace Tunnel {

class Ringlogger {
public:
    // Sentinel: pass to followFromCursor() to read everything currently in the ring.
    static constexpr uint32_t CursorAll = UINT32_MAX;

    // ----------------------------------------------------------------
    //  Construction / destruction
    //  filename – path to the backing file (created if absent)
    //  tag      – prepended to every line as "[tag]"
    // ----------------------------------------------------------------
    Ringlogger(std::wstring_view filename, std::string_view tag, bool readOnly = false);
    ~Ringlogger();

    Ringlogger(const Ringlogger&)            = delete;
    Ringlogger& operator=(const Ringlogger&) = delete;
    Ringlogger(Ringlogger&&)                 = delete;
    Ringlogger& operator=(Ringlogger&&)      = delete;

    // ----------------------------------------------------------------
    //  Write a single line.  Thread-safe; lock-free.
    // ----------------------------------------------------------------
    void write(std::string_view line) const;

    // ----------------------------------------------------------------
    //  Dump all non-empty entries, in ring order, to a callback.
    // ----------------------------------------------------------------
    void writeTo(std::function<void(std::string_view)> sink) const;

    // ----------------------------------------------------------------
    //  Incremental read from a cursor.
    //  Pass CursorAll on the first call to read everything present.
    //  cursor is updated on return; pass it back next call for new lines.
    // ----------------------------------------------------------------
    [[nodiscard]] std::vector<std::string>
    followFromCursor(uint32_t& cursor) const;

private:
    // ================================================================
    //  On-disk layout
    //
    //   Offset 0        : uint32_t magic        (0xbadbabe)
    //   Offset 4        : uint32_t nextIndex     (atomic increment counter)
    //   Offset 8        : Line[2048]
    //
    //  Each Line:
    //   Offset 0        : int64_t  timestampNs   (Unix nanoseconds; 0 = empty)
    //   Offset 8        : char[512] text          (UTF-8, null-terminated)
    // ================================================================

    static constexpr uint32_t kMagic        = 0xbadbabe;
    static constexpr uint32_t kMaxLines     = 2048;
    static constexpr int      kMaxLineBytes = 512;

    // Line on-disk structure
    struct alignas(1) RawLine {
        int64_t timestampNs;                // 0 = empty slot
        char    text[kMaxLineBytes];        // null-terminated UTF-8
    };
    static_assert(sizeof(RawLine) == 8 + 512, "RawLine layout mismatch");

    // File header
    struct alignas(1) Header {
        uint32_t magic;
        uint32_t nextIndex;     // accessed via InterlockedIncrement
    };
    static_assert(sizeof(Header) == 8, "Header layout mismatch");

    static constexpr size_t kHeaderBytes = sizeof(Header);                          // 8
    static constexpr size_t kLineBytes   = sizeof(RawLine);                         // 520
    static constexpr size_t kTotalBytes  = kHeaderBytes + kLineBytes * kMaxLines;   // 8 + 520*2048

    // ----------------------------------------------------------------
    //  Helpers that operate on the mapped view
    // ----------------------------------------------------------------

    [[nodiscard]] Header*  header()  const noexcept {
        return reinterpret_cast<Header*>(m_view);
    }
    [[nodiscard]] RawLine* lineAt(uint32_t index) const noexcept {
        return reinterpret_cast<RawLine*>(
            m_view + kHeaderBytes + (index % kMaxLines) * kLineBytes);
    }

    /// Atomically increment nextIndex and return the OLD value (pre-increment),
    [[nodiscard]] uint32_t insertNextIndex() const noexcept {
        // nextIndex sits at byte-offset 4 in the mapping.
        // We need an atomic fetch-add on the raw mapped memory.
        auto* p = reinterpret_cast<volatile LONG*>(&header()->nextIndex);
        return static_cast<uint32_t>(::InterlockedIncrement(p)) - 1u;
    }

    [[nodiscard]] static std::string formatTimestamp(int64_t ns);
    [[nodiscard]] static std::string lineToString(const RawLine& l);

    void initMapping(const wchar_t* path, bool readOnly);
    void closeMapping() noexcept;

    HANDLE  m_file    = INVALID_HANDLE_VALUE;
    HANDLE  m_mapping = nullptr;
    uint8_t* m_view   = nullptr;

    std::string m_tag;
    bool m_readOnly = false;
};

} // namespace Tunnel
