#include "BetBat/x64_Converter.h"

#include "BetBat/COFF.h"
#include "BetBat/ELF.h"

#include "BetBat/CompilerEnums.h"

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

#define REG_A 0b000
#define REG_C 0b001
#define REG_D 0b010
#define REG_B 0b011
#define REG_SP 0b100
#define REG_BP 0b101
#define REG_SI 0b110
#define REG_DI 0b111

#define REG_R8 0b000
#define REG_R9 0b001
#define REG_R10 0b010
#define REG_R11 0b011
#define REG_R12 0b100
#define REG_R13 0b101
#define REG_R14 0b110
#define REG_R15 0b111

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
            TRACK_ARRAY_FREE(text, u8, _allocationSize);
            // engone::Free(text, _allocationSize);
        }
        text = nullptr;
        _allocationSize = 0;
        head = 0;
        return true;
    }
    if(!text){
        // text = (u8*)engone::Allocate(newAllocationSize);
        text = TRACK_ARRAY_ALLOC(u8, newAllocationSize);
        Assert(text);
        // initialization of elements is done when adding them
        if(!text)
            return false;
        _allocationSize = newAllocationSize;
        return true;
    } else {
        TRACK_DELS(u8, _allocationSize);
        u8* newText = (u8*)engone::Reallocate(text, _allocationSize, newAllocationSize);
        TRACK_ADDS(u8, newAllocationSize);
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
        bool yes = _reserve(_allocationSize*2 + 100);
        Assert(yes);
    }
    *(text + head) = byte;
    head++;
}
void Program_x64::add2(u16 word){
    if(head+2 >= _allocationSize ){
        bool yes = _reserve(_allocationSize*2 + 100);
        Assert(yes);
    }
    
    auto ptr = (u8*)&word;
    *(text + head + 0) = *(ptr + 0);
    *(text + head + 1) = *(ptr + 1);
    head+=2;
}void Program_x64::add3(u32 word){
    if(head>0){
        // This is not how you use rex prefix
        // cvtsi2ss instructions use smashed in between it's other opcode bytes
        Assert(*(text + head - 1) != PREFIX_REXW);
    }
    Assert(0==(word&0xFF000000));
    if(head+3 >= _allocationSize ){
        bool yes = _reserve(_allocationSize*3 + 100);
        Assert(yes);
    }
    
    auto ptr = (u8*)&word;
    *(text + head + 0) = *(ptr + 0);
    *(text + head + 1) = *(ptr + 1);
    *(text + head + 2) = *(ptr + 2);
    head+=3;
}
void Program_x64::add4(u32 dword){
    // wish we could assert to find bugs but immediates may be zero and won't work.
    // we could make a nother functionn specifcially for immediates though.
    // Assert(dword & 0xFF00'0000);
    if(head+4 >= _allocationSize ){
        bool yes = _reserve(_allocationSize*2 + 100);
        Assert(yes);
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
        bool yes = _reserve(_allocationSize*1.2 + 10 + len);
        Assert(yes);
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
void Program_x64::printAsm(const char* path, const char* objpath){
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
    case LINKER_GNU: {
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
// allowRX refers to the registers R8-R11
u8 BCToProgramReg(u8 bcreg, int handlingSizes = 4, bool allowXMM = false,  bool allowRX = false){
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
        return DECODE_REG_TYPE(bcreg) - BC_XMM0;
    }
    if(IS_REG_RX(bcreg)) {
        Assert(allowRX);
        return DECODE_REG_TYPE(bcreg) - BC_R8;
    }
    // normal
    Assert(IS_REG_NORMAL(bcreg));
    u8 id = DECODE_REG_TYPE(bcreg) - BC_AX;
    return id;
}

Program_x64* ConvertTox64(Bytecode* bytecode){
    using namespace engone;
    Assert(bytecode);
    ZoneScopedC(tracy::Color::SkyBlue1);

    _VLOG(log::out <<log::BLUE<< "x64 Converter:\n";)

    Program_x64* prog = Program_x64::Create();

    // steal debug information
    prog->debugInformation = bytecode->debugInformation;
    bytecode->debugInformation = nullptr;

    // TODO: Multithreading here is possible. Each thread could output instructions into it's own
    //   section of instructions. Then you would finish up with relocations which you can use multiple threads
    //   for too. The only thing is that when translating relocations from bytecode to x64 you would need
    //   to store information about which section it points to.
    //   This is my initial thought on how to allow multiple threads. There are probably some other
    //   issues you need to figure out.

    // TODO: You may want some functions to handle the allocation here.
    //  like how you can reserve memory for the text allocation.
    if(bytecode->dataSegment.used!=0){
        prog->globalData = TRACK_ARRAY_ALLOC(u8, bytecode->dataSegment.used);
        // prog->globalData = (u8*)engone::Allocate(bytecode->dataSegment.used);
        Assert(prog->globalData);
        prog->globalSize = bytecode->dataSegment.used;
        memcpy(prog->globalData, bytecode->dataSegment.data, prog->globalSize);

        // OutputAsHex("data.txt", (char*)prog->globalData, prog->globalSize);
    }
    bool failure = false;

    // TODO: Inline assembly can be computed on multiple threads but probably not worth
    //   because you usually have very instances of inline assembly and on top of that very few instructions in them.

    // u32 tempBufferSize = 0x10000;
    // char* tempBuffer = (char*)engone::Allocate(tempBufferSize);
    for (int i=0;i<bytecode->asmInstances.size();i++){
        Bytecode::ASM& asmInst = bytecode->asmInstances.get(i);
        
        #define TEMP_ASM_FILE "bin/inline_asm.asm"
        #define TEMP_ASM_OBJ_FILE "bin/inline_asm.o"
        auto file = engone::FileOpen(TEMP_ASM_FILE,FILE_CLEAR_AND_WRITE);
        defer { if(file) engone::FileClose(file); };
        if(!file) {
            log::out << log::RED << "Could not create " TEMP_ASM_FILE "!\n";
            continue;
        }
        #ifdef OS_WINDOWS
        const char* pretext = ".code\n";
        int asm_line_offset = 1;
        #else
        const char* pretext = ".intel_syntax noprefix\n.text\n";
        int asm_line_offset = 2;
        #endif
        bool yes = engone::FileWrite(file, pretext, strlen(pretext));
        Assert(yes);
        char* asmText = bytecode->rawInlineAssembly._ptr + asmInst.start;
        u32 asmLen = asmInst.end - asmInst.start;
        yes = engone::FileWrite(file, asmText, asmLen);
        Assert(yes);
        #ifdef OS_WINDOWS
        const char* posttext = "END\n";
        yes = engone::FileWrite(file, posttext, strlen(posttext));
        Assert(yes);
        #else
        const char* posttext = "\n"; // assembler complains about not having a newline so we add one here
        yes = engone::FileWrite(file, posttext, strlen(posttext));
        Assert(yes);
        #endif

        engone::FileClose(file);
        file = {};

        auto asmLog = engone::FileOpen("bin/asm.log",FILE_CLEAR_AND_WRITE);
        defer { if(asmLog) engone::FileClose(asmLog); };

        // TODO: Turn off logging from ml64? or at least pipe into somewhere other than stdout.
        //  If ml64 failed then we do want the error messages.
        // TODO: ml64 can compile multiple assembly files into multiple object files.
        //  This is probably faster than doing one by one. Altough, be wary of command line character limit.
        #ifdef OS_WINDOWS
        std::string cmd = "ml64 /nologo /Fo " TEMP_ASM_OBJ_FILE " /c ";
        #else
        std::string cmd = "as -o " TEMP_ASM_OBJ_FILE " ";
        #endif
        cmd += TEMP_ASM_FILE;
        int exitCode = 0;
        yes = engone::StartProgram((char*)cmd.data(), PROGRAM_WAIT, &exitCode, {}, asmLog, asmLog);
        Assert(yes);
        if(exitCode != 0) {
            u64 fileSize = engone::FileGetSize(asmLog);
            engone::FileSetHead(asmLog, 0);
            
            QuickArray<char> errorMessages{};
            errorMessages.resize(fileSize);
            
            yes = engone::FileRead(asmLog, errorMessages.data(), errorMessages.size());
            Assert(yes);
            
            ReformatAssemblerError(asmInst, errorMessages, -asm_line_offset);
            
            failure = true;
            continue;
        }
        engone::FileClose(asmLog);
        asmLog = {};

        // TODO: DeconstructFile isn't optimized and we deconstruct symbols and segments we don't care about.
        //  Write a specific function for just the text segment.
        #ifdef OS_WINDOWS
        auto objfile = FileCOFF::DeconstructFile(TEMP_ASM_OBJ_FILE);
        defer { FileCOFF::Destroy(objfile); };
        if(!objfile) {
            log::out << log::RED << "Could not find " TEMP_ASM_OBJ_FILE "!\n";
            continue;
        }
        int sectionIndex = -1;
        for(int j=0;j<objfile->sections.size();j++){
            std::string name = objfile->getSectionName(j);
            if(name == ".text" || (name.length()>=6 && !strncmp(name.c_str(), ".text$",6))) {
                // auto section = objfile->sections[j];
                // Assert(section->NumberOfRelocations == 0); // not handled at the moment
                sectionIndex = j;
                // u8* ptr = objfile->_rawFileData + section->PointerToRawData;
                // u32 len = section->SizeOfRawData;
            }
        }
        if(sectionIndex == -1){
            log::out << log::RED << "Text section could not be found for the compiled inline assembly. Compiler bug?\n";
            continue;
        }
        auto section = objfile->sections[sectionIndex];
        if(section->NumberOfRelocations!=0){
            // TODO: Tell the user where the relocation was? Which instruction?
            //  user may type 'pop rac' instead of 'pop rax'. pop rac is a valid instruction if rac is a symbol.
            //  This mistake is difficult to detect.
            log::out << log::RED << "Relocations are not supported in inline assembly.\n";
            continue;
        }
        
        u8* ptr = objfile->_rawFileData + section->PointerToRawData;
        u32 len = section->SizeOfRawData;
        // log::out << "Inline assembly "<<i << " size: "<<len<<"\n";
        asmInst.iStart = bytecode->rawInstructions.used;
        bytecode->rawInstructions._reserve(bytecode->rawInstructions.used + len);
        memcpy(bytecode->rawInstructions._ptr + bytecode->rawInstructions.used, ptr, len);
        bytecode->rawInstructions.used += len;
        asmInst.iEnd = bytecode->rawInstructions.used;
        #else
        auto objfile = FileELF::DeconstructFile(TEMP_ASM_OBJ_FILE);
        defer { FileELF::Destroy(objfile); };
        if(!objfile) {
            log::out << log::RED << "Could not find " TEMP_ASM_OBJ_FILE "!\n";
            continue;
        }
        int textSection_index = objfile->sectionIndexByName(".text");
        if(textSection_index == 0) {
            log::out << log::RED << "Text section could not be found for the compiled inline assembly. Compiler bug?\n";
            continue;
        }
        
        int relocations_count=0;
        auto relocations = objfile->relaOfSection(textSection_index, &relocations_count);
        if(relocations_count!=0){
            log::out << log::RED << "Relocations are not supported in inline assembly.\n";
            continue;
        }
        
        int textSize = 0;
        const u8* textData = objfile->dataOfSection(textSection_index, &textSize);
        Assert(textData);
        
        // u8* ptr = objfile->_rawFileData + section->PointerToRawData;
        // u32 len = section->SizeOfRawData;
        // log::out << "Inline assembly "<<i << " size: "<<len<<"\n";
        asmInst.iStart = bytecode->rawInstructions.used;
        bytecode->rawInstructions._reserve(bytecode->rawInstructions.used + textSize);
        memcpy(bytecode->rawInstructions._ptr + bytecode->rawInstructions.used, textData, textSize);
        bytecode->rawInstructions.used += textSize;
        asmInst.iEnd = bytecode->rawInstructions.used;
        #endif
    }
    // engone::Free(tempBuffer,tempBufferSize);
    // TODO: Optionally delete obj and asm files from inline assembly.

    // prog->add(OPCODE_XOR_REG_RM);
    // prog->addModRM(MODE_REG,REG_A,REG_A);
    // prog->add(OPCODE_JMP_IMM);
    // prog->add4(8);
    // for(int i=0;i<16;i++){
    //     prog->add(OPCODE_INCR_RM_SLASH_0);
    //     prog->addModRM(MODE_REG, 0, REG_A);
    // }

    // TODO: Can this be improved?
    //  array to translate addresses of bytecode instructions to
    //  the ones in x64 instructions.
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

    // FP push is fixed by generator
    // prog->add(OPCODE_PUSH_RM_SLASH_6);
    // prog->addModRM(MODE_REG,6,REG_BP);

    // prog->add(PREFIX_REXW);
    // prog->add(OPCODE_MOV_REG_RM);
    // prog->addModRM(MODE_REG, REG_BP, REG_SP);

    // for(int i=0;i<0;i++){
    for(int bcIndex=0;bcIndex<bytecode->length();bcIndex++){
        auto inst = bytecode->get(bcIndex);
        addressTranslation[bcIndex] = prog->size();
        u8 opcode = inst.opcode;
        auto op0 = inst.op0;
        auto op1 = inst.op1;
        auto op2 = inst.op2;
        i32 imm = 0;
        i32 imm2 = 0;
        
        _CLOG(log::out << bcIndex << ": "<< inst;)
        if(opcode == BC_LI || opcode==BC_JMP || opcode==BC_JNZ || opcode==BC_JZ || opcode==BC_CALL || opcode==BC_DATAPTR||
            opcode == BC_MOV_MR_DISP32 || opcode == BC_MOV_RM_DISP32 || opcode == BC_CODEPTR||opcode==BC_TEST_VALUE){
            bcIndex++;
            imm = bytecode->getIm(bcIndex);
            addressTranslation[bcIndex] = prog->size();
            if(opcode == BC_LI && op1 == 2){
                bcIndex++;
                imm2 = bytecode->getIm(bcIndex);
                addressTranslation[bcIndex] = prog->size();
                _CLOG(log::out << " "<<((u64)imm|((u64)imm2)<<32);)
            } else {
                _CLOG(log::out << " "<<imm;)
            }
        }
        _CLOG(log::out << "\n";)
        
        u32 prevSize = prog->size();

        switch(opcode){
            case BC_MOV_RR: {
                u8 size0 = DECODE_REG_SIZE(op0);
                u8 size1 = DECODE_REG_SIZE(op1);
                u8 rex = 0;
                bool extend_op0 = false;
                bool extend_op1 = false;
                bool xmm_op0 = false;
                bool xmm_op1 = false;
                if(IS_REG_RX(op0)) {
                    rex |= PREFIX_REXB;
                    extend_op0 = true;
                }
                if(IS_REG_XMM(op0)) {
                    xmm_op0 = true;
                }
                if(IS_REG_RX(op1)) {
                    rex |= PREFIX_REXR;
                    extend_op1 = true;
                }
                if(IS_REG_XMM(op1)) {
                    xmm_op1 = true;
                }
                u8 reg0 = BCToProgramReg(op0,0xF, xmm_op0, extend_op0);
                u8 reg1 = BCToProgramReg(op1,0xF, xmm_op1, extend_op1);
                if(xmm_op0 && xmm_op1) {
                    // testandcommit
                    Assert(size0 == size1);
                    switch(size0) {
                        case 4: {
                            prog->add3(OPCODE_3_MOVSS_REG_RM);
                            prog->addModRM(MODE_REG, reg1, reg0);
                            break;
                        }
                        case 8: {
                            prog->add3(OPCODE_3_MOVSD_REG_RM);
                            prog->addModRM(MODE_REG, reg1, reg0);
                            break;
                        }
                        default: {
                            Assert(("operands for BC_MOV_RR have invalid size, they must be 4 or 8 bytes (f32 or f64) COMPILER BUG?",false));
                            break;
                        }
                    }
                } else if(xmm_op0) {
                    Assert(size0 == size1);
                    // NOTE: Is it okay to write below the stack pointer?
                    // testandcommit
                    switch(size0) {
                    case 4: {
                        prog->add3(OPCODE_3_MOVSS_RM_REG);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, reg0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);

                        if(extend_op1) 
                            prog->add(PREFIX_REXR);
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, reg1, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);
                        break;
                    }
                    case 8: {
                        prog->add3(OPCODE_3_MOVSD_RM_REG);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, reg0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);

                        if(extend_op1) 
                            prog->add(PREFIX_REXR);
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, reg1, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);
                        break;
                    }
                    default: {
                        Assert(("operands for BC_MOV_RR have invalid size, they must be 4 or 8 bytes (f32 or f64) COMPILER BUG?",false));
                        break;
                    }
                    }
                } else if(xmm_op1) {
                    // testandcommit
                    Assert(size0 == size1);
                    switch(size0) {
                    case 4: {
                        if(extend_op0) 
                            prog->add(PREFIX_REXR);
                        prog->add(OPCODE_MOV_RM_REG);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, reg0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);

                        prog->add3(OPCODE_3_MOVSS_REG_RM);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, reg1, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);
                        break;
                    }
                    case 8: {
                        if(extend_op0) 
                            prog->add(PREFIX_REXR);
                        prog->add(OPCODE_MOV_RM_REG);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, reg0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);

                        prog->add3(OPCODE_3_MOVSD_REG_RM);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, reg1, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);
                        break;
                    }
                    default: {
                        Assert(("operands for BC_MOV_RR have invalid size, they must be 4 or 8 bytes (f32 or f64) COMPILER BUG?",false));
                        break;
                    }
                    }
                } else {
                    // Assert(size0 == size1);
                    // We may be doing a dangerous thing if the operands are
                    // of different sizes.
                    // if(size0 != 8 && size0 != 4){
                    //     prog->add((u8)(PREFIX_REXW|rex));
                    //     prog->add(OPCODE_XOR_REG_RM);
                    //     prog->addModRM(MODE_REG, reg1, reg1);
                    // }

                    if(size0==8)
                        rex |= PREFIX_REXW;
                    
                    if(size0 == 1) {
                        // Assert(!extend_op0 && !extend_op1);
                        if(rex!=0)
                            prog->add(rex);
                        prog->add2(OPCODE_2_MOVZX_REG_RM8);
                        prog->addModRM(MODE_REG, reg1, reg0);
                    } else if(size0 == 2){
                        // Assert(!extend_op0 && !extend_op1);
                        // prog->add(PREFIX_16BIT);
                        if(rex!=0)
                            prog->add(rex);
                        prog->add2(OPCODE_2_MOVZX_REG_RM16);
                        prog->addModRM(MODE_REG, reg1, reg0);
                    } else if(size0 == 4) {
                        if(rex!=0)
                            prog->add(rex);
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM(MODE_REG, reg1, reg0);
                    } else if(size0 == 8) {
                        if(rex!=0)
                            prog->add((u8)(rex | PREFIX_REXW));
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM(MODE_REG, reg1, reg0);
                    }
                }
                break;
            }
            break;
            case BC_MOV_MR:
            case BC_MOV_MR_DISP32:
            case BC_MOV_RM:
            case BC_MOV_RM_DISP32: {
                Assert(DECODE_REG_TYPE(op0)!=DECODE_REG_TYPE(op1));
                // IMPORTANT: Be careful when changing stuff here. You might think you know
                //  but you really don't. Keep track of which operand is destination and output
                //  and which one is register and register/memory
                bool to_memory = opcode == BC_MOV_RM || opcode == BC_MOV_RM_DISP32;
                if(!to_memory) {
                    i8 size = (i8)op2;
                    #ifndef ENABLE_FAULTY_X64
                    if(size != 8 && size != 4) {
                        // the upper bits of 32-bit registers will be cleared automatically.
                        // 8-bit and 16-bit registers will not do that so we do it ourselves.
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_XOR_REG_RM);
                        prog->addModRM(MODE_REG,BCToProgramReg(op1,0xF),BCToProgramReg(op1,0xF));
                    }
                    #endif
                }
                u8 mov_size = op2;
                
                bool xmm_reg, extend_reg;
                u8 reg_reg, reg_rm;

                if(to_memory) {
                    xmm_reg = IS_REG_XMM(op0);
                    extend_reg = IS_REG_RX(op0);
                    reg_reg = BCToProgramReg(op0,0xF, xmm_reg, extend_reg);
                    reg_rm = BCToProgramReg(op1,8);
                } else {
                    xmm_reg = IS_REG_XMM(op1);
                    extend_reg = IS_REG_RX(op1);
                    reg_reg = BCToProgramReg(op1,0xF, xmm_reg, extend_reg);
                    reg_rm = BCToProgramReg(op0,8);
                }


                // special cases, see x64 manual
                u8 mode = MODE_DEREF;
                bool useSIB = false;
                if(reg_rm == REG_SP) {
                    useSIB = true;
                }
                if(imm==0){
                    if(reg_rm == REG_BP) {
                        mode = MODE_DEREF_DISP8;
                    }
                } else if(imm >= -128 && imm <= 127) {
                    mode = MODE_DEREF_DISP8;
                } else {
                    mode = MODE_DEREF_DISP32;
                }

                if(xmm_reg) {
                    Assert(mov_size == 4 || mov_size == 8); // moving 1 or 2 bytes from a float doesn't make since and double floats aren't supported yet.
                    if(to_memory) {
                        if(mov_size == 4)
                            prog->add3(OPCODE_3_MOVSS_RM_REG);
                        else
                            prog->add3(OPCODE_3_MOVSD_RM_REG);
                    } else {
                        if(mov_size == 4)
                            prog->add3(OPCODE_3_MOVSS_REG_RM);
                        else
                            prog->add3(OPCODE_3_MOVSD_REG_RM);
                    }
                } else {
                    switch(mov_size) {
                        case 1: {
                            if(extend_reg)
                                prog->add(PREFIX_REXR);
                            if(to_memory)
                                prog->add(OPCODE_MOV_RM_REG8);
                            else
                                prog->add(OPCODE_MOV_REG8_RM);
                            break;
                        }
                        case 2: {
                            if(extend_reg)
                                prog->add((u8)(PREFIX_16BIT|PREFIX_REXR));
                            else
                                prog->add(PREFIX_16BIT);
                            if(to_memory)
                                prog->add(OPCODE_MOV_RM_REG);
                            else
                                prog->add(OPCODE_MOV_REG_RM);
                            break;
                        }
                        case 4: {
                            if(extend_reg)
                                prog->add(PREFIX_REXR);
                            if(to_memory)
                                prog->add(OPCODE_MOV_RM_REG);
                            else
                                prog->add(OPCODE_MOV_REG_RM);
                            break;
                        }
                        case 8: {
                            if(extend_reg)
                                prog->add((u8)(PREFIX_REXW|PREFIX_REXR));
                            else
                                prog->add(PREFIX_REXW);
                            if(to_memory)
                                prog->add(OPCODE_MOV_RM_REG);
                            else
                                prog->add(OPCODE_MOV_REG_RM);
                            break;
                        }
                        default: {
                            Assert(false);
                        }
                    }
                }
                if(useSIB) {
                    prog->addModRM_SIB(mode, reg_reg, SIB_SCALE_1, SIB_INDEX_NONE, reg_rm);
                } else {
                    prog->addModRM(mode, reg_reg, reg_rm);
                }
                if(mode == MODE_DEREF_DISP8) {
                    prog->add((u8)imm);
                } else if(mode == MODE_DEREF_DISP32) {
                    prog->add4((u32)imm);
                }
                break;
            }
            case BC_NOP: {
                prog->add(OPCODE_NOP);
                break;
            }
            break; case BC_ADDI: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;
                // add doesn't use three operands
                u8 size = DECODE_REG_SIZE(op0);
                if(size==8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_ADD_RM_REG);
                prog->addModRM(MODE_REG, BCToProgramReg(op01,0xF), BCToProgramReg(op2,0xF));
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
                u8 type = op0;
                u8 in0 = BCToProgramReg(op1, 0xF);
                u8 in1 = BCToProgramReg(op2, 0xF);
                u8 out = in1;
                u8 size = DECODE_REG_SIZE(op2);
                
                Assert(in1 == REG_A && out == REG_A);
                
                if(size == 2)
                    prog->add(PREFIX_16BIT);
                if(size == 8)
                    prog->add(PREFIX_REXW);
                
                if(type == ARITHMETIC_SINT){
                    if(size == 1) {
                        prog->add(OPCODE_IMUL_AX_RM8_SLASH_5);
                        prog->addModRM(MODE_REG, 5, in0);
                    } else {
                        prog->add(OPCODE_IMUL_AX_RM_SLASH_5);
                        prog->addModRM(MODE_REG, 4, in0);
                    }
                } else if(type == ARITHMETIC_UINT){
                    if(size == 1) {
                        prog->add(OPCODE_MUL_AX_RM8_SLASH_4);
                        prog->addModRM(MODE_REG, 5, in0);
                    } else {
                        prog->add(OPCODE_MUL_AX_RM_SLASH_4);
                        prog->addModRM(MODE_REG, 4, in0);
                    }
                }
                break;
            }
            break; case BC_DIVI: {
                u8 type = op0;
                u8 in0 = BCToProgramReg(op1, 0xF);
                u8 in1 = BCToProgramReg(op2, 0xF);
                u8 out = in0;
                u8 size = DECODE_REG_SIZE(op2);
                
                Assert(in0 == REG_A && in1 == REG_D && out == REG_A);
    
                // TODO: Do same with BC_MOD
                
                prog->add(OPCODE_MOV_REG_RM);
                prog->addModRM(MODE_REG, REG_SI, in1);
                in1 = REG_SI;
                
                prog->add(PREFIX_REXW);
                prog->add(OPCODE_XOR_REG_RM);
                prog->addModRM(MODE_REG, REG_D, REG_D);
    
                u8 extra = type == ARITHMETIC_SINT ? 7 : 6;
                u8 opc = size == 1 ? OPCODE_IDIV_AX_RM8_SLASH_7 : OPCODE_DIV_AX_RM_SLASH_6;
                
                if(size == 2)
                    prog->add(PREFIX_16BIT);
                if(size == 8)
                    prog->add(PREFIX_REXW);
                if(size == 1)
                    prog->add(PREFIX_REX); // enables usage of low part of SI (SIL)
                prog->add(opc);
                prog->addModRM(MODE_REG, extra, in1);
                if(size == 1) {
                    prog->add(OPCODE_AND_RM8_IMM8_SLASH_4);
                    prog->addModRM(MODE_REG, 4, out);
                }

                // u8 size = DECODE_REG_SIZE(op2);
                
                // if(DECODE_REG_TYPE(op2) != BC_AX){
                //     prog->add(OPCODE_PUSH_RM_SLASH_6);
                //     prog->addModRM(MODE_REG, 6, REG_A);
                // }
                
                // if(DECODE_REG_TYPE(op2) != BC_DX){
                //     prog->add(OPCODE_PUSH_RM_SLASH_6);
                //     prog->addModRM(MODE_REG, 6, REG_D);
                // }

                // // I CANNOT USE EDX, EAX
                // // op0 if eax do nothing, just sign extend, otherwise move op0 into eax
                // // 
                // // op2 can be whatever, move result to it at the end

                // if(DECODE_REG_TYPE(op0)!=BC_AX){
                //     // move reg in op0 to eax
                //     if(size == 8)
                //         prog->add(PREFIX_REXW);
                //     prog->add(OPCODE_MOV_REG_RM);
                //     prog->addModRM(MODE_REG, REG_A, BCToProgramReg(op0));
                // }
                // if(size == 8)
                //     prog->add(PREFIX_REXW);
                // prog->add(OPCODE_CDQ); // sign extend eax into edx
                // if(size == 8)
                //     prog->add(PREFIX_REXW);
                // prog->add(OPCODE_IDIV_AX_RM_SLASH_7);
                // prog->addModRM(MODE_REG, 7, BCToProgramReg(op1,0xF));

                // if(DECODE_REG_TYPE(op2) != BC_DX){
                //     prog->add(OPCODE_POP_RM_SLASH_0);
                //     prog->addModRM(MODE_REG, 0, REG_D);
                // }
                // if(DECODE_REG_TYPE(op2) != BC_AX){
                //     if(size == 8)
                //         prog->add(PREFIX_REXW);
                //     prog->add(OPCODE_MOV_REG_RM);
                //     prog->addModRM(MODE_REG, BCToProgramReg(op2), REG_A);
                //     prog->add(OPCODE_POP_RM_SLASH_0);
                //     prog->addModRM(MODE_REG, 0, REG_A);
                // }

                break;
            }
            break; case BC_MODI: {
                u8 type = op0;
                u8 in0 = BCToProgramReg(op1, 0xF);
                u8 in1 = BCToProgramReg(op2, 0xF);
                u8 out = in1;
                u8 size = DECODE_REG_SIZE(op2);
                
                Assert(in0 == REG_A && in1 == REG_D && out == REG_D);
                
                prog->add(OPCODE_MOV_REG_RM);
                prog->addModRM(MODE_REG, REG_SI, in1);
                in1 = REG_SI;
                
                prog->add(PREFIX_REXW);
                prog->add(OPCODE_XOR_REG_RM);
                prog->addModRM(MODE_REG, REG_D, REG_D);
    
                u8 extra = type == ARITHMETIC_SINT ? 7 : 6;
                u8 opc = size == 1 ? OPCODE_IDIV_AX_RM8_SLASH_7 : OPCODE_DIV_AX_RM_SLASH_6;
                
                if(size == 2)
                    prog->add(PREFIX_16BIT);
                if(size == 8)
                    prog->add(PREFIX_REXW);
                if(size == 1)
                    prog->add(PREFIX_REX); // enables usage of low part of SI (SIL)
                
                prog->add(opc);
                prog->addModRM(MODE_REG, extra, in1);
                if(size == 1) {
                    prog->add(OPCODE_MOV_REG8_RM);
                    prog->addModRM(MODE_REG, out, 0b100); // mov dl, ah
                }
                
                // Assert(DECODE_REG_TYPE(op0) == BC_AX &&
                //     DECODE_REG_TYPE(op1) != BC_AX &&
                //     DECODE_REG_TYPE(op1) != BC_DX);

                // u8 size = DECODE_REG_SIZE(op2);
                // if(DECODE_REG_TYPE(op2) != BC_AX){
                //     prog->add(OPCODE_PUSH_RM_SLASH_6);
                //     prog->addModRM(MODE_REG, 6, REG_A);
                // }
                
                // if(DECODE_REG_TYPE(op2) != BC_DX){
                //     prog->add(OPCODE_PUSH_RM_SLASH_6);
                //     prog->addModRM(MODE_REG, 6, REG_D);
                // }

                // // I CANNOT USE EDX, EAX
                // // op0 if eax do nothing, just sign extend, otherwise move op0 into eax
                // // 
                // // op2 can be whatever, move result to it at the end

                // if(DECODE_REG_TYPE(op0)!=BC_AX){
                //     // move reg in op0 to eax
                //     if(size == 8)
                //         prog->add(PREFIX_REXW);
                //     prog->add(OPCODE_MOV_REG_RM);
                //     prog->addModRM(MODE_REG, REG_A, BCToProgramReg(op0));
                // }
                // if(size == 8)
                //     prog->add(PREFIX_REXW);
                // prog->add(OPCODE_CDQ); // sign extend eax into edx

                // if(size == 8)
                //     prog->add(PREFIX_REXW);
                // prog->add(OPCODE_IDIV_AX_RM_SLASH_7);
                // prog->addModRM(MODE_REG, 7, BCToProgramReg(op1));

                // if(DECODE_REG_TYPE(op2) != BC_DX){
                //     if(size == 8)
                //         prog->add(PREFIX_REXW);
                //     prog->add(OPCODE_MOV_REG_RM);
                //     prog->addModRM(MODE_REG, BCToProgramReg(op2), REG_D);
                //     prog->add(OPCODE_POP_RM_SLASH_0);
                //     prog->addModRM(MODE_REG, 0, REG_D);
                // }
                // if(DECODE_REG_TYPE(op2) != BC_AX){
                //     prog->add(OPCODE_POP_RM_SLASH_0);
                //     prog->addModRM(MODE_REG, 0, REG_A);
                // }

                break;
            }
            break; case BC_FMOD: {
                Assert((op0 == BC_REG_XMM0f && op1 == BC_REG_XMM1f  && op2 == BC_REG_XMM2f) ||
                    (op0 == BC_REG_XMM0d && op1 == BC_REG_XMM1d  && op2 == BC_REG_XMM2d)
                );

                // out = x - rounddown(x / y) * y
                // xmm2 = xmm0 - rounddown(xmm0 / xmm1) * xmm1

                if(op0 == BC_REG_XMM0f) { // float, f32
                    /*
                        movss xmm2, xmm0
                        divss xmm2, xmm1
                        roundss xmm2, xmm2, 1
                        mulss xmm2, xmm1
                        subss xmm0, xmm2
                        movss xmm2, xmm0
                    */
                    // OPCODE_3_MOVSS_REG_RM
                    // OPCODE_3_ROUNDSS_REG_RM
                    // OPCODE_3_DIVSS_REG_RM
                    // OPCODE_3_MULSS_REG_RM
                    // OPCODE_3_SUBSS_REG_RM
                    u8 arr[]{ 0xF3, 0x0F, 0x10, 0xD0, 0xF3, 0x0F, 0x5E, 0xD1, 0x66, 0x0F, 0x3A, 0x0A, 0xD2, 0x01, 0xF3, 0x0F, 0x59, 0xD1, 0xF3, 0x0F, 0x5C, 0xC2, 0xF3, 0x0F, 0x10, 0xD0 };
                    prog->addRaw(arr,sizeof(arr));
                } else { // double, f64
                    u8 arr[] { 0xF2, 0x0F, 0x10, 0xD0, 0xF2, 0x0F, 0x5E, 0xD1, 0x66, 0x0F, 0x3A, 0x0B, 0xD2, 0x01, 0xF2, 0x0F, 0x59, 0xD1, 0xF2, 0x0F, 0x5C, 0xC2, 0xF2, 0x0F, 0x10, 0xD0 };
                    prog->addRaw(arr,sizeof(arr));
                }
                break;
            }
            break; case BC_FSUB:
            case BC_FMUL:
            case BC_FDIV:
            case BC_FADD: {
                u8 size0 = DECODE_REG_SIZE(op0);
                Assert(IS_REG_XMM(op0) && IS_REG_XMM(op1) && op0 == op2 
                    && size0 == DECODE_REG_SIZE(op1)
                    && size0 == DECODE_REG_SIZE(op2));

                
                u8 reg0 = BCToProgramReg(op0, 4|8, true);
                u8 reg1 = BCToProgramReg(op1, 4|8, true);
                u8 reg2 = reg0;

                u32 operation = 0;
                switch(size0) {
                case 4: {
                    switch(opcode) {
                        case BC_FADD: operation = OPCODE_3_ADDSS_REG_RM; break;
                        case BC_FSUB: operation = OPCODE_3_SUBSS_REG_RM; break;
                        case BC_FMUL: operation = OPCODE_3_MULSS_REG_RM; break;
                        case BC_FDIV: operation = OPCODE_3_DIVSS_REG_RM; break;
                    }
                    break;
                }
                case 8: {
                    switch(opcode) {
                        case BC_FADD: operation = OPCODE_3_ADDSD_REG_RM; break;
                        case BC_FSUB: operation = OPCODE_3_SUBSD_REG_RM; break;
                        case BC_FMUL: operation = OPCODE_3_MULSD_REG_RM; break;
                        case BC_FDIV: operation = OPCODE_3_DIVSD_REG_RM; break;
                    }
                    break;
                }
                default: {
                    Assert(("float operations only work on single or double precision. Operans are of a different size. Compiler bug?",false));
                    break;
                }
                }
                prog->add3(operation);
                prog->addModRM(MODE_REG, reg2, reg1);

                // Code that works with eax instead of xmm
                // u8 reg0 = BCToProgramReg(op0, 4);
                // u8 reg1 = BCToProgramReg(op1, 4);
                // prog->add(OPCODE_MOV_RM_REG);
                // prog->addModRM_SIB(MODE_DEREF_DISP8, reg0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                // prog->add((u8)-8);

                // prog->add3(OPCODE_3_MOVSS_REG_RM);
                // prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                // prog->add((u8)-8);

                // prog->add(OPCODE_MOV_RM_REG);
                // prog->addModRM_SIB(MODE_DEREF_DISP8, reg1, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                // prog->add((u8)-8);

                // u32 operation = 0;
                // switch(opcode) {
                //     case BC_FADD: operation = OPCODE_3_ADDSS_REG_RM; break;
                //     case BC_FSUB: operation = OPCODE_3_SUBSS_REG_RM; break;
                //     case BC_FMUL: operation = OPCODE_3_MULSS_REG_RM; break;
                //     case BC_FDIV: operation = OPCODE_3_DIVSS_REG_RM; break;
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
            }
            break; case BC_EQ: {
                Assert(op0 == op2 || op1 == op2);
                u8 op01 = op0 == op2 ? op1 : op0;
                u8 size = DECODE_REG_SIZE(op2);
                // TODO: Test if this is okay? is cmp better?
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
                // IMPORTANT: THERE MAY BE BUGS IF YOU COMPARE OPERANDS OF DIFFERENT SIZES.
                //  SOME BITS MAY BE UNINITIALZIED.
                u8 size = DECODE_REG_SIZE(op2);
                u8 size2 = DECODE_REG_SIZE(op01);
                // Assert(size == size2);
                if(size == 8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_XOR_REG_RM);
                prog->addModRM(MODE_REG, BCToProgramReg(op2,0xF), BCToProgramReg(op01,0xF));
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

                int temp = 0xF;

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
                // prog->add(PREFIX_REXW);
                // prog->add(OPCODE_XOR_REG_RM);
                // prog->addModRM(MODE_REG,BCToProgramReg(op2,0xC),BCToProgramReg(op2,0xC));
            
            break;
            case BC_LT:
            case BC_LTE:
            case BC_GT:
            case BC_GTE: {
                u8 type = op0;

                u8 size1 = DECODE_REG_SIZE(op1);
                u8 size2 = DECODE_REG_SIZE(op2);
                
                // NOTE: I was about to make R registers available for this instruction
                //  But then I realized it wasn't necessary. The code disabled with "newstuff"
                //  has not been tested. But if you need R registers in the future then most of
                //  it has been implemented.

                u8 prefix_op1 = 0;
                u8 prefix_op2 = 0;
                // #define newstuff
                #ifdef newstuff
                u8 reg1 = BCToProgramReg(op1,0xF, false, true);
                u8 reg2 = BCToProgramReg(op2,0xF, false, true);
                bool extend_op1 = IS_REG_RX(op1);
                bool extend_op2 = IS_REG_RX(op2);
                if(extend_op1)
                    prefix_op1 = PREFIX_REXB | PREFIX_REXR;
                if(extend_op2)
                    prefix_op2 = PREFIX_REXB | PREFIX_REXR;
                #else
                u8 reg1 = BCToProgramReg(op1,0xF);
                u8 reg2 = BCToProgramReg(op2,0xF);
                #endif

                if(size1>size2){
                    if(size2==1) {
                        prog->add((u8)(PREFIX_REXW | prefix_op2));
                        prog->add2(OPCODE_2_MOVSX_REG_RM8); // DON'T SIGN EXTEND IF UNSIGNED!
                        prog->addModRM(MODE_REG, reg2, reg2);
                    } else if(size2==2) {
                        prog->add((u8)(PREFIX_REXW | prefix_op2));
                        prog->add2(OPCODE_2_MOVSX_REG_RM16);
                        prog->addModRM(MODE_REG, reg2, reg2);
                    } else if(size2==4) {
                        prog->add((u8)(PREFIX_REXW| prefix_op2));
                        prog->add(OPCODE_MOVSXD_REG_RM);
                        prog->addModRM(MODE_REG, reg2, reg2);
                    }
                }else if(size2>size1) {
                    if(size1==1) {
                        prog->add((u8)(PREFIX_REXW| prefix_op1));
                        prog->add2(OPCODE_2_MOVSX_REG_RM8);
                        prog->addModRM(MODE_REG, reg1, reg1);
                    } else if(size1==2) {
                        prog->add((u8)(PREFIX_REXW| prefix_op1));
                        prog->add2(OPCODE_2_MOVSX_REG_RM16);
                        prog->addModRM(MODE_REG, reg1, reg1);\
                    } else if(size1==4) {
                        prog->add((u8)(PREFIX_REXW| prefix_op1));
                        prog->add(OPCODE_MOVSXD_REG_RM);
                        prog->addModRM(MODE_REG, reg1, reg1);
                    }
                }
                #ifdef newstuff
                u8 prefix = 0;
                if(size1 == 8 || size2 == 8)
                    prefix |= PREFIX_REXW;
                if(extend_op1)
                    prefix |= PREFIX_REXR;
                if(extend_op2)
                    prefix |= PREFIX_REXB;

                if(prefix!=0)
                    prog->add(prefix);
#else
                if(size1 == 8 || size2 == 8)
                    prog->add(PREFIX_REXW);
                #endif
                prog->add(OPCODE_CMP_REG_RM);
                prog->addModRM(MODE_REG, reg1, reg2);

                u16 setType = 0;
                switch(type){
                    case CMP_SINT_SINT: {
                        switch(opcode) {
                            case BC_LT: {
                                setType = OPCODE_2_SETL_RM8;
                                break;
                            }
                            case BC_LTE: {
                                setType = OPCODE_2_SETLE_RM8;
                                break;
                            }
                            case BC_GT: {
                                setType = OPCODE_2_SETG_RM8;
                                break;
                            }
                            case BC_GTE: {
                                setType = OPCODE_2_SETGE_RM8;
                                break;
                            }
                        }
                        break;
                    }
                    case CMP_UINT_UINT: {
                        switch(opcode) {
                            case BC_LT: {
                                setType = OPCODE_2_SETB_RM8;
                                break;
                            }
                            case BC_LTE: {
                                setType = OPCODE_2_SETBE_RM8;
                                break;
                            }
                            case BC_GT: {
                                setType = OPCODE_2_SETA_RM8;
                                break;
                            }
                            case BC_GTE: {
                                setType = OPCODE_2_SETAE_RM8;
                                break; 
                            }
                        }
                        break; 
                    }
                    // These are a little special.
                    case CMP_UINT_SINT:
                    case CMP_SINT_UINT: {
                        switch(opcode) {
                            case BC_LT: {
                                setType = OPCODE_2_SETB_RM8; 
                                break; 
                            }
                            case BC_LTE: {
                                setType = OPCODE_2_SETBE_RM8;
                                break; 
                            }
                            case BC_GT: {
                                setType = OPCODE_2_SETA_RM8;
                                break;
                            }
                            case BC_GTE: {
                                setType = OPCODE_2_SETAE_RM8;
                                break;
                            }
                        }
                        break;
                    }
                }
                #ifdef newstuff
                prefix = 0;
                if(extend_op2)
                    prefix |= PREFIX_REXB;
                if(prefix!=0)
                    prog->add(prefix);
#endif
                Assert(setType!=0);
                prog->add2(setType);
                prog->addModRM(MODE_REG, 0, reg2);
      #ifdef newstuff          
                prefix = 0;
                if(size2 == 8)
                    prefix |= PREFIX_REXW;
                if(extend_op2)
                    prefix |= PREFIX_REXB | PREFIX_REXR;
                if(prefix!=0)
                    prog->add(prefix);
#else
                prog->add(PREFIX_REXW);
                #endif
                prog->add2(OPCODE_2_MOVZX_REG_RM8);
                prog->addModRM(MODE_REG, reg2, reg2);
                break;
            }
            case BC_FLT:
            case BC_FLTE:
            case BC_FGT:
            case BC_FGTE:
            case BC_FEQ:
            case BC_FNEQ: {
                Assert((op0 == BC_REG_XMM0f && op1 == BC_REG_XMM1f) ||
                    (op0 == BC_REG_XMM0d && op1 == BC_REG_XMM1d)
                );

                u8 reg0 = BCToProgramReg(op0,4|8, true);
                u8 reg1 = BCToProgramReg(op1,4|8, true);
                u8 reg2 = BCToProgramReg(op2,0xF);

                u8 size0 = DECODE_REG_SIZE(op0);
                u8 size2 = DECODE_REG_SIZE(op2);
                
                // NOTE: UCOMISS is another instructions you could use.
                // It depends if you care about ordered or unordered (NaN comparison)
                // I am not sure how we care about it at the moment so we always use ordered (COMISS)
                if(size0 == 4) {
                    prog->add2(OPCODE_2_COMISS_REG_RM);
                } else {
                    prog->add3(OPCODE_3_COMISD_REG_RM);
                }
                prog->addModRM(MODE_REG, reg0, reg1);
                
                switch(opcode) {
                    break; case BC_FLT: {
                        prog->add2(OPCODE_2_SETB_RM8);
                    }
                    break; case BC_FLTE: {
                        prog->add2(OPCODE_2_SETBE_RM8);
                    }
                    break; case BC_FGT: {
                        prog->add2(OPCODE_2_SETA_RM8);
                    }
                    break; case BC_FGTE: {
                        prog->add2(OPCODE_2_SETAE_RM8);
                    }
                    break; case BC_FEQ: {
                        prog->add2(OPCODE_2_SETE_RM8);
                    }
                    break; case BC_FNEQ: {
                        prog->add2(OPCODE_2_SETNE_RM8);
                    }
                    break; default: {
                        Assert(false);
                    }
                }
                prog->addModRM(MODE_REG, 0, reg2);
                if(size2 == 8)
                    prog->add(PREFIX_REXW);
                prog->add2(OPCODE_2_MOVZX_REG_RM8);
                prog->addModRM(MODE_REG, reg2, reg2);
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

                u8 size = DECODE_REG_SIZE(op1);
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
                // if(size == 8)
                prog->add(PREFIX_REXW);
                prog->add(OPCODE_SHL_RM_CL_SLASH_4);
                prog->addModRM(MODE_REG, 4, BCToProgramReg(op2,0xF));

                // prog->add(OPCODE_AND_RM_IMM_SLASH_4)
                // prog->add(OPCODE_2_MOVZX_REG_RM8);
                // prog->add(OPCODE_2_MOVZX_REG_RM16);

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
                bool extend_op0 = IS_REG_RX(op0);
                bool xmm_op0 = IS_REG_XMM(op0);
                u8 reg0 = BCToProgramReg(op0,0xF, xmm_op0, extend_op0);
                if(xmm_op0) {
                    if (op1 == 1) {
                        u8 size0 = DECODE_REG_SIZE(op0);
                        Assert(size0 == 4);

                        prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, 0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);
                        prog->add4((u32)imm);

                        prog->add3(OPCODE_3_MOVSS_REG_RM);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, reg0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);
                    } else if(op1 == 2) {
                        u8 size0 = DECODE_REG_SIZE(op0);
                        Assert(size0 == 8);

                        // TODO: Use movabs instruction
                        prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, 0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);
                        prog->add4((u32)imm);

                        prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, 0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-4);
                        prog->add4((u32)imm2);

                        prog->add3(OPCODE_3_MOVSD_REG_RM);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, reg0, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);
                    } else {
                        Assert(false);
                    }
                } else {
                    u8 size0 = DECODE_REG_SIZE(op0);
                    u8 rex = 0;
                    if(size0 == 8)
                        rex |= PREFIX_REXW;
                    if(imm == 0 && imm2 == 0){
                        if(extend_op0)
                            prog->add((u8)(rex | PREFIX_REXB | PREFIX_REXR));
                        else if(rex!=0)
                            prog->add((u8)(rex));
                        prog->add(OPCODE_XOR_REG_RM);
                        prog->addModRM(MODE_REG, reg0, reg0);
                    } else {
                        if(op1 == 2) {
                            Assert(size0 == 8);
                            u8 regField = -1;
                            // see chapter 3.1.1.1, table 3-1
                            switch(op0){
                                case BC_REG_RAX: regField = 0; break;
                                case BC_REG_RCX: regField = 1; break;
                                case BC_REG_RDX: regField = 2; break;
                                case BC_REG_RBX: regField = 3; break;
                                case BC_REG_SP: regField = 4; break;
                                case BC_REG_FP: regField = 5; break;
                                case BC_REG_RSI: regField = 6; break;
                                case BC_REG_RDI: regField = 7;  break;
                                // TODO: r8, r9, ...
                            }
                            Assert(regField!=(u8)-1);

                            prog->add(PREFIX_REXW);
                            prog->add((u8)(OPCODE_MOV_REG_IMM_RD_IO | regField));
                            prog->add4((u32)imm);
                            prog->add4((u32)imm2);
                        } else {
                            // IMPORTANT: SIGN EXTENSION WILL OCCUR ON IMMEDIATE
                            //  WITH 64-bit OPERAND! THIS MAY NOT BE WANTED.
                            //  THE BYTECODE INSTRUCTION NEED TO TELL US WHETHER IT IS
                            //  SIGNED OR NOT!
                            if(size0 != 8 && size0 != 4){
                                if(extend_op0)
                                    prog->add((u8)(PREFIX_REXW | PREFIX_REXB | PREFIX_REXR));
                                else
                                    prog->add((u8)(PREFIX_REXW));
                                prog->add(OPCODE_XOR_REG_RM);
                                prog->addModRM(MODE_REG, reg0, reg0);
                            }
                            if(size0 == 8){
                                if(extend_op0)
                                    prog->add((u8)(PREFIX_REXW | PREFIX_REXB));
                                else
                                    prog->add((u8)(PREFIX_REXW));
                            }
                            prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                            prog->addModRM(MODE_REG, 0, reg0);
                            prog->add4((u32)imm); // NOTE: imm is i32 while add4 is u32, should be converted though
                        }
                    }
                }
                break;
            }
            break; case BC_DATAPTR: {
                u8 size = DECODE_REG_SIZE(op0);
                Assert(size==8);

                prog->add(PREFIX_REXW);
                prog->add(OPCODE_LEA_REG_M);
                prog->addModRM_rip(BCToProgramReg(op0,8), (u32)0);
                Program_x64::DataRelocation reloc{};
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

                    int size0 = DECODE_REG_SIZE(op0);
                    
                    prog->add(PREFIX_REXW);
                    prog->add(OPCODE_ADD_RM_IMM8_SLASH_0);
                    prog->addModRM(MODE_REG, 0, REG_SP);
                    prog->add((u8)-8);

                    switch(size0) {
                    case 4: {
                        prog->add3(OPCODE_3_MOVSS_RM_REG);
                        prog->addModRM_SIB(MODE_DEREF, reg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        break;
                    }
                    case 8:{
                        prog->add3(OPCODE_3_MOVSD_RM_REG);
                        prog->addModRM_SIB(MODE_DEREF, reg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        break;
                    }
                    default: {
                        Assert(("operand was not the size of a float or double",false));
                    }
                    }
                } else {
                    int size = DECODE_REG_SIZE(op0);
                    // Shouldn't need this if the register was properly zero/sign extended to begin with
                    // #ifndef ENABLE_FAULTY_X64
                    // if(size == 4){
                    //     // prog->add(PREFIX_REXW);
                    //     // rexw will sign extend the immediate so we can't actually
                    //     // do and here with just one instruction (I think).
                    //     // prog->add(OPCODE_AND_RM_IMM_SLASH_4);
                    //     // prog->addModRM(MODE_REG, 4, BCToProgramReg(op0,0xF));
                    //     // prog->add4((u32)0xFFFFFFFF);
                    // } else if(size == 2) {
                    //     prog->add(PREFIX_REXW);
                    //     prog->add(OPCODE_AND_RM_IMM_SLASH_4);
                    //     prog->addModRM(MODE_REG, 4, BCToProgramReg(op0,0xF));
                    //     prog->add4((u32)0xFFFF);
                    // }else if(size == 1) {
                    //     prog->add(PREFIX_REXW);
                    //     prog->add(OPCODE_AND_RM_IMM_SLASH_4);
                    //     prog->addModRM(MODE_REG, 4, BCToProgramReg(op0,0xF));
                    //     prog->add4((u32)0xFF);
                    // }
                    // #endif

                    prog->add(OPCODE_PUSH_RM_SLASH_6);
                    prog->addModRM(MODE_REG, 6,BCToProgramReg(op0,0xF)); // 0xF = 1|2|4|8, meaning all sizes
                }
                break;
            }
            break; case BC_POP: {
                if(IS_REG_XMM(op0)) {
                    u8 reg = BCToProgramReg(op0, 4|8, true);
                    int size0 = DECODE_REG_SIZE(op0);
                    
                    switch(size0) {
                    case 4: {
                        prog->add3(OPCODE_3_MOVSS_REG_RM);
                        prog->addModRM_SIB(MODE_DEREF, reg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        break;
                    }
                    case 8:{
                        prog->add3(OPCODE_3_MOVSD_REG_RM);
                        prog->addModRM_SIB(MODE_DEREF, reg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        break;
                    }
                    default: {
                        Assert(("operand was not the size of a float or double",false));
                    }
                    }
                    
                    prog->add(PREFIX_REXW);
                    prog->add(OPCODE_ADD_RM_IMM8_SLASH_0);
                    prog->addModRM(MODE_REG, 0, REG_SP);
                    prog->add((u8)8);
                } else {
                    u8 size = DECODE_REG_SIZE(op0);
                    bool extend_op0 = IS_REG_RX(op0);
                    u8 reg = BCToProgramReg(op0,0xF,false, extend_op0);
                    #ifndef ENABLE_FAULTY_X64
                    if(size != 8 && size != 4){
                        if(extend_op0)
                            prog->add((u8)(PREFIX_REXW|PREFIX_REXB));
                        else
                            prog->add(PREFIX_REXW);
                        prog->add(OPCODE_XOR_REG_RM);
                        prog->addModRM(MODE_REG, reg,reg);
                    }
                    #endif
                    if(extend_op0)
                        prog->add(PREFIX_REXB);
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
            break; case BC_JZ: {
                i16 offset = (i16)((i16)op1| ((i16)op2<<8));

                // TODO: Replace with a better instruction? If there is one.
                u8 size = DECODE_REG_SIZE(op0);

                bool extend_op0 = IS_REG_RX(op0);

                u8 reg0 = BCToProgramReg(op0,0xF, false, true);

                u8 prefix = 0;
                if(size==8)
                    prefix |= PREFIX_REXW;
                if(extend_op0)
                    prefix |= PREFIX_REXB;
                if(prefix!=0)
                    prog->add(prefix);
                prog->add(OPCODE_CMP_RM_IMM8_SLASH_7);
                prog->addModRM(MODE_REG, 7, reg0);
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
            break; case BC_JNZ: {
                i16 offset = (i16)((i16)op1| ((i16)op2<<8));

                // TODO: Replace with a better instruction? If there is one.
                u8 size = DECODE_REG_SIZE(op0);

                if(size==8)
                    prog->add(PREFIX_REXW);
                prog->add(OPCODE_CMP_RM_IMM8_SLASH_7);
                prog->addModRM(MODE_REG, 7, BCToProgramReg(op0,0xF));
                prog->add((u8)0);
                
                prog->add2(OPCODE_2_JNE_IMM32);
                prog->add4((u32)0);
                
                RelativeRelocation reloc{};
                reloc.currentIP = prog->size();
                reloc.bcAddress = imm;
                reloc.immediateToModify = prog->size()-4;
                relativeRelocations.add(reloc);
                break;
            }
            case BC_CAST: {
                u8 type = op0;

                int fsize = DECODE_REG_SIZE(op1);
                int tsize = DECODE_REG_SIZE(op2);

                bool fromXmm = IS_REG_XMM(op1);
                bool toXmm = IS_REG_XMM(op2);

                u8 freg = BCToProgramReg(op1, 0xF, fromXmm);
                u8 treg = BCToProgramReg(op2, 0xF, toXmm);
                // Assert(freg == treg); // Does this make sense? What about casting eax into xmm0?

                u8 minSize = fsize < tsize ? fsize : tsize;

                if(type==CAST_FLOAT_SINT){
                    Assert(fsize == 4 || fsize == 8);
                    Assert(!toXmm);

                    if(!fromXmm) {
                        if(fsize == 8)
                            prog->add(PREFIX_REXW);
                        prog->add(OPCODE_MOV_RM_REG);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, freg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);
                    }
                    if        (fsize == 4 && tsize == 4) {
                        prog->add3(OPCODE_3_CVTTSS2SI_REG_RM);
                    } else if (fsize == 4 && tsize == 8) {
                        prog->add4(OPCODE_4_REXW_CVTTSS2SI_REG_RM);
                    } else if (fsize == 8 && tsize == 4) {
                        prog->add3(OPCODE_3_CVTTSD2SI_REG_RM);
                    } else if (fsize == 8 && tsize == 8) {
                        prog->add4(OPCODE_4_REXW_CVTTSD2SI_REG_RM);
                    } else {
                        Assert(false);
                    }
                    if(fromXmm) {
                        prog->addModRM(MODE_REG, treg, freg);
                    } else {
                        prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);
                    }
                } else if(type==CAST_FLOAT_UINT){
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
                        prog->addModRM(MODE_REG, treg, freg);
                    } else {
                        prog->add(OPCODE_MOV_RM_REG);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, freg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);

                        if(tsize == 8)
                            prog->add4(OPCODE_4_REXW_CVTTSS2SI_REG_RM);
                        else
                            prog->add3(OPCODE_3_CVTTSS2SI_REG_RM);

                        prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);
                    }
                } else if(type==CAST_SINT_FLOAT){
                    Assert(tsize == 4 || tsize == 8);
                    Assert(!fromXmm);
                    if(fsize == 1){
                        Assert(false); // we might need to sign extend
                        // prog->add(PREFIX_REXW);
                        // prog->add(OPCODE_AND_RM_IMM_SLASH_4);
                        // prog->addModRM(MODE_REG, 4, freg);
                        // prog->add4((u32)0xFF);
                        if(tsize == 4)
                            prog->add3(OPCODE_3_CVTSI2SS_REG_RM);
                        else
                            prog->add3(OPCODE_3_CVTSI2SD_REG_RM);
                    } else if(fsize == 2){
                        Assert(false); // we might need to sign extend
                        // prog->add(PREFIX_REXW);
                        // prog->add(OPCODE_AND_RM_IMM_SLASH_4);
                        // prog->addModRM(MODE_REG, 4, freg);
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
                        prog->addModRM(MODE_REG, treg, freg);
                        // prog->add(OPCODE_NOP);
                    } else {
                        // BUG: Hoping that xmm7 isn't used is considered bad programming.
                        prog->addModRM(MODE_REG, REG_XMM7, freg);

                        if(tsize == 4)
                            prog->add3(OPCODE_3_MOVSS_RM_REG);
                        else
                            prog->add3(OPCODE_3_MOVSD_RM_REG);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM7, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);

                        if(tsize == 8)
                            prog->add(PREFIX_REXW);
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);
                    }
                } else if(type==CAST_UINT_FLOAT){
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
                            prog->add(OPCODE_PUSH_RM_SLASH_6);
                            prog->addModRM(MODE_REG, 6, temp_reg);

                            // Check signed bit
                            prog->add(PREFIX_REXW);
                            prog->add(OPCODE_TEST_RM_REG);
                            prog->addModRM(MODE_REG, from_reg, from_reg);

                            prog->add(OPCODE_JS_REL8);
                            int jmp_offset_unsigned = prog->size();
                            prog->add((u8)0); // set later

                            // Normal conversion since the signed bit is off
                            prog->add3(OPCODE_3_PXOR_RM_REG);
                            prog->addModRM(MODE_REG, xmm_reg, xmm_reg);

                            prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                            prog->addModRM(MODE_REG, xmm_reg, from_reg);

                            prog->add(OPCODE_JMP_IMM8);
                            int jmp_offset_end = prog->size();
                            prog->add((u8)0);

                            prog->set(jmp_offset_unsigned, prog->size() - (jmp_offset_unsigned +1)); // +1 gives the offset after the jump instruction (immediate is one byte)
                            
                            // Special conversion because the signed bit is on but since our conversion is unsigned
                            // we need some special stuff so that our float isn't negative and half the number we except

                            prog->add(PREFIX_REXW);
                            prog->add(OPCODE_MOV_REG_RM);
                            prog->addModRM(MODE_REG, temp_reg, from_reg);

                            prog->add(PREFIX_REXW);    
                            prog->add(OPCODE_SHR_RM_ONCE_SLASH_5);
                            prog->addModRM(MODE_REG, 5, temp_reg);

                            prog->add(OPCODE_AND_RM_IMM8_SLASH_4);
                            prog->addModRM(MODE_REG, 4, from_reg);
                            prog->add((u8)1);

                            prog->add(PREFIX_REXW);
                            prog->add(OPCODE_OR_RM_REG);
                            prog->addModRM(MODE_REG, from_reg, temp_reg);

                            prog->add3(OPCODE_3_PXOR_RM_REG);
                            prog->addModRM(MODE_REG, xmm_reg, xmm_reg);

                            prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                            prog->addModRM(MODE_REG, xmm_reg, temp_reg);

                            prog->add3(OPCODE_3_ADDSS_REG_RM);
                            prog->addModRM(MODE_REG, xmm_reg, xmm_reg);

                            prog->set(jmp_offset_end, prog->size() - (jmp_offset_end +1));

                            prog->add(OPCODE_POP_RM_SLASH_0);
                            prog->addModRM(MODE_REG, 0, temp_reg);

                            if(!toXmm) {
                                prog->add3(OPCODE_3_MOVSS_RM_REG);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, xmm_reg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                prog->add((u8)-8);

                                prog->add(OPCODE_MOV_REG_RM);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                prog->add((u8)-8);
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
                            prog->add(OPCODE_PUSH_RM_SLASH_6);
                            prog->addModRM(MODE_REG, 6, temp_reg);

                            // Check signed bit
                            prog->add(PREFIX_REXW);
                            prog->add(OPCODE_TEST_RM_REG);
                            prog->addModRM(MODE_REG, from_reg, from_reg);

                            prog->add(OPCODE_JS_REL8);
                            int jmp_offset_unsigned = prog->size();
                            prog->add((u8)0); // set later

                            // Normal conversion since the signed bit is off
                            prog->add3(OPCODE_3_PXOR_RM_REG);
                            prog->addModRM(MODE_REG, xmm_reg, xmm_reg);

                            prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                            prog->addModRM(MODE_REG, xmm_reg, from_reg);

                            prog->add(OPCODE_JMP_IMM8);
                            int jmp_offset_end = prog->size();
                            prog->add((u8)0);

                            prog->set(jmp_offset_unsigned, prog->size() - (jmp_offset_unsigned +1)); // +1 gives the offset after the jump instruction (immediate is one byte)
                            
                            // Special conversion because the signed bit is on but since our conversion is unsigned
                            // we need some special stuff so that our float isn't negative and half the number we except

                            prog->add(PREFIX_REXW);
                            prog->add(OPCODE_MOV_REG_RM);
                            prog->addModRM(MODE_REG, temp_reg, from_reg);

                            prog->add(PREFIX_REXW);    
                            prog->add(OPCODE_SHR_RM_ONCE_SLASH_5);
                            prog->addModRM(MODE_REG, 5, temp_reg);

                            prog->add(OPCODE_AND_RM_IMM8_SLASH_4);
                            prog->addModRM(MODE_REG, 4, from_reg);
                            prog->add((u8)1);

                            prog->add(PREFIX_REXW);
                            prog->add(OPCODE_OR_RM_REG);
                            prog->addModRM(MODE_REG, from_reg, temp_reg);

                            prog->add3(OPCODE_3_PXOR_RM_REG);
                            prog->addModRM(MODE_REG, xmm_reg, xmm_reg);

                            prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                            prog->addModRM(MODE_REG, xmm_reg, temp_reg);

                            prog->add3(OPCODE_3_ADDSD_REG_RM);
                            prog->addModRM(MODE_REG, xmm_reg, xmm_reg);

                            prog->set(jmp_offset_end, prog->size() - (jmp_offset_end +1));

                            prog->add(OPCODE_POP_RM_SLASH_0);
                            prog->addModRM(MODE_REG, 0, temp_reg);

                            if(!toXmm) {
                                prog->add3(OPCODE_3_MOVSD_RM_REG);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, xmm_reg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                prog->add((u8)-8);

                                prog->add(PREFIX_REXW);
                                prog->add(OPCODE_MOV_REG_RM);
                                prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                                prog->add((u8)-8);
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
                            prog->addModRM(MODE_REG, treg, freg);
                        } else {
                            prog->addModRM(MODE_REG, REG_XMM7, freg);

                            if(tsize==4)
                                prog->add3(OPCODE_3_MOVSS_RM_REG);
                            else
                                prog->add3(OPCODE_3_MOVSD_RM_REG);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM7, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);

                            if(tsize==8)
                                prog->add(PREFIX_REXW);
                            prog->add(OPCODE_MOV_REG_RM);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);
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

                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_TEST_RM_REG);
                        prog->addModRM(MODE_REG, freg, freg);

                        prog->add(OPCODE_JL_REL8);
                        u32 jumpSign = prog->size();
                        prog->add((u8)0);

                        // prog->add((u8)0xF3);
                        // prog->add((u8)(PREFIX_REXW));
                        // prog->add2((u16)0x2A0F);
                        if(tsize == 4) {
                            prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                        } else {
                            prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                        }
                        prog->addModRM(MODE_REG, xmm, freg);

                        prog->add(OPCODE_JMP_IMM8);
                        u32 jumpEnd = prog->size();
                        prog->add((u8)0);

                        prog->set(jumpSign, prog->size() - (jumpSign+1)); // +1 because jumpSign points to the byte to change in the jump instruction, we want the end of the instruction.

                        // prog->add((u8)(PREFIX_REXW|PREFIX_REXR));
                        prog->add((u8)(PREFIX_REXR)); // 64-bit operands not needed, we just want 1 bit from freg
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM(MODE_REG, tempReg, freg);

                        prog->add((u8)(PREFIX_REXW|PREFIX_REXB));
                        prog->add(OPCODE_AND_RM_IMM8_SLASH_4);
                        prog->addModRM(MODE_REG, 4, tempReg);
                        prog->add((u8)1);


                        prog->add((u8)(PREFIX_REXW));
                        prog->add(OPCODE_SHR_RM_IMM8_SLASH_5);
                        prog->addModRM(MODE_REG, 5, freg);
                        prog->add((u8)1);

                        prog->add((u8)(PREFIX_REXW|PREFIX_REXB));
                        prog->add(OPCODE_OR_RM_REG);
                        prog->addModRM(MODE_REG, freg, tempReg);
                        
                        // F3 REX.W 0F 2A  <- the CVTSI2SS_REG_RM requires the rex byte smashed inside it
                        if(tsize == 4) {
                            prog->add((u8)0xF3);
                            prog->add((u8)(PREFIX_REXW|PREFIX_REXB));
                            prog->add2((u16)0x2A0F);
                        } else {
                            prog->add((u8)0xF2);
                            prog->add((u8)(PREFIX_REXW|PREFIX_REXB));
                            prog->add2((u16)0x2A0F);
                        }
                        prog->addModRM(MODE_REG, xmm, tempReg);
                        
                        prog->add((u8)(PREFIX_REXW|PREFIX_REXB|PREFIX_REXR));
                        prog->add(OPCODE_XOR_REG_RM);
                        prog->addModRM(MODE_REG, tempReg, tempReg);

                        // TODO: REXW may be needed here
                        prog->add(PREFIX_REXB);
                        prog->add(OPCODE_MOV_RM_IMM8_SLASH_0);
                        prog->addModRM(MODE_REG, 0, tempReg);
                        prog->add((u8)2);

                        if(tsize == 4)
                            prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                        else
                            prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                        prog->addModRM(MODE_REG, REG_XMM7, tempReg);
                        if(tsize == 4) 
                            prog->add3(OPCODE_3_MULSS_REG_RM);
                        else
                            prog->add3(OPCODE_3_MULSD_REG_RM);
                        prog->addModRM(MODE_REG, xmm, REG_XMM7);

                        // END
                        prog->set(jumpEnd, prog->size() - (jumpEnd+1)); // +1 because jumpEnd points to the byte to change in the jump instruction, we want the end of the instruction.

                        if (!toXmm) {
                            if(tsize==4)
                                prog->add3(OPCODE_3_MOVSS_RM_REG);
                            else
                                prog->add3(OPCODE_3_MOVSD_RM_REG);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, xmm, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);


                            prog->add(OPCODE_MOV_REG_RM);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);
                        } else {
                            // xmm register is already loaded with correct value
                        }
                        #endif
                    } else {
                        // nocheckin, rexw when converting u32 to f32/f64 otherwise the last bit will
                        // be treated as a signed bit by CVTSI2SS
                        if(fsize == 1){
                            // prog->add(PREFIX_REXW);
                            // prog->add(OPCODE_AND_RM_IMM_SLASH_4);
                            // prog->addModRM(MODE_REG, 4, freg);
                            // prog->add4((u32)0xFF);
                            if(tsize==4)
                                prog->add3(OPCODE_3_CVTSI2SS_REG_RM);
                            else
                                prog->add3(OPCODE_3_CVTSI2SD_REG_RM);
                        } else if(fsize == 2){
                            // prog->add(PREFIX_REXW);
                            // prog->add(OPCODE_AND_RM_IMM_SLASH_4);
                            // prog->addModRM(MODE_REG, 4, freg);
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
                            prog->add(PREFIX_REXW);
                            prog->add(OPCODE_MOV_REG_RM);
                            prog->addModRM(MODE_REG, freg, freg); // clear upper 32 bits
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
                            prog->addModRM(MODE_REG, treg, freg);
                        } else {
                            prog->addModRM(MODE_REG, REG_XMM7, freg);

                            if(tsize==4)
                                prog->add3(OPCODE_3_MOVSS_RM_REG);
                            else
                                prog->add3(OPCODE_3_MOVSD_RM_REG);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, REG_XMM7, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);

                            if(tsize==8)
                                prog->add(PREFIX_REXW);
                            prog->add(OPCODE_MOV_REG_RM);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);

                            // prog->add(OPCODE_NOP);
                        }
                    }
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
                        
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM(MODE_REG, treg, freg);
                        
                    } else if(minSize == 8) {
                        Assert(freg == treg);
                        // nothing needs to be done
                    }
                } else if(type==CAST_SINT_SINT) {
                    // i8 -> i16,i32,i64
                    // i64 -> i32,i16,i8

                    // TODO: Sign extend properly
                    // prog->add(PREFIX_REXW);
                    // prog->add(OPCODE_MOV_REG_RM);
                    // prog->addModRM(MODE_REG, treg, freg);
                    if(minSize==1){
                        prog->add(PREFIX_REXW);
                        prog->add2(OPCODE_2_MOVSX_REG_RM8);
                        prog->addModRM(MODE_REG, treg, freg);
                    } else if(minSize == 2) {
                        prog->add(PREFIX_REXW);
                        prog->add2(OPCODE_2_MOVSX_REG_RM16);
                        prog->addModRM(MODE_REG, treg, freg);
                    } else if(minSize == 4) {
                        // Assert(freg == treg);
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_MOVSXD_REG_RM);
                        prog->addModRM(MODE_REG, treg, freg);
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
                } else if(type==CAST_FLOAT_FLOAT){
                    Assert((fsize == 4 || fsize == 8) && (tsize == 4 || tsize == 8));

                    if(fromXmm && toXmm) {
                        if(fsize == 4 && tsize == 8) {
                            prog->add3(OPCODE_3_CVTSS2SD_REG_RM);
                            prog->addModRM(MODE_REG, treg, freg);
                        } else if(fsize == 8 && tsize == 4) {
                            prog->add3(OPCODE_3_CVTSD2SS_REG_RM);
                            prog->addModRM(MODE_REG, treg, freg);
                        } else {
                            // do nothing
                        }
                    } else if(!fromXmm && toXmm) {
                        u8 temp = REG_XMM7;
                        if(fsize == 8)
                            prog->add(PREFIX_REXW);
                        prog->add(OPCODE_MOV_RM_REG);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, freg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);

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
                        prog->add((u8)-8);
                    } else if(fromXmm && !toXmm) {
                        u8 temp = REG_XMM7;
                        if(fsize == 4 && tsize == 8) {
                            prog->add3(OPCODE_3_CVTSS2SD_REG_RM);
                            prog->addModRM(MODE_REG, temp, freg);

                            prog->add3(OPCODE_3_MOVSD_RM_REG);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, temp, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);

                            prog->add(PREFIX_REXW);
                            prog->add(OPCODE_MOV_REG_RM);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);
                        } else if(fsize == 8 && tsize == 4) {
                            prog->add3(OPCODE_3_CVTSD2SS_REG_RM);
                            prog->addModRM(MODE_REG, temp, freg);

                            prog->add3(OPCODE_3_MOVSS_RM_REG);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, temp, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);

                            prog->add(OPCODE_MOV_REG_RM);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);
                        } else if(fsize == 4 && tsize == 4){
                            prog->add3(OPCODE_3_MOVSS_RM_REG);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, freg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);

                            prog->add(OPCODE_MOV_REG_RM);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);
                        } else if(fsize == 8 && tsize == 8) {
                            prog->add3(OPCODE_3_MOVSD_RM_REG);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, freg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);

                            prog->add(PREFIX_REXW);
                            prog->add(OPCODE_MOV_REG_RM);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);
                        }
                    } else if(!fromXmm && !toXmm) {
                        u8 temp = REG_XMM7;
                        if(fsize == 8)
                            prog->add(PREFIX_REXW);
                        prog->add(OPCODE_MOV_RM_REG);
                        prog->addModRM_SIB(MODE_DEREF_DISP8, freg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                        prog->add((u8)-8);

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
                        prog->add((u8)-8);

                        if(tsize == 4) {
                            prog->add3(OPCODE_3_MOVSS_RM_REG);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, temp, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);
                            
                            prog->add(OPCODE_MOV_REG_RM);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);
                        } else if(tsize == 8) {
                            prog->add3(OPCODE_3_MOVSS_RM_REG);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, temp, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);

                            prog->add(PREFIX_REXW);
                            prog->add(OPCODE_MOV_REG_RM);
                            prog->addModRM_SIB(MODE_DEREF_DISP8, treg, SIB_SCALE_1, SIB_INDEX_NONE, REG_SP);
                            prog->add((u8)-8);
                        }
                    }
                } else {
                    Assert(("Cast type not implemented in x64 backend, Compiler bug",false));
                }
                break;
            }
            case BC_CALL: {
                LinkConventions linkConvention = (LinkConventions)op0;
                CallConventions callConvention = (CallConventions)op1;
                if(linkConvention == LinkConventions::DLLIMPORT || linkConvention == LinkConventions::VARIMPORT) {
                    Assert(bytecode->externalRelocations[imm].location == bcIndex);
                    prog->add(OPCODE_CALL_RM_SLASH_2);
                    prog->addModRM_rip(2,(u32)0);
                    Program_x64::NamedUndefinedRelocation namedReloc{};
                    namedReloc.name = bytecode->externalRelocations[imm].name;
                    namedReloc.textOffset = prog->size() - 4;
                    prog->namedUndefinedRelocations.add(namedReloc);
                } else if(linkConvention == LinkConventions::IMPORT) {
                    Assert(bytecode->externalRelocations[imm].location == bcIndex);
                    prog->add(OPCODE_CALL_IMM);
                    Program_x64::NamedUndefinedRelocation namedReloc{};
                    namedReloc.name = bytecode->externalRelocations[imm].name;
                    namedReloc.textOffset = prog->size();
                    prog->namedUndefinedRelocations.add(namedReloc);
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
                        // Assert(false); // is the assembly wrong?
                        // rdx should be buffer and r8 length
                        Program_x64::NamedUndefinedRelocation reloc0{};
                        reloc0.name = "__imp_GetStdHandle"; // C creates these symbol names in it's object file
                        reloc0.textOffset = prog->size() + 0xB;
                        Program_x64::NamedUndefinedRelocation reloc1{};
                        reloc1.name = "__imp_WriteFile";
                        reloc1.textOffset = prog->size() + 0x26;
                        u8 arr[]={ 0x48, 0x83, 0xEC, 0x38, 0xB9, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0x4D, 0x31, 0xC9, 0x49, 0x89, 0xD8, 0x48, 0x89, 0xF2, 0x48, 0x89, 0xC1, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x38 };
                        prog->addRaw(arr,sizeof(arr));

                        prog->namedUndefinedRelocations.add(reloc0);
                        prog->namedUndefinedRelocations.add(reloc1);
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
                        prog->add4((u32)1); // stdout

                        prog->add(OPCODE_CALL_IMM);
                        int reloc_pos = prog->size();
                        prog->add4((u32)0);

                        // We call the Unix write system call, altough not directly
                        Program_x64::NamedUndefinedRelocation reloc0{};
                        reloc0.name = "write"; // symbol name, gcc (or other linker) knows how to relocate it
                        reloc0.textOffset = reloc_pos;
                        prog->namedUndefinedRelocations.add(reloc0);
                    #endif
                        break;
                    }
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
                        Program_x64::NamedUndefinedRelocation reloc0{};
                        reloc0.name = "__imp_GetStdHandle"; // C creates these symbol names in it's object file
                        reloc0.textOffset = prog->size() + 0xB;
                        Program_x64::NamedUndefinedRelocation reloc1{};
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
                        Program_x64::NamedUndefinedRelocation reloc0{};
                        reloc0.name = "write"; // symbol name, gcc (or other linker) knows how to relocate it
                        reloc0.textOffset = reloc_pos;
                        prog->namedUndefinedRelocations.add(reloc0);
                    #endif
                        break;
                    }
                    default: {
                        failure = true;
                        // Assert(bytecode->nativeRegistry);
                        auto nativeRegistry = NativeRegistry::GetGlobal();
                        auto* nativeFunction = nativeRegistry->findFunction(imm);
                        // auto* nativeFunction = bytecode->nativeRegistry->findFunction(imm);
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
            break; case BC_ASM: {
                u32 asmIndex = ASM_DECODE_INDEX(op0,op1,op2);
                Bytecode::ASM asmInstance = bytecode->asmInstances.get(asmIndex);
                u32 len = asmInstance.iEnd - asmInstance.iStart;
                if(len != 0) {
                    u8* ptr = bytecode->rawInstructions._ptr + asmInstance.iStart;
                    // log::out << "InlineASM "<<len<<" machine code bytes\n";
                    prog->addRaw(ptr, len);
                } else {
                    
                    log::out << log::RED << "BC_ASM at "<<bcIndex<<" was incomplete\n";
                    // print error or has that been done already?
                }
                break;
            }
            break; case BC_TEST_VALUE: {
                // Assert(op0 == 8 && op1 == BC_REG_RDX && op2 == BC_REG_RAX);
                // // op0 bytes
                // // op1 test value
                // // op2 computed value



                // // IMPORTANT: stack must be aligned when calling functions
                // #ifdef OS_WINDOWS
                // /*
                // 0:  48 89 e3                mov    rbx,rsp
                // 3:  48 83 c3 04             add    rbx,0x4
                // 7:  48 83 ec 30             sub    rsp,0x30
                // b:  48 89 e3                mov    rbx,rsp
                // e:  c6 03 5f                mov    BYTE PTR [rbx],0x5f
                // 11: 48 39 c2                cmp    rdx,rax
                // 14: 74 03                   je     19 <hop>
                // 16: c6 03 78                mov    BYTE PTR [rbx],0x78
                // 0000000000000019 <hop>:
                // 19: b9 f4 ff ff ff          mov    ecx,0xfffffff4
                // 1e: ff 15 00 00 00 00       call   QWORD PTR [rip+0x0]        # 24 <hop+0xb>
                // 24: 48 c7 44 24 20 00 00    mov    QWORD PTR [rsp+0x20],0x0
                // 2b: 00 00
                // 2d: 4d 31 c9                xor    r9,r9
                // 30: 49 c7 c0 01 00 00 00    mov    r8,0x1
                // 37: 48 89 da                mov    rdx,rbx
                // 3a: 48 89 c1                mov    rcx,rax
                // 3d: ff 15 00 00 00 00       call   QWORD PTR [rip+0x0]        # 43 <hop+0x2a>
                // 43: 48 83 c4 30             add    rsp,0x30
                // */
                // NamedRelocation reloc0{};
                // reloc0.name = "__imp_GetStdHandle"; // C creates these symbol names in it's object file
                // reloc0.textOffset = prog->size() + 0x20;
                // NamedRelocation reloc1{};
                // reloc1.name = "__imp_WriteFile";
                // reloc1.textOffset = prog->size() + 0x3F;
                // u8 arr[]={ 0x48, 0x89, 0xE3, 0x48, 0x83, 0xC3, 0x04, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x89, 0xE3, 0xC6, 0x03, 0x5F, 0x48, 0x39, 0xC2, 0x74, 0x03, 0xC6, 0x03, 0x78, 0xB9, 0xF4, 0xFF, 0xFF, 0xFF, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0x4D, 0x31, 0xC9, 0x49, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00, 0x48, 0x89, 0xDA, 0x48, 0x89, 0xC1, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x30 };
                // prog->addRaw(arr,sizeof(arr));

                // prog->namedRelocations.add(reloc0);
                // prog->namedRelocations.add(reloc1);
                // #endif
                Assert(op0 == 8 && op1 == BC_REG_RDX && op2 == BC_REG_RAX);
                // op0 bytes
                // op1 test value
                // op2 computed value

                // imm

                // IMPORTANT: stack must be aligned when calling functions
                #ifdef OS_WINDOWS
                /*
                mov    rbx,rsp
                sub    rbx,0x8
                sub    rsp,0x30
                mov    DWORD PTR [rbx],0x99993057
                cmp    rdx,rax
                je     hop
                mov    BYTE PTR [rbx],0x78
                hop:
                mov    ecx,0xfffffff4
                call   QWORD PTR [rip+0x0]    
                mov    QWORD PTR [rsp+0x20],0x0
                xor    r9,r9
                mov    r8,0x4
                mov    rdx,rbx
                mov    rcx,rax
                call   QWORD PTR [rip+0x0]    
                add    rsp,0x30

                0x00:  48 89 e3                mov    rbx,rsp
                0x03:  48 83 c3 08             add    rbx,0x8
                0x07:  48 83 ec 30             sub    rsp,0x30
                0x0b:  c7 03 57 30 99 99       mov    DWORD PTR [rbx],0x99993057
                0x11: 48 39 c2                cmp    rdx,rax
                0x14: 74 03                   je     19 <hop>
                0x16: c6 03 78                mov    BYTE PTR [rbx],0x78
                0x0000000000000019 <hop>:
                0x19: b9 f4 ff ff ff          mov    ecx,0xfffffff4     # stderr handle
                0x1e: ff 15 00 00 00 00       call   QWORD PTR [rip+0x0]        # 24 <hop+0xb>
                0x24: 48 c7 44 24 20 00 00    mov    QWORD PTR [rsp+0x20],0x0
                0x2b: 00 00
                0x2d: 4d 31 c9                xor    r9,r9
                0x30: 49 c7 c0 04 00 00 00    mov    r8,0x4
                0x37: 48 89 da                mov    rdx,rbx
                0x3a: 48 89 c1                mov    rcx,rax
                0x3d: ff 15 00 00 00 00       call   QWORD PTR [rip+0x0]        # 43 <hop+0x2a>
                0x43: 48 83 c4 30             add    rsp,0x30
                */

                // TODO: This code could be a subroutine. 100 tests instructions alone would generate
                // 4600 bytes of machine code. With a subroutine you would get less than 1500 (100*12 + 46) which scales well
                // when using even more test intstructions.

                Program_x64::NamedUndefinedRelocation reloc0{};
                reloc0.name = "__imp_GetStdHandle"; // C creates these symbol names in it's object file
                reloc0.textOffset = prog->size() + 0x20;
                Program_x64::NamedUndefinedRelocation reloc1{};
                reloc1.name = "__imp_WriteFile";
                reloc1.textOffset = prog->size() + 0x3F;
                u8 arr[]= {
                    0x48, 0x89, 0xE3, 0x48, 0x83, 0xEB, 0x08, 0x48, 
                    0x83, 0xEC, 0x30, 0xC7, 0x03, 0x57, 0x30, 0x99,
                    0x99, 0x48, 0x39, 0xC2, 0x74, 0x03, 0xC6, 0x03,
                    0x78, 0xB9, 0xF4, 0xFF, 0xFF, 0xFF, 0xFF, 0x15,
                    0x00, 0x00, 0x00, 0x00, 0x48, 0xC7, 0x44, 0x24,
                    0x20, 0x00, 0x00, 0x00, 0x00, 0x4D, 0x31, 0xC9,
                    0x49, 0xC7, 0xC0, 0x04, 0x00, 0x00, 0x00, 0x48,
                    0x89, 0xDA, 0x48, 0x89, 0xC1, 0xFF, 0x15, 0x00,
                    0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x30 };
                arr[0x0F] = (imm>>8)&0xFF;
                arr[0x10] = imm&0xFF;
                prog->addRaw(arr,sizeof(arr));

                prog->namedUndefinedRelocations.add(reloc0);
                prog->namedUndefinedRelocations.add(reloc1);
                #else
                /*
                sub    rsp,0x10  # must be 16-byte aligned when calling unix write
                mov    rbx,rsp
                mov    DWORD PTR [rbx],0x99993a57 # temporary data
                cmp    rdx,rax
                je     hop
                mov    BYTE PTR [rbx],0x78 # err character
                hop:
                mov    rdx,0x4 # buffer size
                mov    rsi,rbx # buffer ptr
                mov    rdi,0x2 # stderr
                call   0
                add    rsp,0x10
                */

                u8 arr[]={
                    0x48, 0x83, 0xEC, 0x10, 0x48, 0x89, 0xE3, 0xC7,
                    0x03, 0x57, 0x3A, 0x99, 0x99, 0x48, 0x39, 0xC2,
                    0x74, 0x03, 0xC6, 0x03, 0x78, 0x48, 0xC7, 0xC2, 
                    0x04, 0x00, 0x00, 0x00, 0x48, 0x89, 0xDE, 0x48, 
                    0xC7, 0xC7, 0x02, 0x00, 0x00, 0x00, 0xE8, 0x00, 
                    0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x10 
                };

                Program_x64::NamedUndefinedRelocation reloc0{};
                reloc0.name = "write"; // symbol name, gcc (or other linker) knows how to relocate it
                reloc0.textOffset = prog->size() + 0x27;
                prog->namedUndefinedRelocations.add(reloc0);
                arr[0x0B] = (imm>>8)&0xFF; // set location info
                arr[0x0C] = imm&0xFF;
                prog->addRaw(arr,sizeof(arr));

                #endif
                break;
            }
            case BC_MEMZERO: {
                // The operands we supplied will be modified. SP, FP should not be modified like we do in memzero.
                Assert(op0 != BC_REG_SP && op0 != BC_REG_FP && op1 != BC_REG_SP && op1 != BC_REG_FP);

                // Assert(op0 == BC_REG_RDI && op1 == BC_REG_RBX);
                // here so you can turn it off.
                // Some time later...
                // I hate you. WHY DID YOU JUST LEAVE if(false)
                // You gotta take care of your mess because I wasted time
                // wondering what's going on!
                // You should have put a log message or something
                // #ifdef gone
                u8 batch_size = op2;
                if(batch_size == 0)
                    batch_size = 1;

                u8 reg_cur = BCToProgramReg(op0,8);
                u8 reg_fin = BCToProgramReg(op1,8);

                prog->add(PREFIX_REXW);
                prog->add(OPCODE_ADD_RM_REG);
                prog->addModRM(MODE_REG, reg_cur, reg_fin);

                int offset_loop = prog->size();

                prog->add(PREFIX_REXW);
                prog->add(OPCODE_CMP_REG_RM);
                prog->addModRM(MODE_REG, reg_fin, reg_cur);

                prog->add(OPCODE_JE_IMM8);
                int offset_jmp_imm = prog->size();
                prog->add((u8)0);

                switch(batch_size) {
                case 1:
                    prog->add(OPCODE_MOV_RM_IMM8_SLASH_0);
                    break;
                case 2:
                    Assert(("Memzero conversion incomplete for 2-byte batch size",false));
                    break;
                case 4:
                    prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                    break;
                case 8:
                    prog->add(PREFIX_REXW);
                    prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                    break;
                default: Assert(false);
                }

                if(reg_cur == REG_BP || reg_cur == REG_SP) {
                    prog->addModRM_SIB(MODE_DEREF, 0, SIB_SCALE_1, SIB_INDEX_NONE, reg_cur);
                } else {
                    prog->addModRM(MODE_DEREF, 0, reg_cur);
                }

                switch(batch_size) {
                case 1:
                    prog->add((u8)0);
                    break;
                case 2:
                    Assert(("Memzero conversion incomplete for 2-byte batch size",false));
                    break;
                case 4:
                    prog->add4((u32)0);
                    break;
                case 8:
                    prog->add4((u32)0);
                    break;
                default: Assert(false);
                }

                prog->add(PREFIX_REXW);
                prog->add(OPCODE_ADD_RM_IMM8_SLASH_0);
                prog->addModRM(MODE_REG, 0, reg_cur);
                prog->add((u8)batch_size);

                prog->add(OPCODE_JMP_IMM8);
                prog->add((u8)(offset_loop - (prog->size()+1)));

                prog->set(offset_jmp_imm, (u8)(prog->size() - (offset_jmp_imm + 1)));
                // #endif

                // if(op0 != BC_REG_RDI) {
                //     prog->add(PREFIX_REXW);
                //     prog->add(OPCODE_MOV_REG_RM);
                //     prog->addModRM(MODE_REG, REG_DI, BCToProgramReg(op0,8));
                // }
                // if(op1 != BC_REG_RBX) {
                //     prog->add(PREFIX_REXW);
                //     prog->add(OPCODE_MOV_REG_RM);
                //     prog->addModRM(MODE_REG, REG_B, BCToProgramReg(op1,8));
                // }


                /*
                    0:  48 01 fb                add    rbx,rdi
                    3:  48 39 df                cmp    rdi,rbx
                    6:  74 08                   je     0x10
                    8:  c6 07 00                mov    BYTE PTR [rdi],0x0
                    b:  48 ff c7                inc    rdi
                    e:  eb f3                   jmp    0x3
                */
                // you might want this to be a function
                // u8 arr[] = { 0x48, 0x01, 0xFB, 0x48, 0x39, 0xDF, 0x74, 0x08, 0xC6, 0x07, 0x00, 0x48, 0xFF, 0xC7, 0xEB, 0xF3 };
                // prog->addRaw(arr, sizeof(arr));

                break;
            }
            break; case BC_MEMCPY: {
                Assert(op0 == BC_REG_RDI && op1 == BC_REG_RSI && op2 == BC_REG_RBX);
                // if(op0 != BC_REG_RDI) {
                //     prog->add(PREFIX_REXW);
                //     prog->add(OPCODE_MOV_REG_RM);
                //     prog->addModRM(MODE_REG, REG_DI, BCToProgramReg(op0,8));
                // }
                // if(op1 != BC_REG_RSI) {
                //     prog->add(PREFIX_REXW);
                //     prog->add(OPCODE_MOV_REG_RM);
                //     prog->addModRM(MODE_REG, REG_SI, BCToProgramReg(op1,8));
                // }
                // if(op2 != BC_REG_RBX) {
                //     prog->add(PREFIX_REXW);
                //     prog->add(OPCODE_MOV_REG_RM);
                //     prog->addModRM(MODE_REG, REG_B, BCToProgramReg(op2,8));
                // }
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
            break; case BC_STRLEN: {
                Assert(op0 == BC_REG_RSI && op1 == BC_REG_RCX && op2 == BC_REG_RAX);
                /*
                xor rcx, rcx
                top:
                mov al, [rsi + rcx]
                inc ecx
                cmp al, 0
                jne top
                dec ecx
                */
                u8 arr[] = { 0x48, 0x31, 0xC9, 0x8A, 0x04, 0x0E, 0xFF, 0xC1, 0x3C, 0x00, 0x75, 0xF7, 0xFF, 0xC9 };
                prog->addRaw(arr, sizeof(arr));
                break;
            }
            break; case BC_RDTSC: {
                // This instruction uses edx:eax and ecx.
                // That is why the operands should specify these registers to indicate
                // that they are used. This isn't a must, you could edx, eax and ecx before
                // executing the instruction and then mov values into the operands
                Assert(op0 == BC_REG_RAX && op1 == BC_REG_RDX);
                
                prog->add2(OPCODE_2_RDTSC);
                
                prog->add(PREFIX_REXW);
                prog->add(OPCODE_SHL_RM_IMM8_SLASH_4);
                prog->addModRM(MODE_REG, 4, REG_D);
                prog->add((u8)32);

                // rdtsc will zero extern 8 byte registers
                prog->add(PREFIX_REXW);
                prog->add(OPCODE_OR_RM_REG);
                prog->addModRM(MODE_REG, REG_D, REG_A);
                
                break;
            }
            // break; case BC_RDTSCP: {
            //     // This instruction uses edx:eax and ecx.
            //     // That is why the operands should specify these registers to indicate
            //     // that they are used. This isn't a must, you could edx, eax and ecx before
            //     // executing the instruction and then mov values into the operands
            //     Assert(op0 == BC_REG_RAX && op1 == BC_REG_ECX && op2 == BC_REG_RDX);
                
            //     prog->add3(OPCODE_3_RDTSCP);
            //     // prog->add2(OPCODE_2_RDTSC);
                
            //     prog->add(PREFIX_REXW);
            //     prog->add(OPCODE_SHL_RM_IMM8_SLASH_4);
            //     prog->addModRM(MODE_REG, 4, REG_D);
            //     prog->add((u8)32);

            //     // rdtscp will zero extern 8 byte registers
            //     prog->add(PREFIX_REXW);
            //     prog->add(OPCODE_OR_RM_REG);
            //     prog->addModRM(MODE_REG, REG_D, REG_A);
                
            //     break;
            // }
            break; case BC_CMP_SWAP: {
                // This instruction uses edx:eax and ecx.
                // That is why the operands should specify these registers to indicate
                // that they are used. This isn't a must, you could edx, eax and ecx before
                // executing the instruction and then mov values into the operands
                Assert(op0 == BC_REG_RBX && op1 == BC_REG_EAX && op2 == BC_REG_EDX);

                u8 regPtr = BCToProgramReg(op0,8);
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
            break; case BC_ATOMIC_ADD: {
                // This instruction uses edx:eax and ecx.
                // That is why the operands should specify these registers to indicate
                // that they are used. This isn't a must, you could edx, eax and ecx before
                // executing the instruction and then mov values into the operands
                Assert(op0 == BC_REG_RBX && op1 == BC_REG_EAX);

                u8 regPtr = BCToProgramReg(op0, 8);
                u8 regVal = BCToProgramReg(op1, 4);
                
                prog->add(PREFIX_LOCK);
                prog->add(OPCODE_ADD_RM_REG);
                prog->addModRM(MODE_DEREF, regVal, regPtr);

                break;
            }
            break; case BC_SQRT: {
                // This instruction uses edx:eax and ecx.
                // That is why the operands should specify these registers to indicate
                // that they are used. This isn't a must, you could edx, eax and ecx before
                // executing the instruction and then mov values into the operands
                Assert(op0 == BC_REG_XMM0f);

                u8 reg = BCToProgramReg(op0, 4, true);

                prog->add3(OPCODE_3_SQRTSS_REG_RM);
                prog->addModRM(MODE_REG, reg, reg);

                break;
            }
            break; case BC_ROUND: {
                // This instruction uses edx:eax and ecx.
                // That is why the operands should specify these registers to indicate
                // that they are used. This isn't a must, you could edx, eax and ecx before
                // executing the instruction and then mov values into the operands
                Assert(op0 == BC_REG_XMM0f);

                u8 reg = BCToProgramReg(op0, 4, true);

                prog->add4(OPCODE_4_ROUNDSS_REG_RM_IMM8);
                prog->addModRM(MODE_REG, reg, reg);
                prog->add((u8)0); // immediate describes rounding mode. 0 means "round to" which I believe will
                // choose the number that is closest to the actual number. There where round toward, up, and toward as well (round to is probably a good default).

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

    // TODO: If you have many relocations you can use process them using multiple threads.
    //  It can't be too few because the overhead of managing the threads would be worse
    //  than the performance gain.
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

    // Assert(sizeof(Bytecode::PtrDataRelocation) == sizeof(Program_x64::PtrDataRelocation));
    // prog->ptrDataRelocations.resize(bytecode->ptrDataRelocations.size());
    // memcpy(prog->ptrDataRelocations.data(), bytecode->ptrDataRelocations.data(), bytecode->ptrDataRelocations.size() * sizeof(Bytecode::PtrDataRelocation));

    // FP pop is generated by the bytecode generator
    // prog->add(OPCODE_POP_RM_SLASH_0);
    // prog->addModRM(MODE_REG,0,REG_BP);
    Assert(prog->text[prog->size()-1] == OPCODE_RET); // bytecode generator should have added a return
    // prog->add(OPCODE_RET);

    for(int i=0;i<bytecode->exportedSymbols.size();i++) {
        auto& sym = bytecode->exportedSymbols[i];
        Program_x64::ExportedSymbol tmp{};
        tmp.name = sym.name;
        tmp.textOffset = addressTranslation[sym.location];
        prog->exportedSymbols.add(tmp);
    }

    if(prog->debugInformation) {
        int funcs = prog->debugInformation->functions.size();
        for(int i=0;i<funcs;i++){
            auto& fun = prog->debugInformation->functions[i];
            fun.funcStart = addressTranslation[fun.funcStart];
            fun.codeStart = addressTranslation[fun.codeStart];
            // Since funcEnd/codeEnd is exclusive, they may refer to instructions that are out of bounds.
            // We must handle this with some ifs.
            if(fun.funcEnd >= addressTranslation.size())
                fun.funcEnd = prog->size();
            else
                fun.funcEnd = addressTranslation[fun.funcEnd];
            if(fun.codeEnd >= addressTranslation.size())
                fun.codeEnd = prog->size();
            else
                fun.codeEnd = addressTranslation[fun.codeEnd];
            // Don't forget to add new translations here

            int lines = fun.lines.size();
            for(int j=0;j<lines;j++) {
                auto& line = fun.lines[j];
                line.asm_address = addressTranslation[line.bc_address];
            }
        }
        for (int i=0;i<prog->debugInformation->ast->_scopeInfos.size();i++) {
            auto scope = prog->debugInformation->ast->_scopeInfos[i];
            scope->asm_start = addressTranslation[scope->bc_start];
             if(scope->bc_end >= addressTranslation.size())
                scope->asm_end = prog->size();
            else
                scope->asm_end = addressTranslation[scope->bc_end];
        }
    }

    for(int i=0;i<bytecode->debugDumps.size();i++){
        Bytecode::Dump& dump = bytecode->debugDumps[i];
        if(dump.dumpAsm) {
            dump.asm_startIndex = addressTranslation[dump.bc_startIndex];
            dump.asm_endIndex = addressTranslation[dump.bc_endIndex];
        }
    }
    
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
    TRACK_FREE(program, Program_x64);
    // engone::Free(program,sizeof(Program_x64));
}
Program_x64* Program_x64::Create() {
    using namespace engone;

    // auto program = (Program_x64*)engone::Allocate(sizeof(Program_x64));
    auto program = TRACK_ALLOC(Program_x64);
    new(program)Program_x64();
    return program;
}