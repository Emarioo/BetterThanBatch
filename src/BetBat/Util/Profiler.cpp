#include "BetBat/Util/Profiler.h"

#include "Engone/Logger.h"
#include "Engone/Asserts.h"

void ProfilerSession::cleanup() {
    if(contextTLS)
        engone::Thread::DestroyTLSIndex(contextTLS);
    for(int i=0;i<usedContexts;i++) {
        auto context = contexts[i];
        context.cleanup();
    }
}
void ProfilerEnable(bool yes){
    global_profilerSession.allowNewContexts = yes;
}
void ProfilerInitThread() {
    global_profilerSession.initContextForThread();
}
void ProfilerSession::initContextForThread() {
    using namespace engone;
    if(!allowNewContexts)
        return;
    if(usedContexts == maxContexts) {
        log::out << log::RED << "Profiler has no more available contexts!\n";
        return;
    }
    #ifdef OS_WINDOWS
    long res = _InterlockedIncrement(&usedContexts);
    #elif OS_LINUX
    Assert(("LINUX impl. for profiling not thread safe",false));
    long res = usedContexts; // NOT THREAD SAFE
    #endif
    // log::out << "Good day "<<(res-1)<<"\n";
    ProfilerContext* context = &contexts[res-1];
    new(context)ProfilerContext();
    context->ensure(4096*4);
    bool yes = Thread::SetTLSValue(global_profilerSession.contextTLS, context);
    Assert(yes);
}
ProfilerSession global_profilerSession;
void ProfilerInitialize(){
    using namespace engone;
    global_profilerSession={};

    TLSIndex index = Thread::CreateTLSIndex();
    if (!index) {
        log::out << log::RED << "Profiler failed initialization. Could not get a thread local slot.\n";
        return;
    }

    global_profilerSession.contextTLS = index;
}
void ProfilerCleanup(){
    global_profilerSession.cleanup();
}
void ProfilerContext::ensure(int count) {
    if(max >= count)
        return;
    if(!zones) {
        zones = (ProfilerZone*)engone::Allocate(sizeof(ProfilerZone) * count);
        Assert(zones);
        max = count;
        used = 0;
    } else {
        reallocations++;
        ProfilerZone* newZones = (ProfilerZone*)engone::Reallocate(zones, sizeof(ProfilerZone) * max, sizeof(ProfilerZone) * count);
        Assert(newZones);
        zones = newZones;
        max = count;
    }
}
void ProfilerContext::insertNewZone(ProfilerZone* zone) {
    ensure(used + 1);
    zones[used++] = *zone;
}
void ProfilerContext::cleanup() {
    if(reallocations)
        engone::log::out << engone::log::RED << "Profiler did "<<reallocations<<"!\n";
    // destructors shouldn't have to be called on the zones
    engone::Free(zones, sizeof(ProfilerZone) * max);
    max = 0;
    zones = nullptr;
    used = 0;
}
void ProfilerPrint() {
    using namespace engone;
    log::out << log::BLUE << "##  Profiler  ##\n";
    for(int j=0;j<global_profilerSession.usedContexts;j++){
        auto& context = global_profilerSession.contexts[j];
        log::out << " Context "<<j<<"\n";
        // Current data is useless. You would print the same data as Perf.h has
        // but that data hasn't been implemented for the profiler yet.
        for(int i=0;i<context.used;i++){
            auto& zone = context.zones[i];

            log::out << " "<<zone.startCycles << " "<<zone.endCycles<<"\n";
        }
    }
}
// struct ProfilerData {
//     u32 contextCount;
//     struct Context {
//         u64 zoneCount;
//         struct Zone {
            

//         };
//         Zone zones[];
//     };    
//     Context contexts[];
//     Context 
// }

/*
struct DataFormat {
    u32 contextCount
    u32 zoneCounts[contextCount]

    struct Zone {
        u64 start;
        u64 end;
        u32 stringLength;
        u32 stringIndex; // this allows multiple zones to point to the same string
    } zones[zoneCounts];

    u32 stringDataSize;
    char stringData[stringDataSize]
}
*/
void ProfilerExport(const char* path) {
    using namespace engone;

    auto file = FileOpen(path, 0, FILE_ALWAYS_CREATE);

    u32 contextCount = global_profilerSession.usedContexts;
    FileWrite(file, &contextCount, sizeof(contextCount));

    for(int j=0;j<global_profilerSession.usedContexts;j++){
        auto& context = global_profilerSession.contexts[j];
        u32 zoneCount = context.used;
        FileWrite(file, &zoneCount, sizeof(zoneCount));
    }

    for(int j=0;j<global_profilerSession.usedContexts;j++){
        auto& context = global_profilerSession.contexts[j];

        // TODO: Strings
        FileWrite(file, context.zones, context.used * sizeof(ProfilerZone));
    }

    FileClose(file);
}