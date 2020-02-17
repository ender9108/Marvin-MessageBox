#include "main.h"

const char *configFilePath  = "/config.json";
const char *wifiApSsid      = "MarvinMessageBoxSsid";
const char *wifiApPassw     = "MarvinMessageBox";
const char *appName         = "MessageBox";
const char *hostname        = "marvin-messagebox.local";

Ticker ticker;

#if MQTT_ENABLE == true
    PubSubClient mqttClient;
#endif

Config config;
WebServer server(80);
WiFiClientSecure wifiClient;
TelegramBot telegramBot(wifiClient);
TFT_eSPI screen = TFT_eSPI(); // Invoke custom library

bool wifiConnected = false;
bool startApp = false;
bool messageIsReading = false;
bool newMessageArrived = false;
bool ledStatus = false;
bool ledErrorMode= false;
long telegramBotLasttime; 
String errorMessage = "";
unsigned int previousResetBtnState = HIGH;
unsigned long resetBtnDuration = 0;
unsigned long resetRequested = 0;
unsigned long restartRequested = 0;
unsigned int blinkColor = 1;
JsonArray lastTelegramUpdate;
JsonObject currentReadingMessage;
String emoji[3] = {
    "\u2764\ufe0f", // kiss
    "\u2764\ufe0f", // heart
    "\u2709"        // enveloppe
};

/* ********** MQTT ********** */

#if MQTT_ENABLE == true
    bool mqttConnected = false;

    bool mqttConnect() {
        int count = 0;

        while (!mqttClient.connected()) {
            logger("Attempting MQTT connection (host: " + String(config.mqttHost) + ")...");

            if (mqttClient.connect(appName, config.mqttUsername, config.mqttPassword)) {
                logger(F("connected !"));

                if (strlen(config.mqttSubscribeChannel) > 1) {
                    mqttClient.subscribe(config.mqttSubscribeChannel);
                }
                
                return true;
            } else {
                logger(F("failed, rc="), false);
                logger(String(mqttClient.state()));
                logger(F("try again in 5 seconds"));
                // Wait 5 seconds before retrying
                delay(5000);

                if (count == 10) {
                    return false;
                }
            }

            count++;
        }

        return false;
    }

    void callback(char* topic, byte* payload, unsigned int length) {
        StaticJsonDocument<256> json;
        deserializeJson(json, payload, length);

        char response[512];
        
        if (json.containsKey("action")) {
            JsonVariant action = json["action"];

            if (json["action"] == "ping") {
                sprintf(response, "{\"code\":\"200\",\"uuid\":\"%s\",\"actionCalled\":\"%s\",\"payload\":\"pong\"}", config.uuid, action.as<char*>());
            }
            else if (json["action"] == "restart") {
                sprintf(response, "{\"code\":\"200\",\"uuid\":\"%s\",\"actionCalled\":\"%s\",\"payload\":\"Restart in progress\"}", config.uuid, action.as<char*>());
                restartRequested = millis();
            }
            else if (json["action"] == "reset") {
                sprintf(response, "{\"code\": \"200\",\"uuid\":\"%s\",\"actionCalled\":\"%s\",\"payload\":\"Reset in progress\"}", config.uuid, action.as<char*>());
                resetRequested = millis();
            }
            else {
                sprintf(response, "{\"code\":\"404\",\"uuid\":\"%s\",\"payload\":\"Action %s not found !\"}", config.uuid, action.as<char*>());
            }

            mqttClient.publish(config.mqttPublishChannel, response);
        }

        memset(response, 0, sizeof(response));
    }
#endif

