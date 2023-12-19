#pragma once

#include "BetBat/MessageTool.h"

struct CompileInfo;
struct PhaseContext {
    
    CompileInfo* compileInfo=nullptr;
    int errors = 0;
    int warnings = 0;
    
    bool hasErrors();
    bool hasForeignErrors();
};