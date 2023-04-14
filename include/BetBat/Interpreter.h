#pragma once

#include "BetBat/Parser.h"

const char* RefToString(int type);
// engone::Logger operator<<(engone::Logger logger, Ref& ref);
// A function scope. StackFrame? not if, for or while scope
struct Scope{
    Ref references[256]{0};
    
    int returnAddress=0;
};
struct Context {
    engone::Memory numbers{sizeof(Number)};
    engone::Memory strings{sizeof(String)};
    engone::Memory infoValues{sizeof(uint8)};

    int numberCount=0;
    int stringCount=0;

    engone::Memory scopes{sizeof(Scope)};
    uint currentScope=0;

    engone::Memory valueStack{sizeof(Ref)}; // holds references to values

    std::unordered_map<std::string, ExternalCall> externalCalls;

    struct TestValue {
        int type=0;
        union {
            String string;
            Number number;
        };
    };
    std::vector<TestValue> testValues;

    Bytecode activeCode;
    
    void cleanup();

    Scope* getScope(uint index);
    bool ensureScopes(uint depth);

    void enterScope();
    void exitScope();

    uint32 makeNumber();
    void deleteNumber(uint index);
    Number* getNumber(uint index);
    
    uint32 makeString();
    void deleteString(uint index);
    String* getString(uint index);

    void execute(Bytecode& code);
    
    static void Execute(Bytecode& code);
};