#include "BetBat/x64_gen.h"

#include "BetBat/COFF.h"
#include "BetBat/ELF.h"

#include "BetBat/CompilerEnums.h"

#include "BetBat/Compiler.h"

/*
    x86/x64 instructions are complicated and it's not really my fault
    if things seem confusing. I have tried my best to keep it simple.

    Incomplete layout:    
    [Prefix] [Opcode] [Mod|REG|R/M] [Displacement] [Immediate]

    Opcode tells you about which instruction to run (add, mul, push, ...)
    ModRM tells you about which register or memory location to use and how (reg to reg, mem to reg, reg to mem).
    Displacement holds an offset when refering to memory
    Some opcodes use immediates. There are multiple versions of add isntructions.
    Some use immediates and some use registers.

    Mod refers to addressing modes (or forms?). There are 4 values
    0b11 means register to register.
    The other 3 values means memory to register (or vice versa)
    

*/

// https://www.felixcloutier.com/x86/index.html
// https://defuse.ca/online-x86-assembler.htm#disassembly2
// https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

#define OPCODE_RET (u8)0xC3
#define OPCODE_CALL_IMM (u8)0xE8
#define OPCODE_CALL_RM_SLASH_2 (u8)0xFF
#define OPCODE_NOP (u8)0x90

// RM_REG means: Add REG to RM, RM = RM + REG
// REG_RM: REG = REG + RM

// #define OPCODE_ADD_REG_RM (u8)0x03
#define OPCODE_ADD_RM_REG (u8)0x01
#define OPCODE_ADD_RM_IMM_SLASH_0 (u8)0x81
#define OPCODE_ADD_RM_IMM8_SLASH_0 (u8)0x83

#define OPCODE_SUB_REG_RM (u8)0x2B
#define OPCODE_SUB_RM_IMM_SLASH_5 (u8)0x81

// This instruction has reg field in opcode.
// See x64 manual.
#define OPCODE_MOV_REG_IMM_RD_IO (u8)0xB8

#define OPCODE_MOV_RM_IMM32_SLASH_0 (u8)0xC7
#define OPCODE_MOV_RM_REG8 (u8)0x88
#define OPCODE_MOV_REG8_RM (u8)0x8A
#define OPCODE_MOV_RM_REG (u8)0x89
#define OPCODE_MOV_REG_RM (u8)0x8B

#define OPCODE_MOV_RM_IMM8_SLASH_0 (u8)0xC6

#define OPCODE_LEA_REG_M (u8)0x8D

// 2 means that the opcode takes 2 bytes
// note that the bytes are swapped
#define OPCODE_2_IMUL_REG_RM (u16)0xAF0F

#define OPCODE_IMUL_AX_RM_SLASH_5 (u8)0xF7
#define OPCODE_IMUL_AX_RM8_SLASH_5 (u8)0xF6

#define OPCODE_MUL_AX_RM_SLASH_4 (u8)0xF7
#define OPCODE_MUL_AX_RM8_SLASH_4 (u8)0xF6

#define OPCODE_IDIV_AX_RM_SLASH_7 (u8)0xF7
#define OPCODE_IDIV_AX_RM8_SLASH_7 (u8)0xF6

#define OPCODE_DIV_AX_RM_SLASH_6 (u8)0xF7
#define OPCODE_DIV_AX_RM8_SLASH_6 (u8)0xF6

// sign extends EAX into EDX, useful for IDIV
#define OPCODE_CDQ (u8)0x99

#define OPCODE_XOR_REG_RM (u8)0x33
#define OPCODE_XOR_RM_IMM8_SLASH_6 (u8)0x83

#define OPCODE_AND_RM_REG (u8)0x21

#define OPCODE_OR_RM_REG (u8)0x09

#define OPCODE_SHL_RM_CL_SLASH_4 (u8)0xD3
#define OPCODE_SHR_RM_CL_SLASH_5 (u8)0xD3

#define OPCODE_SHL_RM_IMM8_SLASH_4 (u8)0xC1
#define OPCODE_SHR_RM_IMM8_SLASH_5 (u8)0xC1

#define OPCODE_SHR_RM_ONCE_SLASH_5 (u8)0xD1

// logical and with flags being set, registers are not modified
#define OPCODE_TEST_RM_REG (u8)0x85

#define OPCODE_JL_REL8 (u8)0x7C
#define OPCODE_JS_REL8 (u8)0x78

#define OPCODE_NOT_RM_SLASH_2 (u8)0xF7

#define OPCODE_2_SETE_RM8 (u16)0x940F
#define OPCODE_2_SETNE_RM8 (u16)0x950F

#define OPCODE_2_SETG_RM8 (u16)0x9F0F
#define OPCODE_2_SETGE_RM8 (u16)0x9D0F
#define OPCODE_2_SETL_RM8 (u16)0x9C0F
#define OPCODE_2_SETLE_RM8 (u16)0x9E0F

#define OPCODE_2_SETA_RM8 (u16)0x970F
#define OPCODE_2_SETAE_RM8 (u16)0x930F
#define OPCODE_2_SETB_RM8 (u16)0x920F
#define OPCODE_2_SETBE_RM8 (u16)0x960F

// zero extension
#define OPCODE_2_MOVZX_REG_RM8 (u16)0xB60F
#define OPCODE_2_MOVZX_REG_RM16 (u16)0xB70F

#define OPCODE_2_CMOVZ_REG_RM (u16)0x440F

// cannot be 64 bit immediate even with REXW
#define OPCODE_AND_RM_IMM_SLASH_4 (u8)0x81
#define OPCODE_AND_RM_IMM8_SLASH_4 (u8)0x83
// not sign extended
#define OPCODE_AND_RM8_IMM8_SLASH_4 (u8)0x80

// sign extension
#define OPCODE_2_MOVSX_REG_RM8 (u16)0xBE0F
#define OPCODE_2_MOVSX_REG_RM16 (u16)0xBF0F
// intel manual encourages REX.W with MOVSXD. Use normal mov otherwise
#define OPCODE_MOVSXD_REG_RM (u8)0x63

// "FF /6" can be seen in x86 instruction sets.
// This means that the REG field in ModRM should be 6
// to use the push instruction
#define OPCODE_PUSH_RM_SLASH_6 (u8)0xFF
#define OPCODE_POP_RM_SLASH_0 (u8)0x8F

#define OPCODE_INCR_RM_SLASH_0 (u8)0xFF

#define OPCODE_CMP_RM_IMM8_SLASH_7 (u8)0x83
#define OPCODE_CMP_REG_RM (u8)0x3B

#define OPCODE_JMP_IMM32 (u8)0xE9

// bytes are flipped
#define OPCODE_2_JNE_IMM32 (u16)0x850F
#define OPCODE_2_JE_IMM32 (u16)0x840F
// je rel8
#define OPCODE_JE_IMM8 (u8)0x74
// jmp rel8
#define OPCODE_JMP_IMM8 (u8)0xEB


#define OPCODE_3_MOVSS_RM_REG (u32)0x110fF3
// manual says "MOVSS xmm1, m32" but register to register might work too, test it though
#define OPCODE_3_MOVSS_REG_RM (u32)0x100fF3
#define OPCODE_3_MOVSD_RM_REG (u32)0x110fF2
#define OPCODE_3_MOVSD_REG_RM (u32)0x100fF2

#define OPCODE_3_ADDSS_REG_RM (u32)0x580FF3
#define OPCODE_3_SUBSS_REG_RM (u32)0x5C0FF3
#define OPCODE_3_MULSS_REG_RM (u32)0x590FF3
#define OPCODE_3_DIVSS_REG_RM (u32)0x5E0FF3

#define OPCODE_3_ADDSD_REG_RM (u32)0x580FF2
#define OPCODE_3_SUBSD_REG_RM (u32)0x5C0FF2
#define OPCODE_3_MULSD_REG_RM (u32)0x590FF2
#define OPCODE_3_DIVSD_REG_RM (u32)0x5E0FF2


// IMPORTANT: REXW can't be pre-appended to the opcode.
//  It's sort of built in for some reason. You have to define
//  OPCODE_4_REXW if you need rexw
// NOTE: C/C++ uses the truncated conversion instead. This language should too.
//  convert float to i32 (or i64 with rexw) with truncation (float is rounded down)

// int -> float
#define OPCODE_3_CVTSI2SS_REG_RM (u32)0x2A0FF3
#define OPCODE_4_REXW_CVTSI2SS_REG_RM (u32)0x2A0F48F3
// float -> int
#define OPCODE_3_CVTTSS2SI_REG_RM (u32)0x2C0FF3
#define OPCODE_4_REXW_CVTTSS2SI_REG_RM (u32)0x2C0F48F3
// double -> int
#define OPCODE_3_CVTTSD2SI_REG_RM (u32)0x2C0FF2
#define OPCODE_4_REXW_CVTTSD2SI_REG_RM (u32)0x2C0F48F2
// int -> double
#define OPCODE_3_CVTSI2SD_REG_RM (u32)0x2A0FF2
#define OPCODE_4_REXW_CVTSI2SD_REG_RM (u32)0x2A0F48F2
// double -> float
#define OPCODE_3_CVTSD2SS_REG_RM (u32)0x5A0FF2
// float -> double
#define OPCODE_3_CVTSS2SD_REG_RM (u32)0x5F0FF3


// float comparisson, sets flags, see https://www.felixcloutier.com/x86/comiss
#define OPCODE_2_COMISS_REG_RM (u16)0x2F0F
#define OPCODE_2_UCOMISS_REG_RM (u16)0x2E0F

#define OPCODE_3_COMISD_REG_RM (u32)0x2F0F66

#define OPCODE_4_ROUNDSS_REG_RM_IMM8 (u32)0x0A3A0F66
#define OPCODE_3_PXOR_RM_REG (u32)0xEF0F66

#define OPCODE_3_SQRTSS_REG_RM (u32)0x510FF3

#define OPCODE_2_CMPXCHG_RM_REG (u16)0xB10F

// #define OPCODE_2_FSIN (u16)0xFED9
// #define OPCODE_2_FCOS (u16)0xFFD9
// // useful when calculating tan = sin/cos
// #define OPCODE_2_FSINCOS (u16)0xFBD9

#define OPCODE_3_RDTSCP (u32)0xF9010F
#define OPCODE_2_RDTSC (u16)0x310F

// the three other modes deal with memory
#define MODE_REG 0b11
// straight deref, no displacement
// SP, BP does not work with this! see intel manual 32-bit addressing forms
// TODO: Test that this works
#define MODE_DEREF 0b00
#define MODE_DEREF_DISP8 0b01
#define MODE_DEREF_DISP32 0b10

// other options specify registers
// see x64 manual for details
#define SIB_INDEX_NONE 0b100

#define SIB_SCALE_1 0b00
#define SIB_SCALE_2 0b01
#define SIB_SCALE_4 0b10
#define SIB_SCALE_8 0b11

// extension of RM field in ModR/M byte (base field if SIB byte is used)
#define PREFIX_REXB (u8)0b01000001
// extension of reg field in opcode
#define PREFIX_REXX (u8)0b01000010
// extension of reg field in ModR/M byte
#define PREFIX_REXR (u8)0b01000100
// 64-bit operands will be used
#define PREFIX_REXW (u8)0b01001000
#define PREFIX_REX (u8)0b01000000
#define PREFIX_16BIT (u8)0x66
#define PREFIX_LOCK (u8)0xF0

X64Register ToNativeRegister(BCRegister reg) {
    if(reg == BC_REG_SP) return X64_REG_SP;
    if(reg == BC_REG_BP) return X64_REG_BP;
    return X64_REG_INVALID;
}
bool IsNativeRegister(BCRegister reg) {
    return ToNativeRegister(reg) != X64_REG_INVALID;
}
bool IsNativeRegister(X64Register reg) {
    return reg == X64_REG_BP || reg == X64_REG_SP;
}

void GenerateX64(Compiler* compiler, TinyBytecode* tinycode) {
    using namespace engone;
    ZoneScopedC(tracy::Color::SkyBlue1);
    
    _VLOG(log::out <<log::BLUE<< "x64 Converter:\n";)
    
    // steal debug information
    // prog->debugInformation = bytecode->debugInformation;
    // bytecode->debugInformation = nullptr;
    
    // make sure dependencies have been fixed first
    bool yes = tinycode->applyRelocations(compiler->code);
    if(!yes) {
        log::out << "Incomplete call relocation\n";
        return;
    }

    X64Builder builder{};
    builder.init(compiler->program);
    builder.bytecode = compiler->code;
    builder.tinycode = tinycode;
    builder.tinyprog = compiler->program->createProgram();
    
    builder.generateFromTinycode(builder.bytecode, builder.tinycode);
    // bool fast = true;
    // if(fast) {
    // } else {
    //     Assert(false);
    //     builder.generateInstructions_slow();
    // }
}

