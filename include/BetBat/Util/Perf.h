#pragma once

#include "BetBat/Util/Utility.h"

#define MEASURE MeasureScope scopeMeasure = {__FUNCTION__};

struct ScopeStat {
    const char* fname=0;
    double time=0;
    u32 hits=0;
};
extern std::unordered_map<u64, ScopeStat> scopeStatMap;
struct MeasureScope {
    MeasureScope(const char* str) : fname(str){
        tp = engone::MeasureSeconds();
    }
    ~MeasureScope(){
        double t = engone::StopMeasure(tp);
        auto pair = scopeStatMap.find((u64)fname);
        if(pair == scopeStatMap.end()){
            scopeStatMap[(u64)fname] = {fname,t,1};
        }else{
            pair->second.time += t;
            pair->second.hits++;
        }
    }
    const char* fname;
    engone::TimePoint tp;
};
enum PrintFilters : u32 {
    NO_FILTER = 0,
    MIN_MICROSECOND = 0x1,
    SORT_HITS = 0x2,
    SORT_TIME = 0x4,
};
void PrintMeasures(u32 filters = SORT_TIME, u32 limit=(u32)-1);