#pragma once
#include "BetBat/Tokenizer.h"
#include "BetBat/Value.h"
#include "BetBat/ExternalCalls.h"

#define BC_MOV_RR 1
#define BC_MOV_RM 2
#define BC_MOV_MR 3

#define BC_ADDI 10
#define BC_ADDF 11
#define BC_SUBI 12
#define BC_SUBF 13
#define BC_MULI 14
#define BC_MULF 15
#define BC_DIVI 16
#define BC_DIVF 17

#define BC_JMP 20
#define BC_CALL 21
#define BC_RET 22

#define BC_PUSH 30
#define BC_POP 31
#define BC_LI 32

#define BC_REG_MASK 0xC0
#define BC_REG_8 0
#define BC_REG_16 1
#define BC_REG_32 2
#define BC_REG_64 3

#define DECODE_REG_TYPE(X) ((X&BC_REG_MASK)>>6)
#define ENCODE_REG_TYPE(X) (X<<6)

#define BC_REG_AL (ENCODE_REG_TYPE(BC_REG_8)|0)
#define BC_REG_AH (ENCODE_REG_TYPE(BC_REG_8)|1)
#define BC_REG_BL (ENCODE_REG_TYPE(BC_REG_8)|2)
#define BC_REG_BH (ENCODE_REG_TYPE(BC_REG_8)|3)
#define BC_REG_CL (ENCODE_REG_TYPE(BC_REG_8)|4)
#define BC_REG_CH (ENCODE_REG_TYPE(BC_REG_8)|5)
#define BC_REG_DL (ENCODE_REG_TYPE(BC_REG_8)|6)
#define BC_REG_DH (ENCODE_REG_TYPE(BC_REG_8)|7)

#define BC_REG_AX (ENCODE_REG_TYPE(BC_REG_16)|0)
#define BC_REG_BX (ENCODE_REG_TYPE(BC_REG_16)|1)
#define BC_REG_CX (ENCODE_REG_TYPE(BC_REG_16)|2)
#define BC_REG_DX (ENCODE_REG_TYPE(BC_REG_16)|3)

#define BC_REG_EAX (ENCODE_REG_TYPE(BC_REG_32)|0)
#define BC_REG_EBX (ENCODE_REG_TYPE(BC_REG_32)|1)
#define BC_REG_ECX (ENCODE_REG_TYPE(BC_REG_32)|2)
#define BC_REG_EDX (ENCODE_REG_TYPE(BC_REG_32)|3)

#define BC_REG_RAX (ENCODE_REG_TYPE(BC_REG_64)|0)
#define BC_REG_RBX (ENCODE_REG_TYPE(BC_REG_64)|1)
#define BC_REG_RCX (ENCODE_REG_TYPE(BC_REG_64)|2)
#define BC_REG_RDX (ENCODE_REG_TYPE(BC_REG_64)|3)

#define BC_REG_SP (ENCODE_REG_TYPE(BC_REG_64)|4)
#define BC_REG_FP (ENCODE_REG_TYPE(BC_REG_64)|5)
// #define BC_REG_SS (ENCODE_REG_TYPE(BC_REG_64)|5)

const char* RegToStr(u8 reg);

struct InstructionX {
    uint8 opcode=0;
    union {
        uint8 operands[3]{0};
        struct{
            uint8 op0;
            uint8 op1;
            uint8 op2;
        };
    };

    // uint32 singleReg(){return (uint32)reg0|((uint32)reg1<<8)|((uint32)reg2<<16);}
    void print(); 
};
engone::Logger& operator<<(engone::Logger& logger, InstructionX& instruction);

// class Interpreter;
struct BytecodeX {
    static BytecodeX* Create();
    static void Destroy(BytecodeX*);
    void cleanup();
    
    uint32 getMemoryUsage();
    
    engone::Memory codeSegment{sizeof(InstructionX)};
    // engone::Memory constNumbers{sizeof(Number)};
    // engone::Memory constStrings{sizeof(String)};
    // engone::Memory constStringText{1};
    
    engone::Memory debugSegment{sizeof(u32)};
    std::vector<std::string> debugText;
    // -1 as index will add text to last instruction
    void addDebugText(const std::string& text, u32 instructionIndex=-1);
    const std::string& getDebugText(u32 instructionIndex);
    
    // struct DebugLine{
    //     char* str=0;
    //     int instructionIndex=0;
    //     uint16 length=0;
    //     uint16 line=0;
    // };
    // engone::Memory debugLines{sizeof(DebugLine)};
    // engone::Memory debugLineText{1};

    // Const strings and debug lines use char pointers to other memory.
    // that memory may be reallocated and thus invalidate the char pointers.
    // This is prevented by treating char* as an offest in the memory first.
    // then call this function to replace the char* with base + offset
    // void finalizePointers(); 

    // engone::Memory linePointers{sizeof(uint16)};

    // Todo: A map<instructionIndex,DebugLine> instead of an array.
    //  There may be some benefit if you have many jump instructions.
    

    // latestIndex is used to skip already read debug lines. 
    // DebugLine* getDebugLine(int instructionIndex, int* latestIndex);
    // DebugLine* getDebugLine(int instructionIndex);

    bool add(InstructionX inst);
    bool add(u32 data);
    // bool add(uint8 type, uint8 reg0, uint8 reg1, uint8 reg2);
    // bool add(uint8 type, uint8 reg0, uint16 reg12);
    // bool add(uint8 type, uint reg012);
    // bool addLoadNC(uint8 reg0, uint constIndex);
    // bool addLoadSC(uint8 reg0, uint constIndex);
    // bool addLoadV(uint8 reg0, uint stackIndex, bool global);
    InstructionX* get(uint index);
    int length();
    bool removeLast();
    // bool remove(int index);
    
    // uint addConstNumber(Decimal number);
    // Number* getConstNumber(uint index);
    
    // uint addConstString(Token& token, const char* padding=0);
    // String* getConstString(uint index);

    void printStats();
};