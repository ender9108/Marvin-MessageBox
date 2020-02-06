#pragma once

#include <Arduino.h>
#include "ESPTools.h"
#ifndef ARDUINOJSON_DECODE_UNICODE
    #define ARDUINOJSON_DECODE_UNICODE 1
#endif
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
    bool mqttEnable                 = true;
    char mqttHost[128]              = "";
    int  mqttPort                   = 1883;
    char mqttUsername[32]           = "";
    char mqttPassword[64]           = "";
    char mqttPublishChannel[128]    = "device/to/marvin";
    char mqttSubscribeChannel[128]  = "marvin/to/device";
    char telegramBotToken[128]      = "";
    char uuid[64]                   = "";
};

bool getConfig(const char *configPath, Config &config);
bool setConfig(const char *configPath, Config newConfig);
void resetConfig(const char *configPath);