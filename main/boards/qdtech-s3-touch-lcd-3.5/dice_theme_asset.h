#pragma once

#include "lvgl.h"

#include <cstddef>
#include <cstdint>

namespace QdDiceTheme {

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

constexpr uint8_t kRollFrameCount = 12;
constexpr uint8_t kLandingFrameCount = 6;

template <size_t FrameCount>
struct ImageAtlas {
    lv_img_dsc_t frames[FrameCount]{};
    uint8_t* data = nullptr;
    size_t data_size = 0;
};

using RollAtlas = ImageAtlas<kRollFrameCount>;
using LandingAtlas = ImageAtlas<kLandingFrameCount>;

// Loads the optional 480x320 RGB565 stage and two 96x96 ARGB8888 sprite
// atlases from /sdcard/shake_lab/dice into PSRAM. Geometry, bevels, projected
// pips and shadows are pre-rendered offline; runtime only swaps frame sources.
Status LoadStage(ImageFrame* frame);
Status LoadRollAtlas(RollAtlas* atlas);
Status LoadLandingAtlas(LandingAtlas* atlas);
void ReleaseImage(ImageFrame* frame);
void ReleaseAtlas(RollAtlas* atlas);
void ReleaseAtlas(LandingAtlas* atlas);
const char* StatusText(Status status);

}  // namespace QdDiceTheme
