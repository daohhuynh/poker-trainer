#include "bridge/platform.hpp"

#include "bridge/canvas_sizing.hpp"
#include "bridge/gl_renderer.hpp"

#include "backbone/event_router.hpp"

#include "imgui.h"

#include <cstring>

#include <GLES3/gl3.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgl.h>

// Binding code: compiled only into the wasm app and held to -Wall -Wextra
// -Werror (not the full -Wconversion baseline) per the gen_placeholders
// precedent — DOM/GL plumbing would otherwise bury the logic in casts.

namespace poker_trainer::bridge {

namespace {

// Map a DOM KeyboardEvent.code string to a backbone KeyCode.
[[nodiscard]] backbone::KeyCode map_key_code(const char* code) noexcept {
    using KC = backbone::KeyCode;
    if (code == nullptr) {
        return KC::Unknown;
    }
    if (std::strcmp(code, "Tab") == 0) return KC::Tab;
    if (std::strcmp(code, "Enter") == 0 || std::strcmp(code, "NumpadEnter") == 0)
        return KC::Enter;
    if (std::strcmp(code, "Escape") == 0) return KC::Escape;
    if (std::strcmp(code, "Space") == 0) return KC::Space;
    if (std::strcmp(code, "ArrowUp") == 0) return KC::ArrowUp;
    if (std::strcmp(code, "ArrowDown") == 0) return KC::ArrowDown;
    if (std::strcmp(code, "ArrowLeft") == 0) return KC::ArrowLeft;
    if (std::strcmp(code, "ArrowRight") == 0) return KC::ArrowRight;
    if (std::strcmp(code, "Backspace") == 0) return KC::Backspace;
    if (std::strcmp(code, "Delete") == 0) return KC::Delete;
    // "Digit0".."Digit9"
    if (std::strncmp(code, "Digit", 5) == 0 && code[5] >= '0' && code[5] <= '9' &&
        code[6] == '\0') {
        const int offset = code[5] - '0';
        return static_cast<KC>(static_cast<int>(KC::Digit0) + offset);
    }
    // "KeyA".."KeyZ"
    if (std::strncmp(code, "Key", 3) == 0 && code[3] >= 'A' && code[3] <= 'Z' &&
        code[4] == '\0') {
        const int offset = code[3] - 'A';
        return static_cast<KC>(static_cast<int>(KC::LetterA) + offset);
    }
    return KC::Unknown;
}

[[nodiscard]] backbone::ModMask map_mods(const EmscriptenKeyboardEvent* e) noexcept {
    backbone::ModMask mods = backbone::ModMask::None;
    if (e->shiftKey) mods = mods | backbone::ModMask::Shift;
    if (e->ctrlKey) mods = mods | backbone::ModMask::Ctrl;
    if (e->altKey) mods = mods | backbone::ModMask::Alt;
    if (e->metaKey) mods = mods | backbone::ModMask::Meta;
    return mods;
}

// DOM mouse button (0 left, 1 middle, 2 right) -> ImGui button (0 left, 1
// right, 2 middle).
[[nodiscard]] int imgui_mouse_button(unsigned short dom_button) noexcept {
    if (dom_button == 2) return 1;
    if (dom_button == 1) return 2;
    return 0;
}

EM_BOOL on_key_down(int, const EmscriptenKeyboardEvent* e, void*) {
    const backbone::KeyCode code = map_key_code(e->code);
    backbone::dispatch_key_event(
        {backbone::KeyEventType::KeyDown, code, map_mods(e)});
    // Consume Tab and the arrow keys so the browser does not scroll / move focus
    // out of the canvas; everything else passes through to the page.
    switch (code) {
        case backbone::KeyCode::Tab:
        case backbone::KeyCode::ArrowUp:
        case backbone::KeyCode::ArrowDown:
        case backbone::KeyCode::ArrowLeft:
        case backbone::KeyCode::ArrowRight:
        case backbone::KeyCode::Space:
            return EM_TRUE;
        default:
            return EM_FALSE;
    }
}

EM_BOOL on_key_up(int, const EmscriptenKeyboardEvent* e, void*) {
    backbone::dispatch_key_event(
        {backbone::KeyEventType::KeyUp, map_key_code(e->code), map_mods(e)});
    return EM_FALSE;
}

void feed_imgui_mouse_pos(const EmscriptenMouseEvent* e) {
    ImGui::GetIO().AddMousePosEvent(static_cast<float>(e->targetX),
                                    static_cast<float>(e->targetY));
}

EM_BOOL on_mouse_move(int, const EmscriptenMouseEvent* e, void*) {
    feed_imgui_mouse_pos(e);
    backbone::dispatch_mouse_event(
        {backbone::MouseEventType::MouseMove, static_cast<float>(e->targetX),
         static_cast<float>(e->targetY), 0, 0.0f});
    return EM_FALSE;
}

EM_BOOL on_mouse_down(int, const EmscriptenMouseEvent* e, void*) {
    feed_imgui_mouse_pos(e);
    ImGui::GetIO().AddMouseButtonEvent(imgui_mouse_button(e->button), true);
    backbone::dispatch_mouse_event(
        {backbone::MouseEventType::MouseDown, static_cast<float>(e->targetX),
         static_cast<float>(e->targetY), static_cast<int>(e->button), 0.0f});
    return EM_TRUE;
}

EM_BOOL on_mouse_up(int, const EmscriptenMouseEvent* e, void*) {
    feed_imgui_mouse_pos(e);
    ImGui::GetIO().AddMouseButtonEvent(imgui_mouse_button(e->button), false);
    backbone::dispatch_mouse_event(
        {backbone::MouseEventType::MouseUp, static_cast<float>(e->targetX),
         static_cast<float>(e->targetY), static_cast<int>(e->button), 0.0f});
    return EM_TRUE;
}

EM_BOOL on_wheel(int, const EmscriptenWheelEvent* e, void*) {
    ImGui::GetIO().AddMouseWheelEvent(0.0f, static_cast<float>(-e->deltaY) * 0.01f);
    backbone::dispatch_mouse_event(
        {backbone::MouseEventType::Wheel, static_cast<float>(e->mouse.targetX),
         static_cast<float>(e->mouse.targetY), 0,
         static_cast<float>(e->deltaY)});
    return EM_TRUE;
}

}  // namespace

bool platform_init() {
    // Ensure a full-window #canvas exists and the page chrome does not scroll.
    EM_ASM({
        var c = document.getElementById('canvas');
        if (!c) {
            c = document.createElement('canvas');
            c.id = 'canvas';
            document.body.appendChild(c);
        }
        c.style.display = 'block';
        c.oncontextmenu = function(ev) { ev.preventDefault(); };
        document.body.style.margin = '0';
        document.documentElement.style.overflow = 'hidden';
    });

    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.majorVersion = 2;  // WebGL2 / GLES3
    attrs.minorVersion = 0;
    attrs.alpha = EM_FALSE;
    attrs.depth = EM_FALSE;
    attrs.stencil = EM_FALSE;
    attrs.antialias = EM_TRUE;
    attrs.premultipliedAlpha = EM_TRUE;
    const EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx =
        emscripten_webgl_create_context("#canvas", &attrs);
    if (ctx <= 0) {
        return false;
    }
    emscripten_webgl_make_context_current(ctx);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // no settings persistence at this layer
    // Deliberately NOT setting ImGuiBackendFlags_RendererHasVtxOffset: WebGL2 has
    // no base-vertex draw, so ImGui must keep vertex offsets at zero.

    if (!gl_renderer_init()) {
        return false;
    }

    // DOM input -> backbone event_router (+ ImGui IO for mouse). Keys on the
    // window; mouse/wheel on the canvas. useCapture = false.
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr,
                                    EM_FALSE, on_key_down);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr,
                                  EM_FALSE, on_key_up);
    emscripten_set_mousemove_callback("#canvas", nullptr, EM_FALSE, on_mouse_move);
    emscripten_set_mousedown_callback("#canvas", nullptr, EM_FALSE, on_mouse_down);
    emscripten_set_mouseup_callback("#canvas", nullptr, EM_FALSE, on_mouse_up);
    emscripten_set_wheel_callback("#canvas", nullptr, EM_FALSE, on_wheel);
    return true;
}

