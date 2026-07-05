#ifndef DISPLAY_H
#define DISPLAY_H

#include <esp_log.h>
#include <string>

#define TAG_DISPLAY "Display"

class Theme {
public:
    Theme(const std::string& name) : name_(name) {}
    ~Theme() = default;

    inline std::string name() const { return name_; }
private:
    std::string name_;
};

class Display {
public:
    Display() : width_(0), height_(0), setup_ui_called_(false), current_theme_(nullptr) {}
    ~Display() = default;

    void SetStatus(const char* status) {
        ESP_LOGW(TAG_DISPLAY, "SetStatus: %s", status);
    }
    
    void ShowNotification(const char* notification, int duration_ms = 3000) {
        ESP_LOGW(TAG_DISPLAY, "ShowNotification: %s", notification);
    }
    
    void ShowNotification(const std::string &notification, int duration_ms = 3000) {
        ShowNotification(notification.c_str(), duration_ms);
    }
    
    void SetEmotion(const char* emotion) {
        ESP_LOGW(TAG_DISPLAY, "SetEmotion: %s", emotion);
    }
    
    void SetChatMessage(const char* role, const char* content) {
        ESP_LOGW(TAG_DISPLAY, "Role:%s", role);
        ESP_LOGW(TAG_DISPLAY, "     %s", content);
    }
    
    void ClearChatMessages() {}
    
    void SetTheme(Theme* theme) {
        current_theme_ = theme;
    }
    
    Theme* GetTheme() { return current_theme_; }
    
    void UpdateStatusBar(bool update_all = false) {}
    
    void SetPowerSaveMode(bool on) {
        ESP_LOGW(TAG_DISPLAY, "SetPowerSaveMode: %d", on);
    }
    
    void SetupUI() { 
        setup_ui_called_ = true;
    }

    inline int width() const { return width_; }
    inline int height() const { return height_; }
    inline bool IsSetupUICalled() const { return setup_ui_called_; }

protected:
    int width_;
    int height_;
    bool setup_ui_called_;
    Theme* current_theme_;

    friend class DisplayLockGuard;
    virtual bool Lock(int timeout_ms = 0) { return true; }
    virtual void Unlock() {}
};

class DisplayLockGuard {
public:
    DisplayLockGuard(Display *display) : display_(display) {
        if (!display_->Lock(30000)) {
            ESP_LOGE(TAG_DISPLAY, "Failed to lock display");
        }
    }
    ~DisplayLockGuard() {
        display_->Unlock();
    }

private:
    Display *display_;
};

class NoDisplay : public Display {
private:
    bool Lock(int timeout_ms = 0) override {
        return true;
    }
    void Unlock() override {}
};

#endif