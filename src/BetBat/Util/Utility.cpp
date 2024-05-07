#include "BetBat/Util/Utility.h"

bool streq(const char* a, const char* b) {
    int len_a = strlen(a);
    int len_b = strlen(b);
    if(len_a != len_b)
        return false;
    for(int i=0;i<len_a;i++) {
        if(a[i] != b[i])
            return false;
    }
    return true;
}
std::string NumberToHex_signed(i64 number, bool withPrefix) {
    if(number < 0)
        return "-"+NumberToHex(-number,withPrefix);
    else
        return NumberToHex(number,withPrefix);
}
std::string NumberToHex(u64 number, bool withPrefix) {
    if(number == 0) {
        if (withPrefix)
            return "0x0";
        else
            return "0";
    }

    std::string out = "";
    while(true) {
        u64 part = number & 0xF;
        number = number >> 4;
        // TODO: Optimize
        out.insert(out.begin() + 0, (char)(part < 10 ? part + '0' : part - 10 + 'a'));
        if(number == 0)
            break;
    }
    if(withPrefix)
        out = "0x" + out;

    return out;
}
u64 ConvertHexadecimal_content(char* str, int length){
    Assert(str && length > 0);
    u64 hex = 0;
    for(int i=0;i<length;i++){
        char chr = str[i];
        if(chr>='0' && chr <='9'){
            hex = 16*hex + chr-'0';
            continue;
        }
        chr = chr&(~32); // what is this for? chr = chr&0x20 ?
        if(chr>='A' && chr<='F'){
            hex = 16*hex + chr-'A' + 10;
            continue;
        }
    }
    return hex;
}

