#include "fc_emulator_service.h"

#include "application.h"
#include "audio_codecs/audio_codec.h"
#include "boards/common/board.h"
#include "config.h"
#include "qdtech_nofrendo.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include <driver/gpio.h>
#include <driver/sdmmc_host.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_lvgl_port.h>
#include <esp_random.h>
#include <esp_vfs_fat.h>
#include <freertos/idf_additions.h>

#define TAG "FcEmulator"

namespace {
constexpr const char* kMountPoint = "/sdcard";
constexpr const char* kRomDirs[] = {
    "/sdcard/FC",
    "/sdcard/nes",
    "/sdcard/roms",
    "/sdcard",
};
constexpr size_t kMaxScannedRoms = 192;
constexpr long kMaxNofrendoRomBytes = 2 * 1024 * 1024;
constexpr uint16_t kNesFrameWidth = 256;
constexpr uint16_t kNesFrameHeight = 240;
constexpr uint16_t kFrameWidth = 360;
constexpr uint16_t kFrameHeight = 154;
constexpr size_t kFramePixels = kFrameWidth * kFrameHeight;
constexpr TickType_t kButtonReleaseHold = pdMS_TO_TICKS(120);
constexpr TickType_t kIdleDelay = pdMS_TO_TICKS(250);
constexpr TickType_t kFrameDelay = pdMS_TO_TICKS(1);
constexpr int64_t kLvglFramePublishIntervalUs = 50000;
constexpr int64_t kDirectFramePublishIntervalUs = 33333;
constexpr int kSdFreqDefaultKhz = 10000;
constexpr int kSdFreqFallbackKhz = 10000;

bool ends_with_nes_ext(const char* name) {
    if (!name) {
        return false;
    }
    std::string lower(name);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lower.ends_with(".nes");
}

bool is_hidden_or_resource_file(const char* name) {
    if (!name || name[0] == '\0') {
        return true;
    }
    return name[0] == '.' || (name[0] == '_' && name[1] == '.');
}

std::string file_name_from_path(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

void strip_nes_extension(std::string& name) {
    if (name.size() >= 4 && ends_with_nes_ext(name.c_str())) {
        name.resize(name.size() - 4);
    }
}

void trim_ascii_space(std::string& text) {
    while (!text.empty() && static_cast<unsigned char>(text.back()) <= ' ') {
        text.pop_back();
    }
    size_t start = 0;
    while (start < text.size() && static_cast<unsigned char>(text[start]) <= ' ') {
        ++start;
    }
    if (start > 0) {
        text.erase(0, start);
    }
}

size_t utf8_char_len(unsigned char c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

std::string utf8_truncate(const std::string& text, size_t max_chars) {
    size_t pos = 0;
    size_t chars = 0;
    while (pos < text.size() && chars < max_chars) {
        const size_t len = utf8_char_len(static_cast<unsigned char>(text[pos]));
        if (pos + len > text.size()) {
            break;
        }
        pos += len;
        ++chars;
    }
    if (pos >= text.size()) {
        return text;
    }
    return text.substr(0, pos) + "~";
}

std::string rom_display_name(const std::string& path, size_t max_chars = 22) {
    std::string name = file_name_from_path(path);
    strip_nes_extension(name);
    trim_ascii_space(name);
    if (name.empty()) {
        name = file_name_from_path(path);
    }
    return utf8_truncate(name, max_chars);
}

void strip_utf8_bom(std::string& text) {
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xef &&
        static_cast<unsigned char>(text[1]) == 0xbb &&
        static_cast<unsigned char>(text[2]) == 0xbf) {
        text.erase(0, 3);
    }
}

void unquote_ascii(std::string& text) {
    if (text.size() >= 2 &&
        ((text.front() == '"' && text.back() == '"') ||
         (text.front() == '\'' && text.back() == '\''))) {
        text = text.substr(1, text.size() - 2);
        trim_ascii_space(text);
    }
}

std::string rom_alias_key(const std::string& path_or_name) {
    std::string key = file_name_from_path(path_or_name);
    strip_nes_extension(key);
    trim_ascii_space(key);
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return c < 0x80 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
    });
    return key;
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
}

int16_t clamp16(int value) {
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return static_cast<int16_t>(value);
}

bool is_supported_nofrendo_mapper(uint8_t mapper) {
    static constexpr uint8_t kSupportedMappers[] = {
        0, 1, 2, 3, 4, 5, 7, 8, 9, 11, 15, 16, 18, 19,
        21, 22, 23, 24, 25, 32, 33, 34, 40, 64, 65, 66,
        70, 75, 78, 79, 85, 94, 99, 231,
    };
    for (uint8_t supported : kSupportedMappers) {
        if (supported == mapper) {
            return true;
        }
    }
    return false;
}

