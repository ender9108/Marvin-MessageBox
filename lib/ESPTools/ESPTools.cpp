#include <ESPTools.h>

void logger(String message, bool endLine) {
    if (true == DEBUG) {
        if (true == endLine) {
            Serial.println(message);
        } else {
            Serial.print(message);
        }

        Serial.flush();
    }
}

void restart() {
    logger("Restart ESP");
    ESP.restart();
}

void clearScreen(TFT_eSPI screen) {
    screen.fillScreen(TFT_BLACK);
    screen.setCursor(0, 0);
}

/*void printCenterText(Adafruit_ILI9341 screen, const char *string, int txtSize, uint16_t txtColor) {
    screen.setTextColor(txtColor);
	screen.setTextSize(txtSize);
	screen.setCursor(screen.width() - (strlen(string) * 3 * txtSize), screen.height() - (4 * txtSize));
    screen.println(string);
}

void printCenterText(Adafruit_ILI9341 screen, String string, int txtSize, uint16_t txtColor) {
    screen.setTextColor(txtColor);
	screen.setTextSize(txtSize);
	screen.setCursor(screen.width() - (string.length() * 3 * txtSize), screen.height() - (4 * txtSize));
    screen.println(string);
}

void clearScreen(Adafruit_ILI9341 screen) {
    screen.fillScreen(ILI9341_BLACK);
    screen.setCursor(0, 0);
}*/