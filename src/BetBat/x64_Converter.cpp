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
    // memcpy(text + size, &byte, sizeof(u8));
    *((u8*)text + size) = byte;
    size++;
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
    // memcpy(text + size, &dword, sizeof(u32));
    size+=4;
}
void Program_x64::addModRM(u8 mod, u8 reg, u8 rm){
    add(rm | (reg << 3) | (mod << 6));
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

#define OPCODE_RET 0xC3

// RM_REG means: Add REG to RM, RM = RM + REG
#define OPCODE_ADD_RM_REG 0x01
// REG = REG + RM
#define OPCODE_ADD_REG_RM 0x03

#define OPCODE_MOV_RM_IMM 0xC7

// "FF /6" can be seen in x86 instruction sets.
// This means that the REG field in ModRM should be 6
// to use the push instruction
#define OPCODE_PUSH_RM_SLASH_6 0xFF
#define OPCODE_POP_RM_SLASH_0 0x8F

// the three other modes deal with memory
#define MODE_REG 0b11

#define REG_AX 0b000
#define REG_CX 0b001
#define REG_DX 0b010
#define REG_BX 0b011
#define REG_SP 0b100
#define REG_BP 0b101
#define REG_SI 0b110
#define REG_DI 0b111

u8 BC_TO_64_REG(u8 bcreg){
    u8 size = DECODE_REG_SIZE(bcreg);
    Assert(size==4);

    u8 id = (bcreg&(~BC_REG_MASK)) - 1;

    return id;
}

Program_x64* ConvertTox64(Bytecode* bytecode){
    using namespace engone;
    Assert(bytecode);

    Program_x64* prog = (Program_x64*)Allocate(sizeof(Program_x64));
    new(prog)Program_x64();

    for(int i=0;i<bytecode->length();i++){
        auto inst = bytecode->get(i);
        u8 opcode = inst.opcode;
        i32 imm = 0;
        log::out << inst;
        if(opcode == BC_LI || opcode==BC_JMP || opcode==BC_JE || opcode==BC_JNE ){
            i++;
            imm = bytecode->getIm(i);
            log::out << imm;
        }
        log::out << "\n";
        
        auto op0 = inst.op0;
        auto op1 = inst.op1;
        auto op2 = inst.op2;
        switch(opcode){
            case BC_ADDI: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;
                // add doesn't use three operands

                prog->add(OPCODE_ADD_RM_REG);
                prog->addModRM(MODE_REG, BC_TO_64_REG(op01), BC_TO_64_REG(op2));
                break;
            }
            case BC_LI: {
                prog->add(OPCODE_MOV_RM_IMM);
                prog->addModRM(MODE_REG, 0, BC_TO_64_REG(op0));
                prog->add4(imm); // NOTE: imm is i32 while add4 is u32, should be converted though
                break;
            }
            case BC_PUSH: {
                prog->add(OPCODE_PUSH_RM_SLASH_6);
                prog->addModRM(MODE_REG, 6,BC_TO_64_REG(op0));
                break;
            }
            case BC_POP: {
                prog->add(OPCODE_POP_RM_SLASH_0);
                prog->addModRM(MODE_REG, 0,BC_TO_64_REG(op0));
                break;
            }
        }
    }
    
    prog->add(OPCODE_RET);

    return prog;
}

void Program_x64::Destroy(Program_x64* program) {
    using namespace engone;
    program->~Program_x64();
    Free(program,sizeof(Program_x64));
}