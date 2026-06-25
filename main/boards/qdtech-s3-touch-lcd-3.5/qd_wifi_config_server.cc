#include "qd_wifi_config_server.h"

#include "application.h"
#include "qd_user_config.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_netif.h"

static const char* TAG = "QdWifiConfig";

namespace {
static constexpr size_t kMinHttpInternalFree = 10 * 1024;
static constexpr size_t kMinHttpLargestBlock = 4 * 1024;
static constexpr size_t kMaxPostBody = 768;
static constexpr size_t kGeoResponseSize = 1536;

esp_err_t geo_http_event_handler(esp_http_client_event_t* evt) {
    if (evt->event_id != HTTP_EVENT_ON_DATA || !evt->user_data || !evt->data || evt->data_len <= 0) {
        return ESP_OK;
    }
    char* buffer = static_cast<char*>(evt->user_data);
    const size_t used = strlen(buffer);
    const size_t free_len = used < kGeoResponseSize - 1 ? (kGeoResponseSize - 1 - used) : 0;
    if (free_len == 0) {
        return ESP_OK;
    }
    const size_t copy_len = evt->data_len < static_cast<int>(free_len) ? evt->data_len : free_len;
    memcpy(buffer + used, evt->data, copy_len);
    buffer[used + copy_len] = 0;
    return ESP_OK;
}

std::string html_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += ch; break;
        }
    }
    return out;
}

int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

std::string url_decode(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+') {
            out.push_back(' ');
        } else if (value[i] == '%' && i + 2 < value.size()) {
            const int hi = hex_value(value[i + 1]);
            const int lo = hex_value(value[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
            } else {
                out.push_back(value[i]);
            }
        } else {
            out.push_back(value[i]);
        }
    }
    return out;
}

std::string url_encode(const std::string& value) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() * 3);
    for (unsigned char ch : value) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out.push_back(static_cast<char>(ch));
        } else if (ch == ' ') {
            out += "%20";
        } else {
            out.push_back('%');
            out.push_back(hex[ch >> 4]);
            out.push_back(hex[ch & 0x0f]);
        }
    }
    return out;
}

void add_form_field(cJSON* root, const std::string& key, const std::string& value) {
    if (key == "name" || key == "owner" || key == "logo" ||
        key == "city" || key == "latitude" || key == "longitude") {
        cJSON_AddStringToObject(root, key.c_str(), value.c_str());
    }
}

std::string form_to_json(const char* data, size_t len) {
    cJSON* root = cJSON_CreateObject();
    std::string body(data, len);
    size_t pos = 0;
    while (pos <= body.size()) {
        const size_t amp = body.find('&', pos);
        const size_t end = amp == std::string::npos ? body.size() : amp;
        const size_t eq = body.find('=', pos);
        if (eq != std::string::npos && eq < end) {
            add_form_field(root, url_decode(body.substr(pos, eq - pos)), url_decode(body.substr(eq + 1, end - eq - 1)));
        }
        if (amp == std::string::npos) {
            break;
        }
        pos = amp + 1;
    }

    char* text = cJSON_PrintUnformatted(root);
    std::string result = text ? text : "{}";
    if (text) {
        cJSON_free(text);
    }
    cJSON_Delete(root);
    return result;
}

std::string page_html(const char* url) {
    const auto profile = QdGetCachedUserProfile();
    const auto weather = QdGetCachedWeatherConfig();
    std::string html =
        "<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>QDTech Config</title><style>"
        "body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;margin:24px;background:#f6f0e6;color:#1c1b19}"
        "label{display:block;margin:14px 0 6px;font-weight:600}input{box-sizing:border-box;width:100%;padding:12px;border:1px solid #c9bfae;border-radius:8px;font-size:16px}"
        "button{margin-top:18px;width:100%;padding:13px;border:0;border-radius:8px;background:#1c1b19;color:#fff;font-size:16px}"
        "p{color:#6f725f;font-size:14px}.hint{font-size:13px}.box{max-width:420px;margin:auto}</style></head><body><div class=\"box\">"
        "<h2>QDTech Settings</h2><p>";
    html += html_escape(url ? url : "http://device-ip/");
    html += "</p><form method=\"post\" action=\"/config\">";
    html += "<label>Logo</label><input name=\"logo\" maxlength=\"28\" value=\"" + html_escape(profile.logo) + "\">";
    html += "<label>Name</label><input name=\"name\" maxlength=\"18\" value=\"" + html_escape(profile.owner) + "\">";
    html += "<label>Weather City</label><input name=\"city\" maxlength=\"31\" value=\"" + html_escape(weather.city) + "\">";
    html += "<p class=\"hint\">Coordinates are resolved automatically. Current: " +
            html_escape(weather.latitude) + ", " + html_escape(weather.longitude) + "</p>";
    html += "<button type=\"submit\">Save</button></form></div></body></html>";
    return html;
}

