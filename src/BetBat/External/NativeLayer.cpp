#include "BetBat/External/NativeLayer.h"

#include "Engone/Win32Includes.h"

extern "C" {
    // int other();

    int __stdcall printme(){
        // WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),"Oh snap\n",strlen("Oh snap\n"),NULL,NULL);
        // return other();
        return 3;
    }
    #ifdef OS_WINDOWS
    // void* Allocate(u64 size ){
    //     return HeapAlloc(GetProcessHeap(),0, size);
    // }
    // void* Rellocate(void* ptr, u64 oldSize, u64 newSize) {
    //     return HeapReAlloc(GetProcessHeap(), 0, ptr, newSize);
    // }
    // void Free(void* ptr, u64 size){
    //     HeapFree(GetProcessHeap(),0,ptr);
    // }

    // // Returns an id representing the file handle, zero means null
    // u64 FileOpen(char* str, u64 len, u64 flags){
    //     CreateFile(
    // }
    // u64 FileRead(u64 file, void* buffer, u64 length);
    // u64 FileWrite(u64 file, void* buffer, u64 length);
    // void FileClose(u64 file);
    #else

    #endif
}