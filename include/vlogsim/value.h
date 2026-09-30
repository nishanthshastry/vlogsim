#pragma once

#include <cstdint>

namespace vlogsim {

// Milestone-1 values are two-state and capped at 64 bits; the Milestone-4
// upgrade replaces this with a {value, unknown-mask} pair of arbitrary width.
inline uint64_t mask_for_width(int width) {
    if (width <= 0) return 0;
    if (width >= 64) return ~uint64_t{0};
    return (uint64_t{1} << width) - 1;
}

inline uint64_t mask_to_width(uint64_t value, int width) {
    return value & mask_for_width(width);
}

} // namespace vlogsim
