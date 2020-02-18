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

void TelegramBot::setTimeToRefresh(long ttr) {
  this->timeToRefresh = ttr;
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

void TelegramBot::pause() {
  this->loopOn = false;
}

void TelegramBot::resume() {
  this->loopOn = true;
}

bool TelegramBot::on(int event, EventCallback callback) {
  switch (event) {
    case TELEGRAM_EVT_NEW_MSG:
      this->onNewUpdate = callback;
      return true;
      break;
    default:
      this->logger("Uncknow " + String(event) + " event");
      break;
  }

  return false;
}

int TelegramBot::loop() {
  if (true == this->loopOn && millis() > this->lastUpdateTime + this->timeToRefresh)  {
      this->lastUpdateTime = millis();
      this->logger(F("Checking new messages.. "));

      int newMessages = this->getUpdates(this->lastUpdateId);

      if (newMessages > 0 && this->onNewUpdate != NULL) {
        this->onNewUpdate(this->updates, newMessages);
      }

      return newMessages;
  }

  return 0;
}

long TelegramBot::getLastUpdateId() {
  return this->lastUpdateId;
}

JsonObject TelegramBot::getMe() {
  DynamicJsonDocument document = this->sendGetCommand("getMe");
  JsonObject response = document.to<JsonObject>();

  return response;
}

int TelegramBot::getUpdates(int offset, int limit) {
  DynamicJsonDocument response = this->sendGetCommand("getUpdates?offset=" + String(offset) + "&limit=" + String(limit));

  if (response.containsKey("ok") && true == response["ok"]) {
    int size = response["result"].size();

    if (size > 0) {
      if (size > TELEGRAM_MAX_UPDATE) {
        size = TELEGRAM_MAX_UPDATE;
      }

      int messageIndex = 0;
      DynamicJsonDocument document(2048);
      JsonArray temp = document.to<JsonArray>();

      for (int i = 0; i < size; i++) {
        if (this->parseUpdates(response["result"][i])) {
          temp.add(response["result"][i]);
          messageIndex++;
        }
      }

      this->updates = temp;

      return messageIndex;
    } else {
      this->logger("No new message.");
    }
  }

  return 0;
}

bool TelegramBot::parseUpdates(JsonObject update) {
  int updateId = update["update_id"];

  if (this->lastUpdateId != updateId) {
    this->lastUpdateId = updateId;
    return true;
  }

  return false;
}

const size_t TelegramBot::calculateJsonCapacity(int object, int array, int multiplier) {
  const size_t CAPACITY = (JSON_OBJECT_SIZE(object) + JSON_ARRAY_SIZE(array)) * multiplier;

  return CAPACITY;
}

DynamicJsonDocument TelegramBot::sendMessage(
  long chatId, 
  String text, 
  String parseMode, 
  bool disablePreview, 
  long replyToMessageId,
  bool disableNotification
) {
  DynamicJsonDocument doc(this->calculateJsonCapacity(8, 0, 2));

  JsonObject jsonData = doc.to<JsonObject>();
  jsonData["chat_id"] = chatId;
  jsonData["text"] = text;
  jsonData["parse_mode"] = parseMode;
  jsonData["disable_preview"] = disablePreview;
  jsonData["reply_to_message_id"] = replyToMessageId;
  jsonData["disable_notification"] = disableNotification;

  return this->sendPostCommand("sendMessage", jsonData);
}

DynamicJsonDocument TelegramBot::sendContact(
  long chatId, 
  String phoneNumber, 
  String firstName, 
  String lastName, 
  long replyToMessageId, 
  bool disableNotification
) {
  DynamicJsonDocument doc(this->calculateJsonCapacity(6));

  JsonObject postData = doc.to<JsonObject>();
  postData["chat_id"] = chatId;
  postData["phone_number"] = phoneNumber;
  postData["first_name"] = firstName;
  postData["last_name"] = lastName;
  postData["reply_to_message_id"] = replyToMessageId;
  postData["disable_notification"] = disableNotification;

  return this->sendPostCommand("sendContact", postData);
}

DynamicJsonDocument TelegramBot::sendChatAction(long chatId, String action) {
  DynamicJsonDocument doc(this->calculateJsonCapacity(2));

  JsonObject postData = doc.to<JsonObject>();
  postData["chat_id"] = chatId;
  postData["action"] = action;

  return this->sendPostCommand("sendChatAction", postData);
}

DynamicJsonDocument TelegramBot::sendLocation(
  long chatId, 
  float latitude, 
  float longitude, 
  long replyToMessageId, 
  bool disableNotification, 
  int livePeriod
) {
  DynamicJsonDocument doc(this->calculateJsonCapacity(6));

  JsonObject postData = doc.to<JsonObject>();
  postData["chat_id"] = chatId;
  postData["latitude"] = latitude;
  postData["longitude"] = longitude;
  postData["reply_to_message_id"] = replyToMessageId;
  postData["disable_notification"] = disableNotification;
  postData["live_period"] = livePeriod;

  return this->sendPostCommand("sendLocation", postData);
}

DynamicJsonDocument TelegramBot::editMessageReplyMarkup(long chatId, long messageId, long inlineMessageId) {
  DynamicJsonDocument doc(this->calculateJsonCapacity(3));

  JsonObject postData = doc.to<JsonObject>();
  postData["chat_id"] = chatId;
  postData["message_id"] = messageId;
  postData["inline_message_id"] = inlineMessageId;

  return this->sendPostCommand("editMessageReplyMarkup", postData);
}

DynamicJsonDocument TelegramBot::deleteMessage(long chatId, long messageId) {
  DynamicJsonDocument doc(this->calculateJsonCapacity(2));

  JsonObject postData = doc.to<JsonObject>();
  postData["chat_id"] = chatId;
  postData["message_id"] = messageId;

  return this->sendPostCommand("deleteMessage", postData);
}

DynamicJsonDocument TelegramBot::editMessageLiveLocation(long chatId, long messageId, long inlineMessageId, float latitude, float longitude) {
    DynamicJsonDocument doc(this->calculateJsonCapacity(5));

    JsonObject postData = doc.to<JsonObject>();
    postData["chat_id"] = chatId;
    postData["message_id"] = messageId;
    postData["inline_message_id"] = inlineMessageId;
    postData["latitude"] = latitude;
    postData["longitude"] = longitude;

    return this->sendPostCommand("editMessageLiveLocation", postData);
}

DynamicJsonDocument TelegramBot::stopMessageLiveLocation(long chatId, long messageId, long inlineMessageId) {
  DynamicJsonDocument doc(this->calculateJsonCapacity(3));

  JsonObject postData = doc.to<JsonObject>();
  postData["chat_id"] = chatId;
  postData["message_id"] = messageId;
  postData["inline_message_id"] = inlineMessageId;

  return this->sendPostCommand("stopMessageLiveLocation", postData);
}

DynamicJsonDocument TelegramBot::forwardMessage(long chatId, long fromChatId, long messageId, bool disableNotification) {
  DynamicJsonDocument doc(this->calculateJsonCapacity(4));

  JsonObject postData = doc.to<JsonObject>();
  postData["chat_id"] = chatId;
  postData["from_chat_id"] = messageId;
  postData["message_id"] = messageId;
  postData["disable_notification"] = disableNotification;

  return this->sendPostCommand("forwardMessage", postData);
}

DynamicJsonDocument TelegramBot::kickChatMember(long chatId, long userId, long untilDate) {
  DynamicJsonDocument doc(this->calculateJsonCapacity(3));

  JsonObject postData = doc.to<JsonObject>();
  postData["chat_id"] = chatId;
  postData["user_id"] = userId;

  if (untilDate != -1) {
    postData["until_date"] = untilDate;
  }

  return this->sendPostCommand("kickChatMember", postData);
}

DynamicJsonDocument TelegramBot::unbanChatMember(long chatId, long userId) {
  DynamicJsonDocument doc(this->calculateJsonCapacity(2));

  JsonObject postData = doc.to<JsonObject>();
  postData["chat_id"] = chatId;
  postData["user_id"] = userId;

  return this->sendPostCommand("unbanChatMember", postData);
}

DynamicJsonDocument TelegramBot::editMessageText(long chatId, long messageId, String text, String parseMode, bool disablePreview, long inlineMessageId) {
  DynamicJsonDocument doc(this->calculateJsonCapacity(6));

  JsonObject postData = doc.to<JsonObject>();
  postData["chat_id"] = chatId;
  postData["message_id"] = messageId;
  postData["text"] = text;
  postData["parse_mode"] = parseMode;
  postData["disable_preview"] = disablePreview;
  postData["inline_message_id"] = inlineMessageId;

  return this->sendPostCommand("editMessageText", postData);
}

DynamicJsonDocument TelegramBot::editMessageCaption(long chatId, long messageId, String caption, long inlineMessageId) {
  DynamicJsonDocument doc(this->calculateJsonCapacity(4));

  JsonObject postData = doc.to<JsonObject>();
  postData["chat_id"] = chatId;
  postData["message_id"] = messageId;
  postData["caption"] = caption;
  postData["inline_message_id"] = inlineMessageId;

  return this->sendPostCommand("editMessageCaption", postData);
}

DynamicJsonDocument TelegramBot::sendPhoto(
  long chatId, 
  String photo, 
  String caption, 
  long replyToMessageId, 
  bool disableNotification, 
  String parseMode
) {
  DynamicJsonDocument doc(this->calculateJsonCapacity(6));

  JsonObject postData = doc.to<JsonObject>();
  postData["chat_id"] = chatId;
  postData["photo"] = photo;
  postData["caption"] = caption;
  postData["reply_to_messageId"] = replyToMessageId;
  postData["disable_notification"] = disableNotification;
  postData["parse_mode"] = parseMode;

  return this->sendPostCommand("editMessageCaption", postData);
}

DynamicJsonDocument TelegramBot::sendDocument(
  long chatId, 
  String document, 
  String caption, 
  long replyToMessageId, 
  bool disableNotification, 
  String parseMode
) {
  DynamicJsonDocument doc(this->calculateJsonCapacity(6));

  JsonObject postData = doc.to<JsonObject>();
  postData["chat_id"] = chatId;
  postData["document"] = document;
  postData["caption"] = caption;
  postData["reply_to_messageId"] = replyToMessageId;
  postData["disable_notification"] = disableNotification;
  postData["parse_mode"] = parseMode;

  return this->sendPostCommand("sendDocument", postData);
}

DynamicJsonDocument TelegramBot::sendGetCommand(String action) {
  DynamicJsonDocument response(4096);

  HTTPClient httpClient;
  this->logger("-- CALL -> GET " + String(TELEGRAM_HOST)+(this->baseAction + action));
  httpClient.begin(*this->client, String(TELEGRAM_HOST), TELEGRAM_PORT, (this->baseAction + action));
  int httpCode = httpClient.GET();
  this->logger("---- HTTP RESPONSE CODE : " + String(httpCode));

  if (httpCode == HTTP_CODE_OK) {
    String payload = httpClient.getString();
    this->logger("---- HTTP RESPONSE PAYLOAD : " + payload);
    DeserializationError error = deserializeJson(response, payload);

    if (error) {
      this->logger(F("---- DeserializeJson() failed : "), false);
      this->logger(error.c_str());
      response = this->buildJsonResponseError(httpCode, F("deserializeJson() failed"));
    }
  } else {
    response = this->buildJsonResponseError(httpCode, httpClient.getString());
  }

  httpClient.end();

  return response;
}

DynamicJsonDocument TelegramBot::sendPostCommand(String action, JsonObject payloadObject) {
  DynamicJsonDocument response(4096);

  HTTPClient httpClient;
  this->logger("-- CALL -> POST " + String(TELEGRAM_HOST)+(this->baseAction + action));
  httpClient.begin(*this->client, String(TELEGRAM_HOST), TELEGRAM_PORT, (this->baseAction + action));
  httpClient.addHeader(F("Content-Type"), F("application/json"));

  String postData;
  serializeJson(payloadObject, postData);

  this->logger("-- Post data : " + postData);
  int httpCode = httpClient.POST(postData);
  this->logger("---- HTTP RESPONSE CODE : " + String(httpCode));

  if (httpCode == HTTP_CODE_OK) {
    String payload = httpClient.getString();
    this->logger("---- HTTP RESPONSE PAYLOAD : " + payload);

    DeserializationError error = deserializeJson(response, payload);

    if (error) {
      this->logger(F("---- DeserializeJson() failed : "), false);
      this->logger(error.c_str());
      
      response = this->buildJsonResponseError(httpCode, F("deserializeJson() failed"));
    }
  } else {
    response = this->buildJsonResponseError(httpCode, httpClient.getString());
  }

  httpClient.end();

  return response;
}

DynamicJsonDocument TelegramBot::buildJsonResponseError(int statusCode, String message) {
  DynamicJsonDocument response(this->calculateJsonCapacity(3));
  response["ok"] = false;
  response["statusCode"] = statusCode;
  response["message"] = message;

  return response;
}