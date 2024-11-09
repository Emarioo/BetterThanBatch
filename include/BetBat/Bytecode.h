#pragma once
// #include "BetBat/Tokenizer.h"
#include "BetBat/NativeRegistry.h"
#include "BetBat/DebugInformation.h"
#include "BetBat/CompilerOptions.h"
#include "BetBat/ExceptionInformation.h"

#include "BetBat/AST.h"

struct Compiler; // check for errors
enum InstructionControl : u8 {
    CONTROL_NONE,
    CONTROL_MASK_SIZE         = 0x3,
    CONTROL_MASK_CONVERT_SIZE = 0xC,
    CONTROL_MASK_TYPE         = 0xF0,
    
    CONTROL_8B  = 0x0,
    CONTROL_16B = 0x1,
    CONTROL_32B = 0x2,
    CONTROL_64B = 0x3,
    
    CONTROL_CONVERT_8B  = 0x0,
    CONTROL_CONVERT_16B = 0x4,
    CONTROL_CONVERT_32B = 0x8,
    CONTROL_CONVERT_64B = 0xC,
    
    CONTROL_FLOAT_OP    = 0x10,
    CONTROL_SIGNED_OP = 0x20,

    CONTROL_CONVERT_FLOAT_OP = 0x40,
    CONTROL_CONVERT_SIGNED_OP = 0x80,

    // shorts
    CAST_UINT_UINT = 0 | 0,
    CAST_UINT_SINT = 0 | CONTROL_CONVERT_SIGNED_OP,
    CAST_SINT_UINT = CONTROL_SIGNED_OP | 0,
    CAST_SINT_SINT = CONTROL_SIGNED_OP | CONTROL_CONVERT_SIGNED_OP,
    CAST_FLOAT_UINT = CONTROL_FLOAT_OP | 0,
    CAST_FLOAT_SINT = CONTROL_FLOAT_OP | CONTROL_CONVERT_SIGNED_OP,
    CAST_UINT_FLOAT = 0 | CONTROL_CONVERT_FLOAT_OP,
    CAST_SINT_FLOAT = CONTROL_SIGNED_OP | CONTROL_CONVERT_FLOAT_OP,
    CAST_FLOAT_FLOAT = CONTROL_FLOAT_OP | CONTROL_CONVERT_FLOAT_OP,
};
struct DebugInformation;
#define GET_CONTROL_SIZE(x) (InstructionControl)(CONTROL_MASK_SIZE & x)
#define GET_CONTROL_CONVERT_SIZE(x) (InstructionControl)((CONTROL_MASK_CONVERT_SIZE & x) >> 2)
#define GET_CONTROL_CAST(x) (x & CONTROL_MASK_TYPE)
#define IS_CONTROL_UNSIGNED(x) (!((CONTROL_SIGNED_OP|CONTROL_FLOAT_OP) & x))
#define IS_CONTROL_SIGNED(x) (0!=(CONTROL_SIGNED_OP & x))
#define IS_CONTROL_FLOAT(x) (0!=(CONTROL_FLOAT_OP & x))
#define IS_CONTROL_CONVERT_UNSIGNED(x) (!((CONTROL_CONVERT_SIGNED_OP|CONTROL_CONVERT_FLOAT_OP) & x))
#define IS_CONTROL_CONVERT_SIGNED(x) (0 != (CONTROL_CONVERT_SIGNED_OP & x))
#define IS_CONTROL_CONVERT_FLOAT(x) (0 != (CONTROL_CONVERT_FLOAT_OP & x))
// from -> to (CAST_FROM_TO)

InstructionControl operator |(InstructionControl a, InstructionControl b);

engone::Logger& operator <<(engone::Logger& logger, InstructionControl c);

enum InstructionOpcode : u8 {
    // DO NOT REARRAGNE THESE INSTRUCTIONS, instructions_names and instruction_contents depend on the order!
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
    
    BC_ALLOC_LOCAL,      // opcode, op, size16         - allocate local memory (max 64KB)
    BC_FREE_LOCAL,       // opcode,  size16         - allocate args memory (max 64KB)

    BC_ALLOC_ARGS,
    BC_FREE_ARGS,

    // BC_REG_LOCALS, // local pointer (similar to base/frame pointer)

