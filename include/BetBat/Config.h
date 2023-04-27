
typedef double Decimal;

// Major config
// #define DEBUG
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
// #define USE_DEBUG_INFO
// #define PRINT_DEBUG_LINES
// #define CLOG_THREAD
// #define CLOG
#endif


#ifndef SILENT
#define _SILENT(X) X
#else
#define _SILENT(X)
#endif