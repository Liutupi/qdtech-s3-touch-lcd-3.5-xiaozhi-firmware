#include "freecell_game.h"

#include <algorithm>
#include <cstring>

namespace QdFreecell {

uint8_t Game::Rank(uint8_t card) {
    return card % 13;
}

uint8_t Game::Suit(uint8_t card) {
    return card / 13;
}

bool Game::IsRed(uint8_t card) {
    const uint8_t suit = Suit(card);
    return suit == 0 || suit == 2;
}

bool Game::IsDescendingAlternating(uint8_t upper, uint8_t lower) {
    return IsRed(upper) != IsRed(lower) && Rank(upper) == Rank(lower) + 1;
}

void Game::NewDeal(uint32_t deal_number) {
    // Classic Microsoft-compatible selection shuffle. Deal 11982 is the one
    // known unsolvable game in the original 1..32000 set, so never offer it.
    deal_number_ = deal_number == 0 ? 1 : deal_number;
    if (deal_number_ == 11982) deal_number_ = 1;

    // Windows starts from rank-major C, D, H, S order. Map that order into
    // this UI's H, C, D, S encoding so the numbered layouts remain exact.
    static constexpr uint8_t suit_map[4] = {1, 2, 0, 3};
    uint8_t pool[kDeckSize];
    for (uint8_t rank = 0; rank < 13; ++rank) {
        for (uint8_t source_suit = 0; source_suit < 4; ++source_suit) {
            pool[rank * 4 + source_suit] = suit_map[source_suit] * 13 + rank;
        }
    }
    uint8_t remaining = kDeckSize;
    uint32_t random_state = deal_number_;

    memset(tableau_, 0, sizeof(tableau_));
    memset(tableau_count_, 0, sizeof(tableau_count_));
    memset(cells_, kEmptyCard, sizeof(cells_));
    memset(foundations_, 0, sizeof(foundations_));

    for (uint8_t i = 0; i < kDeckSize; ++i) {
        random_state = (random_state * 214013u + 2531011u) & 0x7fffffffu;
        const uint16_t value = static_cast<uint16_t>((random_state >> 16) & 0x7fffu);
        const uint8_t selected = static_cast<uint8_t>(value % remaining);
        const uint8_t card = pool[selected];
        pool[selected] = pool[remaining - 1];
        --remaining;
        const uint8_t column = i % kTableauColumns;
        tableau_[column][tableau_count_[column]++] = card;
    }

    selection_ = {};
    history_head_ = 0;
    history_count_ = 0;
    moves_ = 0;
    won_ = false;
}

bool Game::SelectTableau(uint8_t column, uint8_t index) {
    if (column >= kTableauColumns || index >= tableau_count_[column]) return false;
    selection_ = {SourceKind::TABLEAU, column, index};
    if (!IsSelectedRunValid()) {
        selection_ = {};
        return false;
    }
    return true;
}

bool Game::SelectFreeCell(uint8_t cell) {
    if (cell >= kFreeCells || cells_[cell] == kEmptyCard) return false;
    selection_ = {SourceKind::FREE_CELL, cell, 0};
    return true;
}

bool Game::SelectFoundation(uint8_t foundation) {
    if (foundation >= kFoundations || foundations_[foundation] == 0) return false;
    selection_ = {SourceKind::FOUNDATION, foundation, 0};
    return true;
}

void Game::ClearSelection() {
    selection_ = {};
}

bool Game::IsSelectedRunValid() const {
    if (selection_.kind != SourceKind::TABLEAU ||
        selection_.slot >= kTableauColumns ||
        selection_.index >= tableau_count_[selection_.slot]) {
        return false;
    }
    const uint8_t count = tableau_count_[selection_.slot];
    for (uint8_t i = selection_.index; i + 1 < count; ++i) {
        if (!IsDescendingAlternating(tableau_[selection_.slot][i],
                                     tableau_[selection_.slot][i + 1])) {
            return false;
        }
    }
    return true;
}

bool Game::GetSelectedCard(uint8_t* card, uint8_t* count) const {
    if (!card || !count) return false;
    switch (selection_.kind) {
        case SourceKind::TABLEAU:
            if (!IsSelectedRunValid()) return false;
            *card = tableau_[selection_.slot][selection_.index];
            *count = tableau_count_[selection_.slot] - selection_.index;
            return true;
        case SourceKind::FREE_CELL:
            if (selection_.slot >= kFreeCells || cells_[selection_.slot] == kEmptyCard) return false;
            *card = cells_[selection_.slot];
            *count = 1;
            return true;
        case SourceKind::FOUNDATION:
            if (selection_.slot >= kFoundations || foundations_[selection_.slot] == 0) return false;
            *card = static_cast<uint8_t>(selection_.slot * 13 + foundations_[selection_.slot] - 1);
            *count = 1;
            return true;
        case SourceKind::NONE:
            return false;
    }
    return false;
}

uint8_t Game::MovableCapacity(uint8_t destination) const {
    if (destination >= kTableauColumns) return 0;
    uint16_t capacity = EmptyFreeCellCount() + 1;
    for (uint8_t column = 0; column < kTableauColumns; ++column) {
        if (column != destination && tableau_count_[column] == 0) {
            capacity = std::min<uint16_t>(kDeckSize, capacity * 2);
        }
    }
    return static_cast<uint8_t>(capacity);
}

MoveResult Game::MoveToTableau(uint8_t destination) {
    if (!HasSelection()) return MoveResult::NO_SELECTION;
    if (destination >= kTableauColumns) return MoveResult::INVALID_SELECTION;
    if (selection_.kind == SourceKind::TABLEAU && selection_.slot == destination) {
        return MoveResult::SAME_SOURCE;
    }

    uint8_t card = 0;
    uint8_t count = 0;
    if (!GetSelectedCard(&card, &count)) return MoveResult::INVALID_SELECTION;
    if (count > MovableCapacity(destination)) return MoveResult::TOO_MANY_CARDS;
    if (tableau_count_[destination] + count > kDeckSize) return MoveResult::COLUMN_FULL;
    if (tableau_count_[destination] > 0) {
        const uint8_t target = tableau_[destination][tableau_count_[destination] - 1];
        if (!IsDescendingAlternating(target, card)) return MoveResult::WRONG_TABLEAU_ORDER;
    }

    SaveUndo();
    if (selection_.kind == SourceKind::TABLEAU) {
        memcpy(&tableau_[destination][tableau_count_[destination]],
               &tableau_[selection_.slot][selection_.index], count);
    } else {
        tableau_[destination][tableau_count_[destination]] = card;
    }
    tableau_count_[destination] += count;
    RemoveSelection();
    FinishMove();
    return MoveResult::OK;
}

MoveResult Game::MoveToFreeCell(uint8_t destination) {
    if (!HasSelection()) return MoveResult::NO_SELECTION;
    if (destination >= kFreeCells) return MoveResult::INVALID_SELECTION;
    if (selection_.kind == SourceKind::FREE_CELL && selection_.slot == destination) {
        return MoveResult::SAME_SOURCE;
    }
    if (cells_[destination] != kEmptyCard) return MoveResult::DESTINATION_OCCUPIED;

    uint8_t card = 0;
    uint8_t count = 0;
    if (!GetSelectedCard(&card, &count)) return MoveResult::INVALID_SELECTION;
    if (count != 1) return MoveResult::TOO_MANY_CARDS;

    SaveUndo();
    cells_[destination] = card;
    RemoveSelection();
    FinishMove();
    return MoveResult::OK;
}

MoveResult Game::MoveToFoundation(uint8_t destination) {
    if (!HasSelection()) return MoveResult::NO_SELECTION;
    if (destination >= kFoundations) return MoveResult::INVALID_SELECTION;
    if (selection_.kind == SourceKind::FOUNDATION && selection_.slot == destination) {
        return MoveResult::SAME_SOURCE;
    }

    uint8_t card = 0;
    uint8_t count = 0;
    if (!GetSelectedCard(&card, &count)) return MoveResult::INVALID_SELECTION;
    if (count != 1 || Suit(card) != destination || Rank(card) != foundations_[destination]) {
        return MoveResult::WRONG_FOUNDATION;
    }

    SaveUndo();
    ++foundations_[destination];
    RemoveSelection();
    FinishMove();
    return MoveResult::OK;
}

void Game::RemoveSelection() {
    switch (selection_.kind) {
        case SourceKind::TABLEAU:
            tableau_count_[selection_.slot] = selection_.index;
            break;
        case SourceKind::FREE_CELL:
            cells_[selection_.slot] = kEmptyCard;
            break;
        case SourceKind::FOUNDATION:
            --foundations_[selection_.slot];
            break;
        case SourceKind::NONE:
            break;
    }
}

void Game::SaveUndo() {
    Snapshot& snapshot = history_[history_head_];
    memcpy(snapshot.tableau, tableau_, sizeof(tableau_));
    memcpy(snapshot.tableau_count, tableau_count_, sizeof(tableau_count_));
    memcpy(snapshot.cells, cells_, sizeof(cells_));
    memcpy(snapshot.foundations, foundations_, sizeof(foundations_));
    snapshot.moves = moves_;
    history_head_ = (history_head_ + 1) % kUndoDepth;
    if (history_count_ < kUndoDepth) ++history_count_;
}

void Game::FinishMove() {
    ++moves_;
    selection_ = {};
    uint16_t foundation_cards = 0;
    for (uint8_t count : foundations_) foundation_cards += count;
    won_ = foundation_cards == kDeckSize;
}

bool Game::Undo() {
    if (history_count_ == 0) return false;
    history_head_ = (history_head_ + kUndoDepth - 1) % kUndoDepth;
    const Snapshot& snapshot = history_[history_head_];
    memcpy(tableau_, snapshot.tableau, sizeof(tableau_));
    memcpy(tableau_count_, snapshot.tableau_count, sizeof(tableau_count_));
    memcpy(cells_, snapshot.cells, sizeof(cells_));
    memcpy(foundations_, snapshot.foundations, sizeof(foundations_));
    moves_ = snapshot.moves;
    --history_count_;
    selection_ = {};
    won_ = false;
    return true;
}

uint8_t Game::TableauCount(uint8_t column) const {
    return column < kTableauColumns ? tableau_count_[column] : 0;
}

uint8_t Game::TableauCard(uint8_t column, uint8_t index) const {
    return column < kTableauColumns && index < tableau_count_[column]
        ? tableau_[column][index] : kEmptyCard;
}

uint8_t Game::FreeCellCard(uint8_t cell) const {
    return cell < kFreeCells ? cells_[cell] : kEmptyCard;
}

uint8_t Game::FoundationCount(uint8_t foundation) const {
    return foundation < kFoundations ? foundations_[foundation] : 0;
}

uint8_t Game::SelectedCount() const {
    uint8_t card = 0;
    uint8_t count = 0;
    return GetSelectedCard(&card, &count) ? count : 0;
}

uint8_t Game::EmptyFreeCellCount() const {
    uint8_t count = 0;
    for (uint8_t card : cells_) if (card == kEmptyCard) ++count;
    return count;
}

uint8_t Game::EmptyTableauCount() const {
    uint8_t count = 0;
    for (uint8_t cards : tableau_count_) if (cards == 0) ++count;
    return count;
}

bool Game::IsSelected(SourceKind kind, uint8_t slot, uint8_t index) const {
    if (selection_.kind != kind || selection_.slot != slot) return false;
    return kind != SourceKind::TABLEAU || index >= selection_.index;
}

bool Game::HasSelection() const { return selection_.kind != SourceKind::NONE; }
bool Game::CanUndo() const { return history_count_ > 0; }
bool Game::Won() const { return won_; }
uint16_t Game::Moves() const { return moves_; }
uint32_t Game::DealNumber() const { return deal_number_; }
const Selection& Game::CurrentSelection() const { return selection_; }

}  // namespace QdFreecell
