#pragma once

#include <ESPTools.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

/* Conditional compilation */
#define MQTT_ENABLE     false
#define OTA_ENABLE      false
#define SCREEN_TYPE     oled

/* General */
#define SERIAL_BAUDRATE 115200

struct Config {
    char wifiSsid[32]               = "";
    char wifiPassword[64]           = "";
    char mqttHost[128]              = "";
    int  mqttPort                   = 1883;
    char mqttUsername[32]           = "";
    char mqttPassword[64]           = "";
    char mqttPublishChannel[128]    = "device/to/marvin";
    char mqttSubscribeChannel[128]  = "marvin/to/device";
    char uuid[64]                   = "";
};

bool getConfig(char *configPath);
bool setConfig(char *configPath, Config newConfig);