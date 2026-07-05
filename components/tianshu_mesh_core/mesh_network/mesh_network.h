#ifndef MESH_NETWORK_H
#define MESH_NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <esp_log.h>

#define NODE_ID_LEN 6
#define MAX_NODES 32
#define MAX_DIRECT_CONNECTIONS 8

#define TAG_MESH_NETWORK "MeshNetwork"

typedef enum {
    NODE_ROLE_MASTER = 0,
    NODE_ROLE_SLAVE,
    NODE_ROLE_STANDBY
} node_role_t;

typedef enum {
    NETWORK_STATE_DISCONNECTED = 0,
    NETWORK_STATE_CONNECTING,
    NETWORK_STATE_CONNECTED,
    NETWORK_STATE_AP_MODE
} network_state_t;

typedef enum {
    CONNECTION_POLICY_FULL_MESH = 0,
    CONNECTION_POLICY_HYBRID,
    CONNECTION_POLICY_STAR,
    CONNECTION_POLICY_MESH_ONLY
} connection_policy_t;

typedef struct {
    uint8_t id[NODE_ID_LEN];
    node_role_t role;
    network_state_t network_state;
    bool online;
    bool direct_connected;
    int32_t last_heartbeat;
    int8_t rssi;
} node_t;

class MeshNetwork {
public:
    MeshNetwork() : node_count_(0), role_(NODE_ROLE_SLAVE), state_(NETWORK_STATE_DISCONNECTED), policy_(CONNECTION_POLICY_HYBRID) {
        memset(nodes_, 0, sizeof(nodes_));
    }
    
    int Init() {
        ESP_LOGI(TAG_MESH_NETWORK, "Initializing BLE Mesh...");
        return 0;
    }
    
    int Join() {
        ESP_LOGI(TAG_MESH_NETWORK, "Joining BLE Mesh network...");
        state_ = NETWORK_STATE_CONNECTING;
        return 0;
    }
    
    int Leave() {
        ESP_LOGI(TAG_MESH_NETWORK, "Leaving BLE Mesh network...");
        state_ = NETWORK_STATE_DISCONNECTED;
        return 0;
    }
    
    int Send(uint8_t *dest_id, const uint8_t *data, size_t len) {
        ESP_LOGI(TAG_MESH_NETWORK, "Sending %d bytes to mesh node", len);
        return 0;
    }
    
    int Receive(uint8_t *src_id, uint8_t *data, size_t *len) {
        *len = 0;
        return -1;
    }
    
    int GetNodeCount() const { return node_count_; }
    
    node_t* GetNode(int index) { 
        return (index < node_count_) ? &nodes_[index] : nullptr; 
    }
    
    node_role_t GetRole() const { return role_; }
    
    network_state_t GetState() const { return state_; }
    
    void SetRole(node_role_t role) { role_ = role; }
    
    void SetState(network_state_t state) { state_ = state; }
    
    int GetMaxConnections() const { return MAX_DIRECT_CONNECTIONS; }
    
    int GetConnectionCount() const {
        int count = 0;
        for (int i = 0; i < node_count_; i++) {
            if (nodes_[i].direct_connected) {
                count++;
            }
        }
        return count;
    }
    
    bool IsDirectConnected(uint8_t *node_id) {
        for (int i = 0; i < node_count_; i++) {
            if (memcmp(nodes_[i].id, node_id, NODE_ID_LEN) == 0) {
                return nodes_[i].direct_connected;
            }
        }
        return false;
    }
    
    int32_t GetRssi(uint8_t *node_id) {
        for (int i = 0; i < node_count_; i++) {
            if (memcmp(nodes_[i].id, node_id, NODE_ID_LEN) == 0) {
                return nodes_[i].rssi;
            }
        }
        return 0;
    }
    
    int SetConnectionPolicy(connection_policy_t policy) {
        ESP_LOGI(TAG_MESH_NETWORK, "Setting connection policy: %d", policy);
        policy_ = policy;
        return 0;
    }
    
    connection_policy_t GetConnectionPolicy() const { return policy_; }

private:
    node_t nodes_[MAX_NODES];
    int node_count_;
    node_role_t role_;
    network_state_t state_;
    connection_policy_t policy_;
};

class NoMeshNetwork : public MeshNetwork {
public:
    int Init() { return 0; }
    int Join() { return 0; }
    int Leave() { return 0; }
    int Send(uint8_t *dest_id, const uint8_t *data, size_t len) { (void)dest_id; (void)data; (void)len; return -1; }
    int Receive(uint8_t *src_id, uint8_t *data, size_t *len) { (void)src_id; (void)data; *len = 0; return -1; }
    int GetNodeCount() const { return 0; }
    node_t* GetNode(int index) { (void)index; return nullptr; }
    node_role_t GetRole() const { return NODE_ROLE_SLAVE; }
    network_state_t GetState() const { return NETWORK_STATE_DISCONNECTED; }
    int GetMaxConnections() const { return 0; }
    int GetConnectionCount() const { return 0; }
    bool IsDirectConnected(uint8_t *node_id) { (void)node_id; return false; }
    int32_t GetRssi(uint8_t *node_id) { (void)node_id; return 0; }
    int SetConnectionPolicy(connection_policy_t policy) { (void)policy; return 0; }
    connection_policy_t GetConnectionPolicy() const { return CONNECTION_POLICY_HYBRID; }
};

#endif