/* ********** CONFIG ********** */
bool getConfig() {
    File configFile = SPIFFS.open(configFilePath, FILE_READ);

    if (!configFile) {
        logger("Failed to open config file \"" + String(configFilePath) + "\".");
        return false;
    }

    size_t size = configFile.size();

    if (size == 0) {
        logger(F("Config file is empty !"));
        return false;
    }

    if (size > 1024) {
        logger(F("Config file size is too large"));
        return false;
    }

    StaticJsonDocument<512> json;
    DeserializationError err = deserializeJson(json, configFile);

    configFile.close();

    switch (err.code()) {
        case DeserializationError::Ok:
            logger(F("Deserialization succeeded"));
            break;
        case DeserializationError::InvalidInput:
            logger(F("Invalid input!"));
            return false;
            break;
        case DeserializationError::NoMemory:
            logger(F("Not enough memory"));
            return false;
            break;
        default:
            logger(F("Deserialization failed"));
            return false;
            break;
    }

    // Copy values from the JsonObject to the Config
    if (
        !json.containsKey("wifiSsid") ||
        !json.containsKey("wifiPassword") ||
        !json.containsKey("mqttHost") ||
        !json.containsKey("mqttPort") ||
        !json.containsKey("mqttUsername") ||
        !json.containsKey("mqttPassword") ||
        !json.containsKey("mqttPublishChannel") ||
        !json.containsKey("mqttSubscribeChannel") ||
        !json.containsKey("telegramBotToken") ||
        !json.containsKey("otaPassword") ||
        !json.containsKey("uuid")
    ) {
        logger(F("getConfig"));
        serializeJson(json, Serial);
        logger(F(""));
        logger(F("Key not found in json fille"));
        return false;
    }

    strlcpy(config.wifiSsid, json["wifiSsid"], sizeof(config.wifiSsid));
    strlcpy(config.wifiPassword, json["wifiPassword"], sizeof(config.wifiPassword));
    strlcpy(config.mqttHost, json["mqttHost"], sizeof(config.mqttHost));
    config.mqttPort = json["mqttPort"] | 1883;
    strlcpy(config.mqttUsername, json["mqttUsername"], sizeof(config.mqttUsername));
    strlcpy(config.mqttPassword, json["mqttPassword"], sizeof(config.mqttPassword));
    strlcpy(config.mqttPublishChannel, json["mqttPublishChannel"], sizeof(config.mqttPublishChannel));
    strlcpy(config.mqttSubscribeChannel, json["mqttSubscribeChannel"], sizeof(config.mqttSubscribeChannel));
    strlcpy(config.telegramBotToken, json["telegramBotToken"], sizeof(config.telegramBotToken));
    strlcpy(config.otaPassword, json["otaPassword"], sizeof(config.otaPassword));
    strlcpy(config.uuid, json["uuid"], sizeof(config.uuid));

    return true;
}

bool setConfig(Config newConfig) {
    StaticJsonDocument<512> json;
    
    json["wifiSsid"] = String(newConfig.wifiSsid);
    json["wifiPassword"] = String(newConfig.wifiPassword);
    json["mqttHost"] = String(newConfig.mqttHost);
    json["mqttPort"] = newConfig.mqttPort;
    json["mqttUsername"] = String(newConfig.mqttUsername);
    json["mqttPassword"] = String(newConfig.mqttPassword);
    json["mqttPublishChannel"] = String(newConfig.mqttPublishChannel);
    json["mqttSubscribeChannel"] = String(newConfig.mqttSubscribeChannel);
    json["telegramBotToken"] = String(newConfig.telegramBotToken);
    json["otaPassword"] = String(newConfig.otaPassword);

    if (strlen(newConfig.uuid) == 0) {
        uint32_t tmpUuid = esp_random();
        String(tmpUuid).toCharArray(newConfig.uuid, 64);
    }

    json["uuid"] = String(newConfig.uuid);

    File configFile = SPIFFS.open(configFilePath, FILE_WRITE);
    
    if (!configFile) {
        logger(F("Failed to open config file for writing"));
        return false;
    }

    serializeJson(json, configFile);

    configFile.close();

    return true;
}

void resetConfig() {
    logger(F("Reset ESP"));
    Config resetConfig;
    setConfig(resetConfig);
}

/* ********** WEB SERVER ********** */

