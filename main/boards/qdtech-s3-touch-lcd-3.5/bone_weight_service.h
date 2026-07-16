#pragma once

#include <cstddef>
#include <cstdint>

namespace QdBoneWeight {

enum class Status : uint8_t {
    OK,
    INVALID_DATE,
    OUT_OF_RANGE,
    SD_UNAVAILABLE,
    DATA_MISSING,
    DATA_INVALID,
    SONG_MISSING,
    INTERPRETATION_MISSING,
    DETAIL_MISSING,
};

enum class TextKind : uint8_t {
    SONG,
    INTERPRETATION,
};

struct Input {
    int year;
    int month;
    int day;
    int hour;
};

struct Result {
    uint8_t lunar_year_cycle;
    uint8_t lunar_month;
    uint8_t lunar_day;
    uint8_t shichen;
    bool lunar_leap_month;
    uint8_t year_weight;
    uint8_t month_weight;
    uint8_t day_weight;
    uint8_t hour_weight;
    uint8_t total_weight;
    char song[256];
};

Status Calculate(const Input& input, Result* result);
Status LoadText(uint8_t total_weight, TextKind kind, char* output, size_t output_size);
Status LoadReaderPage(uint8_t total_weight, uint8_t page,
                      char* output, size_t output_size);
const char* ReaderPageTitle(uint8_t page);
constexpr uint8_t ReaderPageCount() {
    return 6;
}
const char* StatusText(Status status);

}  // namespace QdBoneWeight
