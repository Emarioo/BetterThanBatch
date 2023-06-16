#pragma once

#include "BetBat/Bytecode.h"

struct Interpreter {
    
    u64 rax;
    u64 rbx;
    u64 rcx;
    u64 rdx;
    // #define UNION_REG(N) union{u64 r##N##x; u32 e##N##x; u16 N##x; struct{u8 N##l; u8 N##h;};};
    
    // UNION_REG(a)
    // UNION_REG(b)
    // UNION_REG(c)
    // UNION_REG(d)
    
    u64 sp=0;
    u64 fp=0;
    u64 pc=0;
    u64 dp=0;
    
    void* getReg(u8 id);
    void* setReg(u8 id);
    
    engone::Memory stack{1};
    bool silent = false;

    void execute(Bytecode* bytecode);
    
    void cleanup();
    void printRegisters();
};