    BC_SET_ARG,   // opcode, op, disp
    BC_GET_PARAM, // opcode, op, disp
    BC_GET_VAL,   // opcode, op, disp
    BC_SET_RET,   // opcode, op, disp
    BC_PTR_TO_LOCALS,
    BC_PTR_TO_PARAMS,

    BC_JMP,
    BC_CALL,
    BC_CALL_REG,
    BC_RET,
    // jump if not zero
    BC_JNZ,
    // jump if zero
    BC_JZ,

    BC_DATAPTR,
    BC_EXT_DATAPTR,
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
    BC_ATOMIC_CMP_SWAP,
    BC_ATOMIC_ADD,

    BC_SQRT,
    BC_ROUND,

    BC_ASM,

    // used when running test cases
    // The stack must be aligned to 16 bytes because
    // there are some functions being called inside which reguire it.
    BC_TEST_VALUE,

    BC_EXTEND1 = 253, // extend opcode by 1 byte
    BC_EXTEND2 = 254, // extend opcode by 2 bytes
    BC_RESERVED = 255,
};
#define ASM_ENCODE_INDEX(ind) (u8)(asmInstanceIndex&0xFF), (u8)((asmInstanceIndex>>8)&0xFF), (u8)((asmInstanceIndex>>16)&0xFF)
#define ASM_DECODE_INDEX(op0,op1,op2) (u32)(op0 | (op1<<8) | (op2<<16))
engone::Logger& operator<<(engone::Logger&, InstructionOpcode);
enum BCRegister : u8 {
    BC_REG_INVALID = 0,
    // general purpose registers
    BC_REG_A, // accumulator
    BC_REG_B,
    BC_REG_C,
    BC_REG_D,
    BC_REG_E,
    BC_REG_F,
    BC_REG_G,
    BC_REG_H,
    BC_REG_I,
    BC_REG_J,
    BC_REG_K,
    BC_REG_L,
    BC_REG_M,
    BC_REG_N,
    BC_REG_O,
    BC_REG_P,
    
    // temporary registers, used in small parts when most other registers are in use
    BC_REG_T0,
    BC_REG_T1,

    BC_REG_LOCALS, // local pointer, can only be used with mov_rm/mr (similar to base/frame pointer)

    BC_REG_MAX,
};
enum InstBaseType : u16 {
    BASE_NONE,
    BASE_op1     = 0x1, // op starts at op1, not op0 in enums
    BASE_op2     = 0x2,
    BASE_op3     = 0x4,
    BASE_imm8    = 0x8,
    BASE_imm16   = 0x10,
    BASE_imm32   = 0x20,
    BASE_imm64   = 0x40,
    BASE_ctrl    = 0x80,
    BASE_link    = 0x100,
    BASE_call    = 0x200,
};
struct BCInstructionInfo {
    int size;
    InstBaseType type;
};
extern BCInstructionInfo instruction_contents[256];
#pragma pack(push)
#pragma pack(1)
struct InstBase {
    InstructionOpcode opcode;
};
struct InstBase_imm16 {
    InstructionOpcode opcode;
    i16 imm16;
};
struct InstBase_imm32 {
    InstructionOpcode opcode;
    i32 imm32;
};
struct InstBase_imm32_imm8_2 {
    InstructionOpcode opcode;
    u8 imm8_0;
    u8 imm8_1;
    i32 imm32;
};
struct InstBase_op1 {
    InstructionOpcode opcode;
    BCRegister op0;
};
struct InstBase_op1_link {
    InstructionOpcode opcode;
    BCRegister op0;
    LinkConvention link;
};
struct InstBase_op1_imm16 {
    InstructionOpcode opcode;
    BCRegister op0;
    i16 imm16;
};
struct InstBase_op1_imm32 {
    InstructionOpcode opcode;
    BCRegister op0;
    i32 imm32;
};
struct InstBase_op1_imm64 {
    InstructionOpcode opcode;
    BCRegister op0;
    i64 imm64;
};
struct InstBase_op1_ctrl_imm16 {
    InstructionOpcode opcode;
    BCRegister op0;
    InstructionControl control;
    i16 imm16;
};
struct InstBase_op2 {
    InstructionOpcode opcode;
    BCRegister op0;
    BCRegister op1;
};
struct InstBase_op2_imm8 {
    InstructionOpcode opcode;
    BCRegister op0;
    BCRegister op2;
    i8 imm8;
};
struct InstBase_op2_ctrl {
    InstructionOpcode opcode;
    BCRegister op0;
    BCRegister op1;
    InstructionControl control;
};
struct InstBase_op2_ctrl_imm16 {
    InstructionOpcode opcode;
    BCRegister op0;
    BCRegister op1;
    InstructionControl control;
    i16 imm16;
};
struct InstBase_op2_ctrl_imm32 {
    InstructionOpcode opcode;
    BCRegister op0;
    BCRegister op1;
    InstructionControl control;
    i32 imm32;
};
struct InstBase_op3 {
    InstructionOpcode opcode;
    union {
        struct {
            BCRegister op0;
            BCRegister op1;
            BCRegister op2;
        };
        BCRegister ops[3];
    };
};
struct InstBase_link_call_imm32 {
    InstructionOpcode opcode;
    LinkConvention link;
    CallConvention call;
    i32 imm32;
};
struct InstBase_op1_link_call {
    InstructionOpcode opcode;
    BCRegister op0;
    LinkConvention link; // not needed, we call a pointer, we don't resolve a symbol so there is only one way to link
    CallConvention call;
};
#pragma pack(pop)

