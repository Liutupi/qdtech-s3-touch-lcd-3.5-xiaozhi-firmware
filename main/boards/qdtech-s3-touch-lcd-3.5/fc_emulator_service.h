#pragma once

#include "desktop_ui.h"
#include "lvgl.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdmmc_cmd.h>

class FcEmulatorService {
public:
    using DirectFrameCallback = std::function<bool(const uint16_t* pixels, uint16_t width, uint16_t height)>;

    void Start(DesktopUI* desktop_ui);
    void SetDirectFrameCallback(DirectFrameCallback callback);
    void SetActive(bool active);
    bool PrepareSdCard();
    void PrepareTask();
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
    std::atomic<bool> scanning_{false};
    std::atomic<bool> scan_requested_{false};
    std::atomic<bool> start_requested_{false};
    std::atomic<bool> release_roms_requested_{false};
    bool mounted_ = false;
    sdmmc_card_t* card_ = nullptr;
    std::vector<std::string> roms_;
    std::vector<std::pair<std::string, std::string>> rom_aliases_;
    std::atomic<size_t> selected_index_{0};
    uint32_t frame_counter_ = 0;
    uint32_t produced_frame_counter_ = 0;
    uint32_t last_perf_frame_counter_ = 0;
    int64_t last_perf_log_us_ = 0;
    uint32_t sd_mount_failures_ = 0;
    Frame frames_[2];
    uint8_t frame_slot_ = 0;
    int64_t last_frame_publish_us_ = 0;
    uint32_t audio_frame_counter_ = 0;
    int64_t last_audio_log_us_ = 0;

    uint8_t last_logged_controller_state_ = 0xff;

    std::atomic<uint8_t> controller_state_{0};
    std::atomic<uint32_t> controller_release_tick_{0};

    static void TaskWrapper(void* arg);
    static int NofrendoFrameThunk(const uint16_t* pixels, uint16_t width, uint16_t height, void* user);
    static void NofrendoAudioThunk(const int16_t* samples, int sample_count, int sample_rate, void* user);
    void EnsureTaskStarted();
    void TaskLoop();
    bool MountSdCard();
    bool TryMountSdCard(uint8_t width, int max_freq_khz);
    bool ScanRoms();
    size_t ScanRomDir(const char* dir_path);
    void LoadRomAliases(const char* dir_path);
    bool LoadRomAliasFile(const char* path);
    bool ValidateSelectedRom();
    void RunNofrendoRom();
    void FreeFrame(Frame& frame);
    void ClearFrames();
    void PublishState(const char* title, const char* detail);
    void PublishMode(bool playing);
    bool PublishFrame(Frame& frame);
    bool PublishLvglFrame(const lv_img_dsc_t* frame);
    bool PublishDirectFrame(const uint16_t* pixels, uint16_t width, uint16_t height);
    void WriteNofrendoAudio(const int16_t* samples, int sample_count, int sample_rate);
    std::string RomDisplayName(const std::string& path, size_t max_chars = 22) const;
    std::string SelectedName() const;
    std::string BuildRomList() const;

    DirectFrameCallback direct_frame_cb_;
    std::vector<int16_t> audio_output_buf_;
};
