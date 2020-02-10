#pragma once

#include <Arduino.h>

struct User {
    long id;
    String firstName;
    String lastName;
    String username;
    String languageCode;
    bool isBot;
};

struct ChatPhoto {
    String smallFileId;
    String bigFileId;
};

struct Chat {
    int id;
    String type;
    String title;
    String username;
    String firstName;
    String lastName;
    String allMembersAreAdministrator;
    ChatPhoto photo; 
    String description;
    String inviteLink;
    String stickerSetName;
    bool canSetStickerSet;
};

struct Message {
    long id;
    int date;
    Chat chat;
    String text;
    User from;
    User forwardFrom;
    Chat forwardFromChat;
    long forwardFromMessageId;
    long forwardDate;
    int mediaGroupId;
    String caption;
    String newChatTitle;
    bool deleteChatPhoto;
    bool groupChatCreated;
    bool supergroupChatCreated;
    bool channelChatCreated;
    long migrateToChatId;
    int migrateFromChatId;
    String forwardSignature;
    String authorSignature;
    String connectedWebsite;
    User newChatMember;
    User leftChatMember;
    Message* replyToMessage;
    Message* pinnedMessage;
};


/*
'entities' => ArrayOfMessageEntity::class,
'caption_entities' => ArrayOfMessageEntity::class,
'audio' => Audio::class,
'document' => Document::class,
'photo' => ArrayOfPhotoSize::class,
'sticker' => Sticker::class,
'video' => Video::class,
'animation' => Animation::class,
'voice' => Voice::class,
'contact' => Contact::class,
'location' => Location::class,
'venue' => Venue::class,
'poll' => Poll::class,
'new_chat_photo' => ArrayOfPhotoSize::class,
'invoice' => Invoice::class,
'successful_payment' => SuccessfulPayment::class,
*/

struct InlineQuery {

};

struct ChosenInlineResult {

};

struct CallbackQuery {

};

struct ShippingQuery {

};

struct PreCheckoutQuery {

};

struct Update {
    long updateId;
    Message message;
    Message editedMessage;
    Message channelPost;
    Message editedChannelPost;
    InlineQuery inlineQuery;
    ChosenInlineResult chosenInlineResult;
    CallbackQuery callbackQuery;
    ShippingQuery shippingQuery;
    PreCheckoutQuery preCheckoutQuery;
};