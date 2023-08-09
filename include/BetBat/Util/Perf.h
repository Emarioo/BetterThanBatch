#pragma once

#ifndef NO_PERF
#include <unordered_map>
#include <intrin.h>

/*
    std::unordered_map was used before but it was really slow.
    Instead we use __COUNTER__ to get a unique number for each scope.
    Then we have a static array which we index into using the unique numbers
*/

#define PERF_WITH_MAP

#ifdef LOG_MEASURES
#define MEASURE MeasureScope scopeMeasure = {__FUNCTION__, __COUNTER__};
struct ScopeStat {
    const char* fname=0;
    u64 counter=0;
    u32 hits=0;
};
#define SCOPE_STAT_ARRAY 1000
extern ScopeStat scopeStatArray[SCOPE_STAT_ARRAY];
extern std::unordered_map<const char*, u32> scopeStatMap;
struct MeasureScope {
    inline MeasureScope(const char* str, u32 id) : fname(str), statId(id), startPoint(__rdtsc()) {}

    inline ~MeasureScope(){
        u64 now = __rdtsc();
        // Don't use assert for speed improvement
        // Assert(statId < SCOPE_STAT_ARRAY);
        ScopeStat* scopeStat;
        #ifdef PERF_WITH_MAP
        auto pair = scopeStatMap.find(fname);
        if(pair == scopeStatMap.end()) {
            u32 index = scopeStatMap.size();
            scopeStatMap[fname] = index;
            scopeStat = scopeStatArray + index;
        } else {
            scopeStat = scopeStatArray + pair->second;
        }
        #else
            scopeStat = scopeStatArray + statId;
        #endif
        scopeStat->counter += now-startPoint;
        scopeStat->hits++;
        scopeStat->fname = fname;
    }
    const char* fname;
    u32 statId;
    u64 startPoint;
};
enum PrintFilters : u32 {
    NO_FILTER = 0,
    MIN_MICROSECOND = 0x1,
    SORT_HITS = 0x2,
    SORT_TIME = 0x4,
};
void PrintMeasures(u32 filters = SORT_TIME, u32 limit=(u32)-1);
#endif // LOG_MEASURES
#endif // NO_PERF

#ifndef MEASURE
#define MEASURE
#endif