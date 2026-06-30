#include "radio_service.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <vector>

#include "application.h"
#include "audio_codec.h"
#include "board.h"
#include "cJSON.h"
#include "config.h"
#include "desktop_ui.h"
#include "driver/sdmmc_host.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mp3dec.h"
#include "sdmmc_cmd.h"
#include "settings.h"
#include "wifi_station.h"

static const char* TAG = "RadioService";

static constexpr int kReadBufferSize = 16 * 1024;
static constexpr int kReadTargetBytes = 8 * 1024;
static constexpr int kReadChunkBytes = 1024;
static constexpr int kPcmMaxSamples = MAX_NCHAN * MAX_NGRAN * MAX_NSAMP;
static constexpr TickType_t kCustomUrlSpeakingGraceTicks = pdMS_TO_TICKS(4000);

enum class RadioCategory {
    NATIONAL,    // 全国
    BEIJING,     // 北京
    SHANGHAI,    // 上海
    GUANGDONG,   // 广东
    ZHEJIANG,    // 浙江
    JIANGSU,     // 江苏
    SICHUAN,     // 四川
    HUNAN,       // 湖南
    HUBEI,       // 湖北
    SHANDONG,    // 山东
    MUSIC,       // 音乐
    TRAFFIC,     // 交通
    OTHER,       // 其他
};

struct RadioStation {
    std::string name;
    std::string urls[3];
    std::string codec;
    int bitrate_kbps;
    RadioCategory category;
    bool favorite;
};

static std::vector<RadioStation> kStations;

static std::string Lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

static RadioCategory ParseCategory(const char* cat) {
    if (!cat) return RadioCategory::OTHER;
    std::string c = Lower(cat);
    if (c == "national" || c == "全国") return RadioCategory::NATIONAL;
    if (c == "beijing" || c == "北京") return RadioCategory::BEIJING;
    if (c == "shanghai" || c == "上海") return RadioCategory::SHANGHAI;
    if (c == "guangdong" || c == "广东") return RadioCategory::GUANGDONG;
    if (c == "zhejiang" || c == "浙江") return RadioCategory::ZHEJIANG;
    if (c == "jiangsu" || c == "江苏") return RadioCategory::JIANGSU;
    if (c == "sichuan" || c == "四川") return RadioCategory::SICHUAN;
    if (c == "hunan" || c == "湖南") return RadioCategory::HUNAN;
    if (c == "hubei" || c == "湖北") return RadioCategory::HUBEI;
    if (c == "shandong" || c == "山东") return RadioCategory::SHANDONG;
    if (c == "music" || c == "音乐") return RadioCategory::MUSIC;
    if (c == "traffic" || c == "交通") return RadioCategory::TRAFFIC;
    return RadioCategory::OTHER;
}

