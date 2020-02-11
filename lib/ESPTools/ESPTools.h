#pragma once

#include <Arduino.h>
#include <Adafruit_ILI9341.h>

#define DEBUG               true
#define DEFAULT_TXT_COLOR   ILI9341_WHITE
#define DEFAULT_BG_COLOR    ILI9341_BLACK
#define DEFAULT_TXT_SIZE    1

#define CONFIG_ERROR    9
#define CONFIG_OK       1

//const int ledStatus = CONFIG_OK;

void logger(String message, bool endLine = true);
void restart();
void printCenterText(Adafruit_ILI9341 screen, const char *string, int txtSize = DEFAULT_TXT_SIZE, uint16_t txtColor = DEFAULT_TXT_COLOR);
void printCenterText(Adafruit_ILI9341 screen, String *string, int txtSize = DEFAULT_TXT_SIZE, uint16_t txtColor = DEFAULT_TXT_COLOR);
void clearScreen(Adafruit_ILI9341 screen);