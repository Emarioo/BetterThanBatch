#pragma once

#include "Engone/Alloc.h"

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