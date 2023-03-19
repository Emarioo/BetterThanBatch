#pragma once

#include "BetBat/Generator.h"

const char* RefToString(int type);
#define REF_NUMBER 1
#define REF_STRING 2
struct Ref {
    int type;
    int index;
};
struct Context {
    engone::Memory numbers{sizeof(Number)};

    Ref references[256];    

    Bytecode activeCode;

    int makeNumber();
    Number* getNumber(int index);

    void load(Bytecode code);
    void execute();
};