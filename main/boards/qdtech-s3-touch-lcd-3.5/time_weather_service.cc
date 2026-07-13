#include "time_weather_service.h"

#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "cJSON.h"
#include "application.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "settings.h"
#include "wifi_station.h"

static const char* TAG = "TimeWeather";
static constexpr size_t WEATHER_RESPONSE_SIZE = 2048;
static constexpr size_t kWeatherMinInternalFree = 7 * 1024;
// HTTP weather uses small PSRAM-backed buffers; keep enough contiguous internal
// memory for lwIP bookkeeping without blocking forever after a long music stream.
static constexpr size_t kWeatherMinLargestInternalBlock = 5 * 1024;

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

static double JsonNumberOr(cJSON* object, const char* key, double fallback) {
    cJSON* item = object ? cJSON_GetObjectItem(object, key) : nullptr;
    return cJSON_IsNumber(item) ? item->valuedouble : fallback;
}

static int JsonIntOr(cJSON* object, const char* key, int fallback) {
    cJSON* item = object ? cJSON_GetObjectItem(object, key) : nullptr;
    return cJSON_IsNumber(item) ? item->valueint : fallback;
}

static bool ExtractWeatherTime(cJSON* current, char* out, size_t out_size) {
    cJSON* time = current ? cJSON_GetObjectItem(current, "time") : nullptr;
    if (!cJSON_IsString(time) || !time->valuestring || !out || out_size < 6) {
        return false;
    }
    const char* t = time->valuestring;
    const char* hour = strchr(t, 'T');
    if (!hour || strlen(hour) < 6) {
        return false;
    }
    snprintf(out, out_size, "%.5s", hour + 1);
    return true;
}

static int RefineOpenMeteoCode(int raw_code, double precipitation, double rain,
                               double showers, int cloud_cover) {
    const double wet = fmax(precipitation, fmax(rain, showers));
    if (raw_code >= 95 && wet < 0.1) {
        return cloud_cover >= 70 ? 3 : (cloud_cover >= 35 ? 2 : 1);
    }
    if ((raw_code >= 51 && raw_code <= 67) || (raw_code >= 80 && raw_code <= 82)) {
        if (wet < 0.1) {
            return cloud_cover >= 70 ? 3 : 2;
        }
    }
    if (raw_code == 0 && cloud_cover >= 80) {
        return 3;
    }
    if (raw_code == 0 && cloud_cover >= 45) {
        return 2;
    }
    return raw_code;
}

void TimeWeatherService::LoadLocationCacheFromNvs() {
    std::string city = "Zhongshan";
    double latitude = 22.5176;
    double longitude = 113.3928;
    Settings settings("weather_cfg", false);
    std::string city_text = settings.GetString("city", "Zhongshan");
    std::string lat_text = settings.GetString("lat", "22.5176");
    std::string lon_text = settings.GetString("lon", "113.3928");

    double parsed_lat = 0.0;
    double parsed_lon = 0.0;
    if (ParseCoordinate(lat_text, -90.0, 90.0, &parsed_lat) &&
        ParseCoordinate(lon_text, -180.0, 180.0, &parsed_lon)) {
        latitude = parsed_lat;
        longitude = parsed_lon;
        if (!city_text.empty()) {
            city = city_text;
        }
    }

    std::lock_guard<std::mutex> lock(location_mutex_);
    location_city_ = city;
    location_latitude_ = latitude;
    location_longitude_ = longitude;
    location_loaded_ = true;
}

void TimeWeatherService::CopyLocationCache(std::string* city, double* latitude, double* longitude) {
    std::lock_guard<std::mutex> lock(location_mutex_);
    if (city) {
        *city = location_city_;
    }
    if (latitude) {
        *latitude = location_latitude_;
    }
    if (longitude) {
        *longitude = location_longitude_;
    }
}

struct FestivalEntry {
    uint8_t month;
    uint8_t day;
    const char* title;
    const char* text;
};

struct DatedFestivalEntry {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    const char* title;
    const char* text;
};

struct HistoryEntry {
    uint8_t month;
    uint8_t day;
    const char* year;
    const char* text;
};

