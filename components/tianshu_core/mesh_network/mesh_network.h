#ifndef MESH_NETWORK_H
#define MESH_NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <esp_log.h>

#define NODE_ID_LEN 6
#define MAX_NODES 32
#define MAX_DIRECT_CONNECTIONS 8
#define MAX_ROUTE_HOPS 8
#define BEACON_DATA_LEN 31

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
    CONNECTION_POLICY_MESH_ONLY,
    CONNECTION_POLICY_AUTO
} connection_policy_t;

typedef enum {
    SCAN_MODE_IDLE = 0,
    SCAN_MODE_ACTIVE,
    SCAN_MODE_PASSIVE,
    SCAN_MODE_LOW_POWER
} scan_mode_t;

typedef enum {
    BROADCAST_MODE_SILENT = 0,
    BROADCAST_MODE_ADVERTISING,
    BROADCAST_MODE_LOW_POWER,
    BROADCAST_MODE_BEACON_ONLY
} broadcast_mode_t;

typedef struct {
    uint8_t id[NODE_ID_LEN];
    node_role_t role;
    network_state_t network_state;
    bool online;
    bool direct_connected;
    bool authenticated;
    int32_t last_heartbeat_ms;
    int32_t last_seen_ms;
    int32_t connect_time_ms;
    int8_t rssi;
    int8_t avg_rssi;
    int8_t hop_count;
} node_t;

typedef struct {
    uint8_t dest_id[NODE_ID_LEN];
    uint8_t next_hop_id[NODE_ID_LEN];
    int hop_count;
    int rssi;
    uint32_t last_update_ms;
} route_entry_t;

typedef struct {
    uint8_t neighbor_id[NODE_ID_LEN];
    int8_t rssi;
    int link_quality;
    uint32_t last_comm_ms;
} adjacency_entry_t;

typedef struct {
    uint8_t magic[2];
    uint8_t version;
    uint8_t node_id[NODE_ID_LEN];
    node_role_t role;
    network_state_t network_state;
    uint8_t node_count;
    uint8_t connection_count;
    uint8_t hop_count;
    uint8_t reserved[17];
} beacon_data_t;

class MeshNetwork {
public:
    MeshNetwork();
    virtual ~MeshNetwork() = default;
    
    virtual int Init();
    virtual int Join();
    virtual int Leave();
    virtual int Run();
    
    virtual int Send(uint8_t *dest_id, const uint8_t *data, size_t len);
    virtual int Receive(uint8_t *src_id, uint8_t *data, size_t *len);
    
    virtual int GetNodeCount() const;
    virtual int GetOnlineNodeCount() const;
    virtual node_t* GetNode(int index);
    virtual node_t* FindNode(uint8_t *node_id);
    
    virtual node_role_t GetRole() const;
    virtual network_state_t GetState() const;
    virtual scan_mode_t GetScanMode() const;
    virtual broadcast_mode_t GetBroadcastMode() const;
    
    virtual void SetRole(node_role_t role);
    virtual void SetState(network_state_t state);
    virtual void SetBroadcastMode(broadcast_mode_t mode);
    
    virtual int GetMaxConnections() const;
    virtual int GetConnectionCount() const;
    
    virtual bool IsDirectConnected(uint8_t *node_id);
    virtual int32_t GetRssi(uint8_t *node_id);
    
    virtual int SetConnectionPolicy(connection_policy_t policy);
    virtual connection_policy_t GetConnectionPolicy() const;
    
    virtual int GetScanInterval() const;
    virtual int GetBroadcastInterval() const;
    virtual bool IsScanning() const;
    virtual bool IsBroadcasting() const;
    
    virtual void TriggerScan();
    virtual void SendHeartbeat();
    
    virtual int GetHopCount(uint8_t *node_id);
    virtual int GetNetworkDiameter() const;
    
    virtual void PrintTopology();

private:
    int StartBroadcast();
    int StopBroadcast();
    int UpdateBroadcastData();
    int ProcessBeacon(const uint8_t *data, size_t len, int8_t rssi);
    
    int StartScan();
    int StopScan();
    int PerformScan();
    int ProcessScanResults();
    
    int ConnectToNode(node_t *node);
    int DisconnectFromNode(node_t *node);
    int EstablishConnections();
    int PruneConnections();
    
    void UpdateHeartbeats();
    void DetectOfflineNodes();
    void DetectWeakConnections();
    
    void AdjustScanInterval();
    void AdjustBroadcastInterval();
    
    bool AddNode(const uint8_t *node_id, int8_t rssi);
    bool UpdateNode(const uint8_t *node_id, int8_t rssi);
    void RemoveNode(int index);
    
    void BuildTopology();
    void UpdateRoutingTable();
    int FindRoute(uint8_t *dest_id, uint8_t *next_hop_id);
    void FloodRoutingUpdate();
    
    void ApplyConnectionPolicy();
    void BuildFullMeshTopology();
    void BuildStarTopology();
    void BuildHybridTopology();
    
    void UpdateHopCounts();
    int CalculateShortestPath(uint8_t *start_id, uint8_t *end_id);
    
    connection_policy_t AutoSelectPolicy();
    int CalculateNetworkDensity();
    int CalculateAverageRssi();
    int CalculateNetworkSpread();
    
    node_t nodes_[MAX_NODES];
    int node_count_;
    node_role_t role_;
    network_state_t state_;
    connection_policy_t policy_;
    
    scan_mode_t scan_mode_;
    broadcast_mode_t broadcast_mode_;
    
    uint32_t last_scan_time_ms_;
    uint32_t last_broadcast_time_ms_;
    uint32_t last_heartbeat_time_ms_;
    uint32_t last_topology_update_ms_;
    
    uint32_t current_scan_interval_ms_;
    uint32_t current_broadcast_interval_ms_;
    
    int consecutive_stable_scans_;
    int nodes_found_in_last_scan_;
    int connection_attempts_;
    
    beacon_data_t beacon_data_;
    uint8_t my_node_id_[NODE_ID_LEN];
    
    adjacency_entry_t adjacency_table_[MAX_DIRECT_CONNECTIONS];
    int adjacency_count_;
    
    route_entry_t routing_table_[MAX_NODES];
    int routing_table_size_;
};

#endif