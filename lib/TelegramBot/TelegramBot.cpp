#include "TelegramBot.h"

TelegramBot::TelegramBot(WiFiClientSecure &wifiClient) {
  this->wifiClient = &wifiClient;
  //this->client->begin(wifiClient, TELEGRAM_HOST, 443);
}

TelegramBot::TelegramBot(WiFiClientSecure &wifiClient, String token) {
  this->wifiClient = &wifiClient;
  //this->client->begin(wifiClient, TELEGRAM_HOST, 443);
  this->token  = token;
}

void TelegramBot::setToken(String token) {
  this->token = token;
}

void TelegramBot::enableDebugMode() {
  this->debugMode = true;
}

void TelegramBot::logger(String message, bool endLine) {
  if (true == this->debugMode) {
    if (true == endLine) {
        Serial.println(message);
    } else {
        Serial.print(message);
    }
  }
}

void TelegramBot::close() {
  if (this->client->connected()) {
    this->client->end();
  }
}

bool TelegramBot::connected() {
  return this->client->connected();
}

User TelegramBot::getMe() {
  this->logger("Start getMe", true);
  DynamicJsonDocument userResponse = this->sendGetCommand("/bot" + this->token + "/getMe");
  serializeJson(userResponse, Serial);

  if (userResponse.containsKey("status") && userResponse["status"] == "nok") {
    this->logger(userResponse["message"]);
  }

  this->user.id = userResponse["id"].as<int>();
  this->user.firstName = userResponse["first_name"].as<String>();
  this->user.lastName = userResponse["last_name"].as<String>();
  this->user.username = userResponse["username"].as<String>();
  this->user.languageCode = userResponse["language_code"].as<String>();
  this->user.isBot = userResponse["is_bot"].as<bool>();

  return this->user;
}

DynamicJsonDocument TelegramBot::sendGetCommand(String action) {
  DynamicJsonDocument response(1024);

  this->client->begin(*this->wifiClient, "api.telegram.org", 443, action);
  //int httpCode = this->client->GET();
  //logger("Status: " + String(httpCode));
  //if (this->connected()) {
    //int httpCode = this->client->GET(action);
    /*this->client->println("GET /" + action + " HTTP/1.1");
    this->client->print("Host:");
    this->client->println(TELEGRAM_HOST);
    this->client->println(F("Connection: close"));
    
    if (this->client->println() == 0) {
      response = this->buildJsonResponseError(response, F("Failed to send request"));
    } else {
      unsigned long now = millis();
      bool avail = false;
      String mess = "";

      // loop for a while to send the message
      while (millis() - now < 0 * 1000 + 1500) {
        while (client->available()) {
          char c = client->read();
          mess = mess + c;
          avail = true;
        }
        if (avail) {
          this->logger(mess);
          break;
        }
      }
    }*/
  /*} else {
    response = this->buildJsonResponseError(response, F("Client telegram conection error"));
  }*/

  this->close();

  return response;
}

DynamicJsonDocument TelegramBot::sendPostCommand(String action, JsonObject payload){
  DynamicJsonDocument response(512);
  return response;
}

DynamicJsonDocument TelegramBot::buildJsonResponseError(DynamicJsonDocument response, String message) {
  response["status"] = "nok";
  response["message"] = message;

  return response;
}