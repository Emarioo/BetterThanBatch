#pragma once

#include "Engone/Alloc.h"
#include <functional>

engone::Memory ReadFile(const char* path);
bool WriteFile(const char* path, engone::Memory buffer);
bool WriteFile(const char* path, std::string& buffer);
void ReplaceChar(char* str, int length,char from, char to);

// not thread safe
const char* FormatUnit(double number);
// not thread safe
const char* FormatUnit(uint64 number);
// not thread safe
const char* FormatBytes(uint64 bytes);
// not thread safe
const char* FormatTime(double seconds);

// bool BeginsWith(const std::string& string, const std::string& has);

#define FUNC_ENTER ScopeDebug scopeDebug{__FUNCTION__,info.funcDepth};
#define SCOPE_LOG(X) ScopeDebug scopeDebug{X,info.funcDepth};

struct ScopeDebug {
    ScopeDebug(const char* msg, int& funcDepth) : _msg(msg), _depth(funcDepth) {
        engone::log::out << engone::log::GRAY; 
        for(int i=0;i<_depth;i++) engone::log::out << "  ";
        engone::log::out << "enter "<<_msg<<"\n";
        _depth++;
    }
    ~ScopeDebug(){
        _depth--;
        engone::log::out << engone::log::GRAY;
        for(int i=0;i<_depth;i++) engone::log::out << "  ";
        engone::log::out << "exit  "<<_msg<<"\n";   
    }
    const char* _msg;
    int& _depth;
};

#define COMBINE1(X,Y) X##Y
#define COMBINE(X,Y) COMBINE1(X,Y)

#define defer std::function<void()> COMBINE(func,__LINE__){};DeferStruct COMBINE(defer,__LINE__)(COMBINE(func,__LINE__));COMBINE(func,__LINE__)=[&]()
struct DeferStruct {
    DeferStruct(std::function<void()>& func) : _func(func) {}
    ~DeferStruct() { _func(); }
    std::function<void()>& _func;
};  