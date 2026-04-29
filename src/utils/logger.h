#ifndef LOGGER_H_
#define LOGGER_H_

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/msvc_sink.h> // For Visual Studio Output window
#include <string>
#include <string_view>

void setup_logging();
std::string win32_error_str(unsigned long code);
//std::string to_utf8(std::wstring_view ws);

// 用函数替代宏（更安全）
void LOG_WIN32_ERROR(const char* msg);

/* void setup_logging() */
/* { */
/*     // Log to file */
/*     auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("xybox.log", true); */
/*     // Log to Visual Studio Output window */
/*     auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>(); */

/*     std::vector<spdlog::sink_ptr> sinks{file_sink, msvc_sink}; */
/*     auto logger = std::make_shared<spdlog::logger>("clipboard", sinks.begin(), sinks.end()); */
/*     spdlog::set_default_logger(logger); */

/*     spdlog::set_pattern("[%Y-%m-%d %H:%M:%S][%l][thread %t] %v"); */
/*     spdlog::set_level(spdlog::level::info); // change to debug/trace for more details */
/* } */

/* template<typename... Args> */
/* void log_info_w(const std::wstring& fmt, Args&&... args) { */
/*     spdlog::info(string_util::wstring_to_utf8(fmt).c_str(), args...); */
/*     //spdlog::flush(); */
/* } */

// Usage
// Call setup_logging() once at startup, before logging anything.

/* void log_example(int item_id) */
/* { */
/*     spdlog::info("Added clipboard item: {}", item_id); */
/*     spdlog::error("DB error: something went wrong"); */
/* } */




#endif // LOGGER_H_
