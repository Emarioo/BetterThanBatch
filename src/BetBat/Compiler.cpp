#include "BetBat/Compiler.h"

#ifdef OS_WINDOWS
#include <intrin.h>
#else
#include <x86intrin.h>
#endif


Path::Path(const char* path) : text(path), _type((Type)0) {
    ReplaceChar((char*)text.data(), text.length(), '\\', '/');
#ifdef OS_WINDOWS
    size_t colon = text.find(":/");
    size_t lastSlash = text.find_last_of("/");

    if(lastSlash + 1 == text.length()){
        _type |= (u32)DIR;
    }
    
    if(colon != std::string::npos) {
        _type |= (u32)ABSOLUTE;
    } else {
        // trim slashes at beginning of path
        for(int i=0;i<(int)text.length();i++){
            if(text[i] != '/') {
                text = text.substr(i,text.length()-i);
                break;
            }
        }
    }
#else
    size_t lastSlash = text.find_last_of("/");

    if(lastSlash + 1 == text.length()){
        _type |= (u32)DIR;
    }
    bool absolute = text[0] == '/' || text[0] == '~';
    if(absolute) {
        _type |= (u32)ABSOLUTE;
    }
#endif
}
engone::Logger& operator<<(engone::Logger& logger, const Path& v) {
    return logger << v.text;
}
Path::Path(const std::string& path) : text(path), _type((Type)0) {
    ReplaceChar((char*)text.data(), text.length(), '\\', '/');
#ifdef OS_WINDOWS
    size_t colon = text.find(":/");
    size_t lastSlash = text.find_last_of("/");

    if(lastSlash + 1 == text.length()){
        _type |= (u32)DIR;
    }
    
    if(colon != std::string::npos) {
        _type |= (u32)ABSOLUTE;
    } else {
        // trim slashes at beginning of path
        for(int i=0;i<(int)text.length();i++){
            if(text[i] != '/') {
                text = text.substr(i,text.length()-i);
                break;
            }
        }
    }
#elif defined(OS_UNIX)
    size_t lastSlash = text.find_last_of("/");

    if(lastSlash + 1 == text.length()){
        _type |= (u32)DIR;
    }
    bool absolute = text[0] == '/' || text[0] == '~';
    if(absolute) {
        _type |= (u32)ABSOLUTE;
    }
#endif
}
std::string Path::getFormat() const {
    int index = text.find(".");
    if(index == (int)std::string::npos || index + 1 == (int)text.length()) {
        return "";
    }
    return text.substr(index+1);
}
Path Path::getFileName(bool withoutFormat) const {
    std::string name = TrimDir(text);
    if(!withoutFormat)
        return name;
    int index = name.find(".");
    if(index == (int)std::string::npos || index + 1 == (int)text.length()) {
        return name;
    }
    return name.substr(0,index);
}
bool Path::isAbsolute() const {
    #ifdef OS_WINDOWS
    return text.size() >= 3 && (text[1] == ':' && text[2] == '/');
    #else
    return text.size() >= 1 && (text[0] == '/' || text[0] == '~');
    #endif
}
Path Path::getAbsolute() const {
    // if(isAbsolute()) return *this;

    std::string cwd = engone::GetWorkingDirectory();
    ReplaceChar((char*)cwd.data(), cwd.length(),'\\','/');
    if(cwd.empty()) return {}; // error?
    
    if(text.length() > 0 && text[0] == '.') {
        if(text.length() > 1 && text[1] == '/') {
            if(cwd.back() == '/')
                cwd += text.substr(2);
            else
                cwd += text.substr(1);
        } else {
            if(cwd.back() == '/')
                cwd += text.substr(1);
            else
                cwd += "/" + text.substr(1);
        }
    } 
    #ifdef OS_WINDOWS
    else if(text.size() >= 3 && (text[1] == ':' && text[2] == '/')) {
        cwd = text;
    }
    #else
    else if(text.size() >= 1 && (text[0] == '/' || text[0] == '~')) {
        cwd = text;
    }
    #endif
    else {
        if(cwd.back() == '/')
            cwd += text;
        else
            cwd += "/" + text;
    }
    while(true) {
        // engone::log::out << "cwd "<<cwd<<"\n";
        int dotdot = cwd.find("..");
        if(dotdot == -1) break;
        // engone::log::out << "dot "<<dotdot<<"\n";

        int slash = cwd.find_last_of("/",dotdot - 2);
        Assert(slash != -1);
        // engone::log::out << "slash " <<slash<<"\n";
        cwd = cwd.substr(0,slash+1) + cwd.substr(dotdot + 3);
    }
    return Path(cwd);
}
Path Path::getDirectory() const {
    if(isDir()) return *this;

    size_t lastSlash = text.find_last_of("/");
    if(lastSlash == std::string::npos){
        if(isAbsolute()) return *this;
        return Path("/");
    }
    return text.substr(0,lastSlash+1);
}

// You can search for "COMPILER_VERSION:" in the compiler executable to find the
// version of the compiler. Useful in case it crashes and you can't get the version.
static const char* str_static_version = "COMPILER_VERSION:" COMPILER_VERSION;

const char* CompilerVersion::global_version = COMPILER_VERSION;
CompilerVersion CompilerVersion::Current(){
    CompilerVersion version{};
    version.deserialize(global_version);
    return version;
}
void CompilerVersion::deserialize(const char* str) {
    int versionLength = strlen(str);
    Assert(versionLength <= MAX_STRING_VERSION_LENGTH);
    if(versionLength > MAX_STRING_VERSION_LENGTH) return; // in case you disable asserts for whatever reason
    // "0.1.0/0-2023.8.29"
    // "65535.65535.65535/65535-65535.256.256"
    // "9999.9999.9999/9999-9999.99.99"
    
    // good defaults
    memset(this, 0, sizeof(CompilerVersion));
    
    char buffer[MAX_STRING_VERSION_LENGTH+1];
    strcpy(buffer, str); // buffer and string length is already checked
    int slashIndex = -1, dashIndex = -1;
    int dots[3] = {-1, -1, -1 };
    int lastDots[2] = { -1, -1 };
    int dotCount = 0;
    int lastDotCount = 0;
    for(int i=0;i<versionLength;i++){
        if(str[i] == '/') { // indicates state/name
            Assert(("too many slashes in version string",slashIndex == -1));
            slashIndex = i + 1;
            buffer[i] = '\0';
        }
        if(str[i] == '-') { // indicates date
            Assert(("too many dashes in version string",dashIndex == -1));
            dashIndex = i + 1;
            buffer[i] = '\0';
        }
        if(str[i] == '.') {
            if (slashIndex == -1 && dashIndex == -1) {
                if(dotCount < 3) {
                    dots[dotCount] = i;
                    buffer[i] = '\0';
                }
                dotCount++;
            }
            if (dashIndex != -1) {
                if(lastDotCount < 2) {
                    lastDots[lastDotCount] = i;
                    buffer[i] = '\0';
                }
                lastDotCount++;
            }
        }
    }
    Assert(("there must be two or three dots in first part",dotCount==2 || dotCount==3));
    if(dashIndex != -1){
        Assert(("there must be 2 dots in date part (when using dash)",lastDotCount==2));
    }
    if(slashIndex != -1) {
        Assert(("missing name string", slashIndex != dashIndex - 1));
    }
    
    int temp=0;
    #define SET_VER_INT(VAR, OFF) temp = atoi(buffer + OFF); Assert(temp < ((1<<(sizeof(VAR)<<3))-1)); VAR = temp;
    SET_VER_INT(major, 0)
    SET_VER_INT(minor, dots[0] + 1)
    SET_VER_INT(patch, dots[1] + 1)
    if(dots[2] != -1) {
        SET_VER_INT(revision, dots[2] + 1)
    }
    if(slashIndex != -1) {
        int nameLen = 0;
        if(dashIndex == -1){
            nameLen = versionLength - slashIndex - 1;
        } else {
            nameLen = dashIndex - slashIndex - 1;
        }
        Assert(nameLen < (int)sizeof(name));
        if(nameLen>0){
            strcpy(name, buffer + slashIndex);
        }
    }
    if(dashIndex != -1) {
        SET_VER_INT(year, dashIndex)
        SET_VER_INT(month, lastDots[0] + 1)
        SET_VER_INT(day, lastDots[1] + 1)
    }
    #undef SET_VER_INT
}
void CompilerVersion::serialize(char* outBuffer, int bufferSize, u32 flags) {
    Assert(bufferSize > 50); // version is usually, probably, never bigger than this
    int offset = 0;
    offset += snprintf(outBuffer + offset, bufferSize - offset, "%d.%d.%d", (int)major, (int)minor, (int)patch);
    if(flags & INCLUDE_AVAILABLE) {
        if(0 == (flags & EXCLUDE_REVISION) && revision != 0)
            offset += snprintf(outBuffer + offset, bufferSize - offset, ".%d", (int)revision);
        if(0 == (flags & EXCLUDE_NAME) && *name != '\0') {
            name[sizeof(name)-1] = '\0'; // just in case
            offset += snprintf(outBuffer + offset, bufferSize - offset, "/%s", name);
        }
        if(0==(flags & EXCLUDE_DATE) && year != 0) // year should normally not be zero so if it is we have special stuff going on
            offset += snprintf(outBuffer + offset, bufferSize - offset, "-%d.%d.%d", (int)year, (int)month, (int)day);
    } else {
        if(flags & INCLUDE_REVISION)
            offset += snprintf(outBuffer + offset, bufferSize - offset, ".%d", (int)revision);
        if(flags & INCLUDE_NAME) {
            name[sizeof(name)-1] = '\0'; // just in case
            offset += snprintf(outBuffer + offset, bufferSize - offset, "/%s", name);
        }
        if(flags & INCLUDE_DATE)
            offset += snprintf(outBuffer + offset, bufferSize - offset, "-%d.%d.%d", (int)year, (int)month, (int)day);
    }
}
const char* ToString(TargetPlatform target){
    #define CASE(X,N) case X: return N;
    switch(target){
        CASE(TARGET_WINDOWS_x64,"win-x64")
        CASE(TARGET_UNIX_x64,"unix-x64")
        CASE(TARGET_BYTECODE,"bytecode")
        CASE(TARGET_UNKNOWN,"unknown-target")
    }
    return "unknown";
    #undef CASE
}
TargetPlatform ToTarget(const std::string& str){
    #define CASE(N,X) if (str==X) return N;
    CASE(TARGET_WINDOWS_x64,"win-x64")
    CASE(TARGET_UNIX_x64,"unix-x64")
    CASE(TARGET_BYTECODE,"bytecode")
    return TARGET_UNKNOWN;
    #undef CASE
}
engone::Logger& operator<<(engone::Logger& logger,TargetPlatform target){
    return logger << ToString(target);
}
const char* ToString(LinkerChoice v) {
    #define CASE(X,N) case X: return N;
    switch(v){
        CASE(LINKER_GCC,"gcc")
        CASE(LINKER_MSVC,"msvc")
        CASE(LINKER_CLANG,"clang")
        CASE(LINKER_UNKNOWN,"unknown-linker")
        default: {}
    }
    return "unknown";
    #undef CASE
}
LinkerChoice ToLinker(const std::string& str) {
    #define CASE(N,X) if (str==X) return N;
    CASE(LINKER_GCC,"gcc")
    CASE(LINKER_MSVC,"msvc")
    CASE(LINKER_CLANG,"clang")
    CASE(LINKER_UNKNOWN,"unknown-linker")
    return LINKER_UNKNOWN;
    #undef CASE
}
engone::Logger& operator<<(engone::Logger& logger,LinkerChoice v) {
    return logger << ToString(v);
}

