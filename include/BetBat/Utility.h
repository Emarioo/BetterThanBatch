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

#define FUNC_ENTER ScopeDebug scopeDebug{__FUNCTION__};
#define SCOPE_LOG(X) ScopeDebug scopeDebug{X};

struct ScopeDebug {
    ScopeDebug(const char* msg) : _msg(msg) {
        // engone::log::out << engone::log::GRAY << "    \\ "<<_msg<<"\n";
        // engone::log::out << engone::log::GRAY << "    > "<<_msg<<"\n";
        engone::log::out << engone::log::GRAY << "    enter "<<_msg<<"\n";
    }
    ~ScopeDebug(){
        // engone::log::out << engone::log::GRAY << "    / "<<_msg<<"\n";   
        // engone::log::out << engone::log::GRAY << "    < "<<_msg<<"\n";   
        engone::log::out << engone::log::GRAY << "    exit  "<<_msg<<"\n";   
    }
    const char* _msg=0;
};

#define COMBINE1(X,Y) X##Y
#define COMBINE(X,Y) COMBINE1(X,Y)

#define defer std::function<void()> COMBINE(func,__LINE__){};DeferStruct COMBINE(defer,__LINE__)(COMBINE(func,__LINE__));COMBINE(func,__LINE__)=[&]()
struct DeferStruct {
    DeferStruct(std::function<void()>& func) : _func(func) {}
    ~DeferStruct() { _func(); }
    std::function<void()>& _func;
};  