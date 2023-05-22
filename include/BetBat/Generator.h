#pragma once

#include "BetBat/AST.h"
#include "BetBat/BytecodeX.h"
#include "BetBat/Config.h"

struct GenInfo {
    BytecodeX* code=0;
    AST* ast=0;
    int errors=0;
    
    struct Variable {
        i32 frameOffset=0;
        TypeId typeId=AST_VOID;
    };
    struct Function {
        ASTFunction* astFunc=0;
        u64 address=0;
    };
    struct Identifier{
        Identifier() {}
        enum Type {
            VAR, FUNC
        };
        Type type=VAR;
        std::string name{};
        int index=0;
    };
    std::vector<Variable> variables;
    std::vector<Function> functions;
    std::unordered_map<std::string,Identifier> identifiers;
    
    Variable* addVariable(const std::string& name);
    Function* addFunction(const std::string& name);
    
    Identifier* addIdentifier(const std::string& name);
    Identifier* getIdentifier(const std::string& name);
    Variable* getVariable(int index);
    Function* getFunction(int index);
    
    void removeIdentifier(const std::string& name);

    struct AlignInfo {
        int diff=0;
        int size=0;
    };
    std::vector<AlignInfo> stackAlignment;
    int relativeStackPointer=0;
    void addPop(int reg);
    void addPush(int reg);
    void addIncrSp(i16 offset);
    int saveStackMoment();
    void restoreStackMoment(int moment);

    std::vector<int> constStringMapping;

    ASTFunction* currentFunction=0;
    
    int funcDepth=0;
    
    // TODO: avoid vector and unordered_map. resizing vector may do unwanted things with the map (like whole a copy).
    // std::vector<FunctionScope*> functionScopes;
    // int currentFunctionScope=0;
    // FunctionScope* getFunctionScope(int index=-1);
    int currentFrameOffset=0;
    static const int ARG_OFFSET=16;
    // static const 

    
};

bool EvaluateTypes(AST* ast, ASTBody* body, int* err);

BytecodeX* Generate(AST* ast, int* err);