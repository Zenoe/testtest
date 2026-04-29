
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

#include "slogger.h"
void setup_logging(std::string logfileName) {
    // ── tunables ────────────────────────────────────────────────────────────
    constexpr int    MAX_SIZE_MB = 10;
    constexpr int    MAX_FILES = 5;
    const size_t     MAX_SIZE = static_cast<size_t>(MAX_SIZE_MB) * 1024 * 1024;

    if (logfileName.empty()) {

    }
#ifdef _DEBUG
    const std::string DEFAULT_LOG_FILE = "ReService_debug.log";   // BUG FIX: was duplicating prefix
#else
    const std::string DEFAULT_LOG_FILE = "ReService.log";
#endif

    const std::string LOG_FILE = logfileName.empty() ? DEFAULT_LOG_FILE : logfileName;

    // ── sinks ───────────────────────────────────────────────────────────────
    std::vector<spdlog::sink_ptr> sinks;

    try {
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            LOG_FILE, MAX_SIZE, MAX_FILES));
    }
    catch (const spdlog::spdlog_ex& ex) {
        // Can't log yet – fall back to debug output so the problem is visible
        OutputDebugStringA(("setup_logging: failed to open log file: " +
            std::string(ex.what()) + "\n").c_str());
    }

#ifdef _DEBUG
    sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif

    // ── logger ──────────────────────────────────────────────────────────────
    auto logger = std::make_shared<spdlog::logger>("app", sinks.begin(), sinks.end());

#ifdef _DEBUG
    logger->set_level(spdlog::level::trace);
#else
    logger->set_level(spdlog::level::info);
#endif

    logger->flush_on(spdlog::level::err);
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e][%^%l%$][tid:%t] %v");

    spdlog::info("Logging initialised | file={} max_size={}MB max_files={}",
        LOG_FILE, MAX_SIZE_MB, MAX_FILES);
}



std::string to_utf8(std::wstring_view ws) {
    if (ws.empty()) return {};

    int sz = WideCharToMultiByte(
        CP_UTF8,
        0,
        ws.data(),
        static_cast<int>(ws.size()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (sz <= 0)
        return {};

    std::string out(sz, '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        ws.data(),
        static_cast<int>(ws.size()),
        out.data(),
        sz,
        nullptr,
        nullptr);

    return out;
}