static void LoadBuiltinStations() {
    if (!kStations.empty()) return;
    
    struct BuiltinStation {
        const char* name;
        const char* urls[3];
        const char* codec;
        int bitrate_kbps;
        RadioCategory category;
    };
    
    static const BuiltinStation builtin[] = {
        {"CNR China Voice", {"https://lhttp.qtfm.cn/live/15318317/64k.mp3", "https://lhttp-hw.qtfm.cn/live/15318317/64k.mp3", nullptr}, "MP3", 64, RadioCategory::NATIONAL},
        {"CNR Economic", {"https://lhttp.qingting.fm/live/15318569/64k.mp3", "https://lhttp.qtfm.cn/live/15318569/64k.mp3", nullptr}, "MP3", 64, RadioCategory::NATIONAL},
        {"CNR Music", {"https://lhttp.qtfm.cn/live/15318497/64k.mp3", "https://lhttp-hw.qtfm.cn/live/15318497/64k.mp3", nullptr}, "MP3", 64, RadioCategory::NATIONAL},
        {"CNR Traffic", {"https://lhttp.qtfm.cn/live/15318641/64k.mp3", "https://lhttp-hw.qtfm.cn/live/15318641/64k.mp3", nullptr}, "MP3", 64, RadioCategory::NATIONAL},
        {"CNR Literature", {"https://lhttp.qtfm.cn/live/15318785/64k.mp3", "https://lhttp-hw.qtfm.cn/live/15318785/64k.mp3", nullptr}, "MP3", 64, RadioCategory::NATIONAL},
        {"CNR Senior", {"https://lhttp.qtfm.cn/live/15318857/64k.mp3", "https://lhttp-hw.qtfm.cn/live/15318857/64k.mp3", nullptr}, "MP3", 64, RadioCategory::NATIONAL},
        {"Beijing News", {"https://lhttp.qtfm.cn/live/4848/64k.mp3", "https://lhttp-hw.qtfm.cn/live/4848/64k.mp3", nullptr}, "MP3", 64, RadioCategory::BEIJING},
        {"Beijing Traffic", {"https://lhttp.qtfm.cn/live/4955/64k.mp3", "https://lhttp-hw.qtfm.cn/live/4955/64k.mp3", nullptr}, "MP3", 64, RadioCategory::BEIJING},
        {"Beijing Music", {"https://lhttp.qtfm.cn/live/4938/64k.mp3", "https://lhttp-hw.qtfm.cn/live/4938/64k.mp3", nullptr}, "MP3", 64, RadioCategory::BEIJING},
        {"Shanghai News", {"https://lhttp.qtfm.cn/live/1259/64k.mp3", "https://lhttp-hw.qtfm.cn/live/1259/64k.mp3", nullptr}, "MP3", 64, RadioCategory::SHANGHAI},
        {"Shanghai Traffic", {"https://lhttp.qtfm.cn/live/1260/64k.mp3", "https://lhttp-hw.qtfm.cn/live/1260/64k.mp3", nullptr}, "MP3", 64, RadioCategory::SHANGHAI},
        {"Shanghai Music", {"https://lhttp.qtfm.cn/live/1271/64k.mp3", "https://lhttp-hw.qtfm.cn/live/1271/64k.mp3", nullptr}, "MP3", 64, RadioCategory::SHANGHAI},
        {"Guangzhou News", {"http://lhttp.qingting.fm/live/4848/64k.mp3", "https://lhttp.qtfm.cn/live/4848/64k.mp3", nullptr}, "MP3", 64, RadioCategory::GUANGDONG},
        {"Guangzhou Traffic", {"http://lhttp.qingting.fm/live/4955/64k.mp3", "https://lhttp.qtfm.cn/live/4955/64k.mp3", nullptr}, "MP3", 64, RadioCategory::GUANGDONG},
        {"Pearl River FM", {"http://lhttp.qingting.fm/live/1259/64k.mp3", "https://lhttp.qtfm.cn/live/1259/64k.mp3", nullptr}, "MP3", 64, RadioCategory::GUANGDONG},
        {"Guangdong Music", {"http://lhttp.qingting.fm/live/1260/64k.mp3", "https://lhttp.qtfm.cn/live/1260/64k.mp3", nullptr}, "MP3", 64, RadioCategory::GUANGDONG},
        {"Guangdong News", {"https://lhttp.qtfm.cn/live/471/64k.mp3", "https://lhttp-hw.qtfm.cn/live/471/64k.mp3", nullptr}, "MP3", 64, RadioCategory::GUANGDONG},
        {"Shenzhen FM971", {"http://lhttp.qingting.fm/live/1271/64k.mp3", "https://lhttp.qtfm.cn/live/1271/64k.mp3", nullptr}, "MP3", 64, RadioCategory::GUANGDONG},
        {"Zhejiang Voice", {"https://lhttp.qtfm.cn/live/1223/64k.mp3", "https://lhttp-hw.qtfm.cn/live/1223/64k.mp3", nullptr}, "MP3", 64, RadioCategory::ZHEJIANG},
        {"Zhejiang Traffic", {"https://lhttp.qtfm.cn/live/5021381/64k.mp3", "https://lhttp-hw.qtfm.cn/live/5021381/64k.mp3", nullptr}, "MP3", 64, RadioCategory::ZHEJIANG},
        {"Zhejiang Music", {"https://lhttp.qtfm.cn/live/5022107/64k.mp3", "https://lhttp-hw.qtfm.cn/live/5022107/64k.mp3", nullptr}, "MP3", 64, RadioCategory::ZHEJIANG},
        {"Jiangsu News", {"https://lhttp.qtfm.cn/live/5022308/64k.mp3", "https://lhttp-hw.qtfm.cn/live/5022308/64k.mp3", nullptr}, "MP3", 64, RadioCategory::JIANGSU},
        {"Jiangsu Traffic", {"https://lhttp.qtfm.cn/live/4915/64k.mp3", "https://lhttp-hw.qtfm.cn/live/4915/64k.mp3", nullptr}, "MP3", 64, RadioCategory::JIANGSU},
        {"Sichuan News", {"https://lhttp.qtfm.cn/live/4848/64k.mp3", "https://lhttp-hw.qtfm.cn/live/4848/64k.mp3", nullptr}, "MP3", 64, RadioCategory::SICHUAN},
        {"Sichuan Traffic", {"https://lhttp.qtfm.cn/live/4955/64k.mp3", "https://lhttp-hw.qtfm.cn/live/4955/64k.mp3", nullptr}, "MP3", 64, RadioCategory::SICHUAN},
        {"Hunan News", {"https://lhttp.qtfm.cn/live/1259/64k.mp3", "https://lhttp-hw.qtfm.cn/live/1259/64k.mp3", nullptr}, "MP3", 64, RadioCategory::HUNAN},
        {"Hunan Traffic", {"https://lhttp.qtfm.cn/live/1260/64k.mp3", "https://lhttp-hw.qtfm.cn/live/1260/64k.mp3", nullptr}, "MP3", 64, RadioCategory::HUNAN},
        {"Hubei News", {"https://lhttp.qtfm.cn/live/1271/64k.mp3", "https://lhttp-hw.qtfm.cn/live/1271/64k.mp3", nullptr}, "MP3", 64, RadioCategory::HUBEI},
        {"Hubei Traffic", {"https://lhttp.qtfm.cn/live/5021381/64k.mp3", "https://lhttp-hw.qtfm.cn/live/5021381/64k.mp3", nullptr}, "MP3", 64, RadioCategory::HUBEI},
        {"Shandong News", {"https://lhttp.qtfm.cn/live/5022107/64k.mp3", "https://lhttp-hw.qtfm.cn/live/5022107/64k.mp3", nullptr}, "MP3", 64, RadioCategory::SHANDONG},
        {"Shandong Traffic", {"https://lhttp.qtfm.cn/live/5022308/64k.mp3", "https://lhttp-hw.qtfm.cn/live/5022308/64k.mp3", nullptr}, "MP3", 64, RadioCategory::SHANDONG},
        {"Music Radio", {"http://lhttp.qingting.fm/live/5022107/64k.mp3", "https://lhttp.qtfm.cn/live/5022107/64k.mp3", nullptr}, "MP3", 64, RadioCategory::MUSIC},
        {"Music FM", {"http://lhttp.qingting.fm/live/4938/64k.mp3", "https://lhttp.qtfm.cn/live/4938/64k.mp3", nullptr}, "MP3", 64, RadioCategory::MUSIC},
        {"West Lake Voice", {"http://lhttp.qingting.fm/live/1223/64k.mp3", "https://lhttp.qtfm.cn/live/1223/64k.mp3", nullptr}, "MP3", 64, RadioCategory::MUSIC},
        {"Traffic 959", {"http://lhttp.qingting.fm/live/5021381/64k.mp3", "https://lhttp.qtfm.cn/live/5021381/64k.mp3", nullptr}, "MP3", 64, RadioCategory::TRAFFIC},
        {"Business Radio", {"https://lhttp.qtfm.cn/live/5022308/64k.mp3", "https://lhttp-hw.qtfm.cn/live/5022308/64k.mp3", nullptr}, "MP3", 64, RadioCategory::TRAFFIC},
        {"Night Radio", {"http://lhttp.qingting.fm/live/4915/64k.mp3", "https://lhttp.qtfm.cn/live/4915/64k.mp3", nullptr}, "MP3", 64, RadioCategory::OTHER},
    };
    
    for (const auto& s : builtin) {
        RadioStation station;
        station.name = s.name;
        station.urls[0] = s.urls[0] ? s.urls[0] : "";
        station.urls[1] = s.urls[1] ? s.urls[1] : "";
        station.urls[2] = s.urls[2] ? s.urls[2] : "";
        station.codec = s.codec;
        station.bitrate_kbps = s.bitrate_kbps;
        station.category = s.category;
        station.favorite = false;
        kStations.push_back(station);
    }
}

