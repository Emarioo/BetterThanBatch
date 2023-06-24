#pragma once
#include "BetBat/AST.h"

struct CompileInfo;
struct CheckInfo {
    AST* ast = nullptr;
    CompileInfo* compileInfo=nullptr;
    int errors=0;
    
    int funcDepth=0;
    
    // struct step    
    bool completedStructs = false;
    bool showErrors = false;
    bool anotherTurn = false;
};

int TypeCheck(AST* ast, ASTScope* scope, CompileInfo* compileInfo);