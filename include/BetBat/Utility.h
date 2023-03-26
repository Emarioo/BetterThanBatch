#pragma once

#include "Engone/Alloc.h"

engone::Memory ReadFile(const char* path);
bool WriteFile(const char* path, engone::Memory buffer);