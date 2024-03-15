#pragma once

#include "BetBat/Compiler.h"

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
    std::unordered_map<std::string, TokenStream*> includeStreams;

    QuickArray<StreamToProcess*> streams;
    QuickArray<int> compileOrder;

    DynamicArray<Path> importDirectories;
    DynamicArray<std::string> linkDirectives; // no duplicates, passed to the linker, not with interpreter
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

