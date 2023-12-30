#pragma once
#include "BetBat/Tokenizer.h"
#include "BetBat/NativeRegistry.h"
#include "BetBat/DebugInformation.h"

/*
    New bytecode instructions
    The purpose of these new instructions is to be less
    Not implemented yet though because it's a lot of work and not a priority.
*/
// opcode
// enum BytecodeOpcode : u8 {
//     BCO_NONE = 0,
//     BCO_MOV_RR = 1,
//     BCO_MOV_RM = 2,
//     BCO_MOV_RM_DISP32 = 3,
//     BCO_MOV_MR = 4,
//     BCO_MOV_MR_DISP32 = 5,
    
//     BCO_MOD = 10, // control, op1, op2
//     BCO_ADD = 11, // control, op1, op2
//     BCO_SUB = 12, // control, op1, op2
//     BCO_MUL = 13, // control, op1, op2
//     BCO_DIV = 14, // control, op1, op2
//     BCO_INCR = 18, // control, op1, imm
    
//     BCO_JMP = 30,
//     BCO_CALL = 31,
//     BCO_RET = 32,
//     BCO_JE = 33,
//     BCO_JNE = 34,
    
//     BCO_PUSH = 40,
//     BCO_POP = 41,
//     BCO_LI = 42,
//     BCO_DATAPTR = 43,
//     BCO_CODEPTR = 44,
    
//     BCO_EQ =   50,
//     BCO_NEQ =  51,
//     BCO_LT =   52,
//     BCO_LTE =  53,
//     BCO_GT =   54,
//     BCO_GTE =  55,
    
//     BCO_ANDI =  56,
//     BCO_ORI =   57,
//     BCO_NOT =  58,

//     BCO_BXOR =  60,
//     BCO_BOR =  61,
//     BCO_BAND =  62,
//     BCO_BNOT = 63,
//     BCO_BLSHIFT = 64,
//     BCO_BRSHIFT = 65,
    
//     BCO_CAST = 70,
//     BCO_ASM = 71,
// // #define ASM_ENCODE_INDEX(ind) (u8)(asmInstanceIndex&0xFF), (u8)((asmInstanceIndex>>8)&0xFF), (u8)((asmInstanceIndex>>16)&0xFF)
// // #define ASM_DECODE_INDEX(op0,op1,op2) (u32)(op0 | (op1<<8) | (op2<<16))
    
//     BCO_MEMZERO = 90,
//     BCO_MEMCPY = 91,
//     BCO_STRLEN = 92,
//     BCO_RDTSC = 93,
//     // BC_RDTSCP = 94,
//     // compare and swap, atomic
//     BCO_CMP_SWAP = 95,
//     BCO_ATOMIC_ADD = 96,

//     BCO_SQRT = 180,
//     BCO_ROUND = 181,
    
//     BCO_TEST_VALUE = 240,
    
//     BCO_RESERVED0 = 254, // may be used to extend the opcode to two bytes
//     BCO_RESERVED1 = 255, // may be used to extend the opcode to three bytes
// };
// enum BytecodeControl : u8 {
//     BCC_NONE = 0,
//     BCC_CAST_FLOAT_SINT = 1,
//     // BCC_CAST_FLOAT_UINT = 1, // float to unsigned doesn't work if float is negative but it's the same with signed to unsigned. Same with signed to unsigned so maybe we cast float to signed
//     BCC_CAST_SINT_FLOAT = 2,
//     BCC_CAST_UINT_FLOAT = 3,
//     BCC_CAST_SINT_UINT = 4,
//     BCC_CAST_UINT_UINT = 4,
//     BCC_CAST_UINT_SINT = 5,
//     BCC_CAST_SINT_SINT = 6,
    
//     BCC_ARITHMETIC_UINT = 20,
//     BCC_ARITHMETIC_SINT = 21,
//     BCC_ARITHMETIC_FLOAT = 22,
    
//     BCC_CMP_UINT = 30,
//     BCC_CMP_SINT = 31,
//     BCC_CMP_FLOAT = 32,
    
