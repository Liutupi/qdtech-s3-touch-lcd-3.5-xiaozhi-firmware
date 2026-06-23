#include "ota.h"
#include "system_info.h"
#include "settings.h"
#include "assets/lang_config.h"

#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <esp_efuse.h>
#include <esp_efuse_table.h>
#ifdef SOC_HMAC_SUPPORTED
#include <esp_hmac.h>
#endif

#include <cctype>
#include <cstring>
#include <memory>
#include <vector>
#include <sstream>
#include <algorithm>

#define TAG "Ota"

static constexpr int kFirmwareHttpBufferSize = 2048;
static constexpr const char* kFirmwareDownloadProxyPrefixes[] = {
    "https://ghfast.top/",
    "https://gh-proxy.com/",
};

struct FirmwareHttpHeaders {
    std::string location;
};

static std::string ToLowerHeaderKey(const char* key) {
    std::string result = key ? key : "";
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

static esp_err_t FirmwareHttpEventHandler(esp_http_client_event_t* evt) {
    if (evt->event_id == HTTP_EVENT_ON_HEADER && evt->user_data && evt->header_key && evt->header_value) {
        auto* headers = static_cast<FirmwareHttpHeaders*>(evt->user_data);
        if (ToLowerHeaderKey(evt->header_key) == "location") {
            headers->location = evt->header_value;
        }
    }
    return ESP_OK;
}

static esp_http_client_handle_t OpenFirmwareHttp(const std::string& firmware_url,
                                                 int* status_code,
                                                 int* content_length) {
    std::string current_url = firmware_url;
    for (int redirect = 0; redirect < 5; ++redirect) {
        FirmwareHttpHeaders headers;
        esp_http_client_config_t config = {};
        config.url = current_url.c_str();
        config.crt_bundle_attach = esp_crt_bundle_attach;
        config.timeout_ms = 30000;
        config.event_handler = FirmwareHttpEventHandler;
        config.user_data = &headers;
        config.buffer_size = kFirmwareHttpBufferSize;
        config.buffer_size_tx = kFirmwareHttpBufferSize;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "Failed to initialize firmware HTTP client");
            return nullptr;
        }

        auto app_desc = esp_app_get_description();
        std::string user_agent = std::string(BOARD_NAME "/") + app_desc->version;
        esp_http_client_set_method(client, HTTP_METHOD_GET);
        esp_http_client_set_header(client, "User-Agent", user_agent.c_str());

        esp_err_t err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open firmware HTTP connection: %s", esp_err_to_name(err));
            esp_http_client_cleanup(client);
            return nullptr;
        }

        const int fetched_length = esp_http_client_fetch_headers(client);
        const int fetched_status = esp_http_client_get_status_code(client);
        if (fetched_status == 301 || fetched_status == 302 || fetched_status == 303 ||
            fetched_status == 307 || fetched_status == 308) {
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            if (headers.location.empty()) {
                ESP_LOGE(TAG, "Firmware redirect missing Location header");
                return nullptr;
            }
            ESP_LOGI(TAG, "Firmware redirect %d location_len=%u",
                     fetched_status, static_cast<unsigned>(headers.location.size()));
            current_url = headers.location;
            continue;
        }

        *status_code = fetched_status;
        *content_length = fetched_length;
        return client;
    }

    ESP_LOGE(TAG, "Too many firmware download redirects");
    return nullptr;
}

static bool StartsWith(const std::string& text, const char* prefix) {
    const size_t prefix_len = strlen(prefix);
    return text.size() >= prefix_len && text.compare(0, prefix_len, prefix) == 0;
}

static std::vector<std::string> BuildFirmwareUrlCandidates(const std::string& firmware_url) {
    std::vector<std::string> urls;
    urls.push_back(firmware_url);

    if (!StartsWith(firmware_url, "https://github.com/")) {
        return urls;
    }

    for (const char* proxy_prefix : kFirmwareDownloadProxyPrefixes) {
        urls.push_back(std::string(proxy_prefix) + firmware_url);
    }
    return urls;
}

