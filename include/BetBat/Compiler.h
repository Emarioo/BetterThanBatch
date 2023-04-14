#include "BetBat/Tokenizer.h"
#include "BetBat/Preprocessor.h"
#include "BetBat/Parser.h"
#include "BetBat/Optimizer.h"
#include "BetBat/Interpreter.h"
#include "BetBat/Utility.h"

// compiles based on file format/extension
void CompileFile(const char* path);

void CompileScript(const char* path);
void CompileInstructions(const char* path);
void CompileDisassemble(const char* path);