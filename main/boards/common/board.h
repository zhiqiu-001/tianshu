#ifndef BOARD_H
#define BOARD_H

#include <string>
#include <functional>

#include "display.h"
#include "sensor_manager.h"
#include "mesh_network.h"
#include "host_election.h"
#include "web_server.h"

enum class NetworkEvent {
    Scanning,
    Connecting,
    Connected,
    Disconnected,
    WifiConfigModeEnter,
    WifiConfigModeExit,
};

enum class PowerSaveLevel {
    LOW_POWER,
    BALANCED,
    PERFORMANCE,
};

using NetworkEventCallback = std::function<void(NetworkEvent event, const std::string& data)>;

void* create_board();

class Board {
private:
    Board(const Board&) = delete;
    Board& operator=(const Board&) = delete;

protected:
    Board();
    std::string GenerateUuid();

    std::string uuid_;

public:
    static Board& GetInstance() {
        static Board* instance = static_cast<Board*>(create_board());
        return *instance;
    }

    virtual ~Board() = default;

    virtual std::string GetBoardType() = 0;
    virtual std::string GetUuid() { return uuid_; }
    virtual Display* GetDisplay();
    virtual SensorManager* GetSensorManager();
    virtual MeshNetwork* GetMeshNetwork();
    virtual HostElection* GetHostElection();
    virtual WebServer* GetWebServer();
    
    virtual void StartNetwork() = 0;
    virtual void SetNetworkEventCallback(NetworkEventCallback callback) { (void)callback; }
    virtual const char* GetNetworkStateIcon() = 0;
    
    virtual bool GetTemperature(float& esp32temp);
    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging);
    
    virtual std::string GetSystemInfoJson();
    virtual std::string GetBoardJson() = 0;
    virtual std::string GetDeviceStatusJson() = 0;
    
    virtual void SetPowerSaveLevel(PowerSaveLevel level) = 0;
    
    virtual int Update() { return 0; }
};

#define DECLARE_BOARD(BOARD_CLASS_NAME) \
void* create_board() { \
    return new BOARD_CLASS_NAME(); \
}

#endif