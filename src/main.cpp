#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <UniversalTelegramBot.h>

/* Conditional compilation */
#define MQTT_ENABLE     false
#define OTA_ENABLE      false
#define SCREEN_TYPE     oled

/* General */
#define DEBUG           true
#define SERIAL_BAUDRATE 115200

/* Telegram */
//#define CHECK_MSG_DELAY 1800000
#define CHECK_MSG_DELAY 10000

/* Leds pin */
#define LED_1_PIN       10
#define LED_2_PIN       11
#define LED_3_PIN       12
#define LED_4_PIN       13

/* Switch pin */
#define SWITCH_PIN      14

#if SCREEN_TYPE == oled
    /*#define SCREEN_SCL_PIN  22
    #define SCREEN_SDA_PIN  21
    #include <SPI.h>
    #include <Wire.h>
    #include <Adafruit_GFX.h>
    #include <Adafruit_SSD1306.h>*/
#elif SCREEN_TYPE == tft
    /*#include "SPI.h"
    #include "TFT_eSPI.h"*/
#endif

#if MQTT_ENABLE == true
    // @todo See large message method in exemple
    #define MQTT_MAX_PACKET_SIZE 2048
    #include <PubSubClient.h>
#endif

#if OTA_ENABLE == true
    #include <ArduinoOTA.h>
#endif

void logger(String message, bool endLine = true);

struct Config {
    char wifiSsid[32] = "";
    char wifiPassword[64] = "";
    #if MQTT_ENABLE == true
    bool mqttEnable = true;
    char mqttHost[128] = "";
    int  mqttPort = 1883;
    char mqttUsername[32] = "";
    char mqttPassword[64] = "";
    char mqttPublishChannel[128] = "device/to/marvin";
    char mqttSubscribeChannel[128] = "marvin/to/device";
    #endif
    char uuid[64] = "";
};

struct LastMessage {
    String chatId = "";
    String name = "";
    String message = "";
};

#if MQTT_ENABLE == true
    const char *configFilePath = "/config.json";
    const char *mqttName = "MessageBox";
#else
    const char *configFilePath = "/config_cc.json";
#endif

const char *wifiApSsid = "message-box-wifi-ssid";
const char *wifiApPassw = "message-box-wifi-passw";
const char *appName = "MessageBox";
const char *telegramToken = "885625605:AAEKPq1UljVYaiQ9CdyfIOtE15G8oKTy3v4";

#if OTA_ENABLE == true
    const char *otaPasswordHash = "***** MD5 password *****";
#endif

WiFiClientSecure wifiClient;

#if MQTT_ENABLE == true
    PubSubClient mqttClient;
#endif

Config config;
LastMessage lastMessage;
WebServer server(80);
UniversalTelegramBot bot(telegramToken, wifiClient);

#if SCREEN_TYPE == oled
    //
#elif SCREEN_TYPE == tft
    //TFT_eSPI screen = TFT_eSPI();
#endif

bool wifiConnected = false;
bool startApp = false;
long telegramBotLasttime; 
String errorMessage = "";

#if MQTT_ENABLE == true
    bool mqttConnected = false;
    unsigned long restartRequested = 0;
    unsigned long resetRequested = 0;
#endif

void logger(String message, bool endLine) {
    if (true == DEBUG) {
        if (true == endLine) {
            Serial.println(message);
        } else {
            Serial.print(message);
        }
    }
}

/*unsigned long getMillis() {
    return esp_timer_get_time() / 1000;
}*/

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
        #if MQTT_ENABLE == true
            !json.containsKey("mqttEnable") ||    
            !json.containsKey("mqttHost") ||
            !json.containsKey("mqttPort") ||
            !json.containsKey("mqttUsername") ||
            !json.containsKey("mqttPassword") ||
            !json.containsKey("mqttPublishChannel") ||
            !json.containsKey("mqttSubscribeChannel") ||
        #endif
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

    #if MQTT_ENABLE == true
        config.mqttEnable = json["mqttEnable"] | true;
        strlcpy(config.mqttHost, json["mqttHost"], sizeof(config.mqttHost));
        config.mqttPort = json["mqttPort"] | 1883;
        strlcpy(config.mqttUsername, json["mqttUsername"], sizeof(config.mqttUsername));
        strlcpy(config.mqttPassword, json["mqttPassword"], sizeof(config.mqttPassword));
        strlcpy(config.mqttPublishChannel, json["mqttPublishChannel"], sizeof(config.mqttPublishChannel));
        strlcpy(config.mqttSubscribeChannel, json["mqttSubscribeChannel"], sizeof(config.mqttSubscribeChannel));
    #endif

    strlcpy(config.uuid, json["uuid"], sizeof(config.uuid));

    configFile.close();

    return true;
}

