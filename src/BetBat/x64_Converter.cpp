#include "BetBat/x64_Converter.h"

#include "BetBat/ObjectWriter.h"

void ConversionInfo::write(u8 byte){
    textSegment.add(byte);
}
bool Program_x64::_reserve(u32 newAllocationSize){
    if(newAllocationSize==0){
        if(_allocationSize!=0){
            engone::Free(text, _allocationSize);
        }
        text = nullptr;
        _allocationSize = 0;
        size = 0;
        return true;
    }
    if(!text){
        text = (u8*)engone::Allocate(newAllocationSize);
        Assert(text);
        // initialization of elements is done when adding them
        if(!text)
            return false;
        _allocationSize = newAllocationSize;
        return true;
    } else {
        u8* newText = (u8*)engone::Reallocate(text, _allocationSize, newAllocationSize);
        Assert(newText);
        if(!newText)
            return false;
        text = newText;
        _allocationSize = newAllocationSize;
        if(size > newAllocationSize){
            size = newAllocationSize;
        }
        return true;
    }
    return false;
}

void Program_x64::add(u8 byte){
    if(size+1 >= _allocationSize ){
        Assert(_reserve(_allocationSize*2 + 100));
    }
    *(text + size) = byte;
    size++;
}
void Program_x64::add2(u16 word){
    if(size+2 >= _allocationSize ){
        Assert(_reserve(_allocationSize*2 + 100));
    }
    
    auto ptr = (u8*)&word;
    *(text + size + 0) = *(ptr + 0);
    *(text + size + 1) = *(ptr + 1);
    size+=2;
}
void Program_x64::add4(u32 dword){
    if(size+4 >= _allocationSize ){
        Assert(_reserve(_allocationSize*2 + 100));
    }
    auto ptr = (u8*)&dword;

    // deals with non-alignment
    *(text + size + 0) = *(ptr + 0);
    *(text + size + 1) = *(ptr + 1);
    *(text + size + 2) = *(ptr + 2);
    *(text + size + 3) = *(ptr + 3);
    size+=4;
}
void Program_x64::addModRM(u8 mod, u8 reg, u8 rm){
    add((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));

}

void Program_x64::printHex(){
    using namespace engone;
    Assert(this);
    log::out << log::LIME << "HEX:\n";
    #define HEXIFY(X) (char)(X<10 ? '0'+X : 'A'+X - 10)
    for(int i = 0;i<size;i++){
        u8 a = text[i]>>4;
        u8 b = text[i]&0xF;
        log::out << HEXIFY(a) << HEXIFY(b) <<" ";
    }
    log::out << "\n";
}

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

// RM_REG means: Add REG to RM, RM = RM + REG
// REG_RM: REG = REG + RM

// #define OPCODE_ADD_RM_REG (u8)0x01
#define OPCODE_ADD_REG_RM (u8)0x03
#define OPCODE_ADD_RM_IMM_SLASH_0 (u8)0x81

#define OPCODE_SUB_REG_RM (u8)0x2B

#define OPCODE_MOV_RM_IMM32_SLASH_0 (u8)0xC7
#define OPCODE_MOV_RM_REG (u8)0x89
#define OPCODE_MOV_REG_RM (u8)0x8B

#define OPCODE_MOV_REG8_RM8 (u8)0x8A

// 2 means that the opcode takes 2 bytes
// note that the bytes are swapped
#define OPCODE_2_IMUL_REG_RM (u16)0xAF0F
#define OPCODE_IDIV_RM_SLASH_7 (u8)0xF7

// sign extends EAX into EDX, useful for IDIV
#define OPCODE_CDQ (u8)0x99

#define OPCODE_XOR_REG_RM (u8)0x33
#define OPCODE_XOR_RM_IMM8_SLASH_6 (u8)0x83

#define OPCODE_AND_RM_REG (u8)0x21

#define OPCODE_OR_RM_REG (u8)0x09

#define OPCODE_SHL_RM_CL_SLASH_4 (u8)0xD3
#define OPCODE_SHR_RM_CL_SLASH_5 (u8)0xD3

