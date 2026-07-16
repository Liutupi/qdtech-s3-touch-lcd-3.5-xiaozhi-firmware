#include "bone_weight_service.h"

#include "sdkconfig.h"

#if defined(CONFIG_QDTECH_EXPERIMENT_CALENDAR_BONE_WEIGHT) && \
    CONFIG_QDTECH_EXPERIMENT_CALENDAR_BONE_WEIGHT

#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>

#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

namespace QdBoneWeight {
namespace {

constexpr char kCalendarPath[] = "/sdcard/calendar/bone_weight/calendar.bin";
constexpr char kWeightsPath[] = "/sdcard/calendar/bone_weight/weights.bin";
constexpr char kSongsPath[] = "/sdcard/calendar/bone_weight/songs.tsv";
constexpr char kInterpretationsPath[] =
    "/sdcard/calendar/bone_weight/interpretations.tsv";
constexpr char kDetailsPath[] = "/sdcard/calendar/bone_weight/details.tsv";

#pragma pack(push, 1)
struct CalendarHeader {
    char magic[8];
    uint16_t version;
    uint16_t header_size;
    int16_t start_year;
    uint8_t start_month;
    uint8_t start_day;
    int16_t end_year;
    uint8_t end_month;
    uint8_t end_day;
    uint32_t record_count;
    uint8_t record_size;
    uint8_t reserved[7];
};

struct CalendarRecord {
    uint8_t year_cycle;
    uint8_t month;
    uint8_t day;
};

struct WeightsHeader {
    char magic[8];
    uint16_t version;
    uint16_t header_size;
    uint8_t year_count;
    uint8_t month_count;
    uint8_t day_count;
    uint8_t hour_count;
};
#pragma pack(pop)

static_assert(sizeof(CalendarHeader) == 32);
static_assert(sizeof(CalendarRecord) == 3);
static_assert(sizeof(WeightsHeader) == 16);

bool IsLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int DaysInMonth(int year, int month) {
    static constexpr uint8_t kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 0;
    }
    return kDays[month - 1] + (month == 2 && IsLeapYear(year) ? 1 : 0);
}

bool IsValidDate(int year, int month, int day) {
    return year >= 1 && month >= 1 && month <= 12 &&
           day >= 1 && day <= DaysInMonth(year, month);
}

int64_t DaysFromCivil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const unsigned adjusted_month = month > 2 ? month - 3 : month + 9;
    const unsigned day_of_year = (153 * adjusted_month + 2) / 5 + day - 1;
    const unsigned day_of_era =
        year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return era * 146097 + static_cast<int>(day_of_era);
}

