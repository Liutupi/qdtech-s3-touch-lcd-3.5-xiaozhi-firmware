#include "qd_esp_mqtt.h"

#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

static const char* TAG = "QdEspMqtt";

QdEspMqtt::QdEspMqtt() {
    event_group_handle_ = xEventGroupCreate();
}

QdEspMqtt::~QdEspMqtt() {
    Disconnect();
    if (event_group_handle_) {
        vEventGroupDelete(event_group_handle_);
        event_group_handle_ = nullptr;
    }
}

bool QdEspMqtt::Connect(const std::string broker_address, int broker_port, const std::string client_id,
                        const std::string username, const std::string password) {
    if (mqtt_client_handle_) {
        Disconnect();
    }

    for (int attempt = 1; attempt <= kConnectAttempts; ++attempt) {
        ESP_LOGI(TAG, "connect start attempt=%d/%d host=%s port=%d client_id_len=%u user_len=%u free_internal=%u largest_internal=%u",
                 attempt, kConnectAttempts, broker_address.c_str(), broker_port,
                 static_cast<unsigned>(client_id.size()), static_cast<unsigned>(username.size()),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));

        esp_mqtt_client_config_t mqtt_config = {};
        mqtt_config.broker.address.hostname = broker_address.c_str();
        mqtt_config.broker.address.port = broker_port;
        mqtt_config.broker.address.transport = broker_port == 8883 ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP;
        mqtt_config.broker.verification.crt_bundle_attach = broker_port == 8883 ? esp_crt_bundle_attach : nullptr;
        mqtt_config.credentials.client_id = client_id.c_str();
        mqtt_config.credentials.username = username.c_str();
        mqtt_config.credentials.authentication.password = password.c_str();
        mqtt_config.session.keepalive = keep_alive_seconds_;
        mqtt_config.network.timeout_ms = kConnectTimeoutMs;

        mqtt_client_handle_ = esp_mqtt_client_init(&mqtt_config);
        if (!mqtt_client_handle_) {
            ESP_LOGE(TAG, "client init failed free_internal=%u largest_internal=%u",
                     static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
            return false;
        }

        esp_mqtt_client_register_event(mqtt_client_handle_, MQTT_EVENT_ANY, [](void* handler_args,
                                                                               esp_event_base_t base,
                                                                               int32_t event_id,
                                                                               void* event_data) {
            static_cast<QdEspMqtt*>(handler_args)->MqttEventCallback(base, event_id, event_data);
        }, this);

        esp_err_t err = esp_mqtt_client_start(mqtt_client_handle_);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "client start failed err=%s", esp_err_to_name(err));
            Disconnect();
            return false;
        }

        EventBits_t bits = xEventGroupWaitBits(event_group_handle_, kConnectedEvent | kErrorEvent,
                                               pdTRUE, pdFALSE, pdMS_TO_TICKS(kConnectTimeoutMs));
        if (bits & kConnectedEvent) {
            ESP_LOGI(TAG, "connect ok attempt=%d host=%s free_internal=%u largest_internal=%u",
                     attempt, broker_address.c_str(),
                     static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
            return true;
        }

        ESP_LOGW(TAG, "connect attempt failed attempt=%d/%d bits=0x%lx host=%s free_internal=%u largest_internal=%u",
                 attempt, kConnectAttempts, static_cast<unsigned long>(bits), broker_address.c_str(),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
        Disconnect();
        if (attempt < kConnectAttempts) {
            vTaskDelay(pdMS_TO_TICKS(1500));
        }
    }

    return false;
}

void QdEspMqtt::Disconnect() {
    if (!mqtt_client_handle_) {
        connected_ = false;
        if (event_group_handle_) {
            xEventGroupClearBits(event_group_handle_, kConnectedEvent | kDisconnectedEvent | kErrorEvent);
        }
        return;
    }
    esp_mqtt_client_stop(mqtt_client_handle_);
    esp_mqtt_client_destroy(mqtt_client_handle_);
    mqtt_client_handle_ = nullptr;
    connected_ = false;
    if (event_group_handle_) {
        xEventGroupClearBits(event_group_handle_, kConnectedEvent | kDisconnectedEvent | kErrorEvent);
    }
}

bool QdEspMqtt::Publish(const std::string topic, const std::string payload, int qos) {
    if (!connected_) {
        return false;
    }
    return esp_mqtt_client_publish(mqtt_client_handle_, topic.c_str(), payload.data(), payload.size(), qos, 0) >= 0;
}

bool QdEspMqtt::Subscribe(const std::string topic, int qos) {
    if (!connected_) {
        return false;
    }
    return esp_mqtt_client_subscribe_single(mqtt_client_handle_, topic.c_str(), qos) >= 0;
}

bool QdEspMqtt::Unsubscribe(const std::string topic) {
    if (!connected_) {
        return false;
    }
    return esp_mqtt_client_unsubscribe(mqtt_client_handle_, topic.c_str()) >= 0;
}

bool QdEspMqtt::IsConnected() {
    return connected_;
}

void QdEspMqtt::MqttEventCallback(esp_event_base_t base, int32_t event_id, void* event_data) {
    (void)base;
    auto* event = static_cast<esp_mqtt_event_t*>(event_data);
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            connected_ = true;
            if (on_connected_callback_) {
                on_connected_callback_();
            }
            xEventGroupSetBits(event_group_handle_, kConnectedEvent);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT_EVENT_DISCONNECTED");
            connected_ = false;
            if (on_disconnected_callback_) {
                on_disconnected_callback_();
            }
            xEventGroupSetBits(event_group_handle_, kDisconnectedEvent);
            break;
        case MQTT_EVENT_DATA: {
            auto topic = std::string(event->topic, event->topic_len);
            auto payload = std::string(event->data, event->data_len);
            if (event->data_len == event->total_data_len) {
                if (on_message_callback_) {
                    on_message_callback_(topic, payload);
                }
            } else {
                message_payload_.append(payload);
                if (message_payload_.size() >= event->total_data_len && on_message_callback_) {
                    on_message_callback_(topic, message_payload_);
                    message_payload_.clear();
                }
            }
            break;
        }
        case MQTT_EVENT_ERROR:
            xEventGroupSetBits(event_group_handle_, kErrorEvent);
            if (event->error_handle) {
                ESP_LOGW(TAG, "MQTT_EVENT_ERROR type=%d esp=%s tls_stack=0x%x cert_flags=0x%x connect_rc=%d",
                         event->error_handle->error_type,
                         esp_err_to_name(event->error_handle->esp_tls_last_esp_err),
                         event->error_handle->esp_tls_stack_err,
                         event->error_handle->esp_tls_cert_verify_flags,
                         event->error_handle->connect_return_code);
            } else {
                ESP_LOGW(TAG, "MQTT_EVENT_ERROR without error handle");
            }
            break;
        default:
            ESP_LOGI(TAG, "Unhandled event id %ld", event_id);
            break;
    }
}
