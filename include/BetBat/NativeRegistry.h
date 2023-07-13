#pragma once

#include "BetBat/Tokenizer.h"
#include "BetBat/Util/Array.h"

#define NATIVE_printi -1
#define NATIVE_printc -2
#define NATIVE_prints -3
#define NATIVE_printd -4

#define NATIVE_malloc -5
#define NATIVE_realloc -6
#define NATIVE_free -7

#define NATIVE_FileOpen -10
#define NATIVE_FileRead -11
#define NATIVE_FileWrite -12
#define NATIVE_FileClose -13

#define NATIVE_DirectoryIteratorCreate -20
#define NATIVE_DirectoryIteratorDestroy -21
#define NATIVE_DirectoryIteratorNext -22
#define NATIVE_DirectoryIteratorSkip -23

namespace Language {
    template <typename T>
    struct Slice {
        T* ptr=nullptr;
        u64 len=0;
    };

    struct DirectoryIteratorData {
        Slice<char> name;
        u64 fileSize;
        float lastWriteSeconds;
        u8 _[3]; // this language uses padding here
        bool isDirectory;
    };

    struct DirectoryIterator {
        u64 _handle;
        Slice<char> rootPath;
        DirectoryIteratorData result{};

        // fn next() -> DirectoryIteratorData* {
        //     return DirectoryIteratorNext(this);
        // }
        // fn skip() {
        //     DirectoryIteratorSkip(this);
        // }
    };
}
engone::Logger& operator<<(engone::Logger& logger, Language::Slice<char>& slice);

struct NativeRegistry {
    static int initializations;

    std::unordered_map<std::string,u32> nativeFunctionMap;

    static NativeRegistry* Create();
    static void Destroy(NativeRegistry*);
    
    struct NativeFunction {
        NativeFunction(i64 jumpAddress) : jumpAddress(jumpAddress) {}
        i64 jumpAddress=0;
        std::string name;
    };
    DynamicArray<NativeFunction> nativeFunctions{};

    bool initialized=false;
    void initNativeContent();

    // TODO: Returned pointer can become invalid if array is resized. Use a bucket array to fix it.
    NativeFunction* findFunction(const Token& name);
    NativeFunction* findFunction(i64 jumpAddress);

    void _addFunction(const std::string& name, const NativeFunction& func);
};