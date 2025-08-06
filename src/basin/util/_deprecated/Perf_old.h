#pragma once
#ifdef gone
// #include "tracy/Tracy.hpp"

#define NO_PERF

#ifndef NO_PERF
#include <unordered_map>

#ifdef OS_WINDOWS
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include "Engone/PlatformLayer.h"

/*
    std::unordered_map was used before but it was really slow.
    Instead we use __COUNTER__ to get a unique number for each scope.
    Then we have a static array which we index into using the unique numbers
*/

// #define PERF_WITH_MAP

#ifdef LOG_MEASURES

#define MEASURE MeasureScope scopeMeasure = {__FUNCTION__, __COUNTER__};
#define MEASURE_WHO(CSTR) MeasureScope scopeMeasure = {CSTR, __COUNTER__};

// #define MEASURE_THREAD() 
// #define MEASURE_SCOPE PerfZone perfZone = {__FUNCTION__, __COUNTER__};
// #define MEASURE_SCOPE_N(CSTR) PerfZone perfZone = {CSTR, __COUNTER__};
// void InitializePerf();
// struct PerfContext {
//     // ScopeStat scopeStatArray[SCOPE_STAT_ARRAY];
//     // std::unordered_map<const char*, u32> scopeStatMap;
//     PerfThread* perfThreads[16];
//     int usedThreads = 0;

//     engone::TLSIndex perfThreadTLS;
// };
// extern PerfContext global_perfContext;
// struct PerfThread {
//     void init(){
//         max = 1000;
//         perfZones = (PerfZone**)engone::Allocate(sizeof(PerfZone*) * max);
//         used = 0;
//     }
//     void cleanup(){
//         engone::Free(perfZones, sizeof(PerfZone*) * max);
//         max = 0;
//         used = 0;
//     }
//     PerfZone** perfZones = nullptr;
//     int used = 0;
//     int max = 0;
//     int indexInContext = 0;

//     int 
// };
// struct PerfZone {
//     PerfZone(const char* name) {
//         fname = name;
//         startCycles = __rdtsc();

//         auto perfThread = (PerfThread*)engone::Thread::GetTLSValue(global_perfContext.perfThreadTLS);
//         perfThread->perfZones
//     }
//     ~PerfZone() {
//         endCycles = __rdtsc();


//         // engone::Thread::SetTLSValue(measureParentTLSIndex,parent);
//     }
//     u64 startCycles = 0;
//     u64 endCycles = 0;
//     const char* fname=0;
//     // int hits = 0;
// };


struct ScopeStat {

    // u64 totalCycles=0;
    // u64 uniqueCycles=0;
    // u32 hits=0;
    
    const char* fname=0;
    volatile long long totalCycles=0;
    volatile long long uniqueCycles=0;
    volatile long hits=0;
};
struct MeasureScope;
#define SCOPE_STAT_ARRAY 1000
extern ScopeStat scopeStatArray[SCOPE_STAT_ARRAY];
extern std::unordered_map<const char*, u32> scopeStatMap;
extern engone::Mutex scopeStatLock;
// extern MeasureScope* parentMeasureScope;
extern engone::TLSIndex measureParentTLSIndex;

struct MeasureScope {
    const char* fname;
    // volatile long statId;
    volatile long outsideCycles;
    // volatile long long startPoint;
    u32 statId;
    // u32 outsideCycles;
    u64 startPoint;
    MeasureScope* parent;
    MeasureScope(const char* str, u32 id) : fname(str), outsideCycles(0), statId(id), startPoint(__rdtsc())
        // ,parent((MeasureScope*)engone::Thread::GetTLSValue(measureParentTLSIndex))
        // ,parent(parentMeasureScope)
    {
        parent = (MeasureScope*)engone::Thread::GetTLSValue(measureParentTLSIndex);
        engone::Thread::SetTLSValue(measureParentTLSIndex,this);
        // parent = parentMeasureScope;
        // parentMeasureScope = this;
    }

    ~MeasureScope(){
        u64 now = __rdtsc();
        // Don't use assert for speed improvement
        // Assert(statId < SCOPE_STAT_ARRAY);
        ScopeStat* scopeStat;
        #ifdef PERF_WITH_MAP
        scopeStatLock.lock();
        auto pair = scopeStatMap.find(fname);
        if(pair == scopeStatMap.end()) {
            u32 index = scopeStatMap.size();
            scopeStatMap[fname] = index;
            scopeStat = scopeStatArray + index;
        } else {
            scopeStat = scopeStatArray + pair->second;
        }
        scopeStatLock.unlock();
        #else
        // Assert(statId < SCOPE_START_ARRAY);
        scopeStat = scopeStatArray + statId;
        #endif
        
        u64 diff = now-startPoint;
        scopeStat->fname = fname;

        #ifdef OS_WINDOWS
        if(parent) {
            _interlockedadd(&parent->outsideCycles, diff);
        }
        _interlockedadd64(&scopeStat->totalCycles, diff);
        _interlockedadd64(&scopeStat->uniqueCycles, diff-outsideCycles);
        _InterlockedIncrement(&scopeStat->hits);
        #elif defined(OS_LINUX)
        scopeStatLock.lock();
        if(parent) {
            parent->outsideCycles += diff;
        }
        scopeStat->totalCycles += diff;
        scopeStat->uniqueCycles += diff-outsideCycles;
        scopeStat->hits++;
        scopeStatLock.unlock();
        #endif

        engone::Thread::SetTLSValue(measureParentTLSIndex,parent);
        // parentMeasureScope = parent;
    }
};
enum PrintFilters : u32 {
    NO_FILTER = 0,
    MIN_MICROSECOND = 0x1,
    SORT_HITS = 0x2,
    SORT_TIME = 0x4,
};
void MeasureInit();
void PrintMeasures(u32 filters = SORT_TIME, u32 limit=(u32)-1);
u32 MeasureGetMemoryUsage();
void MeasureCleanup();
#else // LOG_MEASURES
#define MeasureInit(...)
#define PrintMeasures(...)
#define MeasureGetMemoryUsage(...) 0
#define MeasureCleanup(...)
#endif // LOG_MEASURES
#else // NO_PERF
#define MeasureInit(...)
#define PrintMeasures(...)
#define MeasureGetMemoryUsage(...) 0
#define MeasureCleanup(...)
#endif // NO_PERF

#ifndef MEASURE

#ifdef TRACY_ENABLE
#define MEASURE ZoneScoped;
#else
#define MEASURE
#endif

#endif

#ifndef MEASURE_WHO

// #ifdef TRACY_ENABLE
// #define MEASURE_WHO(CSTR) ZoneScoped;
// #else
#define MEASURE_WHO(CSTR)
// #endif

#endif
#endif