#include "BetBat/Util/Utility.h"

engone::Memory ReadFile(const char* path){
    engone::Memory buffer{1};
    uint64 fileSize;
    
    if(!engone::FileExist(path)) {
        buffer.data = (void*)1;
        return buffer;
    }
    
    auto file = engone::FileOpen(path,&fileSize,engone::FILE_ONLY_READ);
    if(!file)
        goto ReadFileFailed;
    
    if(fileSize == 0) {
        buffer.data = (void*)1;
    } else {
        if(!buffer.resize(fileSize))
            goto ReadFileFailed;
        
        if(engone::FileRead(file,buffer.data,buffer.max) == (u64)-1)
            goto ReadFileFailed;
        // printf("Read " FORMAT_64 "u\n",buffer.max);
    }
    
    engone::FileClose(file);
    
    return buffer;
    
ReadFileFailed:
    buffer.resize(0);
    if(file)
        engone::FileClose(file);
    printf("ReadFile : Error %s\n",path);
    return {0};
}
// bool WriteFile(const char* path, engone::Memory buffer){
//     auto file = engone::FileOpen(path,0,engone::FILE_WILL_CREATE);
//     if(!file) return false;
//     if(!engone::FileWrite(file,buffer.data,buffer.used)) return false;
//     engone::FileClose(file);
//     return true;
// }
// bool WriteFile(const char* path, std::string& buffer){
//     auto file = engone::FileOpen(path,0,engone::FILE_WILL_CREATE);
//     if(!file) return false;
//     if(!engone::FileWrite(file,buffer.data(),buffer.length())) return false;
//     engone::FileClose(file);
//     return true;
// }
void ReplaceChar(char* str, int length,char from, char to){
    for(int i=0;i<length;i++)
        if(str[i]==from)
            str[i]=to;
}
// bool BeginsWith(const std::string& string, const std::string& has){
//     if(has.length()==0)
//         return false; // wierd stuff, assert?
//     // TODO: optimization where string.length-i<has.length then you can quit since
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

static const int MAX_FORMAT_BUFFERS=10;
static int s_curFormatBuffer=0;
static char s_formatBuffers[MAX_FORMAT_BUFFERS][100];
const char* FormatUnit(double number){
    char* buf = s_formatBuffers[(s_curFormatBuffer = 
        (s_curFormatBuffer+1)%MAX_FORMAT_BUFFERS)];
    if(number<1000) sprintf(buf,"%.2lf",number);
    else if(number<1e6) sprintf(buf,"%.2lf K",number/1000.f);
    else if(number<1e9) sprintf(buf,"%.2lf M",number/1e6);
    else sprintf(buf,"%.2lf G",number/1e9);
    return buf;
}
const char* FormatUnit(uint64 number){
    char* buf = s_formatBuffers[(s_curFormatBuffer = 
        (s_curFormatBuffer+1)%MAX_FORMAT_BUFFERS)];
    if(number<1000) sprintf(buf,FORMAT_64 "u",number);
    else if(number<1e6) sprintf(buf,"%.2lf K",number/1000.f);
    else if(number<1e9) sprintf(buf,"%.2lf M",number/1e6);
    else sprintf(buf,"%.2lf G",number/1e9);
    return buf;
}
const char* FormatBytes(uint64 bytes){
    char* buf = s_formatBuffers[(s_curFormatBuffer = 
        (s_curFormatBuffer+1)%MAX_FORMAT_BUFFERS)];
    if(bytes<pow(2,10)) sprintf(buf,FORMAT_64"u B",bytes);
    else if(bytes<pow(2,20)) sprintf(buf,"%.2lf KB",bytes/pow(2,10));
    else if(bytes<pow(2,30)) sprintf(buf,"%.2lf MB",bytes/pow(2,20));
    else sprintf(buf,"%.2lf GB",bytes/pow(2,30));
    return buf;
}
const char* FormatTime(double seconds){
    char* buf = s_formatBuffers[(s_curFormatBuffer = 
        (s_curFormatBuffer+1)%MAX_FORMAT_BUFFERS)];
    if(seconds>0.1) sprintf(buf,"%.2lf s",seconds);
    else if(seconds>1e-3) sprintf(buf,"%.2lf ms",seconds*1e3);
    else if(seconds>1e-6) sprintf(buf,"%.2lf us",seconds*1e6);
    else sprintf(buf,"%.2lf ns",seconds*1e9);
    return buf;
}

std::string TrimLastFile(const std::string& path){
    int slashI = path.find_last_of("/");
    if(slashI==-1)
        return "/";
    return path.substr(0,slashI + 1);
}
std::string TrimDir(const std::string& path){
    int slashI = path.find_last_of("/");
    if(slashI==-1)
        return "/";
    return path.substr(slashI+1);
}
std::string BriefPath(const std::string& path, int max){
    if((int)path.length()>max){
        return std::string("...") + path.substr(path.length()-max,max);
    }   
    return path;
}
/*

Some example code with pipes
/ const char* hm = R"(
    // // Okay koapjdopaJDOPjAODjoöAJDoajodpJAPdjkpajkdpakPDKpakdpakpakdpAKdpjAPJDpAKDpaKPDkAPKDpaKDkAPKDpAKPDkaPDkpAKDpAKPDkaPDKpAKPDkaPKDpaKDdapdkmpadkpöakldpaklpdkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk___D:ADKPAD_AD__DA_DPA_DP-D-HF_AH_FhaFh-iawHF_ha_F__paklfalflaflak,fp<jkbpm hyO IWHBRTOAWHJOÖBTJMAPÄWJNPTÄAin2åpbtImawäpå-tib,äawåtbiäåaJODjaodjAOJdoajDoaJKOdjaoJDoaJodjaOdjaoJdoaJKODkaopKDpoAKPODkaPKDpkaPDkap Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess
    // )";
    
    // auto mem = ReadFile("tests/benchmark/string.btb");
    // fwrite(mem.data,1,mem.used,stdout);
    // return 0;
    // printf("%s",hm);
    // printf("%s",hm);
    // printf("%s",hm);
    // fwrite(file);
    // HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    // printf("%p\n",h);
    // FileWrite((APIFile)((uint64)h+1),hm,strlen(hm));
    // FileWrite((APIFile)((uint64)h+1),hm,strlen(hm));
    // return 0;
    
    // auto pipe = PipeCreate(false,true);
    
    // std::string cmd = "cmd /C \"dir\"";
    // StartProgram("",(char*)cmd.data(),0,0,0,pipe);
    
    // char buffer[1024];
    // while(1){
    //     int bytes = PipeRead(pipe,buffer,sizeof(buffer));
    //     printf("Read %d\n",bytes);
    //     if(!bytes)
    //         break;
    //     for(int i=0;i<bytes;i++){
    //         printf("%c",buffer[i]);
    //     }
    //     printf("\n");
    // }
    // printf("Done\n");
    
    // PipeDestroy(pipe);

*/