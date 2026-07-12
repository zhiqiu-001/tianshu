#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <string>
#include <esp_log.h>
#include <esp_err.h>

#define TAG_WEB_SERVER "WebServer"

typedef std::string (*web_data_callback_t)(void);

typedef struct httpd_req httpd_req_t;

class EspHttpWebServer {
public:
    EspHttpWebServer();
    ~EspHttpWebServer();
    
    int Init();
    int Start();
    int Stop();
    bool IsRunning() const;
    
    void RegisterDataCallback(const std::string& endpoint, web_data_callback_t callback);

private:
    void* server_;
    bool running_;
    
    web_data_callback_t status_callback_;
    web_data_callback_t device_callback_;
    
    friend esp_err_t api_status_handler(httpd_req_t *req);
    friend esp_err_t api_device_handler(httpd_req_t *req);
};

#endif
