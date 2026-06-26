#include "podcast_service.h"

#include "application.h"
#include "audio_codec.h"
#include "board.h"
#include "cJSON.h"
#include "config.h"
#include "desktop_ui.h"
#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "jpeg_decoder.h"
#include "mp3dec.h"
#include "sdmmc_cmd.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <utility>

static const char* TAG = "PodcastService";

namespace {
constexpr const char* kMountPoint = "/sdcard";
constexpr const char* kPodcastDir = "/sdcard/podcast";
constexpr const char* kIndexPath = "/sdcard/podcast/index.json";
constexpr int kReadBufferSize = 16 * 1024;
constexpr int kReadTargetBytes = 8 * 1024;
constexpr int kReadChunkBytes = 2048;
constexpr int kPcmMaxSamples = MAX_NCHAN * MAX_NGRAN * MAX_NSAMP;
constexpr size_t kMaxIndexBytes = 96 * 1024;
constexpr size_t kMaxSummaryBytes = 1536;
constexpr size_t kMaxCoverInputBytes = 768 * 1024;
constexpr size_t kMaxCoverOutputBytes = 240 * 240 * 2;
constexpr size_t kPodcastVisibleRows = 8;

std::string JoinPodcastPath(const char* value) {
    if (!value || value[0] == '\0') {
        return "";
    }
    std::string path(value);
    if (path.rfind("/sdcard/", 0) == 0) {
        return path;
    }
    return std::string(kPodcastDir) + "/" + path;
}

std::string TrimSummary(const std::string& text, size_t limit) {
    std::string clean;
    clean.reserve(std::min(text.size(), limit + 8));
    for (char c : text) {
        if (c == '\r') {
            continue;
        }
        clean.push_back(c == '\n' ? ' ' : c);
        if (clean.size() >= limit) {
            clean += "...";
            break;
        }
    }
    return clean;
}

std::string ReplaceAll(std::string text, const char* from, const char* to) {
    size_t pos = 0;
    const size_t from_len = strlen(from);
    while (from_len > 0 && (pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from_len, to);
        pos += strlen(to);
    }
    return text;
}

std::string NormalizeDisplayText(const std::string& text) {
    std::string clean = text;
    clean = ReplaceAll(std::move(clean), "\xE2\x80\x9C", "\"");
    clean = ReplaceAll(std::move(clean), "\xE2\x80\x9D", "\"");
    clean = ReplaceAll(std::move(clean), "\xE2\x80\x98", "'");
    clean = ReplaceAll(std::move(clean), "\xE2\x80\x99", "'");
    clean = ReplaceAll(std::move(clean), "\xE2\x80\x94", "-");
    clean = ReplaceAll(std::move(clean), "\xE2\x80\x93", "-");
    return clean;
}

std::string Utf8SafePrefix(const std::string& text, size_t max_bytes) {
    if (text.size() <= max_bytes) {
        return text;
    }
    size_t pos = 0;
    while (pos < text.size() && pos < max_bytes) {
        const unsigned char c = static_cast<unsigned char>(text[pos]);
        size_t char_len = 1;
        if ((c & 0x80) == 0x00) {
            char_len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            char_len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            char_len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            char_len = 4;
        }
        if (pos + char_len > max_bytes || pos + char_len > text.size()) {
            break;
        }
        pos += char_len;
    }
    std::string out = text.substr(0, pos);
    out += "...";
    return out;
}

int16_t Clamp16(int value) {
    if (value > 32767) {
        return 32767;
    }
    if (value < -32768) {
        return -32768;
    }
    return static_cast<int16_t>(value);
}

esp_jpeg_image_scale_t ChooseCoverScale(uint16_t width, uint16_t height) {
    if (width <= 240 && height <= 240 && static_cast<size_t>(width) * height * 2 <= kMaxCoverOutputBytes) {
        return JPEG_IMAGE_SCALE_0;
    }
    if (width / 2 <= 240 && height / 2 <= 240 &&
        static_cast<size_t>(width / 2) * (height / 2) * 2 <= kMaxCoverOutputBytes) {
        return JPEG_IMAGE_SCALE_1_2;
    }
    if (width / 4 <= 240 && height / 4 <= 240 &&
        static_cast<size_t>(width / 4) * (height / 4) * 2 <= kMaxCoverOutputBytes) {
        return JPEG_IMAGE_SCALE_1_4;
    }
    return JPEG_IMAGE_SCALE_1_8;
}

bool TryMountSdCard(uint8_t width, int max_freq_khz) {
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
        .max_files = 6,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    sdmmc_card_t* card = nullptr;
    esp_err_t err = esp_vfs_fat_sdmmc_mount(kMountPoint, &host, &slot_config, &mount_config, &card);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "sd mount failed width=%u err=%s", width, esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "sd mounted width=%u", width);
    return true;
}
} // namespace

