#pragma once

// Compiler v2
// #include "BetBat/Tokenizer.h"
// #include "BetBat/old_Preprocessor.h"
// #include "BetBat/old_Parser.h"
#include "BetBat/NativeRegistry.h"

#include "BetBat/UserProfile.h"
#include "BetBat/CompilerEnums.h"

#include "BetBat/COFF.h"
#include "BetBat/ELF.h"
#include "BetBat/ObjectFile.h"

// Compiler v0.2.1 (these had many changes)
#include "BetBat/TypeChecker.h"
#include "BetBat/Generator.h"
#include "BetBat/x64_gen.h"
#include "BetBat/Lexer.h"
#include "BetBat/Preprocessor.h"
#include "BetBat/Parser.h"
#include "BetBat/VirtualMachine.h"

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
    volatile int errors=0;
    volatile int warnings=0;
    volatile int lines=0;
    volatile int readBytes=0; // from the files, DOES NOT COUNT includeStreams! yet?
    volatile int commentCount=0;

    // time measurements
    u64 start_compile = 0; // whole compiler
    u64 end_compile = 0;
    u64 start_linker = 0; // just linker
    u64 end_linker = 0;

    u32 bytecodeSize = 0;

    ~CompileStats(){
        generatedFiles.cleanup();
    }
    DynamicArray<std::string> generatedFiles;
    
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
    }

    std::string source_file;
    std::string output_file = "main.exe"; // Should .exe be default on Unix too? no right?
    TargetPlatform target = CONFIG_DEFAULT_TARGET;
    LinkerChoice linker = CONFIG_DEFAULT_LINKER;
    // std::string linker_cmd = "";
    TextBuffer source_buffer; // pure text instead of a path to some file

    bool useDebugInformation = false;
    bool silent = false;
    bool verbose = false;
    bool executeOutput = false;
    bool incremental_build = false;
    bool stable_global_data = false; // I though about disallowing stable globals when using executable and mostly allowing it for dlls and libs but then I thought, "I dont know how users will use it so why should I limit the possibilities.".

    bool disable_multithreading = true; // TODO: Should be false
    bool disable_preload = false;

    bool quit = false;
    bool instant_report = true;
    bool devmode = false;
    bool only_preprocess = false;
    bool performTests = false;
    bool show_profiling = false;
    std::string pattern_for_files;

    std::string modulesDirectory{"./modules/"}; // Where the standard library can be found. Typically "modules"

    DynamicArray<std::string> defined_macros;

    DynamicArray<std::string> userArguments; // Ignored if output isn't executed. Arguments to pass to the interpreter or executable

    DynamicArray<Path> importDirectories; // Directories to look for imports (source files)
    int threadCount=0; // zero will use the CPU's number of core

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

    TASK_TYPE_ENUMS           = 0x10,
    TASK_TYPE_STRUCTS         = 0x20,
    TASK_TYPE_FUNCTIONS       = 0x40,
    TASK_TYPE_BODY            = 0x80,
    
    TASK_GEN_BYTECODE         = 0x100,
    TASK_GEN_MACHINE_CODE     = 0x200,
};
struct CompilerTask {
    TaskType type;
    u32 import_id;
    ScopeId scopeId;
    bool no_change = false;

    // used with TASK_TYPE_BODY
    ASTFunction* astFunc=nullptr;
    FuncImpl* funcImpl=nullptr;
};
struct CompilerImport {
    TaskType state = TASK_NONE;
    std::string path; // file path (sometimes name for preloaded imports)
    
    u32 import_id=0;
    u32 preproc_import_id=0;
    bool type_checked_import_scope = false;

    ScopeId scopeId = 0;

    DynamicArray<TinyBytecode*> tinycodes;
    
    struct Dep {
        u32 id;
        std::string as_name;
        bool disabled = false;
        bool circular_dependency_to_myself = false;
    };
    DynamicArray<Dep> dependencies;
    struct Lib {
        std::string path;
        std::string named_as;
    };
    DynamicArray<Lib> libraries;
};
extern const char* const PRELOAD_NAME;
extern const char* const TYPEINFO_NAME;
struct Compiler {
    ~Compiler() {
        cleanup();   
    }
    void cleanup() {
        if(bytecode && bytecode->debugInformation)
            DebugInformation::Destroy(bytecode->debugInformation);
        if(program)
            X64Program::Destroy(program);
        program = nullptr;

        if(ast)
            AST::Destroy(ast);
        ast = nullptr;
        if(bytecode)
            Bytecode::Destroy(bytecode);
        bytecode = nullptr;
        lexer.cleanup();
        preprocessor.cleanup();
        typeChecker.cleanup();

        imports.cleanup();
        tasks.cleanup();
        importDirectories.cleanup();
        linkDirectives.cleanup();
        import_id_to_base_id.cleanup();
        errorTypes.cleanup();
    }

