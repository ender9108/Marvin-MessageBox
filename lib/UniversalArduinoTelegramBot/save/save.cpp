String UniversalTelegramBot::sendMultipartFormDataToTelegram(
    String command, 
    String binaryProperyName, 
    String fileName,
    String contentType, 
    String chat_id, int fileSize,
    MoreDataAvailable moreDataAvailableCallback,
    GetNextByte getNextByteCallback
) {

  String body = "";
  String headers = "";
  long now;
  bool responseReceived;
  String boundry = F("------------------------b8f610217e83e29b");

  // Connect with api.telegram.org if not already connected
  if (!client->connected()) {
    if (_debug)
      Serial.println(F("[BOT Client]Connecting to server"));
    if (!client->connect(HOST, SSL_PORT)) {
      if (_debug)
        Serial.println(F("[BOT Client]Conection error"));
    }
  }
  if (client->connected()) {

    String start_request = "";
    String end_request = "";

    start_request = start_request + "--" + boundry + "\r\n";
    start_request = start_request +
                    "content-disposition: form-data; name=\"chat_id\"" + "\r\n";
    start_request = start_request + "\r\n";
    start_request = start_request + chat_id + "\r\n";

    start_request = start_request + "--" + boundry + "\r\n";
    start_request = start_request + "content-disposition: form-data; name=\"" +
                    binaryProperyName + "\"; filename=\"" + fileName + "\"" +
                    "\r\n";
    start_request = start_request + "Content-Type: " + contentType + "\r\n";
    start_request = start_request + "\r\n";

    end_request = end_request + "\r\n";
    end_request = end_request + "--" + boundry + "--" + "\r\n";

    client->print("POST /bot" + _token + "/" + command);
    client->println(F(" HTTP/1.1"));
    // Host header
    client->print(F("Host: "));
    client->println(HOST);
    client->println(F("User-Agent: arduino/1.0"));
    client->println(F("Accept: */*"));

    int contentLength =
        fileSize + start_request.length() + end_request.length();
    if (_debug)
      Serial.println("Content-Length: " + String(contentLength));
    client->print("Content-Length: ");
    client->println(String(contentLength));
    client->println("Content-Type: multipart/form-data; boundary=" + boundry);
    client->println("");

    client->print(start_request);

    if (_debug)
      Serial.print(start_request);

    byte buffer[512];
    int count = 0;
    //char ch;
    while (moreDataAvailableCallback()) {
      buffer[count] = getNextByteCallback();
      // client->write(ch);
      // Serial.write(ch);
      count++;
      if (count == 512) {
        // yield();
        if (_debug) {
          Serial.println(F("Sending full buffer"));
        }
        client->write((const uint8_t *)buffer, 512);
        count = 0;
      }
    }

    if (count > 0) {
      if (_debug) {
        Serial.println(F("Sending remaining buffer"));
      }
      client->write((const uint8_t *)buffer, count);
    }

    client->print(end_request);
    if (_debug)
      Serial.print(end_request);

    count = 0;
    int ch_count = 0;
    //char c;
    now = millis();
    bool finishedHeaders = false;
    bool currentLineIsBlank = true;
    while (millis() - now < waitForResponse) {
      while (client->available()) {
        char c = client->read();
        responseReceived = true;

        if (!finishedHeaders) {
          if (currentLineIsBlank && c == '\n') {
            finishedHeaders = true;
          } else {
            headers = headers + c;
          }
        } else {
          if (ch_count < maxMessageLength) {
            body = body + c;
            ch_count++;
          }
        }

        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }

      if (responseReceived) {
        if (_debug) {
          Serial.println();
          Serial.println(body);
          Serial.println();
        }
        break;
      }
    }
  }

  closeClient();
  return body;
}

bool UniversalTelegramBot::getMe() {
  String command = "bot" + _token + "/getMe";
  String response =
      sendGetToTelegram(command); // receive reply from telegram.org

  /**
   * Update ArduinoJson 5 to 6
   * DynamicJsonBuffer jsonBuffer;
   * JsonObject &root = jsonBuffer.parseObject(response);
   */
  
  DynamicJsonDocument root(1024);
  auto error = deserializeJson(root, response);

  closeClient();

  /**
   * Update ArduinoJson 5 to 6
   * if (root.success()) {
   *   if (root.containsKey("result")) {
   *     String _name = root["result"]["first_name"];
   *     String _username = root["result"]["username"];
   *     name = _name;
   *     userName = _username;
   *     return true;
   *   }
   * }
   */

  if (error) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
  } else {
    if (root.containsKey("result")) {
      String _name = root["result"]["first_name"];
      String _username = root["result"]["username"];
      name = _name;
      userName = _username;
      return true;
    }
  }

  return false;
}

