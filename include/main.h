#pragma once

#include <Arduino.h>
#include <SPIFFS.h>
#ifndef ARDUINOJSON_DECODE_UNICODE
    #define ARDUINOJSON_DECODE_UNICODE 1
#endif
#include <ArduinoJson.h>
#include <Ticker.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <TelegramBot.h>
#include <ESPTools.h>
#include <ESPWifi.h>

/* Conditional compilation */
#define MQTT_ENABLE                 false
#define OTA_ENABLE                  false

/* General */
#define SERIAL_BAUDRATE             115200
#define DURATION_BEFORE_RESET       5000
#define DURATION_BEFORE_RESTART     5000
#define DURATION_BTN_RESET_PRESS    10000

/* Telegram */
#define CHECK_MSG_DELAY             10000

/* Leds pin */
#define LED_1                       25
#define LED_2                       26
#define LED_3                       33
#define LED_4                       35

/* Button pin */
#define BTN_READ_PIN                14
#define BTN_RESTART_PIN             34
#define BTN_RESET_PIN               27

/* TFT SCREEN */
#define CALIBRATION_FILE            "/TouchCalData"
#define REPEAT_CAL                  false
#define TFT_BCK_LED                 32
/*#define TFT_DC                      4
#define TFT_CS                      15
#define TFT_RST                     2
#define TFT_MISO                    19         
#define TFT_MOSI                    23           
#define TFT_CLK                     18
#define TFT_LED                     17*/

/* LEDS */
#define LED_CHAN_0                  0
#define LED_CHAN_1                  1
#define LED_CHAN_2                  2
#define LED_CHAN_3                  3
#define LED_FREQUENCY               5000
#define LED_RES                     8

#if MQTT_ENABLE == true
    // @todo See large message method in exemple
    #define MQTT_MAX_PACKET_SIZE    2048
    #include <PubSubClient.h>
#endif

#if OTA_ENABLE == true
    #include <ArduinoOTA.h>
#endif

/* CONFIG */
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
    char otaPassword[64]            = "";
    char uuid[64]                   = "";
};

void tickerManager(bool start = true, float timer = 1);