
#pragma once

#include "BetBat/Tokenizer.h"

struct TokenRef{
    uint16 index=0;
    uint16 flags=0;
    
    operator int() { return index; };
};
// Tokens in argument
typedef QuickArray<TokenRef> TokenList;
typedef DynamicArray<TokenList> Arguments;
struct TokenSpan {
    int start=0;
    int end=0;
    TokenStream* stream=nullptr;
};
engone::Logger& operator<<(engone::Logger& logger, const TokenSpan& span);
struct CertainMacro {
    ~CertainMacro(){
        cleanup();
    }
    void cleanup() {
        // sortedParameters.cleanup();
        parameters.cleanup();
    }
    Token name{};
    TokenSpan contentRange{};
    int start=-1;
    int end=-1;

    // struct Parameter {
    //     Token name{};
    //     int index=-1;
    // };

    // DynamicArray<Parameter> sortedParameters{};

    // I tested how fast an unordered_map would be and it was slower than a normal array.
    DynamicArray<Token> parameters;
    void addParam(const Token& name);
    int indexOfVariadic=-1; // tells you which argument is the infinite one, -1 for none
    // returns index to argumentNames
    bool isVariadic() { return indexOfVariadic != -1; }
    u32 nonVariadicArguments() { return isVariadic() ? parameters.size() - 1 : parameters.size(); }
    // bool isBlank() { return blank; }
    // bool blank = false;
    // -1 if not found
    int matchArg(const Token& token);
    // int matchArg(Token token);
};
struct RootMacro {
    Token name;
    // TODO: Allocator for macros
    std::unordered_map<int, CertainMacro*> certainMacros;
    bool hasVariadic = false;
    bool hasBlank = false;
    CertainMacro variadicMacro{};
    // CertainMacro* matchArgCount(int count, bool includeInf = true);
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

    QuickArray<CertainMacro*> superMacros{};
    QuickArray<Arguments*> superArgs{};

    bool matchSuperArg(const Token& name, CertainMacro*& superMacro, Arguments*& superArgs, int& argIndex);

    CertainMacro* macro=0;

    TokenList output{};
};
struct Env {
    TokenSpan range{};
    int callIndex=0;
    int ownsCall=-1; // env will pop the call when popped

    bool firstTime=true;
    bool unwrapOutput=false;
    int outputToCall=-1; // index of call
    // int calculationArgIndex=-1; // used with unwrap

    int finalFlags = 0;
};
struct MacroCall {
    RootMacro* rootMacro = nullptr;
    CertainMacro* certainMacro = nullptr;
    DynamicArray<TokenSpan> argumentRanges{}; // input to current range
    bool unwrapped=false;
    bool useDetailedArgs=false;
    DynamicArray<DynamicArray<const Token*>> detailedArguments{};
};
struct CompileInfo;
struct PreprocInfo {
    // TokenStream inTokens{};
    // TokenStream tokens{};
    TokenStream* inTokens = 0;
    TokenStream* outTokens = 0;
    TokenStream* tempStream = 0; // Used when parsing final contents of a macro.
    bool usingTempStream = false; // used to find recursion bugs

    CompileInfo* compileInfo = 0; // for caching includes

    u32 errors = 0;
    u32 warnings = 0;
    bool ignoreErrors = false;
    // int ifdefDepth=0;
    // token range
    // start ... end-1
    // end index is not included
    // std::unordered_map<std::string,RootMacro> _macros; // moved to compiler info for global access
    
    Token parsedMacroName{}; // #line and #column use this when inside macros

    int macroRecursionDepth=0;

    // DynamicArray<CertainMacro*> superMacros{};
    // DynamicArray<Arguments*> superArgs{};
    // bool matchSuperArg(Token& name, CertainMacro*& superMacro, Arguments*& superArgs, int& argIndex);

    // DynamicArray<std::string> defineStack;
    
    // ptr may be invalidated if you add defines to the unordered map.
    // RootMacro* createRootMacro(const Token& name);
    // RootMacro* matchMacro(const Token& token);
    
    void addToken(Token inToken);
    
    int index=0; // using readHead from inTokens instead
    int at();
    bool end();
    Token& now();
    Token& next();
    int length();
    Token& get(int index);
    // void nextline();

    // Used when parsing macros
    bool macroArraysInUse = false;
    DynamicArray<const Token*> outputTokens{};
    DynamicArray<i16> outputTokensFlags{};
    // outputTokens._reserve(15000);
    DynamicArray<MacroCall> calls{};
    // calls._reserve(30);
    DynamicArray<Env> environments{};
    // environments._reserve(50);
    DynamicArray<bool> unwrappedArgs{};
};
TokenStream* Preprocess(CompileInfo* compileInfo, TokenStream* tokens);
void PreprocessImports(CompileInfo* compileInfo, TokenStream* tokens);