#pragma once
#include "BetBat/Tokenizer.h"
#include "BetBat/Value.h"
#include "BetBat/ExternalCalls.h"

// 2 bits to indicate instruction structure
// 3, 2 or 1 register. 2 registers can also be interpreted as
// 1 register + contant integer
// #define BC_MASK (0b11<<6)
// #define BC_R3 (0b00<<6)
// #define BC_R2 (0b01<<6)
// #define BC_R1 (0b10<<6)

#define BC_MASK          (0xC0)
#define BC_R1            (0x40)
#define BC_R2            (0x80)
#define BC_R3            (0xC0)

#define BC_ADD           (BC_R3|0x01)
#define BC_SUB           (BC_R3|0x02)
#define BC_MUL           (BC_R3|0x03)
#define BC_DIV           (BC_R3|0x04)
#define BC_MOD           (BC_R3|0x05)
#define BC_GREATER       (BC_R3|0x06)
#define BC_EQUAL         (BC_R3|0x07)
#define BC_NOT_EQUAL     (BC_R3|0x08)
#define BC_AND           (BC_R3|0x09)
#define BC_OR            (BC_R3|0x10)
#define BC_LESS          (BC_R3|0x11)
#define BC_SUBSTR        (BC_R3|0x12)
#define BC_LESS_EQ       (BC_R3|0x13)
#define BC_GREATER_EQ    (BC_R3|0x14)
#define BC_THREAD        (BC_R3|0x15)
#define BC_SETCHAR       (BC_R3|0x16)

#define BC_COPY          (BC_R2|0x01)
#define BC_MOV           (BC_R2|0x02)
#define BC_RUN           (BC_R2|0x03)
#define BC_JUMPNIF       (BC_R2|0x04)
#define BC_TYPE          (BC_R2|0x05)
#define BC_LEN           (BC_R2|0x06)
#define BC_NOT           (BC_R2|0x07)

#define BC_LOADV         (BC_R2|0x10)
#define BC_STOREV        (BC_R2|0x11)

#define BC_WRITE_FILE    (BC_R2|0x12)
#define BC_APPEND_FILE   (BC_R2|0x13)
#define BC_READ_FILE     (BC_R2|0x14)
#define BC_JOIN          (BC_R2|0x15)

// #define BC_TONUM         (BC_R2|0x12)

#define BC_NUM           (BC_R1|0x01)
#define BC_STR           (BC_R1|0x02)
#define BC_DEL           (BC_R1|0x03)
// #define BC_DELNV         (BC_R1|0x04)

#define BC_LOADSC        (BC_R1|0x10)
#define BC_LOADNC        (BC_R1|0x11)

#define BC_TEST          (BC_R1|0x13)

#define BC_RETURN        (BC_R1|0x15)
// #define BC_ENTERSCOPE    (BC_R1|0x16)
// #define BC_EXITSCOPE     (BC_R1|0x17)

#define BC_JUMP          (BC_R1|0x30)

#define REG_NULL 0
#define REG_NULL_S "null"
#define REG_ZERO 1
#define REG_ZERO_S "zero"
#define REG_RETURN_VALUE 3
#define REG_RETURN_VALUE_S "rv"
#define REG_ARGUMENT 4
#define REG_ARGUMENT_S "rz"
#define REG_FRAME_POINTER 5
#define REG_FRAME_POINTER_S "fp"
#define REG_TEMP 9
#define REG_TEMP_S "temp"
// #define REG_STACK_POINTER 6
// #define REG_STACK_POINTER_S "sp"

#define REG_ACC0 10

const char* InstToString(int type);

struct Instruction {
    uint8 type=0;
    union {
        uint8 regs[3]{0};
        struct{
            uint8 reg0;
            uint8 reg1;
            uint8 reg2;
        };
    };

    uint32 singleReg(){return (uint32)reg0|((uint32)reg1<<8)|((uint32)reg2<<16);}
    void print(); 
};
engone::Logger& operator<<(engone::Logger& logger, Instruction& instruction);

class Context;
struct Bytecode {
    engone::Memory codeSegment{sizeof(Instruction)};
    engone::Memory constNumbers{sizeof(Number)};
    engone::Memory constStrings{sizeof(String)};
    engone::Memory constStringText{1};
    
    struct DebugLine{
        char* str=0;
        int instructionIndex=0;
        uint16 length=0;
        uint16 line=0;
    };
    engone::Memory debugLines{sizeof(DebugLine)};
    engone::Memory debugLineText{1};

    // Const strings and debug lines use char pointers to other memory.
    // that memory may be reallocated and thus invalidate the char pointers.
    // This is prevented by treating char* as an offest in the memory first.
    // then call this function to replace the char* with base + offset
    void finalizePointers(); 

    // engone::Memory linePointers{sizeof(uint16)};

    // Todo: A map<instructionIndex,DebugLine> instead of an array.
    //  There may be some benefit if you have many jump instructions.
    
    void cleanup();

    uint32 getMemoryUsage();

    // latestIndex is used to skip already read debug lines. 
    DebugLine* getDebugLine(int instructionIndex, int* latestIndex);
    DebugLine* getDebugLine(int instructionIndex);

    bool add(Instruction inst);
    bool add(uint8 type, uint8 reg0, uint8 reg1, uint8 reg2);
    bool add(uint8 type, uint8 reg0, uint16 reg12);
    bool add(uint8 type, uint reg012);
    bool addLoadNC(uint8 reg0, uint constIndex);
    bool addLoadSC(uint8 reg0, uint constIndex);
    bool addLoadV(uint8 reg0, uint stackIndex, bool global);
    Instruction& get(uint index);
    int length();
    bool removeLast();
    bool remove(int index);
    
    uint addConstNumber(Decimal number);
    Number* getConstNumber(uint index);
    
    uint addConstString(Token& token, const char* padding=0);
    String* getConstString(uint index);

    void printStats();
};