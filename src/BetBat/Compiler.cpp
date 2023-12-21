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
Path Path::getAbsolute() const {
    if(isAbsolute()) return *this;

    std::string cwd = engone::GetWorkingDirectory();
    if(cwd.empty()) return {}; // error?
    if(text.length() > 0 && text[0] == '.') {
        if(text.length() > 1 && text[1] == '/') {
            if(cwd.back() == '/')
                return Path(cwd + text.substr(2));
            else
                return Path(cwd + text.substr(1));
        } else {
            if(cwd.back() == '/')
                return Path(cwd + text.substr(1));
            else
                return Path(cwd + "/" + text.substr(1));
        }
    } else {
        if(cwd.back() == '/')
            return Path(cwd + text);
        else
            return Path(cwd + "/" + text);
    }
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
        CASE(UNKNOWN_TARGET,"unknown-target")
    }
    return "unknown";
    #undef CASE
}
TargetPlatform ToTarget(const std::string& str){
    #define CASE(N,X) if (str==X) return N;
    CASE(TARGET_WINDOWS_x64,"win-x64")
    CASE(TARGET_UNIX_x64,"unix-x64")
    CASE(TARGET_BYTECODE,"bytecode")
    return UNKNOWN_TARGET;
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
        CASE(UNKNOWN_LINKER,"unknown-linker")
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
    CASE(UNKNOWN_LINKER,"unknown-linker")
    return UNKNOWN_LINKER;
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
        // engone::Free(it,sizeof(Stream));
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
            // engone::Free(pair.second, sizeof(CertainMacro));
        }
        pair.second->~RootMacro();
        // engone::log::out << "no\n";
        TRACK_FREE(pair.second, RootMacro);
        // engone::Free(pair.second, sizeof(RootMacro));
    }
    _rootMacros.clear();
    // for(auto& stream : streamsToClean){
    //     TokenStream::Destroy(stream);
    // }
    // streamsToClean.resize(0);
}

