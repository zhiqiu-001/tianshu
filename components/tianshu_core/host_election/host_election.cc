#include "host_election.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"

#define TAG_HOST_ELECTION "HostElection"

static uint32_t GetCurrentTimestamp() {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

HostElection::HostElection() 
    : mesh_(nullptr), 
      role_(NODE_ROLE_STANDBY), 
      election_state_(ElectionState::WAITING),
      wait_start_ms_(0),
      stable_since_ms_(0),
      last_node_count_(0),
      consecutive_stable_checks_(0) {}

int HostElection::Init(MeshNetwork* mesh) {
    ESP_LOGI(TAG_HOST_ELECTION, "Initializing host election...");
    mesh_ = mesh;
    role_ = NODE_ROLE_STANDBY;
    election_state_ = ElectionState::WAITING;
    wait_start_ms_ = GetCurrentTimestamp();
    stable_since_ms_ = 0;
    last_node_count_ = 0;
    consecutive_stable_checks_ = 0;
    return 0;
}

int HostElection::Run() {
    if (!mesh_) {
        role_ = NODE_ROLE_MASTER;
        election_state_ = ElectionState::STABLE;
        return 0;
    }
    
    uint32_t now = GetCurrentTimestamp();
    
    switch (election_state_) {
        case ElectionState::WAITING: {
            if (now - wait_start_ms_ >= WAIT_TIMEOUT_MS) {
                ESP_LOGI(TAG_HOST_ELECTION, "Wait timeout reached (%lu ms), starting stabilization check", 
                         (unsigned long)WAIT_TIMEOUT_MS);
                election_state_ = ElectionState::STABILIZING;
                stable_since_ms_ = now;
                last_node_count_ = mesh_->GetOnlineNodeCount();
            } else {
                ESP_LOGD(TAG_HOST_ELECTION, "Waiting for network... (%lu ms remaining)", 
                         (unsigned long)(WAIT_TIMEOUT_MS - (now - wait_start_ms_)));
            }
            break;
        }
        
        case ElectionState::STABILIZING: {
            int current_node_count = mesh_->GetOnlineNodeCount();
            
            if (current_node_count != last_node_count_) {
                ESP_LOGI(TAG_HOST_ELECTION, "Network changing: %d -> %d nodes, resetting stabilization timer", 
                         last_node_count_, current_node_count);
                stable_since_ms_ = now;
                last_node_count_ = current_node_count;
                consecutive_stable_checks_ = 0;
            } else if (now - stable_since_ms_ >= STABLE_THRESHOLD_MS) {
                consecutive_stable_checks_++;
                
                if (consecutive_stable_checks_ >= STABLE_CHECK_THRESHOLD) {
                    ESP_LOGI(TAG_HOST_ELECTION, "Network stable for %d checks, starting election with %d nodes", 
                             consecutive_stable_checks_, current_node_count);
                    election_state_ = ElectionState::ELECTING;
                }
            }
            break;
        }
        
        case ElectionState::ELECTING: {
            PerformElection();
            election_state_ = ElectionState::STABLE;
            ESP_LOGI(TAG_HOST_ELECTION, "Election complete, role: %s", 
                     role_ == NODE_ROLE_MASTER ? "MASTER" : "SLAVE");
            break;
        }
        
        case ElectionState::STABLE: {
            MonitorMasterHealth();
            break;
        }
    }
    
    return 0;
}

void HostElection::PerformElection() {
    if (!mesh_) {
        role_ = NODE_ROLE_MASTER;
        return;
    }
    
    int node_count = mesh_->GetOnlineNodeCount();
    
    if (node_count == 0) {
        ESP_LOGI(TAG_HOST_ELECTION, "No other nodes, becoming master");
        role_ = NODE_ROLE_MASTER;
        return;
    }
    
    uint8_t my_uuid[16];
    esp_fill_random(my_uuid, sizeof(my_uuid));
    
    bool should_be_master = true;
    
    for (int i = 0; i < node_count; i++) {
        auto node = mesh_->GetNode(i);
        if (node && node->online) {
            if (memcmp(node->id, my_uuid, NODE_ID_LEN) < 0) {
                should_be_master = false;
                break;
            }
        }
    }
    
    role_ = should_be_master ? NODE_ROLE_MASTER : NODE_ROLE_SLAVE;
}

void HostElection::MonitorMasterHealth() {
    if (!mesh_) {
        return;
    }
    
    if (role_ == NODE_ROLE_SLAVE) {
        int node_count = mesh_->GetOnlineNodeCount();
        
        if (node_count == 0) {
            ESP_LOGI(TAG_HOST_ELECTION, "Master node not found, re-electing");
            election_state_ = ElectionState::ELECTING;
        }
    } else {
        int node_count = mesh_->GetOnlineNodeCount();
        
        if (node_count > 0) {
            bool master_still_valid = true;
            
            for (int i = 0; i < node_count; i++) {
                auto node = mesh_->GetNode(i);
                if (node && node->online) {
                    uint8_t my_uuid[16];
                    esp_fill_random(my_uuid, sizeof(my_uuid));
                    
                    if (memcmp(node->id, my_uuid, NODE_ID_LEN) < 0) {
                        master_still_valid = false;
                        break;
                    }
                }
            }
            
            if (!master_still_valid) {
                ESP_LOGI(TAG_HOST_ELECTION, "Found node with lower ID, stepping down");
                role_ = NODE_ROLE_SLAVE;
            }
        }
    }
}

node_role_t HostElection::GetRole() const { return role_; }

bool HostElection::IsMaster() const { return role_ == NODE_ROLE_MASTER; }

ElectionState HostElection::GetElectionState() const { return election_state_; }

bool HostElection::IsElectionComplete() const {
    return election_state_ == ElectionState::STABLE;
}

int HostElection::GetElectionProgress() const {
    switch (election_state_) {
        case ElectionState::WAITING:
            return 0;
        case ElectionState::STABILIZING:
            return 50;
        case ElectionState::ELECTING:
            return 80;
        case ElectionState::STABLE:
            return 100;
        default:
            return 0;
    }
}