#pragma once

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <UniversalTelegramBot.h>
#include <Ticker.h>
#include <ESPTools.h>
#include <ESPConfig.h>
#include <WiFiClientSecure.h>

/* Conditional compilation */
#define MQTT_ENABLE     false
#define OTA_ENABLE      false

/* General */
#define SERIAL_BAUDRATE 115200
#define DURATION_BEFORE_RESET 5000
#define DURATION_BEFORE_RESTART 5000
#define DURATION_BTN_RESET_PRESS 10000

/* Telegram */
#define CHECK_MSG_DELAY 5000

/* Leds pin */
#define LED_1_PIN       10
#define LED_2_PIN       11
#define LED_3_PIN       12
#define LED_4_PIN       13

/* Button pin */
#define BTN_READ_PIN    14
#define BTN_RESTART_PIN 15
#define BTN_RESET_PIN   16

#if SCREEN_TYPE == oled
    // define and include
#elif SCREEN_TYPE == tft
    // define and include
#endif

#if MQTT_ENABLE == true
    // @todo See large message method in exemple
    #define MQTT_MAX_PACKET_SIZE 2048
    #include <PubSubClient.h>
#endif

#if OTA_ENABLE == true
    #include <ArduinoOTA.h>
#endif

void blinkLed(int repeat = 0, int time = 250);