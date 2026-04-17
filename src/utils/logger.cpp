
#include "logger.h"
#include "ConfigManager.h"

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

    // 1. Always log to file (standard for troubleshooting)
    sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFile, max_size, max_files));

    // 2. Conditional Sinks & Levels
#ifdef _DEBUG
    // Only show MSVC output in Debug builds
    sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());

    auto logger = std::make_shared<spdlog::logger>("clipboard", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace); // Verbose logging for devs
#else
    auto logger = std::make_shared<spdlog::logger>("clipboard", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::info);  // Clean logging for users
#endif

    // 3. Optimization: Flush on Errors
    // This ensures that if the app crashes, the error that caused it is actually written to disk.
    logger->flush_on(spdlog::level::err);

    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S][%l][thread %t] %v");
}