Ota::Ota() {
#ifdef ESP_EFUSE_BLOCK_USR_DATA
    // Read Serial Number from efuse user_data
    uint8_t serial_number[33] = {0};
    if (esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA, serial_number, 32 * 8) == ESP_OK) {
        if (serial_number[0] == 0) {
            has_serial_number_ = false;
        } else {
            serial_number_ = std::string(reinterpret_cast<char*>(serial_number), 32);
            has_serial_number_ = true;
        }
    }
#endif
}

Ota::~Ota() {
}

std::string Ota::GetCheckVersionUrl() {
    Settings settings("wifi", false);
    std::string url = settings.GetString("ota_url");
    if (url.empty()) {
        url = CONFIG_OTA_URL;
    }
    return url;
}

Http* Ota::SetupHttp() {
    auto& board = Board::GetInstance();
    auto app_desc = esp_app_get_description();

    auto http = board.CreateHttp();
    http->SetHeader("Activation-Version", has_serial_number_ ? "2" : "1");
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", board.GetUuid());
    if (has_serial_number_) {
        http->SetHeader("Serial-Number", serial_number_.c_str());
    }
    http->SetHeader("User-Agent", std::string(BOARD_NAME "/") + app_desc->version);
    http->SetHeader("Accept-Language", Lang::CODE);
    http->SetHeader("Content-Type", "application/json");

    return http;
}

/* 
 * Specification: https://ccnphfhqs21z.feishu.cn/wiki/FjW6wZmisimNBBkov6OcmfvknVd
 */
bool Ota::CheckVersion() {
    auto& board = Board::GetInstance();
    auto app_desc = esp_app_get_description();

    // Check if there is a new firmware version available
    current_version_ = app_desc->version;
    ESP_LOGI(TAG, "Current version: %s", current_version_.c_str());

    std::string url = GetCheckVersionUrl();
    if (url.length() < 10) {
        ESP_LOGE(TAG, "Check version URL is not properly set");
        return false;
    }

    auto http = std::unique_ptr<Http>(SetupHttp());

    std::string data = board.GetJson();
    std::string method = data.length() > 0 ? "POST" : "GET";
    http->SetContent(std::move(data));

    if (!http->Open(method, url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        return false;
    }

    auto status_code = http->GetStatusCode();
    if (status_code != 200) {
        ESP_LOGE(TAG, "Failed to check version, status code: %d", status_code);
        return false;
    }

    data = http->ReadAll();
    http->Close();

    // Response: { "firmware": { "version": "1.0.0", "url": "http://" } }
    // Parse the JSON response and check if the version is newer
    // If it is, set has_new_version_ to true and store the new version and URL
    
    cJSON *root = cJSON_Parse(data.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return false;
    }

    has_activation_code_ = false;
    has_activation_challenge_ = false;
    cJSON *activation = cJSON_GetObjectItem(root, "activation");
    if (cJSON_IsObject(activation)) {
        cJSON* message = cJSON_GetObjectItem(activation, "message");
        if (cJSON_IsString(message)) {
            activation_message_ = message->valuestring;
        }
        cJSON* code = cJSON_GetObjectItem(activation, "code");
        if (cJSON_IsString(code)) {
            activation_code_ = code->valuestring;
            has_activation_code_ = true;
        }
        cJSON* challenge = cJSON_GetObjectItem(activation, "challenge");
        if (cJSON_IsString(challenge)) {
            activation_challenge_ = challenge->valuestring;
            has_activation_challenge_ = true;
        }
        cJSON* timeout_ms = cJSON_GetObjectItem(activation, "timeout_ms");
        if (cJSON_IsNumber(timeout_ms)) {
            activation_timeout_ms_ = timeout_ms->valueint;
        }
    }

    has_mqtt_config_ = false;
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (cJSON_IsObject(mqtt)) {
        Settings settings("mqtt", true);
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, mqtt) {
            if (cJSON_IsString(item)) {
                if (settings.GetString(item->string) != item->valuestring) {
                    settings.SetString(item->string, item->valuestring);
                }
            } else if (cJSON_IsNumber(item)) {
                if (settings.GetInt(item->string) != item->valueint) {
                    settings.SetInt(item->string, item->valueint);
                }
            }
        }
        has_mqtt_config_ = true;
    } else {
        ESP_LOGI(TAG, "No mqtt section found !");
    }

    has_websocket_config_ = false;
    cJSON *websocket = cJSON_GetObjectItem(root, "websocket");
    if (cJSON_IsObject(websocket)) {
        Settings settings("websocket", true);
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, websocket) {
            if (cJSON_IsString(item)) {
                if (settings.GetString(item->string) != item->valuestring) {
                    settings.SetString(item->string, item->valuestring);
                }
            } else if (cJSON_IsNumber(item)) {
                if (settings.GetInt(item->string) != item->valueint) {
                    settings.SetInt(item->string, item->valueint);
                }
            }
        }
        has_websocket_config_ = true;
    } else {
        ESP_LOGI(TAG, "No websocket section found!");
    }

    has_server_time_ = false;
    cJSON *server_time = cJSON_GetObjectItem(root, "server_time");
    if (cJSON_IsObject(server_time)) {
        cJSON *timestamp = cJSON_GetObjectItem(server_time, "timestamp");
        if (cJSON_IsNumber(timestamp)) {
            // 设置系统时间
            struct timeval tv;
            double ts = timestamp->valuedouble;
            
            tv.tv_sec = (time_t)(ts / 1000);  // 转换毫秒为秒
            tv.tv_usec = (suseconds_t)((long long)ts % 1000) * 1000;  // 剩余的毫秒转换为微秒
            settimeofday(&tv, NULL);
            has_server_time_ = true;
        }
    } else {
        ESP_LOGW(TAG, "No server_time section found!");
    }

    has_new_version_ = false;
    cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
    if (cJSON_IsObject(firmware)) {
        cJSON *version = cJSON_GetObjectItem(firmware, "version");
        if (cJSON_IsString(version)) {
            firmware_version_ = version->valuestring;
        }
        cJSON *url = cJSON_GetObjectItem(firmware, "url");
        if (cJSON_IsString(url)) {
            firmware_url_ = url->valuestring;
        }

        if (cJSON_IsString(version) && cJSON_IsString(url)) {
            // Check if the version is newer, for example, 0.1.0 is newer than 0.0.1
            has_new_version_ = IsNewVersionAvailable(current_version_, firmware_version_);
            if (has_new_version_) {
                ESP_LOGI(TAG, "New version available: %s", firmware_version_.c_str());
            } else {
                ESP_LOGI(TAG, "Current is the latest version");
            }
            // If the force flag is set to 1, the given version is forced to be installed
            cJSON *force = cJSON_GetObjectItem(firmware, "force");
            if (cJSON_IsNumber(force) && force->valueint == 1) {
                has_new_version_ = true;
            }
        }
    } else {
        ESP_LOGW(TAG, "No firmware section found!");
    }

    cJSON_Delete(root);
    return true;
}

