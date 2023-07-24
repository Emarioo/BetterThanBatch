#pragma once

#include <string>

#include "BetBat/Util/Array.h"
#include "Engone/Logger.h"

// #define DISABLE_CUSTOM_SETTINGS

enum SettingType : u32 {
    UNKOWN_SETTING = (u32)-1,
    DEFAULT_TARGET = 0,
    DEFAULT_LINKER,
    EXTRA_FLAGS, // these flags will be passed to the compiler

};
const char* ToString(SettingType type);
engone::Logger& operator<<(engone::Logger& logger, SettingType type);
SettingType ToSetting(const char* str);

struct UserProfile {

    void cleanup();

    // key value pairs
    struct Value {
        u32 contentIndex = 0;
        std::string value;
        bool inUse = false;
        bool invalid() { return !inUse; }
    };
    struct Pair : Value {
        std::string key;
    };
    DynamicArray<Value> knownSettings; // SettingType is the index
    DynamicArray<Pair> customSettings; // disabled for now
    // The reason you don't want custom settings is because you may
    // mispell a known setting which causes unexepected things.
    // if it's somehow marked as a custom setting then sure.

    struct Comment {
        bool enclosed = false;
        std::string str;
    };
    DynamicArray<Comment> comments;
    
    enum ContentType : u8 {
        KNOWN,
        CUSTOM,
        COMMENT,
    };
    struct Spot {
        ContentType type;
        u16 index=(u16)-1;
        u8 newLines=(u8)-1;
    };
    DynamicArray<Spot> contentOrder;

    std::string* getSetting(const std::string& key);
    std::string* getSetting(SettingType type);

    bool setSetting(const std::string& key, const std::string& value);
    void setSetting(SettingType key, const std::string& value);

    void delSetting(const std::string& key);
    void delSetting(SettingType key);

    bool serialize(const std::string& path);
    bool deserialize(const std::string& path);

    void print(bool printComments = false);
};