void PodcastService::Start(DesktopUI* desktop_ui) {
    desktop_ui_ = desktop_ui;
    if (!desktop_ui_ || started_) {
        return;
    }
    started_ = true;
    desktop_ui_->SetPodcastActions(
        [this]() { PlayPause(); },
        [this]() { Stop(); },
        [this]() { Next(); },
        [this]() { Prev(); },
        [this]() { Up(); },
        [this]() { Down(); },
        [this](int percent) { SeekPercent(percent); });
    desktop_ui_->podcast_activate_ = [this]() {
        EnsureTaskStarted();
        PostCommand(Command::REFRESH_UI);
    };
    audio_focus_blocked_.store(IsXiaozhiAudioState(), std::memory_order_relaxed);
    Application::GetInstance().RegisterDeviceStateCallback([this](DeviceState previous, DeviceState current) {
        OnDeviceStateChanged(static_cast<int>(previous), static_cast<int>(current));
    });
}

void PodcastService::EnsureTaskStarted() {
    if (task_handle_) {
        return;
    }
    queue_ = xQueueCreate(8, sizeof(Command));
    BaseType_t ret = xTaskCreateWithCaps([](void* arg) {
        static_cast<PodcastService*>(arg)->Task();
    }, "podcast_service", 7168, this, 3, reinterpret_cast<TaskHandle_t*>(&task_handle_),
       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ret != pdPASS) {
        task_handle_ = nullptr;
        ret = xTaskCreate([](void* arg) {
            static_cast<PodcastService*>(arg)->Task();
        }, "podcast_service", 7168, this, 3, reinterpret_cast<TaskHandle_t*>(&task_handle_));
    }
    ESP_LOGI(TAG, "podcast task start ret=%ld", static_cast<long>(ret));
}

void PodcastService::PostCommand(Command command) {
    EnsureTaskStarted();
    auto queue = static_cast<QueueHandle_t>(queue_);
    if (queue) {
        xQueueSend(queue, &command, 0);
    }
}

void PodcastService::PlayPause() { PostCommand(Command::PLAY_PAUSE); }
void PodcastService::Stop() { PostCommand(Command::STOP); }
void PodcastService::Next() { PostCommand(Command::NEXT); }
void PodcastService::Prev() { PostCommand(Command::PREV); }
void PodcastService::Up() { PostCommand(Command::UP); }
void PodcastService::Down() { PostCommand(Command::DOWN); }
void PodcastService::SeekPercent(int percent) {
    pending_seek_percent_.store(std::clamp(percent, 0, 100), std::memory_order_relaxed);
    PostCommand(Command::SEEK);
}

