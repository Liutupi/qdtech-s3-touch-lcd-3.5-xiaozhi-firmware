#include "puzzle_2048_logic.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

namespace {

uint32_t NextRandom(uint32_t* state) {
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

bool IsPowerOfTwo(uint32_t value) {
    return value == 0 || (value & (value - 1)) == 0;
}

void Reset(uint32_t cells[16], uint32_t* score, uint32_t* best,
           bool* won, uint32_t* random) {
    std::memset(cells, 0, sizeof(uint32_t) * 16);
    *score = 0;
    *best = 0;
    *won = false;
    assert(QdPuzzle2048::Spawn(cells, NextRandom(random), NextRandom(random)));
    assert(QdPuzzle2048::Spawn(cells, NextRandom(random), NextRandom(random)));
}

}  // namespace

int main() {
    {
        uint32_t cells[16] = {2, 2, 2, 2};
        uint32_t score = 0;
        uint32_t best = 0;
        bool won = false;
        assert(QdPuzzle2048::Move(cells, &score, &best, &won, -1, 0));
        assert(cells[0] == 4 && cells[1] == 4 && cells[2] == 0 && cells[3] == 0);
        assert(score == 8 && best == 4 && !won);
    }
    {
        uint32_t cells[16] = {2, 4, 8, 16};
        uint32_t before[16];
        std::memcpy(before, cells, sizeof(cells));
        uint32_t score = 0;
        uint32_t best = 16;
        bool won = false;
        assert(!QdPuzzle2048::Move(cells, &score, &best, &won, -1, 0));
        assert(std::memcmp(before, cells, sizeof(cells)) == 0);
    }

    uint32_t cells[16]{};
    uint32_t score = 0;
    uint32_t best = 0;
    bool won = false;
    uint32_t random = 0x20481234U;
    Reset(cells, &score, &best, &won, &random);
    static constexpr std::array<std::array<int, 2>, 4> directions{{
        {{0, -1}}, {{-1, 0}}, {{0, 1}}, {{1, 0}},
    }};
    uint32_t completed_moves = 0;
    for (uint32_t operation = 0; operation < 10000; ++operation) {
        const auto& direction = directions[NextRandom(&random) & 3U];
        if (QdPuzzle2048::Move(cells, &score, &best, &won,
                               direction[0], direction[1])) {
            ++completed_moves;
            QdPuzzle2048::Spawn(cells, NextRandom(&random), NextRandom(&random));
        }
        for (uint32_t value : cells) assert(IsPowerOfTwo(value));
        assert(best == 0 || IsPowerOfTwo(best));
        if (!QdPuzzle2048::CanMove(cells)) {
            Reset(cells, &score, &best, &won, &random);
        }
    }
    assert(completed_moves > 5000);
    return 0;
}
