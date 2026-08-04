#include "wooden_fish_asset.h"

#include "config.h"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "jpeg_decoder.h"
#include "sdmmc_cmd.h"

namespace QdWoodenFish {
namespace {

constexpr char kTag[] = "WoodenFishAsset";
constexpr char kRawBackgroundPath[] = "/sdcard/wooden_fish/background.rgb565";
constexpr char kJpegBackgroundPath[] = "/sdcard/wooden_fish/background.jpg";
constexpr size_t kMaxInputBytes = 256 * 1024;
constexpr uint16_t kMaxWidth = 240;
constexpr uint16_t kMaxHeight = 160;
constexpr size_t kMaxOutputBytes = kMaxWidth * kMaxHeight * 2;

void AdoptRgb565Frame(ImageFrame* frame, uint8_t* data, uint16_t width,
                      uint16_t height, size_t data_size) {
    ReleaseImage(frame);
    memset(&frame->dsc, 0, sizeof(frame->dsc));
    frame->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    frame->dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    frame->dsc.header.flags = 0;
    frame->dsc.header.w = width;
    frame->dsc.header.h = height;
    frame->dsc.header.stride = width * 2;
    frame->dsc.data_size = data_size;
    frame->dsc.data = data;
    frame->data = data;
    frame->data_size = data_size;
}

Status LoadRawBackground(ImageFrame* frame) {
    struct stat st{};
    if (stat(kRawBackgroundPath, &st) != 0) {
        return Status::IMAGE_MISSING;
    }
    if (st.st_size != static_cast<off_t>(kMaxOutputBytes)) {
        ESP_LOGW(kTag, "raw image size invalid path=%s size=%ld expected=%u",
                 kRawBackgroundPath, static_cast<long>(st.st_size),
                 static_cast<unsigned>(kMaxOutputBytes));
        return Status::IMAGE_INVALID;
    }
    FILE* file = fopen(kRawBackgroundPath, "rb");
    if (!file) {
        ESP_LOGW(kTag, "raw image open failed path=%s", kRawBackgroundPath);
        return Status::IMAGE_INVALID;
    }
    auto* output = static_cast<uint8_t*>(
        heap_caps_malloc(kMaxOutputBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!output) {
        fclose(file);
        return Status::NO_MEMORY;
    }
    const size_t read = fread(output, 1, kMaxOutputBytes, file);
    fclose(file);
    if (read != kMaxOutputBytes) {
        ESP_LOGW(kTag, "raw image read failed path=%s read=%u expected=%u",
                 kRawBackgroundPath, static_cast<unsigned>(read),
                 static_cast<unsigned>(kMaxOutputBytes));
        heap_caps_free(output);
        return Status::IMAGE_INVALID;
    }
    AdoptRgb565Frame(frame, output, kMaxWidth, kMaxHeight, kMaxOutputBytes);
    ESP_LOGI(kTag, "raw RGB565 loaded path=%s size=%u dimensions=%ux%u",
             kRawBackgroundPath, static_cast<unsigned>(kMaxOutputBytes),
             static_cast<unsigned>(kMaxWidth),
             static_cast<unsigned>(kMaxHeight));
    return Status::OK;
}

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

esp_jpeg_image_scale_t ChooseScale(uint16_t width, uint16_t height) {
    if (width <= kMaxWidth && height <= kMaxHeight) return JPEG_IMAGE_SCALE_0;
    if (width / 2 <= kMaxWidth && height / 2 <= kMaxHeight) {
        return JPEG_IMAGE_SCALE_1_2;
    }
    if (width / 4 <= kMaxWidth && height / 4 <= kMaxHeight) {
        return JPEG_IMAGE_SCALE_1_4;
    }
    return JPEG_IMAGE_SCALE_1_8;
}

}  // namespace

Status LoadBackground(ImageFrame* frame) {
    if (!frame) return Status::IMAGE_INVALID;
    if (!EnsureSdCardMounted()) return Status::SD_UNAVAILABLE;

    const Status raw_status = LoadRawBackground(frame);
    if (raw_status != Status::IMAGE_MISSING) {
        return raw_status;
    }
    ESP_LOGI(kTag, "raw RGB565 missing; trying legacy JPEG path=%s",
             kJpegBackgroundPath);

    struct stat st{};
    if (stat(kJpegBackgroundPath, &st) != 0 || st.st_size <= 0 ||
        static_cast<size_t>(st.st_size) > kMaxInputBytes) {
        return Status::IMAGE_MISSING;
    }
    FILE* file = fopen(kJpegBackgroundPath, "rb");
    if (!file) return Status::IMAGE_MISSING;
    auto* input = static_cast<uint8_t*>(
        heap_caps_malloc(st.st_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!input) {
        fclose(file);
        return Status::NO_MEMORY;
    }
    const size_t read = fread(input, 1, st.st_size, file);
    fclose(file);
    if (read != static_cast<size_t>(st.st_size)) {
        heap_caps_free(input);
        return Status::IMAGE_INVALID;
    }

    esp_jpeg_image_cfg_t config = {
        .indata = input,
        .indata_size = static_cast<uint32_t>(st.st_size),
        .outbuf = nullptr,
        .outbuf_size = 0,
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale = JPEG_IMAGE_SCALE_0,
        .flags = {.swap_color_bytes = 0},
        .advanced = {.working_buffer = nullptr, .working_buffer_size = 0},
        .priv = {},
    };
    esp_jpeg_image_output_t info{};
    esp_err_t err = esp_jpeg_get_image_info(&config, &info);
    if (err != ESP_OK || info.width == 0 || info.height == 0) {
        ESP_LOGW(kTag, "legacy JPEG info failed path=%s err=%s dimensions=%ux%u",
                 kJpegBackgroundPath, esp_err_to_name(err),
                 static_cast<unsigned>(info.width),
                 static_cast<unsigned>(info.height));
        heap_caps_free(input);
        return Status::IMAGE_INVALID;
    }
    config.out_scale = ChooseScale(info.width, info.height);
    esp_jpeg_image_output_t output_info{};
    err = esp_jpeg_get_image_info(&config, &output_info);
    if (err != ESP_OK || output_info.output_len == 0 ||
        output_info.width > kMaxWidth || output_info.height > kMaxHeight ||
        output_info.output_len > kMaxOutputBytes) {
        ESP_LOGW(kTag,
                 "legacy JPEG scaled info failed path=%s err=%s dimensions=%ux%u bytes=%u",
                 kJpegBackgroundPath, esp_err_to_name(err),
                 static_cast<unsigned>(output_info.width),
                 static_cast<unsigned>(output_info.height),
                 static_cast<unsigned>(output_info.output_len));
        heap_caps_free(input);
        return Status::IMAGE_INVALID;
    }
    auto* output = static_cast<uint8_t*>(
        heap_caps_malloc(output_info.output_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!output) {
        heap_caps_free(input);
        return Status::NO_MEMORY;
    }
    config.outbuf = output;
    config.outbuf_size = output_info.output_len;
    err = esp_jpeg_decode(&config, &output_info);
    heap_caps_free(input);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "legacy JPEG decode failed path=%s err=%s",
                 kJpegBackgroundPath, esp_err_to_name(err));
        heap_caps_free(output);
        return Status::IMAGE_INVALID;
    }

    AdoptRgb565Frame(frame, output, output_info.width, output_info.height,
                     output_info.output_len);
    ESP_LOGI(kTag, "legacy JPEG decoded path=%s bytes=%u dimensions=%ux%u",
             kJpegBackgroundPath, static_cast<unsigned>(output_info.output_len),
             static_cast<unsigned>(output_info.width),
             static_cast<unsigned>(output_info.height));
    return Status::OK;
}

void ReleaseImage(ImageFrame* frame) {
    if (!frame) return;
    if (frame->data) heap_caps_free(frame->data);
    frame->data = nullptr;
    frame->data_size = 0;
    memset(&frame->dsc, 0, sizeof(frame->dsc));
}

const char* StatusText(Status status) {
    switch (status) {
        case Status::OK: return "读取完成";
        case Status::SD_UNAVAILABLE: return "未检测到 SD 卡";
        case Status::IMAGE_MISSING: return "木鱼图片未找到";
        case Status::IMAGE_INVALID: return "木鱼图片无法读取";
        case Status::NO_MEMORY: return "木鱼图片内存不足";
    }
    return "未知错误";
}

}  // namespace QdWoodenFish
