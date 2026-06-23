#include "firmware_update_service.h"

#include "application.h"
#include "audio_codecs/audio_codec.h"
#include "board.h"
#include "ota.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/idf_additions.h"
#include "wifi_station.h"

static const char* TAG = "FirmwareUpdate";
static constexpr char kLatestReleaseUrl[] =
    "https://api.github.com/repos/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware/releases/latest";
static constexpr char kBoardAssetPrefix[] = "qdtech-s3-touch-lcd-3.5-";
static constexpr char kAppAssetSuffix[] = "-app.bin";
static constexpr size_t kMaxReleaseJson = 32768;

struct ReleaseHttpBuffer {
    std::string data;
    bool truncated = false;
};

static bool EndsWith(const std::string& text, const char* suffix) {
    const size_t suffix_len = strlen(suffix);
    return text.size() >= suffix_len &&
           text.compare(text.size() - suffix_len, suffix_len, suffix) == 0;
}

static std::string StripLeadingV(const std::string& version) {
    if (!version.empty() && (version[0] == 'v' || version[0] == 'V')) {
        return version.substr(1);
    }
    return version;
}

static esp_err_t ReleaseHttpEventHandler(esp_http_client_event_t* evt) {
    if (evt->event_id != HTTP_EVENT_ON_DATA || !evt->user_data || !evt->data || evt->data_len <= 0) {
        return ESP_OK;
    }

    auto* buffer = static_cast<ReleaseHttpBuffer*>(evt->user_data);
    const size_t room = kMaxReleaseJson > buffer->data.size()
        ? kMaxReleaseJson - buffer->data.size()
        : 0;
    if (room == 0) {
        buffer->truncated = true;
        return ESP_OK;
    }

    const size_t copy_len = std::min(room, static_cast<size_t>(evt->data_len));
    buffer->data.append(static_cast<const char*>(evt->data), copy_len);
    if (copy_len < static_cast<size_t>(evt->data_len)) {
        buffer->truncated = true;
    }
    return ESP_OK;
}

FirmwareUpdateService& FirmwareUpdateService::GetInstance() {
    static FirmwareUpdateService service;
    return service;
}

void FirmwareUpdateService::Start(DesktopUI* ui) {
    desktop_ui_ = ui;
    SetUi("Tap Check", false, false);
}

void FirmwareUpdateService::HandleButton() {
    if (busy_.load(std::memory_order_acquire)) {
        bool has_update = false;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            has_update = update_available_;
        }
        SetUi("Busy", has_update, true);
        return;
    }

    bool has_update = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        has_update = update_available_ && !firmware_url_.empty();
    }
    StartTask(has_update ? TaskAction::Upgrade : TaskAction::Check);
}

void FirmwareUpdateService::StartTask(TaskAction action) {
    bool expected = false;
    if (!busy_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    auto* arg = new (std::nothrow) TaskArg{this, action};
    if (!arg) {
        busy_.store(false, std::memory_order_release);
        SetUi("No memory", false, false);
        return;
    }

    constexpr uint32_t stack_size = 10240;
    const char* task_name = action == TaskAction::Check ? "fw_check" : "fw_upgrade";
    ESP_LOGI(TAG, "firmware task create action=%d free_internal=%u largest_internal=%u",
             static_cast<int>(action),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));

    BaseType_t ret = xTaskCreateWithCaps(
        TaskWrapper, task_name, stack_size, arg, 2, &task_handle_,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ret == pdPASS) {
        return;
    }

    ESP_LOGW(TAG, "firmware task PSRAM create failed ret=%ld, trying internal memory",
             static_cast<long>(ret));
    task_handle_ = nullptr;
    ret = xTaskCreate(TaskWrapper, task_name, stack_size, arg, 2, &task_handle_);
    if (ret != pdPASS) {
        delete arg;
        task_handle_ = nullptr;
        busy_.store(false, std::memory_order_release);
        SetUi("Task failed", false, false);
        ESP_LOGE(TAG, "firmware task create failed ret=%ld", static_cast<long>(ret));
    }
}

void FirmwareUpdateService::TaskWrapper(void* arg) {
    std::unique_ptr<TaskArg> task_arg(static_cast<TaskArg*>(arg));
    FirmwareUpdateService* service = task_arg->service;
    service->Task(task_arg->action);
    service->task_handle_ = nullptr;
    vTaskDelete(nullptr);
}