void handleHome() {
    String content = "";

    File file = SPIFFS.open("/index.html", FILE_READ);

    if (!file) {
        logger("Failed to open file \"/index.html\".");
        server.send(500, "text/plain", "Internal error");
    } else {
        content = file.readString();
        content.replace("%TITLE%", String(appName));
        content.replace("%MODULE_NAME%", String(appName));
        content.replace("%ERROR_MESSAGE%", errorMessage);

        if (errorMessage.length() == 0) {
            content.replace("%ERROR_HIDDEN%", "d-none");
        } else {
            content.replace("%ERROR_HIDDEN%", "");
        }

        content.replace("%WIFI_SSID%", String(config.wifiSsid));
        content.replace("%WIFI_PASSWD%", String(config.wifiPassword));

        if (true == config.mqttEnable) {
            content.replace("%MQTT_ENABLE%", "checked");
        } else {
            content.replace("%MQTT_ENABLE%", "");
        }

        if (false == MQTT_ENABLE) {
            content.replace("%MQTT_INPUT_HIDDEN%", "d-none");
        } else {
            content.replace("%MQTT_INPUT_HIDDEN%", "");
        }

        if (false == OTA_ENABLE) {
            content.replace("%OTA_INPUT_HIDDEN%", "d-none");
        } else {
            content.replace("%OTA_INPUT_HIDDEN%", "");
        }

        content.replace("%MQTT_HOST%", String(config.mqttHost));
        content.replace("%MQTT_PORT%", String(config.mqttPort));
        content.replace("%MQTT_USERNAME%", String(config.mqttUsername));
        content.replace("%MQTT_PASSWD%", String(config.mqttPassword));
        content.replace("%MQTT_PUB_CHAN%", String(config.mqttPublishChannel));
        content.replace("%MQTT_SUB_CHAN%", String(config.mqttSubscribeChannel));
        content.replace("%TELEGRAM_TOKEN%", String(config.telegramBotToken));
        content.replace("%OTA_PASSWORD%", String(config.otaPassword));

        server.sendHeader("Content-Length", String(content.length()));
        server.send(200, "text/html", content);
    }
}

void handleSave() {
    bool error = false;

    for (int i = 0; i < server.args(); i++) {
        logger(server.argName(i), false);
        logger(" : ", false);
        logger(server.arg(i));
    }

    if (!server.hasArg("wifiSsid") || !server.hasArg("wifiPassword")){  
        error = true;
        logger("No wifiSsid and wifiPassword args");
        errorMessage = "[WIFI ERROR] - No ssid or password send";
    }

    if (server.arg("wifiSsid").length() <= 1 || server.arg("wifiPassword").length() <= 1) {
        error = true;
        logger("wifiSsid and wifiPassword args is empty");
        errorMessage = "[WIFI ERROR] - Ssid or password is empty";
    }

    if (false == error) {
        server.arg("wifiSsid").toCharArray(config.wifiSsid, 32);
        server.arg("wifiPassword").toCharArray(config.wifiPassword, 64);
        server.arg("mqttHost").toCharArray(config.mqttHost, 128);
        config.mqttPort = server.arg("mqttPort").toInt();

        if (server.hasArg("mqttEnable")) {
            config.mqttEnable = true;
        } else {
            config.mqttEnable = false;
        }

        server.arg("mqttUsername").toCharArray(config.mqttUsername, 32);
        server.arg("mqttPassword").toCharArray(config.mqttPassword, 64);
        server.arg("mqttPublishChannel").toCharArray(config.mqttPublishChannel, 128);
        server.arg("mqttSubscribeChannel").toCharArray(config.mqttSubscribeChannel, 128);
        server.arg("telegramBotToken").toCharArray(config.telegramBotToken, 128);
        server.arg("otaPassword").toCharArray(config.otaPassword, 64);

        setConfig(config);

        String content = "";
        File file = SPIFFS.open("/restart.html", FILE_READ);

        if (!file) {
            logger("Failed to open file \"restart.html\".");
            server.send(500, "text/plain", "Internal error");
        } else {
            content = file.readString();
            content.replace("%TITLE%", String(appName));
            content.replace("%MODULE_NAME%", String(appName));

            server.sendHeader("Content-Length", String(content.length()));
            server.send(200, "text/html", content);
        }
    } else {
        Serial.println("Config error, redirect to /");
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
    }
}

void handleRestart() {
    String content = "Redémarrage en cours. Vous pouvez vous reconnecter à votre réseau wifi";

    server.sendHeader("Content-Length", String(content.length()));
    server.send(200, "text/plain", content);
    delay(1000);
    restart();
}

void handleCss() {
    String content = "";
    File file = SPIFFS.open("/bootstrap.min.css", FILE_READ);

    if (!file) {
        logger("Failed to open file \"bootstrap.min.css\".");
        server.send(500, "text/plain", "Internal error");
    } else {
        server.send(200, "text/css", file.readString());
    }
}

