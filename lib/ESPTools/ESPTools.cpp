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