void FirmwareUpdateService::Task(TaskAction action) {
    if (action == TaskAction::Check) {
        CheckLatest();
    } else {
        RunUpgrade();
    }
    busy_.store(false, std::memory_order_release);
}

void FirmwareUpdateService::CheckLatest() {
    if (!WifiStation::GetInstance().IsConnected()) {
        SetUi("WiFi needed", false, false);
        return;
    }

    SetUi("Checking...", false, true);
    std::string latest_version;
    std::string firmware_url;
    if (!FetchLatestRelease(&latest_version, &firmware_url)) {
        SetUi("Check failed", false, false);
        return;
    }

    const esp_app_desc_t* app_desc = esp_app_get_description();
    const std::string current_version = app_desc ? app_desc->version : "";
    const bool newer = IsNewerVersion(current_version, latest_version);
    if (!newer) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            latest_version_ = latest_version;
            firmware_url_.clear();
            update_available_ = false;
        }
        SetUi("Latest", false, false);
        ESP_LOGI(TAG, "firmware latest current=%s release=%s",
                 current_version.c_str(), latest_version.c_str());
        return;
    }

    if (firmware_url.empty()) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            latest_version_ = latest_version;
            firmware_url_.clear();
            update_available_ = false;
        }
        SetUi("No OTA asset", false, false);
        ESP_LOGW(TAG, "new release %s has no app OTA asset", latest_version.c_str());
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        latest_version_ = latest_version;
        firmware_url_ = firmware_url;
        update_available_ = true;
    }

    char status[32];
    snprintf(status, sizeof(status), "v%s ready", latest_version.c_str());
    SetUi(status, true, false);
    ESP_LOGI(TAG, "firmware update ready current=%s latest=%s url=%s",
             current_version.c_str(), latest_version.c_str(), firmware_url.c_str());
}

bool FirmwareUpdateService::FetchLatestRelease(std::string* version, std::string* firmware_url) {
    ReleaseHttpBuffer buffer;
    esp_err_t err = ESP_FAIL;
    int status = 0;

    for (int attempt = 0; attempt < 2; ++attempt) {
        buffer = ReleaseHttpBuffer{};
        buffer.data.reserve(8192);

        esp_http_client_config_t config = {};
        config.url = kLatestReleaseUrl;
        config.timeout_ms = 12000;
        config.event_handler = ReleaseHttpEventHandler;
        config.user_data = &buffer;
        config.crt_bundle_attach = esp_crt_bundle_attach;
        config.buffer_size = 1024;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "release http init failed");
            return false;
        }

        const esp_app_desc_t* app_desc = esp_app_get_description();
        std::string user_agent = std::string(BOARD_NAME "/") + (app_desc ? app_desc->version : "unknown");
        esp_http_client_set_header(client, "User-Agent", user_agent.c_str());
        esp_http_client_set_header(client, "Accept", "application/vnd.github+json");

        err = esp_http_client_perform(client);
        status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);
        if (err == ESP_OK && status >= 200 && status < 300 && !buffer.data.empty() && !buffer.truncated) {
            break;
        }

        ESP_LOGW(TAG, "release fetch attempt %d failed err=%s status=%d len=%u truncated=%d",
                 attempt + 1, esp_err_to_name(err), status,
                 static_cast<unsigned>(buffer.data.size()), buffer.truncated ? 1 : 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (err != ESP_OK || status < 200 || status >= 300 || buffer.data.empty() || buffer.truncated) {
        ESP_LOGE(TAG, "release fetch failed err=%s status=%d len=%u truncated=%d",
                 esp_err_to_name(err), status, static_cast<unsigned>(buffer.data.size()),
                 buffer.truncated ? 1 : 0);
        return false;
    }

    cJSON* root = cJSON_Parse(buffer.data.c_str());
    if (!root) {
        ESP_LOGE(TAG, "release json parse failed");
        return false;
    }

    cJSON* tag = cJSON_GetObjectItem(root, "tag_name");
    if (!cJSON_IsString(tag) || !tag->valuestring || tag->valuestring[0] == 0) {
        ESP_LOGE(TAG, "release tag missing");
        cJSON_Delete(root);
        return false;
    }

    const std::string release_tag = tag->valuestring;
    *version = StripLeadingV(release_tag);
    firmware_url->clear();

    const std::string expected_asset =
        std::string(kBoardAssetPrefix) + release_tag + kAppAssetSuffix;
    cJSON* assets = cJSON_GetObjectItem(root, "assets");
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, assets) {
        cJSON* name = cJSON_GetObjectItem(item, "name");
        cJSON* download_url = cJSON_GetObjectItem(item, "browser_download_url");
        if (!cJSON_IsString(name) || !cJSON_IsString(download_url)) {
            continue;
        }
        const std::string asset_name = name->valuestring;
        if (asset_name == expected_asset ||
            (asset_name.find(kBoardAssetPrefix) != std::string::npos &&
             EndsWith(asset_name, kAppAssetSuffix))) {
            *firmware_url = download_url->valuestring;
            ESP_LOGI(TAG, "release asset selected: %s", asset_name.c_str());
            break;
        }
    }

    cJSON_Delete(root);
    return true;
}

