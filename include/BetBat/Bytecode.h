#pragma once
#include "BetBat/Tokenizer.h"
#include "BetBat/NativeRegistry.h"
#include "BetBat/DebugInformation.h"

#include "BetBat/AST.h"

enum InstructionControl : u8 {
    CONTROL_NONE,
    CONTROL_MASK_SIZE         = 0x3,
    CONTROL_MASK_CONVERT_SIZE = 0xC,
    CONTROL_MASK_OTHER        = 0xF0,
    
    CONTROL_8B  = 0x0,
    CONTROL_16B = 0x1,
    CONTROL_32B = 0x2,
    CONTROL_64B = 0x3,
    
    CONTROL_CONVERT_8B  = 0x0,
    CONTROL_CONVERT_16B = 0x4,
    CONTROL_CONVERT_32B = 0x8,
    CONTROL_CONVERT_64B = 0xC,
    
    CONTROL_FLOAT_OP    = 0x10,
    CONTROL_UNSIGNED_OP = 0x20,
};

#define GET_CONTROL_SIZE(x) (InstructionControl)(CONTROL_MASK_SIZE & x)
#define GET_CONTROL_CONVERT_SIZE(x) (InstructionControl)((CONTROL_MASK_CONVERT_SIZE & x) >> 2)
#define IS_CONTROL_SIGNED(x) (!(CONTROL_UNSIGNED_OP & x))
#define IS_CONTROL_FLOAT(x) (CONTROL_FLOAT_OP & x)
// from -> to (CAST_FROM_TO)
enum InstructionCast : u8 {
    CAST_UINT_UINT = 0,
    CAST_UINT_SINT,
    CAST_SINT_UINT,
    CAST_SINT_SINT,
    CAST_FLOAT_UINT,
    CAST_FLOAT_SINT,
    CAST_UINT_FLOAT,
    CAST_SINT_FLOAT,
    CAST_FLOAT_FLOAT,
};
enum InstructionType : u8 {
    BC_HALT = 0,
    BC_NOP,
    
    BC_MOV_RR,
    BC_MOV_RM,
    BC_MOV_MR,
    BC_MOV_RM_DISP16,
    BC_MOV_MR_DISP16,
    
    BC_PUSH,
    BC_POP,
    BC_LI32,
    BC_LI64,
    BC_INCR, // usually used with stack pointer

    BC_JMP,
    BC_CALL,
    BC_RET,
    // jump if not zero
    BC_JNZ,
    // jump if zero
    BC_JZ,

    BC_DATAPTR,
    BC_CODEPTR,

    // arithmetic operations
    BC_ADD,
    BC_SUB,
    BC_MUL,
    BC_DIV,
    BC_MOD,
    
    // comparisons
    BC_EQ,
    BC_NEQ,
    BC_LT,
    BC_LTE,
    BC_GT,
    BC_GTE,

    // Don't rearrange these. You can use the bits to tell
    // whether the left or right type is signed/unsigned.
    #define CMP_UINT_UINT 0
    #define CMP_UINT_SINT 1
    #define CMP_SINT_UINT 2
    #define CMP_SINT_SINT 3
    #define CMP_DECODE(L,R,...) (u8)(((u8)__VA_ARGS__(ltype)<<1) | (u8)__VA_ARGS__(rtype)) 

    // logical operations
    BC_LAND,
    BC_LOR,
    BC_LNOT,

    // bitwise operations
    BC_BAND,
    BC_BOR,
    BC_BNOT,
    BC_BXOR,
    BC_BLSHIFT,
    BC_BRSHIFT,

    BC_CAST,

    BC_MEMZERO,
    BC_MEMCPY,
    BC_STRLEN,
    BC_RDTSC,
    // #define BC_RDTSCP 109
    // compare and swap, atomic
    BC_CMP_SWAP,
    BC_ATOMIC_ADD,

    BC_SQRT,
    BC_ROUND,

