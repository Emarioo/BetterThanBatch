#pragma once

#include "BetBat/Bytecode.h"
#include "BetBat/NativeRegistry.h"
#include "Native/NativeLayer.h"

struct Interpreter {
    ~Interpreter(){
        cleanup();
    }
    engone::Memory<char> cmdArgsBuffer{};
    Language::Slice<Language::Slice<char>> cmdArgs{};
    void setCmdArgs(const DynamicArray<std::string>& inCmdArgs);
    
    volatile u64 rax=0;
    volatile u64 rbx=0;
    volatile u64 rcx=0;
    volatile u64 rdx=0;

    volatile u64 xmm0d=0;
    volatile u64 xmm1d=0;
    volatile u64 xmm2d=0;
    volatile u64 xmm3d=0;
    
    u64 sp=0;
    u64 fp=0;
    u64 pc=0;
    u64 rsi=0;
    u64 rdi=0;
    
    bool expectValuesOnStack = false;

    void moveMemory(u8 reg, volatile void* from, volatile void* to);

    volatile void* getReg(u8 id);
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

    // resets registers and other things but keeps the alloctions.
    void reset();
    void cleanup();
    void printRegisters();
};