void handleFavicon() {
    server.send(200, "text/html", String(""));
}

void handleNotFound() {
    String content = "";
    File file = SPIFFS.open("/404.html", FILE_READ);

    if (!file) {
        logger("Failed to open file \"404.html\".");
        server.send(500, "text/plain", "Internal error");
    } else {
        content = file.readString();
        content.replace("%TITLE%", String(appName));
        content.replace("%MODULE_NAME%", String(appName));

        server.send(404, "text/html", content);
    }
}

void serverConfig() {
    server.on("/", HTTP_GET, handleHome);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/restart", HTTP_GET, handleRestart);
    server.on("/bootstrap.min.css", HTTP_GET, handleCss);
    server.on("/favicon.ico", HTTP_GET, handleFavicon);
    server.onNotFound(handleNotFound);

    server.begin();
    logger("HTTP server started");
}

/* ********** SCREEN ********** */
void touchCalibrate() {
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check if calibration file exists and size is correct
  if (SPIFFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL)
    {
      // Delete if we want to re-calibrate
      SPIFFS.remove(CALIBRATION_FILE);
    }
    else
    {
      File f = SPIFFS.open(CALIBRATION_FILE, "r");
      if (f) {
        if (f.readBytes((char *)calData, 14) == 14)
          calDataOK = 1;
        f.close();
      }
    }
  }

  if (!calDataOK || REPEAT_CAL) {
    screen.fillScreen(TFT_BLACK);
    screen.setCursor(20, 0);
    screen.setTextFont(2);
    screen.setTextSize(1);
    screen.setTextColor(TFT_WHITE, TFT_BLACK);

    screen.println("Touch corners as indicated");

    screen.setTextFont(1);
    screen.println();

    screen.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    Serial.println(); Serial.println();
    Serial.println("// Use this calibration code in setup():");
    Serial.print("  uint16_t calData[5] = ");
    Serial.print("{ ");

    for (uint8_t i = 0; i < 5; i++)
    {
        Serial.print(calData[i]);
        if (i < 4) Serial.print(", ");
    }

    Serial.println(" };");
    Serial.print("  screen.setTouch(calData);");
    Serial.println(); Serial.println();

    screen.fillScreen(TFT_BLACK);
    
    screen.setTextColor(TFT_GREEN, TFT_BLACK);
    screen.println("Calibration complete!");
    screen.println("Calibration code sent to Serial port.");
 
    // store data
    File f = SPIFFS.open(CALIBRATION_FILE, "w");

    if (f) {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }

    delay(4000);
  }
}

void drawBottomButtonBar() {
    screen.fillRect(0, 195, 320, 4, TFT_WHITE);

    if (lastTelegramUpdate.size() > 1){
        screen.drawRect(ARROW_BTN_POS_X, ARROW_BTN_POS_Y, ARROW_BTN_WIDTH, ARROW_BTN_HEIGHT, TFT_GREEN);
        screen.pushImage(ARROW_BTN_IMG_POS_X, ARROW_BTN_IMG_POS_Y, ARROW_BTN_IMG_WIDTH, ARROW_BTN_IMG_HEIGHT, heart);        
    }

    screen.drawRect(HEART_BTN_POS_X, HEART_BTN_POS_Y, HEART_BTN_WIDTH, HEART_BTN_HEIGHT, TFT_GREEN);
    screen.pushImage(HEART_BTN_IMG_POS_X, HEART_BTN_IMG_POS_Y, HEART_BTN_IMG_WIDTH, HEART_BTN_IMG_HEIGHT, heart);
}

void drawScreenNoMessage() {
    screen.setTextDatum(TC_DATUM);
    screen.drawString(F("Pas de nouveau message"), 160, 120, 1);

    drawBottomButtonBar();
}

void drawScreenMessage(String message) {
    screen.setTextDatum(TC_DATUM);
    screen.drawString(message, 160, 120, 1);

    drawBottomButtonBar();
}

