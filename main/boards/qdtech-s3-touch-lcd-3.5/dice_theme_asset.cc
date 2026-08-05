#include "dice_theme_asset.h"

#include "sdkconfig.h"

#if defined(CONFIG_QDTECH_EXPERIMENT_PSEUDO3D_DICE) && \
    CONFIG_QDTECH_EXPERIMENT_PSEUDO3D_DICE

#include "config.h"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

namespace QdDiceTheme {
namespace {

constexpr char kTag[] = "DiceThemeAsset";
constexpr char kStagePath[] = "/sdcard/shake_lab/dice/stage.rgb565";
constexpr char kRollPath[] = "/sdcard/shake_lab/dice/roll.argb8888";
constexpr char kLandingPath[] = "/sdcard/shake_lab/dice/land.argb8888";
constexpr uint16_t kStageWidth = 480;
constexpr uint16_t kStageHeight = 320;
constexpr uint16_t kSpriteWidth = 96;
constexpr uint16_t kSpriteHeight = 96;
constexpr size_t kStageDataSize = kStageWidth * kStageHeight * 2;
constexpr size_t kSpriteFrameSize = kSpriteWidth * kSpriteHeight * 4;

bool TryMountSdCard(uint8_t width) {
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = 10000;
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = width;
    slot_config.clk = PHOTO_SDMMC_CLK_PIN;
    slot_config.cmd = PHOTO_SDMMC_CMD_PIN;
    slot_config.d0 = PHOTO_SDMMC_D0_PIN;
    if (width >= 4) {
        slot_config.d1 = PHOTO_SDMMC_D1_PIN;
        slot_config.d2 = PHOTO_SDMMC_D2_PIN;
        slot_config.d3 = PHOTO_SDMMC_D3_PIN;
    } else {
        slot_config.d1 = GPIO_NUM_NC;
        slot_config.d2 = GPIO_NUM_NC;
        slot_config.d3 = GPIO_NUM_NC;
    }
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 0,
    };
    sdmmc_card_t* card = nullptr;
    return esp_vfs_fat_sdmmc_mount(
               "/sdcard", &host, &slot_config, &mount_config, &card) == ESP_OK;
}

bool EnsureSdCardMounted() {
    DIR* directory = opendir("/sdcard");
    if (directory) {
        closedir(directory);
        return true;
    }
    if (TryMountSdCard(PHOTO_SDMMC_BUS_WIDTH)) return true;
    return PHOTO_SDMMC_BUS_WIDTH != 1 && TryMountSdCard(1);
}

Status ReadAsset(const char* path, size_t expected_size, uint8_t** output) {
    if (!path || !output) return Status::IMAGE_INVALID;
    *output = nullptr;
    if (!EnsureSdCardMounted()) return Status::SD_UNAVAILABLE;

    struct stat st{};
    if (stat(path, &st) != 0) return Status::IMAGE_MISSING;
    if (st.st_size != static_cast<off_t>(expected_size)) {
        ESP_LOGW(kTag, "asset size invalid path=%s size=%ld expected=%u",
                 path, static_cast<long>(st.st_size),
                 static_cast<unsigned>(expected_size));
        return Status::IMAGE_INVALID;
    }

    FILE* file = fopen(path, "rb");
    if (!file) return Status::IMAGE_INVALID;
    auto* data = static_cast<uint8_t*>(
        heap_caps_malloc(expected_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!data) {
        fclose(file);
        return Status::NO_MEMORY;
    }
    const size_t read = fread(data, 1, expected_size, file);
    fclose(file);
    if (read != expected_size) {
        heap_caps_free(data);
        ESP_LOGW(kTag, "asset read failed path=%s read=%u expected=%u",
                 path, static_cast<unsigned>(read),
                 static_cast<unsigned>(expected_size));
        return Status::IMAGE_INVALID;
    }
    *output = data;
    return Status::OK;
}

template <size_t FrameCount>
Status LoadAtlas(const char* path, ImageAtlas<FrameCount>* atlas) {
    if (!atlas) return Status::IMAGE_INVALID;
    if (atlas->data) heap_caps_free(atlas->data);
    memset(atlas, 0, sizeof(*atlas));
    constexpr size_t data_size = kSpriteFrameSize * FrameCount;
    const Status status = ReadAsset(path, data_size, &atlas->data);
    if (status != Status::OK) return status;

    atlas->data_size = data_size;
    for (size_t index = 0; index < FrameCount; ++index) {
        lv_img_dsc_t& frame = atlas->frames[index];
        memset(&frame, 0, sizeof(frame));
        frame.header.magic = LV_IMAGE_HEADER_MAGIC;
        frame.header.cf = LV_COLOR_FORMAT_ARGB8888;
        frame.header.w = kSpriteWidth;
        frame.header.h = kSpriteHeight;
        frame.header.stride = kSpriteWidth * 4;
        frame.data_size = kSpriteFrameSize;
        frame.data = atlas->data + index * kSpriteFrameSize;
    }
    ESP_LOGI(kTag, "atlas loaded path=%s bytes=%u frames=%u dimensions=%ux%u",
             path, static_cast<unsigned>(data_size),
             static_cast<unsigned>(FrameCount),
             static_cast<unsigned>(kSpriteWidth),
             static_cast<unsigned>(kSpriteHeight));
    return Status::OK;
}

template <size_t FrameCount>
void ReleaseAtlasData(ImageAtlas<FrameCount>* atlas) {
    if (!atlas) return;
    if (atlas->data) heap_caps_free(atlas->data);
    memset(atlas, 0, sizeof(*atlas));
}

}  // namespace

Status LoadStage(ImageFrame* frame) {
    if (!frame) return Status::IMAGE_INVALID;
    ReleaseImage(frame);
    uint8_t* data = nullptr;
    const Status status = ReadAsset(kStagePath, kStageDataSize, &data);
    if (status != Status::OK) return status;

    memset(&frame->dsc, 0, sizeof(frame->dsc));
    frame->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    frame->dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    frame->dsc.header.w = kStageWidth;
    frame->dsc.header.h = kStageHeight;
    frame->dsc.header.stride = kStageWidth * 2;
    frame->dsc.data_size = kStageDataSize;
    frame->dsc.data = data;
    frame->data = data;
    frame->data_size = kStageDataSize;
    ESP_LOGI(kTag, "stage loaded path=%s bytes=%u dimensions=%ux%u",
             kStagePath, static_cast<unsigned>(kStageDataSize),
             static_cast<unsigned>(kStageWidth),
             static_cast<unsigned>(kStageHeight));
    return Status::OK;
}

Status LoadRollAtlas(RollAtlas* atlas) {
    return LoadAtlas(kRollPath, atlas);
}

Status LoadLandingAtlas(LandingAtlas* atlas) {
    return LoadAtlas(kLandingPath, atlas);
}

void ReleaseImage(ImageFrame* frame) {
    if (!frame) return;
    if (frame->data) heap_caps_free(frame->data);
    frame->data = nullptr;
    frame->data_size = 0;
    memset(&frame->dsc, 0, sizeof(frame->dsc));
}

void ReleaseAtlas(RollAtlas* atlas) {
    ReleaseAtlasData(atlas);
}

void ReleaseAtlas(LandingAtlas* atlas) {
    ReleaseAtlasData(atlas);
}

const char* StatusText(Status status) {
    switch (status) {
        case Status::OK: return "骰子舞台已加载";
        case Status::SD_UNAVAILABLE: return "未检测到 SD 卡";
        case Status::IMAGE_MISSING: return "骰子舞台资源未找到";
        case Status::IMAGE_INVALID: return "骰子舞台资源无效";
        case Status::NO_MEMORY: return "骰子舞台内存不足";
    }
    return "未知错误";
}

}  // namespace QdDiceTheme

#endif  // CONFIG_QDTECH_EXPERIMENT_PSEUDO3D_DICE
