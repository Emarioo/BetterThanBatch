#pragma once

#include "BetBat/Program.h"
#include "BetBat/Bytecode.h"
#include "BetBat/arm_defs.h"

struct Compiler;


enum ARMRegister : u8 {
    ARM_REG_INVALID = 0,
    ARM_REG_R0,
    ARM_REG_R1,
    ARM_REG_R2,
    ARM_REG_R3,
    ARM_REG_R4,
    ARM_REG_R5,
    ARM_REG_R6,
    ARM_REG_R7,
    ARM_REG_R8,
    ARM_REG_R9,
    ARM_REG_R10,
    
    ARM_REG_R11,
    ARM_REG_GENERAL_MAX = ARM_REG_R11,
    ARM_REG_R12,
    ARM_REG_R13,
    ARM_REG_R14,
    ARM_REG_R15,
    
    ARM_REG_MAX,
    
    // aliases
    ARM_REG_FP = ARM_REG_R11,
    ARM_REG_IP = ARM_REG_R12,
    ARM_REG_SP = ARM_REG_R13,
    ARM_REG_LR = ARM_REG_R14,
    ARM_REG_PC = ARM_REG_R15,
    
};

extern const char* arm_register_names[];
engone::Logger& operator <<(engone::Logger&, ARMRegister);

struct ARMBuilder : public ProgramBuilder {
    
    int FRAME_SIZE = 8; // @TODO: Should be 16 bytes on aarch64
    int REGISTER_SIZE = 4; // @TODO: 8 bytes on aarch64
    
    bool generate();
    
    void emit_add(ARMRegister rd, ARMRegister rn, ARMRegister rm);
    void emit_add_imm(ARMRegister rd, ARMRegister rn, int imm, int cond = ARM_COND_AL);
    void emit_sub(ARMRegister rd, ARMRegister rn, ARMRegister rm);
    void emit_sub_imm(ARMRegister rd, ARMRegister rn, int imm);
    void emit_smull(ARMRegister rdlo, ARMRegister rdhi, ARMRegister rn, ARMRegister rm);
    void emit_umull(ARMRegister rdlo, ARMRegister rdhi, ARMRegister rn, ARMRegister rm);
    void emit_sdiv(ARMRegister rd, ARMRegister rn, ARMRegister rm);
    void emit_udiv(ARMRegister rd, ARMRegister rn, ARMRegister rm);
    void emit_and(ARMRegister rd, ARMRegister rn, ARMRegister rm, int cond = ARM_COND_AL);
    void emit_and_imm(ARMRegister rd, ARMRegister rn, int imm, int cond = ARM_COND_AL);
    void emit_orr(ARMRegister rd, ARMRegister rn, ARMRegister rm, bool set_flags = false);
    void emit_eor(ARMRegister rd, ARMRegister rn, ARMRegister rm);
    void emit_lsl(ARMRegister rd, ARMRegister rn, ARMRegister rm);
    void emit_lsr(ARMRegister rd, ARMRegister rn, ARMRegister rm);
    void emit_mvn(ARMRegister rd, ARMRegister rm);

    void emit_cmp(ARMRegister rn, ARMRegister rm);
    void emit_cmp_imm(ARMRegister rn, int imm);
    
    void emit_mov(ARMRegister rd, ARMRegister rm);
    void emit_movw(ARMRegister rd, u16 imm, int cond = ARM_COND_AL);
    void emit_movt(ARMRegister rd, u16 imm);
    void emit_mov_imm(ARMRegister rd, u16 imm, int cond = ARM_COND_AL);
    
    void emit_push(ARMRegister r);
    void emit_pop(ARMRegister r);
    void emit_push_reglist(const ARMRegister* regs, int count);
    void emit_pop_reglist(const ARMRegister* regs, int count);
    void emit_push_fp_lr() {
        ARMRegister regs[]{ ARM_REG_FP, ARM_REG_LR };
        emit_push_reglist(regs,2);
    }
    void emit_pop_fp_lr() {
        ARMRegister regs[]{ ARM_REG_FP, ARM_REG_LR };
        emit_pop_reglist(regs,2);
    }
    // immediate is a 13-bit immediate, last bit indicating signedness
    void emit_ldr(ARMRegister rt, ARMRegister rn, i16 imm16);
    void emit_ldrb(ARMRegister rt, ARMRegister rn, i16 imm16);
    void emit_ldrh(ARMRegister rt, ARMRegister rn, i16 imm16);
    void emit_str(ARMRegister rt, ARMRegister rn, i16 imm16);
    void emit_strb(ARMRegister rt, ARMRegister rn, i16 imm16);
    void emit_strh(ARMRegister rt, ARMRegister rn, i16 imm16);
    
    void emit_b(int imm, int cond);
    void emit_bl(int imm);
    void emit_blx(ARMRegister rm);
    void emit_bx(ARMRegister rm);
    
    void set_imm24(int index, int imm);
    void set_imm12(int index, int imm);
    
    struct RegInfo {
        bool used = false;
        int last_used = 0;
        ARMRegister arm_reg = ARM_REG_INVALID;
        BCRegister bc_reg = BC_REG_INVALID;
    };
    RegInfo arm_registers[ARM_REG_MAX]{};
    RegInfo bc_registers[BC_REG_MAX]{};
    
    ARMRegister alloc_register(BCRegister reg = BC_REG_INVALID);
    ARMRegister find_register(BCRegister reg);
    void free_register(ARMRegister reg);
    
    bool prepare_assembly(Bytecode::ASM& inst);
};

bool GenerateARM(Compiler* compiler, TinyBytecode* tinycode);
