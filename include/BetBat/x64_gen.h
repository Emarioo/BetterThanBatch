#pragma once

#include "BetBat/Bytecode.h"
#include "BetBat/Util/StringBuilder.h"
#include "BetBat/CompilerEnums.h"

/*
Common mistakes

x64 program requires this:
    stack pointer end with the value it began with
    ret must exist at the end

-1073741819 as error code means memory access violation
you may have forgotten ret or messed up the stack pointer

using a macro or some other function when converting bytecode operands to
x64 operands

*/

enum X64Register : u8 {
    X64_REG_INVALID = 0,
    
    X64_REG_BEGIN,
    X64_REG_A = X64_REG_BEGIN, // DO NOT CHANGE THE ORDER OF THE REGISTERS! intel manual specifies this specific order!
    X64_REG_C,
    X64_REG_D,
    X64_REG_B,
    X64_REG_SP,
    X64_REG_BP,
    X64_REG_SI,
    X64_REG_DI,

    X64_REG_R8, 
    X64_REG_R9, 
    X64_REG_R10,
    X64_REG_R11,
    X64_REG_R12,
    X64_REG_R13,
    X64_REG_R14,
    X64_REG_R15,

    X64_REG_XMM0,
    X64_REG_XMM1,
    X64_REG_XMM2,
    X64_REG_XMM3,
    X64_REG_XMM4,
    X64_REG_XMM5,
    X64_REG_XMM6,
    X64_REG_XMM7,
    X64_REG_END,
};
extern const char* x64_register_names[];
engone::Logger& operator <<(engone::Logger&, X64Register);
// NOTE: XMM8-15 requires special handling with prefix I think? Similar to extended general registers with R8-R15
#define IS_REG_XMM(R) (R >= X64_REG_XMM0 && R <= X64_REG_XMM7)
#define IS_REG_EXTENDED(R) (R >= X64_REG_R8 && R <= X64_REG_R15)
#define IS_REG_NORM(R) (R >= X64_REG_A && R <= X64_REG_DI)

#define CLAMP_XMM(R) (Assert(IS_REG_XMM(R)), (X64Register)(((R-1) & 7) + 1))
#define CLAMP_EXT_REG(R) (Assert(IS_REG_EXTENDED(R)||IS_REG_NORM(R)), (X64Register)(((R-1) & 7) + 1))

struct X64TinyProgram {
    u8* text=nullptr;
    u64 _allocationSize=0;
    u64 head=0;

    // We use InternalFuncRelocation instead of this
    // struct TinyProgramRelocation {
    //     int text_offset; // offset within this program
    //     int tinyprogram_index; // index to other program
    // };
    // DynamicArray<TinyProgramRelocation> relocations;
};
struct X64Program {
    ~X64Program(){
        TRACK_ARRAY_FREE(globalData, u8, globalSize);
        // engone::Free(globalData, globalSize);
        dataRelocations.cleanup();
        namedUndefinedRelocations.cleanup();

        // compiler->program borrows debugInformation from compiler->code
        // and it is unclear who should destroy it so we don't.
        // if(debugInformation) {
        //     DebugInformation::Destroy(debugInformation);
        //     debugInformation = nullptr;
        // }
    }
    
    DynamicArray<X64TinyProgram*> tinyPrograms;
    X64TinyProgram* createProgram(int requested_index) {
        if(tinyPrograms.size() <= requested_index) {
            tinyPrograms.resize(requested_index+1);
        }

        auto ptr = TRACK_ALLOC(X64TinyProgram);
        new(ptr)X64TinyProgram();
        tinyPrograms[requested_index] = ptr;
        // tinyPrograms.add(ptr);
        return ptr;
    }
    
    // u64 size() const { return head; }

    u8* globalData = nullptr;
    u64 globalSize = 0;

    int index_of_main = 0;

    struct DataRelocation {
        u32 dataOffset; // offset in data segment
        u32 textOffset; // where to modify       
        i32 tinyprog_index; 
    };
    // struct PtrDataRelocation {
    //     u32 referer_dataOffset;
    //     u32 target_dataOffset;
    // };
    struct NamedUndefinedRelocation {
        std::string name; // name of symbol
        u32 textOffset; // where to modify
        i32 tinyprog_index;
    };
    // exported functions
    struct ExportedSymbol {
        std::string name; // name of symbol
        // u32 textOffset; // where to modify?
        i32 tinyprog_index;
    };
    struct InternalFuncRelocation {
        i32 from_tinyprog_index;
        u32 textOffset;
        i32 to_tinyprog_index;
    };

