#include "Native/NativeLayer.h"
#ifdef NATIVE_BUILD
#ifdef OS_WINDOWS
#include "../src/Engone/Win32.cpp"
#else
#include "../src/Engone/Unix.cpp"
#endif
#include "../src/Engone/UIModule.cpp"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#undef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_WRITE_IMPLEMENTATION

#else
#endif

#ifdef OS_WINDOWS
#else
// skip ui module on linux for now
#define NO_UIMODULE
#endif

#include "Engone/PlatformLayer.h"
#ifndef NO_UIMODULE
#include "Engone/UIModule.h"
#endif

#ifdef OS_WINDOWS
#include "Engone/Win32Includes.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "BetBat/Config.h"
// #define GLEW_STATIC
// #include "GL/glew.h"
// #include "GLFW/glfw3.h"

static Language::Slice<Language::Slice<char>> commandLineArgs{};
extern "C" {
    #ifndef NO_UIMODULE
    engone::UIModule* attr_stdcall CreateUIModule() {
        auto* ptr = (engone::UIModule*)engone::Allocate(sizeof(engone::UIModule));
        new(ptr)engone::UIModule();
        ptr->init();
        return ptr;
    }
    void attr_stdcall DestroyUIModule(engone::UIModule* ptr) {
        ptr->~UIModule();
        engone::Free(ptr,sizeof(engone::UIModule));
    }
    void attr_stdcall RenderUIModule(engone::UIModule* ptr, float w, float h) {
        ptr->render(w,h);
    }
    engone::UIBox* attr_stdcall MakeBox(engone::UIModule* ptr, u64 id) {
        return ptr->makeBox(id);
    }
    engone::UIText* attr_stdcall MakeText(engone::UIModule* ptr, u64 id) {
        return ptr->makeText(id);
    }
    float GetWidthOfText(engone::UIModule* ptr, engone::UIText* text) {
        return ptr->getWidthOfText(text);
    }
    #endif // NO_UIMODULE

    bool attr_stdcall ExecuteCommand(Language::Slice<char>* path, bool asynchronous, int* exitCode){
        std::string str = std::string(path->ptr, path->len);
        return engone::ExecuteCommand(str, asynchronous, exitCode);
    }
    Language::Slice<Language::Slice<char>>* attr_stdcall CmdLineArgs() {
        if(commandLineArgs.ptr) // already calculated once
            return &commandLineArgs;

        // TODO: The parsing doesn't fully follow the standard.
        //   \\\\"hej" should become \\hej but doesn't.
        //  The standard: https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments?view=msvc-170

        #ifdef OS_WINDOWS
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
            // Assert((u64)raw % 16 == 0);
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
        #else
        commandLineArgs.ptr = nullptr;
        commandLineArgs.len = 0;
        #endif

        return &commandLineArgs;
    }
    u64 attr_stdcall StartMeasure() {
        return engone::StartMeasure();
    }
    float attr_stdcall StopMeasure(u64 timePoint) {
        return engone::StopMeasure(timePoint);
    }
    float attr_stdcall DiffMeasure(u64 endSubStart) {
        return engone::DiffMeasure(endSubStart);
    }
    u64 attr_stdcall GetClockSpeed() {
        return engone::GetClockSpeed();
    }
    void* attr_stdcall Allocate(u64 size) {
        return engone::Allocate(size);
    }
    void* attr_stdcall Reallocate(void* ptr, u64 oldSize, u64 newSize) {
        return engone::Reallocate(ptr, oldSize, newSize);
    }
    void attr_stdcall Free(void* ptr, u64 size) {
        engone::Free(ptr,size);
    }
    u64 attr_stdcall FileOpen(Language::Slice<char>* path, bool readOnly, u64* outFileSize) {
        return engone::FileOpen(path->ptr, path->len, outFileSize, readOnly ? engone::FILE_ONLY_READ : engone::FILE_ALWAYS_CREATE).internal;
    }
    u64 attr_stdcall FileRead(u64 file, void* buffer, u64 length) {
        return engone::FileRead({file}, buffer, length);
    }
    u64 attr_stdcall FileWrite(u64 file, void* buffer, u64 length) {
        return engone::FileWrite({file}, buffer, length);
    }
    void attr_stdcall FileClose(u64 file) {
        engone::FileClose({file});
    }
    Language::DirectoryIterator* attr_stdcall DirectoryIteratorCreate(Language::Slice<char>* rootPath) {
        void* iteratorHandle = engone::DirectoryIteratorCreate(rootPath->ptr, rootPath->len);
        
        auto iterator = (Language::DirectoryIterator*)engone::Allocate(sizeof(Language::DirectoryIterator));
        new(iterator)Language::DirectoryIterator();
        iterator->_handle = (u64)iteratorHandle;
        iterator->rootPath.ptr = (char*)engone::Allocate(rootPath->len);
        iterator->rootPath.len = rootPath->len;
        memcpy(iterator->rootPath.ptr, rootPath->ptr, rootPath->len);
        return iterator;
    }
    void attr_stdcall DirectoryIteratorDestroy(Language::DirectoryIterator* iterator) {
        engone::DirectoryIteratorDestroy((void*)iterator->_handle, (engone::DirectoryIteratorData*)&iterator->result);
        // Assert(!iterator->result.name.ptr);
        // log::out << log::GRAY<<"Destroy dir iterator: "<<iterator->rootPath<<"\n";
        engone::Free(iterator->rootPath.ptr,iterator->rootPath.len);
        iterator->~DirectoryIterator();
        engone::Free(iterator,sizeof(Language::DirectoryIterator));
    }
    Language::DirectoryIteratorData* attr_stdcall DirectoryIteratorNext(Language::DirectoryIterator* iterator) {
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
    void attr_stdcall DirectoryIteratorSkip(Language::DirectoryIterator* iterator) {
        engone::DirectoryIteratorSkip((void*)iterator->_handle);
    }
    
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
    // float attr_stdcall sine(float x) {
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
    void attr_stdcall NativeSleep(float seconds) {
        engone::Sleep(seconds);
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