engone::Memory<char> ReadFile(const char* path){
    engone::Memory<char> buffer{};
    u64 fileSize;
    
    if(!engone::FileExist(path)) {
        // buffer.data = (void*)1;
        return {};
    }
    
    auto file = engone::FileOpen(path,engone::FILE_READ_ONLY, &fileSize);
    if(!file)
        goto ReadFileFailed;
    
    if(fileSize == 0) {
        buffer.data = (char*)1;
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
    return {};
}
void OutputAsHex(const char* path, char* data, int size) {
    using namespace engone;
    auto file = FileOpen(path,FILE_CLEAR_AND_WRITE);
    Assert(file);

    const int stride = 16;
    // Intentionally not tracking this allocation. It's allocated and freed here so there's no need.
    int bufferSize = size*2 + size / stride;
    char* buffer = (char*)Allocate(bufferSize);
    int offset = 0;
    for(int i = 0;i<size;i++){
        u8 a = data[i]>>4;
        u8 b = data[i]&0xF;
        
        #define HEXIFY(X) (char)(X<10 ? '0'+X : 'A'+X - 10)
        buffer[offset] = HEXIFY(a);
        buffer[offset+1] = HEXIFY(b);
        offset+=2;
        if(i%stride == stride - 1) {
            buffer[offset] = '\n';
            offset++;
        }
        #undef HEXIFY
    }

    FileWrite(file, buffer, bufferSize);

    Free(buffer,bufferSize);
    FileClose(file);
}
// bool WriteFile(const char* path, engone::Memory buffer){
//     auto file = engone::FileOpen(path,0,engone::FILE_CLEAR_AND_WRITE);
//     if(!file) return false;
//     if(!engone::FileWrite(file,buffer.data,buffer.used)) return false;
//     engone::FileClose(file);
//     return true;
// }
// bool WriteFile(const char* path, std::string& buffer){
//     auto file = engone::FileOpen(path,0,engone::FILE_CLEAR_AND_WRITE);
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
const char* FormatUnit(u64 number){
    char* buf = s_formatBuffers[(s_curFormatBuffer = 
        (s_curFormatBuffer+1)%MAX_FORMAT_BUFFERS)];
    if(number<1000) sprintf(buf,FORMAT_64 "u",number);
    else if(number<1e6) sprintf(buf,"%.2lf K",number/1000.f);
    else if(number<1e9) sprintf(buf,"%.2lf M",number/1e6);
    else sprintf(buf,"%.2lf G",number/1e9);
    return buf;
}
const char* FormatBytes(u64 bytes){
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

bool PatternMatchString(const std::string& pattern, const char* str, int len) {
    bool left_wildcard = pattern[0] == '*';
    bool right_wildcard = pattern[pattern.length()-1] == '*';
    // bool left_slash = false;
    // if(data.namelen > 1)
    //     left_slash = left_wildcard ? data.name[1] == '/' : data.name[0] == '/';
    // bool rule_left_slash = false;
    // if(it.length() > 1)
    //     rule_left_slash = it[0] == '*' ? it[1] == '/' : it[0] == '/';

    // TODO: '*/src/*' won't match this 'src/main.cpp'
    //  But if root is different, this 'Project/src/main.cpp' would match
    //  the beginning slash should be ignored if the rule begins with a slash.

    if(!left_wildcard && !right_wildcard) { // full match
        if(pattern == str) {
            return true;
        }
    } else if(left_wildcard && !right_wildcard) { // *file
        if(len >= pattern.length()-1) {
            if(!strncmp(pattern.c_str()+1, str + len - (pattern.length() - 1), pattern.length()-1)) {
                return true;
            }
        }
    } else if(!left_wildcard && right_wildcard) { // file*
        if(len >= pattern.length()-1) {
            if(!strncmp(pattern.c_str(),str,pattern.length()-1)) {
                return true;
            }
        }
    } else /*if (left_wildcard && right_wildcard)*/ { // *file*
        int correct = 0;
        for(int k=0;k<len;k++) {
            char chr = str[k];
            if(pattern[1 + correct] == chr) {
                correct++;
                if(correct == pattern.length()-2) {
                    return true;
                }
            } else {
                correct = 0;
            }
        }
    }
    return false;
}
// Example code
// DynamicArray<std::string> files{};
// PatternMatchFiles("*main.cpp|*.h|!libs",&files);
// log::out << "RESULT:\n";
// FOR(files)
//     log::out << it<<"\n";
int PatternMatchFiles(const std::string& pattern, DynamicArray<std::string>* matched_files, const std::string& root_path, bool output_relative_to_cwd) {
    using namespace engone;
    int num_matched_files = 0;
    
    DynamicArray<std::string> rules{};
    DynamicArray<std::string> exclusions{};
    int lastSplit = 0;
    int index = 0;
    bool is_exclusion = false;
    while(index < pattern.length()){
        char chr = pattern[index];
        index++;
        if(index - 1 == lastSplit && chr == '!') {
            is_exclusion = true;
            lastSplit = index;
            continue;
        }
        if(chr == '|' || index == pattern.length()) {
            int len = (index-1) - lastSplit;
            if(index == pattern.length())
                len++;
            if(len > 0) {
                if(is_exclusion)
                    exclusions.add(pattern.substr(lastSplit, len));
                else
                    rules.add(pattern.substr(lastSplit, len));
            }
            is_exclusion = false;
            lastSplit = index;
            continue;
        }
    }
    // FOR(rules)  log::out << it << "\n";

    // TODO: verify wildcard positions. These are forbidden **  *   a*d 

    exclusions.add(".git");
    exclusions.add(".vs");
    exclusions.add(".vscode");

    Assert(rules.size() != 0);
    engone::DirectoryIterator iter{};
    // if(root_path.empty()) {
        iter = engone::DirectoryIteratorCreate(root_path.c_str(), root_path.length());
    // } else {

    // }
    engone::DirectoryIteratorData data{};
    while(engone::DirectoryIteratorNext(iter,&data)) {
        if(data.isDirectory) {
            FOR(exclusions) {
                if(PatternMatchString(it, data.name, data.namelen)) {
                    engone::DirectoryIteratorSkip(iter);
                    break;
                }
            }
            continue;
        }
        Assert(data.namelen != 0);
        // log::out << data.name<<"\n";
        

        bool include = false;
        FOR(rules) {
            if(PatternMatchString(it, data.name, data.namelen)) {
                include = true;
                break;
            }
        }
        if(include) {
            if(matched_files)
                matched_files->add(data.name);
            num_matched_files++;
        }
    }

    engone:DirectoryIteratorDestroy(iter, &data);

    return num_matched_files;
}
std::string TrimLastFile(const std::string& path){
    int slashI = path.find_last_of("/");
    if(slashI==std::string::npos)
        return "/";
    return path.substr(0,slashI + 1);
}
std::string TrimDir(const std::string& path){
    int slashI = path.find_last_of("/");
    if(slashI==std::string::npos)
        return path;
    return path.substr(slashI+1);
}
std::string TrimCWD(const std::string& path) {
    std::string cwd = engone::GetWorkingDirectory() + "/";
    ReplaceChar((char*)cwd.data(), cwd.length(),'\\','/');
    int where = cwd.size();
    Assert(path.size()!=0); // there are bugs here if path is empty
    for(int i=0;i<path.size();i++) {
        if(i >= cwd.size() || cwd[i] != path[i]) {
            where = i;
            break;
        }
    }
    if(where == 0)
        return path;
    else
        return "./"+path.substr(where);
}
std::string BriefString(const std::string& path, int max, bool skip_cwd){
    if(path.size() == 0)
        return "";
    if(skip_cwd) {
        std::string cwd = engone::GetWorkingDirectory() + "/";
        ReplaceChar((char*)cwd.data(), cwd.length(),'\\','/');
        int where = cwd.size();
        for(int i=0;i<path.size();i++) {
            if(i >= cwd.size() || cwd[i] != path[i]) {
                where = i;
                break;
            }
        }
        if((int)path.length() - where > max){
            return std::string("...") + path.substr(path.length()-max,max);
        } else {
            return path.substr(where);
        }
    } else {
        if((int)path.length()>max){
            return std::string("...") + path.substr(path.length()-max,max);
        }
    }
    return path;
}

std::string StringFromExitCode(u32 exit_code) {
    #define _STATUS_ACCESS_VIOLATION         0xC0000005
    #define _STATUS_ILLEGAL_INSTRUCTION      0xC000001D
    #define _STATUS_FLOAT_DIVIDE_BY_ZERO     0xC000008E
    #define _STATUS_INTEGER_DIVIDE_BY_ZERO   0xC0000094
    #define _STATUS_STACK_OVERFLOW           0xC00000FD
    #define _STATUS_DLL_NOT_FOUND            0xC0000135
    #define _STATUS_HEAP_CORRUPTION          0xc0000374
    switch(exit_code) {
        case _STATUS_ACCESS_VIOLATION:          return "Access violation";
        case _STATUS_ILLEGAL_INSTRUCTION:       return "Illegal instruction";
        case _STATUS_FLOAT_DIVIDE_BY_ZERO:      return "Division by zero (float)";
        case _STATUS_INTEGER_DIVIDE_BY_ZERO:    return "Division by zero (integer)";
        case _STATUS_STACK_OVERFLOW:            return "Stack overflow";
        case _STATUS_DLL_NOT_FOUND:             return "Missing DLL";
        case _STATUS_HEAP_CORRUPTION:           return "Heap corruption";
        default: return "";
    }
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
    // FileWrite((APIFile)((u64)h+1),hm,strlen(hm));
    // FileWrite((APIFile)((u64)h+1),hm,strlen(hm));
    // return 0;
    
    // auto pipe = PipeCreate(false,true);
    
    // std::string cmd = "cmd /C \"dir\"";
    // StartProgram((char*)cmd.data(),0,0,0,pipe);
    
    // char buffer[1024];
    // WHILE_TRUE {
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