uint8_t ines_mapper(const uint8_t header[16]) {
    return static_cast<uint8_t>((header[6] >> 4) | (header[7] & 0xf0));
}

bool is_nes2_header(const uint8_t header[16]) {
    return (header[7] & 0x0c) == 0x08;
}

static const uint16_t kNesPaletteRgb565[64] = {
    0x4208, 0x900A, 0xA804, 0x8812, 0x4018, 0x1021, 0x0120, 0x012C,
    0x0138, 0x0020, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0xA514, 0xF98A, 0xF104, 0xD818, 0x8821, 0x3029, 0x0131, 0x09BD,
    0x0251, 0x02A0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0xFFFF, 0xFDEA, 0xFCB0, 0xFC58, 0xFB9E, 0xA3BF, 0x53DF, 0x2D7F,
    0x15DF, 0x0EB0, 0x23B0, 0x4390, 0x838F, 0x0000, 0x0000, 0x0000,
    0xFFFF, 0xFF7F, 0xFEBF, 0xFDBF, 0xFCBF, 0xB5BF, 0x6DFF, 0x46FF,
    0x36FF, 0x37F0, 0x5FF0, 0x87D7, 0xC7D7, 0x0000, 0x0000, 0x0000,
};
} // namespace

void FcEmulatorService::Start(DesktopUI* desktop_ui) {
    desktop_ui_ = desktop_ui;
    if (desktop_ui_) {
        desktop_ui_->SetFcActiveCallback([this](bool active) { SetActive(active); });
        desktop_ui_->SetFcActions(
            [this]() { PlayPause(); },
            [this]() { Stop(); },
            [this]() { Next(); },
            [this]() { Prev(); });
        desktop_ui_->SetFcControllerCallback([this](uint8_t controller) { SetController(controller); });
    }
    ESP_LOGI(TAG, "fc emulator service registered");
}

void FcEmulatorService::SetDirectFrameCallback(DirectFrameCallback callback) {
    direct_frame_cb_ = std::move(callback);
}

void FcEmulatorService::SetActive(bool active) {
    const bool previous = active_.exchange(active);
    if (previous != active) {
        ESP_LOGI(TAG, "fc page %s", active ? "active" : "inactive");
        if (active) {
            Application::GetInstance().SetExternalAudioActive(true);
            EnsureTaskStarted();
            playing_.store(false);
            PublishMode(false);
            if (roms_.empty()) {
                scan_requested_.store(true);
            } else {
                PublishState("Select ROM", SelectedName().c_str());
            }
        } else {
            playing_.store(false);
            qd_nofrendo_request_stop();
            controller_state_.store(0);
            controller_release_tick_.store(0);
            scan_requested_.store(false);
            release_roms_requested_.store(true);
            PublishMode(false);
            Application::GetInstance().SetExternalAudioActive(false);
        }
    }
}

bool FcEmulatorService::PrepareSdCard() {
    return MountSdCard();
}

void FcEmulatorService::PrepareTask() {
    EnsureTaskStarted();
}

void FcEmulatorService::PlayPause() {
    EnsureTaskStarted();
    if (scanning_.load()) {
        PublishMode(false);
        PublishState("Scanning", "Please wait");
        return;
    }
    if (playing_.load()) {
        Stop();
        return;
    }
    if (roms_.empty()) {
        scan_requested_.store(true);
        PublishMode(false);
        PublishState("Scanning", "Looking for .nes files");
        return;
    }
    if (!ValidateSelectedRom()) {
        playing_.store(false);
        PublishMode(false);
        PublishState("Unsupported ROM", SelectedName().c_str());
        return;
    }
    frame_counter_ = 0;
    produced_frame_counter_ = 0;
    last_perf_frame_counter_ = 0;
    last_perf_log_us_ = esp_timer_get_time();
    controller_state_.store(0);
    controller_release_tick_.store(0);
    playing_.store(true);
    PublishMode(true);
    PublishState(SelectedName().c_str(), "Playing");
    ESP_LOGI(TAG, "fc play rom=%s", SelectedName().c_str());
}

void FcEmulatorService::Stop() {
    playing_.store(false);
    qd_nofrendo_request_stop();
    controller_state_.store(0);
    controller_release_tick_.store(0);
    PublishMode(false);
    PublishState(roms_.empty() ? "No ROM" : "Select ROM",
                 roms_.empty() ? "Put .nes files in /nes" : SelectedName().c_str());
    ESP_LOGI(TAG, "fc stop/list");
}

