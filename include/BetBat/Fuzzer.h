/*
    Fuzzer for creating large amounts of random code with variations of correctness.
    
    Purpose:
    - Measure performance of the compiler (requires generation of correct code)
    - Break the compiler, memory leaks, access violations, integer overflow, to much data.
*/
#pragma once

#include "Engone/Logger.h"
#include "Engone/Util/Allocator.h"

struct FuzzerOptions {
    
    u64 requested_size=0;
    
    float correctness = 1.f;
    
    union {
        float frequencies[6];
        struct {
            float frequency_struct;
            float frequency_enum;
            float frequency_switch;
            float frequency_for;
            float frequency_if;
            float frequency_while;
        };
    };
    
    engone::Allocator* allocator = nullptr;
};
struct FuzzedText {
    char* buffer;
    u64 buffer_max;
    u64 written;
    // allocator?
};
bool GenerateFuzz(FuzzerOptions& options, FuzzedText* out_text);
bool GenerateFuzzedFile(FuzzerOptions& options, const std::string& path);