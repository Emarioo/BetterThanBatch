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
    
    struct AlignInfo {
        int diff=0;
        int size=0;
    };
    std::vector<AlignInfo> stackAlignment;
    int virtualStackPointer=0;
    void addPop(int reg);
    void addPush(int reg);
    void addIncrSp(i16 offset);
    void addAlign(int alignment);
    // Negative value to make some space for values
    // Positive to remove values
    // like push and pop but with a size
    void addStackSpace(i32 size);
    int saveStackMoment();
    void restoreStackMoment(int moment);

    DynamicArray<ASTNode*> nodeStack; // kind of like a stack trace
    // ASTNode* prevNode=nullptr;
    TokenStream* lastStream=nullptr;
    u32 lastLine = 0;
    u32 lastLocationIndex = (u32)-1;
    void pushNode(ASTNode* node);
    void popNode();
    void addInstruction(Instruction inst);
    void addLoadIm(u8 reg, i32 value);

    ASTFunction* currentFunction=nullptr;
    FuncImpl* currentFuncImpl=nullptr;
    ScopeId currentScopeId = 0;
    ScopeId fromScopeId = 0; // AST_FROM_NAMESPACE

    u32 currentPolyVersion=0;

    int funcDepth=0;
    struct LoopScope {
        // Index of instruction where looping starts.h
        int continueAddress = 0;
        int stackMoment=0;
        std::vector<int> resolveBreaks;
        Identifier counter = {}; // may not be used right now but the size of
        // loop scope collides with std::string when tracking allocations.
        int _ = 0; // offset memory for tracking again
    };
    std::vector<LoopScope*> loopScopes;
    
    LoopScope* pushLoop();
    LoopScope* getLoop(int index);
    bool popLoop();

    struct ResolveCall {
        int bcIndex = 0;
        FuncImpl* funcImpl = nullptr;
    };
    std::vector<ResolveCall> callsToResolve;

    // TODO: avoid vector and unordered_map. resizing vector may do unwanted things with the map (like whole a copy).
    // std::vector<FunctionScope*> functionScopes;
    // int currentFunctionScope=0;
    // FunctionScope* getFunctionScope(int index=-1);
    int currentFrameOffset=0;
    static const int FRAME_SIZE=16; // pc, fp

    // Extra details
    // FuncImpl* recentFuncImpl=nullptr; // used by fncall

};
struct NodeScope {
    NodeScope(GenInfo* info) : info(info) {}
    ~NodeScope() {
        info->popNode();
    }
    GenInfo* info = nullptr;
};
Bytecode* Generate(AST* ast, CompileInfo* compileInfo);