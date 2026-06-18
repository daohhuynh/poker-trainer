#ifdef __EMSCRIPTEN__

#include "bridge/texture_bind.hpp"

#include "bridge/bridge_runtime.hpp"

#include "assets/asset_paths.hpp"
#include "assets/registry.hpp"
#include "assets/texture.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <GLES3/gl3.h>

// WebGL2 art-texture upload. Binding-adjacent code held to the reduced binding
// baseline in bridge_platform (the GL type plumbing would otherwise bury the
// logic in casts). C++ casts only, per CLAUDE.md §10.

namespace poker_trainer::bridge {

namespace {

// GL texture name per asset; 0 = not yet uploaded. The decoded pixels live in the
// asset registry for the tab's lifetime, so one upload per asset is enough; a
// real-art swap is a rebuild, so there is no runtime invalidation to do.
std::array<GLuint, assets::kAssetCount> g_tex_cache{};

}  // namespace

std::uint64_t asset_gl_texture(assets::AssetId id) {
    const std::size_t idx = static_cast<std::size_t>(id);
    if (g_tex_cache[idx] != 0) {
        return g_tex_cache[idx];
    }

    const assets::AssetRegistry& reg = runtime().registry;
    if (!reg.is_loaded(id)) {
        return 0;  // pending or unavailable -> caller draws the procedural fallback
    }
    const assets::TextureHandle tex = reg.get_texture(id);
    const std::uint32_t w = tex.width();
    const std::uint32_t h = tex.height();
    const std::span<const std::byte> pixels = tex.pixels();
    if (!tex.valid() || w == 0 || h == 0 ||
        pixels.size() < static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u) {
        return 0;
    }

    GLuint name = 0;
    glGenTextures(1, &name);
    glBindTexture(GL_TEXTURE_2D, name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, static_cast<const void*>(pixels.data()));

    g_tex_cache[idx] = name;
    return name;
}

}  // namespace poker_trainer::bridge

#else

#include "bridge/texture_bind.hpp"

#include <cstdint>

// Native test build: there is no GL context, so the art-texture seam is a no-op
// that always reports "no texture available" — the render helpers then draw their
// procedural fallback. This keeps the pure `bridge` library ImGui/GL-free while
// still resolving bridge::asset_gl_texture for the screens / game test binaries.

namespace poker_trainer::bridge {

std::uint64_t asset_gl_texture(assets::AssetId) {
    return 0;
}

}  // namespace poker_trainer::bridge

#endif
