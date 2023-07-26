#include "BetBat/External/NativeLayer.h"

#include "Engone/Win32Includes.h"

extern "C" {
    // int other();
    
    void(*WIN_Sleep)(DWORD) = Sleep;

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
    float __stdcall sine(float x) {
        const float RAD = 1.57079632679f;
        int inv = (x<0)<<1;
        if(inv)
            x = -x;
        int t = x/RAD;
        if (t&1)
            x = RAD * (1+t) - x;
        else
            x = x - RAD * t;
        // float taylor = (x - x*x*x/(2*3) + x*x*x*x*x/(2*3*4*5) - x*x*x*x*x*x*x/(2*3*4*5*6*7) + x*x*x*x*x*x*x*x*x/(2*3*4*5*6*7*8*9));
        float taylor = x - x*x*x/(2*3) * ( 1 - x*x/(4*5) * ( 1 - x*x/(6*7) * (1 - x*x/(8*9))));
        if ((t&2) ^ inv)
            taylor = -taylor;
        return taylor;
    }
    void NativeSleep(float seconds) {
        WIN_Sleep((DWORD)(seconds * 1000));
    }
    /*
    Code to test sine
    auto tp = MeasureTime();
    float value = 3;
    float sum = 0;
    // for(int t=0;t<1000000;t++){
    for(int t=0;t<100;t++){
        value -= 0.1f;
        // value += 0.1f;
        // 1.57079632679f/8.f;
        float f = sine(value);
        // float f = sinf(value);
        sum += f;
        printf("%f: %f\n", value, sine(value) - sinf(value));
        // printf("%f: %f = %f ~ %f\n", value, sine(value) - sinf(value), sine(value), sinf(value));
    }
    printf("Time %f\n",(float)StopMeasure(tp)*1000.f);
    printf("sum: %f\n", sum);
    */
}