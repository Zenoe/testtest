
#include "logger.h"
#include "ConfigManager.h"
#include <Windows.h>

void setup_logging()
{
    // Get full path + rotation settings from config
    std::string logFile     = ConfigManager::instance().get("log.file", std::string("revelationapp.log"));

    int max_size_mb = ConfigManager::instance().get("log.max_size_mb", 10);
    int max_files   = ConfigManager::instance().get("log.max_files",   5);
    size_t max_size = static_cast<size_t>(max_size_mb) * 1024 * 1024;

#ifdef _DEBUG
	logFile = logFile + "revelationapp_debug.log"; // Separate log file for debug builds
#endif
    std::vector<spdlog::sink_ptr> sinks;

    // Always log to file (standard for troubleshooting)
    //sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFile, max_size, max_files));
    try {
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logFile, max_size, max_files));
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
    logger->flush_on(spdlog::level::trace);
#else
    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::err);
#endif

    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e][%^%l%$][tid:%t] %v");

    spdlog::info("Logging initialised | file={} max_size={}MB max_files={}",
        logFile, max_size_mb, max_files);

}

std::string win32_error_str(DWORD code = GetLastError()) {
    wchar_t* buf = nullptr;

    DWORD len = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        code,
        0,
        reinterpret_cast<LPWSTR>(&buf),
        0,
        nullptr);

    if (len == 0 || !buf)
        return "Unknown error";

    int utf8_len = WideCharToMultiByte(
        CP_UTF8,
        0,
        buf,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr);

    if (utf8_len <= 1) {
        LocalFree(buf);
        return "Unknown error";
    }

    std::string msg(utf8_len - 1, '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        buf,
        -1,
        msg.data(),
        utf8_len,
        nullptr,
        nullptr);

    LocalFree(buf);

    while (!msg.empty() &&
        (msg.back() == '\n' || msg.back() == '\r')) {
        msg.pop_back();
    }

    return msg.empty() ? "Unknown error" : msg;
}

void LOG_WIN32_ERROR(const char* msg) {
    DWORD e = GetLastError();
    spdlog::error("{} | code={} ({})", msg, e, win32_error_str(e));
}

