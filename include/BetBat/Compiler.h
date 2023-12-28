#pragma once

#include "BetBat/Preprocessor.h"
#include "BetBat/Parser.h"
#include "BetBat/TypeChecker.h"
#include "BetBat/Generator.h"
#include "BetBat/Interpreter.h"
#include "BetBat/NativeRegistry.h"
#include "BetBat/COFF.h"
#include "BetBat/ELF.h"
#include "BetBat/x64_Converter.h"
#include "BetBat/UserProfile.h"
#include "BetBat/CompilerEnums.h"
// #include "BetBat/MessageTool.h"

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
    u16 major; // once a year
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

    Path sourceFile{};
    Path outputFile{};
    TargetPlatform target = CONFIG_DEFAULT_TARGET;
    LinkerChoice linker = CONFIG_DEFAULT_LINKER;
    std::string linker_cmd = "";

    bool useDebugInformation = false;
    bool silent=false;
    bool verbose=false;
    bool instant_report = true;

    Path modulesDirectory{"./modules/"}; // Where the standard library can be found. Typically "modules"

    bool executeOutput = false;
    DynamicArray<std::string> userArguments; // Ignored if output isn't executed. Arguments to pass to the interpreter or executable

    TextBuffer initialSourceBuffer;
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
    int addTestLocation(TokenRange& range);

    CompileStats compileStats{};
};
// NOTE: struct since more info may be added to each import name
struct StreamToProcess {
    // these are read only
    TokenStream* initialStream = nullptr; // from tokenizer
    TokenStream* finalStream = nullptr; // preprocessed
    std::string as="";
    // #define INVALID_DEPENDENCY_INDEX 0x7FFFFFFF
    // int dependencyIndex = INVALID_DEPENDENCY_INDEX;
    QuickArray<int> dependencies;
    bool available = true; // for processing
    bool completed = false;
    int index = 0;
};
struct SourceToProcess {
    Path path = "";
    std::string as = "";
    TextBuffer* textBuffer = nullptr;
    StreamToProcess* stream = nullptr;
    // StreamToProcess* dependent = nullptr;
};
// ThreadLexingInfo was another name I considered but
// it didn't seem appropriate since the thread may do more than lexing in the future.
struct ThreadCompileInfo {

    CompileInfo* info = nullptr;

    engone::Thread _thread{}; // should not be touched by the thread itself
};
struct CompileInfo {
    ~CompileInfo() { cleanup(); }
    void cleanup();
    
    // TODO: allocator for streams
    std::unordered_map<std::string, StreamToProcess*> tokenStreams; // import streams

    QuickArray<StreamToProcess*> streams;
    QuickArray<int> compileOrder;

    std::unordered_map<std::string, TokenStream*> includeStreams;

    DynamicArray<Path> importDirectories;
    DynamicArray<std::string> linkDirectives; // passed to the linker, not with interpreter
    // thread safe, duplicates will be ignored
    void addLinkDirective(const std::string& name);

    // thread safe
    // StreamToProcess* addStream(TokenStream* stream);
    // returns null if the stream's name already exists.
    StreamToProcess* addStream(const Path& path, StreamToProcess** existingStream = nullptr);
    // NOT thread safe
    StreamToProcess* getStream(const Path& name);
    // thread safe
    bool hasStream(const Path& name);
    int indexOfStream(const Path& name);

    // #ifdef SINGLE_THREADED
    // static const u32 THREAD_COUNT = 1;
    // #else
    // static const u32 THREAD_COUNT = 4;
    // #endif

    // returns absolute path
    Path findSourceFile(const Path& path, const Path& sourceDirectory = "");

    DynamicArray<SourceToProcess> sourcesToProcess{};
    engone::Semaphore emptyLock{}; // locks when empty
    engone::Mutex arrayLock{}; // lock when modifying the array

    engone::Semaphore sourceWaitLock{};
    engone::Mutex sourceLock{};
    int waitingThreads = 0;
    int availableThreads = 0;
    bool waitForContent = false;
    bool signaled=false;
    // int readDependencyIndex = 0;
    int completedStreams = 0;
    bool circularError = false;
    // int completedDependencyIndex = 0;

    engone::Mutex streamLock{}; // tokenStream
    // engone::Mutex includeStreamLock{};
    engone::Mutex otherLock{};
    
    // thread safe
    void addStats(i32 errors, i32 warnings);
    void addStats(i32 lines, i32 blankLines, i32 commentCount, i32 readBytes);

    CompileOptions* compileOptions = nullptr;
    
    Reporter reporter{};

    AST* ast = nullptr;
    // std::string compilerDir="";

    // It's a bit unfortunate that macros have moved into this struct when they belong to the preprocessor
    // but they are global so this is the natural way things went. You can make a struct in the preprocessor
    // which has all this functionality put then instantiate it here.
    long volatile globalUniqueCounter = 0; // type must be long volatile because of _InterlockedIncrement
    // i32 globalUniqueCounter = 0;
    std::unordered_map<std::string, RootMacro*> _rootMacros; // don't touch, not thread safe
    // thread safe
    // returns false if macro doesn't exist
    bool removeRootMacro(const Token& name);
    // returns false if macro doesn't exist
    bool removeCertainMacro(RootMacro* rootMacro, int argumentAmount, bool variadic);
    RootMacro* ensureRootMacro(const Token& name, bool ensureBlank);
    RootMacro* matchMacro(const Token& name);
    CertainMacro* matchArgCount(RootMacro* rootMacro, int count, bool includeInf);
    // localMacro should be thread safe
    void insertCertainMacro(RootMacro* rootMacro, CertainMacro* localMacro);

    engone::Mutex macroLock{};

    // DynamicArray<TokenStream*> streamsToClean{}; // streams which tokens are used somewhere.
};

Bytecode* CompileSource(CompileOptions* options);
bool ExportTarget(CompileOptions* options, Bytecode* bytecode);
bool ExecuteTarget(CompileOptions* options);
// bool ExecuteBytecode(CompileOptions* options, Bytecode* bytecode);

bool CompileAll(CompileOptions* options);