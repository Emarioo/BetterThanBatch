#include "BetBat/UserProfile.h"

#include "Engone/PlatformLayer.h"

#include "BetBat/Util/Tracker.h"

// #define _UP_LOG(X) X
#define _UP_LOG(X) ;

const char* ToString(SettingType type){
    #define CASE(X,N) case X: return N;
    switch(type){
        CASE(DEFAULT_TARGET, "default-target")
        CASE(EXTRA_FLAGS,"extra-flags")
    }
    return "unknown";
    #undef CASE
}
SettingType ToSetting(const std::string& str){
    #define CASE(N,X) if(str==X) return N;
    CASE(DEFAULT_TARGET, "default-target")
    CASE(EXTRA_FLAGS,"extra-flags")
    return UNKOWN_SETTING;
    #undef CASE
}
engone::Logger& operator<<(engone::Logger& logger, SettingType type){
    return logger << ToString(type);
}
std::string* UserProfile::getSetting(SettingType key){
    auto ptr = knownSettings.getPtr((u32)key);
    if(!ptr || !ptr->inUse)
        return nullptr;
    return &ptr->value;
}
std::string* UserProfile::getSetting(const std::string& key){
    auto type = ToSetting(key);
    if(type != UNKOWN_SETTING)
        return getSetting(type);
    for(int i=0;i<customSettings.size();i++){
        if(customSettings[i].key == key) {
            if(customSettings[i].invalid())
                return nullptr;
            return &customSettings[i].key;
        }
    }
    return nullptr;
}
void UserProfile::setSetting(SettingType key, const std::string& value){
    knownSettings.resize((u32)key+1);
    auto ptr = knownSettings.getPtr((u32)key);
    Assert(ptr);
    ptr->inUse = true;
    ptr->value = value;
}
bool UserProfile::setSetting(const std::string& key, const std::string& value){
    using namespace engone;
    auto type = ToSetting(key);
    if(type != UNKOWN_SETTING) {
        getSetting(type);
        return true;
    }
    #ifdef DISABLE_CUSTOM_SETTINGS
    log::out << log::RED << "Unknown setting "<< key <<" for user profile\n";
    return false;
    #else
    int index = -1;
    for(int i=0;i<customSettings.size();i++){
        if(customSettings[i].key == key) {
            index = i;
            break;
        }
    }
    if(index == -1) {
        index = customSettings.size();
        customSettings.add({});
        customSettings.last().key = key;
    }
    customSettings[index].inUse = true;
    customSettings[index].value = value;
    return true;
    #endif
}
void UserProfile::delSetting(SettingType key){
    auto ptr = knownSettings.getPtr((u32)key);
    Assert(ptr);
    ptr->inUse = false;
}
void UserProfile::delSetting(const std::string& key){
    using namespace engone;
    auto type = ToSetting(key);
    if(type != UNKOWN_SETTING) {
        getSetting(type);
        return;
    }

    for(int i=0;i<customSettings.size();i++){
        if(customSettings[i].key == key) {
            customSettings[i].inUse = false;
            return;
        }
    }
}
void UserProfile::cleanup(){
    contentOrder.cleanup();
    comments.cleanup();
    knownSettings.cleanup();
    customSettings.cleanup();
}
bool UserProfile::serialize(const std::string& path){
    using namespace engone;

    auto file = FileOpen(path, nullptr, FILE_ALWAYS_CREATE);
    if(!file) return false;

    // TODO: Use a buffer and sprintf and decrease the amount of calls to FileWrite

    for(int index=0;index<contentOrder.size();index++){
        auto& spot = contentOrder[index];

        switch(spot.type) {
        case COMMENT: {
            Comment& com = comments[spot.index];
            if(com.enclosed)
                FileWrite(file, "/*",2);
            else if (com.hashtag)
                FileWrite(file, "#",1);
            else
                FileWrite(file, "//",2);

            FileWrite(file, com.str.data(), com.str.length());
            
            if(com.enclosed)
                FileWrite(file, "*/",2);
        }
        break; case KNOWN: {
            auto& setting = knownSettings[spot.index];
            if(!setting.invalid()) {
                const char* key = ToString((SettingType)spot.index);
                int len = strlen(key);
                std::string& value = setting.value;

                if(key[0] == ' ' || key[len-1] == ' ')
                    FileWrite(file,"\"",1);
                FileWrite(file, key, len);
                if(key[0] == ' ' || key[len-1] == ' ')
                    FileWrite(file,"\"",1);

                FileWrite(file, " = ", 3);
                
                if(value[0] == ' ' || value.back() == ' ')
                    FileWrite(file,"\"",1);
                FileWrite(file, value.data(), value.length());
                if(value[0] == ' ' || value.back() == ' ')
                    FileWrite(file,"\"",1);
            }
        }
        break; case CUSTOM: {
            auto& setting = customSettings[spot.index];
            if(!setting.invalid()) {
                std::string& key = setting.key;
                std::string& value = setting.value;

                if(key[0] == ' ' || key.back() == ' ')
                    FileWrite(file,"\"",1);
                FileWrite(file, key.data(), key.length());
                if(key[0] == ' ' || key.back() == ' ')
                    FileWrite(file,"\"",1);

                FileWrite(file, " = ", 3);
                
                if(value[0] == ' ' || value.back() == ' ')
                    FileWrite(file,"\"",1);
                FileWrite(file, value.data(), value.length());
                if(value[0] == ' ' || value.back() == ' ')
                    FileWrite(file,"\"",1);
            }
        }
        }
        for(int j=0;j<spot.newLines;j++){
            FileWrite(file,"\n",1);
        }
    }

    FileClose(file);
    return true;
}
// some settings to test with
#ifdef GONE
hello = shit
// okat
default-target = "win-x64"
/*
waiting
*/
" for you!" = "sour thumb "
neat = "sour thumb"
#endif
bool UserProfile::deserialize(const std::string& path){
    using namespace engone;
    u64 fileSize=0;
    auto file = FileOpen(path, &fileSize, FILE_ONLY_READ); 
    if(!file) return false;

    if(fileSize == 0){
        FileClose(file);
        return true;
    }

    char* buffer = TRACK_ARRAY_ALLOC(char, fileSize);
    // char* buffer = (char*)Allocate(fileSize);
    bool yes = FileRead(file,buffer,fileSize);
    if(!yes) {
        FileClose(file);
        // Free(buffer, fileSize);
        TRACK_ARRAY_FREE(buffer, char, fileSize);
        return false;
    };
    
    FileClose(file);

    cleanup(); // clear existing content
    
    std::string key{};
    std::string value{};
    std::string comment{};

    bool enclosedComment = false;
    bool hashtagComment = false;
    bool inComment = false;
    bool inQuote = false;
    bool inKey = true;

    bool forcedCodePath = false;

    #define IsSpace(X) (X==' '||X=='\t')

    int index = 0;
    while(index < fileSize){
        char chr = buffer[index];
        char nextChr = 0;
        if(index+1<fileSize)
            nextChr = buffer[index+1];
        index++;

        if(chr == '\r')
            continue;

        if(inComment){
            bool end = enclosedComment ? 
                chr == '*' && nextChr == '/':
                chr == '\n';
            _UP_LOG(log::out << chr;)
            if(!end)
                comment += chr;
            if(chr == '*' && nextChr == '/')
                index++;
            if(end || index == fileSize) {
                _UP_LOG(log::out << "<end-comment>";)
                Comment com{};
                com.str = comment;
                com.enclosed = enclosedComment;
                com.hashtag = hashtagComment;
                u8 newLines = chr == '\n';
                while(index < fileSize) {
                    char chr = buffer[index];
                    if(IsSpace(chr) || chr=='\r') {
                        index++;
                        continue;
                    }
                    if(chr =='\n') {
                        newLines++;
                        index++;
                        continue;
                    }
                    break;
                }
                comments.add(com);
                Assert(comments.size() < (1<<16));
                contentOrder.add({COMMENT, (u16)(comments.size()-1), newLines});

                comment.clear();
                inComment = false;
                enclosedComment = false;
                hashtagComment = false;
            }
            continue;
        } else if(inQuote) {
            if(chr == '"') {
                inQuote = false;
                _UP_LOG(log::out << "<end-quote>";)
                if(inKey || index != fileSize)
                    continue;
                forcedCodePath = true;
            } else {
                _UP_LOG(log::out << chr;)
                if(inKey)
                    key += chr;
                else
                    value += chr;
                continue;
            }
        }
        if(chr == '#') {
            inComment = true;
            hashtagComment = true;
            continue;
        } else if (chr=='/' && nextChr == '/') {
            _UP_LOG(log::out << "<comment>";)
            inComment = true;
            index++;
            continue;
        } else if (chr=='/' && nextChr == '*') {
            _UP_LOG(log::out << "<comment>";)
            inComment = true;
            enclosedComment = true;
            index++;
            continue;
        } else if (chr=='"' && !forcedCodePath) {
            _UP_LOG(log::out << "<quote>";)
            inQuote = true;
            continue;
        }

        if(inKey) {
            Assert(index != fileSize);
            if(chr == '=') {
                int newLen = key.length();
                while(newLen > 0 && IsSpace(key[newLen - 1])) 
                    newLen--;

                key.resize(newLen);

                // log::out << "key: "<<key<<"\n";

                inKey = false;
            } else {
                if(key.empty() && IsSpace(chr)){
                    continue; // trim starting space
                }
                _UP_LOG(
                if(key.empty())
                    log::out << "<key>";
                )
                key += chr;
                _UP_LOG(log::out << chr;)
            }
        } else {
            // The quote handles this case
            // key = "stuff" <- we stop in "inQuote" if it's the end of file.
            // that's no good which is why we allow the quote character to
            // show up here.
            if(chr!='\n' && !forcedCodePath) {
                if(value.empty() && IsSpace(chr)){
                    continue; // trim starting space
                }
                _UP_LOG(if(value.empty())
                    log::out << "<value>";)
                value += chr;
                _UP_LOG(log::out << chr;)
            }
            if(chr == '\n' || index == fileSize) {
                int newLen = value.length();
                while(newLen > 0 && IsSpace(value[newLen - 1])) 
                    newLen--;

                value.resize(newLen);

                inKey = true;

                // log::out << "Setting: "<<key << " : "<<value << "\n";

                u8 newLines = chr == '\n';
                while(index < fileSize) {
                    char chr = buffer[index];
                    if(IsSpace(chr) || chr=='\r') {
                        index++;
                        continue;
                    }
                    if(chr =='\n') {
                        newLines++;
                        index++;
                        continue;
                    }
                    break;
                }

                auto type = ToSetting(key);
                if(type == UNKOWN_SETTING) {
                    bool yes = setSetting(key, value);
                    if(yes){
                        Assert(customSettings.size() < (1<<16));
                        contentOrder.add({CUSTOM, (u16)(customSettings.size()-1), newLines});
                    }
                } else {
                    setSetting(type, value);
                    Assert(knownSettings.size() < (1<<16));
                    contentOrder.add({KNOWN, (u16)(type), newLines});
                }
                key.clear();
                value.clear();
            }
        }
    }

    TRACK_ARRAY_FREE(buffer, char, fileSize);
    // Free(buffer, fileSize);
    _UP_LOG(
    print(true);
    )

    return true;
}

void UserProfile::print(bool printComments){
    using namespace engone;    
    log::out << log::LIME << "\nKnown settings:\n";
    for(int i = 0; i< knownSettings.size();i++){
        if(knownSettings[i].invalid()) continue;
        log::out << ToString((SettingType)i) << " : " <<knownSettings[i].value<<"\n";
    }
    
    log::out << log::LIME << "Custom settings:\n";
    for(int i = 0; i < customSettings.size();i++){
        if(customSettings[i].invalid()) continue;
        log::out << customSettings[i].key << " : " <<customSettings[i].value<<"\n";
    }
    log::out << log::LIME << "Comments:\n";
    for(int i = 0; i < comments.size();i++){
        log::out << "["<<i<<"]: "<<comments[i].str << "\n";
    }
}
UserProfile* UserProfile::CreateDefault(){
    // UserProfile* ptr = (UserProfile*)engone::Allocate(sizeof(UserProfile));
    UserProfile* ptr = TRACK_ALLOC(UserProfile);
    new(ptr)UserProfile();
    return ptr;
}
void UserProfile::Destroy(UserProfile* ptr){
    ptr->~UserProfile();
    TRACK_FREE(ptr, UserProfile);
    // engone::Free(ptr,sizeof(UserProfile));
}