static const FestivalEntry FESTIVALS[] = {
    {1, 1, "元旦", "新的一年，愿你日日有光。"},
    {2, 14, "情人节", "愿爱与被爱，都温柔以待。"},
    {3, 8, "妇女节", "愿她们自在生长，温柔而有力量。"},
    {3, 12, "植树节", "种下一点绿色，也种下一点希望。"},
    {4, 1, "愚人节", "生活需要一点轻松的玩笑。"},
    {5, 1, "劳动节", "向认真生活的人致敬。"},
    {5, 4, "青年节", "愿热爱不老，脚步不停。"},
    {5, 12, "护士节", "白衣执甲，温柔守护。"},
    {6, 1, "儿童节", "愿童心常在，眼里有光。"},
    {6, 7, "高考日", "十二年磨一剑，愿你从容落笔。"},
    {7, 1, "建党节", "初心如磐，步履向前。"},
    {8, 1, "建军节", "山河有守，万家灯火。"},
    {9, 10, "教师节", "一支粉笔，点亮许多远方。"},
    {10, 1, "国庆节", "山河锦绣，家国同庆。"},
    {10, 31, "万圣节", "今夜扮鬼，明天继续可爱。"},
    {11, 11, "双十一", "理性消费，快乐购物。"},
    {12, 24, "平安夜", "愿今夜平安，岁岁年年。"},
    {12, 25, "圣诞节", "愿冬夜有暖，心中有光。"},
    {12, 31, "跨年夜", "旧岁已展千重锦，新年再进百尺竿。"},
};