    lexer::Lexer lexer{};
    preproc::Preprocessor preprocessor{};
    TypeChecker typeChecker{};
    AST* ast = nullptr;
    Bytecode* bytecode = nullptr;
    
    X64Program* program = nullptr;
    
    Reporter reporter{};

    CompileOptions* options = nullptr;

    std::string entry_point = "main";
    lexer::SourceLocation location_of_entry_point;
    bool has_generated_entry_point = false;
    bool force_default_entry_point = false; // libc requires it's own entry point

    u32 initial_import_id = 0;
    u32 preload_import_id = 0;
    u32 typeinfo_import_id = 0;

    double last_modified_time = 0.0;
    bool output_is_up_to_date = false;

    bool code_has_exceptions = false; // true if source code contains at least one try-catch
    bool have_prepared_global_data = false;
    volatile bool have_generated_comp_time_global_data = false; // this variable should be volatile to prevent compiler from rearraning it in dangerous ways when multiple threads modify it.
    bool compiler_got_stuck = false;

    int struct_tasks_since_last_change = 0;

    CompilerImport* getImport(u32 import_id);
    BucketArray<CompilerImport> imports{256};
    long volatile globalUniqueCounter = 0; // type must be long volatile because of _InterlockedIncrement
    engone::Mutex otherLock{};

    QuickArray<int> import_id_to_base_id{}; // preproc id -> import id, import id -> import id
    void map_id(u32 preproc_id, u32 import_id) {
        Assert(preproc_id > import_id);
        if(import_id_to_base_id.size() <= preproc_id) {
            import_id_to_base_id.resize(preproc_id + 1);
        }
        // Assert if the slot was mapped already.
        Assert(import_id_to_base_id[preproc_id] == 0);
        Assert(import_id_to_base_id[import_id] == 0);
        import_id_to_base_id[preproc_id] = import_id;
        import_id_to_base_id[import_id] = import_id;
    }
    u32 get_map_id(u32 id) {
        return import_id_to_base_id[id];
    }
    
    DynamicArray<CompilerTask> tasks;

    void addTask_type_body(ASTFunction* ast_func, FuncImpl* func_impl);
    void addTask_type_body(u32 import_id);
    
    // DynamicArray<u32> queue_import_ids;
        
    // Lex, preprocess, parse.
    // Multiple threads can call this function to perform
    // processing faster.
    void processImports();
    
    // does stuff based on options (compiling, executing...)
    void run(CompileOptions* options);
    
    // path can be absolute, relative to CWD, relative to the file's directory where the import was specified, or available in the import directories
    // adds task if new import was created
    u32 addOrFindImport(const std::string& path, const std::string& dir_of_origin_file = "", std::string* assumed_path_on_error = nullptr);
    // addImport existed but was removed because of addOrFindImport
    void addDependency(u32 import_id, u32 dep_import_id, const std::string& as_name = "", bool disabled = false);
    void addLibrary(u32 import_id, const std::string& path, const std::string& as_name = "");
    
    DynamicArray<Path> importDirectories;
    Path findSourceFile(const Path& path, const Path& sourceDirectory = "", std::string* assumed_path_on_error = nullptr);
    
    DynamicArray<std::string> linkDirectives; // no duplicates, passed to the linker, not with interpreter
    engone::Mutex lock_miscellaneous;
    // thread safe, duplicates will be ignored
    void addLinkDirective(const std::string& text);

    double compute_last_modified_time();

    struct Error {
        CompileError errorType;
        u32 line;
    };
    DynamicArray<Error> errorTypes;
    void addError(const lexer::SourceLocation& location, CompileError errorType = ERROR_UNSPECIFIED);
    void addError(const lexer::Token& token, CompileError errorType = ERROR_UNSPECIFIED);

    // #############################
    //    Generator stuff
    // #############################
    #define VAR_INFOS 0
    #define VAR_MEMBERS 1
    #define VAR_STRINGS 2
    #define VAR_COUNT 3
    IdentifierVariable* varInfos[VAR_COUNT]{nullptr};
    int dataOffset_types = -1;
    int dataOffset_members = -1;
    int dataOffset_strings = -1;

    engone::Mutex lock_imports;

    
    const char* const TEMP_TINYCODE_NAME = "_comp_time_";
    TinyBytecode* temp_tinycode = nullptr;
    TinyBytecode* get_temp_tinycode() {
        if(!temp_tinycode) {
            temp_tinycode = bytecode->createTiny(TEMP_TINYCODE_NAME, BETCALL);
        }
        temp_tinycode->restore_to_empty();
        return temp_tinycode;
    }

private:
    engone::Semaphore lock_wait_for_imports;
    bool signaled = true;
    volatile int waiting_threads = 0;
    volatile int total_threads = 0;
    volatile int next_thread_id = 0;
    
};