// #include "BetBat/v2/Interpreter.h"
// #include "Engone/Logger.h"

// #define BITS(P,B,E,S) ((P<<(S-E))>>B)

// #define DECODE_OPCODE(ptr) (*((u8*)ptr))
// #define DECODE_TYPE(ptr) (*((u8*)ptr+1))
// #define DECODE_REG_X(ptr) (*((u8*)ptr+2))
// #define DECODE_REG_Y(ptr) (*((u8*)ptr+3))
// #define DECODE_REG_Z(ptr) (*((u8*)ptr+4))


// void* Interpreter::getReg(u8 id){
//     switch(id){
//         case BC_REG_RAX: return &rax;
//     }
//     return 0;
// }

// // void Interpreter::execute(Bytecode* bytecode){
// //     using namespace engone;
// //     while (true) {
        
        
// //         // check used

// //         u8* bc_ptr = (u8*)bytecode->codeSegment.data + rpc;
// //         u8 opcode = DECODE_OPCODE(bc_ptr);
// //         switch (opcode) {
// //         case BC_MOV_RR:{
// //             u8 r0 = DECODE_REG_X(bc_ptr);
// //             u8 r1 = DECODE_REG_Y(bc_ptr);
// //             rpc+=3;

// //             if ((BC_REG_MASK&r0)!=(BC_REG_MASK&r1)){
// //                 log::out << "bad\n";
// //                 continue;
// //             }

// //             void* ptr0 = getReg(r0);
// //             void* ptr1 = getReg(r1);
// //             if(r0&BC_REG_8)
// //                 *((u8*) ptr1) = *((u8*) ptr0);
// //             if(r0&BC_REG_16)
// //                 *((u8*) ptr1) = *((u16*) ptr0);
// //             if(r0&BC_REG_32)
// //                 *((u32*) ptr1) = *((u32*) ptr0);
// //             if(r0&BC_REG_64)
// //                 *((u64*) ptr1) = *((u64*) ptr0);

// //             break;
// //         }
// //         }
// //     }
// // }