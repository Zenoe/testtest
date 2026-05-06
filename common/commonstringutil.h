#ifndef STRUTIL_H_
#define STRUTIL_H_
#include <string>

namespace common{
  inline std::wstring getBasename(const std::wstring& path) {
    size_t lastSlash = path.find_last_of(L"/\\");
    size_t lastDot = path.find_last_of(L'.');

    std::wstring basename = (lastSlash == std::wstring::npos) ? path : path.substr(lastSlash + 1);

    // Remove extension if present
    if (lastDot != std::wstring::npos && lastDot > lastSlash) {
      basename = basename.substr(0, basename.find_last_of(L'.'));
    }

    return basename;
  }
}

#endif // STRUTIL_H_
