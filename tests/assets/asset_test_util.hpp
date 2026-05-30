#pragma once

// Shared helpers for the Zone 02 tests that touch the on-disk placeholder PNGs.
// POKER_TRAINER_SOURCE_DIR is injected by CMake so the tests can locate the
// committed assets/ tree regardless of the build directory's location.

#include "assets/asset_paths.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace poker_trainer::assets::test {

[[nodiscard]] inline std::filesystem::path source_dir() {
#ifdef POKER_TRAINER_SOURCE_DIR
    return std::filesystem::path{POKER_TRAINER_SOURCE_DIR};
#else
    return std::filesystem::current_path();
#endif
}

[[nodiscard]] inline std::filesystem::path asset_file(AssetId id) {
    return source_dir() / std::filesystem::path{std::string{asset_path(id)}};
}

[[nodiscard]] inline std::vector<std::byte> read_file_bytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    const std::vector<char> raw((std::istreambuf_iterator<char>(stream)),
                                std::istreambuf_iterator<char>());
    std::vector<std::byte> bytes(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        bytes[i] = static_cast<std::byte>(raw[i]);
    }
    return bytes;
}

[[nodiscard]] inline std::vector<std::byte> read_asset_bytes(AssetId id) {
    return read_file_bytes(asset_file(id));
}

}  // namespace poker_trainer::assets::test