// logical and with flags being set, registers are not modified
#define OPCODE_TEST_RM_REG (u8)0x85

#define OPCODE_NOT_RM_SLASH_2 (u8)0xF7

#define OPCODE_2_SETE_RM8 (u16)0x940F
#define OPCODE_2_SETNE_RM8 (u16)0x950F

#define OPCODE_2_SETG_RM8 (u16)0x9F0F
#define OPCODE_2_SETGE_RM8 (u16)0x9D0F
#define OPCODE_2_SETL_RM8 (u16)0x9C0F
#define OPCODE_2_SETLE_RM8 (u16)0x9E0F

// sign extension
#define OPCODE_2_MOVZX_REG_RM8 (u16)0xB60F
#define OPCODE_2_MOVZX_REG_RM16 (u16)0xB70F

#define OPCODE_2_CMOVZ_REG_RM (u16)0x440F

// "FF /6" can be seen in x86 instruction sets.
// This means that the REG field in ModRM should be 6
// to use the push instruction
#define OPCODE_PUSH_RM_SLASH_6 (u8)0xFF
#define OPCODE_POP_RM_SLASH_0 (u8)0x8F

#define OPCODE_INCR_RM_SLASH_0 (u8)0xFF

#define OPCODE_JMP_IMM32 (u8)0xE9

#define OPCODE_CMP_RM_IMM8_SLASH_7 (u8)0x83

#define OPCODE_CMP_REG_RM (u8)0x3B

// bytes are flipped
#define OPCODE_2_JNE_IMM32 (u16)0x850F
#define OPCODE_2_JE_IMM32 (u16)0x840F

// the three other modes deal with memory
#define MODE_REG 0b11
// straight deref, no displacement
// SP, BP does not work with this! see intel manual 32-bit addressing forms
// TODO: Test that this works
#define MODE_DEREF 0b00

#define PREFIX_REXW (u8)0b01001000

#define REG_A 0b000
#define REG_C 0b001
#define REG_D 0b010
#define REG_B 0b011
#define REG_SP 0b100
#define REG_BP 0b101
#define REG_SI 0b110
#define REG_DI 0b111

u8 BC_TO_64_REG(u8 bcreg, int handlingSizes = 4){
    u8 size = DECODE_REG_SIZE(bcreg);
    Assert(size&handlingSizes);

    u8 id = (bcreg&(~BC_REG_MASK)) - 1;

    return id;
}