static const DatedFestivalEntry LUNAR_FESTIVALS[] = {
    {2024, 2, 10, "春节", "新春开岁，愿你万事顺意。"},
    {2024, 2, 24, "元宵节", "灯火映团圆，心里有暖光。"},
    {2024, 6, 10, "端午节", "粽叶飘香，愿你安康顺遂。"},
    {2024, 8, 10, "七夕", "星河有约，温柔常在。"},
    {2024, 9, 17, "中秋节", "月满人团圆，清辉照归途。"},
    {2024, 10, 11, "重阳节", "登高望远，岁岁安康。"},
    {2025, 1, 7, "腊八节", "一碗腊八粥，暖过岁寒时。"},
    {2025, 1, 28, "除夕", "旧岁至此团圆，新年就在门前。"},
    {2025, 1, 29, "春节", "新春开岁，愿你万事顺意。"},
    {2025, 2, 12, "元宵节", "灯火映团圆，心里有暖光。"},
    {2025, 5, 31, "端午节", "粽叶飘香，愿你安康顺遂。"},
    {2025, 8, 29, "七夕", "星河有约，温柔常在。"},
    {2025, 10, 6, "中秋节", "月满人团圆，清辉照归途。"},
    {2025, 10, 29, "重阳节", "登高望远，岁岁安康。"},
    {2026, 1, 26, "腊八节", "一碗腊八粥，暖过岁寒时。"},
    {2026, 2, 16, "除夕", "旧岁至此团圆，新年就在门前。"},
    {2026, 2, 17, "春节", "新春开岁，愿你万事顺意。"},
    {2026, 3, 3, "元宵节", "灯火映团圆，心里有暖光。"},
    {2026, 6, 19, "端午节", "粽叶飘香，愿你安康顺遂。"},
    {2026, 8, 19, "七夕", "星河有约，温柔常在。"},
    {2026, 9, 25, "中秋节", "月满人团圆，清辉照归途。"},
    {2026, 10, 18, "重阳节", "登高望远，岁岁安康。"},
    {2027, 1, 15, "腊八节", "一碗腊八粥，暖过岁寒时。"},
    {2027, 2, 5, "除夕", "旧岁至此团圆，新年就在门前。"},
    {2027, 2, 6, "春节", "新春开岁，愿你万事顺意。"},
    {2027, 2, 20, "元宵节", "灯火映团圆，心里有暖光。"},
    {2027, 6, 9, "端午节", "粽叶飘香，愿你安康顺遂。"},
    {2027, 8, 8, "七夕", "星河有约，温柔常在。"},
    {2027, 9, 15, "中秋节", "月满人团圆，清辉照归途。"},
    {2027, 10, 8, "重阳节", "登高望远，岁岁安康。"},
    {2028, 1, 4, "腊八节", "一碗腊八粥，暖过岁寒时。"},
    {2028, 1, 25, "除夕", "旧岁至此团圆，新年就在门前。"},
    {2028, 1, 26, "春节", "新春开岁，愿你万事顺意。"},
    {2028, 2, 9, "元宵节", "灯火映团圆，心里有暖光。"},
    {2028, 5, 28, "端午节", "粽叶飘香，愿你安康顺遂。"},
    {2028, 8, 26, "七夕", "星河有约，温柔常在。"},
    {2028, 10, 3, "中秋节", "月满人团圆，清辉照归途。"},
    {2028, 10, 26, "重阳节", "登高望远，岁岁安康。"},
    {2029, 1, 22, "腊八节", "一碗腊八粥，暖过岁寒时。"},
    {2029, 2, 12, "除夕", "旧岁至此团圆，新年就在门前。"},
    {2029, 2, 13, "春节", "新春开岁，愿你万事顺意。"},
    {2029, 2, 27, "元宵节", "灯火映团圆，心里有暖光。"},
    {2029, 6, 16, "端午节", "粽叶飘香，愿你安康顺遂。"},
    {2029, 8, 16, "七夕", "星河有约，温柔常在。"},
    {2029, 9, 22, "中秋节", "月满人团圆，清辉照归途。"},
    {2029, 10, 16, "重阳节", "登高望远，岁岁安康。"},
    {2030, 1, 11, "腊八节", "一碗腊八粥，暖过岁寒时。"},
    {2030, 2, 2, "除夕", "旧岁至此团圆，新年就在门前。"},
    {2030, 2, 3, "春节", "新春开岁，愿你万事顺意。"},
    {2030, 2, 17, "元宵节", "灯火映团圆，心里有暖光。"},
    {2030, 6, 5, "端午节", "粽叶飘香，愿你安康顺遂。"},
    {2030, 8, 5, "七夕", "星河有约，温柔常在。"},
    {2030, 9, 12, "中秋节", "月满人团圆，清辉照归途。"},
    {2030, 10, 5, "重阳节", "登高望远，岁岁安康。"},
    {2031, 1, 1, "腊八节", "一碗腊八粥，暖过岁寒时。"},
    {2031, 1, 22, "除夕", "旧岁至此团圆，新年就在门前。"},
};

static const HistoryEntry HISTORY_TODAY[] = {
    {1, 1, "1912", "中华民国临时政府成立。"},
    {1, 8, "1942", "斯蒂芬·霍金出生。"},
    {1, 15, "2001", "维基百科上线。"},
    {2, 14, "1876", "贝尔申请电话专利。"},
    {2, 19, "1997", "邓小平逝世。"},
    {3, 8, "1910", "国际妇女节首次庆祝。"},
    {3, 12, "1925", "孙中山先生逝世。"},
    {3, 14, "1879", "爱因斯坦出生。"},
    {4, 12, "1961", "加加林进入太空。"},
    {4, 22, "1970", "第一个地球日。"},
    {5, 1, "1886", "芝加哥工人大罢工。"},
    {5, 4, "1919", "五四运动爆发。"},
    {5, 12, "1820", "南丁格尔出生。"},
    {6, 1, "1925", "上海儿童幸福节。"},
    {6, 5, "1972", "联合国人类环境会议开幕。"},
    {6, 15, "1215", "英国《大宪章》签署。"},
    {7, 1, "1997", "香港回归中国。"},
    {7, 4, "1776", "美国独立宣言发表。"},
    {7, 20, "1969", "人类首次登上月球。"},
    {8, 15, "1945", "日本宣布无条件投降。"},
    {9, 1, "1983", "苏联击落韩国客机。"},
    {9, 10, "1985", "中国第一个教师节。"},
    {9, 11, "2001", "美国911事件。"},
    {10, 1, "1949", "中华人民共和国成立。"},
    {10, 24, "1945", "联合国成立。"},
    {11, 11, "1918", "第一次世界大战结束。"},
    {11, 12, "1866", "孙中山出生。"},
    {12, 12, "1936", "西安事变。"},
    {12, 13, "1937", "南京大屠杀纪念日。"},
    {12, 20, "1999", "澳门回归中国。"},
    {12, 26, "1893", "毛泽东出生。"},
};

