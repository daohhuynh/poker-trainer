// Placeholder asset generator.
//
// Emits a valid, functional stand-in PNG at every texture path declared in the
// frozen asset_paths.hpp contract, so the rest of Zone 02 (and downstream
// zones) can load canonical paths today and have real art dropped in later with
// zero code change. The generator iterates kAssetEntries directly, so it can
// never drift from the contract's path set.
//
// Self-contained: a minimal PNG writer (stored / uncompressed DEFLATE, with
// hand-rolled CRC-32 and Adler-32) and a small embedded 5x7 font keep this tool
// free of any third-party dependency. Placeholders are deliberately low-res and
// uncompressed; they exist for development legibility, not delivery.
//
// Usage: gen_placeholders [output_root]   (output_root defaults to ".")

#include "assets/asset_paths.hpp"
#include "tools/placeholder_layout.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using poker_trainer::assets::AssetId;
using poker_trainer::assets::asset_path;
using poker_trainer::assets::kAssetCount;
using poker_trainer::assets::placeholder::Size;
using poker_trainer::assets::placeholder::size_for;

struct Color {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
};

// ------------------------------------------------------------------ PNG writer

std::uint32_t crc32(const std::vector<std::uint8_t>& data) {
    static std::array<std::uint32_t, 256> table{};
    static bool ready = false;
    if (!ready) {
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[n] = c;
        }
        ready = true;
    }
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::uint8_t byte : data) {
        crc = table[(crc ^ byte) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

std::uint32_t adler32(const std::vector<std::uint8_t>& data) {
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (std::uint8_t byte : data) {
        a = (a + byte) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

void push_be32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
}

void write_chunk(std::vector<std::uint8_t>& out, std::string_view type,
                 const std::vector<std::uint8_t>& data) {
    push_be32(out, static_cast<std::uint32_t>(data.size()));
    std::vector<std::uint8_t> crc_input;
    crc_input.reserve(type.size() + data.size());
    for (char c : type) {
        crc_input.push_back(static_cast<std::uint8_t>(c));
    }
    crc_input.insert(crc_input.end(), data.begin(), data.end());
    out.insert(out.end(), crc_input.begin(), crc_input.end());
    push_be32(out, crc32(crc_input));
}

// Wrap raw bytes in a zlib stream using only stored (BTYPE=00) DEFLATE blocks.
std::vector<std::uint8_t> zlib_store(const std::vector<std::uint8_t>& raw) {
    std::vector<std::uint8_t> z;
    z.push_back(0x78);  // CMF: 32K window, deflate
    z.push_back(0x01);  // FLG: no dict, fastest; 0x7801 % 31 == 0
    std::size_t pos = 0;
    do {
        const std::size_t len = std::min<std::size_t>(65535, raw.size() - pos);
        const bool final_block = (pos + len == raw.size());
        z.push_back(final_block ? 0x01 : 0x00);  // BFINAL bit, BTYPE=00
        const auto len16 = static_cast<std::uint16_t>(len);
        const std::uint16_t nlen16 = static_cast<std::uint16_t>(~len16);
        z.push_back(static_cast<std::uint8_t>(len16 & 0xFFu));
        z.push_back(static_cast<std::uint8_t>((len16 >> 8) & 0xFFu));
        z.push_back(static_cast<std::uint8_t>(nlen16 & 0xFFu));
        z.push_back(static_cast<std::uint8_t>((nlen16 >> 8) & 0xFFu));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                 raw.begin() + static_cast<std::ptrdiff_t>(pos + len));
        pos += len;
    } while (pos < raw.size());
    push_be32(z, adler32(raw));
    return z;
}

bool write_png(const std::filesystem::path& file, std::uint32_t width, std::uint32_t height,
               const std::vector<std::uint8_t>& rgba) {
    // Raw image: each scanline is a filter byte (0 = None) followed by RGBA.
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(height) * (1 + static_cast<std::size_t>(width) * 4));
    for (std::uint32_t y = 0; y < height; ++y) {
        raw.push_back(0);
        const std::size_t row = static_cast<std::size_t>(y) * width * 4;
        raw.insert(raw.end(), rgba.begin() + static_cast<std::ptrdiff_t>(row),
                   rgba.begin() + static_cast<std::ptrdiff_t>(row + static_cast<std::size_t>(width) * 4));
    }

    std::vector<std::uint8_t> out{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

    std::vector<std::uint8_t> ihdr;
    push_be32(ihdr, width);
    push_be32(ihdr, height);
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(6);  // color type: RGBA
    ihdr.push_back(0);  // compression: deflate
    ihdr.push_back(0);  // filter: adaptive
    ihdr.push_back(0);  // interlace: none
    write_chunk(out, "IHDR", ihdr);
    write_chunk(out, "IDAT", zlib_store(raw));
    write_chunk(out, "IEND", {});

    std::ofstream stream(file, std::ios::binary);
    if (!stream) {
        return false;
    }
    stream.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return static_cast<bool>(stream);
}

// ------------------------------------------------------------------ Canvas

struct Canvas {
    std::uint32_t w;
    std::uint32_t h;
    std::vector<std::uint8_t> px;  // RGBA

    Canvas(std::uint32_t width, std::uint32_t height)
        : w{width}, h{height}, px(static_cast<std::size_t>(width) * height * 4, 0) {}

    void put(int x, int y, Color c) {
        if (x < 0 || y < 0 || static_cast<std::uint32_t>(x) >= w || static_cast<std::uint32_t>(y) >= h) {
            return;
        }
        const std::size_t i = (static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x)) * 4;
        px[i] = c.r;
        px[i + 1] = c.g;
        px[i + 2] = c.b;
        px[i + 3] = c.a;
    }

    void fill(Color c) {
        for (std::uint32_t y = 0; y < h; ++y) {
            for (std::uint32_t x = 0; x < w; ++x) {
                put(static_cast<int>(x), static_cast<int>(y), c);
            }
        }
    }

    void fill_rect(int x0, int y0, int rw, int rh, Color c) {
        for (int y = y0; y < y0 + rh; ++y) {
            for (int x = x0; x < x0 + rw; ++x) {
                put(x, y, c);
            }
        }
    }

    void stroke_rect(int x0, int y0, int rw, int rh, int t, Color c) {
        fill_rect(x0, y0, rw, t, c);
        fill_rect(x0, y0 + rh - t, rw, t, c);
        fill_rect(x0, y0, t, rh, c);
        fill_rect(x0 + rw - t, y0, t, rh, c);
    }

    void fill_circle(int cx, int cy, int r, Color c) {
        for (int y = -r; y <= r; ++y) {
            for (int x = -r; x <= r; ++x) {
                if (x * x + y * y <= r * r) {
                    put(cx + x, cy + y, c);
                }
            }
        }
    }

    void ring(int cx, int cy, int r, int t, Color c) {
        const int inner = (r - t) * (r - t);
        const int outer = r * r;
        for (int y = -r; y <= r; ++y) {
            for (int x = -r; x <= r; ++x) {
                const int d = x * x + y * y;
                if (d <= outer && d >= inner) {
                    put(cx + x, cy + y, c);
                }
            }
        }
    }
};

// ------------------------------------------------------------------ 5x7 font

// Each glyph is 7 rows of 5 columns; bit 4 (0b10000) is the leftmost column.
struct Glyph {
    char ch;
    std::array<std::uint8_t, 7> rows;
};

constexpr std::array<Glyph, 40> kFont{{
    {' ', {0, 0, 0, 0, 0, 0, 0}},
    {'-', {0, 0, 0, 0b11111, 0, 0, 0}},
    {'/', {0b00001, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b10000}},
    {'$', {0b00100, 0b01111, 0b10100, 0b01110, 0b00101, 0b11110, 0b00100}},
    {'0', {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}},
    {'1', {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'2', {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}},
    {'3', {0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110}},
    {'4', {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}},
    {'5', {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110}},
    {'6', {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}},
    {'7', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}},
    {'8', {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}},
    {'9', {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100}},
    {'A', {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'B', {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}},
    {'C', {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110}},
    {'D', {0b11100, 0b10010, 0b10001, 0b10001, 0b10001, 0b10010, 0b11100}},
    {'E', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}},
    {'F', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}},
    {'G', {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111}},
    {'H', {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'I', {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'J', {0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100}},
    {'K', {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001}},
    {'L', {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}},
    {'M', {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}},
    {'N', {0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001}},
    {'O', {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'P', {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}},
    {'Q', {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101}},
    {'R', {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}},
    {'S', {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}},
    {'T', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'U', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'V', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}},
    {'W', {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001}},
    {'X', {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}},
    {'Y', {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'Z', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111}},
}};

const std::array<std::uint8_t, 7>& glyph_rows(char c) {
    if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
    }
    for (const Glyph& g : kFont) {
        if (g.ch == c) {
            return g.rows;
        }
    }
    return kFont[0].rows;  // space for anything unmapped
}

int text_width(std::string_view text, int scale) {
    if (text.empty()) {
        return 0;
    }
    const int glyphs = static_cast<int>(text.size());
    return glyphs * 5 * scale + (glyphs - 1) * scale;  // 1*scale column gap
}

void draw_text(Canvas& cv, int x, int y, std::string_view text, int scale, Color c) {
    int cursor = x;
    for (char ch : text) {
        const std::array<std::uint8_t, 7>& rows = glyph_rows(ch);
        for (int ry = 0; ry < 7; ++ry) {
            for (int rx = 0; rx < 5; ++rx) {
                if ((rows[static_cast<std::size_t>(ry)] >> (4 - rx)) & 1u) {
                    cv.fill_rect(cursor + rx * scale, y + ry * scale, scale, scale, c);
                }
            }
        }
        cursor += 6 * scale;  // 5 columns + 1 gap
    }
}

void draw_text_centered(Canvas& cv, int cx, int y, std::string_view text, int scale, Color c) {
    draw_text(cv, cx - text_width(text, scale) / 2, y, text, scale, c);
}

// Largest scale at which `text` fits within `max_w`, clamped to [1, max_scale].
int fit_scale(std::string_view text, int max_w, int max_scale) {
    for (int s = max_scale; s > 1; --s) {
        if (text_width(text, s) <= max_w) {
            return s;
        }
    }
    return 1;
}

// ------------------------------------------------------------------ Per-asset rendering

std::string basename_no_ext(std::string_view path) {
    const std::size_t slash = path.find_last_of('/');
    std::string_view file = (slash == std::string_view::npos) ? path : path.substr(slash + 1);
    const std::size_t dot = file.find_last_of('.');
    if (dot != std::string_view::npos) {
        file = file.substr(0, dot);
    }
    return std::string{file};
}

std::string upper_words(std::string s) {
    for (char& c : s) {
        if (c == '_') {
            c = ' ';
        } else if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return s;
}

// A simple labeled portrait silhouette for dealer characters.
void draw_character(Canvas& cv, std::string_view label, Color body) {
    cv.fill_rect(0, 0, static_cast<int>(cv.w), static_cast<int>(cv.h), Color{30, 26, 24, 255});
    const int cx = static_cast<int>(cv.w) / 2;
    const int head_r = static_cast<int>(cv.w) / 6;
    const int head_y = static_cast<int>(cv.h) / 4;
    cv.fill_circle(cx, head_y, head_r, body);
    const int bw = static_cast<int>(cv.w) / 2;
    cv.fill_rect(cx - bw / 2, head_y + head_r, bw, static_cast<int>(cv.h) - (head_y + head_r) - 24,
                 body);
    cv.fill_circle(cx + head_r / 2, head_y, head_r / 6, Color{212, 175, 55, 255});  // monocle hint
    const int scale = fit_scale(label, static_cast<int>(cv.w) - 8, 2);
    draw_text_centered(cv, cx, static_cast<int>(cv.h) - 14, label, scale, Color{225, 215, 200, 255});
}

void render_asset(AssetId id, Canvas& cv) {
    using A = AssetId;
    const std::string base = basename_no_ext(asset_path(id));

    // Cards (52 faces + back).
    if ((id >= A::CardSpadeA && id <= A::CardClubK) || id == A::CardBack) {
        if (id == A::CardBack) {
            cv.fill(Color{40, 44, 86, 255});
            for (int y = 0; y < static_cast<int>(cv.h); y += 6) {
                for (int x = 0; x < static_cast<int>(cv.w); x += 6) {
                    cv.put(x, y, Color{80, 88, 150, 255});
                }
            }
            cv.stroke_rect(0, 0, static_cast<int>(cv.w), static_cast<int>(cv.h), 2, Color{20, 22, 44, 255});
            draw_text_centered(cv, static_cast<int>(cv.w) / 2, static_cast<int>(cv.h) / 2 - 7, "BACK", 2,
                               Color{225, 225, 240, 255});
            return;
        }
        cv.fill(Color{244, 242, 234, 255});
        cv.stroke_rect(0, 0, static_cast<int>(cv.w), static_cast<int>(cv.h), 2, Color{60, 56, 50, 255});
        // base is e.g. "spade_a": <suit>_<rank>.
        const std::size_t us = base.find('_');
        const std::string suit = base.substr(0, us);
        const std::string rank = base.substr(us + 1);
        const bool red = (suit == "heart" || suit == "diamond");
        const Color ink = red ? Color{196, 40, 40, 255} : Color{30, 30, 30, 255};
        std::string rank_label = rank;
        for (char& c : rank_label) {
            c = static_cast<char>((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);
        }
        const char suit_letter = static_cast<char>(suit.empty() ? '?' : (suit[0] - 'a' + 'A'));
        draw_text(cv, 5, 5, rank_label, 3, ink);
        draw_text_centered(cv, static_cast<int>(cv.w) / 2, static_cast<int>(cv.h) / 2 - 10,
                           std::string{suit_letter}, 4, ink);
        return;
    }

    // Chips (8 denominations) — solid cardroom-colored discs.
    if (id >= A::ChipWhite && id <= A::ChipGold) {
        static constexpr std::array<Color, 8> chip_colors{{
            {245, 245, 245, 255},  // white
            {196, 42, 42, 255},    // red
            {34, 139, 69, 255},    // green
            {28, 28, 28, 255},     // black
            {126, 52, 158, 255},   // purple
            {224, 196, 42, 255},   // yellow
            {120, 74, 38, 255},    // brown
            {212, 175, 55, 255},   // gold
        }};
        static constexpr std::array<std::string_view, 8> denom{
            {"$1", "$5", "$25", "$100", "$500", "$1000", "$5000", "$25000"}};
        const auto idx = static_cast<std::size_t>(id) - static_cast<std::size_t>(A::ChipWhite);
        const Color fill = chip_colors[idx];
        const int cx = static_cast<int>(cv.w) / 2;
        const int cy = static_cast<int>(cv.h) / 2;
        const int r = static_cast<int>(cv.w) / 2 - 3;
        cv.fill_circle(cx, cy, r, fill);
        cv.ring(cx, cy, r, 3, Color{250, 250, 250, 255});           // rim
        cv.ring(cx, cy, r - 8, 2, Color{250, 250, 250, 200});       // inset edge ring
        const bool light = (idx == 0 || idx == 5 || idx == 7);      // white/yellow/gold
        const Color ink = light ? Color{30, 30, 30, 255} : Color{245, 245, 245, 255};
        const int scale = fit_scale(denom[idx], 2 * r - 6, 2);
        draw_text_centered(cv, cx, cy - 3 * scale, denom[idx], scale, ink);
        return;
    }

    // Cluster icons.
    if (id >= A::IconShop && id <= A::IconClose) {
        static constexpr std::array<std::string_view, 5> labels{{"SHOP", "HELP", "SET", "HOME", "X"}};
        const auto idx = static_cast<std::size_t>(id) - static_cast<std::size_t>(A::IconShop);
        cv.fill(Color{0, 0, 0, 0});
        cv.fill_rect(2, 2, static_cast<int>(cv.w) - 4, static_cast<int>(cv.h) - 4, Color{58, 50, 44, 255});
        cv.stroke_rect(2, 2, static_cast<int>(cv.w) - 4, static_cast<int>(cv.h) - 4, 1, Color{120, 104, 88, 255});
        const int scale = fit_scale(labels[idx], static_cast<int>(cv.w) - 8, 2);
        draw_text_centered(cv, static_cast<int>(cv.w) / 2, static_cast<int>(cv.h) / 2 - 4 * scale, labels[idx],
                           scale, Color{235, 225, 210, 255});
        return;
    }

    // Position indicators.
    if (id >= A::PositionUTG && id <= A::PositionBB) {
        static constexpr std::array<std::string_view, 6> labels{{"UTG", "HJ", "CO", "BTN", "SB", "BB"}};
        const auto idx = static_cast<std::size_t>(id) - static_cast<std::size_t>(A::PositionUTG);
        cv.fill(Color{0, 0, 0, 0});
        cv.fill_circle(static_cast<int>(cv.w) / 2, static_cast<int>(cv.h) / 2, static_cast<int>(cv.w) / 2 - 2,
                       Color{44, 40, 38, 255});
        const int scale = fit_scale(labels[idx], static_cast<int>(cv.w) - 6, 2);
        draw_text_centered(cv, static_cast<int>(cv.w) / 2, static_cast<int>(cv.h) / 2 - 4 * scale, labels[idx],
                           scale, Color{235, 225, 210, 255});
        return;
    }

    // Root backgrounds (per theme) — solid dark tints.
    if (id >= A::RootBackgroundNoLimit && id <= A::RootBackgroundSage) {
        static constexpr std::array<Color, 4> tints{{
            {26, 18, 16, 255},  // no limit (warm brown)
            {22, 24, 28, 255},  // slate
            {14, 28, 34, 255},  // ocean
            {18, 26, 18, 255},  // sage
        }};
        static constexpr std::array<std::string_view, 4> names{
            {"NO LIMIT", "SLATE", "OCEAN", "SAGE"}};
        const auto idx = static_cast<std::size_t>(id) - static_cast<std::size_t>(A::RootBackgroundNoLimit);
        cv.fill(tints[idx]);
        draw_text_centered(cv, static_cast<int>(cv.w) / 2, static_cast<int>(cv.h) / 2 - 12, "ROOT BG", 2,
                           Color{120, 112, 104, 255});
        draw_text_centered(cv, static_cast<int>(cv.w) / 2, static_cast<int>(cv.h) / 2 + 4, names[idx], 2,
                           Color{120, 112, 104, 255});
        return;
    }

    // Dealer characters and Frog (front/side portraits).
    if (id == A::ButlerSideProfile) {
        draw_character(cv, "BUTLER SIDE", Color{70, 60, 52, 255});
        return;
    }
    if (id == A::ButlerFrontNeutral) {
        draw_character(cv, "BUTLER NEUTRAL", Color{70, 60, 52, 255});
        return;
    }
    if (id == A::ButlerFrontRaised) {
        draw_character(cv, "BUTLER RAISED", Color{70, 60, 52, 255});
        return;
    }
    if (id == A::FrogSideProfile) {
        draw_character(cv, "FROG SIDE", Color{70, 110, 64, 255});
        return;
    }
    if (id == A::FrogFrontNeutral) {
        draw_character(cv, "FROG NEUTRAL", Color{70, 110, 64, 255});
        return;
    }
    if (id == A::FrogFrontRaised) {
        draw_character(cv, "FROG RAISED", Color{70, 110, 64, 255});
        return;
    }

    // Frog expression overlays — transparent but for the expression element.
    if (id >= A::FrogExpressionPass && id <= A::FrogExpressionPerfect) {
        cv.fill(Color{0, 0, 0, 0});
        const int cx = static_cast<int>(cv.w) / 2;
        const int cy = static_cast<int>(cv.h) / 4;
        std::string_view label;
        switch (id) {
            case A::FrogExpressionPass:
                cv.fill_circle(cx - 28, cy, 8, Color{255, 150, 170, 220});
                cv.fill_circle(cx + 28, cy, 8, Color{255, 150, 170, 220});
                label = "PASS";
                break;
            case A::FrogExpressionFail:
                cv.fill_rect(cx - 6, cy + 6, 12, 24, Color{210, 70, 80, 220});
                label = "FAIL";
                break;
            case A::FrogExpressionOvertime:
                cv.fill_circle(cx, cy, 10, Color{225, 140, 40, 220});
                label = "OVERTIME";
                break;
            case A::FrogExpressionPerfect:
            default:
                cv.fill_circle(cx, cy, 10, Color{212, 175, 55, 230});
                label = "PERFECT";
                break;
        }
        const int scale = fit_scale(label, static_cast<int>(cv.w) - 8, 2);
        draw_text_centered(cv, cx, static_cast<int>(cv.h) - 24, label, scale, Color{255, 255, 255, 230});
        return;
    }

    // Remaining singletons.
    if (id == A::AppLogo) {
        cv.fill(Color{24, 18, 16, 255});
        cv.stroke_rect(0, 0, static_cast<int>(cv.w), static_cast<int>(cv.h), 2, Color{120, 96, 48, 255});
        draw_text_centered(cv, static_cast<int>(cv.w) / 2, static_cast<int>(cv.h) / 2 - 7, "POKER TRAINER", 2,
                           Color{239, 180, 46, 255});
        return;
    }
    if (id == A::DealerButton) {
        cv.fill(Color{0, 0, 0, 0});
        const int cx = static_cast<int>(cv.w) / 2;
        const int cy = static_cast<int>(cv.h) / 2;
        const int r = static_cast<int>(cv.w) / 2 - 4;
        cv.fill_circle(cx, cy, r, Color{38, 110, 158, 255});         // ocean blue
        cv.ring(cx, cy, r - 10, 3, Color{245, 245, 245, 220});       // inset dashed-ring stand-in
        draw_text_centered(cv, cx, cy - 14, "M", 4, Color{122, 158, 108, 255});  // sage monogram
        return;
    }
    if (id == A::TableFelt) {
        cv.fill(Color{28, 64, 40, 255});
        cv.stroke_rect(0, 0, static_cast<int>(cv.w), static_cast<int>(cv.h), 3, Color{60, 40, 24, 255});
        draw_text_centered(cv, static_cast<int>(cv.w) / 2, static_cast<int>(cv.h) / 2 - 7, "TABLE FELT", 2,
                           Color{120, 150, 128, 255});
        return;
    }
    if (id == A::SidePotAllInMarker) {
        cv.fill(Color{0, 0, 0, 0});
        cv.fill_circle(static_cast<int>(cv.w) / 2, static_cast<int>(cv.h) / 2, static_cast<int>(cv.w) / 2 - 2,
                       Color{176, 56, 48, 255});
        draw_text_centered(cv, static_cast<int>(cv.w) / 2, static_cast<int>(cv.h) / 2 - 11, "ALL", 1,
                           Color{245, 240, 235, 255});
        draw_text_centered(cv, static_cast<int>(cv.w) / 2, static_cast<int>(cv.h) / 2 + 2, "IN", 1,
                           Color{245, 240, 235, 255});
        return;
    }

    // Fallback (should be unreachable): a labeled neutral tile.
    cv.fill(Color{50, 50, 50, 255});
    draw_text_centered(cv, static_cast<int>(cv.w) / 2, static_cast<int>(cv.h) / 2 - 4,
                       upper_words(base), 1, Color{220, 220, 220, 255});
}

}  // namespace

int main(int argc, char** argv) {
    const std::filesystem::path root = (argc > 1) ? argv[1] : ".";

    int written = 0;
    for (std::size_t i = 0; i < kAssetCount; ++i) {
        const auto id = static_cast<AssetId>(i);
        const Size size = size_for(id);
        Canvas cv(size.width, size.height);
        render_asset(id, cv);

        const std::filesystem::path out = root / std::filesystem::path(asset_path(id));
        std::error_code ec;
        std::filesystem::create_directories(out.parent_path(), ec);
        if (ec) {
            std::fprintf(stderr, "gen_placeholders: cannot create %s: %s\n",
                         out.parent_path().string().c_str(), ec.message().c_str());
            return 1;
        }
        if (!write_png(out, size.width, size.height, cv.px)) {
            std::fprintf(stderr, "gen_placeholders: failed to write %s\n", out.string().c_str());
            return 1;
        }
        ++written;
    }

    std::printf("gen_placeholders: wrote %d placeholder PNGs under %s\n", written,
                root.string().c_str());
    return 0;
}
