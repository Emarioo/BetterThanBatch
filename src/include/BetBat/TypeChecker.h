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

    int FRAME_SIZE = -1;
    int REGISTER_SIZE = -1;

    // u32 current_import_id;
    
    int funcDepth=0; // FUNC_ENTER
    bool ignoreErrors = false; // not thread safe, you need one of these per thread
    bool ignore_polymorphism_with_struct = false;
    bool is_initial_import = false;
    TypeId inferred_type{};

    engone::LinearAllocator scratch_allocator;

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
    SignalIO checkFunctionSignature(ASTFunction* func, FuncImpl* funcImpl, ASTStruct* parentStruct, QuickArray<TypeId>* outTypes, StructImpl* parentStructImpl = nullptr);
    SignalIO checkFunctionScope(ASTFunction* func, FuncImpl* funcImpl);

    SignalIO checkDeclaration(ASTStatement* now, ContentOrder contentOrder, ASTScope* scope);
    SignalIO checkRest(ASTScope* scope);
    SignalIO checkExpression(ScopeId scopeId, ASTExpression* expr, QuickArray<TypeId>* outTypes, bool attempt, int* array_length = nullptr);
    
    SignalIO checkDefaultArguments(ASTFunction* astFunc, FuncImpl* funcImpl, ASTExpressionCall* expr, bool implicit_this, ScopeId scopeId);
    SignalIO checkFncall(ScopeId scopeId, ASTExpression* expr, QuickArray<TypeId>* outTypes, bool attempt, bool operatorOverloadAttempt, QuickArray<TypeId>* operatorArgs = nullptr);
    
    // used by checkFnCall and for-loop with user iterators (in checkRest)
    OverloadGroup::Overload* computePolymorphicFunction(ASTFunction* polyFunc, StructImpl* parentStructImpl, const BaseArray<TypeId>& fnPolyArgs, OverloadGroup* fnOverloads);
    // used by checkFnCall and for-loop with user iterators (in checkRest)
    // there are no default values for arguments because it is important that you give each argument thought.
    ASTFunction* findPolymorphicFunction(OverloadGroup* fnOverloads, int nonNamedArgs, const BaseArray<TypeId>& argTypes, bool implicit_this, ScopeId scopeId, StructImpl* parentStructImpl, ASTStruct* parentStructAst, QuickArray<TypeId>& out_polyArgs, const BaseArray<bool>* inferred_args, ASTExpression* expr, bool operatorOverloadAttempt);
    
    void init_context(Compiler* compiler);
};

void TypeCheckEnums(AST* ast, ASTScope* scope, Compiler* compiler);
// may fail in which case we should try again
SignalIO TypeCheckStructs(AST* ast, ASTScope* scope, Compiler* compiler, bool ignore_errors, bool* changed);
void TypeCheckFunctions(AST* ast, ASTScope* scope, Compiler* compiler, bool is_initial_import);
// DO NOT TYPE CHECK IMPORT SCOPE TWICE!
void TypeCheckBody(Compiler* compiler, ASTFunction* ast_func, FuncImpl* func_impl, ASTScope* import_scope = nullptr);
// deprecated
// void TypeCheckBodies(AST* ast, ASTScope* scope, Compiler* compiler);