void X64Builder::generateFromTinycode(Bytecode* bytecode, TinyBytecode* tinycode) {
    using namespace engone;
    bool logging = false;
    // instruction_indices.clear();
    
    this->bytecode = bytecode;
    this->tinycode = tinycode;
    
    generateInstructions();
}


void X64Builder::generateInstructions(int depth, BCRegister find_reg, int origin_inst_index, X64Register* out_reg) {
    using namespace engone;
    auto& instructions = tinycode->instructionSegment;
    bool logging = false;
    bool gen_forward = depth == 0;
    
    if(logging && gen_forward)
        log::out << log::GOLD << "Code gen\n";
    
    // log::out << "GEN depth: "<<depth << " " << find_reg << "\n"; 
    
    bool find_push = false;
    int push_level = 0;
    bool complete = false;
    i64 pc = 0;
    int next_inst_index = origin_inst_index;
    while(!complete) {
        if(!(pc < instructions.size()))
            break;
        i64 prev_pc = pc;
        // i64 inst_index = next_inst_index;
        
        InstructionType opcode{};
        // if(gen_forward) {
            opcode = (InstructionType)instructions[pc];
            pc++;
        //     instruction_indices.add(prev_pc);
        // } else {
        //     opcode = (InstructionType)instructions[instruction_indices[inst_index]];
        //     next_inst_index--;
        // }
        
        BCRegister op0=BC_REG_INVALID, op1=BC_REG_INVALID, op2=BC_REG_INVALID;
        InstructionControl control;
        InstructionCast cast;
        i64 imm;
        
        // _ILOG(log::out <<log::GRAY<<" sp: "<< sp <<" fp: "<<fp<<"\n";)
        if(logging) {
            tinycode->print(prev_pc, prev_pc + 1, bytecode);
            log::out << "\n";
        }

        switch (opcode) {
        case BC_HALT: {
            INCOMPLETE
        } break;
        case BC_NOP: {
            INCOMPLETE
        } break;
        case BC_MOV_RR: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            auto node = new OPNode();
            node->opcode = opcode;
            node->op0 = op0;
            node->op1 = op1;
            
            if(!IsNativeRegister(op1)) {
                node->in1 = reg_values[op1];
            }
            reg_values[op0] = node;
        } break;
        case BC_MOV_RM: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            auto node = new OPNode();
            node->opcode = opcode;
            node->op0 = op0;
            node->op1 = op1;
            node->control = control;
            
            if(!IsNativeRegister(op1))
                node->in1 = reg_values[op1];
            reg_values[op0] = node;
        } break;
        case BC_MOV_MR: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            auto node = new OPNode();
            node->opcode = opcode;
            node->op0 = op0;
            node->op1 = op1;
            node->control = control;
            
            if(IsNativeRegister(op0)) {
                
            } else {
                node->in0 = reg_values[op0];
            }
            node->in1 = reg_values[op1];
            
            nodes.add(node);
        } break;
        case BC_MOV_RM_DISP16: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc += 2;
            
            auto node = new OPNode();
            node->opcode = opcode;
            node->op0 = op0;
            node->op1 = op1;
            node->control = control;
            node->imm = imm;
            
            if(!IsNativeRegister(op1))
                node->in1 = reg_values[op1];
            reg_values[op0] = node;
        } break;
        case BC_MOV_MR_DISP16: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc += 2;
            
            Assert(op1 != BC_REG_SP && op1 != BC_REG_BP);
            Assert(control == CONTROL_32B);
            // TODO: op0, op1 as stack or base pointer requires special handling
            
            auto node = new OPNode();
            node->opcode = opcode;
            node->op0 = op0;
            node->op1 = op1;
            node->control = control;
            node->imm = imm;
            
            if(!IsNativeRegister(op0)) {
                node->in0 = reg_values[op0];
            }
            node->in1 = reg_values[op1];
            
            nodes.add(node);
        } break;
        case BC_PUSH: {
            op0 = (BCRegister)instructions[pc++];
            
            Assert(!IsNativeRegister(op0));
            
            auto n = reg_values[op0];
            stack_values.add(n);
        } break;
        case BC_POP: {
            op0 = (BCRegister)instructions[pc++];
            Assert(!IsNativeRegister(op0));
            
            auto n = stack_values.last();
            stack_values.pop();
            reg_values[op0] = n;
        } break;
        case BC_LI32: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto n = new OPNode();
            n->opcode = opcode;
            n->imm = imm;
            reg_values[op0] = n;
        } break;
        case BC_LI64: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i64*)&instructions[pc];
            pc+=8;
            
            auto n = new OPNode();
            n->opcode = opcode;
            n->imm = imm;
            reg_values[op0] = n;
        } break;
        case BC_INCR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = new OPNode();
            node->opcode = opcode;
            node->op0 = op0;
            node->imm = imm;
            
            if(!IsNativeRegister(op0)) {
                node->in0 = reg_values[op0];
                reg_values[op0] = node->in0;
            } else {
                nodes.add(node);
            }
        } break;
        case BC_JMP: {
            imm = *(i32*)&instructions[pc];
            pc+=4;
              
            auto node = new OPNode();
            node->opcode = opcode;
            node->imm = imm;
            nodes.add(node);
        } break;
        case BC_JZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = new OPNode();
            node->opcode = opcode;
            node->op0 = op0;
            node->imm = imm;
            
            node->in0 = reg_values[op0];
            
            nodes.add(node);
        } break;
        case BC_JNZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = new OPNode();
            node->opcode = opcode;
            node->op0 = op0;
            node->imm = imm;
            
            node->in0 = reg_values[op0];
            
            nodes.add(node);
        } break;
        case BC_CALL: {
            LinkConventions l = (LinkConventions)instructions[pc++];
            CallConventions c = (CallConventions)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;

            auto node = new OPNode();
            node->opcode = opcode;
            node->link = l;
            node->call = c;
            node->imm = imm;
            
            nodes.add(node);
        } break;
        case BC_RET: {
            auto node = new OPNode();
            node->opcode = opcode;
            
            nodes.add(node);
        } break;
        case BC_DATAPTR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = new OPNode();
            node->opcode = opcode;
            node->op0 = op0;
            node->imm = imm;
            
            reg_values[op0] = node;
        } break;
        case BC_CODEPTR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = new OPNode();
            node->opcode = opcode;
            node->op0 = op0;
            node->imm = imm;
            
            reg_values[op0] = node;
        } break;
        case BC_CAST: {
            op0 = (BCRegister)instructions[pc++];
            InstructionCast cast = (InstructionCast)instructions[pc++];
            u8 fsize = (u8)instructions[pc++];
            u8 tsize = (u8)instructions[pc++];

            auto node = new OPNode();
            node->opcode = opcode;
            node->op0 = op0;
            node->cast = cast;
            node->fsize = fsize;
            node->tsize = tsize;
            
            node->in0 = reg_values[op0];
            reg_values[op0] = node;
        } break;
        case BC_MEMZERO: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            auto node = new OPNode();
            node->opcode = opcode;
            node->op0 = op0;
            node->op1 = op1;
            
            node->in0 = reg_values[op0];
            node->in1 = reg_values[op1];
            
            nodes.add(node);
        } break;
        case BC_MEMCPY: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            op2 = (BCRegister)instructions[pc++];
            
            auto node = new OPNode();
            node->opcode = opcode;
            node->op0 = op0;
            node->op1 = op1;
            node->op2 = op2;
            
            node->in0 = reg_values[op0];
            node->in1 = reg_values[op1];
            node->in2 = reg_values[op2];
            
            nodes.add(node);
        } break;
        case BC_ADD:
        case BC_SUB:
        case BC_MUL:
        case BC_DIV:
        case BC_MOD:
        case BC_EQ:
        case BC_NEQ:
        case BC_LT:
        case BC_LTE:
        case BC_GT:
        case BC_GTE: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            auto n = new OPNode();
            n->opcode = opcode;
            n->op0 = op0;
            n->op1 = op1;
            n->control = control;
            n->in0 = reg_values[op0];
            n->in1 = reg_values[op1];
            
            reg_values[op0] = n;
        } break;
        case BC_LAND:
        case BC_LOR:
        case BC_BAND:
        case BC_BOR:
        case BC_BXOR:
        case BC_BLSHIFT:
        case BC_BRSHIFT: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            auto n = new OPNode();
            n->opcode = opcode;
            n->op0 = op0;
            n->op1 = op1;
            n->in0 = reg_values[op0];
            n->in1 = reg_values[op1];
            
            reg_values[op0] = n;
        } break;
        case BC_LNOT:
        case BC_BNOT: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            auto n = new OPNode();
            n->opcode = opcode;
            n->op0 = op0;
            n->op1 = op1;
            // n->in0 = reg_values[op0];
            n->in1 = reg_values[op1];
            
            reg_values[op0] = n;
        } break;
        default: Assert(false);
        }
    }
    
    // Generate x64 from OPNode tree
    
    // struct Operand {
    //     X64Register reg{};
    //     bool stacked = false;
    // };
    struct Env {
        OPNode* node=nullptr;

        Env* env_in0=nullptr;
        Env* env_in1=nullptr;

        X64Register reg0{};
        X64Register reg1{};
        X64Register out{};
        
        bool reg0_stack = false;
        bool reg1_stack = false;
        bool out_stack = false;
    };
    QuickArray<Env*> envs;
    
    int node_i = 0;

    bool pop_env = true;
    Env* env = nullptr;
    OPNode* n = nullptr;

    auto push_env0 = [&]() {
        Assert(n->in0);
        auto e = new Env();
        env->env_in0 = e;
        e->node = n->in0;

        envs.add(e);
        pop_env = false;
    };
    auto push_env1 = [&]() {
        Assert(n->in1);
        auto e = new Env();
        env->env_in1 = e;
        e->node = n->in1;

        envs.add(e);
        pop_env = false;
    };

    emit1(OPCODE_PUSH_RM_SLASH_6);
    emit_modrm_slash(MODE_REG, 6, X64_REG_BP);

    emit1(PREFIX_REXW);
    emit1(OPCODE_MOV_REG_RM);
    emit_modrm(MODE_REG, X64_REG_BP, X64_REG_SP);

    while(true) {
        if(envs.size() == 0) {
            if(node_i >= nodes.size())
                break;
                
            envs.add(new Env());
            envs.last()->node = nodes[node_i];
            node_i++;
        }
        
        env = envs.last();
        n = env->node;
        pop_env = true;

        if(IsNativeRegister(n->op0)) {
            env->reg0 = ToNativeRegister(n->op0);
        }
        if(IsNativeRegister(n->op1)) {
            env->reg1 = ToNativeRegister(n->op1);
        }
        
        switch(n->opcode) {
            case BC_BXOR: {
                if(n->opcode == BC_BXOR && n->op0 == n->op1) {
                    // xor clear on register
                    Assert(!IsNativeRegister(n->op0));

                    env->out = alloc_register();
                    if(env->out == X64_REG_INVALID) {
                        env->out_stack = true;
                        env->out = RESERVED_REG0;
                    }
                    emit1(PREFIX_REXW);
                    emit1(OPCODE_XOR_REG_RM);
                    emit_modrm(MODE_REG, env->out, env->out);
                    
                    if(env->out_stack) {
                        emit1(OPCODE_PUSH_RM_SLASH_6);
                        emit_modrm_slash(MODE_REG, 6, env->out);
                    }
                    break;
                }
                // fall through
            }
            case BC_ADD:
            case BC_SUB:
            case BC_MUL: 
            
            case BC_LAND:
            case BC_LOR:
            // case BC_LNOT: // unary operator, "reg = !reg" instead of "reg = reg ! reg"
            case BC_BAND:
            case BC_BOR:
            // case BC_BNOT: // unary operator
            case BC_BLSHIFT:
            case BC_BRSHIFT:

            case BC_EQ:
            case BC_NEQ:
            case BC_LT:
            case BC_LTE:
            case BC_GT:
            case BC_GTE:
            {
                Assert(!(n->opcode == BC_BXOR && n->op0 == n->op1)); // just making sure we don't do a bxor because we don't handle those here
                if(!env->env_in0 && !env->env_in1) {
                    // NOTE: Evaluate the node with the most calculations and register allocations first so that we don't unnecesarily allocate registers and push values to the stack. We do the easiest work first.
                    // TODO: Optimize get_node_depth, recalculating depth like this is expensive. (calculate once and reuse for parent and child nodes)
                    int depth0 = get_node_depth(n->in0);
                    int depth1 = get_node_depth(n->in1);

                    if(depth0 < depth1) {
                        if(!env->env_in0 && !IsNativeRegister(n->op0)) {
                            push_env0();
                        }
                        if(!env->env_in1 && !IsNativeRegister(n->op1)) {
                            push_env1();
                        }
                    } else {
                        if(!env->env_in1 && !IsNativeRegister(n->op1)) {
                            push_env1();
                        }
                        if(!env->env_in0 && !IsNativeRegister(n->op0)) {
                            push_env0();
                        }
                    }
                    Assert(!pop_env);
                    break;
                }

                if(!IsNativeRegister(n->op0)) {
                    env->reg0_stack = env->env_in0->out_stack;
                    env->reg0 = env->env_in0->out;
                } else {
                    env->reg1 = ToNativeRegister(n->op0);
                }
                if(!IsNativeRegister(n->op1)) {
                    env->reg1_stack = env->env_in1->out_stack;
                    env->reg1 = env->env_in1->out;
                } else {
                    env->reg1 = ToNativeRegister(n->op1);
                }

                if(env->reg1_stack) {
                    env->reg1 = RESERVED_REG0;
                    emit1(OPCODE_POP_RM_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, env->reg1);
                }
                if(env->reg0_stack) {
                    env->reg0 = RESERVED_REG1;
                    emit1(OPCODE_POP_RM_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, env->reg0);
                }
                
                switch(n->opcode) {
                    case BC_ADD: {
                        emit1(PREFIX_REXW);
                        emit1(OPCODE_ADD_RM_REG);
                        emit_modrm(MODE_REG, env->reg1, env->reg0);
                    } break;
                    case BC_SUB: {
                        emit1(PREFIX_REXW);
                        emit1(OPCODE_SUB_REG_RM);
                        emit_modrm(MODE_REG, env->reg0, env->reg1);
                    } break;
                    case BC_MUL: {
                        // TODO: Handle signed/unsigned multiplication (using InstructionControl)
                        emit1(PREFIX_REXW);
                        emit2(OPCODE_2_IMUL_REG_RM);
                        emit_modrm(MODE_REG, env->reg1, env->reg0);
                    } break;
                     case BC_LAND: {
                        Assert(false);
                    } break;
                    case BC_LOR: {
                        Assert(false);
                    } break;
                    case BC_LNOT: {
                        Assert(false);
                    } break;
                    case BC_BAND: {
                        emit1(PREFIX_REXW);
                        emit1(OPCODE_AND_RM_REG);
                        emit_modrm(MODE_REG, env->reg1, env->reg0);
                    } break;
                    case BC_BOR: {
                        emit1(PREFIX_REXW);
                        emit1(OPCODE_OR_RM_REG);
                        emit_modrm(MODE_REG, env->reg1, env->reg0);
                    } break;
                    case BC_BXOR: {
                        emit1(PREFIX_REXW);
                        emit1(OPCODE_XOR_REG_RM);
                        emit_modrm(MODE_REG, env->reg0, env->reg1);
                    } break;
                    case BC_BLSHIFT:
                    case BC_BRSHIFT: {
                        bool c_was_used = false;
                        if(!is_register_free(X64_REG_C)) {
                            c_was_used = true;
                            emit1(OPCODE_PUSH_RM_SLASH_6);
                            emit_modrm_slash(MODE_REG, 6, X64_REG_C);

                            emit1(PREFIX_REXW);
                            emit1(OPCODE_MOV_REG_RM);
                            emit_modrm(MODE_REG, X64_REG_C, env->reg1);
                        }

                        emit1(PREFIX_REXW);
                        switch(n->opcode) {
                            case BC_BLSHIFT: {
                                emit1(PREFIX_REXW);
                                emit1(OPCODE_SHL_RM_CL_SLASH_4);
                                emit_modrm_slash(MODE_REG, 4, env->reg0);
                            } break;
                            case BC_BRSHIFT: {
                                emit1(PREFIX_REXW);
                                emit1(OPCODE_SHR_RM_CL_SLASH_5);
                                emit_modrm_slash(MODE_REG, 5, env->reg0);
                            } break;
                            default: Assert(false);
                        }

                        if(c_was_used) {
                            emit1(OPCODE_POP_RM_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, X64_REG_C);
                        }
                    } break;
                    case BC_EQ: {
                        
                        // TODO: Test if this is okay? is cmp better?
                        emit1(PREFIX_REXW);
                        emit1(OPCODE_XOR_REG_RM);
                        emit_modrm(MODE_REG, env->reg0, env->reg1);
                        
                        emit2(OPCODE_2_SETE_RM8);
                        emit_modrm_slash(MODE_REG, 0, env->reg0);

                        emit1(PREFIX_REXW);
                        emit2(OPCODE_2_MOVZX_REG_RM8);
                        emit_modrm_slash(MODE_REG, env->reg0, env->reg0);
                    } break;
                    case BC_NEQ: {
                        // IMPORTANT: THERE MAY BE BUGS IF YOU COMPARE OPERANDS OF DIFFERENT SIZES.
                        //  SOME BITS MAY BE UNINITIALZIED.
                        emit1(PREFIX_REXW);
                        emit1(OPCODE_XOR_REG_RM);
                        emit_modrm(MODE_REG, env->reg0, env->reg1);
                    } break;
                    case BC_LT:
                    case BC_LTE:
                    case BC_GT:
                    case BC_GTE: {
                        emit1(PREFIX_REXW);
                        emit1(OPCODE_CMP_REG_RM);
                        emit_modrm(MODE_REG, env->reg0, env->reg1);

                        u16 setType = 0;
                        if(IS_CONTROL_SIGNED(n->control)) {
                            switch(n->opcode) {
                                case BC_LT: {
                                    setType = OPCODE_2_SETL_RM8;
                                } break;
                                case BC_LTE: {
                                    setType = OPCODE_2_SETLE_RM8;
                                } break;
                                case BC_GT: {
                                    setType = OPCODE_2_SETG_RM8;
                                } break;
                                case BC_GTE: {
                                    setType = OPCODE_2_SETGE_RM8;
                                } break;
                                default: Assert(false);
                            }
                        } else {
                            switch(n->opcode) {
                                case BC_LT: {
                                    setType = OPCODE_2_SETB_RM8;
                                } break;
                                case BC_LTE: {
                                    setType = OPCODE_2_SETBE_RM8;
                                } break;
                                case BC_GT: {
                                    setType = OPCODE_2_SETA_RM8;
                                } break;
                                case BC_GTE: {
                                    setType = OPCODE_2_SETAE_RM8;
                                } break;
                                default: Assert(false);
                            }
                        }
                        
                        emit2(setType);
                        emit_modrm_slash(MODE_REG, 0, env->reg0);
                        
                        // do we need more stuff or no? I don't think so?
                        // prog->add2(OPCODE_2_MOVZX_REG_RM8);
                        // prog->addModRM(MODE_REG, reg2, reg2);

                    } break;
                    default: Assert(false);
                }
                
                if(!env->reg1_stack && !IsNativeRegister(n->op1))
                    free_register(env->reg1);
                    
                if(env->reg0_stack) {
                    emit1(OPCODE_PUSH_RM_SLASH_6);
                    emit_modrm_slash(MODE_REG, 6, env->reg0);
                }

                env->out = env->reg0;
                env->out_stack = env->reg0_stack;

            } break;
            case BC_DIV:
            case BC_MOD: {
                if(!env->env_in0 && !env->env_in1) {
                    // NOTE: Evaluate the node with the most calculations and register allocations first so that we don't unnecesarily allocate registers and push values to the stack. We do the easiest work first.
                    // TODO: Optimize get_node_depth, recalculating depth like this is expensive. (calculate once and reuse for parent and child nodes)
                    int depth0 = get_node_depth(n->in0);
                    int depth1 = get_node_depth(n->in1);

                    if(depth0 < depth1) {
                        if(!env->env_in0 && !IsNativeRegister(n->op0)) {
                            push_env0();
                        }
                        if(!env->env_in1  && !IsNativeRegister(n->op1)) {
                            push_env1();
                        }
                    } else {
                        if(!env->env_in1  && !IsNativeRegister(n->op1)) {
                            push_env1();
                        }
                        if(!env->env_in0  && !IsNativeRegister(n->op0)) {
                            push_env0();
                        }
                    }
                    Assert(!pop_env);
                    break;
                }
                
                if(!IsNativeRegister(n->op0)) {
                    env->reg0_stack = env->env_in0->out_stack;
                    env->reg0 = env->env_in0->out;
                } else {
                    env->reg0 = ToNativeRegister(n->op0);
                }
                if(!IsNativeRegister(n->op1)) {
                    env->reg1_stack = env->env_in1->out_stack;
                    env->reg1 = env->env_in1->out;
                } else {
                    env->reg1 = ToNativeRegister(n->op0);
                }

                if(env->reg1_stack) {
                    env->reg1 = RESERVED_REG0;
                    emit1(PREFIX_REXW);
                    emit1(OPCODE_POP_RM_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, env->reg1);
                }
                if(env->reg0_stack) {
                    env->reg0 = RESERVED_REG1;
                    emit1(PREFIX_REXW);
                    emit1(OPCODE_POP_RM_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, env->reg0);
                }
                
                switch(n->opcode) {
                    case BC_DIV: {
                        // TODO: Handled signed/unsigned

                        bool d_is_free = is_register_free(X64_REG_D);
                        if(!d_is_free) {
                            emit1(OPCODE_PUSH_RM_SLASH_6);
                            emit_modrm_slash(MODE_REG, 6, X64_REG_D);
                        }
                        emit1(PREFIX_REXW);
                        emit1(OPCODE_XOR_REG_RM);
                        emit_modrm(MODE_REG, X64_REG_D, X64_REG_D);
                        
                        bool a_is_free = true;
                        if(env->reg0 != X64_REG_A) {
                            a_is_free = is_register_free(X64_REG_A);
                            if(!a_is_free) {
                                emit1(OPCODE_PUSH_RM_SLASH_6);
                                emit_modrm_slash(MODE_REG, 6, X64_REG_A);
                            }
                            emit1(PREFIX_REXW);
                            emit1(OPCODE_MOV_REG_RM);
                            emit_modrm(MODE_REG, X64_REG_A, env->reg0);
                        }

                        emit1(PREFIX_REXW);
                        emit1(OPCODE_DIV_AX_RM_SLASH_6);
                        emit_modrm_slash(MODE_REG, 6, env->reg1);
                        
                        if(env->reg0 != X64_REG_A) {
                            emit1(PREFIX_REXW);
                            emit1(OPCODE_MOV_REG_RM);
                            emit_modrm(MODE_REG, env->reg0, X64_REG_A);
                        }
                        if(!a_is_free) {
                            emit1(OPCODE_POP_RM_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, X64_REG_A);
                        }
                        if(!d_is_free) {
                            emit1(OPCODE_POP_RM_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, X64_REG_D);
                        }
                    } break;
                    case BC_MOD: {
                        // TODO: Handled signed/unsigned

                        bool d_is_free = is_register_free(X64_REG_D);
                        if(!d_is_free) {
                            emit1(OPCODE_PUSH_RM_SLASH_6);
                            emit_modrm_slash(MODE_REG, 6, X64_REG_D);
                        }
                        emit1(PREFIX_REXW);
                        emit1(OPCODE_XOR_REG_RM);
                        emit_modrm(MODE_REG, X64_REG_D, X64_REG_D);
                        
                        bool a_is_free = true;
                        if(env->reg0 != X64_REG_A) {
                            a_is_free = is_register_free(X64_REG_A);
                            if(!a_is_free) {
                                emit1(OPCODE_PUSH_RM_SLASH_6);
                                emit_modrm_slash(MODE_REG, 6, X64_REG_A);
                            }
                            emit1(PREFIX_REXW);
                            emit1(OPCODE_MOV_REG_RM);
                            emit_modrm(MODE_REG, X64_REG_A, env->reg0);
                        }

                        emit1(PREFIX_REXW);
                        emit1(OPCODE_DIV_AX_RM_SLASH_6);
                        emit_modrm_slash(MODE_REG, 6, env->reg1); 
                        
                        // if(env->reg0 != X64_REG_A) {
                        emit1(PREFIX_REXW);
                        emit1(OPCODE_MOV_REG_RM);
                        emit_modrm(MODE_REG, env->reg0, X64_REG_D);
                        // }
                        if(!a_is_free) {
                            emit1(OPCODE_POP_RM_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, X64_REG_A);
                        }
                        if(!d_is_free) {
                            emit1(OPCODE_POP_RM_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, X64_REG_D);
                        }
                    } break;
                    default: Assert(false);
                }
                
                if(!env->reg1_stack && !IsNativeRegister(n->op1))
                    free_register(env->reg1);
                    
                if(env->reg0_stack) {
                    emit1(OPCODE_PUSH_RM_SLASH_6);
                    emit_modrm_slash(MODE_REG, 6, env->reg0);
                }
                
                env->out = env->reg0;
                env->out_stack = env->reg0_stack;
            } break;
            case BC_LNOT:
            case BC_BNOT: {
                Assert(!ToNativeRegister(n->op0));

                if(!env->env_in1) {
                    if(!env->env_in1 && !IsNativeRegister(n->op1)) {
                        push_env1();
                    }
                    break;
                }

                env->reg0 = alloc_register();
                if(env->reg0 == X64_REG_INVALID) {
                    env->reg0_stack = true;
                    env->reg0 = RESERVED_REG0;
                }

                if(!IsNativeRegister(n->op1)) {
                    env->reg1_stack = env->env_in1->out_stack;
                    env->reg1 = env->env_in1->out;
                }

                if(env->reg1_stack) {
                    env->reg1 = RESERVED_REG0;
                    emit1(OPCODE_POP_RM_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, env->reg1);
                }
                
                switch(n->opcode) {
                    case BC_LNOT: {

                        emit1(PREFIX_REXW);
                        emit1(OPCODE_TEST_RM_REG);
                        emit_modrm(MODE_REG, env->reg1, env->reg1);

                        emit2(OPCODE_2_SETE_RM8);
                        emit_modrm_slash(MODE_REG, 0, env->reg0);

                        // not necessary?
                        // emit1(PREFIX_REXW);
                        // emit2(OPCODE_2_MOVZX_REG_RM8);
                        // emit_modrm(MODE_REG, env->reg0, env->reg0);
                    } break;
                    case BC_BNOT: {
                        // TODO: Optimize by using one register. No need to allocate an IN and OUT register. We can then skip the MOV instruction.
                        emit1(PREFIX_REXW);
                        emit1(OPCODE_MOV_REG_RM);
                        emit_modrm_slash(MODE_REG, env->reg0, env->reg1);

                        emit1(PREFIX_REXW);
                        emit1(OPCODE_NOT_RM_SLASH_2);
                        emit_modrm_slash(MODE_REG, 2, env->reg0);
                    } break;
                    default: Assert(false);
                }
                
                if(!env->reg1_stack && !IsNativeRegister(n->op1))
                    free_register(env->reg1);
                    
                if(env->reg0_stack) {
                    emit1(OPCODE_PUSH_RM_SLASH_6);
                    emit_modrm_slash(MODE_REG, 6, env->reg0);
                }

                env->out = env->reg0;
                env->out_stack = env->reg0_stack;
            } break;
            
            case BC_MOV_RM:
            case BC_MOV_RM_DISP16: {
                Assert(!ToNativeRegister(n->op0));

                Assert(n->op1 != BC_REG_SP && n->op1 != BC_REG_BP);
                Assert(n->control == CONTROL_32B);
                
                env->reg1 = ToNativeRegister(n->op1);
                // Assert(env->reg1 == X64_REG_INVALID);
                
                if(!env->env_in1 && !IsNativeRegister(n->op1)) {
                    auto e = new Env();
                    env->env_in1 = e;
                    e->node = n->in1;

                    envs.add(e);
                    pop_env = false;
                    break;
                }

                env->reg0 = alloc_register();
                if(env->reg0 == X64_REG_INVALID) {
                    env->reg0_stack = true;
                    env->reg0 = RESERVED_REG0;
                }

                if(!IsNativeRegister(n->op1)) {
                    env->reg1_stack = env->env_in1->out_stack;
                    env->reg1 = env->env_in1->out;
                }

                if(env->reg1_stack) {
                    env->reg1 = RESERVED_REG0;
                    emit1(OPCODE_POP_RM_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, env->reg1);
                }

                if(GET_CONTROL_SIZE(n->control) == CONTROL_8B) {
                    emit1(OPCODE_MOV_RM_REG8);
                } else if(GET_CONTROL_SIZE(n->control) == CONTROL_16B) {
                    emit1(PREFIX_16BIT);
                    emit1(OPCODE_MOV_RM_REG);
                } else if(GET_CONTROL_SIZE(n->control) == CONTROL_32B) {
                    emit1(OPCODE_MOV_RM_REG);
                } else if(GET_CONTROL_SIZE(n->control) == CONTROL_64B) {
                    emit1(PREFIX_REXW);
                    emit1(OPCODE_MOV_RM_REG);
                }

                u8 mode = MODE_DEREF_DISP32;
                if(n->imm == 0 || n->opcode == BC_MOV_RM) {
                    mode = MODE_DEREF;
                } else if(n->imm < 0x80 && n->imm >= -0x80) {
                    mode = MODE_DEREF_DISP8;
                }
                if(env->reg1 == X64_REG_SP) {
                    emit_modrm_sib(mode, env->reg0, SIB_SCALE_1, SIB_INDEX_NONE, env->reg1);
                } else {
                    emit_modrm(mode, env->reg0, env->reg1);
                }
                if(mode == MODE_DEREF_DISP8)
                    emit1((u8)(i8)n->imm);
                else if(mode == MODE_DEREF_DISP32)
                    emit4((u32)(i32)n->imm);
                
                if(!env->reg1_stack && !IsNativeRegister(env->reg1))
                    free_register(env->reg1);
                
                env->out = env->reg0;
                env->out_stack = env->reg0_stack;
            } break;
            case BC_MOV_MR:
            case BC_MOV_MR_DISP16: {
                Assert(n->op1 != BC_REG_SP && n->op1 != BC_REG_BP);
                Assert(n->control == CONTROL_32B);
                
                env->reg0 = ToNativeRegister(n->op0);
                // Assert(env->reg0 != X64_REG_INVALID);
                
                env->reg1 = ToNativeRegister(n->op1);
                // Assert(env->reg1 == X64_REG_INVALID);
                
                // TODO: Check node depth, do the most register allocs first
                if(!env->env_in0 && !IsNativeRegister(n->op0)) {
                    push_env0();
                    break;
                }

                if(!env->env_in1 && !IsNativeRegister(n->op1)) {
                    push_env1();
                    break;
                }

                if(!IsNativeRegister(n->op1)) {
                    env->reg1_stack = env->env_in1->out_stack;
                    env->reg1 = env->env_in1->out;
                }
                
                if(!IsNativeRegister(n->op0)) {
                    env->reg0_stack = env->env_in0->out_stack;
                    env->reg0 = env->env_in0->out;
                }
                
                if(env->reg1_stack) {
                    env->reg1 = RESERVED_REG0;
                    emit1(OPCODE_POP_RM_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, env->reg1);
                }

                if(env->reg0_stack) {
                    env->reg0 = RESERVED_REG1;
                    emit1(OPCODE_POP_RM_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, env->reg0);
                }

                if(GET_CONTROL_SIZE(n->control) == CONTROL_8B) {
                    emit1(OPCODE_MOV_RM_REG8);
                } else if(GET_CONTROL_SIZE(n->control) == CONTROL_16B) {
                    emit1(PREFIX_16BIT);
                    emit1(OPCODE_MOV_RM_REG);
                } else if(GET_CONTROL_SIZE(n->control) == CONTROL_32B) {
                    emit1(OPCODE_MOV_RM_REG);
                } else if(GET_CONTROL_SIZE(n->control) == CONTROL_64B) {
                    emit1(PREFIX_REXW);
                    emit1(OPCODE_MOV_RM_REG);
                }

                u8 mode = MODE_DEREF_DISP32;
                if(n->imm == 0 || n->opcode == BC_MOV_MR) {
                    mode = MODE_DEREF;
                } else if(n->imm < 0x80 && n->imm >= -0x80) {
                    mode = MODE_DEREF_DISP8;
                }
                if(env->reg0 == X64_REG_SP) {
                    emit_modrm_sib(mode, env->reg1, SIB_SCALE_1, SIB_INDEX_NONE, env->reg0);
                } else {
                    emit_modrm(mode, env->reg1, env->reg0);
                }
                if(mode == MODE_DEREF) {

                } else  if(mode == MODE_DEREF_DISP8)
                    emit1((u8)(i8)n->imm);
                else
                    emit4((u32)(i32)n->imm);
                
                if(!env->reg0_stack && !IsNativeRegister(env->reg0))
                    free_register(env->reg0);

                if(!env->reg1_stack && !IsNativeRegister(env->reg1))
                    free_register(env->reg1);
            } break;
            case BC_INCR: {
                env->reg0 = ToNativeRegister(n->op0);
                if(env->reg0 == X64_REG_INVALID) {
                    if(!env->env_in0) {
                        push_env0();
                        break;
                    }
                    env->reg0_stack = env->env_in0->out_stack;
                    env->reg0 = env->env_in0->out;
                }

                if(env->reg0_stack) {
                    env->reg0 = RESERVED_REG0;
                    emit1(OPCODE_POP_RM_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, env->reg0);
                }
                if(n->imm < 0) {
                    emit1(PREFIX_REXW);
                    emit1(OPCODE_SUB_RM_IMM_SLASH_5);
                    emit_modrm_slash(MODE_REG, 5, env->reg0);
                    emit4((u32)(i32)-n->imm); // NOTE: cast from i16 to i32 to u32, should be fine
                } else {
                    emit1(PREFIX_REXW);
                    emit1(OPCODE_ADD_RM_IMM_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, env->reg0);
                    emit4((u32)(i32)n->imm); // NOTE: cast from i16 to i32 to u32, should be fine
                }
                if(!env->reg0_stack && !IsNativeRegister(env->reg0))
                    free_register(env->reg0);

                break;
            } break;
            case BC_LI32: {
                env->out = alloc_register();
                if(env->out == X64_REG_INVALID) {
                    env->out_stack = true;
                    env->out = RESERVED_REG0;
                }
                emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                emit_modrm_slash(MODE_REG, 0, env->out);
                emit4((u32)(i32)n->imm);
                
                if(env->out_stack) {
                    emit1(OPCODE_PUSH_RM_SLASH_6);
                    emit_modrm_slash(MODE_REG, 6, env->out);
                }
            } break;
            case BC_LI64: {
                env->out = alloc_register();
                if(env->out == X64_REG_INVALID) {
                    env->out_stack = true;
                    env->out = RESERVED_REG0;
                }
                u8 reg_field = env->out - 1;
                Assert(reg_field >= 0 && reg_field <= 7);

                emit1(PREFIX_REXW);
                emit1((u8)(OPCODE_MOV_REG_IMM_RD_IO | reg_field));
                emit_modrm_slash(MODE_REG, 0, env->out);
                emit8((u64)n->imm);
                
                if(env->out_stack) {
                    emit1(OPCODE_PUSH_RM_SLASH_6);
                    emit_modrm_slash(MODE_REG, 6, env->out);
                }
            } break;
            case BC_CALL: {
                Assert(false);
            } break;
            case BC_RET: {
                emit1(OPCODE_POP_RM_SLASH_0);
                emit_modrm_slash(MODE_REG, 0, X64_REG_BP);

                emit1(OPCODE_RET);
            } break;
            default: Assert(false);
        }
        if(pop_env) {
            envs.pop();   
        }
    }
}
#ifdef gone
void X64Builder::generateInstructions(int depth, BCRegister find_reg, int origin_inst_index, X64Register* out_reg) {
    using namespace engone;
    auto& instructions = tinycode->instructionSegment;
    bool logging = false;
    bool gen_forward = depth == 0;
    
    if(logging && gen_forward)
        log::out << log::GOLD << "Code gen\n";
    
    // log::out << "GEN depth: "<<depth << " " << find_reg << "\n"; 
    
    bool find_push = false;
    int push_level = 0;
    bool complete = false;
    i64 pc = 0;
    int next_inst_index = origin_inst_index;
    while(!complete) {
        if(!(pc < instructions.size()))
            break;
        i64 prev_pc = pc;
        i64 inst_index = next_inst_index;
        
        InstructionType opcode{};
        if(gen_forward) {
            opcode = (InstructionType)instructions[pc];
            pc++;
            instruction_indices.push_back(prev_pc);
        } else {
            opcode = (InstructionType)instructions[instruction_indices[inst_index]];
            next_inst_index--;
        }
        
        BCRegister op0=BC_REG_INVALID, op1=BC_REG_INVALID, op2=BC_REG_INVALID;
        InstructionControl control;
        InstructionCast cast;
        i64 imm;
        
        // _ILOG(log::out <<log::GRAY<<" sp: "<< sp <<" fp: "<<fp<<"\n";)
        if(logging) {
            tinycode->print(prev_pc, prev_pc + 1, bytecode);
            log::out << "\n";
        }

        switch (opcode) {
        case BC_HALT: {
            INCOMPLETE
        } break;
        case BC_NOP: {
            INCOMPLETE
        } break;
        case BC_MOV_RR: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            // auto reg0 = builder.allocate_register();
            // auto reg1 = builder.allocate_register();

            // builder.emit1(PREFIX_REX);
            // builder.emit1(OPCODE_MOV_REG_RM);
            // builder.emit_modrm(MODE_REG, reg0, reg1);

            // values in registers
            // values on stack
            // values in memory


            // registers[op0] = registers[op1];
        } break;
        case BC_MOV_RM: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            // if(control & CONTROL_8B) registers[op0] = *(i8*)registers[op1];
            // else if(control & CONTROL_16B) registers[op0] = *(i16*)registers[op1];
            // else if(control & CONTROL_32B) registers[op0] = *(i32*)registers[op1];
            // else if(control & CONTROL_64B) registers[op0] = *(i64*)registers[op1];
        } break;
        case BC_MOV_MR: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            // 1 + 2 + 3 + 4 + (5 + 6 + 7 + 8)
            
            // if(control & CONTROL_8B) *(i8*)registers[op0] = registers[op1];
            // else if(control & CONTROL_16B) *(i16*)registers[op0] = registers[op1];
            // else if(control & CONTROL_32B) *(i32*)registers[op0] = registers[op1];
            // else if(control & CONTROL_64B) *(i64*)registers[op0] = registers[op1];
        } break;
        case BC_MOV_RM_DISP16: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            pc += 2;
            // INCOMPLETE
        } break;
        case BC_MOV_MR_DISP16: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc += 2;
            
            Assert(op1 != BC_REG_SP && op1 != BC_REG_BP);
            Assert(control == CONTROL_32B);
            // TODO: op0, op1 as stack or base pointer requires special handling
            
            
            X64Register reg_op0{};
            reg_op0 = ToNativeRegister(op0);
            if(reg_op0 == X64_REG_INVALID) {
                generateInstructions(depth + 1, op0, instruction_indices.size() - 2, &reg_op0);
            }
            
            X64Register reg_op1{};
            reg_op1 = ToNativeRegister(op1);
            if(reg_op1 == X64_REG_INVALID) {
                generateInstructions(depth + 1, op1, instruction_indices.size() - 2, &reg_op1);
            }
            u8 mode = MODE_DEREF_DISP32;
            if(imm < 0x80 && imm >= -0x80)
                mode = MODE_DEREF_DISP8;
            emit1(OPCODE_MOV_RM_REG);
            if(reg_op0 == X64_REG_SP) {
                emit_modrm_sib(mode, reg_op1, SIB_SCALE_1, SIB_INDEX_NONE, reg_op0);
            } else {
                emit_modrm(mode, reg_op1, reg_op0);
            }
            if(mode == MODE_DEREF_DISP8)
                emit1((u8)(i8)imm);
            else
                emit4((u32)(i32)imm);
            
            // generate op0 (address) (not needed if sp or bp)
            // generate op1 (value) (not needed if sp or bp, maybe same with rax?)
        } break;
        case BC_PUSH: {
            if(!gen_forward) {
                op0 = (BCRegister)instructions[instruction_indices[inst_index] + 1];
                if(find_push) {
                    if(push_level == 0) {
                        find_reg = op0;
                        find_push = false;
                    } else {
                        push_level--;
                    }
                }
            } else {
                // ignore   
                op0 = (BCRegister)instructions[pc++];
            }
        } break;
        case BC_POP: {
            if(!gen_forward) {
                op0 = (BCRegister)instructions[instruction_indices[inst_index] + 1];
                            
                if(find_push) {
                    push_level++;
                } else if (!find_push && find_reg == op0) {
                    // Value we are looking for, is on the stack.
                    // Now we must find the push instruction
                    // Assert(false);
                    find_push = true;
                }
            } else {
                // ignore
                op0 = (BCRegister)instructions[pc++];
            }
        } break;
        case BC_LI32: {
            if(!gen_forward) {
                op0 = (BCRegister)instructions[instruction_indices[inst_index] + 1];
                imm = *(i32*)&instructions[instruction_indices[inst_index] + 2];
                
                if(!find_push && find_reg == op0) {
                    X64Register reg = alloc_register(false);
                    // Assert(reg != X64_REG_INVALID);
                    
                    if(reg == X64_REG_INVALID) {
                        reg = X64_REG_A;
                        
                        emit1(OPCODE_PUSH_RM_SLASH_6);
                        emit_modrm_slash(MODE_REG, 6, reg);
                        
                    }
                    
                    emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, reg);
                    emit4((u32)(i32)imm);
                    complete = true;
                    
                    Assert(out_reg);
                    *out_reg = reg;
                }
            } else {
                // ignore
                op0 = (BCRegister)instructions[pc++];
                imm = *(i32*)&instructions[pc];
                pc+=4;
            }
        } break;
        case BC_LI64: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i64*)&instructions[pc];
            pc+=8;
            // registers[op0] = imm;
        } break;
        case BC_INCR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            // registers[op0] += imm;
        } break;
        case BC_JMP: {
            imm = *(i32*)&instructions[pc];
            pc+=4;
            // pc += imm;
        } break;
        case BC_JZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            // if(op0 == 0)
            //     pc += imm;
        } break;
        case BC_JNZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            // if(op0 != 0)
            //     pc += imm;
        } break;
        case BC_CALL: {
            LinkConventions l = (LinkConventions)instructions[pc++];
            CallConventions c = (CallConventions)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;

            // registers[BC_REG_SP] -= 8;
            // CHECK_STACK();
            // *(i64*)registers[BC_REG_SP] = pc | ((i64)tiny_index << 32);

            // registers[BC_REG_SP] -= 8;
            // CHECK_STACK();
            // *(i64*)registers[BC_REG_SP] = registers[BC_REG_BP];
            
            // registers[BC_REG_BP] = registers[BC_REG_SP];

        } break;
        case BC_RET: {
            // i64 diff = registers[BC_REG_SP] - (i64)stack.data;
            // if(diff == (i64)(stack.max)) {
            //     running = false;
            //     break;
            // }

            // registers[BC_REG_BP] = *(i64*)registers[BC_REG_SP];
            // registers[BC_REG_SP] += 8;
            // CHECK_STACK();

            // i64 encoded_pc = *(i64*)registers[BC_REG_SP];
            // registers[BC_REG_SP] += 8;
            // CHECK_STACK();

            // pc = encoded_pc & 0xFFFF'FFFF;
            // tiny_index = encoded_pc >> 32;
            // tinycode = bytecode->tinyBytecodes[tiny_index];
        } break;
        case BC_DATAPTR: {
            Assert(false);
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
        } break;
        case BC_CODEPTR: {
            Assert(false);
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
        } break;
        case BC_CAST: {
            op0 = (BCRegister)instructions[pc++];
            InstructionCast cast = (InstructionCast)instructions[pc++];
            u8 fsize = (u8)instructions[pc++];
            u8 tsize = (u8)instructions[pc++];

            // switch(cast){
            // case CAST_UINT_UINT:
            // case CAST_UINT_SINT:
            // case CAST_SINT_UINT:
            // case CAST_SINT_SINT: {
            //     u64 tmp = registers[op0];
            //     if(tsize == 1) *(u8*)&registers[op0] = tmp;
            //     else if(tsize == 2) *(u16*)&registers[op0] = tmp;
            //     else if(tsize == 4) *(u32*)&registers[op0] = tmp;
            //     else if(tsize == 8) *(u64*)&registers[op0] = tmp;
            //     else Assert(false);
            // } break;
            // case CAST_UINT_FLOAT: {
            //     u64 tmp = registers[op0];
            //     if(tsize == 4) *(float*)&registers[op0] = tmp;
            //     else if(tsize == 8) *(double*)&registers[op0] = tmp;
            //     else Assert(false);
            // } break;
            // case CAST_SINT_FLOAT: {
            //     i64 tmp = registers[op0];
            //     if(tsize == 4) *(float*)&registers[op0] = tmp;
            //     else if(tsize == 8) *(double*)&registers[op0] = tmp;
            //     else Assert(false);
            // } break;
            // case CAST_FLOAT_UINT: {
            //     if(fsize == 4){
            //         float tmp = *(float*)&registers[op0];
            //         if(tsize == 1) *(u8*)&registers[op0] = tmp;
            //         else if(tsize == 2) *(u16*)&registers[op0] = tmp;
            //         else if(tsize == 4) *(u32*)&registers[op0] = tmp;
            //         else if(tsize == 8) *(u64*)&registers[op0] = tmp;
            //         else Assert(false);
            //     } else if(fsize == 8) {
            //         double tmp = *(double*)&registers[op0];
            //         if(tsize == 1) *(u8*)&registers[op0] = tmp;
            //         else if(tsize == 2) *(u16*)&registers[op0] = tmp;
            //         else if(tsize == 4) *(u32*)&registers[op0] = tmp;
            //         else if(tsize == 8) *(u64*)&registers[op0] = tmp;
            //         else Assert(false);
            //     } else Assert(false);
            // } break;
            // case CAST_FLOAT_SINT: {
            //     if(fsize == 4){
            //         float tmp = *(float*)&registers[op0];
            //         if(tsize == 1) *(i8*)&registers[op0] = tmp;
            //         else if(tsize == 2) *(i16*)&registers[op0] = tmp;
            //         else if(tsize == 4) *(i32*)&registers[op0] = tmp;
            //         else if(tsize == 8) *(i64*)&registers[op0] = tmp;
            //         else Assert(false);
            //     } else if(fsize == 8) {
            //         double tmp = *(double*)&registers[op0];
            //         if(tsize == 1) *(i8*)&registers[op0] = tmp;
            //         else if(tsize == 2) *(i16*)&registers[op0] = tmp;
            //         else if(tsize == 4) *(i32*)&registers[op0] = tmp;
            //         else if(tsize == 8) *(i64*)&registers[op0] = tmp;
            //         else Assert(false);
            //     } else Assert(false);
            // } break;
            // case CAST_FLOAT_FLOAT: {
            //     if(fsize == 4 && tsize == 4) *(float*)&registers[op0] = *(float*)registers[op0];
            //     else if(fsize == 4 && tsize == 8) *(double*)&registers[op0] = *(float*)registers[op0];
            //     else if(fsize == 8 && tsize == 4) *(float*)&registers[op0] = *(double*)registers[op0];
            //     else if(fsize == 8 && tsize == 8) *(double*)&registers[op0] = *(double*)registers[op0];
            //     else Assert(false);
            // } break;
            // default: Assert(false);
            // }
            
        } break;
        case BC_MEMZERO: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            memset((void*)op0,0, op1);
        } break;
        case BC_MEMCPY: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            op2 = (BCRegister)instructions[pc++];
            memcpy((void*)op0,(void*)op1, op2);
        } break;
        case BC_ADD:
        case BC_SUB:
        case BC_MUL:
        case BC_DIV:
        case BC_MOD:
        case BC_EQ:
        case BC_NEQ:
        case BC_LT:
        case BC_LTE:
        case BC_GT:
        case BC_GTE: {
            if(!gen_forward) {
                op0 = (BCRegister)instructions[instruction_indices[inst_index] + 1];
                op1 = (BCRegister)instructions[instruction_indices[inst_index] + 2];
                control = (InstructionControl)instructions[instruction_indices[inst_index] + 3];
                
                if(!find_push && find_reg == op0) {
                    // It is time to generate add instruction.
                    // But first we must generate values for operands.
                    
                    X64Register reg0 = ToNativeRegister(op0);
                    if(reg0 == X64_REG_INVALID) {
                        generateInstructions(depth + 1, op0, inst_index - 1, &reg0);
                    }
                    
                    X64Register reg1 = ToNativeRegister(op1);
                    if(reg1 == X64_REG_INVALID) {
                        generateInstructions(depth + 1, op1, inst_index - 1, &reg1);
                    }
                    
                    switch(opcode) {
                        case BC_ADD: {
                            // emit1(PREFIX_REXW);
                            emit1(OPCODE_ADD_RM_REG);
                            emit_modrm(MODE_REG, reg1, reg0);
                        } break;
                        case BC_SUB: {
                            // emit1(PREFIX_REXW);
                            emit1(OPCODE_SUB_REG_RM);
                            emit_modrm(MODE_REG, reg0, reg1);
                        } break;
                        case BC_MUL: {
                            // TODO: Handle signed/unsigned multiplication (using InstructionControl)
                            emit2(OPCODE_2_IMUL_REG_RM);
                            emit_modrm(MODE_REG, reg1, reg0);
                        } break;
                        default: Assert(false);
                    }
                    
                    free_register(reg1);
                    
                    Assert(out_reg);
                    *out_reg = reg0;
                    
                    complete = true;
                }
            } else {
                // ignore
                op0 = (BCRegister)instructions[pc++];
                op1 = (BCRegister)instructions[pc++];
                control = (InstructionControl)instructions[pc++];
            }

            // nocheckin, we assume 32 bit float operation
            
            // if(control & CONTROL_FLOAT_OP) {
            //     switch(opcode){
            //         #define OP(P) *(float*)&registers[op0] = *(float*)&registers[op0] P *(float*)&registers[op1]; break;
            //         case BC_ADD: OP(+)
            //         case BC_SUB: OP(-)
            //         case BC_MUL: OP(*)
            //         case BC_DIV: OP(/)
            //         case BC_MOD: *(float*)&registers[op0] = fmodf(*(float*)&registers[op0], *(float*)&registers[op1]); break;
            //         case BC_EQ:  OP(==)
            //         case BC_NEQ: OP(!=)
            //         case BC_LT:  OP(<)
            //         case BC_LTE: OP(<=)
            //         case BC_GT:  OP(>)
            //         case BC_GTE: OP(>=)
            //         default: Assert(false);
            //         #undef OP
            //     }
            // } else {
            //     switch(opcode){
            //         #define OP(P) registers[op0] = registers[op0] P registers[op1]; break;
            //         case BC_ADD: OP(+)
            //         case BC_SUB: OP(-)
            //         case BC_MUL: OP(*)
            //         case BC_DIV: OP(/)
            //         case BC_MOD: OP(%)
            //         case BC_EQ:  OP(==)
            //         case BC_NEQ: OP(!=)
            //         case BC_LT:  OP(<)
            //         case BC_LTE: OP(<=)
            //         case BC_GT:  OP(>)
            //         case BC_GTE: OP(>=)
            //         default: Assert(false);
            //         #undef OP
            //     }
            // }
        } break;
        case BC_LAND:
        case BC_LOR:
        case BC_LNOT:
        case BC_BAND:
        case BC_BOR:
        case BC_BXOR:
        case BC_BLSHIFT:
        case BC_BRSHIFT: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            // switch(opcode){
            //     #define OP(P) registers[op0] = registers[op0] P registers[op1]; break;
            //     case BC_BXOR: OP(^)
            //     case BC_BOR: OP(|)
            //     case BC_BAND: OP(&)
            //     case BC_BLSHIFT: OP(<<)
            //     case BC_BRSHIFT: OP(>>)
            //     case BC_LAND: OP(&&)
            //     case BC_LOR: OP(||)
            //     case BC_LNOT: registers[op0] = !registers[op1];
            //     default: Assert(false);
            //     #undef OP
            // }
        } break;
        }
    }
}
#endif
X64Register X64Builder::alloc_register(X64Register reg, bool is_float) {
    if(!is_float) {
        if(reg != X64_REG_INVALID) {
            auto pair = registers.find(reg);
            if(pair == registers.end()) {
                registers[reg] = {};
                registers[reg].used = true;
                return reg;
            } else if(!pair->second.used) {
                pair->second.used = true;
                return reg;
            }
        } else {
            static const X64Register regs[]{
                // X64_REG_A,
                X64_REG_C,
                // X64_REG_D,
                X64_REG_B,
                X64_REG_SI,
                X64_REG_DI,

                // X64_REG_R8, 
                // X64_REG_R9, 
                // X64_REG_R10,
                // X64_REG_R11,
                // X64_REG_R12,
                // X64_REG_R13,
                // X64_REG_R14,
                // X64_REG_R15,
            };
            for(int i = 0; i < sizeof(regs)/sizeof(*regs);i++) {
                X64Register reg = regs[i];
                auto pair = registers.find(reg);
                if(pair == registers.end()) {
                    registers[reg] = {};
                    registers[reg].used = true;
                    return reg;
                } else if(!pair->second.used) {
                    pair->second.used = true;
                    return reg;
                }
            }
        }
    } else {
        INCOMPLETE
    }
    // Assert(("out of registers",false));
    return X64_REG_INVALID;
}
bool X64Builder::is_register_free(X64Register reg) {
    auto pair = registers.find(reg);
    if(pair == registers.end()) {
        return true;
    }
    return false;
}
void X64Builder::free_register(X64Register reg) {
    auto pair = registers.find(reg);
    if(pair == registers.end()) {
        Assert(false);
    }
    pair->second.used = false;
}

