#pragma once
#include "BetBat/Bytecode.h"

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
    u64 size=0;

    void add(u8 byte);
    void add4(u32 dword);

    void addModRM(u8 mod, u8 reg, u8 rm);

    // TODO: Relocations, global data, symbols
    static void Destroy(Program_x64* program);

    bool _reserve(u32 newAllocationSize);
};
Program_x64* ConvertTox64(Bytecode* bytecode);