#include "BetBat/Value.h"

bool String::copy(String* str){
    if(!str->memory.resize(memory.used))
        return false;
    memcpy((char*)str->memory.data,memory.data,memory.used);
    str->memory.used = memory.used;
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
void PrintRawString(String& str){
    for(uint i=0;i<str.memory.used;i++){
        char chr =*((char*)str.memory.data+i);
        if(chr=='\n'){ 
            engone::log::out << "\\n";
        }else{
            engone::log::out << chr;
        }
    }
}
engone::Logger& operator<<(engone::Logger& logger, String& str){
    for(uint i=0;i<str.memory.used;i++){
        char chr =*((char*)str.memory.data+i);
        logger << chr;
    }
    return logger;   
}