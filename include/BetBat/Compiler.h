#pragma once

#include "BetBat/Tokenizer.h"
#include "BetBat/Preprocessor.h"

#include "BetBat/Parser.h"
#include "BetBat/GenParser.h"

#include "BetBat/Optimizer.h"
#include "BetBat/Interpreter.h"
#include "BetBat/Utility.h"

void CompileScript(const char* path, int extra = 1);
void CompileInstructions(const char* path);
void CompileDisassemble(const char* path);