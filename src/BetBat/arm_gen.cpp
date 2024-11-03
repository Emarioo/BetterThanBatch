#include "BetBat/arm_gen.h"
#include "BetBat/Compiler.h"

#include "BetBat/arm_defs.h"

// https://armconverter.com/?disasm

bool GenerateARM(Compiler* compiler, TinyBytecode* tinycode) {
    using namespace engone;
    TRACE_FUNC()
    
    ZoneScopedC(tracy::Color::SkyBlue1);

    _VLOG(log::out << log::BLUE << "ARM Generator:\n";)


    // make sure dependencies have been fixed first
    bool yes = tinycode->applyRelocations(compiler->bytecode);
    if (!yes) {
        log::out << "Incomplete call relocation\n";
        return false;
    }

    ARMBuilder builder{};
    builder.init(compiler, tinycode);

    yes = builder.generate();
    return yes;
}

bool ARMBuilder::generate() {
    using namespace engone;
    
    TRACE_FUNC()

    CALLBACK_ON_ASSERT(
        tinycode->print(0,-1, code);
    )
    
    if(tinycode->required_asm_instances.size() > 0) {
        log::out << log::RED << "Inline assembly not supported when targeting arm.\n";
    }
    
    auto& instructions = tinycode->instructionSegment;
    
    BytecodePrintCache print_cache{};
    bool logging = true;
    bool running = true;
    i64 pc = 0;
    while(running) {
        if(!(pc < instructions.size()))
            break;
        i64 prev_pc = pc;
        InstructionOpcode opcode = (InstructionOpcode)instructions[pc];
        pc++;
        
        InstBase* base = (InstBase*)&instructions[prev_pc];
        pc += instruction_contents[opcode].size - 1; // -1 because we incremented pc earlier
        
        if(logging) {
            tinycode->print(prev_pc, prev_pc + 1, bytecode, nullptr, false, &print_cache);
            log::out << "\n";
        }
        
        switch(opcode) {
            case BC_HALT: {
                Assert(("HALT not implemented",false));
            } break;
            case BC_NOP: {
                // don't emit nops? they don't do anything unless the user wants to mark the machine code with nops to more easily navigate the disassembly
                // for now, we disable it
                // emit1(OPCODE_NOP);
            } break;
            case BC_MOV_RR: {
                auto inst = (InstBase_op2*)base;
                
                ARMRegister reg_dst = alloc_register(inst->op0);
                ARMRegister reg_op = find_register(inst->op1);
                emit_mov(reg_dst, reg_op);
            } break;
            case BC_MOV_RM:
            case BC_MOV_RM_DISP16: {
                auto inst = (InstBase_op2_ctrl_imm16*)base;
                
                i16 imm = 0;
                if(opcode == BC_MOV_RM_DISP16)
                    imm = inst->imm16;
                ARMRegister reg_dst = alloc_register(inst->op0);
                ARMRegister reg_op = find_register(inst->op1);
                emit_ldr(reg_dst, reg_op, imm);
            } break;
            case BC_MOV_MR:
            case BC_MOV_MR_DISP16: {
                auto inst = (InstBase_op2_ctrl_imm16*)base;
                
                i16 imm = 0;
                if(opcode == BC_MOV_RM_DISP16)
                    imm = inst->imm16;
                ARMRegister reg_dst = find_register(inst->op0);
                ARMRegister reg_op = find_register(inst->op1);
                emit_str(reg_dst, reg_op, imm);
            } break;
            case BC_PUSH: {
                auto inst = (InstBase_op1*)base;
                
                ARMRegister reg_op = find_register(inst->op0);
                emit_push(reg_op);
            } break;
            case BC_POP: {
                auto inst = (InstBase_op1*)base;
                
                ARMRegister reg_dst = alloc_register(inst->op0);
                emit_pop(reg_dst);
            } break;
            case BC_LI32: {
                auto inst = (InstBase_op1_imm32*)base;
                
                ARMRegister reg_dst = alloc_register(inst->op0);
                emit_movw(reg_dst, inst->imm32 & 0xFFFF);
                if((inst->imm32 >> 16) != 0)
                    emit_movt(reg_dst, inst->imm32 >> 16);
            } break;
            case BC_LI64: {
                
            } break;
            case BC_INCR: {
                
            } break;
            case BC_ALLOC_LOCAL: {
                
            } break;
            case BC_ALLOC_ARGS: {
                
            } break;
            case BC_FREE_LOCAL: {
                
            } break;
            case BC_FREE_ARGS: {
                
            } break;
            case BC_SET_ARG: {
                
            } break;
            case BC_GET_PARAM: {
                
            } break;
            case BC_GET_VAL: {
                
            } break;
            case BC_SET_RET: {
                
            } break;
            case BC_PTR_TO_LOCALS: {
                
            } break;
            case BC_PTR_TO_PARAMS: {
                
            } break;
            case BC_CALL:
            case BC_CALL_REG: {
                
            } break;
            case BC_RET: {
                emit_bx(ARM_REG_LR);
            } break;
            case BC_JNZ: {
                
            } break;
            case BC_JZ: {
                
            } break;
            case BC_JMP: {
                
            } break;
            case BC_DATAPTR: {
                
            } break;
            case BC_EXT_DATAPTR: {
                
            } break;
            case BC_CODEPTR: {
                
            } break;
            case BC_ADD: {
                auto inst = (InstBase_op2_ctrl*)base;
                
                ARMRegister reg_dst = find_register(inst->op0);
                ARMRegister reg_op = find_register(inst->op1);
                emit_add(reg_dst, reg_op, reg_op);
            } break;
            case BC_SUB:
            case BC_MUL: {
            //  TODO: less than, equal, bitwise ops
            } break;
            case BC_CAST: {
            } break;
            default: {
                log::out << log::RED << "TODO: Implement BC instrtuction in ARM generator: "<< log::PURPLE<< opcode << "\n";
                Assert(false);
            }
        }
    }
    
    funcprog->printHex();
    
    Assert(("Size of generated ARM is not divisible by 4",code_size() % 4 == 0));
    
    return true;
}