void Ota::MarkCurrentVersionValid() {
    auto partition = esp_ota_get_running_partition();
    if (strcmp(partition->label, "factory") == 0) {
        ESP_LOGI(TAG, "Running from factory partition, skipping");
        return;
    }

    ESP_LOGI(TAG, "Running partition: %s", partition->label);
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(partition, &state) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get state of partition");
        return;
    }

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "Marking firmware as valid");
        esp_ota_mark_app_valid_cancel_rollback();
    }
}

void Ota::Upgrade(const std::string& firmware_url) {
    ESP_LOGI(TAG, "Upgrading firmware from %s", firmware_url.c_str());
    esp_ota_handle_t update_handle = 0;
    auto update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to get update partition");
        return;
    }

    ESP_LOGI(TAG, "Writing to partition %s at offset 0x%lx", update_partition->label, update_partition->address);
    bool image_header_checked = false;
    constexpr size_t kRequiredHeaderSize =
        sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t);
    auto header_free = [](uint8_t* ptr) {
        if (ptr) {
            heap_caps_free(ptr);
        }
    };
    std::unique_ptr<uint8_t, decltype(header_free)> image_header(
        static_cast<uint8_t*>(heap_caps_malloc(kRequiredHeaderSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        header_free);
    if (!image_header) {
        ESP_LOGE(TAG, "No internal memory for OTA image header");
        return;
    }
    size_t image_header_size = 0;

    int status_code = 0;
    int content_length = 0;
    esp_http_client_handle_t client = nullptr;
    const auto firmware_urls = BuildFirmwareUrlCandidates(firmware_url);
    for (size_t i = 0; i < firmware_urls.size(); ++i) {
        status_code = 0;
        content_length = 0;
        ESP_LOGI(TAG, "Firmware download attempt %u/%u: %s",
                 static_cast<unsigned>(i + 1),
                 static_cast<unsigned>(firmware_urls.size()),
                 i == 0 ? "direct" : "proxy");
        client = OpenFirmwareHttp(firmware_urls[i], &status_code, &content_length);
        if (!client) {
            continue;
        }
        if (status_code == 200) {
            if (i > 0) {
                ESP_LOGI(TAG, "Firmware download proxy selected");
            }
            break;
        }

        ESP_LOGW(TAG, "Firmware URL returned status %d, trying next candidate", status_code);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        client = nullptr;
    }
    if (!client) {
        ESP_LOGE(TAG, "All firmware download candidates failed");
        return;
    }

    if (status_code != 200) {
        ESP_LOGE(TAG, "Failed to get firmware, status code: %d", status_code);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return;
    }

    if (content_length <= 0) {
        ESP_LOGW(TAG, "Firmware content length unavailable, continuing with byte-count progress");
    } else if (content_length > static_cast<int>(update_partition->size)) {
        ESP_LOGE(TAG, "Firmware too large: %d bytes > partition %lu bytes",
                 content_length, update_partition->size);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return;
    }

    auto buffer_free = [](char* ptr) {
        if (ptr) {
            heap_caps_free(ptr);
        }
    };
    std::unique_ptr<char, decltype(buffer_free)> buffer(
        static_cast<char*>(heap_caps_malloc(kFirmwareHttpBufferSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        buffer_free);
    if (!buffer) {
        ESP_LOGE(TAG, "No internal memory for OTA download buffer");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return;
    }

    bool ota_begun = false;
    size_t total_read = 0, recent_read = 0;
    auto last_calc_time = esp_timer_get_time();
    while (true) {
        int ret = esp_http_client_read(client, buffer.get(), kFirmwareHttpBufferSize);
        if (ret < 0) {
            ESP_LOGE(TAG, "Failed to read HTTP data: %s", esp_err_to_name(ret));
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return;
        }

        // Calculate speed and progress every second
        recent_read += ret;
        total_read += ret;
        if (esp_timer_get_time() - last_calc_time >= 1000000 || ret == 0) {
            size_t progress = content_length > 0 ? total_read * 100 / content_length : 0;
            if (content_length > 0) {
                ESP_LOGI(TAG, "Progress: %u%% (%u/%u), Speed: %uB/s",
                         progress, total_read, content_length, recent_read);
            } else {
                ESP_LOGI(TAG, "Progress: %u bytes, Speed: %uB/s", total_read, recent_read);
            }
            if (upgrade_callback_) {
                upgrade_callback_(progress, recent_read);
            }
            last_calc_time = esp_timer_get_time();
            recent_read = 0;
        }

        if (ret == 0) {
            break;
        }

        if (!image_header_checked) {
            const size_t copy_len = std::min(
                static_cast<size_t>(ret),
                kRequiredHeaderSize - image_header_size);
            if (copy_len > 0) {
                memcpy(image_header.get() + image_header_size, buffer.get(), copy_len);
                image_header_size += copy_len;
            }
            if (image_header_size < kRequiredHeaderSize) {
                continue;
            }

            esp_app_desc_t new_app_info;
            memcpy(&new_app_info,
                   image_header.get() + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t),
                   sizeof(esp_app_desc_t));
            ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

            auto current_version = esp_app_get_description()->version;
            if (memcmp(new_app_info.version, current_version, sizeof(new_app_info.version)) == 0) {
                ESP_LOGE(TAG, "Firmware version is the same, skipping upgrade");
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return;
            }

            esp_err_t begin_err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
            if (begin_err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to begin OTA: %s", esp_err_to_name(begin_err));
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return;
            }

            image_header_checked = true;
            ota_begun = true;
            auto err = esp_ota_write(update_handle, image_header.get(), image_header_size);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write OTA header data: %s", esp_err_to_name(err));
                esp_ota_abort(update_handle);
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return;
            }
            image_header.reset();
            const size_t remaining_len = static_cast<size_t>(ret) - copy_len;
            if (remaining_len > 0) {
                err = esp_ota_write(update_handle, buffer.get() + copy_len, remaining_len);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to write OTA buffered data: %s", esp_err_to_name(err));
                    esp_ota_abort(update_handle);
                    esp_http_client_close(client);
                    esp_http_client_cleanup(client);
                    return;
                }
            }
            continue;
        }

        if (!ota_begun) {
            ESP_LOGE(TAG, "OTA data arrived before OTA begin");
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return;
        }

        auto err = esp_ota_write(update_handle, buffer.get(), ret);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write OTA data: %s", esp_err_to_name(err));
            esp_ota_abort(update_handle);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return;
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (!ota_begun) {
        ESP_LOGE(TAG, "Firmware download ended before image header was received");
        return;
    }

    esp_err_t err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "Failed to end OTA: %s", esp_err_to_name(err));
        }
        return;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Firmware upgrade successful, rebooting in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}

