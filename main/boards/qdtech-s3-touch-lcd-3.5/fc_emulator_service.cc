#include "fc_emulator_service.h"

#include "config.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

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
    "/sdcard/nes",
    "/sdcard/NES",
    "/sdcard/FC",
    "/sdcard/fc",
    "/sdcard/roms",
    "/sdcard/ROMS",
    "/sdcard",
};
constexpr uint16_t kNesFrameWidth = 256;
constexpr uint16_t kNesFrameHeight = 240;
constexpr uint16_t kFrameWidth = 360;
constexpr uint16_t kFrameHeight = 154;
constexpr size_t kFramePixels = kFrameWidth * kFrameHeight;
constexpr TickType_t kButtonReleaseHold = pdMS_TO_TICKS(1500);
constexpr TickType_t kIdleDelay = pdMS_TO_TICKS(250);
constexpr TickType_t kFrameDelay = pdMS_TO_TICKS(1);
constexpr int64_t kFramePublishIntervalUs = 50000;
constexpr int kSdFreqDefaultKhz = SDMMC_FREQ_DEFAULT;
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

bool is_supported_nes_rom(const char* path, uint8_t* mapper_out = nullptr) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    uint8_t header[16] = {};
    const size_t read = fread(header, 1, sizeof(header), file);
    fclose(file);
    if (read != sizeof(header) || memcmp(header, "NES\x1a", 4) != 0) {
        return false;
    }
    const uint8_t mapper = (header[6] >> 4) | (header[7] & 0xf0);
    if (mapper_out) {
        *mapper_out = mapper;
    }
    return mapper == 0 || mapper == 2;
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
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
    scan_requested_.store(true);
    EnsureTaskStarted();
}

void FcEmulatorService::SetActive(bool active) {
    const bool previous = active_.exchange(active);
    if (previous != active) {
        ESP_LOGI(TAG, "fc page %s", active ? "active" : "inactive");
        if (active) {
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
            controller_state_.store(0);
            controller_release_tick_.store(0);
            PublishMode(false);
        }
    }
}

void FcEmulatorService::PlayPause() {
    EnsureTaskStarted();
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
    if (!LoadNesRom()) {
        PublishMode(false);
        PublishState("Load failed", "ROM format error");
        playing_.store(false);
        return;
    }
    frame_counter_ = 0;
    controller_state_.store(0);
    controller_release_tick_.store(0);
    playing_.store(true);
    PublishMode(true);
    PublishState(SelectedName().c_str(), "Playing");
    ESP_LOGI(TAG, "fc play rom=%s", SelectedName().c_str());
}

void FcEmulatorService::Stop() {
    playing_.store(false);
    controller_state_.store(0);
    controller_release_tick_.store(0);
    PublishMode(false);
    PublishState(roms_.empty() ? "No ROM" : "Select ROM",
                 roms_.empty() ? "Put .nes files in /nes" : SelectedName().c_str());
    ESP_LOGI(TAG, "fc stop/list");
}

void FcEmulatorService::Next() {
    EnsureTaskStarted();
    if (roms_.empty()) {
        scan_requested_.store(true);
        PublishMode(false);
        PublishState("Scanning", "Looking for .nes files");
        return;
    }
    playing_.store(false);
    controller_state_.store(0);
    controller_release_tick_.store(0);
    selected_index_ = (selected_index_ + 1) % roms_.size();
    ESP_LOGI(TAG, "fc next index=%u rom=%s", static_cast<unsigned>(selected_index_), SelectedName().c_str());
    PublishMode(false);
    PublishState("Select ROM", SelectedName().c_str());
}

void FcEmulatorService::Prev() {
    EnsureTaskStarted();
    if (roms_.empty()) {
        scan_requested_.store(true);
        PublishMode(false);
        PublishState("Scanning", "Looking for .nes files");
        return;
    }
    playing_.store(false);
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
    ESP_LOGI(TAG, "fc task create free_internal=%u largest_internal=%u",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));

    BaseType_t ret = xTaskCreateWithCaps(
        TaskWrapper, "fc_emulator", stack_size, this, 3, &task_handle_,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ret == pdPASS) {
        ESP_LOGI(TAG, "fc task started stack=%u memory=psram", static_cast<unsigned>(stack_size));
        return;
    }

    ESP_LOGW(TAG, "fc task PSRAM create failed ret=%ld, trying internal memory", static_cast<long>(ret));
    task_handle_ = nullptr;
    ret = xTaskCreate(TaskWrapper, "fc_emulator", stack_size, this, 3, &task_handle_);
    if (ret != pdPASS) {
        task_handle_ = nullptr;
        ESP_LOGE(TAG, "fc task create failed ret=%ld", static_cast<long>(ret));
        PublishState("FC unavailable", "Not enough memory");
        return;
    }
    ESP_LOGI(TAG, "fc task started stack=%u memory=internal", static_cast<unsigned>(stack_size));
}

void FcEmulatorService::TaskWrapper(void* arg) {
    static_cast<FcEmulatorService*>(arg)->TaskLoop();
}