engone::Logger& operator<<(engone::Logger&, BCRegister);

extern const char* control_names[];
extern const char* cast_names[];
extern const char* instruction_names[];
extern const char* register_names[];

#define MISALIGNMENT(X,ALIGNMENT) ((ALIGNMENT - (X) % ALIGNMENT) % ALIGNMENT)

struct BCRelocation {
    int tinycode_index=-1;
    int pc=-1;
    bool valid() { return tinycode_index != -1; }
};
enum ExternalRelocationType : u8{
    BC_REL_FUNCTION,
    BC_REL_GLOBAL_VAR,
};
struct ExternalRelocation {
    std::string name;
    std::string library_path;
    int tinycode_index=0;
    int pc=0;
    
    ExternalRelocationType type = BC_REL_FUNCTION;
};
struct BytecodePrintCache {
    int prev_tinyindex = -1;
    int prev_line = -1;
};
struct Bytecode;
typedef u32 TinyBytecodeID;
// Look at me I'm tiny bytecode! 
struct TinyBytecode {
    std::string name;
    QuickArray<u8> instructionSegment{};
    CallConvention call_convention = CallConvention::BETCALL;
    int index = 0;
    int size() const { return instructionSegment.size(); }
    // debug information
    QuickArray<u32> index_of_lines{};
    struct Line {
        int line_number;
        std::string text;
    };
    DynamicArray<Line> lines{};
    DebugFunction* debugFunction = nullptr;
    DynamicArray<TryBlock> try_blocks{};
    DynamicArray<int> required_asm_instances; // x64 gen needs to know what inline assembly to generate

    // bool is_used_as_function_pointer = false; // used in x64 gen for enabling/disabling callee saved registers

    struct Relocation {
        i32 pc=0; // index of the 32-bit integer to relocate
        FuncImpl* funcImpl = nullptr;
        // std::string func_name;
    };
    DynamicArray<Relocation> call_relocations;
    bool applyRelocations(Bytecode* code);
    // void addRelocation(int pc, const std::string& name) {
    //     call_relocations.add({});
    //     call_relocations.last().pc = pc;
    //     call_relocations.last().func_name = name;
    // }
    void addRelocation(int pc, FuncImpl* impl) {
        call_relocations.add({});
        call_relocations.last().pc = pc;
        call_relocations.last().funcImpl = impl;
    }
    
    // high_index is exclusive
    void print(int low_index, int high_index, Bytecode* code = nullptr, DynamicArray<std::string>* dll_functions = nullptr, bool force_newline = false, BytecodePrintCache* print_cache = nullptr);