void Ota::StartUpgrade(std::function<void(int progress, size_t speed)> callback) {
    upgrade_callback_ = callback;
    Upgrade(firmware_url_);
}

void Ota::StartUpgradeFromUrl(const std::string& firmware_url,
                              std::function<void(int progress, size_t speed)> callback) {
    upgrade_callback_ = callback;
    Upgrade(firmware_url);
}

std::vector<int> Ota::ParseVersion(const std::string& version) {
    std::vector<int> versionNumbers;
    std::stringstream ss(version);
    std::string segment;
    
    while (std::getline(ss, segment, '.')) {
        versionNumbers.push_back(std::stoi(segment));
    }
    
    return versionNumbers;
}

bool Ota::IsNewVersionAvailable(const std::string& currentVersion, const std::string& newVersion) {
    std::vector<int> current = ParseVersion(currentVersion);
    std::vector<int> newer = ParseVersion(newVersion);
    
    for (size_t i = 0; i < std::min(current.size(), newer.size()); ++i) {
        if (newer[i] > current[i]) {
            return true;
        } else if (newer[i] < current[i]) {
            return false;
        }
    }
    
    return newer.size() > current.size();
}

std::string Ota::GetActivationPayload() {
    if (!has_serial_number_) {
        return "{}";
    }

    std::string hmac_hex;
#ifdef SOC_HMAC_SUPPORTED
    uint8_t hmac_result[32]; // SHA-256 输出为32字节
    
    // 使用Key0计算HMAC
    esp_err_t ret = esp_hmac_calculate(HMAC_KEY0, (uint8_t*)activation_challenge_.data(), activation_challenge_.size(), hmac_result);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HMAC calculation failed: %s", esp_err_to_name(ret));
        return "{}";
    }

    for (size_t i = 0; i < sizeof(hmac_result); i++) {
        char buffer[3];
        sprintf(buffer, "%02x", hmac_result[i]);
        hmac_hex += buffer;
    }
