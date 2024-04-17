#include "BetBat/PhaseContext.h"

#include "BetBat/Compiler.h"

bool PhaseContext::hasErrors() {
    if(compiler) {
        return errors != 0; // nocheckin, incomplete
    } else {
        Assert(compiler);
        return errors != 0 || compiler->options->compileStats.errors != 0; 
    }
}
// Type checker used this for it's foreign function but Generator used the other one.
// Is it a problem if both use the latter one.
// bool PhaseContext::hasForeignErrors() { Assert(compileInfo); return compileInfo->options->compileStats.errors != 0; }
bool PhaseContext::hasForeignErrors() {
    if(compiler) {
        return compiler->options->compileStats.errors != 0;
    } else {
        Assert(compiler);
        return compiler->options->compileStats.errorTypes.size() != 0;
    }
}
