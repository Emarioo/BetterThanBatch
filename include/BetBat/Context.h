#pragma once

#include "BetBat/Values.h"

#include "BetBat/Generator.h"

#define REF_STRING 1
#define REF_NUMBER 2
struct Ref {
    int type=0;
    int index=0;
};
const char* RefToString(int type);
struct Context {
    engone::Memory numbers{sizeof(Number)};
    engone::Memory strings{sizeof(String)};
    
    Ref references[256]{0};
    
    Bytecode activeCode{};
    
    Number* getNumber(int index);
    String* getString(int index);
    int makeNumber();
    int makeString();
    
    void load(Bytecode bytecode);
    void execute();
};