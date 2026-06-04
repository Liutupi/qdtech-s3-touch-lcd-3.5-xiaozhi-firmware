#include "time_weather_service.h"

#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "cJSON.h"
#include "application.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "settings.h"
#include "wifi_station.h"

static const char* TAG = "TimeWeather";
static constexpr size_t WEATHER_RESPONSE_SIZE = 1536;

struct WeatherLocation {
    char city[32];
    double latitude;
    double longitude;
};

static bool ParseCoordinate(const std::string& text, double min_value, double max_value, double* value) {
    char* end = nullptr;
    double parsed = strtod(text.c_str(), &end);
    while (end && *end && isspace(static_cast<unsigned char>(*end))) {
        ++end;
    }
    if (!end || *end != 0 || parsed < min_value || parsed > max_value) {
        return false;
    }
    *value = parsed;
    return true;
}

static void LoadWeatherLocation(WeatherLocation* location) {
    snprintf(location->city, sizeof(location->city), "%s", "Zhongshan");
    location->latitude = 22.5176;
    location->longitude = 113.3928;

    Settings settings("weather_cfg", false);
    std::string city = settings.GetString("city", "Zhongshan");
    std::string lat_text = settings.GetString("lat", "22.5176");
    std::string lon_text = settings.GetString("lon", "113.3928");

    double latitude = 0.0;
    double longitude = 0.0;
    if (ParseCoordinate(lat_text, -90.0, 90.0, &latitude) &&
        ParseCoordinate(lon_text, -180.0, 180.0, &longitude)) {
        if (!city.empty()) {
            snprintf(location->city, sizeof(location->city), "%s", city.c_str());
        }
        location->latitude = latitude;
        location->longitude = longitude;
    }
}

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
    if (!task_handle_) {
        xTaskCreate(TaskWrapper, "time_weather", 6144, this, 2, &task_handle_);
    }
}

bool TimeWeatherService::SetLocation(const std::string& city, const std::string& latitude, const std::string& longitude) {
    double lat = 0.0;
    double lon = 0.0;
    if (city.empty() ||
        !ParseCoordinate(latitude, -90.0, 90.0, &lat) ||
        !ParseCoordinate(longitude, -180.0, 180.0, &lon)) {
        return false;
    }

    Settings settings("weather_cfg", true);
    settings.SetString("city", city);
    settings.SetString("lat", latitude);
    settings.SetString("lon", longitude);

    location_update_requested_.store(true);
    if (task_handle_) {
        xTaskNotifyGive(task_handle_);
    }
    ShowCachedWeather("Location updated");
    ESP_LOGI(TAG, "weather location updated: %s %.4f %.4f", city.c_str(), lat, lon);
    return true;
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
    WaitApplicationReady();
    
    while (true) {
        // 启动SNTP
        StartSntp();
        
        // 等待时间同步
        if (WaitTimeReady()) {
            ESP_LOGI(TAG, "Time synchronized");
            UpdateTime();
        } else {
            ESP_LOGW(TAG, "Time sync timeout");
        }
        
        // 获取天气
        if (weather_ticks >= 3600) {
            ShowCachedWeather("Weather sync");
            weather_ok = FetchWeather();
            weather_ticks = 0;
            retry_ticks = 0;
        } else if (!weather_ok && retry_ticks >= 10) {
            ShowCachedWeather("Weather retry");
            weather_ok = FetchWeather();
            retry_ticks = 0;
        }
        
        // 更新网络状态
        SetNetworkStatusSafe("XiaoZhi AI Ready");
        
        // 等待60秒
        for (int i = 0; i < 60; ++i) {
            if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) > 0 ||
                location_update_requested_.exchange(false)) {
                weather_ticks = 3600;
                retry_ticks = 3600;
                break;
            }
            if (!WifiStation::GetInstance().IsConnected()) {
                ESP_LOGW(TAG, "WiFi disconnected, waiting...");
                while (!WifiStation::GetInstance().IsConnected()) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                ESP_LOGI(TAG, "WiFi reconnected");
                break;
            }
            ++weather_ticks;
            ++retry_ticks;
        }
        UpdateTime();
    }
}

