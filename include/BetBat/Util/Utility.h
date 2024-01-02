#pragma once

#include "Engone/Alloc.h"
#include "Engone/Logger.h"
#include "Engone/PlatformLayer.h"

#include "BetBat/Config.h"

#include <functional>
#include <string.h>
#include <unordered_map>
#include <math.h>

#include <atomic>

// File not found or innaccesible: Returns memory with NO data.
// File with zero in size: Returns memory with valid pointer (memory.data = (void*)1) but zero in size.
// If data could be read: Returns memory with data.
engone::Memory<char> ReadFile(const char* path);
// bool WriteFile(const char* path, engone::Memory buffer);
// bool WriteFile(const char* path, std::string& buffer);
void ReplaceChar(char* str, int length,char from, char to);

// not thread safe
const char* FormatUnit(double number);
// not thread safe
const char* FormatUnit(u64 number);
// not thread safe
const char* FormatBytes(u64 bytes);
// not thread safe
const char* FormatTime(double seconds);


// src/util/base.btb -> src/util/
// base.btb -> /
// src -> /
std::string TrimLastFile(const std::string& path);
// Removes the last slash and all text before it leaving the file name.
std::string TrimDir(const std::string& path);
std::string BriefString(const std::string& path, int max=25, bool skip_cwd = true);
std::string TrimCWD(const std::string& path);

void OutputAsHex(const char* path, char* data, int size);

// bool BeginsWith(const std::string& string, const std::string& has);

#define FUNC_ENTER ScopeDebug scopeDebug{__FUNCTION__,info.funcDepth};
#define FUNC_ENTER_IF(COND) ScopeDebug scopeDebug{(COND)?__FUNCTION__:nullptr,info.funcDepth};
#define SCOPE_LOG(X) ScopeDebug scopeDebug{X,info.funcDepth};

struct ScopeDebug {
    ScopeDebug(const char* msg, int& funcDepth) : _msg(msg), _depth(funcDepth) {
        if(!msg) return;
        engone::log::out << engone::log::GRAY; 
        for(int i=0;i<_depth;i++) engone::log::out << "  ";
        engone::log::out << "enter "<<_msg<<"\n";
        _depth++;
    }
    ~ScopeDebug(){
        if(!_msg) return;
        _depth--;
        engone::log::out << engone::log::GRAY;
        for(int i=0;i<_depth;i++) engone::log::out << "  ";
        engone::log::out << "exit  "<<_msg<<"\n";   
    }
    const char* _msg;
    int& _depth;
};

#define BROKEN engone::log::out << engone::log::RED<< __FUNCTION__<<":"<<__LINE__<<" is broken\n"

#define CHECK_RANGE(DST,A,B) Assert((u64)(DST) >= (u64)(A) && (u64)(DST) < (u64)(B));

#define FOR(LIST) for(auto& it : LIST)
// #define FORN(LIST) auto it = LIST.data(); for(int nr=0; nr < (int)LIST.size() && (it = &LIST[nr]); nr++)
#define FORN(LIST) for(int nr=0; nr < (int)LIST.size(); nr++)
// #define FORNI(LIST) for(int nr=0; nr < (int)LIST.size(); nr++) { auto it = LIST[nr];

// #define COMBINE1(X,Y) X##Y
// #define COMBINE(X,Y) COMBINE1(X,Y)

// // #define WHILE_TRUE u64 COMBINE(limit,__LINE__)=1000; while(Assert(COMBINE(limit,__LINE__)--))
// // #define WHILE_TRUE_N(LIMIT) u64 COMBINE(limit,__LINE__)=LIMIT; while(Assert(COMBINE(limit,__LINE__)--))

// #define WHILE_TRUE while(true)
// #define WHILE_TRUE_N(LIMIT) while(true)

// #define defer std::function<void()> COMBINE(func,__LINE__){};DeferStruct COMBINE(defer,__LINE__)(COMBINE(func,__LINE__));COMBINE(func,__LINE__)=[&]()
#define defer DeferStruct COMBINE(defer,__LINE__){};COMBINE(defer,__LINE__)._func=[&]()
struct DeferStruct {
    DeferStruct() = default;
    // DeferStruct(std::function<void()>& func) : _func(func) {}
    ~DeferStruct() { _func(); }
    // std::function<void()>& _func;
    std::function<void()> _func;
};