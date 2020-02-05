#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <UniversalTelegramBot.h>
#include <Ticker.h>
#include <ESPTools.h>
#include <ESPConfig.h>

/* Conditional compilation */
#define MQTT_ENABLE     false
#define OTA_ENABLE      false

/* General */
#define SERIAL_BAUDRATE 115200

/* Telegram */
#define CHECK_MSG_DELAY 5000

/* Leds pin */
#define LED_1_PIN       10
#define LED_2_PIN       11
#define LED_3_PIN       12
#define LED_4_PIN       13

/* Switch pin */
#define SWITCH_PIN      14

#if SCREEN_TYPE == oled
// define and include
#elif SCREEN_TYPE == tft
// define and include
#endif

#if MQTT_ENABLE == true
// @todo See large message method in exemple
#define MQTT_MAX_PACKET_SIZE 2048
#include <PubSubClient.h>
#endif

#if OTA_ENABLE == true
#include <ArduinoOTA.h>
#endif

struct LastMessage {
    String chatId = "";
    String name = "";
    String message = "";
};

const char *configFilePath  = "/config.json";
const char *wifiApSsid      = "MarvinMessageBoxSsid";
const char *wifiApPassw     = "MarvinMessageBox";
const char *appName         = "MessageBox";
const char *telegramToken   = "***** TOKEN *****";

#if OTA_ENABLE == true
const char *otaPasswordHash = "***** MD5 password *****";
#endif

WiFiClientSecure wifiClient;
Ticker ticker;

#if MQTT_ENABLE == true
PubSubClient mqttClient;
#endif

Config config;
LastMessage lastMessage;
UniversalTelegramBot bot(telegramToken, wifiClient);

#if SCREEN_TYPE == oled
// INIT SCREEN LIBRARY OLED
#elif SCREEN_TYPE == tft
// INIT SCREEN LIBRARY TFT
#endif

bool startApp = false;
long telegramBotLasttime;
bool wifiShouldSaveConfig = false;

#if MQTT_ENABLE == true
bool mqttConnected = false;
unsigned long restartRequested = 0;
unsigned long resetRequested = 0;
#endif

#if MQTT_ENABLE == true
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
#endif

void restart() {
    logger("Restart ESP");
    ESP.restart();
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
            restartRequested = millis();
        }
        else if (json["action"] == "reset") {
            sprintf(response, "{\"code\": \"200\",\"uuid\":\""+String(config.uuid)+"\",\"actionCalled\":\"%s\",\"payload\":\"Reset in progress\"}", action.as<char *>());
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

void tick() {
    digitalWrite(LED_1_PIN, !digitalRead(LED_1_PIN));
    digitalWrite(LED_2_PIN, !digitalRead(LED_2_PIN));
    digitalWrite(LED_3_PIN, !digitalRead(LED_3_PIN));
    digitalWrite(LED_4_PIN, !digitalRead(LED_4_PIN));
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
            notification(true);
            text.replace("/message", "");

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
            String welcome = "Welcome to MessageBox, " + fromName + ".\n";
            welcome += "/message [MY_TEXT] : to send message in this box !\n";
            bot.sendSimpleMessage(chatId, welcome, "Markdown");
        }
  }
}

