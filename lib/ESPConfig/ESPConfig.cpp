#include <ESPConfig.h>

bool getConfig(char *configPath) {
    File configFile = SPIFFS.open(configFilePath, FILE_READ);

    if (!configFile) {
        logger("Failed to open config file \"" + String(configPath) + "\".");
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

    strlcpy(config.uuid, json["uuid"], sizeof(config.uuid));

    return true;
}

bool setConfig(char *configPath, Config newConfig) {
    StaticJsonDocument<512> json;
    
    json["wifiSsid"] = String(newConfig.wifiSsid);
    json["wifiPassword"] = String(newConfig.wifiPassword);
    json["mqttHost"] = String(newConfig.mqttHost);
    json["mqttPort"] = newConfig.mqttPort;
    json["mqttUsername"] = String(newConfig.mqttUsername);
    json["mqttPassword"] = String(newConfig.mqttPassword);
    json["mqttPublishChannel"] = String(newConfig.mqttPublishChannel);
    json["mqttSubscribeChannel"] = String(newConfig.mqttSubscribeChannel);

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