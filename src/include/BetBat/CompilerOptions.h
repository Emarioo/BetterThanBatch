#pragma once

#include "Engone/Logger.h"
#include "Engone/Util/Array.h"
#include "BetBat/Config.h"

enum TargetPlatform : u32 {
    TARGET_UNKNOWN = 0,
    TARGET_BYTECODE,
    // TODO: Add some option for COFF or ELF format? Probably not here.
    TARGET_WINDOWS_x64,
    TARGET_LINUX_x64,
    // TARGET_WASM_v1_0,
    
    TARGET_ARM,
    TARGET_AARCH64,

    TARGET_END, // start/end if you want iterate targets
    TARGET_START = TARGET_UNKNOWN + 1,
};
struct ArchitectureInfo {
    int FRAME_SIZE=-1;
    int REGISTER_SIZE=-1;
};
static const ArchitectureInfo ARCH_x86_64 = {16, 8};
static const ArchitectureInfo ARCH_aarch64 = {16, 8};
static const ArchitectureInfo ARCH_arm = {8, 4};
// Also known as linker tools
enum LinkerChoice : u32 {
    LINKER_UNKNOWN = 0,
    LINKER_GCC,
    LINKER_MSVC,
    LINKER_CLANG,

    LINKER_END,
    LINKER_START = LINKER_UNKNOWN + 1,
};
// Used by TestSuite
struct TextBuffer {
    std::string origin;
    char* buffer = nullptr;
    int size = 0;
    int startLine = 1;
    int startColumn = 1;
};
struct CompileOptions {
    CompileOptions() = default;
    ~CompileOptions() {
        
    }
    void cleanup() {
        userArguments.cleanup();
        importDirectories.cleanup();
        defined_macros.cleanup();
    }

    std::string source_file;
    std::string output_file;
    TargetPlatform target = CONFIG_DEFAULT_TARGET;
    LinkerChoice linker = CONFIG_DEFAULT_LINKER;
    // std::string linker_cmd = "";
    TextBuffer source_buffer; // pure text instead of a path to some file

    // bool useDebugInformation = true;
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
    bool cache_tests = false;
    bool show_profiling = false;
    std::string pattern_for_files;
    
    bool debug_qemu_with_gdb = false;
    std::string qemu_gdb_port = "1234";
    
    bool execute_in_vm = false;
    bool interactive_vm = false;
    bool logged_vm = false;

    std::string modulesDirectory = "./modules/"; // Where the standard library can be found. Typically "modules"

    DynamicArray<std::string> defined_macros;

    DynamicArray<std::string> userArguments; // Ignored if output isn't executed. Arguments to pass to the interpreter or executable

    DynamicArray<std::string> importDirectories; // Directories to look for imports (source files)
    int threadCount=0; // zero will use the CPU's number of core
};
// returns false if failure, error message is printed, you just have to exit the program
bool InterpretArguments(const BaseArray<std::string>& commands, CompileOptions* options);

const char* ToString(TargetPlatform target);
engone::Logger& operator<<(engone::Logger& logger,TargetPlatform target);
TargetPlatform ToTarget(const std::string& str);
const char* ToString(LinkerChoice v);
engone::Logger& operator<<(engone::Logger& logger,LinkerChoice v);
LinkerChoice ToLinker(const std::string& str);


enum Annotation : u32 {
    // DO NOT REARRANGE ANNOTATIONS!
    ANOT_INVALID = 0,
    ANOT_CUSTOM,
    // TODO: Add more annotations
};
extern const char* annotation_names[];