    // DynamicArray<PtrDataRelocation> ptrDataRelocations;
    DynamicArray<DataRelocation> dataRelocations;
    DynamicArray<ExportedSymbol> exportedSymbols;
    DynamicArray<NamedUndefinedRelocation> namedUndefinedRelocations;
    DynamicArray<InternalFuncRelocation> internalFuncRelocations;

    void addDataRelocation(u32 dataOffset, u32 textOffset, i32 tinyprog_index) {
        dataRelocations.add({dataOffset, textOffset, tinyprog_index});
    }
    void addNamedUndefinedRelocation(const std::string& name, u32 textOffset, i32 tinyprog_index) {
        namedUndefinedRelocations.add({name, textOffset, tinyprog_index});
    }
    void addExportedSymbol(const std::string& name, i32 tinyprog_index) {
        exportedSymbols.add({name, tinyprog_index});
    }
    void addInternalFuncRelocation(i32 from_func, u32 text_offset, i32 to_func) {
        internalFuncRelocations.add({from_func, text_offset, to_func});
    }


    DebugInformation* debugInformation = nullptr;

    void printHex(const char* path = nullptr);
    void printAsm(const char* path, const char* objpath = nullptr);

    static void Destroy(X64Program* program);
    static X64Program* Create();

    // void generateFromTinycode(Bytecode* code, TinyBytecode* tinycode);

    // static X64Program* ConvertFromBytecode(Bytecode* code);
private:
    
};

struct X64Env;
// This structure is WAY to large, how to minimize it?
struct OPNode {
    OPNode(u32 bc_index, InstructionOpcode type) : bc_index(bc_index), opcode(type) {}
    u32 bc_index = 0;
    InstructionOpcode opcode = BC_HALT;
    i64 imm = 0;
    BCRegister op0 = BC_REG_INVALID;
    BCRegister op1 = BC_REG_INVALID;
    BCRegister op2 = BC_REG_INVALID;
    
    // TODO: Union on these?
    InstructionControl control = CONTROL_NONE;
    
    LinkConventions link = LinkConventions::NONE;
    CallConventions call = CallConventions::BETCALL;
    
    OPNode* in0 = nullptr;
    OPNode* in1 = nullptr;
    OPNode* in2 = nullptr;

    X64Env* computed_env = nullptr;
};

struct X64Operand {
    X64Register reg{};
    bool on_stack = false;

    // meaning no register/value is set
    bool invalid() const { return reg == X64_REG_INVALID && !on_stack; }
};
struct X64Env {
    OPNode* node=nullptr;

    X64Env* env_in0=nullptr;
    X64Env* env_in1=nullptr;
    X64Env* env_in2=nullptr;

    X64Operand reg0{};
    X64Operand reg1{};
    X64Operand reg2{};
};
typedef int ArtificalRegister;
struct X64Inst {
    // int id=0;
    u32 bc_index = 0;

    union {
        struct {
            ArtificalRegister reg0;
            ArtificalRegister reg1;
            ArtificalRegister reg2;
        };
        ArtificalRegister regs[3]{};
    };
    InstBase* base = nullptr;
};
engone::Logger& operator<<(engone::Logger& l, X64Inst& i);

struct X64Builder {
    X64Program* prog = nullptr;
    X64TinyProgram* tinyprog = nullptr;
    i32 current_tinyprog_index = 0;
    Bytecode* bytecode = nullptr;
    TinyBytecode* tinycode = nullptr;
    
    QuickArray<OPNode*> nodes;
    
    // QuickArray<u32> instruction_indices;
    
    DynamicArray<OPNode*> all_nodes; // TODO: Bucket array
    OPNode* createNode(u32 bc_index, InstructionOpcode opcode) {
        auto ptr = TRACK_ALLOC(OPNode);
        new(ptr)OPNode(bc_index, opcode);
        all_nodes.add(ptr);
        return ptr;
    }
    void destroyNode(OPNode* node) {
        node->~OPNode();
        TRACK_FREE(node, OPNode);
    }
    
    // struct ValueLocation {
    //     enum Kind {
    //         NONE,
    //         IN_REGISTER,
    //         IN_STACK,
    //     };
    //     Kind kind = NONE;
    //     int reg; // which register
    //     int stack_offset; // where in stack
    // };
    // std::unordered_map<BCRegister, ValueLocation*> valueLocations;
    
    struct RegisterInfo {
        bool used = false;
        
        // OPNode* node = nullptr;
    };
    std::unordered_map<X64Register, RegisterInfo> registers;
    
    std::unordered_map<BCRegister, OPNode*> reg_values;
    
    // void set_reg_value(BCRegister reg, OPNode* n) {

    // }
    // void get_reg_value() {

    // }
    
    QuickArray<OPNode*> stack_values;
    
