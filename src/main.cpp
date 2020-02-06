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

WiFiClientSecure wifiClient;
Ticker ticker;

#if MQTT_ENABLE == true
    PubSubClient mqttClient;
#endif

Config config;
LastMessage lastMessage;
WebServer server(80);
UniversalTelegramBot bot(wifiClient);

#if SCREEN_TYPE == oled
    // INIT SCREEN LIBRARY OLED
#elif SCREEN_TYPE == tft
    // INIT SCREEN LIBRARY TFT
#endif

bool wifiConnected = false;
bool startApp = false;
long telegramBotLasttime; 
String errorMessage = "";
unsigned int previousResetBtnState = 0;
unsigned long resetBtnDuration = 0;
unsigned long resetRequested = 0;
unsigned long restartRequested = 0;

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

void blinkLed(int repeat, int time) {
    if (repeat == 0) {
        digitalWrite(LED_1_PIN, !digitalRead(LED_1_PIN));
        digitalWrite(LED_2_PIN, !digitalRead(LED_2_PIN));
        digitalWrite(LED_3_PIN, !digitalRead(LED_3_PIN));
        digitalWrite(LED_4_PIN, !digitalRead(LED_4_PIN));
    } else {
        for (int i = 0; i < repeat; i++) {
            digitalWrite(LED_1_PIN, !digitalRead(LED_1_PIN));
            digitalWrite(LED_2_PIN, !digitalRead(LED_2_PIN));
            digitalWrite(LED_3_PIN, !digitalRead(LED_3_PIN));
            digitalWrite(LED_4_PIN, !digitalRead(LED_4_PIN));
            delay(time);
        }
    }
}

void tickerBlinkLed() {
    logger("tickerBlinkLed !");
    blinkLed();
}

void shutdownLed() {
    digitalWrite(LED_1_PIN, LOW);
    digitalWrite(LED_2_PIN, LOW);
    digitalWrite(LED_3_PIN, LOW);
    digitalWrite(LED_4_PIN, LOW);
}

void tickerManager(bool start, float timer) {
    shutdownLed();

    if (true == start) {
        logger(F("Start ticker !"));
        ticker.attach(timer, tickerBlinkLed);
        // @todo led color red
    } else {
        logger(F("Stop ticker !"));
        ticker.detach();
        // @todo led color blue
    }
}

void handleNewMessages(int numNewMessages) {
    for (int i=0; i<numNewMessages; i++) {
        String chatId = String(bot.messages[i].chat_id);
        String text = bot.messages[i].text;
        String fromName = bot.messages[i].from_name;

        logger("Chat id :" + chatId);
        logger("From name :" + fromName);
        logger("Text :" + text);

        if (fromName == "") {
            logger(F("No from name (Guest)"));
            fromName = "Guest";
        }

        if (text.indexOf("/message") == 0) {
            logger(F("New message !"));
            tickerManager(true);
            text.replace("/message", "");
            logger("Content :" + text);

            lastMessage.chatId = chatId;
            lastMessage.name = fromName;
            lastMessage.message = text;

            logger(F("Message waiting to be read"));
            bot.sendSimpleMessage(chatId, "Message reçu. En attente de lecture.", "");
            #if MQTT_ENABLE == true
                char response[256];
                sprintf(response, "{\"code\":\"200\",\"uuid\":\"%s\",\"actionCalled\":\"newMessageReceived\",\"payload\":\"Message reçu. En attente de lecture.\"}", config.uuid);
                mqttClient.publish(config.mqttPublishChannel, response);
            #endif
        }

        if (text == "/start") {
            logger(F("Send message to telegram bot (action called : /start)"));
            // @todo concatenation not working
            String welcome = "Welcome to MessageBox, " + fromName + ".\n";
            welcome += "/message [MY_TEXT] : to send message in this box !\n";
            bot.sendSimpleMessage(chatId, welcome, "Markdown");
        }
  }
}

