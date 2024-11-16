#pragma once

#include "BetBat/CompilerOptions.h"
#include "BetBat/Bytecode.h"
#include "BetBat/NativeRegistry.h"
// #include "Native/NativeLayer.h"

/*
    VirtualMachine may not be the accurate term for executing bytecode.
    VirtualMachine is more executing high level code, statement by statement.
    A virtual machine or bytecode runner would be more accurate.
*/
struct VirtualMachine {
    ~VirtualMachine(){
        cleanup();
    }

    int REGISTER_SIZE = -1;
    int FRAME_SIZE = -1;
    
    i64 registers[BC_REG_MAX];
    // engone::Memory<u8> stack{};
    QuickArray<u8> stack{};

    bool has_return_values_on_stack = false;
    int ret_offset = 0;
    // int push_offset = 0;
    DynamicArray<int> push_offsets;
    i64 stack_pointer = 0;
    i64 base_pointer = 0;

    bool expectValuesOnStack = false;
    bool silent = false;
    
    struct MemoryMapping {
        u64 start_address;
        u64 physical_start_address;
        u64 size;
    };
    DynamicArray<MemoryMapping> memory_map;
    // returns false if memory mapping overlaps with another mapping.
    // multiple mappings can map to overlapping addresses.
    void reset_memory_map(){
        memory_map.clear();
    }
    bool add_memory_mapping(u64 start, u64 physical, u64 size);
    // was_mapped is set to false if no mapping was found
    void* map_pointer(u64 virtual_pointer, bool& was_mapped);

    void init_stack(int stack_size = 0x10000);
    void execute(Bytecode* bytecode, const std::string& tinycode_name, bool apply_related_relocations = false, CompileOptions* options = nullptr);
    TinyBytecode* fetch_tinycode(Bytecode* bytecode, const std::string& tinycode_name);
    
    // resets registers and other things but keeps the alloctions.
    void reset();
    void cleanup();
    void printRegisters();

    void executeNative(int tiny_index);

    void moveMemory(u8 reg, volatile void* from, volatile void* to);
    volatile void* getReg(u8 id);
    void* setReg(u8 id);

    // engone::Memory<char> cmdArgsBuffer{};
    // Language::Slice<Language::Slice<char>> cmdArgs{};
    // void setCmdArgs(const DynamicArray<std::string>& inCmdArgs);

    static const int CWD_LIMIT = 256;
    char cwdBuffer[CWD_LIMIT]{0};
    u32 usedCwd=0;
};

// defined in hacky_stdcall_asm
#ifdef OS_WINDOWS
extern "C" void __stdcall Makeshift_stdcall(engone::VoidFunction func, void* stack_pointer);
#else
extern "C"  void Makeshift_sysvcall(engone::VoidFunction func, void* stack_pointer);
#endif