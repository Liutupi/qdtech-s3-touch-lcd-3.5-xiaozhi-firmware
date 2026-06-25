#pragma once

#include "desktop_ui.h"
#include "time_weather_service.h"

#include <string>

#include "esp_http_server.h"

class QdWifiConfigServer {
public:
    void Start(DesktopUI* ui, TimeWeatherService* weather_service);
    bool IsRunning() const { return server_ != nullptr; }
    std::string Status() const { return status_; }

private:
    DesktopUI* ui_ = nullptr;
    TimeWeatherService* weather_service_ = nullptr;
    httpd_handle_t server_ = nullptr;
    std::string status_ = "WiFi config idle";

    void SetStatus(const char* status);
    std::string GetConfigUrl() const;
    bool ApplyConfig(const char* data, size_t len, std::string* error);
    bool ScheduleApply(const char* data, size_t len);

    static esp_err_t RootHandler(httpd_req_t* req);
    static esp_err_t ConfigGetHandler(httpd_req_t* req);
    static esp_err_t ConfigPostHandler(httpd_req_t* req);
};
