#pragma once

#include "BetBat/AST.h"
// #include "BetBat/Bytecode.h"

// Extensive debug information.
// The information is converted debug information
// for PDB, ".debug$S", and ".debug$T".
// Don't forget to translate all bytecode offsets
// to machine instruction offsets.
// typedef int DebugFunctionID;
struct TinyBytecode;
struct DebugLine {
    u32 lineNumber;
    u32 bc_address; // relative to function start
    u32 asm_address; // relative to function start
    lexer::TokenOrigin origin;
};
struct DebugLocalVar {
    std::string name;
    int frameOffset = 0;
    TypeId typeId;
    int scopeLevel = 0;
    ScopeId scopeId;
    bool is_global;
};
struct DebugFunction {
    DebugFunction(FuncImpl* impl, TinyBytecode* tinycode, u32 fileIndex) : 
        fileIndex(fileIndex), funcAst(impl ? impl->astFunction : nullptr), funcImpl(impl), tinycode(tinycode) { }
    
    u32 asm_start; // set later
    u32 asm_end;

    u32 offset_from_bp_to_locals = 0; // zero unless non-volatile registers and paramters are present
    u32 args_offset = 0; // zero unless non-volatile registers are used

    std::string name;
    // ScopeId scopeId; // can be derived from funcAst so we could skip this member

    u32 declared_at_line = 0;
    u32 fileIndex;
    u32 import_id; // needed in DWARF.cpp, scope of function when main doesnt't have ASTFunction
    ASTFunction* funcAst = nullptr; // needed for name of arguments
    FuncImpl* funcImpl = nullptr; // needed for type information (arguments, return values)
    TinyBytecode* tinycode = nullptr;

    void addVar(const std::string& name, int frameOffset, TypeId typeId, int scopeLevel, ScopeId scopeId, bool is_global = false) {
        localVariables.add({});
        localVariables.last().name=name;
        localVariables.last().frameOffset=frameOffset;
        localVariables.last().typeId=typeId;
        localVariables.last().scopeLevel=scopeLevel;
        localVariables.last().scopeId=scopeId;
        localVariables.last().is_global=is_global;
    }
    
    void addLine(u32 line, u32 bc_address, lexer::TokenOrigin origin) {
        lines.add({});
        lines.last().lineNumber = line;
        lines.last().bc_address = bc_address;
        lines.last().origin = origin;
    }

    DynamicArray<DebugLocalVar> localVariables;
    // DynamicArray<LexicalScope> scopes;


    DynamicArray<DebugLine> lines;

    // may need some flags for type of function, calling convention?
};
struct DebugGlobalVariable {
    std::string name;
    int location; // offset from data section
    TypeId typeId;
    int line=0;
    int column=0;
    int file=0;
    int uuid=0;
};
struct DebugInformation {
    DebugInformation(AST* ast) : ast(ast) {}
    static DebugInformation* Create(AST* ast) {
        auto ptr = TRACK_ALLOC(DebugInformation);
        new(ptr)DebugInformation(ast);
        return ptr;
    }
    static void Destroy(DebugInformation* ptr) {
        ptr->~DebugInformation();
        TRACK_FREE(ptr, DebugInformation);
    }
    ~DebugInformation() {
        cleanup();
    }
    void cleanup() {
        for(auto f : functions){
            f->~DebugFunction();
            TRACK_FREE(f,DebugFunction);
        }
        for(auto v : global_variables){
            v->~DebugGlobalVariable();
            TRACK_FREE(v,DebugGlobalVariable);
        }
        global_variables.cleanup();
        functions.cleanup();
        files.cleanup();
        // ast = nullptr;
    }

    // struct Line {
    //     u32 lineNumber;
    //     u32 bc_address; // absolute offset, probably should be relative
    //     u32 asm_address; // absolute offset, probably should be relative
    //     lexer::TokenOrigin origin;
    // };
    // struct LocalVar {
    //     std::string name;
    //     int frameOffset = 0;
    //     TypeId typeId;
    //     int scopeLevel = 0;
    //     ScopeId scopeId;
    // };
    // struct LexicalScope {
    //     ScopeId scopeId;
    // };
    DynamicArray<DebugGlobalVariable*> global_variables;
    DynamicArray<DebugFunction*> functions;
    DynamicArray<std::string> files;

    // file should be absolute path
    u32 addOrGetFile(const std::string& file, bool skip_mutex = false);
    DebugFunction* addFunction(FuncImpl* impl, TinyBytecode* tinycode, const std::string& from_file, int declared_at_line);
    DebugGlobalVariable* addGlobalVariable(const std::string& name, int offset, TypeId type, int line, int column, const std::string& from_file);

    void print();

    MUTEX(mutex);

    AST* ast = nullptr;
};