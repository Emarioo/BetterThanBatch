#pragma once

#include "BetBat/Tokenizer.h"

// TODO: also defined in parser. define where both use it?
#define PARSE_ERROR 0
#define PARSE_BAD_ATTEMPT 2
#define PARSE_SUCCESS 1
// success but no accumulation
#define PARSE_NO_VALUE 3

struct TokenRef{
    uint16 index=0;
    uint16 flags=0;
    
    operator int() { return index; };
};
// Tokens in argument
typedef std::vector<TokenRef> TokenList;
typedef std::vector<TokenList> Arguments;
struct CertainMacro {
    int start=-1;
    int end=-1;
    std::vector<std::string> argumentNames;
    bool called=false;
    int infiniteArg=-1; // -1 for none, otherwise the index of where is indicated
    // returns index to argumentNames
    // -1 if not found
    int matchArg(Token& token);
};
struct RootMacro {
    std::unordered_map<int, CertainMacro> certainMacros;
    bool hasInfinite=false;
    CertainMacro infDefined{};
    CertainMacro* matchArgCount(int count, bool includeInf = true);
};
struct EvalInfo {
    // step 1
    RootMacro* rootMacro=0;
    Arguments arguments{};
    int argIndex=0;
    TokenList workingRange{};
    int workIndex=0;
    Token macroName{};

    int finalFlags=0;

    std::vector<CertainMacro*> superMacros{};
    std::vector<Arguments*> superArgs{};

    bool matchSuperArg(Token& name, CertainMacro*& superMacro, Arguments*& superArgs, int& argIndex);

    CertainMacro* macro=0;

    TokenList output{};
};
struct CompileInfo;
struct PreprocInfo {
    // TokenStream inTokens{};
    // TokenStream tokens{};
    TokenStream* inTokens = 0;
    TokenStream* outTokens = 0;
    TokenStream* tempStream = 0; // Used when parsing final contents of a macro.
    bool usingTempStream = false;

    CompileInfo* compileInfo = 0; // for caching includes

    int errors=0;
    int warnings=0;
    
    
    // int ifdefDepth=0;
    // token range
    // start ... end-1
    // end index is not included
    std::unordered_map<std::string,RootMacro> macros;
    
    int evaluationDepth=0;

    // std::vector<CertainMacro*> superMacros{};
    // std::vector<Arguments*> superArgs{};
    // bool matchSuperArg(Token& name, CertainMacro*& superMacro, Arguments*& superArgs, int& argIndex);

    
    // std::vector<std::string> defineStack;
    
    // ptr may be invalidated if you add defines to the unordered map.
    RootMacro* matchMacro(Token& token);
    
    void addToken(Token inToken);
    
    // int index=0; // using readHead from inTokens instead
    int at();
    bool end();
    Token& now();
    Token& next();
    int length();
    Token& get(int index);
    void nextline();
};
void Preprocess(CompileInfo* compileInfo, TokenStream* tokens, int* error=0);