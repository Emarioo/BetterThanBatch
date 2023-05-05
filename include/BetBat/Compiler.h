#pragma once

#include "BetBat/Tokenizer.h"
#include "BetBat/Preprocessor.h"
#include "BetBat/Optimizer.h"
#include "BetBat/Utility.h"

// with data types
#include "BetBat/Parser.h"
#include "BetBat/Generator.h"
#include "BetBat/Interpreter.h"

// no data types
#include "BetBat/GenParser.h"
#include "BetBat/Context.h"


// compiles based on file format/extension
void CompileFile(const char* path);

void CompileScript(const char* path, int extra = 1);
void CompileInstructions(const char* path);
void CompileDisassemble(const char* path);