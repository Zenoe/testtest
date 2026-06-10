#pragma once

#include <cstdint>

namespace XyreControl {

inline constexpr wchar_t kControllerServiceName[] = L"XyreController";
inline constexpr wchar_t kControllerDisplayName[] = L"Xyre VPN Controller";
inline constexpr wchar_t kPipeName[] = L"\\\\.\\pipe\\XyreController.v1";
inline constexpr std::uint32_t kMaxMessageBytes = 64 * 1024;
inline constexpr int kProtocolVersion = 3;

} // namespace XyreControl
