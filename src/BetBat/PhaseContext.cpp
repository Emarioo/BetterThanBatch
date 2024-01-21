#include "BetBat/PhaseContext.h"

#include "BetBat/Compiler.h"

bool PhaseContext::hasErrors() {
    if(compiler) {
        return errors != 0; // nocheckin, incomplete
    } else {
        Assert(compileInfo);
        return errors != 0 || compileInfo->compileOptions->compileStats.errors != 0; 
    }
}
// Type checker used this for it's foreign function but Generator used the other one.
// Is it a problem if both use the latter one.
// bool PhaseContext::hasForeignErrors() { Assert(compileInfo); return compileInfo->compileOptions->compileStats.errors != 0; }
bool PhaseContext::hasForeignErrors() {
    if(compiler) {
        return false; // nocheckin, incomplete
    } else {
        Assert(compileInfo);
        return compileInfo->compileOptions->compileStats.errorTypes.size() != 0;
    }
}
