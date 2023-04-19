#pragma once

#include "Engone/Alloc.h"

engone::Memory ReadFile(const char* path);
bool WriteFile(const char* path, engone::Memory buffer);
bool WriteFile(const char* path, std::string& buffer);
void ReplaceChar(char* str, int length,char from, char to);

// bool BeginsWith(const std::string& string, const std::string& has);