/* ********** TELEGRAM ********** */
void displayNewMessage() {
    logger("displayNewMessage");
    messageIsReading = true;

    if (lastTelegramUpdate.size() > 0) {
        currentReadingMessage = lastTelegramUpdate[0];
        String message = lastTelegramUpdate[0]["message"]["text"].as<String>();
        drawScreenMessage(message);
        telegramBot.sendMessage(lastTelegramUpdate[0]["message"]["chat"]["id"], emoji[2] + "Message \"" + message + "\" lu ");
        lastTelegramUpdate.remove(0);
    } else {
        drawScreenNoMessage();
    }
    /*if (true == newMessageArrived) {
        messageIsReading = true;
        logger("displayNewMessage !");
        lastTelegramUpdate.message.text.replace("/message", "");
        lastTelegramUpdate.message.text.trim();
        clearScreen(screen);
        screen.setCursor(0, 0);
        delay(100);

        if (lastTelegramUpdate.message.text == "") {
            logger(F("No new message"));
            screen.println("Pas de nouveau message.");
        } else {
            // Display message on screen
            logger(F("Display message on screen"));
            screen.println(lastTelegramUpdate.message.text);
            delay(100);

            logger(F("Send confirmation read message to bot chat id: "), false);
            logger(String(lastTelegramUpdate.message.chat.id));

            
            logger("chat id: " + String(lastTelegramUpdate.message.chat.id));
            tickerManager(false);

            #if MQTT_ENABLE == true
                char response[256];
                sprintf(response, "{\"code\":\"200\",\"uuid\":\"%s\",\"actionCalled\":\"readMessage\",\"payload\":\"Message lu.\"}", config.uuid);
                mqttClient.publish(config.mqttPublishChannel, response);
            #endif
        }
    }*/

}

void handlerNewMessage(JsonArray updates, int newMessages) {
    for (int i = 0; i < updates.size(); i++) {
        if (updates[i].containsKey("message")) {
            if (updates[i]["message"].containsKey("text")) {
                String message = updates[i]["message"]["text"].as<String>();
                message.trim();

                if (message != "") {
                    logger(F("New message !"));
                    newMessageArrived = true;
                    tickerManager(true, 2);

                    lastTelegramUpdate.add(updates[i]);
                    telegramBot.sendMessage(
                        updates[i]["message"]["chat"]["id"], 
                        "Le message \"" + message + "\" à été reçu. En attente de lecture."
                    );
                }
            }

            if (updates[i]["message"].containsKey("photo")) {
                // gérer les photos
            }
        }
    }

    if (lastTelegramUpdate.size() == TELEGRAM_MAX_UPDATE) {
        telegramBot.pause();
    }
}

void sendHeart() {
    logger(F("Send emoji heart."));
    telegramBot.sendMessage(currentReadingMessage["message"]["chat"]["id"], emoji[1]);
}

/* ********** LEDS ********** */

void blinkLed() {
    if (false == ledStatus) {
        ledStatus = true;
    } else {
        ledStatus = false;
    }

    if (true == ledErrorMode) {
        if (true == ledStatus) {
            ledcWrite(LED_CHAN_0, 255);
            ledcWrite(LED_CHAN_1, 255);
            ledcWrite(LED_CHAN_2, 255);
            ledcWrite(LED_CHAN_3, 255);
        } else {
            ledcWrite(LED_CHAN_0, 0);
            ledcWrite(LED_CHAN_1, 0);
            ledcWrite(LED_CHAN_2, 0);
            ledcWrite(LED_CHAN_3, 0);
        }
    } else {
        if (true == ledStatus) {
            for (int i = 0; i < 256; i++) {
                ledcWrite(LED_CHAN_0, i);
                ledcWrite(LED_CHAN_1, i);
                ledcWrite(LED_CHAN_2, i);
                ledcWrite(LED_CHAN_3, i);
                delay(15);
            }
        } else {
            for (int i = 255; i >= 0; i--) {
                ledcWrite(LED_CHAN_0, i);
                ledcWrite(LED_CHAN_1, i);
                ledcWrite(LED_CHAN_2, i);
                ledcWrite(LED_CHAN_3, i);
                delay(15);
            }
        }
    }
}

void shutdownLed() {
    ledcWrite(LED_CHAN_0, 0);
    ledcWrite(LED_CHAN_1, 0);
    ledcWrite(LED_CHAN_2, 0);
    ledcWrite(LED_CHAN_3, 0);
}

