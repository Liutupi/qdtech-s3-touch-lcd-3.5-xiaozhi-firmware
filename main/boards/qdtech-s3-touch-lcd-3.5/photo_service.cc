#include "photo_service.h"

#include "config.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

#include <driver/gpio.h>
#include <driver/sdmmc_host.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_vfs_fat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>
#include <jpeg_decoder.h>

#define TAG "PhotoService"

namespace {
constexpr const char* kMountPoint = "/sdcard";
constexpr const char* kPhotoDir = "/sdcard/photos";
constexpr const char* kPhotoDirs[] = {
    "/sdcard/photos",
    "/sdcard/PHOTOS",
    "/sdcard/Photos",
    "/sdcard/PHOTOS_READY",
    "/sdcard/photos_ready",
    "/sdcard",
};
constexpr size_t kMaxInputBytes = 4 * 1024 * 1024;
constexpr size_t kMaxOutputBytes = 960 * 640 * 2;
constexpr TickType_t kSlideDelay = pdMS_TO_TICKS(6000);
constexpr TickType_t kIdleDelay = pdMS_TO_TICKS(500);
constexpr int kSdFreqDefaultKhz = SDMMC_FREQ_DEFAULT;
constexpr int kSdFreqFallbackKhz = 10000;

bool ends_with_photo_ext(const char* name) {
    if (!name) {
        return false;
    }
    std::string lower(name);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lower.ends_with(".jpg") || lower.ends_with(".jpeg");
}

bool is_hidden_or_resource_file(const char* name) {
    if (!name || name[0] == '\0') {
        return true;
    }
    return name[0] == '.' || (name[0] == '_' && name[1] == '.');
}

esp_jpeg_image_scale_t choose_scale(uint16_t width, uint16_t height) {
    if (width <= 960 && height <= 640 && static_cast<size_t>(width) * height * 2 <= kMaxOutputBytes) {
        return JPEG_IMAGE_SCALE_0;
    }
    if (width / 2 <= 960 && height / 2 <= 640 &&
        static_cast<size_t>(width / 2) * (height / 2) * 2 <= kMaxOutputBytes) {
        return JPEG_IMAGE_SCALE_1_2;
    }
    if (width / 4 <= 960 && height / 4 <= 640 &&
        static_cast<size_t>(width / 4) * (height / 4) * 2 <= kMaxOutputBytes) {
        return JPEG_IMAGE_SCALE_1_4;
    }
    return JPEG_IMAGE_SCALE_1_8;
}
} // namespace

void PhotoService::Start(DesktopUI* desktop_ui) {
    desktop_ui_ = desktop_ui;
    if (desktop_ui_) {
        desktop_ui_->SetPhotoActiveCallback([this](bool active) { SetActive(active); });
        desktop_ui_->SetPhotoRefreshCallback([this]() { Refresh(); });
    }
    ESP_LOGI(TAG, "photo service registered");
}

void PhotoService::SetActive(bool active) {
    const bool previous = active_.exchange(active);
    if (previous != active) {
        ESP_LOGI(TAG, "photo playback %s", active ? "active" : "paused");
        if (active) {
            EnsureTaskStarted();
            refresh_requested_.store(true);
        }
    }
}

void PhotoService::Refresh() {
    EnsureTaskStarted();
    refresh_requested_.store(true);
    ESP_LOGI(TAG, "photo refresh requested");
}

void PhotoService::EnsureTaskStarted() {
    if (task_handle_) {
        return;
    }

    constexpr uint32_t stack_size = 6144;
    ESP_LOGI(TAG, "photo task create free_internal=%u largest_internal=%u",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));

    BaseType_t ret = xTaskCreateWithCaps(
        TaskWrapper, "photo_service", stack_size, this, 2, &task_handle_,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ret == pdPASS) {
        ESP_LOGI(TAG, "photo service task started stack=%u memory=psram",
                 static_cast<unsigned>(stack_size));
        return;
    }

    ESP_LOGW(TAG, "photo task PSRAM create failed ret=%ld, trying internal memory",
             static_cast<long>(ret));
    task_handle_ = nullptr;
    ret = xTaskCreate(TaskWrapper, "photo_service", stack_size, this, 2, &task_handle_);
    if (ret != pdPASS) {
        task_handle_ = nullptr;
        ESP_LOGE(TAG, "photo service task create failed ret=%ld free_internal=%u largest_internal=%u",
                 static_cast<long>(ret),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
        SetUiState("Photos unavailable", "Not enough memory");
        return;
    }
    ESP_LOGI(TAG, "photo service task started stack=%u memory=internal",
             static_cast<unsigned>(stack_size));
}

