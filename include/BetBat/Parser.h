#pragma once

#include "BetBat/AST.h"

#define PARSE_ERROR 0
#define PARSE_BAD_ATTEMPT 2
// #define PARSE_SUDDEN_END 4
#define PARSE_SUCCESS 1
// success but no accumulation
#define PARSE_NO_VALUE 3
struct ParseInfo {
    ParseInfo(TokenStream* tokens) : tokens(tokens){}
    int index=0;
    TokenStream* tokens;
    int errors=0;
    int funcDepth=0;
    AST* ast=0;

    // Does not handle out of bounds
    Token &prev();
    Token& next();
    Token& now();
    bool revert();
    Token& get(uint index);
    int at();
    bool end();
    void finish();

    // print line of where current token exists
    // not including \n
    void printLine();
    void printPrevLine();

    void nextLine();
};

ASTBody* ParseTokens(TokenStream* tokens, AST* ast, int* outErr=0);