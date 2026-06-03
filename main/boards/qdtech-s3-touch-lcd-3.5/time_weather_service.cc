#include "time_weather_service.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_station.h"

static const char* TAG = "TimeWeather";

static const char* DAILY_QUOTES[] = {
    "Stay patient and trust your next small step.",
    "Make today lighter, clearer, and a little braver.",
    "A calm mind turns hard work into steady progress.",
    "Do the useful thing first; confidence follows.",
    "Small wins compound when you keep showing up.",
    "The day opens when attention becomes simple.",
    "Less noise, more signal, one good move at a time.",
    "What you build with care keeps working for you.",
    "Begin before it feels perfect.",
    "Clarity is made by moving, not waiting.",
    "A focused hour can change the shape of the day.",
    "Keep the promise small enough to keep.",
    "Good work is quiet before it is obvious.",
    "Choose the next honest action.",
    "Energy follows direction.",
    "Steady is faster than scattered.",
    "Leave the page better than you found it.",
    "The future likes prepared hands.",
    "One clean decision clears a crowded room.",
    "Build the rhythm, then let the rhythm carry you.",
    "You do not need more pressure; you need a clearer path.",
    "Attention is the first form of care.",
    "Progress often sounds like a quiet yes.",
    "Make room for the better version to arrive.",
    "The work becomes easier when the next step is visible.",
    "Trust repetition. It is how skill learns your name.",
    "A good day starts by removing one needless thing.",
    "Let the simple thing be enough to begin.",
    "Direction beats intensity when the road is long.",
    "Your next step is allowed to be modest.",
    "Keep your standards high and your breath slow.",
};

void TimeWeatherService::Start(DesktopUI* ui) {
    desktop_ui_ = ui;
    xTaskCreate(TaskWrapper, "time_weather", 6144, this, 4, NULL);
}

void TimeWeatherService::TaskWrapper(void* arg) {
    static_cast<TimeWeatherService*>(arg)->Task();
}

void TimeWeatherService::Task() {
    int weather_ticks = 3600;
    int retry_ticks = 3600;
    bool weather_ok = false;
    
    // 等待 WiFi 连接
    while (!WifiStation::GetInstance().IsConnected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "WiFi connected, starting time weather service");
    
    while (true) {
        // 启动SNTP
        StartSntp();
        
        // 等待时间同步
        if (WaitTimeReady()) {
            UpdateTime();
        }
        
        // 获取天气
        if (weather_ticks >= 3600) {
            if (desktop_ui_) {
                desktop_ui_->SetWeather("-- C", "Weather sync", -1);
            }
            weather_ok = FetchWeather();
            weather_ticks = 0;
            retry_ticks = 0;
        } else if (!weather_ok && retry_ticks >= 30) {
            weather_ok = FetchWeather();
            retry_ticks = 0;
        }
        
        // 更新网络状态
        if (desktop_ui_) {
            desktop_ui_->SetNetworkStatus("XiaoZhi AI Ready");
        }
        
        // 等待60秒
        for (int i = 0; i < 60; ++i) {
            if (!WifiStation::GetInstance().IsConnected()) {
                ESP_LOGW(TAG, "WiFi disconnected, waiting...");
                while (!WifiStation::GetInstance().IsConnected()) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                ESP_LOGI(TAG, "WiFi reconnected");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            ++weather_ticks;
            ++retry_ticks;
        }
        UpdateTime();
    }
}

void TimeWeatherService::StartSntp() {
    if (sntp_started_) {
        return;
    }
    sntp_started_ = true;
    setenv("TZ", "CST-8", 1);
    tzset();
    
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "pool.ntp.org");
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP started");
}

bool TimeWeatherService::WaitTimeReady() {
    time_t now = 0;
    tm timeinfo = {};
    for (int i = 0; i < 30; ++i) {
        time(&now);
        localtime_r(&now, &timeinfo);
        if (timeinfo.tm_year >= (2024 - 1900)) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return false;
}

void TimeWeatherService::UpdateTime() {
    if (!desktop_ui_) {
        return;
    }
    
    time_t now = 0;
    tm info = {};
    time(&now);
    localtime_r(&now, &info);
    if (info.tm_year < (2024 - 1900)) {
        return;
    }
    
    desktop_ui_->SetTime(info.tm_hour, info.tm_min, info.tm_mon + 1, info.tm_mday, WeekdayName(info.tm_wday));
    
    // 每日金句
    if (info.tm_yday != last_quote_yday_) {
        last_quote_yday_ = info.tm_yday;
        const size_t quote_count = sizeof(DAILY_QUOTES) / sizeof(DAILY_QUOTES[0]);
        desktop_ui_->SetDailyQuote(DAILY_QUOTES[info.tm_yday % quote_count]);
    }
}

bool TimeWeatherService::FetchWeather() {
    // 默认位置：上海
    double latitude = 31.2304;
    double longitude = 121.4737;
    const char* city = "Shanghai";
    
    char url[256];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code&timezone=Asia%%2FShanghai",
             latitude, longitude);
    
    char response[1536] = {};
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 8000;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return false;
    }
    
    // 设置响应缓冲区
    esp_http_client_set_header(client, "Accept", "application/json");
    
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    
    if (err == ESP_OK) {
        int content_length = esp_http_client_get_content_length(client);
        if (content_length > 0 && content_length < (int)sizeof(response)) {
            esp_http_client_read(client, response, content_length);
        }
    }
    
    esp_http_client_cleanup(client);
    
    if (err != ESP_OK || status < 200 || status >= 300 || response[0] == 0) {
        ESP_LOGW(TAG, "weather fetch failed err=%s status=%d", esp_err_to_name(err), status);
        if (desktop_ui_) {
            char fail_text[32];
            snprintf(fail_text, sizeof(fail_text), "Weather %d", status);
            desktop_ui_->SetWeather("-- C", fail_text, -1);
        }
        return false;
    }
    
    cJSON* root = cJSON_Parse(response);
    cJSON* current = root ? cJSON_GetObjectItem(root, "current") : NULL;
    cJSON* temp = current ? cJSON_GetObjectItem(current, "temperature_2m") : NULL;
    cJSON* code = current ? cJSON_GetObjectItem(current, "weather_code") : NULL;
    
    if (!cJSON_IsNumber(temp) || !cJSON_IsNumber(code)) {
        ESP_LOGW(TAG, "weather parse failed: %.96s", response);
        cJSON_Delete(root);
        if (desktop_ui_) {
            desktop_ui_->SetWeather("-- C", "Weather parse", -1);
        }
        return false;
    }
    
    char temp_text[16];
    char summary[48];
    snprintf(temp_text, sizeof(temp_text), "%d C", (int)lround(temp->valuedouble));
    snprintf(summary, sizeof(summary), "%s %s", city, WeatherDesc(code->valueint));
    
    if (desktop_ui_) {
        desktop_ui_->SetWeather(temp_text, summary, code->valueint);
    }
    
    cJSON_Delete(root);
    ESP_LOGI(TAG, "weather ok %s %s code=%d", temp_text, summary, code->valueint);
    return true;
}

const char* TimeWeatherService::WeekdayName(int wday) {
    static const char* names[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    if (wday < 0 || wday > 6) {
        return "---";
    }
    return names[wday];
}

const char* TimeWeatherService::WeatherDesc(int code) {
    if (code == 0) return "Sunny";
    if (code == 1 || code == 2 || code == 3) return "Cloudy";
    if (code == 45 || code == 48) return "Fog";
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return "Rain";
    if (code >= 71 && code <= 77) return "Snow";
    if (code >= 95) return "Storm";
    return "Weather";
}
