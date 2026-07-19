#include "wifi_configuration_ap.h"
#include <cstdio>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_heap_caps.h>
#include <lwip/ip_addr.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <cJSON.h>
#include <esp_smartconfig.h>
#include <cstring>
#include "ssid_manager.h"

#define TAG "WifiConfigurationAp"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static constexpr size_t kMaxSavedWifiCount = 5;
#ifdef QDTECH_PROVISIONING_STA_BEACON
static constexpr uint64_t kRawBeaconIntervalUs = 100 * 1000;
// The normal AP interface reports successful raw-beacon TX completion on this
// board without producing an externally visible frame. Submit the identical
// frame through the already running Station interface to isolate that queue.
static constexpr wifi_interface_t kRawBeaconTxInterface = WIFI_IF_STA;
#endif

extern const char index_html_start[] asm("_binary_wifi_configuration_html_start");
extern const char done_html_start[] asm("_binary_wifi_configuration_done_html_start");

WifiConfigurationAp& WifiConfigurationAp::GetInstance() {
    static WifiConfigurationAp instance;
    return instance;
}

WifiConfigurationAp::WifiConfigurationAp()
{
    event_group_ = xEventGroupCreate();
    language_ = "zh-CN";
}

WifiConfigurationAp::~WifiConfigurationAp()
{
#ifdef QDTECH_PROVISIONING_STA_BEACON
    StopRawBeaconFallback();
#endif
    if (scan_timer_) {
        esp_timer_stop(scan_timer_);
        esp_timer_delete(scan_timer_);
    }
    if (event_group_) {
        vEventGroupDelete(event_group_);
    }
    // Unregister event handlers if they were registered
    if (instance_any_id_) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id_);
    }
    if (instance_got_ip_) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip_);
    }
}

void WifiConfigurationAp::SetLanguage(const std::string &&language)
{
    language_ = language;
}

void WifiConfigurationAp::SetSsidPrefix(const std::string &&ssid_prefix)
{
    ssid_prefix_ = ssid_prefix;
}

void WifiConfigurationAp::Start()
{
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &WifiConfigurationAp::WifiEventHandler,
                                                        this,
                                                        &instance_any_id_));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &WifiConfigurationAp::IpEventHandler,
                                                        this,
                                                        &instance_got_ip_));

    StartAccessPoint();
    StartWebServer();
}

#ifdef QDTECH_PROVISIONING_APSTA
void WifiConfigurationAp::StartWithExistingWifi(esp_netif_t* sta_netif)
{
    ESP_ERROR_CHECK(sta_netif != nullptr ? ESP_OK : ESP_ERR_INVALID_ARG);
    sta_netif_ = sta_netif;
    reuse_wifi_driver_ = true;
    ESP_LOGI(TAG, "Starting provisioning with existing WiFi driver sta_netif=%p nvs=unchanged",
             sta_netif_);
    Start();
}
#endif

std::string WifiConfigurationAp::GetSsid()
{
    // Get MAC and use it to generate a unique SSID
    uint8_t mac[6];
#if CONFIG_IDF_TARGET_ESP32P4
    esp_wifi_get_mac(WIFI_IF_AP, mac);
#else
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
#endif
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "%s-%02X%02X", ssid_prefix_.c_str(), mac[4], mac[5]);
    return std::string(ssid);
}

std::string WifiConfigurationAp::GetWebServerUrl()
{
    // http://192.168.4.1
    return "http://192.168.4.1";
}

void WifiConfigurationAp::StartAccessPoint()
{
    // Initialize the TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create the default event loop
    ap_netif_ = esp_netif_create_default_wifi_ap();

    // Set the router IP address to 192.168.4.1
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(ap_netif_);
    esp_netif_set_ip_info(ap_netif_, &ip_info);
    esp_netif_dhcps_start(ap_netif_);
    // Start the DNS server
    dns_server_.Start(ip_info.gw);

    // Initialize the WiFi stack in Access Point mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
#ifdef QDTECH_PROVISIONING_STATELESS_RF
    // Provisioning sets every AP parameter below, so do not load retained
    // driver state that may prevent a valid beacon after repeated upgrades.
    cfg.nvs_enable = false;
#endif
#ifdef QDTECH_PROVISIONING_APSTA
    if (!reuse_wifi_driver_) {
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    }
#else
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
#endif

    // Get the SSID
    std::string ssid = GetSsid();

    // Set the WiFi configuration
    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.ap.ssid, ssid.c_str());
    wifi_config.ap.ssid_len = ssid.length();
