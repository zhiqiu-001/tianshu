#ifndef HOST_ELECTION_H
#define HOST_ELECTION_H

#include "mesh_network.h"
#include <esp_log.h>

#define TAG_HOST_ELECTION "HostElection"

class HostElection {
public:
    HostElection() : mesh_(nullptr), role_(NODE_ROLE_SLAVE) {}
    
    int Init(MeshNetwork* mesh) {
        ESP_LOGI(TAG_HOST_ELECTION, "Initializing host election...");
        mesh_ = mesh;
        role_ = NODE_ROLE_SLAVE;
        return 0;
    }
    
    int Run() {
        if (!mesh_ || mesh_->GetNodeCount() == 0) {
            role_ = NODE_ROLE_MASTER;
        }
        return 0;
    }
    
    node_role_t GetRole() const { return role_; }
    
    bool IsMaster() const { return role_ == NODE_ROLE_MASTER; }

private:
    MeshNetwork* mesh_;
    node_role_t role_;
};

class NoHostElection : public HostElection {
public:
    int Init(MeshNetwork* mesh) { (void)mesh; return 0; }
    int Run() { return 0; }
    node_role_t GetRole() const { return NODE_ROLE_SLAVE; }
    bool IsMaster() const { return false; }
};

#endif