#ifndef APPLICATION_H
#define APPLICATION_H

#include <string>
#include "boards/common/board.h"

enum class AppState {
    INIT = 0,
    SENSOR_SCAN,
    NETWORK_INIT,
    MESH_INIT,
    HOST_ELECTION,
    RUNNING,
    ERROR
};

class Application {
private:
    Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

public:
    static Application& GetInstance() {
        static Application instance;
        return instance;
    }

    int Initialize();
    int Deinitialize();
    void Run();

    AppState GetState() const;
    Board* GetBoard() const;

private:
    int Transition(AppState next_state);

    AppState state_;
    bool initialized_;
};

#endif