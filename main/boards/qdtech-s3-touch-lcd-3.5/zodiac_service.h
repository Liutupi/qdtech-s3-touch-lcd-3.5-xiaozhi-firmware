#pragma once

#include "lvgl.h"

#include <cstddef>
#include <cstdint>

namespace QdZodiac {

enum class Status : uint8_t {
    OK,
    INVALID_DATE,
    OUT_OF_RANGE,
    SD_UNAVAILABLE,
    DETAIL_MISSING,
    DETAIL_INVALID,
    IMAGE_MISSING,
    IMAGE_INVALID,
    NO_MEMORY,
};

enum class Sign : uint8_t {
    ARIES,
    TAURUS,
    GEMINI,
    CANCER,
    LEO,
    VIRGO,
    LIBRA,
    SCORPIO,
    SAGITTARIUS,
    CAPRICORN,
    AQUARIUS,
    PISCES,
};

struct Input {
    int year;
    int month;
    int day;
};

struct Result {
    Sign sign;
    const char* id;
    const char* name;
    const char* english_name;
    const char* date_range;
    const char* element;
    const char* modality;
    const char* ruling_planet;
};

struct ImageFrame {
    lv_img_dsc_t dsc{};
    uint8_t* data = nullptr;
    size_t data_size = 0;
};

Status Calculate(const Input& input, Result* result);
Status LoadReaderPage(Sign sign, uint8_t page,
                      char* title, size_t title_size,
                      char* text, size_t text_size);
Status LoadImage(Sign sign, ImageFrame* frame);
void ReleaseImage(ImageFrame* frame);
const Result& Metadata(Sign sign);
constexpr uint8_t ReaderPageCount() {
    return 6;
}
const char* StatusText(Status status);

}  // namespace QdZodiac
