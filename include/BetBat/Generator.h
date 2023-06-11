#pragma once

#include "BetBat/AST.h"
#include "BetBat/Bytecode.h"

struct GenInfo {
    Bytecode* code=0;
    AST* ast=0;
    int errors=0;
    
    struct AlignInfo {
        int diff=0;
        int size=0;
    };
    std::vector<AlignInfo> stackAlignment;
    int relativeStackPointer=0;
    void addPop(int reg);
    void addPush(int reg);
    void addIncrSp(i16 offset);
    int saveStackMoment();
    void restoreStackMoment(int moment);

    std::vector<int> constStringMapping;

    ASTFunction* currentFunction=0;
    ScopeId currentScopeId = 0;
    ScopeId fromScopeId = 0; // AST_FROM_NAMESPACE
    
    int funcDepth=0;

    struct ResolveCall {
        int bcIndex = 0;
        ASTFunction* astFunc = 0;
    };
    std::vector<ResolveCall> callsToResolve;
    
    // TODO: avoid vector and unordered_map. resizing vector may do unwanted things with the map (like whole a copy).
    // std::vector<FunctionScope*> functionScopes;
    // int currentFunctionScope=0;
    // FunctionScope* getFunctionScope(int index=-1);
    int currentFrameOffset=0;
    static const int ARG_OFFSET=16;
    // static const 

    
};

// bool EvaluateTypes(AST* ast, ASTScope* body, int* err);

Bytecode* Generate(AST* ast, int* err);