#pragma once

#include "BetBat/Parser.h"

// returns false if something failed.
// code may be modified during failure.
bool OptimizeBytecode(Bytecode& code);
