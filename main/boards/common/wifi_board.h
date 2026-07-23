#ifndef WIFI_BOARD_H
#define WIFI_BOARD_H

#include "board.h"
#include <vector>
#include <string>

class WifiBoard : public Board {
protected:
    bool wifi_config_mode_ = false;
    void EnterWifiConfigMode(bool station_already_started = false);
    virtual int GetStartupWifiConnectTimeoutMs() const;
    virtual std::string GetBoardJson() override;

public:
    WifiBoard();
    virtual std::string GetBoardType() override;
    virtual void StartNetwork() override;
    virtual Http* CreateHttp() override;
    virtual WebSocket* CreateWebSocket() override;
    virtual Mqtt* CreateMqtt() override;
    virtual Udp* CreateUdp() override;
    virtual const char* GetNetworkStateIcon() override;
    virtual void SetPowerSaveMode(bool enabled) override;
    virtual void ResetWifiConfiguration();
    virtual AudioCodec* GetAudioCodec() override { return nullptr; }
    virtual std::string GetDeviceStatusJson() override;
    
    // WiFi管理功能
    bool SwitchToWifi(const std::string& ssid, const std::string& password);
    std::vector<std::string> GetSavedWifiList();
    bool RemoveSavedWifi(int index);
    bool SetDefaultWifi(int index);
};

#endif // WIFI_BOARD_H
