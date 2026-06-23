#include "RingLogStore.h"

#include "RingLogger.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace Tunnel {
namespace {

std::filesystem::path ringLogPath(const std::filesystem::path& configPath) {
    if (configPath.empty() || !configPath.is_absolute() || !configPath.has_parent_path())
        throw std::invalid_argument("config path must be absolute");
    return configPath.parent_path() / L"log.bin";
}

} // namespace

void writeRingLog(const std::filesystem::path& configPath, std::string_view source,
                  std::string_view message) noexcept {
    try {
        std::string line;
        line.reserve(source.size() + message.size() + 3);
        line.append(source);
        line.append(" | ");
        line.append(message);
        Ringlogger(ringLogPath(configPath).wstring(), "xyre").write(line);
    }
    catch (...) {
        // Ring logging must never affect service control or tunnel operation.
    }
}

std::vector<std::string> readRingLog(const std::filesystem::path& configPath,
                                     std::size_t maxLines, std::size_t maxBytes) {
    const std::filesystem::path path = ringLogPath(configPath);
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        if (error)
            throw std::system_error(error, "failed to inspect log.bin");
        throw std::runtime_error("log.bin does not exist beside the tunnel config");
    }

    Ringlogger logger(path.wstring(), "xyre", true);
    std::vector<std::string> all;
    logger.writeTo([&all](std::string_view line) {
        all.emplace_back(line);
    });

    std::vector<std::string> result;
    std::size_t usedBytes = 0;
    const std::size_t first = all.size() > maxLines ? all.size() - maxLines : 0;
    for (std::size_t i = all.size(); i > first; --i) {
        std::string& line = all[i - 1];
        const std::size_t lineBytes = line.size() + 1;
        if (usedBytes + lineBytes > maxBytes)
            continue;
        usedBytes += lineBytes;
        result.push_back(std::move(line));
    }
    std::reverse(result.begin(), result.end());
    return result;
}

} // namespace Tunnel
