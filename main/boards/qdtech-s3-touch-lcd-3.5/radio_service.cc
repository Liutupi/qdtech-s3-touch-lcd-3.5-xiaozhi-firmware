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
#include "wifi_station.h"

static const char* TAG = "RadioService";

static constexpr int kReadBufferSize = 16 * 1024;
static constexpr int kReadTargetBytes = 8 * 1024;
static constexpr int kReadChunkBytes = 1024;
static constexpr int kPcmMaxSamples = MAX_NCHAN * MAX_NGRAN * MAX_NSAMP;

struct RadioStation {
    const char* name;
    const char* urls[3];
    const char* codec;
    int bitrate_kbps;
};

static const RadioStation kStations[] = {
    {"CNR China Voice", {"https://lhttp.qtfm.cn/live/15318317/64k.mp3", "https://lhttp-hw.qtfm.cn/live/15318317/64k.mp3", nullptr}, "MP3", 64},
    {"Guangzhou News", {"http://lhttp.qingting.fm/live/4848/64k.mp3", "https://lhttp.qtfm.cn/live/4848/64k.mp3", nullptr}, "MP3", 64},
    {"Guangzhou Traffic", {"http://lhttp.qingting.fm/live/4955/64k.mp3", "https://lhttp.qtfm.cn/live/4955/64k.mp3", nullptr}, "MP3", 64},
    {"Pearl River FM", {"http://lhttp.qingting.fm/live/1259/64k.mp3", "https://lhttp.qtfm.cn/live/1259/64k.mp3", nullptr}, "MP3", 64},
    {"Guangdong Music", {"http://lhttp.qingting.fm/live/1260/64k.mp3", "https://lhttp.qtfm.cn/live/1260/64k.mp3", nullptr}, "MP3", 64},
    {"Shenzhen FM971", {"http://lhttp.qingting.fm/live/1271/64k.mp3", "https://lhttp.qtfm.cn/live/1271/64k.mp3", nullptr}, "MP3", 64},
};

static int StationCount() {
    return sizeof(kStations) / sizeof(kStations[0]);
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
    queue_ = xQueueCreate(8, sizeof(Command));
    xTaskCreate([](void* arg) {
        static_cast<RadioService*>(arg)->Task();
    }, "radio_service", 8192, this, 4, nullptr);
    SetUi("Ready", "Tap Play");
}

void RadioService::PlayPause() {
    PostCommand(Command::PLAY_PAUSE);
}

void RadioService::Play() {
    play_requested_ = false;
    PostCommand(Command::PLAY_PAUSE);
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
            continue;
        }
        if (!WifiStation::GetInstance().IsConnected()) {
            SetUi("Waiting WiFi", "Need network");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        PlayCurrentStation();
        if (play_requested_) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void RadioService::HandleCommand(Command command) {
    switch (command) {
        case Command::PLAY_PAUSE:
            play_requested_ = !play_requested_;
            stop_requested_ = !play_requested_;
            if (!play_requested_) {
                Application::GetInstance().SetExternalAudioActive(false);
            }
            SetUi(play_requested_ ? "Connecting" : "Paused", play_requested_ ? "Opening stream" : "Stopped");
            break;
        case Command::STOP:
            play_requested_ = false;
            stop_requested_ = true;
            Application::GetInstance().SetExternalAudioActive(false);
            SetUi("Stopped", "Ready");
            break;
        case Command::NEXT:
            NextStation(1);
            play_requested_ = true;
            stop_requested_ = false;
            SetUi("Connecting", "Next station");
            break;
        case Command::PREV:
            NextStation(-1);
            play_requested_ = true;
            stop_requested_ = false;
            SetUi("Connecting", "Previous station");
            break;
    }
}

void RadioService::PlayCurrentStation() {
    const auto& station = kStations[station_index_];
    bool tried_any = false;
    stop_requested_ = false;
    for (int i = 0; i < 3 && play_requested_ && !stop_requested_; ++i) {
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
        ESP_LOGW(TAG, "open failed station=%s err=%s", station.name, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status < 200 || status >= 400) {
        ESP_LOGW(TAG, "bad stream status station=%s status=%d len=%d", station.name, status, content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    ESP_LOGI(TAG, "stream open station=%s status=%d len=%d", station.name, status, content_length);
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
        auto app_state = Application::GetInstance().GetDeviceState();
        if (app_state == kDeviceStateConnecting || app_state == kDeviceStateListening || app_state == kDeviceStateSpeaking) {
            SetUi("Paused", "XiaoZhi is using audio");
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
            ESP_LOGW(TAG, "stream read failed read=%d", read);
            stop_requested_ = true;
            break;
        }

        if (bytes_left <= 0) {
            SetUi("Buffering", "Waiting for data");
            vTaskDelay(pdMS_TO_TICKS(80));
            continue;
        }

        int offset = MP3FindSyncWord(read_ptr, bytes_left);
        if (offset < 0) {
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
            ESP_LOGW(TAG, "decode err=%d count=%d", mp3_err, decode_errors);
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
    return decoded_frames > 0 && play_requested_ && !stop_requested_;
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