    void restore_to_empty() {
        // When I add a new list to tinycode I will forget to reset it in this function.
        // restore_to_empty is only used with compile time execution so as long as the instructions are reset we may be fine.
        instructionSegment.resize(0);
        index_of_lines.resize(0);
        lines.resize(0);
        required_asm_instances.resize(0);
        call_relocations.resize(0);
        try_blocks.resize(0);
    }
};
struct BytecodeLocation {
    TinyBytecodeID id; // id-1 to get index
    u32 address;
};
struct BytecodeRange {
    TinyBytecodeID id; // id-1 to get index
    u32 start_address;
    u32 end_address;
};
struct Bytecode {
    static const i32 BEGIN_DLL_FUNC_INDEX = 0x800'0000;
    ~Bytecode() {
        cleanup();   
    }
    static Bytecode* Create();
    static void Destroy(Bytecode*);
    void cleanup();
    
    TinyBytecode* createTiny(const std::string& name, CallConvention convention) {
        auto ptr = TRACK_ALLOC(TinyBytecode);
        new(ptr)TinyBytecode();
        ptr->call_convention = convention;
        ptr->name = name;
        tinyBytecodes.add(ptr);
        ptr->index = tinyBytecodes.size()-1;
        return ptr;
    }
    
    u32 getMemoryUsage();
    
    TargetPlatform target = {};
    ArchitectureInfo arch = {};

    DynamicArray<TinyBytecode*> tinyBytecodes;
    int index_of_main = -1; // rename to index_of_entry_point?

    QuickArray<u8> dataSegment{};
    engone::Mutex lock_global_data;

    DebugInformation* debugInformation = nullptr;

    // DynamicArray<std::string> linkDirectives;

    // Assembly or bytecode dump after the compilation is done.
    struct Dump {
        bool dumpBytecode = false;
        bool dumpAsm = false;
        int tinycode_index = -1;
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
        bool generated = false;
        
        u32 lineStart = 0;
        u32 lineEnd = 0;
        std::string file;
        
        struct ExternalNamedReloc {
            std::string name; // name of function/symbol
            u32 textOffset; // where to modify
        };
        DynamicArray<ExternalNamedReloc> relocations{}; // comes from prepare_assembly
    };
    DynamicArray<ASM> asmInstances;
    int add_assembly(const char* text, int len, const std::string& file, int line_start, int line_end);

    // usually a function like main
    struct ExportedFunction {
        std::string name;
        int tinycode_index = 0;
    };
    DynamicArray<ExportedFunction> exportedFunctions;
    // returns false if a symbol with 'name' has been exported already
    bool addExportedFunction(const std::string& name, int tinycode_index, int* existing_tinycode_index = nullptr);

    
    // Relocation for external functions
    DynamicArray<ExternalRelocation> externalRelocations;
    void addExternalRelocation(const std::string& name,const std::string& library_path, int tinycode_index, int pc, ExternalRelocationType rel_type);

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

    // returns an offset relative to the beginning of the data segment where data was placed.
    // data may be nullptr which will give you space with zero-initialized data.
    int appendData(const void* data, int size);
    void ensureAlignmentInData(int alignment);
    // returns offset into data, also sets out_ptr which is a DANGEROUS pointer that may be invalidated.
    // write to it before calling appendData again!

    // int appendData_late(void** out_ptr, int size);

    bool removeLast();
    void printStats();
    void print();
};
    
struct BytecodeBuilder {
    Compiler* compiler = nullptr;
    Bytecode* code=nullptr;
    TinyBytecode* tinycode=nullptr;
    
    void init(Bytecode* code, TinyBytecode* tinycode, Compiler* compiler);
    
    void emit_push(BCRegister reg);
    void emit_pop(BCRegister reg);
    void emit_li32(BCRegister reg, i32 imm);
    void emit_li64(BCRegister reg, i64 imm);
    // calls li32 or li64 based on size
    void emit_li(BCRegister reg, i64 imm, int size);
    
    void emit_incr(BCRegister reg, i32 imm);

    // allocates space on the stack for local variables
    // reg may be invalid
    void emit_alloc_local(BCRegister reg, u16 size);
    void emit_free_local(u16 size);
    void emit_free_local(int* index_to_size);
    void emit_alloc_local(BCRegister reg, int* index_to_size);
    void fix_local_imm(int index, u16 size);
    // allocates space on stack for arguments but ensures 16-byte alignment DURING EXECUTION or final x64 gen
    void emit_alloc_args(BCRegister reg, u16 size);
    void emit_empty_alloc_args(int* out_size);
    void fix_alloc_args(int index, u16 size);
    void emit_free_args(u16 size);

