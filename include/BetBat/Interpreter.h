#pragma once

#include "BetBat/Bytecode.h"
#include "BetBat/NativeRegistry.h"
#include "BetBat/External/NativeLayer.h"

struct Interpreter {
    ~Interpreter(){
        cleanup();
    }
    engone::Memory<char> cmdArgsBuffer{};
    Language::Slice<Language::Slice<char>> cmdArgs{};
    void setCmdArgs(const std::vector<std::string>& inCmdArgs);
    
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
    u64 rsi=0;
    u64 rdi=0;
    
    bool expectValuesOnStack = false;

    void moveMemory(u8 reg, void* from, void* to);

    void* getReg(u8 id);
    void* setReg(u8 id);
    
    engone::Memory<u8> stack{};
    bool silent = false;

    void execute(Bytecode* bytecode);
    // lastInstruction is exclusive
    // execution will stop when program counters is equal to lastInstruction
    void executePart(Bytecode* bytecode, u32 startInstruction, u32 lastInstruction);

    static const int CWD_LIMIT = 256;
    char cwdBuffer[CWD_LIMIT]{0};
    u32 usedCwd=0;

    
    void cleanup();
    void printRegisters();
};