#include "number_slide_logic.h"

#include <cassert>
#include <cstdint>

int main() {
    uint8_t cells[QdNumberSlide::kCellCount]{};
    QdNumberSlide::ResetSolved(cells);
    assert(QdNumberSlide::IsSolved(cells));
    assert(QdNumberSlide::IsValidPermutation(cells));
    assert(!QdNumberSlide::MoveTile(cells, 0));
    assert(QdNumberSlide::MoveTile(cells, 14));
    assert(!QdNumberSlide::IsSolved(cells));
    assert(QdNumberSlide::MoveTile(cells, 15));
    assert(QdNumberSlide::IsSolved(cells));

    for (uint32_t seed = 1; seed <= 10000; ++seed) {
        QdNumberSlide::Shuffle(cells, seed, 160);
        assert(QdNumberSlide::IsValidPermutation(cells));
        assert(!QdNumberSlide::IsSolved(cells));
        const uint8_t blank = QdNumberSlide::FindBlank(cells);
        assert(blank < QdNumberSlide::kCellCount);
        bool moved = false;
        for (uint8_t tile = 0; tile < QdNumberSlide::kCellCount; ++tile) {
            uint8_t copy[QdNumberSlide::kCellCount];
            for (uint8_t i = 0; i < QdNumberSlide::kCellCount; ++i) copy[i] = cells[i];
            if (QdNumberSlide::MoveTile(copy, tile)) {
                assert(QdNumberSlide::IsValidPermutation(copy));
                moved = true;
            }
        }
        assert(moved);
    }
    return 0;
}
