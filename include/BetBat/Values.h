#pragma once

#include "Engone/Alloc.h"

struct Number {
    double value=0;
};
struct String {
    engone::Memory data{1};
};