void X64Builder::generateInstructions_slow() {
    using namespace engone;
    auto& instructions = tinycode->instructionSegment;
    bool logging = false;
    
    if(logging)
        log::out << log::GOLD << "Code gen\n";
    
    // log::out << "GEN depth: "<<depth << " " << find_reg << "\n"; 
    
    i64 pc = 0;
    while(true) {
        if(!(pc < instructions.size()))
            break;
        i64 prev_pc = pc;
        
        InstructionType opcode{};
        opcode = (InstructionType)instructions[pc++];
        
        BCRegister op0=BC_REG_INVALID, op1=BC_REG_INVALID, op2=BC_REG_INVALID;
        InstructionControl control;
        InstructionCast cast;
        i64 imm;
        
        // _ILOG(log::out <<log::GRAY<<" sp: "<< sp <<" fp: "<<fp<<"\n";)
        if(logging) {
            tinycode->print(prev_pc, prev_pc + 1, bytecode);
            log::out << "\n";
        }

        switch (opcode) {
        case BC_HALT: {
            INCOMPLETE
        } break;
        case BC_NOP: {
            INCOMPLETE
        } break;
        case BC_MOV_RR: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            // auto reg0 = builder.allocate_register();
            // auto reg1 = builder.allocate_register();

            // builder.emit1(PREFIX_REX);
            // builder.emit1(OPCODE_MOV_REG_RM);
            // builder.emit_modrm(MODE_REG, reg0, reg1);

            // values in registers
            // values on stack
            // values in memory


            // registers[op0] = registers[op1];
        } break;
        case BC_MOV_RM: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            // if(control & CONTROL_8B) registers[op0] = *(i8*)registers[op1];
            // else if(control & CONTROL_16B) registers[op0] = *(i16*)registers[op1];
            // else if(control & CONTROL_32B) registers[op0] = *(i32*)registers[op1];
            // else if(control & CONTROL_64B) registers[op0] = *(i64*)registers[op1];
        } break;
        case BC_MOV_MR: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            // 1 + 2 + 3 + 4 + (5 + 6 + 7 + 8)
            
            // if(control & CONTROL_8B) *(i8*)registers[op0] = registers[op1];
            // else if(control & CONTROL_16B) *(i16*)registers[op0] = registers[op1];
            // else if(control & CONTROL_32B) *(i32*)registers[op0] = registers[op1];
            // else if(control & CONTROL_64B) *(i64*)registers[op0] = registers[op1];
        } break;
        case BC_MOV_RM_DISP16: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            pc += 2;
            // INCOMPLETE
        } break;
        case BC_MOV_MR_DISP16: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc += 2;
            
            Assert(op1 != BC_REG_SP && op1 != BC_REG_BP);
            Assert(control == CONTROL_32B);
            // TODO: op0, op1 as stack or base pointer requires special handling
            
            
            X64Register reg_op0{};
            X64Register reg_op1{};
            u8 mode = MODE_DEREF_DISP32;
            if(imm < 0x80 && imm >= -0x80)
                mode = MODE_DEREF_DISP8;
            emit1(PREFIX_REXW);
            emit1(OPCODE_MOV_RM_REG);
            if(reg_op0 == X64_REG_SP) {
                emit_modrm_sib(mode, reg_op1, SIB_SCALE_1, SIB_INDEX_NONE, reg_op0);
            } else {
                emit_modrm(mode, reg_op1, reg_op0);
            }
            if(mode == MODE_DEREF_DISP8)
                emit1((u8)(i8)imm);
            else
                emit4((u32)(i32)imm);
            
            // generate op0 (address) (not needed if sp or bp)
            // generate op1 (value) (not needed if sp or bp, maybe same with rax?)
        } break;
        case BC_PUSH: {
            op0 = (BCRegister)instructions[pc++];
        } break;
        case BC_POP: {
            op0 = (BCRegister)instructions[pc++];
        } break;
        case BC_LI32: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
        } break;
        case BC_LI64: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i64*)&instructions[pc];
            pc+=8;
            // registers[op0] = imm;
        } break;
        case BC_INCR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            // registers[op0] += imm;
        } break;
        case BC_JMP: {
            imm = *(i32*)&instructions[pc];
            pc+=4;
            // pc += imm;
        } break;
        case BC_JZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            // if(op0 == 0)
            //     pc += imm;
        } break;
        case BC_JNZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            // if(op0 != 0)
            //     pc += imm;
        } break;
        case BC_CALL: {
            LinkConventions l = (LinkConventions)instructions[pc++];
            CallConventions c = (CallConventions)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;

            // registers[BC_REG_SP] -= 8;
            // CHECK_STACK();
            // *(i64*)registers[BC_REG_SP] = pc | ((i64)tiny_index << 32);

            // registers[BC_REG_SP] -= 8;
            // CHECK_STACK();
            // *(i64*)registers[BC_REG_SP] = registers[BC_REG_BP];
            
            // registers[BC_REG_BP] = registers[BC_REG_SP];

        } break;
        case BC_RET: {
            // i64 diff = registers[BC_REG_SP] - (i64)stack.data;
            // if(diff == (i64)(stack.max)) {
            //     running = false;
            //     break;
            // }

            // registers[BC_REG_BP] = *(i64*)registers[BC_REG_SP];
            // registers[BC_REG_SP] += 8;
            // CHECK_STACK();

            // i64 encoded_pc = *(i64*)registers[BC_REG_SP];
            // registers[BC_REG_SP] += 8;
            // CHECK_STACK();

            // pc = encoded_pc & 0xFFFF'FFFF;
            // tiny_index = encoded_pc >> 32;
            // tinycode = bytecode->tinyBytecodes[tiny_index];
        } break;
        case BC_DATAPTR: {
            Assert(false);
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
        } break;
        case BC_CODEPTR: {
            Assert(false);
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
        } break;
        case BC_CAST: {
            op0 = (BCRegister)instructions[pc++];
            InstructionCast cast = (InstructionCast)instructions[pc++];
            u8 fsize = (u8)instructions[pc++];
            u8 tsize = (u8)instructions[pc++];

            // switch(cast){
            // case CAST_UINT_UINT:
            // case CAST_UINT_SINT:
            // case CAST_SINT_UINT:
            // case CAST_SINT_SINT: {
            //     u64 tmp = registers[op0];
            //     if(tsize == 1) *(u8*)&registers[op0] = tmp;
            //     else if(tsize == 2) *(u16*)&registers[op0] = tmp;
            //     else if(tsize == 4) *(u32*)&registers[op0] = tmp;
            //     else if(tsize == 8) *(u64*)&registers[op0] = tmp;
            //     else Assert(false);
            // } break;
            // case CAST_UINT_FLOAT: {
            //     u64 tmp = registers[op0];
            //     if(tsize == 4) *(float*)&registers[op0] = tmp;
            //     else if(tsize == 8) *(double*)&registers[op0] = tmp;
            //     else Assert(false);
            // } break;
            // case CAST_SINT_FLOAT: {
            //     i64 tmp = registers[op0];
            //     if(tsize == 4) *(float*)&registers[op0] = tmp;
            //     else if(tsize == 8) *(double*)&registers[op0] = tmp;
            //     else Assert(false);
            // } break;
            // case CAST_FLOAT_UINT: {
            //     if(fsize == 4){
            //         float tmp = *(float*)&registers[op0];
            //         if(tsize == 1) *(u8*)&registers[op0] = tmp;
            //         else if(tsize == 2) *(u16*)&registers[op0] = tmp;
            //         else if(tsize == 4) *(u32*)&registers[op0] = tmp;
            //         else if(tsize == 8) *(u64*)&registers[op0] = tmp;
            //         else Assert(false);
            //     } else if(fsize == 8) {
            //         double tmp = *(double*)&registers[op0];
            //         if(tsize == 1) *(u8*)&registers[op0] = tmp;
            //         else if(tsize == 2) *(u16*)&registers[op0] = tmp;
            //         else if(tsize == 4) *(u32*)&registers[op0] = tmp;
            //         else if(tsize == 8) *(u64*)&registers[op0] = tmp;
            //         else Assert(false);
            //     } else Assert(false);
            // } break;
            // case CAST_FLOAT_SINT: {
            //     if(fsize == 4){
            //         float tmp = *(float*)&registers[op0];
            //         if(tsize == 1) *(i8*)&registers[op0] = tmp;
            //         else if(tsize == 2) *(i16*)&registers[op0] = tmp;
            //         else if(tsize == 4) *(i32*)&registers[op0] = tmp;
            //         else if(tsize == 8) *(i64*)&registers[op0] = tmp;
            //         else Assert(false);
            //     } else if(fsize == 8) {
            //         double tmp = *(double*)&registers[op0];
            //         if(tsize == 1) *(i8*)&registers[op0] = tmp;
            //         else if(tsize == 2) *(i16*)&registers[op0] = tmp;
            //         else if(tsize == 4) *(i32*)&registers[op0] = tmp;
            //         else if(tsize == 8) *(i64*)&registers[op0] = tmp;
            //         else Assert(false);
            //     } else Assert(false);
            // } break;
            // case CAST_FLOAT_FLOAT: {
            //     if(fsize == 4 && tsize == 4) *(float*)&registers[op0] = *(float*)registers[op0];
            //     else if(fsize == 4 && tsize == 8) *(double*)&registers[op0] = *(float*)registers[op0];
            //     else if(fsize == 8 && tsize == 4) *(float*)&registers[op0] = *(double*)registers[op0];
            //     else if(fsize == 8 && tsize == 8) *(double*)&registers[op0] = *(double*)registers[op0];
            //     else Assert(false);
            // } break;
            // default: Assert(false);
            // }
            
        } break;
        case BC_MEMZERO: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
        } break;
        case BC_MEMCPY: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            op2 = (BCRegister)instructions[pc++];
        } break;
        case BC_ADD:
        case BC_SUB:
        case BC_MUL:
        case BC_DIV:
        case BC_MOD:
        case BC_EQ:
        case BC_NEQ:
        case BC_LT:
        case BC_LTE:
        case BC_GT:
        case BC_GTE: {
                   
                op0 = (BCRegister)instructions[pc++];
                op1 = (BCRegister)instructions[pc++];
                control = (InstructionControl)instructions[pc++];
            


            // nocheckin, we assume 32 bit float operation
            
            // if(control & CONTROL_FLOAT_OP) {
            //     switch(opcode){
            //         #define OP(P) *(float*)&registers[op0] = *(float*)&registers[op0] P *(float*)&registers[op1]; break;
            //         case BC_ADD: OP(+)
            //         case BC_SUB: OP(-)
            //         case BC_MUL: OP(*)
            //         case BC_DIV: OP(/)
            //         case BC_MOD: *(float*)&registers[op0] = fmodf(*(float*)&registers[op0], *(float*)&registers[op1]); break;
            //         case BC_EQ:  OP(==)
            //         case BC_NEQ: OP(!=)
            //         case BC_LT:  OP(<)
            //         case BC_LTE: OP(<=)
            //         case BC_GT:  OP(>)
            //         case BC_GTE: OP(>=)
            //         default: Assert(false);
            //         #undef OP
            //     }
            // } else {
            //     switch(opcode){
            //         #define OP(P) registers[op0] = registers[op0] P registers[op1]; break;
            //         case BC_ADD: OP(+)
            //         case BC_SUB: OP(-)
            //         case BC_MUL: OP(*)
            //         case BC_DIV: OP(/)
            //         case BC_MOD: OP(%)
            //         case BC_EQ:  OP(==)
            //         case BC_NEQ: OP(!=)
            //         case BC_LT:  OP(<)
            //         case BC_LTE: OP(<=)
            //         case BC_GT:  OP(>)
            //         case BC_GTE: OP(>=)
            //         default: Assert(false);
            //         #undef OP
            //     }
            // }
        } break;
        case BC_LAND:
        case BC_LOR:
        case BC_LNOT:
        case BC_BAND:
        case BC_BOR:
        case BC_BXOR:
        case BC_BLSHIFT:
        case BC_BRSHIFT: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            // switch(opcode){
            //     #define OP(P) registers[op0] = registers[op0] P registers[op1]; break;
            //     case BC_BXOR: OP(^)
            //     case BC_BOR: OP(|)
            //     case BC_BAND: OP(&)
            //     case BC_BLSHIFT: OP(<<)
            //     case BC_BRSHIFT: OP(>>)
            //     case BC_LAND: OP(&&)
            //     case BC_LOR: OP(||)
            //     case BC_LNOT: registers[op0] = !registers[op1];
            //     default: Assert(false);
            //     #undef OP
            // }
        } break;
        }
    }
}

