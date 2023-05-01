// #pragma once
// /*
//     Bytecode for version 2 of the language

//     instructions vary in size
// */

// #include "Engone/Alloc.h"
// #include "Engone/Typedefs.h"

// #define BC_MOV_RR 1
// #define BC_MOV_RM 2
// #define BC_MOV_MR 3
// #define BC_ADD 4
// #define BC_SUB 5
// #define BC_MUL 6
// #define BC_DIV 7
// #define BC_JUMP 8
// #define BC_CALL 9
// #define BC_RET 10
// #define BC_PUSH 11
// #define BC_POP 12

// // struct Interpreter {
// //     union {
// //         u64 rax;
// //         struct {
// //             u32 eax;
// //         };
// //         struct {
// //             u16 ax;
// //         };
// //         struct {
// //             u8 al;
// //             u8 ah;
// //         };
// //     };
// // }

// #define BC_REG_MASK 0xC0
// #define BC_REG_8 0x00
// #define BC_REG_16 0x40
// #define BC_REG_32 0x80
// #define BC_REG_64 0xC0

// #define BC_REG_AL (BC_REG_8|1)
// #define BC_REG_AH (BC_REG_8|2)
// #define BC_REG_BL 6
// #define BC_REG_BH 7

// #define BC_REG_AX (BC_REG_16|3)
// #define BC_REG_EAX (BC_REG_32|4)
// #define BC_REG_RAX (BC_REG_64|5)
// #define BC_REG_BX 8
// #define BC_REG_EBX 9
// #define BC_REG_RBX 10

// // struct Bytecode {
// //     engone::Memory codeSegment{1};
// // };