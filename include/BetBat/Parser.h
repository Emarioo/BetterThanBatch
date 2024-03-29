#pragma once

#include "BetBat/AST.h"
#include "BetBat/PhaseContext.h"

struct CompileInfo;
struct ParseInfo : public PhaseContext {
    ParseInfo(TokenStream* tokens) : tokens(tokens){}
    int index=0; // index of the next token, index-1 is the current token
    TokenStream* tokens;
    int funcDepth=0;
    AST* ast=nullptr;

    QuickArray<ContentOrder> nextContentOrder;
    ContentOrder getNextOrder() { Assert(nextContentOrder.size()>0); return nextContentOrder.last(); }

    ScopeId currentScopeId=0;
    std::string currentNamespace = "";
    bool ignoreErrors = false;

    struct LoopScope {
        DynamicArray<ASTStatement*> defers; // apply these when continue, break or return is encountered
    };
    // function or global scope
    struct FunctionScope {
        DynamicArray<ASTStatement*> defers; // apply these when return is encountered
        DynamicArray<LoopScope> loopScopes;
    };
    DynamicArray<FunctionScope> functionScopes;

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
    // void printLine();
    // void printPrevLine();

    // void nextLine();
};

ASTScope* ParseTokenStream(TokenStream* tokens, AST* ast, CompileInfo* compileInfo, std::string theNamespace = "");