bool setConfig(Config newConfig) {
    StaticJsonDocument<512> json;
    
    json["wifiSsid"] = String(newConfig.wifiSsid);
    json["wifiPassword"] = String(newConfig.wifiPassword);
    #if MQTT_ENABLE == true
        json["mqttEnable"] = newConfig.mqttEnable;
        json["mqttHost"] = String(newConfig.mqttHost);
        json["mqttPort"] = newConfig.mqttPort;
        json["mqttUsername"] = String(newConfig.mqttUsername);
        json["mqttPassword"] = String(newConfig.mqttPassword);
        json["mqttPublishChannel"] = String(newConfig.mqttPublishChannel);
        json["mqttSubscribeChannel"] = String(newConfig.mqttSubscribeChannel);
    #endif

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

bool wifiConnect() {
    unsigned int count = 0;
    WiFi.begin(config.wifiSsid, config.wifiPassword);
    Serial.print(F("Try to connect to "));
    logger(config.wifiSsid);

    while (count < 20) {
        if (WiFi.status() == WL_CONNECTED) {
            logger("");
            Serial.print(F("WiFi connected (IP : "));  
            Serial.print(WiFi.localIP());
            logger(F(")"));

            return true;
        } else {
            delay(500);
            Serial.print(F("."));  
        }

        count++;
    }

    Serial.print(F("Error connection to "));
    logger(String(config.wifiSsid));
    return false;
}

bool checkWifiConfigValues() {
    logger(F("config.wifiSsid length : "), false);
    logger(String(strlen(config.wifiSsid)));

    logger(F("config.wifiPassword length : "), false);
    logger(String(strlen(config.wifiPassword)));

    if ( strlen(config.wifiSsid) > 1 && strlen(config.wifiPassword) > 1 ) {
        return true;
    }

    logger(F("Ssid and passw not present in SPIFFS"));
    return false;
}

#if MQTT_ENABLE == true
    bool mqttConnect() {
        int count = 0;

        while (!mqttClient.connected()) {
            logger("Attempting MQTT connection (host: " + String(config.mqttHost) + ")...");

            if (mqttClient.connect(mqttName, config.mqttUsername, config.mqttPassword)) {
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
#endif

void restart() {
    logger("Restart ESP");
    ESP.restart();
}

void resetConfig() {
    logger(F("Reset ESP"));
    Config resetConfig;
    setConfig(resetConfig);
    logger(F("Restart ESP"));
    restart();
}

void handleHome() {
    String content = "";

    #if MQTT_ENABLE == true
        char indexFile[] = "/index.html";
    #else
        char indexFile[] = "/index_cc.html";
    #endif

    File file = SPIFFS.open(indexFile, FILE_READ);

    if (!file) {
        logger("Failed to open file \"" + String(indexFile) + "\".");
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
        #if MQTT_ENABLE == true
            if (true == config.mqttEnable) {
                content.replace("%MQTT_ENABLE%", "checked");
            } else {
                content.replace("%MQTT_ENABLE%", "");
            }

            content.replace("%MQTT_HOST%", String(config.mqttHost));
            content.replace("%MQTT_PORT%", String(config.mqttPort));
            content.replace("%MQTT_USERNAME%", String(config.mqttUsername));
            content.replace("%MQTT_PASSWD%", String(config.mqttPassword));
            content.replace("%MQTT_PUB_CHAN%", String(config.mqttPublishChannel));
            content.replace("%MQTT_SUB_CHAN%", String(config.mqttSubscribeChannel));
        #endif

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

    Serial.println(server.hasArg("wifiSsid"));
    Serial.println(server.hasArg("wifiPassword"));
    Serial.println(server.arg("wifiSsid"));
    Serial.println(server.arg("wifiPassword"));

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
        #if MQTT_ENABLE == true
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
        #endif

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

            server.send(200, "text/html", content);
        }
    } else {
        Serial.println("Config error, redirect to /");
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
    }
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
    server.on("/restart", HTTP_GET, restart);
    server.on("/bootstrap.min.css", HTTP_GET, handleCss);
    server.on("/favicon.ico", HTTP_GET, handleFavicon);
    server.onNotFound(handleNotFound);

    server.begin();
    logger("HTTP server started");
}

#if MQTT_ENABLE == true
    void callback(char* topic, byte* payload, unsigned int length) {
        StaticJsonDocument<256> json;
        deserializeJson(json, payload, length);

        char response[512];
        
        if (json.containsKey("action")) {
            JsonVariant action = json["action"];

            if (json["action"] == "ping") {
                sprintf(response, "{\"code\":\"200\",\"uuid\":\""+String(config.uuid)+"\",\"actionCalled\":\"%s\",\"payload\":\"pong\"}", action.as<char *>());
            }
            else if (json["action"] == "restart") {
                sprintf(response, "{\"code\":\"200\",\"uuid\":\""+String(config.uuid)+"\",\"actionCalled\":\"%s\",\"payload\":\"Restart in progress\"}", action.as<char *>());
                //restartRequested = getMillis();
                restartRequested = millis();
            }
            else if (json["action"] == "reset") {
                sprintf(response, "{\"code\": \"200\",\"uuid\":\""+String(config.uuid)+"\",\"actionCalled\":\"%s\",\"payload\":\"Reset in progress\"}", action.as<char *>());
                //resetRequested = getMillis();
                resetRequested = millis();
            }
            else {
                sprintf(response, "{\"code\":\"404\",\"uuid\":\""+String(config.uuid)+"\",\"payload\":\"Action %s not found !\"}", action.as<char *>());
            }

            mqttClient.publish(config.mqttPublishChannel, response);
        }

        memset(response, 0, sizeof(response));
    }
#endif

void notification(bool turnOn) {
    if (true == turnOn) {
        digitalWrite(LED_1_PIN, HIGH);
        digitalWrite(LED_2_PIN, HIGH);
        digitalWrite(LED_3_PIN, HIGH);
        digitalWrite(LED_4_PIN, HIGH);
        logger(F("Led on"));
    } else {
        digitalWrite(LED_1_PIN, LOW);
        digitalWrite(LED_2_PIN, LOW);
        digitalWrite(LED_3_PIN, LOW);
        digitalWrite(LED_4_PIN, LOW);
        logger(F("Led off"));
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
            notification(true);
            text.replace("/message", "");
            logger("Content :" + text);

            lastMessage.chatId = chatId;
            lastMessage.name = fromName;
            lastMessage.message = text;

            logger(F("Message waiting to be read"));
            bot.sendSimpleMessage(chatId, "Message reçu. En attente de lecture.", "");
            #if MQTT_ENABLE == true
            mqttClient.publish(
                config.mqttPublishChannel, 
                "{\"code\":\"200\",\"uuid\":\""+String(config.uuid)+"\",\"actionCalled\":\"readMessage\",\"payload\":\"Message reçu. En attente de lecture.\"}"
            );
            #endif
        }

        if (text == "/start") {
            logger(F("Send message to telegram bot (action called : /start)"));
            // @todo concatenation not working
            String welcome = "Welcome to MessageBox, " + fromName + ".\n";
            welcome += "/message [MY_TEXT] : to send message in this box !\n";
            bot.sendSimpleMessage(chatId, welcome, "");
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
        notification(false);
        #if MQTT_ENABLE == true
        mqttClient.publish(
            config.mqttPublishChannel, 
            "{\"code\":\"200\",\"uuid\":\""+String(config.uuid)+"\",\"actionCalled\":\"readMessage\",\"payload\":\"Message lu.\"}"
        );
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
    if (true == getConfig()) {
        if (true == checkWifiConfigValues()) {
            wifiConnected = wifiConnect();
        
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
        WiFi.mode(WIFI_AP);
        WiFi.softAP(wifiApSsid, wifiApPassw);
        logger(F("WiFi AP is ready (IP : "), false); 
        logger(WiFi.softAPIP().toString(), false);
        logger(F(")"));
        serverConfig();
    } else {
        pinMode(LED_1_PIN, OUTPUT);
        pinMode(LED_2_PIN, OUTPUT);
        pinMode(LED_3_PIN, OUTPUT);
        pinMode(LED_4_PIN, OUTPUT);

        notification(false);

        pinMode(SWITCH_PIN, INPUT);

        /*screen.init();
        screen.setRotation(1);
        screen.fillScreen(TFT_BLACK);
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
        screen.setTextSize(1);*/

        logger(F("App started !"));
    }

    #if OTA_ENABLE == true
        ArduinoOTA.setHostname(appName);
        ArduinoOTA.setPasswordHash(otaPasswordHash);

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

        if (restartRequested != 0) {
            //if (getMillis() - restartRequested >= 5000 ) {
            if (millis() - restartRequested >= 5000 ) {
                restart();
            }
        }

        if (resetRequested != 0) {
            //if (getMillis() - resetRequested >= 5000) {
            if (millis() - resetRequested >= 5000) {
                resetConfig();
            }
        }
        #endif

        if (digitalRead(SWITCH_PIN) == HIGH) {
            readMessage();
        }

        //if (getMillis() > telegramBotLasttime + CHECK_MSG_DELAY)  {
        if (millis() > telegramBotLasttime + CHECK_MSG_DELAY)  {
            //telegramBotLasttime = getMillis();
            telegramBotLasttime = millis();
            logger(F("Checking for messages.. "));
            int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

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