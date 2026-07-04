#pragma once

#include "mqtt.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <mqtt_client.h>

#include <string>

class QdEspMqtt : public Mqtt {
public:
    QdEspMqtt();
    ~QdEspMqtt() override;

    bool Connect(const std::string broker_address, int broker_port, const std::string client_id,
                 const std::string username, const std::string password) override;
    void Disconnect() override;
    bool Publish(const std::string topic, const std::string payload, int qos = 0) override;
    bool Subscribe(const std::string topic, int qos = 0) override;
    bool Unsubscribe(const std::string topic) override;
    bool IsConnected() override;

private:
    static constexpr int kConnectTimeoutMs = 25000;
    static constexpr int kConnectAttempts = 2;
    static constexpr EventBits_t kConnectedEvent = BIT0;
    static constexpr EventBits_t kDisconnectedEvent = BIT1;
    static constexpr EventBits_t kErrorEvent = BIT2;

    bool connected_ = false;
    EventGroupHandle_t event_group_handle_ = nullptr;
    std::string message_payload_;
    esp_mqtt_client_handle_t mqtt_client_handle_ = nullptr;

    void MqttEventCallback(esp_event_base_t base, int32_t event_id, void* event_data);
};
