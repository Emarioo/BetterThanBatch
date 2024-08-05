#pragma once

#include "tracy/Tracy.hpp"

// Enable/disable Mutex tracing (the whole codebase doesn't use MUTEX macro)
// #define MUTEX(NAME) TracyLockable(engone::Mutex,NAME)
#define MUTEX(NAME) engone::Mutex NAME

// Disable allocation tracing (used in Win32.cpp)
#undef TracyAlloc
#undef TracyFree
#define TracyAlloc(...)
#define TracyFree(...)

