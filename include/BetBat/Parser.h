#pragma once

#include "BetBat/Tokenizer.h"
#include "BetBat/BytecodeX.h"
#include "BetBat/AST.h"

#include <unordered_map>

// 2 bits to indicate instruction structure
// 3, 2 or 1 register. 2 registers can also be interpreted as
// 1 register + contant integer
// #define BC_MASK (0b11<<6)
// #define BC_R3 (0b00<<6)
// #define BC_R2 (0b01<<6)
// #define BC_R1 (0b10<<6)

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