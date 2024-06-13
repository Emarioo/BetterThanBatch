/*
    Manual stack trace for asserts.
    Saves a lot of time when debugging asserts.

    Goal:
        Provide useful information on asserts,
        That way, you don't have to add temporary print functions.

    Usage:
        On assert, call the current function pointer
    
*/

#pragma once

#include <functional>

#define defer_2 DeferStruct_2 COMBINE(defer_2,__LINE__){};COMBINE(defer_2,__LINE__)._func=[&]()
struct DeferStruct_2 {
    DeferStruct_2() = default;
    // DeferStruct(std::function<void()>& func) : _func(func) {}
    ~DeferStruct_2() { _func(); }
    // std::function<void()>& _func;
    std::function<void()> _func;
};

#define TRACE_FUNC() PushStackTrace(__FILE__, __LINE__); defer_2 { PopStackTrace(); };

#define CALLBACK_ON_ASSERT(X) SetCallbackOnAssert([&](){ X });

// TODO: NOTHING HERE IS THREAD SAFE!

void PushStackTrace(const char* name, int line = 0);
void PopStackTrace();
// sets callback for last trace
void SetCallbackOnAssert(std::function<void()> func);
// calls added callbacks for each trace and prints the stack trace
void FireAssertHandler();



