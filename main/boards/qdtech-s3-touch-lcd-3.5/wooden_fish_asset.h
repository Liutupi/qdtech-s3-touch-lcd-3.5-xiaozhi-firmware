#pragma once

#include "lvgl.h"

#include <cstddef>
#include <cstdint>

namespace QdWoodenFish {

enum class Status : uint8_t {
    OK,
    SD_UNAVAILABLE,
    IMAGE_MISSING,
    IMAGE_INVALID,
    NO_MEMORY,
};

struct ImageFrame {
    lv_img_dsc_t dsc{};
    uint8_t* data = nullptr;
    size_t data_size = 0;
};

// Prefer the fixed 240x160 RGB565 file at
// /sdcard/wooden_fish/background.rgb565. The legacy JPEG remains a fallback.
// The caller owns the returned PSRAM frame until ReleaseImage().
Status LoadBackground(ImageFrame* frame);
void ReleaseImage(ImageFrame* frame);
const char* StatusText(Status status);

}  // namespace QdWoodenFish
