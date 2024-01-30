#pragma once

#ifndef NO_PERF
#include "tracy/Tracy.hpp"
// #define MUTEX(NAME) TracyLockable(engone::Mutex,NAME)
#define MUTEX(NAME) engone::Mutex NAME
#else
// NativeLayer/Win32 needs this
#define TracyAlloc(...)
#define TracyFree(...)
#endif
// #if !defined(NO_PERF) && defined(TRACY_ENABLE)

// // #define MEASURE ZoneScoped;
// // #define MEASURE_WHO(CSTR)

// #else

// // #define MEASURE
// // #define MEASURE_WHO(CSTR)

// #endif