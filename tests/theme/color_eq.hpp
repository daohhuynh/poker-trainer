#pragma once

// Shared color-comparison helpers for the Z06 theme tests. Palette values are
// produced by identical constexpr computations on both sides of every
// assertion, so exact float comparison is appropriate; EXPECT_FLOAT_EQ guards
// against ULP-level surprises regardless.

#include <cmath>

#include <gtest/gtest.h>

#include <imgui.h>

namespace poker_trainer::theme::test {

inline void expect_color_eq(const ImVec4& actual, const ImVec4& expected) {
    EXPECT_FLOAT_EQ(actual.x, expected.x);
    EXPECT_FLOAT_EQ(actual.y, expected.y);
    EXPECT_FLOAT_EQ(actual.z, expected.z);
    EXPECT_FLOAT_EQ(actual.w, expected.w);
}

inline bool colors_equal(const ImVec4& a, const ImVec4& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

// Euclidean distance between two colors in 0..255 RGB space (alpha ignored).
// Used to assert the Ocean/Sage accents are visually distinct from the fixed
// dealer-button colors.
inline float rgb_distance_255(const ImVec4& a, const ImVec4& b) {
    const float dr = (a.x - b.x) * 255.0f;
    const float dg = (a.y - b.y) * 255.0f;
    const float db = (a.z - b.z) * 255.0f;
    return std::sqrt(dr * dr + dg * dg + db * db);
}

}  // namespace poker_trainer::theme::test
