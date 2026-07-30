#include "puzzle_arcade_service.h"

#if defined(CONFIG_QDTECH_EXPERIMENT_PUZZLE_ARCADE) && \
    CONFIG_QDTECH_EXPERIMENT_PUZZLE_ARCADE

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
#include "esp_random.h"
#include "esp_vfs_fat.h"
#include "jpeg_decoder.h"
#include "sdmmc_cmd.h"

namespace QdPuzzleArcade {
namespace {

constexpr char kTag[] = "PuzzleArcade";
constexpr char kSudokuPath[] = "/sdcard/games/puzzle_arcade/sudoku/puzzles.tsv";
constexpr char kLockPath[] = "/sdcard/games/puzzle_arcade/code_lock/challenges.tsv";
constexpr char kMatch3Path[] = "/sdcard/games/puzzle_arcade/match3/levels.tsv";
constexpr char kSokobanPath[] = "/sdcard/games/puzzle_arcade/sokoban/levels.tsv";
constexpr char kMazePath[] = "/sdcard/games/puzzle_arcade/motion_maze/levels.tsv";
constexpr char kCoverDirectory[] = "/sdcard/games/puzzle_arcade/covers";
constexpr size_t kMaxImageInputBytes = 256 * 1024;
constexpr size_t kMaxImageOutputBytes = 180 * 120 * 2;

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

void TrimLine(char* line) {
    if (!line) {
        return;
    }
    size_t length = strlen(line);
    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
        line[--length] = '\0';
    }
}

bool CopyField(const char* source, char* output, size_t output_size) {
    if (!source || !output || output_size == 0) {
        return false;
    }
    const size_t length = strlen(source);
    if (length == 0 || length >= output_size) {
        return false;
    }
    memcpy(output, source, length + 1);
    return true;
}

bool SplitTsv(char* line, char** fields, size_t count) {
    if (!line || !fields || count == 0 || line[0] == '#' || line[0] == '\0') {
        return false;
    }
    fields[0] = line;
    for (size_t index = 1; index < count; ++index) {
        char* tab = strchr(fields[index - 1], '\t');
        if (!tab) {
            return false;
        }
        *tab = '\0';
        fields[index] = tab + 1;
    }
    return strchr(fields[count - 1], '\t') == nullptr;
}

bool ParseSudoku(char* line, SudokuPuzzle* puzzle) {
    char* fields[4]{};
    if (!SplitTsv(line, fields, 4) || strlen(fields[2]) != 81 ||
        strlen(fields[3]) != 81 || strspn(fields[2], ".123456789") != 81 ||
        strspn(fields[3], "123456789") != 81) {
        return false;
    }
    return CopyField(fields[0], puzzle->id, sizeof(puzzle->id)) &&
           CopyField(fields[1], puzzle->difficulty, sizeof(puzzle->difficulty)) &&
           CopyField(fields[2], puzzle->puzzle, sizeof(puzzle->puzzle)) &&
           CopyField(fields[3], puzzle->solution, sizeof(puzzle->solution));
}

bool ParseLock(char* line, LockChallenge* challenge) {
    char* fields[3]{};
    if (!SplitTsv(line, fields, 3) || strlen(fields[2]) != 4 ||
        strspn(fields[2], "0123456789") != 4) {
        return false;
    }
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = i + 1; j < 4; ++j) {
            if (fields[2][i] == fields[2][j]) {
                return false;
            }
        }
    }
    return CopyField(fields[0], challenge->id, sizeof(challenge->id)) &&
           CopyField(fields[1], challenge->difficulty, sizeof(challenge->difficulty)) &&
           CopyField(fields[2], challenge->code, sizeof(challenge->code));
}

bool ParseMatch3(char* line, Match3Level* level) {
    char* fields[4]{};
    if (!SplitTsv(line, fields, 4)) {
        return false;
    }
    char* end = nullptr;
    const long target = strtol(fields[2], &end, 10);
    if (!end || *end != '\0' || target < 100 || target > 60000) {
        return false;
    }
    const long moves = strtol(fields[3], &end, 10);
    if (!end || *end != '\0' || moves < 5 || moves > 99) {
        return false;
    }
    Match3Level parsed{};
    if (!CopyField(fields[0], parsed.id, sizeof(parsed.id)) ||
        !CopyField(fields[1], parsed.name, sizeof(parsed.name))) {
        return false;
    }
    parsed.target_score = static_cast<uint16_t>(target);
    parsed.moves = static_cast<uint8_t>(moves);
    *level = parsed;
    return true;
}