    BC_ASM,
    #define ASM_ENCODE_INDEX(ind) (u8)(asmInstanceIndex&0xFF), (u8)((asmInstanceIndex>>8)&0xFF), (u8)((asmInstanceIndex>>16)&0xFF)
    #define ASM_DECODE_INDEX(op0,op1,op2) (u32)(op0 | (op1<<8) | (op2<<16))

    // used when running test cases
    // The stack must be aligned to 16 bytes because
    // there are some functions being called inside which reguire it.
    BC_TEST_VALUE,
};
enum BCRegister : u8 {
    BC_REG_INVALID = 0,
    // general purpose registers
    BC_REG_A, // accumulator
    BC_REG_B,
    BC_REG_C,
    BC_REG_D,
    BC_REG_E,
    BC_REG_F,
    
     // temporary registers
    BC_REG_T0,
    BC_REG_T1,
    
    // special registers
    BC_REG_SP, // stack pointer
    BC_REG_BP, // base pointer
    
    // platform specific (we would prefer to be architecture independent but I am not sure how to deal with calling conventions without them here)
    BC_REG_RAX,
    BC_REG_RBX,
    BC_REG_RCX,
    BC_REG_RDX,
    BC_REG_RSI,
    BC_REG_RDI,
    BC_REG_R8,
    BC_REG_R9,
    BC_REG_XMM0,
    BC_REG_XMM1,
    BC_REG_XMM2,
    BC_REG_XMM3,

    BC_REG_MAX,
};
extern const char* control_names[];
extern const char* cast_names[];
extern const char* instruction_names[];
extern const char* register_names[];

#define MISALIGNMENT(X,ALIGNMENT) ((ALIGNMENT - (X) % ALIGNMENT) % ALIGNMENT)

struct Bytecode;
// Look at me I'm tiny bytecode! 
struct TinyBytecode {
    std::string name;
    QuickArray<u8> instructionSegment{};
    int index = 0;
    // debug information
    QuickArray<u32> index_of_lines{};
    struct Line {
        int line_number;
        std::string text;  
    };
    DynamicArray<Line> lines{};
    // QuickArray<u32> index_of_opcodes{};

    struct Relocation{
        i32 pc=0; // index of the 32-bit integer to relocate
        FuncImpl* funcImpl = nullptr;
        std::string func_name;
    };
    DynamicArray<Relocation> call_relocations;
    bool applyRelocations(Bytecode* code);
    void addRelocation(int pc, const std::string& name) {
        call_relocations.add({});
        call_relocations.last().pc = pc;
        call_relocations.last().func_name = name;
    }
    void addRelocation(int pc, FuncImpl* impl) {
        call_relocations.add({});
        call_relocations.last().pc = pc;
        call_relocations.last().funcImpl = impl;
    }
    
    void print(int low_index, int high_index, Bytecode* code = nullptr, bool force_newline = false);
};

struct Bytecode {
    static Bytecode* Create();
    static void Destroy(Bytecode*);
    void cleanup();
    
    TinyBytecode* createTiny() {
        auto ptr = new TinyBytecode();
        tinyBytecodes.add(ptr);
        ptr->index = tinyBytecodes.size()-1;
        return ptr;
    }
    
    u32 getMemoryUsage();
    
    DynamicArray<TinyBytecode*> tinyBytecodes;

    QuickArray<u8> dataSegment{};
    engone::Mutex lock_global_data;

    DebugInformation* debugInformation = nullptr;

    // This is debug data for interpreter
    // QuickArray<u32> debugSegment{}; // indices to debugLocations
    
    // struct Location {
    //     u32 line=0;
    //     u32 column=0;
    //     std::string file{};
    //     std::string desc{};
    //     std::string preDesc{};
    //     void* stream = nullptr;
    //     // You are not supposed to access any content in the stream. It is just here to compare against other stream pointers.
    //     // GenInfo::addInstruction needs the stream pointer.
    // };
    // DynamicArray<Location> debugLocations;

