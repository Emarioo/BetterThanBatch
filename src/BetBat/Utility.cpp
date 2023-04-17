#include "BetBat/Utility.h"
#include "Engone/PlatformLayer.h"

engone::Memory ReadFile(const char* path){
    engone::Memory buffer{1};
    uint64 fileSize;
    auto file = engone::FileOpen(path,&fileSize,engone::FILE_ONLY_READ);
    if(!file)
        goto ReadFileFailed;
    
    if(!buffer.resize(fileSize))
        goto ReadFileFailed;
    
    if(!engone::FileRead(file,buffer.data,buffer.max))
        goto ReadFileFailed;
    
    engone::FileClose(file);
    
    buffer.used = fileSize;
    // printf("ReadFile : Read %lld bytes\n",fileSize);
    return buffer;
    
ReadFileFailed:
    buffer.resize(0);
    if(file)
        engone::FileClose(file);
    printf("ReadFile : Error %s\n",path);
    return {0};
}
bool WriteFile(const char* path, engone::Memory buffer){
    auto file = engone::FileOpen(path,0,engone::FILE_WILL_CREATE);
    if(!file) return false;
    if(!engone::FileWrite(file,buffer.data,buffer.used)) return false;
    engone::FileClose(file);
    return true;
}
bool WriteFile(const char* path, std::string& buffer){
    auto file = engone::FileOpen(path,0,engone::FILE_WILL_CREATE);
    if(!file) return false;
    if(!engone::FileWrite(file,buffer.data(),buffer.length())) return false;
    engone::FileClose(file);
    return true;
}