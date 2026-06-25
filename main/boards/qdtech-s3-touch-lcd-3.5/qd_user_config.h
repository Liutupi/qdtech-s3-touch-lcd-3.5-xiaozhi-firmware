#pragma once

#include <cstddef>
#include <string>

struct QdUserProfile {
    std::string logo;
    std::string owner;
};

struct QdWeatherConfig {
    std::string city;
    std::string latitude;
    std::string longitude;
};

struct QdConfigUpdate {
    bool ok = false;
    bool profile_changed = false;
    bool weather_changed = false;
    bool city_changed = false;
    bool latitude_changed = false;
    bool longitude_changed = false;
    std::string error;
    QdUserProfile profile;
    QdWeatherConfig weather;
};

QdUserProfile QdLoadUserProfile();
QdUserProfile QdGetCachedUserProfile();
void QdSaveUserProfile(const QdUserProfile& profile);
QdWeatherConfig QdLoadWeatherConfig();
QdWeatherConfig QdGetCachedWeatherConfig();
QdConfigUpdate QdApplyConfigJson(const char* data, size_t len);
std::string QdBuildConfigJson();
std::string QdBuildCachedConfigJson();
