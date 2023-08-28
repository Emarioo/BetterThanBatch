#pragma once
#include "BetBat/AST.h"

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

    struct Line {
        u32 lineNumber;
        u32 funcOffset; // offset of function start
    };
    struct LocalVar {
        std::string name;
        u32 frameOffset = 0;
        TypeId typeId;
    };
    struct Function {
        u32 funcStart; // first instruction in the function
        u32 funcEnd; // the byte after the last instruction (also called exclusive)
        u32 srcStart; // the instruction where actual source code starts
        u32 srcEnd; // exclusive

        std::string name;

        FuncImpl* funcImpl = nullptr; // nullptr indicates no arguments or return values

        DynamicArray<LocalVar> localVariables;

        u32 fileIndex;

        DynamicArray<Line> lines;

        // may need some flags for type of function
        // call convention, far or near call and such
    };
    DynamicArray<Function> functions;
    DynamicArray<std::string> files;

    // file should be absolute path
    u32 addOrGetFile(const std::string& file);

    AST* ast = nullptr; // make sure you don't destroy the AST while debug information is using it
};