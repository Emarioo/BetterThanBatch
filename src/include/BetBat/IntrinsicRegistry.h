#pragma once

#include "Engone/Asserts.h"

enum IntrinsicType {
    INTRINSIC_INVALID = 0,
    INTRINSIC_PRINTS,
    INTRINSIC_PRINTC,
    INTRINSIC_MEMZERO,
    INTRINSIC_MEMCPY,
    INTRINSIC_STRLEN,
    INTRINSIC_RDTSC,
    INTRINSIC_ATOMIC_CMP_SWAP,
    INTRINSIC_ATOMIC_ADD,
    INTRINSIC_SQRT,
    INTRINSIC_ROUND,
};

const char* ToString(IntrinsicType type);
IntrinsicType ToIntrinsic(const std::string& name);