void FirmwareUpdateService::RunUpgrade() {
    std::string firmware_url;
    bool has_update = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        firmware_url = firmware_url_;
        has_update = update_available_ && !firmware_url.empty();
    }

    if (!has_update) {
        SetUi("Check first", false, false);
        return;
    }
    if (!WifiStation::GetInstance().IsConnected()) {
        SetUi("WiFi needed", true, false);
        return;
    }

    auto& app = Application::GetInstance();
    if (app.IsExternalAudioActive()) {
        SetUi("Stop audio first", true, false);
        return;
    }
    if (app.GetDeviceState() != kDeviceStateIdle) {
        SetUi("Wait idle", true, false);
        return;
    }

    Board::GetInstance().SetPowerSaveMode(false);
    AudioCodec* codec = Board::GetInstance().GetAudioCodec();
    bool restore_input = false;
    bool restore_output = false;
    if (codec) {
        restore_input = codec->input_enabled();
        restore_output = codec->output_enabled();
        codec->EnableInput(false);
        codec->EnableOutput(false);
    }

    app.SetDeviceState(kDeviceStateUpgrading);
    SetUi("Updating...", true, true, 0);

    Ota ota;
    ota.StartUpgradeFromUrl(firmware_url, [this](int progress, size_t speed) {
        char status[32];
        snprintf(status, sizeof(status), "%d%% %uKB/s", progress,
                 static_cast<unsigned>(speed / 1024));
        SetUi(status, true, true, progress);
    });

    SetUi("Update failed", true, false);
    if (codec) {
        codec->EnableInput(restore_input);
        codec->EnableOutput(restore_output);
    }
    app.SetDeviceState(kDeviceStateIdle);
}

void FirmwareUpdateService::SetUi(const char* status, bool update_available, bool busy, int progress) {
    if (!desktop_ui_) {
        return;
    }
    if (lvgl_port_lock(200)) {
        desktop_ui_->SetFirmwareUpdateStatus(status, update_available, busy, progress);
        lvgl_port_unlock();
    }
}

static std::vector<int> ParseVersionParts(const std::string& text) {
    std::vector<int> parts;
    int value = 0;
    bool in_number = false;
    for (char ch : text) {
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            value = value * 10 + (ch - '0');
            in_number = true;
        } else {
            if (in_number) {
                parts.push_back(value);
                value = 0;
                in_number = false;
            }
            if (ch == '-' || ch == '+') {
                break;
            }
        }
    }
    if (in_number) {
        parts.push_back(value);
    }
    return parts;
}

bool FirmwareUpdateService::IsNewerVersion(const std::string& current, const std::string& latest) {
    std::vector<int> current_parts = ParseVersionParts(StripLeadingV(current));
    std::vector<int> latest_parts = ParseVersionParts(StripLeadingV(latest));
    const size_t count = std::max(current_parts.size(), latest_parts.size());
    for (size_t i = 0; i < count; ++i) {
        const int current_value = i < current_parts.size() ? current_parts[i] : 0;
        const int latest_value = i < latest_parts.size() ? latest_parts[i] : 0;
        if (latest_value > current_value) {
            return true;
        }
        if (latest_value < current_value) {
            return false;
        }
    }
    return false;
}
