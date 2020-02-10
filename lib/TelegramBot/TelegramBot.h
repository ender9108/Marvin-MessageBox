#pragma once

#include <Arduino.h>
#ifndef ARDUINOJSON_DECODE_UNICODE
    #define ARDUINOJSON_DECODE_UNICODE 1
#endif
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "TelegramStruct.h"

#define TELEGRAM_HOST           "api.telegram.org"
#define TELEGRAM_PORT           443
#define TELEGRAM_MAX_MSG        3
#define TELEGRAM_TTR            10000

#define TELEGRAM_EVT_NEW_MSG    1

typedef bool (*DataAvailable)();
typedef byte (*GetNextByte)();
typedef void (*EventCallback)(Message *msg, long lastUpdateId);

class TelegramBot {
    public:
        TelegramBot(WiFiClientSecure &wifiClient);
        TelegramBot(WiFiClientSecure &wifiClient, String token);
        void setToken(String token);
        void enableDebugMode();
        User getMe();
        int getUpdates(int offset = 0, int limit = TELEGRAM_MAX_MSG);
        DynamicJsonDocument sendMessage(long chatId, String text, String parseMode = "", bool disablePreview = false, long replyToMessageId = 0, bool disableNotification = false);
        Message* getMessages(bool forceUpdate = false);
        Message getLastMessage(bool forceUpdate = false);
        long getLastUpdateId();
        int loop();
        bool on(int event, EventCallback callback);
        DynamicJsonDocument sendContact(long chatId, String phoneNumber, String firstName, String lastName = "", long replyToMessageId = 0, bool disableNotification = false);
        DynamicJsonDocument sendChatAction(long chatId, String action);
        DynamicJsonDocument sendLocation(long chatId, float latitude, float longitude, long replyToMessageId = 0, bool disableNotification = false, int livePeriod = 0);
        DynamicJsonDocument editMessageReplyMarkup(long chatId, long messageId, long inlineMessageId = 0);
        DynamicJsonDocument deleteMessage(long chatId, long messageId);
        DynamicJsonDocument editMessageLiveLocation(long chatId, long messageId, long inlineMessageId, float latitude, float longitude);
        DynamicJsonDocument stopMessageLiveLocation(long chatId, long messageId, long inlineMessageId);
        DynamicJsonDocument forwardMessage(long chatId, long fromChatId, long messageId, bool disableNotification = false);
        DynamicJsonDocument kickChatMember(long chatId, long userId, long untilDate = -1);
        DynamicJsonDocument unbanChatMember(long chatId, long userId);
        DynamicJsonDocument editMessageText(long chatId, long messageId, String text, String parseMode = "", bool disablePreview = false, long inlineMessageId = 0);
        DynamicJsonDocument editMessageCaption(long chatId, long messageId, String caption = "", long inlineMessageId = 0);
        DynamicJsonDocument sendPhoto(long chatId, String photo, String caption = "", long replyToMessageId = 0, bool disableNotification = false, String parseMode = "");
        DynamicJsonDocument sendDocument(long chatId, String document, String caption = "", long replyToMessageId = 0, bool disableNotification = false, String parseMode = "");
        
        /* @todo implement */
        
        DynamicJsonDocument sendPhoto(
            long chatId, 
            int fileSize, 
            DataAvailable dataAvailableCallback, 
            GetNextByte getNextByteCallback, 
            String caption = "", 
            long replyToMessageId = 0, 
            bool disableNotification = false, 
            String parseMode = ""
        );
        DynamicJsonDocument sendDocument(
            long chatId, 
            int fileSize, 
            DataAvailable dataAvailableCallback, 
            GetNextByte getNextByteCallback, 
            String caption = "", 
            long replyToMessageId = 0, 
            bool disableNotification = false, 
            String parseMode = ""
        );
    private:
        String token;
        WiFiClientSecure *client;
        bool debugMode = false;
        User botUser;
        Message messages[TELEGRAM_MAX_MSG];
        String baseAction;
        long lastUpdateId = 0;
        long lastUpdateTime = 0;
        EventCallback onNewMessage;
        DynamicJsonDocument sendGetCommand(String action);
        DynamicJsonDocument sendPostCommand(String action, JsonObject payload);
        DynamicJsonDocument buildJsonResponseError(int statusCode, String message);
        bool parseMessages(JsonObject message, int index);
        Message hydrateMessageStruct(JsonObject message);
        User hydrateUserStruct(JsonObject message);
        Chat hydrateChatStruct(JsonObject jsonChat);
        void logger(String message, bool endLine = true);
};