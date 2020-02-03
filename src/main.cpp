#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <UniversalTelegramBot.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

void logger(String message, bool endLine = true);

struct Config {
    char wifiSsid[32] = "";
    char wifiPassword[64] = "";
    bool mqttEnable = true;
    char mqttHost[128] = "";
    int  mqttPort = 1883;
    char mqttUsername[32] = "";
    char mqttPassword[64] = "";
    char mqttPublishChannel[128] = "device/to/marvin";
    char mqttSubscribeChannel[128] = "marvin/to/device";
    char uuid[64] = "";
};

/* ***** pin component ***** */

const char *configFilePath = "/config.json";
const bool debug = true;
const char *mqttName = "MyLoveBox";
const char *wifiApSsid = "love-box-wifi-ssid";
const char *wifiApPassw = "love-box-wifi-passw";
const char *appName = "My love box";
const char *telegramToken = "991468015:AAFcBsqCqDlwecDNOIztzgxGbDkPYPrllhg";

bool wifiConnected = false;
bool mqttConnected = false;
bool startApp = false;
int telegramBotMtbs = 1000;
long telegramBotLasttime; 
String errorMessage = "";

WiFiClientSecure wifiClient;
PubSubClient mqttClient;
Config config;
AsyncWebServer server(80);
UniversalTelegramBot bot(telegramToken, wifiClient);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

void logger(String message, bool endLine) {
    if (true == debug) {
        if (true == endLine) {
            Serial.println(message);
        } else {
            Serial.print(message);
        }
    }
}

unsigned long getMillis() {
    return esp_timer_get_time() / 1000;
}

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
        !json.containsKey("mqttEnable") ||
        !json.containsKey("mqttHost") ||
        !json.containsKey("mqttPort") ||
        !json.containsKey("mqttUsername") ||
        !json.containsKey("mqttPassword") ||
        !json.containsKey("mqttPublishChannel") ||
        !json.containsKey("mqttSubscribeChannel") ||
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
    config.mqttEnable = json["mqttEnable"] | true;
    strlcpy(config.mqttHost, json["mqttHost"], sizeof(config.mqttHost));
    config.mqttPort = json["mqttPort"] | 1883;
    strlcpy(config.mqttUsername, json["mqttUsername"], sizeof(config.mqttUsername));
    strlcpy(config.mqttPassword, json["mqttPassword"], sizeof(config.mqttPassword));
    strlcpy(config.mqttPublishChannel, json["mqttPublishChannel"], sizeof(config.mqttPublishChannel));
    strlcpy(config.mqttSubscribeChannel, json["mqttSubscribeChannel"], sizeof(config.mqttSubscribeChannel));
    strlcpy(config.uuid, json["uuid"], sizeof(config.uuid));

    configFile.close();

    logger(F("wifiSsid : "), false);
    logger(String(config.wifiSsid));
    logger(F("wifiPassword : "), false);
    logger(String(config.wifiPassword));
    logger(F("mqttHost : "), false);
    logger(String(config.mqttHost));
    logger(F("mqttPort : "), false);
    logger(String(config.mqttPort));
    logger(F("mqttUsername : "), false);
    logger(String(config.mqttUsername));
    logger(F("mqttPassword : "), false);
    logger(String(config.mqttPassword));
    logger(F("mqttPublishChannel : "), false);
    logger(String(config.mqttPublishChannel));
    logger(F("mqttSubscribeChannel : "), false);
    logger(String(config.mqttSubscribeChannel));
    logger(F("uuid : "), false);
    logger(String(config.uuid));

    return true;
}