ARMRegister ARMBuilder::alloc_register(BCRegister reg) {
    for(int i=1;i<ARM_REG_MAX;i++) {
        auto& info = arm_registers[i];
        if(info.used)
            continue;
        info.used = true;
        info.bc_reg = reg;
        
        auto& bc_info = bc_registers[reg];
        bc_info.arm_reg = (ARMRegister)i;
        return (ARMRegister)i;
    }
    Assert(("no free registers",false));
    return ARM_REG_INVALID;
}
ARMRegister ARMBuilder::find_register(BCRegister reg) {
    auto& bc_info = bc_registers[reg];
    // Assert(bc_info.arm_reg != ARM_REG_INVALID);
    if(bc_info.arm_reg == ARM_REG_INVALID)
        return ARM_REG_R0; // @nocheckin temporary
    return bc_info.arm_reg;
}
void ARMBuilder::free_register(ARMRegister reg) {
    auto& info = arm_registers[reg-1];
    Assert(info.used);
    
    if(info.bc_reg != BC_REG_INVALID) {
        auto& bc_info = bc_registers[info.bc_reg];
        bc_info.arm_reg = ARM_REG_INVALID;
    }
    info.used = false;
    info.bc_reg = BC_REG_INVALID;
}

void ARMBuilder::emit_add(ARMRegister rd, ARMRegister rn, ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/ADD--register--ARM-?lang=en
    ARM_CLAMP_dnm()
    int S = 0;
    int cond = ARM_COND_AL;
    int imm5 = 0;
    int type = ARM_SHIFT_TYPE_LSL;
    int inst = BITS(cond, 4, 28) | BITS(0b0000100, 7, 21) | BITS(S, 1, 20)
        | BITS(Rn,4,16) | BITS(Rd,4,12) | BITS(imm5, 5, 7) | BITS(type, 2, 5)
         | BITS(Rm, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_mov(ARMRegister rd, ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/MOV--register--ARM-?lang=en
    ARM_CLAMP_dm()
    int S = 0;
    int cond = ARM_COND_AL;
    int inst = BITS(cond, 4, 28) | BITS(0b0001101, 7, 21) | BITS(S, 1, 20)
        | BITS(Rd,4,12) | BITS(Rm, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_movw(ARMRegister rd, u16 imm16) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/MOV--immediate-
    ARM_CLAMP_d()
    int cond = ARM_COND_AL;
    int imm4 = imm16 >> 12;
    int imm12 = imm16 & 0x0FFF;
    int inst = BITS(cond, 4, 28) | BITS(0b00110000, 8, 20) | BITS(imm4, 4, 16)
        | BITS(Rd,4,12) | BITS(imm12, 12, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_movt(ARMRegister rd, u16 imm16) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/MOVT
    ARM_CLAMP_d()
    int cond = ARM_COND_AL;
    int imm4 = imm16 >> 12;
    int imm12 = imm16 & 0x0FFF;
    int inst = BITS(cond, 4, 28) | BITS(0b00110100, 8, 20) | BITS(imm4, 4, 16)
        | BITS(Rd,4,12) | BITS(imm12, 12, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_push(ARMRegister r) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/PUSH?lang=en
    Assert(r > 0 && r < ARM_REG_MAX);
    u8 reg = r-1;
    int cond = ARM_COND_AL;
    int reglist = 1 << reg;
    int inst = BITS(cond, 4, 28) | BITS(0b100100101101, 12, 16) | BITS(reglist, 16, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_pop(ARMRegister r) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/POP--ARM-?lang=en
    Assert(r > 0 && r < ARM_REG_MAX);
    u8 reg = r-1;
    int cond = ARM_COND_AL;
    int reglist = 1 << reg;
    int inst = BITS(cond, 4, 28) | BITS(0b100010111101, 12, 16) | BITS(reglist, 16, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_ldr(ARMRegister rt, ARMRegister rn, i16 imm16) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/LDR--immediate--ARM-?lang=en
    Assert(rt > 0 && rt < ARM_REG_MAX);
    Assert(rn > 0 && rn < ARM_REG_MAX);
    // immediate can't be larger than 12-bits. it can be signed, not including the 12 bits
    Assert(((imm16 & 0x8000) && (imm16 & 0x7000) == 0x7000) || (imm16 & 0xF000) == 0);
    u8 Rt = rt-1;
    u8 Rn = rn-1;
    
    int cond = ARM_COND_AL;
    int P = 1; // P=0 would write back (Rn=Rn+imm) which we don't want
    int U = !(imm16 >> 15);
    int W = 0; // W=1 would write back
    int imm12 = imm16 & 0xFFF;
    if(!U)
        imm12 = -imm12;
    int inst = BITS(cond, 4, 28) | BITS(0b010, 3, 25) | BITS(P, 1, 24)
        | BITS(U, 1, 23) | BITS(W, 1, 21) | BITS(1, 1, 20) | BITS(Rn, 4, 16)
        | BITS(Rt, 4, 12) | BITS(imm12, 12, 0);
    // NOTE: Differs from str at bit 20, bit is 0 in str and 1 in ldr
    emit4((u32)inst);
}
void ARMBuilder::emit_str(ARMRegister rt, ARMRegister rn, i16 imm16) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/LDR--immediate--ARM-?lang=en
    Assert(rt > 0 && rt < ARM_REG_MAX);
    Assert(rn > 0 && rn < ARM_REG_MAX);
    // immediate can't be larger than 12-bits. it can be signed, not including the 12 bits
    Assert(((imm16 & 0x8000) && (imm16 & 0x7000) == 0) || (imm16 & 0xF000) == 0);
    u8 Rt = rt-1;
    u8 Rn = rn-1;
    
    int cond = ARM_COND_AL;
    int P = 1; // P=0 would write back (Rn=Rn+imm) which we don't want
    int U = !(imm16 >> 15);
    int W = 0; // W=1 would write back
    int imm12 = imm16 & 0xFFF;
    if(!U)
        imm12 = -imm12;
    int inst = BITS(cond, 4, 28) | BITS(0b010, 3, 25) | BITS(P, 1, 24)
        | BITS(U, 1, 23) | BITS(W, 1, 21) | BITS(Rn, 4, 16)
        | BITS(Rt, 4, 12) | BITS(imm12, 12, 0);
    // NOTE: Differs from ldr at bit 20, bit is 0 in str and 1 in ldr
    emit4((u32)inst);
}
void ARMBuilder::emit_bx(ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/BX?lang=en
    Assert(rm > 0 && rm < ARM_REG_MAX);
    u8 Rm = rm-1;
    int cond = ARM_COND_AL;
    int inst = BITS(cond, 4, 28) | BITS(0b00010010, 8, 20)
        | BITS(0xFFF, 12, 8) | BITS(0b0001, 4, 4) | BITS(Rm, 4, 0);
    emit4((u32)inst);
}
const char* arm_register_names[]{
    "INVALID", // ARM_REG_INVALID = 0,
    "r0",      // ARM_REG_R0,
    "r1",      // ARM_REG_R1,
    "r2",      // ARM_REG_R2,
    "r3",      // ARM_REG_R3,
    "r4",      // ARM_REG_R4,
    "r5",      // ARM_REG_R5,
    "r6",      // ARM_REG_R6,
    "r7",      // ARM_REG_R7,
    "r8",      // ARM_REG_R8,
    "r9",      // ARM_REG_R9,
    "r10",     // ARM_REG_R10,
    "fp",     // ARM_REG_R11,
    "ip",     // ARM_REG_R12,
    "sp",     // ARM_REG_R13,
    "lr",     // ARM_REG_R14,
    "pc",     // ARM_REG_R15,
};
engone::Logger &operator<<(engone::Logger &l, ARMRegister r) {
    l << arm_register_names[r];
    return l;
}