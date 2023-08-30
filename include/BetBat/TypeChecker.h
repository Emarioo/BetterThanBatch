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
    QuickArray<CheckImpl> checkImpls{};

    int funcDepth=0; // FUNC_ENTER
    bool ignoreErrors = false; // not thread safe, you need one of these per thread
    
    bool hasErrors();
    bool hasForeignErrors();

    FuncImpl* currentFuncImpl = nullptr;
    ASTFunction* currentAstFunc = nullptr;
    
    // struct step    
    bool completedStructs = false;
    bool showErrors = false;
    bool anotherTurn = false;

    u32 currentPolyVersion=0;
    QuickArray<ContentOrder> currentContentOrder;
    ContentOrder getCurrentOrder() { Assert(currentContentOrder.size()>0); return currentContentOrder.last(); }

    DynamicArray<TypeId> prevVirtuals{};
    TinyArray<TypeId> temp_defaultArgs{};
};

int TypeCheck(AST* ast, ASTScope* scope, CompileInfo* compileInfo);