static const char* DAILY_QUOTES[] = {
    "把眼前的小事做好，日子自然会发光。",
    "慢慢来，也是在向前。",
    "今天也要认真吃饭，认真生活。",
    "心里有光，路就不算远。",
    "先完成，再慢慢变好。",
    "把复杂留给系统，把温柔留给自己。",
    "愿你有耐心，也有锋芒。",
    "安静努力的人，终会被时间看见。",
    "每一次稳定，都是未来的底座。",
    "不必追赶所有风，只要守住方向。",
    "把今天过清楚，就是很好的答案。",
    "小步不停，也会走到远方。",
    "愿你的热爱，有处安放。",
    "清醒一点，松弛一点，继续一点。",
    "生活会奖励认真修补的人。",
    "今天的好心情，从少一点杂音开始。",
    "日拱一卒，功不唐捐。",
    "保持热爱，奔赴山海。",
    "你只管努力，剩下的交给时间。",
    "简单的事重复做，你就是专家。",
    "每天进步一点点，就是最好的状态。",
    "把期待降到最低，把热情提到最高。",
    "生活不是等待暴风雨过去，而是学会在雨中跳舞。",
    "你现在的努力，是未来的底气。",
    "别着急，最好的总会在最不经意的时候出现。",
    "做自己生命的主角，而非别人生命的看客。",
    "温柔半两，从容一生。",
    "愿你眼中有光，心中有爱，脚下有路。",
    "今天的你，比昨天更值得被爱。",
    "生活明朗，万物可爱。",
    "慢慢变好，就是给自己最好的礼物。",
    "愿所有的后会有期，都是他日的别来无恙。",
};

static const FestivalEntry* FindFestival(int month, int day) {
    for (const auto& entry : FESTIVALS) {
        if (entry.month == month && entry.day == day) {
            return &entry;
        }
    }
    return nullptr;
}

static const DatedFestivalEntry* FindLunarFestival(int year, int month, int day) {
    for (const auto& entry : LUNAR_FESTIVALS) {
        if (entry.year == year && entry.month == month && entry.day == day) {
            return &entry;
        }
    }
    return nullptr;
}

static const HistoryEntry* FindHistoryToday(int month, int day) {
    for (const auto& entry : HISTORY_TODAY) {
        if (entry.month == month && entry.day == day) {
            return &entry;
        }
    }
    return nullptr;
}

enum class DailyCardKind {
    Festival,
    History,
    Quote,
};

static const char* DailyCardKindName(DailyCardKind kind) {
    switch (kind) {
        case DailyCardKind::Festival:
            return "festival";
        case DailyCardKind::History:
            return "history";
        case DailyCardKind::Quote:
            return "quote";
    }
    return "quote";
}

static DailyCardKind BuildDailyCardText(const tm& info, char* title, size_t title_size,
                                        char* body, size_t body_size) {
    const int month = info.tm_mon + 1;
    const int day = info.tm_mday;
    const int year = info.tm_year + 1900;

    if (const DatedFestivalEntry* festival = FindLunarFestival(year, month, day)) {
        snprintf(title, title_size, "%s", festival->title);
        snprintf(body, body_size, "%s", festival->text);
        return DailyCardKind::Festival;
    }

    if (const FestivalEntry* festival = FindFestival(month, day)) {
        snprintf(title, title_size, "%s", festival->title);
        snprintf(body, body_size, "%s", festival->text);
        return DailyCardKind::Festival;
    }

    if (const HistoryEntry* history = FindHistoryToday(month, day)) {
        snprintf(title, title_size, "%s", "历史上的今天");
        snprintf(body, body_size, "%s · %s", history->year, history->text);
        return DailyCardKind::History;
    }

    const size_t quote_count = sizeof(DAILY_QUOTES) / sizeof(DAILY_QUOTES[0]);
    snprintf(title, title_size, "%s", "今日一句");
    snprintf(body, body_size, "%s", DAILY_QUOTES[info.tm_yday % quote_count]);
    return DailyCardKind::Quote;
}