#ifdef gone
void X64Program::generateFromTinycode(Bytecode* code, TinyBytecode* tinycode) {
    using namespace engone;
    
    struct Inst {
        InstructionType opcode;
        BCRegister op0;
        BCRegister op1;
        InstructionControl control;
        i64 imm;
    };
    DynamicArray<Inst> insts{};
    
    struct Reg {
        bool in_use = false; // occupied
        bool used_at_least_once = false;
    };
    Reg registers[BC_REG_MAX];
    registers[BC_REG_SP].in_use = true;
    registers[BC_REG_BP].in_use = true;
    
    int pc = 0;
    bool running = true;
    while(running) {
        i64 prev_pc = pc;
        if(pc>=(u64)tinycode->instructionSegment.used)
            break;

        InstructionType opcode = (InstructionType)tinycode->instructionSegment[pc++];
        
        BCRegister op0=BC_REG_INVALID, op1, op2;
        InstructionControl control;
        InstructionCast cast;
        i64 imm;
        
        if(opcode == BC_LI32) {
            op0 = (BCRegister)tinycode->instructionSegment[pc++];
            imm = *(i32*)&tinycode->instructionSegment[pc];
            pc+=4;
            insts.add({opcode, op0, BC_REG_INVALID, CONTROL_NONE, imm});
            
            registers[op0].in_use = true;
        } else if(opcode == BC_PUSH) {
            op0 = (BCRegister)tinycode->instructionSegment[pc++];
            insts.add({opcode, op0});
            registers[op0].used_at_least_once = true;
        } else if(opcode == BC_MOV_RR) {
            op0 = (BCRegister)tinycode->instructionSegment[pc++];
            op1 = (BCRegister)tinycode->instructionSegment[pc++];
            insts.add({opcode, op0, op1});
            registers[op0].in_use = true;
            registers[op1].used_at_least_once = true;
            Assert(registers[op1].in_use);
        } else if(opcode == BC_INCR) {
            op0 = (BCRegister)tinycode->instructionSegment[pc++];
            imm = *(i32*)&tinycode->instructionSegment[pc];
            pc+=4;
            insts.add({opcode, op0, BC_REG_INVALID, CONTROL_NONE, imm});
            Assert(registers[op0].in_use);
        } else if(opcode == BC_POP) {
            op0 = (BCRegister)tinycode->instructionSegment[pc++];
            
            bool modified = false;
            for(int i=insts.size()-1;i>=0;i--) {
                auto& inst = insts[i];
                if(inst.opcode == BC_PUSH) {
                    modified = true;
                    
                    /*
                    li a, 5
                    push a
                    li a, 5
                    push a
                    pop a
                    pop b
                    add a, b
                    push a
                    pop a
                    mov_mr bp, a
                    
                    li a, 5
                    add a, 5
                    mov mr bp, a
                    
                    a, b
                    [0, 0]
                    */
                    
                    // inst.op0
                    // break;
                }
            }
            if(!modified) {
                insts.add({ opcode, op0 });
                Assert(registers[op0].in_use);
            }
        }
    }
}

