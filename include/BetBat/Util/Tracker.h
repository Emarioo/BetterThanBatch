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

    HOLY SHIT!
        The tracker should exist per thread (thread local storage?).
        Otherwise we will use mutexes every time you allocate so that
        the tracker isn't susceptible to race conditions.
        THAT'S WHY THE PARSER IS SO F***** SLOW. (sorry for the swearing, i just got excited because this has been troubling me for a while)
        -Emarioo, 2024-01-28
*/

// #include "BetBat/Config.h"

// #include "Engone/Typedefs.h"
// #include "Engone/Logger.h"

// #include "Engone/PlatformLayer.h"
// #include "Engone/Asserts.h"

// #include <unordered_map>
// #include <string>
// #include <typeinfo>

// struct DebugLocation {
//     const char* file;
//     int line;
//     int column;
// };
// #define HERE DebugLocation{__FILE__,__LINE__,__COLUMN__}

// #define CONSTRUCT(TYPE, LOC, ...) (AddTracking(typeid(TYPE), sizeof(TYPE), LOC), new(engone::Allocate(sizeof(TYPE))TYPE(__VA_ARGS__)))
// #define CONSTRUCT_ARRAY(TYPE, COUNT, LOC) (AddTracking(typeid(TYPE), sizeof(TYPE), LOC, COUNT), engone::Allocate(sizeof(TYPE) * COUNT))
// #define DECONSTRUCT(PTR, TYPE, LOC) (DelTracking(typeid(TYPE), sizeof(TYPE), LOC), PTR->~TYPE(), engone::Free(PTR, sizeof(TYPE)))
// #define DECONSTRUCT_ARRAY(PTR, TYPE, COUNT, LOC) (DelTracking(typeid(TYPE), sizeof(TYPE), LOC, COUNT), engone::Free(PTR, sizeof(TYPE) * COUNT))

// void EnableTracking(bool enabled);
// void AddTracking(const std::type_info& typeInfo, u32 size, const DebugLocation& loc, u32 count = 1);
// void DelTracking(const std::type_info& typeInfo, u32 size, const DebugLocation& loc, u32 count = 1);
// void PrintTrackedTypes();
// int GetTrackerMemoryUsage();

