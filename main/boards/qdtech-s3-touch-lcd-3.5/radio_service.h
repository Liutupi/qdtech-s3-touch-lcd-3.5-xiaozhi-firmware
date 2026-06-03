#pragma once

#include <cstdint>
#include <string>

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

private:
    enum class Command {
        PLAY_PAUSE,
        STOP,
        NEXT,
        PREV,
    };

    void PostCommand(Command command);
    void Task();
    void HandleCommand(Command command);
    void PlayCurrentStation();
    bool PlayUrl(const char* url, int url_index);
    void NextStation(int delta);
    void SetUi(const char* state, const char* detail);
    void WritePcm(const int16_t* pcm, int samples, int channels, int sample_rate);

    DesktopUI* desktop_ui_ = nullptr;
    void* queue_ = nullptr;
    bool started_ = false;
    bool play_requested_ = false;
    bool stop_requested_ = false;
    int station_index_ = 0;
};
