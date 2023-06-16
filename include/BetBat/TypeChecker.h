#pragma once
#include "BetBat/AST.h"

struct CheckInfo {
    AST* ast = 0;
    int errors=0;
    int funcDepth=0;
    
    // struct step    
    bool completedStructs = false;
    bool showErrors = false;
    bool anotherTurn = false;
};

int TypeCheck(AST* ast, ASTScope* scope);