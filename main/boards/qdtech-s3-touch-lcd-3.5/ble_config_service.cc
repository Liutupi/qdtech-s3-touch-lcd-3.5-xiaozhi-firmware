#include "ble_config_service.h"

#include <cstring>
#include <string>

#include "cJSON.h"
#include "esp_log.h"

static const char* TAG = "BleConfig";

bool BleConfigService::Start() {
    if (started_) {
        ESP_LOGW(TAG, "BLE service already started");
        return true;
    }

    // BLE 服务需要额外的 ESP-IDF 蓝牙组件支持
    // 当前版本暂不启用 BLE，通过 MCP 工具和设备端 UI 进行配置
    ESP_LOGI(TAG, "BLE config service stub started (BLE not enabled in this build)");
    started_ = true;
    if (status_callback_) {
        status_callback_("Config Ready (BLE stub)");
    }
    return true;
}

void BleConfigService::Stop() {
    if (!started_) return;
    started_ = false;
    ESP_LOGI(TAG, "BLE config service stopped");
}

void BleConfigService::HandleConfigData(const char* json_data) {
    cJSON* root = cJSON_Parse(json_data);
    if (!root) {
        ESP_LOGE(TAG, "Invalid JSON data");
        return;
    }

    cJSON* logo = cJSON_GetObjectItem(root, "logo");
    if (logo && cJSON_IsString(logo)) {
        ConfigService::GetInstance().SetLogoText(logo->valuestring);
        ESP_LOGI(TAG, "Logo text updated: %s", logo->valuestring);
    }

    cJSON* name = cJSON_GetObjectItem(root, "name");
    if (name && cJSON_IsString(name)) {
        ConfigService::GetInstance().SetOwnerName(name->valuestring);
        ESP_LOGI(TAG, "Owner name updated: %s", name->valuestring);
    }

    cJSON* city = cJSON_GetObjectItem(root, "city");
    if (city && cJSON_IsString(city)) {
        ConfigService::GetInstance().SetWeatherCity(city->valuestring);
        ESP_LOGI(TAG, "Weather city updated: %s", city->valuestring);
    }

    cJSON* lat = cJSON_GetObjectItem(root, "lat");
    if (lat && cJSON_IsString(lat)) {
        ConfigService::GetInstance().SetWeatherLat(lat->valuestring);
    }

    cJSON* lon = cJSON_GetObjectItem(root, "lon");
    if (lon && cJSON_IsString(lon)) {
        ConfigService::GetInstance().SetWeatherLon(lon->valuestring);
    }

    cJSON_Delete(root);

    if (status_callback_) {
        status_callback_("Config updated");
    }
}
