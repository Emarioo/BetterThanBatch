#pragma once

#include "BetBat/Preprocessor.h"
#include "BetBat/Parser.h"
#include "BetBat/TypeChecker.h"
#include "BetBat/Generator.h"
#include "BetBat/Interpreter.h"
#include "BetBat/NativeRegistry.h"

// This class is here to standardise the usage of paths.
// It also provides a contained/maintained place with functions related to paths.
struct Path {
    Path() = default;
    Path(const std::string& path);
    Path(const char* path);
    enum Type : u32 {
        DIR = 0x1,
        ABSOLUTE = 0x2,
    };
    bool isDir() const { return _type & DIR; }
    bool isAbsolute() const { return _type & ABSOLUTE; }
    // Turns path into the absolute form based on CWD.
    // Nothing happens if it already is in absolute form
    Path getAbsolute() const;
    Path getDirectory() const;

    std::string text{};
    u32 _type = 0;
};
struct CompileOptions {
    CompileOptions(engone::Memory source) : rawSource(source) {}
    CompileOptions(const std::string& sourceFile) : initialSourceFile(sourceFile) {}
    CompileOptions(const std::string& sourceFile, const std::string& compilerDirectory) 
    : initialSourceFile(sourceFile), compilerDirectory(
        compilerDirectory.empty() ? "" : 
            (compilerDirectory.back() == '/' ? compilerDirectory : 
                (compilerDirectory + "/"))
    ), silent(false) {}
    Path initialSourceFile; // Path
    const engone::Memory rawSource{0}; // Path
    Path compilerDirectory; // Where resources for the compiler is located. Typically modules.
    std::vector<Path> importDirectories; // Additional directories where imports can be found.
    bool silent=true;
};
struct CompileInfo {
    void cleanup();
    
    // NOTE: struct since more info may be added to each import name
    struct FileInfo{
        TokenStream* stream;  
    };
    std::unordered_map<std::string, FileInfo> tokenStreams; // import streams

    std::unordered_map<std::string, TokenStream*> includeStreams;

    std::vector<Path> importDirectories;

    bool addStream(TokenStream* stream);
    FileInfo* getStream(const Path& name);
    
    int typeErrors=0; // used in generator to avoid printing the same errors
    int errors=0;
    int warnings=0;
    int lines=0;
    int blankLines=0;
    int commentCount=0;
    int readBytes=0; // from the files, DOES NOT COUNT includeStreams! yet?
    AST* ast=0;
    // std::string compilerDir="";

    //-- GLOBAL COMPILER STUFF
    i32 globalUniqueCounter = 0; // for _UNIQUE_
    std::unordered_map<std::string,RootMacro> _macros;

    DynamicArray<TokenStream*> streamsToClean{}; // streams which tokens are used somewhere.

    NativeRegistry nativeRegistry{};
};

Bytecode* CompileSource(CompileOptions options);
void CompileAndRun(CompileOptions options);
void RunBytecode(Bytecode* bytecode);
bool ExportBytecode(Path filePath, const Bytecode* bytecode);
Bytecode* ImportBytecode(Path filePath);