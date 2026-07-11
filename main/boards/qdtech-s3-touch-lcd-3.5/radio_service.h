#pragma once

#include <cstdint>
#include <string>
#include <atomic>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class DesktopUI;

class RadioService {
public:
    void Start(DesktopUI* desktop_ui);
    void Play();
    void Pause();
    void PlayPause();
    void Stop();
    void Next();
    void Prev();
    std::string PlayUrlFromTool(const std::string& title, const std::string& artist, const std::string& url);
    std::string GetStatusJson() const;
    std::string SelectStation(const std::string& station);
    
    // 新增功能
    void ToggleFavorite(int index);
    bool IsFavorite(int index) const;
    std::vector<int> GetFavorites() const;
    std::vector<int> GetByCategory(int category) const;
    int GetCurrentIndex() const { return station_index_; }
    int GetStationCount() const;
    const char* GetStationName(int index) const;
    const char* GetStationCategory(int index) const;

private:
    enum class Command {
        PLAY_PAUSE,
        STOP,
        NEXT,
        PREV,
        FOCUS_CHANGED,
        PLAY_CUSTOM_URL,
    };

    void PostCommand(Command command);
    void Task();
    void HandleCommand(Command command);
    void PlayCurrentStation(uint32_t stream_generation);
    bool PlayUrl(const std::string& url, int url_index, uint32_t stream_generation);
    bool IsXiaozhiAudioState() const;
    bool IsCustomUrlSpeakingGraceActive() const;
    bool IsAutonomousCustomUrlSpeaking(int previous_state, int current_state) const;
    bool ShouldYieldAudio() const;
    void OnDeviceStateChanged(int previous_state, int current_state);
    void NextStation(int delta);
    void SetUi(const char* state, const char* detail);
    void WritePcm(const int16_t* pcm, int samples, int channels, int sample_rate,
                  int16_t* mono_buffer, int mono_capacity,
                  int16_t* output_buffer, int output_capacity);
    void ResetAudioLeveler();
    void ApplyAudioLeveler(int16_t* pcm, int samples);
    void LoadFavorites();
    void SaveFavorites();
    void LoadStationIndex();
    void SaveStationIndex();

    DesktopUI* desktop_ui_ = nullptr;
    void* queue_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    bool task_stack_internal_ = false;
    bool started_ = false;
    std::atomic<bool> play_requested_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> audio_focus_blocked_{false};
    bool focus_pause_logged_ = false;
    int reconnect_attempt_ = 0;
    std::vector<int> last_success_url_;
    int station_index_ = 0;
    int custom_station_index_ = -1;
    int last_radio_station_index_ = 0;
    bool playing_custom_url_ = false;
    bool custom_url_stream_completed_ = false;
    bool custom_url_fatal_error_ = false;
    bool last_url_permanent_error_ = false;
    bool skip_reconnect_once_ = false;
    TickType_t custom_url_speaking_grace_until_ = 0;
    std::atomic<uint32_t> stream_generation_{0};
    int32_t audio_gain_q12_ = 4096;
};
