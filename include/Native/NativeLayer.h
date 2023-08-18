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

extern "C" {
    Language::Slice<Language::Slice<char>>* __stdcall CmdLineArgs();

    Language::DirectoryIterator* __stdcall DirectoryIteratorCreate(Language::Slice<char>* rootPath);
    void __stdcall DirectoryIteratorDestroy(Language::DirectoryIterator* iterator);
    Language::DirectoryIteratorData* __stdcall DirectoryIteratorNext(Language::DirectoryIterator* iterator);
    void __stdcall DirectoryIteratorSkip(Language::DirectoryIterator* iterator);
    // Functions not needed here
    // void* Allocate(u64 size);
    // void* Rellocate(void* ptr, u64 oldSize, u64 newSize);
    // void Free(void* ptr, u64 size);

    // // Returns an id representing the file handle, zero means null
    u64 __stdcall FileOpen(Language::Slice<char>* path, bool readOnly, u64* outFileSize);
    u64 __stdcall FileRead(u64 file, void* buffer, u64 length);
    u64 __stdcall FileWrite(u64 file, void* buffer, u64 length);
    void __stdcall FileClose(u64 file);

    bool __stdcall ExecuteCommand(Language::Slice<char>* path, bool asynchronous, int* exitCode);
}