#ifdef QDTECH_PROVISIONING_CHANNEL_6
    wifi_config.ap.channel = 6;
#else
    wifi_config.ap.channel = 1;
#endif
    wifi_config.ap.max_connection = 2;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
#ifdef QDTECH_PROVISIONING_STATELESS_RF
    wifi_config.ap.ssid_hidden = 0;
    wifi_config.ap.beacon_interval = 100;
#endif

    // QDTech production provisioning keeps the already running Station driver
    // and adds the AP on the same instance; other boards retain pure AP mode.
#ifdef QDTECH_PROVISIONING_APSTA
    ESP_ERROR_CHECK(esp_wifi_set_mode(
        reuse_wifi_driver_ ? WIFI_MODE_APSTA : WIFI_MODE_AP));
#else
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
#endif
#ifdef QDTECH_PROVISIONING_STATELESS_RF
    wifi_country_t country = {};
    country.cc[0] = 'C';
    country.cc[1] = 'N';
    country.schan = 1;
    country.nchan = 13;
    country.policy = WIFI_COUNTRY_POLICY_MANUAL;
    ESP_ERROR_CHECK(esp_wifi_set_country(&country));
#endif
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
#ifdef QDTECH_PROVISIONING_STATELESS_RF
    ESP_ERROR_CHECK(esp_wifi_set_protocol(
        WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20));
#endif
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
#ifdef QDTECH_PROVISIONING_APSTA
    if (!reuse_wifi_driver_) {
        ESP_ERROR_CHECK(esp_wifi_start());
    }
#else
    ESP_ERROR_CHECK(esp_wifi_start());
#endif
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(78));

#ifdef CONFIG_SOC_WIFI_SUPPORT_5G
    // Temporarily use only 2.4G Wi-Fi.
    ESP_ERROR_CHECK(esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY));
#endif

    ESP_LOGI(TAG, "Access Point started with SSID %s", ssid.c_str());

#ifdef QDTECH_PROVISIONING_STA_BEACON
    StartRawBeaconFallback(ssid, wifi_config.ap.channel);
#endif

    // 加载高级配置
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("wifi", NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        // 读取OTA URL
        char ota_url[256] = {0};
        size_t ota_url_size = sizeof(ota_url);
        err = nvs_get_str(nvs, "ota_url", ota_url, &ota_url_size);
        if (err == ESP_OK) {
            ota_url_ = ota_url;
        }

        // 读取WiFi功率
        err = nvs_get_i8(nvs, "max_tx_power", &max_tx_power_);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "WiFi max tx power from NVS: %d", max_tx_power_);
            ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(max_tx_power_));
        } else {
            esp_wifi_get_max_tx_power(&max_tx_power_);
        }

        // 读取BSSID记忆设置
        uint8_t remember_bssid = 0;
        err = nvs_get_u8(nvs, "remember_bssid", &remember_bssid);
        if (err == ESP_OK) {
            remember_bssid_ = remember_bssid;
        } else {
            remember_bssid_ = true; // 默认值
        }

        nvs_close(nvs);
    }

#ifdef QDTECH_PROVISIONING_STATELESS_RF
    wifi_mode_t actual_mode = WIFI_MODE_NULL;
    wifi_config_t actual_config = {};
    uint8_t actual_primary = 0;
    wifi_second_chan_t actual_secondary = WIFI_SECOND_CHAN_NONE;
    uint8_t actual_protocol = 0;
    wifi_bandwidth_t actual_bandwidth = WIFI_BW_HT20;
    wifi_country_t actual_country = {};
    int8_t actual_tx_power = 0;
    ESP_ERROR_CHECK(esp_wifi_get_mode(&actual_mode));
    ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_AP, &actual_config));
    ESP_ERROR_CHECK(esp_wifi_get_channel(&actual_primary, &actual_secondary));
    ESP_ERROR_CHECK(esp_wifi_get_protocol(WIFI_IF_AP, &actual_protocol));
    ESP_ERROR_CHECK(esp_wifi_get_bandwidth(WIFI_IF_AP, &actual_bandwidth));
    ESP_ERROR_CHECK(esp_wifi_get_country(&actual_country));
    ESP_ERROR_CHECK(esp_wifi_get_max_tx_power(&actual_tx_power));
