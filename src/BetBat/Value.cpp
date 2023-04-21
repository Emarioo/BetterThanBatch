#include "BetBat/Value.h"

bool String::copy(String* str){
    if(str->memory.max<memory.used){
        if(!str->memory.resize(memory.used))
            return false;
    }
    str->memory.used = memory.used;
    memcpy((char*)str->memory.data,memory.data,memory.used);
    return true;
}
bool String::operator==(String& str){
    if(memory.used!=str.memory.used) return false;
    for(uint i=0;i<memory.used;i++){
        if(*((char*)memory.data+i)!=*((char*)str.memory.data+i))
            return false;
    }
    return true;
}
bool String::operator!=(String& str){
    return !(*this==str);
}
bool String::operator==(const char* str){
    if(!str) false;
    int len = strlen(str);
    if((int)memory.used!=len) return false;
    return !strncmp((char*)memory.data,str,len);
}
bool String::operator!=(const char* str){
    return !(*this==str);
}
String& String::operator=(const char* str){
    uint64 len = strlen(str);
    if(memory.max<len){
        if(!memory.resize(len)) {
            return *this;
        }
    }
    memory.used = len;
    memcpy(memory.data,str,len);
    return *this;
}
std::string& operator+=(std::string& str, String& str2){
    int len = str.length();
    str.resize(len+str2.memory.used);
    memcpy((char*)str.data()+len,str2.memory.data,str2.memory.used);
    return str;
}

String::operator std::string(){
    std::string out="";
    out.resize(memory.used);
    memcpy((void*)out.data(),memory.data,memory.used);
    return out;
}
void PrintRawString(String& str, int truncate){
    using namespace engone;
    int limit = str.memory.used;
    if(truncate>0&&(int)str.memory.used>truncate){
        limit = truncate;
    }
    for(int i=0;i<limit;i++){
        char chr =*((char*)str.memory.data+i);
        if(chr=='\n'){ 
            engone::log::out << "\\n";
        }else{
            engone::log::out << chr;
        }
    }
    if(truncate>0&&(int)str.memory.used>truncate){
        log::out << "..."<<(str.memory.used-truncate);
    }
}
engone::Logger& operator<<(engone::Logger& logger, String& str){
    for(uint i=0;i<str.memory.used;i++){
        char chr =*((char*)str.memory.data+i);
        logger << chr;
    }
    return logger;   
}