bool CompileInfo::removeRootMacro(const Token& name) {
    macroLock.lock();
    auto pair = _rootMacros.find(name);
    bool removed = false;
    if(pair!=_rootMacros.end()){
        for(auto& pair : pair->second->certainMacros) {
            pair.second->~CertainMacro();
            TRACK_FREE(pair.second, CertainMacro);
            // engone::Free(pair.second, sizeof(CertainMacro));
        }
        pair->second->~RootMacro();
        TRACK_FREE(pair->second, RootMacro);
        _rootMacros.erase(pair);
        removed = true;
    }
    macroLock.unlock();
    return removed;
}
void CompileInfo::addStats(i32 errors, i32 warnings){
    #ifdef OS_WINDOWS
    _interlockedadd(&compileOptions->compileStats.errors, errors);
    _interlockedadd(&compileOptions->compileStats.warnings, warnings);
    #else
    otherLock.lock();
    compileOptions->compileStats.errors += errors;
    compileOptions->compileStats.warnings += warnings;
    otherLock.unlock();
    #endif
}
void CompileInfo::addStats(i32 lines, i32 blankLines, i32 commentCount, i32 readBytes){
    #ifdef OS_WINDOWS
    _interlockedadd(&compileOptions->compileStats.lines, lines);
    _interlockedadd(&compileOptions->compileStats.blankLines, blankLines);
    _interlockedadd(&compileOptions->compileStats.commentCount, commentCount);
    _interlockedadd(&compileOptions->compileStats.readBytes, readBytes);
    #else
    otherLock.lock();
    compileOptions->compileStats.lines += lines;
    compileOptions->compileStats.blankLines += blankLines;
    compileOptions->compileStats.commentCount += commentCount;
    compileOptions->compileStats.readBytes += readBytes;
    otherLock.unlock();
    #endif
}
Path CompileInfo::findSourceFile(const Path& path, const Path& sourceDirectory) {
    using namespace engone;
    // absolute
    // ./path always from sourceDirectory
    // relative to cwd OR import directories
    // .btb is implicit

    Path fullPath = {};
    int dotindex = path.text.find_last_of(".");
    int slashindex = path.text.find_last_of("/");
    if(dotindex==-1 || dotindex<slashindex){
        fullPath = path.text+".btb";
    } else {
        fullPath = path.text;
    }

    //-- Search directory of current source file
    if(fullPath.text.find("./")==0) {
        Assert(!sourceDirectory.text.empty());
        if(sourceDirectory.text[sourceDirectory.text.size()-1] == '/') {
            fullPath = sourceDirectory.text + fullPath.text.substr(2);
        } else {
            fullPath = sourceDirectory.text + fullPath.text.substr(1);
        }
        fullPath = fullPath.getAbsolute();
    } 
    //-- Search cwd or absolute path
    else if(engone::FileExist(fullPath.text)){
        // if(!fullPath.isAbsolute())
        fullPath = fullPath.getAbsolute();
    }
    else {
        Path temp = sourceDirectory.text;
        if(!sourceDirectory.text.empty() && sourceDirectory.text[sourceDirectory.text.size()-1]!='/')
            temp.text += "/";
        temp.text += fullPath.text;
        if(FileExist(temp.text)){ // search directory of current source file again but implicit ./
            fullPath = temp.getAbsolute();
        } else {
            //-- Search additional import directories.
            // TODO: DO THIS WITH #INCLUDE TOO!
            // We only read importDirectories which makes this thread safe
            // if we had modified it in anyway then it wouldn't have been.
            bool yes = false;
            for(int i=0;i<(int)importDirectories.size();i++){
                const Path& dir = importDirectories[i];
                Assert(dir.isDir() && dir.isAbsolute());
                if(dir.text.size()>0 && dir.text[dir.text.size()-1] == '/')
                    temp = dir.text + fullPath.text;
                else
                    temp = dir.text + "/" + fullPath.text;

                if(FileExist(temp.text)) {
                    fullPath = temp.getAbsolute();
                    yes = true;
                    break;
                }
            }
            if(!yes) {
                fullPath = "";
            }
        }
    }
    return fullPath;
}
void CompileInfo::addLinkDirective(const std::string& name){
    otherLock.lock();
    for(int i=0;i<(int)linkDirectives.size();i++){
        if(name == linkDirectives[i]) {
            otherLock.unlock();
            return;
        }
    }
    linkDirectives.add(name);
    otherLock.unlock();
}
void CompileInfo::cleanup(){
    // for(auto& pair : tokenStreams){
    //     TokenStream::Destroy(pair.second->initialStream);
    //     TokenStream::Destroy(pair.second->finalStream);
    // }
    tokenStreams.clear();
    for(auto& it : streams){
        if(it->initialStream)
            TokenStream::Destroy(it->initialStream);
        if(it->finalStream)
            TokenStream::Destroy(it->finalStream);
        it->~StreamToProcess();
        TRACK_FREE(it, StreamToProcess);
    }
    streams.cleanup();
    for(auto& pair : includeStreams){
        TokenStream::Destroy(pair.second);
    }
    includeStreams.clear();
    for(auto& pair : _rootMacros) {
        for(auto& pair : pair.second->certainMacros) {
            pair.second->~CertainMacro();
            TRACK_FREE(pair.second, CertainMacro);
        }
        // engone::log::out << "no\n";
        pair.second->~RootMacro();
        TRACK_FREE(pair.second, RootMacro);
    }
    _rootMacros.clear();
    // for(auto& stream : streamsToClean){
    //     TokenStream::Destroy(stream);
    // }
    // streamsToClean.resize(0);
}

StreamToProcess* CompileInfo::addStream(const Path& path, StreamToProcess** existingStream){
    streamLock.lock();
    auto pair = tokenStreams.find(path.text);
    if(pair!=tokenStreams.end() && pair->second) {
        if(existingStream)
            *existingStream = pair->second;
        streamLock.unlock();
        return nullptr;
    }
    // tokenStreams[path.text] = nullptr;
    StreamToProcess* stream = tokenStreams[path.text] = TRACK_ALLOC(StreamToProcess);
    new(stream)StreamToProcess();
    // stream->initialStream = tokenStream;
    streams.add(stream);
    stream->index = streams.size()-1;

    // TODO: Atomic add here? I haven't because we need a lock here for the streams anyway.
    // compileOptions->compileStats.lines += tokenStream->lines;
    // compileOptions->compileStats.blankLines += tokenStream->blankLines;
    // compileOptions->compileStats.commentCount += tokenStream->commentCount;
    // compileOptions->compileStats.readBytes += tokenStream->readBytes;
    streamLock.unlock();
    return stream;
}
// StreamToProcess* CompileInfo::addStream(TokenStream* tokenStream){
//     streamLock.lock();
//     auto pair = tokenStreams.find(tokenStream->streamName);
//     if(pair!=tokenStreams.end() && pair->second) {
//         streamLock.unlock();
//         return nullptr;
//     }
//     StreamToProcess* stream = tokenStreams[tokenStream->streamName] = TRACK_ALLOC(StreamToProcess);
//     // Stream* stream = tokenStreams[tokenStream->streamName] = (Stream*)engone::Allocate(sizeof(Stream));
//     new(stream)StreamToProcess();
//     stream->initialStream = tokenStream;
//     streams.add(stream);

