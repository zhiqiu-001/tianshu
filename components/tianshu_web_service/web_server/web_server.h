#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <string>
#include <esp_log.h>

#define TAG_WEB_SERVER "WebServer"

class WebServer {
public:
    WebServer() : running_(false) {}
    
    int Init() {
        ESP_LOGI(TAG_WEB_SERVER, "Initializing web server...");
        return 0;
    }
    
    int Start() {
        ESP_LOGI(TAG_WEB_SERVER, "Starting web server...");
        running_ = true;
        return 0;
    }
    
    int Stop() {
        ESP_LOGI(TAG_WEB_SERVER, "Stopping web server...");
        running_ = false;
        return 0;
    }
    
    bool IsRunning() const { return running_; }
    
    int RegisterEndpoint(const std::string& path, 
                         const std::string& method,
                         void (*handler)(void*)) {
        (void)path; (void)method; (void)handler;
        return 0;
    }

private:
    bool running_;
};

class NoWebServer : public WebServer {
public:
    int Init() { return 0; }
    int Start() { return 0; }
    int Stop() { return 0; }
    bool IsRunning() const { return false; }
    int RegisterEndpoint(const std::string& path, 
                         const std::string& method,
                         void (*handler)(void*)) { 
        (void)path; (void)method; (void)handler; 
        return 0; 
    }
};

#endif