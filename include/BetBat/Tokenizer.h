#pragma once

#include "Engone/Alloc.h"
#include "Engone/Logger.h"

#define TOKEN_SUFFIX_LINE_FEED 1
// SPACE suffic is remove if LINE_FEED is present in mask/flag 
#define TOKEN_SUFFIX_SPACE 2
#define TOKEN_QUOTED 4

#define TOKEN_PRINT_LN_COL 1
#define TOKEN_PRINT_SUFFIXES 2
#define TOKEN_PRINT_QUOTES 4

// If defined, Tokenize will print information
// #define TOKENIZER_LOG

struct Token {
    char* str=0; // NOT null terminated
    int length=0;
    int flags=0; // bit mask, TOKEN_...
    int line=0;
    int column=0;

    bool operator==(const char* str);
    bool operator!=(const char* str);
    
    operator std::string();

    // prints the string, does not print suffix, line or column
    void print(int printFlags = TOKEN_PRINT_QUOTES);
};
engone::Logger& operator<<(engone::Logger& logger, Token& token);
struct Tokens {
    engone::Memory tokens{sizeof(Token)}; // the tokens themselves
    engone::Memory tokenData{1}; // the data the tokens refer to
    
    void cleanup();

    bool add(Token token);
    Token& get(uint index);
    uint length();
    // flags is a bitmask, TOKEN_PRINT_...
    void printTokens(int tokensPerLine = 14, int flags = TOKEN_PRINT_QUOTES);
    
    void printData(int charsPerLine = 40);
};
// DO NOT FREE textData! 
// The function is responsible for that.
Tokens Tokenize(engone::Memory textData);