//     // TODO: Atomic add here? I haven't because we need a lock here for the streams anyway.
//     compileOptions->compileStats.lines += tokenStream->lines;
//     compileOptions->compileStats.blankLines += tokenStream->blankLines;
//     compileOptions->compileStats.commentCount += tokenStream->commentCount;
//     compileOptions->compileStats.readBytes += tokenStream->readBytes;
//     streamLock.unlock();
//     return stream;
// }
bool CompileInfo::hasStream(const Path& name){
    streamLock.lock();
    auto pair = tokenStreams.find(name.text);
    if(pair == tokenStreams.end()) {
        streamLock.unlock();
        return false;
    }
    streamLock.unlock();
    return true;
}
int CompileInfo::indexOfStream(const Path& name){
    streamLock.lock();
    auto pair = tokenStreams.find(name.text);
    if(pair == tokenStreams.end()) {
        streamLock.unlock();
        return -1;
    }
    int index = pair->second->index;
    streamLock.unlock();
    return index;
}
StreamToProcess* CompileInfo::getStream(const Path& name){
    streamLock.lock();
    auto pair = tokenStreams.find(name.text);
    StreamToProcess* ptr = nullptr;
    if(pair != tokenStreams.end())
        ptr = pair->second;
    streamLock.lock();
    return ptr;
}
RootMacro* CompileInfo::matchMacro(const Token& name){
    if(name.flags&TOKEN_MASK_QUOTED)
        return nullptr;
        
    macroLock.lock();
    auto pair = _rootMacros.find(name);
    RootMacro* ptr = nullptr;
    if(pair != _rootMacros.end()){
        ptr = pair->second;
    }
    _MMLOG(engone::log::out << engone::log::CYAN << "match root "<<name<<"\n")
    macroLock.unlock();
    
    return ptr;
}
RootMacro* CompileInfo::ensureRootMacro(const Token& name, bool ensureBlank){
    if(name.flags&TOKEN_MASK_QUOTED)
        return nullptr;
    macroLock.lock();

    RootMacro* rootMacro = nullptr;
    auto pair = _rootMacros.find(name);
    if(pair != _rootMacros.end()){
        rootMacro = pair->second;
    } else {
        rootMacro = TRACK_ALLOC(RootMacro);
        // engone::log::out << "yes\n";
        new(rootMacro)RootMacro();
        rootMacro->name = name;
        _rootMacros[name] = rootMacro;
    }
    if(ensureBlank) {
        // READ BEFORE CHANGING! 
        //You usually want a base case for variadic arguments when
        // using it recursively. Instead of having to specify the blank
        // base macro yourself, the compiler does it for you.
        // IMPORTANT: It should NOT happen for macro(...)
        // because the blank macro with zero arguments would be
        // used instead of the variadic one making it impossible to
        // use macro(...).
        auto pair = rootMacro->certainMacros.find(0);
        if(pair == rootMacro->certainMacros.end()) {
            CertainMacro* macro = TRACK_ALLOC(CertainMacro);
            new(macro)CertainMacro();
            rootMacro->certainMacros[0] = macro;
            rootMacro->hasBlank = true;
        }
    }
    macroLock.unlock();
    return rootMacro;
}
void CompileInfo::insertCertainMacro(RootMacro* rootMacro, CertainMacro* localMacro) {
    macroLock.lock();
    if(!localMacro->isVariadic()){
        auto pair = rootMacro->certainMacros.find(localMacro->parameters.size());
        CertainMacro* macro = nullptr;
        if(pair!=rootMacro->certainMacros.end()){
            // if(!certainMacro->isVariadic() && !certainMacro->isBlank()){
            //     WARN_HEAD3(name, "Intentional redefinition of '"<<name<<"'?\n\n";
            //         ERR_LINE(certainMacro->name, "previous");
            //         ERR_LINE2(name.tokenIndex, "replacement");
            //     )
            // }
            if(localMacro->parameters.size() == 0) {
                rootMacro->hasBlank = false;
            }
            macro = pair->second;
            *macro = *localMacro;
            // if(includeInf && rootMacro->hasVariadic && (int)rootMacro->variadicMacro.parameters.size()-1<=count) {
            //     macro = &rootMacro->variadicMacro;
            //     _MLOG(MLOG_MATCH(engone::log::out<<engone::log::MAGENTA << "match argcount "<<count<<" with "<< rootMacro->variadicMacro.parameters.size()<<" (inf)\n";))
            // }
        }else{
            // _MLOG(MLOG_MATCH(engone::log::out <<engone::log::MAGENTA<< "match argcount "<<count<<" with "<< pair->second.parameters.size()<<"\n";))
            macro = TRACK_ALLOC(CertainMacro);
            new(macro)CertainMacro(*localMacro);
            rootMacro->certainMacros[localMacro->parameters.size()] = macro;
        }
    }else{
        // if(rootMacro->hasVariadic){
        //     WARN_HEAD3(name, "Intentional redefinition of '"<<name<<"'?\n\n";
        //         ERR_LINE(certainMacro->name, "previous");
        //         ERR_LINE2(name.tokenIndex, "replacement");
        //     )
        // }
        rootMacro->hasVariadic = true;
        rootMacro->variadicMacro = *localMacro;
    }
    macroLock.unlock();
}