/***********************************************************************
 * SendMessage - function to send message to telegram                  *
 * (Arguments to pass: chat_id, text to transmit and markup(optional)) *
 ***********************************************************************/
bool UniversalTelegramBot::sendSimpleMessage(String chat_id, String text,
                                             String parse_mode) {

  bool sent = false;
  if (_debug)
    Serial.println(F("SEND Simple Message"));
  long sttime = millis();

  if (text != "") {
    while (millis() < sttime + 8000) { // loop for a while to send the message
      String command = "bot" + _token + "/sendMessage?chat_id=" + chat_id +
                       "&text=" + text + "&parse_mode=" + parse_mode;
      String response = sendGetToTelegram(command);
      if (_debug)
        Serial.println(response);
      sent = checkForOkResponse(response);
      if (sent) {
        break;
      }
    }
  }
  closeClient();
  return sent;
}

bool UniversalTelegramBot::sendMessage(String chat_id, String text,
                                       String parse_mode) {

  
  /**
   * Update ArduinoJson 5 to 6
   * DynamicJsonBuffer jsonBuffer;
   * JsonObject &payload = jsonBuffer.createObject();
   */
  
  DynamicJsonDocument payload(1024);

  payload["chat_id"] = chat_id;
  payload["text"] = text;

  if (parse_mode != "") {
    payload["parse_mode"] = parse_mode;
  }

  return sendPostMessage(payload.as<JsonObject>());
}

bool UniversalTelegramBot::sendMessageWithReplyKeyboard(
    String chat_id, String text, String parse_mode, String keyboard,
    bool resize, bool oneTime, bool selective) {

  /**
   * Update ArduinoJson 5 to 6
   * DynamicJsonBuffer jsonBuffer;
   * JsonObject &payload = jsonBuffer.createObject();
   */

  DynamicJsonDocument payload(1024);

  payload["chat_id"] = chat_id;
  payload["text"] = text;

  if (parse_mode != "") {
    payload["parse_mode"] = parse_mode;
  }

  JsonObject replyMarkup = payload.createNestedObject("reply_markup");

  // Reply keyboard is an array of arrays.
  // Outer array represents rows
  // Inner arrays represents columns
  // This example "ledon" and "ledoff" are two buttons on the top row
  // and "status is a single button on the next row"
  
  /**
   * Update ArduinoJson 5 to 6
   * DynamicJsonBuffer keyboardBuffer;
   * replyMarkup["keyboard"] = keyboardBuffer.parseArray(keyboard);
   */

  DynamicJsonDocument keyboardBuffer(1024);
  deserializeJson(keyboardBuffer, replyMarkup["keyboard"]);

  // Telegram defaults these values to false, so to decrease the size of the
  // payload we will only send them if needed
  if (resize) {
    replyMarkup["resize_keyboard"] = resize;
  }

  if (oneTime) {
    replyMarkup["one_time_keyboard"] = oneTime;
  }

  if (selective) {
    replyMarkup["selective"] = selective;
  }

  return sendPostMessage(payload.as<JsonObject>());
}

bool UniversalTelegramBot::sendMessageWithInlineKeyboard(String chat_id,
                                                         String text,
                                                         String parse_mode,
                                                         String keyboard) {
  /**
   * Update ArduinoJson 5 to 6
   * DynamicJsonBuffer jsonBuffer;
   * JsonObject &payload = jsonBuffer.createObject();
   */

  DynamicJsonDocument payload(1024);

  payload["chat_id"] = chat_id;
  payload["text"] = text;

  if (parse_mode != "") {
    payload["parse_mode"] = parse_mode;
  }

  JsonObject replyMarkup = payload.createNestedObject("reply_markup");

  /**
   * Update ArduinoJson 5 to 6
   * DynamicJsonBuffer keyboardBuffer;
   * replyMarkup["inline_keyboard"] = keyboardBuffer.parseArray(keyboard);
   */

  DynamicJsonDocument keyboardBuffer(1024);
  deserializeJson(keyboardBuffer, replyMarkup["keyboard"]);

  return sendPostMessage(payload.as<JsonObject>());
}

