#pragma once
/*
    The purpose of these functions is to track allocations.
    This makes it easy to catch memory leaks.

    What you want:
        List of allocated objects
        Very automated. You should't need to name the allocations.
        Option to turn off since tracking will cause overhead
        Information of where in the code the allocation came from (need macros for this or you pass this info through arguments). Try macros since you want it to be automatic.
*/

#define TRACK_ADDS(TYPE, COUNT)
#define TRACK_DELS(TYPE, COUNT)

#define TRACK_ALLOC(TYPE) (TYPE*)engone::Allocate(sizeof(TYPE))
#define TRACK_FREE(PTR, TYPE) engone::Free(static_cast<TYPE*>(PTR), sizeof(TYPE))

#define TRACK_ARRAY_ALLOC(TYPE, COUNT) (TYPE*)engone::Allocate(sizeof(TYPE) * COUNT)
#define TRACK_ARRAY_FREE(PTR, TYPE, COUNT) engone::Free(static_cast<TYPE*>(PTR), sizeof(TYPE) * COUNT)


#ifndef NO_TRACKER
#include "BetBat/Config.h"

#include "Engone/Typedefs.h"
#include "Engone/Logger.h"

#include "Engone/PlatformLayer.h"
#include "Engone/Asserts.h"

#include <unordered_map>
#include <string>
#include <typeinfo>

// kind of needs to be in a struct because of C++'s awful templates and headers
struct TrackLocation {
    const char* fname=nullptr;
    u32 line = 0;
    u32 count = 0;
};
#define TRACK_LOCATION TrackLocation{__FILE__, __LINE__}
// #define TRACK_ADD(TYPE) Tracker::AddTracking(typeid(TYPE), sizeof(TYPE), TRACK_LOCATION);
// #define TRACK_DEL(TYPE) Tracker::DelTracking(typeid(TYPE), sizeof(TYPE), TRACK_LOCATION);

#ifdef ENABLE_TRACKER
#undef TRACK_ADDS
#undef TRACK_DELS
#undef TRACK_ALLOC
#undef TRACK_FREE
#undef TRACK_ARRAY_ALLOC
#undef TRACK_ARRAY_FREE
#define TRACK_ADDS(TYPE, COUNT) Tracker::AddTracking(typeid(TYPE), sizeof(TYPE), TRACK_LOCATION, COUNT);
#define TRACK_DELS(TYPE, COUNT) Tracker::DelTracking(typeid(TYPE), sizeof(TYPE), TRACK_LOCATION, COUNT);
#define TRACK_ALLOC(TYPE) (Tracker::AddTracking(typeid(TYPE), sizeof(TYPE), TRACK_LOCATION), (TYPE*)engone::Allocate(sizeof(TYPE)))
#define TRACK_FREE(PTR, TYPE) (Tracker::DelTracking(typeid(TYPE), sizeof(TYPE), TRACK_LOCATION), engone::Free(static_cast<TYPE*>(PTR), sizeof(TYPE)))
#define TRACK_ARRAY_ALLOC(TYPE, COUNT) (Tracker::AddTracking(typeid(TYPE), sizeof(TYPE), TRACK_LOCATION, COUNT), (TYPE*)engone::Allocate(sizeof(TYPE) * COUNT))
#define TRACK_ARRAY_FREE(PTR, TYPE, COUNT) (Tracker::DelTracking(typeid(TYPE), sizeof(TYPE), TRACK_LOCATION, COUNT), engone::Free(static_cast<TYPE*>(PTR), sizeof(TYPE) * COUNT))
#endif // ENABLE_TRACKER

// completly thread safe
// TODO init and cleanup functions
// you have to disable tracking when the destructors of
// global data is called. 
struct Tracker {
    // enabled by default
    static void SetTracking(bool enabled);
    static void AddTracking(const type_info& typeInfo, u32 size, const TrackLocation& loc = {}, u32 count = 1);
    static void DelTracking(const type_info& typeInfo, u32 size, const TrackLocation& loc = {}, u32 count = 1);
    static void PrintTrackedTypes();
    // how much memory the tracker uses
    static u32 GetMemoryUsage();
};
#endif // NO_TRACKER