#pragma once
#include "BetBat/Tokenizer.h"

#define BC_MOV_RR 1
#define BC_MOV_RM 2
#define BC_MOV_MR 3

#define BC_MODI 8
#define BC_MODF 9
#define BC_ADDI 10
#define BC_ADDF 11
#define BC_SUBI 12
#define BC_SUBF 13
#define BC_MULI 14
#define BC_MULF 15
#define BC_DIVI 16
#define BC_DIVF 17

#define BC_INCR 18


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
#define BC_NOT  58

#define BC_BXOR  60
#define BC_BOR  61
#define BC_BAND  62
#define BC_BNOT 63
#define BC_BLSHIFT 64
#define BC_BRSHIFT 65

#define BC_CAST 90
// flags for first operand
#define CAST_FLOAT_SINT 0
#define CAST_SINT_FLOAT 1
#define CAST_SINT_UINT 2
#define CAST_UINT_SINT 3
#define CAST_SINT_SINT 4

#define BC_ZERO_MEM 100
#define BC_MEMCPY 101
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
#define DECODE_REG_SIZE(X) (X==0?0:(1<<DECODE_REG_TYPE(reg)));

// BC_REG_ALL can't be 0 because it's seen as no register so we do 8.
#define BC_REG_AL (ENCODE_REG_TYPE(BC_REG_8)|1)
#define BC_REG_BL (ENCODE_REG_TYPE(BC_REG_8)|2)
#define BC_REG_CL (ENCODE_REG_TYPE(BC_REG_8)|3)
#define BC_REG_DL (ENCODE_REG_TYPE(BC_REG_8)|4)

#define BC_REG_AH (ENCODE_REG_TYPE(BC_REG_8)|5)
#define BC_REG_BH (ENCODE_REG_TYPE(BC_REG_8)|6)
#define BC_REG_CH (ENCODE_REG_TYPE(BC_REG_8)|7)
#define BC_REG_DH (ENCODE_REG_TYPE(BC_REG_8)|8)

#define BC_REG_AX (ENCODE_REG_TYPE(BC_REG_16)|1)
#define BC_REG_BX (ENCODE_REG_TYPE(BC_REG_16)|2)
#define BC_REG_CX (ENCODE_REG_TYPE(BC_REG_16)|3)
#define BC_REG_DX (ENCODE_REG_TYPE(BC_REG_16)|4)

#define BC_REG_EAX (ENCODE_REG_TYPE(BC_REG_32)|1)
#define BC_REG_EBX (ENCODE_REG_TYPE(BC_REG_32)|2)
#define BC_REG_ECX (ENCODE_REG_TYPE(BC_REG_32)|3)
#define BC_REG_EDX (ENCODE_REG_TYPE(BC_REG_32)|4)

#define BC_REG_RAX (ENCODE_REG_TYPE(BC_REG_64)|1)
#define BC_REG_RBX (ENCODE_REG_TYPE(BC_REG_64)|2)
#define BC_REG_RCX (ENCODE_REG_TYPE(BC_REG_64)|3)
#define BC_REG_RDX (ENCODE_REG_TYPE(BC_REG_64)|4)

#define BC_REG_SP (ENCODE_REG_TYPE(BC_REG_64)|5)
#define BC_REG_FP (ENCODE_REG_TYPE(BC_REG_64)|6)
#define BC_REG_PC (ENCODE_REG_TYPE(BC_REG_64)|7)
#define BC_REG_DP (ENCODE_REG_TYPE(BC_REG_64)|8)

const char* RegToStr(u8 reg);

// regName refers to A,B,C,D (1,2,3,4)
int RegBySize(int regName, int size);

#define BC_EXT_ALLOC -1
#define BC_EXT_REALLOC -2
#define BC_EXT_FREE -3
#define BC_EXT_PRINTI -4
#define BC_EXT_PRINTC -5

struct Instruction {
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
engone::Logger& operator<<(engone::Logger& logger, Instruction& instruction);

struct Bytecode {
    static Bytecode* Create();
    static void Destroy(Bytecode*);
    void cleanup();
    
    uint32 getMemoryUsage();
    
    engone::Memory codeSegment{sizeof(Instruction)};
    engone::Memory dataSegment{1};
    
    engone::Memory debugSegment{sizeof(u32)};
    std::vector<std::string> debugText;
    // -1 as index will add text to next instruction
    void addDebugText(const char* str, int length, u32 instructionIndex=-1);
    void addDebugText(const std::string& text, u32 instructionIndex=-1);
    void addDebugText(Token& token, u32 instructionIndex=-1);
    const char* getDebugText(u32 instructionIndex);
    // returns an offset relative to the beginning of the data segment where data was placed.
    int appendData(const void* data, int size);

    bool add(Instruction inst);
    bool addIm(i32 data);
    inline Instruction* get(uint index){
        return ((Instruction*)codeSegment.data + index);
    }
    inline int length(){
        return codeSegment.used;
    }
    bool removeLast();

    void printStats();
};