void TimeWeatherService::Start(DesktopUI* ui) {
    desktop_ui_ = ui;
    if (!location_loaded_) {
        LoadLocationCacheFromNvs();
    }
    if (!task_handle_) {
        constexpr uint32_t stack_size = 6144;
        ESP_LOGI(TAG, "time weather task create free_internal=%u largest_internal=%u",
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
        BaseType_t ret = xTaskCreateWithCaps(
            TaskWrapper, "time_weather", stack_size, this, 2, &task_handle_,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ret == pdPASS) {
            ESP_LOGI(TAG, "time weather task started stack=%u memory=psram",
                     static_cast<unsigned>(stack_size));
            return;
        }

        ESP_LOGW(TAG, "time weather task PSRAM create failed ret=%ld, trying internal memory",
                 static_cast<long>(ret));
        task_handle_ = nullptr;
        ret = xTaskCreate(TaskWrapper, "time_weather", stack_size, this, 2, &task_handle_);
        if (ret != pdPASS) {
            task_handle_ = nullptr;
            ESP_LOGE(TAG, "time weather task create failed ret=%ld free_internal=%u largest_internal=%u",
                     static_cast<long>(ret),
                     static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
        }
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

    {
        std::lock_guard<std::mutex> lock(location_mutex_);
        location_city_ = city;
        location_latitude_ = lat;
        location_longitude_ = lon;
        location_loaded_ = true;
    }

    location_update_requested_.store(true);
    if (task_handle_) {
        xTaskNotifyGive(task_handle_);
    }
    ShowCachedWeather("Location updated");
    ESP_LOGI(TAG, "weather location updated: %s %.4f %.4f", city.c_str(), lat, lon);
    return true;
}

void TimeWeatherService::RequestRefresh(bool retry_weather) {
    refresh_requested_.store(true);
    if (retry_weather) {
        weather_refresh_requested_.store(true);
    }
    if (task_handle_) {
        xTaskNotifyGive(task_handle_);
    }
}

void TimeWeatherService::TaskWrapper(void* arg) {
    static_cast<TimeWeatherService*>(arg)->Task();
}

void TimeWeatherService::Task() {
    static constexpr int kWeatherRefreshSeconds = 3600;
    static constexpr int kWeatherLowMemoryRetrySeconds = 300;
    static constexpr int kClockRefreshSeconds = 5;
    int weather_ticks = kWeatherRefreshSeconds;
    int retry_ticks = WeatherRetrySeconds();
    int clock_ticks = kClockRefreshSeconds;
    bool weather_ok = false;
    
    // 等待 WiFi 连接
    while (!WifiStation::GetInstance().IsConnected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "WiFi connected, starting time weather service");
    ShowCachedWeather("Weather sync");
    // Weather only needs a working Wi-Fi route.  Do not wait for the XiaoZhi
    // OTA/MQTT activation path: either remote service can be slow while the
    // independent Open-Meteo endpoint is already reachable.
    vTaskDelay(pdMS_TO_TICKS(3000));
    StartSntp();
    
    while (true) {
        // 启动SNTP
        StartSntp();
        
        // Weather is independent of SNTP.  Fetch it first so a slow or blocked
        // time server cannot delay the desktop weather card.
        const int retry_limit = weather_low_memory_deferred_
                                    ? kWeatherLowMemoryRetrySeconds
                                    : WeatherRetrySeconds();
        if (weather_ticks >= kWeatherRefreshSeconds) {
            ShowCachedWeather("Weather sync");
            weather_ok = FetchWeather();
            weather_ticks = 0;
            retry_ticks = 0;
        } else if (!weather_ok && retry_ticks >= retry_limit) {
            ShowCachedWeather("Weather retry");
            weather_ok = FetchWeather();
            retry_ticks = 0;
        }

        // 等待时间同步
        if (WaitTimeReady()) {
            ESP_LOGI(TAG, "Time synchronized");
            UpdateTime();
        } else {
            ESP_LOGW(TAG, "Time sync timeout");
        }
        clock_ticks = 0;
        
        // 更新网络状态
        SetNetworkStatusSafe("XiaoZhi AI Ready");
        
        // Wait up to 60 seconds, but wake sooner for FC exit or weather retries.
        for (int i = 0; i < 60; ++i) {
            const bool notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) > 0;
            const bool location_update = location_update_requested_.exchange(false);
            const bool refresh_update = refresh_requested_.exchange(false);
            const bool weather_refresh = weather_refresh_requested_.exchange(false);
            if (notified || location_update || refresh_update || weather_refresh) {
                if (location_update) {
                    weather_ticks = kWeatherRefreshSeconds;
                    retry_ticks = WeatherRetrySeconds();
                }
                if (refresh_update || weather_refresh) {
                    UpdateTime();
                    // Force weather refresh when returning to main page
                    if (refresh_update) {
                        weather_ticks = kWeatherRefreshSeconds;
                    }
                    ShowCachedWeather(weather_ok ? "Weather sync" : "Weather retry");
                    if (weather_refresh && !weather_ok) {
                        retry_ticks = WeatherRetrySeconds();
                    }
                }
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
            ++clock_ticks;
            if (clock_ticks >= kClockRefreshSeconds) {
                UpdateTime();
                clock_ticks = 0;
            }
            const int wake_retry_limit = weather_low_memory_deferred_
                                             ? kWeatherLowMemoryRetrySeconds
                                             : WeatherRetrySeconds();
            if (!weather_ok && retry_ticks >= wake_retry_limit) {
                break;
            }
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
    esp_sntp_set_sync_interval(5 * 60 * 1000);
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP started");
}

bool TimeWeatherService::WaitTimeReady() {
    time_t now = 0;
    tm timeinfo = {};
    for (int i = 0; i < 15; ++i) {
        time(&now);
        localtime_r(&now, &timeinfo);
        const auto sync_status = esp_sntp_get_sync_status();
        if (sync_status == SNTP_SYNC_STATUS_COMPLETED ||
            sync_status == SNTP_SYNC_STATUS_IN_PROGRESS) {
            sntp_synced_once_ = true;
        }
        if (sntp_synced_once_ && timeinfo.tm_year >= (2024 - 1900)) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
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
    static int last_logged_minute = -1;
    if (info.tm_min != last_logged_minute) {
        last_logged_minute = info.tm_min;
        ESP_LOGI(TAG, "clock source %04d-%02d-%02d %02d:%02d:%02d wday=%d synced=%d",
                 info.tm_year + 1900, info.tm_mon + 1, info.tm_mday,
                 info.tm_hour, info.tm_min, info.tm_sec, info.tm_wday,
                 sntp_synced_once_ ? 1 : 0);
    }
    
    if (lvgl_port_lock(100)) {
        desktop_ui_->SetTime(info.tm_hour, info.tm_min, info.tm_year + 1900,
                             info.tm_mon + 1, info.tm_mday, WeekdayName(info.tm_wday));
        lvgl_port_unlock();
    }
    
    // Daily card priority: lunar/fixed festival, then history-on-this-day, then quote fallback.
    const int daily_card_date = (info.tm_year + 1900) * 10000 + (info.tm_mon + 1) * 100 + info.tm_mday;
    if (daily_card_date != last_daily_card_date_) {
        last_daily_card_date_ = daily_card_date;
        char daily_date[16];
        char daily_title[48];
        char daily_body[128];
        snprintf(daily_date, sizeof(daily_date), "%02d/%02d", info.tm_mon + 1, info.tm_mday);
        DailyCardKind daily_kind = BuildDailyCardText(info, daily_title, sizeof(daily_title), daily_body, sizeof(daily_body));
        ESP_LOGI(TAG, "Daily card updated for %04d-%02d-%02d kind=%s", info.tm_year + 1900,
                 info.tm_mon + 1, info.tm_mday, DailyCardKindName(daily_kind));
        SetDailyCardSafe(daily_date, daily_title, daily_body);
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
    weather_low_memory_deferred_ = false;
    if (Application::GetInstance().IsExternalAudioActive()) {
        ESP_LOGW(TAG, "Skip weather fetch while external audio is active");
        return false;
    }
    const DeviceState state = Application::GetInstance().GetDeviceState();
    // Weather is an independent HTTPS request. OTA/MQTT activation can stay pending
    // when the XiaoZhi service is unreachable, so only defer during the earliest
    // boot state and keep desktop weather usable on an otherwise healthy Wi-Fi link.
    if (state == kDeviceStateStarting) {
        ESP_LOGW(TAG, "defer weather fetch, application state=%d", state);
        weather_low_memory_deferred_ = true;
        ++weather_failure_count_;
        ShowCachedWeather("Weather wait");
        return false;
    }

    const size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t largest_internal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (free_internal < kWeatherMinInternalFree ||
        largest_internal < kWeatherMinLargestInternalBlock) {
        ESP_LOGW(TAG, "skip weather fetch, low internal memory free=%u largest=%u",
                 static_cast<unsigned>(free_internal),
                 static_cast<unsigned>(largest_internal));
        weather_low_memory_deferred_ = true;
        ++weather_failure_count_;
        ShowCachedWeather("Weather low memory");
        return false;
    }

    std::string city;
    double latitude = 22.5176;
    double longitude = 113.3928;
    CopyLocationCache(&city, &latitude, &longitude);
    
    char url[256];
    snprintf(url, sizeof(url),
             "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,relative_humidity_2m,precipitation,rain,showers,weather_code,cloud_cover,is_day&timezone=Asia%%2FShanghai&forecast_days=1",
             latitude, longitude);
    
    char* response = static_cast<char*>(heap_caps_calloc(1, WEATHER_RESPONSE_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!response) {
        ESP_LOGW(TAG, "weather response buffer alloc failed");
        weather_low_memory_deferred_ = true;
        ++weather_failure_count_;
        ShowCachedWeather("Weather no memory");
        return false;
    }
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 6000;
    config.event_handler = HttpEventHandler;
    config.user_data = response;
    // Open-Meteo serves HTTP directly. Avoiding a second TLS session after music
    // playback materially reduces internal-heap fragmentation on this board.
    config.buffer_size = 1024;
    config.buffer_size_tx = 512;
    config.disable_auto_redirect = false;
    config.max_redirection_count = 3;
    
    static constexpr int kWeatherFetchAttempts = 2;
    esp_err_t err = ESP_FAIL;
    int status = 0;
    for (int attempt = 1; attempt <= kWeatherFetchAttempts; ++attempt) {
        response[0] = 0;
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGW(TAG, "weather http init failed attempt=%d", attempt);
            err = ESP_ERR_NO_MEM;
        } else {
            esp_http_client_set_header(client, "Accept", "application/json");
            esp_http_client_set_header(client, "Connection", "close");
            ESP_LOGI(TAG, "weather request start attempt=%d free_internal=%u largest_internal=%u",
                     attempt + 1,
                     static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
            err = esp_http_client_perform(client);
            status = esp_http_client_get_status_code(client);
            esp_http_client_cleanup(client);
            ESP_LOGI(TAG, "weather request done attempt=%d err=%s status=%d free_internal=%u largest_internal=%u",
                     attempt + 1, esp_err_to_name(err), status,
                     static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
        }

        if (err == ESP_OK && status >= 200 && status < 300 && response[0] != 0) {
            break;
        }

        ESP_LOGW(TAG, "weather fetch failed attempt=%d err=%s status=%d", attempt, esp_err_to_name(err), status);
        if (attempt < kWeatherFetchAttempts) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    if (err != ESP_OK || status < 200 || status >= 300 || response[0] == 0) {
        char fail_text[40];
        snprintf(fail_text, sizeof(fail_text), "Weather fail %d", status);
        ShowCachedWeather(fail_text);
        ++weather_failure_count_;
        heap_caps_free(response);
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
        ++weather_failure_count_;
        heap_caps_free(response);
        return false;
    }

    const int raw_code = code->valueint;
    const double precipitation = JsonNumberOr(current, "precipitation", 0.0);
    const double rain = JsonNumberOr(current, "rain", 0.0);
    const double showers = JsonNumberOr(current, "showers", 0.0);
    const int cloud_cover = JsonIntOr(current, "cloud_cover", -1);
    const int humidity = JsonIntOr(current, "relative_humidity_2m", -1);
    const int refined_code = RefineOpenMeteoCode(raw_code, precipitation, rain, showers, cloud_cover);
    
    char temp_text[16];
    char summary[96];
    snprintf(temp_text, sizeof(temp_text), "%d C", (int)lround(temp->valuedouble));
    if (!ExtractWeatherTime(current, last_update_, sizeof(last_update_))) {
        time_t now = 0;
        tm info = {};
        time(&now);
        localtime_r(&now, &info);
        if (info.tm_year >= (2024 - 1900)) {
            snprintf(last_update_, sizeof(last_update_), "%02d:%02d", info.tm_hour, info.tm_min);
        }
    }
    if (humidity >= 0 && cloud_cover >= 0) {
        snprintf(summary, sizeof(summary), "%s %s %s H%d%% C%d%%",
                 city.c_str(), WeatherDesc(refined_code), last_update_, humidity, cloud_cover);
    } else {
        snprintf(summary, sizeof(summary), "%s %s %s", city.c_str(), WeatherDesc(refined_code), last_update_);
    }
    
    snprintf(last_temperature_, sizeof(last_temperature_), "%s", temp_text);
    snprintf(last_summary_, sizeof(last_summary_), "%s", summary);
    last_weather_code_ = refined_code;
    last_weather_valid_ = true;
    SetWeatherSafe(temp_text, summary, refined_code);
    
    cJSON_Delete(root);
    heap_caps_free(response);
    weather_failure_count_ = 0;
    ESP_LOGI(TAG, "weather ok %s %s raw=%d refined=%d rain=%.2f cloud=%d humidity=%d updated=%s",
             temp_text, summary, raw_code, refined_code, fmax(precipitation, fmax(rain, showers)),
             cloud_cover, humidity, last_update_);
    return true;
}

int TimeWeatherService::WeatherRetrySeconds() const {
    if (weather_failure_count_ <= 0) {
        return 10;
    }
    if (weather_failure_count_ == 1) {
        return 30;
    }
    if (weather_failure_count_ == 2) {
        return 60;
    }
    if (weather_failure_count_ == 3) {
        return 120;
    }
    return 300;
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
    // Returning from Hourglass or Shake Lab can briefly keep LVGL busy while their
    // object trees are reclaimed. Never silently discard a successful weather result.
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (lvgl_port_lock(pdMS_TO_TICKS(200))) {
            desktop_ui_->SetWeather(temperature, summary, weather_code);
            lvgl_port_unlock();
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
    ESP_LOGW(TAG, "weather UI update deferred: LVGL busy");
    weather_refresh_requested_.store(true);
    if (task_handle_) {
        xTaskNotifyGive(task_handle_);
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

void TimeWeatherService::SetDailyCardSafe(const char* date, const char* title, const char* body) {
    if (!desktop_ui_) {
        return;
    }
    if (lvgl_port_lock(100)) {
        desktop_ui_->SetDailyCard(date, title, body);
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
    if (code == 0) return "晴";
    if (code == 1) return "少云";
    if (code == 2) return "多云";
    if (code == 3) return "阴";
    if (code == 45 || code == 48) return "雾";
    if (code >= 51 && code <= 57) return "毛毛雨";
    if (code >= 61 && code <= 67) return "雨";
    if (code >= 80 && code <= 82) return "阵雨";
    if (code >= 71 && code <= 77) return "雪";
    if (code >= 95) return "雷雨";
    return "天气";
}