void tickerManager(bool start, float timer) {
    shutdownLed();

    if (true == start) {
        logger(F("Start ticker !"));
        ticker.attach(timer, blinkLed);
    } else {
        logger(F("Stop ticker !"));
        ledErrorMode = false;
        ticker.detach();
    }
}

/* ********** COMMON ********** */

void setup() {
    Serial.begin(SERIAL_BAUDRATE);
    logger(F("Start program !"));

    if (!SPIFFS.begin(true)) {
        logger(F("An Error has occurred while mounting SPIFFS"));
        return;
    }

    logger(F("SPIFFS mounted"));

    pinMode(TFT_BCK_LED, OUTPUT);
    digitalWrite(TFT_BCK_LED, HIGH);

    screen.init();
    screen.setRotation(1);
    screen.setTextSize(1);
    screen.setTextColor(TFT_WHITE, TFT_BLACK);
    touchCalibrate();
    clearScreen(screen);

    uint16_t calData[5] = { 1, 1, 1, 1, 0 };
    screen.setTouch(calData);

    screen.println("Start in progress...");

    ledcSetup(LED_CHAN_0, LED_FREQUENCY, LED_RES);
    ledcSetup(LED_CHAN_1, LED_FREQUENCY, LED_RES);
    ledcSetup(LED_CHAN_2, LED_FREQUENCY, LED_RES);
    ledcSetup(LED_CHAN_3, LED_FREQUENCY, LED_RES);

    ledcAttachPin(LED_1, LED_CHAN_0);
    ledcAttachPin(LED_2, LED_CHAN_1);
    ledcAttachPin(LED_3, LED_CHAN_2);
    ledcAttachPin(LED_4, LED_CHAN_3);
    
    tickerManager(false);
    shutdownLed();

    // Get wifi SSID and PASSW from SPIFFS
    if (true == getConfig()) {
        screen.println("[Ok] - Get configuration");
        if (true == checkWifiConfigValues(config.wifiSsid, config.wifiPassword)) {
            wifiConnected = wifiConnect(config.wifiSsid, config.wifiPassword);
        
            #if MQTT_ENABLE == true
            if (true == wifiConnected && true == config.mqttEnable) {
                mqttClient.setClient(wifiClient);
                mqttClient.setServer(config.mqttHost, config.mqttPort);
                mqttClient.setCallback(callback);
                mqttConnected = mqttConnect();
            }
            #endif
        }
    } // endif true == getConfig()

    if (false == wifiConnected) {
        screen.println("[Nok] - Wifi connection error (" + String(config.wifiSsid) + ")");

        if (strlen(config.wifiSsid) > 1) {
            errorMessage = "Wifi connection error to " + String(config.wifiSsid);
        }

        startApp = false;
    }
    #if MQTT_ENABLE == true
        else if (true == wifiConnected &&true == config.mqttEnable && false == mqttConnected) {
            screen.println("[Nok] - Mqtt connection error (" + String(config.mqttHost) + ")");
            errorMessage = "Mqtt connection error to " + String(config.mqttHost);
            startApp = false;
        }
    #endif
    else {
        startApp = true;
    }

    if (false == startApp) {
        ledErrorMode = true;
        tickerManager(true, 0.5);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(wifiApSsid, wifiApPassw);
        WiFi.setHostname(hostname);
        logger("WiFi AP is ready (IP : " + WiFi.softAPIP().toString() + ")");
        serverConfig();

        screen.fillScreen(TFT_BLACK);
        screen.setCursor(0, 0);
        screen.println("Settings interface is started.");
        screen.println("");
        screen.println("Connect your wifi on : ");
        screen.println("SSID : " + String(wifiApSsid));
        screen.println("PWD  : " + String(wifiApPassw));
        screen.println("");
        screen.println("Open your browser and connect to ");
        screen.println("http://" + String(hostname));
        logger(String(hostname));
    } else {
        pinMode(BTN_READ_PIN, INPUT);
        pinMode(BTN_RESTART_PIN, INPUT);
        pinMode(BTN_RESET_PIN, INPUT);

        telegramBot.setToken(config.telegramBotToken);
        telegramBot.on(TELEGRAM_EVT_NEW_MSG, handlerNewMessage);
        telegramBot.setTimeToRefresh(CHECK_MSG_DELAY);
        clearScreen(screen);
        tickerManager(false);
        //digitalWrite(TFT_BCK_LED, LOW);
        logger(F("App started !"));
    }

    #if OTA_ENABLE == true
        ArduinoOTA.setHostname(appName);
        ArduinoOTA.setPasswordHash(config.otaPassword);

        ArduinoOTA.onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH) {
                type = "sketch";
            } else { // U_SPIFFS
                type = "filesystem";
            }

            SPIFFS.end();
            logger("Start updating " + type);
        }).onEnd([]() {
            logger(F("\nEnd"));
        }).onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        }).onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) logger(F("Auth Failed"));
            else if (error == OTA_BEGIN_ERROR) logger(F("Begin Failed"));
            else if (error == OTA_CONNECT_ERROR) logger(F("Connect Failed"));
            else if (error == OTA_RECEIVE_ERROR) logger(F("Receive Failed"));
            else if (error == OTA_END_ERROR) logger(F("End Failed"));
        });

        ArduinoOTA.begin();
    #endif
}