    void emit_set_arg  (BCRegister reg, i16 imm, int size, bool is_float, bool is_signed = true);
    void emit_get_param(BCRegister reg, i16 imm, int size, bool is_float, bool is_signed = true);
    void emit_set_ret  (BCRegister reg, i16 imm, int size, bool is_float, bool is_signed = true);
    void emit_get_val  (BCRegister reg, i16 imm, int size, bool is_float, bool is_signed = true); // get return value
    
    void emit_ptr_to_locals(BCRegister reg, int imm16);
    void emit_ptr_to_params(BCRegister reg, int imm16);

    void emit_call(LinkConvention l, CallConvention c, i32* index_of_relocation, i32 imm = 0);
    void emit_call_reg(BCRegister reg, LinkConvention l, CallConvention c);
    void emit_ret();
    
    void emit_jmp(int pc);
    void emit_jmp(int* out_imm_offset);
    void emit_jz (BCRegister reg, int pc);
    void emit_jz (BCRegister reg, int* out_imm_offset);
    void emit_jnz(BCRegister reg, int pc);
    void emit_jnz(BCRegister reg, int* out_imm_offset);
    
    void emit_mov_rr(BCRegister to, BCRegister from);
    void emit_mov_rm(BCRegister to, BCRegister from, int size);
    void emit_mov_mr(BCRegister to, BCRegister from, int size);
    void emit_mov_rm_disp(BCRegister to, BCRegister from, int size, int displacement);
    void emit_mov_mr_disp(BCRegister to, BCRegister from, int size, int displacement);
    
    void emit_add(BCRegister to, BCRegister from, int size, bool is_float, bool is_signed = true);
    void emit_sub(BCRegister to, BCRegister from, int size, bool is_float, bool is_signed = true);
    void emit_mul(BCRegister to, BCRegister from, int size, bool is_float, bool is_signed);
    void emit_div(BCRegister to, BCRegister from, int size, bool is_float, bool is_signed);
    void emit_mod(BCRegister to, BCRegister from, int size, bool is_float, bool is_signed);
    // This cannot be a float operation
    void emit_add_imm32(BCRegister to, BCRegister from, int imm, int size);

    void emit_band   (BCRegister to, BCRegister from, int size);
    void emit_bor    (BCRegister to, BCRegister from, int size);
    void emit_bxor   (BCRegister to, BCRegister from, int size);
    void emit_bnot   (BCRegister to, BCRegister from, int size);
    void emit_blshift(BCRegister to, BCRegister from, int size);
    void emit_brshift(BCRegister to, BCRegister from, int size);
    
    void emit_eq    (BCRegister to, BCRegister from, int size, bool is_float);
    void emit_neq   (BCRegister to, BCRegister from, int size, bool is_float);
    void emit_lt    (BCRegister to, BCRegister from, int size, bool is_float, bool is_signed);
    void emit_lte   (BCRegister to, BCRegister from, int size, bool is_float, bool is_signed);
    void emit_gt    (BCRegister to, BCRegister from, int size, bool is_float, bool is_signed);
    void emit_gte   (BCRegister to, BCRegister from, int size, bool is_float, bool is_signed);

    void emit_land(BCRegister to, BCRegister from, int size);
    void emit_lor (BCRegister to, BCRegister from, int size);
    void emit_lnot(BCRegister to, BCRegister from, int size);
    
    void emit_dataptr(BCRegister reg, i32 imm);
    void emit_ext_dataptr(BCRegister reg, LinkConvention link);
    void emit_codeptr(BCRegister reg, i32 imm);
    
    void emit_cast(BCRegister to, BCRegister from, InstructionControl control, u8 from_size, u8 to_size);
    
    void emit_asm(int asm_instance, int inputs, int outputs);
    void emit_fake_push(); // useful with inline assembly
    void emit_fake_pop(); // useful with inline assembly

    void emit_memzero(BCRegister dst, BCRegister size_reg, u8 batch);
    void emit_memcpy(BCRegister dst, BCRegister src, BCRegister size_reg);
    void emit_strlen(BCRegister len_reg, BCRegister src_reg);
    void emit_rdtsc(BCRegister to);
    
