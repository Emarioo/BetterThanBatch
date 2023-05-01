#pragma once

#include "BetBat/Tokenizer.h"
#include "BetBat/Value.h"
#include "BetBat/ExternalCalls.h"
#include "BetBat/Bytecode.h"
#include "BetBat/AST.h"

#include <unordered_map>

// 2 bits to indicate instruction structure
// 3, 2 or 1 register. 2 registers can also be interpreted as
// 1 register + contant integer
// #define BC_MASK (0b11<<6)
// #define BC_R3 (0b00<<6)
// #define BC_R2 (0b01<<6)
// #define BC_R1 (0b10<<6)

#define PARSE_ERROR 0
#define PARSE_BAD_ATTEMPT 2
// #define PARSE_SUDDEN_END 4
#define PARSE_SUCCESS 1
// success but no accumulation
#define PARSE_NO_VALUE 3
engone::Logger& operator<<(engone::Logger& logger, Bytecode::DebugLine& debugLine);
struct ParseInfo {
    ParseInfo(Tokens& tokens) : tokens(tokens){}
    Bytecode code{};
    uint index=0;
    Tokens& tokens;
    int parDepth=0;
    
    int errors=0;
    
    int spaceConstIndex=-1;

    struct Scope{
        std::vector<Token> variableNames;
        std::vector<Token> functionsNames;
        std::vector<int> delRegisters;
        // how many instructions it takes to
        // delete variables
        int getInstsBeforeDelReg(ParseInfo& info);
        void removeReg(int reg);
    };
    std::vector<Scope> scopes;

    void makeScope();
    void dropScope(int index=-1);

    int frameOffsetIndex = 0;
    #define VAR_FUNC 1
    struct Variable{
        // int type=0;
        int frameIndex=0;
        // int constIndex=-1;
    };
    std::unordered_map<std::string,Variable> globalVariables;
    Variable* getVariable(const std::string& name);
    void removeVariable(const std::string& name);
    Variable* addVariable(const std::string& name);
    
    struct Function {
        int jumpAddress=0;
        int constIndex=-1;
        std::unordered_map<std::string,Variable> variables;
    };
    std::unordered_map<std::string,Function> functions;
    Function* getFunction(const std::string& name);
    Function* addFunction(const std::string& name);
    std::string currentFunction; // empty means global
    
    struct LoopScope{
        int iReg=0;
        int vReg=0;
        int jumpReg=0;
        int continueConstant=0;
        int breakConstant=0;
        int scopeIndex = 0; // reference to scopes
    };
    struct FuncScope{
        int scopeIndex = 0; // reference to scopes
    };
    std::vector<LoopScope> loopScopes;
    std::vector<FuncScope> funcScopes;

    // Todo: converting from Token to std::string can be slow since it may
    //   require memory to be allocated. Make a custom hash map?
    std::unordered_map<std::string,uint> nameOfNumberMap;
    std::unordered_map<std::string,uint> nameOfStringMap;

    struct IncompleteInstruction{
        int instIndex=0;
        Token token;
    };
    std::vector<IncompleteInstruction> instructionsToResolve;

    // Does not handle out of bounds
    Token &prev();
    Token& next();
    Token& now();
    bool revert();
    Token& get(uint index);
    int at();
    bool end();
    void finish();

    bool addDebugLine(uint tokenIndex);
    bool addDebugLine(const char* str, int line=65565);

    // print line of where current token exists
    // not including \n
    void printLine();

    void nextLine();
};
struct ExpressionInfo {
    int acc0Reg = 0;
    int regCount=0;
    int operations[5]{0};
    int opCount=0;  
};

Bytecode GenerateScript(Tokens& tokens, int* outErr=0);
Bytecode GenerateInstructions(Tokens& tokens, int* outErr=0);

std::string Disassemble(Bytecode& code);