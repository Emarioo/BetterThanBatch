#pragma once
#include "BetBat/Tokenizer.h"
#include "BetBat/NativeRegistry.h"

#define BC_MOV_RR 1
#define BC_MOV_RM 2
#define BC_MOV_RM_DISP32 3
#define BC_MOV_MR 4
#define BC_MOV_MR_DISP32 5

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

#define BC_DATAPTR 40
#define BC_CODEPTR 41

// TODO: You forgot about floating point comparisons!
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
#define CAST_FLOAT_UINT 1
#define CAST_SINT_FLOAT 2
#define CAST_UINT_FLOAT 3
#define CAST_SINT_UINT 4
#define CAST_UINT_SINT 5
#define CAST_SINT_SINT 6

#define BC_MEMZERO 100
#define BC_MEMCPY 101
#define BC_RDTSC 102
// #define BC_RDTSCP 103
// compare and swap, atomic
#define BC_CMP_SWAP 104

// #define BC_SIN 110
// #define BC_COS 111
// #define BC_TAN 112
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

#define BC_REG_MASK 0xC0
#define BC_REG_8 0
#define BC_REG_16 1
#define BC_REG_32 2
#define BC_REG_64 3

#define DECODE_REG_TYPE(X) ((X)&~BC_REG_MASK)

#define DECODE_REG_SIZE_TYPE(X) (((X)&BC_REG_MASK)>>6)
#define ENCODE_REG_SIZE_TYPE(X) ((X)<<6)
#define DECODE_REG_SIZE(X) (X==0?0:(1<<DECODE_REG_SIZE_TYPE(X)))

// The ordering is strange to make it easier to convert it
// to x86 representation.

// it does not start from zero BECAUSE if it's zero
// then it's an invalid instruction. Asserts have caught
// a lot of mistakes with this and save a lot of debug time.
// DON'T CHANGE IT.
#define BC_AX 1
#define BC_BX 4
#define BC_CX 2
#define BC_DX 3
#define BC_SP 5
#define BC_FP 6
#define BC_SI 7
#define BC_DI 8

// These are used with calling conventions
#define BC_R8 9
#define BC_R9 10

#define BC_XMM0 20
#define BC_XMM1 21
#define BC_XMM2 22
#define BC_XMM3 23

#define IS_REG_XMM(reg) (BC_XMM0 <= DECODE_REG_TYPE(reg) && DECODE_REG_TYPE(reg) <= BC_XMM3)

// BC_REG_ALL can't be 0 because it's seen as no register so we do 8.
#define BC_REG_AL (ENCODE_REG_SIZE_TYPE(BC_REG_8)|BC_AX)
#define BC_REG_BL (ENCODE_REG_SIZE_TYPE(BC_REG_8)|BC_BX)
#define BC_REG_CL (ENCODE_REG_SIZE_TYPE(BC_REG_8)|BC_CX)
#define BC_REG_DL (ENCODE_REG_SIZE_TYPE(BC_REG_8)|BC_DX)

// #define BC_REG_AH (ENCODE_REG_SIZE_TYPE(BC_REG_8)|5)
// #define BC_REG_BH (ENCODE_REG_SIZE_TYPE(BC_REG_8)|6)
// #define BC_REG_CH (ENCODE_REG_SIZE_TYPE(BC_REG_8)|7)
// #define BC_REG_DH (ENCODE_REG_SIZE_TYPE(BC_REG_8)|8)

#define BC_REG_AX (ENCODE_REG_SIZE_TYPE(BC_REG_16)|BC_AX)
#define BC_REG_BX (ENCODE_REG_SIZE_TYPE(BC_REG_16)|BC_BX)
#define BC_REG_CX (ENCODE_REG_SIZE_TYPE(BC_REG_16)|BC_CX)
#define BC_REG_DX (ENCODE_REG_SIZE_TYPE(BC_REG_16)|BC_DX)

#define BC_REG_EAX (ENCODE_REG_SIZE_TYPE(BC_REG_32)|BC_AX)
#define BC_REG_EBX (ENCODE_REG_SIZE_TYPE(BC_REG_32)|BC_BX)
#define BC_REG_ECX (ENCODE_REG_SIZE_TYPE(BC_REG_32)|BC_CX)
#define BC_REG_EDX (ENCODE_REG_SIZE_TYPE(BC_REG_32)|BC_DX)

#define BC_REG_RAX (ENCODE_REG_SIZE_TYPE(BC_REG_64)|BC_AX)
#define BC_REG_RBX (ENCODE_REG_SIZE_TYPE(BC_REG_64)|BC_BX)
#define BC_REG_RCX (ENCODE_REG_SIZE_TYPE(BC_REG_64)|BC_CX)
#define BC_REG_RDX (ENCODE_REG_SIZE_TYPE(BC_REG_64)|BC_DX)