void FcEmulatorService::TaskLoop() {
    PublishMode(false);
    PublishState("FC Emulator", "Open /nes on SD card");

    while (true) {
        const bool is_active = active_.load();
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

        const int64_t now = esp_timer_get_time();
        const bool should_publish =
            last_frame_publish_us_ == 0 || now - last_frame_publish_us_ >= kFramePublishIntervalUs;
        Frame& frame = frames_[frame_slot_];
        RenderFrame(frame, should_publish);
        if (should_publish && frame.dsc.data) {
            PublishFrame(&frame.dsc);
            last_frame_publish_us_ = now;
            frame_slot_ = (frame_slot_ + 1) % 2;
        }
        ++frame_counter_;
        vTaskDelay(kFrameDelay);
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
        .max_files = 3,
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
    roms_.clear();
    selected_index_ = 0;

    for (const char* dir : kRomDirs) {
        ScanRomDir(dir);
    }

    std::sort(roms_.begin(), roms_.end());
    roms_.erase(std::unique(roms_.begin(), roms_.end()), roms_.end());
    ESP_LOGI(TAG, "fc scan found %u nes files", static_cast<unsigned>(roms_.size()));
    return !roms_.empty();
}

void FcEmulatorService::ScanRomDir(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        ESP_LOGI(TAG, "skip rom dir: %s errno=%d %s", dir_path, errno, strerror(errno));
        return;
    }

    uint32_t entries = 0;
    uint32_t rom_count = 0;
    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        ++entries;
        if (is_hidden_or_resource_file(entry->d_name) || !ends_with_nes_ext(entry->d_name)) {
            continue;
        }

        std::string path = std::string(dir_path) + "/" + entry->d_name;
        struct stat st = {};
        if (stat(path.c_str(), &st) != 0 || S_ISDIR(st.st_mode) || st.st_size < 16) {
            ESP_LOGW(TAG, "skip invalid rom candidate: %s errno=%d %s", path.c_str(), errno, strerror(errno));
            continue;
        }
        uint8_t mapper = 0xff;
        if (!is_supported_nes_rom(path.c_str(), &mapper)) {
            if (rom_count < 5) {
                ESP_LOGI(TAG, "skip unsupported rom: %s mapper=%u", path.c_str(), mapper);
            }
            continue;
        }
        roms_.push_back(path);
        ++rom_count;
        if (rom_count <= 5) {
            ESP_LOGI(TAG, "rom candidate: %s size=%u", path.c_str(), static_cast<unsigned>(st.st_size));
        }
    }
    closedir(dir);
    ESP_LOGI(TAG, "rom dir scanned: %s entries=%lu nes=%lu",
             dir_path, static_cast<unsigned long>(entries), static_cast<unsigned long>(rom_count));
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
    uint8_t header[16] = {};
    const size_t read = fread(header, 1, sizeof(header), file);
    fclose(file);
    if (read != sizeof(header) || memcmp(header, "NES\x1a", 4) != 0) {
        PublishState("Bad ROM", "Missing iNES header");
        ESP_LOGW(TAG, "rom header invalid: %s", roms_[selected_index_].c_str());
        return false;
    }
    const uint8_t mapper = (header[6] >> 4) | (header[7] & 0xf0);
    if (mapper != 0 && mapper != 2) {
        PublishState("Unsupported ROM", "Only mapper 0/2 now");
        ESP_LOGW(TAG, "rom mapper unsupported mapper=%u path=%s",
                 static_cast<unsigned>(mapper), roms_[selected_index_].c_str());
        return false;
    }

    ESP_LOGI(TAG, "rom header ok prg=%u chr=%u mapper=%u",
             header[4], header[5], static_cast<unsigned>(mapper));
    return true;
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

    constexpr uint32_t kMaxInstructionsPerFrame = 5200;
    constexpr int64_t kFrameCpuBudgetUs = 14000;
    const int64_t deadline = esp_timer_get_time() + kFrameCpuBudgetUs;
    bool produced_frame = false;
    nes_bus_.ClearFrameReady();
    for (uint32_t i = 0; i < kMaxInstructionsPerFrame; ++i) {
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

void FcEmulatorService::RenderFrame(Frame& frame, bool update_pixels) {
    if (playing_.load() && nes_initialized_) {
        const uint32_t release_tick = controller_release_tick_.load();
        if (release_tick != 0 && xTaskGetTickCount() >= release_tick) {
            controller_state_.store(0);
            controller_release_tick_.store(0);
        }
        nes_bus_.SetController(controller_state_.load());
        const bool produced_frame = RunNesFrame();

        const uint8_t* nes_fb = nes_bus_.FrameBuffer();
        if (!nes_fb) {
            return;
        }

        if (update_pixels) {
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
                    return;
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
        return;
    }

    if (!frame.pixels) {
        frame.pixel_count = kFramePixels;
        frame.pixels = static_cast<uint16_t*>(heap_caps_malloc(
            frame.pixel_count * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!frame.pixels) {
            ESP_LOGE(TAG, "fc frame alloc failed bytes=%u", static_cast<unsigned>(frame.pixel_count * sizeof(uint16_t)));
            return;
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

void FcEmulatorService::PublishFrame(const lv_img_dsc_t* frame) {
    if (!desktop_ui_ || !frame || !frame->data) {
        return;
    }
    if (lvgl_port_lock(50)) {
        desktop_ui_->SetFcFrame(frame);
        lvgl_port_unlock();
    }
}

std::string FcEmulatorService::SelectedName() const {
    if (roms_.empty() || selected_index_ >= roms_.size()) {
        return "No ROM";
    }
    const std::string& path = roms_[selected_index_];
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
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
        const std::string& path = roms_[i];
        const size_t slash = path.find_last_of('/');
        std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
        if (name.size() > 34) {
            name = name.substr(0, 33) + "~";
        }
        text += (i == selected_index_ ? "> " : "  ");
        text += name;
        if (row + 1 < count) {
            text += "\n";
        }
    }
    return text;
}

void FcEmulatorService::SetController(uint8_t controller) {
    if (controller != 0) {
        controller_state_.store(controller);
        controller_release_tick_.store(0);
        nes_bus_.SetController(controller);
    } else if (controller_state_.load() != 0) {
        controller_release_tick_.store(xTaskGetTickCount() + kButtonReleaseHold);
    }
    if (controller != last_logged_controller_state_) {
        last_logged_controller_state_ = controller;
        ESP_LOGI(TAG, "controller=0x%02x", controller);
    }
}
