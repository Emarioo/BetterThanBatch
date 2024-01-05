#pragma once
#include "Engone/Logger.h"

enum TargetPlatform : u32 {
    TARGET_UNKNOWN = 0,
    TARGET_BYTECODE,
    // TODO: Add some option for COFF or ELF format? Probably not here.
    TARGET_WINDOWS_x64,
    TARGET_UNIX_x64,

    TARGET_END,
    TARGET_START = TARGET_UNKNOWN + 1,
};
const char* ToString(TargetPlatform target);
engone::Logger& operator<<(engone::Logger& logger,TargetPlatform target);
TargetPlatform ToTarget(const std::string& str);
// Also known as linker tools
enum LinkerChoice : u32 {
    LINKER_UNKNOWN = 0,
    LINKER_GCC,
    LINKER_MSVC,
    LINKER_CLANG,

    LINKER_END,
    LINKER_START = LINKER_UNKNOWN + 1,
};
const char* ToString(LinkerChoice v);
engone::Logger& operator<<(engone::Logger& logger,LinkerChoice v);
LinkerChoice ToLinker(const std::string& str);
