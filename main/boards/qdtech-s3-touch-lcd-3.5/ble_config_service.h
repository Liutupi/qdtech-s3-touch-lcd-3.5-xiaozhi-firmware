#pragma once

#include <string>
#include <functional>
#include "config_service.h"

class BleConfigService {
public:
    static BleConfigService& GetInstance() {
        static BleConfigService instance;
        return instance;
    }

    // 初始化 BLE 服务
    bool Start();

    // 停止 BLE 服务
    void Stop();

    // 设置状态回调
    void SetStatusCallback(std::function<void(const char*)> callback) {
        status_callback_ = callback;
    }

private:
    BleConfigService() = default;
    ~BleConfigService() = default;
    BleConfigService(const BleConfigService&) = delete;
    BleConfigService& operator=(const BleConfigService&) = delete;

    std::function<void(const char*)> status_callback_;
    bool started_ = false;

    // 处理接收到的配置数据
    void HandleConfigData(const char* json_data);
};
