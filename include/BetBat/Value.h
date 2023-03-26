#pragma once

#include "Engone/Alloc.h"
#include "Engone/Logger.h"

struct String {
    engone::Memory memory{1};
    
    // copies this into str
    bool copy(String* str);
    
    bool operator==(String& str);
    
    operator std::string();
};
std::string& operator+=(std::string& str, String& str2);
// \n is replaced with \\n
void PrintRawString(String& str);
typedef float Decimal;
engone::Logger& operator<<(engone::Logger& logger, String& str);
struct Number{
    Decimal value;    
};