#pragma once

// Compiler v2
// #include "BetBat/Tokenizer.h"
#include "BetBat/old_Preprocessor.h"
#include "BetBat/old_Parser.h"
#include "BetBat/TypeChecker.h"
#include "BetBat/Generator.h"
#include "BetBat/VirtualMachine.h"
#include "BetBat/NativeRegistry.h"

#include "BetBat/UserProfile.h"
#include "BetBat/CompilerEnums.h"

#include "BetBat/x64_gen.h"
#include "BetBat/COFF.h"
#include "BetBat/ELF.h"
#include "BetBat/ObjectFile.h"

// Compiler v2.1
#include "BetBat/Lexer.h"
#include "BetBat/Preprocessor.h"
#include "BetBat/Parser.h"

// This class is here to standardise the usage of paths.
// It also provides a contained/maintained place with functions related to paths.
// The Unix way is the standard
struct Path {
    Path() = default;
    Path(const std::string& path);
    Path(const char* path);
    // ~Path() {
    //     // text.~basic_string();
    // }
    enum Type : u32 {
        DIR = 0x1,
        ABSOLUTE = 0x2,
    };
    bool isDir() const { return _type & DIR; }
    bool isAbsolute() const;
    //  { return  _type & ABSOLUTE; }
    // Turns path into the absolute form based on CWD.
    // Nothing happens if it already is in absolute form
    Path getAbsolute() const;
    Path getDirectory() const;
    Path getFileName(bool withoutFormat = false) const;
    // does not include .
    std::string getFormat() const;

    std::string text{};
    u32 _type = 0;
};
engone::Logger& operator<<(engone::Logger& logger, const Path& v);

struct CompileOptions;
struct CompileStats {
    volatile long errors=0;
    volatile long warnings=0;
    volatile long lines=0;
    volatile long blankLines=0;
    volatile long readBytes=0; // from the files, DOES NOT COUNT includeStreams! yet?
    volatile long commentCount=0;

    // time measurements
    u64 start_bytecode = 0;
    u64 end_bytecode = 0;
    u64 start_convertx64 = 0;
    u64 end_convertx64 = 0;
    u64 start_objectfile = 0;
    u64 end_objectfile = 0;
    u64 start_linker = 0;
    u64 end_linker = 0;

    u32 bytecodeSize = 0;

    ~CompileStats(){
        generatedFiles.cleanup();
    }
    DynamicArray<std::string> generatedFiles;
    struct Error {
        CompileError errorType;
        u32 line;
        // TODO: union {CompileError error, u32 line; struct { u64 data; };}
    };
    DynamicArray<Error> errorTypes;
    void addError(const TokenRange& range, CompileError errorType = ERROR_UNSPECIFIED) { errorTypes.add({errorType, (u32)range.firstToken.line}); }
    void addError(const Token& token, CompileError errorType = ERROR_UNSPECIFIED) { errorTypes.add({errorType, (u32)token.line}); }

    void printSuccess(CompileOptions* options);
    void printFailed();
    void printWarnings();
};
struct CompilerVersion {
    static const char* global_version;
    static const int MAX_STRING_VERSION_LENGTH = 4*4 + 19 + 4 + 2 + 2; // integers are limited to 9999 and 99
    u16 major; // 1-3 year
    u16 minor; // 1-3 months
    u16 patch; // 3 - 10 days
    u16 revision; // 0 - 24 hours (optional)
    // sublimentary information
    char name[20];
    u16 year;
    u8 month;
    u8 day;
    static CompilerVersion Current();
    void deserialize(const char* str);
    enum Flags : u32 {
        INCLUDE_DATE=0x1,
        EXCLUDE_DATE=0x1, // exclude applies when INCLUDE_AVAILABLE is used
        INCLUDE_REVISION=0x2,
        EXCLUDE_REVISION=0x2,
        INCLUDE_NAME=0x4,
        EXCLUDE_NAME=0x4,
        INCLUDE_AVAILABLE=0x10,
    };
    // bufferSize should include null termination
    void serialize(char* outString, int bufferSize, u32 flags = INCLUDE_AVAILABLE|EXCLUDE_REVISION);
};
struct CompileOptions {
    CompileOptions() = default;
    ~CompileOptions() {
        
    }
    void cleanup() {
        userArguments.cleanup();
        importDirectories.cleanup();
        testLocations.cleanup();
        compileStats.generatedFiles.cleanup();
        compileStats.errorTypes.cleanup();
    }

