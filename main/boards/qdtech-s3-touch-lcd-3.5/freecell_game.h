#pragma once

#include <cstddef>
#include <cstdint>

namespace QdFreecell {

constexpr uint8_t kEmptyCard = 0xff;
constexpr uint8_t kTableauColumns = 8;
constexpr uint8_t kFreeCells = 4;
constexpr uint8_t kFoundations = 4;
constexpr uint8_t kDeckSize = 52;

enum class SourceKind : uint8_t {
    NONE,
    TABLEAU,
    FREE_CELL,
    FOUNDATION,
};

enum class MoveResult : uint8_t {
    OK,
    NO_SELECTION,
    INVALID_SELECTION,
    SAME_SOURCE,
    DESTINATION_OCCUPIED,
    WRONG_TABLEAU_ORDER,
    WRONG_FOUNDATION,
    TOO_MANY_CARDS,
    COLUMN_FULL,
};

struct Selection {
    SourceKind kind = SourceKind::NONE;
    uint8_t slot = 0;
    uint8_t index = 0;
};

class Game {
public:
    void NewDeal(uint32_t deal_number);

    bool SelectTableau(uint8_t column, uint8_t index);
    bool SelectFreeCell(uint8_t cell);
    bool SelectFoundation(uint8_t foundation);
    void ClearSelection();

    MoveResult MoveToTableau(uint8_t destination);
    MoveResult MoveToFreeCell(uint8_t destination);
    MoveResult MoveToFoundation(uint8_t destination);
    bool Undo();

    uint8_t TableauCount(uint8_t column) const;
    uint8_t TableauCard(uint8_t column, uint8_t index) const;
    uint8_t FreeCellCard(uint8_t cell) const;
    uint8_t FoundationCount(uint8_t foundation) const;
    uint8_t SelectedCount() const;
    uint8_t EmptyFreeCellCount() const;
    uint8_t EmptyTableauCount() const;
    uint8_t MovableCapacity(uint8_t destination) const;
    bool IsSelected(SourceKind kind, uint8_t slot, uint8_t index = 0) const;
    bool HasSelection() const;
    bool CanUndo() const;
    bool Won() const;
    uint16_t Moves() const;
    uint32_t DealNumber() const;
    const Selection& CurrentSelection() const;

    static uint8_t Rank(uint8_t card);
    static uint8_t Suit(uint8_t card);
    static bool IsRed(uint8_t card);
    static bool IsDescendingAlternating(uint8_t upper, uint8_t lower);

private:
    static constexpr uint8_t kUndoDepth = 24;

    struct Snapshot {
        uint8_t tableau[kTableauColumns][kDeckSize]{};
        uint8_t tableau_count[kTableauColumns]{};
        uint8_t cells[kFreeCells]{};
        uint8_t foundations[kFoundations]{};
        uint16_t moves = 0;
    };

    uint8_t tableau_[kTableauColumns][kDeckSize]{};
    uint8_t tableau_count_[kTableauColumns]{};
    uint8_t cells_[kFreeCells]{};
    uint8_t foundations_[kFoundations]{};
    Selection selection_{};
    Snapshot history_[kUndoDepth]{};
    uint8_t history_head_ = 0;
    uint8_t history_count_ = 0;
    uint16_t moves_ = 0;
    uint32_t deal_number_ = 1;
    bool won_ = false;

    bool IsSelectedRunValid() const;
    bool GetSelectedCard(uint8_t* card, uint8_t* count) const;
    void RemoveSelection();
    void SaveUndo();
    void FinishMove();
};

}  // namespace QdFreecell
