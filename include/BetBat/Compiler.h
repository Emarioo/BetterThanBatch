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
    // does not include .
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

    void printSuccess(CompileOptions* options);
    void printFailed();
    void printWarnings();
};
struct CompileOptions {
    CompileOptions() = default;
    // CompileOptions(engone::Memory<char> source) : rawSource(source) {}
    // CompileOptions(const std::string& sourceFile) : initialSourceFile(sourceFile) {}
    // CompileOptions(const std::string& sourceFile, const std::string& compilerDirectory) 
    // : initialSourceFile(sourceFile), compilerDirectory(
    //     compilerDirectory.empty() ? "" : 
    //         (compilerDirectory.back() == '/' ? compilerDirectory : 
    //             (compilerDirectory + "/"))
    // ), silent(false) {}

    // engone::Memory<char> rawSource{}; // Path
    Path initialSourceFile; // Path
    TextBuffer initialSourceBuffer;
    Path resourceDirectory{"/"}; // Where resources for the compiler are located. Typically modules.
    DynamicArray<Path> importDirectories; // Additional directories where imports can be found.
    TargetPlatform target = BYTECODE;
    Path outputFile{};
    DynamicArray<std::string> userArgs; // arguments to pass to the interpreter or executable
    bool silent=false;
    bool singleThreaded = false;
    bool useDebugInformation = false;
    bool executeOutput = false;
    u32 threadCount=3;

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

    CompileStats compileStats;
};
// NOTE: struct since more info may be added to each import name
struct StreamToProcess {
    // these are read only
    TokenStream* initialStream = nullptr; // from tokenizer
    TokenStream* finalStream = nullptr; // preprocessed
    std::string as="";
    int dependencyIndex = 0x7FFFFFFF;
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
    void cleanup();
    
    // TODO: allocator for streams
    std::unordered_map<std::string, StreamToProcess*> tokenStreams; // import streams

    QuickArray<StreamToProcess*> streams;

    std::unordered_map<std::string, TokenStream*> includeStreams;

    DynamicArray<Path> importDirectories;
    DynamicArray<std::string> linkDirectives; // passed to the linker, not with interpreter
    // thread safe, duplicates will be ignored
    void addLinkDirective(const std::string& name);

    // thread safe
    // StreamToProcess* addStream(TokenStream* stream);
    // returns null if the stream's name already exists.
    StreamToProcess* addStream(const Path& path);
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

    DynamicArray<SourceToProcess> sourcesToProcess{};
    engone::Semaphore sourceWaitLock{};
    engone::Mutex sourceLock{};
    int waitingThreads = 0;
    int availableThreads = 0;
    bool waitForContent = false;
    bool signaled=false;
    // int readDependencyIndex = 0;
    int completedDependencyIndex = 0;

    engone::Mutex streamLock{}; // tokenStream
    // engone::Mutex includeStreamLock{};
    engone::Mutex otherLock{};
    
    // thread safe
    void addStats(i32 errors, i32 warnings);
    void addStats(i32 lines, i32 blankLines, i32 commentCount, i32 readBytes);

    CompileOptions* compileOptions = nullptr;

    AST* ast=0;
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
bool ExportExecutable(CompileOptions* options, Bytecode* bytecode);
void RunBytecode(CompileOptions* options, Bytecode* bytecode);

void CompileAndRun(CompileOptions* options);
void CompileAndExport(CompileOptions* options);

// bool ExportBytecode(CompileOptions* options, Bytecode* bytecode);
// // Content of userArgs is copied
// Bytecode* ImportBytecode(Path filePath);