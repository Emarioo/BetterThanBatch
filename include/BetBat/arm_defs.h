#pragma once

// https://developer.arm.com/documentation/ddi0602/2024-09/Base-Instructions?lang=en
// https://www.youtube.com/watch?v=laovVqgbOH8&list=PLgIjRMdFBe6uKsHSSPyPSno9x4emd0c4p


#define ARM_COND_EQ 0b0000 // Equal	Equal	Z == 1
#define ARM_COND_NE 0b0001 // Not equal	Not equal, or unordered	Z == 0
#define ARM_COND_CS 0b0010 // Carry set	Greater than, equal, or unordered	C == 1
#define ARM_COND_CC 0b0011 // Carry clear	Less than	C == 0
#define ARM_COND_MI 0b0100 // Minus, negative	Less than	N == 1
#define ARM_COND_PL 0b0101 // Plus, positive or zero	Greater than, equal, or unordered	N == 0
#define ARM_COND_VS 0b0110 // Overflow	Unordered	V == 1
#define ARM_COND_VC 0b0111 // No overflow	Not unordered	V == 0
#define ARM_COND_HI 0b1000 // Unsigned higher	Greater than, or unordered	C == 1 and Z == 0
#define ARM_COND_LS 0b1001 // Unsigned lower or same	Less than or equal	C == 0 or Z == 1
#define ARM_COND_GE 0b1010 // Signed greater than or equal	Greater than or equal	 N == V
#define ARM_COND_LT 0b1011 // Signed less than	Less than, or unordered	 N != V
#define ARM_COND_GT 0b1100 // Signed greater than	Greater than	 Z == 0 and N == V
#define ARM_COND_LE 0b1101 // Signed less than or equal	Less than, equal, or unordered	 Z == 1 or N != V
#define ARM_COND_AL 0b1110 // Always (unconditional)	Always (unconditional)	Any

#define ARM_SHIFT_TYPE_LSL 0b00
#define ARM_SHIFT_TYPE_LSR 0b01
#define ARM_SHIFT_TYPE_ASR 0b10
#define ARM_SHIFT_TYPE_ROR 0b11

#define BITS(VALUE, BIT_COUNT, BIT_START) (((VALUE) & ((1 << (BIT_COUNT)) -1)) << (BIT_START))

#define ARM_CLAMP_dnm() u8 Rd = rd-1; u8 Rn = rn-1; u8 Rm = rm-1; Assert(rd > 0 && rd < ARM_REG_MAX); Assert(rn > 0 && rn < ARM_REG_MAX); Assert(rm > 0 && rm < ARM_REG_MAX);
#define ARM_CLAMP_dm() u8 Rd = rd-1; u8 Rm = rm-1; Assert(rd > 0 && rd < ARM_REG_MAX); Assert(rm > 0 && rm < ARM_REG_MAX);
#define ARM_CLAMP_d() u8 Rd = rd-1; Assert(rd > 0 && rd < ARM_REG_MAX);