#ifdef QDTECH_PROVISIONING_APSTA
    ESP_LOGI(TAG,
             "SoftAP RF mode=%d channel=%u secondary=%d protocol=0x%02x bandwidth=%d "
             "country=%c%c start=%u count=%u tx_power=%d hidden=%u beacon=%u "
             "nvs=off reused_driver=%d",
             actual_mode, actual_primary, actual_secondary, actual_protocol,
             actual_bandwidth, actual_country.cc[0], actual_country.cc[1],
             actual_country.schan, actual_country.nchan, actual_tx_power,
             actual_config.ap.ssid_hidden, actual_config.ap.beacon_interval,
             reuse_wifi_driver_);
#else
    ESP_LOGI(TAG,
             "SoftAP RF mode=%d channel=%u secondary=%d protocol=0x%02x bandwidth=%d "
             "country=%c%c start=%u count=%u tx_power=%d hidden=%u beacon=%u nvs=off",
             actual_mode, actual_primary, actual_secondary, actual_protocol,
             actual_bandwidth, actual_country.cc[0], actual_country.cc[1],
             actual_country.schan, actual_country.nchan, actual_tx_power,
             actual_config.ap.ssid_hidden, actual_config.ap.beacon_interval);
#endif
#endif
}

#ifdef QDTECH_PROVISIONING_STA_BEACON
void WifiConfigurationAp::StartRawBeaconFallback(const std::string& ssid,
                                                  uint8_t channel)
{
    if (raw_beacon_timer_ != nullptr) {
        ESP_LOGW(TAG, "Raw beacon fallback already running");
        return;
    }
    if (ssid.empty() || ssid.size() > 32 || channel == 0) {
        ESP_LOGE(TAG, "Raw beacon fallback invalid AP identity ssid_len=%u channel=%u",
                 static_cast<unsigned>(ssid.size()), channel);
        return;
    }

    uint8_t ap_mac[6] = {};
    const esp_err_t mac_result = esp_wifi_get_mac(WIFI_IF_AP, ap_mac);
    if (mac_result != ESP_OK) {
        ESP_LOGE(TAG, "Raw beacon fallback cannot read AP MAC: %s",
                 esp_err_to_name(mac_result));
        return;
    }

    raw_beacon_frame_.clear();
    raw_beacon_frame_.reserve(96);
    const auto append = [this](std::initializer_list<uint8_t> bytes) {
        raw_beacon_frame_.insert(raw_beacon_frame_.end(), bytes.begin(), bytes.end());
    };

    // IEEE 802.11 beacon MAC header. The driver supplies the sequence number
    // because esp_wifi_80211_tx() is called with en_sys_seq=true.
    append({0x80, 0x00, 0x00, 0x00});
    append({0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
    raw_beacon_frame_.insert(raw_beacon_frame_.end(), ap_mac, ap_mac + sizeof(ap_mac));
    raw_beacon_frame_.insert(raw_beacon_frame_.end(), ap_mac, ap_mac + sizeof(ap_mac));
    append({0x00, 0x00});

    // Timestamp is refreshed before every transmission. Interval is 100 TU;
    // capability bits advertise an open ESS with short preamble/slot support.
    append({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    append({0x64, 0x00, 0x21, 0x04});

    raw_beacon_frame_.push_back(0x00);  // SSID element
    raw_beacon_frame_.push_back(static_cast<uint8_t>(ssid.size()));
    raw_beacon_frame_.insert(raw_beacon_frame_.end(), ssid.begin(), ssid.end());
    append({0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24});
    append({0x03, 0x01, channel});
    append({0x05, 0x04, 0x00, 0x01, 0x00, 0x00});
    append({0x07, 0x06, 'C', 'N', ' ', 0x01, 0x0d, 0x14});
    append({0x32, 0x04, 0x30, 0x48, 0x60, 0x6c});

    raw_beacon_tx_error_logged_ = false;

    const esp_timer_create_args_t timer_args = {
        .callback = &RawBeaconTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "qd_raw_beacon",
        .skip_unhandled_events = true,
    };
    esp_err_t result = esp_timer_create(&timer_args, &raw_beacon_timer_);
    if (result == ESP_OK) {
        result = esp_timer_start_periodic(raw_beacon_timer_, kRawBeaconIntervalUs);
    }
    if (result != ESP_OK) {
        if (raw_beacon_timer_ != nullptr) {
            esp_timer_delete(raw_beacon_timer_);
            raw_beacon_timer_ = nullptr;
        }
        ESP_LOGE(TAG, "Raw beacon fallback timer start failed: %s",
                 esp_err_to_name(result));
        return;
    }

    ESP_LOGW(TAG,
             "Raw beacon fallback started ssid=%s bssid=%02x:%02x:%02x:%02x:%02x:%02x "
             "channel=%u interval_ms=100 frame_len=%u tx_if=%d",
             ssid.c_str(), ap_mac[0], ap_mac[1], ap_mac[2], ap_mac[3], ap_mac[4],
             ap_mac[5], channel, static_cast<unsigned>(raw_beacon_frame_.size()),
             kRawBeaconTxInterface);
}

void WifiConfigurationAp::StopRawBeaconFallback()
{
    if (raw_beacon_timer_ != nullptr) {
        esp_timer_stop(raw_beacon_timer_);
        esp_timer_delete(raw_beacon_timer_);
        raw_beacon_timer_ = nullptr;
    }
    raw_beacon_frame_.clear();
    raw_beacon_tx_error_logged_ = false;
}

void WifiConfigurationAp::RawBeaconTimerCallback(void* arg)
{
    auto* owner = static_cast<WifiConfigurationAp*>(arg);
    if (owner == nullptr || owner->raw_beacon_frame_.size() < 36) {
        return;
    }

    const uint64_t timestamp = static_cast<uint64_t>(esp_timer_get_time());
    for (size_t i = 0; i < sizeof(timestamp); ++i) {
        owner->raw_beacon_frame_[24 + i] =
            static_cast<uint8_t>((timestamp >> (8 * i)) & 0xff);
    }

    const esp_err_t result = esp_wifi_80211_tx(
        kRawBeaconTxInterface, owner->raw_beacon_frame_.data(),
        static_cast<int>(owner->raw_beacon_frame_.size()), true);
    if (result != ESP_OK && !owner->raw_beacon_tx_error_logged_) {
        owner->raw_beacon_tx_error_logged_ = true;
        ESP_LOGE(TAG, "Raw beacon fallback TX failed: %s",
                 esp_err_to_name(result));
    }
}
#endif

void WifiConfigurationAp::StartWebServer()
{
    // Start the web server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 24;
    config.uri_match_fn = httpd_uri_match_wildcard;
    ESP_ERROR_CHECK(httpd_start(&server_, &config));

    // Register the index.html file
    httpd_uri_t index_html = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, index_html_start, strlen(index_html_start));
            return ESP_OK;
        },
        .user_ctx = NULL
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &index_html));

    // Register the /saved/list URI
    httpd_uri_t saved_list = {
        .uri = "/saved/list",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            auto ssid_list = SsidManager::GetInstance().GetSsidList();
            std::string json_str = "[";
            for (const auto& ssid : ssid_list) {
                json_str += "\"" + ssid.ssid + "\",";
            }
            if (json_str.length() > 1) {
                json_str.pop_back(); // Remove the last comma
            }
            json_str += "]";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, json_str.c_str(), HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        },
        .user_ctx = NULL
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &saved_list));

    // Register the /saved/set_default URI
    httpd_uri_t saved_set_default = {
        .uri = "/saved/set_default",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            std::string uri = req->uri;
            auto pos = uri.find("?index=");
            if (pos != std::string::npos) {
                int index = std::stoi(uri.substr(pos + 7));
                ESP_LOGI(TAG, "Set default item %d", index);
                SsidManager::GetInstance().SetDefaultSsid(index);
            }
            // send {}
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        },
        .user_ctx = NULL
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &saved_set_default));

    // Register the /saved/delete URI
    httpd_uri_t saved_delete = {
        .uri = "/saved/delete",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            std::string uri = req->uri;
            auto pos = uri.find("?index=");
            if (pos != std::string::npos) {
                int index = std::stoi(uri.substr(pos + 7));
                ESP_LOGI(TAG, "Delete saved list item %d", index);
                SsidManager::GetInstance().RemoveSsid(index);
            }
            // send {}
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        },
        .user_ctx = NULL
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &saved_delete));

    // Register the /scan URI
    httpd_uri_t scan = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            auto *this_ = static_cast<WifiConfigurationAp *>(req->user_ctx);
            if (!this_->is_connecting_) {
                wifi_scan_config_t scan_config = {};
                scan_config.show_hidden = true;
                ESP_LOGI(TAG, "manual scan requested");
                if (this_->sta_netif_ == nullptr) {
                    this_->sta_netif_ = esp_netif_create_default_wifi_sta();
                    if (this_->sta_netif_ == nullptr) {
                        ESP_LOGW(TAG, "create STA netif for scan failed");
                    }
                }
                esp_err_t mode_err = esp_wifi_set_mode(WIFI_MODE_APSTA);
                if (mode_err != ESP_OK) {
                    ESP_LOGW(TAG, "switch APSTA for scan failed: %s", esp_err_to_name(mode_err));
                } else {
                    esp_err_t scan_err = esp_wifi_scan_start(&scan_config, true);
                    if (scan_err == ESP_OK) {
                        std::lock_guard<std::mutex> lock(this_->mutex_);
                        ESP_LOGI(TAG, "manual scan done ap_num=%u",
                                 static_cast<unsigned>(this_->ap_records_.size()));
                    } else {
                        ESP_LOGW(TAG, "manual scan failed: %s", esp_err_to_name(scan_err));
                    }
                }
            }

            // Send the scan results as JSON
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_sendstr_chunk(req, "[");
            std::lock_guard<std::mutex> lock(this_->mutex_);
            for (int i = 0; i < this_->ap_records_.size(); i++) {
                ESP_LOGI(TAG, "SSID: %s, RSSI: %d, Authmode: %d",
                    (char *)this_->ap_records_[i].ssid, this_->ap_records_[i].rssi, this_->ap_records_[i].authmode);
                char buf[128];
                snprintf(buf, sizeof(buf), "{\"ssid\":\"%s\",\"rssi\":%d,\"authmode\":%d}",
                    (char *)this_->ap_records_[i].ssid, this_->ap_records_[i].rssi, this_->ap_records_[i].authmode);
                httpd_resp_sendstr_chunk(req, buf);
                if (i < this_->ap_records_.size() - 1) {
                    httpd_resp_sendstr_chunk(req, ",");
                }
            }
            httpd_resp_sendstr_chunk(req, "]");
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_OK;
        },
        .user_ctx = this
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &scan));

    // Register the form submission
    httpd_uri_t form_submit = {
        .uri = "/submit",
        .method = HTTP_POST,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            char *buf;
            size_t buf_len = req->content_len;
            if (buf_len > 1024) { // 限制最大请求体大小
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
                return ESP_FAIL;
            }

            buf = (char *)malloc(buf_len + 1);
            if (!buf) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate memory");
                return ESP_FAIL;
            }

            int ret = httpd_req_recv(req, buf, buf_len);
            if (ret <= 0) {
                free(buf);
                if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                    httpd_resp_send_408(req);
                } else {
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive request");
                }
                return ESP_FAIL;
            }
            buf[ret] = '\0';

            // 解析 JSON 数据
            cJSON *json = cJSON_Parse(buf);
            free(buf);
            if (!json) {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
                return ESP_FAIL;
            }

            cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(json, "ssid");
            cJSON *password_item = cJSON_GetObjectItemCaseSensitive(json, "password");

            if (!cJSON_IsString(ssid_item) || (ssid_item->valuestring == NULL)) {
                cJSON_Delete(json);
                httpd_resp_send(req, "{\"success\":false,\"error\":\"无效的 SSID\"}", HTTPD_RESP_USE_STRLEN);
                return ESP_OK;
            }

            std::string ssid_str = ssid_item->valuestring;
            std::string password_str = "";
            if (cJSON_IsString(password_item) && (password_item->valuestring != NULL)) {
                password_str = password_item->valuestring;
            }

            // 获取当前对象
            auto *this_ = static_cast<WifiConfigurationAp *>(req->user_ctx);
            if (!this_->ConnectToWifi(ssid_str, password_str)) {
                cJSON_Delete(json);
                httpd_resp_send(req, "{\"success\":false,\"error\":\"无法连接到 WiFi\"}", HTTPD_RESP_USE_STRLEN);
                return ESP_OK;
            }

            this_->Save(ssid_str, password_str);
            cJSON_Delete(json);
            // 设置成功响应
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        },
        .user_ctx = this
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &form_submit));

    // Register the done.html page
    httpd_uri_t done_html = {
        .uri = "/done.html",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, done_html_start, strlen(done_html_start));
            return ESP_OK;
        },
        .user_ctx = NULL
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &done_html));

    // Register the reboot endpoint
    httpd_uri_t reboot = {
        .uri = "/reboot",
        .method = HTTP_POST,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            auto* this_ = static_cast<WifiConfigurationAp*>(req->user_ctx);

            // 设置响应头，防止浏览器缓存
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Cache-Control", "no-store");
            httpd_resp_set_hdr(req, "Connection", "close");
            // 发送响应
            httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);

            // 创建一个延迟重启任务
            ESP_LOGI(TAG, "Rebooting...");
            xTaskCreate([](void *ctx) {
                // 等待200ms确保HTTP响应完全发送
                vTaskDelay(pdMS_TO_TICKS(200));
                // 停止Web服务器
                auto* self = static_cast<WifiConfigurationAp*>(ctx);
                if (self->server_) {
                    httpd_stop(self->server_);
                }
                // 再等待100ms确保所有连接都已关闭
                vTaskDelay(pdMS_TO_TICKS(100));
                // 执行重启
                esp_restart();
            }, "reboot_task", 4096, this_, 5, NULL);

            return ESP_OK;
        },
        .user_ctx = this
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &reboot));

    auto captive_portal_handler = [](httpd_req_t *req) -> esp_err_t {
        auto *this_ = static_cast<WifiConfigurationAp *>(req->user_ctx);
        std::string url = this_->GetWebServerUrl() + "/?lang=" + this_->language_;
        // Set content type to prevent browser warnings
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", url.c_str());
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    };

    // Register all common captive portal detection endpoints
    const char* captive_portal_urls[] = {
        "/hotspot-detect.html",    // Apple
        "/generate_204",           // Android
        "/mobile/status.php",      // Android
        "/check_network_status.txt", // Windows
        "/ncsi.txt",              // Windows
        "/fwlink/",               // Microsoft
        "/connectivity-check.html", // Firefox
        "/success.txt",           // Various
        "/portal.html",           // Various
        "/library/test/success.html" // Apple
    };

    for (const auto& url : captive_portal_urls) {
        httpd_uri_t redirect_uri = {
            .uri = url,
            .method = HTTP_GET,
            .handler = captive_portal_handler,
            .user_ctx = this
        };
        ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &redirect_uri));
    }

    // Register the /advanced/config URI
    httpd_uri_t advanced_config = {
        .uri = "/advanced/config",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            // 获取当前对象
            auto *this_ = static_cast<WifiConfigurationAp *>(req->user_ctx);

            // 创建JSON对象
            cJSON *json = cJSON_CreateObject();
            if (!json) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create JSON");
                return ESP_FAIL;
            }

            // 添加配置项到JSON
            if (!this_->ota_url_.empty()) {
                cJSON_AddStringToObject(json, "ota_url", this_->ota_url_.c_str());
            }
            cJSON_AddNumberToObject(json, "max_tx_power", this_->max_tx_power_);
            cJSON_AddBoolToObject(json, "remember_bssid", this_->remember_bssid_);

            // 发送JSON响应
            char *json_str = cJSON_PrintUnformatted(json);
            cJSON_Delete(json);
            if (!json_str) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to print JSON");
                return ESP_FAIL;
            }

            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, json_str, strlen(json_str));
            free(json_str);
            return ESP_OK;
        },
        .user_ctx = this
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &advanced_config));

    // Register the /advanced/submit URI
    httpd_uri_t advanced_submit = {
        .uri = "/advanced/submit",
        .method = HTTP_POST,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            char *buf;
            size_t buf_len = req->content_len;
            if (buf_len > 1024) {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
                return ESP_FAIL;
            }

            buf = (char *)malloc(buf_len + 1);
            if (!buf) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate memory");
                return ESP_FAIL;
            }

            int ret = httpd_req_recv(req, buf, buf_len);
            if (ret <= 0) {
                free(buf);
                if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                    httpd_resp_send_408(req);
                } else {
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive request");
                }
                return ESP_FAIL;
            }
            buf[ret] = '\0';

            // 解析JSON数据
            cJSON *json = cJSON_Parse(buf);
            free(buf);
            if (!json) {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
                return ESP_FAIL;
            }

            // 获取当前对象
            auto *this_ = static_cast<WifiConfigurationAp *>(req->user_ctx);

            // 打开NVS
            nvs_handle_t nvs;
            esp_err_t err = nvs_open("wifi", NVS_READWRITE, &nvs);
            if (err != ESP_OK) {
                cJSON_Delete(json);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open NVS");
                return ESP_FAIL;
            }

            // 保存OTA URL
            cJSON *ota_url = cJSON_GetObjectItem(json, "ota_url");
            if (cJSON_IsString(ota_url) && ota_url->valuestring) {
                this_->ota_url_ = ota_url->valuestring;
                err = nvs_set_str(nvs, "ota_url", this_->ota_url_.c_str());
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to save OTA URL: %d", err);
                }
            }

            // 保存WiFi功率
            cJSON *max_tx_power = cJSON_GetObjectItem(json, "max_tx_power");
            if (cJSON_IsNumber(max_tx_power)) {
                this_->max_tx_power_ = max_tx_power->valueint;
                err = esp_wifi_set_max_tx_power(this_->max_tx_power_);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to set WiFi power: %d", err);
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to set WiFi power");
                    return ESP_FAIL;
                }
                err = nvs_set_i8(nvs, "max_tx_power", this_->max_tx_power_);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to save WiFi power: %d", err);
                }
            }

            // 保存BSSID记忆设置
            cJSON *remember_bssid = cJSON_GetObjectItem(json, "remember_bssid");
            if (cJSON_IsBool(remember_bssid)) {
                this_->remember_bssid_ = cJSON_IsTrue(remember_bssid);
                err = nvs_set_u8(nvs, "remember_bssid", this_->remember_bssid_);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to save remember_bssid: %d", err);
                }
            }

            // 提交更改
            err = nvs_commit(nvs);
            nvs_close(nvs);
            cJSON_Delete(json);

            if (err != ESP_OK) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save configuration");
                return ESP_FAIL;
            }

            // 发送成功响应
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        },
        .user_ctx = this
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &advanced_submit));

    ESP_LOGI(TAG, "Web server started");
}

