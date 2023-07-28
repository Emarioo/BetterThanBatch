#pragma once
#include "BetBat/AST.h"

struct CompileInfo;
struct CheckInfo {
    AST* ast = nullptr;
    CompileInfo* compileInfo=nullptr;
    int errors=0;
    
    struct CheckImpl {
        ASTFunction* astFunc=nullptr;
        FuncImpl* funcImpl=nullptr;
        // ASTScope* scope = nullptr; // scope where function came from
    };
    DynamicArray<CheckImpl> checkImpls{};

    int funcDepth=0;
    
    // struct step    
    bool completedStructs = false;
    bool showErrors = false;
    bool anotherTurn = false;

    u32 currentPolyVersion=0;
    std::vector<ContentOrder> currentContentOrder;
    ContentOrder getCurrentOrder() { Assert(currentContentOrder.size()>0); return currentContentOrder.back(); }
};

int TypeCheck(AST* ast, ASTScope* scope, CompileInfo* compileInfo);