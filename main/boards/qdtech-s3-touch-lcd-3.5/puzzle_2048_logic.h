#pragma once

#include <cstdint>

namespace QdPuzzle2048 {

bool Move(uint32_t cells[16], uint32_t* score, uint32_t* best_tile,
          bool* won, int dx, int dy);
bool CanMove(const uint32_t cells[16]);
bool Spawn(uint32_t cells[16], uint32_t random_index, uint32_t random_tile);

}  // namespace QdPuzzle2048