    X64Register alloc_register(X64Register reg = X64_REG_INVALID, bool is_float = false);
    bool is_register_free(X64Register reg);
    void free_register(X64Register reg);
    void free_all_registers();
    
    int code_size() const { return tinyprog->head; }
    void ensure_bytes(int size) {
        if(tinyprog->head + size >= tinyprog->_allocationSize ){
            bool yes = _reserve(tinyprog->_allocationSize * 2 + 50 + size);
            Assert(yes);
        }
    }

    void init(X64Program* p) {
        prog = p;
    }

    void emit1(u8 byte);
    void emit2(u16 word);
    void emit3(u32 word);
    void emit4(u32 dword);
    void emit8(u64 dword);
    void emit_modrm(u8 mod, X64Register reg, X64Register rm);
    void emit_modrm_slash(u8 mod, u8 reg, X64Register rm);
    // RIP-relative addressing
    void emit_modrm_rip32(X64Register reg, u32 disp32);
    void emit_modrm_sib(u8 mod, X64Register reg, u8 scale, u8 index, X64Register base_reg);
    void emit_modrm_sib_slash(u8 mod, u8 reg, u8 scale, u8 index, X64Register base_reg);

    void emit_bytes(const u8* arr, u64 len);

    // These are here to prevent implicit casting
    // of arguments which causes mistakes.
    void emit1(i64 _);
    void emit2(i64 _);
    void emit3(i64 _);
    void emit4(i64 _);
    void emit8(i8 _);
    void emit_modrm_rip32(X64Register, i64);
    void emit_modrm_slash(u8 mod, X64Register reg, X64Register rm);
    void emit_modrm_sib_slash(u8 mod, X64Register reg, u8 scale, u8 index, X64Register base_reg);
    
    // void fix_jump_imm8(u32 offset, u8 value);
    void emit_jmp_imm8(u32 offset);
    void fix_jump_here_imm8(u32 offset);
    void set_imm32(u32 offset, u32 value);
    void set_imm8(u32 offset, u8 value);
    
    bool _reserve(u32 size);

    // float registers require size, general registers will always use REXW
    void emit_mov_reg_reg(X64Register reg, X64Register rm, int size = 0);
    void emit_mov_reg_mem(X64Register reg, X64Register rm, InstructionControl control, int disp32);
    void emit_mov_mem_reg(X64Register reg, X64Register rm, InstructionControl control, int disp32);
    // always sign extends TO 64-bit but may extend FROM 8/16/32/64 bit based on control
    void emit_movsx(X64Register reg, X64Register rm, InstructionControl control);
    void emit_movzx(X64Register reg, X64Register rm, InstructionControl control);

    // only emits if non-zero
    void emit_prefix(u8 inherited_prefix, X64Register reg, X64Register rm);
    u8 construct_prefix(u8 inherited_prefix, X64Register reg, X64Register rm);
    void emit_push(X64Register reg, int size = 0);
    void emit_pop(X64Register reg, int size = 0);
    // REXW prefixed
    void emit_add_imm32(X64Register reg, i32 imm32);
    // REXW prefixed
    void emit_sub_imm32(X64Register reg, i32 imm32);

    // don't assume all opcodes will work
    void emit_operation(u8 opcode, X64Register reg, X64Register rm, InstructionControl control);

    static const int FRAME_SIZE = 16;
    int push_offset = 0; // used when set arguments while values are pushed and popped
    int ret_offset = 0;

    OPNode* last_call = nullptr;

    struct Arg {
        InstructionControl control = CONTROL_NONE;
    };
    DynamicArray<Arg> recent_set_args;

    // void generateFromTinycode(Bytecode* code, TinyBytecode* tinycode);
    void generateFromTinycode_v2(Bytecode* code, TinyBytecode* tinycode);

    // void generateInstructions_slow();

    int get_node_depth(OPNode* n) {
        if(!n) return 0;
        
        int a = get_node_depth(n->in0);
        int b = get_node_depth(n->in1);

        if (a > b)
            return a + 1;
        return b + 1;
    }
    
    static const X64Register RESERVED_REG0 = X64_REG_DI; // You cannot change these because code in x64_gen assume that other registers are available and that DI and SI are reserved
    static const X64Register RESERVED_REG1 = X64_REG_SI;
    static const X64Register RESERVED_REG2 = X64_REG_R11;
    
private:
    // recursively
    // void generateInstructions(int depth = 0, BCRegister find_reg = BC_REG_INVALID, int inst_index = 0, X64Register* out_reg = nullptr);
    
public:
    // experimental

    DynamicArray<X64Inst*> inst_list;
    X64Inst* last_inst_call = nullptr;
    
