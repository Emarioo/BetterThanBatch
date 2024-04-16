#pragma once

#include "BetBat/AST.h"
#include "BetBat/Bytecode.h"
#include "BetBat/PhaseContext.h"

#include "BetBat/Lang.h"
struct CompilerImport;
struct GenContext : public PhaseContext {
    TinyBytecode* tinycode = nullptr;
    Bytecode* code=nullptr;
    AST* ast=nullptr;
    Reporter* reporter=nullptr;
    CompilerImport* imp = nullptr;
    DynamicArray<TinyBytecode*>* out_codes = nullptr;
    
    GenContext() : info(*this) { } // well this is dumb
    GenContext& info;

    // struct AlignInfo {
    //     int diff=0;
    //     int size=0;
    // };
    // DynamicArray<AlignInfo> stackAlignment;
    // int virtualStackPointer = 0;
    int currentFrameOffset = 0;

    BytecodeBuilder builder{};

    // void addPop(int reg);
    // DON'T USE withoutInstruction UNLESS YOU ARE 100% CERTAIN ABOUT WHAT YOU ARE DOING.
    // It's mainly used for cast with ast_asm.
    // void addPush(int reg, bool withoutInstruction = false);
    // void addIncrSp(i16 offset);
    // void addAlign(int alignment);
    // int saveStackMoment();
    // void restoreStackMoment(int moment, bool withoutModification = false, bool withoutInstruction = false);

    void popInstructions(u32 count);

    QuickArray<ASTNode*> nodeStack; // kind of like a stack trace
    // ASTNode* prevNode=nullptr;
    // TokenStream* lastStream=nullptr;
    u32 lastLine = 0;
    u32 lastLocationIndex = (u32)-1;
    void pushNode(ASTNode* node);
    void popNode();
    // returns false if a modified or different instruction was added
    // bool addInstruction(Instruction inst, bool bypassAsserts = false);
    // void addLoadIm(u8 reg, i32 value);
    // void addLoadIm2(u8 reg, i64 value);
    // different from load immediate
    // void addImm(i32 value);
    // void addCall(LinkConventions linkConvention, CallConventions callConvention);
    void addExternalRelocation(const std::string& name, const std::string& lib_path, u32 codeAddress) {
        if(!disableCodeGeneration)
            code->addExternalRelocation(name, lib_path, tinycode->index, codeAddress);
    }
    QuickArray<u32> indexOfNonImmediates{}; // this list is probably inefficient but other solutions are tedious.

    u32 debugFunctionIndex = (u32)-1;
    ASTFunction* currentFunction=nullptr;
    FuncImpl* currentFuncImpl=nullptr;
    ScopeId currentScopeId = 0;
    ScopeId fromScopeId = 0; // AST_FROM_NAMESPACE

    u32 currentPolyVersion=0;
    int currentScopeDepth = 0; // necessary for scoped variables in debug information

    // won't work with multiple threads
    bool disableCodeGeneration = false; // used with @no-code
    bool ignoreErrors = false; // used with @no-code

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

    #define VAR_INFOS 0
    #define VAR_MEMBERS 1
    #define VAR_STRINGS 2
    #define VAR_COUNT 3
    VariableInfo* varInfos[VAR_COUNT]{nullptr};
    int dataOffset_types = -1;
    int dataOffset_members = -1;
    int dataOffset_strings = -1;

    static const int FRAME_SIZE=16; // pc, fp
    // what the relative stack pointer should be right after a funtion call.
    // frame pointer should be pushed afterwards which will result in -16 as virtualStackPointer
    static const int VIRTUAL_STACK_START = 0;

    // Extra details
    // FuncImpl* recentFuncImpl=nullptr; // used by fncall


    SignalIO framePush(TypeId typeId, i32* outFrameOffset, bool genDefault, bool staticData);
    SignalIO generatePush(BCRegister baseReg, int offset, TypeId typeId);
    SignalIO generatePop(BCRegister baseReg, int offset, TypeId typeId);
    SignalIO generateArtificialPush(TypeId typeId);
    // Generate a push from pointer (baseReg) where a list of pushed values are stored. generatePush reads memory from a struct layout while this function "copies" pushed values from a pointer.
    SignalIO generatePushFromValues(BCRegister baseReg, int baseOffset, TypeId typeId, int* movingOffset = nullptr);
    void genMemzero(BCRegister ptr_reg, BCRegister size_reg, int size);
    
    SignalIO generateDefaultValue(BCRegister baseReg, int offset, TypeId typeId, lexer::SourceLocation* location = nullptr, bool zeroInitialize=true);
    SignalIO generateReference(ASTExpression* _expression, TypeId* outTypeId, ScopeId idScope = -1, bool* wasNonReference = nullptr);
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