void TimeWeatherService::WaitApplicationReady() {
    for (int i = 0; i < 45; ++i) {
        auto state = Application::GetInstance().GetDeviceState();
        if (state != kDeviceStateStarting &&
            state != kDeviceStateActivating &&
            state != kDeviceStateConnecting) {
            ESP_LOGI(TAG, "Application ready for time weather sync");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGW(TAG, "Application readiness wait timed out, continuing");
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
#if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
    esp_sntp_setservername(1, "pool.ntp.org");
#endif
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
    
    if (lvgl_port_lock(100)) {
        desktop_ui_->SetTime(info.tm_hour, info.tm_min, info.tm_mon + 1, info.tm_mday, WeekdayName(info.tm_wday));
        lvgl_port_unlock();
    }
    
    // 每日金句
    if (info.tm_yday != last_quote_yday_) {
        last_quote_yday_ = info.tm_yday;
        const size_t quote_count = sizeof(DAILY_QUOTES) / sizeof(DAILY_QUOTES[0]);
        SetDailyQuoteSafe(DAILY_QUOTES[info.tm_yday % quote_count]);
    }
}

static esp_err_t HttpEventHandler(esp_http_client_event_t* evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->user_data && evt->data_len > 0) {
        char* buf = static_cast<char*>(evt->user_data);
        size_t used = strlen(buf);
        if (used >= WEATHER_RESPONSE_SIZE - 1) {
            return ESP_OK;
        }
        size_t room = WEATHER_RESPONSE_SIZE - used - 1;
        size_t copy = evt->data_len < static_cast<int>(room) ? evt->data_len : room;
        memcpy(buf + used, evt->data, copy);
        buf[used + copy] = 0;
    }
    return ESP_OK;
}

bool TimeWeatherService::FetchWeather() {
    WeatherLocation location = {};
    LoadWeatherLocation(&location);
    
    char url[256];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code&timezone=Asia%%2FShanghai",
             location.latitude, location.longitude);
    
    char response[WEATHER_RESPONSE_SIZE] = {};
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 20000;
    config.event_handler = HttpEventHandler;
    config.user_data = response;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.buffer_size = 1024;
    
    esp_err_t err = ESP_FAIL;
    int status = 0;
    for (int attempt = 1; attempt <= 3; ++attempt) {
        response[0] = 0;
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGW(TAG, "weather http init failed attempt=%d", attempt);
            err = ESP_ERR_NO_MEM;
        } else {
            esp_http_client_set_header(client, "Accept", "application/json");
            err = esp_http_client_perform(client);
            status = esp_http_client_get_status_code(client);
            esp_http_client_cleanup(client);
        }

        if (err == ESP_OK && status >= 200 && status < 300 && response[0] != 0) {
            break;
        }

        ESP_LOGW(TAG, "weather fetch failed attempt=%d err=%s status=%d", attempt, esp_err_to_name(err), status);
        if (attempt < 3) {
            vTaskDelay(pdMS_TO_TICKS(1500 * attempt));
        }
    }

    if (err != ESP_OK || status < 200 || status >= 300 || response[0] == 0) {
        char fail_text[40];
        snprintf(fail_text, sizeof(fail_text), "Weather fail %d", status);
        ShowCachedWeather(fail_text);
        return false;
    }
    
    cJSON* root = cJSON_Parse(response);
    cJSON* current = root ? cJSON_GetObjectItem(root, "current") : NULL;
    cJSON* temp = current ? cJSON_GetObjectItem(current, "temperature_2m") : NULL;
    cJSON* code = current ? cJSON_GetObjectItem(current, "weather_code") : NULL;
    
    if (!cJSON_IsNumber(temp) || !cJSON_IsNumber(code)) {
        ESP_LOGW(TAG, "weather parse failed: %.96s", response);
        cJSON_Delete(root);
        ShowCachedWeather("Weather parse");
        return false;
    }
    
    char temp_text[16];
    char summary[96];
    snprintf(temp_text, sizeof(temp_text), "%d C", (int)lround(temp->valuedouble));
    time_t now = 0;
    tm info = {};
    time(&now);
    localtime_r(&now, &info);
    if (info.tm_year >= (2024 - 1900)) {
        snprintf(last_update_, sizeof(last_update_), "%02d:%02d", info.tm_hour, info.tm_min);
    }
    snprintf(summary, sizeof(summary), "%s %s %s", location.city, WeatherDesc(code->valueint), last_update_);
    
    snprintf(last_temperature_, sizeof(last_temperature_), "%s", temp_text);
    snprintf(last_summary_, sizeof(last_summary_), "%s", summary);
    last_weather_code_ = code->valueint;
    last_weather_valid_ = true;
    SetWeatherSafe(temp_text, summary, code->valueint);
    
    cJSON_Delete(root);
    ESP_LOGI(TAG, "weather ok %s %s code=%d updated=%s", temp_text, summary, code->valueint, last_update_);
    return true;
}

void TimeWeatherService::ShowCachedWeather(const char* status) {
    char summary[128];
    if (last_weather_valid_) {
        snprintf(summary, sizeof(summary), "%s; %s", status ? status : "Cached", last_summary_);
        SetWeatherSafe(last_temperature_, summary, last_weather_code_);
        ESP_LOGW(TAG, "weather using cached data temp=%s summary=%s", last_temperature_, last_summary_);
    } else {
        SetWeatherSafe("-- C", status ? status : "Weather pending", -1);
    }
}

void TimeWeatherService::SetWeatherSafe(const char* temperature, const char* summary, int weather_code) {
    if (!desktop_ui_) {
        return;
    }
    if (lvgl_port_lock(100)) {
        desktop_ui_->SetWeather(temperature, summary, weather_code);
        lvgl_port_unlock();
    }
}

void TimeWeatherService::SetNetworkStatusSafe(const char* status) {
    if (!desktop_ui_) {
        return;
    }
    if (lvgl_port_lock(100)) {
        desktop_ui_->SetNetworkStatus(status);
        lvgl_port_unlock();
    }
}

void TimeWeatherService::SetDailyQuoteSafe(const char* quote) {
    if (!desktop_ui_) {
        return;
    }
    if (lvgl_port_lock(100)) {
        desktop_ui_->SetDailyQuote(quote);
        lvgl_port_unlock();
    }
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
