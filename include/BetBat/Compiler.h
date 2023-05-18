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
};

// compiles based on file format/extension
void CompileFile(const char* path);

void CompileScript(const char* path, int extra = 1);
void CompileInstructions(const char* path);
void CompileDisassemble(const char* path);