void FcEmulatorService::Next() {
    EnsureTaskStarted();
    if (scanning_.load()) {
        PublishMode(false);
        PublishState("Scanning", "Please wait");
        return;
    }
    if (roms_.empty()) {
        scan_requested_.store(true);
        PublishMode(false);
        PublishState("Scanning", "Looking for .nes files");
        return;
    }
    playing_.store(false);
    qd_nofrendo_request_stop();
    controller_state_.store(0);
    controller_release_tick_.store(0);
    selected_index_ = (selected_index_ + 1) % roms_.size();
    ESP_LOGI(TAG, "fc next index=%u rom=%s", static_cast<unsigned>(selected_index_), SelectedName().c_str());
    PublishMode(false);
    PublishState("Select ROM", SelectedName().c_str());
}

void FcEmulatorService::Prev() {
    EnsureTaskStarted();
    if (scanning_.load()) {
        PublishMode(false);
        PublishState("Scanning", "Please wait");
        return;
    }
    if (roms_.empty()) {
        scan_requested_.store(true);
        PublishMode(false);
        PublishState("Scanning", "Looking for .nes files");
        return;
    }
    playing_.store(false);
    qd_nofrendo_request_stop();
    controller_state_.store(0);
    controller_release_tick_.store(0);
    selected_index_ = selected_index_ == 0 ? roms_.size() - 1 : selected_index_ - 1;
    ESP_LOGI(TAG, "fc prev index=%u rom=%s", static_cast<unsigned>(selected_index_), SelectedName().c_str());
    PublishMode(false);
    PublishState("Select ROM", SelectedName().c_str());
}

void FcEmulatorService::EnsureTaskStarted() {
    if (task_handle_) {
        return;
    }

    constexpr uint32_t stack_size = 6144;
    constexpr UBaseType_t task_priority = 1;
    ESP_LOGI(TAG, "fc task create free_internal=%u largest_internal=%u",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));

    BaseType_t ret = xTaskCreateWithCaps(
        TaskWrapper, "fc_emulator", stack_size, this, task_priority, &task_handle_,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ret == pdPASS) {
        ESP_LOGI(TAG, "fc task started stack=%u memory=psram", static_cast<unsigned>(stack_size));
        return;
    }

    ESP_LOGW(TAG, "fc task PSRAM create failed ret=%ld, trying internal memory", static_cast<long>(ret));
    task_handle_ = nullptr;
    const BaseType_t freertos_ret = xTaskCreate(
        TaskWrapper, "fc_emulator", stack_size, this, task_priority, &task_handle_);
    if (freertos_ret != pdPASS) {
        task_handle_ = nullptr;
        ESP_LOGE(TAG, "fc task create failed ret=%ld", static_cast<long>(freertos_ret));
        PublishState("FC unavailable", "Not enough memory");
        return;
    }
    ESP_LOGI(TAG, "fc task started stack=%u memory=internal", static_cast<unsigned>(stack_size));
}

void FcEmulatorService::TaskWrapper(void* arg) {
    static_cast<FcEmulatorService*>(arg)->TaskLoop();
}

int FcEmulatorService::NofrendoFrameThunk(const uint16_t* pixels, uint16_t width, uint16_t height, void* user) {
    auto* self = static_cast<FcEmulatorService*>(user);
    return self && self->PublishDirectFrame(pixels, width, height) ? 1 : 0;
}

void FcEmulatorService::NofrendoAudioThunk(const int16_t* samples, int sample_count, int sample_rate, void* user) {
    auto* self = static_cast<FcEmulatorService*>(user);
    if (self) {
        self->WriteNofrendoAudio(samples, sample_count, sample_rate);
    }
}

void FcEmulatorService::TaskLoop() {
    PublishMode(false);
    PublishState("FC Emulator", "Open /nes on SD card");

    while (true) {
        const bool is_active = active_.load();
        if (release_roms_requested_.exchange(false)) {
            roms_.clear();
            roms_.shrink_to_fit();
            rom_aliases_.clear();
            rom_aliases_.shrink_to_fit();
            selected_index_ = 0;
            ESP_LOGI(TAG, "fc rom list released");
        }
        if (!is_active && !scan_requested_.load()) {
            vTaskDelay(kIdleDelay);
            continue;
        }

        if (!MountSdCard()) {
            ++sd_mount_failures_;
            playing_.store(false);
            PublishMode(false);
            PublishState("SD card not ready", "Insert FAT SD with .nes files");
            if (!is_active && sd_mount_failures_ >= 1) {
                scan_requested_.store(false);
            }
            vTaskDelay(pdMS_TO_TICKS(sd_mount_failures_ < 3 ? 1500 : 5000));
            continue;
        }
        sd_mount_failures_ = 0;

        if (scan_requested_.exchange(false) || roms_.empty()) {
            if (!ScanRoms()) {
                playing_.store(false);
                PublishMode(false);
                PublishState("No NES ROMs", "Checked /nes, /FC, /roms");
                vTaskDelay(pdMS_TO_TICKS(1500));
                continue;
            }
            playing_.store(false);
            controller_state_.store(0);
            controller_release_tick_.store(0);
            PublishMode(false);
            PublishState("Select ROM", SelectedName().c_str());
        }

        if (!is_active) {
            vTaskDelay(kIdleDelay);
            continue;
        }

        if (!playing_.load()) {
            vTaskDelay(kIdleDelay);
            continue;
        }

        RunNofrendoRom();
    }
}