#endif
bool X64Builder::_reserve(u32 newAllocationSize){
    if(newAllocationSize==0){
        if(tinyprog->_allocationSize!=0){
            TRACK_ARRAY_FREE(tinyprog->text, u8, tinyprog->_allocationSize);
            // engone::Free(text, _allocationSize);
        }
        tinyprog->text = nullptr;
        tinyprog->_allocationSize = 0;
        tinyprog->head = 0;
        return true;
    }
    if(!tinyprog->text){
        // text = (u8*)engone::Allocate(newAllocationSize);
        tinyprog->text = TRACK_ARRAY_ALLOC(u8, newAllocationSize);
        Assert(tinyprog->text);
        // initialization of elements is done when adding them
        if(!tinyprog->text)
            return false;
        tinyprog->_allocationSize = newAllocationSize;
        return true;
    } else {
        TRACK_DELS(u8, tinyprog->_allocationSize);
        u8* newText = (u8*)engone::Reallocate(tinyprog->text, tinyprog->_allocationSize, newAllocationSize);
        TRACK_ADDS(u8, newAllocationSize);
        Assert(newText);
        if(!newText)
            return false;
        tinyprog->text = newText;
        tinyprog->_allocationSize = newAllocationSize;
        if(tinyprog->head > newAllocationSize){
            tinyprog->head = newAllocationSize;
        }
        return true;
    }
    return false;
}

