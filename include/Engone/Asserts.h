#pragma once

#define COMBINE1(X,Y) X##Y
#define COMBINE(X,Y) COMBINE1(X,Y)

// #ifdef DISABLE_ASSERTS
// You can only disable this if you don't use any important code in the expression.
// Like a resize or something.
// #define Assert(expression)
// #define WHILE_TRUE while(true)
// #define WHILE_TRUE_N(LIMIT) while(true)
// #else
// logger doesn't exist in Win32 so you can't flush it.
// #define Assert(expression) ((expression) ? true : (engone::log::out.flushInternal(), fprintf(stderr,"[Assert] %s (%s:%u)\n",#expression,__FILE__,__LINE__), *((char*)0) = 0))
#define Assert(expression) ((expression) ? true : (fprintf(stderr,"[Assert] %s (%s:%u)\n",#expression,__FILE__,__LINE__), *((char*)0) = 0))
#define WHILE_TRUE u64 COMBINE(limit,__LINE__)=1000; while(Assert(COMBINE(limit,__LINE__)--))
#define WHILE_TRUE_N(LIMIT) u64 COMBINE(limit,__LINE__)=LIMIT; while(Assert(COMBINE(limit,__LINE__)--))
// #endif