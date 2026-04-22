#pragma once
#include <Windows.h>
#include <string>
#include <cwctype>
#include <vector>

namespace string_util {
    // Helper for tolower, handles wchar_t and char
    inline wchar_t to_lower(wchar_t ch) { return std::towlower(ch); }
    inline char    to_lower(char ch) { return std::tolower(static_cast<unsigned char>(ch)); }

    // Convert UTF-16 (wstring) to UTF-8 (string)
    inline std::string wstring_to_utf8(const std::wstring& wstr) {
        if (wstr.empty()) return {};
        int sz = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string str(sz, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0], sz, nullptr, nullptr);
        return str;
    }

    // Convert UTF-8 (string) to UTF-16 (wstring)
    inline std::wstring utf8_to_wstring(const std::string& str) {
        if (str.empty()) return {};
        int sz = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
        std::wstring wstr(sz, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], sz);
        return wstr;
    }
} // namespace string_util
