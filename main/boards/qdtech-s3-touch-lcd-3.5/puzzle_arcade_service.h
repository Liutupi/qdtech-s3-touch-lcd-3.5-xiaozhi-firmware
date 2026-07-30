#pragma once

#include "sdkconfig.h"

#if defined(CONFIG_QDTECH_EXPERIMENT_PUZZLE_ARCADE) && \
    CONFIG_QDTECH_EXPERIMENT_PUZZLE_ARCADE

#include "lvgl.h"

#include <cstddef>
#include <cstdint>

namespace QdPuzzleArcade {

enum class Game : uint8_t {
    SUDOKU,
    CODE_LOCK,
    SOKOBAN,
    MATCH3,
    MOTION_MAZE,
};

enum class Status : uint8_t {
    OK,
    SD_UNAVAILABLE,
    FILE_MISSING,
    INVALID_DATA,
    IMAGE_MISSING,
    IMAGE_INVALID,
    NO_MEMORY,
};

struct SudokuPuzzle {
    char id[16]{};
    char difficulty[16]{};
    char puzzle[82]{};
    char solution[82]{};
};

struct LockChallenge {
    char id[16]{};
    char difficulty[16]{};
    char code[5]{};
};

struct Match3Level {
    char id[16]{};
    char name[32]{};
    uint16_t target_score = 0;
    uint8_t moves = 0;
};

constexpr uint8_t kSokobanMaxWidth = 12;
constexpr uint8_t kSokobanMaxHeight = 10;

struct SokobanLevel {
    char id[16]{};
    char name[32]{};
    uint8_t width = 0;
    uint8_t height = 0;
    char cells[kSokobanMaxWidth * kSokobanMaxHeight]{};
};

constexpr uint8_t kMazeMaxWidth = 18;
constexpr uint8_t kMazeMaxHeight = 12;

struct MazeLevel {
    char id[16]{};
    char name[32]{};
    uint8_t width = 0;
    uint8_t height = 0;
    char cells[kMazeMaxWidth * kMazeMaxHeight]{};
};

struct ImageFrame {
    lv_img_dsc_t dsc{};
    uint8_t* data = nullptr;
    size_t data_size = 0;
};

Status LoadRandomSudoku(SudokuPuzzle* puzzle);
Status LoadRandomLockChallenge(LockChallenge* challenge);
Status LoadRandomMatch3Level(Match3Level* level);
Status LoadSokobanLevel(uint16_t ordinal, SokobanLevel* level, uint16_t* level_count = nullptr);
Status LoadMazeLevel(uint16_t ordinal, MazeLevel* level, uint16_t* level_count = nullptr);
Status LoadCover(Game game, ImageFrame* frame);
void ReleaseImage(ImageFrame* frame);
const char* GameId(Game game);
const char* StatusText(Status status);

}  // namespace QdPuzzleArcade

#endif
