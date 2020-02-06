#include "ESPWifi.h"

bool wifiConnect(char *wifiSsid, char *wifiPassword) {
    unsigned int count = 0;
    WiFi.begin(wifiSsid, wifiPassword);
    Serial.print(F("Try to connect to "));
    logger(wifiSsid);

    while (count < 20) {
        if (WiFi.status() == WL_CONNECTED) {
            logger("");
            logger(F("WiFi connected (IP : "));  
            logger(WiFi.localIP().toString());
            logger(F(")"));

            return true;
        } else {
            delay(500);
            Serial.print(F("."));  
        }

        count++;
    }

    Serial.print(F("Error connection to "));
    logger(String(wifiSsid));
    return false;
}

bool checkWifiConfigValues(char *wifiSsid, char *wifiPassword) {
    logger(F("config.wifiSsid length : "), false);
    logger(String(strlen(wifiSsid)));

    logger(F("config.wifiPassword length : "), false);
    logger(String(strlen(wifiPassword)));

    if ( strlen(wifiSsid) > 1 && strlen(wifiPassword) > 1 ) {
        return true;
    }

    logger(F("Ssid and passw not present in SPIFFS"));
    return false;
}