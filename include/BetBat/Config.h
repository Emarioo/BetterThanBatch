#pragma once

typedef double Decimal;

// Major config
#define DEBUG
// #define SILENT

// not added bug would be nice
// #define DEBUG_RESIZE

// Language config
#define INST_LIMIT 999999999
#define PREPROC_REC_LIMIT 100
#define PREPROC_TERM "#"

// Debug config
#ifdef DEBUG

#define VLOG

// #define TLOG
#define TLOG_IMPORTS

// #define MLOG
// #define PLOG
// #define GLOG
// #define OLOG
// #define ILOG_THREAD
// #define ILOG


// #define USE_DEBUG_INFO
// #define PRINT_DEBUG_LINES
#endif

#ifdef PLOG
#define USE_DEBUG_INFO
#endif

#ifdef ILOG
#ifndef USE_DEBUG_INFO
#define USE_DEBUG_INFO
#endif
#define PRINT_DEBUG_LINES
#endif


#define LOG_TOKENIZER 1
#define LOG_PREPROCESSOR 2
#define LOG_PARSER 4
#define LOG_GENERATOR 4
#define LOG_OPTIMIZER 8
#define LOG_INTERPRETER 16
#define LOG_THREADS 32
#define LOG_OVERVIEW 64
#define LOG_ANY -1

void SetLog(int type, bool active);
bool GetLog(int type);

#ifdef TLOG
#define _TLOG(x) x
#elif defined(DEBUG)
#define _TLOG(x) if(GetLog(LOG_TOKENIZER)){x}
#else
#define _TLOG(x)
#endif

#ifdef MLOG
#define _MLOG(x) x
#elif defined(DEBUG)
#define _MLOG(x) if(GetLog(LOG_PREPROCESSOR)){x}
#else
#define _MLOG(x)
#endif

#ifdef PLOG
#define _PLOG(x) x
#elif defined(DEBUG)
#define _PLOG(x) if(GetLog(LOG_PARSER)){x}
#else
#define _PLOG(x)
#endif

#ifdef GLOG
#define _GLOG(x) x
#elif defined(DEBUG)
#define _GLOG(x) if(GetLog(LOG_INTERPRETER)){x;}
#else
#define _GLOG(x)
#endif

#ifdef OLOG
#define _OLOG(x) x
#elif defined(DEBUG)
#define _OLOG(x) if(GetLog(LOG_OPTIMIZER)){x;}
#else
#define _OLOG(x)
#endif

#ifdef ILOG
#define _ILOG(x) x
#elif defined(DEBUG)
#define _ILOG(x) if(GetLog(LOG_INTERPRETER)){x;}
#else
#define _ILOG(x)
#endif

#ifdef ILOG_THREAD
#define _ILOG_THREAD(x) x
#elif defined(DEBUG)
#define _ILOG_THREAD(x) if(GetLog(LOG_THREADS)){x;}
#else
#define _ILOG_THREAD(x)
#endif

#ifdef VLOG
#define _VLOG(x) x
#elif defined(DEBUG)
#define _VLOG(x) if(GetLog(LOG_ANY)) {x}
#else
#define _VLOG(x)
#endif