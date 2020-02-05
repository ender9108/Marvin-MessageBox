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

long telegramBotLasttime;
bool wifiShouldSaveConfig = false;
unsigned int previousResetBtnState = 0;
unsigned long resetBtnDuration = 0;
unsigned long resetRequested = 0;
unsigned long restartRequested = 0;

#if MQTT_ENABLE == true
    bool mqttConnected = false;
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
            //JsonVariant action = json["action"];

            /*if (json["action"] == "ping") {
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

            mqttClient.publish(config.mqttPublishChannel, response);*/
        }

        memset(response, 0, sizeof(response));
    }
#endif

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
    blinkLed();
}

void shutdownLed() {
    digitalWrite(LED_1_PIN, LOW);
    digitalWrite(LED_2_PIN, LOW);
    digitalWrite(LED_3_PIN, LOW);
    digitalWrite(LED_4_PIN, LOW);
}

void tickerManager(bool start) {
    shutdownLed();

    if (true == start) {
        ticker.attach(1, tickerBlinkLed);
    } else {
        ticker.detach();
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
            tickerManager(true);
            text.replace("/message", "");

            lastMessage.chatId = chatId;
            lastMessage.name = fromName;
            lastMessage.message = text;

            logger(F("Message waiting to be read"));
            bot.sendSimpleMessage(chatId, "Message reçu. En attente de lecture.", "");
            #if MQTT_ENABLE == true
            char response[512];
            sprintf(response, "{\"code\":\"200\",\"uuid\":\"%s\",\"actionCalled\":\"readMessage\",\"payload\":\"Message reçu. En attente de lecture.\"}", config.uuid);
            mqttClient.publish(
                config.mqttPublishChannel, 
                response
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
        char response[512];
        sprintf(response, "{\"code\":\"200\",\"uuid\":\"%s\",\"actionCalled\":\"readMessage\",\"payload\":\"Message lu.\"}", config.uuid);
        mqttClient.publish(
            config.mqttPublishChannel, 
            response
        );
        #endif
        
        lastMessage.chatId = "";
        lastMessage.name = "";
        lastMessage.message = "";
    }
}

#if MQTT_ENABLE == true
    void wifiSaveConfigCallback() {
        logger("Wifi - Should save config");
        wifiShouldSaveConfig = true;
    }
#endif

void wifiConfigModeCallback (WiFiManager *myWiFiManager) {
  logger(F("Entered config mode"));
  logger(WiFi.softAPIP().toString());

  tickerManager(true);
}

void setup() {
    Serial.begin(SERIAL_BAUDRATE);
    logger(F("Start program !"));

    if (SPIFFS.begin(true)) {
        logger(F("SPIFFS mounted"));

        if (false == getConfig(configFilePath, config)) {
            logger(F("An Error has occurred while loading config"));
        }
    } else {
        logger(F("An Error has occurred while mounting SPIFFS"));
    }

    pinMode(BTN_READ_PIN, INPUT);
    pinMode(BTN_RESTART_PIN, INPUT);
    pinMode(BTN_RESET_PIN, INPUT);
    pinMode(LED_1_PIN, OUTPUT);
    pinMode(LED_2_PIN, OUTPUT);
    pinMode(LED_3_PIN, OUTPUT);
    pinMode(LED_4_PIN, OUTPUT);
    digitalWrite(LED_1_PIN, LOW);
    digitalWrite(LED_2_PIN, LOW);
    digitalWrite(LED_3_PIN, LOW);
    digitalWrite(LED_4_PIN, LOW);

    WiFi.mode(WIFI_STA);
    WiFiManager wm;
    #if MQTT_ENABLE == true
        wm.setSaveConfigCallback(wifiSaveConfigCallback);
    #endif
    wm.setAPCallback(wifiConfigModeCallback);

    /*IPAddress _ip(192,168,4,2);
    IPAddress _gw(192,168,4,1);
    IPAddress _sn(255,255,255,255);

    wm.setSTAStaticIPConfig(_ip, _gw, _sn);
    wm.setHostname(hostname);*/
    wm.setClass("invert");

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

    if (!wm.autoConnect(wifiApSsid, wifiApPassw)) {
        logger("WifiManager - failed to connect and hit timeout");
        delay(3000);
        // if we still have not connected restart and try all over again
        restart();
        delay(5000);
    }

    #if MQTT_ENABLE == true
        strcpy(config.mqttHost,             cmMqttHost.getValue());
        strcpy(config.mqttPort,             cmMqttPort.getValue());
        strcpy(config.mqttUsername,         cmMqttUsername.getValue());
        strcpy(config.mqttPassword,         cmMqttPassword.getValue());
        strcpy(config.mqttPublishChannel,   cmMqttPublishChannel.getValue());
        strcpy(config.mqttSubscribeChannel, cmMqttSubscribeChannel.getValue());

        if (wifiShouldSaveConfig) {
            setConfig(configFilePath, config);
            wifiShouldSaveConfig = false;
        }
    #endif

    tickerManager(false);

    logger(F("IP: "), false);
    logger(WiFi.localIP().toString());

    #if MQTT_ENABLE == true
        mqttClient.setClient(wifiClient);
        mqttClient.setServer(config.mqttHost, atoi(config.mqttPort));
        mqttClient.setCallback(callback);
        mqttConnected = mqttConnect();

        if (false == mqttConnected) {
            wm.resetSettings();
            restart();
        }
    #endif

    /**
     * Screen init
     */

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

    logger(F("App started !"));
}

void loop() {
    #if MQTT_ENABLE == true
        if (!mqttClient.connected()) {
            mqttConnect();
        }

        mqttClient.loop();
    #endif

    if (digitalRead(BTN_READ_PIN) == HIGH) {
        readMessage();
    }

    if (digitalRead(BTN_RESTART_PIN) == HIGH) {
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
            blinkLed(5, 250);
            resetRequested = millis();
        }
    }

    if (digitalRead(BTN_RESTART_PIN) == LOW){                              //extinction de la led 1 si le bouton est relaché
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