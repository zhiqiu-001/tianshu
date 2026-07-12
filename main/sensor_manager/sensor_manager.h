#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <esp_log.h>

#define MAX_SENSORS 16
#define SENSOR_ID_LEN 4
#define MAX_SENSOR_VALUE_LEN 32

#define TAG_SENSOR_MANAGER "SensorManager"

typedef enum {
    SENSOR_TYPE_UNKNOWN = 0,
    SENSOR_TYPE_TEMPERATURE,
    SENSOR_TYPE_HUMIDITY,
    SENSOR_TYPE_PRESSURE,
    SENSOR_TYPE_LIGHT,
    SENSOR_TYPE_SOIL_MOISTURE,
    SENSOR_TYPE_CO2,
    SENSOR_TYPE_FORMALDEHYDE,
    SENSOR_TYPE_CURRENT,
    SENSOR_TYPE_VOLTAGE,
    SENSOR_TYPE_POWER,
    SENSOR_TYPE_VIBRATION,
    SENSOR_TYPE_INCLINATION,
    SENSOR_TYPE_RELAY,
    SENSOR_TYPE_SOLENOID_VALVE,
    SENSOR_TYPE_FAN,
    SENSOR_TYPE_LIGHTING,
    SENSOR_TYPE_ALARM
} sensor_type_t;

typedef struct {
    uint8_t id[SENSOR_ID_LEN];
    sensor_type_t type;
    char name[32];
    bool online;
    float value;
    char unit[8];
    int32_t last_update;
} sensor_t;

class SensorManager {
public:
    SensorManager() : sensor_count_(0) {
        memset(sensors_, 0, sizeof(sensors_));
    }
    virtual ~SensorManager() = default;
    
    virtual int Scan() {
        ESP_LOGI(TAG_SENSOR_MANAGER, "Scanning for sensors...");
        sensor_count_ = 0;
        return 0;
    }
    
    virtual int Read(uint8_t *sensor_id, float *value) {
        for (int i = 0; i < sensor_count_; i++) {
            if (memcmp(sensors_[i].id, sensor_id, SENSOR_ID_LEN) == 0) {
                *value = sensors_[i].value;
                sensors_[i].last_update++;
                return 0;
            }
        }
        return -1;
    }
    
    virtual int Write(uint8_t *sensor_id, float value) {
        for (int i = 0; i < sensor_count_; i++) {
            if (memcmp(sensors_[i].id, sensor_id, SENSOR_ID_LEN) == 0) {
                sensors_[i].value = value;
                sensors_[i].last_update++;
                return 0;
            }
        }
        return -1;
    }
    
    virtual int GetInfo(uint8_t *sensor_id, sensor_t *info) {
        for (int i = 0; i < sensor_count_; i++) {
            if (memcmp(sensors_[i].id, sensor_id, SENSOR_ID_LEN) == 0) {
                memcpy(info, &sensors_[i], sizeof(sensor_t));
                return 0;
            }
        }
        return -1;
    }
    
    virtual int GetSensorCount() const { return sensor_count_; }
    
    virtual sensor_t* GetSensor(int index) { 
        return (index < sensor_count_) ? &sensors_[index] : nullptr; 
    }

private:
    sensor_t sensors_[MAX_SENSORS];
    int sensor_count_;
};

class NoSensorManager : public SensorManager {
public:
    int Scan() override { return 0; }
    int Read(uint8_t *sensor_id, float *value) override { (void)sensor_id; (void)value; return -1; }
    int Write(uint8_t *sensor_id, float value) override { (void)sensor_id; (void)value; return -1; }
    int GetInfo(uint8_t *sensor_id, sensor_t *info) override { (void)sensor_id; (void)info; return -1; }
    int GetSensorCount() const override { return 0; }
    sensor_t* GetSensor(int index) override { (void)index; return nullptr; }
};

#endif