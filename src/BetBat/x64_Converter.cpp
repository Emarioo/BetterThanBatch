#include "BetBat/x64_Converter.h"

#include "BetBat/ObjectWriter.h"

// SAFE_CONVERSIONS MUST BE DEFINED FOR THINGS TO WORK PROPERLY!
// If the output is bloated with instructions then you can temporarily turn this off.
#define SAFE_CONVERSIONS

const char* ToString(CallConventions stuff){
    #define CASE(X,N) case X: return N;
    switch(stuff){
        CASE(BETCALL,"betcall")
        CASE(STDCALL,"stdcall")
        CASE(INTRINSIC,"intrinsic")
        CASE(CDECL_CONVENTION,"cdecl")
    }
    return "<unknown-call>";
    #undef CASE
}
const char* ToString(LinkConventions stuff){
    #define CASE(X,N) case X: return N;
    switch(stuff){
        CASE(NONE,"none")
        CASE(NATIVE,"native")
        CASE(DLLIMPORT,"dllimport")
        CASE(IMPORT,"import")
    }
    return "<unknown-link>";
    #undef CASE
}
engone::Logger& operator<<(engone::Logger& logger, CallConventions convention){
    return logger << ToString(convention);
}
engone::Logger& operator<<(engone::Logger& logger, LinkConventions convention){
    return logger << ToString(convention);
}

