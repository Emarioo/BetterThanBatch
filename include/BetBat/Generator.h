#pragma once

#include "BetBat/AST.h"
#include "BetBat/BytecodeX.h"
#include "BetBat/Config.h"

struct GenInfo {
    BytecodeX* code=0;
    AST* ast=0;
    int errors=0;
    
    struct Variable {
        u32 frameOffset=0;
        DataType dataType=AST_NONETYPE;
    };
    // struct FunctionScope {
        
    // };
    // TODO: avoid vector and unordered_map. resizing vector may do unwanted things with the map (like whole a copy).
    // std::vector<FunctionScope*> functionScopes;
    // int currentFunctionScope=0;
    // FunctionScope* getFunctionScope(int index=-1);
    int nextFrameOffset=0;

    std::unordered_map<std::string,Variable*> variables;
    Variable* getVariable(const std::string& name);
    void removeVariable(const std::string& name);
    Variable* addVariable(const std::string& name);
    
};

BytecodeX* Generate(AST* ast, int* err);