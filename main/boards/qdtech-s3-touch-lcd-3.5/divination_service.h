#pragma once

#include "lvgl.h"

#include <cstddef>
#include <cstdint>

namespace QdDivination {

enum class Status : uint8_t {
    OK,
    SD_UNAVAILABLE,
    INDEX_MISSING,
    INDEX_INVALID,
    IMAGE_MISSING,
    IMAGE_INVALID,
    NO_MEMORY,
};

// Every field in this record is read from /sdcard/shake_lab/divination/index.tsv.
// This deliberately keeps divination copy and artwork out of the firmware image.
struct Reading {
    char id[20]{};
    char name[32]{};
    char image[64]{};
    char judgment[192]{};
    char guidance[256]{};
    // Six lines are stored bottom-to-top: 1 = solid (阳), 0 = broken (阴).
    char lines[8]{};
};

struct ImageFrame {
    lv_img_dsc_t dsc{};
    uint8_t* data = nullptr;
    size_t data_size = 0;
};

Status Draw(Reading* reading);
Status LoadImage(const Reading& reading, ImageFrame* frame);
void ReleaseImage(ImageFrame* frame);
const char* StatusText(Status status);

}  // namespace QdDivination