bool FcEmulatorService::MountSdCard() {
    if (mounted_) {
        return true;
    }

    DIR* existing = opendir(kMountPoint);
    if (existing) {
        closedir(existing);
        mounted_ = true;
        ESP_LOGI(TAG, "reuse existing sdcard mount");
        return true;
    }

    if (TryMountSdCard(PHOTO_SDMMC_BUS_WIDTH, kSdFreqDefaultKhz)) {
        return true;
    }
    if (PHOTO_SDMMC_BUS_WIDTH != 1) {
        ESP_LOGW(TAG, "sd 4-bit mount failed, fallback to 1-bit low speed");
        return TryMountSdCard(1, kSdFreqFallbackKhz);
    }
    return false;
}

bool FcEmulatorService::TryMountSdCard(uint8_t width, int max_freq_khz) {
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = max_freq_khz;

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
        .allocation_unit_size = 4 * 1024,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    ESP_LOGI(TAG, "mount sdmmc for fc width=%u freq=%d free_internal=%u largest_internal=%u",
             width, max_freq_khz,
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
    esp_err_t err = esp_vfs_fat_sdmmc_mount(kMountPoint, &host, &slot_config, &mount_config, &card_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "fc sd mount failed width=%u err=%s", width, esp_err_to_name(err));
        card_ = nullptr;
        return false;
    }

    mounted_ = true;
    ESP_LOGI(TAG, "fc sd card mounted width=%u", width);
    return true;
}

bool FcEmulatorService::ScanRoms() {
    scanning_.store(true);
    roms_.clear();
    rom_aliases_.clear();
    roms_.reserve(kMaxScannedRoms);
    selected_index_ = 0;

    for (const char* dir : kRomDirs) {
        const size_t before = roms_.size();
        ScanRomDir(dir);
        if (roms_.size() > before) {
            ESP_LOGI(TAG, "fc using rom dir: %s", dir);
            LoadRomAliases(dir);
            break;
        }
    }

    std::sort(roms_.begin(), roms_.end());
    roms_.erase(std::unique(roms_.begin(), roms_.end()), roms_.end());
    std::sort(roms_.begin(), roms_.end(), [this](const std::string& a, const std::string& b) {
        return RomDisplayName(a, 96) < RomDisplayName(b, 96);
    });
    ESP_LOGI(TAG, "fc scan found %u nes files", static_cast<unsigned>(roms_.size()));
    const bool found = !roms_.empty();
    scanning_.store(false);
    return found;
}

size_t FcEmulatorService::ScanRomDir(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        ESP_LOGI(TAG, "skip rom dir: %s errno=%d %s", dir_path, errno, strerror(errno));
        return 0;
    }

    uint32_t entries = 0;
    uint32_t rom_count = 0;
    struct dirent* entry = nullptr;
    while (roms_.size() < kMaxScannedRoms && (entry = readdir(dir)) != nullptr) {
        ++entries;
        if (is_hidden_or_resource_file(entry->d_name) || !ends_with_nes_ext(entry->d_name)) {
            continue;
        }

        std::string path = std::string(dir_path) + "/" + entry->d_name;
        roms_.push_back(path);
        ++rom_count;
        if (rom_count == 1) {
            ESP_LOGI(TAG, "first rom candidate found in %s", dir_path);
        }
    }
    closedir(dir);
    ESP_LOGI(TAG, "rom dir scanned: %s entries=%lu nes=%lu%s",
             dir_path, static_cast<unsigned long>(entries), static_cast<unsigned long>(rom_count),
             roms_.size() >= kMaxScannedRoms ? " capped" : "");
    return rom_count;
}

void FcEmulatorService::LoadRomAliases(const char* dir_path) {
    if (!dir_path || dir_path[0] == '\0') {
        return;
    }

    const std::string dir(dir_path);
    const std::string files[] = {
        dir + "/roms.txt",
        dir + "/roms.csv",
        dir + "/games.txt",
    };
    for (const auto& file : files) {
        if (LoadRomAliasFile(file.c_str())) {
            return;
        }
    }
}

