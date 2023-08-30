#pragma once

#include <vector>
#include <string>

#include "BetBat/Compiler.h"

struct TestCase {
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
    TEST_ARITHMETIC = 0x1,
    TEST_FLOW       = 0x2,
};

u32 TestSuite(TestSelection testSelection);

u32 VerifyTests(DynamicArray<std::string>& filesToTest);