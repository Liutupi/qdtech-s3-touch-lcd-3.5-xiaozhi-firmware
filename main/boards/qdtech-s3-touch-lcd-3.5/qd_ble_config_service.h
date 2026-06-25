#pragma once

#include "desktop_ui.h"
#include "time_weather_service.h"

#include <string>

class QdBleConfigService {
public:
    void Start(DesktopUI* ui, TimeWeatherService* weather_service);
    std::string Status() const { return status_; }

    static int ConfigAccess(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt* ctxt, void* arg);

private:
    DesktopUI* ui_ = nullptr;
    TimeWeatherService* weather_service_ = nullptr;
    std::string status_ = "BLE idle";
    bool started_ = false;
    uint8_t own_addr_type_ = 0;

    bool ApplyConfig(const char* data, size_t len);
    void SetStatus(const char* status);
    void Advertise();

    static QdBleConfigService* instance_;
    static void HostTask(void* param);
    static void OnSync();
    static void OnReset(int reason);
    static int GapEvent(struct ble_gap_event* event, void* arg);
};