    DynamicArray<std::string> linkDirectives;

    // Assembly or bytecode dump after the compilation is done.
    struct Dump {
        bool dumpBytecode = false;
        bool dumpAsm = false;
        int bc_startIndex = 0;
        int bc_endIndex = 0;
        int asm_startIndex = 0;
        int asm_endIndex = 0;
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
    struct ExportedFunction {
        std::string name;
        int tinycode_index = 0;
    };
    DynamicArray<ExportedFunction> exportedFunctions;
    // returns false if a symbol with 'name' has been exported already
    bool addExportedFunction(const std::string& name, int tinycode_index);

    struct ExternalRelocation {
        std::string name;
        int tinycode_index=0;
        int pc=0;
    };
    // Relocation for external functions
    DynamicArray<ExternalRelocation> externalRelocations;
    void addExternalRelocation(const std::string& name,  int tinycode_index, int pc);

    // struct PtrDataRelocation {
    //     u32 referer_dataOffset;
    //     u32 target_dataOffset;
    // };
    // DynamicArray<PtrDataRelocation> ptrDataRelocations;

    // struct Symbol {
    //     std::string name;
    //     u32 bcIndex=0;
    // };
    // DynamicArray<Symbol> exportedSymbols; // usually function names
    // bool exportSymbol(const std::string& name, u32 instructionIndex);

    // const Location* getLocation(u32 instructionIndex, u32* locationIndex = nullptr);
    // Location* setLocationInfo(const TokenRange& token, u32 InstructionIndex=-1, u32* locationIndex = nullptr);
    // Location* setLocationInfo(const char* preText, u32 InstructionIndex=-1);
    // // use same location as said register
    // Location* setLocationInfo(u32 locationIndex, u32 instructionIndex=-1);
    
    // -1 as index will add text to next instruction
    // void addDebugText(const char* str, int length, u32 instructionIndex=-1);
    // void addDebugText(const std::string& text, u32 instructionIndex=-1);
    // void addDebugText(Token& token, u32 instructionIndex=-1);
    // const char* getDebugText(u32 instructionIndex);

    // returns an offset relative to the beginning of the data segment where data was placed.
    // data may be nullptr which will give you space with zero-initialized data.
    int appendData(const void* data, int size);
    void ensureAlignmentInData(int alignment);
    // returns offset into data, also sets out_ptr which is a DANGEROUS pointer that may be invalidated.
    // write to it before calling appendData again!

    // int appendData_late(void** out_ptr, int size);

    // void printInstruction(u32 index, bool printImmediates);
    // Returns the number of immediates an instruction uses.
    // When getting the next instruction you should skip by the amount of 
    // immediates.
    // u32 immediatesOfInstruction(u32 index);

    // bool add_notabug(Instruction inst);
    // bool addIm_notabug(i32 data);
    // inline Instruction& get(uint index){
    //     return *((Instruction*)instructionSegment.data + index);
    // }
    // inline i32& getIm(u32 index){
    //     return *((i32*)instructionSegment.data() + index);
    // }

    // inline int length(){
    //     return instructionSegment.used;
    // }
    bool removeLast();

    void printStats();
    
    void print();
};
struct BytecodeBuilder {
    Bytecode* code=nullptr;
    TinyBytecode* tinycode=nullptr;
    
    int getStackPointer() const { return virtualStackPointer; }
    // in the future, this function may differ from getStackPointer
    int saveStackMoment() const { return virtualStackPointer; }
    void restoreStackMoment(int saved_sp, bool no_modification = false, bool no_instruction = false);
    
    void init(Bytecode* code, TinyBytecode* tinycode);
    
    // make space for local variables
    void emit_stack_space(int size);
    void emit_stack_alignment(int alignment);
    
    void emit_push(BCRegister reg, bool without_instruction = false);
    void emit_pop(BCRegister reg);
    void emit_li32(BCRegister reg, i32 imm);
    void emit_li64(BCRegister reg, i64 imm);
    
