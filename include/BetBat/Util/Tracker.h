#pragma once
/*
    The purpose of these functions is to track allocations.
    This makes it easy to catch memory leaks.

    What you want:
        List of allocated objects
        Very automated. You should't need to name the allocations.
        Option to turn off since tracking will cause overhead
        Information of where in the code the allocation came from (need macros for this or you pass this info through arguments). Try macros since you want it to be automatic.

    Side note:
        Tracker.h and Tracker.cpp have gotten a little strange because I wanted to
        use DynamicArray in the tracker for storing counts and types.
        But Array.h (where DynamicArray is) can't be included here because it 
        uses Tracker.h. We would get circular includes. We can't put method definitions from DynamicArray
        into a .cpp file and include Tracker.h there instead of Array.h because the methods uses templates.
        C++ is really annoying because the two options is doing stupid stuff here or
        create a non-template definition for every template function where the tracker is used.
        Sigh.
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
// struct Tracker_impl;
struct Tracker {
    // static Tracker_impl* Create();
    // static void Destroy(Tracker_impl* tracker);
    static void DestroyGlobal();
    // enabled by default
    static void SetTracking(bool enabled);
    static void AddTracking(const std::type_info& typeInfo, u32 size, const TrackLocation& loc = {}, u32 count = 1);
    static void DelTracking(const std::type_info& typeInfo, u32 size, const TrackLocation& loc = {}, u32 count = 1);
    static void PrintTrackedTypes();
    // how much memory the tracker uses
    static u32 GetMemoryUsage();

    // // enabled by default
    // void setTracking(bool enabled);
    // void addTracking(const std::type_info& typeInfo, u32 size, const TrackLocation& loc = {}, u32 count = 1);
    // void delTracking(const std::type_info& typeInfo, u32 size, const TrackLocation& loc = {}, u32 count = 1);
    // void printTrackedTypes();
    // // how much memory the tracker uses
    // u32 getMemoryUsage();
};
#endif // NO_TRACKER