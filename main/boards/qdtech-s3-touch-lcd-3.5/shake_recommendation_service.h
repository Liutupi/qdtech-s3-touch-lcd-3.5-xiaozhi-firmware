#pragma once

#include "lvgl.h"

#include <cstddef>
#include <cstdint>

namespace QdShakeRecommendation {

enum class Kind : uint8_t {
    MOVIE,
    BOOK,
};

enum class Status : uint8_t {
    OK,
    SD_UNAVAILABLE,
    INDEX_MISSING,
    INDEX_INVALID,
    IMAGE_MISSING,
    IMAGE_INVALID,
    NO_MEMORY,
};

// Both libraries use the same compact TSV schema, so only the selected row
// and cover are kept in memory. The full catalog remains on the SD card.
struct Record {
    char id[40]{};
    char title[96]{};
    char primary[128]{};
    char secondary[128]{};
    char meta[160]{};
    char rating[16]{};
    char summary[768]{};
    char image[96]{};
};

struct ImageFrame {
    lv_img_dsc_t dsc{};
    uint8_t* data = nullptr;
    size_t data_size = 0;
};

Status Draw(Kind kind, Record* record);
Status LoadImage(Kind kind, const Record& record, ImageFrame* frame);
void ReleaseImage(ImageFrame* frame);
const char* StatusText(Status status);
const char* KindTitle(Kind kind);

}  // namespace QdShakeRecommendation
