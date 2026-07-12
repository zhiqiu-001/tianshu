#ifndef BOARD_H
#define BOARD_H

#include <string>
#include <functional>

#include "sensor_manager.h"
#include "mesh_network.h"
#include "host_election.h"
#include "web_server.h"

void* create_board();

class Display;

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
    virtual EspHttpWebServer* GetWebServer();

    virtual bool GetTemperature(float& esp32temp);
    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging);
    
    virtual std::string GetSystemInfoJson();
    virtual std::string GetBoardJson() = 0;
    virtual std::string GetDeviceStatusJson() = 0;
    
    virtual int Update() { return 0; }


    MeshNetwork* mesh_network_;
    HostElection* host_election_;
    EspHttpWebServer* web_server_;
};

#define DECLARE_BOARD(BOARD_CLASS_NAME) \
void* create_board() { \
    return new BOARD_CLASS_NAME(); \
}

#endif