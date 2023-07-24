#pragma once

#include "Engone/Typedefs.h"

/*
    A platform layer for the programs created by the compiler
*/

extern "C" {
    // void 
    // void prints(char* ptr, u64 len);

    // void* Allocate(u64 size);
    // void* Rellocate(void* ptr, u64 oldSize, u64 newSize);
    // void Free(void* ptr, u64 size);

    // // Returns an id representing the file handle, zero means null
    // u64 FileOpen(char* str, u64 len, u64 flags);
    // u64 FileRead(u64 file, void* buffer, u64 length);
    // u64 FileWrite(u64 file, void* buffer, u64 length);
    // void FileClose(u64 file);
}