#include "application.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "APPLICATION"
#define MAIN_LOOP_DELAY_MS 1000

int Application::Initialize()
{
    ESP_LOGI(TAG, "Initializing Tianshu application");
    
    if (initialized_) {
        ESP_LOGW(TAG, "Application already initialized");
        return 0;
    }
    
    Board& board = Board::GetInstance();
    
    ESP_LOGI(TAG, "Board type: %s, UUID: %s", 
             board.GetBoardType().c_str(), 
             board.GetUuid().c_str());
    
    auto sensor_manager = board.GetSensorManager();
    auto mesh_network = board.GetMeshNetwork();
    auto host_election = board.GetHostElection();
    auto web_server = board.GetWebServer();
    
    if (sensor_manager) sensor_manager->Scan();
    if (mesh_network) mesh_network->Init();
    if (host_election) host_election->Init(mesh_network);
    if (web_server) web_server->Init();
    
    state_ = AppState::INIT;
    initialized_ = true;
    
    ESP_LOGI(TAG, "Application initialized successfully");
    
    return 0;
}

int Application::Deinitialize()
{
    ESP_LOGI(TAG, "Deinitializing Tianshu application");
    
    if (!initialized_) {
        ESP_LOGW(TAG, "Application not initialized");
        return 0;
    }
    
    Board& board = Board::GetInstance();
    
    auto mesh_network = board.GetMeshNetwork();
    auto web_server = board.GetWebServer();
    
    if (mesh_network) mesh_network->Leave();
    if (web_server) web_server->Stop();
    
    initialized_ = false;
    state_ = AppState::INIT;
    
    ESP_LOGI(TAG, "Application deinitialized successfully");
    
    return 0;
}

void Application::Run()
{
    if (!initialized_) {
        ESP_LOGE(TAG, "Application not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Starting Tianshu application");
    state_ = AppState::SENSOR_SCAN;
    
    while (1) {
        Board& board = Board::GetInstance();
        
        switch (state_) {
            case AppState::INIT:
                break;
                
            case AppState::SENSOR_SCAN: {
                auto sensor_manager = board.GetSensorManager();
                if (sensor_manager && sensor_manager->Scan() == 0) {
                    Transition(AppState::NETWORK_INIT);
                }
                break;
            }
            
            case AppState::NETWORK_INIT: {
                auto mesh_network = board.GetMeshNetwork();
                if (mesh_network) {
                    if (mesh_network->Init() == 0) {
                        board.StartNetwork();
                        if (mesh_network->Join() == 0) {
                            Transition(AppState::MESH_INIT);
                        }
                    }
                } else {
                    Transition(AppState::MESH_INIT);
                }
                break;
            }
            
            case AppState::MESH_INIT: {
                auto host_election = board.GetHostElection();
                if (host_election && host_election->Init(board.GetMeshNetwork()) == 0) {
                    Transition(AppState::HOST_ELECTION);
                } else {
                    Transition(AppState::RUNNING);
                }
                break;
            }
            
            case AppState::HOST_ELECTION: {
                Transition(AppState::RUNNING);
                break;
            }
            
            case AppState::RUNNING: {
                board.Update();
                
                auto host_election = board.GetHostElection();
                if (host_election) host_election->Run();
                
                auto web_server = board.GetWebServer();
                if (web_server && !web_server->IsRunning()) {
                    web_server->Start();
                }
                break;
            }
            
            case AppState::ERROR:
                break;
                
            default:
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY_MS));
    }
}

AppState Application::GetState() const
{
    return state_;
}

Board* Application::GetBoard() const
{
    return &Board::GetInstance();
}

int Application::Transition(AppState next_state)
{
    const char* state_names[] = {
        "INIT",
        "SENSOR_SCAN",
        "NETWORK_INIT",
        "MESH_INIT",
        "HOST_ELECTION",
        "RUNNING",
        "ERROR"
    };
    
    ESP_LOGI(TAG, "State transition: %s -> %s", 
             state_names[static_cast<int>(state_)], 
             state_names[static_cast<int>(next_state)]);
    
    state_ = next_state;
    
    return 0;
}