void readMessage() {
    if (lastMessage.message.trim() == "") {
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

void wifiSaveConfigCallback() {
    logger("Wifi - Should save config");
    wifiShouldSaveConfig = true;
}

void wifiConfigModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void setup() {
    Serial.begin(SERIAL_BAUDRATE);
    logger(F("Start program !"));

    bool wifiConnected = false;

    if (SPIFFS.begin(true)) {
        logger(F("SPIFFS mounted"));

        if (false == getConfig(configFilePath)) {
            logger(F("An Error has occurred while loading config"));
        }
    } else {
        logger(F("An Error has occurred while mounting SPIFFS"));
    }

    pinMode(LED_1_PIN, OUTPUT);
    pinMode(LED_2_PIN, OUTPUT);
    pinMode(LED_3_PIN, OUTPUT);
    pinMode(LED_4_PIN, OUTPUT);
    notification(false);

    WiFi.mode(WIFI_STA);
    WiFiManager wm;
    wm.setSaveConfigCallback(wifiSaveConfigCallback);
    wm.setAPCallback(wifiConfigModeCallback);

    IPAddress _ip(192,168,4,2);
    IPAddress _gw(192,168,4,1);
    IPAddress _sn(255,255,255,255);

    wm.setSTAStaticIPConfig(_ip, _gw, _sn);

    #if MQTT_ENABLE == true
    WiFiManagerParameter cmMqttHost("Host", "mqtt host", config.mqttHost, 128);
    WiFiManagerParameter cmMqttPort("Port", "mqtt port", config.mqttPort, 6);
    WiFiManagerParameter cmMqttUsername("Username", "mqtt username", config.mqttUsername, 32);
    WiFiManagerParameter cmMqttPassword("Password", "mqtt password", config.mqttPassword, 64);
    WiFiManagerParameter cmMqttPublishChannel("Publish channel", "mqtt publish channel", config.mqttPublishChannel, 128);
    WiFiManagerParameter cmMqttSubscribeChannel("Subscribe channel", "mqtt subscribe channel", config.mqttSubscribeChannel, 128);

    //add all your parameters here
    wm.addParameter(&cmMqttHost);
    wm.addParameter(&cmMqttPort);
    wm.addParameter(&cmMqttUsername);
    wm.addParameter(&cmMqttPassword);
    wm.addParameter(&cmMqttPublishChannel);
    wm.addParameter(&cmMqttSubscribeChannel);
    #endif

    //wifiApSsid
    if (!wm.autoConnect(wifiApSsid, wifiApPassw)) {
        logger("WifiManager - failed to connect and hit timeout");
        delay(3000);
        // if we still have not connected restart and try all over again
        restart();
        delay(5000);
    }

    strcpy(config.mqttHost,             cmMqttHost.getValue());
    strcpy(config.mqttPort,             cmMqttPort.getValue());
    strcpy(config.mqttUsername,         cmMqttUsername.getValue());
    strcpy(config.mqttPassword,         cmMqttPassword.getValue());
    strcpy(config.mqttPublishChannel,   cmMqttPublishChannel.getValue());
    strcpy(config.mqttSubscribeChannel, cmMqttSubscribeChannel.getValue());

    ticker.detach();
    notification(false);

    /*
    // Get wifi SSID and PASSW from SPIFFS
    if (true == getConfig(configFilePath)) {
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
    } // endif true == getConfig(configFilePath)

    if (false == wifiConnected) {
        errorMessage = "Wifi connection error to " + String(config.wifiSsid);
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

        screen.init();
        screen.setRotation(1);
        screen.fillScreen(TFT_BLACK);
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
        screen.setTextSize(1);

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
    */
}

void loop() {
    if (true == startApp) {
        #if MQTT_ENABLE == true
        if (!mqttClient.connected()) {
            mqttConnect();
        }

        mqttClient.loop();

        if (restartRequested != 0) {
            if (millis() - restartRequested >= 5000 ) {
                restart();
            }
        }

        if (resetRequested != 0) {
            if (millis() - resetRequested >= 5000) {
                resetConfig();
                restart();
            }
        }
        #endif

        if (digitalRead(SWITCH_PIN) == HIGH) {
            readMessage();
        }

        if (millis() > telegramBotLasttime + CHECK_MSG_DELAY)  {
            telegramBotLasttime = millis();
            logger(F("Checking for messages.. "));
            int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

            if (numNewMessages > 0) {
                logger(F("New message !"));
                handleNewMessages(numNewMessages);
            }
        }

        #if OTA_ENABLE == true
            logger(F("Start ArduinoOTA handle"));
            ArduinoOTA.handle();
        #endif
    }
}