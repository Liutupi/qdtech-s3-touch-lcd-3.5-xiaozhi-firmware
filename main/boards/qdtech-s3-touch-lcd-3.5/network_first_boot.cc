#include "network_first_boot.h"
#include "sdkconfig.h"

#if defined(CONFIG_QDTECH_PROVISIONING_COMPAT) || \
    defined(CONFIG_QDTECH_EXPERIMENT_NETWORK_FIRST_BOOT)

#include "settings.h"
#include "assets/lang_config.h"

#include <ssid_manager.h>
#include <wifi_configuration_ap.h>
#include <wifi_station.h>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>

namespace {

constexpr int kSavedNetworkConnectTimeoutMs = 8 * 1000;
#ifdef CONFIG_QDTECH_EXPERIMENT_WIFI_STA_TX_SELF_TEST
constexpr int kTemporaryHotspotConnectTimeoutMs = 15 * 1000;
#endif
const char* const kTag = "QdNetFirst";

void LogMemory(const char* stage) {
    const size_t free_internal =
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t largest_internal =
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t minimum_internal =
        heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(kTag,
             "%s memory internal_free=%u internal_largest=%u internal_min=%u psram_free=%u",
             stage, static_cast<unsigned>(free_internal),
             static_cast<unsigned>(largest_internal),
             static_cast<unsigned>(minimum_internal),
             static_cast<unsigned>(free_psram));
}

bool ConsumeForceApFlag() {
    Settings settings("wifi", true);
    if (settings.GetInt("force_ap", 0) != 1) {
        return false;
    }
    settings.SetInt("force_ap", 0);
    ESP_LOGI(kTag, "force_ap requested; entering minimal provisioning");
    return true;
}

void EnsureStrongestBssidDefault() {
    nvs_handle_t nvs;
    const esp_err_t open_result = nvs_open("wifi", NVS_READWRITE, &nvs);
    if (open_result != ESP_OK) {
        ESP_LOGW(kTag, "open WiFi NVS failed: %s", esp_err_to_name(open_result));
        return;
    }

    uint8_t remember_bssid = 0;
    esp_err_t result = nvs_get_u8(nvs, "remember_bssid", &remember_bssid);
    if (result == ESP_ERR_NVS_NOT_FOUND || remember_bssid != 0) {
        result = nvs_set_u8(nvs, "remember_bssid", 0);
        if (result == ESP_OK) {
            result = nvs_commit(nvs);
        }
        if (result == ESP_OK) {
            ESP_LOGI(kTag, "WiFi BSSID memory disabled before early connection");
        } else {
            ESP_LOGW(kTag, "save WiFi BSSID default failed: %s",
                     esp_err_to_name(result));
        }
    } else if (result != ESP_OK) {
        ESP_LOGW(kTag, "read WiFi BSSID default failed: %s",
                 esp_err_to_name(result));
    }
    nvs_close(nvs);
}

[[noreturn]] void StartMinimalProvisioning(
    const char* reason
#ifdef QDTECH_PROVISIONING_APSTA
    , esp_netif_t* reused_sta_netif = nullptr
#endif
) {
    ESP_LOGW(kTag, "Starting minimal provisioning: %s", reason);
    LogMemory("before_softap");

    auto& wifi_ap = WifiConfigurationAp::GetInstance();
    wifi_ap.SetLanguage(Lang::CODE);
    wifi_ap.SetSsidPrefix("Xiaozhi");
#ifdef QDTECH_PROVISIONING_APSTA
    if (reused_sta_netif == nullptr) {
        auto& wifi_station = WifiStation::GetInstance();
        wifi_station.Start();
        reused_sta_netif = wifi_station.ReleaseForApstaHandoff();
    }
    wifi_ap.StartWithExistingWifi(reused_sta_netif);
#else
    wifi_ap.Start();
#endif

    ESP_LOGI(kTag, "Minimal provisioning ready ssid=%s url=%s",
             wifi_ap.GetSsid().c_str(), wifi_ap.GetWebServerUrl().c_str());
    LogMemory("after_softap");

    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}

}  // namespace

