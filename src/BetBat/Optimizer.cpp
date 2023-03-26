#include "BetBat/Optimizer.h"

bool OptimizeBytecode(Bytecode& code){
    
    // Todo: use OLOG(...)
    
    #define MIN_SIZE(ARR) code.ARR.resize(code.ARR.used);
    MIN_SIZE(codeSegment)
    MIN_SIZE(constNumbers)
    MIN_SIZE(constStrings)
    MIN_SIZE(constStringText)
    MIN_SIZE(debugLines)
    MIN_SIZE(debugLineText)
    
    return true;
}