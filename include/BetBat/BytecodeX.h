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
// jump if not zero
#define BC_JE 23
// jump if zero
#define BC_JNE 24

#define BC_PUSH 30
#define BC_POP 31
#define BC_LI 32

#define BC_EQ 50
#define BC_NEQ  51
#define BC_LT   52
#define BC_LTE  53
#define BC_GT   54
#define BC_GTE  55
#define BC_ANDI  56
#define BC_ORI   57
#define BC_NOTB  58

#define BC_BXOR  60
#define BC_BOR  61
#define BC_BAND  62

#define BC_CASTF32 90
#define BC_CASTI64 91
/*
f32
f64

i32
u32
u64
i64
u16
u8
*/
// NOTE: Some instructions have odd names to avoid collision with Bytecode.h.

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
#define BC_REG_PC (ENCODE_REG_TYPE(BC_REG_64)|6)

const char* RegToStr(u8 reg);

#define BC_EXT_ALLOC -1
#define BC_EXT_REALLOC -2
#define BC_EXT_FREE -3
#define BC_EXT_PRINTI -4

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

    void print(); 
};
engone::Logger& operator<<(engone::Logger& logger, InstructionX& instruction);

struct BytecodeX {
    static BytecodeX* Create();
    static void Destroy(BytecodeX*);
    void cleanup();
    
    uint32 getMemoryUsage();
    
    engone::Memory codeSegment{sizeof(InstructionX)};
    
    engone::Memory debugSegment{sizeof(u32)};
    std::vector<std::string> debugText;
    // std::vector<std::string> debugText;
    // -1 as index will add text to next instruction
    void addDebugText(const char* str, int length, u32 instructionIndex=-1);
    void addDebugText(const std::string& text, u32 instructionIndex=-1);
    void addDebugText(Token& token, u32 instructionIndex=-1);
    const char* getDebugText(u32 instructionIndex);

    bool add(InstructionX inst);
    bool addIm(i32 data);
    inline InstructionX* get(uint index){
        return ((InstructionX*)codeSegment.data + index);
    }
    inline int length(){
        return codeSegment.used;
    }
    bool removeLast();

    void printStats();
};