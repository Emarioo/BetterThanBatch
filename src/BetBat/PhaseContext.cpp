#include "BetBat/PhaseContext.h"

#include "BetBat/Compiler.h"

bool PhaseContext::hasAnyErrors() {
    if(disableCodeGeneration) return true;
    return errors != 0 || hasForeignErrors();
}
// Type checker used this for it's foreign function but Generator used the other one.
// Is it a problem if both use the latter one.
// bool PhaseContext::hasForeignErrors() { Assert(compileInfo); return compileInfo->options->compileStats.errors != 0; }
bool PhaseContext::hasForeignErrors() {
    if(disableCodeGeneration) return true;
    Assert(compiler);
    return compiler->options->compileStats.errors != 0;
    // return compiler->errorTypes.size() != 0;
}
