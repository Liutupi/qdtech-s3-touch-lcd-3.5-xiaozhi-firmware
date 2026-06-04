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
#include <freertos/task.h>
#include <jpeg_decoder.h>

#define TAG "PhotoService"

namespace {
constexpr const char* kMountPoint = "/sdcard";
constexpr const char* kPhotoDir = "/sdcard/photos";
constexpr size_t kMaxInputBytes = 4 * 1024 * 1024;
constexpr size_t kMaxOutputBytes = 960 * 640 * 2;
constexpr TickType_t kSlideDelay = pdMS_TO_TICKS(6000);
constexpr TickType_t kIdleDelay = pdMS_TO_TICKS(500);

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
    if (task_handle_) {
        return;
    }
    desktop_ui_ = desktop_ui;
    if (desktop_ui_) {
        desktop_ui_->SetPhotoActiveCallback([this](bool active) { SetActive(active); });
        desktop_ui_->SetPhotoRefreshCallback([this]() { Refresh(); });
    }
    xTaskCreate(TaskWrapper, "photo_service", 8192, this, 2, &task_handle_);
    ESP_LOGI(TAG, "photo service started");
}

void PhotoService::SetActive(bool active) {
    const bool previous = active_.exchange(active);
    if (previous != active) {
        ESP_LOGI(TAG, "photo playback %s", active ? "active" : "paused");
        if (active) {
            refresh_requested_.store(true);
        }
    }
}

void PhotoService::Refresh() {
    refresh_requested_.store(true);
    ESP_LOGI(TAG, "photo refresh requested");
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
            SetUiState("SD card not ready", "Check MicroSD card");
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        if (refresh_requested_.exchange(false) || photos_.empty()) {
            if (!ScanPhotos()) {
                SetUiState("No photos", "Put JPG files in /photos");
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

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = PHOTO_SDMMC_BUS_WIDTH;
    slot_config.clk = PHOTO_SDMMC_CLK_PIN;
    slot_config.cmd = PHOTO_SDMMC_CMD_PIN;
    slot_config.d0 = PHOTO_SDMMC_D0_PIN;
    slot_config.d1 = PHOTO_SDMMC_D1_PIN;
    slot_config.d2 = PHOTO_SDMMC_D2_PIN;
    slot_config.d3 = PHOTO_SDMMC_D3_PIN;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    ESP_LOGI(TAG, "mount sdmmc clk=%d cmd=%d d0=%d d1=%d d2=%d d3=%d width=%d",
             PHOTO_SDMMC_CLK_PIN, PHOTO_SDMMC_CMD_PIN, PHOTO_SDMMC_D0_PIN,
             PHOTO_SDMMC_D1_PIN, PHOTO_SDMMC_D2_PIN, PHOTO_SDMMC_D3_PIN,
             PHOTO_SDMMC_BUS_WIDTH);
    esp_err_t err = esp_vfs_fat_sdmmc_mount(kMountPoint, &host, &slot_config, &mount_config, &card_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "sd card mount failed: %s", esp_err_to_name(err));
        card_ = nullptr;
        return false;
    }

    mounted_ = true;
    sdmmc_card_print_info(stdout, card_);
    ESP_LOGI(TAG, "sd card mounted");
    return true;
}

bool PhotoService::ScanPhotos() {
    photos_.clear();
    current_index_ = 0;

    DIR* dir = opendir(kPhotoDir);
    if (!dir) {
        ESP_LOGW(TAG, "open photo dir failed: %s", kPhotoDir);
        return false;
    }

    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR || !ends_with_photo_ext(entry->d_name)) {
            continue;
        }
        std::string path = std::string(kPhotoDir) + "/" + entry->d_name;
        photos_.push_back(path);
    }
    closedir(dir);

    std::sort(photos_.begin(), photos_.end());
    ESP_LOGI(TAG, "photo scan found %u jpg files", static_cast<unsigned>(photos_.size()));
    return !photos_.empty();
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
            .swap_color_bytes = 1,
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