bool WifiConfigurationAp::ConnectToWifi(const std::string &ssid, const std::string &password)
{
    if (ssid.empty()) {
        ESP_LOGE(TAG, "SSID cannot be empty");
        return false;
    }

    if (ssid.length() > 32) {  // WiFi SSID 最大长度
        ESP_LOGE(TAG, "SSID too long");
        return false;
    }

    is_connecting_ = true;
    esp_wifi_scan_stop();
    xEventGroupClearBits(event_group_, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    if (sta_netif_ == nullptr) {
        sta_netif_ = esp_netif_create_default_wifi_sta();
        if (sta_netif_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create STA netif");
            is_connecting_ = false;
            return false;
        }
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t wifi_config;
    bzero(&wifi_config, sizeof(wifi_config));
    strcpy((char *)wifi_config.sta.ssid, ssid.c_str());
    strcpy((char *)wifi_config.sta.password, password.c_str());
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.failure_retry_cnt = 1;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    auto ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to WiFi: %d", ret);
        is_connecting_ = false;
        return false;
    }
    ESP_LOGI(TAG, "Connecting to WiFi %s", ssid.c_str());

    // Wait for the connection to complete for 5 seconds
    EventBits_t bits = xEventGroupWaitBits(event_group_, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    is_connecting_ = false;

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi %s", ssid.c_str());
        esp_wifi_disconnect();
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to connect to WiFi %s", ssid.c_str());
        return false;
    }
}

void WifiConfigurationAp::Save(const std::string &ssid, const std::string &password)
{
    ESP_LOGI(TAG, "Save SSID %s %d", ssid.c_str(), ssid.length());
    auto& ssid_manager = SsidManager::GetInstance();
    ssid_manager.AddSsid(ssid, password);
    auto ssid_list = ssid_manager.GetSsidList();
    while (ssid_list.size() > kMaxSavedWifiCount) {
        ESP_LOGW(TAG, "Saved WiFi list exceeds %u, removing oldest entry %s",
                 static_cast<unsigned>(kMaxSavedWifiCount),
                 ssid_list.back().ssid.c_str());
        ssid_manager.RemoveSsid(static_cast<int>(ssid_list.size() - 1));
        ssid_list = ssid_manager.GetSsidList();
    }
}

void WifiConfigurationAp::WifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    WifiConfigurationAp* self = static_cast<WifiConfigurationAp*>(arg);
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        xEventGroupSetBits(self->event_group_, WIFI_CONNECTED_BIT);
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        auto* event = static_cast<wifi_event_sta_disconnected_t*>(event_data);
        ESP_LOGW(TAG, "STA disconnected during setup reason=%d", event ? event->reason : -1);
        xEventGroupSetBits(self->event_group_, WIFI_FAIL_BIT);
    } else if (event_id == WIFI_EVENT_SCAN_DONE) {
        std::lock_guard<std::mutex> lock(self->mutex_);
        uint16_t ap_num = 0;
        esp_wifi_scan_get_ap_num(&ap_num);

        self->ap_records_.resize(ap_num);
        esp_wifi_scan_get_ap_records(&ap_num, self->ap_records_.data());

        ESP_LOGI(TAG, "scan done ap_num=%u free_internal=%u largest_internal=%u",
                 ap_num,
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
        if (self->scan_timer_) {
            esp_timer_start_once(self->scan_timer_, 30 * 1000000);
        }
    }
}

