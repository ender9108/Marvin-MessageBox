#pragma once

#include <Arduino.h>
#include "ESPTools.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>

/* General */
#define SERIAL_BAUDRATE 115200

struct Config {
    char wifiSsid[32]               = "";
    char wifiPassword[64]           = "";
    char mqttHost[128]              = "";
    char mqttPort[6]                = "1883";
    char mqttUsername[32]           = "";
    char mqttPassword[64]           = "";
    char mqttPublishChannel[128]    = "device/to/marvin";
    char mqttSubscribeChannel[128]  = "marvin/to/device";
    char uuid[64]                   = "";
};

bool getConfig(const char *configPath, Config config);
bool setConfig(const char *configPath, Config newConfig);
void resetConfig(const char *configPath);