
#include "logger.h"
#include "ConfigManager.h"
#include "Win32Log.h"

// ── Unicode-aware MSVC sink ──────────────────────────────────────────────────
// spdlog's built-in msvc_sink uses OutputDebugStringA — garbles non-ASCII.
// This sink converts UTF-8 → UTF-16 and calls OutputDebugStringW instead.
#ifdef _DEBUG
template<typename Mutex>
class msvc_sink_utf8 : public spdlog::sinks::base_sink<Mutex>
{
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t buf;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, buf);
        // buf is UTF-8; convert to UTF-16 for OutputDebugStringW
        std::string utf8(buf.data(), buf.size());
        int wlen = MultiByteToWideChar(CP_UTF8, 0,
                                       utf8.c_str(), static_cast<int>(utf8.size()),
                                       nullptr, 0);
        if (wlen <= 0) return;
        std::wstring utf16(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0,
                            utf8.c_str(), static_cast<int>(utf8.size()),
                            utf16.data(), wlen);
        OutputDebugStringW(utf16.c_str());
    }
    void flush_() override {}
};

using msvc_sink_utf8_mt = msvc_sink_utf8<std::mutex>;
#endif

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
    // sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
    sinks.push_back(std::make_shared<msvc_sink_utf8_mt>());
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

void LOG_WIN32_ERROR(const char* msg) {
    DWORD e = GetLastError();
    spdlog::error("{} | code={} ({})", msg, e, win32_error_str(e));
}

