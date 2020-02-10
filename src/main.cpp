#include "main.h"

struct LastMessage {
    String chatId = "";
    String name = "";
    String message = "";
};

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
LastMessage lastMessage;
WebServer server(80);
WiFiClientSecure wifiClient;
//UniversalTelegramBot telegramBot(wifiClient);
TelegramBot telegramBot(wifiClient);
Adafruit_ILI9341 screen = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

bool wifiConnected = false;
bool startApp = false;
bool messageIsReading = false;
bool ledStatus = false;
long telegramBotLasttime; 
String errorMessage = "";
unsigned int previousResetBtnState = HIGH;
unsigned long resetBtnDuration = 0;
unsigned long resetRequested = 0;
unsigned long restartRequested = 0;
unsigned int blinkColor = 1;

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

        setConfig(configFilePath, config);

        String content = "";
        File file = SPIFFS.open("/restart.html", FILE_READ);

        if (!file) {
            logger("Failed to open file \"restart.html\".");
            server.send(500, "text/plain", "Internal error");
        } else {
            content = file.readString();
            content.replace("%TITLE%", String(appName));
            content.replace("%MODULE_NAME%", String(appName));

            server.send(200, "text/html", content);
        }
    } else {
        Serial.println("Config error, redirect to /");
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
    }
}

