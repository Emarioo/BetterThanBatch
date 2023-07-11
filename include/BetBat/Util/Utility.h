#pragma once

#include "Engone/Alloc.h"
#include "Engone/Logger.h"
#include "Engone/PlatformLayer.h"

#include "BetBat/Config.h"

#include <functional>
#include <string.h>
#include <unordered_map>
#include <math.h>

// File not found or innaccesible: Returns memory with NO data.
// File with zero in size: Returns memory with valid pointer (memory.data = (void*)1) but zero in size.
// If data could be read: Returns memory with data.
engone::Memory ReadFile(const char* path);
// bool WriteFile(const char* path, engone::Memory buffer);
// bool WriteFile(const char* path, std::string& buffer);
void ReplaceChar(char* str, int length,char from, char to);

// not thread safe
const char* FormatUnit(double number);
// not thread safe
const char* FormatUnit(uint64 number);
// not thread safe
const char* FormatBytes(uint64 bytes);
// not thread safe
const char* FormatTime(double seconds);


// src/util/base.btb -> src/util/
// base.btb -> /
// src -> /
std::string TrimLastFile(const std::string& path);
std::string TrimDir(const std::string& path);
std::string BriefPath(const std::string& path, int max=40);

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

#define COMBINE1(X,Y) X##Y
#define COMBINE(X,Y) COMBINE1(X,Y)

#define WHILE_TRUE int COMBINE(limit,__LINE__)=1000; while(Assert(COMBINE(limit,__LINE__)--))

// #define defer std::function<void()> COMBINE(func,__LINE__){};DeferStruct COMBINE(defer,__LINE__)(COMBINE(func,__LINE__));COMBINE(func,__LINE__)=[&]()
#define defer DeferStruct COMBINE(defer,__LINE__){};COMBINE(defer,__LINE__)._func=[&]()
struct DeferStruct {
    DeferStruct() = default;
    // DeferStruct(std::function<void()>& func) : _func(func) {}
    ~DeferStruct() { _func(); }
    // std::function<void()>& _func;
    std::function<void()> _func;
};