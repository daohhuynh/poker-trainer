#include "bridge/gl_renderer.hpp"

#include "imgui.h"

#include <cstddef>
#include <cstdint>

#include <GLES3/gl3.h>

// WebGL2 / GLES3 ImGui backend. Binding code: compiled only into the wasm app
// and held to -Wall -Wextra -Werror (not the full -Wconversion baseline) per the
// gen_placeholders precedent in CMakeLists — GL type plumbing would otherwise
// bury the logic in casts without catching real defects. Still uses C++ casts
// (no C-style casts) per CLAUDE.md §10.

namespace poker_trainer::bridge {

namespace {

GLuint g_program = 0;
GLint g_loc_proj = 0;
GLint g_loc_tex = 0;
GLuint g_vao = 0;
GLuint g_vbo = 0;
GLuint g_ebo = 0;
GLuint g_font_tex = 0;

constexpr GLuint kAttribPosition = 0;
constexpr GLuint kAttribUV = 1;
constexpr GLuint kAttribColor = 2;

const char* const kVertexShader =
    "#version 300 es\n"
    "precision highp float;\n"
    "layout(location = 0) in vec2 Position;\n"
    "layout(location = 1) in vec2 UV;\n"
    "layout(location = 2) in vec4 Color;\n"
    "uniform mat4 ProjMtx;\n"
    "out vec2 Frag_UV;\n"
    "out vec4 Frag_Color;\n"
    "void main() {\n"
    "  Frag_UV = UV;\n"
    "  Frag_Color = Color;\n"
    "  gl_Position = ProjMtx * vec4(Position.xy, 0.0, 1.0);\n"
    "}\n";

const char* const kFragmentShader =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D Texture;\n"
    "in vec2 Frag_UV;\n"
    "in vec4 Frag_Color;\n"
    "layout(location = 0) out vec4 Out_Color;\n"
    "void main() {\n"
    "  Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
    "}\n";

[[nodiscard]] GLuint compile_shader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void build_font_atlas() {
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    glGenTextures(1, &g_font_tex);
    glBindTexture(GL_TEXTURE_2D, g_font_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels);

    io.Fonts->SetTexID(static_cast<ImTextureID>(g_font_tex));
}

[[nodiscard]] const void* byte_offset(std::size_t bytes) noexcept {
    return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(bytes));
}

}  // namespace

bool gl_renderer_init() {
    const GLuint vs = compile_shader(GL_VERTEX_SHADER, kVertexShader);
    const GLuint fs = compile_shader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (vs == 0 || fs == 0) {
        return false;
    }
    g_program = glCreateProgram();
    glAttachShader(g_program, vs);
    glAttachShader(g_program, fs);
    glLinkProgram(g_program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = GL_FALSE;
    glGetProgramiv(g_program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        return false;
    }

    g_loc_proj = glGetUniformLocation(g_program, "ProjMtx");
    g_loc_tex = glGetUniformLocation(g_program, "Texture");

    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);
    glGenBuffers(1, &g_ebo);

    build_font_atlas();
    return true;
}

void gl_renderer_render(ImDrawData* draw_data) {
    if (draw_data == nullptr) {
        return;
    }
    const int fb_width =
        static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    const int fb_height =
        static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0) {
        return;
    }

    glViewport(0, 0, fb_width, fb_height);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                        GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_SCISSOR_TEST);

    // Orthographic projection (column-major), mapping DisplayPos..DisplayPos+
    // DisplaySize to the [-1, 1] clip cube.
    const float l = draw_data->DisplayPos.x;
    const float r = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    const float t = draw_data->DisplayPos.y;
    const float b = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    const float ortho[4][4] = {
        {2.0f / (r - l), 0.0f, 0.0f, 0.0f},
        {0.0f, 2.0f / (t - b), 0.0f, 0.0f},
        {0.0f, 0.0f, -1.0f, 0.0f},
        {(r + l) / (l - r), (t + b) / (b - t), 0.0f, 1.0f},
    };

    glUseProgram(g_program);
    glUniformMatrix4fv(g_loc_proj, 1, GL_FALSE, &ortho[0][0]);
    glUniform1i(g_loc_tex, 0);
    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
    glEnableVertexAttribArray(kAttribPosition);
    glEnableVertexAttribArray(kAttribUV);
    glEnableVertexAttribArray(kAttribColor);
    glVertexAttribPointer(kAttribPosition, 2, GL_FLOAT, GL_FALSE,
                          sizeof(ImDrawVert),
                          byte_offset(offsetof(ImDrawVert, pos)));
    glVertexAttribPointer(kAttribUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert),
                          byte_offset(offsetof(ImDrawVert, uv)));
    glVertexAttribPointer(kAttribColor, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                          sizeof(ImDrawVert),
                          byte_offset(offsetof(ImDrawVert, col)));

    const ImVec2 clip_off = draw_data->DisplayPos;
    const ImVec2 clip_scale = draw_data->FramebufferScale;
    const GLenum index_type =
        sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;

    for (int n = 0; n < draw_data->CmdListsCount; ++n) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(cmd_list->VtxBuffer.Size) *
                         static_cast<GLsizeiptr>(sizeof(ImDrawVert)),
                     cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(cmd_list->IdxBuffer.Size) *
                         static_cast<GLsizeiptr>(sizeof(ImDrawIdx)),
                     cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; ++cmd_i) {
            const ImDrawCmd& cmd = cmd_list->CmdBuffer[cmd_i];
            const float clip_min_x = (cmd.ClipRect.x - clip_off.x) * clip_scale.x;
            const float clip_min_y = (cmd.ClipRect.y - clip_off.y) * clip_scale.y;
            const float clip_max_x = (cmd.ClipRect.z - clip_off.x) * clip_scale.x;
            const float clip_max_y = (cmd.ClipRect.w - clip_off.y) * clip_scale.y;
            if (clip_max_x <= clip_min_x || clip_max_y <= clip_min_y) {
                continue;
            }
            glScissor(static_cast<GLint>(clip_min_x),
                      static_cast<GLint>(static_cast<float>(fb_height) - clip_max_y),
                      static_cast<GLsizei>(clip_max_x - clip_min_x),
                      static_cast<GLsizei>(clip_max_y - clip_min_y));
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(cmd.GetTexID()));
            glDrawElements(
                GL_TRIANGLES, static_cast<GLsizei>(cmd.ElemCount), index_type,
                byte_offset(static_cast<std::size_t>(cmd.IdxOffset) *
                            sizeof(ImDrawIdx)));
        }
    }

    glDisable(GL_SCISSOR_TEST);
    glBindVertexArray(0);
}

void gl_renderer_shutdown() {
    if (g_font_tex != 0) {
        glDeleteTextures(1, &g_font_tex);
        g_font_tex = 0;
    }
    if (g_vbo != 0) {
        glDeleteBuffers(1, &g_vbo);
        g_vbo = 0;
    }
    if (g_ebo != 0) {
        glDeleteBuffers(1, &g_ebo);
        g_ebo = 0;
    }
    if (g_vao != 0) {
        glDeleteVertexArrays(1, &g_vao);
        g_vao = 0;
    }
    if (g_program != 0) {
        glDeleteProgram(g_program);
        g_program = 0;
    }
}

}  // namespace poker_trainer::bridge