bool CompileInfo::removeCertainMacro(RootMacro* rootMacro, int argumentAmount, bool variadic){
    bool removed=false;
    macroLock.lock();
    if(variadic) {
        if(rootMacro->hasVariadic) {
            rootMacro->hasVariadic = false;
            removed = true;
            rootMacro->variadicMacro.cleanup();
            rootMacro->variadicMacro = {};
            if(rootMacro->hasBlank){
                auto blank = rootMacro->certainMacros[0];
                blank->~CertainMacro();
                TRACK_FREE(blank,CertainMacro);
                rootMacro->certainMacros.erase(0);
                rootMacro->hasBlank = false;
            }
        }
    } else {
        auto pair = rootMacro->certainMacros.find(argumentAmount);
        if(pair != rootMacro->certainMacros.end()){
            removed = true;
            pair->second->~CertainMacro();
            TRACK_FREE(pair->second,CertainMacro);
            rootMacro->certainMacros.erase(pair);
        }
    }
    macroLock.unlock();
    return removed;
}
CertainMacro* CompileInfo::matchArgCount(RootMacro* rootMacro, int count, bool includeInf){
    macroLock.lock();
    auto pair = rootMacro->certainMacros.find(count);
    CertainMacro* macro = nullptr;
    if(pair==rootMacro->certainMacros.end()){
        if(includeInf && rootMacro->hasVariadic && (int)rootMacro->variadicMacro.parameters.size()-1<=count) {
            macro = &rootMacro->variadicMacro;
            _MMLOG(engone::log::out<<engone::log::MAGENTA << "match argcount "<<count<<" with "<< rootMacro->variadicMacro.parameters.size()<<" (inf)\n")
        }
    }else{
        macro = pair->second;
        _MMLOG(engone::log::out <<engone::log::MAGENTA<< "match argcount "<<count<<" with "<< macro->parameters.size()<<"\n")
    }
    // Assert(((u64)macro & 0xFF000000000000) == 0);
    macroLock.unlock();
    return macro;
}
// Thread procedure
u32 ProcessSource(void* ptr) {
    using namespace engone;
    // MEASURE_THREAD()
    // MEASURE_SCOPE
    PROFILE_THREAD
    PROFILE_SCOPE

    ThreadCompileInfo* thread = (ThreadCompileInfo*)ptr;
    // get file from a list to tokenize, wait with semaphore if list is empty.
    CompileInfo* info = thread->info;
    DynamicArray<Path> tempPaths;
    
    // TODO: Optimize the code here. There is a lot slow locks. Is it possible to use atomic instructions instead?

    // TODO: There is a very rare bug here where a thread locks a mutex twice.
    //   There may be a bug in the mutex struct or here. Not sure.
    //   Luckily it's rare and the system will probably change so fixing it isn't necessary.

    info->sourceLock.lock();
    info->availableThreads++;
    info->waitingThreads++;
    info->sourceLock.unlock();
    WHILE_TRUE {
        MEASURE_WHO("Process Source")
        // info->sourceLock.lock();
        // info->waitingThreads++;
        // // If the last thread finished and there are no sources we are done.
        // // We must signal the lock that we are done so that all thread doesn't freeze.
        // if(info->waitingThreads == info->compileOptions->threadCount && info->sourcesToProcess.size() == 0) {
        //     info->sourceWaitLock.signal();
        // }
        // info->sourceLock.unlock();

        info->sourceWaitLock.wait(); // wait here when there are no sources to process

        info->sourceLock.lock();
        info->signaled = false;
        info->waitingThreads--;

        if(info->sourcesToProcess.size() == 0) {
            // we quit because the semaphore shouldn't be signaled unless there is content
            // if there isn't content then it was signaled to stop
            // log::out << "Quit no sources\n";
            info->availableThreads--;
            if(!info->signaled){
                info->sourceWaitLock.signal();
                info->signaled=true;
            }
            info->sourceLock.unlock();
            break;
        }
        SourceToProcess source = info->sourcesToProcess.last();
        info->sourcesToProcess.pop();
        if(info->sourcesToProcess.size() > 0) {
            if(!info->signaled) {
                // log::out << "Signal more sources\n";
                info->sourceWaitLock.signal();
                info->signaled = true;
            }
        }
        // if(info->sourcesToProcess.size() != 0)
        //     // signal that there are more sources to process for other threads
        //     info->sourceWaitLock.signal();
        // if(source.textBuffer)
        //     log::out << "Proc source "<<source.textBuffer->origin<<"\n";
        // else
        //     log::out << "Proc source "<<source.path.text<<"\n";
        info->sourceLock.unlock();
        
        if(!source.textBuffer) {
            // log::out << "Yeet "<<source.path.text <<"\n";
            auto pathp = info->findSourceFile(source.path);
            if(pathp.text.empty()) {
                log::out << log::RED << "Could not find '" << BriefString(source.path.text) <<"'\n";
                info->compileOptions->compileStats.errors++;
                
                info->sourceLock.lock();
                if(info->waitingThreads == info->availableThreads-1 || info->sourcesToProcess.size()>0) {
                    if(!info->signaled) {
                        info->sourceWaitLock.signal();
                        info->signaled=true;
                    }
                }
                info->waitingThreads++;
                info->sourceLock.unlock();
                continue;
            }
            source.path = pathp; // don't set source.path if pathp is empty? seems kind of dumb to do that.
        }
        // log::out << "Proc "<<source.path.text << " "<<pathp.text<<"\n";

        // bool yes = FileExist(source.path.text);
        // if(!yes) {
        //     int dotindex = source.path.text.find_last_of(".");
        //     int slashindex = source.path.text.find_last_of("/");
        //     if(dotindex==-1 || dotindex<slashindex){
        //         source.path = source.path.text + ".btb";
        //     }
        // }
        
        _VLOG(log::out <<log::BLUE<< "Tokenize: "<<BriefString(source.path.text)<<"\n";)
        TokenStream* tokenStream = nullptr;
        if(source.textBuffer) {
            tokenStream = TokenStream::Tokenize(source.textBuffer);
            // tokenStream->streamName = path.text;
        } else if(source.path.text.size() != 0) {
            Assert(source.path.isAbsolute()); // A bug at the call site if not absolute
            tokenStream = TokenStream::Tokenize(source.path.text);
        }
        if(!tokenStream){
            log::out << log::RED << "Failed tokenization: " << BriefString(source.path.text) <<" (probably not found)\n";
            info->compileOptions->compileStats.errors++;
            
            info->sourceLock.lock();
            if(info->waitingThreads == info->availableThreads-1 || info->sourcesToProcess.size()>0) {
                if(!info->signaled) {
                    info->sourceWaitLock.signal();
                    info->signaled=true;
                }
            }
            info->waitingThreads++;
            info->sourceLock.unlock();
            continue;
        }
        info->addStats(tokenStream->lines, tokenStream->blankLines, tokenStream->commentCount, tokenStream->readBytes);
        // tokenStream->printData();
        
        // if (tokenStream->enabled & LAYER_PREPROCESSOR) {
            
        //     _VLOG(log::out <<log::BLUE<< "Preprocess (imports): "<<BriefString(source.path.text)<<"\n";)
        //     PreprocessImports(info, tokenStream);
        // }

        // Assert(("can't add stream until it's completly processed, other threads would have access to it otherwise which may cause strange behaviour",false));
        // IMPORTANT: If you decide to multithread preprocessor then you need to 
        // add the stream later since the preprocessor will modify it.
        auto stream = source.stream;
        stream->initialStream = tokenStream;
        // auto stream = info->addStream(tokenStream);
        // Assert(stream);
        // stream->as = source.as;
        // Stream is nullptr if it already exists.
        // We assert because this should never happen.
        // If it does then we just tokenized the same file twice.
        
        // TODO: How does this work when using textBuffer which isn't a file path
        tempPaths.resize(tokenStream->importList.size());
        Path dir = source.path.getDirectory();
        for(int i=0;i<(int)tokenStream->importList.size();i++){
            auto& item = tokenStream->importList[i];

            Path fullPath = info->findSourceFile(item.name, dir);

            if(fullPath.text.empty()){
                log::out << log::RED << "Could not find import '"<<fullPath.text<<"' (import from '"<<BriefString(source.path.text,20)<<"'\n";
            } else {
                tempPaths[i] = fullPath;
            }
        }
        info->sourceLock.lock();
        for(int i=0;i<(int)tokenStream->importList.size();i++){
            auto& item = tokenStream->importList[i];
            if(tempPaths[i].text.size()==0)
                continue;
            // log::out << "Add source "<<tempPaths[i].text<<"\n";
            StreamToProcess* existingStream = nullptr;
            auto importStream = info->addStream(tempPaths[i], &existingStream);
            
            if(!importStream) {
                if(existingStream) {
                    stream->dependencies.add(existingStream->index);
                }
                _VLOG(log::out << log::LIME << "Already imported "<<BriefString(source.path.text,20)<<"\n";)
                continue;
            }
            stream->dependencies.add(importStream->index);
            importStream->as = item.as;
            SourceToProcess source{};
            source.path = tempPaths[i];
            // source.as = item.as;
            source.textBuffer = nullptr;
            source.stream = importStream;
            info->sourcesToProcess.add(source);
        }
        if(info->waitingThreads == info->availableThreads-1 || info->sourcesToProcess.size()>0) {
            if(!info->signaled) {
                info->sourceWaitLock.signal();
                info->signaled=true;
            }
        }
        info->waitingThreads++;
        info->sourceLock.unlock();
    }
    
    // a: c, b
    // b: c
    // c: 
    // a, c, b
    
    // TODO: Run Preprocess in multiple threads. This requires a depencency system
    //   since one stream may need another stream's macros so that stream must
    //   be processed first.

    // Use an array of streams where a stream in it will only depend on stream to the right.
    // You use an integer called currentDependencyDepth (or something). Each stream has it's own
    // dependency depth. If the current depth is less or equal than a steam's depth, it is safe to
    // process that stream. The question then is how to calculate the depth.
    // There may be a flaw with this approach.

    _LOG(LOG_IMPORTS,
    FORN(info->streams) {
        auto it = info->streams[nr];
        log::out << "Stream "<<nr << " " <<BriefString(it->initialStream->streamName,15)<< ": ind:"<<it->index << " deps:\n";
        for(int j=0;j<it->dependencies.size();j++) {
            if(j!=0) log::out << ", ";
            log::out << " "<<it->dependencies[j];
        }
        if(it->dependencies.size()!=0)
            log::out << "\n";
    }
    )

    info->sourceLock.lock();
    // int readDependencyIndex = info->streams.size()-1;
    // info->completedDependencyIndex = info->streams.size();
    info->waitingThreads++;
    info->availableThreads++;
    // log::out << "Cool\n";
    if(info->streams.size()>0 && info->streams[0]->initialStream && !info->streams[0]->completed) {
        auto stream = info->streams[0];
        Assert(stream->initialStream->streamName == "<base>");
        // log::out << "Pre "<<BriefString(stream->initialStream->streamName)<<"\n";
        _VLOG(log::out <<log::BLUE<< "Preprocess: "<<BriefString(stream->initialStream->streamName)<<"\n";)
        stream->finalStream = Preprocess(info, stream->initialStream);
        stream->completed = true;
        stream->available = false;
        
        info->compileOrder.add(0);
    }
    // log::out << "End\n";

    // for(int i=0;i<info->streams.size();i++){
    //     StreamToProcess* stream = info->streams[i];
    //     log::out << BriefString(stream->initialStream->streamName,10)<<"\n";
    // }
    // if(info->waitingThreads == info->compileOptions->threadCount)
    //     info->sourceWaitLock.signal(); // we must signal to prevent all threads from waiting
    info->sourceLock.unlock();
    // Another solution would be to have a list of dependencies for each stream but you would need
    // to do a lot of linear searches so you probably want to avoid that.
    // info->dependencyIndex = info->streams.size()-1;
    // return 0;
    WHILE_TRUE {
        MEASURE_WHO("Process macro")
        info->sourceWaitLock.wait();

        // TODO: Is it possible to skip some locks and use atomic intrinsics instead?
        info->sourceLock.lock();
        info->signaled = false;
        info->waitingThreads--;
        // TODO: Optimize by not going starting from the end and going through the whole stream array. Start from an index where
        //  you know that everything to the right has been processed.
        if(info->completedStreams == info->streams.size()) {
            if(!info->signaled) {
                info->sourceWaitLock.signal();
                info->signaled = true;
            }
            info->sourceLock.unlock();
            break;
        }
        StreamToProcess* stream = nullptr;
        int index = info->streams.size()-1; // TODO: Optimize by choosing an index were the all streams on the right side are completed
        bool allCompleted = true;
        bool circularDependency = true;
        while(index >= 0) {
            stream = info->streams[index];
            if(stream->available) { // && stream->dependencyIndex >= info->completedDependencyIndex) {
                // log::out << "avail "<<index<<"\n";
                bool ready = true;
                for(int di=0;di<stream->dependencies.size();di++) {
                    int i = stream->dependencies[di];
                    // log::out << " "<<i<<" "<<info->streams[i]->completed<<"\n";
                    if(!info->streams[i]->completed) {
                        ready = false;
                        break;
                    }
                }
                if(ready) {
                    // log::out << "ready, compute "<<index<<"\n";
                    stream->available = false;
                    circularDependency = false;
                    break;
                }
            } else {
                if(!stream->completed) {
                    // log::out << "being processed "<<index<<"\n";
                    circularDependency = false;
                }
            }
            if(!stream->completed){
                allCompleted = false;
            }
            index--;
        }
        if(index == -1) {
            if(allCompleted) {
                if(!info->signaled) {
                    info->sourceWaitLock.signal();
                    info->signaled = true;
                }
                info->sourceLock.unlock();
                break;
            } else {
                if(circularDependency) {
                    if(!info->circularError) {
                        log::out << log::RED <<"There is a circular dependency when importing files. Circular imports are not supported right now (problems with preprocessor).\n";
                        log::out << log::BLOOD <<"Files causing circular dependencies:\n";
                        for(int i=0;i<info->streams.size();i++) {
                            auto stream = info->streams[i];
                            // log::out << "s "<<i<<"\n";
                            for(auto di : stream->dependencies) {
                                // log::out << " d "<<di<<"\n";
                                auto stream2 = info->streams[di];
                                for(int j=0;j<stream2->dependencies.size();j++) {
                                    int di2 = stream2->dependencies[j];
                                    // log::out << "  a "<<di2<<"\n";
                                    if(i == di2) {
                                        stream2->dependencies.removeAt(j);
                                        j--;
                                        log::out << log::CYAN << BriefString(stream->initialStream->streamName, 24) << log::GOLD<< " <-> "<< log::CYAN <<BriefString(stream2->initialStream->streamName,24)<<"\n";
                                    }
                                }
                            }
                        }
                        log::out << "\n";
                        info->circularError = true;
                    }
                }

                info->waitingThreads++;
                info->sourceLock.unlock();
                continue;
            }
        }
        info->compileOrder.add(index);
        if(!info->signaled) {
            info->sourceWaitLock.signal(); // one stream was available there may be more
            info->signaled = true;
        }
        info->sourceLock.unlock();

         // are used in this source file
        if (stream->initialStream && stream->initialStream->enabled & LAYER_PREPROCESSOR) {
            // log::out << "Pre "<<BriefString(stream->initialStream->streamName)<<"\n";
            _VLOG(log::out <<log::BLUE<< "Preprocess: "<<BriefString(stream->initialStream->streamName)<<"\n";)
            stream->finalStream = Preprocess(info, stream->initialStream);
        }

        info->sourceLock.lock();

        stream->completed = true;
        info->completedStreams++;
        
        if(!info->signaled) {
            info->sourceWaitLock.signal();
            info->signaled = true;
        }
        info->waitingThreads++;
        info->sourceLock.unlock();
    }
    return 0;
}