bool setConfig(Config newConfig) {
    StaticJsonDocument<512> json;

    json["wifiSsid"] = String(newConfig.wifiSsid);
    json["wifiPassword"] = String(newConfig.wifiPassword);
    json["mqttEnable"] = newConfig.mqttEnable;
    json["mqttHost"] = String(newConfig.mqttHost);
    json["mqttPort"] = newConfig.mqttPort;
    json["mqttUsername"] = String(newConfig.mqttUsername);
    json["mqttPassword"] = String(newConfig.mqttPassword);
    json["mqttPublishChannel"] = String(newConfig.mqttPublishChannel);
    json["mqttSubscribeChannel"] = String(newConfig.mqttSubscribeChannel);

    if (strlen(newConfig.uuid) == 0) {
        uint32_t tmpUuid = esp_random();
        String(tmpUuid).toCharArray(config.uuid, 64);
    }

    json["uuid"] = String(config.uuid);

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
    errorMessage = "Wifi connection error to " + String(config.wifiSsid);
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

bool mqttConnect() {
    int count = 0;

    while (!mqttClient.connected()) {
        logger("Attempting MQTT connection (host: " + String(config.mqttHost) + ")...");
        // Attempt to connect
        if (mqttClient.connect(mqttName, config.mqttUsername, config.mqttPassword)) {
            logger(F("connected !"));
            mqttClient.subscribe(config.mqttSubscribeChannel);
            return true;
        } else {
            logger(F("failed, rc="), false);
            logger(String(mqttClient.state()));
            logger(F("try again in 5 seconds"));
            // Wait 5 seconds before retrying
            delay(5000);

            if (count == 10) {
              errorMessage = "Mqtt connection error to " + String(config.mqttHost);
              return false;
            }
        }

        count++;
    }

    errorMessage = "Mqtt connection error to " + String(config.mqttHost);
    return false;
}

String processor(const String& var){
    Serial.println(var);

    if (var == "TITLE" || var == "MODULE_NAME"){
        return String(appName);
    } else if (var == "WIFI_SSID") {
        return String(config.wifiSsid);
    } else if (var == "WIFI_PASSWD") {
        return String(config.wifiPassword);
    } else if(var == "MQTT_ENABLE") {
        if (true == config.mqttEnable) {
            return String("checked");
        }
    } else if (var == "MQTT_HOST") {
        return String(config.mqttHost);
    } else if (var == "MQTT_PORT") {
        return String(config.mqttPort);
    } else if (var == "MQTT_USERNAME") {
        return String(config.mqttUsername);
    } else if (var == "MQTT_PASSWD") {
        return String(config.mqttPassword);
    } else if (var == "MQTT_PUB_CHAN") {
        return String(config.mqttPublishChannel);
    } else if (var == "MQTT_SUB_CHAN") {
        return String(config.mqttSubscribeChannel);
    } else if (var == "ERROR_MESSAGE") {
        return errorMessage;
    } else if (var == "ERROR_HIDDEN") {
        if (errorMessage.length() == 0) {
        return String("d-none");
        }
    }

    return String();
}

void restart() {
  ESP.restart();
}

void serverConfig() {
    server.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html", false, processor);
    });

    server.on("/bootstrap.min.css", HTTP_GET, [] (AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/bootstrap.min.css", "text/css");
    });

    server.on("/save", HTTP_POST, [] (AsyncWebServerRequest *request) {
        int params = request->params();

        if (request->hasParam("mqttEnable", true)) {
            config.mqttEnable = true;
        } else {
            config.mqttEnable = false;
        }

        for (int i = 0 ; i < params ; i++) {
            AsyncWebParameter* p = request->getParam(i);

            if (p->name() == "wifiSsid") {
                strlcpy(config.wifiSsid, p->value().c_str(), sizeof(config.wifiSsid));
            } else if (p->name() == "wifiPasswd") {
                strlcpy(config.wifiPassword, p->value().c_str(), sizeof(config.wifiPassword));
            } else if (p->name() == "mqttHost") {
                strlcpy(config.mqttHost, p->value().c_str(), sizeof(config.mqttHost));
            } else if (p->name() == "mqttPort") {
                config.mqttPort = p->value().toInt();
            } else if (p->name() == "mqttUsername") {
                strlcpy(config.mqttUsername, p->value().c_str(), sizeof(config.mqttUsername));
            } else if (p->name() == "mqttPasswd") {
                strlcpy(config.mqttPassword, p->value().c_str(), sizeof(config.mqttPassword));
            } else if (p->name() == "mqttPublishChannel") {
                strlcpy(config.mqttPublishChannel, p->value().c_str(), sizeof(config.mqttPublishChannel));
            } else if (p->name() == "mqttSubscribeChannel") {
                strlcpy(config.mqttSubscribeChannel, p->value().c_str(), sizeof(config.mqttSubscribeChannel));
            }
        }
        // save config
        setConfig(config);

        request->send(SPIFFS, "/restart.html", "text/html", false, processor);
    });

    server.on("/restart", HTTP_GET, [] (AsyncWebServerRequest *request) {
        restart();
    });

    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/404.html", "text/html", false, processor);
    });

    server.begin();
    logger("HTTP server started");
}

