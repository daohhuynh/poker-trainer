#include "bridge/loading_screen.hpp"

#include <cstddef>

namespace poker_trainer::bridge {

float loading_arc_fraction(std::size_t resolved, std::size_t total) noexcept {
    if (total == 0) {
        return 1.0f;
    }
    if (resolved >= total) {
        return 1.0f;
    }
    return static_cast<float>(resolved) / static_cast<float>(total);
}

}  // namespace poker_trainer::bridge
