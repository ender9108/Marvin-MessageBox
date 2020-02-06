#pragma once

#include <Arduino.h>
#include <Adafruit_ILI9341.h>

#define DEBUG           true

void logger(String message, bool endLine = true);
void restart();
void printCenterText(Adafruit_ILI9341 screen, const char *string, uint8_t txtSize, uint16_t txtColor);
void printCenterText(Adafruit_ILI9341 screen, String *string, uint8_t txtSize, uint16_t txtColor);