#include "qd_user_config.h"

#include <algorithm>
#include <cctype>
#include <mutex>

#include "cJSON.h"
#include "settings.h"

namespace {
constexpr const char* kProfileNs = "profile_cfg";
constexpr const char* kWeatherNs = "weather_cfg";

std::mutex g_config_mutex;
QdUserProfile g_cached_profile{"nothing impossible", "tupi"};
QdWeatherConfig g_cached_weather{"Zhongshan", "22.5176", "113.3928"};
bool g_profile_cache_loaded = false;
bool g_weather_cache_loaded = false;

std::string sanitize_text(std::string value, size_t max_len, const char* fallback) {
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
        return ch < 0x20 || ch == 0x7f;
    }), value.end());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    if (value.empty()) {
        value = fallback;
    }
    if (value.size() > max_len) {
        value.resize(max_len);
    }
    return value;
}
}  // namespace

QdUserProfile QdLoadUserProfile() {
    Settings settings(kProfileNs, false);
    QdUserProfile profile;
    profile.logo = sanitize_text(settings.GetString("logo", "nothing impossible"), 28, "nothing impossible");
    profile.owner = sanitize_text(settings.GetString("owner", "tupi"), 18, "tupi");
    {
        std::lock_guard<std::mutex> lock(g_config_mutex);
        g_cached_profile = profile;
        g_profile_cache_loaded = true;
    }
    return profile;
}

QdUserProfile QdGetCachedUserProfile() {
    {
        std::lock_guard<std::mutex> lock(g_config_mutex);
        if (g_profile_cache_loaded) {
            return g_cached_profile;
        }
    }
    return QdLoadUserProfile();
}

void QdSaveUserProfile(const QdUserProfile& profile) {
    QdUserProfile sanitized;
    sanitized.logo = sanitize_text(profile.logo, 28, "nothing impossible");
    sanitized.owner = sanitize_text(profile.owner, 18, "tupi");
    Settings settings(kProfileNs, true);
    settings.SetString("logo", sanitized.logo);
    settings.SetString("owner", sanitized.owner);
    {
        std::lock_guard<std::mutex> lock(g_config_mutex);
        g_cached_profile = sanitized;
        g_profile_cache_loaded = true;
    }
}

QdWeatherConfig QdLoadWeatherConfig() {
    Settings settings(kWeatherNs, false);
    QdWeatherConfig config;
    config.city = sanitize_text(settings.GetString("city", "Zhongshan"), 31, "Zhongshan");
    config.latitude = sanitize_text(settings.GetString("lat", "22.5176"), 16, "22.5176");
    config.longitude = sanitize_text(settings.GetString("lon", "113.3928"), 16, "113.3928");
    {
        std::lock_guard<std::mutex> lock(g_config_mutex);
        g_cached_weather = config;
        g_weather_cache_loaded = true;
    }
    return config;
}

QdWeatherConfig QdGetCachedWeatherConfig() {
    {
        std::lock_guard<std::mutex> lock(g_config_mutex);
        if (g_weather_cache_loaded) {
            return g_cached_weather;
        }
    }
    return QdLoadWeatherConfig();
}

QdConfigUpdate QdApplyConfigJson(const char* data, size_t len) {
    QdConfigUpdate update;
    cJSON* root = cJSON_ParseWithLength(data, len);
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        update.error = "Bad JSON";
        return update;
    }

    auto profile = QdLoadUserProfile();
    auto weather = QdLoadWeatherConfig();

    if (const cJSON* logo = cJSON_GetObjectItem(root, "logo"); cJSON_IsString(logo) && logo->valuestring) {
        profile.logo = logo->valuestring;
        update.profile_changed = true;
    }
    if (const cJSON* name = cJSON_GetObjectItem(root, "name"); cJSON_IsString(name) && name->valuestring) {
        profile.owner = name->valuestring;
        update.profile_changed = true;
    }
    if (const cJSON* owner = cJSON_GetObjectItem(root, "owner"); cJSON_IsString(owner) && owner->valuestring) {
        profile.owner = owner->valuestring;
        update.profile_changed = true;
    }
    if (const cJSON* city = cJSON_GetObjectItem(root, "city"); cJSON_IsString(city) && city->valuestring) {
        weather.city = city->valuestring;
        update.weather_changed = true;
        update.city_changed = true;
    }
    if (const cJSON* latitude = cJSON_GetObjectItem(root, "latitude"); cJSON_IsString(latitude) && latitude->valuestring) {
        std::string value = latitude->valuestring;
        if (!value.empty()) {
            weather.latitude = value;
            update.weather_changed = true;
            update.latitude_changed = true;
        }
    }
    if (const cJSON* longitude = cJSON_GetObjectItem(root, "longitude"); cJSON_IsString(longitude) && longitude->valuestring) {
        std::string value = longitude->valuestring;
        if (!value.empty()) {
            weather.longitude = value;
            update.weather_changed = true;
            update.longitude_changed = true;
        }
    }
    cJSON_Delete(root);

    if (update.profile_changed) {
        QdSaveUserProfile(profile);
    }
    update.ok = true;
    update.profile = QdLoadUserProfile();
    update.weather = weather;
    return update;
}

std::string QdBuildConfigJson() {
    const auto profile = QdLoadUserProfile();
    const auto weather = QdLoadWeatherConfig();
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "logo", profile.logo.c_str());
    cJSON_AddStringToObject(root, "name", profile.owner.c_str());
    cJSON_AddStringToObject(root, "city", weather.city.c_str());
    cJSON_AddStringToObject(root, "latitude", weather.latitude.c_str());
    cJSON_AddStringToObject(root, "longitude", weather.longitude.c_str());
    char* text = cJSON_PrintUnformatted(root);
    std::string result = text ? text : "{}";
    if (text) {
        cJSON_free(text);
    }
    cJSON_Delete(root);
    return result;
}

std::string QdBuildCachedConfigJson() {
    const auto profile = QdGetCachedUserProfile();
    const auto weather = QdGetCachedWeatherConfig();
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "logo", profile.logo.c_str());
    cJSON_AddStringToObject(root, "name", profile.owner.c_str());
    cJSON_AddStringToObject(root, "city", weather.city.c_str());
    cJSON_AddStringToObject(root, "latitude", weather.latitude.c_str());
    cJSON_AddStringToObject(root, "longitude", weather.longitude.c_str());
    char* text = cJSON_PrintUnformatted(root);
    std::string result = text ? text : "{}";
    if (text) {
        cJSON_free(text);
    }
    cJSON_Delete(root);
    return result;
}
