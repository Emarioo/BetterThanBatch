#pragma once

// #include <unordered_map>


#ifdef OS_WINDOWS
#include <intrin.h>
#elif OS_LINUX
#include <x86intrin.h>
#endif

#include "Engone/PlatformLayer.h"

// TODO: Naming threads would be nice
#define PROFILE_THREAD ProfilerInitThread();
#define PROFILE_SCOPE ProfilerZone profilerZone = {__FUNCTION__, __COUNTER__};
// #define PROFILE_SCOPE_N(CSTR) PerfZone perfZone = {CSTR, __COUNTER__};
// Not thread safe. It's a good idea to call this before compiling source code when there's only the main thread.
void ProfilerEnable(bool yes);
void ProfilerInitialize();
void ProfilerInitThread();
void ProfilerCleanup();
// Make sure no threads continues to profile while printing
void ProfilerPrint();
// Make sure no threads continues to profile while exporting
void ProfilerExport(const char* path);
// One per thread
// Does not need to use mutexes as long
// as you stop profiling when it's time to sum up the data
// of all contexts.
struct ProfilerZone;
struct ProfilerContext {
    void cleanup();
    void ensure(int count);
    ProfilerZone* zones = nullptr;
    int used = 0;
    int max = 0;
    int reallocations = 0;

    void insertNewZone(ProfilerZone* zone);

};
// Global for all threads (or each session)
struct ProfilerSession {
    void cleanup();
    
    // the name of the variable isn't upper case because
    // the contexts may become a dynamic array.
    static const int maxContexts = 16;
    ProfilerContext contexts[maxContexts];
    volatile long usedContexts = 0;
    bool allowNewContexts = true;

    // init context for current thread
    // a slot in thread local storage is set
    void initContextForThread();

    engone::TLSIndex contextTLS=0;
};
extern ProfilerSession global_profilerSession;
struct ProfilerZone {
    ProfilerZone(const char* name, u32 id) {
        fname = name;
        startCycles = __rdtsc();
        // You may want to store calls stack and swap parents
        // as I did in Perf.h
        // sum up hits too
    }
    ~ProfilerZone() {
        endCycles = __rdtsc();
        auto context = (ProfilerContext*)engone::Thread::GetTLSValue(global_profilerSession.contextTLS);
        if(context) context->insertNewZone(this);
    }
    u64 startCycles = 0;
    u64 endCycles = 0;
    const char* fname=0;
};