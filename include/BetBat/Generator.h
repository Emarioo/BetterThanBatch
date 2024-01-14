#pragma once

#include "BetBat/AST.h"
#include "BetBat/Bytecode.h"
#include "BetBat/PhaseContext.h"

#include "BetBat/Lang.h"

struct CompileInfo;
struct GenInfo;
struct GenInfo : public PhaseContext {
    Bytecode* code=nullptr;
    AST* ast=nullptr;

    struct AlignInfo {
        int diff=0;
        int size=0;
    };
    DynamicArray<AlignInfo> stackAlignment;
    int virtualStackPointer = 0;
    int currentFrameOffset = 0;

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
    VariableInfo* varInfos[VAR_COUNT];
    int dataOffset_types = -1;
    int dataOffset_members = -1;
    int dataOffset_strings = -1;

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