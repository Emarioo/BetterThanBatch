#pragma once

#include "BetBat/Preprocessor.h"
#include "BetBat/Parser.h"
#include "BetBat/TypeChecker.h"
#include "BetBat/Generator.h"
#include "BetBat/Interpreter.h"
#include "BetBat/NativeRegistry.h"
#include "BetBat/ObjectWriter.h"
#include "BetBat/x64_Converter.h"
#include "BetBat/UserProfile.h"

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
    Path getFileName(bool withoutFormat = false) const;
    std::string getFormat() const;

    std::string text{};
    u32 _type = 0;
};
enum TargetPlatform : u32 {
    UNKNOWN_TARGET,
    BYTECODE,
    WINDOWS_x64,
    LINUX_x64,
};
const char* ToString(TargetPlatform target);
engone::Logger& operator<<(engone::Logger& logger,TargetPlatform target);
TargetPlatform ToTarget(const std::string& str);
struct CompileOptions {
    CompileOptions() = default;
    // CompileOptions(engone::Memory<char> source) : rawSource(source) {}
    CompileOptions(const std::string& sourceFile) : initialSourceFile(sourceFile) {}
    CompileOptions(const std::string& sourceFile, const std::string& compilerDirectory) 
    : initialSourceFile(sourceFile), compilerDirectory(
        compilerDirectory.empty() ? "" : 
            (compilerDirectory.back() == '/' ? compilerDirectory : 
                (compilerDirectory + "/"))
    ), silent(false) {}
    Path initialSourceFile; // Path
    engone::Memory<char> rawSource{}; // Path
    Path compilerDirectory{"/"}; // Where resources for the compiler is located. Typically modules.
    std::vector<Path> importDirectories; // Additional directories where imports can be found.
    bool silent=false;
    std::vector<std::string> userArgs;
    TargetPlatform target = BYTECODE;
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
    DynamicArray<std::string> linkDirectives; // passed to the linker, not with interpreter
    // duplicates will be ignored
    void addLinkDirective(const std::string& name);

    bool addStream(TokenStream* stream);
    FileInfo* getStream(const Path& name);

    // TODO: Don't use macros when deciding target platform
    TargetPlatform targetPlatform = 
    #ifdef COMPILE_x64
    #ifdef OS_WINDOWS
    TargetPlatform::WINDOWS_x64
    #elif defined(OS_LINUX)
    TargetPlatform::LINUX_x64
    #endif
    #else
    TargetPlatform::BYTECODE
    #endif
    ;
    
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

    NativeRegistry* nativeRegistry = nullptr;
};

Bytecode* CompileSource(CompileOptions options);
void CompileAndRun(CompileOptions options);
void CompileAndExport(CompileOptions options, Path outPath);

// Content of userArgs is copied
void RunBytecode(Bytecode* bytecode, const std::vector<std::string>& userArgs);
bool ExportBytecode(Path filePath, const Bytecode* bytecode);
Bytecode* ImportBytecode(Path filePath);