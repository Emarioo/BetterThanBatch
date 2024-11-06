#pragma once

#include "BetBat/Util/StringBuilder.h"
#include "Engone/Util/Array.h"
#include "BetBat/DebugInformation.h"
#include "BetBat/Bytecode.h"

struct ArchitectureInfo {
    int FRAME_SIZE=-1;
    int REGISTER_SIZE=-1;
};
static const ArchitectureInfo ARCH_x86_64 = {16, 8};
static const ArchitectureInfo ARCH_aarch64 = {16, 8};
static const ArchitectureInfo ARCH_arm = {8, 4};

struct FunctionProgram {
    ~FunctionProgram() {
        if(_allocationSize!=0){
            TRACK_ARRAY_FREE(text, u8, _allocationSize);
        }
    }
    u8* text=nullptr;
    u64 _allocationSize=0;
    u64 head=0;
    
    void printHex(const char* path = nullptr);
};

struct Program {
    ~Program(){
        TRACK_ARRAY_FREE(globalData, u8, globalSize);
        // engone::Free(globalData, globalSize);
        dataRelocations.cleanup();
        namedUndefinedRelocations.cleanup();
        
        for(auto p : functionPrograms) {
            // may be null if we had an error
            // tinyPrograms is resized based on requested_index so if a bytecode with a high index was generated first and then an error happened the other bytecodes won't even try to create tinyPrograms, therefore leaving some nullptrs.
            if(p) {
                p->~FunctionProgram();    
                TRACK_FREE(p, FunctionProgram);
            }
        }
        functionPrograms.cleanup();

        // compiler->program borrows debugInformation from compiler->code
        // and it is unclear who should destroy it so we don't.
        // if(debugInformation) {
        //     DebugInformation::Destroy(debugInformation);
        //     debugInformation = nullptr;
        // }
    }
   
    struct DataRelocation {
        u32 dataOffset; // offset in data segment
        u32 textOffset; // where to modify       
        i32 tinyprog_index; 
    };
    // struct PtrDataRelocation {
    //     u32 referer_dataOffset;
    //     u32 target_dataOffset;
    // };
    struct NamedUndefinedRelocation {
        std::string name; // name of symbol
        u32 textOffset; // where to modify
        i32 tinyprog_index;
        std::string library_path;

        bool is_global_var = false;
    };
    // exported functions
    struct ExportedSymbol {
        std::string name; // name of symbol
        // u32 textOffset; // where to modify?
        i32 tinyprog_index;
    };
    struct InternalFuncRelocation {
        i32 from_tinyprog_index;
        u32 textOffset;
        i32 to_tinyprog_index;
    };
     
    DynamicArray<FunctionProgram*> functionPrograms;
    
    u8* globalData = nullptr;
    u64 globalSize = 0;
    DebugInformation* debugInformation = nullptr;
    
    int index_of_main = -1;
    
    // DynamicArray<PtrDataRelocation> ptrDataRelocations;
    DynamicArray<DataRelocation> dataRelocations;
    DynamicArray<ExportedSymbol> exportedSymbols;
    DynamicArray<NamedUndefinedRelocation> namedUndefinedRelocations;
    DynamicArray<InternalFuncRelocation> internalFuncRelocations;
    
    DynamicArray<std::string> forced_libraries;
    DynamicArray<std::string> libraries; // path to libraries, unique entries
     
    FunctionProgram* createProgram(int requested_index) {
        if(functionPrograms.size() <= requested_index) {
            functionPrograms.resize(requested_index+1);
        }

        auto ptr = TRACK_ALLOC(FunctionProgram);
        new(ptr)FunctionProgram();
        functionPrograms[requested_index] = ptr;
        // tinyPrograms.add(ptr);
        return ptr;
    }

    void addDataRelocation(u32 dataOffset, u32 textOffset, i32 tinyprog_index) {
        dataRelocations.add({dataOffset, textOffset, tinyprog_index});
    }
    void addNamedUndefinedRelocation(const std::string& name, u32 textOffset, i32 tinyprog_index, const std::string& library_path = "", bool is_var = false) {
        namedUndefinedRelocations.add({name, textOffset, tinyprog_index, library_path, is_var});
    }
    void addExportedSymbol(const std::string& name, i32 tinyprog_index) {
        exportedSymbols.add({name, tinyprog_index});
    }
    void addInternalFuncRelocation(i32 from_func, u32 text_offset, i32 to_func) {
        internalFuncRelocations.add({from_func, text_offset, to_func});
    }
    
