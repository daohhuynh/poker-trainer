#include "bridge/idbfs_backend.hpp"

#include "persistence/idbfs.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <emscripten/emscripten.h>

// IDBFS-backed StorageBackend. Binding-adjacent (Emscripten FS + EM_ASM): held to
// the reduced -Wall -Wextra -Werror binding baseline in bridge_platform, compiled
// only into the wasm app. Still C++ casts only (no C-style casts), per CLAUDE.md.

namespace poker_trainer::bridge {

namespace {

// Flush the MEMFS-backed IDBFS mount to IndexedDB. Asynchronous and
// fire-and-forget: the in-memory write already succeeded, and the durable flush
// completes shortly after on the browser event loop.
void idbfs_flush() {
    EM_ASM({
        FS.syncfs(false, function(err) {
            if (err) { console.warn('IDBFS flush failed', err); }
        });
    });
}

class IdbfsStorageBackend final : public persistence::StorageBackend {
public:
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read() const override {
        std::FILE* file = std::fopen(kIdbfsStatePath, "rb");
        if (file == nullptr) {
            return std::nullopt;  // first launch: nothing persisted yet
        }
        std::fseek(file, 0, SEEK_END);
        const long size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);
        if (size <= 0) {
            std::fclose(file);
            return std::nullopt;
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        const std::size_t got = std::fread(bytes.data(), 1, bytes.size(), file);
        std::fclose(file);
        if (got != bytes.size()) {
            return std::nullopt;  // short read: treat as no usable blob
        }
        return bytes;
    }

    void write(std::span<const std::uint8_t> bytes) override {
        std::FILE* file = std::fopen(kIdbfsStatePath, "wb");
        if (file == nullptr) {
            return;
        }
        if (!bytes.empty()) {
            std::fwrite(bytes.data(), 1, bytes.size(), file);
        }
        std::fclose(file);
        idbfs_flush();  // schedule the durable IndexedDB flush
    }

    void clear() override {
        std::remove(kIdbfsStatePath);
        idbfs_flush();
    }
};

}  // namespace

std::unique_ptr<persistence::StorageBackend> make_idbfs_storage_backend() {
    return std::make_unique<IdbfsStorageBackend>();
}

}  // namespace poker_trainer::bridge