Bytecode* CompileSource(CompileOptions* options) {
    using namespace engone;
    MEASURE
    
    #ifdef SINGLE_THREADED
    options->threadCount = 1;
    #else
    // log::out << log::YELLOW << "Multithreading is turned of because there are bugs and there is no point in fixing them since the system will change anyway. It's better to fix them then."
    #endif

    // NOTE: Parser and generator uses tokens. Do not free tokens before compilation is complete.
    options->compileStats.start_bytecode = engone::StartMeasure();
    
    CompileInfo compileInfo{};
    // compileInfo.nativeRegistry = NativeRegistry::GetGlobal();
    // compileInfo.nativeRegistry = NativeRegistry::Create();
    // compileInfo.nativeRegistry->initNativeContent();
    // compileInfo.targetPlatform = options->target;
    compileInfo.compileOptions = options;
    compileInfo.reporter.instant_report = options->instant_report;
    
    compileInfo.ast = AST::Create();
    defer {
        if(compileInfo.ast) {
            AST::Destroy(compileInfo.ast);
            compileInfo.ast = nullptr;
        }
    };
    // compileInfo.compilerDir = TrimLastFile(compilerPath);
    options->modulesDirectory = options->modulesDirectory.getAbsolute();
    compileInfo.importDirectories.add(options->modulesDirectory.text);
    for(auto& path : options->importDirectories){
        compileInfo.importDirectories.add(path);
    }
    #ifndef DISABLE_BASE_IMPORT
    StringBuilder essentialStructs{};
    essentialStructs +=
    // "struct Slice<T> {"
    "struct @hide Slice<T> {"
        "ptr: T*;"
        "len: u64;"
    "}\n"
    "operator [](slice: Slice<char>, index: u32) -> char {"
        "return slice.ptr[index];"
    "}\n"
    // "struct Range {" 
    "struct @hide Range {" 
        "beg: i32;"
        "end: i32;"
    "}\n"
    #ifdef OS_WINDOWS
    "#define OS_WINDOWS\n"
    #else
    // #elif defined(OS_UNIX)
    "#define OS_UNIX\n"
    #endif
    "fn @native prints(str: char[]);\n"
    "fn @native printc(str: char);\n"
    // "#define Assert(X) { prints(#quoted X); }"
    // "#define Assert(X) { prints(#quoted X); *cast<char>null; }"
    ;
    essentialStructs += (options->target == TARGET_BYTECODE ? "#define LINK_BYTECODE\n" : "");
    TextBuffer essentialBuffer{};
    essentialBuffer.origin = "<base>";
    essentialBuffer.size = essentialStructs.size();
    essentialBuffer.buffer = essentialStructs.data();
    
    SourceToProcess essentialSource{};
    essentialSource.textBuffer = &essentialBuffer;
    essentialSource.stream = compileInfo.addStream(essentialSource.textBuffer->origin);
    compileInfo.sourcesToProcess.add(essentialSource);
    #endif

    SourceToProcess initialSource{};
    if(options->initialSourceBuffer.buffer){
        initialSource.textBuffer = &options->initialSourceBuffer;
    } else {
        initialSource.path = options->sourceFile;
    }
    initialSource.stream = compileInfo.addStream(initialSource.path);
    compileInfo.sourcesToProcess.add(initialSource);
    
    // Assert(options->threadCount == 1); // TODO: Implement not multithreading

    compileInfo.sourceWaitLock.init(1, 1);

    TINY_ARRAY(ThreadCompileInfo, threadInfos, 4);
    threadInfos.resize(options->threadCount);
    memset((void*)threadInfos._ptr, 0, sizeof(ThreadCompileInfo)*threadInfos.max);
    // ThreadCompileInfo threadInfos[CompileInfo::THREAD_COUNT]{};
    // DynamicArray<ThreadCompileInfo>
    for(int i=1;i<options->threadCount;i++){
        threadInfos[i].info = &compileInfo;
        threadInfos[i]._thread.init(ProcessSource, threadInfos._ptr+i);
    }
    threadInfos[0].info = &compileInfo;
    ProcessSource(threadInfos._ptr + 0);

    for(int i=1;i<options->threadCount;i++){
        threadInfos[i]._thread.join();
    }
    // for(int i=0;i<compileInfo.compileOrder.size();i++){
    //     StreamToProcess* stream = compileInfo.streams[i];
    //     log::out << BriefString(stream->initialStream->streamName,10)<<"\n";
    // }
    for(int i=0;i<compileInfo.compileOrder.size();i++) {
        StreamToProcess* stream = compileInfo.streams[i];
        
        TokenStream* procStream = stream->finalStream ? stream->finalStream : stream->initialStream;
        if(procStream) {
            _VLOG(log::out <<log::BLUE<< "Parse: "<<BriefString(stream->initialStream->streamName)<<"\n";)
            ASTScope* body = ParseTokenStream(procStream,compileInfo.ast, &compileInfo, stream->as);
            if(body){
                if(stream->as.empty()){
                    compileInfo.ast->appendToMainBody(body);
                } else {
                    compileInfo.ast->mainBody->add(compileInfo.ast, body);
                }
            }
        }
    }
    // {
    //     int i = compileInfo.streams.size()-1; // process essential first
    //     StreamToProcess* stream = compileInfo.streams[i];
    //      // are used in this source file
    //     // if (!stream->finalStream && (stream->initialStream->enabled & LAYER_PREPROCESSOR)) {
    //     //     _VLOG(log::out <<log::BLUE<< "Preprocess: "<<BriefString(stream->initialStream->streamName)<<"\n";)
    //     //     stream->finalStream = Preprocess(&compileInfo, stream->initialStream);
    //     // }

    //     TokenStream* procStream = stream->finalStream ? stream->finalStream : stream->initialStream;
    //     if(procStream) {
    //         _VLOG(log::out <<log::BLUE<< "Parse: "<<BriefString(stream->initialStream->streamName)<<"\n";)
    //         ASTScope* body = ParseTokenStream(procStream,compileInfo.ast, &compileInfo, stream->as);
    //         if(body){
    //             if(stream->as.empty()){
    //                 compileInfo.ast->appendToMainBody(body);
    //             } else {
    //                 compileInfo.ast->mainBody->add(compileInfo.ast, body);
    //             }
    //         }
    //     }
    // }
    // for(int i=compileInfo.streams.size()-2;i>=0;i--){
    //     StreamToProcess* stream = compileInfo.streams[i];
    //      // are used in this source file
    //     // TokenStream* procStream = stream->initialStream;
    //     // if (stream->initialStream->enabled & LAYER_PREPROCESSOR) {
    //     //     _VLOG(log::out <<log::BLUE<< "Preprocess: "<<BriefString(stream->initialStream->streamName)<<"\n";)
    //     //     procStream = Preprocess(&compileInfo, stream->initialStream);
    //     //     // if(macroBenchmark){
    //     //     //     log::out << log::LIME<<"Finished with " << finalStream->length()<<" token(s)\n";
    //     //     //     #ifndef LOG_MEASURES
    //     //     //     if(finalStream->length()<50){
    //     //     //         finalStream->print();
    //     //     //         log::out<<"\n";
    //     //     //     }
    //     //     //     #endif
    //     //     // }
    //     //     stream->finalStream = procStream;
    //     //     // tokenStream->print();
    //     // }
    //     TokenStream* procStream = stream->finalStream ? stream->finalStream : stream->initialStream;
    //     if(procStream) {
    //         _VLOG(log::out <<log::BLUE<< "Parse: "<<BriefString(stream->initialStream->streamName)<<"\n";)
    //         ASTScope* body = ParseTokenStream(procStream,compileInfo.ast, &compileInfo, stream->as);
    //         if(body){
    //             if(stream->as.empty()){
    //                 compileInfo.ast->appendToMainBody(body);
    //             } else {
    //                 compileInfo.ast->mainBody->add(compileInfo.ast, body);
    //             }
    //         }
    //     }
    // }

    _LOG(LOG_AST,log::out << log::BLUE<< "Final "; compileInfo.ast->print();)
    TypeCheck(compileInfo.ast, compileInfo.ast->mainBody, &compileInfo);
    
    // if(compileInfo.errors==0){
    // }
    // if (tokens.enabled & LAYER_GENERATOR){

    Bytecode* bytecode = nullptr;
    _VLOG(log::out <<log::BLUE<< "Generating code:\n";)
    // auto tp = StartMeasure();
    bytecode = Generate(compileInfo.ast, &compileInfo);
    // log::out << "TIM: "<<StopMeasure(tp)*1000<<"\n";
    if(bytecode) {
        compileInfo.compileOptions->compileStats.bytecodeSize = bytecode->getMemoryUsage();
        bytecode->linkDirectives.stealFrom(compileInfo.linkDirectives);

        if(!options->useDebugInformation && bytecode->debugInformation) {
            // We don't want debug information unless we specified it in 
            // the options. We delete the information here as a temporary
            // solution but we shouldn't create it to begin with.
            DebugInformation::Destroy(bytecode->debugInformation);
            bytecode->debugInformation = nullptr;
        }
    }
    if(bytecode->debugInformation && bytecode->debugInformation->ast) {
        bytecode->debugInformation->ownerOfAST = true;
        compileInfo.ast = nullptr; // steal AST
        for(auto& stream : compileInfo.tokenStreams) {
            if(stream.second->finalStream)
                bytecode->debugInformation->tokenStreams.add(stream.second->finalStream);
            if(stream.second->initialStream)
                bytecode->debugInformation->tokenStreams.add(stream.second->initialStream);
            stream.second->finalStream = nullptr;
            stream.second->initialStream = nullptr;
        }
        for(auto& stream : compileInfo.includeStreams) {
            bytecode->debugInformation->tokenStreams.add(stream.second);
        }
        compileInfo.includeStreams.clear();
        // compileInfo.tokenStreams.clear(); // don't clear, we stole the token streams but there is other stuff that needs to be cleared by CompileInfo::cleanup
    }
    if (compileInfo.ast) {
        AST::Destroy(compileInfo.ast);
        compileInfo.ast = nullptr;
    }

    options->compileStats.end_bytecode = StartMeasure();

    if(compileInfo.compileOptions->compileStats.errors!=0){ 
        if(!options->silent)
            compileInfo.compileOptions->compileStats.printFailed();
        // if(compileInfo.nativeRegistry == bytecode->nativeRegistry) {
        //     bytecode->nativeRegistry = nullptr;
        // }
        Bytecode::Destroy(bytecode);
        bytecode = nullptr;
    }
    if(compileInfo.compileOptions->compileStats.warnings!=0){
        if(!options->silent)
            compileInfo.compileOptions->compileStats.printWarnings();
    }

    return bytecode;
}

