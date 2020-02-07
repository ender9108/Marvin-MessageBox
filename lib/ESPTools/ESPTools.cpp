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

void printCenterText(Adafruit_ILI9341 screen, const char *string, uint8_t txtSize, uint16_t txtColor) {
    screen.setTextColor(txtColor);
	screen.setTextSize(txtSize);
	screen.setCursor(screen.width() - (strlen(string) * 3 * txtSize), screen.height() - (4 * txtSize));
    screen.println(string);
}

void printCenterText(Adafruit_ILI9341 screen, String string, uint8_t txtSize, uint16_t txtColor) {
    screen.setTextColor(txtColor);
	screen.setTextSize(txtSize);
	screen.setCursor(screen.width() - (string.length() * 3 * txtSize), screen.height() - (4 * txtSize));
    screen.println(string);
}

void clearScreen(Adafruit_ILI9341 screen) {
    screen.fillScreen(ILI9341_BLACK);
}