void AdvanceOneDay(int* year, int* month, int* day) {
    if (++(*day) <= DaysInMonth(*year, *month)) {
        return;
    }
    *day = 1;
    if (++(*month) <= 12) {
        return;
    }
    *month = 1;
    ++(*year);
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

bool ReadCalendarRecord(int year, int month, int day, CalendarRecord* record) {
    FILE* file = fopen(kCalendarPath, "rb");
    if (!file) {
        return false;
    }

    CalendarHeader header{};
    const bool header_ok =
        fread(&header, 1, sizeof(header), file) == sizeof(header) &&
        memcmp(header.magic, "QDBWCAL1", sizeof(header.magic)) == 0 &&
        header.version == 1 &&
        header.header_size == sizeof(CalendarHeader) &&
        header.record_size == sizeof(CalendarRecord);
    if (!header_ok) {
        fclose(file);
        return false;
    }

    const int64_t index =
        DaysFromCivil(year, month, day) -
        DaysFromCivil(header.start_year, header.start_month, header.start_day);
    if (index < 0 || index >= header.record_count) {
        fclose(file);
        return false;
    }

    const long offset =
        static_cast<long>(header.header_size + index * header.record_size);
    const bool ok =
        fseek(file, offset, SEEK_SET) == 0 &&
        fread(record, 1, sizeof(*record), file) == sizeof(*record);
    fclose(file);
    return ok;
}

bool ReadWeights(uint8_t* weights, size_t size) {
    FILE* file = fopen(kWeightsPath, "rb");
    if (!file) {
        return false;
    }

    WeightsHeader header{};
    const bool header_ok =
        fread(&header, 1, sizeof(header), file) == sizeof(header) &&
        memcmp(header.magic, "QDBWGT1", 7) == 0 &&
        header.magic[7] == '\0' &&
        header.version == 1 &&
        header.header_size == sizeof(WeightsHeader) &&
        header.year_count == 60 &&
        header.month_count == 12 &&
        header.day_count == 30 &&
        header.hour_count == 12;
    const bool ok = header_ok && size == 114 && fread(weights, 1, size, file) == size;
    fclose(file);
    return ok;
}

bool IsValidUtf8(const char* text) {
    const auto* cursor = reinterpret_cast<const uint8_t*>(text);
    while (*cursor) {
        if (*cursor < 0x80) {
            if (*cursor < 0x20 && *cursor != '\t') {
                return false;
            }
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
            if ((cursor[i] & 0xc0) != 0x80) {
                return false;
            }
            codepoint = (codepoint << 6) | (cursor[i] & 0x3f);
        }
        if ((length == 2 && codepoint < 0x80) ||
            (length == 3 && codepoint < 0x800) ||
            (length == 4 && (codepoint < 0x10000 || codepoint > 0x10ffff)) ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            return false;
        }
        cursor += length;
    }
    return true;
}

bool ReadTextFile(const char* path, uint8_t total_weight,
                  char* output, size_t output_size) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    char line[320];
    bool found = false;
    while (fgets(line, sizeof(line), file)) {
        char* tab = strchr(line, '\t');
        if (!tab) {
            continue;
        }
        *tab = '\0';
        if (strtol(line, nullptr, 10) != total_weight) {
            continue;
        }

        char* text = tab + 1;
        text[strcspn(text, "\r\n")] = '\0';
        const size_t length = strlen(text);
        if (length == 0 || length >= output_size || !IsValidUtf8(text)) {
            break;
        }
        memcpy(output, text, length + 1);
        found = true;
        break;
    }
    fclose(file);
    return found;
}

bool ReadDetailText(uint8_t total_weight, uint8_t page,
                    char* output, size_t output_size) {
    FILE* file = fopen(kDetailsPath, "rb");
    if (!file) {
        return false;
    }

    char line[320];
    bool found = false;
    while (fgets(line, sizeof(line), file)) {
        char* first_tab = strchr(line, '\t');
        if (!first_tab) {
            continue;
        }
        *first_tab = '\0';
        char* second_tab = strchr(first_tab + 1, '\t');
        if (!second_tab) {
            continue;
        }
        *second_tab = '\0';
        char* third_tab = strchr(second_tab + 1, '\t');
        if (!third_tab) {
            continue;
        }
        *third_tab = '\0';

        const long minimum = strtol(line, nullptr, 10);
        const long maximum = strtol(first_tab + 1, nullptr, 10);
        const long detail_page = strtol(second_tab + 1, nullptr, 10);
        if (total_weight < minimum || total_weight > maximum ||
            detail_page != page) {
            continue;
        }

        char* text = third_tab + 1;
        text[strcspn(text, "\r\n")] = '\0';
        const size_t length = strlen(text);
        if (length == 0 || length >= output_size || !IsValidUtf8(text)) {
            break;
        }
        memcpy(output, text, length + 1);
        found = true;
        break;
    }
    fclose(file);
    return found;
}

}  // namespace

