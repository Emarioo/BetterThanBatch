#include "BetBat/Compiler.h"

#ifdef OS_WINDOWS
#include <intrin.h>
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
#elif defined(OS_LINUX)
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
#elif defined(OS_LINUX)
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
    if(index == std::string::npos || index + 1 == text.length()) {
        return "";
    }
    return text.substr(index+1);
}
Path Path::getFileName(bool withoutFormat) const {
    std::string name = TrimDir(text);
    if(!withoutFormat)
        return name;
    int index = name.find(".");
    if(index == std::string::npos || index + 1 == text.length()) {
        return name;
    }
    return name.substr(0,index);
}
Path Path::getAbsolute() const {
    if(isAbsolute()) return *this;

    std::string cwd = engone::GetWorkingDirectory();
    if(cwd.empty()) return {}; // error?
    if(cwd.back() == '/')
        return Path(cwd + text);
    else
        return Path(cwd + "/" + text);
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

const char* ToString(TargetPlatform target){
    #define CASE(X,N) case X: return N;
    switch(target){
        CASE(WINDOWS_x64,"win-x64")
        CASE(LINUX_x64,"linux-x64")
        CASE(BYTECODE,"bytecode")
    }
    return "unknown";
    #undef CASE
}
engone::Logger& operator<<(engone::Logger& logger,TargetPlatform target){
    return logger << ToString(target);
}
TargetPlatform ToTarget(const std::string& str){
    #define CASE(N,X) if (str==X) return N;
    CASE(WINDOWS_x64,"win-x64")
    CASE(LINUX_x64,"linux-x64")
    CASE(BYTECODE,"bytecode")
    return UNKNOWN_TARGET;
    #undef CASE
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
    compileOptions->compileStats.lines += tokenStream->lines;
    compileOptions->compileStats.blankLines += tokenStream->blankLines;
    compileOptions->compileStats.commentCount += tokenStream->commentCount;
    compileOptions->compileStats.readBytes += tokenStream->readBytes;
    otherLock.unlock();
    #endif
}
void CompileInfo::addLinkDirective(const std::string& name){
    otherLock.lock();
    for(int i=0;i<linkDirectives.size();i++){
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
    // Stream* stream = tokenStreams[tokenStream->streamName] = (Stream*)engone::Allocate(sizeof(Stream));
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
    _MLOG(MLOG_MATCH(engone::log::out << engone::log::CYAN << "match root "<<token<<"\n";))
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
        // rootMacro = (RootMacro*)engone::Allocate(sizeof(RootMacro));
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
            // CertainMacro* macro = (CertainMacro*) engone::Allocate(sizeof(CertainMacro));
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
            // macro = (CertainMacro*) engone::Allocate(sizeof(CertainMacro));
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
            _MLOG(MLOG_MATCH(engone::log::out<<engone::log::MAGENTA << "match argcount "<<count<<" with "<< rootMacro->variadicMacro.parameters.size()<<" (inf)\n";))
        }
    }else{
        macro = pair->second;
        _MLOG(MLOG_MATCH(engone::log::out <<engone::log::MAGENTA<< "match argcount "<<count<<" with "<< macro->parameters.size()<<"\n";))
    }
    macroLock.unlock();
    return macro;
}
// Thread procedure
u32 ProcessSource(void* ptr) {
    using namespace engone;
    // MEASURE_THREAD()
    // MEASURE_SCOPE

    ThreadCompileInfo* thread = (ThreadCompileInfo*)ptr;
    // get file from a list to tokenize, wait with semaphore if list is empty.
    CompileInfo* info = thread->info;
    DynamicArray<Path> tempPaths;
    
    // TODO: Optimize the code here. There is a lot slow locks. Is it possible to use atomic instructions instead?

    info->sourceLock.lock();
    info->availableThreads++;
    info->waitingThreads++;
    info->sourceLock.unlock();
    WHILE_TRUE {
        MEASUREN("Process Source")
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
        
        _VLOG(log::out <<log::BLUE<< "Tokenize: "<<BriefPath(source.path.text)<<"\n";)
        TokenStream* tokenStream = nullptr;
        if(source.textBuffer) {
            tokenStream = TokenStream::Tokenize(source.textBuffer);
            // tokenStream->streamName = path.text;
        } else {
            Assert(source.path.isAbsolute()); // A bug at the call site if not absolute
            tokenStream = TokenStream::Tokenize(source.path.text);
        }
        if(!tokenStream){
            log::out << log::RED << "Failed tokenization: " << BriefPath(source.path.text) <<"\n";
            info->compileOptions->compileStats.errors++;
            return false;
        }
        info->addStats(tokenStream->lines, tokenStream->blankLines, tokenStream->commentCount, tokenStream->readBytes);
        // tokenStream->printData();
        
        if (tokenStream->enabled & LAYER_PREPROCESSOR) {
            
            _VLOG(log::out <<log::BLUE<< "Preprocess (imports): "<<BriefPath(source.path.text)<<"\n";)
            PreprocessImports(info, tokenStream);
        }

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
        for(int i=0;i<tokenStream->importList.size();i++){
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
                log::out << log::RED << "Could not find import '"<<importName.text<<"' (import from '"<<BriefPath(source.path.text,20)<<"'\n";
            } else {
                // if(info->hasStream(fullPath)){   
                //     _VLOG(log::out << log::LIME << "Already imported "<<BriefPath(fullPath.text,20)<<"\n";)
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
        for(int i=0;i<tokenStream->importList.size();i++){
            auto& item = tokenStream->importList[i];
            if(tempPaths[i].text.size()==0)
                continue;
            // log::out << "Add source "<<tempPaths[i].getFileName().text<<"\n";
            auto importStream = info->addStream(tempPaths[i]);
            if(!importStream) {
                _VLOG(log::out << log::LIME << "Already imported "<<BriefPath(source.path.text,20)<<"\n";)
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
    // for(int i=0;i<info->streams.size();i++){
    //     StreamToProcess* stream = info->streams[i];
    //     log::out << BriefPath(stream->initialStream->streamName,10)<<"\n";
    // }
    // if(info->waitingThreads == info->compileOptions->threadCount)
    //     info->sourceWaitLock.signal(); // we must signal to prevent all threads from waiting
    info->sourceLock.unlock();
    // Another solution would be to have a list of dependencies for each stream but you would need
    // to do a lot of linear searches so you probably want to avoid that.
    // info->dependencyIndex = info->streams.size()-1;
    // return 0;
    WHILE_TRUE {
        MEASUREN("Process macro")
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
                Assert(info->completedDependencyIndex == 0);
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
        if (stream->initialStream->enabled & LAYER_PREPROCESSOR) {
            // log::out << "Pre "<<BriefPath(stream->initialStream->streamName)<<"\n";
            _VLOG(log::out <<log::BLUE<< "Preprocess: "<<BriefPath(stream->initialStream->streamName)<<"\n";)
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
    
    #ifdef SINGLE_THREADED
    options->threadCount = 1;
    #endif

    // NOTE: Parser and generator uses tokens. Do not free tokens before compilation is complete.
    options->compileStats.start_bytecode = engone::StartMeasure();
    
    CompileInfo compileInfo{};
    // compileInfo.nativeRegistry = NativeRegistry::GetGlobal();
    // compileInfo.nativeRegistry = NativeRegistry::Create();
    // compileInfo.nativeRegistry->initNativeContent();
    // compileInfo.targetPlatform = options->target;
    compileInfo.compileOptions = options;
    
    compileInfo.ast = AST::Create();
    defer { AST::Destroy(compileInfo.ast); compileInfo.ast = nullptr; };
    // compileInfo.compilerDir = TrimLastFile(compilerPath);
    options->resourceDirectory = options->resourceDirectory.getAbsolute();
    compileInfo.importDirectories.add(options->resourceDirectory.text + "modules/");
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
    // "struct Range {" 
    "struct @hide Range {" 
        "beg: i32;"
        "end: i32;"
    "}\n"
    #ifdef OS_WINDOWS
    "#define OS_WINDOWS\n"
    #elif defined(OS_LINUX)
    "#define OS_LINUX\n"
    #endif
    ;
    essentialStructs += (options->target == BYTECODE ? "#define LINK_BYTECODE\n" : "");
    TextBuffer essentialBuffer{};
    essentialBuffer.origin = "<base>";
    essentialBuffer.size = essentialStructs.size();
    essentialBuffer.buffer = essentialStructs.data();
    // ParseFile(compileInfo, "","",&essentialBuffer);
    #endif
    // if(options->initialSourceBuffer.buffer){
    //     ParseFile(compileInfo, "","",&options->initialSourceBuffer);
    // } else {
    //     ParseFile(compileInfo, options->initialSourceFile.getAbsolute());
    // }

    SourceToProcess essentialSource{};
    essentialSource.textBuffer = &essentialBuffer;
    essentialSource.stream = compileInfo.addStream(essentialSource.textBuffer->origin);
    compileInfo.sourcesToProcess.add(essentialSource);

    SourceToProcess initialSource{};
    if(options->initialSourceBuffer.buffer){
        initialSource.textBuffer = &options->initialSourceBuffer;
    } else {
        initialSource.path = options->initialSourceFile.getAbsolute();
    }
    initialSource.stream = compileInfo.addStream(initialSource.path);
    compileInfo.sourcesToProcess.add(initialSource);


    Assert(!options->singleThreaded); // TODO: Implement not multithreading

    compileInfo.sourceWaitLock.init(1, 1);

    TINY_ARRAY(ThreadCompileInfo, threadInfos, 4);
    threadInfos.resize(options->threadCount);
    memset(threadInfos._ptr, 0, sizeof(ThreadCompileInfo)*threadInfos.max);
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
    //     log::out << BriefPath(stream->initialStream->streamName,10)<<"\n";
    // }
    {
        int i = compileInfo.streams.size()-1; // process essential first
        StreamToProcess* stream = compileInfo.streams[i];
         // are used in this source file
        // if (!stream->finalStream && (stream->initialStream->enabled & LAYER_PREPROCESSOR)) {
        //     _VLOG(log::out <<log::BLUE<< "Preprocess: "<<BriefPath(stream->initialStream->streamName)<<"\n";)
        //     stream->finalStream = Preprocess(&compileInfo, stream->initialStream);
        // }

        TokenStream* procStream = stream->finalStream ? stream->finalStream : stream->initialStream;
        _VLOG(log::out <<log::BLUE<< "Parse: "<<BriefPath(stream->initialStream->streamName)<<"\n";)
        ASTScope* body = ParseTokenStream(procStream,compileInfo.ast, &compileInfo, stream->as);
        if(body){
            if(stream->as.empty()){
                compileInfo.ast->appendToMainBody(body);
            } else {
                compileInfo.ast->mainBody->add(compileInfo.ast, body);
            }
        }
    }
    for(int i=compileInfo.streams.size()-2;i>=0;i--){
        StreamToProcess* stream = compileInfo.streams[i];
         // are used in this source file
        // TokenStream* procStream = stream->initialStream;
        // if (stream->initialStream->enabled & LAYER_PREPROCESSOR) {
        //     _VLOG(log::out <<log::BLUE<< "Preprocess: "<<BriefPath(stream->initialStream->streamName)<<"\n";)
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
        _VLOG(log::out <<log::BLUE<< "Parse: "<<BriefPath(stream->initialStream->streamName)<<"\n";)
        ASTScope* body = ParseTokenStream(procStream,compileInfo.ast, &compileInfo, stream->as);
        if(body){
            if(stream->as.empty()){
                compileInfo.ast->appendToMainBody(body);
            } else {
                compileInfo.ast->mainBody->add(compileInfo.ast, body);
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
    }

    AST::Destroy(compileInfo.ast);
    compileInfo.ast = nullptr;

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
    if(bytecode) {
        // if(!bytecode->nativeRegistry)
        //     bytecode->nativeRegistry = compileInfo.nativeRegistry;
        // compileInfo.nativeRegistry = nullptr;
        bytecode->linkDirectives.stealFrom(compileInfo.linkDirectives);
    }

    compileInfo.cleanup();
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
            log::out << " initial file: "<< BriefPath(opts->initialSourceFile.getAbsolute().text,30)<<"\n";
        }
        log::out << " target: "<<opts->target<<", output: "<<opts->outputFile.text << "\n";
    }
    bool withoutLinker = end_objectfile == 0;
    if(withoutLinker) {
        double time_bytecode = DiffMeasure(end_bytecode - start_bytecode);
        log::out << " source->bytecode (total): " <<log::LIME<< FormatTime(time_bytecode)<<log::SILVER << " (" << FormatUnit(lines / time_bytecode) << " lines/s)\n";
    } else {
        double time_compiler = DiffMeasure(end_objectfile - start_bytecode);
        double time_bytecode = DiffMeasure(end_bytecode - start_bytecode);
        double time_converter = DiffMeasure(end_convertx64 - start_convertx64);
        double time_objwriter = DiffMeasure(end_objectfile - start_objectfile);
        double time_linker = DiffMeasure(end_linker - start_linker);
        double time_total = DiffMeasure(end_linker - start_bytecode);
        log::out << " compiler: " <<log::LIME<< FormatTime(time_compiler)<<log::SILVER << " (" << FormatUnit(lines / time_compiler) << " lines/s)\n";
        log::out << "  (bc: "<<log::LIME<< FormatTime(time_bytecode)<<log::SILVER <<", x64: " <<log::LIME<< FormatTime(time_converter)<<log::SILVER << ", obj: " <<log::LIME<< FormatTime(time_objwriter)<<log::SILVER << ")\n";
        log::out << " linker: " <<log::LIME<< FormatTime(time_linker)<<log::SILVER << "\n";
        log::out << " total: "<<log::LIME<<FormatTime(time_total)<<log::SILVER<<" (" << FormatUnit(lines / time_total) << " lines/s)\n"; 
        // log::out << " total: "<<log::LIME<<FormatTime(time_total)<<log::SILVER<<" (" <<log::LIME<< FormatUnit(lines / time_total) << " lines/s"<<log::SILVER<<")\n"; 
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
void CompileAndRun(CompileOptions* options) {
    using namespace engone;
    
    Bytecode* bytecode = CompileSource(options);
    if(!bytecode)
        return;
    defer {
        Bytecode::Destroy(bytecode);
        bytecode = nullptr;
    };

    switch (options->target) {
        case WINDOWS_x64: {
            options->outputFile = "bin/temp.exe";
            bool yes = ExportTarget(options, bytecode);
            if(!yes)
                return;
        }
        break;
    }

    options->compileStats.printSuccess(options);

    log::out <<"\n";

    switch (options->target) {
        case BYTECODE: {
            RunBytecode(options, bytecode);
        }
        break; case WINDOWS_x64: {
            std::string hoho{};
            hoho += options->outputFile.text;
            for(auto& arg : options->userArgs){
                hoho += " ";
                hoho += arg;
            }
            int errorCode = 0;
            engone::StartProgram("",(char*)hoho.data(),PROGRAM_WAIT,&errorCode);
            log::out << "Error level: "<<errorCode<<"\n";
        }
    }

    #ifdef LOG_MEASURES
    PrintMeasures();
    #endif
}
void RunBytecode(CompileOptions* options, Bytecode* bytecode){
    Assert(bytecode);
        
    Interpreter interpreter{};
    interpreter.setCmdArgs(options->userArgs);
    interpreter.silent = options->silent;
    interpreter.execute(bytecode);
    interpreter.cleanup();
}
void CompileAndExport(CompileOptions* options){
    using namespace engone;
    
    Bytecode* bytecode = CompileSource(options);
    if(!bytecode) {
        return;
    }
    defer {
        Bytecode::Destroy(bytecode);
        bytecode = nullptr;
    };

    bool yes = ExportTarget(options, bytecode);

    options->compileStats.printSuccess(options);
}
bool ExportTarget(CompileOptions* options, Bytecode* bytecode) {
    using namespace engone;
    Assert(bytecode);
    switch(options->target){
        case WINDOWS_x64: {
            Path& outPath = options->outputFile;

            options->compileStats.start_convertx64 = StartMeasure();
            Program_x64* program = ConvertTox64(bytecode);
            options->compileStats.end_convertx64 = StartMeasure();

            if(program){
                auto format = options->outputFile.getFormat();
                bool outputIsObject = format == "o" || format == "obj";

                #ifdef DUMP_ASM
                // program->printHex("bin/temphex.txt");
                program->printAsm("bin/temp.asm");
                #endif
                if(outputIsObject){
                    options->compileStats.start_objectfile = StartMeasure();
                    WriteObjectFile(outPath.text,program);
                    options->compileStats.end_objectfile = StartMeasure();
                    // log::out << log::LIME<<"Exported "<<options->initialSourceFile.text << " into an object file: "<<outPath.text<<".\n";
                } else {
                    std::string objPath = "bin/" + outPath.getFileName(true).text + ".obj";
                    options->compileStats.start_objectfile = StartMeasure();
                    WriteObjectFile(objPath,program);
                    options->compileStats.end_objectfile = StartMeasure();

                    std::string cmd = "link /nologo ";
                    cmd += objPath + " ";
                    cmd += "bin/NativeLayer.lib ";
                    cmd += "uuid.lib ";
                    cmd += "shell32.lib ";
                    for (int i = 0;i<bytecode->linkDirectives.size();i++) {
                        auto& dir = bytecode->linkDirectives[i];
                        cmd += dir + " ";
                    }
                    cmd += "/DEFAULTLIB:MSVCRT ";
                    // cmd += "/DEFAULTLIB:LIBCMT ";
                    cmd += "/OUT:" + outPath.text+" ";
                    
                    options->compileStats.start_linker = StartMeasure();
                    int exitCode = 0;
                    engone::StartProgram("",(char*)cmd.c_str(),PROGRAM_WAIT, &exitCode);
                    options->compileStats.end_linker = StartMeasure();
                    
                    if(!options->silent)
                        log::out << log::LIME<<"Link cmd: "<<cmd<<"\n";

                    // msvc linker returns a fatal error as exit code
                    // if(exitCode == 0)
                    //     log::out << log::LIME<<"Exported "<<options->initialSourceFile.text << " into an executable: "<<outPath.text<<".\n";
                }
                Program_x64::Destroy(program); 
                return true;
            } else {
                // log::out <<log::RED <<"Failed converting "<<options->initialSourceFile.text << " to x64 program.\n";
                return false;
            }
        }
        //break; case BYTECODE: {
        //     return ExportBytecode(options, bytecode);
        // }
        break; default: {
            log::out << log::RED << "Target "<<options->target << " is not supported\n";
        }
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
//     auto file = FileOpen(options->outputFile.text, 0, engone::FILE_WILL_CREATE);
//     if(!file)
//         return false;
//     BTBCHeader header{};

//     Assert(bytecode->codeSegment.used*bytecode->codeSegment.getTypeSize() < ((u64)1<<32));
//     header.codeSize = bytecode->codeSegment.used*bytecode->codeSegment.getTypeSize();
//     Assert(bytecode->dataSegment.used*bytecode->dataSegment.getTypeSize() < ((u64)1<<32));
//     header.dataSize = bytecode->dataSegment.used*bytecode->dataSegment.getTypeSize();

//     // TOOD: Check error when writing files
//     int err = FileWrite(file, &header, sizeof(header));
//     if(err!=-1 && bytecode->codeSegment.data){
//         err = FileWrite(file, bytecode->codeSegment.data, header.codeSize);
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
//     bc->codeSegment.resize(header.codeSize/bc->codeSegment.getTypeSize());
//     bc->codeSegment.used = header.codeSize/bc->codeSegment.getTypeSize();
//     bc->dataSegment.resize(header.dataSize/bc->dataSegment.getTypeSize());
//     bc->dataSegment.used = header.dataSize/bc->dataSegment.getTypeSize();
//     FileRead(file, bc->codeSegment.data, header.codeSize);
//     FileRead(file, bc->dataSegment.data, header.dataSize);

//     // debugSegment is ignored

//     FileClose(file);
//     return bc;
// }