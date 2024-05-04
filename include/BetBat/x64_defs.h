#pragma once

/*
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

    Instructions that operate on 8-bit operands will NOT clear upper bits
    Instructions with 32-bit operands WILL clear upper bits
                 with 16-bit operands will NOT clear upper bits
    SETcc will always set the 8 lower bits to either 1 or 0

    RM_REG means: Add REG to RM, RM = RM + REG
    REG_RM: REG = REG + RM
*/

// https://www.felixcloutier.com/x86/index.html
// https://defuse.ca/online-x86-assembler.htm#disassembly2
// https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

#define OPCODE_RET (u8)0xC3
#define OPCODE_CALL_IMM (u8)0xE8
#define OPCODE_CALL_RM_SLASH_2 (u8)0xFF
#define OPCODE_NOP (u8)0x90


#define OPCODE_ADD_REG8_RM8 (u8)0x02
#define OPCODE_ADD_REG_RM (u8)0x03
#define OPCODE_ADD_RM_REG (u8)0x01
#define OPCODE_ADD_RM_IMM_SLASH_0 (u8)0x81
#define OPCODE_ADD_RM_IMM8_SLASH_0 (u8)0x83

#define OPCODE_SUB_REG8_RM8 (u8)0x2A
#define OPCODE_SUB_REG_RM (u8)0x2B
#define OPCODE_SUB_RM_IMM_SLASH_5 (u8)0x81

// This instruction has reg field in opcode.
// Extension of general registers is done with REXB, not REXR
// With ModR/M REXR is for reg field while REXB is for r/m.
// But not in this case.
// See x64 manual.
#define OPCODE_MOV_REG_IMM_RD (u8)0xB8

#define OPCODE_MOV_RM_IMM32_SLASH_0 (u8)0xC7
#define OPCODE_MOV_RM_REG8 (u8)0x88
#define OPCODE_MOV_REG8_RM8 (u8)0x8A
#define OPCODE_MOV_RM_REG (u8)0x89
#define OPCODE_MOV_REG_RM (u8)0x8B

#define OPCODE_MOV_RM_IMM8_SLASH_0 (u8)0xC6

#define OPCODE_LEA_REG_M (u8)0x8D

// 2 means that the opcode takes 2 bytes
// note that the bytes are swapped
#define OPCODE_2_IMUL_REG_RM (u16)0xAF0F

#define OPCODE_IMUL_A_RM_SLASH_5 (u8)0xF7
#define OPCODE_IMUL_A8_RM8_SLASH_5 (u8)0xF6

#define OPCODE_MUL_A_RM_SLASH_4 (u8)0xF7
#define OPCODE_MUL_A8_RM8_SLASH_4 (u8)0xF6

#define OPCODE_IDIV_A_RM_SLASH_7 (u8)0xF7
#define OPCODE_IDIV_A8_RM8_SLASH_7 (u8)0xF6

#define OPCODE_DIV_A_RM_SLASH_6 (u8)0xF7
#define OPCODE_DIV_A8_RM8_SLASH_6 (u8)0xF6

// sign extends EAX into EDX, useful for IDIV
#define OPCODE_CDQ (u8)0x99

#define OPCODE_XOR_REG8_RM8 (u8)0x32
#define OPCODE_XOR_REG_RM (u8)0x33
#define OPCODE_XOR_RM_IMM8_SLASH_6 (u8)0x83

#define OPCODE_AND_REG8_RM8 (u8)0x22
#define OPCODE_AND_REG_RM (u8)0x23

#define OPCODE_OR_REG8_RM8 (u8)0x0A
#define OPCODE_OR_REG_RM (u8)0x0B

#define OPCODE_SHL_RM_CL_SLASH_4 (u8)0xD3
#define OPCODE_SHR_RM_CL_SLASH_5 (u8)0xD3

#define OPCODE_SHL_RM_IMM8_SLASH_4 (u8)0xC1
#define OPCODE_SHR_RM_IMM8_SLASH_5 (u8)0xC1

#define OPCODE_SHR_RM_ONCE_SLASH_5 (u8)0xD1

// logical and with flags being set, registers are not modified
#define OPCODE_TEST_RM_REG (u8)0x85

#define OPCODE_JL_REL8 (u8)0x7C
#define OPCODE_JGE_REL8 (u8)0x7D
#define OPCODE_JS_REL8 (u8)0x78

#define OPCODE_NOT_RM_SLASH_2 (u8)0xF7

// NOTE: SETcc will always set the value to 0 or 1.
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

// sign extension, use 0x66 prefix for 16-bit
#define OPCODE_2_MOVSX_REG_RM8 (u16)0xBE0F
// intel manual encourages REX.W with MOVSXD. Use normal mov otherwise
#define OPCODE_MOVSXD_REG_RM (u8)0x63

// "FF /6" can be seen in x86 instruction sets.
// This means that the REG field in ModRM should be 6
// to use the push instruction
#define OPCODE_PUSH_RM_SLASH_6 (u8)0xFF
#define OPCODE_POP_RM_SLASH_0 (u8)0x8F

#define OPCODE_INC_RM_SLASH_0 (u8)0xFF
#define OPCODE_INC_REG32_RD (u8)0x40

#define OPCODE_CMP_RM_IMM8_SLASH_7 (u8)0x83 // rm can be 64-bit with rexw
#define OPCODE_CMP_REG_RM (u8)0x3B
#define OPCODE_CMP_REG8_RM8 (u8)0x3A

#define OPCODE_JMP_IMM32 (u8)0xE9

// bytes are flipped
#define OPCODE_2_JNE_IMM32 (u16)0x850F
#define OPCODE_2_JE_IMM32 (u16)0x840F
// je rel8
#define OPCODE_JE_IMM8 (u8)0x74
#define OPCODE_JZ_IMM8 (u8)0x74

#define OPCODE_JNE_IMM8 (u8)0x75
#define OPCODE_JNZ_IMM8 (u8)0x75


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
#define OPCODE_3_CVTSS2SD_REG_RM (u32)0x5A0FF3


// float comparisson, sets flags, see https://www.felixcloutier.com/x86/comiss
#define OPCODE_2_COMISS_REG_RM (u16)0x2F0F
#define OPCODE_2_UCOMISS_REG_RM (u16)0x2E0F

#define OPCODE_3_COMISD_REG_RM (u32)0x2F0F66

#define OPCODE_4_ROUNDSS_REG_RM_IMM8 (u32)0x0A3A0F66
#define OPCODE_4_ROUNDSD_REG_RM_IMM8 (u32)0x0B3A0F66
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
// needed with 8-bit operand to access sil and dil instead of dh and bh
#define PREFIX_REX (u8)0b01000000
#define PREFIX_16BIT (u8)0x66
#define PREFIX_LOCK (u8)0xF0