bool platform_launch_is_mobile() noexcept {
    static const bool mobile = [] {
        const char* ua = emscripten_run_script_string("navigator.userAgent");
        return is_mobile_user_agent(ua != nullptr ? ua : "");
    }();
    return mobile;
}

CanvasDims platform_sync_viewport() noexcept {
    const int viewport_w = EM_ASM_INT({ return window.innerWidth | 0; });
    const int viewport_h = EM_ASM_INT({ return window.innerHeight | 0; });
    const double dpr = EM_ASM_DOUBLE({ return window.devicePixelRatio || 1.0; });

    const CanvasDims css = canvas_dims_from_viewport(viewport_w, viewport_h);
    const int fb_w = static_cast<int>(static_cast<double>(css.width) * dpr);
    const int fb_h = static_cast<int>(static_cast<double>(css.height) * dpr);
    emscripten_set_canvas_element_size("#canvas", fb_w, fb_h);
    EM_ASM(
        {
            var c = document.getElementById('canvas');
            if (c) {
                c.style.width = $0 + 'px';
                c.style.height = $1 + 'px';
            }
        },
        css.width, css.height);

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize =
        ImVec2(static_cast<float>(css.width), static_cast<float>(css.height));
    io.DisplayFramebufferScale =
        ImVec2(static_cast<float>(dpr), static_cast<float>(dpr));
    return css;
}

void platform_present(float r, float g, float b, float a) noexcept {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
    gl_renderer_render(ImGui::GetDrawData());
}

}  // namespace poker_trainer::bridge