void WifiConfigurationAp::IpEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    WifiConfigurationAp* self = static_cast<WifiConfigurationAp*>(arg);
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(self->event_group_, WIFI_CONNECTED_BIT);
    }
}

void WifiConfigurationAp::StartSmartConfig()
{
    // 注册SmartConfig事件处理器
    ESP_ERROR_CHECK(esp_event_handler_instance_register(SC_EVENT, ESP_EVENT_ANY_ID,
                                                        &WifiConfigurationAp::SmartConfigEventHandler, this, &sc_event_instance_));

    // 初始化SmartConfig配置
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    // cfg.esp_touch_v2_enable_crypt = true;
    // cfg.esp_touch_v2_key = "1234567890123456"; // 设置16字节加密密钥

    // 启动SmartConfig服务
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    ESP_LOGI(TAG, "SmartConfig started");
}

void WifiConfigurationAp::SmartConfigEventHandler(void *arg, esp_event_base_t event_base,
                                                  int32_t event_id, void *event_data)
{
    WifiConfigurationAp *self = static_cast<WifiConfigurationAp *>(arg);

    if (event_base == SC_EVENT){
        switch (event_id){
        case SC_EVENT_SCAN_DONE:
            ESP_LOGI(TAG, "SmartConfig scan done");
            break;
        case SC_EVENT_FOUND_CHANNEL:
            ESP_LOGI(TAG, "Found SmartConfig channel");
            break;
        case SC_EVENT_GOT_SSID_PSWD:{
            ESP_LOGI(TAG, "Got SmartConfig credentials");
            smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;

            char ssid[32], password[64];
            memcpy(ssid, evt->ssid, sizeof(evt->ssid));
            memcpy(password, evt->password, sizeof(evt->password));
            ESP_LOGI(TAG, "SmartConfig SSID: %s, Password: %s", ssid, password);
            // 尝试连接WiFi会失败，故不连接
            self->Save(ssid, password);
            xTaskCreate([](void *ctx){
                ESP_LOGI(TAG, "Restarting in 3 second");
                vTaskDelay(pdMS_TO_TICKS(3000));
                esp_restart();
            }, "restart_task", 4096, NULL, 5, NULL);
            break;
        }
        case SC_EVENT_SEND_ACK_DONE:
            ESP_LOGI(TAG, "SmartConfig ACK sent");
            esp_smartconfig_stop();
            break;
        }
    }
}

