/*
   Copyright (c) 2018 Brian Lough. All right reserved.

   UniversalTelegramBot - Library to create your own Telegram Bot using
   ESP8266 or ESP32 on Arduino IDE.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
   **** Note Regarding Client Connection Keeping ****
   Client connection is established in functions that directly involve use of
   client, i.e sendGetToTelegram, sendPostToTelegram, and
   sendMultipartFormDataToTelegram. It is closed at the end of
   sendMultipartFormDataToTelegram, but not at the end of sendGetToTelegram and
   sendPostToTelegram as these may need to keep the connection alive for respose
   / response checking. Re-establishing a connection then wastes time which is
   noticeable in user experience. Due to this, it is important that connection
   be closed manually after calling sendGetToTelegram or sendPostToTelegram by
   calling closeClient(); Failure to close connection causes memory leakage and
   SSL errors
 */

#include "UniversalTelegramBot.h"

UniversalTelegramBot::UniversalTelegramBot(String token, Client &client) {
  _token = token;
  this->client = &client;
}

String UniversalTelegramBot::sendGetToTelegram(String command) {
  String mess = "";
  long now;
  bool avail;

  // Connect with api.telegram.org if not already connected
  if (!client->connected()) {
    if (_debug)
      Serial.println(F("[BOT]Connecting to server"));
    if (!client->connect(HOST, SSL_PORT)) {
      if (_debug)
        Serial.println(F("[BOT]Conection error"));
    }
  }
  if (client->connected()) {

    if (_debug)
      Serial.println(F(".... connected to server"));

    String a = "";
    //char c;
    int ch_count = 0;
    client->println("GET /" + command);
    now = millis();
    avail = false;
    while (millis() - now < longPoll * 1000 + waitForResponse) {
      while (client->available()) {
        char c = client->read();
        // Serial.write(c);
        if (ch_count < maxMessageLength) {
          mess = mess + c;
          ch_count++;
        }
        avail = true;
      }
      if (avail) {
        if (_debug) {
          Serial.println();
          Serial.println(mess);
          Serial.println();
        }
        break;
      }
    }
  }

  return mess;
}

String UniversalTelegramBot::sendPostToTelegram(String command, JsonObject payload) {

  String body = "";
  String headers = "";
  long now;
  bool responseReceived;

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
    // POST URI
    client->print("POST /" + command);
    client->println(F(" HTTP/1.1"));
    // Host header
    client->print(F("Host:"));
    client->println(HOST);
    // JSON content type
    client->println(F("Content-Type: application/json"));

    // Content length
    /**
     * Update ArduinoJson 5 to 6
     * int length = payload.measureLength();
     */
    int length = measureJson(payload);
    client->print(F("Content-Length:"));
    client->println(length);
    // End of headers
    client->println();
    // POST message body
    // json.printTo(client); // very slow ??
    String out;
    /**
     * Update ArduinoJson 5 to 6
     * payload.printTo(out);
     */
    serializeJson(payload, out);
    client->println(out);

    int ch_count = 0;
    //char c;
    now = millis();
    responseReceived = false;
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

  return body;
}

