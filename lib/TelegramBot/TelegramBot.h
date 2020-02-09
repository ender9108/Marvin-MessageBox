#pragma once

#include <Arduino.h>
#ifndef ARDUINOJSON_DECODE_UNICODE
    #define ARDUINOJSON_DECODE_UNICODE 1
#endif
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#define TELEGRAM_HOST       "api.telegram.org"
#define TELEGRAM_PORT       443
#define TELEGRAM_MAX_MSG    3

typedef bool (*DataAvailable)();
typedef byte (*GetNextByte)();

struct User {
    long id;
    String firstName;
    String lastName;
    String username;
    String languageCode;
    bool isBot;
};

struct Chat {
    int id;
};

struct Message {
    long id;
    int date;
    Chat chat;
    String chatTitle;
    String text;
    User from;
    float longitude;
    float latitude;
    String type;
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
        int getUpdates(int offset = 0, int limit = TELEGRAM_MAX_MSG);
        bool sendMessage(long chatId, String text, String parseMode = "", bool disablePreview = false, long replyToMessageId = 0, bool disableNotification = false);
        Message* getMessages(bool forceUpdate = true);
        Message getLastMessage(bool forceUpdate = true);
        long getLastMessageId();

        bool sendContact(long chatId, String phoneNumber, String firstName, String lastName = "", long replyToMessageId = 0, bool disableNotification = false);
        bool sendChatAction(long chatId, String action);
        bool sendLocation(long chatId, float latitude, float longitude, long replyToMessageId = 0, bool disableNotification = false, int livePeriod = 0);
        bool editMessageLiveLocation(long chatId, long messageId, long inlineMessageId, float latitude, float longitude);
        bool stopMessageLiveLocation(long chatId, long messageId, long inlineMessageId);
        bool forwardMessage(long chatId, long fromChatId, long messageId, bool disableNotification = false);
        bool sendPhoto(long chatId, String photo, String caption = "", long replyToMessageId = 0, bool disableNotification = false, String parseMode = "");
        bool sendPhoto(long chatId, int fileSize, DataAvailable dataAvailableCallback, GetNextByte getNextByteCallback, String caption = "", long replyToMessageId = 0, bool disableNotification = false, String parseMode = "");
    private:
        String token;
        WiFiClientSecure *client;
        bool debugMode = false;
        User botUser;
        Message messages[TELEGRAM_MAX_MSG];
        String baseAction;
        long lastMessageId = 0;
        DynamicJsonDocument sendGetCommand(String action);
        DynamicJsonDocument sendPostCommand(String action, JsonObject payload);
        DynamicJsonDocument buildJsonResponseError(int statusCode, String message);
        bool hydrateMessageStruct(JsonObject message, int index);
        void logger(String message, bool endLine = true);
};