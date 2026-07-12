#include "mesh_network.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"

#define TAG_MESH_NETWORK "MeshNetwork"

#define BEACON_MAGIC 0x5453

#define INITIAL_SCAN_INTERVAL_MS 2000
#define ACTIVE_SCAN_INTERVAL_MS 5000
#define PASSIVE_SCAN_INTERVAL_MS 30000
#define LOW_POWER_SCAN_INTERVAL_MS 60000

#define INITIAL_BROADCAST_INTERVAL_MS 1000
#define ACTIVE_BROADCAST_INTERVAL_MS 2000
#define PASSIVE_BROADCAST_INTERVAL_MS 5000
#define LOW_POWER_BROADCAST_INTERVAL_MS 10000

#define STABLE_SCAN_THRESHOLD 5
#define NODE_OFFLINE_TIMEOUT_MS 30000
#define NODE_WEAK_RSSI_THRESHOLD -80
#define TOPOLOGY_UPDATE_INTERVAL_MS 10000

static uint32_t GetCurrentTimestamp() {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

MeshNetwork::MeshNetwork() 
    : node_count_(0), 
      role_(NODE_ROLE_SLAVE), 
      state_(NETWORK_STATE_DISCONNECTED), 
      policy_(CONNECTION_POLICY_AUTO),
      scan_mode_(SCAN_MODE_IDLE),
      broadcast_mode_(BROADCAST_MODE_SILENT),
      last_scan_time_ms_(0),
      last_broadcast_time_ms_(0),
      last_heartbeat_time_ms_(0),
      last_topology_update_ms_(0),
      current_scan_interval_ms_(INITIAL_SCAN_INTERVAL_MS),
      current_broadcast_interval_ms_(INITIAL_BROADCAST_INTERVAL_MS),
      consecutive_stable_scans_(0),
      nodes_found_in_last_scan_(0),
      connection_attempts_(0),
      adjacency_count_(0),
      routing_table_size_(0) {
    memset(nodes_, 0, sizeof(nodes_));
    memset(my_node_id_, 0, sizeof(my_node_id_));
    memset(&beacon_data_, 0, sizeof(beacon_data_));
    memset(adjacency_table_, 0, sizeof(adjacency_table_));
    memset(routing_table_, 0, sizeof(routing_table_));
}

int MeshNetwork::Init() {
    ESP_LOGI(TAG_MESH_NETWORK, "Initializing BLE Mesh...");
    
    esp_fill_random(my_node_id_, NODE_ID_LEN);
    
    beacon_data_.magic[0] = BEACON_MAGIC & 0xFF;
    beacon_data_.magic[1] = (BEACON_MAGIC >> 8) & 0xFF;
    beacon_data_.version = 1;
    memcpy(beacon_data_.node_id, my_node_id_, NODE_ID_LEN);
    beacon_data_.role = NODE_ROLE_STANDBY;
    beacon_data_.network_state = NETWORK_STATE_DISCONNECTED;
    beacon_data_.node_count = 0;
    beacon_data_.connection_count = 0;
    beacon_data_.hop_count = 0;
    
    scan_mode_ = SCAN_MODE_ACTIVE;
    broadcast_mode_ = BROADCAST_MODE_ADVERTISING;
    
    return 0;
}

int MeshNetwork::Join() {
    ESP_LOGI(TAG_MESH_NETWORK, "Joining BLE Mesh network...");
    
    state_ = NETWORK_STATE_CONNECTING;
    scan_mode_ = SCAN_MODE_ACTIVE;
    broadcast_mode_ = BROADCAST_MODE_ADVERTISING;
    
    current_scan_interval_ms_ = INITIAL_SCAN_INTERVAL_MS;
    current_broadcast_interval_ms_ = INITIAL_BROADCAST_INTERVAL_MS;
    consecutive_stable_scans_ = 0;
    connection_attempts_ = 0;
    
    last_scan_time_ms_ = GetCurrentTimestamp();
    last_broadcast_time_ms_ = GetCurrentTimestamp();
    last_heartbeat_time_ms_ = GetCurrentTimestamp();
    last_topology_update_ms_ = GetCurrentTimestamp();
    
    StartScan();
    StartBroadcast();
    
    return 0;
}

int MeshNetwork::Leave() {
    ESP_LOGI(TAG_MESH_NETWORK, "Leaving BLE Mesh network...");
    
    StopScan();
    StopBroadcast();
    
    state_ = NETWORK_STATE_DISCONNECTED;
    scan_mode_ = SCAN_MODE_IDLE;
    broadcast_mode_ = BROADCAST_MODE_SILENT;
    
    for (int i = 0; i < node_count_; i++) {
        if (nodes_[i].direct_connected) {
            DisconnectFromNode(&nodes_[i]);
        }
    }
    
    node_count_ = 0;
    adjacency_count_ = 0;
    routing_table_size_ = 0;
    memset(nodes_, 0, sizeof(nodes_));
    memset(adjacency_table_, 0, sizeof(adjacency_table_));
    memset(routing_table_, 0, sizeof(routing_table_));
    
    return 0;
}

int MeshNetwork::Run() {
    if (state_ == NETWORK_STATE_DISCONNECTED) {
        return 0;
    }
    
    uint32_t now = GetCurrentTimestamp();
    
    UpdateBroadcastData();
    
    if (broadcast_mode_ != BROADCAST_MODE_SILENT && 
        now - last_broadcast_time_ms_ >= current_broadcast_interval_ms_) {
        last_broadcast_time_ms_ = now;
    }
    
    if (scan_mode_ != SCAN_MODE_IDLE && 
        now - last_scan_time_ms_ >= current_scan_interval_ms_) {
        PerformScan();
        ProcessScanResults();
        last_scan_time_ms_ = now;
    }
    
    if (now - last_heartbeat_time_ms_ >= 5000) {
        SendHeartbeat();
        UpdateHeartbeats();
        last_heartbeat_time_ms_ = now;
    }
    
    DetectOfflineNodes();
    DetectWeakConnections();
    EstablishConnections();
    PruneConnections();
    
    if (now - last_topology_update_ms_ >= TOPOLOGY_UPDATE_INTERVAL_MS) {
        BuildTopology();
        UpdateRoutingTable();
        UpdateHopCounts();
        last_topology_update_ms_ = now;
    }
    
    AdjustScanInterval();
    AdjustBroadcastInterval();
    
    if (GetOnlineNodeCount() > 0 && state_ != NETWORK_STATE_CONNECTED) {
        state_ = NETWORK_STATE_CONNECTED;
        ESP_LOGI(TAG_MESH_NETWORK, "Network connected, %d online nodes", GetOnlineNodeCount());
    }
    
    return 0;
}

int MeshNetwork::StartBroadcast() {
    ESP_LOGI(TAG_MESH_NETWORK, "Starting broadcast (mode: %d)", broadcast_mode_);
    return 0;
}

int MeshNetwork::StopBroadcast() {
    ESP_LOGI(TAG_MESH_NETWORK, "Stopping broadcast");
    return 0;
}

int MeshNetwork::UpdateBroadcastData() {
    memcpy(beacon_data_.node_id, my_node_id_, NODE_ID_LEN);
    beacon_data_.role = role_;
    beacon_data_.network_state = state_;
    beacon_data_.node_count = GetOnlineNodeCount();
    beacon_data_.connection_count = GetConnectionCount();
    beacon_data_.hop_count = GetNetworkDiameter();
    return 0;
}

int MeshNetwork::ProcessBeacon(const uint8_t *data, size_t len, int8_t rssi) {
    if (len < sizeof(beacon_data_t)) {
        return -1;
    }
    
    beacon_data_t *beacon = (beacon_data_t *)data;
    
    if (beacon->magic[0] != (BEACON_MAGIC & 0xFF) || 
        beacon->magic[1] != ((BEACON_MAGIC >> 8) & 0xFF)) {
        return -1;
    }
    
    if (memcmp(beacon->node_id, my_node_id_, NODE_ID_LEN) == 0) {
        return 0;
    }
    
    bool updated = UpdateNode(beacon->node_id, rssi);
    if (!updated) {
        AddNode(beacon->node_id, rssi);
    }
    
    return 0;
}

int MeshNetwork::StartScan() {
    ESP_LOGI(TAG_MESH_NETWORK, "Starting scan (mode: %d, interval: %lu ms)", 
             scan_mode_, (unsigned long)current_scan_interval_ms_);
    return 0;
}

int MeshNetwork::StopScan() {
    ESP_LOGI(TAG_MESH_NETWORK, "Stopping scan");
    return 0;
}

int MeshNetwork::PerformScan() {
    ESP_LOGD(TAG_MESH_NETWORK, "Performing scan (mode: %d, interval: %lu ms)", 
             scan_mode_, (unsigned long)current_scan_interval_ms_);
    
    nodes_found_in_last_scan_ = 0;
    
    return 0;
}

int MeshNetwork::ProcessScanResults() {
    if (nodes_found_in_last_scan_ > 0) {
        ESP_LOGI(TAG_MESH_NETWORK, "Scan found %d new nodes", nodes_found_in_last_scan_);
        consecutive_stable_scans_ = 0;
        
        if (scan_mode_ != SCAN_MODE_ACTIVE) {
            scan_mode_ = SCAN_MODE_ACTIVE;
            ESP_LOGI(TAG_MESH_NETWORK, "Switching to active scan mode");
        }
    } else {
        consecutive_stable_scans_++;
        ESP_LOGD(TAG_MESH_NETWORK, "No new nodes found, stable count: %d", consecutive_stable_scans_);
    }
    
    return 0;
}

int MeshNetwork::ConnectToNode(node_t *node) {
    if (!node || node->direct_connected) {
        return 0;
    }
    
    if (adjacency_count_ >= MAX_DIRECT_CONNECTIONS) {
        ESP_LOGW(TAG_MESH_NETWORK, "Max direct connections reached");
        return -1;
    }
    
    ESP_LOGI(TAG_MESH_NETWORK, "Connecting to node: %02X:%02X:%02X:%02X:%02X:%02X (RSSI: %d)",
             node->id[0], node->id[1], node->id[2], 
             node->id[3], node->id[4], node->id[5], node->rssi);
    
    node->direct_connected = true;
    node->authenticated = true;
    node->connect_time_ms = GetCurrentTimestamp();
    node->last_heartbeat_ms = 0;
    
    adjacency_entry_t *entry = &adjacency_table_[adjacency_count_];
    memcpy(entry->neighbor_id, node->id, NODE_ID_LEN);
    entry->rssi = node->rssi;
    entry->link_quality = (node->rssi + 100) * 100 / 100;
    entry->last_comm_ms = GetCurrentTimestamp();
    adjacency_count_++;
    
    return 0;
}

int MeshNetwork::DisconnectFromNode(node_t *node) {
    if (!node || !node->direct_connected) {
        return 0;
    }
    
    ESP_LOGI(TAG_MESH_NETWORK, "Disconnecting from node: %02X:%02X:%02X:%02X:%02X:%02X",
             node->id[0], node->id[1], node->id[2], 
             node->id[3], node->id[4], node->id[5]);
    
    node->direct_connected = false;
    node->authenticated = false;
    
    for (int i = 0; i < adjacency_count_; i++) {
        if (memcmp(adjacency_table_[i].neighbor_id, node->id, NODE_ID_LEN) == 0) {
            for (int j = i; j < adjacency_count_ - 1; j++) {
                memcpy(&adjacency_table_[j], &adjacency_table_[j + 1], sizeof(adjacency_entry_t));
            }
            adjacency_count_--;
            break;
        }
    }
    
    return 0;
}

int MeshNetwork::EstablishConnections() {
    ApplyConnectionPolicy();
    return 0;
}

int MeshNetwork::PruneConnections() {
    int current_connections = GetConnectionCount();
    
    if (current_connections <= MAX_DIRECT_CONNECTIONS / 2) {
        return 0;
    }
    
    int worst_node_index = -1;
    int8_t worst_rssi = 127;
    
    for (int i = 0; i < node_count_; i++) {
        if (!nodes_[i].online || !nodes_[i].direct_connected) {
            continue;
        }
        
        if (nodes_[i].rssi < worst_rssi) {
            worst_rssi = nodes_[i].rssi;
            worst_node_index = i;
        }
    }
    
    if (worst_node_index >= 0 && worst_rssi < NODE_WEAK_RSSI_THRESHOLD) {
        DisconnectFromNode(&nodes_[worst_node_index]);
        ESP_LOGI(TAG_MESH_NETWORK, "Pruned weak connection (RSSI: %d)", worst_rssi);
    }
    
    return 0;
}

void MeshNetwork::UpdateHeartbeats() {
    uint32_t now = GetCurrentTimestamp();
    
    for (int i = 0; i < node_count_; i++) {
        if (nodes_[i].online && nodes_[i].direct_connected) {
            nodes_[i].last_heartbeat_ms = now;
        }
    }
    
    for (int i = 0; i < adjacency_count_; i++) {
        adjacency_table_[i].last_comm_ms = now;
    }
}

void MeshNetwork::DetectOfflineNodes() {
    uint32_t now = GetCurrentTimestamp();
    
    for (int i = node_count_ - 1; i >= 0; i--) {
        if (!nodes_[i].online) {
            continue;
        }
        
        uint32_t last_activity_ms = nodes_[i].direct_connected ? 
                                     nodes_[i].last_heartbeat_ms : nodes_[i].last_seen_ms;
        
        if (now - last_activity_ms >= NODE_OFFLINE_TIMEOUT_MS) {
            ESP_LOGI(TAG_MESH_NETWORK, "Node %02X:%02X:%02X:%02X:%02X:%02X offline",
                     nodes_[i].id[0], nodes_[i].id[1], nodes_[i].id[2],
                     nodes_[i].id[3], nodes_[i].id[4], nodes_[i].id[5]);
            
            if (nodes_[i].direct_connected) {
                DisconnectFromNode(&nodes_[i]);
            }
            
            RemoveNode(i);
            
            if (scan_mode_ == SCAN_MODE_LOW_POWER || scan_mode_ == SCAN_MODE_PASSIVE) {
                ESP_LOGI(TAG_MESH_NETWORK, "Node went offline, resuming active scan");
                scan_mode_ = SCAN_MODE_ACTIVE;
                current_scan_interval_ms_ = ACTIVE_SCAN_INTERVAL_MS;
                consecutive_stable_scans_ = 0;
            }
        }
    }
}

void MeshNetwork::DetectWeakConnections() {
    for (int i = 0; i < node_count_; i++) {
        if (nodes_[i].direct_connected && nodes_[i].avg_rssi < NODE_WEAK_RSSI_THRESHOLD) {
            ESP_LOGW(TAG_MESH_NETWORK, "Weak connection detected: %02X:%02X:%02X:%02X:%02X:%02X (avg RSSI: %d)",
                     nodes_[i].id[0], nodes_[i].id[1], nodes_[i].id[2],
                     nodes_[i].id[3], nodes_[i].id[4], nodes_[i].id[5], nodes_[i].avg_rssi);
        }
    }
}

void MeshNetwork::AdjustScanInterval() {
    if (consecutive_stable_scans_ >= STABLE_SCAN_THRESHOLD) {
        if (scan_mode_ == SCAN_MODE_ACTIVE) {
            scan_mode_ = SCAN_MODE_PASSIVE;
            current_scan_interval_ms_ = PASSIVE_SCAN_INTERVAL_MS;
            ESP_LOGI(TAG_MESH_NETWORK, "Network stable, switching to passive scan (%lu ms)", 
                     (unsigned long)current_scan_interval_ms_);
        } else if (scan_mode_ == SCAN_MODE_PASSIVE && GetOnlineNodeCount() > 2) {
            scan_mode_ = SCAN_MODE_LOW_POWER;
            current_scan_interval_ms_ = LOW_POWER_SCAN_INTERVAL_MS;
            ESP_LOGI(TAG_MESH_NETWORK, "Network stable with many nodes, switching to low power scan (%lu ms)", 
                     (unsigned long)current_scan_interval_ms_);
        }
    } else {
        if (scan_mode_ != SCAN_MODE_ACTIVE) {
            scan_mode_ = SCAN_MODE_ACTIVE;
            current_scan_interval_ms_ = ACTIVE_SCAN_INTERVAL_MS;
            ESP_LOGI(TAG_MESH_NETWORK, "Network changing, switching to active scan (%lu ms)", 
                     (unsigned long)current_scan_interval_ms_);
        }
    }
}

void MeshNetwork::AdjustBroadcastInterval() {
    if (scan_mode_ == SCAN_MODE_LOW_POWER) {
        current_broadcast_interval_ms_ = LOW_POWER_BROADCAST_INTERVAL_MS;
        broadcast_mode_ = BROADCAST_MODE_BEACON_ONLY;
    } else if (scan_mode_ == SCAN_MODE_PASSIVE) {
        current_broadcast_interval_ms_ = PASSIVE_BROADCAST_INTERVAL_MS;
        broadcast_mode_ = BROADCAST_MODE_ADVERTISING;
    } else {
        current_broadcast_interval_ms_ = ACTIVE_BROADCAST_INTERVAL_MS;
        broadcast_mode_ = BROADCAST_MODE_ADVERTISING;
    }
}

void MeshNetwork::ApplyConnectionPolicy() {
    connection_policy_t effective_policy = policy_;
    
    if (policy_ == CONNECTION_POLICY_AUTO) {
        effective_policy = AutoSelectPolicy();
    }
    
    switch (effective_policy) {
        case CONNECTION_POLICY_FULL_MESH:
            BuildFullMeshTopology();
            break;
        case CONNECTION_POLICY_STAR:
            BuildStarTopology();
            break;
        case CONNECTION_POLICY_HYBRID:
        case CONNECTION_POLICY_AUTO:
        default:
            BuildHybridTopology();
            break;
    }
}

void MeshNetwork::BuildFullMeshTopology() {
    int current_connections = GetConnectionCount();
    int target_connections = GetOnlineNodeCount() - 1;
    
    if (current_connections >= MAX_DIRECT_CONNECTIONS || current_connections >= target_connections) {
        return;
    }
    
    for (int i = 0; i < node_count_; i++) {
        if (!nodes_[i].online || nodes_[i].direct_connected) {
            continue;
        }
        
        if (GetConnectionCount() < MAX_DIRECT_CONNECTIONS) {
            ConnectToNode(&nodes_[i]);
        } else {
            break;
        }
    }
    
    ESP_LOGI(TAG_MESH_NETWORK, "Full mesh topology: %d connections", GetConnectionCount());
}

void MeshNetwork::BuildStarTopology() {
    bool is_master = (role_ == NODE_ROLE_MASTER);
    int current_connections = GetConnectionCount();
    
    if (is_master) {
        for (int i = 0; i < node_count_; i++) {
            if (!nodes_[i].online || nodes_[i].direct_connected) {
                continue;
            }
            
            if (current_connections < MAX_DIRECT_CONNECTIONS) {
                ConnectToNode(&nodes_[i]);
                current_connections++;
            } else {
                break;
            }
        }
        
        ESP_LOGI(TAG_MESH_NETWORK, "Star topology (master): %d connections", current_connections);
    } else {
        for (int i = 0; i < node_count_; i++) {
            if (!nodes_[i].online || nodes_[i].direct_connected) {
                continue;
            }
            
            if (nodes_[i].role == NODE_ROLE_MASTER) {
                ConnectToNode(&nodes_[i]);
                break;
            }
        }
        
        ESP_LOGI(TAG_MESH_NETWORK, "Star topology (slave): %d connections", GetConnectionCount());
    }
}

void MeshNetwork::BuildHybridTopology() {
    bool is_master = (role_ == NODE_ROLE_MASTER);
    int current_connections = GetConnectionCount();
    
    if (is_master) {
        for (int i = 0; i < node_count_; i++) {
            if (!nodes_[i].online || nodes_[i].direct_connected) {
                continue;
            }
            
            if (current_connections < MAX_DIRECT_CONNECTIONS) {
                ConnectToNode(&nodes_[i]);
                current_connections++;
            } else {
                break;
            }
        }
    } else {
        bool connected_to_master = false;
        
        for (int i = 0; i < node_count_; i++) {
            if (!nodes_[i].online) {
                continue;
            }
            
            if (nodes_[i].role == NODE_ROLE_MASTER) {
                if (!nodes_[i].direct_connected) {
                    ConnectToNode(&nodes_[i]);
                    connected_to_master = true;
                }
                break;
            }
        }
        
        for (int i = 0; i < node_count_ && current_connections < MAX_DIRECT_CONNECTIONS; i++) {
            if (!nodes_[i].online || nodes_[i].direct_connected) {
                continue;
            }
            
            if (nodes_[i].role != NODE_ROLE_MASTER) {
                ConnectToNode(&nodes_[i]);
                current_connections++;
            }
        }
    }
    
    ESP_LOGI(TAG_MESH_NETWORK, "Hybrid topology: %d connections", GetConnectionCount());
}

void MeshNetwork::BuildTopology() {
    ApplyConnectionPolicy();
}

void MeshNetwork::UpdateRoutingTable() {
    routing_table_size_ = 0;
    
    for (int i = 0; i < node_count_; i++) {
        if (!nodes_[i].online) {
            continue;
        }
        
        if (nodes_[i].direct_connected) {
            route_entry_t *entry = &routing_table_[routing_table_size_];
            memcpy(entry->dest_id, nodes_[i].id, NODE_ID_LEN);
            memcpy(entry->next_hop_id, nodes_[i].id, NODE_ID_LEN);
            entry->hop_count = 1;
            entry->rssi = nodes_[i].rssi;
            entry->last_update_ms = GetCurrentTimestamp();
            routing_table_size_++;
        }
    }
}

int MeshNetwork::FindRoute(uint8_t *dest_id, uint8_t *next_hop_id) {
    for (int i = 0; i < routing_table_size_; i++) {
        if (memcmp(routing_table_[i].dest_id, dest_id, NODE_ID_LEN) == 0) {
            memcpy(next_hop_id, routing_table_[i].next_hop_id, NODE_ID_LEN);
            return routing_table_[i].hop_count;
        }
    }
    
    for (int i = 0; i < node_count_; i++) {
        if (!nodes_[i].online || !nodes_[i].direct_connected) {
            continue;
        }
        
        if (memcmp(nodes_[i].id, dest_id, NODE_ID_LEN) == 0) {
            memcpy(next_hop_id, nodes_[i].id, NODE_ID_LEN);
            return 1;
        }
    }
    
    return -1;
}

void MeshNetwork::FloodRoutingUpdate() {
    ESP_LOGD(TAG_MESH_NETWORK, "Flooding routing update to %d neighbors", adjacency_count_);
}

void MeshNetwork::UpdateHopCounts() {
    for (int i = 0; i < node_count_; i++) {
        if (!nodes_[i].online) {
            continue;
        }
        
        if (nodes_[i].direct_connected) {
            nodes_[i].hop_count = 1;
        } else {
            nodes_[i].hop_count = CalculateShortestPath(my_node_id_, nodes_[i].id);
        }
    }
}

int MeshNetwork::CalculateShortestPath(uint8_t *start_id, uint8_t *end_id) {
    if (memcmp(start_id, end_id, NODE_ID_LEN) == 0) {
        return 0;
    }
    
    for (int i = 0; i < node_count_; i++) {
        if (memcmp(nodes_[i].id, end_id, NODE_ID_LEN) == 0 && nodes_[i].direct_connected) {
            return 1;
        }
    }
    
    return MAX_ROUTE_HOPS;
}

bool MeshNetwork::AddNode(const uint8_t *node_id, int8_t rssi) {
    if (node_count_ >= MAX_NODES) {
        ESP_LOGW(TAG_MESH_NETWORK, "Max nodes reached (%d)", MAX_NODES);
        return false;
    }
    
    node_t *new_node = &nodes_[node_count_];
    memcpy(new_node->id, node_id, NODE_ID_LEN);
    new_node->role = NODE_ROLE_SLAVE;
    new_node->network_state = NETWORK_STATE_CONNECTING;
    new_node->online = true;
    new_node->direct_connected = false;
    new_node->authenticated = false;
    new_node->last_heartbeat_ms = 0;
    new_node->last_seen_ms = GetCurrentTimestamp();
    new_node->connect_time_ms = 0;
    new_node->rssi = rssi;
    new_node->avg_rssi = rssi;
    new_node->hop_count = MAX_ROUTE_HOPS;
    
    node_count_++;
    nodes_found_in_last_scan_++;
    
    ESP_LOGI(TAG_MESH_NETWORK, "Added new node: %02X:%02X:%02X:%02X:%02X:%02X (RSSI: %d)",
             node_id[0], node_id[1], node_id[2], node_id[3], node_id[4], node_id[5], rssi);
    
    return true;
}

bool MeshNetwork::UpdateNode(const uint8_t *node_id, int8_t rssi) {
    uint32_t now = GetCurrentTimestamp();
    
    for (int i = 0; i < node_count_; i++) {
        if (memcmp(nodes_[i].id, node_id, NODE_ID_LEN) == 0) {
            nodes_[i].online = true;
            nodes_[i].last_seen_ms = now;
            nodes_[i].rssi = rssi;
            nodes_[i].avg_rssi = (nodes_[i].avg_rssi + rssi) / 2;
            return true;
        }
    }
    return false;
}

void MeshNetwork::RemoveNode(int index) {
    if (index < 0 || index >= node_count_) {
        return;
    }
    
    for (int i = index; i < node_count_ - 1; i++) {
        memcpy(&nodes_[i], &nodes_[i + 1], sizeof(node_t));
    }
    node_count_--;
}

int MeshNetwork::Send(uint8_t *dest_id, const uint8_t *data, size_t len) {
    uint8_t next_hop_id[NODE_ID_LEN];
    int hop_count = FindRoute(dest_id, next_hop_id);
    
    if (hop_count == -1) {
        ESP_LOGW(TAG_MESH_NETWORK, "No route to destination");
        return -1;
    }
    
    if (memcmp(next_hop_id, dest_id, NODE_ID_LEN) == 0) {
        ESP_LOGI(TAG_MESH_NETWORK, "Sending %d bytes directly to node", len);
    } else {
        ESP_LOGI(TAG_MESH_NETWORK, "Routing %d bytes via %02X:%02X:%02X:%02X:%02X:%02X (hop %d)", 
                 len, next_hop_id[0], next_hop_id[1], next_hop_id[2], 
                 next_hop_id[3], next_hop_id[4], next_hop_id[5], hop_count);
    }
    
    return 0;
}

int MeshNetwork::Receive(uint8_t *src_id, uint8_t *data, size_t *len) {
    *len = 0;
    return -1;
}

int MeshNetwork::GetNodeCount() const { 
    return node_count_; 
}

int MeshNetwork::GetOnlineNodeCount() const { 
    int count = 0;
    for (int i = 0; i < node_count_; i++) {
        if (nodes_[i].online) count++;
    }
    return count; 
}

node_t* MeshNetwork::GetNode(int index) { 
    if (index >= node_count_) return nullptr;
    
    int online_count = 0;
    for (int i = 0; i < node_count_; i++) {
        if (nodes_[i].online) {
            if (online_count == index) {
                return &nodes_[i];
            }
            online_count++;
        }
    }
    return nullptr; 
}

node_t* MeshNetwork::FindNode(uint8_t *node_id) {
    for (int i = 0; i < node_count_; i++) {
        if (memcmp(nodes_[i].id, node_id, NODE_ID_LEN) == 0) {
            return &nodes_[i];
        }
    }
    return nullptr;
}

node_role_t MeshNetwork::GetRole() const { return role_; }

network_state_t MeshNetwork::GetState() const { return state_; }

scan_mode_t MeshNetwork::GetScanMode() const { return scan_mode_; }

broadcast_mode_t MeshNetwork::GetBroadcastMode() const { return broadcast_mode_; }

void MeshNetwork::SetRole(node_role_t role) { role_ = role; }

void MeshNetwork::SetState(network_state_t state) { state_ = state; }

void MeshNetwork::SetBroadcastMode(broadcast_mode_t mode) { broadcast_mode_ = mode; }

int MeshNetwork::GetMaxConnections() const { return MAX_DIRECT_CONNECTIONS; }

int MeshNetwork::GetConnectionCount() const {
    int count = 0;
    for (int i = 0; i < node_count_; i++) {
        if (nodes_[i].direct_connected) {
            count++;
        }
    }
    return count;
}

bool MeshNetwork::IsDirectConnected(uint8_t *node_id) {
    for (int i = 0; i < node_count_; i++) {
        if (memcmp(nodes_[i].id, node_id, NODE_ID_LEN) == 0) {
            return nodes_[i].direct_connected;
        }
    }
    return false;
}

int32_t MeshNetwork::GetRssi(uint8_t *node_id) {
    for (int i = 0; i < node_count_; i++) {
        if (memcmp(nodes_[i].id, node_id, NODE_ID_LEN) == 0) {
            return nodes_[i].rssi;
        }
    }
    return 0;
}

int MeshNetwork::SetConnectionPolicy(connection_policy_t policy) {
    ESP_LOGI(TAG_MESH_NETWORK, "Setting connection policy: %d", policy);
    policy_ = policy;
    BuildTopology();
    return 0;
}

connection_policy_t MeshNetwork::GetConnectionPolicy() const { return policy_; }

int MeshNetwork::GetScanInterval() const { return current_scan_interval_ms_; }

int MeshNetwork::GetBroadcastInterval() const { return current_broadcast_interval_ms_; }

bool MeshNetwork::IsScanning() const { return scan_mode_ != SCAN_MODE_IDLE; }

bool MeshNetwork::IsBroadcasting() const { return broadcast_mode_ != BROADCAST_MODE_SILENT; }

void MeshNetwork::TriggerScan() {
    last_scan_time_ms_ = GetCurrentTimestamp() - current_scan_interval_ms_;
}

void MeshNetwork::SendHeartbeat() {
    ESP_LOGD(TAG_MESH_NETWORK, "Sending heartbeat to %d connected nodes", GetConnectionCount());
}

int MeshNetwork::GetHopCount(uint8_t *node_id) {
    for (int i = 0; i < node_count_; i++) {
        if (memcmp(nodes_[i].id, node_id, NODE_ID_LEN) == 0) {
            return nodes_[i].hop_count;
        }
    }
    return MAX_ROUTE_HOPS;
}

int MeshNetwork::GetNetworkDiameter() const {
    int max_hop = 0;
    for (int i = 0; i < node_count_; i++) {
        if (nodes_[i].online && nodes_[i].hop_count > max_hop) {
            max_hop = nodes_[i].hop_count;
        }
    }
    return max_hop;
}

void MeshNetwork::PrintTopology() {
    ESP_LOGI(TAG_MESH_NETWORK, "=== Network Topology ===");
    ESP_LOGI(TAG_MESH_NETWORK, "My ID: %02X:%02X:%02X:%02X:%02X:%02X", 
             my_node_id_[0], my_node_id_[1], my_node_id_[2], 
             my_node_id_[3], my_node_id_[4], my_node_id_[5]);
    ESP_LOGI(TAG_MESH_NETWORK, "Role: %d, Connections: %d/%d", role_, GetConnectionCount(), MAX_DIRECT_CONNECTIONS);
    ESP_LOGI(TAG_MESH_NETWORK, "Policy: %d, Diameter: %d", policy_, GetNetworkDiameter());
    
    ESP_LOGI(TAG_MESH_NETWORK, "--- Adjacency Table ---");
    for (int i = 0; i < adjacency_count_; i++) {
        ESP_LOGI(TAG_MESH_NETWORK, "[%d] %02X:%02X:%02X:%02X:%02X:%02X, RSSI: %d, Quality: %d%%", 
                 i, adjacency_table_[i].neighbor_id[0], adjacency_table_[i].neighbor_id[1],
                 adjacency_table_[i].neighbor_id[2], adjacency_table_[i].neighbor_id[3],
                 adjacency_table_[i].neighbor_id[4], adjacency_table_[i].neighbor_id[5],
                 adjacency_table_[i].rssi, adjacency_table_[i].link_quality);
    }
    
    ESP_LOGI(TAG_MESH_NETWORK, "--- Routing Table ---");
    for (int i = 0; i < routing_table_size_; i++) {
        ESP_LOGI(TAG_MESH_NETWORK, "[%d] Dest: %02X:%02X:%02X:%02X:%02X:%02X, Next: %02X:%02X:%02X:%02X:%02X:%02X, Hops: %d", 
                 i, routing_table_[i].dest_id[0], routing_table_[i].dest_id[1],
                 routing_table_[i].dest_id[2], routing_table_[i].dest_id[3],
                 routing_table_[i].dest_id[4], routing_table_[i].dest_id[5],
                 routing_table_[i].next_hop_id[0], routing_table_[i].next_hop_id[1],
                 routing_table_[i].next_hop_id[2], routing_table_[i].next_hop_id[3],
                 routing_table_[i].next_hop_id[4], routing_table_[i].next_hop_id[5],
                 routing_table_[i].hop_count);
    }
    
    ESP_LOGI(TAG_MESH_NETWORK, "--- All Nodes ---");
    for (int i = 0; i < node_count_; i++) {
        ESP_LOGI(TAG_MESH_NETWORK, "[%d] %02X:%02X:%02X:%02X:%02X:%02X, Role: %d, Online: %d, Connected: %d, RSSI: %d, Hops: %d", 
                 i, nodes_[i].id[0], nodes_[i].id[1], nodes_[i].id[2],
                 nodes_[i].id[3], nodes_[i].id[4], nodes_[i].id[5],
                 nodes_[i].role, nodes_[i].online, nodes_[i].direct_connected,
                 nodes_[i].rssi, nodes_[i].hop_count);
    }
}

connection_policy_t MeshNetwork::AutoSelectPolicy() {
    int node_count = GetOnlineNodeCount();
    
    if (node_count <= 1) {
        ESP_LOGI(TAG_MESH_NETWORK, "Auto policy: only %d node(s), using HYBRID", node_count);
        return CONNECTION_POLICY_HYBRID;
    }
    
    int density = CalculateNetworkDensity();
    int avg_rssi = CalculateAverageRssi();
    int spread = CalculateNetworkSpread();
    
    ESP_LOGI(TAG_MESH_NETWORK, "Auto policy metrics - Density: %d%%, Avg RSSI: %d dBm, Spread: %d", 
             density, avg_rssi, spread);
    
    if (node_count <= 3) {
        ESP_LOGI(TAG_MESH_NETWORK, "Auto policy: few nodes (%d), using FULL_MESH for reliability", node_count);
        return CONNECTION_POLICY_FULL_MESH;
    }
    
    if (node_count <= MAX_DIRECT_CONNECTIONS && density >= 80 && avg_rssi >= -65) {
        ESP_LOGI(TAG_MESH_NETWORK, "Auto policy: dense network with strong signals, using FULL_MESH");
        return CONNECTION_POLICY_FULL_MESH;
    }
    
    if (spread >= MAX_ROUTE_HOPS - 1 || avg_rssi < -85) {
        ESP_LOGI(TAG_MESH_NETWORK, "Auto policy: sparse/weak network, using STAR");
        return CONNECTION_POLICY_STAR;
    }
    
    if (density >= 50 && avg_rssi >= -75) {
        ESP_LOGI(TAG_MESH_NETWORK, "Auto policy: moderate density with good signals, using HYBRID");
        return CONNECTION_POLICY_HYBRID;
    }
    
    ESP_LOGI(TAG_MESH_NETWORK, "Auto policy: defaulting to HYBRID");
    return CONNECTION_POLICY_HYBRID;
}

int MeshNetwork::CalculateNetworkDensity() {
    int online_nodes = GetOnlineNodeCount();
    
    if (online_nodes <= 1) {
        return 0;
    }
    
    int total_possible = online_nodes - 1;
    int actual_connections = GetConnectionCount();
    
    if (total_possible == 0) {
        return 0;
    }
    
    return (actual_connections * 100) / total_possible;
}

int MeshNetwork::CalculateAverageRssi() {
    int online_nodes = GetOnlineNodeCount();
    
    if (online_nodes == 0) {
        return -128;
    }
    
    int sum_rssi = 0;
    int count = 0;
    
    for (int i = 0; i < node_count_; i++) {
        if (nodes_[i].online) {
            sum_rssi += nodes_[i].rssi;
            count++;
        }
    }
    
    if (count == 0) {
        return -128;
    }
    
    return sum_rssi / count;
}

int MeshNetwork::CalculateNetworkSpread() {
    return GetNetworkDiameter();
}