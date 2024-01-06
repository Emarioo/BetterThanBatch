#pragma once

#include "Engone/Typedefs.h"

/*
    A platform layer for the programs created by the compiler
*/

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
        bool isDirectory;
    };
    struct DirectoryIterator {
        u64 _handle;
        Slice<char> rootPath;
        DirectoryIteratorData result{};
    };
}

#ifdef OS_WINDOWS
#define attr_stdcall __stdcall
#else
// #define attr_stdcall __attribute__((stdcall))
#define attr_stdcall
#endif

extern "C" {
    Language::Slice<Language::Slice<char>>* attr_stdcall CmdLineArgs();

    Language::DirectoryIterator* attr_stdcall DirectoryIteratorCreate(Language::Slice<char>* rootPath);
    void attr_stdcall DirectoryIteratorDestroy(Language::DirectoryIterator* iterator);
    Language::DirectoryIteratorData* attr_stdcall DirectoryIteratorNext(Language::DirectoryIterator* iterator);
    void attr_stdcall DirectoryIteratorSkip(Language::DirectoryIterator* iterator);
    // Functions not needed here
    // void* Allocate(u64 size);
    // void* Rellocate(void* ptr, u64 oldSize, u64 newSize);
    // void Free(void* ptr, u64 size);

    // // Returns an id representing the file handle, zero means null
    u64 attr_stdcall FileOpen(Language::Slice<char>* path, u32 flags, u64* outFileSize);
    u64 attr_stdcall FileRead(u64 file, void* buffer, u64 length);
    u64 attr_stdcall FileWrite(u64 file, void* buffer, u64 length);
    void attr_stdcall FileClose(u64 file);

    bool attr_stdcall ExecuteCommand(Language::Slice<char>* path, bool asynchronous, int* exitCode);
}