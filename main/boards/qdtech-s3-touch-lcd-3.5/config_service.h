#pragma once

#include <string>
#include <functional>
#include "settings.h"

class ConfigService {
public:
    static ConfigService& GetInstance() {
        static ConfigService instance;
        return instance;
    }

    // Logo 文本（例如 "nothing impossible"）
    std::string GetLogoText() const {
        Settings settings("ui_cfg", false);
        return settings.GetString("logo_text", "nothing impossible");
    }

    void SetLogoText(const std::string& text) {
        Settings settings("ui_cfg", true);
        settings.SetString("logo_text", text);
    }

    // 主人名字（例如 "tupi"）
    std::string GetOwnerName() const {
        Settings settings("ui_cfg", false);
        return settings.GetString("owner_name", "tupi");
    }

    void SetOwnerName(const std::string& name) {
        Settings settings("ui_cfg", true);
        settings.SetString("owner_name", name);
    }

    // 天气城市
    std::string GetWeatherCity() const {
        Settings settings("weather_cfg", false);
        return settings.GetString("city", "Zhongshan");
    }

    void SetWeatherCity(const std::string& city) {
        Settings settings("weather_cfg", true);
        settings.SetString("city", city);
    }

    // 天气纬度
    std::string GetWeatherLat() const {
        Settings settings("weather_cfg", false);
        return settings.GetString("lat", "22.5176");
    }

    void SetWeatherLat(const std::string& lat) {
        Settings settings("weather_cfg", true);
        settings.SetString("lat", lat);
    }

    // 天气经度
    std::string GetWeatherLon() const {
        Settings settings("weather_cfg", false);
        return settings.GetString("lon", "113.3928");
    }

    void SetWeatherLon(const std::string& lon) {
        Settings settings("weather_cfg", true);
        settings.SetString("lon", lon);
    }

private:
    ConfigService() = default;
    ~ConfigService() = default;
    ConfigService(const ConfigService&) = delete;
    ConfigService& operator=(const ConfigService&) = delete;
};