template <typename Record, typename Parser>
Status LoadRandomRecord(const char* path, Parser parser, Record* output) {
    if (!output) {
        return Status::INVALID_DATA;
    }
    if (!EnsureSdCardMounted()) {
        return Status::SD_UNAVAILABLE;
    }
    FILE* file = fopen(path, "rb");
    if (!file) {
        ESP_LOGW(kTag, "missing data path=%s errno=%d", path, errno);
        return Status::FILE_MISSING;
    }
    char line[256];
    uint32_t valid_count = 0;
    Record selected{};
    while (fgets(line, sizeof(line), file)) {
        TrimLine(line);
        Record candidate{};
        if (!parser(line, &candidate)) {
            continue;
        }
        ++valid_count;
        if (valid_count == 1 || esp_random() % valid_count == 0) {
            selected = candidate;
        }
    }
    fclose(file);
    if (valid_count == 0) {
        return Status::INVALID_DATA;
    }
    *output = selected;
    ESP_LOGI(kTag, "loaded random record path=%s valid=%lu",
             path, static_cast<unsigned long>(valid_count));
    return Status::OK;
}

template <size_t MaxWidth, size_t MaxHeight, typename Level>
bool ParseGridRecord(char* line, Level* level) {
    char* fields[5]{};
    if (!SplitTsv(line, fields, 5)) {
        return false;
    }
    char* end = nullptr;
    const long width = strtol(fields[2], &end, 10);
    if (!end || *end != '\0' || width <= 0 || width > static_cast<long>(MaxWidth)) {
        return false;
    }
    const long height = strtol(fields[3], &end, 10);
    if (!end || *end != '\0' || height <= 0 || height > static_cast<long>(MaxHeight)) {
        return false;
    }
    Level parsed{};
    if (!CopyField(fields[0], parsed.id, sizeof(parsed.id)) ||
        !CopyField(fields[1], parsed.name, sizeof(parsed.name))) {
        return false;
    }
    parsed.width = static_cast<uint8_t>(width);
    parsed.height = static_cast<uint8_t>(height);
    const char* cursor = fields[4];
    for (long row = 0; row < height; ++row) {
        for (long column = 0; column < width; ++column) {
            if (*cursor == '\0' || *cursor == '/') {
                return false;
            }
            parsed.cells[row * MaxWidth + column] = *cursor++;
        }
        if (row + 1 < height) {
            if (*cursor != '/') {
                return false;
            }
            ++cursor;
        }
    }
    if (*cursor != '\0') {
        return false;
    }
    *level = parsed;
    return true;
}

template <size_t MaxWidth, size_t MaxHeight, typename Level>
Status LoadGridLevel(const char* path, uint16_t ordinal, Level* output,
                     uint16_t* level_count) {
    if (!output) {
        return Status::INVALID_DATA;
    }
    if (!EnsureSdCardMounted()) {
        return Status::SD_UNAVAILABLE;
    }
    FILE* file = fopen(path, "rb");
    if (!file) {
        ESP_LOGW(kTag, "missing grid path=%s errno=%d", path, errno);
        return Status::FILE_MISSING;
    }
    char line[384];
    uint16_t count = 0;
    Level selected{};
    bool found = false;
    while (fgets(line, sizeof(line), file)) {
        TrimLine(line);
        Level candidate{};
        if (!ParseGridRecord<MaxWidth, MaxHeight>(line, &candidate)) {
            continue;
        }
        if (count == ordinal) {
            selected = candidate;
            found = true;
        }
        ++count;
    }
    fclose(file);
    if (level_count) {
        *level_count = count;
    }
    if (count == 0) {
        return Status::INVALID_DATA;
    }
    if (!found) {
        return LoadGridLevel<MaxWidth, MaxHeight>(path, ordinal % count, output, nullptr);
    }
    *output = selected;
    ESP_LOGI(kTag, "loaded grid path=%s ordinal=%u count=%u id=%s",
             path, ordinal, count, output->id);
    return Status::OK;
}

