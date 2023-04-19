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
void ReplaceChar(char* str, int length,char from, char to){
    for(int i=0;i<length;i++)
        if(str[i]==from)
            str[i]=to;
}
// bool BeginsWith(const std::string& string, const std::string& has){
//     if(has.length()==0)
//         return false; // wierd stuff, assert?
//     // Todo: optimization where string.length-i<has.length then you can quit since
//     //   string can't contain has. Not doing it right now because it's unnecessary.
//     int correct = 0;
//     for(int i=0;i<string.length();i++){
//         if(string[i]==has[correct]){
//             correct++;
//             if(correct==has.length()){
//                 return true;   
//             }
//         }else{
//             correct=0;
//         }
//     }
//     return false;
// }