#ifndef _WIFI_CONFIGURATION_AP_H_
#define _WIFI_CONFIGURATION_AP_H_

#include "sdkconfig.h"
#include "qdtech_provisioning_compat.h"

#include <string>
#include <vector>
#include <mutex>

#include <esp_http_server.h>
#include <esp_event.h>
#include <esp_timer.h>
#include <esp_netif.h>
#include <esp_wifi_types_generic.h>

#include "dns_server.h"

class WifiConfigurationAp {
public:
    static WifiConfigurationAp& GetInstance();
    void SetSsidPrefix(const std::string &&ssid_prefix);
    void SetLanguage(const std::string &&language);
    void Start();
#ifdef QDTECH_PROVISIONING_APSTA
    void StartWithExistingWifi(esp_netif_t* sta_netif);
#endif
    void Stop();
    void StartSmartConfig();
    bool ConnectToWifi(const std::string &ssid, const std::string &password);
    void Save(const std::string &ssid, const std::string &password);

    std::string GetSsid();
    std::string GetWebServerUrl();

    // Delete copy constructor and assignment operator
    WifiConfigurationAp(const WifiConfigurationAp&) = delete;
    WifiConfigurationAp& operator=(const WifiConfigurationAp&) = delete;

private:
    // Private constructor
    WifiConfigurationAp();
    ~WifiConfigurationAp();

    std::mutex mutex_;
    DnsServer dns_server_;
    httpd_handle_t server_ = NULL;
    EventGroupHandle_t event_group_;
    std::string ssid_prefix_;
    std::string language_;
    esp_event_handler_instance_t instance_any_id_;
    esp_event_handler_instance_t instance_got_ip_;
    esp_timer_handle_t scan_timer_ = nullptr;
    bool is_connecting_ = false;
    esp_netif_t* ap_netif_ = nullptr;
    std::vector<wifi_ap_record_t> ap_records_;

    // 高级配置项
    std::string ota_url_;
    int8_t max_tx_power_;
    bool remember_bssid_;
    esp_netif_t* sta_netif_ = nullptr;
#ifdef QDTECH_PROVISIONING_APSTA
    bool reuse_wifi_driver_ = false;
#endif
#ifdef QDTECH_PROVISIONING_STA_BEACON
    esp_timer_handle_t raw_beacon_timer_ = nullptr;
    std::vector<uint8_t> raw_beacon_frame_;
    bool raw_beacon_tx_error_logged_ = false;
#endif

    void StartAccessPoint();
    void StartWebServer();
#ifdef QDTECH_PROVISIONING_STA_BEACON
    void StartRawBeaconFallback(const std::string& ssid, uint8_t channel);
    void StopRawBeaconFallback();
    static void RawBeaconTimerCallback(void* arg);
#endif

    // Event handlers
    static void WifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void IpEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void SmartConfigEventHandler(void* arg, esp_event_base_t event_base,
                                      int32_t event_id, void* event_data);
    esp_event_handler_instance_t sc_event_instance_ = nullptr;
};

#endif // _WIFI_CONFIGURATION_AP_H_