void loop() {
    if (true == startApp) {
        #if MQTT_ENABLE == true
            if (true == config.mqttEnable) {
                if (!mqttClient.connected()) {
                    mqttConnect();
                }

                mqttClient.loop();
            }
        #endif

        #if DEBUG == true
            telegramBot.enableDebugMode();
        #endif

        telegramBot.loop();

        if (digitalRead(BTN_READ_PIN) == LOW) {
            if (false == messageIsReading) {
                telegramBot.pause();
                //digitalWrite(TFT_BCK_LED, HIGH);
                displayNewMessage();
            }
        }

        if (digitalRead(BTN_READ_PIN) == HIGH) {
            if (true == messageIsReading) {
                messageIsReading = false;
                newMessageArrived = false;
                clearScreen(screen);
                //digitalWrite(TFT_BCK_LED, LOW);
                telegramBot.resume();
            }
        }

        uint16_t x, y;

        if (screen.getTouch(&x, &y)) {
            logger("Touch X: " + String(x));
            logger("Touch Y: " + String(y));

            if ((x > ARROW_BTN_POS_X) && (x < (ARROW_BTN_POS_X + ARROW_BTN_WIDTH))) {
                if ((y > ARROW_BTN_POS_Y) && (y <= (ARROW_BTN_POS_Y + ARROW_BTN_HEIGHT))) {
                    Serial.println("Arrow btn hit");
                    displayNewMessage();
                }
            }

            if ((x > HEART_BTN_POS_X) && (x < (HEART_BTN_POS_X + HEART_BTN_WIDTH))) {
                if ((y > HEART_BTN_POS_Y) && (y <= (HEART_BTN_POS_Y + HEART_BTN_HEIGHT))) {
                    Serial.println("Heart btn hit");
                    sendHeart();
                }
            }
        }

        /*if (digitalRead(BTN_RESTART_PIN) == LOW) {
            logger("Button restart pressed !");
            tickerManager(true, 0.5);
            restartRequested = millis();
        }

        if (restartRequested != 0) {
            if (millis() - restartRequested >= DURATION_BEFORE_RESTART ) {
                tickerManager(false);
                shutdownLed();
                restart();
            }
        }

        if (digitalRead(BTN_RESET_PIN) == LOW && previousResetBtnState == HIGH) {
            previousResetBtnState = LOW;
            resetBtnDuration = millis();
        }

        if (
            resetBtnDuration != 0 &&
            digitalRead(BTN_RESET_PIN) == LOW && 
            previousResetBtnState == LOW
        ) {
            if (millis() - resetBtnDuration >= DURATION_BTN_RESET_PRESS) {
                tickerManager(true, 0.5);
                resetRequested = millis();
            }
        }

        if (resetRequested != 0) {
            if (millis() - resetRequested >= DURATION_BEFORE_RESET ) {
                resetConfig();
                restart();
            }
        }*/

        #if OTA_ENABLE == true
            logger(F("Start ArduinoOTA handle"));
            ArduinoOTA.handle();
        #endif
    } else {
        server.handleClient();
    }
}