    void emit_incr(BCRegister reg, i32 imm, bool no_modification = false);
    
    void emit_call(LinkConventions l, CallConventions c, i32* index_of_relocation, i32 imm = 0);
    void emit_ret();
    
    void emit_jmp(int pc);
    int emit_jmp();
    void emit_jz(BCRegister reg, int pc);
    int emit_jz(BCRegister reg);
    void emit_jnz(BCRegister reg, int pc);
    int emit_jnz(BCRegister reg);
    
    void emit_mov_rr(BCRegister to, BCRegister from);
    void emit_mov_rm(BCRegister to, BCRegister from, int size);
    void emit_mov_mr(BCRegister to, BCRegister from, int size);
    void emit_mov_rm_disp(BCRegister to, BCRegister from, int size, int displacement);
    void emit_mov_mr_disp(BCRegister to, BCRegister from, int size, int displacement);
    
    void emit_add(BCRegister to, BCRegister from, bool is_float, int size);
    void emit_sub(BCRegister to, BCRegister from, bool is_float, int size);
    void emit_mul(BCRegister to, BCRegister from, bool is_float, int size, bool is_signed);
    void emit_div(BCRegister to, BCRegister from, bool is_float, int size, bool is_signed);
    void emit_mod(BCRegister to, BCRegister from, bool is_float, int size, bool is_signed);
    
    // nocheckin TODO: These should have size arguments too!
    void emit_band(BCRegister to, BCRegister from);
    void emit_bor(BCRegister to, BCRegister from);
    void emit_bxor(BCRegister to, BCRegister from);
    void emit_bnot(BCRegister to, BCRegister from);
    void emit_blshift(BCRegister to, BCRegister from);
    void emit_brshift(BCRegister to, BCRegister from);
    
    void emit_eq(BCRegister to, BCRegister from, bool is_float);
    void emit_neq(BCRegister to, BCRegister from, bool is_float);
    void emit_lt(BCRegister to, BCRegister from, bool is_float, bool is_signed);
    void emit_lte(BCRegister to, BCRegister from, bool is_float, bool is_signed);
    void emit_gt(BCRegister to, BCRegister from, bool is_float, bool is_signed);
    void emit_gte(BCRegister to, BCRegister from, bool is_float, bool is_signed);
    
    void emit_land(BCRegister to, BCRegister from);
    void emit_lor(BCRegister to, BCRegister from);
    void emit_lnot(BCRegister to, BCRegister from);
    
    void emit_dataptr(BCRegister reg, i32 imm);
    void emit_codeptr(BCRegister reg, i32 imm);
    
    void emit_cast(BCRegister reg, InstructionCast castType, u8 from_size, u8 to_size);
    
    void emit_memzero(BCRegister dst, BCRegister size_reg, u8 batch);
    void emit_memcpy(BCRegister dst, BCRegister src, BCRegister size_reg);
    // void emit_strlen(BCRegister len_reg, BCRegister src_len);
    // void emit_rdtsc(BCRegister to, BCRegister from, u8 batch);
    
    // void emit_cmp_swap(BCRegister to, BCRegister from, u8 batch);
    // void emit_atomic_add(BCRegister to, BCRegister from, u8 batch);
    
    // void emit_sqrt(BCRegister to, BCRegister from, u8 batch);
    // void emit_round(BCRegister to, BCRegister from, u8 batch);
    
    // void emit_test(BCRegister to, BCRegister from, u8 size);
    
    int get_pc() { return tinycode->instructionSegment.size(); }
    void fix_jump_imm32_here(int imm_index);
    
    // int opcode_count() { return tinycode->index_of_opcodes.size(); }
    // InstructionType get_opcode(int opcode_index) { return (InstructionType)tinycode->instructionSegment[tinycode->index_of_opcodes[opcode_index]]; }
    InstructionType get_last_opcode() { if(tinycode->instructionSegment.size() == 0) return (InstructionType)0; return (InstructionType)tinycode->instructionSegment[index_of_last_instruction]; }
    
