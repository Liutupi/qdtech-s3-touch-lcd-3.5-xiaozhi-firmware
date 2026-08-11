#include "number_slide_logic.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace QdNumberSlide {
namespace {

uint32_t NextRandom(uint32_t* state) {
    uint32_t value = *state ? *state : 0x15a4c3d2U;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

bool Adjacent(uint8_t lhs, uint8_t rhs) {
    const int lhs_row = lhs / 4;
    const int lhs_col = lhs % 4;
    const int rhs_row = rhs / 4;
    const int rhs_col = rhs % 4;
    return std::abs(lhs_row - rhs_row) + std::abs(lhs_col - rhs_col) == 1;
}

}  // namespace

void ResetSolved(uint8_t cells[kCellCount]) {
    if (!cells) return;
    for (uint8_t i = 0; i < kCellCount - 1; ++i) cells[i] = i + 1;
    cells[kCellCount - 1] = kBlank;
}

uint8_t FindBlank(const uint8_t cells[kCellCount]) {
    if (!cells) return kCellCount;
    for (uint8_t i = 0; i < kCellCount; ++i) {
        if (cells[i] == kBlank) return i;
    }
    return kCellCount;
}

bool MoveTile(uint8_t cells[kCellCount], uint8_t tile_index) {
    if (!cells || tile_index >= kCellCount) return false;
    const uint8_t blank = FindBlank(cells);
    if (blank >= kCellCount || cells[tile_index] == kBlank ||
        !Adjacent(tile_index, blank)) {
        return false;
    }
    std::swap(cells[tile_index], cells[blank]);
    return true;
}

bool IsSolved(const uint8_t cells[kCellCount]) {
    if (!cells) return false;
    for (uint8_t i = 0; i < kCellCount - 1; ++i) {
        if (cells[i] != i + 1) return false;
    }
    return cells[kCellCount - 1] == kBlank;
}

bool IsValidPermutation(const uint8_t cells[kCellCount]) {
    if (!cells) return false;
    uint16_t seen = 0;
    for (uint8_t i = 0; i < kCellCount; ++i) {
        if (cells[i] >= kCellCount) return false;
        const uint16_t bit = static_cast<uint16_t>(1U << cells[i]);
        if (seen & bit) return false;
        seen |= bit;
    }
    return seen == 0xffffU;
}

void Shuffle(uint8_t cells[kCellCount], uint32_t seed, uint16_t steps) {
    if (!cells) return;
    ResetSolved(cells);
    uint8_t blank = kCellCount - 1;
    uint8_t previous_blank = kCellCount;
    uint32_t random = seed;
    const uint16_t move_count = std::max<uint16_t>(steps, 32);
    for (uint16_t step = 0; step < move_count; ++step) {
        uint8_t choices[4]{};
        uint8_t count = 0;
        const int row = blank / 4;
        const int col = blank % 4;
        auto add = [&](int index) {
            if (index >= 0 && index < kCellCount && index != previous_blank) {
                choices[count++] = static_cast<uint8_t>(index);
            }
        };
        if (row > 0) add(blank - 4);
        if (row < 3) add(blank + 4);
        if (col > 0) add(blank - 1);
        if (col < 3) add(blank + 1);
        if (count == 0) choices[count++] = previous_blank;
        const uint8_t tile = choices[NextRandom(&random) % count];
        previous_blank = blank;
        std::swap(cells[blank], cells[tile]);
        blank = tile;
    }
    // Extremely rare random walks can return to the solved state. Keep every
    // newly created game meaningful with one final legal move.
    if (IsSolved(cells)) MoveTile(cells, 14);
}

}  // namespace QdNumberSlide
