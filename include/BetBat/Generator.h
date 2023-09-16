#pragma once

#include "BetBat/AST.h"
#include "BetBat/Bytecode.h"
struct CompileInfo;
struct GenInfo;
struct GenInfo {
    Bytecode* code=nullptr;
    AST* ast=nullptr;
    CompileInfo* compileInfo=nullptr;
    int errors=0;
    int warnings=0;

    bool hasErrors();
    bool hasForeignErrors();
    
    struct AlignInfo {
        int diff=0;
        int size=0;
    };
    DynamicArray<AlignInfo> stackAlignment;
    int virtualStackPointer=0;
    void addPop(int reg);
    // DON'T USE withoutInstruction UNLESS YOU ARE 100% CERTAIN ABOUT WHAT YOU ARE DOING.
    // It's mainly used for cast with ast_asm.
    void addPush(int reg, bool withoutInstruction = false);
    void addIncrSp(i16 offset);
    void addAlign(int alignment);
    // Negative value to make some space for values
    // Positive to remove values
    // like push and pop but with a size
    void addStackSpace(i32 size);
    int saveStackMoment();
    void restoreStackMoment(int moment, bool withoutModification = false, bool withoutInstruction = false);

    void popInstructions(u32 count);

    QuickArray<ASTNode*> nodeStack; // kind of like a stack trace
    // ASTNode* prevNode=nullptr;
    TokenStream* lastStream=nullptr;
    u32 lastLine = 0;
    u32 lastLocationIndex = (u32)-1;
    void pushNode(ASTNode* node);
    void popNode();
    // returns false if a modified or different instruction was added
    bool addInstruction(Instruction inst, bool bypassAsserts = false);
    void addLoadIm(u8 reg, i32 value);
    void addLoadIm2(u8 reg, i64 value);
    // different from load immediate
    void addImm(i32 value);
    void addCall(LinkConventions linkConvention, CallConventions callConvention);
    void addExternalRelocation(const std::string name, u32 codeAddress) { if(!disableCodeGeneration) code->addExternalRelocation(name, codeAddress); }
    QuickArray<u32> indexOfNonImmediates{}; // this list is probably inefficient but other solutions are tedious.

    u32 debugFunctionIndex = (u32)-1;
    ASTFunction* currentFunction=nullptr;
    FuncImpl* currentFuncImpl=nullptr;
    ScopeId currentScopeId = 0;
    ScopeId fromScopeId = 0; // AST_FROM_NAMESPACE

    u32 currentPolyVersion=0;

    // won't work with multiple threads
    bool disableCodeGeneration = false; // used with @no-code
    bool ignoreErrors = false; // used with @no-code

    // void addError(const TokenRange& range) {
    //     if(ignoreErrors)
    //         errors++;
    //     compileInfo->compileOptions->compileStats.addError(range, errType);
    // }
    // void addError(const Token& token, CompileError errType) {
    //     errors++;
    //     compileInfo->compileOptions->compileStats.addError(token, errType);
    // }

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

    // TODO: avoid vector and unordered_map. resizing vector may do unwanted things with the map (like whole a copy).
    // DynamicArray<FunctionScope*> functionScopes;
    // int currentFunctionScope=0;
    // FunctionScope* getFunctionScope(int index=-1);
    int currentFrameOffset = 0;
    // int functionStackMoment = 0;    

    static const int FRAME_SIZE=16; // pc, fp
    // what the relative stack pointer should be right after a funtion call.
    // frame pointer should be pushed afterwards which will result in -16 as virtualStackPointer
    static const int VIRTUAL_STACK_START=-8; // pc

    // Extra details
    // FuncImpl* recentFuncImpl=nullptr; // used by fncall

    /*
        New bytecode instructions
    */
//    u8 requestRegister(); // 0 is failure
//    void relinguishRegister(u8 registerNumber);
//    void add_push(u8 registerNumber);
//    void add_pop(u8 registerNumber);

};
struct NodeScope {
    NodeScope(GenInfo* info) : info(info) {}
    ~NodeScope() {
        info->popNode();
    }
    GenInfo* info = nullptr;
};
Bytecode* Generate(AST* ast, CompileInfo* compileInfo);