bool fetch_city_location(const std::string& city, const char* language, QdWeatherConfig* weather) {
    char response[kGeoResponseSize] = {};
    const std::string url = "http://geocoding-api.open-meteo.com/v1/search?name=" +
                            url_encode(city) + "&count=1&language=" + language + "&format=json";
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.timeout_ms = 7000;
    config.event_handler = geo_http_event_handler;
    config.user_data = response;
    config.buffer_size = 1024;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return false;
    }
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_err_t err = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK || status < 200 || status >= 300 || response[0] == 0) {
        ESP_LOGW(TAG, "city lookup failed city=%s lang=%s err=%s status=%d",
                 city.c_str(), language, esp_err_to_name(err), status);
        return false;
    }

    cJSON* root = cJSON_Parse(response);
    cJSON* results = root ? cJSON_GetObjectItem(root, "results") : nullptr;
    cJSON* first = cJSON_IsArray(results) ? cJSON_GetArrayItem(results, 0) : nullptr;
    cJSON* name = first ? cJSON_GetObjectItem(first, "name") : nullptr;
    cJSON* latitude = first ? cJSON_GetObjectItem(first, "latitude") : nullptr;
    cJSON* longitude = first ? cJSON_GetObjectItem(first, "longitude") : nullptr;
    if (!cJSON_IsNumber(latitude) || !cJSON_IsNumber(longitude)) {
        cJSON_Delete(root);
        return false;
    }

    char lat_text[20];
    char lon_text[20];
    snprintf(lat_text, sizeof(lat_text), "%.4f", latitude->valuedouble);
    snprintf(lon_text, sizeof(lon_text), "%.4f", longitude->valuedouble);
    weather->city = cJSON_IsString(name) && name->valuestring ? name->valuestring : city;
    weather->latitude = lat_text;
    weather->longitude = lon_text;
    cJSON_Delete(root);
    ESP_LOGI(TAG, "city resolved %s lang=%s -> %s %s,%s",
             city.c_str(), language, weather->city.c_str(), lat_text, lon_text);
    return true;
}

bool resolve_city_location(const std::string& city, QdWeatherConfig* weather, std::string* error) {
    if (city.empty() || !weather) {
        if (error) {
            *error = "City required";
        }
        return false;
    }

    if (fetch_city_location(city, "zh", weather) ||
        fetch_city_location(city, "en", weather)) {
        return true;
    }

    if (error) {
        *error = "City not found";
    }
    return false;
}
}  // namespace

void QdWifiConfigServer::Start(DesktopUI* ui, TimeWeatherService* weather_service) {
    ui_ = ui;
    weather_service_ = weather_service;
    if (server_) {
        return;
    }

    QdLoadUserProfile();
    QdLoadWeatherConfig();

    const size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t largest_internal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "http memory check free_internal=%u largest_internal=%u",
             static_cast<unsigned>(free_internal), static_cast<unsigned>(largest_internal));
    if (free_internal < kMinHttpInternalFree || largest_internal < kMinHttpLargestBlock) {
        SetStatus("WiFi config low memory");
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 2;
    config.max_uri_handlers = 3;
    config.lru_purge_enable = true;
    config.stack_size = 4096;
    config.task_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    esp_err_t err = httpd_start(&server_, &config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "http server start failed err=%s", esp_err_to_name(err));
        SetStatus("WiFi config failed");
        return;
    }

    httpd_uri_t root = {};
    root.uri = "/";
    root.method = HTTP_GET;
    root.handler = RootHandler;
    root.user_ctx = this;
    httpd_register_uri_handler(server_, &root);

    httpd_uri_t config_get = {};
    config_get.uri = "/config";
    config_get.method = HTTP_GET;
    config_get.handler = ConfigGetHandler;
    config_get.user_ctx = this;
    httpd_register_uri_handler(server_, &config_get);

    httpd_uri_t config_post = {};
    config_post.uri = "/config";
    config_post.method = HTTP_POST;
    config_post.handler = ConfigPostHandler;
    config_post.user_ctx = this;
    httpd_register_uri_handler(server_, &config_post);

    SetStatus(GetConfigUrl().c_str());
}