void RunQdtechNetworkFirstBoot() {
    ESP_LOGI(kTag, "QDTech network-first boot enabled");
    LogMemory("preflight_start");

    if (ConsumeForceApFlag()) {
        StartMinimalProvisioning("force_ap");
    }

    EnsureStrongestBssidDefault();
    const auto saved_networks = SsidManager::GetInstance().GetSsidList();
    auto& wifi_station = WifiStation::GetInstance();

#ifdef CONFIG_QDTECH_EXPERIMENT_WIFI_STA_TX_SELF_TEST
    bool has_connection_candidate = !saved_networks.empty();
    int connect_timeout_ms = kSavedNetworkConnectTimeoutMs;

    const char* const temporary_ssid = CONFIG_QDTECH_WIFI_STA_TX_SELF_TEST_SSID;
    const char* const temporary_password = CONFIG_QDTECH_WIFI_STA_TX_SELF_TEST_PASSWORD;
    if (temporary_ssid[0] == '\0' || temporary_password[0] == '\0') {
        ESP_LOGE(kTag, "Temporary Station TX self-test credentials are incomplete");
    } else {
        wifi_station.SetTemporaryAuth(temporary_ssid, temporary_password);
        has_connection_candidate = true;
        connect_timeout_ms = kTemporaryHotspotConnectTimeoutMs;
        ESP_LOGI(kTag,
                 "Temporary Station TX self-test armed ssid=%s timeout=%d ms nvs=unchanged",
                 temporary_ssid, connect_timeout_ms);
    }
    if (!has_connection_candidate) {
        StartMinimalProvisioning("no_saved_network");
    }

    ESP_LOGI(kTag, "Trying %u saved WiFi network(s) and temporary candidate for %d ms before full startup",
             static_cast<unsigned>(saved_networks.size()),
             connect_timeout_ms);
#else
    if (saved_networks.empty()) {
#ifdef QDTECH_PROVISIONING_APSTA
        ESP_LOGI(kTag, "No saved network; initializing one WiFi driver for APSTA provisioning");
        wifi_station.Start();
        esp_netif_t* reused_sta_netif = wifi_station.ReleaseForApstaHandoff();
        StartMinimalProvisioning("no_saved_network_single_driver", reused_sta_netif);
#else
        StartMinimalProvisioning("no_saved_network");
#endif
    }

    ESP_LOGI(kTag, "Trying %u saved WiFi network(s) for %d ms before full startup",
             static_cast<unsigned>(saved_networks.size()),
             kSavedNetworkConnectTimeoutMs);
    constexpr int connect_timeout_ms = kSavedNetworkConnectTimeoutMs;
#endif

    wifi_station.Start();
    if (wifi_station.WaitForConnected(connect_timeout_ms)) {
#ifdef CONFIG_QDTECH_EXPERIMENT_WIFI_STA_TX_SELF_TEST
        wifi_station.ClearTemporaryAuth();
#endif
        ESP_LOGI(kTag, "Saved WiFi connected ssid=%s ip=%s; starting full application",
                 wifi_station.GetSsid().c_str(), wifi_station.GetIpAddress().c_str());
        LogMemory("station_connected");
        return;
    }

#ifdef CONFIG_QDTECH_EXPERIMENT_WIFI_STA_TX_SELF_TEST
    ESP_LOGW(kTag, "No WiFi candidate connected within %d ms", connect_timeout_ms);
    wifi_station.ClearTemporaryAuth();
#else
    ESP_LOGW(kTag, "No saved WiFi connected within %d ms", connect_timeout_ms);
#endif
#ifdef QDTECH_PROVISIONING_APSTA
    esp_netif_t* reused_sta_netif = wifi_station.ReleaseForApstaHandoff();
    StartMinimalProvisioning("saved_network_unavailable_single_driver", reused_sta_netif);
#else
    wifi_station.Stop();
    StartMinimalProvisioning("saved_network_unavailable");
#endif
}

#endif
