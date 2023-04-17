#pragma once
#include "Value.h"
class Context;
class String;

typedef Ref(*ExternalCall)(Context*, int refType, void*);

ExternalCall GetExternalCall(const std::string& name);
// struct ExternalCalls{
//     std::unordered_map<std::string,ExternalCall> map;
// };
// void ProvideDefaultCalls(ExternalCalls& calls);