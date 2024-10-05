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

u32 TestSuite(CompileOptions* options);

u32 VerifyTests(CompileOptions* options, DynamicArray<std::string>& filesToTest);

struct CachedTests {
    struct Test {
        std::string path;
        std::string name; // name of test case inside file
        u64 timestamp;
    };
    DynamicArray<Test> tests;
};

bool SaveTests(const std::string& path, CachedTests* tests);
bool LoadTests(const std::string& path, CachedTests* tests);