bool FcEmulatorService::LoadRomAliasFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    char line[256];
    uint32_t line_count = 0;
    uint32_t alias_count = 0;
    while (alias_count < kMaxScannedRoms && fgets(line, sizeof(line), file)) {
        ++line_count;
        std::string row(line);
        strip_utf8_bom(row);
        trim_ascii_space(row);
        if (row.empty() || row[0] == '#' || row[0] == ';') {
            continue;
        }

        const size_t sep = row.find_first_of("\t=,");
        if (sep == std::string::npos) {
            continue;
        }
        std::string key = row.substr(0, sep);
        std::string alias = row.substr(sep + 1);
        trim_ascii_space(key);
        trim_ascii_space(alias);
        unquote_ascii(key);
        unquote_ascii(alias);
        key = rom_alias_key(key);
        if (key.empty() || alias.empty()) {
            continue;
        }

        auto existing = std::find_if(rom_aliases_.begin(), rom_aliases_.end(),
                                     [&key](const auto& item) { return item.first == key; });
        if (existing == rom_aliases_.end()) {
            rom_aliases_.push_back({key, alias});
        } else {
            existing->second = alias;
        }
        ++alias_count;
    }
    fclose(file);
    ESP_LOGI(TAG, "rom aliases loaded: %s lines=%lu aliases=%lu",
             path, static_cast<unsigned long>(line_count), static_cast<unsigned long>(rom_aliases_.size()));
    return !rom_aliases_.empty();
}

bool FcEmulatorService::ValidateSelectedRom() {
    if (roms_.empty() || selected_index_ >= roms_.size()) {
        PublishState("No ROM", "Put .nes files in /nes");
        return false;
    }

    FILE* file = fopen(roms_[selected_index_].c_str(), "rb");
    if (!file) {
        PublishState("Open failed", SelectedName().c_str());
        ESP_LOGW(TAG, "rom open failed: %s errno=%d %s", roms_[selected_index_].c_str(), errno, strerror(errno));
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        PublishState("Open failed", "Could not read ROM size");
        ESP_LOGW(TAG, "rom seek failed: %s errno=%d %s", roms_[selected_index_].c_str(), errno, strerror(errno));
        return false;
    }
    const long size = ftell(file);
    if (size <= 16 || size > kMaxNofrendoRomBytes) {
        fclose(file);
        char detail[48];
        snprintf(detail, sizeof(detail), "Size %ldKB > 2048KB", size > 0 ? size / 1024 : 0);
        PublishState("ROM too large", detail);
        ESP_LOGW(TAG, "rom size unsupported: %s size=%ld max=%ld",
                 roms_[selected_index_].c_str(), size, kMaxNofrendoRomBytes);
        return false;
    }
    rewind(file);
    uint8_t header[16] = {};
    const size_t read = fread(header, 1, sizeof(header), file);
    fclose(file);
    if (read != sizeof(header) || memcmp(header, "NES\x1a", 4) != 0) {
        PublishState("Bad ROM", "Missing iNES header");
        ESP_LOGW(TAG, "rom header invalid: %s", roms_[selected_index_].c_str());
        return false;
    }
    const uint8_t mapper = ines_mapper(header);
    if (is_nes2_header(header)) {
        PublishState("Unsupported ROM", "NES 2.0 header");
        ESP_LOGW(TAG, "rom uses NES 2.0 header: %s mapper=%u", roms_[selected_index_].c_str(),
                 static_cast<unsigned>(mapper));
        return false;
    }
    if (!is_supported_nofrendo_mapper(mapper)) {
        char detail[48];
        snprintf(detail, sizeof(detail), "Mapper %u not supported", static_cast<unsigned>(mapper));
        PublishState("Unsupported ROM", detail);
        ESP_LOGW(TAG, "rom mapper unsupported: %s mapper=%u prg=%u chr=%u",
                 roms_[selected_index_].c_str(), static_cast<unsigned>(mapper), header[4], header[5]);
        return false;
    }
    ESP_LOGI(TAG, "rom header ok prg=%u chr=%u mapper=%u size=%ld",
             header[4], header[5], static_cast<unsigned>(mapper), size);
    return true;
}