#endif

    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "algorithm", "hmac-sha256");
    cJSON_AddStringToObject(payload, "serial_number", serial_number_.c_str());
    cJSON_AddStringToObject(payload, "challenge", activation_challenge_.c_str());
    cJSON_AddStringToObject(payload, "hmac", hmac_hex.c_str());
    auto json_str = cJSON_PrintUnformatted(payload);
    std::string json(json_str);
    cJSON_free(json_str);
    cJSON_Delete(payload);

    ESP_LOGI(TAG, "Activation payload: %s", json.c_str());
    return json;
}

esp_err_t Ota::Activate() {
    if (!has_activation_challenge_) {
        ESP_LOGW(TAG, "No activation challenge found");
        return ESP_FAIL;
    }

    std::string url = GetCheckVersionUrl();
    if (url.back() != '/') {
        url += "/activate";
    } else {
        url += "activate";
    }

    auto http = std::unique_ptr<Http>(SetupHttp());

    std::string data = GetActivationPayload();
    http->SetContent(std::move(data));

    if (!http->Open("POST", url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        return ESP_FAIL;
    }
    
    auto status_code = http->GetStatusCode();
    if (status_code == 202) {
        return ESP_ERR_TIMEOUT;
    }
    if (status_code != 200) {
        ESP_LOGE(TAG, "Failed to activate, code: %d, body: %s", status_code, http->ReadAll().c_str());
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Activation successful");
    return ESP_OK;
}
