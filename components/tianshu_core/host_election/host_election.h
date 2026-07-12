#ifndef HOST_ELECTION_H
#define HOST_ELECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_log.h>
#include "mesh_network.h"

enum class ElectionState {
    WAITING = 0,
    STABILIZING,
    ELECTING,
    STABLE
};

class HostElection {
public:
    HostElection();
    
    virtual int Init(MeshNetwork* mesh);
    virtual int Run();
    
    virtual node_role_t GetRole() const;
    virtual bool IsMaster() const;
    virtual ElectionState GetElectionState() const;
    virtual bool IsElectionComplete() const;
    virtual int GetElectionProgress() const;

private:
    void PerformElection();
    void MonitorMasterHealth();
    
    MeshNetwork* mesh_;
    node_role_t role_;
    ElectionState election_state_;
    
    uint32_t wait_start_ms_;
    uint32_t stable_since_ms_;
    int last_node_count_;
    int consecutive_stable_checks_;
    
    const uint32_t WAIT_TIMEOUT_MS = 30000;
    const uint32_t STABLE_THRESHOLD_MS = 10000;
    const uint32_t STABLE_CHECK_THRESHOLD = 3;
    const uint32_t ELECTION_TIMEOUT_MS = 5000;
    const uint32_t HEALTH_CHECK_INTERVAL_MS = 5000;
    const uint32_t MASTER_TIMEOUT_MS = 15000;
};

#endif