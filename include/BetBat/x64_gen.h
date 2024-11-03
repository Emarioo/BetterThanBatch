#pragma once

#include "BetBat/Bytecode.h"
#include "BetBat/Program.h"
#include "BetBat/Util/StringBuilder.h"
#include "BetBat/CompilerOptions.h"

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
struct Compiler;

enum X64Register : u8 {
    X64_REG_INVALID = 0,
    
    X64_REG_A, // DO NOT CHANGE THE ORDER OF THE REGISTERS! intel manual specifies this specific order!
    X64_REG_BEGIN = X64_REG_A,
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

struct X64Operand {
    X64Register reg{};
    bool on_stack = false;

    // meaning no register/value is set
    bool invalid() const { return reg == X64_REG_INVALID && !on_stack; }
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

struct X64Builder : public ProgramBuilder {
    struct RegisterInfo {
        bool used = false;
        int artifical_reg=0;
    };
    std::unordered_map<X64Register, RegisterInfo> registers;
    
    X64Register alloc_register(int artifical, X64Register reg = X64_REG_INVALID, bool is_float = false, bool is_signed = false);
    bool is_register_free(X64Register reg);
    void free_register(X64Register reg);
    void free_all_registers();
    
    void emit_modrm(u8 mod, X64Register reg, X64Register rm);
    void emit_modrm_slash(u8 mod, u8 reg, X64Register rm);
    // RIP-relative addressing
    void emit_modrm_rip32(X64Register reg, u32 disp32);
    void emit_modrm_rip32_slash(u8 reg, u32 disp32);
    void emit_modrm_sib(u8 mod, X64Register reg, u8 scale, u8 index, X64Register base_reg);
    void emit_modrm_sib_slash(u8 mod, u8 reg, u8 scale, u8 index, X64Register base_reg);

    // These are here to prevent implicit casting
    // of arguments which causes mistakes.
    void emit_modrm_rip32(X64Register, i64);
    void emit_modrm_rip32_slash(u64 reg, i64);
    void emit_modrm_slash(u8 mod, X64Register reg, X64Register rm);
    void emit_modrm_sib_slash(u8 mod, X64Register reg, u8 scale, u8 index, X64Register base_reg);
    
    // void fix_jump_imm8(u32 offset, u8 value);
    void emit_jmp_imm8(u32 offset);
    void fix_jump_here_imm8(u32 offset);
    void set_imm32(u32 offset, u32 value);
    void set_imm8(u32 offset, u8 value);
    
    // float registers require size, general registers will always use REXW
    void emit_mov_reg_reg(X64Register reg, X64Register rm, int size = 0);
    void emit_mov_reg_mem(X64Register reg, X64Register rm, InstructionControl control, int disp32);
    void emit_mov_mem_reg(X64Register rm, X64Register reg, InstructionControl control, int disp32);
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
    DynamicArray<int> push_offsets{}; // used when set arguments while values are pushed and popped
    int ret_offset = 0;
    int callee_saved_space = 0;
    bool disable_modrm_asserts = false;

    struct Arg {
        InstructionControl control = CONTROL_NONE;
        int offset_from_rbp = 0;
    };
    DynamicArray<Arg> recent_set_args;

    bool generate();

    static const X64Register RESERVED_REG0 = X64_REG_DI; // You cannot change these because code in x64_gen assume that other registers are available and that DI and SI are reserved
    static const X64Register RESERVED_REG1 = X64_REG_SI;
    static const X64Register RESERVED_REG2 = X64_REG_R11;
    
    bool prepare_assembly(Bytecode::ASM& asmInst);

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
        bool is_signed = false;
        u8 size = 0;
        int started_by_bc_index = false; // responsible for freeing register
        
        bool freed = false;

        void reset() {
            reg = X64_REG_INVALID;
            floaty = false;
            is_signed = false;
            size = 0;
        }
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
        artificalRegisters.last().floaty = false;
        artificalRegisters.last().is_signed = false;
        artificalRegisters.last().size = 0;
        artificalRegisters.last().freed = 0;

        return artificalRegisters.size()-1;
    }
    void suggest_artifical(int id, X64Register reg) {
        artificalRegisters[id].reg = reg;
    }
    void suggest_artifical_float(int id) {
        auto& reg = artificalRegisters[id];
        Assert(!reg.is_signed); // can't be both signed and float
        reg.floaty = true;
    }
    void suggest_artifical_signed(int id) {
        auto& reg = artificalRegisters[id];
        Assert(!reg.floaty); // can't be both float and signed
        reg.is_signed = true;
    }
    void suggest_artifical_size(int id, u8 size) {
        auto& reg = artificalRegisters[id];
        // artificalRegisters[id].floaty = true;
        Assert(reg.size == 0 || reg.size == size); // suggesting contradictory sizes indicates a bug
        reg.size = size;
    }
    ArtificalValue* get_and_alloc_artifical_reg(int id) {
        auto& reg = artificalRegisters[id];
        lock_register_resize = true;
        if(reg.reg == X64_REG_INVALID) {
            reg.reg = alloc_register(id, X64_REG_INVALID, reg.floaty);
            Assert(reg.reg != X64_REG_INVALID);
        }
        return &reg;
    }
    ArtificalValue* get_artifical_reg(int id) {
        lock_register_resize = true;
        // if(artificalRegisters[id].reg == X64_REG_INVALID) {
        //     artificalRegisters[id].reg = alloc_register(X64_REG_INVALID, artificalRegisters[id].floaty);
        //     Assert(artificalRegisters[id].reg != X64_REG_INVALID);
        // }
        return &artificalRegisters[id];
    }
    void free_artifical(int id) {
        auto reg = &artificalRegisters[id];
        free_register(reg->reg);
        reg->reg = X64_REG_INVALID;
        reg->freed = true;
    }

    void clear_register_map() {
        memset((void*)bc_register_map, 0, sizeof (bc_register_map));
    }
    
    void map_reg(X64Inst* n, int nr) {
        InstBase_op3* base = (InstBase_op3*)n->base;
        
        Assert(nr != 0 || (instruction_contents[base->opcode].type & BASE_op1));
        Assert(nr != 1 || (instruction_contents[base->opcode].type & BASE_op2));
        Assert(nr != 2 || (instruction_contents[base->opcode].type & BASE_op3));
        
        bc_register_map[base->ops[nr]].used_by = n;
        bc_register_map[base->ops[nr]].reg_nr = nr;
    }
    void free_map_reg(X64Inst* n, int nr) {
        InstBase_op3* base = (InstBase_op3*)n->base;
        
        Assert(nr != 0 || (instruction_contents[base->opcode].type & BASE_op1));
        Assert(nr != 1 || (instruction_contents[base->opcode].type & BASE_op2));
        Assert(nr != 2 || (instruction_contents[base->opcode].type & BASE_op3));
        
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


bool GenerateX64(Compiler* compiler, TinyBytecode* tinycode);