#define BC_REG_SP (ENCODE_REG_SIZE_TYPE(BC_REG_64)|BC_SP)
#define BC_REG_FP (ENCODE_REG_SIZE_TYPE(BC_REG_64)|BC_FP)
#define BC_REG_RSI (ENCODE_REG_SIZE_TYPE(BC_REG_64)|BC_SI)
#define BC_REG_RDI (ENCODE_REG_SIZE_TYPE(BC_REG_64)|BC_DI)

#define BC_REG_R8 (ENCODE_REG_SIZE_TYPE(BC_REG_64)|BC_R8)
#define BC_REG_R9 (ENCODE_REG_SIZE_TYPE(BC_REG_64)|BC_R9)

#define BC_REG_XMM0 (ENCODE_REG_SIZE_TYPE(BC_REG_64)|BC_XMM0)
#define BC_REG_XMM1 (ENCODE_REG_SIZE_TYPE(BC_REG_64)|BC_XMM1)
#define BC_REG_XMM2 (ENCODE_REG_SIZE_TYPE(BC_REG_64)|BC_XMM2)
#define BC_REG_XMM3 (ENCODE_REG_SIZE_TYPE(BC_REG_64)|BC_XMM3)

#define BC_REG_PC (ENCODE_REG_SIZE_TYPE(BC_REG_64)|10)
// data pointer shouldn't be messed with directly
// #define BC_REG_DP (ENCODE_REG_SIZE_TYPE(BC_REG_64)|11)

#define MISALIGNMENT(X,ALIGNMENT) ((ALIGNMENT - (X) % ALIGNMENT) % ALIGNMENT)

// #define REG_AX 0b000
// #define REG_CX 0b001
// #define REG_DX 0b010
// #define REG_BX 0b011
// #define REG_SP 0b100
// #define REG_BP 0b101
// #define REG_SI 0b110
// #define REG_DI 0b111

const char* InstToString(int type);
const char* RegToStr(u8 reg);

// regName refers to BC_AX/BX/CX/DX
int RegBySize(u8 regName, int size);

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
    
    engone::Memory<Instruction> codeSegment{};
    engone::Memory<u8> dataSegment{};
    
    engone::Memory<u32> debugSegment{}; // indices to debugLocations
    struct Location {
        u32 line=0;
        u32 column=0;
        std::string file{};
        std::string desc{};
        std::string preDesc{};
        void* stream = nullptr;
        // You are not supposed to access any content in the stream. It is just here to compare against other stream pointers.
        // GenInfo::addInstruction needs the stream pointer.
    };
    DynamicArray<Location> debugLocations;

    DynamicArray<std::string> linkDirectives;

    NativeRegistry* nativeRegistry = nullptr;

    struct ExternalRelocation {
        std::string name;
        u32 location=0;
    };
    // Relocation for external functions
    DynamicArray<ExternalRelocation> externalRelocations;
    void addExternalRelocation(const std::string& name,  u32 location);

    // struct Symbol {
    //     std::string name;
    //     u32 bcIndex=0;
    // };
    // DynamicArray<Symbol> exportedSymbols; // usually function names
    // bool exportSymbol(const std::string& name, u32 instructionIndex);

    Location* getLocation(u32 instructionIndex, u32* locationIndex = nullptr);
    Location* setLocationInfo(const TokenRange& token, u32 InstructionIndex=-1, u32* locationIndex = nullptr);
    Location* setLocationInfo(const char* preText, u32 InstructionIndex=-1);
    // use same location as said register
    Location* setLocationInfo(u32 locationIndex, u32 instructionIndex=-1);
    
    // -1 as index will add text to next instruction
    // void addDebugText(const char* str, int length, u32 instructionIndex=-1);
    // void addDebugText(const std::string& text, u32 instructionIndex=-1);
    // void addDebugText(Token& token, u32 instructionIndex=-1);
    // const char* getDebugText(u32 instructionIndex);

    // returns an offset relative to the beginning of the data segment where data was placed.
    // data may be nullptr which will give you space with zero-initialized data.
    int appendData(const void* data, int size);
    void ensureAlignmentInData(int alignment);

    bool add(Instruction inst);
    bool addIm(i32 data);
    inline Instruction& get(uint index){
        return *((Instruction*)codeSegment.data + index);
    }
    inline i32& getIm(u32 index){
        return *((i32*)codeSegment.data + index);
    }

    inline int length(){
        return codeSegment.used;
    }
    bool removeLast();

    void printStats();
};