void QdWifiConfigServer::SetStatus(const char* status) {
    status_ = status ? status : "WiFi config idle";
    if (ui_ && lvgl_port_lock(50)) {
        ui_->SetWifiConfigStatus(status_.c_str());
        lvgl_port_unlock();
    }
}

std::string QdWifiConfigServer::GetConfigUrl() const {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info = {};
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
        char text[40];
        snprintf(text, sizeof(text), "http://" IPSTR "/", IP2STR(&ip_info.ip));
        return text;
    }
    return "http://device-ip/";
}

bool QdWifiConfigServer::ApplyConfig(const char* data, size_t len, std::string* error) {
    std::string json;
    const char* config_data = data;
    size_t config_len = len;
    size_t first = 0;
    while (first < len && std::isspace(static_cast<unsigned char>(data[first]))) {
        ++first;
    }
    if (first >= len || data[first] != '{') {
        json = form_to_json(data, len);
        config_data = json.data();
        config_len = json.size();
    }

    const QdConfigUpdate update = QdApplyConfigJson(config_data, config_len);
    if (!update.ok) {
        if (error) {
            *error = update.error;
        }
        return false;
    }
    QdWeatherConfig weather = update.weather;
    if (update.city_changed && !(update.latitude_changed && update.longitude_changed)) {
        if (!resolve_city_location(update.weather.city, &weather, error)) {
            return false;
        }
    }
    if (update.weather_changed && weather_service_) {
        if (!weather_service_->SetLocation(weather.city, weather.latitude, weather.longitude)) {
            if (error) {
                *error = "Bad weather location";
            }
            return false;
        }
        QdLoadWeatherConfig();
    }
    if (ui_ && lvgl_port_lock(100)) {
        ui_->RefreshSettingsControls();
        lvgl_port_unlock();
    }
    ESP_LOGI(TAG, "wifi config synced profile=%d weather=%d", update.profile_changed, update.weather_changed);
    SetStatus("WiFi config synced");
    return true;
}

bool QdWifiConfigServer::ScheduleApply(const char* data, size_t len) {
    std::string payload(data, len);
    auto task = [this, payload = std::move(payload)]() {
        std::string error;
        if (!ApplyConfig(payload.data(), payload.size(), &error)) {
            ESP_LOGW(TAG, "wifi config apply failed: %s", error.c_str());
            SetStatus(error.empty() ? "WiFi config failed" : error.c_str());
        }
    };
    if (auto* background_task = Application::GetInstance().GetBackgroundTask()) {
        background_task->Schedule(std::move(task));
    } else {
        Application::GetInstance().Schedule(std::move(task));
    }
    SetStatus("WiFi config saving");
    return true;
}

esp_err_t QdWifiConfigServer::RootHandler(httpd_req_t* req) {
    auto* service = static_cast<QdWifiConfigServer*>(req->user_ctx);
    const std::string html = page_html(service ? service->GetConfigUrl().c_str() : nullptr);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html.c_str(), html.size());
}

esp_err_t QdWifiConfigServer::ConfigGetHandler(httpd_req_t* req) {
    const std::string json = QdBuildCachedConfigJson();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

esp_err_t QdWifiConfigServer::ConfigPostHandler(httpd_req_t* req) {
    auto* service = static_cast<QdWifiConfigServer*>(req->user_ctx);
    if (!service) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No service");
    }
    if (req->content_len <= 0 || req->content_len > kMaxPostBody) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad size");
    }

    char body[kMaxPostBody + 1] = {};
    int received = 0;
    while (received < req->content_len) {
        const int ret = httpd_req_recv(req, body + received, req->content_len - received);
        if (ret <= 0) {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Read failed");
        }
        received += ret;
    }

    if (!service->ScheduleApply(body, received)) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Save task failed");
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, "<html><body>Saving. <a href=\"/\">Back</a></body></html>", HTTPD_RESP_USE_STRLEN);
}