    struct ValueUsage {
        X64Inst* used_by = nullptr;
        int reg_nr = 0;
    };
    ValueUsage bc_register_map[BC_REG_MAX]{0};
    DynamicArray<ValueUsage> bc_push_list{};
    struct ArtificalValue { // artifical register/value
        X64Register reg = X64_REG_INVALID; // may be invalid until it's time to generate x64, or some instruction could suggest a register to reserve
        // bool stacked = false;
        bool floaty = false;
        u8 size = 0;
        int started_by_bc_index = false; // responsible for freeing register

    };
    DynamicArray<ArtificalValue> artificalRegisters{};
    // int alloc_artifical_stack() {
    //     artificalRegisters.add({});
    //     artificalRegisters.last().stacked = true;
    //     return artificalRegisters.size()-1;
    // }
    bool lock_register_resize = false;
    int alloc_artifical_reg(int bc_index, X64Register reg = X64_REG_INVALID) {
        Assert(!lock_register_resize); // make sure we don't invalidate pointers
        artificalRegisters.add({});
        if(artificalRegisters.size() == 1) // first id is invalid so we add an extra
            artificalRegisters.add({});

        Assert(reg == X64_REG_BP || reg == X64_REG_INVALID);
        // TODO: Don't alloc new register, we can reuse artifical register for BP.
        artificalRegisters.last().reg = reg;
        artificalRegisters.last().started_by_bc_index = bc_index;
        return artificalRegisters.size()-1;
    }
    void suggest_artifical(int id, X64Register reg) {
        artificalRegisters[id].reg = reg;
    }
    void suggest_artifical_float(int id) {
        artificalRegisters[id].floaty = true;
    }
    void suggest_artifical_size(int id, u8 size) {
        artificalRegisters[id].floaty = true;
        Assert(artificalRegisters[id].size == 0 || artificalRegisters[id].size == size); // suggesting contradictory sizes indicates a bug
        artificalRegisters[id].size = size;
    }
    ArtificalValue* get_and_alloc_artifical_reg(int id) {
        lock_register_resize = true;
        if(artificalRegisters[id].reg == X64_REG_INVALID) {
            artificalRegisters[id].reg = alloc_register(X64_REG_INVALID, artificalRegisters[id].floaty);
            Assert(artificalRegisters[id].reg != X64_REG_INVALID);
        }
        return &artificalRegisters[id];
    }
    ArtificalValue* get_artifical_reg(int id) {
        lock_register_resize = true;
        // if(artificalRegisters[id].reg == X64_REG_INVALID) {
        //     artificalRegisters[id].reg = alloc_register(X64_REG_INVALID, artificalRegisters[id].floaty);
        //     Assert(artificalRegisters[id].reg != X64_REG_INVALID);
        // }
        return &artificalRegisters[id];
    }

    void clear_register_map() {
        memset(bc_register_map, 0, sizeof bc_register_map);
    }
    
    void map_reg(X64Inst* n, int nr) {
        InstBase_op3* base = (InstBase_op3*)n->base;
        
        Assert(nr != 0 || (instruction_contents[base->opcode] & BASE_op1));
        Assert(nr != 1 || (instruction_contents[base->opcode] & BASE_op2));
        Assert(nr != 2 || (instruction_contents[base->opcode] & BASE_op3));
        
        bc_register_map[base->ops[nr]].used_by = n;
        bc_register_map[base->ops[nr]].reg_nr = nr;
    }
    void free_map_reg(X64Inst* n, int nr) {
        InstBase_op3* base = (InstBase_op3*)n->base;
        
        Assert(nr != 0 || (instruction_contents[base->opcode] & BASE_op1));
        Assert(nr != 1 || (instruction_contents[base->opcode] & BASE_op2));
        Assert(nr != 2 || (instruction_contents[base->opcode] & BASE_op3));
        
        bc_register_map[base->ops[nr]].used_by = nullptr;
        bc_register_map[base->ops[nr]].reg_nr = 0;
    }

    X64Inst* createInst(InstructionOpcode opcode) {
        auto ptr = new X64Inst();
        // ptr->id = inst_id++;
        // ptr->opcode = opcode;
        return ptr;
    }
    void insert_inst(X64Inst* inst) {
        inst_list.add(inst);
    }

};

// X64Program* ConvertTox64(Bytecode* bytecode);
// The function will print the reformatted content if outBuffer is null
void ReformatDumpbinAsm(LinkerChoice linker, QuickArray<char>& inBuffer, QuickArray<char>* outBuffer, bool includeBytes);

struct Compiler;
void GenerateX64(Compiler* compiler, TinyBytecode* tinycode);

// Process some final things such as exports, symbols from bytecode program to the x64 program
void GenerateX64_finalize(Compiler* compiler);