void PhotoService::TaskWrapper(void* arg) {
    static_cast<PhotoService*>(arg)->TaskLoop();
}

void PhotoService::TaskLoop() {
    SetUiState("Photos", "Open /photos on SD card");

    while (true) {
        if (!active_.load()) {
            vTaskDelay(kIdleDelay);
            continue;
        }

        if (!MountSdCard()) {
            char detail[96];
            snprintf(detail, sizeof(detail), "mount err=%s try=%lu",
                     esp_err_to_name(last_mount_error_),
                     static_cast<unsigned long>(mount_attempts_));
            SetUiState("SD card not ready", detail);
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        if (refresh_requested_.exchange(false) || photos_.empty()) {
            if (!ScanPhotos()) {
                SetUiState("No photos", "Checked /photos and /PHOTOS");
                vTaskDelay(pdMS_TO_TICKS(3000));
                continue;
            }
        }

        if (current_index_ >= photos_.size()) {
            current_index_ = 0;
        }

        Frame& frame = frames_[frame_slot_];
        uint16_t width = 0;
        uint16_t height = 0;
        const std::string path = photos_[current_index_];
        if (DecodePhoto(path, frame, width, height)) {
            char title[48];
            char detail[96];
            snprintf(title, sizeof(title), "Photo %u / %u",
                     static_cast<unsigned>(current_index_ + 1),
                     static_cast<unsigned>(photos_.size()));
            snprintf(detail, sizeof(detail), "%ux%u %s",
                     static_cast<unsigned>(width),
                     static_cast<unsigned>(height),
                     path.substr(path.find_last_of('/') + 1).c_str());
            SetUiFrame(&frame.dsc, title, detail);
            frame_slot_ = (frame_slot_ + 1) % 2;
            current_index_ = (current_index_ + 1) % photos_.size();
        } else {
            SetUiState("Decode failed", path.c_str());
            current_index_ = (current_index_ + 1) % photos_.size();
        }

        TickType_t waited = 0;
        while (active_.load() && waited < kSlideDelay && !refresh_requested_.load()) {
            vTaskDelay(pdMS_TO_TICKS(200));
            waited += pdMS_TO_TICKS(200);
        }
    }
}

bool PhotoService::MountSdCard() {
    if (mounted_) {
        return true;
    }

    ++mount_attempts_;
    last_mount_width_ = 0;
    last_mount_error_ = ESP_OK;

    if (TryMountSdCard(PHOTO_SDMMC_BUS_WIDTH, kSdFreqDefaultKhz)) {
        return true;
    }

    if (PHOTO_SDMMC_BUS_WIDTH != 1) {
        ESP_LOGW(TAG, "sd 4-bit mount failed, fallback to 1-bit low speed");
        if (TryMountSdCard(1, kSdFreqFallbackKhz)) {
            return true;
        }
    }

    ESP_LOGW(TAG, "sd card not ready after attempt=%lu last_err=%s",
             static_cast<unsigned long>(mount_attempts_), esp_err_to_name(last_mount_error_));
    return false;
}

bool PhotoService::TryMountSdCard(uint8_t width, int max_freq_khz) {
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
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    ESP_LOGI(TAG, "mount sdmmc clk=%d cmd=%d d0=%d d1=%d d2=%d d3=%d width=%u freq=%d",
             PHOTO_SDMMC_CLK_PIN, PHOTO_SDMMC_CMD_PIN, PHOTO_SDMMC_D0_PIN,
             slot_config.d1, slot_config.d2, slot_config.d3, width, max_freq_khz);
    esp_err_t err = esp_vfs_fat_sdmmc_mount(kMountPoint, &host, &slot_config, &mount_config, &card_);
    if (err != ESP_OK) {
        last_mount_error_ = err;
        ESP_LOGW(TAG, "sd card mount failed width=%u freq=%d err=%s",
                 width, max_freq_khz, esp_err_to_name(err));
        card_ = nullptr;
        return false;
    }

    mounted_ = true;
    last_mount_width_ = width;
    last_mount_error_ = ESP_OK;
    sdmmc_card_print_info(stdout, card_);
    ESP_LOGI(TAG, "sd card mounted width=%u freq=%d", width, max_freq_khz);
    return true;
}

bool PhotoService::ScanPhotos() {
    photos_.clear();
    current_index_ = 0;

    for (const char* dir : kPhotoDirs) {
        ScanPhotoDir(dir);
    }

    std::sort(photos_.begin(), photos_.end());
    photos_.erase(std::unique(photos_.begin(), photos_.end()), photos_.end());
    ESP_LOGI(TAG, "photo scan found %u jpg files", static_cast<unsigned>(photos_.size()));
    return !photos_.empty();
}

void PhotoService::ScanPhotoDir(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        ESP_LOGI(TAG, "skip photo dir: %s errno=%d %s", dir_path, errno, strerror(errno));
        return;
    }

    uint32_t entries = 0;
    uint32_t jpg_count = 0;
    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        ++entries;
        if (is_hidden_or_resource_file(entry->d_name) || !ends_with_photo_ext(entry->d_name)) {
            if (entries <= 12) {
                ESP_LOGD(TAG, "skip photo entry dir=%s name=%s", dir_path, entry->d_name);
            }
            continue;
        }

        std::string path = std::string(dir_path) + "/" + entry->d_name;
        struct stat st = {};
        if (stat(path.c_str(), &st) != 0) {
            ESP_LOGW(TAG, "photo candidate stat failed: %s errno=%d %s", path.c_str(), errno, strerror(errno));
            continue;
        }
        if (S_ISDIR(st.st_mode) || st.st_size <= 0) {
            continue;
        }

        photos_.push_back(path);
        ++jpg_count;
        if (jpg_count <= 5) {
            ESP_LOGI(TAG, "photo candidate: %s size=%u", path.c_str(), static_cast<unsigned>(st.st_size));
        }
    }
    closedir(dir);
    ESP_LOGI(TAG, "photo dir scanned: %s entries=%lu jpg=%lu",
             dir_path, static_cast<unsigned long>(entries), static_cast<unsigned long>(jpg_count));
}

