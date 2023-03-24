#pragma once

#include "BetBat/Generator.h"

const char* RefToString(int type);
#define REF_NUMBER 1
#define REF_STRING 2

#define DEFAULT_LOAD_CONST_REG 9
struct Ref {
    int type=0;
    int index=0;
};
// engone::Logger operator<<(engone::Logger logger, Ref& ref);
struct Scope{
    Ref references[256]{0};  
};
struct Context {
    engone::Memory numbers{sizeof(Number)};
    engone::Memory strings{sizeof(String)};

    engone::Memory scopes{sizeof(Scope)};
    uint32 currentScope=0;

    std::unordered_map<std::string, APICall> apiCalls;

    Bytecode activeCode;
    
    Scope* getScope(int index);
    bool ensureScopes(int depth);

    uint32 makeNumber();
    void deleteNumber(uint32 index);
    Number* getNumber(uint32 index);
    
    uint32 makeString();
    void deleteString(uint32 index);
    String* getString(uint32 index);

    void load(Bytecode code);
    void execute();
    
    static void Execute(Bytecode code);
};