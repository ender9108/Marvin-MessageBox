#pragma once

#include <Arduino.h>

#define DEBUG           true

void logger(String message, bool endLine = true);
void restart();