#include "board.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_chip_info.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#define TAG "Board"

Board::Board() {
    uuid_ = GenerateUuid();
    ESP_LOGI(TAG, "UUID=%s", uuid_.c_str());
}

std::string Board::GenerateUuid() {
    uint8_t uuid[16];
    esp_fill_random(uuid, sizeof(uuid));
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    uuid[8] = (uuid[8] & 0x3F) | 0x80;
    
    char uuid_str[37];
    snprintf(uuid_str, sizeof(uuid_str),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid[0], uuid[1], uuid[2], uuid[3],
        uuid[4], uuid[5], uuid[6], uuid[7],
        uuid[8], uuid[9], uuid[10], uuid[11],
        uuid[12], uuid[13], uuid[14], uuid[15]);
    
    return std::string(uuid_str);
}

Display* Board::GetDisplay() {
    static NoDisplay display;
    return &display;
}

SensorManager* Board::GetSensorManager() {
    static NoSensorManager sensor_manager;
    return &sensor_manager;
}

MeshNetwork* Board::GetMeshNetwork() {
    static NoMeshNetwork mesh_network;
    return &mesh_network;
}

HostElection* Board::GetHostElection() {
    static NoHostElection host_election;
    return &host_election;
}

WebServer* Board::GetWebServer() {
    static NoWebServer web_server;
    return &web_server;
}

bool Board::GetTemperature(float& esp32temp) {
    return false;
}

bool Board::GetBatteryLevel(int& level, bool& charging, bool& discharging) {
    return false;
}

std::string Board::GetSystemInfoJson() {
    std::string json = R"({"version":2,)";
    
    json += R"("uuid":")" + uuid_ + R"(",)";
    
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    json += R"("chip_info":{)";
    json += R"("model":)" + std::to_string(chip_info.model) + R"(,)";
    json += R"("cores":)" + std::to_string(chip_info.cores) + R"(,)";
    json += R"("revision":)" + std::to_string(chip_info.revision) + R"(},)";

    auto app_desc = esp_app_get_description();
    json += R"("application":{)";
    json += R"("name":")" + std::string(app_desc->project_name) + R"(",)";
    json += R"("version":")" + std::string(app_desc->version) + R"(",)";
    json += R"("compile_time":")" + std::string(app_desc->date) + R"(T)" + std::string(app_desc->time) + R"(Z",)";
    json += R"("idf_version":")" + std::string(app_desc->idf_ver) + R"(")";
    json += R"(},)";

    auto ota_partition = esp_ota_get_running_partition();
    if (ota_partition) {
        json += R"("ota":{)";
        json += R"("label":")" + std::string(ota_partition->label) + R"(")";
        json += R"(},)";
    }

    auto display = GetDisplay();
    if (display) {
        json += R"("display":{)";
        json += R"("width":)" + std::to_string(display->width()) + R"(,)";
        json += R"("height":)" + std::to_string(display->height()) + R"(})";
    }

    json += R"("board":)" + GetBoardJson();

    json += R"(})";
    return json;
}
