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
};

int TypeCheck(AST* ast, ASTScope* scope, CompileInfo* compileInfo);
// TypeId CheckType(AST* ast, ScopeId scopeId, TypeId typeString, const TokenRange& tokenRange, bool* printedError);
// TypeId CheckType(AST* ast, ScopeId scopeId, Token typeString, const TokenRange& tokenRange, bool* printedError);