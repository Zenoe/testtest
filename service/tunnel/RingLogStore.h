#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Tunnel {

void writeRingLog(const std::filesystem::path& configPath, std::string_view source,
                  std::string_view message) noexcept;

std::vector<std::string> readRingLog(const std::filesystem::path& configPath,
                                     std::size_t maxLines, std::size_t maxBytes);

} // namespace Tunnel
