#include "divination_service.h"

#include "config.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_vfs_fat.h"
#include "jpeg_decoder.h"
#include "sdmmc_cmd.h"

namespace QdDivination {
namespace {

constexpr char kIndexPath[] = "/sdcard/shake_lab/divination/index.tsv";
constexpr char kBaseDirectory[] = "/sdcard/shake_lab/divination";
constexpr size_t kMaxImageInputBytes = 256 * 1024;
constexpr size_t kMaxImageOutputBytes = 180 * 180 * 2;

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

bool CopyField(const char* source, char* output, size_t output_size) {
    if (!source || !output || output_size == 0) return false;
    size_t written = 0;
    for (size_t i = 0; source[i] != '\0'; ++i) {
        char value = source[i];
        if (value == '\\' && source[i + 1] == 'n') {
            value = '\n';
            ++i;
        }
        if (written + 1 >= output_size) return false;
        output[written++] = value;
    }
    output[written] = '\0';
    return written > 0;
}

bool ParseRecord(char* line, Reading* reading) {
    if (!line || !reading || line[0] == '#' || line[0] == '\0') return false;
    char* fields[5]{};
    fields[0] = line;
    for (size_t i = 1; i < 5; ++i) {
        char* tab = strchr(fields[i - 1], '\t');
        if (!tab) return false;
        *tab = '\0';
        fields[i] = tab + 1;
    }
    char* pattern = strchr(fields[4], '\t');
    if (pattern) {
        *pattern = '\0';
        ++pattern;
    }
    const bool has_valid_pattern = pattern && strlen(pattern) == 6 &&
        strspn(pattern, "01") == 6;
    return CopyField(fields[0], reading->id, sizeof(reading->id)) &&
           CopyField(fields[1], reading->name, sizeof(reading->name)) &&
           CopyField(fields[2], reading->image, sizeof(reading->image)) &&
           CopyField(fields[3], reading->judgment, sizeof(reading->judgment)) &&
           CopyField(fields[4], reading->guidance, sizeof(reading->guidance)) &&
           (!has_valid_pattern || CopyField(pattern, reading->lines, sizeof(reading->lines)));
}

esp_jpeg_image_scale_t ChooseScale(uint16_t width, uint16_t height) {
    if (width <= 180 && height <= 180) return JPEG_IMAGE_SCALE_0;
    if (width / 2 <= 180 && height / 2 <= 180) return JPEG_IMAGE_SCALE_1_2;
    if (width / 4 <= 180 && height / 4 <= 180) return JPEG_IMAGE_SCALE_1_4;
    return JPEG_IMAGE_SCALE_1_8;
}

}  // namespace

Status Draw(Reading* reading) {
    if (!reading) return Status::INDEX_INVALID;
    if (!EnsureSdCardMounted()) return Status::SD_UNAVAILABLE;
    FILE* file = fopen(kIndexPath, "rb");
    if (!file) return Status::INDEX_MISSING;

    Reading candidate{};
    uint32_t valid_count = 0;
    char line[640];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        Reading parsed{};
        if (!ParseRecord(line, &parsed)) continue;
        ++valid_count;
        // Reservoir sampling means every valid record has equal probability
        // without retaining all 64 records in RAM.
        if (esp_random() % valid_count == 0) candidate = parsed;
    }
    fclose(file);
    if (valid_count == 0) return Status::INDEX_INVALID;
    *reading = candidate;
    return Status::OK;
}

Status LoadImage(const Reading& reading, ImageFrame* frame) {
    if (!frame || reading.image[0] == '\0') return Status::IMAGE_INVALID;
    if (!EnsureSdCardMounted()) return Status::SD_UNAVAILABLE;
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", kBaseDirectory, reading.image);
    struct stat st{};
    if (stat(path, &st) != 0 || st.st_size <= 0 ||
        static_cast<size_t>(st.st_size) > kMaxImageInputBytes) {
        return Status::IMAGE_MISSING;
    }
    FILE* file = fopen(path, "rb");
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
        heap_caps_free(input);
        return Status::IMAGE_INVALID;
    }
    config.out_scale = ChooseScale(info.width, info.height);
    esp_jpeg_image_output_t output_info{};
    err = esp_jpeg_get_image_info(&config, &output_info);
    if (err != ESP_OK || output_info.output_len == 0 ||
        output_info.output_len > kMaxImageOutputBytes) {
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
        heap_caps_free(output);
        return Status::IMAGE_INVALID;
    }
    ReleaseImage(frame);
    memset(&frame->dsc, 0, sizeof(frame->dsc));
    frame->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    frame->dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    // The service retains ownership and releases this buffer on the next
    // draw/exit.  Do not mark it ALLOCATED: LVGL's image cache would then free
    // the same external buffer during eviction, causing a double-free reset.
    frame->dsc.header.flags = 0;
    frame->dsc.header.w = output_info.width;
    frame->dsc.header.h = output_info.height;
    frame->dsc.header.stride = output_info.width * 2;
    frame->dsc.data_size = output_info.output_len;
    frame->dsc.data = output;
    frame->data = output;
    frame->data_size = output_info.output_len;
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
        case Status::INDEX_MISSING: return "摇卦资料未找到";
        case Status::INDEX_INVALID: return "摇卦资料格式无效";
        case Status::IMAGE_MISSING: return "该卦图片未找到";
        case Status::IMAGE_INVALID: return "该卦图片无法读取";
        case Status::NO_MEMORY: return "图片内存不足";
    }
    return "未知错误";
}

}  // namespace QdDivination
