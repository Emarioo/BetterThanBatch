#include "BetBat/External/NativeLayer.h"

#include "Engone/Win32Includes.h"

#include "Engone/PlatformLayer.h"
#include <stdio.h>

static Language::Slice<Language::Slice<char>> commandLineArgs{};
extern "C" {
    
    Language::Slice<Language::Slice<char>>* __stdcall CmdLineArgs() {
        if(commandLineArgs.ptr) // already calculated once
            return &commandLineArgs;

        // TODO: The parsing doesn't fully follow the standard.
        //   \\\\"hej" should become \\hej but doesn't.
        //  The standard: https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments?view=msvc-170

        char* cmdLine = GetCommandLineA();
        int cmdLineSize = strlen(cmdLine);

        Language::Slice<char>* argPtr = nullptr;
        char* textPtr = nullptr;
        int totalTextSize = 0;
        int argCount = 0;
        {
            #define FINISH_ARG totalTextSize+=textSize + 1;\
                argCount++;\
                textSize=0;
            #define ADD_CHAR textSize++;
        
            int index = 0;
            int textSize = 0;
            bool inQuote = false;
            while(index < cmdLineSize){
                char chr = cmdLine[index];
                char nextChr = 0;
                if (index+1<cmdLineSize)
                    nextChr = cmdLine[index+1];
                index++;

                bool delim = chr ==' ' || chr=='\t';

                if(chr =='"') {
                    if(nextChr == '"') {
                        index++;
                    } else {
                        inQuote = !inQuote;
                        continue;
                    }
                }
                
                if(inQuote || !delim) {
                    if(inQuote) {
                        if(chr == '\\' && nextChr == '"') {
                            index++;
                            chr = '"';
                        } else if(chr == '\\' && nextChr == '\\') {
                            index++;
                            chr = '\\';
                        }
                    } else {
                        if(chr == '\\' && nextChr == '"') {
                            index++;
                            chr = '"';
                        } else if(chr == '\\' && nextChr == '\\') {
                            index++;
                            chr = '\\';
                        }
                    }
                    ADD_CHAR
                }

                if(delim || index == cmdLineSize) {
                    // delimiter
                    if(textSize!=0){
                        FINISH_ARG
                    }
                    continue;
                }
            }
            if(argCount == 0 || totalTextSize == 0)
                return &commandLineArgs;
            char* raw = (char*)engone::Allocate(argCount * sizeof(Language::Slice<char>) + totalTextSize);
            Assert((u64)raw % 16 == 0);
            argPtr = (Language::Slice<char>*)raw;
            textPtr = raw + argCount * sizeof(Language::Slice<char>);
        }
        
        int index = 0;
        int argIndex = 0;
        int textIndex = 0;
        int textSize = 0; // len of current argument
        bool inQuote = false;
        #undef FINISH_ARG
        #undef ADD_CHAR
        #define FINISH_ARG textPtr[textIndex++] = '\0'; argPtr[argIndex].len = textSize; textSize = 0; argIndex++; \
            if(argIndex<argCount) {\
                argPtr[argIndex].len = 0;\
                argPtr[argIndex].ptr = textPtr + textIndex;\
            }
        #define ADD_CHAR textPtr[textIndex++] = chr; textSize++;
        
        argPtr[argIndex].len = 0;
        argPtr[argIndex].ptr = textPtr + textIndex;
        
        while(index < cmdLineSize){
            char chr = cmdLine[index];
            char nextChr = 0;
            if (index+1<cmdLineSize)
                nextChr = cmdLine[index+1];
            index++;

            bool delim = chr ==' ' || chr=='\t';


            if(chr =='"') {
                if(nextChr == '"') {
                    index++;
                } else {
                    inQuote = !inQuote;
                    continue;
                }
            }
            
            if(inQuote || !delim) {
                if(inQuote) {
                    if(chr == '\\' && nextChr == '"') {
                        index++;
                        chr = '"';
                    } else if(chr == '\\' && nextChr == '\\') {
                        index++;
                        chr = '\\';
                    }
                } else {
                    if(chr == '\\' && nextChr == '"') {
                        index++;
                        chr = '"';
                    } else if(chr == '\\' && nextChr == '\\') {
                        index++;
                        chr = '\\';
                    }
                }
                ADD_CHAR
            }

            if(delim || index == cmdLineSize) {
                // delimiter
                if(textSize!=0){
                    FINISH_ARG
                }
                continue;
            }
        }
        #undef FINISH_ARG
        #undef ADD_CHAR

        commandLineArgs.ptr = argPtr;
        commandLineArgs.len = argCount;
        return &commandLineArgs;
    }
    u64 __stdcall StartMeasure() {
        return engone::MeasureTime();
    }
    float __stdcall StopMeasure(u64 timePoint) {
        return engone::StopMeasure(timePoint);
    }

    void* __stdcall Allocate(u64 size) {
        return engone::Allocate(size);
    }
    void* __stdcall Rellocate(void* ptr, u64 oldSize, u64 newSize) {
        return engone::Reallocate(ptr, oldSize, newSize);
    }
    void __stdcall Free(void* ptr, u64 size) {
        engone::Free(ptr,size);
    }
    Language::DirectoryIterator* __stdcall DirectoryIteratorCreate(Language::Slice<char>* rootPath) {
        void* iteratorHandle = engone::DirectoryIteratorCreate(rootPath->ptr, rootPath->len);
        
        auto iterator = (Language::DirectoryIterator*)engone::Allocate(sizeof(Language::DirectoryIterator));
        new(iterator)Language::DirectoryIterator();
        iterator->_handle = (u64)iteratorHandle;
        iterator->rootPath.ptr = (char*)engone::Allocate(rootPath->len);
        iterator->rootPath.len = rootPath->len;
        memcpy(iterator->rootPath.ptr, rootPath->ptr, rootPath->len);
        return iterator;
    }
    void __stdcall DirectoryIteratorDestroy(Language::DirectoryIterator* iterator) {
        engone::DirectoryIteratorDestroy((void*)iterator->_handle, (engone::DirectoryIteratorData*)&iterator->result);
        Assert(!iterator->result.name.ptr);
        #ifdef VLOG
        // log::out << log::GRAY<<"Destroy dir iterator: "<<iterator->rootPath<<"\n";
        #endif
        engone::Free(iterator->rootPath.ptr,iterator->rootPath.len);
        iterator->~DirectoryIterator();
        engone::Free(iterator,sizeof(Language::DirectoryIterator));
    }
    Language::DirectoryIteratorData* __stdcall DirectoryIteratorNext(Language::DirectoryIterator* iterator) {
        // return nullptr;
//         WIN32_FIND_DATAW someData;
//         WIN32_FIND_DATAA someDataa;
//         HANDLE handle = nullptr;
//         // handle = FindFirstFileW(L".",&someData);
//         handle = FindFirstFileA("src\\*",&someDataa);
// // FindFirstFileW
//         printf("Handle: %p\n",handle);
//         return nullptr;

        bool yes = engone::DirectoryIteratorNext((void*)iterator->_handle, (engone::DirectoryIteratorData*)&iterator->result);
        if(!yes) return nullptr;
        return &iterator->result;
    }
    void __stdcall DirectoryIteratorSkip(Language::DirectoryIterator* iterator) {
        engone::DirectoryIteratorSkip((void*)iterator->_handle);
    }
    
    void(*WIN_Sleep)(DWORD) = Sleep;

    // struct Hello {
    //     u16 a = 0;
    //     u16 d = 0;
    //     u64 b = 0;
    //     u32 c = 0;
    // };
    // void FillHello(Hello* ptr) {
    //     memset(ptr, 0, sizeof(Hello));
    //     ptr->a = 1;
    //     ptr->d = 2;
    //     ptr->b = 3;
    //     ptr->c = 4;
    // }

    // #ifdef OS_WINDOWS
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
    // #else
    // #endif
    // float __stdcall sine(float x) {
    //     const float RAD = 1.57079632679f;
    //     int inv = (x<0)<<1;
    //     if(inv)
    //         x = -x;
    //     int t = x/RAD;
    //     if (t&1)
    //         x = RAD * (1+t) - x;
    //     else
    //         x = x - RAD * t;
    //     // float taylor = (x - x*x*x/(2*3) + x*x*x*x*x/(2*3*4*5) - x*x*x*x*x*x*x/(2*3*4*5*6*7) + x*x*x*x*x*x*x*x*x/(2*3*4*5*6*7*8*9));
    //     float taylor = x - x*x*x/(2*3) * ( 1 - x*x/(4*5) * ( 1 - x*x/(6*7) * (1 - x*x/(8*9))));
    //     if ((t&2) ^ inv)
    //         taylor = -taylor;
    //     return taylor;
    // }
    void __stdcall NativeSleep(float seconds) {
        WIN_Sleep((DWORD)(seconds * 1000));
    }
    // void NativeThreadCreate()
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