    void emit_atomic_cmp_swap(BCRegister ptr_dst, BCRegister old, BCRegister new_reg);
    void emit_atomic_add(BCRegister to, BCRegister from, InstructionControl control);
    
    void emit_sqrt(BCRegister to);
    void emit_round(BCRegister to);
    // TODO: double version of sqrt, round...
    // TODO: sin, cos, tan...
    
    void emit_test(BCRegister to, BCRegister from, u8 size, i32 test_location);
    
    // virual stack pointer is affected by push and pop
    int get_virtual_sp() { return virtual_stack_pointer; }
    // 16-byte alignment
    // int get_required_alignment() {
    //     // return (16 - (virtual_stack_pointer % 16)) % 16;
    //     return (0x10 - virtual_stack_pointer) & 0xF;
    // }

    int get_pc() { return tinycode->instructionSegment.size(); }
    void fix_jump_imm32_here(int imm_index);
    
    InstructionOpcode get_last_opcode() {
        int i = get_index_of_previous_instruction();
        if(i==-1)
            return (InstructionOpcode)0; 
        return (InstructionOpcode)tinycode->instructionSegment[i];
    }

    void push_line(int line, const std::string& text) {
        tinycode->lines.add({line, text});
    }

    // returns whether enabled previously
    bool disable_builder(bool yes) { bool tmp = disable_code_gen; disable_code_gen = yes; return tmp; }
    
    BCRelocation get_relocation(int offset) {
        return { tinycode->index, (int)tinycode->instructionSegment.size() + offset };
    }

private:
    // building blocks for every instruction
    void emit_opcode(InstructionOpcode type);
    void emit_operand(BCRegister reg);
    void emit_control(InstructionControl control);
    void emit_imm8(i8 imm);
    void emit_imm16(i16 imm);
    void emit_imm32(i32 imm);
    void emit_imm64(i64 imm);
    
    // call the functions, don't access the fields directly
    static const int PREVIOUS_INSTRUCTIONS_MAX = 5;
    int index_of_previous_instructions[PREVIOUS_INSTRUCTIONS_MAX]; // circular buffer
    int previous_instructions_head = 0; // points to next index to use
    int previous_instructions_count = 0;

    bool disable_code_gen = false;

    // off = 0 means previous, off = 1 means the previous previous inst. and so on.
    // returns -1 if out of bounds
    int get_index_of_previous_instruction(int off = 0) {
        if(off >= previous_instructions_count)
            return -1;
        return index_of_previous_instructions[(previous_instructions_head - 1 - off + PREVIOUS_INSTRUCTIONS_MAX) % PREVIOUS_INSTRUCTIONS_MAX];
    }
    void append_previous_instruction(int index) {
        index_of_previous_instructions[previous_instructions_head] = index;
        previous_instructions_head = (previous_instructions_head + 1) % PREVIOUS_INSTRUCTIONS_MAX;
        if(previous_instructions_count < PREVIOUS_INSTRUCTIONS_MAX)
            previous_instructions_count++;
    }
    void remove_previous_instruction() {
        Assert(previous_instructions_count > 0); // can't remove if there's nothing there
        int ind = index_of_previous_instructions[(previous_instructions_head - 1 + PREVIOUS_INSTRUCTIONS_MAX) % PREVIOUS_INSTRUCTIONS_MAX];
        if(tinycode->instructionSegment[ind] == BC_PUSH) {
            pushed_offset += 8;
            virtual_stack_pointer += 8;
        } else Assert(false); // TODO: Handle remove logic for other instructions
        previous_instructions_count--;
        previous_instructions_head = (previous_instructions_head - 1 + PREVIOUS_INSTRUCTIONS_MAX) % PREVIOUS_INSTRUCTIONS_MAX;
    }

    // used to detect bc_push overwriting values accessed by bc_get_val
    int pushed_offset = 0; // grows down
    int pushed_offset_max = 0; // grows down
    int ret_offset = 0; // grows down
    bool has_return_values = false;

    int virtual_stack_pointer = 0; // needed to ensure 16-byte alignment on function calls
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