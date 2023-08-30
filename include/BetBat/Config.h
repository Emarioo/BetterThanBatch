#pragma once

#ifdef WIN32
#define OS_WINDOWS
#elif defined(_linux_)
#define OS_LINUX
#endif

#ifdef OS_WINDOWS
#define OS_NAME "Windows"
#elif defined(OS_LINUX)
#define OS_NAME "Linux"
#else
#define OS_NAME "<os-none>"
#endif

/* ###############
   Major config
############### */

#define COMPILER_VERSION "0.1.0/hello-2023.8.29"

// DEV_FILE defaults to dev.btb if none is specified
// #define DEV_FILE "examples/debug_test.btb"
// #define DEV_FILE "examples/garb.btb"
// #define DEV_FILE "tests/simple/garb.btb"
#define CONFIG_DEFAULT_TARGET WINDOWS_x64

// #define RUN_TEST_SUITE
// #define RUN_TESTS "tests/simple/garb.btb"

// #define DEBUG

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
#define OPTIMIZED
// #define SINGLE_THREADED
// #define LOG_MEASURES
// Silent is not used at the moment.
#ifndef RELEASE
#define ENABLE_TRACKER
// #define LOG_TRACKER
#define LOG_MSG_LOCATION
#define DUMP_ALL_ASM
#endif

// #define LOG_ALLOCATIONS

// #define LOG_ALLOC
// Config.h is included in Alloc.cpp for alloc to see the macro.
// #define DEBUG_RESIZE

#define PREPROC_REC_LIMIT 100
// You could enforce hashtag (replace macro with hashtag)
// Hashtag will always be used. @ is taken, $ feels wrong, # makes you feel at home.
#define PREPROC_TERM "#"


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

#include "BetBat/Asserts.h"

// Debug config
#ifdef DEBUG


#define VLOG

// will print AST
#define AST_LOG

// #define TLOG

// (DEPRECATED) import list in tokenizfer
// #define LOG_IMPORTS

// newly tokenized includes in preprocessor
// #define LOG_INCLUDES


// #define MLOG_MATCH(X) X
// #define MLOG_MATCH(X)
// #define MLOG
// #define PLOG
// type checker
// #define TC_LOG
#define GLOG
// #define ILOGf
// #define ILOG_REGS
// x64 converter
// #define CLOG

// #define OLOG
// #define USE_DEBUG_INFO
#endif

/*
   Don't touch these
*/

#ifdef PLOG
#define USE_DEBUG_INFO
#endif

#ifdef ILOG
#ifndef USE_DEBUG_INFO
#define USE_DEBUG_INFO
#endif
#endif

#define LOG_TOKENIZER 1
#define LOG_PREPROCESSOR 2
#define LOG_PARSER 4
#define LOG_GENERATOR 4
#define LOG_OPTIMIZER 8
#define LOG_INTERPRETER 16
#define LOG_OVERVIEW 64
#define LOG_ANY -1

void SetLog(int type, bool active);
bool GetLog(int type);

#define INCOMPLETE Assert(("Incomplete",false));

#ifdef TLOG
#define _TLOG(x) x
// #elif defined(DEBUG)
// #define _TLOG(x) if(GetLog(LOG_TOKENIZER)){x}
#else
#define _TLOG(x)
#endif

#ifdef MLOG
#define _MLOG(x) x
// #elif defined(DEBUG)
// #define _MLOG(x) if(GetLog(LOG_PREPROCESSOR)){x}
#else
#define _MLOG(x)
#endif

#ifdef PLOG
#define _PLOG(x) x
// #elif defined(DEBUG)
// #define _PLOG(x) if(GetLog(LOG_PARSER)){x}
#else
#define _PLOG(x)
#endif

#ifdef TC_LOG
#define _TC_LOG(x) x
// #elif defined(DEBUG)
// #define _TC_LOG(x) if(GetLog()){x;}
#else
#define _TC_LOG(x)
#endif

#ifdef GLOG
#define _GLOG(x) x
// #elif defined(DEBUG)
// #define _GLOG(x) if(GetLog(LOG_INTERPRETER)){x;}
#else
#define _GLOG(x)
#endif

#ifdef OLOG
#define _OLOG(x) x
// #elif defined(DEBUG)
// #define _OLOG(x) if(GetLog(LOG_OPTIMIZER)){x;}
#else
#define _OLOG(x)
#endif

#ifdef CLOG
#define _CLOG(x) x
// #elif defined(DEBUG)
// #define _CLOG(x) if(GetLog()){x;}
#else
#define _CLOG(x)
#endif

#ifdef ILOG
#define _ILOG(x) x
// #elif defined(DEBUG)
// #define _ILOG(x) if(GetLog(LOG_INTERPRETER)){x;}
#else
#define _ILOG(x)
#endif

#ifdef VLOG
#define _VLOG(x) x
// #elif defined(DEBUG)
// #define _VLOG(x) if(GetLog(LOG_ANY)) {x}
#else
#define _VLOG(x)
#endif