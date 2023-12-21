#pragma once

#ifdef WIN32
#define OS_WINDOWS
#elif defined(_linux_)
#define OS_UNIX
#endif

#ifdef OS_WINDOWS
#define OS_NAME "Windows"
#elif defined(OS_UNIX)
#define OS_NAME "Unix"
#else
#define OS_NAME "<os-none>"
#endif

#include "Engone/Asserts.h"

/*
###############
   Major config
#################

Try to edit Config.cpp instead of this file because you will have to compile all headers and translation units otherwise.
*/

#define COMPILER_VERSION "0.2.0/unix-2023.12.20"

// DEV_FILE defaults to dev.btb if none is specified
// #define DEV_FILE "examples/debug_test.btb"
// #define DEV_FILE "examples/garb.btb"
// #define DEV_FILE "tests/simple/garb.btb"
#ifdef OS_WINDOWS
#define CONFIG_DEFAULT_TARGET TARGET_WINDOWS_x64
#define CONFIG_DEFAULT_LINKER LINKER_MSVC
#else
#define CONFIG_DEFAULT_TARGET TARGET_UNIX_x64
#define CONFIG_DEFAULT_LINKER LINKER_GCC
#endif

// #define RUN_TEST_SUITE
// #define RUN_TESTS "tests/simple/garb.btb"

#define DEBUG

// PDB should be used if not defined
#define USE_DWARF_AS_DEBUG

// should enable optimized options and disable slow ones.
// #define RELEASE

// Tries to produce a smaller object file and executable.
// for debugging purposes. This is not meant to be a small optimized build.
// #define MINIMAL_DEBUG
// Generates object file and debug information but no executable
// #define DISABLE_LINKING

// With this flag, some shortcuts and other improvements are made
// when compiling. Push and pop after each other is redundant for example.
// The consequence is that the logged instructions won't resemble the final
// output.
// #define OPTIMIZED
// #define SINGLE_THREADED
#define LOG_MEASURES
// Silent is not used at the moment.
#ifndef RELEASE
#define ENABLE_TRACKER
// #define LOG_TRACKER
#define LOG_MSG_LOCATION
// #define DUMP_ALL_ASM
#endif

// #define LOG_ALLOCATIONS

// #define LOG_ALLOC
// Config.h is included in Alloc.cpp for alloc to see the macro.
// #define DEBUG_RESIZE

#define PREPROC_REC_LIMIT 30

// THESE SHOULD BE OFF FOR THE COMPILER TO WORK PROPERLY
// #define DISABLE_BASE_IMPORT
// #define DISABLE_ZERO_INITIALIZATION
// Disabling this will most certainly produce flawed instructions
// but it will be easier to read the instructions when debugging
// #define ENABLE_FAULTY_X64
// Disables the non essential asserts
// not the ones that bound check in arrays
// or perhaps you do both?
#ifdef RELEASE
// all asserts shouldn't be disabled.
// bounds check can be disabled
// asserts that probably won't fail and
// are hit often can be disabled for performance.
// #define DISABLE_ASSERTS
#endif

#include "Engone/Asserts.h"

// Debug config
#ifdef DEBUG


// #define VLOG

// will print AST
// #define AST_LOG

// #define TLOG

// (DEPRECATED) import list in tokenizfer
// #define LOG_IMPORTS

// newly tokenized includes in preprocessor
// #define LOG_INCLUDES

// #define MLOG
// #define PLOG
// type checker
// #define TCLOG
// #define GLOG
// #define ILOGf
// #define ILOG_REGS
// x64 converter
// #define CLOG
// #define OLOG
#endif

/*
   Don't touch these
*/

#define INCOMPLETE Assert(("Incomplete",false));

enum LoggingSection : u32 {
    LOG_TOKENIZER       = 0x1,
    LOG_PREPROCESSOR    = 0x2,
    LOG_PARSER          = 0x4,
    LOG_TYPECHECKER     = 0x8,
    LOG_GENERATOR       = 0x10,
    LOG_OPTIMIZER       = 0x20,
    LOG_CONVERTER       = 0x40,
    LOG_INTERPRETER     = 0x80,
    LOG_OVERVIEW        = 0x100,
    LOG_MACRO_MATCH     = 0x200,
};
extern LoggingSection global_loggingSection;

#ifdef DEBUG
#define _BASE_LOG(F,...) { if(global_loggingSection & F) { __VA_ARGS__; } }
#define _TLOG(x)    _BASE_LOG(LOG_TOKENIZER,x)
#define _MLOG(x)    _BASE_LOG(LOG_PREPROCESSOR,x)
#define _PLOG(x)    _BASE_LOG(LOG_PARSER,x)
#define _TCLOG(x)   _BASE_LOG(LOG_TYPECHECKER,x)
#define _GLOG(x)    _BASE_LOG(LOG_GENERATOR,x)
#define _OLOG(x)    _BASE_LOG(LOG_OPTIMIZER,x)
#define _CLOG(x)    _BASE_LOG(LOG_CONVERTER,x)
#define _ILOG(x)    _BASE_LOG(LOG_INTERPRETER,x)
#define _VLOG(x)    _BASE_LOG(LOG_OVERVIEW,x)

#define _MMLOG(x)   _BASE_LOG(LOG_MACRO_MATCH,x)
#else
#define _TLOG(x) 
#define _MLOG(x) 
#define _PLOG(x) 
#define _TCLOG(x)
#define _GLOG(x) 
#define _OLOG(x) 
#define _CLOG(x) 
#define _ILOG(x) 
#define _VLOG(x) 

#define _MMLOG(x)
#endif