void readMessage() {
    lastMessage.message.trim();

    if (lastMessage.message == "") {
        logger(F("No new message"));
        // Display "Pas de nouveau message";
    } else {
        // Display message on screen
        logger(F("Display message on screen"));

        logger(F("Send confirmation read message to bot"));
        bot.sendSimpleMessage(lastMessage.chatId, "Message lu !", "");
        tickerManager(false);
        
        #if MQTT_ENABLE == true
            char response[256];
            sprintf(response, "{\"code\":\"200\",\"uuid\":\"%s\",\"actionCalled\":\"readMessage\",\"payload\":\"Message lu.\"}", config.uuid);
            mqttClient.publish(config.mqttPublishChannel, response);
        #endif
        
        lastMessage.chatId = "";
        lastMessage.name = "";
        lastMessage.message = "";
    }
}

void setup() {
    Serial.begin(SERIAL_BAUDRATE);
    logger(F("Start program !"));

    if (!SPIFFS.begin(true)) {
        logger(F("An Error has occurred while mounting SPIFFS"));
        return;
    }

    logger(F("SPIFFS mounted"));

    // Get wifi SSID and PASSW from SPIFFS
    if (true == getConfig(configFilePath, config)) {
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
        if (strlen(config.wifiSsid) > 1) {
            errorMessage = "Wifi connection error to " + String(config.wifiSsid);
        }
        startApp = false;
    } 
    #if MQTT_ENABLE == true
        else if (
            true == wifiConnected &&
            true == config.mqttEnable && 
            false == mqttConnected
        ) {
            errorMessage = "Mqtt connection error to " + String(config.mqttHost);
            startApp = false;
        }
    #endif
    else {
        startApp = true;
    }

    if (false == startApp) {
        tickerManager(true);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(wifiApSsid, wifiApPassw);
        WiFi.setHostname(hostname);
        logger(F("WiFi AP is ready (IP : "), false); 
        logger(WiFi.softAPIP().toString(), false);
        logger(F(")"));
        serverConfig();
    } else {
        pinMode(BTN_READ_PIN, INPUT);
        pinMode(BTN_RESTART_PIN, INPUT);
        pinMode(BTN_RESET_PIN, INPUT);
        pinMode(LED_1_PIN, OUTPUT);
        pinMode(LED_2_PIN, OUTPUT);
        pinMode(LED_3_PIN, OUTPUT);
        pinMode(LED_4_PIN, OUTPUT);

        tickerManager(false);
        shutdownLed();

        /*screen.init();
        screen.setRotation(1);
        screen.fillScreen(TFT_BLACK);
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
        screen.setTextSize(1);*/

        bot.setToken(config.telegramBotToken);
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

        if (digitalRead(BTN_READ_PIN) == HIGH) {
            readMessage();
        }

        if (digitalRead(BTN_RESTART_PIN) == HIGH) {
            tickerManager(true, 0.5);
            restartRequested = millis();
        }

        if (digitalRead(BTN_RESTART_PIN) == HIGH && previousResetBtnState == LOW) {
            previousResetBtnState = HIGH;
            resetBtnDuration = millis();
        }

        if (
            resetBtnDuration != 0 &&
            digitalRead(BTN_RESTART_PIN) == HIGH && 
            previousResetBtnState == HIGH
        ) {
            if (millis() - resetBtnDuration >= DURATION_BTN_RESET_PRESS) {
                tickerManager(true, 0.5);
                resetRequested = millis();
            }
        }

        if (digitalRead(BTN_RESTART_PIN) == LOW) {
            previousResetBtnState = LOW;
            resetBtnDuration = 0;
            resetRequested = 0;
        }

        if (restartRequested != 0) {
            if (millis() - restartRequested >= DURATION_BEFORE_RESTART ) {
                restart();
            }
        }

        if (resetRequested != 0) {
            if (millis() - resetRequested >= DURATION_BEFORE_RESET) {
                resetConfig(configFilePath);
                restart();
            }
        }

        if (millis() > telegramBotLasttime + CHECK_MSG_DELAY)  {
            telegramBotLasttime = millis();
            logger(F("Checking for messages.. "));
            int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
            logger("Num: " + String(numNewMessages));
            if (numNewMessages > 0) {
                handleNewMessages(numNewMessages);
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