#pragma once

#include <Arduino.h>
#include <ESPTools.h>
#include <WiFiClientSecure.h>

bool wifiConnect(char *wifiSsid, char *wifiPassword);
bool checkWifiConfigValues(char *wifiSsid, char *wifiPassword);