void FcEmulatorService::RunNofrendoRom() {
    if (roms_.empty() || selected_index_ >= roms_.size()) {
        playing_.store(false);
        PublishMode(false);
        PublishState("No ROM", "Put .nes files in /nes");
        return;
    }

    qd_nofrendo_set_frame_callback(NofrendoFrameThunk, this);
    qd_nofrendo_set_audio_callback(NofrendoAudioThunk, this);
    qd_nofrendo_set_controller(controller_state_.load());
    audio_frame_counter_ = 0;
    last_audio_log_us_ = 0;
    ESP_LOGI(TAG, "nofrendo run rom=%s free_internal=%u largest_internal=%u",
             SelectedName().c_str(),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
    const int result = qd_nofrendo_run(roms_[selected_index_].c_str());
    qd_nofrendo_set_frame_callback(nullptr, nullptr);
    qd_nofrendo_set_audio_callback(nullptr, nullptr);
    controller_state_.store(0);
    controller_release_tick_.store(0);
    playing_.store(false);
    audio_output_buffer_.clear();

    ESP_LOGI(TAG, "nofrendo finished result=%d", result);
    PublishMode(false);
    if (active_.load()) {
        PublishState(result == 0 ? "Select ROM" : "Load failed",
                     result == 0 ? SelectedName().c_str() : "Nofrendo could not run ROM");
    }
}

bool FcEmulatorService::LoadNesRom() {
    if (roms_.empty() || selected_index_ >= roms_.size()) {
        return false;
    }

    FILE* file = fopen(roms_[selected_index_].c_str(), "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open ROM: %s", roms_[selected_index_].c_str());
        return false;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size <= 0 || size > 1024 * 1024) {
        fclose(file);
        ESP_LOGE(TAG, "ROM size invalid: %ld", size);
        return false;
    }

    uint8_t* rom_data = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!rom_data) {
        fclose(file);
        ESP_LOGE(TAG, "Failed to allocate ROM buffer");
        return false;
    }

    size_t read = fread(rom_data, 1, size, file);
    fclose(file);

    if (read != static_cast<size_t>(size)) {
        heap_caps_free(rom_data);
        ESP_LOGE(TAG, "ROM read incomplete");
        return false;
    }

    bool ok = nes_bus_.LoadRom(rom_data, size);
    heap_caps_free(rom_data);

    if (ok) {
        nes_initialized_ = true;
        last_frame_publish_us_ = 0;
        ESP_LOGI(TAG, "NES ROM loaded: %s", SelectedName().c_str());
    }
    return ok;
}

bool FcEmulatorService::RunNesFrame() {
    if (!nes_initialized_) return false;

    constexpr uint32_t kMaxInstructionsPerSlice = 12000;
    constexpr int64_t kFrameCpuBudgetUs = 12000;
    const int64_t deadline = esp_timer_get_time() + kFrameCpuBudgetUs;
    bool produced_frame = false;
    nes_bus_.ClearFrameReady();
    for (uint32_t i = 0; i < kMaxInstructionsPerSlice; ++i) {
        if ((i & 0x7f) == 0 && esp_timer_get_time() >= deadline) {
            break;
        }
        nes_bus_.Step();
        if (nes_bus_.FrameReady()) {
            produced_frame = true;
            nes_bus_.ClearFrameReady();
            break;
        }
    }
    return produced_frame;
}

