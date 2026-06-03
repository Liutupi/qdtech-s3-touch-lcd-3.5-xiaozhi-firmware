#pragma once

#include "desktop_ui.h"

class TimeWeatherService {
public:
    void Start(DesktopUI* ui);
    
private:
    DesktopUI* desktop_ui_ = nullptr;
    bool sntp_started_ = false;
    int last_quote_yday_ = -1;
    
    static void TaskWrapper(void* arg);
    void Task();
    void StartSntp();
    bool WaitTimeReady();
    void UpdateTime();
    bool FetchWeather();
    
    static const char* WeekdayName(int wday);
    static const char* WeatherDesc(int code);
};
