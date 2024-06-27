#pragma once

#include "BetBat/AST.h"
#include "BetBat/PhaseContext.h"

struct TypeChecker {
    void cleanup() {
        // imports.cleanup();
        // checkImpls.cleanup();
    }

    struct CheckImpl {
        ASTFunction* astFunc=nullptr;
        FuncImpl* funcImpl=nullptr;
        // ASTScope* scope = nullptr; // scope where function came from
    };
    // struct Import {
    //     QuickArray<CheckImpl> checkImpls{};
    // };
    // BucketArray<Import> imports;

    // QuickArray<CheckImpl> checkImpls{};
};

struct CompileInfo;
struct TyperContext : public PhaseContext {
    AST* ast = nullptr;
    Reporter* reporter = nullptr;
    TypeChecker* typeChecker = nullptr;

    TyperContext() : info(*this) { } // well this is dumb
    TyperContext& info;

    // u32 current_import_id;
    
    int funcDepth=0; // FUNC_ENTER
    bool ignoreErrors = false; // not thread safe, you need one of these per thread
    bool ignore_polymorphism_with_struct = false;


    FuncImpl* currentFuncImpl = nullptr;
    ASTFunction* currentAstFunc = nullptr;
    
    // struct step    
    bool incompleteStruct = false;
    bool completedStruct = false;
    bool showErrors = true;

    bool do_not_check_global_globals = false;
    bool inside_import_scope = false;

    u32 currentPolyVersion=0;
    QuickArray<ContentOrder> currentContentOrder;
    ContentOrder getCurrentOrder() { Assert(currentContentOrder.size()>0); return currentContentOrder.last(); }

    // DynamicArray<TypeId> prevVirtuals{};
    QuickArray<TypeId> temp_defaultArgs{};
    
    TypeId checkType(ScopeId scopeId, TypeId typeString, lexer::SourceLocation location, bool* printedError);
    TypeId checkType(ScopeId scopeId, StringView typeString, lexer::SourceLocation location, bool* printedError);

    SignalIO checkEnums(ASTScope* scope);

    SignalIO checkStructs(ASTScope* scope);
    SignalIO checkStructImpl(ASTStruct* astStruct, TypeInfo* structInfo, StructImpl* structImpl);

    SignalIO checkFunctions(ASTScope* scope);
    SignalIO checkFunction(ASTFunction* function, ASTStruct* parentStruct, ASTScope* scope);
    SignalIO checkFunctionImpl(ASTFunction* func, FuncImpl* funcImpl, ASTStruct* parentStruct, QuickArray<TypeId>* outTypes, StructImpl* parentStructImpl = nullptr);
    SignalIO checkFuncImplScope(ASTFunction* func, FuncImpl* funcImpl);

    SignalIO checkDeclaration(ASTStatement* now, ContentOrder contentOrder, ASTScope* scope);
    SignalIO checkRest(ASTScope* scope);
    SignalIO checkExpression(ScopeId scopeId, ASTExpression* expr, QuickArray<TypeId>* outTypes, bool attempt, int* array_length = nullptr);
    SignalIO checkFncall(ScopeId scopeId, ASTExpression* expr, QuickArray<TypeId>* outTypes, bool attempt, bool operatorOverloadAttempt, QuickArray<TypeId>* operatorArgs = nullptr);

};

int TypeCheck(AST* ast, ASTScope* scope, CompileInfo* compileInfo);

void TypeCheckEnums(AST* ast, ASTScope* scope, Compiler* compiler);
// may fail in which case we should try again
SignalIO TypeCheckStructs(AST* ast, ASTScope* scope, Compiler* compiler, bool ignore_errors, bool* changed);
void TypeCheckFunctions(AST* ast, ASTScope* scope, Compiler* compiler, bool is_initial_import);
// DO NOT TYPE CHECK IMPORT SCOPE TWICE!
void TypeCheckBody(Compiler* compiler, ASTFunction* ast_func, FuncImpl* func_impl, ASTScope* import_scope = nullptr);
// deprecated
// void TypeCheckBodies(AST* ast, ASTScope* scope, Compiler* compiler);