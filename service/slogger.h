#pragma once

#include <spdlog/spdlog.h>

#include <string>
#include <string_view>
#include "../common/Win32Log.h"

void setup_logging(std::string logfileName="");

std::string to_utf8(std::wstring_view ws);

inline void LOG_WIN32_ERROR(const char* msg) {
    DWORD e = GetLastError();
    spdlog::error("{} | code={} ({})", msg, e, win32_error_str(e));
}