static bool TryMountSdCard(uint8_t width, int max_freq_khz) {
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = max_freq_khz;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = width;
    slot_config.clk = PHOTO_SDMMC_CLK_PIN;
    slot_config.cmd = PHOTO_SDMMC_CMD_PIN;
    slot_config.d0 = PHOTO_SDMMC_D0_PIN;
    if (width >= 4) {
        slot_config.d1 = PHOTO_SDMMC_D1_PIN;
        slot_config.d2 = PHOTO_SDMMC_D2_PIN;
        slot_config.d3 = PHOTO_SDMMC_D3_PIN;
    } else {
        slot_config.d1 = GPIO_NUM_NC;
        slot_config.d2 = GPIO_NUM_NC;
        slot_config.d3 = GPIO_NUM_NC;
    }
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 0,
    };

    sdmmc_card_t* card = nullptr;
    esp_err_t err = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "radio sd mount failed width=%d err=%s", width, esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "radio sd mounted width=%d", width);
    return true;
}

static bool EnsureSdCardMounted() {
    DIR* existing = opendir("/sdcard");
    if (existing) {
        closedir(existing);
        return true;
    }
    if (TryMountSdCard(PHOTO_SDMMC_BUS_WIDTH, 10000)) {
        return true;
    }
    if (PHOTO_SDMMC_BUS_WIDTH != 1) {
        return TryMountSdCard(1, 10000);
    }
    return false;
}

