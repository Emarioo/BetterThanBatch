#pragma once

// #include "BetBat/Tokenizer.h"
#include "Engone/Util/Array.h"

// TODO: Use enum instead? If there is a benefit.
/*
enum NativeFunctionType {
    NATIVE_printi = -100,
    NATIVE_printc,
    ...
}
*/
#define NATIVE_printi -1
#define NATIVE_printc -2
#define NATIVE_prints -3
#define NATIVE_printd -4

#define NATIVE_Allocate -5
#define NATIVE_Reallocate -6
#define NATIVE_Free -7

#define NATIVE_memcpy -8
#define NATIVE_memzero -9

#define NATIVE_FileOpen -10
#define NATIVE_FileRead -11
#define NATIVE_FileWrite -12
#define NATIVE_FileClose -13

#define NATIVE_DirectoryIteratorCreate -20
#define NATIVE_DirectoryIteratorDestroy -21
#define NATIVE_DirectoryIteratorNext -22
#define NATIVE_DirectoryIteratorSkip -23

#define NATIVE_CurrentWorkingDirectory -24
#define NATIVE_StartMeasure -25
#define NATIVE_StopMeasure -26

#define NATIVE_CmdLineArgs -27

// #define NATIVE_rdtscp -34
#define NATIVE_rdtsc -35
#define NATIVE_compare_swap -36
#define NATIVE_atomic_add -37

#define NATIVE_strlen -38

#define NATIVE_sin -40
#define NATIVE_cos -41
#define NATIVE_tan -42

#define NATIVE_sqrt -43
#define NATIVE_round -44

#define NATIVE_NativeSleep -50

// namespace Language {
//     template <typename T>
//     struct Slice {
//         T* ptr=nullptr;
//         u64 len=0;
//     };
//     // should be null terminated
//     // this 
//     struct String {
//         char* ptr;
//         u32 len;
//         u32 max;
//     };
//     // template <typename T>
//     // struct Array {
//     //     T* ptr=nullptr;
//     //     u64 len=0;
//     // };

//     struct DirectoryIteratorData {
//         Slice<char> name;
//         u64 fileSize;
//         float lastWriteSeconds;
//         bool isDirectory;
//     };

//     struct DirectoryIterator {
//         u64 _handle;
//         Slice<char> rootPath;
//         DirectoryIteratorData result{};

//         // fn next() -> DirectoryIteratorData* {
//         //     return DirectoryIteratorNext(this);
//         // }
//         // fn skip() {
//         //     DirectoryIteratorSkip(this);
//         // }
//     };
// }
// engone::Logger& operator<<(engone::Logger& logger, Language::Slice<char>& slice);

struct NativeRegistry {
    static int initializations;

    std::unordered_map<std::string,u32> nativeFunctionMap;
    std::unordered_map<i64,u32> nativeFunctionMap2;

    static NativeRegistry* GetGlobal();
    static void DestroyGlobal();

    // static NativeRegistry* Create();
    // static void Destroy(NativeRegistry*);
    
    struct NativeFunction {
        NativeFunction() {}
        NativeFunction(i64 jumpAddress) : jumpAddress(jumpAddress) {}
        // ~NativeFunction() {
        //     engone::log::out << "yoo\n";
        // }
        i64 jumpAddress=0;
        std::string name{};
    };
    DynamicArray<NativeFunction> nativeFunctions{};

    // std::vector<NativeFunction> nativeFunctions{};

    bool initialized=false;
    void initNativeContent();

    // TODO: Returned pointer can become invalid if array is resized. Use a bucket array to fix it.
    NativeFunction* findFunction(const std::string& name);
    NativeFunction* findFunction(i64 jumpAddress);

    void _addFunction(const std::string& name, const NativeFunction& func);
};