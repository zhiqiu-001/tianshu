#include "board.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"

#define TAG "BOARD_TIANSHU_MESH"

class BoardTianShuMesh : public Board {
public:
    BoardTianShuMesh() 
        : uptime_(0),
          sensor_manager_(),
          mesh_network_(),
          host_election_(),
          web_server_() {
        esp_read_mac(mac_, ESP_MAC_WIFI_STA);
        ESP_LOGI(TAG, "Board MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 mac_[0], mac_[1], mac_[2], mac_[3], mac_[4], mac_[5]);
    }

    ~BoardTianShuMesh() override {}

    std::string GetBoardType() override { return "tianshu-mesh"; }

    Display* GetDisplay() override {
        static NoDisplay display;
        return &display;
    }

    SensorManager* GetSensorManager() override { return &sensor_manager_; }

    MeshNetwork* GetMeshNetwork() override { return &mesh_network_; }

    HostElection* GetHostElection() override { return &host_election_; }

    WebServer* GetWebServer() override { return &web_server_; }

    void StartNetwork() override {
        ESP_LOGI(TAG, "Starting network...");
    }

    const char* GetNetworkStateIcon() override {
        return "wifi";
    }

    std::string GetBoardJson() override {
        return R"({"type":"tianshu-mesh","features":["ble-mesh","wifi","sensor","webserver"]})";
    }

    std::string GetDeviceStatusJson() override {
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac_[0], mac_[1], mac_[2], mac_[3], mac_[4], mac_[5]);
        
        std::string json = R"({"mac":")" + std::string(mac_str) + R"(",)";
        json += R"("uptime":)" + std::to_string(uptime_) + R"(,)";
        json += R"("sensor_count":)" + std::to_string(sensor_manager_.GetSensorCount()) + R"(})";
        return json;
    }

    void SetPowerSaveLevel(PowerSaveLevel level) override {
        ESP_LOGI(TAG, "SetPowerSaveLevel: %d", static_cast<int>(level));
    }

    int Update() override {
        uptime_++;
        return 0;
    }

private:
    uint8_t mac_[6];
    int uptime_;
    
    SensorManager sensor_manager_;
    MeshNetwork mesh_network_;
    HostElection host_election_;
    WebServer web_server_;
};

DECLARE_BOARD(BoardTianShuMesh)