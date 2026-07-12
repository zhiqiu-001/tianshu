#include "esp_http_server.h"
#include "web_server.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>

#define TAG_WEB_SERVER_IMPL "WebServerImpl"

static const char *WEB_MOUNT_POINT = "/web";

static EspHttpWebServer* g_web_server = nullptr;

static esp_err_t index_html_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    
    char index_path[32];
    snprintf(index_path, sizeof(index_path), "%s/index.html", WEB_MOUNT_POINT);
    FILE *file = fopen(index_path, "r");
    if (!file) {
        httpd_resp_sendstr(req, "<html><body><h1>File not found</h1></body></html>");
        return ESP_OK;
    }
    
    char buffer[512];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        httpd_resp_send_chunk(req, buffer, bytes_read);
    }
    httpd_resp_send_chunk(req, NULL, 0);
    fclose(file);
    
    return ESP_OK;
}

static esp_err_t static_file_handler(httpd_req_t *req)
{
    const char *uri = req->uri;
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s%s", WEB_MOUNT_POINT, uri);
    
    FILE *file = fopen(filepath, "r");
    if (!file) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_sendstr(req, "File not found");
        return ESP_OK;
    }
    
    const char *ext = strrchr(uri, '.');
    if (ext) {
        if (strcmp(ext, ".css") == 0) {
            httpd_resp_set_type(req, "text/css");
        } else if (strcmp(ext, ".js") == 0) {
            httpd_resp_set_type(req, "application/javascript");
        } else if (strcmp(ext, ".html") == 0) {
            httpd_resp_set_type(req, "text/html");
        } else if (strcmp(ext, ".json") == 0) {
            httpd_resp_set_type(req, "application/json");
        } else if (strcmp(ext, ".png") == 0) {
            httpd_resp_set_type(req, "image/png");
        } else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
            httpd_resp_set_type(req, "image/jpeg");
        } else if (strcmp(ext, ".svg") == 0) {
            httpd_resp_set_type(req, "image/svg+xml");
        }
    }
    
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");
    
    char buffer[512];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        httpd_resp_send_chunk(req, buffer, bytes_read);
    }
    httpd_resp_send_chunk(req, NULL, 0);
    fclose(file);
    
    return ESP_OK;
}

esp_err_t api_status_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    
    if (g_web_server && g_web_server->status_callback_) {
        std::string json = g_web_server->status_callback_();
        httpd_resp_sendstr(req, json.c_str());
    } else {
        httpd_resp_sendstr(req, "{\"error\":\"callback not registered\"}");
    }
    
    return ESP_OK;
}

esp_err_t api_device_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    
    if (g_web_server && g_web_server->device_callback_) {
        std::string json = g_web_server->device_callback_();
        httpd_resp_sendstr(req, json.c_str());
    } else {
        httpd_resp_sendstr(req, "{\"error\":\"callback not registered\"}");
    }
    
    return ESP_OK;
}

static esp_err_t api_wifi_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    
    std::string json = "{\"status\":\"not implemented\"}";
    httpd_resp_sendstr(req, json.c_str());
    return ESP_OK;
}

static const httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_html_handler,
    .user_ctx = NULL
};

static const httpd_uri_t static_uri = {
    .uri = "/static/*",
    .method = HTTP_GET,
    .handler = static_file_handler,
    .user_ctx = NULL
};

static const httpd_uri_t api_status_uri = {
    .uri = "/api/status",
    .method = HTTP_GET,
    .handler = api_status_handler,
    .user_ctx = NULL
};

static const httpd_uri_t api_device_uri = {
    .uri = "/api/device",
    .method = HTTP_GET,
    .handler = api_device_handler,
    .user_ctx = NULL
};

static const httpd_uri_t api_wifi_uri = {
    .uri = "/api/wifi",
    .method = HTTP_GET,
    .handler = api_wifi_handler,
    .user_ctx = NULL
};

EspHttpWebServer::EspHttpWebServer() 
    : server_(NULL), 
      running_(false),
      status_callback_(nullptr),
      device_callback_(nullptr) {
    g_web_server = this;
}

EspHttpWebServer::~EspHttpWebServer() {
    Stop();
    g_web_server = nullptr;
}

void EspHttpWebServer::RegisterDataCallback(const std::string& endpoint, web_data_callback_t callback) {
    if (endpoint == "/api/status") {
        status_callback_ = callback;
    } else if (endpoint == "/api/device") {
        device_callback_ = callback;
    }
}

int EspHttpWebServer::Init() {
    ESP_LOGI(TAG_WEB_SERVER_IMPL, "Initializing web server...");
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = WEB_MOUNT_POINT,
        .partition_label = "web",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_WEB_SERVER_IMPL, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return -1;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_WEB_SERVER_IMPL, "Failed to get SPIFFS info (%s)", esp_err_to_name(ret));
        esp_vfs_spiffs_unregister(conf.partition_label);
        return -1;
    }
    
    ESP_LOGI(TAG_WEB_SERVER_IMPL, "SPIFFS mounted: %d/%d bytes used", used, total);
    
    return 0;
}

int EspHttpWebServer::Start() {
    if (running_) {
        ESP_LOGW(TAG_WEB_SERVER_IMPL, "Web server already running");
        return 0;
    }
    
    ESP_LOGI(TAG_WEB_SERVER_IMPL, "Starting web server...");
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CONFIG_TIANSHU_WEB_SERVICE_PORT;
    config.max_open_sockets = 4;
    
    httpd_handle_t handle = NULL;
    esp_err_t ret = httpd_start(&handle, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_WEB_SERVER_IMPL, "Failed to start HTTP server (%s)", esp_err_to_name(ret));
        return -1;
    }
    
    server_ = (void*)handle;
    
    httpd_register_uri_handler((httpd_handle_t)server_, &index_uri);
    httpd_register_uri_handler((httpd_handle_t)server_, &static_uri);
    httpd_register_uri_handler((httpd_handle_t)server_, &api_status_uri);
    httpd_register_uri_handler((httpd_handle_t)server_, &api_device_uri);
    httpd_register_uri_handler((httpd_handle_t)server_, &api_wifi_uri);
    
    running_ = true;
    ESP_LOGI(TAG_WEB_SERVER_IMPL, "Web server started on port %d", config.server_port);
    
    return 0;
}

int EspHttpWebServer::Stop() {
    if (!running_) {
        ESP_LOGW(TAG_WEB_SERVER_IMPL, "Web server not running");
        return 0;
    }
    
    ESP_LOGI(TAG_WEB_SERVER_IMPL, "Stopping web server...");
    
    if (server_) {
        httpd_stop((httpd_handle_t)server_);
        server_ = NULL;
    }
    
    esp_vfs_spiffs_unregister("web");
    running_ = false;
    
    ESP_LOGI(TAG_WEB_SERVER_IMPL, "Web server stopped");
    
    return 0;
}

bool EspHttpWebServer::IsRunning() const {
    return running_;
}