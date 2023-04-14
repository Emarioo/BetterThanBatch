#pragma once

#include "Engone/Alloc.h"
#include "Engone/Logger.h"
#include "BetBat/Config.h"

#define REF_NUMBER 1
#define REF_STRING 2
struct Ref {
    int type=0;
    int index=0;
};

struct String {
    engone::Memory memory{1};
    
    // copies this into str
    bool copy(String* str);
    
    bool operator==(String& str);
    bool operator!=(String& str);
    bool operator==(const char* str);
    bool operator!=(const char* str);
    
    operator std::string();
};
std::string& operator+=(std::string& str, String& str2);
// \n is replaced with \\n
void PrintRawString(String& str);

engone::Logger& operator<<(engone::Logger& logger, String& str);
struct Number{
    Decimal value;
};