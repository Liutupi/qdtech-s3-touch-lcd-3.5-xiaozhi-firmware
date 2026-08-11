#include "puzzle_2048_logic.h"

#include <algorithm>

namespace QdPuzzle2048 {

bool Move(uint32_t cells[16], uint32_t* score, uint32_t* best_tile,
          bool* won, int dx, int dy) {
    if (!cells || !score || !best_tile || !won || (dx == 0 && dy == 0)) {
        return false;
    }
    bool changed = false;
    for (int line = 0; line < 4; ++line) {
        uint32_t values[4]{};
        for (int step = 0; step < 4; ++step) {
            const int x = dx ? (dx > 0 ? 3 - step : step) : line;
            const int y = dy ? (dy > 0 ? 3 - step : step) : line;
            values[step] = cells[y * 4 + x];
        }
        uint32_t compact[4]{};
        int count = 0;
        for (uint32_t value : values) {
            if (value) compact[count++] = value;
        }
        uint32_t merged[4]{};
        int out = 0;
        for (int i = 0; i < count; ++i) {
            if (i + 1 < count && compact[i] == compact[i + 1]) {
                merged[out] = compact[i] * 2;
                *score += merged[out];
                *best_tile = std::max(*best_tile, merged[out]);
                if (merged[out] >= 2048) *won = true;
                ++i;
            } else {
                merged[out] = compact[i];
                *best_tile = std::max(*best_tile, merged[out]);
            }
            ++out;
        }
        for (int step = 0; step < 4; ++step) {
            const int x = dx ? (dx > 0 ? 3 - step : step) : line;
            const int y = dy ? (dy > 0 ? 3 - step : step) : line;
            const int index = y * 4 + x;
            changed = changed || cells[index] != merged[step];
            cells[index] = merged[step];
        }
    }
    return changed;
}

bool CanMove(const uint32_t cells[16]) {
    if (!cells) return false;
    for (int i = 0; i < 16; ++i) {
        if (cells[i] == 0) return true;
        const int x = i % 4;
        const int y = i / 4;
        if ((x < 3 && cells[i] == cells[i + 1]) ||
            (y < 3 && cells[i] == cells[i + 4])) {
            return true;
        }
    }
    return false;
}

bool Spawn(uint32_t cells[16], uint32_t random_index, uint32_t random_tile) {
    if (!cells) return false;
    uint8_t empty[16];
    uint8_t count = 0;
    for (uint8_t i = 0; i < 16; ++i) {
        if (cells[i] == 0) empty[count++] = i;
    }
    if (count == 0) return false;
    const uint8_t index = empty[random_index % count];
    cells[index] = (random_tile % 10U == 0U) ? 4U : 2U;
    return true;
}

}  // namespace QdPuzzle2048
