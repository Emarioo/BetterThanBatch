#pragma once
#include "BetBat/Bytecode.h"

#include "BetBat/Util/StringBuilder.h"
#include "BetBat/CompilerEnums.h"

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

struct Program_x64 {
    ~Program_x64(){
        _reserve(0);
        TRACK_ARRAY_FREE(globalData, u8, globalSize);
        // engone::Free(globalData, globalSize);
        dataRelocations.cleanup();
        namedUndefinedRelocations.cleanup();
        if(debugInformation) {
            DebugInformation::Destroy(debugInformation);
            debugInformation = nullptr;
        }
    }
    
    u8* text=nullptr;
    u64 _allocationSize=0;
    u64 head=0;

    u8* globalData = nullptr;
    u64 globalSize = 0;

    struct DataRelocation {
        u32 dataOffset; // offset in data segment
        u32 textOffset; // where to modify        
    };
    // struct PtrDataRelocation {
    //     u32 referer_dataOffset;
    //     u32 target_dataOffset;
    // };
    struct NamedUndefinedRelocation {
        std::string name; // name of symbol
        u32 textOffset; // where to modify
    };
    struct ExportedSymbol {
        std::string name; // name of symbol
        u32 textOffset; // where to modify?
    };

    // DynamicArray<PtrDataRelocation> ptrDataRelocations;
    DynamicArray<DataRelocation> dataRelocations;
    DynamicArray<NamedUndefinedRelocation> namedUndefinedRelocations;
    DynamicArray<ExportedSymbol> exportedSymbols;

    DebugInformation* debugInformation = nullptr;

    u64 size() { return head; }
 
    void add(u8 byte);
    void add2(u16 word);
    void add3(u32 word);
    void add4(u32 dword);
    void addModRM(u8 mod, u8 reg, u8 rm);
    // RIP-relative addressing
    void addModRM_rip(u8 reg, u32 disp32);
    void addModRM_rip(u8 reg, i64 disp32);
    void addModRM_SIB(u8 mod, u8 reg, u8 scale, u8 index, u8 base);

    void addRaw(u8* arr, u64 len);

    // These are here to prevent implicit casting
    // of arguments which causes mistakes.
    void add(i64 byte);
    void add2(i64 word);
    void add3(i64 word);
    void add4(i64 dword);

    void printHex(const char* path = nullptr);
    void printAsm(const char* path, const char* objpath = nullptr);

    void set(u32 index, u8 byte) { Assert(index < head); text[index] = byte; }

    static void Destroy(Program_x64* program);
    static Program_x64* Create();

    bool _reserve(u32 newAllocationSize);
};
Program_x64* ConvertTox64(Bytecode* bytecode);
// The function will print the reformatted content if outBuffer is null
void ReformatDumpbinAsm(LinkerChoice linker, QuickArray<char>& inBuffer, QuickArray<char>* outBuffer, bool includeBytes);