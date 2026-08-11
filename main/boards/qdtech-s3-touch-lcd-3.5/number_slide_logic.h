#pragma once

#include <cstdint>

namespace QdNumberSlide {

constexpr uint8_t kCellCount = 16;
constexpr uint8_t kBlank = 0;

void ResetSolved(uint8_t cells[kCellCount]);
void Shuffle(uint8_t cells[kCellCount], uint32_t seed, uint16_t steps = 160);
bool MoveTile(uint8_t cells[kCellCount], uint8_t tile_index);
bool IsSolved(const uint8_t cells[kCellCount]);
bool IsValidPermutation(const uint8_t cells[kCellCount]);
uint8_t FindBlank(const uint8_t cells[kCellCount]);

}  // namespace QdNumberSlide
