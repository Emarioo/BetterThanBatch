#pragma once

#include "BetBat/Bytecode.h"
#include "BetBat/NativeRegistry.h"
#include "Native/NativeLayer.h"

/*
    VirtualMachine may not be the accurate term for executing bytecode.
    VirtualMachine is more executing high level code, statement by statement.
    A virtual machine or bytecode runner would be more accurate.
*/
struct VirtualMachine {
    ~VirtualMachine(){
        cleanup();
    }
    
    volatile i64 registers[BC_REG_MAX];
    engone::Memory<u8> stack{};

    i64 stack_pointer = 0;
    i64 args_pointer = 0;
    

    bool expectValuesOnStack = false;
    bool silent = false;
    
    void execute(Bytecode* bytecode, const std::string& tinycode_name);

    // resets registers and other things but keeps the alloctions.
    void reset();
    void cleanup();
    void printRegisters();


    void moveMemory(u8 reg, volatile void* from, volatile void* to);
    volatile void* getReg(u8 id);
    void* setReg(u8 id);

    engone::Memory<char> cmdArgsBuffer{};
    Language::Slice<Language::Slice<char>> cmdArgs{};
    void setCmdArgs(const DynamicArray<std::string>& inCmdArgs);

    static const int CWD_LIMIT = 256;
    char cwdBuffer[CWD_LIMIT]{0};
    u32 usedCwd=0;
};