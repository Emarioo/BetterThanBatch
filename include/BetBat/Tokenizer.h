#pragma once

#include "Engone/Alloc.h"
#include "Engone/Logger.h"
#include "Engone/Typedefs.h"

#include "BetBat/Utility.h"
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

// #define INT_TO_VERSION(x) ()

#define LAYER_PREPROCESSOR 1
#define LAYER_PARSER 2
#define LAYER_GENERATOR 4
#define LAYER_OPTIMIZER 8
#define LAYER_INTERPRETER 16
engone::Logger& operator<<(engone::Logger& logger, Token& token);
struct Tokens {
    engone::Memory tokens{sizeof(Token)}; // the tokens themselves
    engone::Memory tokenData{1}; // the data the tokens refer to
    
    void finalizePointers();
    
    int lines=0; // counts token suffix.
    // new line in the middle of a token is not counted, multiline strings doesn't work very well
    
    int enabled=0;
    static const int VERSION_MAX = 5;
    char version[VERSION_MAX+1]{0};
    bool isVersion(const char* ver){return !strcmp(version,ver);}
    // TODO: version with major, minor and revision

    void cleanup();
    
    // modifies tokenData
    bool append(char chr);
    // Make sure token has a valid char* pointer
    bool append(Token& tok);
    
    bool add(const char* str);
    bool add(Token token);
    Token& get(uint index);
    uint length();
    // flags is a bitmask, TOKEN_PRINT_...
    void printTokens(int tokensPerLine = 14, bool showlncol=false);
    
    void printData(int charsPerLine = 40);
    
    // normal print of all tokens
    void print();
    // copys everything except tokens
    bool copyInfo(Tokens& out);
    
    bool copy(Tokens& out);
};
double ConvertDecimal(Token& token);
int IsInteger(Token& token);
int ConvertInteger(Token& token);
bool IsName(Token& token);
// Can also be an integer
int IsDecimal(Token& token);
Tokens Tokenize(engone::Memory& textData);