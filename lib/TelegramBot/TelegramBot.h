#pragma once

#include <Arduino.h>
#ifndef ARDUINOJSON_DECODE_UNICODE
    #define ARDUINOJSON_DECODE_UNICODE 1
#endif
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#define TELEGRAM_HOST   "api.telegram.org"
#define TELEGRAM_PORT   443

struct User {
    int id;
    String firstName;
    String lastName;
    String username;
    String languageCode;
    bool isBot;
};

struct Message {
  String text;
  String chat_id;
  String chat_title;
  String from_id;
  User user;
  String date;
  String type;
  float longitude;
  float latitude;
  int update_id;
};

/*
'message_id' => true,
'from' => User::class,
'date' => true,
'chat' => Chat::class,
'forward_from' => User::class,
'forward_from_chat' => Chat::class,
'forward_from_message_id' => true,
'forward_date' => true,
'reply_to_message' => Message::class,
'text' => true,
'entities' => ArrayOfMessageEntity::class,
'caption_entities' => ArrayOfMessageEntity::class,
'audio' => Audio::class,
'document' => Document::class,
'photo' => ArrayOfPhotoSize::class,
'media_group_id' => true,
'sticker' => Sticker::class,
'video' => Video::class,
'animation' => Animation::class,
'voice' => Voice::class,
'caption' => true,
'contact' => Contact::class,
'location' => Location::class,
'venue' => Venue::class,
'poll' => Poll::class,
'new_chat_member' => User::class,
'left_chat_member' => User::class,
'new_chat_title' => true,
'new_chat_photo' => ArrayOfPhotoSize::class,
'delete_chat_photo' => true,
'group_chat_created' => true,
'supergroup_chat_created' => true,
'channel_chat_created' => true,
'migrate_to_chat_id' => true,
'migrate_from_chat_id' => true,
'pinned_message' => Message::class,
'invoice' => Invoice::class,
'successful_payment' => SuccessfulPayment::class,
'forward_signature' => true,
'author_signature' => true,
'connected_website' => true
*/

class TelegramBot {
    public:
        TelegramBot(WiFiClientSecure &wifiClient);
        TelegramBot(WiFiClientSecure &wifiClient, String token);
        void setToken(String token);
        void enableDebugMode();
        User getMe();
        
    private:
        String token;
        HTTPClient *client;
        WiFiClientSecure *wifiClient;
        bool debugMode = false;
        User user;
        void close();
        DynamicJsonDocument sendGetCommand(String action);
        DynamicJsonDocument sendPostCommand(String action, JsonObject payload);
        void logger(String message, bool endLine = false);
        bool connected();
        DynamicJsonDocument buildJsonResponseError(DynamicJsonDocument response, String message);
};