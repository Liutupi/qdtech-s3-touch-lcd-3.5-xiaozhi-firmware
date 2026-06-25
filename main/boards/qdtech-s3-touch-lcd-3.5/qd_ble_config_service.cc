#include "qd_ble_config_service.h"

#include "qd_user_config.h"

#include <cstring>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "sdkconfig.h"

#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#else
// Fallback definitions when BLE is not enabled
#define BLE_ATT_ERR_UNLIKELY 0x0e
#endif

static const char* TAG = "QdBleConfig";

QdBleConfigService* QdBleConfigService::instance_ = nullptr;

#if CONFIG_BT_NIMBLE_ENABLED
static const ble_uuid128_t kConfigServiceUuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x30, 0x70, 0x6d);
static const ble_uuid128_t kConfigCharUuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x30, 0x70, 0x6d);
static constexpr size_t kMinBleInternalFree = 48 * 1024;
static constexpr size_t kMinBleInternalLargest = 16 * 1024;

static const ble_gatt_svc_def kGattServices[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &kConfigServiceUuid.u,
        .characteristics = (ble_gatt_chr_def[]) {
            {
                .uuid = &kConfigCharUuid.u,
                .access_cb = QdBleConfigService::ConfigAccess,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {0},
        },
    },
    {0},
};
#endif

void QdBleConfigService::Start(DesktopUI* ui, TimeWeatherService* weather_service) {
    ui_ = ui;
    weather_service_ = weather_service;
#if CONFIG_BT_NIMBLE_ENABLED
    if (started_) {
        return;
    }
    instance_ = this;
    const size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t largest_internal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "ble memory check free_internal=%u largest_internal=%u",
             static_cast<unsigned>(free_internal), static_cast<unsigned>(largest_internal));
    if (free_internal < kMinBleInternalFree || largest_internal < kMinBleInternalLargest) {
        ESP_LOGW(TAG, "skip BLE init, not enough internal memory");
        SetStatus("BLE low memory");
        return;
    }
    int rc = nimble_port_init();
    if (rc != 0) {
        ESP_LOGW(TAG, "nimble init failed rc=%d", rc);
        SetStatus("BLE init failed");
        return;
    }

    ble_hs_cfg.sync_cb = OnSync;
    ble_hs_cfg.reset_cb = OnReset;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("QDTech-Config");
    ble_gatts_count_cfg(kGattServices);
    ble_gatts_add_svcs(kGattServices);
    nimble_port_freertos_init(HostTask);
    started_ = true;
    SetStatus("BLE ready");
#else
    SetStatus("BLE disabled");
#endif
}

bool QdBleConfigService::ApplyConfig(const char* data, size_t len) {
    const QdConfigUpdate update = QdApplyConfigJson(data, len);
    if (!update.ok) {
        SetStatus(update.error.c_str());
        return false;
    }
    if (update.weather_changed && weather_service_) {
        if (!weather_service_->SetLocation(update.weather.city, update.weather.latitude, update.weather.longitude)) {
            SetStatus("Bad weather");
            return false;
        }
    }

    if (update.profile_changed && ui_ && lvgl_port_lock(100)) {
        ui_->ReloadUserProfile();
        lvgl_port_unlock();
    } else if (ui_ && lvgl_port_lock(100)) {
        ui_->RefreshSettingsControls();
        lvgl_port_unlock();
    }

    SetStatus("Synced");
    ESP_LOGI(TAG, "config synced profile=%d weather=%d", update.profile_changed, update.weather_changed);
    return true;
}

void QdBleConfigService::SetStatus(const char* status) {
    status_ = status ? status : "BLE idle";
    if (ui_ && lvgl_port_lock(50)) {
        ui_->SetBluetoothConfigStatus(status_.c_str());
        lvgl_port_unlock();
    }
}

#if CONFIG_BT_NIMBLE_ENABLED
void QdBleConfigService::Advertise() {
    ble_gap_adv_params adv_params = {};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = reinterpret_cast<const uint8_t*>("QDTech-Config");
    fields.name_len = std::strlen("QDTech-Config");
    fields.name_is_complete = 1;
    fields.uuids128 = &kConfigServiceUuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    const int rc = ble_gap_adv_start(own_addr_type_, nullptr, BLE_HS_FOREVER,
                                     &adv_params, GapEvent, this);
    if (rc == 0) {
        SetStatus("BLE advertising");
    } else {
        ESP_LOGW(TAG, "adv start failed rc=%d", rc);
        SetStatus("BLE adv failed");
    }
}

void QdBleConfigService::HostTask(void* param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void QdBleConfigService::OnSync() {
    if (!instance_) {
        return;
    }
    int rc = ble_hs_id_infer_auto(0, &instance_->own_addr_type_);
    if (rc != 0) {
        ESP_LOGW(TAG, "addr infer failed rc=%d", rc);
        instance_->SetStatus("BLE addr failed");
        return;
    }
    instance_->Advertise();
}

void QdBleConfigService::OnReset(int reason) {
    ESP_LOGW(TAG, "reset reason=%d", reason);
    if (instance_) {
        instance_->SetStatus("BLE reset");
    }
}

int QdBleConfigService::GapEvent(ble_gap_event* event, void* arg) {
    auto* service = static_cast<QdBleConfigService*>(arg);
    if (!service) {
        return 0;
    }
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        service->SetStatus(event->connect.status == 0 ? "Phone connected" : "BLE connect failed");
        if (event->connect.status != 0) {
            service->Advertise();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        service->SetStatus("Phone disconnected");
        service->Advertise();
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        service->Advertise();
        return 0;
    default:
        return 0;
    }
}

int QdBleConfigService::ConfigAccess(uint16_t conn_handle, uint16_t attr_handle,
                                     ble_gatt_access_ctxt* ctxt, void* arg) {
    if (!instance_) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        const std::string json = QdBuildConfigJson();
        return os_mbuf_append(ctxt->om, json.data(), json.size()) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        char buf[256] = {};
        uint16_t len = 0;
        const int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf) - 1, &len);
        if (rc != 0) {
            instance_->SetStatus("BLE write too long");
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        return instance_->ApplyConfig(buf, len) ? 0 : BLE_ATT_ERR_UNLIKELY;
    }
    return BLE_ATT_ERR_UNLIKELY;
}
#else
void QdBleConfigService::Advertise() {}
void QdBleConfigService::HostTask(void* param) {}
void QdBleConfigService::OnSync() {}
void QdBleConfigService::OnReset(int reason) {}
int QdBleConfigService::GapEvent(ble_gap_event* event, void* arg) { return 0; }
int QdBleConfigService::ConfigAccess(uint16_t conn_handle, uint16_t attr_handle,
                                     ble_gatt_access_ctxt* ctxt, void* arg) {
    return BLE_ATT_ERR_UNLIKELY;
}
#endif