Status Calculate(const Input& input, Result* result) {
    if (!result || !IsValidDate(input.year, input.month, input.day) ||
        input.hour < 0 || input.hour > 23) {
        return Status::INVALID_DATE;
    }
    if (input.year < 1901 || input.year > 2100) {
        return Status::OUT_OF_RANGE;
    }

    int lookup_year = input.year;
    int lookup_month = input.month;
    int lookup_day = input.day;
    if (input.hour == 23) {
        AdvanceOneDay(&lookup_year, &lookup_month, &lookup_day);
        if (lookup_year > 2100) {
            return Status::OUT_OF_RANGE;
        }
    }

    if (!EnsureSdCardMounted()) {
        return Status::SD_UNAVAILABLE;
    }

    CalendarRecord calendar{};
    uint8_t weights[114]{};
    if (!ReadCalendarRecord(lookup_year, lookup_month, lookup_day, &calendar) ||
        !ReadWeights(weights, sizeof(weights))) {
        return Status::DATA_MISSING;
    }

    const uint8_t lunar_month = calendar.month & 0x7f;
    const bool leap_month = (calendar.month & 0x80) != 0;
    if (calendar.year_cycle >= 60 || lunar_month < 1 || lunar_month > 12 ||
        calendar.day < 1 || calendar.day > 30) {
        return Status::DATA_INVALID;
    }

    uint8_t weight_month = lunar_month;
    if (leap_month && calendar.day >= 16) {
        weight_month = lunar_month == 12 ? 1 : lunar_month + 1;
    }
    const uint8_t shichen =
        input.hour == 23 || input.hour == 0 ? 0 : static_cast<uint8_t>((input.hour + 1) / 2);

    Result value{};
    value.lunar_year_cycle = calendar.year_cycle;
    value.lunar_month = lunar_month;
    value.lunar_day = calendar.day;
    value.shichen = shichen;
    value.lunar_leap_month = leap_month;
    value.year_weight = weights[calendar.year_cycle];
    value.month_weight = weights[60 + weight_month - 1];
    value.day_weight = weights[72 + calendar.day - 1];
    value.hour_weight = weights[102 + shichen];
    value.total_weight =
        value.year_weight + value.month_weight + value.day_weight + value.hour_weight;
    if (value.total_weight < 21 || value.total_weight > 72) {
        return Status::DATA_INVALID;
    }
    if (!ReadTextFile(kSongsPath, value.total_weight,
                      value.song, sizeof(value.song))) {
        return Status::SONG_MISSING;
    }

    *result = value;
    return Status::OK;
}

Status LoadText(uint8_t total_weight, TextKind kind,
                char* output, size_t output_size) {
    if (!output || output_size == 0 || total_weight < 21 || total_weight > 72) {
        return Status::DATA_INVALID;
    }
    if (!EnsureSdCardMounted()) {
        return Status::SD_UNAVAILABLE;
    }

    const char* path =
        kind == TextKind::INTERPRETATION ? kInterpretationsPath : kSongsPath;
    if (!ReadTextFile(path, total_weight, output, output_size)) {
        return kind == TextKind::INTERPRETATION
                   ? Status::INTERPRETATION_MISSING
                   : Status::SONG_MISSING;
    }
    return Status::OK;
}

Status LoadReaderPage(uint8_t total_weight, uint8_t page,
                      char* output, size_t output_size) {
    if (page >= ReaderPageCount()) {
        return Status::DATA_INVALID;
    }
    if (page == 0) {
        return LoadText(total_weight, TextKind::SONG, output, output_size);
    }
    if (page == 1) {
        return LoadText(
            total_weight, TextKind::INTERPRETATION, output, output_size);
    }
    if (!output || output_size == 0 || total_weight < 21 || total_weight > 72) {
        return Status::DATA_INVALID;
    }
    if (!EnsureSdCardMounted()) {
        return Status::SD_UNAVAILABLE;
    }
    if (!ReadDetailText(total_weight, page, output, output_size)) {
        return Status::DETAIL_MISSING;
    }
    return Status::OK;
}

const char* ReaderPageTitle(uint8_t page) {
    static constexpr const char* kTitles[] = {
        "称骨歌诀",
        "总体释义",
        "性格与处事",
        "事业与财务",
        "人际与情感",
        "生活建议",
    };
    return page < ReaderPageCount() ? kTitles[page] : "详细解读";
}

const char* StatusText(Status status) {
    switch (status) {
        case Status::OK:
            return "读取完成";
        case Status::INVALID_DATE:
            return "出生日期或时辰无效";
        case Status::OUT_OF_RANGE:
            return "仅支持 1901 至 2100 年";
        case Status::SD_UNAVAILABLE:
            return "未检测到 SD 卡";
        case Status::DATA_MISSING:
            return "SD 卡称骨数据缺失";
        case Status::DATA_INVALID:
            return "SD 卡称骨数据无效";
        case Status::SONG_MISSING:
            return "称骨歌诀缺失或编码无效";
        case Status::INTERPRETATION_MISSING:
            return "白话解读缺失或编码无效";
        case Status::DETAIL_MISSING:
            return "详细解读缺失或编码无效";
    }
    return "未知错误";
}

}  // namespace QdBoneWeight

#endif
