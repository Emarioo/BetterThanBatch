#pragma once

#include "BetBat/Util/StringBuilder.h"
#include "Engone/Util/Array.h"
#include "BetBat/DebugInformation.h"
#include "BetBat/Bytecode.h"

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
    
};