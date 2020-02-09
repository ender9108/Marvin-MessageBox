#pragma once

#include <Arduino.h>
#include <SPIFFS.h>
#ifndef ARDUINOJSON_DECODE_UNICODE
    #define ARDUINOJSON_DECODE_UNICODE 1
#endif
#include <ArduinoJson.h>
#include <Ticker.h>
#include <WebServer.h>
#include <ESPTools.h>
#include <ESPConfig.h>
#include <ESPWifi.h>
#include <WiFiClientSecure.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <TelegramBot.h>

/* Conditional compilation */
#define MQTT_ENABLE     false
#define OTA_ENABLE      false
#define SCREEN_TYPE     oled

/* General */
#define SERIAL_BAUDRATE 115200
#define DURATION_BEFORE_RESET 5000
#define DURATION_BEFORE_RESTART 5000
#define DURATION_BTN_RESET_PRESS 10000

/* Telegram */
#define CHECK_MSG_DELAY 10000

/* Leds pin */
#define LED_1_R_PIN     32
#define LED_1_G_PIN     35
#define LED_1_B_PIN     34
#define LED_2_R_PIN     33
#define LED_2_G_PIN     25
#define LED_2_B_PIN     26
#define LED_3_R_PIN     26
#define LED_3_G_PIN     27
#define LED_3_B_PIN     14
#define LED_4_R_PIN     12
#define LED_4_G_PIN     13
#define LED_4_B_PIN     16

/* Button pin */
#define BTN_READ_PIN    21
#define BTN_RESTART_PIN 16
#define BTN_RESET_PIN   22

/* TFT SCREEN */
#define TFT_DC          4
#define TFT_CS          15
#define TFT_RST         2
#define TFT_MISO        19         
#define TFT_MOSI        23           
#define TFT_CLK         18

#define BLINK_RED       9
#define BLINK_BLUE      1

#define LED_RED_CHAN    0
#define LED_GREEN_CHAN  1
#define LED_BLUE_CHAN   2

#define LED_FREQUENCY   5000
#define LED_RES         8

#if MQTT_ENABLE == true
    // @todo See large message method in exemple
    #define MQTT_MAX_PACKET_SIZE 2048
    #include <PubSubClient.h>
#endif

#if OTA_ENABLE == true
    #include <ArduinoOTA.h>
#endif

void blinkLed(int repeat = 0, int time = 250);
void tickerManager(bool start = true, unsigned int status = BLINK_BLUE, float timer = 1);