    std::string source_file;
    std::string output_file = "main.exe"; // Should .exe be default on Unix too?
    TargetPlatform target = CONFIG_DEFAULT_TARGET;
    LinkerChoice linker = CONFIG_DEFAULT_LINKER;
    // std::string linker_cmd = "";
    TextBuffer source_buffer; // pure text instead of a path to some file

    bool useDebugInformation = false;
    bool silent=false;
    bool verbose=false;
    bool executeOutput = false;

    bool instant_report = true;
    bool devmode = false;
    bool only_preprocess = false;
    bool performTests = false;
    bool show_profiling = false;
    std::string pattern_for_files;

    std::string modulesDirectory{"./modules/"}; // Where the standard library can be found. Typically "modules"

    DynamicArray<std::string> userArguments; // Ignored if output isn't executed. Arguments to pass to the interpreter or executable

    DynamicArray<Path> importDirectories; // Directories to look for imports (source files)
    int threadCount=1;

    struct TestLocation {
        // TODO: store file name elsewhere, duplicated data
        std::string file;
        int line=0;
        int column=0;
    };
    DynamicArray<TestLocation> testLocations;
    // returns index of the newly added test location
    TestLocation* getTestLocation(int index);
    // int addTestLocation(TokenRange& range);
    int addTestLocation(lexer::SourceLocation loc, lexer::Lexer* lexer);

    CompileStats compileStats{};
};

enum TaskType : u32 {
    TASK_NONE = 0,
    TASK_LEX                  = 0x1, // lex and import-preprocess
    TASK_PREPROCESS_AND_PARSE = 0x2,

    TASK_TYPE_ENUMS        = 0x10,
    TASK_TYPE_STRUCTS      = 0x20,
    TASK_TYPE_FUNCTIONS    = 0x40,
    TASK_TYPE_BODIES       = 0x80,
    
    TASK_GEN_BYTECODE      = 0x100,
    
    // TASK_GENERATE_TARGET   = 0x200,
};
struct CompilerTask {
    TaskType type;
    u32 import_id;
    ScopeId scopeId;
    bool no_change = false;
};
struct CompilerImport {
    // bool busy = false;
    // bool finished = false;
    TaskType state = TASK_NONE;
    std::string path; // file path (sometimes name for preloaded imports)
    
    u32 import_id=0;
    u32 preproc_import_id=0;

    ScopeId scopeId = 0;
    
    struct Dep {
        u32 id;
        std::string as_name;
    };
    DynamicArray<Dep> dependencies;
    struct Lib {
        std::string path;
        std::string named_as;
    };
    DynamicArray<Lib> libraries;
};
struct Compiler {
    lexer::Lexer lexer{};
    preproc::Preprocessor preprocessor{};
    TypeChecker typeChecker{};
    AST* ast = nullptr;
    Bytecode* bytecode = nullptr;
    
    X64Program* program = nullptr;
    
    Reporter reporter{};

    CompileOptions* options = nullptr;

    u32 initial_import_id = 0;

    bool have_generated_global_data = false; // move elsewhere?
    
    
    BucketArray<CompilerImport> imports{256};
    long volatile globalUniqueCounter = 0; // type must be long volatile because of _InterlockedIncrement
    engone::Mutex otherLock{};
    
    DynamicArray<CompilerTask> tasks;
    
    // DynamicArray<u32> queue_import_ids;
        
    // Lex, preprocess, parse.
    // Multiple threads can call this function to perform
    // processing faster.
    void processImports();
    
    // does stuff based on options (compiling, executing...)
    void run(CompileOptions* options);
    
    // path can be absolute, relative to CWD, relative to the file's directory where the import was specified, or available in the import directories
    u32 addOrFindImport(const std::string& path, const std::string& dir_of_origin_file = "");
    u32 addImport(const std::string& path, const std::string& dir_of_origin_file = "");
    void addDependency(u32 import_id, u32 dep_import_id, const std::string& as_name = "");
    void addLibrary(u32 import_id, const std::string& path, const std::string& as_name = "");
    
    DynamicArray<Path> importDirectories;
    Path findSourceFile(const Path& path, const Path& sourceDirectory = "");
    
    DynamicArray<std::string> linkDirectives; // no duplicates, passed to the linker, not with interpreter
    engone::Mutex lock_miscellaneous;
    // thread safe, duplicates will be ignored
    void addLinkDirective(const std::string& text);

private:
    engone::Mutex lock_imports;
    engone::Semaphore lock_wait_for_imports;
    bool signaled = true;
    volatile int waiting_threads = 0;
    volatile int total_threads = 0;
    
};