int CompileOptions::addTestLocation(TokenRange& range){
    testLocations.add({});
    testLocations.last().line = range.firstToken.line;
    testLocations.last().column = range.firstToken.column;
    testLocations.last().file = Path{range.tokenStream()->streamName}.getFileName().text;
    return testLocations.size()-1;
}
CompileOptions::TestLocation* CompileOptions::getTestLocation(int index) {
    return testLocations.getPtr(index);
}
void CompileStats::printSuccess(CompileOptions* opts){
    using namespace engone;
    log::out << "Compiled " << log::AQUA << FormatUnit((u64)lines)<<log::NO_COLOR << " non-blank lines ("<<log::AQUA<<FormatUnit((u64)lines + (u64)blankLines)<<log::NO_COLOR<<" total, "<<log::AQUA<<FormatBytes(readBytes)<<log::NO_COLOR<<")\n";
    if(opts) {
        /* Would it be nicer with this:
            details: win-x64, gcc
            files: examples/dev.btb, bin/dev.obj, dev.exe
        */
        if(opts->initialSourceBuffer.buffer) {
            log::out << " text buffer: "<<opts->initialSourceBuffer.origin<<"\n";
        } else {
            // log::out << " initial file: "<< opts->initialSourceFile.getFileName().text<<"\n";
            log::out << " initial file: "<< BriefString(opts->sourceFile.getAbsolute().text,30)<<"\n";
        }
        log::out << " target: "<<log::AQUA<<opts->target<<log::NO_COLOR<<", linker: " << log::AQUA<< opts->linker<<log::NO_COLOR << ", output: ";
        if(opts->compileStats.generatedFiles.size() == 0) {
            log::out << "nothing";
        }
        for(int i=0;i<(int)opts->compileStats.generatedFiles.size();i++){
            if(i!=0)
                log::out << ", ";
            log::out << opts->compileStats.generatedFiles[i];
        }
        log::out << "\n";
    }
    bool onlyBytecode = end_objectfile == 0;
    bool withoutLinker = end_linker == 0;
    if(onlyBytecode) {
        double time_bytecode = DiffMeasure(end_bytecode - start_bytecode);
        log::out << " source->bytecode (total): " <<log::LIME<< FormatTime(time_bytecode)<<log::NO_COLOR << " (" << FormatUnit(lines / time_bytecode) << " lines/s)\n";
    } else if(withoutLinker) {
        double time_compiler = DiffMeasure(end_objectfile - start_bytecode);
        double time_bytecode = DiffMeasure(end_bytecode - start_bytecode);
        double time_converter = DiffMeasure(end_convertx64 - start_convertx64);
        double time_objwriter = DiffMeasure(end_objectfile - start_objectfile);
        double time_total = DiffMeasure(end_objectfile - start_bytecode);
        log::out << " compiler: " <<log::LIME<< FormatTime(time_compiler)<<log::NO_COLOR << " (" << FormatUnit(lines / time_compiler) << " lines/s)\n";
        log::out << "  (bc: "<<log::LIME<< FormatTime(time_bytecode)<<log::NO_COLOR <<", x64: " <<log::LIME<< FormatTime(time_converter)<<log::NO_COLOR << ", obj: " <<log::LIME<< FormatTime(time_objwriter)<<log::NO_COLOR << ")\n";
        log::out << " total: "<<log::LIME<<FormatTime(time_total)<<log::NO_COLOR<<" (" << FormatUnit(lines / time_total) << " lines/s)\n"; 
    } else {
        double time_compiler = DiffMeasure(end_objectfile - start_bytecode);
        double time_bytecode = DiffMeasure(end_bytecode - start_bytecode);
        double time_converter = DiffMeasure(end_convertx64 - start_convertx64);
        double time_objwriter = DiffMeasure(end_objectfile - start_objectfile);
        double time_linker = DiffMeasure(end_linker - start_linker);
        double time_total = DiffMeasure(end_linker - start_bytecode);
        log::out << " compiler: " <<log::LIME<< FormatTime(time_compiler)<<log::NO_COLOR << " (" << FormatUnit(lines / time_compiler) << " lines/s)\n";
        log::out << "  (bc: "<<log::LIME<< FormatTime(time_bytecode)<<log::NO_COLOR <<", x64: " <<log::LIME<< FormatTime(time_converter)<<log::NO_COLOR << ", obj: " <<log::LIME<< FormatTime(time_objwriter)<<log::NO_COLOR << ")\n";
        log::out << " linker: " <<log::LIME<< FormatTime(time_linker)<<log::NO_COLOR << "\n";
        log::out << " total: "<<log::LIME<<FormatTime(time_total)<<log::NO_COLOR<<" (" << FormatUnit(lines / time_total) << " lines/s)\n"; 
        // log::out << " total: "<<log::LIME<<FormatTime(time_total)<<log::NO_COLOR<<" (" <<log::LIME<< FormatUnit(lines / time_total) << " lines/s"<<log::NO_COLOR<<")\n"; 
    }
}
void CompileStats::printFailed(){
    using namespace engone;

    double total = 0.0;
    bool withoutLinker = end_objectfile == 0;
    if(withoutLinker) {
        total = DiffMeasure(end_bytecode - start_bytecode);
    } else {
        total = DiffMeasure(end_linker - start_bytecode);
    }
    
    log::out << log::RED<<"Compiler failed with "<<errors<<" error(s) ("<<FormatTime(total)<<", "<<FormatUnit((u64)lines)<<" line(s), " << FormatUnit(lines / total) << " lines/s)\n";
}
void CompileStats::printWarnings(){
    using namespace engone;
    log::out << log::YELLOW<<"Compiler had "<<warnings<<" warning(s)\n";
}
// returns the amount of errors
int ReformatLinkerError(LinkerChoice linker, QuickArray<char>& inBuffer, Program_x64* program) {
    using namespace engone;
    Assert(program);
    
    struct ErrorInfo {
        std::string func;
        std::string textOffset_hex;
        std::string message;
        int textOffset;
        bool no_location = false;
        // std::string symbolName;
    };
    DynamicArray<ErrorInfo> errors{};
    ErrorInfo errorInfo{};

    switch(linker) {
    case LINKER_MSVC: {
    // NOTE: MSVC linker may sometimes provide hints about potential functions you meant to use.
    //  We ignore those at the moment. Perhaps we shouldn't?

    enum State {
        BEGIN, // beginning of a new error
        MESSAGE, // the error message
    };
    State state = BEGIN;
    std::string lastSymbol = "";
    std::string lastFunc = "";
    int msg_start = 0;
    int index = 0;
    int colon_count = 0;
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        char chr1 = 0;
        if(index+1<inBuffer.size())
            chr1 = inBuffer[index+1];
        char chr2 = 0;
        if(index+2<inBuffer.size())
            chr2 = inBuffer[index+2];
        index++;
        
        switch(state) {
            case BEGIN: {
                if(chr == ' ' && chr1 == ':' && chr2 == ' ') {
                    index+=2; // skip ': '
                    state = MESSAGE;
                    msg_start = index;
                }
                break;
            }
            case MESSAGE: {
                if(chr == '\n') {
                    int msg_end = index-1; // -1 to exclude newline
                    state = BEGIN;
                    errorInfo.message = std::string(inBuffer.data() + msg_start, msg_end - msg_start);
                    
                    int extras = 0;
                    if(program && !lastSymbol.empty()) {
                        // this is really slow
                        for(int i=0;i<program->namedUndefinedRelocations.size();i++) {
                            auto& rel = program->namedUndefinedRelocations[i];
                            if(rel.name == lastSymbol) {
                                errorInfo.textOffset = rel.textOffset;
                                errorInfo.textOffset_hex = NumberToHex(rel.textOffset);
                                errorInfo.func = lastFunc;
                                errorInfo.no_location = false;
                                errors.add(errorInfo);
                                extras++;
                            }
                        }
                    }
                    if(extras == 0) {
                        errorInfo.no_location = true;
                        errors.add(errorInfo);
                    }
                    lastFunc = "";
                    lastSymbol = "";
                    break;
                }
                const char* symbol_str = "external symbol ";
                int symbol_len = strlen(symbol_str);
                int correct = 0;
                for(;correct<symbol_len && index+correct < inBuffer.size();correct++) {
                    char c = inBuffer[index + correct];
                    if(c != symbol_str[correct])
                        break;
                }
                if(correct == symbol_len) {
                    index += symbol_len;
                    int symbol_start = index;
                    while(index<inBuffer.size()) {
                        char chr = inBuffer[index];
                        index++;
                        if(chr == ' ')
                            break;
                    }   
                    int symbol_end = index-1;
                    lastSymbol = std::string(inBuffer.data() + symbol_start, symbol_end - symbol_start);
                }
                const char* func_str = "in function ";
                int func_len = strlen(func_str);
                correct = 0;
                for(;correct<func_len && index+correct < inBuffer.size();correct++) {
                    char c = inBuffer[index + correct];
                    if(c != func_str[correct])
                        break;
                }
                if(correct == func_len) {
                    index += func_len;
                    int func_start = index;
                    while(index<inBuffer.size()) {
                        char chr = inBuffer[index];
                        index++;
                        if(chr == ' ' || chr == '\n') {
                            index--;
                            break;
                        }
                    }
                    int func_end = index-1;
                    lastFunc = std::string(inBuffer.data() + func_start, func_end - func_start);
                }
                break;
            }
            default: {
                Assert(false);
                break;
            }
        }
    }
    break;
    }
    case LINKER_GCC: {
    // HOLY CRAP THIS CODE IS BEAUTIFUL AND SIMPLE
    enum State {
        BEGIN, // beginning of a new error
        FUNCTION,
        OFFSET, // parse text offset
        MESSAGE, // the error message
    };
    State state = BEGIN;
    int hex_start = 0;
    int msg_start = 0;
    int func_start = 0;
    int index = 0;
    while(index < inBuffer.size()) {
        char chr = inBuffer[index];
        char chr1 = 0;
        if(index+1<inBuffer.size())
            chr1 = inBuffer[index+1];
        char chr2 = 0;
        if(index+2<inBuffer.size())
            chr2 = inBuffer[index+2];
        index++;
        
        switch(state) {
            case BEGIN: {
                if(chr == '+' && chr1 == '0' && chr2 == 'x') {
                    state = OFFSET;
                    index+=2;
                    hex_start = index;
                }
                // This catches the usr/bin/ld message which may contain good info such as: /usr/bin/ld: DWARF error: offset (4294967292) greater than or equal to .debug_abbrev size (117)
                
                if(chr == 'l' && chr1 == 'd' && chr2 == ':') {
                    state = MESSAGE;
                    errorInfo.no_location = true;
                    index+=3; // skip "d: "
                    msg_start = index;
                }
                if(chr == ':') {
                    static const char* const func_msg = " in function";
                    static const int func_msg_len = strlen(func_msg);
                    int correct = 0;
                    for(;correct<func_msg_len && index+correct < inBuffer.size();correct++) {
                        char c = inBuffer[index + correct];
                        if(c != func_msg[correct])
                            break;
                    }
                    if(correct == func_msg_len) {
                        index += func_msg_len +2; // +2 to skip ' `'
                        state = FUNCTION;
                        func_start = index;
                    }
                }
                break;
            }
            case FUNCTION: {
                if(chr == '\'') {
                    int func_end = index-1;
                    index++; // skip :
                    state = BEGIN;
                    errorInfo.func = std::string(inBuffer.data() + func_start, func_end - func_start);
                }
                break;
            }
            case OFFSET: {
                // Parse hexidecimal
                if(chr == ')') {
                    int hex_end = index-1; // exclusive, -1 since index is at : right now

                    u64 off = ConvertHexadecimal_content(inBuffer.data() + hex_start, hex_end - hex_start);
                    errorInfo.no_location = false;
                    errorInfo.textOffset = off;
                    errorInfo.textOffset_hex = std::string(inBuffer.data() + hex_start, hex_end - hex_start);

                    index+=2; // skip ': '

                    state = MESSAGE;
                    msg_start = index;
                }
                break;
            }
            case MESSAGE: {
                if(chr == '\n') {
                    int msg_end = index-1; // -1 to exclude newline
                    state = BEGIN;
                    errorInfo.message = std::string(inBuffer.data() + msg_start, msg_end - msg_start);
                    errors.add(errorInfo);
                }
                break;
            }
            default: {
                Assert(false);
                break;
            }
        }
    }
    break;
    }
    default: {
        log::out << log::GOLD << "Reformat of linker errors for "<<ToString(linker) << " is incomplete. Printing direct errors from linker.\n";
        log::out.print(inBuffer.data(), inBuffer.size());
        log::out.flush();
        break;
    }
    }
    
    if(errors.size() > 0 && (!program || !program->debugInformation)) {
        // NOTE: program == nullptr shouldn't really happen
        log::out << log::GOLD << "Enabling debug information will display "<<log::RED<<"file:line"<<log::GOLD<<" instead of "<<log::RED<<".text+0x0"<<log::GOLD<<"\n";
    }
    FOR(errors) {
        bool found = false;
        if(!it.no_location && program && program->debugInformation) {
            auto d = program->debugInformation;
            int lineNumber = 0;
            int fileIndex = 0;
            // TODO: This will be slow with many functions
            for(int k=0;k<d->functions.size();k++) {
                auto& func = d->functions[k];
                if(func.funcStart <= it.textOffset && it.textOffset < func.funcEnd) {
                    fileIndex = func.fileIndex;
                    if(func.lines.size()>0)
                        lineNumber = func.lines[0].lineNumber;
                    for(int i=0;i < func.lines.size();i++) {
                        auto& line = func.lines[i];
                        if(it.textOffset < func.funcStart + line.funcOffset) { // NOTE: Is funcStart + funcOffset correct?
                            found = true;
                            break;
                        }
                        lineNumber = line.lineNumber;
                    }
                    if(!found) {
                        if(func.lines.size()>0)
                            lineNumber = func.lines.last().lineNumber;
                        found = true;
                    }
                    if(found)
                        break;
                }
            }
            if(found) {
                log::out << log::RED << TrimCWD(d->files[fileIndex])<<":" << lineNumber<<": ";
                log::out << log::NO_COLOR << it.message << "\n";
            }
        }
        if(!found) {
            if(it.no_location) {
                log::out << log::RED << "linker: ";
            } else if(it.func.empty()) {
                log::out << log::RED << ".text+0x" << it.textOffset_hex<<": ";
            } else {
                log::out << log::RED << ".text+0x" << it.textOffset_hex<<" ("<<it.func<<"): ";
            }
            log::out << log::NO_COLOR << it.message << "\n";
        }
    }
    return errors.size();
}
bool ExportTarget(CompileOptions* options, Bytecode* bytecode) {
    using namespace engone;
    MEASURE
    
    if(options->target == TARGET_BYTECODE) {
        // Bytecode exporting has been disabled because you rarely use it.
        // You usually want to execute it right away or at compile time.
        // If you want to save time by compiling the bytecode in advance then
        // you probably want to save even more time by compiling into machine instructions
        // instead of bytecode which runs in an interpreter.
        //     return ExportBytecode(options, bytecode);
        //     break;
        log::out << log::RED << "Compiler does not support BYTECODE as an exportable target\n";
        return false;
    }

    #ifdef DISABLE_LINKING
    log::out << log::YELLOW << "Linker is disabled in Config.h\n";
    options->executeOutput = false;
    if(options->outputFile.text.empty()) {
        options->outputFile = "bin/temp.obj";
    } else if(options->outputFile.getFormat() != "obj") {
        options->outputFile = options->outputFile.getDirectory().text
            + options->outputFile.getFileName(true).text
            + ".obj";
    }
    #endif

    if(options->target != TARGET_BYTECODE) {
        // Right now, we can only compile for the platform you are on.
        // That is because we use some default linkers on the user's computer.
        #ifdef OS_WINDOWS
        if(options->target != TARGET_WINDOWS_x64) {
            log::out << log::RED << "Cannot compile target '"<<ToString(options->target)<<"' on Windows\n";
            return false;
        }
        #else
        if(options->target != TARGET_UNIX_x64) {
            log::out << log::RED << "Cannot compile target '"<<ToString(options->target)<<"' on Unix\n";
            return false;
        }
        #endif
    }

    if(options->outputFile.text.empty()) {
        switch (options->target) {
            case TARGET_WINDOWS_x64: {
                options->outputFile = "bin/temp.exe";
                break;
            }
            case TARGET_BYTECODE: {
                // nothing?
                break;
            }
            case TARGET_UNIX_x64: {
                options->outputFile = "bin/temp";
                break;
            }
            default: {
                Assert(false);
            }
        }
    }

    if(options->target != TARGET_WINDOWS_x64 && options->target != TARGET_UNIX_x64) {
        log::out << log::RED << "Cannot export "<<options->target << " (not supported).\n";
        return false;
    }
    Path& outPath = options->outputFile;
    if(options->target == TARGET_WINDOWS_x64) {
        if(outPath.getFormat().empty()) {
            outPath.text += ".exe";
            // Windows doesn't run executables unless they have .exe.
            // You have to force it at least so we might as well ensure
            // .exe is there.
        }
    }

    options->compileStats.start_convertx64 = StartMeasure();
    Program_x64* program = ConvertTox64(bytecode);
    defer { if(program) Program_x64::Destroy(program); };
    options->compileStats.end_convertx64 = StartMeasure();

    if(!program){
        return false;
    }

    auto format = outPath.getFormat();
    bool outputIsObject = format == "o" || format == "obj";

    if(outputIsObject){
        bool yes = false;
        options->compileStats.start_objectfile = StartMeasure();
        switch(options->target) {
        case TARGET_WINDOWS_x64:
            yes = FileCOFF::WriteFile(outPath.text, program);
            break;
        case TARGET_UNIX_x64:
            yes = FileELF::WriteFile(outPath.text, program);
            break;
        default:
            Assert(false);
            break;
        }
        options->compileStats.end_objectfile = StartMeasure();
        if(yes) {
            options->compileStats.generatedFiles.add(outPath.text);
        } else {
            log::out << log::RED << "Failed writing object file '"<<outPath<<"'\n";
            return false;
        }
        return true;
    }

    std::string objPath = "";
    bool yes = false;
    options->compileStats.start_objectfile = StartMeasure();
    switch(options->target) {
    case TARGET_WINDOWS_x64:
        objPath = "bin/" + outPath.getFileName(true).text + ".obj";
        yes = ObjectFile::WriteFile(OBJ_COFF, objPath, program);
        // yes = FileCOFF::WriteFile(objPath,program);
        break;
    case TARGET_UNIX_x64:
        objPath = "bin/" + outPath.getFileName(true).text + ".o";
        yes = ObjectFile::WriteFile(OBJ_ELF, objPath, program);
        // yes = FileELF::WriteFile(objPath, program);
        break;
    default: {
        Assert(false);
        break;
    }
    }
    options->compileStats.end_objectfile = StartMeasure();
    if(yes) {
        options->compileStats.generatedFiles.add(objPath);
    } else {
        log::out << log::RED << "Failed writing object file '"<<objPath<<"'\n";
        return false;
    }

    std::string cmd = "";
    bool outputOtherDirectory = false;
    switch(options->linker) {
    case LINKER_MSVC: {
        Assert(options->target == TARGET_WINDOWS_x64);
        outputOtherDirectory = outPath.text.find("/") != std::string::npos;
        
        // TEMPORARY
        if(options->useDebugInformation) {
            log::out << log::RED << "You must use another linker than MSVC when compiling with debug information. Add the flag "<<log::LIME<<"--linker gcc"<<log::RED<<" (make sure to have gcc installed). The compiler does not support PDB debug information, it only supports DWARF. DWARF uses sections with long names but MSVC linker truncates those names. That's why you cannot use MVSC linker.\n";
            return false;
        }

        cmd = "link /nologo /INCREMENTAL:NO ";
        if(options->useDebugInformation)
            cmd += "/DEBUG ";
        // else cmd += "/DEBUG "; // force debug info for now
        cmd += objPath + " ";
        #ifndef MINIMAL_DEBUG
        cmd += "bin/NativeLayer.lib ";
        cmd += "uuid.lib ";
        cmd += "shell32.lib ";
        // cmd += "Bcrypt.lib "; // random64 uses BCryptGenRandom, #link is used instead
        #endif
        // I don't know which of these we should use when. Sometimes the linker complains about 
        // a certain default lib.
        // cmd += "/NODEFAULTLIB:library";
        // cmd += "/DEFAULTLIB:MSVCRT ";
        cmd += "/DEFAULTLIB:LIBCMT ";
        
        for (int i = 0;i<(int)bytecode->linkDirectives.size();i++) {
            auto& dir = bytecode->linkDirectives[i];
            cmd += dir + " ";
        }
        #define LINK_TEMP_EXE "temp_382.exe"
        #define LINK_TEMP_PDB "temp_382.pdb"
        if(outputOtherDirectory) {
            // MSVC linker cannot output to another directory than the current.
            cmd += "/OUT:" LINK_TEMP_EXE;
        } else {
            cmd += "/OUT:" + outPath.text+" ";
        }
        break;
    }
    case LINKER_CLANG:
    case LINKER_GCC: {
        switch(options->linker) {
            case LINKER_GCC: cmd += "g++ "; break;
            case LINKER_CLANG: cmd += "clang++ "; break;
            default: break;
        }
        
        if(options->useDebugInformation)
            cmd += "-g ";

        cmd += objPath + " ";
        #ifndef MINIMAL_DEBUG
        cmd += "bin/NativeLayer_gcc.lib "; // NOTE: Do we need one for clang too?
        // cmd += "uuid.lib ";
        // cmd += "shell32.lib ";
        #endif
        // cmd += "/DEFAULTLIB:MSVCRT ";
        // cmd += "/DEFAULTLIB:LIBCMT ";
        
        for (int i = 0;i<(int)bytecode->linkDirectives.size();i++) {
            auto& dir = bytecode->linkDirectives[i];
            cmd += dir + " ";
        }
        // if(outputOtherDirectory) {
        cmd += "-o " + outPath.text;
        // } else {
            // cmd += "/OUT:" + outPath.text+" ";
        // }
        break;
    }
    default: {
        Assert(false);
        break;
    }
    }

    if(!options->silent)
        log::out << log::LIME<<"Link cmd: "<<cmd<<"\n";
    
    auto linkerLog = engone::FileOpen("bin/linker.log",FILE_CLEAR_AND_WRITE);
    defer { engone::FileClose(linkerLog); };

    options->compileStats.start_linker = StartMeasure();
    int exitCode = 0;
    engone::StartProgram((char*)cmd.c_str(),PROGRAM_WAIT, &exitCode, {}, linkerLog, linkerLog);
    options->compileStats.end_linker = StartMeasure();
    
    if(exitCode != 0) {
        // linker failed
        u64 fileSize = engone::FileGetSize(linkerLog);
        engone::FileSetHead(linkerLog, 0);
        
        QuickArray<char> errorMessages{};
        errorMessages.resize(fileSize);
        
        yes = engone::FileRead(linkerLog, errorMessages.data(), errorMessages.size());
        Assert(yes);
        
        int errors = ReformatLinkerError(options->linker, errorMessages, program);
        if(errors == 0) {
            // If no errors were printed then the linker gave us some output the reformat function
            // can't handle. Instead of printing nothing to the user, it's better to print the direct messages.
            log::out.print(errorMessages.data(), errorMessages.size());
            log::out.flush();
        } else {
            if(errors > 100) {
                log::out << log::YELLOW << "HEY! I (the compiler) noticed that you have "<<errors<<" linker errors. "
                    "This is rare an usually caused by mixing libraries/code form different linkers.\n"
                    "Perhaps NativeLayer.lib was compiled with MSVC while you chose GCC for the .btb files? "
                    "You must use the same but in the future, NativeLayer.lib will be removed and compiled using .btb files\n"
                ;
            }
        }
        return false;
    }
    if(outputOtherDirectory){
        // Since MSVC linker can only output to CWD we have to move the file ourself.
        // TODO: handle errors in file delete/move
        FileDelete(outPath.text);
        FileMove(LINK_TEMP_EXE, outPath.text);
        // if(options->useDebugInformation) {
        //     Path p = outPath.getDirectory().text + outPath.getFileName(true).text + ".pdb";
        //     FileDelete(p.text);
        //     FileMove(LINK_TEMP_PDB,p.text);
        // }
    }
    options->compileStats.generatedFiles.add(outPath.text);

    // TODO: Move the asm dump code to it's own file/function?
    QuickArray<char> textBuffer{};
    for(int i=0;i<(int)bytecode->debugDumps.size();i++) {
        auto& dump = bytecode->debugDumps[i];
        if(dump.description.empty()) {
            log::out << "Dump: "<<log::GOLD<<"unnamed\n";
        } else {
            log::out << "Dump: "<<log::GOLD<<dump.description<<"\n";
        }
        if(dump.dumpBytecode) {
            for(int j=dump.bc_startIndex;j<dump.bc_endIndex;j++){
                log::out << " ";
                bytecode->printInstruction(j, true);
                u8 immCount = bytecode->immediatesOfInstruction(j);
                j += immCount;
            }
        }
        if(dump.dumpAsm) {
            const char* DUMP_ASM_OBJ = "bin/dump_asm.o";
            const char* DUMP_ASM_ASM = "bin/dump_asm.asm";

            bool yes = false;
            switch(options->target){
            case TARGET_WINDOWS_x64:
                yes = FileCOFF::WriteFile(DUMP_ASM_OBJ, program, dump.asm_startIndex, dump.asm_endIndex);
                break;
            case TARGET_UNIX_x64:
                yes = FileELF::WriteFile(DUMP_ASM_OBJ, program, dump.asm_startIndex, dump.asm_endIndex);
                break;
            default:
                Assert(false);
                break;
            }
            // if(yes)
            //     options->compileStats.generatedFiles.add(DUMP_ASM_OBJ);

            auto file = engone::FileOpen(DUMP_ASM_ASM, FILE_CLEAR_AND_WRITE);
            defer { if(file) engone::FileClose(file); };

            std::string cmd = "";
            switch(options->linker) {
            case LINKER_MSVC:
                cmd = "dumpbin /nologo /DISASM:BYTES ";
                cmd += DUMP_ASM_OBJ;
                break;
            case LINKER_GCC:
                    cmd = "objdump -Mintel -d ";
                cmd += DUMP_ASM_OBJ;
                break;
            default:
                Assert(false);
                break;
            }
            
            int exitCode = 0;
            engone::StartProgram((char*)cmd.c_str(),PROGRAM_WAIT, &exitCode, {}, file);

            u32 fileSize = engone::FileGetHead(file);
            engone::FileSetHead(file, 0);
            textBuffer.resize(fileSize);
            engone::FileRead(file, textBuffer._ptr, fileSize);

            if(exitCode != 0) {
                // something went wrong, show the raw output.
                log::out.print(textBuffer.data(), textBuffer.size());
                log::out.flush();
                // DON'T RETURN FALSE HERE. It's just the dump that failed.
            } else {
                ReformatDumpbinAsm(options->linker, textBuffer, nullptr, true);
            }
        }
    }

    #ifdef DUMP_ALL_ASM
    // program->printHex("bin/temphex.txt");
    program->printAsm("bin/temp.asm", objPath.data());
    #endif
    
    return true;
}
bool ExecuteTarget(CompileOptions* options, Bytecode* bytecode) {
    using namespace engone;
    switch (options->target) {
        case TARGET_BYTECODE: {
            Assert(bytecode);
            Interpreter interpreter{};
            interpreter.setCmdArgs(options->userArguments);
            interpreter.silent = options->silent;
            interpreter.execute(bytecode);
            interpreter.cleanup();
            break; 
        }
        case TARGET_WINDOWS_x64: {
            // if(options->outputFile.getFormat() != "exe") {
            //     log::out << log::RED << "Cannot execute '" << options->outputFile.getFileName().text <<"'. The file format must be '.exe'.\n";
            // } else {
                
                std::string hoho{};
                hoho += options->outputFile.text;
                if(options->outputFile.getFormat().empty()) {
                    hoho += ".exe";
                }
                for(auto& arg : options->userArguments){
                    hoho += " ";
                    hoho += arg;
                }
                int exitCode = 0;
                engone::StartProgram((char*)hoho.data(),PROGRAM_WAIT,&exitCode);
                log::out << "Exit code: "<<exitCode<<"\n";
            // }
            break;
        }
        case TARGET_UNIX_x64: {
            // if(options->outputFile.getFormat() != "exe") {
            //     log::out << log::RED << "Cannot execute '" << options->outputFile.getFileName().text <<"'. The file format must be '.exe'.\n";
            // } else {
            std::string hoho{};
            hoho += "./"+options->outputFile.text;
            for(auto& arg : options->userArguments){
                hoho += " ";
                hoho += arg;
            }
            int exitCode = 0;
            engone::StartProgram((char*)hoho.data(),PROGRAM_WAIT,&exitCode);
            log::out << "Exit code: "<<exitCode<<"\n";
            // }
            break;
        }
        default: {
             log::out << log::RED << "Cannot execute "<<options->target << " (not supported)\n";
        }
    }
    return true;
}
bool CompileAll(CompileOptions* options){
    auto bytecode = CompileSource(options);
    if(!bytecode) {
        return false;
    }
    defer {
        Bytecode::Destroy(bytecode);
        bytecode = nullptr;
    };

    bool yes = ExportTarget(options, bytecode);
    if(!yes) {
        return false;
    }

    options->compileStats.printSuccess(options);

    if(options->executeOutput) {
        ExecuteTarget(options, bytecode);
    } else {
        // Sometimes I wonder why the code isn't executing and that's because
        // I have forgotten the --run option. Logging that it's off will save me some time.
        engone::log::out << engone::log::GRAY << "automatic execution is off\n"; // TODO: Remove later
    }

    return true;
}

