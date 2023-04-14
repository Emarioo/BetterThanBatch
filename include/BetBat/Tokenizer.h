#pragma once

#include "Engone/Alloc.h"
#include "Engone/Logger.h"
#include "Engone/Typedefs.h"

#include "BetBat/Config.h"

#include <string.h>

#define TOKEN_SUFFIX_LINE_FEED 1
// SPACE suffic is remove if LINE_FEED is present in mask/flag 
#define TOKEN_SUFFIX_SPACE 2
#define TOKEN_QUOTED 4

// #define TOKEN_PRINT_LN_COL 1
// #define TOKEN_PRINT_SUFFIXES 2
// #define TOKEN_PRINT_QUOTES 4

struct Token {
    Token() = default;
    Token(const char* str) : str((char*)str), length(strlen(str)) {};
    
    char* str=0; // NOT null terminated
    int length=0;
    int flags=0; // bit mask, TOKEN_...
    int line=0;
    int column=0;

    bool operator==(const char* str);
    bool operator!=(const char* str);
    
    operator std::string();

    // prints the string, does not print suffix, line or column
    void print(int printFlags = 0);
};
engone::Logger& operator<<(engone::Logger& logger, Token& token);
struct Tokens {
    engone::Memory tokens{sizeof(Token)}; // the tokens themselves
    engone::Memory tokenData{1}; // the data the tokens refer to
    
    void cleanup();
    
    // modifies tokenData
    bool append(char chr);
    bool append(Token& tok);
    
    bool add(const char* str);
    bool add(Token token);
    Token& get(uint index);
    uint length();
    // flags is a bitmask, TOKEN_PRINT_...
    void printTokens(int tokensPerLine = 14, int flags = 0);
    
    void printData(int charsPerLine = 40);
    
    // normal print of all tokens
    void print();
    
    bool copy(Tokens& out);
};
Tokens Tokenize(engone::Memory& textData);