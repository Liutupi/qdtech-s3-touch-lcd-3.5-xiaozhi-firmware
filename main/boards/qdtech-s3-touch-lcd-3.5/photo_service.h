#pragma once

#include "desktop_ui.h"
#include "lvgl.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <sdmmc_cmd.h>

class PhotoService {
public:
    void Start(DesktopUI* desktop_ui);
    void SetActive(bool active);
    void Refresh();

private:
    struct Frame {
        lv_img_dsc_t dsc = {};
        uint8_t* data = nullptr;
        size_t data_size = 0;
    };

    DesktopUI* desktop_ui_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    std::atomic<bool> active_{false};
    std::atomic<bool> refresh_requested_{false};
    bool mounted_ = false;
    esp_err_t last_mount_error_ = ESP_OK;
    uint8_t last_mount_width_ = 0;
    uint32_t mount_attempts_ = 0;
    sdmmc_card_t* card_ = nullptr;
    std::vector<std::string> photos_;
    size_t current_index_ = 0;
    Frame frames_[2];
    uint8_t frame_slot_ = 0;

    static void TaskWrapper(void* arg);
    void EnsureTaskStarted();
    void TaskLoop();
    bool MountSdCard();
    bool TryMountSdCard(uint8_t width, int max_freq_khz);
    bool ScanPhotos();
    void ScanPhotoDir(const char* dir_path);
    bool DecodePhoto(const std::string& path, Frame& frame, uint16_t& width, uint16_t& height);
    void FreeFrame(Frame& frame);
    void ClearFrames();
    void SetUiState(const char* title, const char* detail);
    void SetUiFrame(const lv_img_dsc_t* frame, const char* title, const char* detail);
};