void PodcastService::Task() {
    while (true) {
        Command command;
        auto queue = static_cast<QueueHandle_t>(queue_);
        if (queue && xQueueReceive(queue, &command, pdMS_TO_TICKS(250)) == pdTRUE) {
            HandleCommand(command);
        }

        if (!play_requested_) {
            continue;
        }
        if (audio_focus_blocked_.load(std::memory_order_relaxed)) {
            if (!focus_pause_logged_) {
                UpdateUi("Paused", "XiaoZhi is using audio");
                focus_pause_logged_ = true;
            }
            Application::GetInstance().SetExternalAudioActive(false);
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        focus_pause_logged_ = false;
        PlayCurrentEpisode();
    }
}

void PodcastService::HandleCommand(Command command) {
    if (!index_loaded_) {
        if (!LoadIndex()) {
            UpdateUi("No podcast", "Check /podcast on SD card");
            return;
        }
    }

    switch (command) {
        case Command::PLAY_PAUSE:
            if (play_requested_.load(std::memory_order_relaxed) && playing_index_ == selected_index_) {
                play_requested_ = false;
                stop_requested_ = true;
            } else {
                play_requested_ = true;
                stop_requested_ = playing_index_ >= 0 && playing_index_ != selected_index_;
            }
            if (!play_requested_) {
                Application::GetInstance().SetExternalAudioActive(false);
            }
            UpdateUi(play_requested_ ? "Opening" : "Paused", play_requested_ ? "Local MP3" : "Stopped");
            break;
        case Command::STOP:
            play_requested_ = false;
            stop_requested_ = true;
            Application::GetInstance().SetExternalAudioActive(false);
            UpdateUi("Stopped", "Ready");
            UpdateProgressUi(0);
            break;
        case Command::NEXT:
            SelectDelta(1, true);
            play_requested_ = true;
            stop_requested_ = false;
            UpdateUi("Opening", "Next episode");
            break;
        case Command::PREV:
            SelectDelta(-1, true);
            play_requested_ = true;
            stop_requested_ = false;
            UpdateUi("Opening", "Previous episode");
            break;
        case Command::UP:
            SelectDelta(-1, false);
            UpdateUi(play_requested_ ? "Playing" : "Selected", "List selection");
            break;
        case Command::DOWN:
            SelectDelta(1, false);
            UpdateUi(play_requested_ ? "Playing" : "Selected", "List selection");
            break;
        case Command::SEEK:
            pending_seek_percent_.store(-1, std::memory_order_relaxed);
            UpdateUi(play_requested_ ? "Playing" : "Ready", "Seek after playback starts");
            break;
        case Command::REFRESH_UI:
            UpdateUi("Ready", "Select an episode");
            UpdateProgressUi(0);
            break;
        case Command::FOCUS_CHANGED:
            if (audio_focus_blocked_.load(std::memory_order_relaxed)) {
                Application::GetInstance().SetExternalAudioActive(false);
                UpdateUi("Paused", "XiaoZhi is using audio");
            } else if (play_requested_) {
                UpdateUi("Opening", "Audio focus restored");
            }
            break;
    }
}

bool PodcastService::EnsureSdCardMounted() {
    DIR* existing = opendir(kMountPoint);
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

bool PodcastService::LoadIndex() {
    if (!EnsureSdCardMounted()) {
        ESP_LOGW(TAG, "sd card not mounted");
        return false;
    }
    FILE* file = fopen(kIndexPath, "rb");
    if (!file) {
        ESP_LOGW(TAG, "index not found: %s", kIndexPath);
        return false;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    if (size <= 0 || size > static_cast<long>(kMaxIndexBytes)) {
        fclose(file);
        ESP_LOGW(TAG, "index invalid size=%ld", size);
        return false;
    }
    char* buffer = static_cast<char*>(heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buffer) {
        fclose(file);
        return false;
    }
    size_t read = fread(buffer, 1, size, file);
    fclose(file);
    buffer[read] = '\0';

    cJSON* root = cJSON_Parse(buffer);
    heap_caps_free(buffer);
    if (!root) {
        ESP_LOGW(TAG, "index parse failed");
        return false;
    }
    cJSON* episodes = cJSON_GetObjectItem(root, "episodes");
    if (!cJSON_IsArray(episodes)) {
        cJSON_Delete(root);
        return false;
    }

    episodes_.clear();
    const int count = cJSON_GetArraySize(episodes);
    for (int i = 0; i < count && i < 128; ++i) {
        cJSON* item = cJSON_GetArrayItem(episodes, i);
        cJSON* title = cJSON_GetObjectItem(item, "title");
        cJSON* audio = cJSON_GetObjectItem(item, "audio");
        cJSON* cover = cJSON_GetObjectItem(item, "cover");
        cJSON* desc = cJSON_GetObjectItem(item, "desc");
        cJSON* vol = cJSON_GetObjectItem(item, "vol");
        if (!cJSON_IsString(title) || !cJSON_IsString(audio)) {
            continue;
        }
        Episode episode;
        episode.vol = cJSON_IsNumber(vol) ? vol->valueint : i + 1;
        episode.title = title->valuestring;
        episode.audio = JoinPodcastPath(audio->valuestring);
        episode.cover = cJSON_IsString(cover) ? JoinPodcastPath(cover->valuestring) : "";
        episode.desc = cJSON_IsString(desc) ? JoinPodcastPath(desc->valuestring) : "";
        episodes_.push_back(std::move(episode));
    }
    cJSON_Delete(root);
    index_loaded_ = !episodes_.empty();
    selected_index_ = episodes_.empty() ? 0 : static_cast<int>(episodes_.size()) - 1;
    playing_index_ = -1;
    list_top_ = selected_index_ >= 4 ? selected_index_ - 4 : 0;
    ESP_LOGI(TAG, "loaded podcast episodes=%u", static_cast<unsigned>(episodes_.size()));
    return index_loaded_;
}

bool PodcastService::LoadSummary(Episode& episode) {
    if (episode.desc.empty()) {
        episode.summary = "No description file.";
        return false;
    }
    FILE* file = fopen(episode.desc.c_str(), "rb");
    if (!file) {
        episode.summary = "Description not found.";
        return false;
    }
    char buffer[kMaxSummaryBytes + 1];
    size_t read = fread(buffer, 1, kMaxSummaryBytes, file);
    fclose(file);
    buffer[read] = '\0';
    episode.summary = TrimSummary(buffer, 220);
    return true;
}

void PodcastService::EnsureSelectedDetails(bool decode_cover) {
    if (episodes_.empty()) {
        return;
    }
    selected_index_ = std::clamp(selected_index_, 0, static_cast<int>(episodes_.size()) - 1);
    Episode& episode = episodes_[selected_index_];
    if (episode.summary.empty()) {
        LoadSummary(episode);
    }
    if (decode_cover && !episode.cover.empty()) {
        DecodeCover(episode.cover);
    }
}

bool PodcastService::DecodeCover(const std::string& path) {
    struct stat st = {};
    if (path.empty() || stat(path.c_str(), &st) != 0 || st.st_size <= 0 ||
        static_cast<size_t>(st.st_size) > kMaxCoverInputBytes) {
        return false;
    }
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) {
        return false;
    }
    uint8_t* input = static_cast<uint8_t*>(heap_caps_malloc(st.st_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!input) {
        fclose(file);
        return false;
    }
    size_t read = fread(input, 1, st.st_size, file);
    fclose(file);
    if (read != static_cast<size_t>(st.st_size)) {
        heap_caps_free(input);
        return false;
    }

    esp_jpeg_image_cfg_t cfg = {
        .indata = input,
        .indata_size = static_cast<uint32_t>(st.st_size),
        .outbuf = nullptr,
        .outbuf_size = 0,
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale = JPEG_IMAGE_SCALE_0,
        .flags = {
            .swap_color_bytes = 0,
        },
        .advanced = {
            .working_buffer = nullptr,
            .working_buffer_size = 0,
        },
        .priv = {},
    };
    esp_jpeg_image_output_t info = {};
    esp_err_t err = esp_jpeg_get_image_info(&cfg, &info);
    if (err != ESP_OK || info.width == 0 || info.height == 0) {
        heap_caps_free(input);
        return false;
    }
    cfg.out_scale = ChooseCoverScale(info.width, info.height);
    esp_jpeg_image_output_t out = {};
    err = esp_jpeg_get_image_info(&cfg, &out);
    if (err != ESP_OK || out.output_len == 0 || out.output_len > kMaxCoverOutputBytes) {
        heap_caps_free(input);
        return false;
    }
    uint8_t* output = static_cast<uint8_t*>(heap_caps_malloc(out.output_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!output) {
        heap_caps_free(input);
        return false;
    }
    cfg.outbuf = output;
    cfg.outbuf_size = out.output_len;
    err = esp_jpeg_decode(&cfg, &out);
    heap_caps_free(input);
    if (err != ESP_OK) {
        heap_caps_free(output);
        return false;
    }
    FreeCover();
    memset(&cover_.dsc, 0, sizeof(cover_.dsc));
    cover_.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    cover_.dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    cover_.dsc.header.flags = LV_IMAGE_FLAGS_ALLOCATED;
    cover_.dsc.header.w = out.width;
    cover_.dsc.header.h = out.height;
    cover_.dsc.header.stride = out.width * 2;
    cover_.dsc.data_size = out.output_len;
    cover_.dsc.data = output;
    cover_.data = output;
    UpdateCoverUi();
    return true;
}

void PodcastService::FreeCover() {
    if (cover_.data) {
        heap_caps_free(cover_.data);
        cover_.data = nullptr;
    }
    memset(&cover_.dsc, 0, sizeof(cover_.dsc));
}

void PodcastService::UpdateUi(const char* state, const char* detail) {
    if (!desktop_ui_) {
        return;
    }
    if (episodes_.empty()) {
        if (lvgl_port_lock(100)) {
            desktop_ui_->SetPodcastState("Nothing Impossible", state, detail, "No episodes loaded.", "");
            lvgl_port_unlock();
        }
        return;
    }
    selected_index_ = std::clamp(selected_index_, 0, static_cast<int>(episodes_.size()) - 1);
    EnsureSelectedDetails(false);
    const Episode& episode = episodes_[selected_index_];
    if (selected_index_ < static_cast<int>(list_top_)) {
        list_top_ = selected_index_;
    } else if (selected_index_ >= static_cast<int>(list_top_ + kPodcastVisibleRows)) {
        list_top_ = selected_index_ - (kPodcastVisibleRows - 1);
    }

    char meta[96];
    snprintf(meta, sizeof(meta), "Vol.%02d  %u episodes  %s", episode.vol,
             static_cast<unsigned>(episodes_.size()), detail ? detail : "");
    std::string list;
    const size_t end = std::min(episodes_.size(), list_top_ + kPodcastVisibleRows);
    for (size_t i = list_top_; i < end; ++i) {
        const Episode& item = episodes_[i];
        const std::string title = Utf8SafePrefix(NormalizeDisplayText(item.title), 66);
        char line[192];
        snprintf(line, sizeof(line), "%c %02d  %s\n",
                 static_cast<int>(i) == selected_index_ ? '>' : ' ',
                 item.vol, title.c_str());
        list += line;
    }
    if (lvgl_port_lock(100)) {
        const std::string title = NormalizeDisplayText(episode.title);
        const std::string summary = NormalizeDisplayText(episode.summary);
        desktop_ui_->SetPodcastState(title.c_str(), state, meta,
                                     summary.c_str(), list.c_str());
        lvgl_port_unlock();
    }
}

void PodcastService::UpdateProgressUi(int percent) {
    if (!desktop_ui_) {
        return;
    }
    if (lvgl_port_lock(20)) {
        desktop_ui_->SetPodcastProgress(percent);
        lvgl_port_unlock();
    }
}

void PodcastService::UpdateCoverUi() {
    if (!desktop_ui_ || !cover_.data) {
        return;
    }
    if (lvgl_port_lock(100)) {
        desktop_ui_->SetPodcastCover(&cover_.dsc);
        lvgl_port_unlock();
    }
}

void PodcastService::SelectDelta(int delta, bool interrupt_playback) {
    if (episodes_.empty()) {
        return;
    }
    selected_index_ = (selected_index_ + delta + static_cast<int>(episodes_.size())) % static_cast<int>(episodes_.size());
    if (interrupt_playback && playing_index_ != selected_index_) {
        stop_requested_ = true;
    }
    EnsureSelectedDetails(false);
}

void PodcastService::PlayCurrentEpisode() {
    if (episodes_.empty()) {
        play_requested_ = false;
        UpdateUi("No podcast", "No episodes");
        return;
    }
    selected_index_ = std::clamp(selected_index_, 0, static_cast<int>(episodes_.size()) - 1);
    playing_index_ = selected_index_;
    stop_requested_ = false;
    EnsureSelectedDetails(true);
    const Episode& episode = episodes_[selected_index_];
    UpdateProgressUi(0);
    UpdateUi("Buffering", "Opening file");
    if (!PlayFile(episode.audio.c_str()) && play_requested_ && !stop_requested_) {
        UpdateUi("Error", "Playback failed");
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}

bool PodcastService::PlayFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        ESP_LOGW(TAG, "audio open failed: %s", path);
        return false;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    HMP3Decoder decoder = MP3InitDecoder();
    auto* read_buffer = static_cast<uint8_t*>(heap_caps_malloc(kReadBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    auto* pcm_buffer = static_cast<int16_t*>(heap_caps_malloc(kPcmMaxSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!decoder || !read_buffer || !pcm_buffer) {
        if (decoder) {
            MP3FreeDecoder(decoder);
        }
        heap_caps_free(read_buffer);
        heap_caps_free(pcm_buffer);
        fclose(file);
        return false;
    }

    uint8_t* read_ptr = read_buffer;
    int bytes_left = 0;
    long total_read = 0;
    int decoded_frames = 0;
    int decode_errors = 0;
    bool eof = false;
    ResetAudioLeveler();
    Application::GetInstance().SetExternalAudioActive(true);

    while (play_requested_ && !stop_requested_) {
        Command command;
        auto queue = static_cast<QueueHandle_t>(queue_);
        while (queue && xQueueReceive(queue, &command, 0) == pdTRUE) {
            if (!HandlePlaybackCommand(command, file, file_size, read_buffer, read_ptr, bytes_left,
                                       total_read, eof, decode_errors)) {
                break;
            }
        }
        if (!play_requested_ || stop_requested_) {
            break;
        }
        if (ShouldYieldAudio()) {
            UpdateUi("Paused", "XiaoZhi is using audio");
            break;
        }
        if (read_ptr != read_buffer && bytes_left > 0) {
            memmove(read_buffer, read_ptr, bytes_left);
            read_ptr = read_buffer;
        }
        while (!eof && bytes_left < kReadTargetBytes && bytes_left < kReadBufferSize) {
            int room = std::min(kReadChunkBytes, kReadBufferSize - bytes_left);
            size_t read = fread(read_buffer + bytes_left, 1, room, file);
            if (read > 0) {
                bytes_left += static_cast<int>(read);
                total_read += static_cast<long>(read);
            }
            if (read < static_cast<size_t>(room)) {
                eof = true;
                break;
            }
        }
        if (bytes_left <= 0) {
            break;
        }

        int offset = MP3FindSyncWord(read_ptr, bytes_left);
        if (offset < 0) {
            bytes_left = 0;
            read_ptr = read_buffer;
            if (eof) {
                break;
            }
            continue;
        }
        read_ptr += offset;
        bytes_left -= offset;

        int mp3_err = MP3Decode(decoder, &read_ptr, &bytes_left, pcm_buffer, 0);
        if (mp3_err == ERR_MP3_INDATA_UNDERFLOW || mp3_err == ERR_MP3_MAINDATA_UNDERFLOW) {
            if (eof) {
                break;
            }
            continue;
        }
        if (mp3_err != ERR_MP3_NONE) {
            ++decode_errors;
            if (decode_errors > 30) {
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
        if (decoded_frames == 1 || decoded_frames % 128 == 0) {
            char detail[48];
            int percent = file_size > 0 ? static_cast<int>((total_read * 100) / file_size) : 0;
            snprintf(detail, sizeof(detail), "%d kbps %d%%", frame_info.bitrate / 1000, percent);
            UpdateUi("Playing", detail);
            UpdateProgressUi(percent);
        }
    }

    Application::GetInstance().SetExternalAudioActive(false);
    MP3FreeDecoder(decoder);
    heap_caps_free(read_buffer);
    heap_caps_free(pcm_buffer);
    fclose(file);
    if (eof && play_requested_ && !stop_requested_ && playing_index_ == selected_index_) {
        SelectDelta(1, true);
    }
    return decoded_frames > 0;
}

bool PodcastService::HandlePlaybackCommand(Command command, FILE* file, long file_size, uint8_t* read_buffer,
                                           uint8_t*& read_ptr, int& bytes_left, long& total_read,
                                           bool& eof, int& decode_errors) {
    if (episodes_.empty()) {
        play_requested_ = false;
        stop_requested_ = true;
        return false;
    }

    auto move_selection = [this](int delta) {
        selected_index_ = (selected_index_ + delta + static_cast<int>(episodes_.size())) %
                          static_cast<int>(episodes_.size());
    };

    switch (command) {
        case Command::PLAY_PAUSE:
            if (selected_index_ != playing_index_) {
                play_requested_ = true;
                stop_requested_ = true;
                UpdateUi("Opening", "Selected episode");
            } else {
                play_requested_ = false;
                stop_requested_ = true;
                Application::GetInstance().SetExternalAudioActive(false);
                UpdateUi("Paused", "Stopped");
            }
            return false;
        case Command::STOP:
            play_requested_ = false;
            stop_requested_ = true;
            Application::GetInstance().SetExternalAudioActive(false);
            UpdateUi("Stopped", "Ready");
            UpdateProgressUi(0);
            return false;
        case Command::NEXT:
            move_selection(1);
            play_requested_ = true;
            stop_requested_ = true;
            UpdateUi("Opening", "Next episode");
            return false;
        case Command::PREV:
            move_selection(-1);
            play_requested_ = true;
            stop_requested_ = true;
            UpdateUi("Opening", "Previous episode");
            return false;
        case Command::UP:
            move_selection(-1);
            UpdateUi("Playing", "List selection");
            return true;
        case Command::DOWN:
            move_selection(1);
            UpdateUi("Playing", "List selection");
            return true;
        case Command::SEEK: {
            const int percent = pending_seek_percent_.exchange(-1, std::memory_order_relaxed);
            if (percent < 0 || !file || file_size <= 0) {
                return true;
            }
            const long target = std::clamp((file_size * static_cast<long>(percent)) / 100L, 0L, file_size);
            if (fseek(file, target, SEEK_SET) == 0) {
                bytes_left = 0;
                read_ptr = read_buffer;
                total_read = target;
                eof = false;
                decode_errors = 0;
                ResetAudioLeveler();
                UpdateProgressUi(percent);
                UpdateUi("Seeking", "Moving position");
            }
            return true;
        }
        case Command::REFRESH_UI:
            UpdateUi("Playing", "Local MP3");
            return true;
        case Command::FOCUS_CHANGED:
            if (audio_focus_blocked_.load(std::memory_order_relaxed)) {
                UpdateUi("Paused", "XiaoZhi is using audio");
                stop_requested_ = true;
                return false;
            }
            return true;
    }
    return true;
}

bool PodcastService::IsXiaozhiAudioState() const {
    auto app_state = Application::GetInstance().GetDeviceState();
    return app_state == kDeviceStateConnecting ||
           app_state == kDeviceStateListening ||
           app_state == kDeviceStateSpeaking ||
           app_state == kDeviceStateAudioTesting;
}

bool PodcastService::ShouldYieldAudio() const {
    return audio_focus_blocked_.load(std::memory_order_relaxed) || IsXiaozhiAudioState();
}

void PodcastService::OnDeviceStateChanged(int previous_state, int current_state) {
    (void)previous_state;
    const bool blocked = current_state == kDeviceStateConnecting ||
                         current_state == kDeviceStateListening ||
                         current_state == kDeviceStateSpeaking ||
                         current_state == kDeviceStateAudioTesting;
    const bool was_blocked = audio_focus_blocked_.exchange(blocked, std::memory_order_relaxed);
    if (blocked != was_blocked) {
        if (task_handle_ || play_requested_.load(std::memory_order_relaxed)) {
            PostCommand(Command::FOCUS_CHANGED);
        }
    }
}

void PodcastService::WritePcm(const int16_t* pcm, int samples, int channels, int sample_rate) {
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
            pcm_mono_buf_[i] = Clamp16((int)pcm[i] * 2);
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

void PodcastService::ResetAudioLeveler() {
    audio_gain_q12_ = 4096;
}

void PodcastService::ApplyAudioLeveler(std::vector<int16_t>& pcm) {
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

    constexpr int kTargetAvg = 5200;
    constexpr int kLimiterPeak = 25000;
    constexpr int32_t kMinGainQ12 = 2300;   // about 0.56x
    constexpr int32_t kMaxGainQ12 = 9200;   // about 2.25x

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
        if (av > 24000) {
            int32_t over = av - 24000;
            av = 24000 + over / 4;
            if (av > 30000) {
                av = 30000;
            }
            v = v < 0 ? -av : av;
        }
        sample = Clamp16(v);
    }
}