void handleRestart() {
    server.send(200, "text/plain", "Redémarrage en cours. Vous pouvez vous reconnecter à votre réseau wifi");
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

/* ********** TELEGRAM ********** */
void handlerNewMessage(Message *msg, long lastUpdateId) {
    for (int i = (TELEGRAM_MAX_MSG-1); i >= 0; i--) {
        if (msg[i].id != 0) {
            logger("Message content :" + msg[i].text);

            if (msg[i].text.indexOf("/message") == 0) {
                logger(F("New message !"));
                tickerManager(true, BLINK_RED, 2);

                telegramBot.sendMessage(msg[i].chat.id, "Message reçu. En attente de lecture.", "");
                #if MQTT_ENABLE == true
                    char response[256];
                    sprintf(response, "{\"code\":\"200\",\"uuid\":\"%s\",\"actionCalled\":\"newMessageReceived\",\"payload\":\"Message reçu. En attente de lecture.\"}", config.uuid);
                    mqttClient.publish(config.mqttPublishChannel, response);
                #endif
            }

            if (msg[i].text.indexOf("/start") == 0) {
                logger(F("Send message to telegram bot (action called : /start)"));
                String welcome = "Welcome to MessageBox, " + msg[i].from.firstName + ".\n";
                welcome += "/message [MY_TEXT] : to send message in this box !\n";
                telegramBot.sendMessage(msg[i].chat.id, welcome, "Markdown");
            }
        }
    }
}

void displayNewMessage() {
    messageIsReading = true;
    Message lastMessage = telegramBot.getLastMessage();
    lastMessage.text.replace("/message", "");
    lastMessage.text.trim();
    screen.writeCommand(ILI9341_SLPOUT);
    clearScreen(screen);

    if (lastMessage.text == "") {
        logger(F("No new message"));
        screen.println("Pas de nouveau message.");
        // Display "Pas de nouveau message";
    } else {
        // Display message on screen
        logger(F("Display message on screen"));
        screen.println(lastMessage.text);

        logger(F("Send confirmation read message to bot"));

        if (!telegramBot.sendMessage(lastMessage.chat.id, "Message lu !", "")) {
            logger(F("Send confirmation message error."));
        }

        tickerManager(false);

        #if MQTT_ENABLE == true
            char response[256];
            sprintf(response, "{\"code\":\"200\",\"uuid\":\"%s\",\"actionCalled\":\"readMessage\",\"payload\":\"Message lu.\"}", config.uuid);
            mqttClient.publish(config.mqttPublishChannel, response);
        #endif
    }
}

/* ********** LEDS ********** */

void blinkLed() {
    int value = 0;

    if (false == ledStatus) {
        ledStatus = true;
    } else {
        ledStatus = false;
    }

    if (blinkColor == BLINK_BLUE) {
        if (true == ledStatus) {
            for (int i = 0; i < 256; i++) {
                ledcWrite(LED_RED_CHAN, 0);
                ledcWrite(LED_GREEN_CHAN, 0);
                ledcWrite(LED_BLUE_CHAN, i);
                delay(20);
            }
        } else {
            for (int i = 255; i >= 0; i--) {
                ledcWrite(LED_RED_CHAN, 0);
                ledcWrite(LED_GREEN_CHAN, 0);
                ledcWrite(LED_BLUE_CHAN, i);
                delay(20);
            }
        }
    } else {
        ledcWrite(LED_RED_CHAN, value);
        ledcWrite(LED_GREEN_CHAN, 0);
        ledcWrite(LED_BLUE_CHAN, 0);

        if (true == ledStatus) {
            for (int i = 0; i < 256; i++) {
                ledcWrite(LED_RED_CHAN, i);
                ledcWrite(LED_GREEN_CHAN, 0);
                ledcWrite(LED_BLUE_CHAN, 0);
                delay(20);
            }
        } else {
            for (int i = 255; i >= 0; i--) {
                ledcWrite(LED_RED_CHAN, i);
                ledcWrite(LED_GREEN_CHAN, 0);
                ledcWrite(LED_BLUE_CHAN, 0);
                delay(20);
            }
        }
    }
}

void shutdownLed() {
    ledcWrite(LED_RED_CHAN, 0);
    ledcWrite(LED_GREEN_CHAN, 0);
    ledcWrite(LED_BLUE_CHAN, 0);
}

void tickerManager(bool start, unsigned int status, float timer) {
    shutdownLed();
    blinkColor = status;

    if (true == start) {
        logger(F("Start ticker !"));
        ticker.attach(timer, blinkLed);
    } else {
        logger(F("Stop ticker !"));
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

    screen.begin();
    screen.setRotation(1);
    screen.setFont();
    clearScreen(screen);
    screen.setTextColor(ILI9341_WHITE);
    screen.setTextSize(2);

    screen.setCursor(0, 0);
    screen.println("Start in progress...");

    ledcSetup(LED_RED_CHAN, LED_FREQUENCY, LED_RES);
    ledcSetup(LED_GREEN_CHAN, LED_FREQUENCY, LED_RES);
    ledcSetup(LED_BLUE_CHAN, LED_FREQUENCY, LED_RES);

    ledcAttachPin(LED_1_R_PIN, LED_RED_CHAN);
    ledcAttachPin(LED_1_G_PIN, LED_GREEN_CHAN);
    ledcAttachPin(LED_1_B_PIN, LED_BLUE_CHAN);
    ledcAttachPin(LED_2_R_PIN, LED_RED_CHAN);
    ledcAttachPin(LED_2_G_PIN, LED_GREEN_CHAN);
    ledcAttachPin(LED_2_B_PIN, LED_BLUE_CHAN);
    ledcAttachPin(LED_3_R_PIN, LED_RED_CHAN);
    ledcAttachPin(LED_3_G_PIN, LED_GREEN_CHAN);
    ledcAttachPin(LED_3_B_PIN, LED_BLUE_CHAN);
    ledcAttachPin(LED_4_R_PIN, LED_RED_CHAN);
    ledcAttachPin(LED_4_G_PIN, LED_GREEN_CHAN);
    ledcAttachPin(LED_4_B_PIN, LED_BLUE_CHAN);

    tickerManager(false);
    shutdownLed();

    // Get wifi SSID and PASSW from SPIFFS
    if (true == getConfig(configFilePath, config)) {
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
        tickerManager(true, BLINK_RED, 2);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(wifiApSsid, wifiApPassw);
        WiFi.setHostname(hostname);
        logger("WiFi AP is ready (IP : " + WiFi.softAPIP().toString() + ")");
        serverConfig();

        clearScreen(screen);
        screen.println("Settings interface is started.");
        screen.println("Open your browser and connect to http://" + String(WiFi.getHostname()));
    } else {
        pinMode(BTN_READ_PIN, INPUT);
        pinMode(BTN_RESTART_PIN, INPUT);
        pinMode(BTN_RESET_PIN, INPUT);

        telegramBot.setToken(config.telegramBotToken);
        telegramBot.on(TELEGRAM_EVT_NEW_MSG, handlerNewMessage);
        clearScreen(screen);
        tickerManager(false);
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

        if (digitalRead(BTN_RESTART_PIN) == LOW) {
            logger("Button restart pressed !");
            tickerManager(true, BLINK_BLUE, 0.5);
            restartRequested = millis();
        }

        if (restartRequested != 0) {
            if (millis() - restartRequested >= DURATION_BEFORE_RESTART ) {
                restart();
            }
        }

        if (digitalRead(BTN_READ_PIN) == LOW) {
            if (false == messageIsReading) {
                displayNewMessage();
            }
        }

        if (digitalRead(BTN_READ_PIN) == HIGH) {
            if (true == messageIsReading) {
                screen.fillScreen(ILI9341_BLACK);
                screen.writeCommand(ILI9341_SLPIN);
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
                tickerManager(true, BLINK_RED, 0.5);
                resetRequested = millis();
            }
        }

        if (resetRequested != 0) {
            if (millis() - resetRequested >= DURATION_BEFORE_RESET ) {
                resetConfig(configFilePath);
                restart();
            }
        }

        #if OTA_ENABLE == true
            logger(F("Start ArduinoOTA handle"));
            ArduinoOTA.handle();
        #endif
    } else {
        server.handleClient();
    }
}