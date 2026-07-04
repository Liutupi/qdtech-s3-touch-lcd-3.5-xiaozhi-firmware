#pragma once

#include "desktop_ui.h"

#include <atomic>
#include <mutex>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class TimeWeatherService {
public:
    void Start(DesktopUI* ui);
    bool SetLocation(const std::string& city, const std::string& latitude, const std::string& longitude);
    void RequestRefresh(bool retry_weather);
    
private:
    DesktopUI* desktop_ui_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    std::atomic<bool> location_update_requested_{false};
    std::atomic<bool> refresh_requested_{false};
    std::atomic<bool> weather_refresh_requested_{false};
    bool sntp_started_ = false;
    bool sntp_synced_once_ = false;
    bool last_weather_valid_ = false;
    bool weather_low_memory_deferred_ = false;
    int weather_failure_count_ = 0;
    std::mutex location_mutex_;
    std::string location_city_ = "Zhongshan";
    double location_latitude_ = 22.5176;
    double location_longitude_ = 113.3928;
    bool location_loaded_ = false;
    char last_temperature_[16] = "-- C";
    char last_summary_[96] = "Weather pending";
    char last_update_[24] = "never";
    int last_weather_code_ = -1;
    int last_daily_card_date_ = -1;
    
    static void TaskWrapper(void* arg);
    void Task();
    void WaitApplicationReady();
    void LoadLocationCacheFromNvs();
    void CopyLocationCache(std::string* city, double* latitude, double* longitude);
    void StartSntp();
    bool WaitTimeReady();
    void UpdateTime();
    bool FetchWeather();
    int WeatherRetrySeconds() const;
    void ShowCachedWeather(const char* status);
    void SetWeatherSafe(const char* temperature, const char* summary, int weather_code);
    void SetNetworkStatusSafe(const char* status);
    void SetDailyQuoteSafe(const char* quote);
    void SetDailyCardSafe(const char* date, const char* title, const char* body);
    
    static const char* WeekdayName(int wday);
    static const char* WeatherDesc(int code);
};
