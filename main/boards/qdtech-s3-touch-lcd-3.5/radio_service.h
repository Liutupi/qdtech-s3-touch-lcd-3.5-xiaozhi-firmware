#pragma once

#include <cstdint>
#include <string>
#include <atomic>
#include <vector>

class DesktopUI;

class RadioService {
public:
    void Start(DesktopUI* desktop_ui);
    void Play();
    void PlayPause();
    void Stop();
    void Next();
    void Prev();
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
    };

    void PostCommand(Command command);
    void Task();
    void HandleCommand(Command command);
    void PlayCurrentStation();
    bool PlayUrl(const char* url, int url_index);
    bool IsXiaozhiAudioState() const;
    bool ShouldYieldAudio() const;
    void OnDeviceStateChanged(int previous_state, int current_state);
    void NextStation(int delta);
    void SetUi(const char* state, const char* detail);
    void WritePcm(const int16_t* pcm, int samples, int channels, int sample_rate);
    void LoadFavorites();
    void SaveFavorites();
    void LoadStationIndex();
    void SaveStationIndex();

    DesktopUI* desktop_ui_ = nullptr;
    void* queue_ = nullptr;
    bool started_ = false;
    bool play_requested_ = false;
    bool stop_requested_ = false;
    std::atomic<bool> audio_focus_blocked_{false};
    bool focus_pause_logged_ = false;
    int reconnect_attempt_ = 0;
    int last_success_url_[16] = {};
    int station_index_ = 0;
};
