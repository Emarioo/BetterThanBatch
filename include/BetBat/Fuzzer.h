/*
    Fuzzer for creating large amounts of random code with variations of correctness.
    
    Purpose:
    - Measure performance of the compiler (requires generation of correct code)
    - Break the compiler, memory leaks, access violations, integer overflow, to much data.
*/
#pragma once

#include "Engone/Logger.h"
#include "Engone/Util/Allocator.h"
#include "Engone/Util/Array.h"
#include "Engone/Util/Stream.h"

struct FuzzerOptions {
    // u64 size_per_file = 0;
    u64 node_iterations_per_file = 10;
    u64 file_count = 1;
    float corruption_frequency = 0.0f; // 0.0 - 1.0
    float corruption_strength = 1;
    u32 random_seed = 0; // 0 for a random seed
    
    engone::Allocator* allocator = nullptr;
};
    
struct FuzzedText {
    char* buffer;
    u64 buffer_max;
    u64 written;
    // allocator?
    
    bool filled = false;
    
    bool write(char chr) {
        if (written+1 <= buffer_max)
            buffer[written++] = chr;
        else filled = true;
        return true;
    }
    bool write(void* buf, u64 len) {
        if (written+len <= buffer_max) {
            memcpy(buffer+written,buf,len);
            written += len;
        }
        else filled = true;
        return true;
    }
};
struct FuzzerNode {
    enum Type {
        EXPR,
        BODY,
        IF,
        WHILE,
        ASSIGN,
        // SWITCH,
        // FOR,
        // FUNC,
        // STRUCT,
        // ENUM,
        TYPE_MAX,
    };
    FuzzerNode() = default;
    FuzzerNode(Type type) : type(type) {}
    Type type;
    DynamicArray<FuzzerNode*> nodes;
};
struct FuzzerImport {
    std::string filename;
    
    FuzzerNode* root = nullptr;
};
struct FuzzerContext {
    // int paren_depth = 0;
    // DynamicArray<EnvType> env_stack;
    
    // DynamicArray<std::string> identifiers{};
    // std::unordered_map<std::string,bool> identifiers{};
    // std::unordered_map<std::string,bool> functions{};
    
    DynamicArray<FuzzerImport*> imports;
    int current_import_index = 0;
    ByteStream stream{nullptr};
    
    // FuzzedText* output=nullptr;
    FuzzerOptions* options=nullptr;
};
bool GenerateFuzzedFiles(FuzzerOptions& options, const std::string& entry_file);