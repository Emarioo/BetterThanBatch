#pragma once

#include "BetBat/MessageTool.h"

struct CompileInfo;
struct Compiler;
struct PhaseContext {
    Compiler* compiler = nullptr;
    // CompileInfo* compileInfo=nullptr;
    int errors = 0;
    int warnings = 0;
    bool disableCodeGeneration = false; // used by Generator
    
    bool hasForeignErrors(); // has errors from outside this context
    bool hasAnyErrors(); // has error from this and the outside context
};