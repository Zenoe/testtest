#include <Windows.h>

inline std::string win32_error_str(DWORD code = GetLastError()) {
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