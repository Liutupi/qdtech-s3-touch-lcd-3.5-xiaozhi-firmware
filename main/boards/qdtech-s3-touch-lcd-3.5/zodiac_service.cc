#include "zodiac_service.h"

#include "sdkconfig.h"

#if defined(CONFIG_QDTECH_EXPERIMENT_CALENDAR_ZODIAC) && \
    CONFIG_QDTECH_EXPERIMENT_CALENDAR_ZODIAC

#include "config.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "jpeg_decoder.h"
#include "sdmmc_cmd.h"

namespace QdZodiac {
namespace {

constexpr char kDetailsPath[] = "/sdcard/calendar/zodiac/details.tsv";
constexpr char kImageDirectory[] = "/sdcard/calendar/zodiac/images";
constexpr size_t kMaxImageInputBytes = 256 * 1024;
constexpr size_t kMaxImageOutputBytes = 180 * 180 * 2;
constexpr char kTag[] = "QdZodiac";

constexpr Result kMetadata[] = {
    {Sign::ARIES, "aries", "白羊座", "ARIES", "03/21 - 04/19", "火象", "开创", "火星"},
    {Sign::TAURUS, "taurus", "金牛座", "TAURUS", "04/20 - 05/20", "土象", "固定", "金星"},
    {Sign::GEMINI, "gemini", "双子座", "GEMINI", "05/21 - 06/21", "风象", "变动", "水星"},
    {Sign::CANCER, "cancer", "巨蟹座", "CANCER", "06/22 - 07/22", "水象", "开创", "月亮"},
    {Sign::LEO, "leo", "狮子座", "LEO", "07/23 - 08/22", "火象", "固定", "太阳"},
    {Sign::VIRGO, "virgo", "处女座", "VIRGO", "08/23 - 09/22", "土象", "变动", "水星"},
    {Sign::LIBRA, "libra", "天秤座", "LIBRA", "09/23 - 10/23", "风象", "开创", "金星"},
    {Sign::SCORPIO, "scorpio", "天蝎座", "SCORPIO", "10/24 - 11/22", "水象", "固定", "冥王星"},
    {Sign::SAGITTARIUS, "sagittarius", "射手座", "SAGITTARIUS", "11/23 - 12/21", "火象", "变动", "木星"},
    {Sign::CAPRICORN, "capricorn", "摩羯座", "CAPRICORN", "12/22 - 01/19", "土象", "开创", "土星"},
    {Sign::AQUARIUS, "aquarius", "水瓶座", "AQUARIUS", "01/20 - 02/18", "风象", "固定", "天王星"},
    {Sign::PISCES, "pisces", "双鱼座", "PISCES", "02/19 - 03/20", "水象", "变动", "海王星"},
};

static_assert(sizeof(kMetadata) / sizeof(kMetadata[0]) == 12);

bool IsLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int DaysInMonth(int year, int month) {
    static constexpr uint8_t kDays[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    if (month < 1 || month > 12) {
        return 0;
    }
    return kDays[month - 1] + (month == 2 && IsLeapYear(year) ? 1 : 0);
}

bool IsValidDate(int year, int month, int day) {
    return month >= 1 && month <= 12 && day >= 1 &&
           day <= DaysInMonth(year, month);
}

Sign SignForDate(int month, int day) {
    switch (month) {
        case 1: return day <= 19 ? Sign::CAPRICORN : Sign::AQUARIUS;
        case 2: return day <= 18 ? Sign::AQUARIUS : Sign::PISCES;
        case 3: return day <= 20 ? Sign::PISCES : Sign::ARIES;
        case 4: return day <= 19 ? Sign::ARIES : Sign::TAURUS;
        case 5: return day <= 20 ? Sign::TAURUS : Sign::GEMINI;
        case 6: return day <= 21 ? Sign::GEMINI : Sign::CANCER;
        case 7: return day <= 22 ? Sign::CANCER : Sign::LEO;
        case 8: return day <= 22 ? Sign::LEO : Sign::VIRGO;
        case 9: return day <= 22 ? Sign::VIRGO : Sign::LIBRA;
        case 10: return day <= 23 ? Sign::LIBRA : Sign::SCORPIO;
        case 11: return day <= 22 ? Sign::SCORPIO : Sign::SAGITTARIUS;
        default: return day <= 21 ? Sign::SAGITTARIUS : Sign::CAPRICORN;
    }
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
    if (TryMountSdCard(PHOTO_SDMMC_BUS_WIDTH)) {
        return true;
    }
    return PHOTO_SDMMC_BUS_WIDTH != 1 && TryMountSdCard(1);
}

bool IsValidUtf8(const char* text) {
    const auto* cursor = reinterpret_cast<const uint8_t*>(text);
    while (*cursor) {
        if (*cursor < 0x80) {
            if (*cursor < 0x20 && *cursor != '\t' && *cursor != '\n') return false;
            ++cursor;
            continue;
        }
        size_t length = 0;
        uint32_t codepoint = 0;
        if ((*cursor & 0xe0) == 0xc0) {
            length = 2;
            codepoint = *cursor & 0x1f;
        } else if ((*cursor & 0xf0) == 0xe0) {
            length = 3;
            codepoint = *cursor & 0x0f;
        } else if ((*cursor & 0xf8) == 0xf0) {
            length = 4;
            codepoint = *cursor & 0x07;
        } else {
            return false;
        }
        for (size_t i = 1; i < length; ++i) {
            if ((cursor[i] & 0xc0) != 0x80) return false;
            codepoint = (codepoint << 6) | (cursor[i] & 0x3f);
        }
        if ((length == 2 && codepoint < 0x80) ||
            (length == 3 && codepoint < 0x800) ||
            (length == 4 && (codepoint < 0x10000 || codepoint > 0x10ffff)) ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
        cursor += length;
    }
    return true;
}

bool CopyField(const char* source, char* output, size_t output_size,
               bool decode_newlines) {
    if (!source || !output || output_size == 0) return false;
    size_t written = 0;
    for (size_t i = 0; source[i] != '\0'; ++i) {
        char value = source[i];
        if (decode_newlines && value == '\\' && source[i + 1] == 'n') {
            value = '\n';
            ++i;
        }
        if (written + 1 >= output_size) return false;
        output[written++] = value;
    }
    output[written] = '\0';
    return written > 0 && IsValidUtf8(output);
}

esp_jpeg_image_scale_t ChooseScale(uint16_t width, uint16_t height) {
    if (width <= 180 && height <= 180) return JPEG_IMAGE_SCALE_0;
    if (width / 2 <= 180 && height / 2 <= 180) return JPEG_IMAGE_SCALE_1_2;
    if (width / 4 <= 180 && height / 4 <= 180) return JPEG_IMAGE_SCALE_1_4;
    return JPEG_IMAGE_SCALE_1_8;
}

}  // namespace

const Result& Metadata(Sign sign) {
    const size_t index = static_cast<size_t>(sign);
    return kMetadata[index < 12 ? index : 0];
}

Status Calculate(const Input& input, Result* result) {
    if (!result || !IsValidDate(input.year, input.month, input.day)) {
        return Status::INVALID_DATE;
    }
    if (input.year < 1900 || input.year > 2100) {
        return Status::OUT_OF_RANGE;
    }
    *result = Metadata(SignForDate(input.month, input.day));
    return Status::OK;
}

Status LoadReaderPage(Sign sign, uint8_t page,
                      char* title, size_t title_size,
                      char* text, size_t text_size) {
    if (static_cast<uint8_t>(sign) >= 12 || page >= ReaderPageCount() ||
        !title || !text || title_size == 0 || text_size == 0) {
        return Status::DETAIL_INVALID;
    }
    if (!EnsureSdCardMounted()) return Status::SD_UNAVAILABLE;
    FILE* file = fopen(kDetailsPath, "rb");
    if (!file) return Status::DETAIL_MISSING;

    const char* expected_id = Metadata(sign).id;
    char line[1024];
    bool found = false;
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        char* id = line;
        char* first_tab = strchr(id, '\t');
        if (!first_tab) continue;
        *first_tab = '\0';
        char* page_text = first_tab + 1;
        char* second_tab = strchr(page_text, '\t');
        if (!second_tab) continue;
        *second_tab = '\0';
        char* page_title = second_tab + 1;
        char* third_tab = strchr(page_title, '\t');
        if (!third_tab) continue;
        *third_tab = '\0';
        char* page_body = third_tab + 1;
        if (strcmp(id, expected_id) != 0 ||
            strtol(page_text, nullptr, 10) != page) continue;
        found = CopyField(page_title, title, title_size, false) &&
                CopyField(page_body, text, text_size, true);
        break;
    }
    fclose(file);
    return found ? Status::OK : Status::DETAIL_MISSING;
}

Status LoadImage(Sign sign, ImageFrame* frame) {
    if (!frame || static_cast<uint8_t>(sign) >= 12) return Status::IMAGE_INVALID;
    if (!EnsureSdCardMounted()) return Status::SD_UNAVAILABLE;

    char path[128];
    snprintf(path, sizeof(path), "%s/%s.jpg", kImageDirectory, Metadata(sign).id);
    struct stat st{};
    if (stat(path, &st) != 0 || st.st_size <= 0 ||
        static_cast<size_t>(st.st_size) > kMaxImageInputBytes) {
        ESP_LOGW(kTag, "zodiac image missing/large path=%s errno=%d", path, errno);
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
    frame->dsc.header.flags = LV_IMAGE_FLAGS_ALLOCATED;
    frame->dsc.header.w = output_info.width;
    frame->dsc.header.h = output_info.height;
    frame->dsc.header.stride = output_info.width * 2;
    frame->dsc.data_size = output_info.output_len;
    frame->dsc.data = output;
    frame->data = output;
    frame->data_size = output_info.output_len;
    ESP_LOGI(kTag, "loaded %s %ux%u bytes=%u", path,
             static_cast<unsigned>(output_info.width),
             static_cast<unsigned>(output_info.height),
             static_cast<unsigned>(output_info.output_len));
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
        case Status::INVALID_DATE: return "请输入有效的公历日期";
        case Status::OUT_OF_RANGE: return "仅支持 1900 至 2100 年";
        case Status::SD_UNAVAILABLE: return "未检测到 SD 卡";
        case Status::DETAIL_MISSING: return "星座详细资料缺失";
        case Status::DETAIL_INVALID: return "星座详细资料格式无效";
        case Status::IMAGE_MISSING: return "星座图片缺失";
        case Status::IMAGE_INVALID: return "星座图片格式无效";
        case Status::NO_MEMORY: return "图片内存不足";
    }
    return "未知错误";
}

}  // namespace QdZodiac

#endif