void X64Builder::emit1(u8 byte){
    ensure_bytes(1);
    *(tinyprog->text + tinyprog->head) = byte;
    tinyprog->head++;
}
void X64Builder::emit2(u16 word){
    ensure_bytes(2);
    
    auto ptr = (u8*)&word;
    *(tinyprog->text + tinyprog->head + 0) = *(ptr + 0);
    *(tinyprog->text + tinyprog->head + 1) = *(ptr + 1);
    tinyprog->head+=2;
}
void X64Builder::emit3(u32 word){
    if(tinyprog->head>0){
        // This is not how you use rex prefix
        // cvtsi2ss instructions use smashed in between it's other opcode bytes
        Assert(*(tinyprog->text + tinyprog->head - 1) != PREFIX_REXW);
    }
    Assert(0==(word&0xFF000000));
    ensure_bytes(3);
    
    auto ptr = (u8*)&word;
    *(tinyprog->text + tinyprog->head + 0) = *(ptr + 0);
    *(tinyprog->text + tinyprog->head + 1) = *(ptr + 1);
    *(tinyprog->text + tinyprog->head + 2) = *(ptr + 2);
    tinyprog->head+=3;
}
void X64Builder::emit4(u32 dword){
    // wish we could assert to find bugs but immediates may be zero and won't work.
    // we could make a nother functionn specifcially for immediates though.
    // Assert(dword & 0xFF00'0000);
    ensure_bytes(4);
    auto ptr = (u8*)&dword;

    // deals with non-alignment
    *(tinyprog->text + tinyprog->head + 0) = *(ptr + 0);
    *(tinyprog->text + tinyprog->head + 1) = *(ptr + 1);
    *(tinyprog->text + tinyprog->head + 2) = *(ptr + 2);
    *(tinyprog->text + tinyprog->head + 3) = *(ptr + 3);
    tinyprog->head+=4;
}
void X64Builder::emit8(u64 qword){
    // wish we could assert to find bugs but immediates may be zero and won't work.
    // we could make a nother functionn specifcially for immediates though.
    // Assert(dword & 0xFF00'0000);
    ensure_bytes(8);
    auto ptr = (u8*)&qword;

    // deals with non-alignment
    *(tinyprog->text + tinyprog->head + 0) = *(ptr + 0);
    *(tinyprog->text + tinyprog->head + 1) = *(ptr + 1);
    *(tinyprog->text + tinyprog->head + 2) = *(ptr + 2);
    *(tinyprog->text + tinyprog->head + 3) = *(ptr + 3);
    *(tinyprog->text + tinyprog->head + 4) = *(ptr + 4);
    *(tinyprog->text + tinyprog->head + 5) = *(ptr + 5);
    *(tinyprog->text + tinyprog->head + 6) = *(ptr + 6);
    *(tinyprog->text + tinyprog->head + 7) = *(ptr + 7);
    tinyprog->head += 8;
}
void X64Builder::emit_bytes(u8* arr, u64 len){
    ensure_bytes(len);
    
    memcpy(tinyprog->text + tinyprog->head, arr, len);
    tinyprog->head += len;
}
void X64Builder::emit_modrm_slash(u8 mod, u8 reg, X64Register _rm){
    u8 rm = _rm - 1;
    Assert((mod&~3) == 0 && (reg&~7)==0 && (rm&~7)==0);
    // You probably made a mistake and used REG_BP thinking it works with just ModRM byte.
    Assert(("Use addModRM_SIB instead",!(mod!=0b11 && rm==0b100)));
    // REG_SP isn't available with mod=0b00, see intel x64 manual about 32 bit addressing for more details
    Assert(("Use addModRM_disp32 instead",!(mod==0b00 && rm==0b101)));
    // Assert(("Use addModRM_disp32 instead",(mod!=0b10)));
    emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
}
void X64Builder::emit_modrm(u8 mod, X64Register _reg, X64Register _rm){
    u8 reg = _reg - 1;
    u8 rm = _rm - 1;
    Assert((mod&~3) == 0 && (reg&~7)==0 && (rm&~7)==0);
    // You probably made a mistake and used REG_BP thinking it works with just ModRM byte.
    Assert(("Use addModRM_SIB instead",!(mod!=0b11 && rm==0b100)));
    // REG_SP isn't available with mod=0b00, see intel x64 manual about 32 bit addressing for more details
    Assert(("Use addModRM_disp32 instead",!(mod==0b00 && rm==0b101)));
    // Assert(("Use addModRM_disp32 instead",(mod!=0b10)));
    emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
}
void X64Builder::emit_modrm_rip(X64Register _reg, u32 disp32){
    u8 reg = _reg - 1;
    u8 mod = 0b00;
    u8 rm = 0b101;
    Assert((mod&~3) == 0 && (reg&~7)==0 && (rm&~7)==0);
    emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
    emit4(disp32);
}
void X64Builder::emit_modrm_sib(u8 mod, X64Register _reg, u8 scale, u8 index, X64Register _base){
    u8 base = _base - 1;
    u8 reg = _reg - 1;
    //  register to register (mod = 0b11) doesn't work with SIB byte
    Assert(("Use addModRM instead",mod!=0b11));

    Assert(("Ignored meaning in SIB byte. Look at intel x64 manual and fix it.",base != 0b101));

    u8 rm = 0b100;
    Assert((mod&~3) == 0 && (reg&~7)==0 && (rm&~7)==0);
    Assert((scale&~3) == 0 && (index&~7)==0 && (base&~7)==0);

    emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
    emit1((u8)(base | (index << (u8)3) | (scale << (u8)6)));
}
#ifdef gone
void X64Program::printHex(const char* path){
    using namespace engone;
    Assert(this);
    if(path) {
        OutputAsHex(path, (char*)text, head);
    } else {
        #define HEXIFY(X) (char)(X<10 ? '0'+X : 'A'+X - 10)
        log::out << log::LIME << "HEX:\n";
        for(int i = 0;i<head;i++){
            u8 a = text[i]>>4;
            u8 b = text[i]&0xF;
            log::out << HEXIFY(a) << HEXIFY(b) <<" ";
        }
        log::out << "\n";
        #undef HEXIFY
    }
}
void X64Program::printAsm(const char* path, const char* objpath){
    using namespace engone;
    Assert(this);
    if(!objpath) {
        #ifdef OS_WINDOWS
        FileCOFF::WriteFile("bin/garb.obj", this);
        #else
        FileELF::WriteFile("bin/garb.o", this);
        #endif
    }

    auto file = FileOpen(path, FILE_CLEAR_AND_WRITE);

    #ifdef OS_WINDOWS
    std::string cmd = "dumpbin /NOLOGO /DISASM:BYTES ";
    #else
    // -M intel for intel syntax
    std::string cmd = "objdump -M intel -d ";
    #endif
    if(!objpath)
        cmd += "bin/garb.obj";
    else
        cmd += objpath;

    engone::StartProgram((char*)cmd.c_str(), PROGRAM_WAIT, nullptr, {}, file);

    engone::FileClose(file);
}
#endif