bool PhotoService::DecodePhoto(const std::string& path, Frame& frame, uint16_t& width, uint16_t& height) {
    struct stat st = {};
    if (stat(path.c_str(), &st) != 0 || st.st_size <= 0) {
        ESP_LOGW(TAG, "photo stat failed: %s errno=%d %s", path.c_str(), errno, strerror(errno));
        return false;
    }
    if (static_cast<size_t>(st.st_size) > kMaxInputBytes) {
        ESP_LOGW(TAG, "photo too large: %s size=%u", path.c_str(), static_cast<unsigned>(st.st_size));
        return false;
    }

    FILE* file = fopen(path.c_str(), "rb");
    if (!file) {
        ESP_LOGW(TAG, "photo open failed: %s errno=%d %s", path.c_str(), errno, strerror(errno));
        return false;
    }

    uint8_t* input = static_cast<uint8_t*>(heap_caps_malloc(st.st_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!input) {
        fclose(file);
        ESP_LOGE(TAG, "photo input alloc failed size=%u", static_cast<unsigned>(st.st_size));
        return false;
    }

    const size_t read = fread(input, 1, st.st_size, file);
    fclose(file);
    if (read != static_cast<size_t>(st.st_size)) {
        heap_caps_free(input);
        ESP_LOGW(TAG, "photo read failed: %s read=%u size=%u",
                 path.c_str(), static_cast<unsigned>(read), static_cast<unsigned>(st.st_size));
        return false;
    }

    esp_jpeg_image_cfg_t info_cfg = {
        .indata = input,
        .indata_size = static_cast<uint32_t>(st.st_size),
        .outbuf = nullptr,
        .outbuf_size = 0,
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale = JPEG_IMAGE_SCALE_0,
        .flags = {
            .swap_color_bytes = 0,
        },
        .advanced = {
            .working_buffer = nullptr,
            .working_buffer_size = 0,
        },
        .priv = {},
    };

    esp_jpeg_image_output_t info = {};
    esp_err_t err = esp_jpeg_get_image_info(&info_cfg, &info);
    if (err != ESP_OK || info.width == 0 || info.height == 0) {
        heap_caps_free(input);
        ESP_LOGW(TAG, "jpeg info failed: %s err=%s", path.c_str(), esp_err_to_name(err));
        return false;
    }

    info_cfg.out_scale = choose_scale(info.width, info.height);
    esp_jpeg_image_output_t out = {};
    err = esp_jpeg_get_image_info(&info_cfg, &out);
    if (err != ESP_OK || out.output_len == 0 || out.output_len > kMaxOutputBytes) {
        heap_caps_free(input);
        ESP_LOGW(TAG, "jpeg scaled info failed: %s err=%s len=%u",
                 path.c_str(), esp_err_to_name(err), static_cast<unsigned>(out.output_len));
        return false;
    }

    uint8_t* output = static_cast<uint8_t*>(heap_caps_malloc(out.output_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!output) {
        heap_caps_free(input);
        ESP_LOGE(TAG, "photo output alloc failed len=%u", static_cast<unsigned>(out.output_len));
        return false;
    }

    esp_jpeg_image_cfg_t decode_cfg = info_cfg;
    decode_cfg.outbuf = output;
    decode_cfg.outbuf_size = out.output_len;
    err = esp_jpeg_decode(&decode_cfg, &out);
    heap_caps_free(input);
    if (err != ESP_OK) {
        heap_caps_free(output);
        ESP_LOGW(TAG, "jpeg decode failed: %s err=%s", path.c_str(), esp_err_to_name(err));
        return false;
    }

    FreeFrame(frame);
    memset(&frame.dsc, 0, sizeof(frame.dsc));
    frame.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    frame.dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    frame.dsc.header.flags = LV_IMAGE_FLAGS_ALLOCATED;
    frame.dsc.header.w = out.width;
    frame.dsc.header.h = out.height;
    frame.dsc.header.stride = out.width * 2;
    frame.dsc.data_size = out.output_len;
    frame.dsc.data = output;
    frame.data = output;
    frame.data_size = out.output_len;
    width = out.width;
    height = out.height;

    ESP_LOGI(TAG, "photo decoded %s %ux%u len=%u scale=%d",
             path.c_str(), static_cast<unsigned>(width), static_cast<unsigned>(height),
             static_cast<unsigned>(out.output_len), static_cast<int>(info_cfg.out_scale));
    return true;
}

void PhotoService::FreeFrame(Frame& frame) {
    if (frame.data) {
        heap_caps_free(frame.data);
        frame.data = nullptr;
    }
    frame.data_size = 0;
    memset(&frame.dsc, 0, sizeof(frame.dsc));
}

void PhotoService::ClearFrames() {
    FreeFrame(frames_[0]);
    FreeFrame(frames_[1]);
}

void PhotoService::SetUiState(const char* title, const char* detail) {
    if (!desktop_ui_) {
        return;
    }
    if (lvgl_port_lock(100)) {
        desktop_ui_->SetPhotoState(title, detail);
        lvgl_port_unlock();
    }
}

void PhotoService::SetUiFrame(const lv_img_dsc_t* frame, const char* title, const char* detail) {
    if (!desktop_ui_) {
        return;
    }
    if (lvgl_port_lock(100)) {
        desktop_ui_->SetPhotoFrame(frame, title, detail);
        lvgl_port_unlock();
    }
}