// struct BTBCHeader{
//     u32 magicNumber = 0x13579BDF;
//     char humanChars[4] = {'B','T','B','C'};
//     // TODO: Version number for interpreter?
//     // TODO: Version number for bytecode format?
//     u32 codeSize = 0;
//     u32 dataSize = 0;
// };
// bool ExportBytecode(CompileOptions* options, Bytecode* bytecode){
//     using namespace engone;
//     Assert(bytecode);
//     auto file = FileOpen(options->outputFile.text, 0, engone::FILE_CLEAR_AND_WRITE);
//     if(!file)
//         return false;
//     BTBCHeader header{};

//     Assert(bytecode->instructionSegment.used*bytecode->instructionSegment.getTypeSize() < ((u64)1<<32));
//     header.codeSize = bytecode->instructionSegment.used*bytecode->instructionSegment.getTypeSize();
//     Assert(bytecode->dataSegment.used*bytecode->dataSegment.getTypeSize() < ((u64)1<<32));
//     header.dataSize = bytecode->dataSegment.used*bytecode->dataSegment.getTypeSize();

//     // TOOD: Check error when writing files
//     int err = FileWrite(file, &header, sizeof(header));
//     if(err!=-1 && bytecode->instructionSegment.data){
//         err = FileWrite(file, bytecode->instructionSegment.data, header.codeSize);
//     }
//     if(err!=-1 && bytecode->dataSegment.data) {
//         err = FileWrite(file, bytecode->dataSegment.data, header.dataSize);
//     }
//     // debugSegment is ignored