/* dumpbin
Dump of file bin\dev.obj

File Type: COFF OBJECT

main:
  0000000000000000: FF F5              push        rbp
  0000000000000002: 48 8B EC           mov         rbp,rsp
  0000000000000005: 48 81 EC 08 00 00  sub         rsp,8
                    00
  000000000000000C: 33 C0              xor         eax,eax
  000000000000000E: FF F0              push        rax
  0000000000000010: 8F C0              pop         rax
  0000000000000012: 89 45 F8           mov         dword ptr [rbp-8],eax
  0000000000000015: 48 81 EC 08 00 00  sub         rsp,8
                    00
  000000000000001C: 48 33 C0           xor         rax,rax
  000000000000001F: C7 C0 41 00 00 00  mov         eax,41h
  0000000000000025: FF F0              push        rax
  0000000000000027: 48 33 C0           xor         rax,rax
  000000000000002A: 8F C0              pop         rax
  000000000000002C: 88 45 F0           mov         byte ptr [rbp-10h],al
  000000000000002F: 48 81 C4 10 00 00  add         rsp,10h
                    00
  0000000000000036: 8F C5              pop         rbp
  0000000000000038: C3                 ret

  Summary

          12 .debug_abbrev
          4B .debug_info
          31 .debug_line
          39 .text
*/
/* objdump
wa.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <_Z3heyii>:
   0:   f3 0f 1e fa             endbr64 
   4:   55                      push   %rbp
   5:   48 89 e5                mov    %rsp,%rbp
   8:   89 7d fc                mov    %edi,-0x4(%rbp)
   b:   89 75 f8                mov    %esi,-0x8(%rbp)
   e:   8b 55 fc                mov    -0x4(%rbp),%edx
  11:   8b 45 f8                mov    -0x8(%rbp),%eax
  14:   01 d0                   add    %edx,%eax
  16:   5d                      pop    %rbp
  17:   c3                      ret    

0000000000000018 <main>:
  18:   f3 0f 1e fa             endbr64 
  1c:   55                      push   %rbp
  1d:   48 89 e5                mov    %rsp,%rbp
  20:   be 05 00 00 00          mov    $0x5,%esi
  25:   bf 02 00 00 00          mov    $0x2,%edi
  2a:   e8 00 00 00 00          call   2f <main+0x17>
  2f:   b8 00 00 00 00          mov    $0x0,%eax
  34:   5d                      pop    %rbp
  35:   c3                      ret   
*/
void ReformatDumpbinAsm(LinkerChoice linker, QuickArray<char>& inBuffer, QuickArray<char>* outBuffer, bool includeBytes) {
    using namespace engone;
    Assert(!outBuffer); // not implemented
    
    int startIndex = 0;
    int endIndex = 0;
    switch(linker){
    case LINKER_MSVC: {
     // Skip heading
    int index = 0;
    int lineCount = 0;
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        index++;
        if(chr == '\n') {
            lineCount++;   
        }
        if(lineCount == 5)
            break;
    }
    startIndex = index;
    index = 0;
    // Find summary
    const char* summary_str = "Summary";
    int summary_len = strlen(summary_str);
    int summary_correct = 0;
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        index++;
        
        if(summary_str[summary_correct] == chr) {
            summary_correct++;
            if(summary_correct == summary_len) {
                endIndex = index - summary_len - 1 + 1; // -1 to skip newline before Summary, +1 for exclusive index
                break;
            }
        } else {
            summary_correct = 0;
        }
    }
    // endIndex = index; endIndex is set in while loop
    break;
    }
    case LINKER_GCC: {
    // Skip heading
    int index = 0;
    int lineCount = 0;
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        index++;
        if(chr == '\n') {
            lineCount++;   
        }
        if(lineCount == 6)
            break;
    }
    startIndex = index;
    index = 0;
    endIndex = inBuffer.size();
    break;
    }
    default: {
        Assert(false);
        break;
    }
    }
    // Print disassembly
    int len = endIndex - startIndex;
    if(outBuffer) {
        outBuffer->resize(len);
        memcpy(outBuffer->data(), inBuffer.data() + startIndex, len);
    } else {
        log::out.print(inBuffer.data() + startIndex, len, true);
    }
}
/*
bin/inline_asm.asm: Assembler messages:
bin/inline_asm.asm:3: Error: no such instruction: `eae'
bin/inline_asm.asm:5: Error: no such instruction: `hoiho eax,9'

*/
void ReformatAssemblerError(Bytecode::ASM& asmInstance, QuickArray<char>& inBuffer, int line_offset) {
    using namespace engone;
    // Assert(!outBuffer); // not implemented
    
    int startIndex = 0;
    int endIndex = 0;
    struct ASMError {
        std::string file;
        int line;
        std::string message;
    };
    DynamicArray<ASMError> errors{};
    
    #ifdef OS_WINDOWS
     // Skip heading
    int index = 0;
    int lineCount = 0;
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        index++;
        if(chr == '\n') {
            lineCount++;   
        }
        if(lineCount == 1)
            break;
    }
    
    
    
    // NOTE: We assume that each line is an error. As far as I have seen, this seems to be the case.
    ASMError asmError{};
    bool parseFileAndLine = true;
    int startOfFile = 0;
    int endOfFile = 0; // exclusive
    int startOfLineNumber = 0; // index where we suspect it to be
    int endOfLineNumber = 0; // exclusive
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        index++;
        
        if(chr == '\n' || inBuffer.size() == index) {
            Assert(!parseFileAndLine);
            errors.add(asmError);
            asmError = {};
            parseFileAndLine = true;
            startOfFile = index;
            continue;
        }
        
        if(parseFileAndLine) {
            if(chr == '(') {
                endOfFile = index - 1;
                startOfLineNumber = index;
            }
            if(chr == ')')
                endOfLineNumber = index-1;
            
            if(chr == ':') {
                // parse number
                char* str = inBuffer.data() + startOfLineNumber;
                inBuffer[endOfLineNumber] = 0;
                asmError.line = atoi(str);
                
                asmError.file.resize(endOfFile - startOfFile);
                memcpy((char*)asmError.file.data(), inBuffer.data() + startOfFile, endOfFile - startOfFile);
                
                parseFileAndLine = false;
                continue;
            }
            endOfFile = index;
        } else { // parse message
            asmError.message += chr;
        }
    }
    #else
     // Skip heading
    int index = 0;
    int lineCount = 0;
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        index++;
        if(chr == '\n') {
            lineCount++;   
        }
        if(lineCount == 1)
            break;
    }
    
    // NOTE: We assume that each line is an error. As far as I have seen, this seems to be the case.
    ASMError asmError{};
    bool parseFileAndLine = true;
    bool parseFindFinalColon = false;
    int startOfFile = 0;
    int endOfFile = 0; // exclusive
    int startOfLineNumber = 0; // index where we suspect it to be
    int endOfLineNumber = 0; // exclusive
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        index++;
        
        if(chr == '\n' || inBuffer.size() == index) {
            Assert(!parseFileAndLine);
            errors.add(asmError);
            asmError = {};
            parseFileAndLine = true;
            startOfFile = index;
            continue;
        }
        
        if(parseFileAndLine) {
            if(chr == ':') {
                if(!parseFindFinalColon) {
                    endOfFile = index - 1;
                    startOfLineNumber = index;
                    parseFindFinalColon = true;
                } else {
                    parseFindFinalColon = false;
                    endOfLineNumber = index-1;
                    // parse number
                    char* str = inBuffer.data() + startOfLineNumber;
                    inBuffer[endOfLineNumber] = 0;
                    asmError.line = atoi(str);
                    
                    asmError.file.resize(endOfFile - startOfFile);
                    memcpy((char*)asmError.file.data(), inBuffer.data() + startOfFile, endOfFile - startOfFile);
                    
                    parseFileAndLine = false;
                    continue;
                }
            }
            endOfFile = index;
        } else { // parse message
            asmError.message += chr;
        }
    }
    #endif
    
    FOR(errors) {
        log::out << log::RED << TrimCWD(asmInstance.file) << ":" << (asmInstance.lineStart-1 + it.line + line_offset);
        log::out << log::NO_COLOR << it.message << "\n";
    }
}
void X64Program::Destroy(X64Program* program) {
    using namespace engone;
    Assert(program);
    program->~X64Program();
    TRACK_FREE(program, X64Program);
    // engone::Free(program,sizeof(X64Program));
}
X64Program* X64Program::Create() {
    using namespace engone;

    // auto program = (X64Program*)engone::Allocate(sizeof(X64Program));
    auto program = TRACK_ALLOC(X64Program);
    new(program)X64Program();
    return program;
}