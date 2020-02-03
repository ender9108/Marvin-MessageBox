#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <UniversalTelegramBot.h>

#define MQTT_ENABLE false
#define OTA_ENABLE false

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

WiFiClientSecure wifiClient;
#if MQTT_ENABLE == true
PubSubClient mqttClient;
#endif
Config config;
AsyncWebServer server(80);

#if MQTT_ENABLE == true
const char *configFilePath = "/config.json";
const char *mqttName = "MessageBox";
#else
const char *configFilePath = "/config_cc.json";
#endif
const bool debug = true;
const char *wifiApSsid = "message-box-wifi-ssid";
const char *wifiApPassw = "message-box-wifi-passw";
const char *appName = "MessageBox";
const char *telegramToken = "***** TOKEN *****";
#if OTA_ENABLE == true
const char *otaPasswordHash = "***** MD5 password *****";
#endif

bool wifiConnected = false;
bool startApp = false;
int telegramBotMtbs = 1000;
long telegramBotLasttime; 
String errorMessage = "";

#if MQTT_ENABLE == true
bool mqttConnected = false;
unsigned long restartRequested = 0;
unsigned long resetRequested = 0;
#endif

UniversalTelegramBot bot(telegramToken, wifiClient);

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

    logger(F("wifiSsid : "), false);
    logger(String(config.wifiSsid));
    logger(F("wifiPassword : "), false);
    logger(String(config.wifiPassword));
    #if MQTT_ENABLE == true
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
    #endif
    logger(F("uuid : "), false);
    logger(String(config.uuid));

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

String processor(const String& var){
    Serial.println(var);

    if (var == "TITLE" || var == "MODULE_NAME"){
        return String(appName);
    } else if (var == "WIFI_SSID") {
        return String(config.wifiSsid);
    } else if (var == "WIFI_PASSWD") {
        return String(config.wifiPassword);
    }
    #if MQTT_ENABLE == true 
    else if(var == "MQTT_ENABLE") {
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
    } 
    #endif
    else if (var == "ERROR_MESSAGE") {
        return errorMessage;
    } else if (var == "ERROR_HIDDEN") {
        if (errorMessage.length() == 0) {
            return String("d-none");
        }
    }

    return String();
}

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

void serverConfig() {
    server.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
        #if MQTT_ENABLE == true
        request->send(SPIFFS, "/index.html", "text/html", false, processor);
        #else
        request->send(SPIFFS, "/index_cc.html", "text/html", false, processor);
        #endif
    });
    server.on("/bootstrap.min.css", HTTP_GET, [] (AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/bootstrap.min.css", "text/css");
    });
    server.on("/save", HTTP_POST, [] (AsyncWebServerRequest *request) {
        int params = request->params();

        #if MQTT_ENABLE == true
        if (request->hasParam("mqttEnable", true)) {
            config.mqttEnable = true;
        } else {
            config.mqttEnable = false;
        }
        #endif

        for (int i = 0 ; i < params ; i++) {
            AsyncWebParameter* p = request->getParam(i);

            if (p->name() == "wifiSsid") {
                strlcpy(config.wifiSsid, p->value().c_str(), sizeof(config.wifiSsid));
            } else if (p->name() == "wifiPasswd") {
                strlcpy(config.wifiPassword, p->value().c_str(), sizeof(config.wifiPassword));
            } 
            #if MQTT_ENABLE == true
            else if (p->name() == "mqttHost") {
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
            #endif
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

#if MQTT_ENABLE == true
void callback(char* topic, byte* payload, unsigned int length) {
    StaticJsonDocument<256> json;
    deserializeJson(json, payload, length);

    char response[512];
    
    if (json.containsKey("action")) {
        JsonVariant action = json["action"];

        if (json["action"] == "ping") {
            sprintf(response, "{\"code\": \"200\", \"actionCalled\": \"%s\" \"payload\": \"pong\"}", action.as<char *>());
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
#endif

void handleNewMessages(int numNewMessages) {

}

void setup() {
    Serial.begin(115200);
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
        logger(F("App started !"));
    }

    #if OTA_ENABLE == true
    // Port defaults to 3232
    // ArduinoOTA.setPort(3232);

    // Hostname defaults to esp3232-[MAC]
    ArduinoOTA.setHostname(appName);

    // No authentication by default
    // ArduinoOTA.setPassword("admin");

    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
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
            if (getMillis() - restartRequested >= 5000 ) {
                restart();
            }
        }

        if (resetRequested != 0) {
            if (getMillis() - resetRequested >= 5000) {
                resetConfig();
            }
        }
        #endif

        if (getMillis() > telegramBotLasttime + telegramBotMtbs)  {
            telegramBotLasttime = getMillis();
            logger(F("Checking for messages.. "));
            int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

            if (numNewMessages > 0) {
                handleNewMessages(numNewMessages);
            }
        }
    }

    #if OTA_ENABLE == true
    ArduinoOTA.handle();
    #endif
}