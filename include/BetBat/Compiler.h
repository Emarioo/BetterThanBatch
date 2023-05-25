#pragma once

#include "BetBat/Tokenizer.h"
#include "BetBat/Preprocessor.h"
#include "BetBat/Utility.h"

// with data types
#include "BetBat/Parser.h"
#include "BetBat/Generator.h"
#include "BetBat/Interpreter.h"

// no data types
#include "BetBat/old/GenParser.h"
#include "BetBat/old/Context.h"
#include "BetBat/old/Optimizer.h"


struct CompileInfo {
    void cleanup();
    
    // NOTE: struct since more info may be added to each import name
    struct FileInfo{
        TokenStream* stream;  
    };
    std::unordered_map<std::string, FileInfo> tokenStreams;
    bool addStream(TokenStream* stream);
    FileInfo* getStream(const std::string& name);
    
    int errors=0;
    int lines=0; // based on token suffix, probably shouldn't be
    int readBytes=0; // from the files
    AST* ast=0;
    std::string compilerDir="";
};

BytecodeX* CompileSource(const std::string& path, const std::string& compilerPath);

// compilerPath is the executable and that folder is where standard modules are found
void CompileAndRun(const std::string& path, const std::string& compilerPath);

void CompileInstructions(const char* path);
void CompileDisassemble(const char* path);

void CompileScriptOld(const char* path, int extra = 1);