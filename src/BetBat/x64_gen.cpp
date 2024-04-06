#include "BetBat/x64_gen.h"

#include "BetBat/COFF.h"
#include "BetBat/ELF.h"

#include "BetBat/CompilerEnums.h"

#include "BetBat/Compiler.h"

/*
Hello!
    The x64 generator is based on SSA (static single-assignment).
    The bytecode is converted to a tree of nodes where registers and
    the value on the stack determines the relations between the nodes.
    This results in a couple of root nodes which are instructions with
    side effects such as "mov [rbp], eax" or "add rsp, 8".
    x64 registers are allocated and freed when traversing the nodes and generating x64 instructions.
    There will probably be a refactor in the future for further 
    optimized code generation (reuse of values in registers).

When writing code in this file:
    The code for the generator is messy since this is a first proper
    implementation of a decent generator (x64 is also a bit scuffed,
    but with good documentation).

    I urge you to be in a smart and consistent mood when changing the code.
    It is easy to accidently emit the wrong instruction SO be sure to 
    create GREAT test cases so that all edge cases are covered.

    Good luck!

Notes:
    I tried to think of a linear approach (instead of SSA) where you can perform optimizations (dead code elimination, efficient usage of registers)
    but I could not make it work. A bottom-up and then top-down approach with a tree is much easier.

About x64:
    x86/x64 instructions are complicated and it's not really my fault
    if things seem confusing. I have tried my best to keep it simple.

    x64 instruction layout:
    [Prefix] [Opcode] [Mod|REG|R/M] [SIB-byte] [Displacement] [Immediate]

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

#define OPCODE_ADD_REG_RM (u8)0x03
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

void GenerateX64_finalize(Compiler* compiler) {
    Assert(compiler->program); // don't call this function if there is no program
    Assert(compiler->program->debugInformation); // we expect debug information?

    auto prog = compiler->program;
    auto bytecode = compiler->code;

    // prog->debugInformation = bytecode->debugInformation;
    // bytecode->debugInformation = nullptr;

    if(bytecode->dataSegment.size() != 0) {
        prog->globalData = TRACK_ARRAY_ALLOC(u8, bytecode->dataSegment.size());
        // prog->globalData = (u8*)engone::Allocate(bytecode->dataSegment.used);
        Assert(prog->globalData);
        prog->globalSize = bytecode->dataSegment.size();
        memcpy(prog->globalData, bytecode->dataSegment.data(), prog->globalSize);

        // OutputAsHex("data.txt", (char*)prog->globalData, prog->globalSize);
    }

    for(int i=0;i<compiler->code->exportedFunctions.size();i++) {
        auto& sym = compiler->code->exportedFunctions[i];
        prog->addExportedSymbol(sym.name, sym.tinycode_index);
        // X64Program::ExportedSymbol tmp{};
        // tmp.name = sym.name;
        // tmp.textOffset = addressTranslation[sym.location];
        // prog->exportedSymbols.add(tmp);
    }
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
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->op1 = op1;

            if(IsNativeRegister(op0)) {
                // setting a native register is a side effect
                // we can't use reorder the action
                
                if(!IsNativeRegister(op1)) {
                    node->in1 = reg_values[op1];
                }
                nodes.add(node);
            } else {
                if(!IsNativeRegister(op1)) {
                    node->in1 = reg_values[op1];
                }
                reg_values[op0] = node;
            }
        } break;
        case BC_MOV_RM: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            auto node = createNode(prev_pc, opcode);
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
            
            auto node = createNode(prev_pc, opcode);
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
            
            auto node = createNode(prev_pc, opcode);
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
            
            // Assert(op1 != BC_REG_SP && op1 != BC_REG_BP);
            // Assert(control == CONTROL_32B);
            // TODO: op0, op1 as stack or base pointer requires special handling
            
            auto node = createNode(prev_pc, opcode);
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
            
            auto node = createNode(prev_pc, opcode);
            node->imm = imm;
            reg_values[op0] = node;
        } break;
        case BC_LI64: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i64*)&instructions[pc];
            pc+=8;
            
            auto node = createNode(prev_pc, opcode);
            node->imm = imm;
            reg_values[op0] = node;
        } break;
        case BC_INCR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = createNode(prev_pc, opcode);
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
              
            auto node = createNode(prev_pc, opcode);
            node->imm = imm;
            nodes.add(node);
        } break;
        case BC_JZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->imm = imm;
            
            node->in0 = reg_values[op0];
            
            nodes.add(node);
        } break;
        case BC_JNZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = createNode(prev_pc, opcode);
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

            auto node = createNode(prev_pc, opcode);
            node->link = l;
            node->call = c;
            node->imm = imm;
            
            nodes.add(node);
        } break;
        case BC_RET: {
            auto node = createNode(prev_pc, opcode);
            
            nodes.add(node);
        } break;
        case BC_DATAPTR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->imm = imm;
            
            reg_values[op0] = node;
        } break;
        case BC_CODEPTR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->imm = imm;
            
            reg_values[op0] = node;
        } break;
        case BC_CAST: {
            op0 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            cast = (InstructionCast)instructions[pc++];

            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->control = control;
            node->cast = cast;
            
            node->in0 = reg_values[op0];
            reg_values[op0] = node;
        } break;
        case BC_MEMZERO: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            imm = (u8)instructions[pc++];
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->op1 = op1;
            node->imm = imm;
            
            node->in0 = reg_values[op0];
            node->in1 = reg_values[op1];
            
            nodes.add(node);
        } break;
        case BC_MEMCPY: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            op2 = (BCRegister)instructions[pc++];
            
            auto node = createNode(prev_pc, opcode);
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
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->op1 = op1;
            node->control = control;
            node->in0 = reg_values[op0];
            node->in1 = reg_values[op1];
            
            reg_values[op0] = node;
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
            
            auto node = createNode(prev_pc, opcode);
            node->opcode = opcode;
            node->op0 = op0;
            node->op1 = op1;
            node->in0 = reg_values[op0];
            node->in1 = reg_values[op1];
            
            reg_values[op0] = node;
        } break;
        case BC_LNOT:
        case BC_BNOT: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            auto n = createNode(prev_pc, opcode);
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
    
    DynamicArray<i32> bc_to_x64_translation;
    bc_to_x64_translation.resize(tinycode->instructionSegment.size());

    struct RelativeRelocation {
        // RelativeRelocation(i32 ip, i32 x64, i32 bc) : currentIP(ip), immediateToModify(x64), bcAddress(bc) {}
        i32 currentIP=0; // It's more of a nextIP rarther than current. Needed when calculating relative offset
        // Address MUST point to an immediate (or displacement?) with 4 bytes.
        // You can add some flags to this struct for more variation.
        i32 immediateToModify=0; // dword/i32 to modify with correct value
        i32 bcAddress=0; // address in bytecode, needed when looking up absolute offset in bc_to_x64_translation
    };
    DynamicArray<RelativeRelocation> relativeRelocations;
    relativeRelocations._reserve(40);
    
   
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
        return e;
    };
    auto push_env1 = [&]() {
        Assert(n->in1);
        auto e = new Env();
        env->env_in1 = e;
        e->node = n->in1;

        envs.add(e);
        pop_env = false;
        return e;
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
        
        // TODO: I quickly threw this here, is that fine? some nodes in ENV will run twice and set the wrong
        //  address first BUT it should all be correct in the end.
        bc_to_x64_translation[n->bc_index] = size();

        if(IsNativeRegister(n->op0)) {
            env->reg0.reg = ToNativeRegister(n->op0);
        }
        if(IsNativeRegister(n->op1)) {
            env->reg1.reg = ToNativeRegister(n->op1);
        }
        
        switch(n->opcode) {
            case BC_BXOR: {
                if(n->opcode == BC_BXOR && n->op0 == n->op1) {
                    // xor clear on register
                    Assert(!IsNativeRegister(n->op0));

                    env->reg0.reg = alloc_register();
                    if(env->reg0.reg == X64_REG_INVALID) {
                        env->reg0.on_stack = true;
                        env->reg0.reg = RESERVED_REG0;
                    }
                    emit_prefix(PREFIX_REXW,env->reg0.reg, env->reg0.reg);
                    emit1(OPCODE_XOR_REG_RM);
                    emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg0.reg));
                    
                    if(env->reg0.on_stack) {
                        emit_push(env->reg0.reg);
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
                            auto e = push_env0(); // TODO: request xmm register on floats
                            if(IS_CONTROL_FLOAT(n->control)) {
                                e->reg0.reg = alloc_register(X64_REG_INVALID, true);
                                if(e->reg0.reg == X64_REG_INVALID)
                                    e->reg0.on_stack = true;
                            } else {
                                // let someone else allocate register
                            }
                        }
                        if(!env->env_in1 && !IsNativeRegister(n->op1)) {
                            auto e = push_env1();
                            if(IS_CONTROL_FLOAT(n->control)) {
                                e->reg0.reg = alloc_register(X64_REG_INVALID, true);
                                if(e->reg0.reg == X64_REG_INVALID)
                                    e->reg0.on_stack = true;
                            } else {
                                // let someone else allocate register
                            }
                        }
                    } else {
                        if(!env->env_in1 && !IsNativeRegister(n->op1)) {
                            auto e = push_env1();
                            if(IS_CONTROL_FLOAT(n->control)) {
                                e->reg0.reg = alloc_register(X64_REG_INVALID, true);
                                if(e->reg0.reg == X64_REG_INVALID)
                                    e->reg0.on_stack = true;
                            } else {
                                // let someone else allocate register
                            }
                        }
                        if(!env->env_in0 && !IsNativeRegister(n->op0)) {
                            auto e = push_env0();
                            if(IS_CONTROL_FLOAT(n->control)) {
                                e->reg0.reg = alloc_register(X64_REG_INVALID, true);
                                if(e->reg0.reg == X64_REG_INVALID)
                                    e->reg0.on_stack = true;
                            } else {
                                // let someone else allocate register
                            }
                        }
                    }
                    Assert(!pop_env);
                    break;
                }

                if(!IsNativeRegister(n->op0)) {
                    env->reg0 = env->env_in0->reg0;
                } else {
                    env->reg1.reg = ToNativeRegister(n->op0);
                }
                if(!IsNativeRegister(n->op1)) {
                    env->reg1 = env->env_in1->reg0;
                } else {
                    env->reg1.reg = ToNativeRegister(n->op1);
                }

                if(env->reg1.on_stack) {
                    env->reg1.reg = RESERVED_REG0;
                    emit_pop(env->reg1.reg);
                }
                if(env->reg0.on_stack) {
                    env->reg0.reg = RESERVED_REG1;
                    emit_pop(env->reg0.reg);
                }
                
                bool is_unsigned = !IS_CONTROL_SIGNED(n->control);
                if(IS_CONTROL_FLOAT(n->control)) {
                    switch(n->opcode) {
                        case BC_ADD: {
                            InstructionControl size = GET_CONTROL_SIZE(n->control);
                            // Assert(size == 4 || size == 8);
                            Assert(size == CONTROL_32B);
                            // TODO: We assume 32-bit float NOT GOOD
                            emit3(OPCODE_3_ADDSS_REG_RM);
                            // TODO: verify float register
                            emit_modrm(MODE_REG, CLAMP_XMM(env->reg1.reg), CLAMP_XMM(env->reg0.reg));
                        } break;
                        case BC_SUB: {
                            Assert(false);
                            // emit1(PREFIX_REXW);
                            // emit1(OPCODE_SUB_REG_RM);
                            // emit_modrm(MODE_REG, env->reg0, env->reg1);
                        } break;
                        case BC_MUL: {
                            Assert(false);
                            // // TODO: Handle signed/unsigned multiplication (using InstructionControl)
                            // if(is_unsigned) {
                            //     bool d_is_free = is_register_free(X64_REG_D);
                            //     bool a_is_free = is_register_free(X64_REG_A);
                            //     if(!d_is_free) {
                            //         emit1(OPCODE_PUSH_RM_SLASH_6);
                            //         emit_modrm_slash(MODE_REG, 6, X64_REG_D);
                            //     }
                            //     if(!a_is_free) {
                            //         emit1(OPCODE_PUSH_RM_SLASH_6);
                            //         emit_modrm_slash(MODE_REG, 6, X64_REG_A);
                            //     }
                            //     emit1(PREFIX_REXW);
                            //     emit1(OPCODE_MOV_REG_RM);
                            //     emit_modrm(MODE_REG, X64_REG_A, env->reg0);
                                
                            //     emit1(PREFIX_REXW);
                            //     emit1(OPCODE_MUL_AX_RM_SLASH_4);
                            //     emit_modrm_slash(MODE_REG, 4, env->reg1);
                                
                            //     emit1(PREFIX_REXW);
                            //     emit1(OPCODE_MOV_REG_RM);
                            //     emit_modrm(MODE_REG, env->reg0, X64_REG_A);
                                
                            //     if(!a_is_free) {
                            //         emit1(OPCODE_POP_RM_SLASH_0);
                            //         emit_modrm_slash(MODE_REG, 0, X64_REG_A);
                            //     }
                            //     if(!d_is_free) {
                            //         emit1(OPCODE_POP_RM_SLASH_0);
                            //         emit_modrm_slash(MODE_REG, 0, X64_REG_D);
                            //     }
                            // } else {
                            //     emit1(PREFIX_REXW);
                            //     emit2(OPCODE_2_IMUL_REG_RM);
                            //     emit_modrm(MODE_REG, env->reg1, env->reg0);
                            // }
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
                            Assert(false); // how does this work with float?
                            emit_prefix(PREFIX_REXW, env->reg1.reg, env->reg0.reg);
                            emit1(OPCODE_AND_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg1.reg), CLAMP_EXT_REG(env->reg0.reg));
                        } break;
                        case BC_BOR: {
                            Assert(false);
                            emit_prefix(PREFIX_REXW, env->reg1.reg, env->reg0.reg);
                            emit1(OPCODE_OR_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg1.reg), CLAMP_EXT_REG(env->reg0.reg));
                        } break;
                        case BC_BXOR: {
                            Assert(false);
                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                        } break;
                        case BC_BLSHIFT:
                        case BC_BRSHIFT: {
                            Assert(false);
                            bool c_was_used = false;
                            if(!is_register_free(X64_REG_C)) {
                                c_was_used = true;
                                emit1(OPCODE_PUSH_RM_SLASH_6);
                                emit_modrm_slash(MODE_REG, 6, X64_REG_C);

                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg1.reg);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, X64_REG_C, CLAMP_EXT_REG(env->reg1.reg));
                            }

                            emit1(PREFIX_REXW);
                            switch(n->opcode) {
                                case BC_BLSHIFT: {
                                    emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                                    emit1(OPCODE_SHL_RM_CL_SLASH_4);
                                    emit_modrm_slash(MODE_REG, 4, CLAMP_EXT_REG(env->reg0.reg));
                                } break;
                                case BC_BRSHIFT: {
                                    emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                                    emit1(OPCODE_SHR_RM_CL_SLASH_5);
                                    emit_modrm_slash(MODE_REG, 5, CLAMP_EXT_REG(env->reg0.reg));
                                } break;
                                default: Assert(false);
                            }

                            if(c_was_used) {
                                emit_pop(X64_REG_C);
                            }
                        } break;
                        case BC_EQ: {
                            // TODO: Test if this is okay? is cmp better?
                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                            
                            emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                            emit2(OPCODE_2_SETE_RM8);
                            emit_modrm_slash(MODE_REG, 0, env->reg0.reg);

                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg0.reg);
                            emit2(OPCODE_2_MOVZX_REG_RM8);
                            emit_modrm_slash(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg0.reg));
                        } break;
                        case BC_NEQ: {
                            // IMPORTANT: THERE MAY BE BUGS IF YOU COMPARE OPERANDS OF DIFFERENT SIZES.
                            //  SOME BITS MAY BE UNINITIALZIED.
                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                        } break;
                        case BC_LT:
                        case BC_LTE:
                        case BC_GT:
                        case BC_GTE: {
                            emit_prefix(PREFIX_REXW,env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_CMP_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));

                            u16 setType = 0;
                            if(!is_unsigned) {
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
                            
                            emit_prefix(0, X64_REG_INVALID, env->reg0.reg);
                            emit2(setType);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));
                            
                            // do we need more stuff or no? I don't think so?
                            // prog->add2(OPCODE_2_MOVZX_REG_RM8);
                            // emit_modrm(MODE_REG, reg2, reg2);

                        } break;
                        default: Assert(false);
                    }
                } else {
                    switch(n->opcode) {
                        case BC_ADD: {
                            emit_prefix(PREFIX_REXW,env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_ADD_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                        } break;
                        case BC_SUB: {
                            emit_prefix(PREFIX_REXW,env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_SUB_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                        } break;
                        case BC_MUL: {
                            // TODO: Handle signed/unsigned multiplication (using InstructionControl)
                            if(is_unsigned) {
                                bool d_is_free = is_register_free(X64_REG_D);
                                bool a_is_free = is_register_free(X64_REG_A);
                                if(!d_is_free) {
                                    emit_push(X64_REG_D);
                                }
                                if(!a_is_free) {
                                    emit_push(X64_REG_A);
                                }
                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, X64_REG_A, CLAMP_EXT_REG(env->reg0.reg));
                                
                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg1.reg);
                                emit1(OPCODE_MUL_AX_RM_SLASH_4);
                                emit_modrm_slash(MODE_REG, 4, CLAMP_EXT_REG(env->reg1.reg));
                                
                                emit_prefix(PREFIX_REXW, env->reg0.reg, X64_REG_INVALID);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), X64_REG_A);
                                
                                if(!a_is_free) {
                                    emit_pop(X64_REG_A);
                                }
                                if(!d_is_free) {
                                    emit_pop(X64_REG_D);
                                }
                            } else {
                                emit_prefix(PREFIX_REXW, env->reg1.reg, env->reg0.reg);
                                emit2(OPCODE_2_IMUL_REG_RM);
                                emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg1.reg), CLAMP_EXT_REG(env->reg0.reg));
                            }
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
                            emit_prefix(PREFIX_REXW, env->reg1.reg, env->reg0.reg);
                            emit1(OPCODE_AND_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg1.reg), CLAMP_EXT_REG(env->reg0.reg));
                        } break;
                        case BC_BOR: {
                            emit_prefix(PREFIX_REXW, env->reg1.reg, env->reg0.reg);
                            emit1(OPCODE_OR_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg1.reg), CLAMP_EXT_REG(env->reg0.reg));
                        } break;
                        case BC_BXOR: {
                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                        } break;
                        case BC_BLSHIFT:
                        case BC_BRSHIFT: {
                            // nocheckin TODO: Always bit shifting 64-bit register is a bad idea
                            bool c_was_used = false;
                            if(!is_register_free(X64_REG_C)) {
                                c_was_used = true;
                                emit_push(X64_REG_C);

                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg1.reg);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, X64_REG_C, CLAMP_EXT_REG(env->reg1.reg));
                            }

                            emit1(PREFIX_REXW);
                            switch(n->opcode) {
                                case BC_BLSHIFT: {
                                    emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                                    emit1(OPCODE_SHL_RM_CL_SLASH_4);
                                    emit_modrm_slash(MODE_REG, 4, CLAMP_EXT_REG(env->reg0.reg));
                                } break;
                                case BC_BRSHIFT: {
                                    emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                                    emit1(OPCODE_SHR_RM_CL_SLASH_5);
                                    emit_modrm_slash(MODE_REG, 5, CLAMP_EXT_REG(env->reg0.reg));
                                } break;
                                default: Assert(false);
                            }

                            if(c_was_used) {
                                emit_pop(X64_REG_C);
                            }
                        } break;
                        case BC_EQ: {
                            // TODO: Test if this is okay? is cmp better?
                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                            
                            emit_prefix(0, X64_REG_INVALID, env->reg1.reg);
                            emit2(OPCODE_2_SETE_RM8);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));

                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg0.reg);
                            emit2(OPCODE_2_MOVZX_REG_RM8);
                            emit_modrm_slash(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg0.reg));
                        } break;
                        case BC_NEQ: {
                            // IMPORTANT: THERE MAY BE BUGS IF YOU COMPARE OPERANDS OF DIFFERENT SIZES.
                            //  SOME BITS MAY BE UNINITIALZIED.
                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                        } break;
                        case BC_LT:
                        case BC_LTE:
                        case BC_GT:
                        case BC_GTE: {
                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_CMP_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));

                            u16 setType = 0;
                            if(!is_unsigned) {
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
                            
                            emit_prefix(0, X64_REG_INVALID, env->reg0.reg);
                            emit2(setType);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));
                            
                            // do we need more stuff or no? I don't think so?
                            // prog->add2(OPCODE_2_MOVZX_REG_RM8);
                            // emit_modrm(MODE_REG, reg2, reg2);

                        } break;
                        default: Assert(false);
                    }
                }
                
                if(!env->reg1.on_stack && !IsNativeRegister(n->op1))
                    free_register(env->reg1.reg);
                    
                if(env->reg0.on_stack) {
                    emit_pop(env->reg0.reg);
                }
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
                    env->reg0 = env->env_in0->reg0;
                } else {
                    env->reg0.reg = ToNativeRegister(n->op0);
                }
                if(!IsNativeRegister(n->op1)) {
                    env->reg1 = env->env_in1->reg0;
                } else {
                    env->reg1.reg = ToNativeRegister(n->op0);
                }

                if(env->reg1.on_stack) {
                    env->reg1.reg = RESERVED_REG0;
                    emit_pop(env->reg1.reg);
                }
                if(env->reg0.on_stack) {
                    env->reg0.reg = RESERVED_REG1;
                    emit_pop(env->reg0.reg);
                }
                
                switch(n->opcode) {
                    case BC_DIV: {
                        // TODO: Handled signed/unsigned

                        bool d_is_free = is_register_free(X64_REG_D);
                        if(!d_is_free) {
                            emit_push(X64_REG_D);
                        }
                        emit_prefix(PREFIX_REXW, X64_REG_INVALID, X64_REG_INVALID);
                        emit1(OPCODE_XOR_REG_RM);
                        emit_modrm(MODE_REG, X64_REG_D, X64_REG_D);
                        
                        bool a_is_free = true;
                        if(env->reg0.reg != X64_REG_A) {
                            a_is_free = is_register_free(X64_REG_A);
                            if(!a_is_free) {
                                emit_push(X64_REG_A);
                            }
                            emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                            emit1(OPCODE_MOV_REG_RM);
                            emit_modrm(MODE_REG, X64_REG_A, env->reg0.reg);
                        }

                        emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg1.reg);
                        emit1(OPCODE_DIV_AX_RM_SLASH_6);
                        emit_modrm_slash(MODE_REG, 6, env->reg1.reg);
                        
                        if(env->reg0.reg != X64_REG_A) {
                            emit_prefix(PREFIX_REXW, env->reg0.reg, X64_REG_INVALID);
                            emit1(OPCODE_MOV_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), X64_REG_A);
                        }
                        if(!a_is_free) {
                            emit_pop(X64_REG_A);
                        }
                        if(!d_is_free) {
                            emit_pop(X64_REG_D);
                        }
                    } break;
                    case BC_MOD: {
                        // TODO: Handled signed/unsigned

                        bool d_is_free = is_register_free(X64_REG_D);
                        if(!d_is_free) {
                            emit_push(X64_REG_D);
                        }
                        emit_prefix(PREFIX_REXW, X64_REG_INVALID, X64_REG_INVALID);
                        emit1(OPCODE_XOR_REG_RM);
                        emit_modrm(MODE_REG, X64_REG_D, X64_REG_D);
                        
                        bool a_is_free = true;
                        if(env->reg0.reg != X64_REG_A) {
                            a_is_free = is_register_free(X64_REG_A);
                            if(!a_is_free) {
                                emit_push(X64_REG_A);
                            }
                            emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                            emit1(OPCODE_MOV_REG_RM);
                            emit_modrm(MODE_REG, X64_REG_A, CLAMP_EXT_REG(env->reg0.reg));
                        }

                        emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg1.reg);
                        emit1(OPCODE_DIV_AX_RM_SLASH_6);
                        emit_modrm_slash(MODE_REG, 6, CLAMP_EXT_REG(env->reg1.reg)); 
                        
                        emit_prefix(PREFIX_REXW, env->reg0.reg, X64_REG_INVALID);
                        emit1(OPCODE_MOV_REG_RM);
                        emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), X64_REG_D);
                        // }
                        if(!a_is_free) {
                            emit_pop(X64_REG_A);
                        }
                        if(!d_is_free) {
                            emit_pop(X64_REG_D);
                        }
                    } break;
                    default: Assert(false);
                }
                
                if(!env->reg1.on_stack && !IsNativeRegister(n->op1))
                    free_register(env->reg1.reg);
                    
                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
            } break;
            case BC_LNOT:
            case BC_BNOT: {
                Assert(!ToNativeRegister(n->op0));

                if(!env->env_in1 && !IsNativeRegister(n->op1)) {
                    push_env1();
                    break;
                }
                if(env->reg0.invalid()) {
                    env->reg0.reg = alloc_register();
                    if(env->reg0.invalid()) {
                        env->reg0.on_stack = true;
                        env->reg0.reg = RESERVED_REG0;
                    }
                }

                if(!IsNativeRegister(n->op1)) {
                    env->reg1 = env->env_in1->reg0;
                }

                if(env->reg1.on_stack) {
                    env->reg1.reg = RESERVED_REG0;
                    emit_pop(env->reg1.reg);
                }
                
                switch(n->opcode) {
                    case BC_LNOT: {
                        emit_prefix(PREFIX_REXW, env->reg1.reg, env->reg1.reg);
                        emit1(OPCODE_TEST_RM_REG);
                        emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg1.reg), CLAMP_EXT_REG(env->reg1.reg));
                        
                        emit_prefix(0, env->reg0.reg, X64_REG_INVALID);
                        emit2(OPCODE_2_SETE_RM8);
                        emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));

                        // not necessary?
                        // emit1(PREFIX_REXW);
                        // emit2(OPCODE_2_MOVZX_REG_RM8);
                        // emit_modrm(MODE_REG, env->reg0, env->reg0);
                    } break;
                    case BC_BNOT: {
                        // TODO: Optimize by using one register. No need to allocate an IN and OUT register. We can then skip the MOV instruction.
                        emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                        emit1(OPCODE_MOV_REG_RM);
                        emit_modrm_slash(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));

                        emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                        emit1(OPCODE_NOT_RM_SLASH_2);
                        emit_modrm_slash(MODE_REG, 2, CLAMP_EXT_REG(env->reg0.reg));
                    } break;
                    default: Assert(false);
                }
                
                if(!env->reg1.on_stack && !IsNativeRegister(n->op1))
                    free_register(env->reg1.reg);
                    
                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
            } break;
            case BC_MOV_RR: {
                Assert(IsNativeRegister(n->op0) || IsNativeRegister(n->op1)); // if a native register is involved then we have a side effect, otherwise the register move instruction should have been abstracted.
                
                if(!env->env_in1 && !IsNativeRegister(n->op1)) {
                    push_env1();
                    break;
                }

                if(IsNativeRegister(n->op0)) {
                    env->reg0.reg = ToNativeRegister(n->op0);
                }

                if(env->reg0.invalid()) {
                    env->reg0.reg = alloc_register();
                    if(env->reg0.invalid()) {
                        env->reg0.on_stack = true;
                        env->reg0.reg = RESERVED_REG0;
                    }
                }

                if(!IsNativeRegister(n->op1)) {
                    env->reg1 = env->env_in1->reg0;
                }

                if(env->reg1.on_stack) {
                    env->reg1.reg = RESERVED_REG0;
                    emit_pop(env->reg1.reg);
                }

                if(IS_REG_XMM(env->reg0.reg)) {
                    if(IS_REG_XMM(env->reg1.reg)) {
                        Assert(false);
                        // How do we know whether a 32-bit float
                        // or 64-bit float should be moved
                        // emit3(OPCODE_3_MOVSS_REG_RM);
                        // emit3(OPCODE_3_MOVSD_REG_RM);
                    } else {
                        Assert(false);
                    }
                } else {
                    if(IS_REG_XMM(env->reg1.reg)) {
                        Assert(false);
                    } else {
                        emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                        emit1(OPCODE_MOV_REG_RM);
                        emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));

                        if(env->reg0.on_stack) {
                            emit_push(env->reg0.reg);
                        }
                    }

                }
            } break;
            case BC_MOV_RM:
            case BC_MOV_RM_DISP16: {
                Assert(!ToNativeRegister(n->op0));
                env->reg1.reg = ToNativeRegister(n->op1);
                
                if(!env->env_in1 && !IsNativeRegister(n->op1)) {
                    push_env1();
                    break;
                }

                if(env->reg0.invalid()) {
                    env->reg0.reg = alloc_register();
                    if(env->reg0.invalid()) {
                        env->reg0.on_stack = true;
                        env->reg0.reg = RESERVED_REG0;
                    }
                }

                if(!IsNativeRegister(n->op1)) {
                    env->reg1 = env->env_in1->reg0;
                }

                if(env->reg1.on_stack) {
                    env->reg1.reg = RESERVED_REG0;
                    emit_pop(env->reg1.reg);
                }

                if (IS_REG_XMM(env->reg0.reg)) {
                    if(GET_CONTROL_SIZE(n->control) == CONTROL_32B)
                        emit3(OPCODE_3_MOVSS_REG_RM);
                    else if(GET_CONTROL_SIZE(n->control) == CONTROL_64B)
                        emit3(OPCODE_3_MOVSD_REG_RM);
                    else Assert(false);

                    u8 mode = MODE_DEREF_DISP32;
                    if(n->imm == 0 || n->opcode == BC_MOV_RM) {
                        mode = MODE_DEREF;
                    } else if(n->imm < 0x80 && n->imm >= -0x80) {
                        mode = MODE_DEREF_DISP8;
                    }
                    emit_modrm(mode, CLAMP_XMM(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                    if(mode == MODE_DEREF_DISP8)
                        emit1((u8)(i8)n->imm);
                    else if(mode == MODE_DEREF_DISP32)
                        emit4((u32)(i32)n->imm);
                } else {
                    if(GET_CONTROL_SIZE(n->control) == CONTROL_16B) {
                        emit1(PREFIX_16BIT);
                    }
                    if(GET_CONTROL_SIZE(n->control) == CONTROL_64B) {
                        emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                    } else {
                        emit_prefix(0, env->reg0.reg, env->reg1.reg);
                    }
                    if(GET_CONTROL_SIZE(n->control) == CONTROL_8B) {
                        emit1(OPCODE_MOV_REG8_RM);
                    } else if(GET_CONTROL_SIZE(n->control) == CONTROL_16B) {
                        emit1(OPCODE_MOV_REG_RM);
                    } else if(GET_CONTROL_SIZE(n->control) == CONTROL_32B) {
                        emit1(OPCODE_MOV_REG_RM);
                    } else if(GET_CONTROL_SIZE(n->control) == CONTROL_64B) {
                        emit1(OPCODE_MOV_REG_RM);
                    }

                    u8 mode = MODE_DEREF_DISP32;
                    if(n->imm == 0 || n->opcode == BC_MOV_RM) {
                        mode = MODE_DEREF;
                    } else if(n->imm < 0x80 && n->imm >= -0x80) {
                        mode = MODE_DEREF_DISP8;
                    }
                    emit_modrm(mode, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                    if(mode == MODE_DEREF_DISP8)
                        emit1((u8)(i8)n->imm);
                    else if(mode == MODE_DEREF_DISP32)
                        emit4((u32)(i32)n->imm);
                }
                
                if(!env->reg1.on_stack && !IsNativeRegister(env->reg1.reg))
                    free_register(env->reg1.reg);
                
                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
            } break;
            case BC_MOV_MR:
            case BC_MOV_MR_DISP16: {
                Assert(n->op1 != BC_REG_SP && n->op1 != BC_REG_BP);
                // Assert(n->control == CONTROL_32B);
                
                // Pre-work
                
                env->reg0.reg = ToNativeRegister(n->op0);
                env->reg1.reg = ToNativeRegister(n->op1);
                
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
                    env->reg1 = env->env_in1->reg0;
                }
                
                if(!IsNativeRegister(n->op0)) {
                    env->reg0 = env->env_in0->reg0;
                }
                
                if(env->reg1.on_stack) {
                    env->reg1.reg = RESERVED_REG0;
                    emit_pop(env->reg1.reg);
                }

                if(env->reg0.on_stack) {
                    env->reg0.reg = RESERVED_REG1;
                    emit_pop(env->reg0.reg);
                }

                // Emit actual instruction
                if (IS_REG_XMM(env->reg1.reg)) {
                    if(GET_CONTROL_SIZE(n->control) == CONTROL_32B)
                        emit3(OPCODE_3_MOVSS_RM_REG);
                    else if(GET_CONTROL_SIZE(n->control) == CONTROL_64B)
                        emit3(OPCODE_3_MOVSD_RM_REG);
                    else Assert(false);

                    u8 mode = MODE_DEREF_DISP32;
                    if(n->imm == 0 || n->opcode == BC_MOV_MR) {
                        mode = MODE_DEREF;
                    } else if(n->imm < 0x80 && n->imm >= -0x80) {
                        mode = MODE_DEREF_DISP8;
                    }
                    emit_modrm(mode, CLAMP_XMM(env->reg1.reg), CLAMP_EXT_REG(env->reg0.reg));
                    if(mode == MODE_DEREF) {

                    } else if(mode == MODE_DEREF_DISP8)
                        emit1((u8)(i8)n->imm);
                    else
                        emit4((u32)(i32)n->imm);
                } else {
                    if(GET_CONTROL_SIZE(n->control) == CONTROL_16B) {
                        emit1(PREFIX_16BIT);
                    }
                    if(GET_CONTROL_SIZE(n->control) == CONTROL_64B) {
                        emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                    } else {
                        emit_prefix(0, env->reg0.reg, env->reg1.reg);
                    }
                    if(GET_CONTROL_SIZE(n->control) == CONTROL_8B) {
                        emit1(OPCODE_MOV_RM_REG8);
                    } else if(GET_CONTROL_SIZE(n->control) == CONTROL_16B) {
                        emit1(OPCODE_MOV_RM_REG);
                    } else if(GET_CONTROL_SIZE(n->control) == CONTROL_32B) {
                        emit1(OPCODE_MOV_RM_REG);
                    } else if(GET_CONTROL_SIZE(n->control) == CONTROL_64B) {
                        emit1(OPCODE_MOV_RM_REG);
                    }

                    u8 mode = MODE_DEREF_DISP32;
                    if(n->imm == 0 || n->opcode == BC_MOV_MR) {
                        mode = MODE_DEREF;
                    } else if(n->imm < 0x80 && n->imm >= -0x80) {
                        mode = MODE_DEREF_DISP8;
                    }
                    emit_modrm(mode, CLAMP_EXT_REG(env->reg1.reg), CLAMP_EXT_REG(env->reg0.reg));
                    if(mode == MODE_DEREF) {

                    } else if(mode == MODE_DEREF_DISP8)
                        emit1((u8)(i8)n->imm);
                    else
                        emit4((u32)(i32)n->imm);
                }
                
                // Late fixing
                if(!env->reg0.on_stack && !IsNativeRegister(env->reg0.reg))
                    free_register(env->reg0.reg);

                if(!env->reg1.on_stack && !IsNativeRegister(env->reg1.reg))
                    free_register(env->reg1.reg);
            } break;
            case BC_INCR: {
                env->reg0.reg = ToNativeRegister(n->op0);
                if(env->reg0.reg == X64_REG_INVALID) {
                    if(!env->env_in0) {
                        push_env0();
                        break;
                    }
                    env->reg0 = env->env_in0->reg0;
                }

                if(env->reg0.on_stack) {
                    env->reg0.reg = RESERVED_REG0;
                    emit_pop(env->reg0.reg);
                    // emit1(OPCODE_POP_RM_SLASH_0);
                    // emit_modrm_slash(MODE_REG, 0, env->reg0.reg);
                }
                if(n->imm < 0) {
                    emit_sub_imm32(env->reg0.reg, (i32)-n->imm);
                    // emit1(PREFIX_REXW);
                    // emit1(OPCODE_SUB_RM_IMM_SLASH_5);
                    // emit_modrm_slash(MODE_REG, 5, env->reg0.reg);
                    // emit4((u32)(i32)-n->imm); // NOTE: cast from i16 to i32 to u32, should be fine
                } else {
                    emit_add_imm32(env->reg0.reg, (i32)n->imm);
                    // emit1(PREFIX_REXW);
                    // emit1(OPCODE_ADD_RM_IMM_SLASH_0);
                    // emit_modrm_slash(MODE_REG, 0, env->reg0.reg);
                    // emit4((u32)(i32)n->imm); // NOTE: cast from i16 to i32 to u32, should be fine
                }
                // if(!env->reg0.on_stack && !IsNativeRegister(env->reg0.reg))
                //     free_register(env->reg0.reg);
            } break;
            case BC_LI32: {
                if(env->reg0.invalid()) {
                    env->reg0.reg = alloc_register();
                    if(env->reg0.reg == X64_REG_INVALID) {
                        env->reg0.on_stack = true;
                        env->reg0.reg = RESERVED_REG0;
                    }
                } else {
                    // already allocated
                }
                if(IS_REG_XMM(env->reg0.reg)) {
                    Assert(!env->reg0.on_stack);
                    emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                    emit_modrm_sib_slash(MODE_DEREF_DISP8, 0, SIB_SCALE_1, SIB_INDEX_NONE, X64_REG_SP);
                    emit1((u8)-8);
                    emit4((u32)(i32)n->imm);

                    emit3(OPCODE_3_MOVSS_REG_RM);
                    emit_modrm_sib(MODE_DEREF_DISP8, CLAMP_XMM(env->reg0.reg), SIB_SCALE_1, SIB_INDEX_NONE, X64_REG_SP);
                    emit1((u8)-8);
                } else {
                    // We do not need a 64-bit move to load a 32-bit into a 64-bit register and ensure that the register is cleared.
                    // A 32-bit move will actually clear the upper bits.
                    if(IS_REG_EXTENDED(env->reg0.reg)) {
                        emit1(PREFIX_REXB);
                    }
                    emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, env->reg0.reg);
                    emit4((u32)(i32)n->imm);

                    if(env->reg0.on_stack) {
                        emit_push(env->reg0.reg);
                    }
                }
            } break;
            case BC_LI64: {
                if(env->reg0.invalid()) {
                    env->reg0.reg = alloc_register();
                    if(env->reg0.reg == X64_REG_INVALID) {
                        env->reg0.on_stack = true;
                        env->reg0.reg = RESERVED_REG0;
                    }
                } else {
                    // already allocated
                }
                if(IS_REG_XMM(env->reg0.reg)) {
                    Assert(!env->reg0.on_stack);

                    Assert(is_register_free(RESERVED_REG0));
                    X64Register tmp_reg = RESERVED_REG0;

                    u8 reg_field = tmp_reg - 1;
                    Assert(reg_field >= 0 && reg_field <= 7);

                    emit1(PREFIX_REXW);
                    emit1((u8)(OPCODE_MOV_REG_IMM_RD_IO | reg_field));
                    emit8((u64)n->imm);

                    emit1(PREFIX_REXW);
                    emit1(OPCODE_MOV_RM_REG);
                    emit_modrm_sib_slash(MODE_DEREF_DISP8, tmp_reg, SIB_SCALE_1, SIB_INDEX_NONE, X64_REG_SP);
                    emit1((u8)-8);

                    emit3(OPCODE_3_MOVSD_REG_RM);
                    emit_modrm_sib(MODE_DEREF_DISP8, CLAMP_EXT_REG(env->reg0.reg), SIB_SCALE_1, SIB_INDEX_NONE, X64_REG_SP);
                    emit1((u8)-8);
                } else {
                    u8 reg_field = CLAMP_EXT_REG(env->reg0.reg) - 1;
                    Assert(reg_field >= 0 && reg_field <= 7);

                    u8 prefix = PREFIX_REXW;
                    if(IS_REG_EXTENDED(env->reg0.reg)) {
                        emit1(PREFIX_REXB);
                    }
                    
                    emit1(prefix);
                    emit1((u8)(OPCODE_MOV_REG_IMM_RD_IO | reg_field));
                    emit8((u64)n->imm);

                    if(env->reg0.on_stack) {
                        emit_push(env->reg0.reg);
                    }
                }
            } break;
            case BC_CALL: {
                if(n->link == LinkConventions::DLLIMPORT || n->link == LinkConventions::VARIMPORT) {
                    Assert(false);
                    // Assert(bytecode->externalRelocations[imm].location == bcIndex);
                    // prog->add(OPCODE_CALL_RM_SLASH_2);
                    // prog->addModRM_rip(2,(u32)0);
                    // X64Program::NamedUndefinedRelocation namedReloc{};
                    // namedReloc.name = bytecode->externalRelocations[imm].name;
                    // namedReloc.textOffset = prog->size() - 4;
                    // prog->namedUndefinedRelocations.add(namedReloc);
                } else if(n->link == LinkConventions::IMPORT) {
                    Assert(false);
                    // Assert(bytecode->externalRelocations[imm].location == bcIndex);
                    // prog->add(OPCODE_CALL_IMM);
                    // X64Program::NamedUndefinedRelocation namedReloc{};
                    // namedReloc.name = bytecode->externalRelocations[imm].name;
                    // namedReloc.textOffset = prog->size();
                    // prog->namedUndefinedRelocations.add(namedReloc);
                    // prog->add4((u32)0);
                } else if (n->link == LinkConventions::NONE){ // or export
                    Assert(false);
                    // prog->add(OPCODE_CALL_IMM);
                    // prog->add4((u32)0);
                    
                    // RelativeRelocation reloc{};
                    // reloc.currentIP = prog->size();
                    // reloc.bcAddress = imm;
                    // reloc.immediateToModify = prog->size()-4;
                    // relativeRelocations.add(reloc);
                } else if (n->link == LinkConventions::NATIVE){
                    //-- native function
                    switch(n->imm) {
                    // You could perhaps implement a platform layer which the language always links too.
                    // That way, you don't need to write different code for each platform.
                    // case NATIVE_malloc: {
                        
                    // }
                    // break;
                    case NATIVE_prints: {
                        // TODO: Check platform target instead
                        #ifdef OS_WINDOWS
                        // ptr = [rsp + 0]
                        // len = [rsp + 8]
                        // char* ptr = *(char**)(fp+argoffset);
                        // u64 len = *(u64*)(fp+argoffset+8);
                        emit1(PREFIX_REXW);
                        emit1(OPCODE_MOV_REG_RM);
                        emit_modrm_sib(MODE_DEREF, X64_REG_SI, SIB_SCALE_1, SIB_INDEX_NONE, X64_REG_SP);
                        
                        emit1((u8)(PREFIX_REXW));
                        emit1(OPCODE_MOV_REG_RM);
                        emit_modrm_sib(MODE_DEREF_DISP8, X64_REG_B, SIB_SCALE_1, SIB_INDEX_NONE, X64_REG_SP);
                        emit1((u8)8);

                        // TODO: You may want to save registers. This is not needed right
                        //   now since we basically handle everything through push and pop.

                        /*
                        sub    rsp,0x38
                        mov    ecx,0xfffffff5
                        call   QWORD PTR [rip+0x0]          # GetStdHandle(-11)
                        mov    QWORD PTR [rsp+0x20],0x0
                        xor    r9,r9
                        mov    r8,rbx                       # rbx = buffer
                        mov    rdx,rsi                      # rsi = length
                        mov    rcx,rax
                        call   QWORD PTR [rip+0x0]          # WriteFile(...)
                        add    rsp,0x38
                        */
                        // Assert(false); // is the assembly wrong?
                        // rdx should be buffer and r8 length
                        // X64Program::NamedUndefinedRelocation reloc0{};
                        // reloc0.name = "__imp_GetStdHandle"; // C creates these symbol names in it's object file
                        // reloc0.textOffset = prog->size() + 0xB;
                        // X64Program::NamedUndefinedRelocation reloc1{};
                        // reloc1.name = "__imp_WriteFile";
                        // reloc1.textOffset = prog->size() + 0x26;
                        u32 start_addr = size();
                        u8 arr[]={ 0x48, 0x83, 0xEC, 0x38, 0xB9, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0x4D, 0x31, 0xC9, 0x49, 0x89, 0xD8, 0x48, 0x89, 0xF2, 0x48, 0x89, 0xC1, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x38 };
                        emit_bytes(arr,sizeof(arr));

                        // C creates these symbol names in it's object file
                        prog->addNamedUndefinedRelocation("__imp_GetStdHandle", start_addr + 0xB, current_tinyprog_index);
                        prog->addNamedUndefinedRelocation("__imp_WriteFile", start_addr + 0x26, current_tinyprog_index);
                        // prog->namedUndefinedRelocations.add(reloc0);
                        // prog->namedUndefinedRelocations.add(reloc1);
                    #else
                        // ptr = [rsp + 0]
                        // len = [rsp + 8]
                        // char* ptr = *(char**)(fp+argoffset);
                        // u64 len = *(u64*)(fp+argoffset+8);
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM_SIB(MODE_DEREF, REG_SI, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        
                        prog->add((u8)(PREFIX_REXW));
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, REG_D, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)8);

                        prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                        prog->addModRM(MODE_REG, 0, REG_DI);
                        prog->add4((u32)1); // 1 = stdout

                        prog->add(OPCODE_CALL_IMM);
                        int reloc_pos = prog->size();
                        prog->add4((u32)0);

                        // We call the Unix write system call, altough not directly
                        X64Program::NamedUndefinedRelocation reloc0{};
                        reloc0.name = "write"; // symbol name, gcc (or other linker) knows how to relocate it
                        reloc0.textOffset = reloc_pos;
                        prog->namedUndefinedRelocations.add(reloc0);
                    #endif
                        break;
                    }
                    #ifdef gone
                    case NATIVE_printc: {
                    #ifdef OS_WINDOWS
                        // char = [rsp + 7]
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM(MODE_REG, REG_SI, REG_SP);

                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_ADD_RM_IMM_SLASH_0);
                        prog->addModRM(MODE_REG, 0, REG_SI);
                        prog->add4((u32)0);

                        prog->add((u8)(PREFIX_REXW));
                        prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                        prog->addModRM(MODE_REG, 0, REG_B);
                        prog->add4((u32)1);

                        // TODO: You may want to save registers. This is not needed right
                        //   now since we basically handle everything through push and pop.

                        /*
                        sub    rsp,0x38
                        mov    ecx,0xfffffff5
                        call   QWORD PTR [rip+0x0]          # GetStdHandle(-11)
                        mov    QWORD PTR [rsp+0x20],0x0
                        xor    r9,r9
                        mov    r8,rbx                       # rbx = buffer (this might be wrong)
                        mov    rdx,rsi                      # rsi = length
                        mov    rcx,rax
                        call   QWORD PTR [rip+0x0]          # WriteFile(...)
                        add    rsp,0x38
                        */
                        X64Program::NamedUndefinedRelocation reloc0{};
                        reloc0.name = "__imp_GetStdHandle"; // C creates these symbol names in it's object file
                        reloc0.textOffset = prog->size() + 0xB;
                        X64Program::NamedUndefinedRelocation reloc1{};
                        reloc1.name = "__imp_WriteFile";
                        reloc1.textOffset = prog->size() + 0x26;
                        u8 arr[]={ 0x48, 0x83, 0xEC, 0x38, 0xB9, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0x4D, 0x31, 0xC9, 0x49, 0x89, 0xD8, 0x48, 0x89, 0xF2, 0x48, 0x89, 0xC1, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x38 };
                        prog->addRaw(arr,sizeof(arr));

                        prog->namedUndefinedRelocations.add(reloc0);
                        prog->namedUndefinedRelocations.add(reloc1);

                    #else
                        // char = [rsp + 7]
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM(MODE_REG, REG_SI, REG_SP);

                        // add an offset, but not needed?
                        // prog->add(PREFIX_REXW);
                        // prog->add(OPCODE_ADD_RM_IMM_SLASH_0);
                        // prog->addModRM(MODE_REG, 0, REG_SI);
                        // prog->add4((u32)8);

                        // TODO: You may want to save registers. This is not needed right
                        //   now since we basically handle everything through push and pop.

                        prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                        prog->addModRM(MODE_REG, 0, REG_D);
                        prog->add4((u32)1); // 1 byte/char length

                        // prog->add(OPCODE_MOV_RM_REG);
                        // prog->addModRM(MODE_REG, REG_SI, REG_SI); // pointer to buffer

                        prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                        prog->addModRM(MODE_REG, 0, REG_DI);
                        prog->add4((u32)1); // stdout

                        prog->add(OPCODE_CALL_IMM);
                        int reloc_pos = prog->size();
                        prog->add4((u32)0);

                        // We call the Unix write system call, altough not directly
                        X64Program::NamedUndefinedRelocation reloc0{};
                        reloc0.name = "write"; // symbol name, gcc (or other linker) knows how to relocate it
                        reloc0.textOffset = reloc_pos;
                        prog->namedUndefinedRelocations.add(reloc0);
                    #endif
                        break;
                    }
                    #endif
                    default: {
                        // failure = true;
                        // Assert(bytecode->nativeRegistry);
                        auto nativeRegistry = NativeRegistry::GetGlobal();
                        auto nativeFunction = nativeRegistry->findFunction(n->imm);
                        // auto* nativeFunction = bytecode->nativeRegistry->findFunction(imm);
                        if(nativeFunction){
                            log::out << log::RED << "Native '"<<nativeFunction->name<<"' (id: "<<n->imm<<") is not implemented in x64 converter (" OS_NAME ").\n";
                        } else {
                            log::out << log::RED << n->imm<<" is not a native function (message from x64 converter).\n";
                        }
                    }
                    } // switch
                } else {
                    Assert(false);
                }
                break;
            } break;
            case BC_RET: {
                emit1(OPCODE_POP_RM_SLASH_0);
                emit_modrm_slash(MODE_REG, 0, X64_REG_BP);

                emit1(OPCODE_RET);
            } break;
            case BC_MEMZERO: {
                Assert(!ToNativeRegister(n->op1));
                env->reg0.reg = ToNativeRegister(n->op0);
                
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
                    env->reg1 = env->env_in1->reg0;
                }
                
                if(!IsNativeRegister(n->op0)) {
                    env->reg0 = env->env_in0->reg0;
                }
                
                if(env->reg1.on_stack) {
                    env->reg1.reg = RESERVED_REG0;
                    emit_pop(env->reg1.reg);
                }

                if(env->reg0.on_stack) {
                    env->reg0.reg = RESERVED_REG1;
                    emit_pop(env->reg0.reg);
                }

                // ##########################
                //   MEMZERO implementation
                // ##########################
                {
                    u8 batch_size = n->imm;
                    if(batch_size == 0)
                        batch_size = 1;

                    X64Register reg_cur = env->reg0.reg;
                    X64Register reg_fin = env->reg1.reg;

                    u8 prefix = PREFIX_REXW;
                    emit1(PREFIX_REXW);
                    emit1(OPCODE_ADD_RM_REG);
                    emit_modrm(MODE_REG, reg_cur, reg_fin);

                    int offset_loop = size();

                    emit1(PREFIX_REXW);
                    emit1(OPCODE_CMP_REG_RM);
                    emit_modrm(MODE_REG, reg_fin, reg_cur);

                    emit1(OPCODE_JE_IMM8);
                    int offset_jmp_imm = size();
                    emit1((u8)0);

                    switch(batch_size) {
                    case 1:
                        emit1(OPCODE_MOV_RM_IMM8_SLASH_0);
                        break;
                    case 2:
                        Assert(("Memzero conversion incomplete for 2-byte batch size",false));
                        break;
                    case 4:
                        emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                        break;
                    case 8:
                        emit1(PREFIX_REXW);
                        emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                        break;
                    default: Assert(false);
                    }

                    if(reg_cur == X64_REG_SP) {
                        // emit_modrm_sib(MODE_DEREF, 0, SIB_SCALE_1, SIB_INDEX_NONE, reg_cur);
                        emit_modrm_sib_slash(MODE_DEREF, 0, SIB_SCALE_1, SIB_INDEX_NONE, reg_cur);
                    } else {
                        emit_modrm_slash(MODE_DEREF, 0, reg_cur);
                    }

                    switch(batch_size) {
                    case 1:
                        emit1((u8)0);
                        break;
                    case 2:
                        Assert(("Memzero conversion incomplete for 2-byte batch size",false));
                        break;
                    case 4:
                        emit4((u32)0);
                        break;
                    case 8:
                        emit4((u32)0);
                        break;
                    default: Assert(false);
                    }

                    emit1(PREFIX_REXW);
                    emit1(OPCODE_ADD_RM_IMM8_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, reg_cur);
                    emit1((u8)batch_size);

                    emit1(OPCODE_JMP_IMM8);
                    emit1((u8)(offset_loop - (size()+1)));

                    fix_relative_jump_imm8(offset_jmp_imm, (u8)(size() - (offset_jmp_imm + 1)));
                    break;
                }
                
                if(!env->reg0.on_stack && !IsNativeRegister(env->reg0.reg))
                    free_register(env->reg0.reg);

                if(!env->reg1.on_stack && !IsNativeRegister(env->reg1.reg))
                    free_register(env->reg1.reg);
            } break;
            case BC_JZ: {
                Assert(!ToNativeRegister(n->op0));
                
                // TODO: Check node depth, do the most register allocs first
                if(!env->env_in0 && !IsNativeRegister(n->op0)) {
                    push_env0();
                    break;
                }
                
                if(!IsNativeRegister(n->op0)) {
                    env->reg0 = env->env_in0->reg0;
                }

                if(env->reg0.on_stack) {
                    env->reg0.reg = RESERVED_REG1;
                    emit_pop(env->reg0.reg);
                }
                
                u8 prefix = PREFIX_REXW;
                if(IS_REG_EXTENDED(env->reg0.reg)) {
                    prefix |= PREFIX_REXB;
                }
                emit1(prefix);
                emit1(OPCODE_CMP_RM_IMM8_SLASH_7);
                emit_modrm_slash(MODE_REG, 7, CLAMP_EXT_REG(env->reg0.reg));
                emit1((u8)0);
                
                emit2(OPCODE_2_JE_IMM32);
                emit4((u32)0);
                
                /*
                  Immediates in bytecode jump instructions are relative to
                    the byte after the final byte in the instruction. That byte offset from
                    start of bytecode instructions PLUS the immediate results in the first
                    byte to execute next cycle. HOWEVER, OPNodes store bc_index based on
                    the first byte in the instruction and we don't have
                    a table of number of bytes per opcode.
                    
                    Either we create a table or use a sneaky hardcoded value here.
                    I am sorry but I choose the lazy option. I have been working on the x64_gen
                    for a while and just want to see it done right now. I will come back
                    and clean it upp later.
                    
                    I do want to add that the hardcoded solution is probably faster than a table
                    unless the compiler does some optimizations on a static const table.
                */
                
                const u8 BYTE_OF_BC_JZ = 1 + 1 + 4; // nocheckin TODO: DON'T HARDCODE VALUES!
               
                RelativeRelocation reloc{};
                reloc.currentIP = size();
                reloc.bcAddress = n->bc_index + n->imm + BYTE_OF_BC_JZ;
                reloc.immediateToModify = size()-4; // NOTE: RelativeRelocation assumes 32-bit integer JMP_IMM8 would not work without changes
                relativeRelocations.add(reloc);
                
                if(!env->reg0.on_stack && !IsNativeRegister(env->reg0.reg))
                    free_register(env->reg0.reg);
            } break;
            case BC_JNZ: {
                Assert(!ToNativeRegister(n->op0));
                
                
                // TODO: Check node depth, do the most register allocs first
                if(!env->env_in0 && !IsNativeRegister(n->op0)) {
                    push_env0();
                    break;
                }
                
                if(!IsNativeRegister(n->op0)) {
                    env->reg0 = env->env_in0->reg0;
                }

                if(env->reg0.on_stack) {
                    env->reg0.reg = RESERVED_REG1;
                    emit_pop(env->reg0.reg);
                }
                
                u8 prefix = PREFIX_REXW;
                if(IS_REG_EXTENDED(env->reg0.reg)) {
                    prefix |= PREFIX_REXB;
                }
                emit1(prefix);
                emit1(OPCODE_CMP_RM_IMM8_SLASH_7);
                emit_modrm_slash(MODE_REG, 7, CLAMP_EXT_REG(env->reg0.reg));
                emit1((u8)0);
                
                emit2(OPCODE_2_JNE_IMM32);
                emit4((u32)0);
                
                const u8 BYTE_OF_BC_JNZ = 1 + 1 + 4; // nocheckin TODO: DON'T HARDCODE VALUES!
                
                RelativeRelocation reloc{};
                reloc.currentIP = size();
                reloc.bcAddress = n->bc_index + n->imm + BYTE_OF_BC_JNZ;
                reloc.immediateToModify = size()-4;
                relativeRelocations.add(reloc);
                
                if(!env->reg0.on_stack && !IsNativeRegister(env->reg0.reg))
                    free_register(env->reg0.reg);
            } break;
            case BC_JMP: {
                emit1(OPCODE_JMP_IMM32);
                emit4((u32)0);
                
                const u8 BYTE_OF_BC_JMP = 1 + 4; // nocheckin TODO: DON'T HARDCODE VALUES!
                
                RelativeRelocation reloc{};
                reloc.currentIP = size();
                reloc.bcAddress = n->bc_index + n->imm + BYTE_OF_BC_JMP;
                reloc.immediateToModify = size()-4;
                relativeRelocations.add(reloc);
            } break;
            case BC_CAST: {
                Assert(!ToNativeRegister(n->op0));
                
                // TODO: Check node depth, do the most register allocs first
                if(!env->env_in0 && !IsNativeRegister(n->op0)) {
                    push_env0();
                    break;
                }
                
                if(!IsNativeRegister(n->op0)) {
                    env->reg0 = env->env_in0->reg0;
                }

                if(env->reg0.on_stack) {
                    env->reg0.reg = RESERVED_REG1;
                    emit_pop(env->reg0.reg);
                }

                int fsize = 1 << GET_CONTROL_SIZE(n->control);
                int tsize = 1 << GET_CONTROL_CONVERT_SIZE(n->control);

                u8 minSize = fsize < tsize ? fsize : tsize;
                
                X64Register origin_reg = env->reg0.reg;

                switch(n->cast) {
                    #ifdef gone
                    case CAST_FLOAT_SINT: {
                        Assert(false);
                        // Assert(fsize == 4 || fsize == 8);
                        // Assert(!toXmm);

                        // if(!fromXmm) {
                        //     if(fsize == 8)
                        //         emit1(PREFIX_REXW);
                        //     emit1(OPCODE_MOV_RM_REG);
                        //     prog->addModRM_SIB(MODE_DEREF_DISP8, freg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        //     emit1((u8)-8);
                        // }
                        // if        (fsize == 4 && tsize == 4) {
                        //     prog->add3(OPCODE_3_CVTTSS2SI_REG_RM);
                        // } else if (fsize == 4 && tsize == 8) {
                        //     prog->add4(OPCODE_4_REXW_CVTTSS2SI_REG_RM);
                        // } else if (fsize == 8 && tsize == 4) {
                        //     prog->add3(OPCODE_3_CVTTSD2SI_REG_RM);
                        // } else if (fsize == 8 && tsize == 8) {
                        //     prog->add4(OPCODE_4_REXW_CVTTSD2SI_REG_RM);
                        // } else {
                        //     Assert(false);
                        // }
                        // if(fromXmm) {
                        //     emit_modrm(MODE_REG, treg, freg);
                        // } else {
                        //     prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        //     emit1((u8)-8);
                        // }
                    } break;
                    case CAST_FLOAT_UINT: {
                        Assert(fsize == 4 || fsize == 8);

                        Assert(false); // TODO: float to uint64 is very special, needs to be handled properly

                        if(tsize == 8) {
                            if(fsize == 4) {
                                /*
                                movss  xmm0,DWORD PTR [rip+0x0]        # 10 <main+0x10>

                                movss  DWORD PTR [rbp-0xc],xmm0
                                movss  xmm0,DWORD PTR [rbp-0xc]
                                comiss xmm0,DWORD PTR [rip+0x0]        # 21 <main+0x21>
                                jae    unsigned
                                movss  xmm0,DWORD PTR [rbp-0xc]
                                cvttss2si rax,xmm0
                                mov    QWORD PTR [rbp-0x8],rax
                                jmp    end
                            unsigned:
                                movss  xmm0,DWORD PTR [rbp-0xc]
                                movss  xmm1,DWORD PTR [rip+0x0]        # 40 <main+0x40>

                                subss  xmm0,xmm1
                                cvttss2si rax,xmm0
                                mov    QWORD PTR [rbp-0x8],rax
                                movabs rax,0x8000000000000000

                                xor    QWORD PTR [rbp-0x8],rax
                            end:
                                mov    rax,QWORD PTR [rbp-0x8]
                                mov    QWORD PTR [rbp-0x8],rax
                                */
                            } else if(fsize == 8) {
                                /*
                                movsd  xmm0,QWORD PTR [rip+0x0]        # 10 <main+0x10>

                                movsd  QWORD PTR [rbp-0x10],xmm0
                                movsd  xmm0,QWORD PTR [rbp-0x10]
                                comisd xmm0,QWORD PTR [rip+0x0]        # 22 <main+0x22>

                                jae    unsigned
                                movsd  xmm0,QWORD PTR [rbp-0x10]
                                cvttsd2si rax,xmm0
                                mov    QWORD PTR [rbp-0x8],rax
                                jmp    end
                            unsigned:
                                movsd  xmm0,QWORD PTR [rbp-0x10]
                                movsd  xmm1,QWORD PTR [rip+0x0]        # 41 <main+0x41>

                                subsd  xmm0,xmm1
                                cvttsd2si rax,xmm0
                                mov    QWORD PTR [rbp-0x8],rax
                                movabs rax,0x8000000000000000
                            end:
                                xor    QWORD PTR [rbp-0x8],rax
                                mov    rax,QWORD PTR [rbp-0x8]
                                mov    QWORD PTR [rbp-0x8],rax
                                */
                            }
                        } else {

                        }

                        if(fromXmm) {
                            prog->add3(OPCODE_3_CVTTSS2SI_REG_RM);
                            emit_modrm(MODE_REG, treg, freg);
                        } else {
                            emit1(OPCODE_MOV_RM_REG);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, freg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            emit1((u8)-8);

                            if(tsize == 8)
                                prog->add4(OPCODE_4_REXW_CVTTSS2SI_REG_RM);
                            else
                                prog->add3(OPCODE_3_CVTTSS2SI_REG_RM);

                            prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            emit1((u8)-8);
                        }
                    } break;
                    case CAST_SINT_FLOAT: {
                        Assert(false);
                        Assert(tsize == 4 || tsize == 8);
                        Assert(!fromXmm);
                        if(fsize == 1){
                            Assert(false); // we might need to sign extend
                            // emit1(PREFIX_REXW);
                            // emit1(OPCODE_AND_RM_IMM_SLASH_4);
                            // emit_modrm(MODE_REG, 4, freg);
                            // prog->add4((u32)0xFF);
                            if(tsize == 4)
                                prog->add3(OPCODE_3_CVTSI2SS_REG_RM);
                            else
                                prog->add3(OPCODE_3_CVTSI2SD_REG_RM);
                        } else if(fsize == 2){
                            Assert(false); // we might need to sign extend
                            // emit1(PREFIX_REXW);
                            // emit1(OPCODE_AND_RM_IMM_SLASH_4);
                            // emit_modrm(MODE_REG, 4, freg);
                            // prog->add4((u32)0xFFFF);
                            if(tsize == 4)
                                prog->add3(OPCODE_3_CVTSI2SS_REG_RM);
                            else
                                prog->add3(OPCODE_3_CVTSI2SD_REG_RM);
                        } else if(fsize == 4) {
                            if(tsize == 4)
                                prog->add3(OPCODE_3_CVTSI2SS_REG_RM);
                            else
                                prog->add3(OPCODE_3_CVTSI2SD_REG_RM);
                        } else if(fsize == 8) {
                            if(tsize == 4)
                                prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                            else
                                prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                        }
                        if(toXmm) {
                            emit_modrm(MODE_REG, treg, freg);
                            // emit1(OPCODE_NOP);
                        } else {
                            // BUG: Hoping that xmm7 isn't used is considered bad programming.
                            emit_modrm(MODE_REG, REG_XMM7, freg);

                            if(tsize == 4)
                                prog->add3(OPCODE_3_MOVSS_RM_REG);
                            else
                                prog->add3(OPCODE_3_MOVSD_RM_REG);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM7, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            emit1((u8)-8);

                            if(tsize == 8)
                                emit1(PREFIX_REXW);
                            emit1(OPCODE_MOV_REG_RM);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            emit1((u8)-8);
                        }
                    } break;
                    case CAST_UINT_FLOAT: {
                        Assert(false);
                        Assert(tsize == 4 || tsize == 8);
                        Assert(!fromXmm);
                        
                        if(fsize == 8) {
                            // We have freg but the code may change so to ensure no future bugs we get
                            // the register again with BCToReg and ensure that it isn't extended (with R8-R11) and that
                            // it is a 64-bit register.
                            u8 from_reg = BCToProgramReg(op1, 8);

                            u8 xmm_reg = REG_XMM7; // nocheckin, use xmm7 if treg is rax,rbx.. otherwise we can actually just use xmm register

                            if(toXmm) {
                                xmm_reg = treg;
                            }

                            if(tsize == 4) {
                                /*
                                test   rax,rax
                                js     unsigned
                                pxor   xmm0,xmm0
                                cvtsi2ss xmm0,rax
                                jmp    end
                            unsigned:
                                mov    rdx,rax
                                shr    rdx,1
                                and    eax,0x1
                                or     rdx,rax
                                pxor   xmm0,xmm0
                                cvtsi2ss xmm0,rdx
                                addss  xmm0,xmm0
                            end:
                                */

                                u8 temp_reg = REG_DI;
                                emit1(OPCODE_PUSH_RM_SLASH_6);
                                emit_modrm(MODE_REG, 6, temp_reg);

                                // Check signed bit
                                emit1(PREFIX_REXW);
                                emit1(OPCODE_TEST_RM_REG);
                                emit_modrm(MODE_REG, from_reg, from_reg);

                                emit1(OPCODE_JS_REL8);
                                int jmp_offset_unsigned = prog->size();
                                emit1((u8)0); // set later

                                // Normal conversion since the signed bit is off
                                prog->add3(OPCODE_3_PXOR_RM_REG);
                                emit_modrm(MODE_REG, xmm_reg, xmm_reg);

                                prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                                emit_modrm(MODE_REG, xmm_reg, from_reg);

                                emit1(OPCODE_JMP_IMM8);
                                int jmp_offset_end = prog->size();
                                emit1((u8)0);

                                prog->set(jmp_offset_unsigned, prog->size() - (jmp_offset_unsigned +1)); // +1 gives the offset after the jump instruction (immediate is one byte)
                                
                                // Special conversion because the signed bit is on but since our conversion is unsigned
                                // we need some special stuff so that our float isn't negative and half the number we except

                                emit1(PREFIX_REXW);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, temp_reg, from_reg);

                                emit1(PREFIX_REXW);    
                                emit1(OPCODE_SHR_RM_ONCE_SLASH_5);
                                emit_modrm(MODE_REG, 5, temp_reg);

                                emit1(OPCODE_AND_RM_IMM8_SLASH_4);
                                emit_modrm(MODE_REG, 4, from_reg);
                                emit1((u8)1);

                                emit1(PREFIX_REXW);
                                emit1(OPCODE_OR_RM_REG);
                                emit_modrm(MODE_REG, from_reg, temp_reg);

                                prog->add3(OPCODE_3_PXOR_RM_REG);
                                emit_modrm(MODE_REG, xmm_reg, xmm_reg);

                                prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                                emit_modrm(MODE_REG, xmm_reg, temp_reg);

                                prog->add3(OPCODE_3_ADDSS_REG_RM);
                                emit_modrm(MODE_REG, xmm_reg, xmm_reg);

                                prog->set(jmp_offset_end, prog->size() - (jmp_offset_end +1));

                                emit1(OPCODE_POP_RM_SLASH_0);
                                emit_modrm(MODE_REG, 0, temp_reg);

                                if(!toXmm) {
                                    prog->add3(OPCODE_3_MOVSS_RM_REG);
                                    prog->addModRM_SIB(MODE_DEREF_DISP8, xmm_reg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                    emit1((u8)-8);

                                    emit1(OPCODE_MOV_REG_RM);
                                    prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                    emit1((u8)-8);
                                }
                            } else if(tsize == 8) {
                                /*
                                test   rax,rax
                                js     unsigned
                                pxor   xmm0,xmm0
                                cvtsi2sd xmm0,rax
                                jmp    end
                            unsigned:
                                mov    rdx,rax
                                shr    rdx,1
                                and    eax,0x1
                                or     rdx,rax
                                pxor   xmm0,xmm0
                                cvtsi2sd xmm0,rdx
                                addsd  xmm0,xmm0
                            end:
                                */
                                u8 temp_reg = REG_DI;
                                emit1(OPCODE_PUSH_RM_SLASH_6);
                                emit_modrm(MODE_REG, 6, temp_reg);

                                // Check signed bit
                                emit1(PREFIX_REXW);
                                emit1(OPCODE_TEST_RM_REG);
                                emit_modrm(MODE_REG, from_reg, from_reg);

                                emit1(OPCODE_JS_REL8);
                                int jmp_offset_unsigned = prog->size();
                                emit1((u8)0); // set later

                                // Normal conversion since the signed bit is off
                                prog->add3(OPCODE_3_PXOR_RM_REG);
                                emit_modrm(MODE_REG, xmm_reg, xmm_reg);

                                prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                                emit_modrm(MODE_REG, xmm_reg, from_reg);

                                emit1(OPCODE_JMP_IMM8);
                                int jmp_offset_end = prog->size();
                                emit1((u8)0);

                                prog->set(jmp_offset_unsigned, prog->size() - (jmp_offset_unsigned +1)); // +1 gives the offset after the jump instruction (immediate is one byte)
                                
                                // Special conversion because the signed bit is on but since our conversion is unsigned
                                // we need some special stuff so that our float isn't negative and half the number we except

                                emit1(PREFIX_REXW);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, temp_reg, from_reg);

                                emit1(PREFIX_REXW);    
                                emit1(OPCODE_SHR_RM_ONCE_SLASH_5);
                                emit_modrm(MODE_REG, 5, temp_reg);

                                emit1(OPCODE_AND_RM_IMM8_SLASH_4);
                                emit_modrm(MODE_REG, 4, from_reg);
                                emit1((u8)1);

                                emit1(PREFIX_REXW);
                                emit1(OPCODE_OR_RM_REG);
                                emit_modrm(MODE_REG, from_reg, temp_reg);

                                prog->add3(OPCODE_3_PXOR_RM_REG);
                                emit_modrm(MODE_REG, xmm_reg, xmm_reg);

                                prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                                emit_modrm(MODE_REG, xmm_reg, temp_reg);

                                prog->add3(OPCODE_3_ADDSD_REG_RM);
                                emit_modrm(MODE_REG, xmm_reg, xmm_reg);

                                prog->set(jmp_offset_end, prog->size() - (jmp_offset_end +1));

                                emit1(OPCODE_POP_RM_SLASH_0);
                                emit_modrm(MODE_REG, 0, temp_reg);

                                if(!toXmm) {
                                    prog->add3(OPCODE_3_MOVSD_RM_REG);
                                    prog->addModRM_SIB(MODE_DEREF_DISP8, xmm_reg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                    emit1((u8)-8);

                                    emit1(PREFIX_REXW);
                                    emit1(OPCODE_MOV_REG_RM);
                                    prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                    emit1((u8)-8);
                                }
                            }


                            #ifdef gone
                            // TODO: THIS IS FLAWED! The convert int to float instruction assumes signed integers but
                            //  we are dealing with unsigned ones. That means that large unsigned 64-bit numbers would
                            //  be converted to negative slighly smaller numbers.
                            if(tsize==4)
                                prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                            else
                                prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);

                            if(toXmm) {
                                emit_modrm(MODE_REG, treg, freg);
                            } else {
                                emit_modrm(MODE_REG, REG_XMM7, freg);

                                if(tsize==4)
                                    prog->add3(OPCODE_3_MOVSS_RM_REG);
                                else
                                    prog->add3(OPCODE_3_MOVSD_RM_REG);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM7, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);

                                if(tsize==8)
                                    emit1(PREFIX_REXW);
                                emit1(OPCODE_MOV_REG_RM);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);
                            }

                            // Assert(false);
                            // This code is bugged, doesn't work.
                            /*
                            t = cast<f32>cast<u64>23
                            println(t)
                            */
                            

                            /*
                            test rax, rax
                            jl signed
                            cvtsi2ss xmm0, r13
                            jmp end
                            signed:
                            mov r13d, eax
                            and r13d, 1
                            shr rax, 1
                            or r13, rax
                            cvtsi2ss xmm0, r13
                            mulss xmm0, 2
                            end:
                            */

                        // TODO: This is way to many instructions to convert an unsigned integer to a float.
                        //   Users who don't realize this will unknowningly have slow code because of casts
                        //   they didn't pay attention to. Can we minimize the amount of instructions?
                        //   Can we be less correct and generate fewer instructions?

                            // Assert(false);
                            // We have to do some extra work since we use unsigned 64-bit integer but
                            // the x64 instruction expects a signed 64-bit integer.

                            // TODO: C/C++ compilers use a jump instruction to differentiate between
                            //  the negative and non-negative case. We don't at the moment but probably should.

                            // IMPORTANT: USING A TEMPORARY REG LIKE THIS IS BAD IF OTHER INSTRUCTIONS USES IT.
                            //  You won't expect this register to be modified when you look at the bytecode instructions.
                            // IMPORTANT: The code below modifies the input register!
                            u8 tempReg = REG_R15;
                            u8 xmm = REG_XMM6;
                            if(toXmm) {
                                xmm = treg;
                                Assert(treg != REG_XMM7);
                                // IMPORTANT: XMM7 is in use. More importantly, we assume registers
                                // above xmm3 are unused by the bytecode. We will have many bugs
                                // if the assumption is wrong.
                            }

                            emit1(PREFIX_REXW);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, freg, freg);

                            emit1(OPCODE_JL_REL8);
                            u32 jumpSign = prog->size();
                            emit1((u8)0);

                            // emit1((u8)0xF3);
                            // emit1((u8)(PREFIX_REXW));
                            // prog->add2((u16)0x2A0F);
                            if(tsize == 4) {
                                prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                            } else {
                                prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                            }
                            emit_modrm(MODE_REG, xmm, freg);

                            emit1(OPCODE_JMP_IMM8);
                            u32 jumpEnd = prog->size();
                            emit1((u8)0);

                            prog->set(jumpSign, prog->size() - (jumpSign+1)); // +1 because jumpSign points to the byte to change in the jump instruction, we want the end of the instruction.

                            // emit1((u8)(PREFIX_REXW|PREFIX_REXR));
                            emit1((u8)(PREFIX_REXR)); // 64-bit operands not needed, we just want 1 bit from freg
                            emit1(OPCODE_MOV_REG_RM);
                            emit_modrm(MODE_REG, tempReg, freg);

                            emit1((u8)(PREFIX_REXW|PREFIX_REXB));
                            emit1(OPCODE_AND_RM_IMM8_SLASH_4);
                            emit_modrm(MODE_REG, 4, tempReg);
                            emit1((u8)1);


                            emit1((u8)(PREFIX_REXW));
                            emit1(OPCODE_SHR_RM_IMM8_SLASH_5);
                            emit_modrm(MODE_REG, 5, freg);
                            emit1((u8)1);

                            emit1((u8)(PREFIX_REXW|PREFIX_REXB));
                            emit1(OPCODE_OR_RM_REG);
                            emit_modrm(MODE_REG, freg, tempReg);
                            
                            // F3 REX.W 0F 2A  <- the CVTSI2SS_REG_RM requires the rex byte smashed inside it
                            if(tsize == 4) {
                                emit1((u8)0xF3);
                                emit1((u8)(PREFIX_REXW|PREFIX_REXB));
                                prog->add2((u16)0x2A0F);
                            } else {
                                emit1((u8)0xF2);
                                emit1((u8)(PREFIX_REXW|PREFIX_REXB));
                                prog->add2((u16)0x2A0F);
                            }
                            emit_modrm(MODE_REG, xmm, tempReg);
                            
                            emit1((u8)(PREFIX_REXW|PREFIX_REXB|PREFIX_REXR));
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, tempReg, tempReg);

                            // TODO: REXW may be needed here
                            emit1(PREFIX_REXB);
                            emit1(OPCODE_MOV_RM_IMM8_SLASH_0);
                            emit_modrm(MODE_REG, 0, tempReg);
                            emit1((u8)2);

                            if(tsize == 4)
                                prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                            else
                                prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                            emit_modrm(MODE_REG, REG_XMM7, tempReg);
                            if(tsize == 4) 
                                prog->add3(OPCODE_3_MULSS_REG_RM);
                            else
                                prog->add3(OPCODE_3_MULSD_REG_RM);
                            emit_modrm(MODE_REG, xmm, REG_XMM7);

                            // END
                            prog->set(jumpEnd, prog->size() - (jumpEnd+1)); // +1 because jumpEnd points to the byte to change in the jump instruction, we want the end of the instruction.

                            if (!toXmm) {
                                if(tsize==4)
                                    prog->add3(OPCODE_3_MOVSS_RM_REG);
                                else
                                    prog->add3(OPCODE_3_MOVSD_RM_REG);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, xmm, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);


                                emit1(OPCODE_MOV_REG_RM);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);
                            } else {
                                // xmm register is already loaded with correct value
                            }
                            #endif
                        } else {
                            // nocheckin, rexw when converting u32 to f32/f64 otherwise the last bit will
                            // be treated as a signed bit by CVTSI2SS
                            if(fsize == 1){
                                // emit1(PREFIX_REXW);
                                // emit1(OPCODE_AND_RM_IMM_SLASH_4);
                                // emit_modrm(MODE_REG, 4, freg);
                                // prog->add4((u32)0xFF);
                                if(tsize==4)
                                    prog->add3(OPCODE_3_CVTSI2SS_REG_RM);
                                else
                                    prog->add3(OPCODE_3_CVTSI2SD_REG_RM);
                            } else if(fsize == 2){
                                // emit1(PREFIX_REXW);
                                // emit1(OPCODE_AND_RM_IMM_SLASH_4);
                                // emit_modrm(MODE_REG, 4, freg);
                                // prog->add4((u32)0xFFFF);
                                if(tsize==4)
                                    prog->add3(OPCODE_3_CVTSI2SS_REG_RM);
                                else
                                    prog->add3(OPCODE_3_CVTSI2SD_REG_RM);
                            } else if(fsize == 4) {
                                // It is probably safe to use rexw so that rax is used
                                // when eax was specified. Most instructions zero the upper bits.
                                // There may be an edge case though.
                                // We must use rexw with this operation since it assumes signed values
                                // but we have an unsigned so we must use 64-bit values.
                                emit1(PREFIX_REXW);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, freg, freg); // clear upper 32 bits
                                if(tsize==4)
                                    prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                                else
                                    prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                                // if(tsize==4)
                                //     prog->add3(OPCODE_3_CVTSI2SS_REG_RM);
                                // else
                                //     prog->add3(OPCODE_3_CVTSI2SD_REG_RM);
                            }

                            if(toXmm) {
                                emit_modrm(MODE_REG, treg, freg);
                            } else {
                                emit_modrm(MODE_REG, REG_XMM7, freg);

                                if(tsize==4)
                                    prog->add3(OPCODE_3_MOVSS_RM_REG);
                                else
                                    prog->add3(OPCODE_3_MOVSD_RM_REG);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM7, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);

                                if(tsize==8)
                                    emit1(PREFIX_REXW);
                                emit1(OPCODE_MOV_REG_RM);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);

                                // emit1(OPCODE_NOP);
                            }
                        }
                    } break;
                    #endif
                    case CAST_UINT_SINT:
                    case CAST_SINT_UINT: {
                        if(minSize==1){
                            emit1(PREFIX_REXW);
                            emit2(OPCODE_2_MOVZX_REG_RM8);
                            emit_modrm(MODE_REG, origin_reg, origin_reg);
                        } else if(minSize == 2) {
                            emit1(PREFIX_REXW);
                            emit2(OPCODE_2_MOVZX_REG_RM16);
                            emit_modrm(MODE_REG, origin_reg, origin_reg);
                        } else if(minSize == 4) {
                            
                            emit1(OPCODE_MOV_REG_RM);
                            emit_modrm(MODE_REG, origin_reg, origin_reg);
                            
                        } else if(minSize == 8) {
                            // nothing needs to be done
                        }
                    } break;
                    case CAST_SINT_SINT: {
                        // i8 -> i16,i32,i64
                        // i64 -> i32,i16,i8

                        // TODO: Sign extend properly
                        // emit1(PREFIX_REXW);
                        // emit1(OPCODE_MOV_REG_RM);
                        // emit_modrm(MODE_REG, treg, freg);
                        if(minSize==1){
                            emit1(PREFIX_REXW);
                            emit2(OPCODE_2_MOVSX_REG_RM8);
                            emit_modrm(MODE_REG, origin_reg, origin_reg);
                        } else if(minSize == 2) {
                            emit1(PREFIX_REXW);
                            emit2(OPCODE_2_MOVSX_REG_RM16);
                            emit_modrm(MODE_REG, origin_reg, origin_reg);
                        } else if(minSize == 4) {
                            // Assert(freg == treg);
                            emit1(PREFIX_REXW);
                            emit1(OPCODE_MOVSXD_REG_RM);
                            emit_modrm(MODE_REG, origin_reg, origin_reg);
                            // emit1(OPCODE_NOP); // this might just work
                            // emit1(PREFIX_REXW);
                            // emit1(OPCODE_AND_RM_IMM_SLASH_4);
                            // emit_modrm(MODE_REG, 4, treg);
                            // prog->add4((u32)0xFFFFFFFF);
                        } else if(minSize == 8) {
                            // nothing needs to be done
                        }
                    } break;
                    case CAST_FLOAT_FLOAT: {
                        Assert(false);
                        #ifdef gone
                        Assert((fsize == 4 || fsize == 8) && (tsize == 4 || tsize == 8));

                        if(fromXmm && toXmm) {
                            if(fsize == 4 && tsize == 8) {
                                prog->add3(OPCODE_3_CVTSS2SD_REG_RM);
                                emit_modrm(MODE_REG, treg, freg);
                            } else if(fsize == 8 && tsize == 4) {
                                prog->add3(OPCODE_3_CVTSD2SS_REG_RM);
                                emit_modrm(MODE_REG, treg, freg);
                            } else {
                                // do nothing
                            }
                        } else if(!fromXmm && toXmm) {
                            u8 temp = REG_XMM7;
                            if(fsize == 8)
                                emit1(PREFIX_REXW);
                            emit1(OPCODE_MOV_RM_REG);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, freg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            emit1((u8)-8);

                            // NOTE: Do we need REXW one some of these?
                            if(fsize == 4 && tsize == 8) {
                                prog->add3(OPCODE_3_CVTSS2SD_REG_RM);
                            } else if(fsize == 8 && tsize == 4) {
                                prog->add3(OPCODE_3_CVTSD2SS_REG_RM);
                            } else if(fsize == 4 && tsize == 4){
                                prog->add3(OPCODE_3_MOVSS_REG_RM);
                            } else if(fsize == 8 && tsize == 8) {
                                prog->add3(OPCODE_3_MOVSD_REG_RM);
                            }
                            prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            emit1((u8)-8);
                        } else if(fromXmm && !toXmm) {
                            u8 temp = REG_XMM7;
                            if(fsize == 4 && tsize == 8) {
                                prog->add3(OPCODE_3_CVTSS2SD_REG_RM);
                                emit_modrm(MODE_REG, temp, freg);

                                prog->add3(OPCODE_3_MOVSD_RM_REG);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, temp, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);

                                emit1(PREFIX_REXW);
                                emit1(OPCODE_MOV_REG_RM);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);
                            } else if(fsize == 8 && tsize == 4) {
                                prog->add3(OPCODE_3_CVTSD2SS_REG_RM);
                                emit_modrm(MODE_REG, temp, freg);

                                prog->add3(OPCODE_3_MOVSS_RM_REG);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, temp, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);

                                emit1(OPCODE_MOV_REG_RM);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);
                            } else if(fsize == 4 && tsize == 4){
                                prog->add3(OPCODE_3_MOVSS_RM_REG);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, freg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);

                                emit1(OPCODE_MOV_REG_RM);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);
                            } else if(fsize == 8 && tsize == 8) {
                                prog->add3(OPCODE_3_MOVSD_RM_REG);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, freg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);

                                emit1(PREFIX_REXW);
                                emit1(OPCODE_MOV_REG_RM);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);
                            }
                        } else if(!fromXmm && !toXmm) {
                            u8 temp = REG_XMM7;
                            if(fsize == 8)
                                emit1(PREFIX_REXW);
                            emit1(OPCODE_MOV_RM_REG);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, freg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            emit1((u8)-8);

                            // NOTE: Do we need REXW here?
                            if(fsize == 4 && tsize == 8) {
                                prog->add3(OPCODE_3_CVTSS2SD_REG_RM);
                            } else if(fsize == 8 && tsize == 4) {
                                prog->add3(OPCODE_3_CVTSD2SS_REG_RM);
                            } else if(fsize == 4 && tsize == 4){
                                prog->add3(OPCODE_3_MOVSS_REG_RM);
                            } else if(fsize == 8 && tsize == 8) {
                                prog->add3(OPCODE_3_MOVSD_REG_RM);
                            }
                            prog->addModRM_SIB(MODE_DEREF_DISP8, temp, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            emit1((u8)-8);

                            if(tsize == 4) {
                                prog->add3(OPCODE_3_MOVSS_RM_REG);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, temp, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);
                                
                                emit1(OPCODE_MOV_REG_RM);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);
                            } else if(tsize == 8) {
                                prog->add3(OPCODE_3_MOVSS_RM_REG);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, temp, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);

                                emit1(PREFIX_REXW);
                                emit1(OPCODE_MOV_REG_RM);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                emit1((u8)-8);
                            }
                        }
                        #endif
                    } break;
                    default: Assert(("Cast type not implemented in x64 backend, Compiler bug",false));
                }
                
                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
            } break;
            case BC_DATAPTR: {
                if(env->reg0.invalid()) {
                    env->reg0.reg = alloc_register();
                    if(env->reg0.reg == X64_REG_INVALID) {
                        env->reg0.on_stack = true;
                        env->reg0.reg = RESERVED_REG0;
                    }
                } else {
                    // already allocated
                }
                Assert(!IS_REG_XMM(env->reg0.reg)); // loading pointer into xmm makes no sense, it's a compiler bug

                emit_prefix(PREFIX_REXW, env->reg0.reg, X64_REG_INVALID);
                emit1(OPCODE_LEA_REG_M);
                emit_modrm_rip32(CLAMP_EXT_REG(env->reg0.reg), (u32)0);
                i32 disp32_offset = size() - 4; // -4 to refer to the 32-bit immediate in modrm_rip
                prog->addDataRelocation(n->imm, disp32_offset, current_tinyprog_index);

                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
            } break;
            case BC_CODEPTR: {
                Assert(false);
                // u8 size = DECODE_REG_SIZE(op0);
                // Assert(size==8);

                // prog->add(PREFIX_REXW);
                // prog->add(OPCODE_LEA_REG_M);
                // prog->addModRM_rip(BCToProgramReg(op0,8), (u32)0);
                // RelativeRelocation reloc{};
                // reloc.immediateToModify = prog->size()-4;
                // reloc.currentIP = prog->size();
                // reloc.bcAddress = imm;
                // relativeRelocations.add(reloc);
                // // NamedRelocation reloc{};
                // // reloc.dataOffset = imm;
                // // reloc.
                // // reloc.textOffset = prog->size() - 4;
                // // prog->dataRelocations.add(reloc);
                // // prog->namedRelocations.add();
            } break;
            default: Assert(false);
        }
        if(pop_env) {
            envs.pop();   
        }
    }
    
    // TODO: If you have many relocations you can use process them using multiple threads.
    //  It can't be too few because the overhead of managing the threads would be worse
    //  than the performance gain.
    for(int i=0;i<relativeRelocations.size();i++) {
        auto& rel = relativeRelocations[i];
        if(rel.bcAddress == tinycode->instructionSegment.size())
            set_imm32(rel.immediateToModify, size() - rel.currentIP);
        else {
            Assert(rel.bcAddress>=0); // native functions not implemented
            i32 value = bc_to_x64_translation[rel.bcAddress] - rel.currentIP;
            set_imm32(rel.immediateToModify, value);
            // If you are wondering why the relocation is zero then it is because it
            // is relative and will jump to the next instruction. This isn't necessary so
            // they should be optimised away. Don't do this now since it makes the conversion
            // confusing.
        }
    }

    // last instruction should be a return
    Assert(tinyprog->text[tinyprog->head-1] == OPCODE_RET);
}
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
                X64_REG_XMM0,
                X64_REG_XMM1,
                X64_REG_XMM2,
                X64_REG_XMM3,
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
void X64Builder::fix_relative_jump_imm8(u32 offset, u8 value) {
    *(u8*)(tinyprog->text + offset) = value;
}
void X64Builder::set_imm32(u32 offset, u32 value) {
    *(u32*)(tinyprog->text + offset) = value;
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
    if(_rm == X64_REG_SP && mod != MODE_REG) {
        // SP register is not allowed with standard modrm byte, we must use a SIB
        emit_modrm_sib(mod, _reg, SIB_SCALE_1, SIB_INDEX_NONE, _rm);
        return;
    }
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
void X64Builder::emit_modrm_rip32(X64Register _reg, u32 disp32){
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
void X64Builder::emit_modrm_sib_slash(u8 mod, u8 reg, u8 scale, u8 index, X64Register _base){
    u8 base = _base - 1;
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

void X64Builder::emit_push(X64Register reg) {
    emit_prefix(0, X64_REG_INVALID, reg);
    emit1(OPCODE_PUSH_RM_SLASH_6);
    emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg));
}
void X64Builder::emit_pop(X64Register reg) {
    emit_prefix(0, X64_REG_INVALID, reg);
    emit1(OPCODE_POP_RM_SLASH_0);
    emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg));
}
void X64Builder::emit_add_imm32(X64Register reg, i32 imm32) {
    emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg);
    emit1(OPCODE_ADD_RM_IMM_SLASH_0);
    emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg));
    emit4((u32)imm32); // NOTE: cast from i16 to i32 to u32, should be fine
}
void X64Builder::emit_sub_imm32(X64Register reg, i32 imm32) {
    emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg);
    emit1(OPCODE_SUB_RM_IMM_SLASH_5);
    emit_modrm_slash(MODE_REG, 5, CLAMP_EXT_REG(reg));
    emit4((u32)imm32);
}
void X64Builder::emit_prefix(u8 inherited_prefix, X64Register reg, X64Register rm) {
    if(reg != X64_REG_INVALID) {
        if(IS_REG_EXTENDED(reg)) {
            inherited_prefix |= PREFIX_REXR;
        } else if(IS_REG_NORM(reg)) {
            
        } else Assert(false); // float register not allowed like this
    }
    if(rm != X64_REG_INVALID) {
        if(IS_REG_EXTENDED(rm)) {
            inherited_prefix |= PREFIX_REXB;
        } else if(IS_REG_NORM(rm)) {
            
        } else Assert(false); // float register not allowed like this
    }
    if(inherited_prefix)
        emit1(inherited_prefix);
}