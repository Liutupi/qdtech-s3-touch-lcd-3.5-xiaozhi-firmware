#include "shake_recommendation_service.h"

#include "config.h"

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

namespace QdShakeRecommendation {
namespace {

constexpr char kMovieIndex[] = "/sdcard/movies/catalog.tsv";
constexpr char kBookIndex[] = "/sdcard/books/catalog.tsv";
constexpr char kMovieBase[] = "/sdcard/movies";
constexpr char kBookBase[] = "/sdcard/books";
constexpr size_t kMaxImageInputBytes = 256 * 1024;
// The SD recommendation covers are normalized to 220x300.  Keep that native
// resolution in PSRAM and let LVGL scale it once for the full-height preview;
// decoding at the old 110x150 half-size made the poster unnecessarily tiny.
constexpr size_t kMaxImageOutputBytes = 240 * 320 * 2;

const char* IndexPath(Kind kind) {
    return kind == Kind::MOVIE ? kMovieIndex : kBookIndex;
}

const char* BasePath(Kind kind) {
    return kind == Kind::MOVIE ? kMovieBase : kBookBase;
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

bool CopyField(const char* source, char* output, size_t output_size) {
    if (!source || !output || output_size == 0) return false;
    size_t written = 0;
    for (size_t i = 0; source[i] != '\0'; ++i) {
        char value = source[i];
        if (value == '\\' && source[i + 1] != '\0') {
            const char escaped = source[++i];
            value = escaped == 'n' ? '\n' : escaped;
        }
        if (written + 1 >= output_size) return false;
        output[written++] = value;
    }
    output[written] = '\0';
    return written > 0;
}

bool ParseRecord(char* line, Record* record) {
    if (!line || !record || line[0] == '#' || line[0] == '\0') return false;
    char* fields[8]{};
    fields[0] = line;
    for (size_t i = 1; i < 8; ++i) {
        char* tab = strchr(fields[i - 1], '\t');
        if (!tab) return false;
        *tab = '\0';
        fields[i] = tab + 1;
    }
    return CopyField(fields[0], record->id, sizeof(record->id)) &&
           CopyField(fields[1], record->title, sizeof(record->title)) &&
           CopyField(fields[2], record->primary, sizeof(record->primary)) &&
           CopyField(fields[3], record->secondary, sizeof(record->secondary)) &&
           CopyField(fields[4], record->meta, sizeof(record->meta)) &&
           CopyField(fields[5], record->rating, sizeof(record->rating)) &&
           CopyField(fields[6], record->summary, sizeof(record->summary)) &&
           CopyField(fields[7], record->image, sizeof(record->image));
}

esp_jpeg_image_scale_t ChooseScale(uint16_t width, uint16_t height) {
    if (width <= 240 && height <= 320) return JPEG_IMAGE_SCALE_0;
    if (width / 2 <= 240 && height / 2 <= 320) return JPEG_IMAGE_SCALE_1_2;
    if (width / 4 <= 240 && height / 4 <= 320) return JPEG_IMAGE_SCALE_1_4;
    return JPEG_IMAGE_SCALE_1_8;
}

}  // namespace

Status Draw(Kind kind, Record* record) {
    if (!record) return Status::INDEX_INVALID;
    if (!EnsureSdCardMounted()) return Status::SD_UNAVAILABLE;
    FILE* file = fopen(IndexPath(kind), "rb");
    if (!file) return Status::INDEX_MISSING;

    Record candidate{};
    uint32_t valid_count = 0;
    char line[1536];
    while (fgets(line, sizeof(line), file)) {
        if (!strchr(line, '\n') && !feof(file)) {
            int ch = 0;
            while ((ch = fgetc(file)) != '\n' && ch != EOF) {}
            continue;
        }
        line[strcspn(line, "\r\n")] = '\0';
        Record parsed{};
        if (!ParseRecord(line, &parsed)) continue;
        ++valid_count;
        if (esp_random() % valid_count == 0) candidate = parsed;
    }
    fclose(file);
    if (valid_count == 0) return Status::INDEX_INVALID;
    *record = candidate;
    return Status::OK;
}

Status LoadImage(Kind kind, const Record& record, ImageFrame* frame) {
    if (!frame || record.image[0] == '\0') return Status::IMAGE_INVALID;
    if (!EnsureSdCardMounted()) return Status::SD_UNAVAILABLE;
    char path[192];
    const int path_len = snprintf(path, sizeof(path), "%s/%s", BasePath(kind), record.image);
    if (path_len <= 0 || path_len >= static_cast<int>(sizeof(path))) return Status::IMAGE_INVALID;
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
        case Status::INDEX_MISSING: return "推荐资料未找到";
        case Status::INDEX_INVALID: return "推荐资料格式无效";
        case Status::IMAGE_MISSING: return "封面图片未找到";
        case Status::IMAGE_INVALID: return "封面图片无法读取";
        case Status::NO_MEMORY: return "封面内存不足";
    }
    return "未知错误";
}

const char* KindTitle(Kind kind) {
    return kind == Kind::MOVIE ? "摇摇电影" : "摇摇书籍";
}

}  // namespace QdShakeRecommendation