esp_jpeg_image_scale_t ChooseScale(uint16_t width, uint16_t height) {
    if (width <= 180 && height <= 120) {
        return JPEG_IMAGE_SCALE_0;
    }
    if (width / 2 <= 180 && height / 2 <= 120) {
        return JPEG_IMAGE_SCALE_1_2;
    }
    if (width / 4 <= 180 && height / 4 <= 120) {
        return JPEG_IMAGE_SCALE_1_4;
    }
    return JPEG_IMAGE_SCALE_1_8;
}

}  // namespace

Status LoadRandomSudoku(SudokuPuzzle* puzzle) {
    return LoadRandomRecord(kSudokuPath, ParseSudoku, puzzle);
}

Status LoadRandomLockChallenge(LockChallenge* challenge) {
    return LoadRandomRecord(kLockPath, ParseLock, challenge);
}

Status LoadRandomMatch3Level(Match3Level* level) {
    return LoadRandomRecord(kMatch3Path, ParseMatch3, level);
}

Status LoadSokobanLevel(uint16_t ordinal, SokobanLevel* level, uint16_t* level_count) {
    return LoadGridLevel<kSokobanMaxWidth, kSokobanMaxHeight>(
        kSokobanPath, ordinal, level, level_count);
}

Status LoadMazeLevel(uint16_t ordinal, MazeLevel* level, uint16_t* level_count) {
    return LoadGridLevel<kMazeMaxWidth, kMazeMaxHeight>(
        kMazePath, ordinal, level, level_count);
}

const char* GameId(Game game) {
    switch (game) {
        case Game::SUDOKU: return "sudoku";
        case Game::CODE_LOCK: return "code_lock";
        case Game::SOKOBAN: return "sokoban";
        case Game::MATCH3: return "match3";
        case Game::MOTION_MAZE: return "motion_maze";
        case Game::TILE_2048: return "tile_2048";
        case Game::FREECELL: return "freecell";
    }
    return "sudoku";
}

Status LoadCover(Game game, ImageFrame* frame) {
    if (!frame || !EnsureSdCardMounted()) {
        return frame ? Status::SD_UNAVAILABLE : Status::IMAGE_INVALID;
    }
    char path[128];
    snprintf(path, sizeof(path), "%s/%s.jpg", kCoverDirectory, GameId(game));
    struct stat st{};
    if (stat(path, &st) != 0 || st.st_size <= 0 ||
        static_cast<size_t>(st.st_size) > kMaxImageInputBytes) {
        ESP_LOGW(kTag, "cover missing/large path=%s errno=%d", path, errno);
        return Status::IMAGE_MISSING;
    }
    FILE* file = fopen(path, "rb");
    if (!file) {
        return Status::IMAGE_MISSING;
    }
    auto* input = static_cast<uint8_t*>(
        heap_caps_malloc(st.st_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!input) {
        fclose(file);
        return Status::NO_MEMORY;
    }
    const size_t bytes_read = fread(input, 1, st.st_size, file);
    fclose(file);
    if (bytes_read != static_cast<size_t>(st.st_size)) {
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
    ESP_LOGI(kTag, "cover loaded path=%s %ux%u bytes=%u", path,
             static_cast<unsigned>(output_info.width),
             static_cast<unsigned>(output_info.height),
             static_cast<unsigned>(output_info.output_len));
    return Status::OK;
}

void ReleaseImage(ImageFrame* frame) {
    if (!frame) {
        return;
    }
    if (frame->data) {
        heap_caps_free(frame->data);
    }
    frame->data = nullptr;
    frame->data_size = 0;
    memset(&frame->dsc, 0, sizeof(frame->dsc));
}

const char* StatusText(Status status) {
    switch (status) {
        case Status::OK: return "读取完成";
        case Status::SD_UNAVAILABLE: return "未检测到 SD 卡";
        case Status::FILE_MISSING: return "游戏数据缺失";
        case Status::INVALID_DATA: return "游戏数据格式无效";
        case Status::IMAGE_MISSING: return "封面图片缺失";
        case Status::IMAGE_INVALID: return "封面图片格式无效";
        case Status::NO_MEMORY: return "图片内存不足";
    }
    return "未知错误";
}

}  // namespace QdPuzzleArcade

#endif
