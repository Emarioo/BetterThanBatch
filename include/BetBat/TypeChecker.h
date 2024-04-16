#pragma once

#include "BetBat/AST.h"
#include "BetBat/PhaseContext.h"

struct TypeChecker {
    struct CheckImpl {
        ASTFunction* astFunc=nullptr;
        FuncImpl* funcImpl=nullptr;
        // ASTScope* scope = nullptr; // scope where function came from
    };
    QuickArray<CheckImpl> checkImpls{};
};

struct CompileInfo;
struct CheckInfo : public PhaseContext {
    AST* ast = nullptr;
    Reporter* reporter = nullptr;
    TypeChecker* typeChecker = nullptr;
    
    int funcDepth=0; // FUNC_ENTER
    bool ignoreErrors = false; // not thread safe, you need one of these per thread
    
    FuncImpl* currentFuncImpl = nullptr;
    ASTFunction* currentAstFunc = nullptr;
    
    // struct step    
    bool incompleteStruct = false;
    bool completedStruct = false;
    bool showErrors = false;

    u32 currentPolyVersion=0;
    QuickArray<ContentOrder> currentContentOrder;
    ContentOrder getCurrentOrder() { Assert(currentContentOrder.size()>0); return currentContentOrder.last(); }

    // DynamicArray<TypeId> prevVirtuals{};
    QuickArray<TypeId> temp_defaultArgs{};
};

int TypeCheck(AST* ast, ASTScope* scope, CompileInfo* compileInfo);

void TypeCheckEnums(AST* ast, ASTScope* scope, Compiler* compiler);
// may fail in which case we should try again
SignalIO TypeCheckStructs(AST* ast, ASTScope* scope, Compiler* compiler, bool ignore_errors, bool* changed);
void TypeCheckFunctions(AST* ast, ASTScope* scope, Compiler* compiler);

void TypeCheckBodies(AST* ast, ASTScope* scope, Compiler* compiler);