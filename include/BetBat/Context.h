#pragma once

#include "BetBat/Generator.h"

const char* RefToString(int type);
#define REF_NUMBER 1
#define REF_STRING 2

// Define/undefine to enable/disable
#define CLOG

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
    engone::Memory infoValues{sizeof(uint8)};

    engone::Memory scopes{sizeof(Scope)};
    uint currentScope=0;

    engone::Memory valueStack{sizeof(Ref)}; // holds references to values

    std::unordered_map<std::string, APICall> apiCalls;

    Bytecode activeCode;
    
    void cleanup();

    Scope* getScope(uint index);
    bool ensureScopes(uint depth);

    uint32 makeNumber();
    void deleteNumber(uint index);
    Number* getNumber(uint index);
    
    uint32 makeString();
    void deleteString(uint index);
    String* getString(uint index);

    void load(Bytecode code);
    void execute();
    
    static void Execute(Bytecode code);
};