bool UniversalTelegramBot::processResult(JsonObject result, int messageIndex) {
  int update_id = result["update_id"];
  // Check have we already dealt with this message (this shouldn't happen!)
  if (last_message_received != update_id) {
    last_message_received = update_id;
    messages[messageIndex].update_id = update_id;

    messages[messageIndex].text = F("");
    messages[messageIndex].from_id = F("");
    messages[messageIndex].from_name = F("");
    messages[messageIndex].longitude = 0;
    messages[messageIndex].latitude = 0;

    if (result.containsKey("message")) {
      JsonObject message = result["message"];
      messages[messageIndex].type = F("message");
      messages[messageIndex].from_id = message["from"]["id"].as<String>();
      messages[messageIndex].from_name =
          message["from"]["first_name"].as<String>();

      messages[messageIndex].date = message["date"].as<String>();
      messages[messageIndex].chat_id = message["chat"]["id"].as<String>();
      messages[messageIndex].chat_title = message["chat"]["title"].as<String>();

      if (message.containsKey("text")) {
        messages[messageIndex].text = message["text"].as<String>();

      } else if (message.containsKey("location")) {
        messages[messageIndex].longitude =
            message["location"]["longitude"].as<float>();
        messages[messageIndex].latitude =
            message["location"]["latitude"].as<float>();
      }
    } else if (result.containsKey("channel_post")) {
      JsonObject message = result["channel_post"];
      messages[messageIndex].type = F("channel_post");

      messages[messageIndex].text = message["text"].as<String>();
      messages[messageIndex].date = message["date"].as<String>();
      messages[messageIndex].chat_id = message["chat"]["id"].as<String>();
      messages[messageIndex].chat_title = message["chat"]["title"].as<String>();

    } else if (result.containsKey("callback_query")) {
      JsonObject message = result["callback_query"];
      messages[messageIndex].type = F("callback_query");
      messages[messageIndex].from_id = message["from"]["id"].as<String>();
      messages[messageIndex].from_name =
          message["from"]["first_name"].as<String>();

      messages[messageIndex].text = message["data"].as<String>();
      messages[messageIndex].date = message["date"].as<String>();
      messages[messageIndex].chat_id =
          message["message"]["chat"]["id"].as<String>();
      messages[messageIndex].chat_title = F("");
    } else if (result.containsKey("edited_message")) {
      JsonObject message = result["edited_message"];
      messages[messageIndex].type = F("edited_message");
      messages[messageIndex].from_id = message["from"]["id"].as<String>();
      messages[messageIndex].from_name =
          message["from"]["first_name"].as<String>();

      messages[messageIndex].date = message["date"].as<String>();
      messages[messageIndex].chat_id = message["chat"]["id"].as<String>();
      messages[messageIndex].chat_title = message["chat"]["title"].as<String>();

      if (message.containsKey("text")) {
        messages[messageIndex].text = message["text"].as<String>();

      } else if (message.containsKey("location")) {
        messages[messageIndex].longitude =
            message["location"]["longitude"].as<float>();
        messages[messageIndex].latitude =
            message["location"]["latitude"].as<float>();
      }
    }

    return true;
  }
  return false;
}

/***************************************************************
 * GetUpdates - function to receive messages from telegram *
 * (Argument to pass: the last+1 message to read)             *
 * Returns the number of new messages           *
 ***************************************************************/
int UniversalTelegramBot::getUpdates(long offset) {

  if (_debug)
    Serial.println(F("GET Update Messages"));

  String command = "bot" + _token + "/getUpdates?offset=" + String(offset) +
                   "&limit=" + String(HANDLE_MESSAGES);
  if (longPoll > 0) {
    command = command + "&timeout=" + String(longPoll);
  }
  String response = sendGetToTelegram(command); // receive reply from telegram.org

  if (response == "") {
    if (_debug)
      Serial.println(F("Received empty string in response!"));
    // close the client as there's nothing to do with an empty string
    closeClient();
    return 0;
  } else {
    if (_debug) {
      Serial.print(F("incoming message length"));
      Serial.println(response.length());
      Serial.println(F("Creating DynamicJsonBuffer"));
    }

    // Parse response into Json object
    DynamicJsonDocument root(1024);
    auto error = deserializeJson(root, response);

    if (error) {
      if (response.length() <
          2) { // Too short a message. Maybe connection issue
        if (_debug)
          Serial.println(F("Parsing error: Message too short"));
      } else {
        // Buffer may not be big enough, increase buffer or reduce max number of
        // messages
        if (_debug)
          Serial.println(F("Failed to parse update, the message could be too "
                           "big for the buffer"));
      }
    } else {
      // root.printTo(Serial);
      if (_debug)
        Serial.println();
      if (root.containsKey("result")) {
        int resultArrayLength = root["result"].size();
        if (resultArrayLength > 0) {
          int newMessageIndex = 0;
          // Step through all results
          for (int i = 0; i < resultArrayLength; i++) {
            JsonObject result = root["result"][i];
            if (processResult(result, newMessageIndex)) {
              newMessageIndex++;
            }
          }
          // We will keep the client open because there may be a response to be
          // given
          return newMessageIndex;
        } else {
          if (_debug)
            Serial.println(F("no new messages"));
        }
      } else {
        if (_debug)
          Serial.println(F("Response contained no 'result'"));
      }
    }

    // Close the client as no response is to be given
    closeClient();
    return 0;
  }
}

void UniversalTelegramBot::closeClient() {
  if (client->connected()) {
    if (_debug) {
      Serial.println(F("Closing client"));
    }
    client->stop();
  }
}