/***********************************************************************
 * SendPostMessage - function to send message to telegram                  *
 * (Arguments to pass: chat_id, text to transmit and markup(optional)) *
 ***********************************************************************/
bool UniversalTelegramBot::sendPostMessage(JsonObject payload) {

  bool sent = false;
  if (_debug)
    Serial.println(F("SEND Post Message"));
  long sttime = millis();

  if (payload.containsKey("text")) {
    while (millis() < sttime + 8000) { // loop for a while to send the message
      String command = "bot" + _token + "/sendMessage";
      String response = sendPostToTelegram(command, payload);
      if (_debug)
        Serial.println(response);
      sent = checkForOkResponse(response);
      if (sent) {
        break;
      }
    }
  }

  closeClient();
  return sent;
}

String UniversalTelegramBot::sendPostPhoto(JsonObject payload) {

  bool sent = false;
  String response = "";
  if (_debug)
    Serial.println(F("SEND Post Photo"));
  long sttime = millis();

  if (payload.containsKey("photo")) {
    while (millis() < sttime + 8000) { // loop for a while to send the message
      String command = "bot" + _token + "/sendPhoto";
      response = sendPostToTelegram(command, payload);
      if (_debug)
        Serial.println(response);
      sent = checkForOkResponse(response);
      if (sent) {
        break;
      }
    }
  }

  closeClient();
  return response;
}

String UniversalTelegramBot::sendPhotoByBinary(
    String chat_id, String contentType, int fileSize,
    MoreDataAvailable moreDataAvailableCallback,
    GetNextByte getNextByteCallback) {

  if (_debug)
    Serial.println("SEND Photo");

  String response = sendMultipartFormDataToTelegram(
      "sendPhoto", "photo", "img.jpg", contentType, chat_id, fileSize,
      moreDataAvailableCallback, getNextByteCallback);

  if (_debug)
    Serial.println(response);

  return response;
}

String UniversalTelegramBot::sendPhoto(String chat_id, String photo,
                                       String caption,
                                       bool disable_notification,
                                       int reply_to_message_id,
                                       String keyboard) {
  /**
   * Update ArduinoJson 5 to 6
   * DynamicJsonBuffer jsonBuffer;
   * JsonObject &payload = jsonBuffer.createObject();
   */

  DynamicJsonDocument payload(1024);

  payload["chat_id"] = chat_id;
  payload["photo"] = photo;

  if (caption) {
    payload["caption"] = caption;
  }

  if (disable_notification) {
    payload["disable_notification"] = disable_notification;
  }

  if (reply_to_message_id && reply_to_message_id != 0) {
    payload["reply_to_message_id"] = reply_to_message_id;
  }

  if (keyboard) {
    JsonObject replyMarkup = payload.createNestedObject("reply_markup");

    /**
     * Update ArduinoJson 5 to 6
     * DynamicJsonBuffer keyboardBuffer;
     * replyMarkup["inline_keyboard"] = keyboardBuffer.parseArray(keyboard);
     */

    DynamicJsonDocument keyboardBuffer(1024);
    deserializeJson(keyboardBuffer, replyMarkup["keyboard"]);
  }

  return sendPostPhoto(payload.as<JsonObject>());
}

bool UniversalTelegramBot::checkForOkResponse(String response) {
  int responseLength = response.length();

  for (int m = 5; m < responseLength + 1; m++) {
    if (response.substring(m - 10, m) ==
        "{\"ok\":true") { // Chek if message has been properly sent
      return true;
    }
  }

  return false;
}

bool UniversalTelegramBot::sendChatAction(String chat_id, String text) {

  bool sent = false;
  if (_debug)
    Serial.println(F("SEND Chat Action Message"));
  long sttime = millis();

  if (text != "") {
    while (millis() < sttime + 8000) { // loop for a while to send the message
      String command = "bot" + _token + "/sendChatAction?chat_id=" + chat_id +
                       "&action=" + text;
      String response = sendGetToTelegram(command);

      if (_debug)
        Serial.println(response);
      sent = checkForOkResponse(response);

      if (sent) {
        break;
      }
    }
  }

  closeClient();
  return sent;
}