//     FileClose(file);
//     return true;
// }
// Bytecode* ImportBytecode(Path filePath){
//     using namespace engone;
//     u64 fileSize = 0;
//     auto file = FileOpen(filePath.text, &fileSize, engone::FILE_ONLY_READ);

//     if(fileSize < sizeof(BTBCHeader))
//         return nullptr; // File is to small for a bytecode file

//     BTBCHeader baseHeader{};
//     BTBCHeader header{};

//     // TOOD: Check error when writing files
//     FileRead(file, &header, sizeof(header));
    
//     if(strncmp((char*)&baseHeader,(char*)&header,8)) // TODO: Don't use hardcoded 8?
//         return nullptr; // Magic number and human characters is incorrect. Meaning not a bytecode file.

//     if(fileSize != sizeof(BTBCHeader) + header.codeSize + header.dataSize)
//         return nullptr; // Corrupt file. Sizes does not match.
    
//     Bytecode* bc = Bytecode::Create();
//     bc->instructionSegment.resize(header.codeSize/bc->instructionSegment.getTypeSize());
//     bc->instructionSegment.used = header.codeSize/bc->instructionSegment.getTypeSize();
//     bc->dataSegment.resize(header.dataSize/bc->dataSegment.getTypeSize());
//     bc->dataSegment.used = header.dataSize/bc->dataSegment.getTypeSize();
//     FileRead(file, bc->instructionSegment.data, header.codeSize);
//     FileRead(file, bc->dataSegment.data, header.dataSize);

//     // debugSegment is ignored

//     FileClose(file);
//     return bc;
// }