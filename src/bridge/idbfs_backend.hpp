#pragma once

#include "persistence/idbfs.hpp"

#include <memory>

// Production storage seam for Zone 04's persistence layer, backed by Emscripten's
// IDBFS. The serialized AppState blob lives in a single file under an IDBFS mount;
// reads come straight from the MEMFS-backed mount (which boot populates with one
// FS.syncfs(true) before the first frame), and writes flush to IndexedDB via an
// async FS.syncfs(false). Zone 04 never branches on the backend — this is swapped
// in at the StorageBackend seam exactly like the in-memory test backend.
//
// Emscripten-only (uses the Emscripten FS + EM_ASM): compiled into bridge_platform
// only, never the native test build. The native tests exercise the same Zone 04
// load/save logic through the in-memory MemoryStorage backend instead.

namespace poker_trainer::bridge {

// The IDBFS mount directory and the state file within it. boot creates and mounts
// the directory and runs the initial FS.syncfs(true) before constructing the
// backend (see begin_persistence_load in boot.cpp).
inline constexpr const char* kIdbfsMountDir = "/idbfs_poker_trainer";
inline constexpr const char* kIdbfsStatePath = "/idbfs_poker_trainer/state.bin";

// Construct the production StorageBackend over the mounted IDBFS state file.
[[nodiscard]] std::unique_ptr<persistence::StorageBackend> make_idbfs_storage_backend();

}  // namespace poker_trainer::bridge