Program_x64* ConvertTox64(Bytecode* bytecode){
    using namespace engone;
    Assert(bytecode);

    Program_x64* prog = (Program_x64*)Allocate(sizeof(Program_x64));
    new(prog)Program_x64();

    bool failure = false;

    // prog->add(OPCODE_XOR_REG_RM);
    // prog->addModRM(MODE_REG,REG_A,REG_A);
    // prog->add(OPCODE_JMP_IMM);
    // prog->add4(8);
    // for(int i=0;i<16;i++){
    //     prog->add(OPCODE_INCR_RM_SLASH_0);
    //     prog->addModRM(MODE_REG, 0, REG_A);
    // }

    // TODO: Can this be improved?
     // array to translate addresses of bytecode instructions to
     // the ones in x64 instructions.
    DynamicArray<i32> addressTranslation;
    addressTranslation.resize(bytecode->length());

    struct RelativeRelocation {
        RelativeRelocation(i32 ip, i32 x64, i32 bc) : currentIP(ip), immediateToModify(x64), bcAddress(bc) {}
        i32 currentIP=0; // It's more of a nextIP rarther than current. Needed when calculating relative offset
        // Address MUST point to an immediate (or displacement?) with 4 bytes.
        // You can add some flags to this struct for more variation.
        i32 immediateToModify=0; // dword/i32 to modify with correct value
        i32 bcAddress=0; // address in bytecode, needed when looking up absolute offset in addressTranslation
    };
    DynamicArray<RelativeRelocation> relativeRelocations;
    relativeRelocations._reserve(40);

    // for(int i=0;i<0;i++){
    for(int i=0;i<bytecode->length();i++){
        auto inst = bytecode->get(i);
        addressTranslation[i] = prog->size;
        u8 opcode = inst.opcode;
        i32 imm = 0;
        log::out << inst;
        if(opcode == BC_LI || opcode==BC_JMP || opcode==BC_JE || opcode==BC_JNE ){
            i++;
            imm = bytecode->getIm(i);
            addressTranslation[i] = prog->size;
            log::out << imm;
        }
        log::out << "\n";
        
        u32 prevSize = prog->size;

        auto op0 = inst.op0;
        auto op1 = inst.op1;
        auto op2 = inst.op2;
        switch(opcode){
            case BC_MOV_RR: {
                prog->add(OPCODE_MOV_REG_RM);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op1), BC_TO_64_REG(op0));
                break;
            }
            case BC_MOV_RM: {
                // TODO: Bytecode could use an offset here but I don't think
                //   the generator uses this so we can leave it be.
                
                i8 offset = (i8)op2; // assert in case the offset is used
                Assert(offset == 0);

                prog->add(OPCODE_MOV_RM_REG);
                prog->addModRM(MODE_DEREF, BC_TO_64_REG(op1), BC_TO_64_REG(op0));
                break;
            }
            case BC_MOV_MR: {
                // TODO: Bytecode could use an offset here but I don't think
                //   the generator uses this so we can leave it be.
                
                i8 offset = (i8)op2; // assert in case the offset is used
                Assert(offset == 0);

                prog->add(OPCODE_MOV_REG_RM);
                prog->addModRM(MODE_DEREF, BC_TO_64_REG(op1), BC_TO_64_REG(op0));
                break;
            }
            case BC_ADDI: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;
                // add doesn't use three operands

                prog->add(OPCODE_ADD_REG_RM);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op2), BC_TO_64_REG(op01));
                break;
            }
            case BC_SUBI: {
                Assert(op0 == op2);
                // Assert(op0 == op2 || op1 == op2);
                // u8 op01 = op0 == op2 ? op1 : op0;

                prog->add(OPCODE_SUB_REG_RM);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op2), BC_TO_64_REG(op1));
                break;
            }
            case BC_MULI: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;

                // TODO: What about overflow? Should we always do signed multiplication?
                //   Overflow is truncated and no other registers are messed.
                prog->add2(OPCODE_2_IMUL_REG_RM);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op2), BC_TO_64_REG(op01));

                break;
            }
            case BC_DIVI: {
                Assert(DECODE_REG_TYPE(op0) == BC_AX &&
                 DECODE_REG_TYPE(op1) != BC_AX &&
                 DECODE_REG_TYPE(op1) != BC_DX
                  );

                
                if(DECODE_REG_TYPE(op2) != BC_AX){
                    prog->add(OPCODE_PUSH_RM_SLASH_6);
                    prog->addModRM(MODE_REG, 6, REG_A);
                }
                
                if(DECODE_REG_TYPE(op2) != BC_DX){
                    prog->add(OPCODE_PUSH_RM_SLASH_6);
                    prog->addModRM(MODE_REG, 6, REG_D);
                }

                // I CANNOT USE EDX, EAX
                // op0 if eax do nothing, just sign extend, otherwise move op0 into eax
                // 
                // op2 can be whatever, move result to it at the end

                if(DECODE_REG_TYPE(op0)!=BC_AX){
                    // move reg in op0 to eax
                    prog->add(OPCODE_MOV_REG_RM);
                    prog->addModRM(MODE_REG, REG_A, BC_TO_64_REG(op0));
                }
                prog->add(OPCODE_CDQ); // sign extend eax into edx

                // TODO: What about overflow? Should we always do signed multiplication?
                prog->add(OPCODE_IDIV_RM_SLASH_7);
                prog->addModRM(MODE_REG, 7, BC_TO_64_REG(op1));

                if(DECODE_REG_TYPE(op2) != BC_DX){
                    prog->add(OPCODE_POP_RM_SLASH_0);
                    prog->addModRM(MODE_REG, 0, REG_D);
                }
                if(DECODE_REG_TYPE(op2) != BC_AX){
                    prog->add(OPCODE_MOV_REG_RM);
                    prog->addModRM(MODE_REG, BC_TO_64_REG(op2), REG_A);
                    prog->add(OPCODE_POP_RM_SLASH_0);
                    prog->addModRM(MODE_REG, 0, REG_A);
                }

                break;
            }
            case BC_EQ: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;

                // TODO: Test if this is okay?
                prog->add(OPCODE_XOR_REG_RM);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op2), BC_TO_64_REG(op01));
                
                prog->add2(OPCODE_2_SETE_RM8);
                prog->addModRM(MODE_REG, 0, BC_TO_64_REG(op2));

                prog->add2(OPCODE_2_MOVZX_REG_RM8);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op2), BC_TO_64_REG(op2));

                break;
            }
            case BC_NEQ: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;

                prog->add(OPCODE_XOR_REG_RM);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op2), BC_TO_64_REG(op01));
                break;
            }
            case BC_NOT: {
                Assert(op0 == op1);

                // Assert(op0 != op1);
                // prog->add(OPCODE_XOR_REG_RM);
                // prog->addModRM(MODE_REG, BC_TO_64_REG(op1), BC_TO_64_REG(op1));

                prog->add(OPCODE_TEST_RM_REG);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op0), BC_TO_64_REG(op0));

                prog->add2(OPCODE_2_SETE_RM8);
                prog->addModRM(MODE_REG, 0, BC_TO_64_REG(op1));

                prog->add2(OPCODE_2_MOVZX_REG_RM8);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op1), BC_TO_64_REG(op1));

                break;
            }
            case BC_ANDI: {
                // If this is the case then we can do some optimization
                // However, there is no need to handle this case and make it faster
                // if it never happens.
                Assert(op0 != op1);

                // Some instructions can be moved if you can be sure that
                // registers don't collide and doesn't need to be maintained before
                // and after this bytecode instruction.

                u8 lreg = BC_TO_64_REG(op0);
                u8 rreg = BC_TO_64_REG(op1);
                u8 zreg = REG_D;
                prog->add(OPCODE_PUSH_RM_SLASH_6);
                prog->addModRM(MODE_REG, 6, REG_D);

                prog->add(OPCODE_XOR_REG_RM);
                prog->addModRM(MODE_REG, REG_D, REG_D);

                if(op0 == op2){
                    Assert(lreg != REG_B && rreg != REG_B);
                    prog->add(OPCODE_PUSH_RM_SLASH_6);
                    prog->addModRM(MODE_REG, 6, REG_B);
                    prog->add(OPCODE_MOV_REG_RM);
                    prog->addModRM(MODE_REG, REG_B, lreg);
                    lreg = REG_B;
                }
                if(op1 == op2){
                    Assert(lreg != REG_B && rreg != REG_B);
                    prog->add(OPCODE_PUSH_RM_SLASH_6);
                    prog->addModRM(MODE_REG, 6, REG_B);
                    prog->add(OPCODE_MOV_REG_RM);
                    prog->addModRM(MODE_REG, REG_B, rreg);
                    rreg = REG_B;
                }

                prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                prog->addModRM(MODE_REG, 0, BC_TO_64_REG(op2));
                prog->add4((u32)1);

                prog->add(OPCODE_TEST_RM_REG);
                prog->addModRM(MODE_REG, lreg, lreg);

                prog->add2(OPCODE_2_CMOVZ_REG_RM);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op2), zreg);

                prog->add(OPCODE_TEST_RM_REG);
                prog->addModRM(MODE_REG, rreg, rreg);
                
                prog->add2(OPCODE_2_CMOVZ_REG_RM);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op2), zreg);
                
                if(op0 == op2){
                    // Assert(lreg != REG_B && rreg != REG_B);
                    lreg = REG_B;
                    prog->add(OPCODE_POP_RM_SLASH_0);
                    prog->addModRM(MODE_REG, 0, REG_B);
                }
                if(op1 == op2){
                    // Assert(lreg != REG_B && rreg != REG_B);
                    rreg = REG_B;
                    prog->add(OPCODE_POP_RM_SLASH_0);
                    prog->addModRM(MODE_REG, 0, REG_B);
                }
                
                prog->add(OPCODE_POP_RM_SLASH_0);
                prog->addModRM(MODE_REG, 0, REG_D);
                
                break;
            }
            #define CODE_COMPARE(OPCODE) \
                prog->add(OPCODE_CMP_REG_RM);\
                prog->addModRM(MODE_REG, BC_TO_64_REG(op0), BC_TO_64_REG(op1));\
                prog->add2(OPCODE);\
                prog->addModRM(MODE_REG, 0, BC_TO_64_REG(op2));\
                prog->add2(OPCODE_2_MOVZX_REG_RM8);\
                prog->addModRM(MODE_REG, BC_TO_64_REG(op2), BC_TO_64_REG(op2));

            case BC_LT: {
                CODE_COMPARE(OPCODE_2_SETL_RM8)
                break;
            }
            case BC_LTE: {
                CODE_COMPARE(OPCODE_2_SETLE_RM8)
                break;
            }
            case BC_GT: {
                CODE_COMPARE(OPCODE_2_SETG_RM8)
                break;
            }
            case BC_GTE: {
                CODE_COMPARE(OPCODE_2_SETGE_RM8)
                break;
            }
            case BC_ORI: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;

                prog->add(OPCODE_OR_RM_REG);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op01), BC_TO_64_REG(op2));
                break;
            }
            case BC_BNOT: {
                Assert(op0 == op1);

                prog->add(OPCODE_NOT_RM_SLASH_2);
                prog->addModRM(MODE_REG, 2, BC_TO_64_REG(op0));
                break;
            }
            case BC_BAND: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;

                prog->add(OPCODE_AND_RM_REG);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op01), BC_TO_64_REG(op2));
                break;
            }
            case BC_BOR: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;

                prog->add(OPCODE_OR_RM_REG);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op01), BC_TO_64_REG(op2));
                break;
            }
            case BC_BXOR: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;

                prog->add(OPCODE_XOR_REG_RM);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op2), BC_TO_64_REG(op01));
                break;
            }
            case BC_BLSHIFT: {
                Assert(op0 == op2 && DECODE_REG_TYPE(op1)==BC_CX);

                prog->add(OPCODE_SHL_RM_CL_SLASH_4);
                prog->addModRM(MODE_REG, 4, BC_TO_64_REG(op2));
                break;
            }
            case BC_BRSHIFT: {
                Assert(op0 == op2 && DECODE_REG_TYPE(op1)==BC_CX);

                prog->add(OPCODE_SHR_RM_CL_SLASH_5);
                prog->addModRM(MODE_REG, 5, BC_TO_64_REG(op2));
                break;
            }
            case BC_INCR: {
                i16 offset = (i16)((i16)op1| ((i16)op2<<8));

                prog->add(OPCODE_ADD_RM_IMM_SLASH_0);
                prog->addModRM(MODE_REG, 0, BC_TO_64_REG(op0));
                prog->add4((u32)(i32)offset); // NOTE: cast from i16 to i32 to u32, should be fine
                break;
            }
            case BC_LI: {
                prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                prog->addModRM(MODE_REG, 0, BC_TO_64_REG(op0));
                prog->add4((u32)imm); // NOTE: imm is i32 while add4 is u32, should be converted though
                break;
            }
            case BC_PUSH: {
                int size = DECODE_REG_SIZE(op0);
                if(size == 8){
                    prog->add(PREFIX_REXW);
                }
                prog->add(OPCODE_PUSH_RM_SLASH_6);
                prog->addModRM(MODE_REG, 6,BC_TO_64_REG(op0,4|8));
                break;
            }
            case BC_POP: {
                int size = DECODE_REG_SIZE(op0);
                if(size == 8){
                    prog->add(PREFIX_REXW);
                }
                prog->add(OPCODE_POP_RM_SLASH_0);
                prog->addModRM(MODE_REG, 0,BC_TO_64_REG(op0,4|8));
                break;
            }
            case BC_JMP: {
                //NOTE: The online disassembler I use does not disassemble relative jumps correctly.
                prog->add(OPCODE_JMP_IMM32);
                relativeRelocations.add({(i32)prog->size+4,(i32)prog->size, imm});
                prog->add4((u32)0);
                break;
            }
            case BC_JNE: {
                i16 offset = (i16)((i16)op1| ((i16)op2<<8));

                // TODO: Replace with a better instruction? If there is one.
                prog->add(OPCODE_CMP_RM_IMM8_SLASH_7);
                prog->addModRM(MODE_REG, 7, BC_TO_64_REG(op0));
                prog->add((u8)0);
                
                // I know, it's kind of funny that we use JE when the bytecode instruction is JNE
                prog->add2(OPCODE_2_JE_IMM32);
                relativeRelocations.add({(i32)prog->size+4,(i32)prog->size, imm,});
                prog->add4((u32)0);
                break;
            }
            case BC_CAST: {
                u8 type = op0;

                // if(type==CAST_FLOAT_SINT){
                //     int size = 1<<DECODE_REG_SIZE_TYPE(r1);
                //     if(size!=4){
                //         log::out << log::RED << "float needs 4 byte register\n";
                //     }
                //     int size2 = 1<<DECODE_REG_SIZE_TYPE(r2);
                //     // TODO: log out
                //     if(size2==1) {
                //         *(i8*)out = *(float*)xp;
                //     } else if(size2==2) {
                //         *(i16*)out = *(float*)xp;
                //     } else if(size2==4) {
                //         *(i32*)out = *(float*)xp;
                //     } else if(size2==8){
                //         *(i64*)out = *(float*)xp;
                //     }
                // } else if(type==CAST_SINT_FLOAT){
                //     int fsize = 1<<DECODE_REG_SIZE_TYPE(r1);
                //     int tsize = 1<<DECODE_REG_SIZE_TYPE(r2);
                //     if(tsize!=4){
                //         log::out << log::RED << "float needs 4 byte register\n";
                //     }
                //     // TODO: log out
                //     if(fsize==1) {
                //         *(float*)out = *(i8*)xp;
                //     } else if(fsize==2) {
                //         *(float*)out = *(i16*)xp;
                //     } else if(fsize==4) {
                //         *(float*)out = *(i32*)xp;
                //     } else if(fsize==8){
                //         *(float*)out = *(i64*)xp;
                //     }
                // }
                // If signed then sign extend
                // if not then don't
                // how to deal with size conversions?
                // how to deal with 16 bits in all instruction?

                
                int fsize = 1<<DECODE_REG_SIZE_TYPE(op1);
                int tsize = 1<<DECODE_REG_SIZE_TYPE(op2);
                u8 freg = BC_TO_64_REG(op1,4|8);
                u8 treg = BC_TO_64_REG(op2,4|8);

                prog->add(PREFIX_REXW);
                prog->add(OPCODE_XOR_REG_RM);
                prog->addModRM(MODE_REG,treg,treg);

                u8 minSize = fsize < tsize ? fsize : tsize;

                if(type==CAST_UINT_SINT){
                    if(minSize==1){
                        prog->add2(OPCODE_2_MOVZX_REG_RM8);
                        prog->addModRM(MODE_REG, treg, freg);
                    } else if(minSize == 2) {
                        prog->add2(OPCODE_2_MOVZX_REG_RM16);
                        prog->addModRM(MODE_REG, treg, freg);
                    } else if(minSize == 4) {
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM(MODE_REG, treg, freg);
                    } else if(minSize == 8) {
                        prog->add(PREFIX_REXW); // Does this work?
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM(MODE_REG, treg, freg);
                    }


                }


                //     // MOVZX can be used with unsigned 8/16 to unsigned/signed 16,32,64

                //     prog->add(OPCODE_XOR_REG_RM);
                //     prog->addModRM(MODE_REG, rreg, rreg);

                //     // prog->add(OPCODE_);

                //     prog->add(OPCODE_MOVf)
                //     u64 temp = 0;
                //     if(fsize==1) {
                //         temp = *(u8*)xp;
                //     } else if(fsize==2) {
                //         temp = *(u16*)xp;
                //     } else if(fsize==4) {
                //         temp = *(u32*)xp;
                //     } else if(fsize==8){
                //         temp = *(u64*)xp;
                //     }
                //     // TODO: log out
                //     if(tsize==1) {
                //         *(i8*)out = temp;
                //     } else if(tsize==2) {
                //         *(i16*)out = temp;
                //     } else if(tsize==4) {
                //         *(i32*)out = temp;
                //     } else if(tsize==8){
                //         *(i64*)out = temp;
                //     }
                // } else if(type==CAST_SINT_UINT){
                //     int fsize = 1<<DECODE_REG_SIZE_TYPE(r1);
                //     int tsize = 1<<DECODE_REG_SIZE_TYPE(r2);
                //     i64 temp = 0;
                //     if(fsize==1) {
                //         temp = *(i8*)xp;
                //     } else if(fsize==2) {
                //         temp = *(i16*)xp;
                //     } else if(fsize==4) {
                //         temp = *(i32*)xp;
                //     } else if(fsize==8){
                //         temp = *(i64*)xp;
                //     }
                //     // TODO: log out
                //     if(tsize==1) {
                //         *(u8*)out = temp;
                //     } else if(tsize==2) {
                //         *(u16*)out = temp;
                //     } else if(tsize==4) {
                //         *(u32*)out = temp;
                //     } else if(tsize==8){
                //         *(u64*)out = temp;
                //     }
                // } else if(type==CAST_SINT_SINT){
                //     int fsize = 1<<DECODE_REG_SIZE_TYPE(op1);
                //     int tsize = 1<<DECODE_REG_SIZE_TYPE(op2);
                //     i64 temp = 0;
                //     if(fsize==1) {
                //         temp = *(i8*)xp;
                //     } else if(fsize==2) {
                //         temp = *(i16*)xp;
                //     } else if(fsize==4) {
                //         temp = *(i32*)xp;
                //     } else if(fsize==8){
                //         temp = *(i64*)xp;
                //     }
                //     // TODO: log out
                //     if(tsize==1) {
                //         *(i8*)out = temp;
                //     } else if(tsize==2) {
                //         *(i16*)out = temp;
                //     } else if(tsize==4) {
                //         *(i32*)out = temp;
                //     } else if(tsize==8){
                //         *(i64*)out = temp;
                //     }
                // }

                break;
            }
            // case BC_ZERO_MEM: {

            //     break;
            // }
            // case BC_MEMCPY: {

            //     break;
            // }
        }

        if(prevSize == prog->size){
            log::out << log::RED << "Implement "<< log::PURPLE<< InstToString(inst.opcode)<< "\n";
            failure = true;
        }
    }

    for(int i=0;i<relativeRelocations.size();i++) {
        auto& rel = relativeRelocations[i];
        if(rel.bcAddress == bytecode->length())
            *(i32*)(prog->text + rel.immediateToModify) = prog->size - rel.currentIP;
        else {
            i32 value = addressTranslation[rel.bcAddress] - rel.currentIP;
            *(i32*)(prog->text + rel.immediateToModify) = value;
            // If you are wondering why the relocation is zero then it is because it
            // is relative and will jump to the next instruction. This isn't necessary so
            // they should be optimised away. Don't do this now since it makes the conversion
            // confusing.
        }
    }

    prog->add(OPCODE_RET);
    
    prog->printHex();

    if(failure){
        Program_x64::Destroy(prog);
        prog = nullptr;
    }
    return prog;
}

void Program_x64::Destroy(Program_x64* program) {
    using namespace engone;
    program->~Program_x64();
    Free(program,sizeof(Program_x64));
}