static bool LoadStationsFromSdCard() {
    if (!EnsureSdCardMounted()) {
        ESP_LOGI(TAG, "SD card not available, using built-in stations");
        return false;
    }
    
    const char* path = "/sdcard/radio.json";
    FILE* file = fopen(path, "rb");
    if (!file) {
        ESP_LOGI(TAG, "radio.json not found on SD card, using built-in stations");
        return false;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    
    if (size <= 0 || size > 32768) {
        fclose(file);
        ESP_LOGW(TAG, "radio.json invalid size: %ld", size);
        return false;
    }
    
    char* buffer = static_cast<char*>(heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buffer) {
        fclose(file);
        ESP_LOGW(TAG, "radio.json alloc failed");
        return false;
    }
    
    size_t read = fread(buffer, 1, size, file);
    fclose(file);
    buffer[read] = '\0';
    
    cJSON* root = cJSON_Parse(buffer);
    heap_caps_free(buffer);
    
    if (!root) {
        ESP_LOGW(TAG, "radio.json parse failed");
        return false;
    }
    
    cJSON* stations = cJSON_GetObjectItem(root, "stations");
    if (!cJSON_IsArray(stations)) {
        cJSON_Delete(root);
        ESP_LOGW(TAG, "radio.json missing stations array");
        return false;
    }
    
    kStations.clear();
    int count = cJSON_GetArraySize(stations);
    for (int i = 0; i < count && i < 128; ++i) {
        cJSON* item = cJSON_GetArrayItem(stations, i);
        if (!item) continue;
        
        cJSON* name = cJSON_GetObjectItem(item, "name");
        cJSON* url = cJSON_GetObjectItem(item, "url");
        cJSON* url2 = cJSON_GetObjectItem(item, "url2");
        cJSON* url3 = cJSON_GetObjectItem(item, "url3");
        cJSON* codec = cJSON_GetObjectItem(item, "codec");
        cJSON* bitrate = cJSON_GetObjectItem(item, "bitrate");
        cJSON* category = cJSON_GetObjectItem(item, "category");
        
        if (!cJSON_IsString(name) || !cJSON_IsString(url)) continue;
        
        RadioStation station;
        station.name = name->valuestring;
        station.urls[0] = url->valuestring;
        station.urls[1] = cJSON_IsString(url2) ? url2->valuestring : "";
        station.urls[2] = cJSON_IsString(url3) ? url3->valuestring : "";
        station.codec = cJSON_IsString(codec) ? codec->valuestring : "MP3";
        station.bitrate_kbps = cJSON_IsNumber(bitrate) ? bitrate->valueint : 64;
        station.category = ParseCategory(cJSON_IsString(category) ? category->valuestring : nullptr);
        station.favorite = false;
        kStations.push_back(station);
    }
    
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded %d stations from radio.json", (int)kStations.size());
    return !kStations.empty();
}

static void EnsureStationsLoaded() {
    if (!kStations.empty()) return;
    if (!LoadStationsFromSdCard()) {
        LoadBuiltinStations();
    }
}

static int StationCount() {
    EnsureStationsLoaded();
    return static_cast<int>(kStations.size());
}

static const char* CategoryName(RadioCategory category) {
    switch (category) {
        case RadioCategory::NATIONAL: return "全国";
        case RadioCategory::BEIJING: return "北京";
        case RadioCategory::SHANGHAI: return "上海";
        case RadioCategory::GUANGDONG: return "广东";
        case RadioCategory::ZHEJIANG: return "浙江";
        case RadioCategory::JIANGSU: return "江苏";
        case RadioCategory::SICHUAN: return "四川";
        case RadioCategory::HUNAN: return "湖南";
        case RadioCategory::HUBEI: return "湖北";
        case RadioCategory::SHANDONG: return "山东";
        case RadioCategory::MUSIC: return "音乐";
        case RadioCategory::TRAFFIC: return "交通";
        case RadioCategory::OTHER: return "其他";
        default: return "未知";
    }
}

static std::vector<int> GetStationsByCategory(RadioCategory category) {
    std::vector<int> result;
    for (int i = 0; i < StationCount(); ++i) {
        if (kStations[i].category == category) {
            result.push_back(i);
        }
    }
    return result;
}

static std::vector<int> GetFavoriteStations() {
    std::vector<int> result;
    for (int i = 0; i < StationCount(); ++i) {
        if (kStations[i].favorite) {
            result.push_back(i);
        }
    }
    return result;
}

static int16_t Clamp16(int value) {
    if (value > 32767) {
        return 32767;
    }
    if (value < -32768) {
        return -32768;
    }
    return static_cast<int16_t>(value);
}

void RadioService::Start(DesktopUI* desktop_ui) {
    desktop_ui_ = desktop_ui;
    if (started_) {
        return;
    }
    started_ = true;
    
    int count = StationCount();
    ESP_LOGI(TAG, "RadioService::Start: %d stations loaded", count);
    
    last_success_url_.resize(count, -1);
    audio_focus_blocked_.store(IsXiaozhiAudioState(), std::memory_order_relaxed);
    queue_ = xQueueCreate(8, sizeof(Command));
    if (!queue_) {
        ESP_LOGE(TAG, "radio queue create failed free_internal=%u largest_internal=%u",
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
        started_ = false;
        SetUi("Unavailable", "No memory");
        return;
    }
    Application::GetInstance().RegisterDeviceStateCallback([this](DeviceState previous, DeviceState current) {
        OnDeviceStateChanged(static_cast<int>(previous), static_cast<int>(current));
    });
    LoadFavorites();
    LoadStationIndex();
    
    constexpr uint32_t kTaskStackBytes = 6144;
    ESP_LOGI(TAG, "radio task create free_internal=%u largest_internal=%u",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
    BaseType_t ret = xTaskCreateWithCaps([](void* arg) {
        static_cast<RadioService*>(arg)->Task();
    }, "radio_service", kTaskStackBytes, this, 4, &task_handle_,
       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    task_stack_internal_ = false;
    if (ret != pdPASS) {
        ESP_LOGW(TAG, "radio PSRAM task create failed ret=%ld, trying internal memory",
                 static_cast<long>(ret));
        task_handle_ = nullptr;
        ret = xTaskCreate([](void* arg) {
            static_cast<RadioService*>(arg)->Task();
        }, "radio_service", kTaskStackBytes, this, 4, &task_handle_);
        task_stack_internal_ = (ret == pdPASS);
    }
    if (ret != pdPASS) {
        auto queue = static_cast<QueueHandle_t>(queue_);
        if (queue) {
            vQueueDelete(queue);
        }
        queue_ = nullptr;
        task_handle_ = nullptr;
        started_ = false;
        ESP_LOGE(TAG, "radio task create failed ret=%ld free_internal=%u largest_internal=%u",
                 static_cast<long>(ret),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
        SetUi("Unavailable", "No memory");
        return;
    }
    ESP_LOGI(TAG, "radio task started stack=%u memory=%s",
             static_cast<unsigned>(kTaskStackBytes),
             task_stack_internal_ ? "internal" : "psram");
    
    SetUi("Ready", "Tap Play");
}

void RadioService::LoadFavorites() {
    Settings settings("radio_fav", false);
    for (int i = 0; i < StationCount(); ++i) {
        char key[16];
        snprintf(key, sizeof(key), "fav_%d", i);
        kStations[i].favorite = settings.GetInt(key, 0) == 1;
    }
}

void RadioService::SaveFavorites() {
    Settings settings("radio_fav", true);
    for (int i = 0; i < StationCount(); ++i) {
        char key[16];
        snprintf(key, sizeof(key), "fav_%d", i);
        settings.SetInt(key, kStations[i].favorite ? 1 : 0);
    }
}

void RadioService::LoadStationIndex() {
    Settings settings("radio_st", false);
    int index = settings.GetInt("last", 0);
    int count = StationCount();
    if (count > 0 && index >= 0 && index < count) {
        station_index_ = index;
        ESP_LOGI(TAG, "Loaded last station index=%d name=%s", station_index_, kStations[station_index_].name.c_str());
    } else {
        station_index_ = 0;
        ESP_LOGW(TAG, "Invalid station index %d, reset to 0 (count=%d)", index, count);
    }
}

void RadioService::SaveStationIndex() {
    if (!task_stack_internal_ && xTaskGetCurrentTaskHandle() == task_handle_) {
        ESP_LOGW(TAG, "skip station index save from radio task on PSRAM stack");
        return;
    }
    Settings settings("radio_st", true);
    settings.SetInt("last", station_index_);
    ESP_LOGI(TAG, "Saved station index=%d", station_index_);
}

void RadioService::ToggleFavorite(int index) {
    if (index >= 0 && index < StationCount()) {
        kStations[index].favorite = !kStations[index].favorite;
        SaveFavorites();
    }
}

bool RadioService::IsFavorite(int index) const {
    if (index >= 0 && index < StationCount()) {
        return kStations[index].favorite;
    }
    return false;
}

std::vector<int> RadioService::GetFavorites() const {
    return GetFavoriteStations();
}

std::vector<int> RadioService::GetByCategory(int category) const {
    return GetStationsByCategory(static_cast<RadioCategory>(category));
}

int RadioService::GetStationCount() const {
    return StationCount();
}

const char* RadioService::GetStationName(int index) const {
    if (index >= 0 && index < StationCount()) {
        return kStations[index].name.c_str();
    }
    return "";
}

const char* RadioService::GetStationCategory(int index) const {
    if (index >= 0 && index < StationCount()) {
        return CategoryName(kStations[index].category);
    }
    return "";
}

void RadioService::PlayPause() {
    ESP_LOGI(TAG, "PlayPause requested play_requested=%d", play_requested_.load(std::memory_order_relaxed));
    stream_generation_.fetch_add(1, std::memory_order_relaxed);
    PostCommand(Command::PLAY_PAUSE);
}

void RadioService::Play() {
    if (!play_requested_) {
        ESP_LOGI(TAG, "Play requested (was stopped)");
        PostCommand(Command::PLAY_PAUSE);
    } else {
        ESP_LOGI(TAG, "Play requested (already playing, refreshing focus)");
        PostCommand(Command::FOCUS_CHANGED);
    }
}

void RadioService::Stop() {
    ESP_LOGI(TAG, "Stop requested");
    stop_requested_ = true;
    stream_generation_.fetch_add(1, std::memory_order_relaxed);
    PostCommand(Command::STOP);
}

void RadioService::Next() {
    stream_generation_.fetch_add(1, std::memory_order_relaxed);
    PostCommand(Command::NEXT);
}

void RadioService::Prev() {
    stream_generation_.fetch_add(1, std::memory_order_relaxed);
    PostCommand(Command::PREV);
}

std::string RadioService::GetStatusJson() const {
    const auto& station = kStations[station_index_];
    char json[192];
    snprintf(json, sizeof(json),
             "{\"station\":\"%s\",\"state\":\"%s\",\"codec\":\"%s\",\"bitrate_kbps\":%d}",
             station.name.c_str(), play_requested_ ? "playing" : "stopped", station.codec.c_str(), station.bitrate_kbps);
    return json;
}

std::string RadioService::SelectStation(const std::string& station) {
    std::string needle = Lower(station);
    for (int i = 0; i < StationCount(); ++i) {
        if (Lower(kStations[i].name).find(needle) != std::string::npos) {
            stream_generation_.fetch_add(1, std::memory_order_relaxed);
            playing_custom_url_ = false;
            station_index_ = i;
            play_requested_ = true;
            stop_requested_ = false;
            SetUi("Connecting", "Selected station");
            return std::string("Radio station selected: ") + kStations[i].name;
        }
    }
    return "Radio station not found.";
}

std::string RadioService::PlayUrlFromTool(const std::string& title, const std::string& artist, const std::string& url) {
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        return "Music URL must start with http:// or https://.";
    }

    EnsureStationsLoaded();
    std::string display_name = title.empty() ? "Music URL" : title;
    if (!artist.empty()) {
        display_name += " - ";
        display_name += artist;
    }

    stream_generation_.fetch_add(1, std::memory_order_relaxed);

    RadioStation station;
    station.name = display_name;
    station.urls[0] = url;
    station.urls[1].clear();
    station.urls[2].clear();
    station.codec = "MP3";
    station.bitrate_kbps = 0;
    station.category = RadioCategory::MUSIC;
    station.favorite = false;

    if (!playing_custom_url_ && station_index_ >= 0 && station_index_ < StationCount()) {
        last_radio_station_index_ = station_index_;
    }
    if (custom_station_index_ >= 0 && custom_station_index_ < static_cast<int>(kStations.size())) {
        kStations[custom_station_index_] = station;
    } else {
        kStations.push_back(station);
        custom_station_index_ = static_cast<int>(kStations.size()) - 1;
        last_success_url_.resize(kStations.size(), -1);
    }

    station_index_ = custom_station_index_;
    playing_custom_url_ = true;
    play_requested_ = true;
    stop_requested_ = false;
    audio_focus_blocked_.store(false, std::memory_order_relaxed);
    custom_url_speaking_grace_until_ = xTaskGetTickCount() + kCustomUrlSpeakingGraceTicks;
    reconnect_attempt_ = 0;
    SetUi("Connecting", "Music URL");

    Application::GetInstance().Schedule([]() {
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateSpeaking) {
            app.AbortSpeaking(kAbortReasonNone);
        }
        if (app.GetDeviceState() == kDeviceStateListening ||
            app.GetDeviceState() == kDeviceStateSpeaking ||
            app.GetDeviceState() == kDeviceStateConnecting) {
            app.SetDeviceState(kDeviceStateIdle);
        }
    });
    PostCommand(Command::PLAY_CUSTOM_URL);
    return std::string("Music URL started on device; no spoken follow-up is needed: ") + station.name;
}

void RadioService::PostCommand(Command command) {
    auto queue = static_cast<QueueHandle_t>(queue_);
    if (queue) {
        xQueueSend(queue, &command, 0);
    }
}

void RadioService::Task() {
    while (true) {
        Command command;
        auto queue = static_cast<QueueHandle_t>(queue_);
        if (queue && xQueueReceive(queue, &command, pdMS_TO_TICKS(250)) == pdTRUE) {
            HandleCommand(command);
        }

        if (!play_requested_) {
            reconnect_attempt_ = 0;
            continue;
        }
        if (audio_focus_blocked_.load(std::memory_order_relaxed)) {
            if (!focus_pause_logged_) {
                ESP_LOGI(TAG, "radio paused by audio focus");
                SetUi("Paused", "XiaoZhi is using audio");
                focus_pause_logged_ = true;
            }
            Application::GetInstance().SetExternalAudioActive(false);
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        focus_pause_logged_ = false;
        if (!WifiStation::GetInstance().IsConnected()) {
            SetUi("Waiting WiFi", "Need network");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        const uint32_t generation = stream_generation_.load(std::memory_order_relaxed);
        PlayCurrentStation(generation);
        if (generation != stream_generation_.load(std::memory_order_relaxed)) {
            reconnect_attempt_ = 0;
            continue;
        }
        if (play_requested_) {
            reconnect_attempt_++;
            int delay_ms = std::min(5000, 500 + reconnect_attempt_ * 500);
            ESP_LOGW(TAG, "radio reconnect scheduled station=%s attempt=%d delay=%dms",
                     kStations[station_index_].name, reconnect_attempt_, delay_ms);
            if (reconnect_attempt_ >= 5) {
                SetUi("Error", "Multiple failures");
            } else {
                SetUi("Reconnecting", "Stream ended");
            }
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }
}

void RadioService::HandleCommand(Command command) {
    const int count = StationCount();
    if (count <= 0) {
        ESP_LOGW(TAG, "radio no stations available");
        return;
    }
    
    if (station_index_ < 0 || station_index_ >= count) {
        station_index_ = 0;
    }
    
    switch (command) {
        case Command::PLAY_PAUSE: {
            play_requested_ = !play_requested_.load(std::memory_order_relaxed);
            stop_requested_ = !play_requested_.load(std::memory_order_relaxed);
            reconnect_attempt_ = 0;
            const bool playing = play_requested_.load(std::memory_order_relaxed);
            if (!playing) {
                Application::GetInstance().SetExternalAudioActive(false);
                if (playing_custom_url_) {
                    playing_custom_url_ = false;
                    if (last_radio_station_index_ >= 0 && last_radio_station_index_ < StationCount()) {
                        station_index_ = last_radio_station_index_;
                    }
                }
            }
            SetUi(playing ? "Connecting" : "Paused", playing ? "Opening stream" : "Stopped");
            ESP_LOGI(TAG, "radio %s station=%s", playing ? "play requested" : "paused", kStations[station_index_].name.c_str());
            break;
        }
        case Command::STOP:
            play_requested_ = false;
            stop_requested_ = true;
            reconnect_attempt_ = 0;
            if (playing_custom_url_) {
                playing_custom_url_ = false;
                if (last_radio_station_index_ >= 0 && last_radio_station_index_ < StationCount()) {
                    station_index_ = last_radio_station_index_;
                }
            }
            Application::GetInstance().SetExternalAudioActive(false);
            SetUi("Stopped", "Ready");
            ESP_LOGI(TAG, "radio stopped");
            break;
        case Command::NEXT:
            if (playing_custom_url_) {
                playing_custom_url_ = false;
                if (last_radio_station_index_ >= 0 && last_radio_station_index_ < StationCount()) {
                    station_index_ = last_radio_station_index_;
                }
            }
            NextStation(1);
            play_requested_ = true;
            stop_requested_ = false;
            reconnect_attempt_ = 0;
            SetUi("Connecting", "Next station");
            ESP_LOGI(TAG, "radio next station=%s", kStations[station_index_].name.c_str());
            break;
        case Command::PREV:
            if (playing_custom_url_) {
                playing_custom_url_ = false;
                if (last_radio_station_index_ >= 0 && last_radio_station_index_ < StationCount()) {
                    station_index_ = last_radio_station_index_;
                }
            }
            NextStation(-1);
            play_requested_ = true;
            stop_requested_ = false;
            reconnect_attempt_ = 0;
            SetUi("Connecting", "Previous station");
            ESP_LOGI(TAG, "radio previous station=%s", kStations[station_index_].name.c_str());
            break;
        case Command::FOCUS_CHANGED:
            if (audio_focus_blocked_.load(std::memory_order_relaxed)) {
                Application::GetInstance().SetExternalAudioActive(false);
                SetUi("Paused", "XiaoZhi is using audio");
            } else if (play_requested_) {
                SetUi("Connecting", "Audio focus restored");
                ESP_LOGI(TAG, "radio audio focus restored, resume station=%s", kStations[station_index_].name.c_str());
            }
            break;
        case Command::PLAY_CUSTOM_URL:
            play_requested_ = true;
            stop_requested_ = false;
            reconnect_attempt_ = 0;
            SetUi("Connecting", "Opening music URL");
            ESP_LOGI(TAG, "music url play requested title=%s", kStations[station_index_].name.c_str());
            break;
    }
}

void RadioService::PlayCurrentStation(uint32_t stream_generation) {
    const int count = StationCount();
    if (count <= 0 || station_index_ < 0 || station_index_ >= count) {
        SetUi("Error", "No station");
        return;
    }
    
    const auto station_urls = kStations[station_index_].urls;
    bool tried_any = false;
    stop_requested_ = false;
    int url_order[3] = {0, 1, 2};
    
    if (station_index_ < static_cast<int>(last_success_url_.size()) && 
        last_success_url_[station_index_] > 0 && last_success_url_[station_index_] < 3) {
        url_order[0] = last_success_url_[station_index_];
        url_order[1] = 0;
        url_order[2] = last_success_url_[station_index_] == 1 ? 2 : 1;
    }
    
    for (int order = 0; order < 3 && play_requested_ && !stop_requested_ &&
         stream_generation == stream_generation_.load(std::memory_order_relaxed); ++order) {
        if (audio_focus_blocked_.load(std::memory_order_relaxed)) {
            ESP_LOGI(TAG, "radio station open deferred by audio focus");
            return;
        }
        const int i = url_order[order];
        if (i < 0 || i >= 3) continue;
        const std::string url = station_urls[i];
        if (url.empty()) {
            continue;
        }
        tried_any = true;
        if (PlayUrl(url, i, stream_generation)) {
            return;
        }
        if (stream_generation != stream_generation_.load(std::memory_order_relaxed)) {
            return;
        }
        SetUi("Connecting", "Trying fallback");
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    if (play_requested_) {
        SetUi("Error", tried_any ? "All sources failed" : "No source");
    }
}

bool RadioService::PlayUrl(const std::string& url, int url_index, uint32_t stream_generation) {
    const std::string station_name = kStations[station_index_].name;
    SetUi("Connecting", url_index == 0 ? "Opening stream" : "Fallback source");
    ResetAudioLeveler();

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.timeout_ms = 1000;
    config.buffer_size = 2048;
    config.buffer_size_tx = 512;
    config.disable_auto_redirect = false;
    config.max_redirection_count = 5;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        SetUi("Error", "HTTP init failed");
        return false;
    }

    esp_http_client_set_header(client, "User-Agent", "Mozilla/5.0 ESP32 Radio");
    esp_http_client_set_header(client, "Accept", "audio/mpeg,*/*");
    esp_http_client_set_header(client, "Icy-MetaData", "0");

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open failed station=%s url_index=%d err=%s url=%s", station_name.c_str(), url_index, esp_err_to_name(err), url.c_str());
        esp_http_client_cleanup(client);
        return false;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status < 200 || status >= 400) {
        ESP_LOGW(TAG, "bad stream status station=%s url_index=%d status=%d len=%d url=%s",
                 station_name.c_str(), url_index, status, content_length, url.c_str());
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    ESP_LOGI(TAG, "stream open station=%s url_index=%d status=%d len=%d url=%s",
             station_name.c_str(), url_index, status, content_length, url.c_str());
    SetUi("Buffering", "Filling buffer");

    HMP3Decoder decoder = MP3InitDecoder();
    auto* read_buffer = static_cast<uint8_t*>(heap_caps_malloc(kReadBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    auto* pcm_buffer = static_cast<int16_t*>(heap_caps_malloc(kPcmMaxSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!decoder || !read_buffer || !pcm_buffer) {
        ESP_LOGE(TAG, "decoder alloc failed");
        SetUi("Error", "No decoder memory");
        if (decoder) {
            MP3FreeDecoder(decoder);
        }
        heap_caps_free(read_buffer);
        heap_caps_free(pcm_buffer);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    uint8_t* read_ptr = read_buffer;
    int bytes_left = 0;
    int decoded_frames = 0;
    int decode_errors = 0;
    size_t total_bytes = 0;
    bool stream_failed = false;
    Application::GetInstance().SetExternalAudioActive(true);

    while (play_requested_ && !stop_requested_ && WifiStation::GetInstance().IsConnected() &&
           stream_generation == stream_generation_.load(std::memory_order_relaxed)) {
        Command command;
        auto queue = static_cast<QueueHandle_t>(queue_);
        while (queue && xQueueReceive(queue, &command, 0) == pdTRUE) {
            HandleCommand(command);
        }
        if (!play_requested_ || stop_requested_ ||
            stream_generation != stream_generation_.load(std::memory_order_relaxed)) {
            break;
        }
        if (ShouldYieldAudio()) {
            SetUi("Paused", "XiaoZhi is using audio");
            ESP_LOGI(TAG, "radio yielded audio focus station=%s", station_name.c_str());
            break;
        }

        if (read_ptr != read_buffer && bytes_left > 0) {
            memmove(read_buffer, read_ptr, bytes_left);
            read_ptr = read_buffer;
        }
        while (bytes_left < kReadTargetBytes && bytes_left < kReadBufferSize &&
               play_requested_ && !stop_requested_ &&
               stream_generation == stream_generation_.load(std::memory_order_relaxed)) {
            int room = std::min(kReadChunkBytes, kReadBufferSize - bytes_left);
            int read = esp_http_client_read(client, reinterpret_cast<char*>(read_buffer + bytes_left), room);
            if (read > 0) {
                bytes_left += read;
                total_bytes += read;
                continue;
            }
            if (read == 0) {
                vTaskDelay(pdMS_TO_TICKS(20));
                break;
            }
            ESP_LOGW(TAG, "stream read failed station=%s url_index=%d read=%d", station_name.c_str(), url_index, read);
            stream_failed = true;
            break;
        }
        if (stream_generation != stream_generation_.load(std::memory_order_relaxed)) {
            break;
        }
        if (stream_failed) {
            SetUi("Reconnecting", "Read failed");
            break;
        }

        if (bytes_left <= 0) {
            SetUi("Buffering", "Waiting for data");
            vTaskDelay(pdMS_TO_TICKS(80));
            continue;
        }

        int offset = MP3FindSyncWord(read_ptr, bytes_left);
        if (offset < 0) {
            ESP_LOGW(TAG, "mp3 sync not found station=%s url_index=%d bytes=%d", station_name.c_str(), url_index, bytes_left);
            bytes_left = 0;
            read_ptr = read_buffer;
            SetUi("Buffering", "Finding sync");
            continue;
        }
        read_ptr += offset;
        bytes_left -= offset;

        int mp3_err = MP3Decode(decoder, &read_ptr, &bytes_left, pcm_buffer, 0);
        if (mp3_err == ERR_MP3_INDATA_UNDERFLOW || mp3_err == ERR_MP3_MAINDATA_UNDERFLOW) {
            continue;
        }
        if (mp3_err != ERR_MP3_NONE) {
            ++decode_errors;
            ESP_LOGW(TAG, "mp3 decode failed station=%s url_index=%d err=%d count=%d", station_name.c_str(), url_index, mp3_err, decode_errors);
            if (decode_errors > 20) {
                SetUi("Reconnecting", "Decode errors");
                break;
            }
            if (bytes_left > 0) {
                ++read_ptr;
                --bytes_left;
            }
            continue;
        }

        decode_errors = 0;
        MP3FrameInfo frame_info = {};
        MP3GetLastFrameInfo(decoder, &frame_info);
        WritePcm(pcm_buffer, frame_info.outputSamps, frame_info.nChans, frame_info.samprate);
        if (decoded_frames == 0) {
            if (station_index_ >= 0 && station_index_ < static_cast<int>(last_success_url_.size())) {
                last_success_url_[station_index_] = url_index;
            }
            reconnect_attempt_ = 0;
            ESP_LOGI(TAG, "radio remembered successful url station=%s url_index=%d", station_name.c_str(), url_index);
        }
        ++decoded_frames;
        if (decoded_frames == 1 || decoded_frames % 64 == 0) {
            char detail[64];
            snprintf(detail, sizeof(detail), "%d kbps %d Hz %u KB",
                     frame_info.bitrate / 1000, frame_info.samprate, static_cast<unsigned>(total_bytes / 1024));
            SetUi("Playing", detail);
            ESP_LOGI(TAG, "playing station=%s frames=%d rate=%d channels=%d total=%u KB",
                     station_name.c_str(), decoded_frames, frame_info.samprate, frame_info.nChans,
                     static_cast<unsigned>(total_bytes / 1024));
        }
    }

    Application::GetInstance().SetExternalAudioActive(false);
    MP3FreeDecoder(decoder);
    heap_caps_free(read_buffer);
    heap_caps_free(pcm_buffer);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return decoded_frames > 0 && play_requested_ && !stop_requested_ && !stream_failed &&
           stream_generation == stream_generation_.load(std::memory_order_relaxed) &&
           !audio_focus_blocked_.load(std::memory_order_relaxed);
}

bool RadioService::IsXiaozhiAudioState() const {
    auto app_state = Application::GetInstance().GetDeviceState();
    return app_state == kDeviceStateConnecting ||
           app_state == kDeviceStateListening ||
           app_state == kDeviceStateSpeaking ||
           app_state == kDeviceStateAudioTesting;
}

bool RadioService::IsCustomUrlSpeakingGraceActive() const {
    if (!playing_custom_url_ || !play_requested_.load(std::memory_order_relaxed)) {
        return false;
    }
    return static_cast<int32_t>(custom_url_speaking_grace_until_ - xTaskGetTickCount()) > 0;
}

bool RadioService::IsAutonomousCustomUrlSpeaking(int previous_state, int current_state) const {
    if (!playing_custom_url_ || !play_requested_.load(std::memory_order_relaxed) ||
        current_state != kDeviceStateSpeaking) {
        return false;
    }
    return previous_state != kDeviceStateListening &&
           previous_state != kDeviceStateConnecting &&
           previous_state != kDeviceStateAudioTesting;
}

bool RadioService::ShouldYieldAudio() const {
    auto app_state = Application::GetInstance().GetDeviceState();
    if (app_state == kDeviceStateSpeaking && IsCustomUrlSpeakingGraceActive()) {
        return false;
    }
    if (app_state == kDeviceStateSpeaking &&
        playing_custom_url_ &&
        play_requested_.load(std::memory_order_relaxed) &&
        !audio_focus_blocked_.load(std::memory_order_relaxed)) {
        return false;
    }
    return audio_focus_blocked_.load(std::memory_order_relaxed) ||
           app_state == kDeviceStateConnecting ||
           app_state == kDeviceStateListening ||
           app_state == kDeviceStateSpeaking ||
           app_state == kDeviceStateAudioTesting;
}

void RadioService::OnDeviceStateChanged(int previous_state, int current_state) {
    (void)previous_state;
    bool blocked = current_state == kDeviceStateConnecting ||
                   current_state == kDeviceStateListening ||
                   current_state == kDeviceStateSpeaking ||
                   current_state == kDeviceStateAudioTesting;
    if (current_state == kDeviceStateSpeaking && blocked &&
        (IsCustomUrlSpeakingGraceActive() || IsAutonomousCustomUrlSpeaking(previous_state, current_state))) {
        ESP_LOGI(TAG, "audio focus speaking ignored during music url playback previous=%d", previous_state);
        blocked = false;
        Application::GetInstance().Schedule([]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateSpeaking) {
                app.AbortSpeaking(kAbortReasonNone);
                app.SetDeviceState(kDeviceStateIdle);
            }
        });
    }
    const bool was_blocked = audio_focus_blocked_.exchange(blocked, std::memory_order_relaxed);
    if (blocked != was_blocked) {
        ESP_LOGI(TAG, "audio focus %s by XiaoZhi state=%d play_requested=%d",
                 blocked ? "blocked" : "released", current_state, play_requested_.load(std::memory_order_relaxed));
        PostCommand(Command::FOCUS_CHANGED);
    }
}

void RadioService::NextStation(int delta) {
    const int count = StationCount();
    if (count <= 0) {
        ESP_LOGW(TAG, "NextStation: no stations available");
        return;
    }
    if (station_index_ < 0 || station_index_ >= count) {
        station_index_ = 0;
        ESP_LOGW(TAG, "NextStation: station_index_ was out of range, reset to 0");
    }
    station_index_ = (station_index_ + delta + count) % count;
    ESP_LOGI(TAG, "NextStation: new index=%d count=%d name=%s", station_index_, count, kStations[station_index_].name.c_str());
    SaveStationIndex();
}

void RadioService::SetUi(const char* state, const char* detail) {
    if (!desktop_ui_) {
        return;
    }
    const auto& station = kStations[station_index_];
    char meta[96];
    snprintf(meta, sizeof(meta), "%s %d kbps  %s", station.codec.c_str(), station.bitrate_kbps, detail ? detail : "");
    if (lvgl_port_lock(100)) {
        desktop_ui_->SetRadioState(station.name.c_str(), state, meta);
        lvgl_port_unlock();
    }
}

void RadioService::WritePcm(const int16_t* pcm, int samples, int channels, int sample_rate) {
    if (!pcm || samples <= 0 || channels <= 0 || sample_rate <= 0) {
        return;
    }

    int frames = channels == 2 ? samples / 2 : samples;
    if (frames <= 0) {
        return;
    }

    pcm_mono_buf_.resize(frames);
    if (channels == 2) {
        for (int i = 0; i < frames; ++i) {
            pcm_mono_buf_[i] = Clamp16(((int)pcm[i * 2] + (int)pcm[i * 2 + 1]) / 2);
        }
    } else {
        for (int i = 0; i < frames; ++i) {
            pcm_mono_buf_[i] = pcm[i];
        }
    }

    auto* codec = Board::GetInstance().GetAudioCodec();
    const int out_rate = codec->output_sample_rate();
    if (sample_rate == out_rate) {
        pcm_output_buf_.swap(pcm_mono_buf_);
    } else {
        int out_frames = std::max(1, frames * out_rate / sample_rate);
        pcm_output_buf_.resize(out_frames);
        for (int i = 0; i < out_frames; ++i) {
            int64_t src_pos_q16 = (int64_t)i * sample_rate * 65536 / out_rate;
            int src_index = src_pos_q16 >> 16;
            int frac = src_pos_q16 & 0xffff;
            if (src_index >= frames - 1) {
                pcm_output_buf_[i] = pcm_mono_buf_[frames - 1];
            } else {
                int a = pcm_mono_buf_[src_index];
                int b = pcm_mono_buf_[src_index + 1];
                pcm_output_buf_[i] = Clamp16((a * (65536 - frac) + b * frac) >> 16);
            }
        }
    }

    ApplyAudioLeveler(pcm_output_buf_);
    if (!codec->output_enabled()) {
        codec->EnableOutput(true);
    }
    codec->OutputData(pcm_output_buf_);
}

void RadioService::ResetAudioLeveler() {
    audio_gain_q12_ = 4096;
}

void RadioService::ApplyAudioLeveler(std::vector<int16_t>& pcm) {
    if (pcm.empty()) {
        return;
    }

    int peak = 0;
    int64_t abs_sum = 0;
    for (int16_t sample : pcm) {
        int v = std::abs(static_cast<int>(sample));
        peak = std::max(peak, v);
        abs_sum += v;
    }

    const int avg_abs = static_cast<int>(abs_sum / static_cast<int64_t>(pcm.size()));
    if (avg_abs <= 0 || peak <= 0) {
        return;
    }

    constexpr int kTargetAvg = 5000;
    constexpr int kLimiterPeak = 24500;
    constexpr int32_t kMinGainQ12 = 2300;   // about 0.56x
    constexpr int32_t kMaxGainQ12 = 8200;   // about 2.00x

    int32_t desired = static_cast<int32_t>((static_cast<int64_t>(kTargetAvg) * 4096) / avg_abs);
    const int32_t peak_limited = static_cast<int32_t>((static_cast<int64_t>(kLimiterPeak) * 4096) / peak);
    desired = std::min(desired, peak_limited);
    desired = std::clamp(desired, kMinGainQ12, kMaxGainQ12);

    if (desired < audio_gain_q12_) {
        audio_gain_q12_ = (audio_gain_q12_ * 3 + desired) / 4;
    } else {
        audio_gain_q12_ = (audio_gain_q12_ * 31 + desired) / 32;
    }

    for (int16_t& sample : pcm) {
        int32_t v = static_cast<int32_t>((static_cast<int64_t>(sample) * audio_gain_q12_) >> 12);
        int32_t av = std::abs(v);
        if (av > 23500) {
            int32_t over = av - 23500;
            av = 23500 + over / 4;
            if (av > 29500) {
                av = 29500;
            }
            v = v < 0 ? -av : av;
        }
        sample = Clamp16(v);
    }
}
