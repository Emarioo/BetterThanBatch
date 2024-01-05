#pragma once

#include <vector>
#include <string>

#include "BetBat/Compiler.h"

struct TestCase {
    ~TestCase() {
        expectedErrors.cleanup();
    }
    std::string testName;
    TextBuffer textBuffer;
    struct Error {
        CompileError errorType;
        u32 line;
    };
    DynamicArray<Error> expectedErrors;
    // char* buffer = nullptr;
    // u64 size = 0;
    // std::string originPath;
};
struct TestOrigin {
    char* buffer = nullptr;
    u32 size = 0;
};

// TODO: Add more selections of tests
enum TestSelection : u64 {
    TEST_ALL = (u64)-1,
    TEST_ARITHMETIC = 0x1, // add, multiply, subtract, integers, floats, conversions
    TEST_FLOW       = 0x2, // for, switch, defer
    TEST_FUNCTION   = 0x4, // local variables, inline assembly, calling conventions
    TEST_ASSEMBLY   = 0x8, // inline assembly
    TEST_POLYMORPHIC   = 0x16, // polymorphic structs, functions...
};

u32 TestSuite(CompileOptions* options, TestSelection testSelection);

u32 VerifyTests(CompileOptions* options, DynamicArray<std::string>& filesToTest);