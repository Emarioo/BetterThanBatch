
#include "BetBat/Config.h"

#include "BetBat/Util/Perf.h"
// #include "Engone/Logger.h"
#include "BetBat/Util/Utility.h"
#include "BetBat/Util/Array.h"

#include <unordered_map>

#ifdef LOG_MEASURES
ScopeStat scopeStatArray[SCOPE_STAT_ARRAY]{0};
std::unordered_map<const char*, u32> scopeStatMap;
engone::Mutex scopeStatLock{};
MeasureScope* parentMeasureScope = nullptr;
engone::TLSIndex measureParentTLSIndex= 0;
static DynamicArray<ScopeStat> stats{};

void MeasureInit(){
    measureParentTLSIndex = engone::Thread::CreateTLSIndex();
    Assert(measureParentTLSIndex);
}
u32 MeasureGetMemoryUsage() {
    return stats.max * sizeof(ScopeStat);
}
void MeasureCleanup() {
    stats.cleanup();
}
// u64 parentScope=0;
void PrintMeasures(u32 filters, u32 limit){
    using namespace engone;
    // struct DisplayStat {
    //     std::string uniqueTime;
    //     std::string totalTime;
    //     std::string hits;
    //     std::string name;
    // };
    stats.resize(0);

    double totalTime = 0;
    for(int i=0;i<SCOPE_STAT_ARRAY;i++){
        ScopeStat* it = scopeStatArray + i;
        if(it->hits) {
            stats.add(*it);
            // log::out << "full "<<i<<"\n";
        }
    }
    // TODO: You could optimise sorting but a human isn't gonna
    //  notice the slowness if this function is called once.

    if((filters & SORT_HITS) || (filters & SORT_TIME)){
        for(int i=0;i<(int)stats.size();i++) {
            for(int j=0;j<(int)stats.size()-1;j++) {
                // TODO: Sorting is doesn't work when using hits and time at
                //  the same time. Fix it you lazy goofball!
                if(((filters & SORT_HITS) && stats[j].hits < stats[j+1].hits) ||
                    ((filters & SORT_TIME) && stats[j].uniqueCycles < stats[j+1].uniqueCycles)
                    // ((filters & SORT_TIME) && stats[j].time < stats[j+1].time)
                ){
                    ScopeStat tmp = stats[j];
                    stats[j] = stats[j+1];
                    stats[j+1] = tmp;
                }
            }
        }
    }

    log::out << log::BLUE << "Measure statistics:\n";
    int maxName = 0;
    int maxTotalTime = 0;
    int maxTime = 0;
    int maxHits = 0;
    u32 i = 0;
    for(auto& stat : stats){
        if(i >= limit)
            break;
        i++;
        double time = engone::DiffMeasure(stat.uniqueCycles);
        // double time = stat.time;
        if((filters & MIN_MICROSECOND) && time < 1e-6)
            continue; // Cannot use break unless you sort by time.
        
        const char* timeString = FormatTime(time);
        int len=0;
        len = strlen(timeString);
        if(maxTime < len)
            maxTime = len;

        double totaltime = engone::DiffMeasure(stat.totalCycles);
        const char* totalString = FormatTime(totaltime);
        len = strlen(totalString);
        if(maxTotalTime < len)
            maxTotalTime = len;

        len = log10(stat.hits);
        if(maxHits < len)
            maxHits = len;

        len = strlen(stat.fname);
        if(maxName < len)
            maxName = len;

    }
    #define SPACING(L)  for(int _j = 0; _j < (L);_j++) log::out << " ";
    i = 0;
    float summedTime = 0;
    for(auto& stat : stats){
        double time = engone::DiffMeasure(stat.uniqueCycles);
        double ttime = engone::DiffMeasure(stat.totalCycles);
        summedTime += time;
        if(i >= limit)
            break;
        // double time = stat.time;
        if((filters & MIN_MICROSECOND) && time < 1e-6)
            continue; // Cannot use break unless you sort by time.
        log::out << log::LIME;
        int len;
        len = log10(stat.hits);
        log::out << stat.hits<<", ";
        SPACING(maxHits - len)

        const char* timeString = FormatTime(time);
        len = strlen(timeString);
        log::out << timeString<<", ";
        SPACING(maxTime - len)

        const char* ttimeString = FormatTime(ttime);
        len = strlen(ttimeString);
        log::out << ttimeString<<", ";
        SPACING(maxTotalTime - len)


        log::out << log::SILVER<<(const char*)stat.fname;
        len = strlen(stat.fname);
        // SPACING(maxName - len)
        log::out << "\n";

        i++;
    }
    
    // Total time of all measurements is useless information
    log::out << log::LIME << " Total : "<<FormatTime(summedTime) <<log::GRAY<<" (threads may cause unexpected timings)\n";
}
#endif