#pragma once

#include "BetBat/v2/Bytecode.h"

struct Interpreter {
    
    u64 rax;
    u64 rbx;
    u64 rcx;
    u64 rdx;
    u64 rsx;
    u64 rfx;
    u64 rpc;

    void* getReg(u8 id);
    void* setReg(u8 id);

    void execute(Bytecode* bytecode);
};