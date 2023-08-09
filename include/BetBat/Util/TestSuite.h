#pragma once

#include <vector>
#include <string>

#include "BetBat/Compiler.h"

struct TestCase {
    std::string testName;
    TextBuffer textBuffer;
    // char* buffer = nullptr;
    // u64 size = 0;
    // std::string originPath;
};
struct TestOrigin {
    char* buffer = nullptr;
    u32 size = 0;
};

void TestSuite(DynamicArray<std::string>& filesToTest);

void VerifyTests(DynamicArray<std::string>& filesToTest);