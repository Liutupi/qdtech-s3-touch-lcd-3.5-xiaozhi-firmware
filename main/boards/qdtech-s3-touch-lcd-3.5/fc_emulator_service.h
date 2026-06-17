#pragma once

#include "desktop_ui.h"
#include "nes_bus.h"
#include "lvgl.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdmmc_cmd.h>

class FcEmulatorService {
public:
    void Start(DesktopUI* desktop_ui);
    void SetActive(bool active);
    void PlayPause();
    void Stop();
    void Next();
    void Prev();
    void SetController(uint8_t controller);

private:
    struct Frame {
        lv_img_dsc_t dsc = {};
        uint16_t* pixels = nullptr;
        size_t pixel_count = 0;
    };

    DesktopUI* desktop_ui_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    std::atomic<bool> active_{false};
    std::atomic<bool> playing_{false};
    std::atomic<bool> scan_requested_{false};
    bool mounted_ = false;
    sdmmc_card_t* card_ = nullptr;
    std::vector<std::string> roms_;
    size_t selected_index_ = 0;
    uint32_t frame_counter_ = 0;
    uint32_t sd_mount_failures_ = 0;
    Frame frames_[2];
    uint8_t frame_slot_ = 0;
    int64_t last_frame_publish_us_ = 0;

    NesBus nes_bus_;
    bool nes_initialized_ = false;
    std::atomic<uint8_t> controller_state_{0};
    std::atomic<uint32_t> controller_release_tick_{0};
    uint8_t last_logged_controller_state_ = 0xff;

    static void TaskWrapper(void* arg);
    void EnsureTaskStarted();
    void TaskLoop();
    bool MountSdCard();
    bool TryMountSdCard(uint8_t width, int max_freq_khz);
    bool ScanRoms();
    void ScanRomDir(const char* dir_path);
    bool ValidateSelectedRom();
    bool LoadNesRom();
    bool RunNesFrame();
    void RenderFrame(Frame& frame, bool update_pixels);
    void FreeFrame(Frame& frame);
    void ClearFrames();
    void PublishState(const char* title, const char* detail);
    void PublishMode(bool playing);
    void PublishFrame(const lv_img_dsc_t* frame);
    std::string SelectedName() const;
    std::string BuildRomList() const;
};
