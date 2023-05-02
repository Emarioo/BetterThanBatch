
typedef double Decimal;

// Major config
#define DEBUG
// #define SILENT

// Language config
#define INST_LIMIT 999999999
#define PREPROC_REC_LIMIT 100
#define PREPROC_TERM "#"

// Debug config
#ifdef DEBUG

// #define TLOG
// #define PLOG
#define GLOG
// #define OLOG
// #define CLOG_THREAD
// #define CLOG

// #define USE_DEBUG_INFO
// #define PRINT_DEBUG_LINES
#endif

#ifdef GLOG
#define USE_DEBUG_INFO
#endif

#ifdef CLOG
#ifndef USE_DEBUG_INFO
#define USE_DEBUG_INFO
#endif
#define PRINT_DEBUG_LINES
#endif


#define LOG_TOKENIZER 1
#define LOG_PREPROCESSOR 2
#define LOG_PARSER 4
#define LOG_OPTIMIZER 8
#define LOG_INTERPRETER 16
#define LOG_THREADS 32
#define LOG_OVERVIEW 64
#define LOG_ANY -1

void SetLog(int type, bool active);
bool GetLog(int type);

#ifdef TLOG
#define _TLOG(x) x
#else
#define _TLOG(x) if(GetLog(LOG_TOKENIZER)){x}
#endif

#ifdef PLOG
#define _PLOG(x) x
#else
#define _PLOG(x) if(GetLog(LOG_PREPROCESSOR)){x}
#endif

#ifdef GLOG
#define _GLOG(x) x
#else
#define _GLOG(x) if(GetLog(LOG_PARSER)){x}
#endif

#ifdef OLOG
#define _OLOG(x) x
#else
#define _OLOG(x) if(GetLog(LOG_OPTIMIZER)){x;}
#endif

#ifdef CLOG
#define _CLOG(x) x
#else
#define _CLOG(x) if(GetLog(LOG_INTERPRETER)){x;}
#endif

#ifdef CLOG_THREAD
#define _CLOG_THREAD(x) x
#else
#define _CLOG_THREAD(x) if(GetLog(LOG_THREADS)){x;}
#endif

#ifdef VLOG
#define _VLOG(x) x
#else
#define _VLOG(x) if(GetLog(LOG_ANY)) {x}
#endif