bool FcEmulatorService::RenderFrame(Frame& frame, bool update_pixels) {
    if (playing_.load() && nes_initialized_) {
        const uint32_t release_tick = controller_release_tick_.load();
        if (release_tick != 0 && xTaskGetTickCount() >= release_tick) {
            controller_state_.store(0);
            controller_release_tick_.store(0);
        }
        nes_bus_.SetController(controller_state_.load());
        const bool produced_frame = RunNesFrame();
        if (produced_frame) {
            ++produced_frame_counter_;
        }

        const uint8_t* nes_fb = nes_bus_.FrameBuffer();
        if (!nes_fb) {
            return false;
        }

        if (update_pixels && produced_frame) {
            constexpr size_t kNesPixels = kNesFrameWidth * kNesFrameHeight;
            if (frame.pixels && frame.pixel_count != kNesPixels) {
                FreeFrame(frame);
            }
            if (!frame.pixels) {
                frame.pixel_count = kNesPixels;
                frame.pixels = static_cast<uint16_t*>(heap_caps_malloc(
                    frame.pixel_count * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
                if (!frame.pixels) {
                    ESP_LOGE(TAG, "fc nes rgb frame alloc failed bytes=%u",
                             static_cast<unsigned>(frame.pixel_count * sizeof(uint16_t)));
                    return false;
                }
            }
            for (size_t i = 0; i < kNesPixels; ++i) {
                frame.pixels[i] = kNesPaletteRgb565[nes_fb[i] & 0x3f];
            }

            memset(&frame.dsc, 0, sizeof(frame.dsc));
            frame.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
            frame.dsc.header.cf = LV_COLOR_FORMAT_RGB565;
            frame.dsc.header.flags = LV_IMAGE_FLAGS_ALLOCATED;
            frame.dsc.header.w = kNesFrameWidth;
            frame.dsc.header.h = kNesFrameHeight;
            frame.dsc.header.stride = kNesFrameWidth * 2;
            frame.dsc.data_size = frame.pixel_count * sizeof(uint16_t);
            frame.dsc.data = reinterpret_cast<const uint8_t*>(frame.pixels);
        }

        if ((frame_counter_ % 120) == 0) {
            uint32_t non_black = 0;
            constexpr size_t kNesPixels = kNesFrameWidth * kNesFrameHeight;
            for (size_t i = 0; i < kNesPixels; i += 64) {
                if (nes_fb[i] != 0) {
                    ++non_black;
                }
            }
            ESP_LOGI(TAG, "fc frame=%lu ready=%u sampled_non_black=%lu",
                     static_cast<unsigned long>(frame_counter_),
                     produced_frame ? 1 : 0,
                     static_cast<unsigned long>(non_black));
        }
        const int64_t now = esp_timer_get_time();
        if (last_perf_log_us_ == 0) {
            last_perf_log_us_ = now;
        } else if (now - last_perf_log_us_ >= 1000000) {
            const uint32_t loop_delta = frame_counter_ - last_perf_frame_counter_;
            ESP_LOGI(TAG, "fc perf produced_fps=%lu loops=%lu free_internal=%u largest_internal=%u",
                     static_cast<unsigned long>(produced_frame_counter_),
                     static_cast<unsigned long>(loop_delta),
                     static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
            produced_frame_counter_ = 0;
            last_perf_frame_counter_ = frame_counter_;
            last_perf_log_us_ = now;
        }
        return update_pixels && produced_frame;
    }

    if (!frame.pixels) {
        frame.pixel_count = kFramePixels;
        frame.pixels = static_cast<uint16_t*>(heap_caps_malloc(
            frame.pixel_count * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!frame.pixels) {
            ESP_LOGE(TAG, "fc frame alloc failed bytes=%u", static_cast<unsigned>(frame.pixel_count * sizeof(uint16_t)));
            return false;
        }
        memset(&frame.dsc, 0, sizeof(frame.dsc));
        frame.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        frame.dsc.header.cf = LV_COLOR_FORMAT_RGB565;
        frame.dsc.header.flags = LV_IMAGE_FLAGS_ALLOCATED;
        frame.dsc.header.w = kFrameWidth;
        frame.dsc.header.h = kFrameHeight;
        frame.dsc.header.stride = kFrameWidth * 2;
        frame.dsc.data_size = frame.pixel_count * sizeof(uint16_t);
        frame.dsc.data = reinterpret_cast<uint8_t*>(frame.pixels);
    }

    const uint16_t c0 = rgb565(0x10, 0x18, 0x24);
    const uint16_t c1 = rgb565(0x47, 0xb3, 0xff);
    const uint16_t c2 = rgb565(0xff, 0xbd, 0x55);
    const uint16_t c3 = rgb565(0xf6, 0xef, 0xdf);
    const uint32_t tick = selected_index_ * 17;
    for (uint16_t y = 0; y < kFrameHeight; ++y) {
        for (uint16_t x = 0; x < kFrameWidth; ++x) {
            uint16_t color = ((x / 16 + y / 16 + tick / 8) & 1) ? c0 : c1;
            if (y < 16 || y >= kFrameHeight - 16 || x < 16 || x >= kFrameWidth - 16) {
                color = c2;
            }
            if (((x + tick * 3) % 96) < 12 && y > 44 && y < 196) {
                color = c3;
            }
            frame.pixels[y * kFrameWidth + x] = color;
        }
    }
    return update_pixels;
}

void FcEmulatorService::FreeFrame(Frame& frame) {
    if (frame.pixels) {
        heap_caps_free(frame.pixels);
        frame.pixels = nullptr;
    }
    frame.pixel_count = 0;
    memset(&frame.dsc, 0, sizeof(frame.dsc));
}

void FcEmulatorService::ClearFrames() {
    FreeFrame(frames_[0]);
    FreeFrame(frames_[1]);
}

void FcEmulatorService::PublishState(const char* title, const char* detail) {
    if (!desktop_ui_) {
        return;
    }
    const std::string list = BuildRomList();
    if (lvgl_port_lock(100)) {
        desktop_ui_->SetFcState(title, detail, list.c_str());
        lvgl_port_unlock();
    }
}

void FcEmulatorService::PublishMode(bool playing) {
    if (!desktop_ui_) {
        return;
    }
    if (lvgl_port_lock(100)) {
        desktop_ui_->SetFcMode(playing);
        lvgl_port_unlock();
    }
}

bool FcEmulatorService::PublishFrame(Frame& frame) {
    constexpr size_t kNesPixels = kNesFrameWidth * kNesFrameHeight;
    if (direct_frame_cb_ && playing_.load() && frame.pixels && frame.pixel_count == kNesPixels) {
        return direct_frame_cb_(frame.pixels, kNesFrameWidth, kNesFrameHeight);
    }
    return PublishLvglFrame(&frame.dsc);
}

bool FcEmulatorService::PublishLvglFrame(const lv_img_dsc_t* frame) {
    if (!desktop_ui_ || !frame || !frame->data) {
        return false;
    }
    if (lvgl_port_lock(50)) {
        desktop_ui_->SetFcFrame(frame);
        lvgl_port_unlock();
        return true;
    }
    return false;
}

bool FcEmulatorService::PublishDirectFrame(const uint16_t* pixels, uint16_t width, uint16_t height) {
    static int64_t last_log_us = 0;
    static uint32_t frames = 0;
    if (!direct_frame_cb_ || !playing_.load() || !pixels || width == 0 || height == 0) {
        return false;
    }

    ++frames;
    const int64_t now = esp_timer_get_time();
    if (last_log_us == 0) {
        last_log_us = now;
    } else if (now - last_log_us >= 1000000) {
        ESP_LOGI(TAG, "nofrendo fps=%lu free_internal=%u largest_internal=%u",
                 static_cast<unsigned long>(frames),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
        frames = 0;
        last_log_us = now;
    }

    return direct_frame_cb_(pixels, width, height);
}


void FcEmulatorService::WriteNofrendoAudio(const int16_t* samples, int sample_count, int sample_rate) {
    if (!playing_.load() || !samples || sample_count <= 0 || sample_rate <= 0) {
        return;
    }

    AudioCodec* codec = Board::GetInstance().GetAudioCodec();
    if (!codec) {
        return;
    }

    const int out_rate = codec->output_sample_rate();
    if (sample_rate == out_rate) {
        audio_output_buffer_.resize(sample_count);
        std::copy(samples, samples + sample_count, audio_output_buffer_.begin());
    } else {
        const int out_count = std::max(1, sample_count * out_rate / sample_rate);
        audio_output_buffer_.resize(out_count);
        for (int i = 0; i < out_count; ++i) {
            const int64_t src_pos_q16 = static_cast<int64_t>(i) * sample_rate * 65536 / out_rate;
            const int src_index = static_cast<int>(src_pos_q16 >> 16);
            const int frac = static_cast<int>(src_pos_q16 & 0xffff);
            if (src_index >= sample_count - 1) {
                audio_output_buffer_[i] = samples[sample_count - 1];
            } else {
                const int a = samples[src_index];
                const int b = samples[src_index + 1];
                audio_output_buffer_[i] = clamp16((a * (65536 - frac) + b * frac) >> 16);
            }
        }
    }

    if (!codec->output_enabled()) {
        codec->EnableOutput(true);
    }
    codec->OutputData(audio_output_buffer_);

    ++audio_frame_counter_;
    const int64_t now = esp_timer_get_time();
    if (last_audio_log_us_ == 0) {
        last_audio_log_us_ = now;
    } else if (now - last_audio_log_us_ >= 2000000) {
        ESP_LOGI(TAG, "fc audio frames=%lu samples=%u rate=%d->%d free_internal=%u largest_internal=%u",
                 static_cast<unsigned long>(audio_frame_counter_),
                 static_cast<unsigned>(audio_output_buffer_.size()),
                 sample_rate, out_rate,
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
        audio_frame_counter_ = 0;
        last_audio_log_us_ = now;
    }
}

std::string FcEmulatorService::RomDisplayName(const std::string& path, size_t max_chars) const {
    const std::string key = rom_alias_key(path);
    auto alias = std::find_if(rom_aliases_.begin(), rom_aliases_.end(),
                              [&key](const auto& item) { return item.first == key; });
    if (alias != rom_aliases_.end()) {
        return utf8_truncate(alias->second, max_chars);
    }
    return rom_display_name(path, max_chars);
}

std::string FcEmulatorService::SelectedName() const {
    if (roms_.empty() || selected_index_ >= roms_.size()) {
        return "No ROM";
    }
    return RomDisplayName(roms_[selected_index_], 24);
}

std::string FcEmulatorService::BuildRomList() const {
    if (roms_.empty()) {
        return "No .nes files found\nPut ROMs in /sdcard/nes";
    }

    std::string text;
    constexpr size_t kMaxRows = 8;
    const size_t count = std::min<size_t>(roms_.size(), kMaxRows);
    size_t start = selected_index_ >= count ? selected_index_ - count + 1 : 0;
    if (start + count > roms_.size()) {
        start = roms_.size() - count;
    }

    char index_line[32];
    snprintf(index_line, sizeof(index_line), "%u/%u\n",
             static_cast<unsigned>(selected_index_ + 1),
             static_cast<unsigned>(roms_.size()));
    text += index_line;

    for (size_t row = 0; row < count; ++row) {
        const size_t i = start + row;
        const std::string name = RomDisplayName(roms_[i]);
        char row_prefix[12];
        snprintf(row_prefix, sizeof(row_prefix), "%03u ", static_cast<unsigned>(i + 1));
        text += (i == selected_index_ ? "> " : "  ");
        text += row_prefix;
        text += name;
        if (row + 1 < count) {
            text += "\n";
        }
    }
    return text;
}

void FcEmulatorService::SetController(uint8_t controller) {
    controller_state_.store(controller);
    controller_release_tick_.store(0);
    qd_nofrendo_set_controller(controller);
}
