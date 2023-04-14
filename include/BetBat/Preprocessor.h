#pragma once

#include "BetBat/Tokenizer.h"
#include <unordered_map>

#define PARSE_ERROR 0
#define PARSE_BAD_ATTEMPT 2
#define PARSE_SUCCESS 1
// success but no accumulation
#define PARSE_NO_VALUE 3
struct TokenRange {
    int start=-1;
    union{
        int end=-1;
        int argIndex;
    };
    std::vector<TokenRange>* argValues=0;
};
struct PreprocInfo {
    Tokens inTokens{};
    Tokens tokens{};
    
    int errors=0;
    
    int index=0;
    int at();
    
    struct CertainDefined {
        int start=-1;
        int end=-1;
        std::vector<std::string> argumentNames;
        bool called=false;
        int infiniteArg=-1; // -1 for none, otherwise the index of where is indicated
        // returns index to argumentNames
        // -1 if not found
        int matchArg(Token& token);
    };
    // int ifdefDepth=0;
    // token range
    // start ... end-1
    // end index is not included
    struct Defined{
        std::unordered_map<int, CertainDefined> argDefines;
        bool hasInfinite=false;
        CertainDefined infDefined{};
        CertainDefined* matchArgCount(int count, bool includeInf = true);
    };
    std::unordered_map<std::string,Defined> defines;
    
    int evaluationDepth=0;
    
    // std::vector<std::string> defineStack;
    
    // ptr may be invalidated if you add defines to the unordered map.
    Defined* matchDefine(Token& token);
    
    void addToken(Token inToken);
    
    bool end();
    Token& now();
    Token& next();
    int length();
    Token& get(int index);
    void nextline();
};

void Preprocess(Tokens& tokens, int* error);