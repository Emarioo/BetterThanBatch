#include "BetBat/AST.h"
#pragma once

// Extensive debug information.
// The information is converted debug information
// for PDB, ".debug$S", and ".debug$T".
// Don't forget to translate all bytecode offsets
// to machine instruction offsets.
struct DebugInformation {
    static DebugInformation* Create(AST* ast) {
        auto ptr = TRACK_ALLOC(DebugInformation);
        new(ptr)DebugInformation();
        ptr->ast = ast;
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
        if(ownerOfAST)
            AST::Destroy(ast);
        ownerOfAST = false;
        ast = nullptr;
        for(auto& stream : tokenStreams) {
            TokenStream::Destroy(stream);
        }
        tokenStreams.cleanup();
    }

    struct Line {
        u32 lineNumber;
        u32 bc_address; // absolute offset, probably should be relative
        u32 asm_address; // absolute offset, probably should be relative
        u32 tokenIndex;
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
    struct Function {
        u32 bc_start;
        u32 bc_end; // exclusive
        
        u32 funcStart; // first instruction in the function
        u32 funcEnd; // the byte after the last instruction (also called exclusive)
        u32 codeStart; // the instruction where actual source code starts, funcStart includes setup of frame pointer and stack, this member does not include that.
        u32 codeEnd; // exclusive

        u32 entry_line; // line where function was declared
        // u32 returning_line; // line where function ends/returns

        std::string name;
        // ScopeId scopeId; // can be derived from funcAst so we could skip this member

        ASTFunction* funcAst = nullptr; // needed for name of arguments
        FuncImpl* funcImpl = nullptr; // needed for type information (arguments, return values)

        void addVar(const std::string& name, int frameOffset, TypeId typeId, int scopeLevel, ScopeId scopeId) {
            localVariables.add({});
            localVariables.last().name=name;
            localVariables.last().frameOffset=frameOffset;
            localVariables.last().typeId=typeId;
            localVariables.last().scopeLevel=scopeLevel;
            localVariables.last().scopeId=scopeId;
        }
        
        void addLine(u32 line, u32 bc_address, u32 tokenIndex) {
            lines.add({});
            lines.last().lineNumber = line;
            lines.last().bc_address = bc_address;
            lines.last().tokenIndex = tokenIndex;
        }

        DynamicArray<LocalVar> localVariables;
        // DynamicArray<LexicalScope> scopes;

        u32 fileIndex;
        TokenStream* tokenStream;

        DynamicArray<Line> lines;

        // may need some flags for type of function
        // call convention, far or near call and such
    };
    DynamicArray<Function> functions;
    DynamicArray<std::string> files;

    // file should be absolute path
    u32 addOrGetFile(const std::string& file);
    // will rename the function if it already exists.
    // return pointer may be invalidated if a reallocation occurs
    Function* addFunction(const std::string name);

    void print();

    bool ownerOfAST = false;
    AST* ast = nullptr; // make sure you don't destroy the AST while debug information is using it
    DynamicArray<TokenStream*> tokenStreams;
};