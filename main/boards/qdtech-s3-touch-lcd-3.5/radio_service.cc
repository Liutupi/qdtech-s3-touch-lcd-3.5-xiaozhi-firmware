#include "radio_service.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "application.h"
#include "audio_codec.h"
#include "board.h"
#include "desktop_ui.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mp3dec.h"
#include "settings.h"
#include "wifi_station.h"

static const char* TAG = "RadioService";

static constexpr int kReadBufferSize = 16 * 1024;
static constexpr int kReadTargetBytes = 8 * 1024;
static constexpr int kReadChunkBytes = 1024;
static constexpr int kPcmMaxSamples = MAX_NCHAN * MAX_NGRAN * MAX_NSAMP;

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
    const char* name;
    const char* urls[3];
    const char* codec;
    int bitrate_kbps;
    RadioCategory category;
    bool favorite;
};

static RadioStation kStations[] = {
    // 全国性电台 (CNR)
    {"CNR China Voice", {"https://lhttp.qtfm.cn/live/15318317/64k.mp3", "https://lhttp-hw.qtfm.cn/live/15318317/64k.mp3", nullptr}, "MP3", 64, RadioCategory::NATIONAL, false},
    {"CNR Economic", {"https://lhttp.qingting.fm/live/15318569/64k.mp3", "https://lhttp.qtfm.cn/live/15318569/64k.mp3", nullptr}, "MP3", 64, RadioCategory::NATIONAL, false},
    {"CNR Music", {"https://lhttp.qtfm.cn/live/15318497/64k.mp3", "https://lhttp-hw.qtfm.cn/live/15318497/64k.mp3", nullptr}, "MP3", 64, RadioCategory::NATIONAL, false},
    {"CNR Traffic", {"https://lhttp.qtfm.cn/live/15318641/64k.mp3", "https://lhttp-hw.qtfm.cn/live/15318641/64k.mp3", nullptr}, "MP3", 64, RadioCategory::NATIONAL, false},
    {"CNR Literature", {"https://lhttp.qtfm.cn/live/15318785/64k.mp3", "https://lhttp-hw.qtfm.cn/live/15318785/64k.mp3", nullptr}, "MP3", 64, RadioCategory::NATIONAL, false},
    {"CNR Senior", {"https://lhttp.qtfm.cn/live/15318857/64k.mp3", "https://lhttp-hw.qtfm.cn/live/15318857/64k.mp3", nullptr}, "MP3", 64, RadioCategory::NATIONAL, false},

    // 北京电台
    {"Beijing News", {"https://lhttp.qtfm.cn/live/4848/64k.mp3", "https://lhttp-hw.qtfm.cn/live/4848/64k.mp3", nullptr}, "MP3", 64, RadioCategory::BEIJING, false},
    {"Beijing Traffic", {"https://lhttp.qtfm.cn/live/4955/64k.mp3", "https://lhttp-hw.qtfm.cn/live/4955/64k.mp3", nullptr}, "MP3", 64, RadioCategory::BEIJING, false},
    {"Beijing Music", {"https://lhttp.qtfm.cn/live/4938/64k.mp3", "https://lhttp-hw.qtfm.cn/live/4938/64k.mp3", nullptr}, "MP3", 64, RadioCategory::BEIJING, false},

    // 上海电台
    {"Shanghai News", {"https://lhttp.qtfm.cn/live/1259/64k.mp3", "https://lhttp-hw.qtfm.cn/live/1259/64k.mp3", nullptr}, "MP3", 64, RadioCategory::SHANGHAI, false},
    {"Shanghai Traffic", {"https://lhttp.qtfm.cn/live/1260/64k.mp3", "https://lhttp-hw.qtfm.cn/live/1260/64k.mp3", nullptr}, "MP3", 64, RadioCategory::SHANGHAI, false},
    {"Shanghai Music", {"https://lhttp.qtfm.cn/live/1271/64k.mp3", "https://lhttp-hw.qtfm.cn/live/1271/64k.mp3", nullptr}, "MP3", 64, RadioCategory::SHANGHAI, false},

    // 广东电台
    {"Guangzhou News", {"http://lhttp.qingting.fm/live/4848/64k.mp3", "https://lhttp.qtfm.cn/live/4848/64k.mp3", nullptr}, "MP3", 64, RadioCategory::GUANGDONG, false},
    {"Guangzhou Traffic", {"http://lhttp.qingting.fm/live/4955/64k.mp3", "https://lhttp.qtfm.cn/live/4955/64k.mp3", nullptr}, "MP3", 64, RadioCategory::GUANGDONG, false},
    {"Pearl River FM", {"http://lhttp.qingting.fm/live/1259/64k.mp3", "https://lhttp.qtfm.cn/live/1259/64k.mp3", nullptr}, "MP3", 64, RadioCategory::GUANGDONG, false},
    {"Guangdong Music", {"http://lhttp.qingting.fm/live/1260/64k.mp3", "https://lhttp.qtfm.cn/live/1260/64k.mp3", nullptr}, "MP3", 64, RadioCategory::GUANGDONG, false},
    {"Guangdong News", {"https://lhttp.qtfm.cn/live/471/64k.mp3", "https://lhttp-hw.qtfm.cn/live/471/64k.mp3", nullptr}, "MP3", 64, RadioCategory::GUANGDONG, false},
    {"Shenzhen FM971", {"http://lhttp.qingting.fm/live/1271/64k.mp3", "https://lhttp.qtfm.cn/live/1271/64k.mp3", nullptr}, "MP3", 64, RadioCategory::GUANGDONG, false},

    // 浙江电台
    {"Zhejiang Voice", {"https://lhttp.qtfm.cn/live/1223/64k.mp3", "https://lhttp-hw.qtfm.cn/live/1223/64k.mp3", nullptr}, "MP3", 64, RadioCategory::ZHEJIANG, false},
    {"Zhejiang Traffic", {"https://lhttp.qtfm.cn/live/5021381/64k.mp3", "https://lhttp-hw.qtfm.cn/live/5021381/64k.mp3", nullptr}, "MP3", 64, RadioCategory::ZHEJIANG, false},
    {"Zhejiang Music", {"https://lhttp.qtfm.cn/live/5022107/64k.mp3", "https://lhttp-hw.qtfm.cn/live/5022107/64k.mp3", nullptr}, "MP3", 64, RadioCategory::ZHEJIANG, false},

    // 江苏电台
    {"Jiangsu News", {"https://lhttp.qtfm.cn/live/5022308/64k.mp3", "https://lhttp-hw.qtfm.cn/live/5022308/64k.mp3", nullptr}, "MP3", 64, RadioCategory::JIANGSU, false},
    {"Jiangsu Traffic", {"https://lhttp.qtfm.cn/live/4915/64k.mp3", "https://lhttp-hw.qtfm.cn/live/4915/64k.mp3", nullptr}, "MP3", 64, RadioCategory::JIANGSU, false},

    // 四川电台
    {"Sichuan News", {"https://lhttp.qtfm.cn/live/4848/64k.mp3", "https://lhttp-hw.qtfm.cn/live/4848/64k.mp3", nullptr}, "MP3", 64, RadioCategory::SICHUAN, false},
    {"Sichuan Traffic", {"https://lhttp.qtfm.cn/live/4955/64k.mp3", "https://lhttp-hw.qtfm.cn/live/4955/64k.mp3", nullptr}, "MP3", 64, RadioCategory::SICHUAN, false},

    // 湖南电台
    {"Hunan News", {"https://lhttp.qtfm.cn/live/1259/64k.mp3", "https://lhttp-hw.qtfm.cn/live/1259/64k.mp3", nullptr}, "MP3", 64, RadioCategory::HUNAN, false},
    {"Hunan Traffic", {"https://lhttp.qtfm.cn/live/1260/64k.mp3", "https://lhttp-hw.qtfm.cn/live/1260/64k.mp3", nullptr}, "MP3", 64, RadioCategory::HUNAN, false},

    // 湖北电台
    {"Hubei News", {"https://lhttp.qtfm.cn/live/1271/64k.mp3", "https://lhttp-hw.qtfm.cn/live/1271/64k.mp3", nullptr}, "MP3", 64, RadioCategory::HUBEI, false},
    {"Hubei Traffic", {"https://lhttp.qtfm.cn/live/5021381/64k.mp3", "https://lhttp-hw.qtfm.cn/live/5021381/64k.mp3", nullptr}, "MP3", 64, RadioCategory::HUBEI, false},

    // 山东电台
    {"Shandong News", {"https://lhttp.qtfm.cn/live/5022107/64k.mp3", "https://lhttp-hw.qtfm.cn/live/5022107/64k.mp3", nullptr}, "MP3", 64, RadioCategory::SHANDONG, false},
    {"Shandong Traffic", {"https://lhttp.qtfm.cn/live/5022308/64k.mp3", "https://lhttp-hw.qtfm.cn/live/5022308/64k.mp3", nullptr}, "MP3", 64, RadioCategory::SHANDONG, false},

    // 音乐电台
    {"Music Radio", {"http://lhttp.qingting.fm/live/5022107/64k.mp3", "https://lhttp.qtfm.cn/live/5022107/64k.mp3", nullptr}, "MP3", 64, RadioCategory::MUSIC, false},
    {"Music FM", {"http://lhttp.qingting.fm/live/4938/64k.mp3", "https://lhttp.qtfm.cn/live/4938/64k.mp3", nullptr}, "MP3", 64, RadioCategory::MUSIC, false},
    {"West Lake Voice", {"http://lhttp.qingting.fm/live/1223/64k.mp3", "https://lhttp.qtfm.cn/live/1223/64k.mp3", nullptr}, "MP3", 64, RadioCategory::MUSIC, false},

    // 交通广播
    {"Traffic 959", {"http://lhttp.qingting.fm/live/5021381/64k.mp3", "https://lhttp.qtfm.cn/live/5021381/64k.mp3", nullptr}, "MP3", 64, RadioCategory::TRAFFIC, false},
    {"Business Radio", {"https://lhttp.qtfm.cn/live/5022308/64k.mp3", "https://lhttp-hw.qtfm.cn/live/5022308/64k.mp3", nullptr}, "MP3", 64, RadioCategory::TRAFFIC, false},

    // 其他电台
    {"Night Radio", {"http://lhttp.qingting.fm/live/4915/64k.mp3", "https://lhttp.qtfm.cn/live/4915/64k.mp3", nullptr}, "MP3", 64, RadioCategory::OTHER, false},
};