void WifiConfigurationAp::Stop() {
    // 停止SmartConfig服务
    if (sc_event_instance_) {
        esp_event_handler_instance_unregister(SC_EVENT, ESP_EVENT_ANY_ID, sc_event_instance_);
        sc_event_instance_ = nullptr;
    }
    esp_smartconfig_stop();

    // 停止定时器
#ifdef QDTECH_PROVISIONING_STA_BEACON
    StopRawBeaconFallback();
#endif
    if (scan_timer_) {
        esp_timer_stop(scan_timer_);
        esp_timer_delete(scan_timer_);
        scan_timer_ = nullptr;
    }

    // 停止Web服务器
    if (server_) {
        httpd_stop(server_);
        server_ = nullptr;
    }

    // 停止DNS服务器
    dns_server_.Stop();

    // 注销事件处理器
    if (instance_any_id_) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id_);
        instance_any_id_ = nullptr;
    }
    if (instance_got_ip_) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip_);
        instance_got_ip_ = nullptr;
    }

    // 停止WiFi并重置模式
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_wifi_set_mode(WIFI_MODE_NULL);

    // 释放网络接口资源
    if (ap_netif_) {
        esp_netif_destroy(ap_netif_);
        ap_netif_ = nullptr;
    }
    if (sta_netif_) {
        esp_netif_destroy(sta_netif_);
        sta_netif_ = nullptr;
    }

    ESP_LOGI(TAG, "Wifi configuration AP stopped");
}
