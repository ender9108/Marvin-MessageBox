#include <ESPTools.h>

void logger(String message, bool endLine) {
    if (true == DEBUG) {
        if (true == endLine) {
            Serial.println(message);
        } else {
            Serial.print(message);
        }
    }
}

void restart() {
    logger("Restart ESP");
    ESP.restart();
}