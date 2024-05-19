#pragma once

#include "BetBat/AST.h"
#include "BetBat/Bytecode.h"
#include "BetBat/PhaseContext.h"
#include "BetBat/DebugInformation.h"
#include "BetBat/Util/Profiler.h"

struct CompilerImport;
struct GenContext : public PhaseContext {
    TinyBytecode* tinycode = nullptr;
    Bytecode* bytecode=nullptr;
    AST* ast=nullptr;
    Reporter* reporter=nullptr;
    CompilerImport* imp = nullptr;
    DynamicArray<TinyBytecode*>* out_codes = nullptr;
    
    GenContext() : info(*this) { } // well this is dumb
    GenContext& info;

    int currentFrameOffset = 0;

    BytecodeBuilder builder{};

    void popInstructions(u32 count);

    QuickArray<ASTNode*> nodeStack; // kind of like a stack trace
    
    u32 lastLine = 0;
    u32 lastLocationIndex = (u32)-1;
    void pushNode(ASTNode* node);
    void popNode();

    void addExternalRelocation(const std::string& name, const std::string& lib_path, u32 codeAddress) {
        if(!disableCodeGeneration)
            bytecode->addExternalRelocation(name, lib_path, tinycode->index, codeAddress);
    }
    QuickArray<u32> indexOfNonImmediates{}; // this list is probably inefficient but other solutions are tedious.

    DebugFunction* debugFunction = nullptr;
    ASTFunction* currentFunction=nullptr;
    FuncImpl* currentFuncImpl=nullptr;
    ScopeId currentScopeId = 0;
    ScopeId fromScopeId = 0; // AST_FROM_NAMESPACE

    u32 currentPolyVersion=0;
    int currentScopeDepth = 0; // necessary for scoped variables in debug information

    // won't work with multiple threads
    // bool disableCodeGeneration = false; // used with @no-code
    bool ignoreErrors = false; // used with @no-code
    bool showErrors = true;

    int funcDepth=0;
    struct LoopScope {
        // Index of instruction where looping starts.h
        int continueAddress = 0;
        int stackMoment=0;
        QuickArray<int> resolveBreaks;
        Identifier counter = {}; // may not be used right now but the size of
        // loop scope collides with std::string when tracking allocations.
    };
    QuickArray<LoopScope*> loopScopes;
    
    LoopScope* pushLoop();
    LoopScope* getLoop(int index);
    bool popLoop();

    struct ResolveCall {
        int bcIndex = 0;
        FuncImpl* funcImpl = nullptr;
    };
    DynamicArray<ResolveCall> callsToResolve;
    void addCallToResolve(int bcIndex, FuncImpl* funcImpl);

    SignalIO framePush(TypeId typeId, i32* outFrameOffset, bool genDefault, bool staticData);
    SignalIO generatePush(BCRegister baseReg, int offset, TypeId typeId);
    SignalIO generatePop(BCRegister baseReg, int offset, TypeId typeId);

    SignalIO generatePush_get_param (int offset, TypeId typeId);
    SignalIO generatePop_set_arg    (int offset, TypeId typeId);
    SignalIO generatePush_get_val   (int offset, TypeId typeId);
    SignalIO generatePop_set_ret    (int offset, TypeId typeId);

    SignalIO generateArtificialPush(TypeId typeId);
    // Generate a push from pointer (baseReg) where a list of pushed values are stored. generatePush reads memory from a struct layout while this function "copies" pushed values from a pointer.
    SignalIO generatePushFromValues(BCRegister baseReg, int baseOffset, TypeId typeId, int* movingOffset = nullptr);
    void genMemzero(BCRegister ptr_reg, BCRegister size_reg, int size);
    void genMemcpy(BCRegister dst_reg, BCRegister src_reg, int size);
    
    SignalIO generateDefaultValue(BCRegister baseReg, int offset, TypeId typeId, lexer::SourceLocation* location = nullptr, bool zeroInitialize=true);
    SignalIO generateReference(ASTExpression* _expression, TypeId* outTypeId, ScopeId idScope = -1, bool* wasNonReference = nullptr, int* array_length = nullptr);
    SignalIO generateFnCall(ASTExpression* expression, DynamicArray<TypeId>* outTypeIds, bool isOperator);
    SignalIO generateExpression(ASTExpression *expression, TypeId *outTypeIds, ScopeId idScope = -1);
    SignalIO generateExpression(ASTExpression *expression, DynamicArray<TypeId> *outTypeIds, ScopeId idScope = -1);
    SignalIO generateFunction(ASTFunction* function, ASTStruct* astStruct = nullptr);
    SignalIO generateFunctions(ASTScope* body);
    SignalIO generateBody(ASTScope *body);
    
    SignalIO generatePreload();
    SignalIO generateData();
    
    bool performSafeCast(TypeId from, TypeId to);
};
struct NodeScope {
    NodeScope(GenContext* info) : info(info) {}
    ~NodeScope() {
        info->popNode();
    }
    GenContext* info = nullptr;
};
// Bytecode* Generate(AST* ast, CompileInfo* compileInfo);
bool GenerateScope(ASTScope* scope, Compiler* compiler, CompilerImport* imp, DynamicArray<TinyBytecode*>* out_codes, bool is_initial_import);