//     // bit mask
//     BCC_MOV_8BIT = 0x00,
//     BCC_MOV_16BIT = 0x40,
//     BCC_MOV_32BIT = 0x80,
//     BCC_MOV_64BIT = 0xC0,
// };
// enum BytecodeRegister : u8 {
//     BCR_NONE = 0,
//     BCR_SP = 1,
//     BCR_FP = 2,
//     BCR_X = 3, // BCR_X + registerNumber
// };

/*
    Old bytecode instructions
*/
enum BCInstruction : u8{
    BC_MOV_RR = 1,
    BC_MOV_RM = 2,
    BC_MOV_RM_DISP32 = 3,
    BC_MOV_MR = 4,
    BC_MOV_MR_DISP32 = 5,

    #define ARITHMETIC_UINT 0
    #define ARITHMETIC_SINT 1
    BC_MODI = 8,
    BC_FMOD = 9,
    BC_ADDI = 10,
    BC_FADD = 11,
    BC_SUBI = 12,
    BC_FSUB = 13,
    BC_MULI = 14,
    BC_FMUL = 15,
    BC_DIVI = 16,
    BC_FDIV = 17,

    BC_INCR = 18,

    BC_JMP = 20,
    BC_CALL = 21,
    BC_RET = 22,
    // jump if not zero
    BC_JE = 23,
    // jump if zero
    BC_JNE = 24,

    BC_PUSH = 30,
    BC_POP = 31,
    BC_LI = 32,

    BC_DATAPTR = 40,
    BC_CODEPTR = 41,

    BC_EQ = 50,
    BC_NEQ = 51,

    BC_LT =   52,
    BC_LTE =  53,
    BC_GT =   54,
    BC_GTE =  55,

    // Don't rearrange these. You can use the bits to tell
    // whether the left or right type is signed/unsigned.
    #define CMP_UINT_UINT 0
    #define CMP_UINT_SINT 1
    #define CMP_SINT_UINT 2
    #define CMP_SINT_SINT 3
    #define CMP_DECODE(L,R,...) (u8)(((u8)__VA_ARGS__(ltype)<<1) | (u8)__VA_ARGS__(rtype)) 

    BC_ANDI =  56,
    BC_ORI =   57,
    BC_NOT =  58,

    BC_BXOR =  60,
    BC_BOR =  61,
    BC_BAND =  62,
    BC_BNOT = 63,
    BC_BLSHIFT = 64,
    BC_BRSHIFT = 65,

    BC_CAST = 90,
    // flags for first operand
    #define CAST_FLOAT_SINT 0
    #define CAST_FLOAT_UINT 1
    #define CAST_SINT_FLOAT 2
    #define CAST_UINT_FLOAT 3
    #define CAST_SINT_UINT 4
    #define CAST_UINT_SINT 5
    #define CAST_SINT_SINT 6
    #define CAST_FLOAT_FLOAT 7

    BC_FEQ =   91,
    BC_FNEQ =  92,
    BC_FLT =   93,
    BC_FLTE =  94,
    BC_FGT =   95,
    BC_FGTE =  96,

    BC_MEMZERO = 100,
    BC_MEMCPY = 101,
    BC_STRLEN = 102,
    BC_RDTSC = 103,
    // #define BC_RDTSCP 109
    // compare and swap, atomic
    BC_CMP_SWAP = 104,
    BC_ATOMIC_ADD = 105,

    BC_SQRT = 120,
    BC_ROUND = 121,

    BC_ASM = 130,
    #define ASM_ENCODE_INDEX(ind) (u8)(asmInstanceIndex&0xFF), (u8)((asmInstanceIndex>>8)&0xFF), (u8)((asmInstanceIndex>>16)&0xFF)
    #define ASM_DECODE_INDEX(op0,op1,op2) (u32)(op0 | (op1<<8) | (op2<<16))

    // used when running test cases
    // The stack must be aligned to 16 bytes because
    // there are some functions being called inside which reguire it.
    BC_TEST_VALUE = 200,
};
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