StreamToProcess* CompileInfo::addStream(const Path& path){
    streamLock.lock();
    auto pair = tokenStreams.find(path.text);
    if(pair!=tokenStreams.end() && pair->second) {
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
        
        bool yes = FileExist(source.path.text);
        if(!yes) {
            int dotindex = source.path.text.find_last_of(".");
            int slashindex = source.path.text.find_last_of("/");
            if(dotindex==-1 || dotindex<slashindex){
                source.path = source.path.text + ".btb";
            }
        }
        
        _VLOG(log::out <<log::BLUE<< "Tokenize: "<<BriefString(source.path.text)<<"\n";)
        TokenStream* tokenStream = nullptr;
        if(source.textBuffer) {
            tokenStream = TokenStream::Tokenize(source.textBuffer);
            // tokenStream->streamName = path.text;
        } else {
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
            Path importName = "";
            int dotindex = item.name.find_last_of(".");
            int slashindex = item.name.find_last_of("/");
            // log::out << "dot "<<dotindex << " slash "<<slashindex<<"\n";
            if(dotindex==-1 || dotindex<slashindex){
                importName = item.name+".btb";
            } else {
                importName = item.name;
            }
            // TODO: import AS
            
            Path fullPath = {};
            // TODO: Test "/src.btb", "ok./hum" and other unusual paths
            
            //-- Search directory of current source file
            if(importName.text.find("./")==0) {
                fullPath = dir.text + importName.text.substr(2);
            } 
            //-- Search cwd or absolute path
            else if(FileExist(importName.text)){
                fullPath = importName;
                if(!fullPath.isAbsolute())
                    fullPath = fullPath.getAbsolute();
            }
            else if(fullPath = dir.text + importName.text, FileExist(fullPath.text)){
                // fullpath =  dir.text + importName.text;
            }
            //-- Search additional import directories.
            // TODO: DO THIS WITH #INCLUDE TOO!
            else {
                // We only read importDirectories which makes this thread safe
                // if we had modified it in anyway then it wouldn't have been.
                for(int i=0;i<(int)info->importDirectories.size();i++){
                    const Path& dir = info->importDirectories[i];
                    Assert(dir.isDir() && dir.isAbsolute());
                    Path path = dir.text + importName.text;
                    bool yes = FileExist(path.text);
                    if(yes) {
                        fullPath = path;
                        break;
                    }
                }
            }
            
            if(fullPath.text.empty()){
                log::out << log::RED << "Could not find import '"<<importName.text<<"' (import from '"<<BriefString(source.path.text,20)<<"'\n";
            } else {
                // if(info->hasStream(fullPath)){   
                //     _VLOG(log::out << log::LIME << "Already imported "<<BriefString(fullPath.text,20)<<"\n";)
                //     tempPaths[i] = {};
                // }else{
                    tempPaths[i] = fullPath;
                    // info->addStream(fullPath);
                    // info->sourceLock.lock();
                    // info->addStream(fullPath);
                    // // log::out << "Add source "<<fullPath.getFileName().text<<"\n";
                    // SourceToProcess source{};
                    // source.path = fullPath;
                    // source.as = item.as;
                    // source.textBuffer = nullptr;
                    // info->sourcesToProcess.add(source);
                    // if(info->sourcesToProcess.size() == 1)
                    //     info->sourceWaitLock.signal();
                    // info->sourceLock.unlock();
                // }
            }
        }
        info->sourceLock.lock();
        for(int i=0;i<(int)tokenStream->importList.size();i++){
            auto& item = tokenStream->importList[i];
            if(tempPaths[i].text.size()==0)
                continue;
            // log::out << "Add source "<<tempPaths[i].getFileName().text<<"\n";
            auto importStream = info->addStream(tempPaths[i]);
            if(!importStream) {
                _VLOG(log::out << log::LIME << "Already imported "<<BriefString(source.path.text,20)<<"\n";)
                continue;
            }
            if(stream->dependencyIndex > importStream->index)
                stream->dependencyIndex = importStream->index;
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

    info->sourceLock.lock();
    // int readDependencyIndex = info->streams.size()-1;
    info->completedDependencyIndex = info->streams.size();
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
        if(info->completedDependencyIndex==0) {
            if(!info->signaled) {
                info->sourceWaitLock.signal();
                info->signaled = true;
            }
            info->sourceLock.unlock();
            break;
        }
        StreamToProcess* stream = nullptr;
        int index = info->completedDependencyIndex-1;
        bool allCompleted = true;
        while(index >= 0) {
            stream = info->streams[index];
            if(stream->available && stream->dependencyIndex >= info->completedDependencyIndex) {
                stream->available = false;
                break;
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
                info->waitingThreads++;
                info->sourceLock.unlock();
                continue;
            }
        }
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
        
        while(info->completedDependencyIndex>0){
            StreamToProcess* s = info->streams[info->completedDependencyIndex-1];
            if(!s->completed) {
                break;
            }
            info->completedDependencyIndex--;
        }

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
        AST::Destroy(compileInfo.ast);
        compileInfo.ast = nullptr;
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
    // "operator [](slice: Slice<char>, index: u32) -> char {"
    //     "return slice.ptr[index];"
    // "}\n"
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
        initialSource.path = options->sourceFile.getAbsolute();
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
    // for(int i=0;i<compileInfo.streams.size();i++){
    //     StreamToProcess* stream = compileInfo.streams[i];
    //     log::out << BriefString(stream->initialStream->streamName,10)<<"\n";
    // }
    {
        int i = compileInfo.streams.size()-1; // process essential first
        StreamToProcess* stream = compileInfo.streams[i];
         // are used in this source file
        // if (!stream->finalStream && (stream->initialStream->enabled & LAYER_PREPROCESSOR)) {
        //     _VLOG(log::out <<log::BLUE<< "Preprocess: "<<BriefString(stream->initialStream->streamName)<<"\n";)
        //     stream->finalStream = Preprocess(&compileInfo, stream->initialStream);
        // }

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
    for(int i=compileInfo.streams.size()-2;i>=0;i--){
        StreamToProcess* stream = compileInfo.streams[i];
         // are used in this source file
        // TokenStream* procStream = stream->initialStream;
        // if (stream->initialStream->enabled & LAYER_PREPROCESSOR) {
        //     _VLOG(log::out <<log::BLUE<< "Preprocess: "<<BriefString(stream->initialStream->streamName)<<"\n";)
        //     procStream = Preprocess(&compileInfo, stream->initialStream);
        //     // if(macroBenchmark){
        //     //     log::out << log::LIME<<"Finished with " << finalStream->length()<<" token(s)\n";
        //     //     #ifndef LOG_MEASURES
        //     //     if(finalStream->length()<50){
        //     //         finalStream->print();
        //     //         log::out<<"\n";
        //     //     }
        //     //     #endif
        //     // }
        //     stream->finalStream = procStream;
        //     // tokenStream->print();
        // }
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

    #ifdef AST_LOG
    _VLOG(log::out << log::BLUE<< "Final "; compileInfo.ast->print();)
    #endif
    TypeCheck(compileInfo.ast, compileInfo.ast->mainBody, &compileInfo);
    
    // if(compileInfo.errors==0){
    // }
    // if (tokens.enabled & LAYER_GENERATOR){

    Bytecode* bytecode=0;
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
    if(compileInfo.ast) {
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

    // compileInfo.cleanup(); // descrutor
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
    log::out << "Compiled " << FormatUnit((u64)lines) << " non-blank lines ("<<FormatUnit((u64)blankLines)<<" blanks, "<<FormatBytes(readBytes)<<")\n";
    if(opts) {
        if(opts->initialSourceBuffer.buffer) {
            log::out << " text buffer: "<<opts->initialSourceBuffer.origin<<"\n";
        } else {
            // log::out << " initial file: "<< opts->initialSourceFile.getFileName().text<<"\n";
            log::out << " initial file: "<< BriefString(opts->sourceFile.getAbsolute().text,30)<<"\n";
        }
        log::out << " target: "<<opts->target<<", output: ";
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
bool ExportTarget(CompileOptions* options, Bytecode* bytecode) {
    using namespace engone;
    MEASURE
    
    bool failure = false;
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
        #ifdef OS_WINDOWS
        if(options->target != TARGET_WINDOWS_x64) {
            log::out << log::RED << "Cannot compile target '"<<ToString(options->target)<<"' on Windows\n";
            return false;
        }
        if(options->target != TARGET_WINDOWS_x64) {
            log::out << log::RED << "Cannot compile target '"<<ToString(options->target)<<"' on Windows\n";
            return false;
        }
        Assert(options->target == TARGET_WINDOWS_x64);
        #else
        if(options->target != TARGET_UNIX_x64) {
            log::out << log::RED << "Cannot compile target '"<<ToString(options->target)<<"' on Unix\n";
            return false;
        }
        Assert(options->target == TARGET_UNIX_x64);
        #endif
    }

    if(options->outputFile.text.empty()) {
        switch (options->target) {
            case TARGET_WINDOWS_x64: {
                options->outputFile = "bin/temp.exe";
                break;
            }
            case TARGET_BYTECODE: {
                // nothing
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

    switch(options->target){
        case TARGET_WINDOWS_x64: {
            Path& outPath = options->outputFile;

            options->compileStats.start_convertx64 = StartMeasure();
            Program_x64* program = ConvertTox64(bytecode);
            options->compileStats.end_convertx64 = StartMeasure();

            if(!program){
                failure = true;
                break;
            }
            auto format = options->outputFile.getFormat();
            bool outputIsObject = format == "o" || format == "obj";

            std::string objPath = "";
            if(outputIsObject){
                options->compileStats.start_objectfile = StartMeasure();
                bool yes = FileCOFF::WriteFile(outPath.text,program);
                if(yes) {
                    options->compileStats.generatedFiles.add(outPath.text);
                }
                options->compileStats.end_objectfile = StartMeasure();
                objPath = outPath.text;
                // log::out << log::LIME<<"Exported "<<options->initialSourceFile.text << " into an object file: "<<outPath.text<<".\n";
            } else {
                objPath = "bin/" + outPath.getFileName(true).text + ".obj";
                options->compileStats.start_objectfile = StartMeasure();
                bool yes = FileCOFF::WriteFile(objPath,program);
                if(yes) {
                    options->compileStats.generatedFiles.add(objPath);
                }
                options->compileStats.end_objectfile = StartMeasure();

                // std::string prevCWD = GetWorkingDirectory();
                bool outputOtherDirectory = outPath.text.find("/") != std::string::npos;
                
                Assert(options->linker == LINKER_MSVC);
                std::string cmd = "link /nologo /INCREMENTAL:NO ";
                if(options->useDebugInformation)
                    cmd += "/DEBUG ";
                // else cmd += "/DEBUG "; // force debug info for now
                cmd += objPath + " ";
                #ifndef MINIMAL_DEBUG
                cmd += "bin/NativeLayer.lib ";
                cmd += "uuid.lib ";
                cmd += "shell32.lib ";
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

                if(!options->silent)
                    log::out << log::LIME<<"Link cmd: "<<cmd<<"\n";
                
                options->compileStats.start_linker = StartMeasure();
                int exitCode = 0;
                engone::StartProgram((char*)cmd.c_str(),PROGRAM_WAIT, &exitCode);
                options->compileStats.end_linker = StartMeasure();
                
                // if(changeCWD) {
                //     engone::SetWorkingDirectory(prevCWD);
                //     log::out << "cwd: "<<prevCWD<<"\n";
                // }

                if(exitCode == 0) { // 0 means success
                    if(outputOtherDirectory){
                        // Since MSVC linker can only output to CWD we have to move the file ourself.
                        FileDelete(outPath.text);
                        FileMove(LINK_TEMP_EXE, outPath.text);
                        if(options->useDebugInformation) {
                            Path p = outPath.getDirectory().text + outPath.getFileName(true).text + ".pdb";
                            FileDelete(p.text);
                            FileMove(LINK_TEMP_PDB,p.text);
                        }
                    }
                    options->compileStats.generatedFiles.add(outPath.text);

                    // TODO: Move the asm dump code to it's own file.
                    QuickArray<char> textBuffer{};
                    for(int i=0;i<(int)bytecode->debugDumps.size();i++) {
                        auto& dump = bytecode->debugDumps[i];
                        if(dump.description.empty()) {
                            log::out << "Dump: "<<log::GOLD<<"unnamed\n";
                        } else {
                            log::out << "Dump: "<<log::GOLD<<dump.description<<"\n";
                        }
                        if(dump.dumpBytecode) {
                            for(int j=dump.startIndex;j<dump.endIndex;j++){
                                log::out << " ";
                                bytecode->printInstruction(j, true);
                                u8 immCount = bytecode->immediatesOfInstruction(j);
                                j += immCount;
                            }
                        }
                        if(dump.dumpAsm) {
                            const char* DUMP_ASM_OBJ = "bin/dump_asm.o";
                            const char* DUMP_ASM_ASM = "bin/dump_asm.asm";

                            bool yes = FileCOFF::WriteFile(DUMP_ASM_OBJ, program, dump.startIndexAsm, dump.endIndexAsm);
                            // if(yes) {
                            //     options->compileStats.generatedFiles.add(DUMP_ASM_OBJ);
                            // }

                            auto file = engone::FileOpen(DUMP_ASM_ASM, 0, FILE_ALWAYS_CREATE);

                            std::string cmd = "dumpbin /nologo /DISASM:BYTES ";
                            cmd += DUMP_ASM_OBJ;
                            
                            int exitCode = 0;
                            engone::StartProgram((char*)cmd.c_str(),PROGRAM_WAIT, &exitCode, {}, file);
                            Assert(("dumpbin failed",exitCode == 0));
                            // if(exitCode == 0) {
                            //     options->compileStats.generatedFiles.add(DUMP_ASM_ASM);
                            // }
                            u32 fileSize = engone::FileGetHead(file);
                            engone::FileSetHead(file, 0);

                            textBuffer.resize(fileSize);
                            engone::FileRead(file, textBuffer._ptr, fileSize);

                            engone::FileClose(file);

                            // TODO: You may want to redirect stdout and reformat the
                            //  text from dumpbin, which we are?
                            ReformatDumpbinAsm(textBuffer, nullptr, true);

                            // memcpy x64 instructions into a buffer, then into a file, then use dumpbin /DISASM
                        }
                    }
                } else {
                    failure = true;
                }
            }
            #ifdef DUMP_ALL_ASM
            // program->printHex("bin/temphex.txt");
            program->printAsm("bin/temp.asm", objPath.data());
            #endif
            Program_x64::Destroy(program);
            break; 
        }
        case TARGET_UNIX_x64: {
            Path& outPath = options->outputFile;

            options->compileStats.start_convertx64 = StartMeasure();
            Program_x64* program = ConvertTox64(bytecode);
            options->compileStats.end_convertx64 = StartMeasure();

            if(!program){
                failure = true;
                break;
            }
            auto format = options->outputFile.getFormat();
            bool outputIsObject = format == "o" || format == "obj";

            // TODO: FileCOFF::WriteFile uses COFF format. Use ELF format for UNIX systems.
            std::string objPath = "";
            if(outputIsObject){
                options->compileStats.start_objectfile = StartMeasure();
                bool yes = FileELF::WriteFile(outPath.text, program);
                if(yes) {
                    options->compileStats.generatedFiles.add(outPath.text);
                }
                options->compileStats.end_objectfile = StartMeasure();
                objPath = outPath.text;
                // log::out << log::LIME<<"Exported "<<options->initialSourceFile.text << " into an object file: "<<outPath.text<<".\n";
            } else {
                objPath = "bin/" + outPath.getFileName(true).text + ".o";
                options->compileStats.start_objectfile = StartMeasure();
                bool yes = FileELF::WriteFile(objPath, program);
                if(yes) {
                    options->compileStats.generatedFiles.add(objPath);
                }
                options->compileStats.end_objectfile = StartMeasure();

                // std::string prevCWD = GetWorkingDirectory();
                // bool outputOtherDirectory = outPath.text.find("/") != std::string::npos;

                // link command
                std::string cmd = "";
                switch(options->linker) {
                    case LINKER_GCC: cmd += "g++ "; break;
                    case LINKER_CLANG: cmd += "clang++ "; break;
                    default: Assert(options->linker == LINKER_GCC || options->linker == LINKER_CLANG); break;
                }
                
                if(options->useDebugInformation)
                    cmd += "-g ";

                cmd += objPath + " ";
                #ifndef MINIMAL_DEBUG
                cmd += "bin/NativeLayer.lib ";
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

                if(!options->silent)
                    log::out << log::LIME<<"Link cmd: "<<cmd<<"\n";
                
                options->compileStats.start_linker = StartMeasure();
                int exitCode = 0;
                engone::StartProgram((char*)cmd.c_str(),PROGRAM_WAIT, &exitCode);
                options->compileStats.end_linker = StartMeasure();
                
                // if(changeCWD) {
                //     engone::SetWorkingDirectory(prevCWD);
                //     log::out << "cwd: "<<prevCWD<<"\n";
                // }

                if(exitCode != 0) { // 0 means success
                    failure = true;
                } else {
                    // if(outputOtherDirectory){
                    //     FileDelete(outPath.text);
                    //     FileMove(LINK_TEMP_EXE, outPath.text);
                    //     if(options->useDebugInformation) {
                    //         Path p = outPath.getDirectory().text + outPath.getFileName(true).text + ".pdb";
                    //         FileDelete(p.text);
                    //         FileMove(LINK_TEMP_PDB,p.text);
                    //     }
                    // }
                    options->compileStats.generatedFiles.add(outPath.text);

                    // TODO: Move the asm dump code to it's own file.
                    QuickArray<char> textBuffer{};
                    for(int i=0;i<(int)bytecode->debugDumps.size();i++) {
                        auto& dump = bytecode->debugDumps[i];
                        if(dump.description.empty()) {
                            log::out << "Dump: "<<log::GOLD<<"unnamed\n";
                        } else {
                            log::out << "Dump: "<<log::GOLD<<dump.description<<"\n";
                        }
                        if(dump.dumpBytecode) {
                            for(int j=dump.startIndex;j<dump.endIndex;j++){
                                log::out << " ";
                                bytecode->printInstruction(j, true);
                                u8 immCount = bytecode->immediatesOfInstruction(j);
                                j += immCount;
                            }
                        }
                        if(dump.dumpAsm) {
                            const char* DUMP_ASM_OBJ = "bin/dump_asm.o";
                            const char* DUMP_ASM_ASM = "bin/dump_asm.asm";
                            // Assert(("Dump asm not implemented for UNIX (COFF is used instead of ELF)",false));
                            bool yes = FileELF::WriteFile(DUMP_ASM_OBJ, program, dump.startIndexAsm, dump.endIndexAsm);
                            // if(yes) {
                            //     options->compileStats.generatedFiles.add(DUMP_ASM_OBJ);
                            // }

                            // TODO: OPTIMIZE by not opening and closing file for each debug dump
                            auto file = engone::FileOpen(DUMP_ASM_ASM, nullptr, FILE_ALWAYS_CREATE);

                            std::string cmd = "objdump -M intel -d ";
                            cmd += DUMP_ASM_OBJ;
                            
                            int exitCode = 0;
                            engone::StartProgram((char*)cmd.c_str(),PROGRAM_WAIT, &exitCode, {}, file);
                            // if(exitCode == 0) {
                            //     options->compileStats.generatedFiles.add(DUMP_ASM_ASM);
                            // }
                            u32 fileSize = engone::FileGetHead(file);
                            engone::FileSetHead(file, 0);

                            textBuffer.resize(fileSize);
                            engone::FileRead(file, textBuffer._ptr, fileSize);

                            engone::FileClose(file);

                            // TODO: You may want to redirect stdout and reformat the
                            //  text from dumpbin, which we are?
                            ReformatDumpbinAsm(textBuffer, nullptr, true);

                            // memcpy x64 instructions into a buffer, then into a file, then use dumpbin /DISASM
                        }
                    }
                }
            }
            #ifdef DUMP_ALL_ASM
            // program->printHex("bin/temphex.txt");
            program->printAsm("bin/temp.asm", objPath.data());
            #endif
            Program_x64::Destroy(program);

            break;
        }
        // Bytecode exporting has been disabled because you rarely use it.
        // You usually want to execute it right away or at compile time.
        // If you want to save time by compiling the bytecode in advance then
        // you probably want to save even more time by compiling into machine instructions
        // instead of bytecode which runs in an interpreter.
        // case TARGET_BYTECODE: {
        //     return ExportBytecode(options, bytecode);
        //     break;
        // }
        default: {
            log::out << log::RED << "Cannot export "<<options->target << " (not supported).\n";
        }
    }
    return !failure;
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
//     auto file = FileOpen(options->outputFile.text, 0, engone::FILE_ALWAYS_CREATE);
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