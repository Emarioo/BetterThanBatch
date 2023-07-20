#pragma once
#include "BetBat/Bytecode.h"

/*
Common mistakes

x64 program requires this:
    stack pointer end with the value it began with
    ret must exist at the end

-1073741819 as error code means memory access violation
you may have forgotten ret or messed up the stack pointer


*/

struct ConversionInfo {
    DynamicArray<u8> textSegment{}; // TODO: Bucket array

    void write(u8 byte);
};

struct Program_x64 {
    ~Program_x64(){
        _reserve(0);
    }
    

    u8* text=nullptr;
    u64 _allocationSize=0;
    u64 head=0;

    u64 size() { return head; }

    void add(u8 byte);
    void add2(u16 word);
    void add4(u32 dword);
    void addModRM(u8 mod, u8 reg, u8 rm);
    void addSIB(u8 scale, u8 index, u8 base);

    void addRaw(u8* arr, u64 len);

    // These are here to prevent implicit casting
    // of arguments which causes mistakes.
    void add(i64 byte);
    void add2(i64 word);
    void add4(i64 dword);

    void printHex();

    // TODO: Relocations, global data, symbols
    static void Destroy(Program_x64* program);
    static Program_x64* Create();

    bool _reserve(u32 newAllocationSize);
};
Program_x64* ConvertTox64(Bytecode* bytecode);