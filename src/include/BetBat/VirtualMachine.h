#pragma once

#include "BetBat/CompilerOptions.h"
#include "BetBat/Bytecode.h"
#include "BetBat/IntrinsicRegistry.h"
#include "Engone/PlatformLayer.h"
// #include "Native/NativeLayer.h"


enum VMErrorType {
    VM_ERROR_NONE = 0,
    VM_ERROR_UNKNOWN,
    VM_UNRESOLVED_CALL,
    VM_STACK_VIOLATION,
};
struct VMError {
    VMErrorType type = VM_ERROR_NONE;
    std::string message;
};
typedef void(*FnMakeshift)(engone::VoidFunction, void*);
/*
    VirtualMachine may not be the accurate term for executing bytecode.
    VirtualMachine is more executing high level code, statement by statement.
    A virtual machine or bytecode runner would be more accurate.
*/
struct VirtualMachine {
    ~VirtualMachine(){
        cleanup();
    }
    Bytecode* bytecode=nullptr;
    CompileOptions* options=nullptr;
    int REGISTER_SIZE = -1;
    int FRAME_SIZE = -1;
    QuickArray<u8> stack{};
    VMError error{};
    
    bool logging = false;
    bool interactive = false;
    bool is_callback_from_stub = false;
    
    struct CallFrame {
        TinyBytecode* func;
        int return_address;
    };
    struct VMState {
        i64 pc = 0;
        i64 prev_pc;
        i64 registers[BC_REG_MAX];
        i64 stack_pointer = 0;
        i64 base_pointer = 0;

        int ret_offset = 0;
        bool has_return_values_on_stack = false;
        bool expectValuesOnStack = false;

        bool running = true;
        DynamicArray<CallFrame> call_stack{};
        DynamicArray<int> push_offsets;

        TinyBytecode* tinycode = nullptr;
        int tiny_index = -1;
    };

    // VMState state{};
    DynamicArray<VMState> states{};
    void push_state(int index, i64 sp);
    void pop_state();

    i64 userAllocatedBytes=0;
    u64 executedInstructions = 0;
    bool enable_fncall_logging = true;
    std::unordered_map<std::string, u64> number_of_fncalls{};
    BytecodePrintCache print_cache{};
    
    struct Breakpoint {
        int pc;
        // TODO: break on tinycode name
    };
    DynamicArray<Breakpoint> breakpoints{};
    
    struct BytecodePointer {
        void* ptr;
        int size=0;
    };
    DynamicArray<BytecodePointer> bytecode_pointers;

    engone::VoidFunction get_bytecode_pointer(int index);

    DynamicArray<int> misalignments{}; // used by BC_ALLOC_ARGS and BC_FREE_ARGS

    engone::TimePoint tp = 0;

    struct LibFunc {
        DynamicArray<ExternalRelocation*> relocs;
        engone::VoidFunction func_ptr;
        FunctionSignature* signature;
    };
    struct Lib {
        std::unordered_map<std::string, LibFunc*> functions;
        engone::DynamicLibrary dll;
    };

    DynamicArray<TinyBytecode*> checked_codes{};
    DynamicArray<TinyBytecode*> codes_to_check{};

    std::unordered_map<std::string, Lib*> libs;
    DynamicArray<LibFunc*> dll_functions{};
    // DynamicArray<engone::VoidFunction> dll_functions{};
    DynamicArray<void*> dll_variables{};
    DynamicArray<std::string> dll_function_names{};

    bool force_mapping = false;
    bool temp_ptr_was_mapped = false;
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

    void init_stack(int stack_size = 0x100000); // default 1 MB stack size, you may experience problems when calling external functions if you use less
    void execute(Bytecode* bytecode, const std::string& tinycode_name, bool apply_related_relocations = false, CompileOptions* options = nullptr);
    void execute();
    TinyBytecode* fetch_tinycode(Bytecode* bytecode, const std::string& tinycode_name);
    
    // resets registers and other things but keeps the alloctions.
    void reset();
    void cleanup();
    void printRegisters();

    void moveMemory(u8 reg, volatile void* from, volatile void* to);
    volatile void* getReg(u8 id);
    void* setReg(u8 id);

    struct MakeshiftAssembly {
        FnMakeshift func;
        int size;
    };
    std::unordered_map<FunctionSignature*, MakeshiftAssembly> makeshift_map;
    FnMakeshift get_makeshift(FunctionSignature* signature);
};

// defined in hacky_stdcall_asm
#ifdef OS_WINDOWS
extern "C" void __stdcall Makeshift_stdcall(engone::VoidFunction func, void* stack_pointer);
#else
extern "C"  void Makeshift_sysvcall(engone::VoidFunction func, void* stack_pointer);
#endif