StringBuilder& operator<<(StringBuilder& builder, CallConventions convention){
    return builder << ToString(convention);
}
StringBuilder& operator<<(StringBuilder& builder, LinkConventions convention){
    return builder << ToString(convention);
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
#define OPCODE_CALL_IMM (u8)0xE8
#define OPCODE_CALL_RM_SLASH_2 (u8)0xFF
#define OPCODE_NOP (u8)0x90

// RM_REG means: Add REG to RM, RM = RM + REG
// REG_RM: REG = REG + RM

// #define OPCODE_ADD_RM_REG (u8)0x01
#define OPCODE_ADD_REG_RM (u8)0x03
#define OPCODE_ADD_RM_IMM_SLASH_0 (u8)0x81
#define OPCODE_ADD_RM_IMM8_SLASH_0 (u8)0x83

#define OPCODE_SUB_REG_RM (u8)0x2B
#define OPCODE_SUB_RM_IMM_SLASH_5 (u8)0x81

#define OPCODE_MOV_RM_IMM32_SLASH_0 (u8)0xC7
#define OPCODE_MOV_RM_REG8 (u8)0x88
#define OPCODE_MOV_RM_REG (u8)0x89
#define OPCODE_MOV_REG_RM (u8)0x8B

#define OPCODE_MOV_REG_RM8 (u8)0x8A

#define OPCODE_LEA_REG_M (u8)0x8D

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
#define OPCODE_SHL_RM_IMM8_SLASH_4 (u8)0xC1

// logical and with flags being set, registers are not modified
#define OPCODE_TEST_RM_REG (u8)0x85

#define OPCODE_NOT_RM_SLASH_2 (u8)0xF7

#define OPCODE_2_SETE_RM8 (u16)0x940F
#define OPCODE_2_SETNE_RM8 (u16)0x950F

#define OPCODE_2_SETG_RM8 (u16)0x9F0F
#define OPCODE_2_SETGE_RM8 (u16)0x9D0F
#define OPCODE_2_SETL_RM8 (u16)0x9C0F
#define OPCODE_2_SETLE_RM8 (u16)0x9E0F

// zero extension
#define OPCODE_2_MOVZX_REG_RM8 (u16)0xB60F
#define OPCODE_2_MOVZX_REG_RM16 (u16)0xB70F

#define OPCODE_2_CMOVZ_REG_RM (u16)0x440F

#define OPCODE_AND_RM_IMM_SLASH_4 (u8)0x81
#define OPCODE_AND_RM_IMM8_SLASH_4 (u8)0x83

// sign extension
#define OPCODE_2_MOVSX_REG_RM8 (u16)0xBE0F
#define OPCODE_2_MOVSX_REG_RM16 (u16)0xBF0F
// intel manual encourages REX.W with MOVSXD
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
// for double
// #define OPCODE_MOVSD

#define OPCODE_3_ADDSS_REG_RM (u32)0x580FF3
#define OPCODE_3_SUBSS_REG_RM (u32)0x5C0FF3
#define OPCODE_3_MULSS_REG_RM (u32)0x590FF3
#define OPCODE_3_DIVSS_REG_RM (u32)0x5E0FF3

// convert i32 (or i64 with rexw) to float
#define OPCODE_3_CVTSI2SS_REG_RM (u32)0x2A0FF3
#define OPCODE_4_REXW_CVTSI2SS_REG_RM (u32)0x2A0F48F3
// convert float to i32 (or i64 with rexw)
#define OPCODE_3_CVTSS2SI_REG_RM (u32)0x2D0FF3
#define OPCODE_4_REXW_CVTSS2SI_REG_RM (u32)0x2D0F48F3

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

#define SIB_INDEX_NONE 0b100

#define SIB_SCALE_1 0b00
#define SIB_SCALE_2 0b01
#define SIB_SCALE_4 0b10
#define SIB_SCALE_8 0b11

#define PREFIX_REXB (u8)0b01000001
#define PREFIX_REXX (u8)0b01000010
#define PREFIX_REXR (u8)0b01000100
#define PREFIX_REXW (u8)0b01001000
#define PREFIX_16BIT (u8)0x66
#define PREFIX_LOCK (u8)0xF0

#define REG_A 0b000
#define REG_C 0b001
#define REG_D 0b010
#define REG_B 0b011
#define REG_SP 0b100
#define REG_BP 0b101
#define REG_SI 0b110
#define REG_DI 0b111

#define REG_XMM0 0b000
#define REG_XMM1 0b001
#define REG_XMM2 0b010
#define REG_XMM3 0b011
#define REG_XMM4 0b100
#define REG_XMM5 0b101
#define REG_XMM6 0b110
#define REG_XMM7 0b111


bool Program_x64::_reserve(u32 newAllocationSize){
    if(newAllocationSize==0){
        if(_allocationSize!=0){
            engone::Free(text, _allocationSize);
        }
        text = nullptr;
        _allocationSize = 0;
        head = 0;
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
        if(head > newAllocationSize){
            head = newAllocationSize;
        }
        return true;
    }
    return false;
}

void Program_x64::add(u8 byte){
    if(head+1 >= _allocationSize ){
        Assert(_reserve(_allocationSize*2 + 100));
    }
    *(text + head) = byte;
    head++;
}
void Program_x64::add2(u16 word){
    if(head+2 >= _allocationSize ){
        Assert(_reserve(_allocationSize*2 + 100));
    }
    
    auto ptr = (u8*)&word;
    *(text + head + 0) = *(ptr + 0);
    *(text + head + 1) = *(ptr + 1);
    head+=2;
}void Program_x64::add3(u32 word){
    if(head>0){
        // This is not how you use rex prefix
        Assert(*(text + head - 1) != PREFIX_REXW);
    }
    Assert(0==(word&0xFF000000));
    if(head+3 >= _allocationSize ){
        Assert(_reserve(_allocationSize*3 + 100));
    }
    
    auto ptr = (u8*)&word;
    *(text + head + 0) = *(ptr + 0);
    *(text + head + 1) = *(ptr + 1);
    *(text + head + 2) = *(ptr + 2);
    head+=3;
}
void Program_x64::add4(u32 dword){
    if(head+4 >= _allocationSize ){
        Assert(_reserve(_allocationSize*2 + 100));
    }
    auto ptr = (u8*)&dword;

    // deals with non-alignment
    *(text + head + 0) = *(ptr + 0);
    *(text + head + 1) = *(ptr + 1);
    *(text + head + 2) = *(ptr + 2);
    *(text + head + 3) = *(ptr + 3);
    head+=4;
}
void Program_x64::addRaw(u8* arr, u64 len){
    if(head+len >= _allocationSize ){
        Assert(_reserve(_allocationSize*1.2 + 10 + len));
    }
    memcpy(text + head, arr, len);
    head += len;
}
void Program_x64::addModRM(u8 mod, u8 reg, u8 rm){
    Assert((mod&~3) == 0 && (reg&~7)==0 && (rm&~7)==0);
    // You probably made a mistake and used REG_BP thinking it works with just ModRM byte.
    Assert(("Use addModRM_SIB instead",!(mod!=0b11 && rm==0b100)));
    // REG_SP isn't available with mod=0b00, see intel x64 manual about 32 bit addressing for more details
    Assert(("Use addModRM_disp32 instead",!(mod==0b00 && rm==0b101)));
    // Assert(("Use addModRM_disp32 instead",(mod!=0b10)));
    add((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
}
void Program_x64::addModRM_rip(u8 reg, u32 disp32){
    u8 mod = 0b00;
    u8 rm = 0b101;
    Assert((mod&~3) == 0 && (reg&~7)==0 && (rm&~7)==0);
    add((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
    add4(disp32);
}
void Program_x64::addModRM_SIB(u8 mod, u8 reg, u8 scale, u8 index, u8 base){
    //  register to register (mod = 0b11) doesn't work with SIB byte
    Assert(("Use addModRM instead",mod!=0b11));

    Assert(("Ignored meaning in SIB byte. Look at intel x64 manual and fix it.",base != 0b101));

    u8 rm = 0b100;
    Assert((mod&~3) == 0 && (reg&~7)==0 && (rm&~7)==0);
    Assert((scale&~3) == 0 && (index&~7)==0 && (base&~7)==0);

    add((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
    add((u8)(base | (index << (u8)3) | (scale << (u8)6)));
}

void Program_x64::printHex(const char* path){
    using namespace engone;
    Assert(this);
    #define HEXIFY(X) (char)(X<10 ? '0'+X : 'A'+X - 10)
    if(path) {
        APIFile file = nullptr;
        Assert(file = FileOpen(path,0,FILE_WILL_CREATE));

        const int stride = 12;
        char* buffer = (char*)Allocate(size()*3);
        for(int i = 0;i<head;i++){
            u8 a = text[i]>>4;
            u8 b = text[i]&0xF;
            buffer[i*3] = HEXIFY(a);
            buffer[i*3+1] = HEXIFY(b);
            if(i%stride == stride-1)
                buffer[i*3+2] = '\n';
            else
                buffer[i*3+2] = ' ';
        }

        FileWrite(file, buffer, size()*3);

        Free(buffer,size()*3);
        FileClose(file);
    } else {
        log::out << log::LIME << "HEX:\n";
        for(int i = 0;i<head;i++){
            u8 a = text[i]>>4;
            u8 b = text[i]&0xF;
            log::out << HEXIFY(a) << HEXIFY(b) <<" ";
        }
        log::out << "\n";
    }
}

u8 BCToProgramReg(u8 bcreg, int handlingSizes = 4, bool allowXMM = false){
    u8 size = DECODE_REG_SIZE(bcreg);
    Assert(size&handlingSizes);
    if(bcreg == BC_REG_SP){
        return REG_SP;
    }
    if(bcreg == BC_REG_FP){
        return REG_BP;
    }

    if(IS_REG_XMM(bcreg)) {
        Assert(allowXMM);
        Assert(BC_REG_XMM3 - BC_REG_XMM0 == 3);
        return DECODE_REG_TYPE(bcreg) - BC_XMM0;
    }

    u8 id = (bcreg&(~BC_REG_MASK)) - 1;
    return id;
}

Program_x64* ConvertTox64(Bytecode* bytecode){
    using namespace engone;
    Assert(bytecode);

    _VLOG(log::out <<log::BLUE<< "x64 Converter:\n";)

    Program_x64* prog = Program_x64::Create();

    // prog->add(PREFIX_REXW);
    // prog->add(OPCODE_MOV_REG_RM);
    // prog->addModRM_SIB(MODE_DEREF, REG_SI, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);

    // prog->add((u8)(PREFIX_REXW));
    // prog->add(OPCODE_MOV_REG_RM);
    // prog->addModRM_SIB(MODE_DEREF_DISP8, REG_B, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
    // prog->add((u8)8);
    // prog->printHex();
    // return prog;

    // TODO: You may want some functions to handle the allocation here.
    //  like how you can reserve memory for the text allocation.
    if(bytecode->dataSegment.used!=0){
        prog->globalData = (u8*)engone::Allocate(bytecode->dataSegment.used);
        Assert(prog->globalData);
        prog->globalSize = bytecode->dataSegment.used;
        memcpy(prog->globalData, bytecode->dataSegment.data, prog->globalSize);
    }
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
        // RelativeRelocation(i32 ip, i32 x64, i32 bc) : currentIP(ip), immediateToModify(x64), bcAddress(bc) {}
        i32 currentIP=0; // It's more of a nextIP rarther than current. Needed when calculating relative offset
        // Address MUST point to an immediate (or displacement?) with 4 bytes.
        // You can add some flags to this struct for more variation.
        i32 immediateToModify=0; // dword/i32 to modify with correct value
        i32 bcAddress=0; // address in bytecode, needed when looking up absolute offset in addressTranslation
    };
    DynamicArray<RelativeRelocation> relativeRelocations;
    relativeRelocations._reserve(40);

    prog->add(OPCODE_PUSH_RM_SLASH_6);
    prog->addModRM(MODE_REG,6,REG_BP);

    prog->add(PREFIX_REXW);
    prog->add(OPCODE_MOV_REG_RM);
    prog->addModRM(MODE_REG, REG_BP, REG_SP);

    // for(int i=0;i<0;i++){
    for(int bcIndex=0;bcIndex<bytecode->length();bcIndex++){
        auto inst = bytecode->get(bcIndex);
        addressTranslation[bcIndex] = prog->size();
        u8 opcode = inst.opcode;
        i32 imm = 0;
        
        _CLOG(log::out << bcIndex << ": "<< inst;)
        if(opcode == BC_LI || opcode==BC_JMP || opcode==BC_JE || opcode==BC_JNE || opcode==BC_CALL || opcode==BC_DATAPTR|
            opcode == BC_MOV_MR_DISP32 || opcode == BC_MOV_RM_DISP32 || opcode == BC_CODEPTR){
            bcIndex++;
            imm = bytecode->getIm(bcIndex);
            addressTranslation[bcIndex] = prog->size();
            _CLOG(log::out << " "<<imm;)
        }
        _CLOG(log::out << "\n";)
        
        u32 prevSize = prog->size();

        auto op0 = inst.op0;
        auto op1 = inst.op1;
        auto op2 = inst.op2;
        switch(opcode){
            case BC_MOV_RR: {
                u8 size = DECODE_REG_SIZE(op1);
                if(size==8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_MOV_REG_RM);
                prog->addModRM(MODE_REG, BCToProgramReg(op1,4|8), BCToProgramReg(op0,4|8));
                break;
            }
            break; case BC_MOV_RM:
            case BC_MOV_RM_DISP32: {
                // IMPORTANT: Be careful when changing stuff here. You might think you know
                //  but you really don't. Keep track of which operand is destination and output
                //  and which one is register and register/memory
                
                Assert(DECODE_REG_TYPE(op0)!=DECODE_REG_TYPE(op1));
                // NOTE: You can't XOR op1 here you dofus
                // prog->add(PREFIX_REXW);
                // prog->add(OPCODE_XOR_REG_RM);
                // prog->addModRM(MODE_REG,DECODE_REG_TYPE(op1),DECODE_REG_TYPE(op1));
                // IMPORTANT: Mov instructions and operands are not used properly.
                //   operand 0b101 is RIP addressing which isn't expected.
                u8 mode = MODE_DEREF;
                if(imm!=0){
                    mode = MODE_DEREF_DISP32;
                }
                i8 size = (i8)op2;
                Assert(size==1||size==2||size==4||size==8);
                switch(size) {
                    case 1: {
                        prog->add(OPCODE_MOV_RM_REG8);
                        prog->addModRM(mode, BCToProgramReg(op0,size), BCToProgramReg(op1,8));
                        break;
                    }
                    break; case 2: {
                        prog->add(PREFIX_16BIT);
                        prog->add(OPCODE_MOV_RM_REG);
                        prog->addModRM(mode, BCToProgramReg(op0,size), BCToProgramReg(op1,8));
                        break;
                    }
                    break; case 4: {
                        prog->add(OPCODE_MOV_RM_REG);
                        prog->addModRM(mode, BCToProgramReg(op0,size), BCToProgramReg(op1,8));
                        break;
                    }
                    break; case 8: {
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_MOV_RM_REG);
                        prog->addModRM(mode, BCToProgramReg(op0,size), BCToProgramReg(op1,8));
                        break;
                    }
                    default: {
                        Assert(false);
                    }
                }
                if(imm != 0){
                    prog->add4((u32)imm);
                }
                break;
            }
            break; case BC_MOV_MR:
            case BC_MOV_MR_DISP32: {
                // IMPORTANT: Be careful when changing stuff here. You might think you know
                //  but you really don't. Keep track of which operand is destination and output
                //  and which one is register and register/memory
                #ifdef SAFE_CONVERSIONS
                if(DECODE_REG_TYPE(op0)!=DECODE_REG_TYPE(op1)) {
                    prog->add(PREFIX_REXW);
                    prog->add(OPCODE_XOR_REG_RM);
                    prog->addModRM(MODE_REG,BCToProgramReg(op1,0xF),BCToProgramReg(op1,0xF));
                }
                #endif
                Assert(DECODE_REG_TYPE(op0)!=DECODE_REG_TYPE(op1));
                u8 mode = MODE_DEREF;
                if(imm!=0){
                    mode = MODE_DEREF_DISP32;
                }
                i8 size = (i8)op2;
                Assert(size==1||size==2||size==4||size==8);
                switch(size) {
                    case 1: {
                        prog->add(OPCODE_MOV_REG_RM8);
                        prog->addModRM(mode, BCToProgramReg(op1,1), BCToProgramReg(op0,8));
                        break;
                    }
                    break; case 2: {
                        prog->add(PREFIX_16BIT);
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM(mode, BCToProgramReg(op1,2), BCToProgramReg(op0,8));
                        break;
                    }
                    break; case 4: {
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM(mode, BCToProgramReg(op1,4), BCToProgramReg(op0,8));
                        break;
                    }
                    break; case 8: {
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM(mode, BCToProgramReg(op1,8), BCToProgramReg(op0,8));
                        break;
                    }
                    default: {
                        Assert(false);
                    }
                }
                if(imm!=0){
                    prog->add4((u32)imm);
                }   
                break;
            }
            break; case BC_ADDI: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;
                // add doesn't use three operands
                u8 size = DECODE_REG_SIZE(op0);
                if(size==8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_ADD_REG_RM);
                prog->addModRM(MODE_REG, BCToProgramReg(op2,0xF), BCToProgramReg(op01,0xF));
                break;
            }
            break; case BC_INCR: {
                i16 offset = (i16)((i16)op1| ((i16)op2<<8));
                Assert(offset!=0); // the byte code generator shouldn't produce this instruction
                u8 size = DECODE_REG_SIZE(op0);
                if(size == 8){
                    prog->add(PREFIX_REXW);
                }
                if(offset>0) {
                    prog->add(OPCODE_ADD_RM_IMM_SLASH_0);
                    prog->addModRM(MODE_REG, 0, BCToProgramReg(op0,4|8));
                    prog->add4((u32)(i32)offset); // NOTE: cast from i16 to i32 to u32, should be fine
                } else {
                    prog->add(OPCODE_SUB_RM_IMM_SLASH_5);
                    prog->addModRM(MODE_REG, 5, BCToProgramReg(op0,4|8));
                    prog->add4((u32)(i32)-offset); // NOTE: cast from i16 to i32 to u32, should be fine
                }
                break;
            }
            break; case BC_SUBI: {
                Assert(op0 == op2);
                // Assert(op0 == op2 || op1 == op2);
                // u8 op01 = op0 == op2 ? op1 : op0;
                u8 size = DECODE_REG_SIZE(op0);
                if(size==8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_SUB_REG_RM);
                prog->addModRM(MODE_REG, BCToProgramReg(op2,0xF), BCToProgramReg(op1,0xF));
                break;
            }
            break; case BC_MULI: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;
                u8 size = DECODE_REG_SIZE(op0) > DECODE_REG_SIZE(op1) ? DECODE_REG_SIZE(op0) : DECODE_REG_SIZE(op1);
                // TODO: What about overflow? Should we always do signed multiplication?
                //   Overflow is truncated and no other registers are messed.
                if(size == 8)
                    prog->add(PREFIX_REXW);
                prog->add2(OPCODE_2_IMUL_REG_RM);
                prog->addModRM(MODE_REG, BCToProgramReg(op2,0xC), BCToProgramReg(op01,0xC));

                break;
            }
            break; case BC_DIVI: {
                Assert(DECODE_REG_TYPE(op0) == BC_AX &&
                    DECODE_REG_TYPE(op1) != BC_AX &&
                    DECODE_REG_TYPE(op1) != BC_DX);

                u8 size = DECODE_REG_SIZE(op2);
                
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
                    if(size == 8)
                        prog->add(PREFIX_REXW);
                    prog->add(OPCODE_MOV_REG_RM);
                    prog->addModRM(MODE_REG, REG_A, BCToProgramReg(op0));
                }
                if(size == 8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_CDQ); // sign extend eax into edx
                if(size == 8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_IDIV_RM_SLASH_7);
                prog->addModRM(MODE_REG, 7, BCToProgramReg(op1));

                if(DECODE_REG_TYPE(op2) != BC_DX){
                    prog->add(OPCODE_POP_RM_SLASH_0);
                    prog->addModRM(MODE_REG, 0, REG_D);
                }
                if(DECODE_REG_TYPE(op2) != BC_AX){
                    if(size == 8)
                        prog->add(PREFIX_REXW);
                    prog->add(OPCODE_MOV_REG_RM);
                    prog->addModRM(MODE_REG, BCToProgramReg(op2), REG_A);
                    prog->add(OPCODE_POP_RM_SLASH_0);
                    prog->addModRM(MODE_REG, 0, REG_A);
                }

                break;
            }
            break; case BC_MODI: {
                Assert(DECODE_REG_TYPE(op0) == BC_AX &&
                    DECODE_REG_TYPE(op1) != BC_AX &&
                    DECODE_REG_TYPE(op1) != BC_DX);

                u8 size = DECODE_REG_SIZE(op2);
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
                    if(size == 8)
                        prog->add(PREFIX_REXW);
                    prog->add(OPCODE_MOV_REG_RM);
                    prog->addModRM(MODE_REG, REG_A, BCToProgramReg(op0));
                }
                if(size == 8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_CDQ); // sign extend eax into edx

                if(size == 8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_IDIV_RM_SLASH_7);
                prog->addModRM(MODE_REG, 7, BCToProgramReg(op1));

                if(DECODE_REG_TYPE(op2) != BC_DX){
                    if(size == 8)
                        prog->add(PREFIX_REXW);
                    prog->add(OPCODE_MOV_REG_RM);
                    prog->addModRM(MODE_REG, BCToProgramReg(op2), REG_D);
                    prog->add(OPCODE_POP_RM_SLASH_0);
                    prog->addModRM(MODE_REG, 0, REG_D);
                }
                if(DECODE_REG_TYPE(op2) != BC_AX){
                    prog->add(OPCODE_POP_RM_SLASH_0);
                    prog->addModRM(MODE_REG, 0, REG_A);
                }

                break;
            }
            break; case BC_SUBF:
            case BC_MULF:
            case BC_DIVF:
            case BC_ADDF: {
                prog->add(OPCODE_MOV_RM_REG);
                prog->addModRM_SIB(MODE_DEREF_DISP8, BCToProgramReg(op0), SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                prog->add((u8)-8);

                prog->add3(OPCODE_3_MOVSS_REG_RM);
                prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                prog->add((u8)-8);

                prog->add(OPCODE_MOV_RM_REG);
                prog->addModRM_SIB(MODE_DEREF_DISP8, BCToProgramReg(op1), SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                prog->add((u8)-8);

                u32 operation = 0;
                switch(opcode) {
                    case BC_ADDF: operation = OPCODE_3_ADDSS_REG_RM; break;
                    case BC_SUBF: operation = OPCODE_3_SUBSS_REG_RM; break;
                    case BC_MULF: operation = OPCODE_3_MULSS_REG_RM; break;
                    case BC_DIVF: operation = OPCODE_3_DIVSS_REG_RM; break;
                }
                prog->add3(operation);
                prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                prog->add((u8)-8);

                prog->add3(OPCODE_3_MOVSS_RM_REG);
                prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                prog->add((u8)-8);

                prog->add(OPCODE_MOV_REG_RM);
                prog->addModRM_SIB(MODE_DEREF_DISP8, BCToProgramReg(op2), SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                prog->add((u8)-8);
            }
            // break; case BC_SUBF: {
            //     prog->add(OPCODE_MOV_RM_REG);
            //     prog->addModRM_SIB(MODE_DEREF_DISP8, BCToProgramReg(op0), SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
            //     prog->add((u8)-8);

            //     prog->add3(OPCODE_3_MOVSS_REG_RM);
            //     prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
            //     prog->add((u8)-8);

            //     prog->add(OPCODE_MOV_RM_REG);
            //     prog->addModRM_SIB(MODE_DEREF_DISP8, BCToProgramReg(op1), SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
            //     prog->add((u8)-8);

            //     prog->add3(OPCODE_3_ADDSS_REG_RM);
            //     prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
            //     prog->add((u8)-8);

            //     prog->add3(OPCODE_3_MOVSS_RM_REG);
            //     prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
            //     prog->add((u8)-8);

            //     prog->add(OPCODE_MOV_REG_RM);
            //     prog->addModRM_SIB(MODE_DEREF_DISP8, BCToProgramReg(op2), SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
            //     prog->add((u8)-8);
            // }
            break; case BC_EQ: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;
                u8 size = DECODE_REG_SIZE(op2);
                // TODO: Test if this is okay?
                if(size == 8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_XOR_REG_RM);
                prog->addModRM(MODE_REG, BCToProgramReg(op2,0xF), BCToProgramReg(op01,0xF));
                
                prog->add2(OPCODE_2_SETE_RM8);
                prog->addModRM(MODE_REG, 0, BCToProgramReg(op2,0xF));

                prog->add(PREFIX_REXW);
                prog->add2(OPCODE_2_MOVZX_REG_RM8);
                prog->addModRM(MODE_REG, BCToProgramReg(op2,0xF), BCToProgramReg(op2,0xF));

                break;
            }
            break; case BC_NEQ: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;

                u8 size = DECODE_REG_SIZE(op2);
                if(size == 8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_XOR_REG_RM);
                prog->addModRM(MODE_REG, BCToProgramReg(op2,0xC), BCToProgramReg(op01,0xC));
                break;
            }
            break; case BC_NOT: {
                Assert(op0 == op1);

                // Assert(op0 != op1);
                // prog->add(OPCODE_XOR_REG_RM);
                // prog->addModRM(MODE_REG, BCToProgramReg(op1), BCToProgramReg(op1));
                    
                u8 size = DECODE_REG_SIZE(op2);
                if(size == 8)
                    prog->add(PREFIX_REXW);

                prog->add(OPCODE_TEST_RM_REG);
                prog->addModRM(MODE_REG, BCToProgramReg(op0,0xF), BCToProgramReg(op0,0xF));

                prog->add2(OPCODE_2_SETE_RM8);
                prog->addModRM(MODE_REG, 0, BCToProgramReg(op1,0xF));

                prog->add(PREFIX_REXW);
                prog->add2(OPCODE_2_MOVZX_REG_RM8);
                prog->addModRM(MODE_REG, BCToProgramReg(op1,0xF), BCToProgramReg(op1,0xF));

                break;
            }
            break; case BC_ANDI: {
                // If this is the case then we can do some optimization
                // However, there is no need to handle this case and make it faster
                // if it never happens.
                Assert(op0 != op1);

                // Some instructions can be moved if you can be sure that
                // registers don't collide and doesn't need to be maintained before
                // and after this bytecode instruction.

                // TODO: This code shouldn't work at all and needs to be fixed.
                //  Differently sized operands aren't handled properly.
                //  Doing AND on rcx and eax will give unexpected result if upper rax
                //  has non-zero bits.

                int temp = 0xC;

                u8 lreg = BCToProgramReg(op0,temp);
                u8 rreg = BCToProgramReg(op1,temp);
                u8 zreg = REG_D;
                prog->add(OPCODE_PUSH_RM_SLASH_6);
                prog->addModRM(MODE_REG, 6, REG_D);

                prog->add(PREFIX_REXW);
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
                prog->addModRM(MODE_REG, 0, BCToProgramReg(op2,temp));
                prog->add4((u32)1);

                prog->add(OPCODE_TEST_RM_REG);
                prog->addModRM(MODE_REG, lreg, lreg);

                prog->add2(OPCODE_2_CMOVZ_REG_RM);
                prog->addModRM(MODE_REG, BCToProgramReg(op2,temp), zreg);

                prog->add(OPCODE_TEST_RM_REG);
                prog->addModRM(MODE_REG, rreg, rreg);
                
                prog->add2(OPCODE_2_CMOVZ_REG_RM);
                prog->addModRM(MODE_REG, BCToProgramReg(op2,temp), zreg);
                
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
                u8 size = DECODE_REG_SIZE(op0);\
                if(size<DECODE_REG_SIZE(op1)) size = DECODE_REG_SIZE(op1);\
                if(size == 8)  prog->add(PREFIX_REXW);\
                prog->add(OPCODE_CMP_REG_RM);\
                prog->addModRM(MODE_REG, BCToProgramReg(op0,0xC), BCToProgramReg(op1,0xC));\
                if(size == 8)  prog->add(PREFIX_REXW);\
                prog->add2(OPCODE);\
                prog->addModRM(MODE_REG, 0, BCToProgramReg(op2,0xC));\
                if(size == 8)  prog->add(PREFIX_REXW);\
                prog->add2(OPCODE_2_MOVZX_REG_RM8);\
                prog->addModRM(MODE_REG, BCToProgramReg(op2,0xC), BCToProgramReg(op2,0xC));

            break; case BC_LT: {
                CODE_COMPARE(OPCODE_2_SETL_RM8)
                break;
            }
            break; case BC_LTE: {
                CODE_COMPARE(OPCODE_2_SETLE_RM8)
                break;
            }
            break; case BC_GT: {
                CODE_COMPARE(OPCODE_2_SETG_RM8)
                break;
            }
            break; case BC_GTE: {
                CODE_COMPARE(OPCODE_2_SETGE_RM8)
                break;
            }
            break; case BC_ORI: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;
                u8 size = DECODE_REG_SIZE(op2);
                if(size==8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_OR_RM_REG);
                prog->addModRM(MODE_REG, BCToProgramReg(op01,0xF), BCToProgramReg(op2,0xF));
                break;
            }
            break; case BC_BNOT: {
                Assert(op0 == op1);

                u8 size = DECODE_REG_SIZE(op2);
                if(size==8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_NOT_RM_SLASH_2);
                prog->addModRM(MODE_REG, 2, BCToProgramReg(op0,0xF));
                break;
            }
            break; case BC_BAND: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;
                u8 size = DECODE_REG_SIZE(op2);
                if(size == 8)
                    prog->add(PREFIX_REXW);

                prog->add(OPCODE_AND_RM_REG);
                prog->addModRM(MODE_REG, BCToProgramReg(op01,0xF), BCToProgramReg(op2,0xF));
                break;
            }
            break; case BC_BOR: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;

                u8 size = DECODE_REG_SIZE(op2);
                if(size==8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_OR_RM_REG);
                prog->addModRM(MODE_REG, BCToProgramReg(op01,0xF), BCToProgramReg(op2,0xF));
                break;
            }
            break; case BC_BXOR: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;
                u8 size = DECODE_REG_SIZE(op2);
                if(size==8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_XOR_REG_RM);
                prog->addModRM(MODE_REG, BCToProgramReg(op2,0xF), BCToProgramReg(op01,0xF));
                break;
            }
            break; case BC_BLSHIFT: {
                Assert(op0 == op2 && DECODE_REG_TYPE(op1)==BC_CX);
                u8 size = DECODE_REG_SIZE(op2);
                if(size == 8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_SHL_RM_CL_SLASH_4);
                prog->addModRM(MODE_REG, 4, BCToProgramReg(op2,0xF));
                break;
            }
            break; case BC_BRSHIFT: {
                Assert(op0 == op2 && DECODE_REG_TYPE(op1)==BC_CX);
                u8 size = DECODE_REG_SIZE(op2);
                if(size == 8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_SHR_RM_CL_SLASH_5);
                prog->addModRM(MODE_REG, 5, BCToProgramReg(op2,0xF));
                break;
            }
            break; case BC_LI: {
                if(imm == 0){
                    prog->add(PREFIX_REXW);
                    prog->add(OPCODE_XOR_REG_RM);
                    prog->addModRM(MODE_REG, BCToProgramReg(op0,0xF),BCToProgramReg(op0,0xF));
                }else{
                    u8 size = DECODE_REG_SIZE(op0);
                    if(size != 8){
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_XOR_REG_RM);
                        prog->addModRM(MODE_REG, BCToProgramReg(op0,0xF),BCToProgramReg(op0,0xF));
                    }
                    if(size==8)
                        prog->add(PREFIX_REXW);
                    prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                    prog->addModRM(MODE_REG, 0, BCToProgramReg(op0,0xF));
                    prog->add4((u32)imm); // NOTE: imm is i32 while add4 is u32, should be converted though
                }
                break;
            }
            break; case BC_DATAPTR: {
                u8 size = DECODE_REG_SIZE(op0);
                Assert(size==8);

                prog->add(PREFIX_REXW);
                prog->add(OPCODE_LEA_REG_M);
                prog->addModRM_rip(BCToProgramReg(op0,8), (u32)0);
                DataRelocation reloc{};
                reloc.dataOffset = imm;
                reloc.textOffset = prog->size() - 4;
                prog->dataRelocations.add(reloc);
                break;
            }
            break; case BC_CODEPTR: {
                u8 size = DECODE_REG_SIZE(op0);
                Assert(size==8);

                prog->add(PREFIX_REXW);
                prog->add(OPCODE_LEA_REG_M);
                prog->addModRM_rip(BCToProgramReg(op0,8), (u32)0);
                RelativeRelocation reloc{};
                reloc.immediateToModify = prog->size()-4;
                reloc.currentIP = prog->size();
                reloc.bcAddress = imm;
                relativeRelocations.add(reloc);
                // NamedRelocation reloc{};
                // reloc.dataOffset = imm;
                // reloc.
                // reloc.textOffset = prog->size() - 4;
                // prog->dataRelocations.add(reloc);
                // prog->namedRelocations.add();
                break;
            }
            break; case BC_PUSH: {
                if(IS_REG_XMM(op0)) {
                    u8 reg = BCToProgramReg(op0, 4|8, true);
                    
                    prog->add(PREFIX_REXW);
                    prog->add(OPCODE_ADD_RM_IMM8_SLASH_0);
                    prog->addModRM(MODE_REG, 0, REG_SP);
                    prog->add((u8)-8);

                    prog->add3(OPCODE_3_MOVSS_RM_REG);
                    prog->addModRM_SIB(MODE_DEREF, reg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                } else {
                    int size = DECODE_REG_SIZE(op0);
                    #ifdef SAFE_CONVERSIONS
                    if(size == 4){
                        // prog->add(PREFIX_REXW); // rexw will sign extend the immediate
                        prog->add(OPCODE_AND_RM_IMM_SLASH_4);
                        prog->addModRM(MODE_REG, 4, BCToProgramReg(op0,0xF));
                        prog->add4((u32)0xFFFFFFFF);
                    } else if(size == 2) {
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_AND_RM_IMM_SLASH_4);
                        prog->addModRM(MODE_REG, 4, BCToProgramReg(op0,0xF));
                        prog->add4((u32)0xFFFF);
                    }else if(size == 1) {
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_AND_RM_IMM_SLASH_4);
                        prog->addModRM(MODE_REG, 4, BCToProgramReg(op0,0xF));
                        prog->add4((u32)0xFF);
                    }
                    #endif

                    prog->add(OPCODE_PUSH_RM_SLASH_6);
                    prog->addModRM(MODE_REG, 6,BCToProgramReg(op0,0xF)); // 0xF = 1|2|4|8, meaning all sizes
                }
                break;
            }
            break; case BC_POP: {
                if(IS_REG_XMM(op0)) {
                    u8 reg = BCToProgramReg(op0, 4|8, true);

                    prog->add3(OPCODE_3_MOVSS_REG_RM);
                    prog->addModRM_SIB(MODE_DEREF, reg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);

                    prog->add(PREFIX_REXW);
                    prog->add(OPCODE_ADD_RM_IMM8_SLASH_0);
                    prog->addModRM(MODE_REG, 0, REG_SP);
                    prog->add((u8)8);
                } else {
                    u8 size = DECODE_REG_SIZE(op0);
                    u8 reg = BCToProgramReg(op0,0xF);
                    if(op0 == BC_REG_R8 || op0 == BC_REG_R9) {
                        prog->add(PREFIX_REXB);
                        reg = reg - 8;
                    }
                    #ifdef SAFE_CONVERSIONS
                    if(size != 8){
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_XOR_REG_RM);
                        prog->addModRM(MODE_REG, reg,reg);
                    }
                    #endif
                    prog->add(OPCODE_POP_RM_SLASH_0);
                    prog->addModRM(MODE_REG, 0, reg);
                }
                break;
            }
            break; case BC_JMP: {
                //NOTE: The online disassembler I use does not disassemble relative jumps correctly.
                prog->add(OPCODE_JMP_IMM32);
                prog->add4((u32)0);
                RelativeRelocation reloc{};
                reloc.currentIP = prog->size();
                reloc.bcAddress = imm;
                reloc.immediateToModify = prog->size()-4;
                relativeRelocations.add(reloc);
                break;
            }
            break; case BC_JNE: {
                i16 offset = (i16)((i16)op1| ((i16)op2<<8));

                // TODO: Replace with a better instruction? If there is one.
                u8 size = DECODE_REG_SIZE(op0);

                if(size==8);
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_CMP_RM_IMM8_SLASH_7);
                prog->addModRM(MODE_REG, 7, BCToProgramReg(op0,0xF));
                prog->add((u8)0);
                
                // I know, it's kind of funny that we use JE when the bytecode instruction is JNE
                prog->add2(OPCODE_2_JE_IMM32);
                prog->add4((u32)0);
                
                RelativeRelocation reloc{};
                reloc.currentIP = prog->size();
                reloc.bcAddress = imm;
                reloc.immediateToModify = prog->size()-4;
                relativeRelocations.add(reloc);
                break;
            }
            break; case BC_CAST: {
                u8 type = op0;

                int fsize = 1<<DECODE_REG_SIZE_TYPE(op1);
                int tsize = 1<<DECODE_REG_SIZE_TYPE(op2);
                u8 freg = BCToProgramReg(op1,0xF);
                u8 treg = BCToProgramReg(op2,0xF);
                Assert(freg == treg);

                // if(fsize == tsize){
                //     break; // nothing to change
                // }
                // TODO: Test some casting with u8,i16,f32,f64,u64 and stuff.

                // prog->add(PREFIX_REXW); this doesn't work if freg == treg, zero-ing the from register will always result in zero in to register
                // prog->add(OPCODE_XOR_REG_RM);
                // prog->addModRM(MODE_REG,treg,treg);

                // prog->add(OPCODE_MOV_RM_REG);
                // prog->addModRM_SIB(MODE_DEREF_DISP8, BCToProgramReg(op0), SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                // prog->add((u8)-8);

                // prog->add3(OPCODE_3_MOVSS_REG_RM);
                // prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                // prog->add((u8)-8);

                // prog->add(OPCODE_MOV_RM_REG);
                // prog->addModRM_SIB(MODE_DEREF_DISP8, BCToProgramReg(op1), SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                // prog->add((u8)-8);

                // u32 operation = 0;
                // switch(opcode) {
                //     case BC_ADDF: operation = OPCODE_3_ADDSS_REG_RM; break;
                //     case BC_SUBF: operation = OPCODE_3_SUBSS_REG_RM; break;
                //     case BC_MULF: operation = OPCODE_3_MULSS_REG_RM; break;
                //     case BC_DIVF: operation = OPCODE_3_DIVSS_REG_RM; break;
                // }
                // prog->add3(operation);
                // prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                // prog->add((u8)-8);

                // prog->add3(OPCODE_3_MOVSS_RM_REG);
                // prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                // prog->add((u8)-8);

                // prog->add(OPCODE_MOV_REG_RM);
                // prog->addModRM_SIB(MODE_DEREF_DISP8, BCToProgramReg(op2), SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                // prog->add((u8)-8);

                u8 minSize = fsize < tsize ? fsize : tsize;

                if(type==CAST_FLOAT_SINT){
                    Assert(fsize == 4);

                    prog->add(OPCODE_MOV_RM_REG);
                    prog->addModRM_SIB(MODE_DEREF_DISP8, freg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                    prog->add((u8)-8);

                    if(tsize == 8)
                        prog->add4(OPCODE_4_REXW_CVTSS2SI_REG_RM);
                    else
                        prog->add3(OPCODE_3_CVTSS2SI_REG_RM);
                    prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                    prog->add((u8)-8);

                } else if(type==CAST_FLOAT_UINT){
                    Assert(fsize == 4);

                    prog->add(OPCODE_MOV_RM_REG);
                    prog->addModRM_SIB(MODE_DEREF_DISP8, freg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                    prog->add((u8)-8);

                    if(tsize == 8)
                        prog->add4(OPCODE_4_REXW_CVTSS2SI_REG_RM);
                    else
                        prog->add3(OPCODE_3_CVTSS2SI_REG_RM);

                    prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                    prog->add((u8)-8);

                } else if(type==CAST_SINT_FLOAT){
                    Assert(tsize == 4);
                    
                    if(fsize == 2){
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_AND_RM_IMM_SLASH_4);
                        prog->addModRM(MODE_REG, 4, freg);
                        prog->add4((u32)0xFFFF);
                    } else if(fsize == 1){
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_AND_RM_IMM_SLASH_4);
                        prog->addModRM(MODE_REG, 4, freg);
                        prog->add4((u32)0xFF);
                    }
                    if(fsize == 8)
                        prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                    else
                        prog->add3(OPCODE_3_CVTSI2SS_REG_RM);
                    
                    prog->addModRM(MODE_REG, REG_XMM0, freg);

                    prog->add3(OPCODE_3_MOVSS_RM_REG);
                    prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                    prog->add((u8)-8);

                    prog->add(OPCODE_MOV_REG_RM);
                    prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                    prog->add((u8)-8);
                } else if(type==CAST_UINT_FLOAT){
                    Assert(tsize == 4);
                    
                    if(fsize == 2){
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_AND_RM_IMM_SLASH_4);
                        prog->addModRM(MODE_REG, 4, freg);
                        prog->add4((u32)0xFFFF);
                    } else if(fsize == 1){
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_AND_RM_IMM_SLASH_4);
                        prog->addModRM(MODE_REG, 4, freg);
                        prog->add4((u32)0xFF);
                    }
                    if(fsize == 8||fsize==4)
                        prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                    else
                        prog->add3(OPCODE_3_CVTSI2SS_REG_RM);
                    
                    prog->addModRM(MODE_REG, REG_XMM0, freg);

                    prog->add3(OPCODE_3_MOVSS_RM_REG);
                    prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                    prog->add((u8)-8);

                    prog->add(OPCODE_MOV_REG_RM);
                    prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                    prog->add((u8)-8);
                } else if(type==CAST_UINT_SINT || type==CAST_SINT_UINT){
                    if(minSize==1){
                        prog->add(PREFIX_REXW);
                        prog->add2(OPCODE_2_MOVZX_REG_RM8);
                        prog->addModRM(MODE_REG, treg, freg);
                    } else if(minSize == 2) {
                        prog->add(PREFIX_REXW);
                        prog->add2(OPCODE_2_MOVZX_REG_RM16);
                        prog->addModRM(MODE_REG, treg, freg);
                    } else if(minSize == 4) {
                        Assert(freg == treg);
                        // prog->add(OPCODE_NOP); // this might just work
                        // prog->add(PREFIX_REXW);
                        // prog->add(OPCODE_AND_RM_IMM_SLASH_4);
                        // prog->addModRM(MODE_REG, 4, treg);
                        // prog->add4((u32)0xFFFFFFFF);
                    } else if(minSize == 8) {
                        Assert(freg == treg);
                        // nothing needs to be done
                        // prog->add(PREFIX_REXW);
                        // prog->add(OPCODE_MOV_REG_RM);
                        // prog->addModRM(MODE_REG, treg, freg);
                    }
                } else if(type==CAST_SINT_SINT){
                    if(minSize==1){
                        if(tsize==8)
                            prog->add(PREFIX_REXW);
                        else if(tsize==2)
                            prog->add(PREFIX_16BIT);
                        prog->add2(OPCODE_2_MOVSX_REG_RM8);
                        prog->addModRM(MODE_REG, treg, freg);
                    } else if(minSize == 2) {
                        if(tsize == fsize) {
                            // do nothing
                        } else {
                            if(tsize==8)
                                prog->add(PREFIX_REXW);
                            if(tsize==2){
                                prog->add(PREFIX_16BIT);
                                prog->add(OPCODE_MOVSXD_REG_RM);
                                prog->addModRM(MODE_REG, treg, freg);
                            } else {
                                prog->add2(OPCODE_2_MOVSX_REG_RM16);
                                prog->addModRM(MODE_REG, treg, freg);
                            }
                        }
                    } else if(minSize == 4) {
                        if(tsize==8)
                            prog->add(PREFIX_REXW);
                        prog->add(OPCODE_MOVSXD_REG_RM);
                        prog->addModRM(MODE_REG, treg, freg);
                    } else if(minSize == 8) {
                        Assert(freg == treg);
                        // do nothing
                        // prog->add(PREFIX_REXW);
                        // prog->add(OPCODE_MOV_REG_RM);
                        // prog->addModRM(MODE_REG, treg, freg);
                    }
                } else {
                    Assert(("Cast type not implemented in x64 backend",false));
                }

                break;
            }
            break; case BC_CALL: {
                LinkConventions linkConvention = (LinkConventions)op0;
                CallConventions callConvention = (CallConventions)op1;
                if(linkConvention == LinkConventions::DLLIMPORT) {
                    Assert(bytecode->externalRelocations[imm].location == bcIndex);
                    prog->add(OPCODE_CALL_RM_SLASH_2);
                    prog->addModRM_rip(2,(u32)0);
                    NamedRelocation namedReloc{};
                    namedReloc.name = bytecode->externalRelocations[imm].name;
                    namedReloc.textOffset = prog->size() - 4;
                    prog->namedRelocations.add(namedReloc);
                } else if(linkConvention == LinkConventions::IMPORT) {
                    Assert(bytecode->externalRelocations[imm].location == bcIndex);
                    prog->add(OPCODE_CALL_IMM);
                    NamedRelocation namedReloc{};
                    namedReloc.name = bytecode->externalRelocations[imm].name;
                    namedReloc.textOffset = prog->size();
                    prog->namedRelocations.add(namedReloc);
                    prog->add4((u32)0);
                } else if (linkConvention == LinkConventions::NONE){ // or export
                    prog->add(OPCODE_CALL_IMM);
                    prog->add4((u32)0);
                    
                    RelativeRelocation reloc{};
                    reloc.currentIP = prog->size();
                    reloc.bcAddress = imm;
                    reloc.immediateToModify = prog->size()-4;
                    relativeRelocations.add(reloc);
                } else if (linkConvention == LinkConventions::NATIVE){
                    //-- native function
                    switch(imm) {
                    // You could perhaps implement a platform layer which the language always links too.
                    // That way, you don't need to write different code for each platform.
                    #ifdef OS_WINDOWS
                    // case NATIVE_malloc: {
                        
                    // }
                    // break;
                        case NATIVE_prints: {
                        // ptr = [rsp + 0]
                        // len = [rsp + 8]
                        // char* ptr = *(char**)(fp+argoffset);
                        // u64 len = *(u64*)(fp+argoffset+8);
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM_SIB(MODE_DEREF, REG_SI, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        
                        prog->add((u8)(PREFIX_REXW));
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, REG_B, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)8);

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
                        NamedRelocation reloc0{};
                        reloc0.name = "__imp_GetStdHandle"; // C creates these symbol names in it's object file
                        reloc0.textOffset = prog->size() + 0xB;
                        NamedRelocation reloc1{};
                        reloc1.name = "__imp_WriteFile";
                        reloc1.textOffset = prog->size() + 0x26;
                        u8 arr[]={ 0x48, 0x83, 0xEC, 0x38, 0xB9, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0x4D, 0x31, 0xC9, 0x49, 0x89, 0xD8, 0x48, 0x89, 0xF2, 0x48, 0x89, 0xC1, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x38 };
                        prog->addRaw(arr,sizeof(arr));

                        prog->namedRelocations.add(reloc0);
                        prog->namedRelocations.add(reloc1);

                        break;
                    }
                    break; case NATIVE_printc: {
                        // char = [rsp + 7]
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM(MODE_REG, REG_SI, REG_SP);

                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_ADD_RM_IMM_SLASH_0);
                        prog->addModRM(MODE_REG, 0, REG_SI);
                        prog->add4((u32)7);

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
                        mov    r8,rbx                       # rbx = buffer
                        mov    rdx,rsi                      # rsi = length
                        mov    rcx,rax
                        call   QWORD PTR [rip+0x0]          # WriteFile(...)
                        add    rsp,0x38
                        */
                        NamedRelocation reloc0{};
                        reloc0.name = "__imp_GetStdHandle"; // C creates these symbol names in it's object file
                        reloc0.textOffset = prog->size() + 0xB;
                        NamedRelocation reloc1{};
                        reloc1.name = "__imp_WriteFile";
                        reloc1.textOffset = prog->size() + 0x26;
                        u8 arr[]={ 0x48, 0x83, 0xEC, 0x38, 0xB9, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0x4D, 0x31, 0xC9, 0x49, 0x89, 0xD8, 0x48, 0x89, 0xF2, 0x48, 0x89, 0xC1, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x38 };
                        prog->addRaw(arr,sizeof(arr));

                        prog->namedRelocations.add(reloc0);
                        prog->namedRelocations.add(reloc1);

                        break;
                    }
                    #endif
                    break; default: {
                        failure = true;
                        Assert(bytecode->nativeRegistry);
                        auto* nativeFunction = bytecode->nativeRegistry->findFunction(imm);
                        if(nativeFunction){
                            log::out << log::RED << "Native '"<<nativeFunction->name<<"' (id: "<<imm<<") is not implemented in x64 converter (" OS_NAME ").\n";
                        } else {
                            log::out << log::RED << imm<<" is not a native function (message from x64 converter).\n";
                        }
                    }
                    } // switch
                } else {
                    Assert(false);
                }
                break;
            }
            break; case BC_RET: {
                prog->add(OPCODE_RET);
                break;
            }
            break; case BC_MEMZERO: {
                // Assert(op0 == BC_REG_RDI && op1 == BC_REG_RBX);
                // here so you can turn it off.
                // Some time later...
                // I hate you. WHY DID YOU JUST LEAVE if(false)
                // You gotta take care of your mess because I wasted time
                // wondering what's going on!
                // You should have put a log message or something

                if(op0 != BC_REG_RDI) {
                    prog->add(PREFIX_REXW);
                    prog->add(OPCODE_MOV_REG_RM);
                    prog->addModRM(MODE_REG, REG_DI, BCToProgramReg(op0,8));
                }
                if(op1 != BC_REG_RBX) {
                    prog->add(PREFIX_REXW);
                    prog->add(OPCODE_MOV_REG_RM);
                    prog->addModRM(MODE_REG, REG_B, BCToProgramReg(op1,8));
                }
                /*
                    mov rdx, rbx
                    add rdx, 10
                    loop:
                    cmp rbx, rdx
                    je end
                    mov byte ptr [rbx], 0
                    inc rbx
                    jmp loop
                    end:
                */
                // you might want this to be a function
                u8 arr[] = { 0x48, 0x01, 0xFB, 0x48, 0x39, 0xDF, 0x74, 0x08, 0xC6, 0x07, 0x00, 0x48, 0xFF, 0xC7, 0xEB, 0xF3 };
                prog->addRaw(arr, sizeof(arr));

                // I begun by writing it by hand like this but this is annoying so I wrote it in
                // x64 assembly instead and then used a encoder to get the bytes.
                // Keep this in case I change my mind.
                /*
                prog->add(PREFIX_REXW);
                prog->add(OPCODE_ADD_REG_RM);
                prog->addModRM(MODE_REG, REG_B, REG_DI);

                int loopAddress = prog->size();
                prog->add(PREFIX_REXW);
                prog->add(OPCODE_CMP_REG_RM);
                prog->addModRM(MODE_REG, REG_B, REG_DI);

                prog->add(OPCODE_JE_IMM8);
                prog->add()
                */
                
                break;
            }
            break; case BC_MEMCPY: {
                // Assert(op0 == BC_REG_RDI && op1 == BC_REG_RSI && op2 == BC_REG_RBX);
                if(op0 != BC_REG_RDI) {
                    prog->add(PREFIX_REXW);
                    prog->add(OPCODE_MOV_REG_RM);
                    prog->addModRM(MODE_REG, REG_DI, BCToProgramReg(op0,8));
                }
                if(op1 != BC_REG_RSI) {
                    prog->add(PREFIX_REXW);
                    prog->add(OPCODE_MOV_REG_RM);
                    prog->addModRM(MODE_REG, REG_SI, BCToProgramReg(op1,8));
                }
                if(op2 != BC_REG_RBX) {
                    prog->add(PREFIX_REXW);
                    prog->add(OPCODE_MOV_REG_RM);
                    prog->addModRM(MODE_REG, REG_B, BCToProgramReg(op2,8));
                }
                /*
                add rbx, rsi
                loop:
                cmp rsi, rbx
                je end
                mov al, byte ptr [rsi]
                mov byte ptr [rdi], al
                inc rsi
                inc rdi
                jmp loop
                end:
                */
                u8 arr[] = { 0x48, 0x01, 0xF3, 0x48, 0x39, 0xDE, 0x74, 0x0C, 0x8A, 0x06, 0x88, 0x07, 0x48, 0xFF, 0xC6, 0x48, 0xFF, 0xC7, 0xEB, 0xEF };
                prog->addRaw(arr, sizeof(arr));
                break;
            }
            break; case BC_RDTSCP: {
                // This instruction uses edx:eax and ecx.
                // That is why the operands should specify these registers to indicate
                // that they are used. This isn't a must, you could edx, eax and ecx before
                // executing the instruction and then mov values into the operands
                Assert(op0 == BC_REG_RAX && op1 == BC_REG_ECX && op2 == BC_REG_RDX);
                
                prog->add3(OPCODE_3_RDTSCP);
                // prog->add2(OPCODE_2_RDTSC);
                
                prog->add(PREFIX_REXW);
                prog->add(OPCODE_SHL_RM_IMM8_SLASH_4);
                prog->addModRM(MODE_REG, 4, REG_D);
                prog->add((u8)32);

                // rdtscp will zero extern 8 byte registers
                prog->add(PREFIX_REXW);
                prog->add(OPCODE_OR_RM_REG);
                prog->addModRM(MODE_REG, REG_D, REG_A);
                
                break;
            }
            break; case BC_CMP_SWAP: {
                // This instruction uses edx:eax and ecx.
                // That is why the operands should specify these registers to indicate
                // that they are used. This isn't a must, you could edx, eax and ecx before
                // executing the instruction and then mov values into the operands
                Assert(op0 == BC_REG_RBX && op1 == BC_REG_EAX && op2 == BC_REG_EDX);

                u8 regPtr = BCToProgramReg(op0);
                u8 regOld = BCToProgramReg(op1);
                u8 regNew = BCToProgramReg(op2);
                
                prog->add(PREFIX_LOCK);
                prog->add2(OPCODE_2_CMPXCHG_RM_REG);
                prog->addModRM(MODE_DEREF, regNew, regPtr);

                prog->add2(OPCODE_2_SETE_RM8);
                prog->addModRM(MODE_REG, 0, regOld);

                prog->add(OPCODE_AND_RM_IMM8_SLASH_4);
                prog->addModRM(MODE_REG, 4, regOld);
                prog->add((u8)0x1);

                break;
            }
            // break; case BC_SIN: {
            //     /*
            //     float sine(float x) {
            //         const float RAD = 1.57079632679f;
            //         int inv = (x<0)<<1;
            //         if(inv)
            //             x = -x;
            //         int t = x/RAD;
            //         if (t&1)
            //             x = RAD * (1+t) - x;
            //         else
            //             x = x - RAD * t;

            //         // float taylor = (x - x*x*x/(2*3) + x*x*x*x*x/(2*3*4*5) - x*x*x*x*x*x*x/(2*3*4*5*6*7) + x*x*x*x*x*x*x*x*x/(2*3*4*5*6*7*8*9));
            //         float taylor = x - x*x*x/(2*3) * ( 1 - x*x/(4*5) * ( 1 - x*x/(6*7) * (1 - x*x/(8*9))));
            //         if ((t&2) ^ inv)
            //             taylor = -taylor;
            //         return taylor;
            //     }
            //     */
            //     // Assert(op0 == BC_REG_RDI && op1 == BC_REG_RSI && op2 == BC_REG_RBX);
            //     if(op0 != BC_REG_EAX) {
            //         prog->add(PREFIX_REXW);
            //         prog->add(OPCODE_MOV_REG_RM);
            //         prog->addModRM(MODE_REG, REG_DI, BCToProgramReg(op0,8));
            //     }
                

                
            //     // prog->add2(OPCODE_2_FSIN);

            //     // u8 arr[]={ 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x08, 0x48, 0x83, 0xEC, 0x28, 0xF3, 0x0F, 0x10, 0x05, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x10, 0x0F, 0x57, 0xC0, 0x0F, 0x2F, 0x44, 0x24, 0x30, 0x76, 0x0A, 0xC7, 0x44, 0x24, 0x08, 0x01, 0x00, 0x00, 0x00, 0xEB, 0x08, 0xC7, 0x44, 0x24, 0x08, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x44, 0x24, 0x08, 0xD1, 0xE0, 0x89, 0x44, 0x24, 0x0C, 0x83, 0x7C, 0x24, 0x0C, 0x00, 0x74, 0x13, 0xF3, 0x0F, 0x10, 0x44, 0x24, 0x30, 0x0F, 0x57, 0x05, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x30, 0xF3, 0x0F, 0x10, 0x44, 0x24, 0x30, 0xF3, 0x0F, 0x5E, 0x05, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x2C, 0xC0, 0x89, 0x04, 0x24, 0x8B, 0x04, 0x24, 0x83, 0xE0, 0x01, 0x85, 0xC0, 0x74, 0x26, 0x8B, 0x04, 0x24, 0xFF, 0xC0, 0xF3, 0x0F, 0x2A, 0xC0, 0xF3, 0x0F, 0x10, 0x0D, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x59, 0xC8, 0x0F, 0x28, 0xC1, 0xF3, 0x0F, 0x5C, 0x44, 0x24, 0x30, 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x30, 0xEB, 0x27, 0xF3, 0x0F, 0x2A, 0x04, 0x24, 0xF3, 0x0F, 0x10, 0x0D, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x59, 0xC8, 0x0F, 0x28, 0xC1, 0xF3, 0x0F, 0x10, 0x4C, 0x24, 0x30, 0xF3, 0x0F, 0x5C, 0xC8, 0x0F, 0x28, 0xC1, 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x30, 0xF3, 0x0F, 0x10, 0x44, 0x24, 0x30, 0xF3, 0x0F, 0x59, 0x44, 0x24, 0x30, 0xF3, 0x0F, 0x59, 0x44, 0x24, 0x30, 0xF3, 0x0F, 0x5E, 0x05, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x10, 0x4C, 0x24, 0x30, 0xF3, 0x0F, 0x59, 0x4C, 0x24, 0x30, 0xF3, 0x0F, 0x5E, 0x0D, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x10, 0x54, 0x24, 0x30, 0xF3, 0x0F, 0x59, 0x54, 0x24, 0x30, 0xF3, 0x0F, 0x5E, 0x15, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x10, 0x5C, 0x24, 0x30, 0xF3, 0x0F, 0x59, 0x5C, 0x24, 0x30, 0xF3, 0x0F, 0x5E, 0x1D, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x10, 0x25, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x5C, 0xE3, 0x0F, 0x28, 0xDC, 0xF3, 0x0F, 0x59, 0xD3, 0xF3, 0x0F, 0x10, 0x1D, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x5C, 0xDA, 0x0F, 0x28, 0xD3, 0xF3, 0x0F, 0x59, 0xCA, 0xF3, 0x0F, 0x10, 0x15, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x5C, 0xD1, 0x0F, 0x28, 0xCA, 0xF3, 0x0F, 0x59, 0xC1, 0xF3, 0x0F, 0x10, 0x4C, 0x24, 0x30, 0xF3, 0x0F, 0x5C, 0xC8, 0x0F, 0x28, 0xC1, 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x04, 0x8B, 0x04, 0x24, 0x83, 0xE0, 0x02, 0x33, 0x44, 0x24, 0x0C, 0x85, 0xC0, 0x74, 0x13, 0xF3, 0x0F, 0x10, 0x44, 0x24, 0x04, 0x0F, 0x57, 0x05, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x04, 0xF3, 0x0F, 0x10, 0x44, 0x24, 0x04, 0x48, 0x83, 0xC4, 0x28, 0xC3 };
            //     // prog->addRaw(arr,sizeof(arr));

            //     break;
            // }
            break; {
                /*
                logb(x) = logb(2) * log2(x)

                log2(50) = log2(5 * 5 * 2)
                log2(5) + log2(5) + log2(2)

                1 + 2 * log2(5)

                (25-1)/5
                24

                How to implement exp
                exp(x,y)
                
                if y%2
                    a = x
                    for y-1
                        a = a * x
                else
                    a = 1
                    for y
                        a = a * x * x


                


                log(x) ~= (x^2 - 1) / x

                log2(x)
                1/(x * ln(2))
                */
            }
            default: {
                log::out << log::RED << "Implement "<< log::PURPLE<< InstToString(inst.opcode)<< "\n";
                failure = true;
            }
        }

        // if(prevSize == prog->size()){
        //     log::out << log::RED << "Implement "<< log::PURPLE<< InstToString(inst.opcode)<< "\n";
        //     // failure = true;
        // }
    }

    for(int i=0;i<relativeRelocations.size();i++) {
        auto& rel = relativeRelocations[i];
        if(rel.bcAddress == bytecode->length())
            *(i32*)(prog->text + rel.immediateToModify) = prog->size() - rel.currentIP;
        else {
            Assert(rel.bcAddress>=0); // native functions not implemented
            i32 value = addressTranslation[rel.bcAddress] - rel.currentIP;
            *(i32*)(prog->text + rel.immediateToModify) = value;
            // If you are wondering why the relocation is zero then it is because it
            // is relative and will jump to the next instruction. This isn't necessary so
            // they should be optimised away. Don't do this now since it makes the conversion
            // confusing.
        }
    }
    
    prog->add(OPCODE_POP_RM_SLASH_0);
    prog->addModRM(MODE_REG,0,REG_BP);

    prog->add(OPCODE_RET);
    
    if(failure){
        log::out << log::RED << "x64 conversion failed\n";
        Program_x64::Destroy(prog);
        prog = nullptr;
    } else {
        // prog->printHex();
    }
    return prog;
}

void Program_x64::Destroy(Program_x64* program) {
    using namespace engone;
    Assert(program);
    program->~Program_x64();
    Free(program,sizeof(Program_x64));
}
Program_x64* Program_x64::Create() {
    using namespace engone;

    auto program = (Program_x64*)Allocate(sizeof(Program_x64));
    new(program)Program_x64();
    return program;
}