#define DECODE_REG_TYPE(R) ((R)&~BC_REG_MASK)

#define DECODE_REG_BITS(R) (u8)(((R)&BC_REG_MASK)>>6)
#define DECODE_REG_SIZE(R) (u8)(R==0?0:(1<<DECODE_REG_BITS(R)))
#define ENCODE_REG_BITS(R,B) (u8)(((B)<<6)|(R&~BC_REG_MASK))

#define SIZE_TO_BITS(X) (u8)(X&8?3:X&4?2:X&2?1:X&1?0:0)
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

#define IS_REG_NORMAL(reg) (BC_AX <= DECODE_REG_TYPE(reg) && DECODE_REG_TYPE(reg) <= BC_DI)

// These are used with calling conventions
#define BC_R8 9
#define BC_R9 10
#define BC_R10 11
#define BC_R11 12

#define IS_REG_RX(reg) (BC_R8 <= DECODE_REG_TYPE(reg) && DECODE_REG_TYPE(reg) <= BC_R11)

#define BC_XMM0 20
#define BC_XMM1 21
#define BC_XMM2 22
#define BC_XMM3 23
#define BC_XMM4 24
#define BC_XMM5 25
#define BC_XMM6 26
#define BC_XMM7 27

#define IS_REG_XMM(reg) (BC_XMM0 <= DECODE_REG_TYPE(reg) && DECODE_REG_TYPE(reg) <= BC_XMM7)

// BC_REG_ALL can't be 0 because it's seen as no register so we do 8.
#define BC_REG_AL (ENCODE_REG_BITS(BC_AX, BC_REG_8))
#define BC_REG_BL (ENCODE_REG_BITS(BC_BX, BC_REG_8))
#define BC_REG_CL (ENCODE_REG_BITS(BC_CX, BC_REG_8))
#define BC_REG_DL (ENCODE_REG_BITS(BC_DX, BC_REG_8))

#define BC_REG_AX (ENCODE_REG_BITS(BC_AX, BC_REG_16))
#define BC_REG_BX (ENCODE_REG_BITS(BC_BX, BC_REG_16))
#define BC_REG_CX (ENCODE_REG_BITS(BC_CX, BC_REG_16))
#define BC_REG_DX (ENCODE_REG_BITS(BC_DX, BC_REG_16))

#define BC_REG_EAX (ENCODE_REG_BITS(BC_AX, BC_REG_32))
#define BC_REG_EBX (ENCODE_REG_BITS(BC_BX, BC_REG_32))
#define BC_REG_ECX (ENCODE_REG_BITS(BC_CX, BC_REG_32))
#define BC_REG_EDX (ENCODE_REG_BITS(BC_DX, BC_REG_32))

#define BC_REG_RAX (ENCODE_REG_BITS(BC_AX, BC_REG_64))
#define BC_REG_RBX (ENCODE_REG_BITS(BC_BX, BC_REG_64))
#define BC_REG_RCX (ENCODE_REG_BITS(BC_CX, BC_REG_64))
#define BC_REG_RDX (ENCODE_REG_BITS(BC_DX, BC_REG_64))

#define BC_REG_SP  (ENCODE_REG_BITS(BC_SP, BC_REG_64))
#define BC_REG_FP  (ENCODE_REG_BITS(BC_FP, BC_REG_64))
#define BC_REG_RSI (ENCODE_REG_BITS(BC_SI, BC_REG_64))
#define BC_REG_RDI (ENCODE_REG_BITS(BC_DI, BC_REG_64))

#define BC_REG_R8  (ENCODE_REG_BITS(BC_R8, BC_REG_64))
#define BC_REG_R9  (ENCODE_REG_BITS(BC_R9, BC_REG_64))

