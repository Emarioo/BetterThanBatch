#pragma once
#include "Engone/Logger.h"

enum TargetPlatform : u32 {
    UNKNOWN_TARGET = 0,
    TARGET_BYTECODE,
    // TODO: Add some option for COFF or ELF format? Probably not here.
    TARGET_WINDOWS_x64,
    TARGET_UNIX_x64,
};
const char* ToString(TargetPlatform target);
engone::Logger& operator<<(engone::Logger& logger,TargetPlatform target);
TargetPlatform ToTarget(const std::string& str);
// Also known as linker tools
enum LinkerChoice : u32 {
    UNKNOWN_LINKER = 0,
    LINKER_GCC,
    LINKER_MSVC,
    LINKER_CLANG,
};
const char* ToString(LinkerChoice v);
engone::Logger& operator<<(engone::Logger& logger,LinkerChoice v);
LinkerChoice ToLinker(const std::string& str);
