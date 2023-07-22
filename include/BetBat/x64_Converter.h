#pragma once
#include "BetBat/Bytecode.h"

/*
Common mistakes

x64 program requires this:
    stack pointer end with the value it began with
    ret must exist at the end

-1073741819 as error code means memory access violation
you may have forgotten ret or messed up the stack pointer

using a macro or some other function when converting bytecode operands to
x64 operands

*/

struct DataRelocation {
    u32 dataOffset; // offset in data segment
    u32 textOffset; // where to modify        
};
struct NamedRelocation {
    std::string name;
    u32 textOffset; // where to modify
};

struct Program_x64 {
    ~Program_x64(){
        _reserve(0);
        engone::Free(globalData, globalSize);
        dataRelocations.cleanup();
        namedRelocations.cleanup();
    }
    
    u8* text=nullptr;
    u64 _allocationSize=0;
    u64 head=0;

    u8* globalData = nullptr;
    u64 globalSize = 0;

    DynamicArray<DataRelocation> dataRelocations;
    DynamicArray<NamedRelocation> namedRelocations;

    u64 size() { return head; }
 
    void add(u8 byte);
    void add2(u16 word);
    void add4(u32 dword);
    void addModRM(u8 mod, u8 reg, u8 rm);
    void addModRM_disp32(u8 reg, u32 disp32);
    void addModRM_SIB(u8 mod, u8 reg, u8 scale, u8 index, u8 base);

    void addRaw(u8* arr, u64 len);

    // These are here to prevent implicit casting
    // of arguments which causes mistakes.
    void add(i64 byte);
    void add2(i64 word);
    void add4(i64 dword);
    void addModRM_disp32(u8 reg, i64 disp32);

    void printHex();

    // TODO: Relocations, global data, symbols
    static void Destroy(Program_x64* program);
    static Program_x64* Create();

    bool _reserve(u32 newAllocationSize);
};
Program_x64* ConvertTox64(Bytecode* bytecode);