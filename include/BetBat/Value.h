#pragma once

#include "Engone/Alloc.h"
#include "Engone/Logger.h"
#include "BetBat/Config.h"

#include <string>

#define REF_NUMBER 1
#define REF_STRING 2
#define REF_NULL 3
// NOTE: REF_NULL isn't 0 because we may be able to catch some errors if we find 0.
//  0 is the default value indicating that we never touched or care about the value.
//  if we suddenly use such a ref then there is a bug in the parser or interpreter.
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
    
    String& operator=(const char* str);

    operator std::string();
};
std::string& operator+=(std::string& str, String& str2);
// \n is replaced with \\n
void PrintRawString(String& str, int truncate=0);

engone::Logger& operator<<(engone::Logger& logger, String& str);
struct Number{
    Decimal value;
};