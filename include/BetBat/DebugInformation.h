#include "BetBat/AST.h"
// #include "BetBat/Bytecode.h"
#pragma once

// Extensive debug information.
// The information is converted debug information
// for PDB, ".debug$S", and ".debug$T".
// Don't forget to translate all bytecode offsets
// to machine instruction offsets.
// typedef int DebugFunctionID;
struct TinyBytecode;
struct DebugLine {
    u32 lineNumber;
    u32 bc_address; // absolute offset, probably should be relative
    u32 asm_address; // absolute offset, probably should be relative
    lexer::TokenOrigin origin;
};
struct DebugLocalVar {
    std::string name;
    int frameOffset = 0;
    TypeId typeId;
    int scopeLevel = 0;
    ScopeId scopeId;
};
struct DebugFunction {
        DebugFunction(FuncImpl* impl, TinyBytecode* tinycode, u32 fileIndex) : 
            funcAst(impl ? impl->astFunction : nullptr), funcImpl(impl), tinycode(tinycode), fileIndex(fileIndex) { }
        
        u32 asm_start; // set later
        u32 asm_end;

        std::string name;
        // ScopeId scopeId; // can be derived from funcAst so we could skip this member

        u32 declared_at_line = 0;
        u32 fileIndex;
        ASTFunction* funcAst = nullptr; // needed for name of arguments
        FuncImpl* funcImpl = nullptr; // needed for type information (arguments, return values)
        TinyBytecode* tinycode = nullptr;

        void addVar(const std::string& name, int frameOffset, TypeId typeId, int scopeLevel, ScopeId scopeId) {
            localVariables.add({});
            localVariables.last().name=name;
            localVariables.last().frameOffset=frameOffset;
            localVariables.last().typeId=typeId;
            localVariables.last().scopeLevel=scopeLevel;
            localVariables.last().scopeId=scopeId;
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
        functions.cleanup();
        files.cleanup();
        // ast = nullptr;
    }

    struct Line {
        u32 lineNumber;
        u32 bc_address; // absolute offset, probably should be relative
        u32 asm_address; // absolute offset, probably should be relative
        lexer::TokenOrigin origin;
    };
    struct LocalVar {
        std::string name;
        int frameOffset = 0;
        TypeId typeId;
        int scopeLevel = 0;
        ScopeId scopeId;
    };
    // struct LexicalScope {
    //     ScopeId scopeId;
    // };
    
    DynamicArray<DebugFunction*> functions;
    DynamicArray<std::string> files;

    // file should be absolute path
    u32 addOrGetFile(const std::string& file);
    // TODO: Thread safety
    DebugFunction* addFunction(FuncImpl* impl, TinyBytecode* tinycode, const std::string& from_file, int declared_at_line);

    void print();

    AST* ast = nullptr;
};