#pragma once

#include "lvgl.h"

#include <atomic>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

class DesktopUI;

class PodcastService {
public:
    void Start(DesktopUI* desktop_ui);
    void PlayPause();
    void Stop();
    void Next();
    void Prev();
    void Up();
    void Down();
    void SeekPercent(int percent);

private:
    enum class Command {
        PLAY_PAUSE,
        STOP,
        NEXT,
        PREV,
        UP,
        DOWN,
        SEEK,
        REFRESH_UI,
        FOCUS_CHANGED,
    };

    struct Episode {
        int vol = 0;
        std::string title;
        std::string audio;
        std::string cover;
        std::string desc;
        std::string summary;
    };

    struct CoverFrame {
        lv_img_dsc_t dsc = {};
        uint8_t* data = nullptr;
    };

    void EnsureTaskStarted();
    void PostCommand(Command command);
    void Task();
    void HandleCommand(Command command);
    bool EnsureSdCardMounted();
    bool LoadIndex();
    bool LoadSummary(Episode& episode);
    void EnsureSelectedDetails(bool decode_cover);
    bool DecodeCover(const std::string& path);
    void FreeCover();
    void UpdateUi(const char* state, const char* detail);
    void UpdateProgressUi(int percent);
    void UpdateCoverUi();
    void SelectDelta(int delta, bool interrupt_playback);
    void PlayCurrentEpisode();
    bool PlayFile(const char* path);
    bool HandlePlaybackCommand(Command command, FILE* file, long file_size, uint8_t* read_buffer,
                               uint8_t*& read_ptr, int& bytes_left, long& total_read,
                               bool& eof, int& decode_errors);
    bool IsXiaozhiAudioState() const;
    bool ShouldYieldAudio() const;
    void OnDeviceStateChanged(int previous_state, int current_state);
    void WritePcm(const int16_t* pcm, int samples, int channels, int sample_rate);
    void ResetAudioLeveler();
    void ApplyAudioLeveler(std::vector<int16_t>& pcm);

    DesktopUI* desktop_ui_ = nullptr;
    void* queue_ = nullptr;
    void* task_handle_ = nullptr;
    std::atomic<bool> play_requested_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> audio_focus_blocked_{false};
    std::atomic<int> pending_seek_percent_{-1};
    bool started_ = false;
    bool index_loaded_ = false;
    bool focus_pause_logged_ = false;
    int selected_index_ = 0;
    int playing_index_ = -1;
    size_t list_top_ = 0;
    std::vector<Episode> episodes_;
    CoverFrame cover_;
    std::vector<int16_t> pcm_mono_buf_;
    std::vector<int16_t> pcm_output_buf_;
    int32_t audio_gain_q12_ = 4096;
};
