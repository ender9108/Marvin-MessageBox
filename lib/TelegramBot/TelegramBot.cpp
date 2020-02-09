#include "TelegramBot.h"

TelegramBot::TelegramBot(WiFiClientSecure &wifiClient) {
  this->client = &wifiClient;
}

TelegramBot::TelegramBot(WiFiClientSecure &wifiClient, String token) {
  this->client = &wifiClient;
  this->token  = token;
  this->baseAction = "/bot" + this->token + "/";
}

void TelegramBot::setToken(String token) {
  this->token = token;
  this->baseAction = "/bot" + this->token + "/";
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

long TelegramBot::getLastMessageId() {
  return this->lastMessageId;
}

User TelegramBot::getMe() {
  this->logger("Start getMe");
  DynamicJsonDocument response = this->sendGetCommand("getMe");
  
  if (this->debugMode) {
    serializeJson(response, Serial);
    this->logger("");
  }

  if (response.containsKey("ok") && response["ok"] == false) {
    this->logger(response["message"]);
  }

  this->botUser.id = response["id"].as<int>();
  this->botUser.firstName = response["first_name"].as<String>();
  this->botUser.lastName = response["last_name"].as<String>();
  this->botUser.username = response["username"].as<String>();
  this->botUser.languageCode = response["language_code"].as<String>();
  this->botUser.isBot = response["is_bot"].as<bool>();

  return this->botUser;
}

int TelegramBot::getUpdates(int offset, int limit) {
  this->logger("Start getUpdates");
  DynamicJsonDocument response = this->sendGetCommand("getUpdates?offset=" + String(offset) + "&limit=" + String(limit));

  if (response.containsKey("ok") && true == response["ok"]) {
    int size = response["result"].size();
    this->logger("Response size: " + String(size));
    if (size > 0) {
      if (size > TELEGRAM_MAX_MSG) {
        size = TELEGRAM_MAX_MSG;
      }

      int messageIndex = 0;

      for (int i = 0; i < size; i++) {
        if (this->hydrateMessageStruct(response["result"][i], messageIndex)) {
          messageIndex++;
        }
      }

      return messageIndex;
    } else {
      this->logger("No message.");
    }
  }

  return 0;
}

Message* TelegramBot::getMessages(bool forceUpdate) {
  if (true == forceUpdate) {
    this->getUpdates(this->lastMessageId);
  }

  return this->messages;
}

Message TelegramBot::getLastMessage(bool forceUpdate) {
  if (true == forceUpdate) {
    this->getUpdates(this->lastMessageId);
  }

  return this->messages[0];
}

bool TelegramBot::sendMessage(
  long chatId, 
  String text, 
  String parseMode, 
  bool disablePreview, 
  long replyToMessageId, 
  bool disableNotification
) {
  const size_t CAPACITY = JSON_OBJECT_SIZE(6);
  StaticJsonDocument<CAPACITY> doc;

  JsonObject postData = doc.to<JsonObject>();
  postData["chat_id"] = chatId;
  postData["text"] = text;
  postData["parse_mode"] = parseMode;
  postData["disable_preview"] = disablePreview;
  postData["reply_to_message_id"] = replyToMessageId;
  postData["disable_notification"] = disableNotification;

  DynamicJsonDocument response = this->sendPostCommand("sendMessage", postData);

  return response["ok"];
}

bool TelegramBot::hydrateMessageStruct(JsonObject message, int index) {
  int updateId = message["update_id"];

  logger("lastMessageId: " + String(this->lastMessageId) + " - updateId: " + String(updateId));

  if (this->lastMessageId != updateId) {
    this->lastMessageId = updateId;

    for (int i = (TELEGRAM_MAX_MSG-2); i >= 0; i--) {
      this->messages[(i+1)] = this->messages[i];
    }
    /*for (int i = 0; i < (TELEGRAM_MAX_MSG-1); i++) {
      this->messages[(i+1)] = this->messages[i];
    }*/

    this->messages[0].id = message["message"]["message_id"].as<int>();
    this->messages[0].date = message["message"]["date"].as<int>();
    this->messages[0].text = message["message"]["text"].as<String>();
    this->messages[0].type = message["message"]["entities"]["type"].as<String>();

    User user;
    user.id = message["message"]["from"]["id"].as<int>();
    user.firstName = message["message"]["from"]["first_name"].as<String>();
    user.lastName = message["message"]["from"]["last_name"].as<String>();
    user.username = message["message"]["from"]["username"].as<String>();
    user.languageCode = message["message"]["from"]["language_code"].as<String>();
    user.isBot = message["message"]["from"]["is_bot"].as<bool>();
    this->messages[0].from = user;

    Chat chat;
    chat.id = message["message"]["chat"]["id"].as<int>();
    this->messages[0].chat = chat;

    return true;
  }

  return false;
}

DynamicJsonDocument TelegramBot::sendGetCommand(String action) {
  DynamicJsonDocument response(4096);

  HTTPClient httpClient;
  this->logger("CALL -> " + String(TELEGRAM_HOST)+(this->baseAction + action));
  httpClient.begin(*this->client, String(TELEGRAM_HOST), TELEGRAM_PORT, (this->baseAction + action));
  int httpCode = httpClient.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = httpClient.getString();
    this->logger(payload);
    DeserializationError error = deserializeJson(response, payload);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return this->buildJsonResponseError(httpCode, F("deserializeJson() failed"));
    }
  } else {
    return this->buildJsonResponseError(httpCode, httpClient.getString());
  }

  httpClient.end();

  return response;
}

DynamicJsonDocument TelegramBot::sendPostCommand(String action, JsonObject payloadObject) {
  DynamicJsonDocument response(512);

  HTTPClient httpClient;
  this->logger("CALL -> " + String(TELEGRAM_HOST)+(this->baseAction + action));
  httpClient.begin(*this->client, String(TELEGRAM_HOST), TELEGRAM_PORT, (this->baseAction + action));
  httpClient.addHeader(F("Content-Type"), F("application/json"));

  String postData;
  serializeJson(payloadObject, postData);

  int httpCode = httpClient.POST(postData);

  if (httpCode == HTTP_CODE_OK) {
    String payload = httpClient.getString();
    this->logger(payload);
    DeserializationError error = deserializeJson(response, payload);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return this->buildJsonResponseError(httpCode, F("deserializeJson() failed"));
    }
  } else {
    return this->buildJsonResponseError(httpCode, httpClient.getString());
  }

  httpClient.end();

  return response;
}

DynamicJsonDocument TelegramBot::buildJsonResponseError(int statusCode, String message) {
  const int capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(1);
  DynamicJsonDocument response(capacity);
  response["ok"] = false;
  response["statusCode"] = statusCode;
  response["message"] = message;

  return response;
}