    // gather up libraries from named undefined relocations
    // done after all x64 generation is done
    void compute_libraries();

    void addForcedLibrary(std::string path) {
        for(auto& s : forced_libraries) {
            if(s == path) {
                return;
            }
        }
        forced_libraries.add(path);
    }

    // void printAsm(const char* path, const char* objpath = nullptr);

    static void Destroy(Program* program);
    static Program* Create();

    bool finalize_program(Compiler* compiler);
};

// x64 and ARM inherits this builder.
// It's never used directly.
struct ProgramBuilder {
    Compiler* compiler = nullptr;
    Program* program = nullptr;
    FunctionProgram* funcprog = nullptr;
    int current_funcprog_index = 0;
    Bytecode* bytecode = nullptr;
    TinyBytecode* tinycode = nullptr;
    
    void init(Compiler* compiler, TinyBytecode* tinycode);
    
    int code_size() const { return funcprog->head; }
    void ensure_bytes(int size) {
        if(funcprog->head + size >= funcprog->_allocationSize ){
            bool yes = reserve(funcprog->_allocationSize * 2 + 50 + size);
            Assert(yes);
        }
    }

    void emit1(u8 byte);
    void emit2(u16 word);
    void emit3(u32 word);
    void emit4(u32 dword);
    void emit8(u64 dword);
    void emit_bytes(const u8* arr, u64 len);
    bool reserve(u32 size);
    
    // These are here to prevent implicit casting
    // of arguments which causes mistakes.
    void emit1(i64 _);
    void emit2(i64 _);
    void emit3(i64 _);
    void emit4(i64 _);
    void emit8(i8 _);
    
    DynamicArray<i32> bc_to_machine_translation;
    int last_pc = 0;
    void map_translation(i32 bc_addr, i32 asm_addr) {
        while(bc_addr >= 0 && bc_to_machine_translation[bc_addr] == 0) {
            bc_to_machine_translation[bc_addr] = asm_addr;
            bc_addr--;
        }
    }
    void map_strict_translation(i32 bc_addr, i32 asm_addr) {
        bc_to_machine_translation[bc_addr] = asm_addr;
    }
    int get_map_translation(i32 bc_addr) {
        if(bc_addr >= bc_to_machine_translation.size())
            return last_pc;
        return bc_to_machine_translation[bc_addr];
    }
    
    struct RelativeRelocation {
        // RelativeRelocation(i32 ip, i32 x64, i32 bc) : currentIP(ip), immediateToModify(x64), bcAddress(bc) {}
        i32 inst_addr=0; // It's more of a nextIP rarther than current. Needed when calculating relative offset
        // Address MUST point to an immediate (or displacement?) with 4 bytes.
        // You can add some flags to this struct for more variation.
        i32 imm32_addr=0; // dword/i32 to modify with correct value
        i32 bc_addr=0; // address in bytecode, needed when looking up absolute offset in bc_to_x64_translation
    };
    DynamicArray<RelativeRelocation> relativeRelocations;
    void addRelocation32(int inst_addr, int imm32_addr, int bc_addr) {
        RelativeRelocation rel{};
        rel.inst_addr = inst_addr;
        rel.imm32_addr = imm32_addr;
        rel.bc_addr = bc_addr;
        relativeRelocations.add(rel);
    }
    
    DynamicArray<int> push_offsets{}; // used when set arguments while values are pushed and popped
    int ret_offset = 0;
    int callee_saved_space = 0;

    struct Arg {
        InstructionControl control = CONTROL_NONE;
        int offset_from_rbp = 0;
    };
    DynamicArray<Arg> recent_set_args;
    
    struct SPMoment {
        int virtual_sp;
        int pop_at_bc_index;
    };
    DynamicArray<SPMoment> sp_moments{};
    DynamicArray<int> misalignments{}; // used by BC_ALLOC_ARGS and BC_FREE_ARGS
    int virtual_stack_pointer = 0;
    void push_stack_moment(int sp, int bc_addr) {
        sp_moments.add({sp, bc_addr});
        // log::out << log::GRAY << "PUSH "<<sp <<" "<< bc_addr <<"\n";
    }
    void pop_stack_moment(int index = -1) {
        if (index == -1)
            index = sp_moments.size()-1;
        auto& moment = sp_moments[index];
        virtual_stack_pointer = moment.virtual_sp;
        // log::out << log::GRAY << "POP " << index << "  sp:" << moment.virtual_sp << " " << moment.pop_at_bc_index << "\n";
        sp_moments.removeAt(index);
    }
    

};