void callback(char* topic, byte* payload, unsigned int length) {
    StaticJsonDocument<256> json;
    deserializeJson(json, payload, length);
    
    char response[1280];
    
    if (json.containsKey("action")) {
        JsonVariant action = json["action"];

        if (json["action"] == "ping") {
        sprintf(response, "{\"code\": \"200\", \"actionCalled\": \"%s\" \"payload\": \"pong\"}", action.as<char *>());
        } 
        else if (json["action"] == "status") {
        int status = 0;

        if(lightOn == true) {
            status = 1;
        }

        if (restartRequested != 0) {
            sprintf(response, "{\"code\": \"200\", \"actionCalled\": \"%s\", \"payload\": \"Restart in progress\"}", action.as<char *>());
        } else {
            sprintf(response, "{\"code\": \"200\", \"actionCalled\": \"%s\", \"payload\": \"%d\"}", action.as<char *>(), status);
        }
        else if (json["action"] == "configure") {
            //String message  = "{\"code\":\"200\",\"actionCalled\":\"\",\"payload\":{\"ip\":\"\",\"Mac address\":\"\",\"protocol\":\"mqtt\",\"port\":\"\",\"actions\":[%ACTIONS_LIST%]}}";
            //message = message.replace('%ACTIONS_LIST%')
            //message.toCharArray(response, 1280);
        }
        else if (json["action"] == "restart") {
            sprintf(response, "{\"code\": \"200\", \"actionCalled\": \"%s\", \"payload\": \"Restart in progress\"}", action.as<char *>());
            restartRequested = getMillis();
        }
        else if (json["action"] == "reset") {
            sprintf(response, "{\"code\": \"200\", \"actionCalled\": \"%s\", \"payload\": \"Reset in progress\"}", action.as<char *>());
            resetRequested = getMillis();
        }
        else {
            sprintf(response, "{\"code\": \"404\", \"payload\": \"Action %s not found !\"}", action.as<char *>());
        }

        mqttClient.publish(config.mqttPublishChannel, response);
    }

    memset(response, 0, sizeof(response));
}

void resetConfig() {
    logger(F("Reset ESP"));
    Config resetConfig;
    setConfig(resetConfig);
    logger(F("Restart ESP"));
    resetRequested = 0;
    restartRequested = 0;
    blinkLed();
    restart();
}

void setup() {
    Serial.begin(115200);
    logger(F("Start program !"));

    if (!SPIFFS.begin(true)) {
        logger(F("An Error has occurred while mounting SPIFFS"));
        for(;;); // Don't proceed, loop forever
    }

    logger(F("SPIFFS mounted"));

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        Serial.println("SSD1306 allocation failed");
        for(;;); // Don't proceed, loop forever
    }

    delay(2000);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);

    // Get wifi SSID and PASSW from SPIFFS
    if (true == getConfig()) {
        if (true == checkWifiConfigValues()) {
            wifiConnected = wifiConnect();

            if (true == wifiConnected && true == config.mqttEnable) {
                mqttClient.setClient(wifiClient);
                mqttClient.setServer(config.mqttHost, config.mqttPort);
                mqttClient.setCallback(callback);
                mqttConnected = mqttConnect();
            }
        }
    } // endif true == getConfig()

    if (false == wifiConnected) {
        startApp = false;
    } else if (
        true == wifiConnected &&
        true == config.mqttEnable && 
        false == mqttConnected
    ) {
        startApp = false;
    } else {
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
        logger(F("App started !"));
    }
}

void loop() {
    if (true == startApp) {
        if (restartRequested != 0) {
            if (getMillis() - restartRequested >= 5000 ) {
                logger(F("Restart ESP"));
                restartRequested = 0;
                restart();
            }
        }

        if (resetRequested != 0) {
            if (getMillis() - resetRequested >= 5000) {
                resetConfig();
            }
        }

        if (millis() > telegramBotLasttime + telegramBotMtbs)  {
            telegramBotLasttime = getMillis();
            Serial.print(F("Checking for messages.. "));
            int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

            if (numNewMessages > 0) {
                //handleNewMessages(numNewMessages);
            }
        }

        mqttClient.loop();
    }
}