static int StationCount() {
    return sizeof(kStations) / sizeof(kStations[0]);
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

static std::string Lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

void RadioService::Start(DesktopUI* desktop_ui) {
    desktop_ui_ = desktop_ui;
    if (started_) {
        return;
    }
    started_ = true;
    std::fill_n(last_success_url_, sizeof(last_success_url_) / sizeof(last_success_url_[0]), -1);
    audio_focus_blocked_.store(IsXiaozhiAudioState(), std::memory_order_relaxed);
    queue_ = xQueueCreate(8, sizeof(Command));
    Application::GetInstance().RegisterDeviceStateCallback([this](DeviceState previous, DeviceState current) {
        OnDeviceStateChanged(static_cast<int>(previous), static_cast<int>(current));
    });
    LoadFavorites();
    xTaskCreate([](void* arg) {
        static_cast<RadioService*>(arg)->Task();
    }, "radio_service", 6144, this, 4, nullptr);
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
        return kStations[index].name;
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
    PostCommand(Command::PLAY_PAUSE);
}

void RadioService::Play() {
    if (!play_requested_) {
        PostCommand(Command::PLAY_PAUSE);
    } else {
        PostCommand(Command::FOCUS_CHANGED);
    }
}

void RadioService::Stop() {
    stop_requested_ = true;
    PostCommand(Command::STOP);
}

void RadioService::Next() {
    PostCommand(Command::NEXT);
}

void RadioService::Prev() {
    PostCommand(Command::PREV);
}

std::string RadioService::GetStatusJson() const {
    const auto& station = kStations[station_index_];
    char json[192];
    snprintf(json, sizeof(json),
             "{\"station\":\"%s\",\"state\":\"%s\",\"codec\":\"%s\",\"bitrate_kbps\":%d}",
             station.name, play_requested_ ? "playing" : "stopped", station.codec, station.bitrate_kbps);
    return json;
}

std::string RadioService::SelectStation(const std::string& station) {
    std::string needle = Lower(station);
    for (int i = 0; i < StationCount(); ++i) {
        if (Lower(kStations[i].name).find(needle) != std::string::npos) {
            station_index_ = i;
            play_requested_ = true;
            stop_requested_ = false;
            SetUi("Connecting", "Selected station");
            return std::string("Radio station selected: ") + kStations[i].name;
        }
    }
    return "Radio station not found.";
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

        PlayCurrentStation();
        if (play_requested_) {
            reconnect_attempt_++;
            int delay_ms = std::min(5000, 500 + reconnect_attempt_ * 500);
            ESP_LOGW(TAG, "radio reconnect scheduled station=%s attempt=%d delay=%dms",
                     kStations[station_index_].name, reconnect_attempt_, delay_ms);
            SetUi("Reconnecting", "Stream ended");
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }
}

void RadioService::HandleCommand(Command command) {
    switch (command) {
        case Command::PLAY_PAUSE:
            play_requested_ = !play_requested_;
            stop_requested_ = !play_requested_;
            reconnect_attempt_ = 0;
            if (!play_requested_) {
                Application::GetInstance().SetExternalAudioActive(false);
            }
            SetUi(play_requested_ ? "Connecting" : "Paused", play_requested_ ? "Opening stream" : "Stopped");
            ESP_LOGI(TAG, "radio %s station=%s", play_requested_ ? "play requested" : "paused", kStations[station_index_].name);
            break;
        case Command::STOP:
            play_requested_ = false;
            stop_requested_ = true;
            reconnect_attempt_ = 0;
            Application::GetInstance().SetExternalAudioActive(false);
            SetUi("Stopped", "Ready");
            ESP_LOGI(TAG, "radio stopped");
            break;
        case Command::NEXT:
            NextStation(1);
            play_requested_ = true;
            stop_requested_ = false;
            reconnect_attempt_ = 0;
            SetUi("Connecting", "Next station");
            ESP_LOGI(TAG, "radio next station=%s", kStations[station_index_].name);
            break;
        case Command::PREV:
            NextStation(-1);
            play_requested_ = true;
            stop_requested_ = false;
            reconnect_attempt_ = 0;
            SetUi("Connecting", "Previous station");
            ESP_LOGI(TAG, "radio previous station=%s", kStations[station_index_].name);
            break;
        case Command::FOCUS_CHANGED:
            if (audio_focus_blocked_.load(std::memory_order_relaxed)) {
                Application::GetInstance().SetExternalAudioActive(false);
                SetUi("Paused", "XiaoZhi is using audio");
            } else if (play_requested_) {
                SetUi("Connecting", "Audio focus restored");
                ESP_LOGI(TAG, "radio audio focus restored, resume station=%s", kStations[station_index_].name);
            }
            break;
    }
}

void RadioService::PlayCurrentStation() {
    const auto& station = kStations[station_index_];
    bool tried_any = false;
    stop_requested_ = false;
    int url_order[3] = {0, 1, 2};
    if (last_success_url_[station_index_] > 0 && last_success_url_[station_index_] < 3) {
        url_order[0] = last_success_url_[station_index_];
        url_order[1] = 0;
        url_order[2] = last_success_url_[station_index_] == 1 ? 2 : 1;
    }
    for (int order = 0; order < 3 && play_requested_ && !stop_requested_; ++order) {
        if (audio_focus_blocked_.load(std::memory_order_relaxed)) {
            ESP_LOGI(TAG, "radio station open deferred by audio focus");
            return;
        }
        const int i = url_order[order];
        const char* url = station.urls[i];
        if (!url) {
            continue;
        }
        tried_any = true;
        if (PlayUrl(url, i)) {
            return;
        }
        SetUi("Connecting", "Trying fallback");
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    if (play_requested_) {
        SetUi("Error", tried_any ? "All sources failed" : "No source");
    }
}

bool RadioService::PlayUrl(const char* url, int url_index) {
    const auto& station = kStations[station_index_];
    SetUi("Connecting", url_index == 0 ? "Opening stream" : "Fallback source");

    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 5000;
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
        ESP_LOGW(TAG, "open failed station=%s url_index=%d err=%s url=%s", station.name, url_index, esp_err_to_name(err), url);
        esp_http_client_cleanup(client);
        return false;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status < 200 || status >= 400) {
        ESP_LOGW(TAG, "bad stream status station=%s url_index=%d status=%d len=%d url=%s",
                 station.name, url_index, status, content_length, url);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    ESP_LOGI(TAG, "stream open station=%s url_index=%d status=%d len=%d url=%s",
             station.name, url_index, status, content_length, url);
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

    while (play_requested_ && !stop_requested_ && WifiStation::GetInstance().IsConnected()) {
        Command command;
        auto queue = static_cast<QueueHandle_t>(queue_);
        while (queue && xQueueReceive(queue, &command, 0) == pdTRUE) {
            HandleCommand(command);
        }
        if (!play_requested_ || stop_requested_) {
            break;
        }
        if (ShouldYieldAudio()) {
            SetUi("Paused", "XiaoZhi is using audio");
            ESP_LOGI(TAG, "radio yielded audio focus station=%s", station.name);
            break;
        }

        if (read_ptr != read_buffer && bytes_left > 0) {
            memmove(read_buffer, read_ptr, bytes_left);
            read_ptr = read_buffer;
        }
        while (bytes_left < kReadTargetBytes && bytes_left < kReadBufferSize) {
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
            ESP_LOGW(TAG, "stream read failed station=%s url_index=%d read=%d", station.name, url_index, read);
            stream_failed = true;
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
            ESP_LOGW(TAG, "mp3 sync not found station=%s url_index=%d bytes=%d", station.name, url_index, bytes_left);
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
            ESP_LOGW(TAG, "mp3 decode failed station=%s url_index=%d err=%d count=%d", station.name, url_index, mp3_err, decode_errors);
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
            last_success_url_[station_index_] = url_index;
            reconnect_attempt_ = 0;
            ESP_LOGI(TAG, "radio remembered successful url station=%s url_index=%d", station.name, url_index);
        }
        ++decoded_frames;
        if (decoded_frames == 1 || decoded_frames % 64 == 0) {
            char detail[64];
            snprintf(detail, sizeof(detail), "%d kbps %d Hz %u KB",
                     frame_info.bitrate / 1000, frame_info.samprate, static_cast<unsigned>(total_bytes / 1024));
            SetUi("Playing", detail);
            ESP_LOGI(TAG, "playing station=%s frames=%d rate=%d channels=%d total=%u KB",
                     station.name, decoded_frames, frame_info.samprate, frame_info.nChans,
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
           !audio_focus_blocked_.load(std::memory_order_relaxed);
}

bool RadioService::IsXiaozhiAudioState() const {
    auto app_state = Application::GetInstance().GetDeviceState();
    return app_state == kDeviceStateConnecting ||
           app_state == kDeviceStateListening ||
           app_state == kDeviceStateSpeaking ||
           app_state == kDeviceStateAudioTesting;
}

bool RadioService::ShouldYieldAudio() const {
    return audio_focus_blocked_.load(std::memory_order_relaxed) || IsXiaozhiAudioState();
}

void RadioService::OnDeviceStateChanged(int previous_state, int current_state) {
    (void)previous_state;
    const bool blocked = current_state == kDeviceStateConnecting ||
                         current_state == kDeviceStateListening ||
                         current_state == kDeviceStateSpeaking ||
                         current_state == kDeviceStateAudioTesting;
    const bool was_blocked = audio_focus_blocked_.exchange(blocked, std::memory_order_relaxed);
    if (blocked != was_blocked) {
        ESP_LOGI(TAG, "audio focus %s by XiaoZhi state=%d play_requested=%d",
                 blocked ? "blocked" : "released", current_state, play_requested_);
        PostCommand(Command::FOCUS_CHANGED);
    }
}

void RadioService::NextStation(int delta) {
    station_index_ = (station_index_ + delta + StationCount()) % StationCount();
}

void RadioService::SetUi(const char* state, const char* detail) {
    if (!desktop_ui_) {
        return;
    }
    const auto& station = kStations[station_index_];
    char meta[96];
    snprintf(meta, sizeof(meta), "%s %d kbps  %s", station.codec, station.bitrate_kbps, detail ? detail : "");
    if (lvgl_port_lock(100)) {
        desktop_ui_->SetRadioState(station.name, state, meta);
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

    std::vector<int16_t> mono(frames);
    if (channels == 2) {
        for (int i = 0; i < frames; ++i) {
            mono[i] = Clamp16(((int)pcm[i * 2] + (int)pcm[i * 2 + 1]) / 2);
        }
    } else {
        for (int i = 0; i < frames; ++i) {
            mono[i] = Clamp16((int)pcm[i] * 2);
        }
    }

    auto* codec = Board::GetInstance().GetAudioCodec();
    const int out_rate = codec->output_sample_rate();
    std::vector<int16_t> output;
    if (sample_rate == out_rate) {
        output = std::move(mono);
    } else {
        int out_frames = std::max(1, frames * out_rate / sample_rate);
        output.resize(out_frames);
        for (int i = 0; i < out_frames; ++i) {
            int64_t src_pos_q16 = (int64_t)i * sample_rate * 65536 / out_rate;
            int src_index = src_pos_q16 >> 16;
            int frac = src_pos_q16 & 0xffff;
            if (src_index >= frames - 1) {
                output[i] = mono[frames - 1];
            } else {
                int a = mono[src_index];
                int b = mono[src_index + 1];
                output[i] = Clamp16((a * (65536 - frac) + b * frac) >> 16);
            }
        }
    }

    if (!codec->output_enabled()) {
        codec->EnableOutput(true);
    }
    codec->OutputData(output);
}