// float
#define BC_REG_XMM0f (ENCODE_REG_BITS(BC_XMM0, BC_REG_32))
#define BC_REG_XMM1f (ENCODE_REG_BITS(BC_XMM1, BC_REG_32))
#define BC_REG_XMM2f (ENCODE_REG_BITS(BC_XMM2, BC_REG_32))
#define BC_REG_XMM3f (ENCODE_REG_BITS(BC_XMM3, BC_REG_32))
#define BC_REG_XMM4f (ENCODE_REG_BITS(BC_XMM4, BC_REG_32))
#define BC_REG_XMM5f (ENCODE_REG_BITS(BC_XMM5, BC_REG_32))
#define BC_REG_XMM6f (ENCODE_REG_BITS(BC_XMM6, BC_REG_32))
#define BC_REG_XMM7f (ENCODE_REG_BITS(BC_XMM7, BC_REG_32))

// double
#define BC_REG_XMM0d (ENCODE_REG_BITS(BC_XMM0, BC_REG_64))
#define BC_REG_XMM1d (ENCODE_REG_BITS(BC_XMM1, BC_REG_64))
#define BC_REG_XMM2d (ENCODE_REG_BITS(BC_XMM2, BC_REG_64))
#define BC_REG_XMM3d (ENCODE_REG_BITS(BC_XMM3, BC_REG_64))
#define BC_REG_XMM4d (ENCODE_REG_BITS(BC_XMM4, BC_REG_64))
#define BC_REG_XMM5d (ENCODE_REG_BITS(BC_XMM5, BC_REG_64))
#define BC_REG_XMM6d (ENCODE_REG_BITS(BC_XMM6, BC_REG_64))
#define BC_REG_XMM7d (ENCODE_REG_BITS(BC_XMM7, BC_REG_64))
// data pointer shouldn't be messed with directly
// #define BC_REG_DP (ENCODE_REG_BITS(BC_REG_64)|11)

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
    BCInstruction opcode=(BCInstruction)0;
    union {
        u8 operands[3]{0};
        struct{
            u8 op0;
            u8 op1;
            u8 op2;
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
    
    // struct Register {
    //     bool inUse = false;
    // };
    // Register registers[256];
    // QuickArray<u8> codeSegment{}; // uses new bytecode instructions
    
    engone::Memory<Instruction> instructionSegment{};
    engone::Memory<u8> dataSegment{};

    DebugInformation* debugInformation = nullptr;

    // This is debug data for interpreter
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

    // Assembly or bytecode dump after the compilation is done.
    struct Dump {
        bool dumpBytecode = false;
        bool dumpAsm = false;
        int startIndex = 0;
        int endIndex = 0;
        int startIndexAsm = 0;
        int endIndexAsm = 0;
        std::string description;
    };
    DynamicArray<Dump> debugDumps;

    QuickArray<char> rawInlineAssembly;
    QuickArray<u8> rawInstructions; // modified when passed converter
    struct ASM {
        u32 start = 0; // points to raw inline assembly
        u32 end = 0; // exclusive
        u32 iStart = 0; // points to raw instructions
        u32 iEnd = 0; // exclusive
        
        u32 lineStart = 0;
        u32 lineEnd = 0;
        std::string file;
    };
    DynamicArray<ASM> asmInstances;
    // NativeRegistry* nativeRegistry = nullptr;

    // usually a function like main
    struct ExportedSymbol {
        std::string name;
        u32 location = 0;
    };
    DynamicArray<ExportedSymbol> exportedSymbols;
    // returns false if a symbol with 'name' has been exported already
    bool addExportedSymbol(const std::string& name,  u32 location);

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

    const Location* getLocation(u32 instructionIndex, u32* locationIndex = nullptr);
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

    void printInstruction(u32 index, bool printImmediates);
    // Returns the number of immediates an instruction uses.
    // When getting the next instruction you should skip by the amount of 
    // immediates.
    u32 immediatesOfInstruction(u32 index);

    bool add_notabug(Instruction inst);
    bool addIm_notabug(i32 data);
    inline Instruction& get(uint index){
        return *((Instruction*)instructionSegment.data + index);
    }
    inline i32& getIm(u32 index){
        return *((i32*)instructionSegment.data + index);
    }

    inline int length(){
        return instructionSegment.used;
    }
    bool removeLast();

    void printStats();
};