    void push_line(int line, const std::string& text) {
        tinycode->lines.add({line, text});
    }
    
private:
    // building blocks for every instruction
    void emit_opcode(InstructionType type);
    void emit_operand(BCRegister reg);
    void emit_control(InstructionControl control);
    void emit_imm8(i8 imm);
    void emit_imm16(i16 imm);
    void emit_imm32(i32 imm);
    void emit_imm64(i64 imm);
    
    // struct AlignInfo {
    //     int diff=0;
    //     int size=0;
    // };
    // DynamicArray<AlignInfo> stackAlignment;
    int virtualStackPointer = 0;
    int index_of_last_instruction = -1;
};

// BC FILE FORMAT

// struct BTBCHeader{
//     u32 magicNumber = 0x13579BDF;
//     char humanChars[4] = {'B','T','B','C'};
//     // TODO: Version number for interpreter?
//     // TODO: Version number for bytecode format?
//     u32 codeSize = 0;
//     u32 dataSize = 0;
// };
// bool ExportBytecode(CompileOptions* options, Bytecode* bytecode){
//     using namespace engone;
//     Assert(bytecode);
//     auto file = FileOpen(options->outputFile.text, 0, engone::FILE_CLEAR_AND_WRITE);
//     if(!file)
//         return false;
//     BTBCHeader header{};

//     Assert(bytecode->instructionSegment.used*bytecode->instructionSegment.getTypeSize() < ((u64)1<<32));
//     header.codeSize = bytecode->instructionSegment.used*bytecode->instructionSegment.getTypeSize();
//     Assert(bytecode->dataSegment.used*bytecode->dataSegment.getTypeSize() < ((u64)1<<32));
//     header.dataSize = bytecode->dataSegment.used*bytecode->dataSegment.getTypeSize();

//     // TOOD: Check error when writing files
//     int err = FileWrite(file, &header, sizeof(header));
//     if(err!=-1 && bytecode->instructionSegment.data){
//         err = FileWrite(file, bytecode->instructionSegment.data, header.codeSize);
//     }
//     if(err!=-1 && bytecode->dataSegment.data) {
//         err = FileWrite(file, bytecode->dataSegment.data, header.dataSize);
//     }
//     // debugSegment is ignored

//     FileClose(file);
//     return true;
// }
// Bytecode* ImportBytecode(Path filePath){
//     using namespace engone;
//     u64 fileSize = 0;
//     auto file = FileOpen(filePath.text, &fileSize, engone::FILE_ONLY_READ);

//     if(fileSize < sizeof(BTBCHeader))
//         return nullptr; // File is to small for a bytecode file

//     BTBCHeader baseHeader{};
//     BTBCHeader header{};

//     // TOOD: Check error when writing files
//     FileRead(file, &header, sizeof(header));
    
//     if(strncmp((char*)&baseHeader,(char*)&header,8)) // TODO: Don't use hardcoded 8?
//         return nullptr; // Magic number and human characters is incorrect. Meaning not a bytecode file.

//     if(fileSize != sizeof(BTBCHeader) + header.codeSize + header.dataSize)
//         return nullptr; // Corrupt file. Sizes does not match.
    
//     Bytecode* bc = Bytecode::Create();
//     bc->instructionSegment.resize(header.codeSize/bc->instructionSegment.getTypeSize());
//     bc->instructionSegment.used = header.codeSize/bc->instructionSegment.getTypeSize();
//     bc->dataSegment.resize(header.dataSize/bc->dataSegment.getTypeSize());
//     bc->dataSegment.used = header.dataSize/bc->dataSegment.getTypeSize();
//     FileRead(file, bc->instructionSegment.data, header.codeSize);
//     FileRead(file, bc->dataSegment.data, header.dataSize);

//     // debugSegment is ignored

//     FileClose(file);
//     return bc;
// }