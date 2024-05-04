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
        default: Assert(false);
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

int CompileOptions::addTestLocation(lexer::SourceLocation loc, lexer::Lexer* lexer){
    auto imp = lexer->getImport_unsafe(loc);
    auto info = lexer->getTokenSource_unsafe(loc);
    
    testLocations.add({});
    testLocations.last().line = info->line;
    testLocations.last().column = info->column;
    testLocations.last().file = imp->path;
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
        if(opts->source_buffer.buffer) {
            log::out << " text buffer: "<<opts->source_buffer.origin<<"\n";
        } else {
            // log::out << " initial file: "<< opts->initialSourceFile.getFileName().text<<"\n";
            log::out << " initial file: "<< BriefString(opts->source_file,30)<<"\n";
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

void Compiler::processImports() {
    using namespace engone;
    // lock_imports.lock();
    // int thread_id = next_thread_id++;
    // total_threads++;
    // waiting_threads++;
    // lock_imports.unlock();

    int thread_id = atomic_add(&next_thread_id, 1);
    atomic_add(&total_threads, 1);
    atomic_add(&waiting_threads, 1);

    #define LOGD(COND, MSG) LOG(COND, log::GRAY << (options->threadCount == 1 ? "" : (StringBuilder{} << "T" << thread_id<<" ")) << log::NO_COLOR << MSG)

    while(true) {
        ZoneNamedNC(zone0,"processImport", tracy::Color::DarkSlateGray,true);
        
        lock_wait_for_imports.wait();
        
        lock_imports.lock();
        signaled=false;
        waiting_threads--;
        
        // LOG(LOG_TASKS, log::GOLD<<"["<<thread_id<<"] Try select task ("<<imports.getCount()<<" imports)\n")
        
        bool finished = true;
        bool found = false;
        CompilerTask picked_task{};
        int task_index=0;
        while(task_index < tasks.size()) {
            CompilerTask& task = tasks[task_index];
            task_index++;
            // if(im->busy) {
            //     finished=false;
            //     LOG(CAT_PROCESSING_DETAILED, log::GRAY<<" busy: "<<im->import_id<<" ("<<TrimCWD(im->path)<<")\n")
            //     continue;
            // }
            // if(im->finished) {
            //     LOG(CAT_PROCESSING_DETAILED, log::GRAY<<" finished: "<<im->import_id<<" ("<<TrimCWD(im->path)<<")\n")
            //     continue;
            // }
            // TODO: Optimize dependency checking
            // TODO: Some checks don't respect nested dependencies.
            bool missing_dependency = false;
            if(task.type == TASK_PREPROCESS_AND_PARSE) {
                // Before preprocessing we must make sure all imports that are linked in any way
                // are lexed before preprocessing.
                DynamicArray<u32> checked_ids{};
                DynamicArray<u32> ids_to_check{};
                ids_to_check.add(task.import_id);
                CompilerImport* im = imports.get(task.import_id-1);
                while(ids_to_check.size()) {
                    // CompilerImport* dep = imports.get(im->dependencies[j].id-1);
                    u32 id = ids_to_check.last();
                    CompilerImport* dep = imports.get(id-1);
                    ids_to_check.pop();
                    checked_ids.add(id);

                    Assert(dep);
                    
                    if(!dep || !(dep->state & TASK_LEX)) {
                        if(ids_to_check.size() == 0)  {
                            // direct dependency
                            LOG(LOG_TASKS, log::GRAY<<" depend: "<<im->import_id<<"->"<<id<<" ("<<TrimCWD(im->path)<<"->"<<(dep?TrimCWD(dep->path):"?")<<")\n")
                        } else {
                            // inherited dependency
                            LOG(LOG_TASKS, log::GRAY<<" depend: "<<im->import_id<<"-->"<<id<<" ("<<TrimCWD(im->path)<<"->"<<(dep?TrimCWD(dep->path):"?")<<")\n")
                        }
                        missing_dependency = true;
                        break;
                    }

                    for(auto& it : dep->dependencies) {
                        bool checked = false;
                        for(auto& it2 : checked_ids) {
                            if(it.id == it2) {
                                    checked = true;
                                    break;
                            }
                        }
                        if(checked) continue;
                        ids_to_check.add(it.id);
                    }
                }
            } else if (task.type == TASK_TYPE_ENUMS) { // when we are about to check functions
                CompilerImport* im = imports.get(task.import_id-1);
                for(int j=0;j<im->dependencies.size();j++) {
                    CompilerImport* dep = imports.get(im->dependencies[j].id-1);
                    // nocheckin, I had a thought about a potential problem here but the bug I thought caused it didn't. So maybe there isn't a problem waiting for lexed and preprocessed? Of course, we will need to add parsed, type checked and generated in the future.
                    if(!dep || !(dep->state & TASK_PREPROCESS_AND_PARSE)) {
                        LOG(LOG_TASKS, log::GRAY<<" depend: "<<im->import_id<<"->"<<im->dependencies[j].id<<" ("<<TrimCWD(im->path)<<"->"<<(dep?TrimCWD(dep->path):"?")<<")\n")
                        missing_dependency = true;
                        break;
                    }
                }
            } else if (task.type == TASK_TYPE_STRUCTS) { // when we are about to check functions
                CompilerImport* im = imports.get(task.import_id-1);
                for(int j=0;j<im->dependencies.size();j++) {
                    CompilerImport* dep = imports.get(im->dependencies[j].id-1);
                    // nocheckin, I had a thought about a potential problem here but the bug I thought caused it didn't. So maybe there isn't a problem waiting for lexed and preprocessed? Of course, we will need to add parsed, type checked and generated in the future.
                    if(!dep || !(dep->state & TASK_TYPE_ENUMS)) {
                        LOG(LOG_TASKS, log::GRAY<<" depend: "<<im->import_id<<"->"<<im->dependencies[j].id<<" ("<<TrimCWD(im->path)<<"->"<<(dep?TrimCWD(dep->path):"?")<<")\n")
                        missing_dependency = true;
                        break;
                    }
                }
            } else if (task.type == TASK_TYPE_BODY) {// when we should check bodies, functions must be available
                CompilerImport* im = imports.get(task.import_id-1);
                for(int j=0;j<im->dependencies.size();j++) {
                    CompilerImport* dep = imports.get(im->dependencies[j].id-1);
                    if(!dep || !(dep->state & TASK_TYPE_FUNCTIONS)) {
                        LOG(LOG_TASKS, log::GRAY<<" depend: "<<im->import_id<<"->"<<im->dependencies[j].id<<" ("<<TrimCWD(im->path)<<"->"<<(dep?TrimCWD(dep->path):"?")<<")\n")
                        missing_dependency = true;
                        break;
                    }
                }
            } else if (task.type == TASK_GEN_BYTECODE) {
                // global data must have been generated before we can generate data
                // (in the current implementation)

                // TODO: Optimize, store an integer of how many non-type checked tasks
                //   we have left instead of iterating all tasks every time.
                for(int i=0;i<tasks.size();i++) {
                    CompilerTask& t = tasks[i];
                    // TODO: We may only have tasks to generate bytecodes but a thread
                    //   may have taken out a task and be working on it right now.
                    //   Redesign this
                    if(t.type < TASK_GEN_BYTECODE) {
                        LOG(LOG_TASKS, log::GRAY<<" depend on task "<<i<< " (import_id: "<<t.import_id<<")\n")
                        missing_dependency = true;
                        // we miss a dependency in that all other imports have
                        // not been type checked yet
                        break;
                    }
                }
            } else if (task.type == TASK_GEN_MACHINE_CODE) {
                // TinyBytecodes must be generated so that we can apply function relocations.
                // relocation.funcImpl->tinycode_id may be zero otherwise.
                // You could designate tinycodes to functions in type checker.
                // Then we know that all funcimpls have tinycode_ids.
                
                CompilerImport* im = imports.get(task.import_id-1);
                for(int j=0;j<im->dependencies.size();j++) {
                    CompilerImport* dep = imports.get(im->dependencies[j].id-1);
                    if(!dep || !(dep->state & TASK_GEN_BYTECODE)) {
                        LOG(LOG_TASKS, log::GRAY<<" depend: "<<im->import_id<<"->"<<im->dependencies[j].id<<" ("<<TrimCWD(im->path)<<"->"<<(dep?TrimCWD(dep->path):"?")<<")\n")
                        missing_dependency = true;
                        break;
                    }
                }
            }
            if(missing_dependency) {
                finished=false;
                continue;
            }
            picked_task = task;
            tasks.removeAt(task_index - 1);
            finished=false;
            found = true;
            break;
        }
        if(found) {
            // imp->busy = true;
            if(!signaled) {
                lock_wait_for_imports.signal();
                signaled = true;
            }
        } else if(!finished && !signaled && waiting_threads+1 == total_threads) {
            if(options->compileStats.errors > 0) {
                // error should have been reported
                lock_imports.unlock();
                break;
            } else {
                log::out << log::RED << "ERROR: Some tasks have dependencies that cannot be computed. Bug in compiler?\n";
                compiler_got_stuck = true;
                lock_imports.unlock();
                break;
            }
        }
        if(finished) {
            if(!signaled) {
                lock_wait_for_imports.signal();
                signaled = true;
            }
            lock_imports.unlock();
            break;
        }
        if(!found) {
            waiting_threads++;
        }
        
        lock_imports.unlock();
        
        if(found) {
            if(picked_task.type == TASK_LEX) {
                CompilerImport* imp = imports.get(picked_task.import_id-1);
                LOGD(LOG_TASKS, log::GREEN<<"Lexing and preprocessing: "<<imp->import_id <<" ("<<TrimCWD(imp->path)<<")\n")
                // imp->import_id may zero but may also be pre-created
                u32 old_id = imp->import_id;
                imp->import_id = lexer.tokenize(imp->path, old_id);
                if(!imp->import_id) {
                    options->compileStats.errors++; // TODO: Temporary. We should call a function that reports an error since we may need to do more things than just incrementing a counter.
                    log::out << log::RED << "ERROR: Could not find "<<imp->path << "\n"; // TODO: Improve error message
                } else {
                    u32 new_id = preprocessor.process(imp->import_id,false);
                    Assert(new_id == 0); // first preprocessor phase should not create an import
                    
                    // lexer.print(old_id);
                    
                    auto intern_imp = lexer.getImport_unsafe(imp->import_id);
                    
                    u32 tokens = 0;
                    if(intern_imp->chunk_indices.size() > 0) {
                        tokens = (intern_imp->chunk_indices.size()-1) * TOKEN_ORIGIN_TOKEN_MAX + lexer.getChunk_unsafe(intern_imp->chunk_indices.last())->tokens.size();
                    }
                    
                    LOG(LOG_TASKS, log::GRAY
                        << " lines: "<<intern_imp->lines
                        << ", tokens: "<<tokens
                        << ", file_size: "<<FormatBytes(intern_imp->fileSize)
                        << "\n")
                    
                    imp->state = (TaskType)(imp->state | picked_task.type);
                    picked_task.type = TASK_PREPROCESS_AND_PARSE;
                    picked_task.import_id = imp->import_id;
                    tasks.add(picked_task); // nocheckin, lock tasks
                }
            } else if(picked_task.type == TASK_PREPROCESS_AND_PARSE) {
                CompilerImport* imp = imports.get(picked_task.import_id-1);
                LOGD(LOG_TASKS, log::GREEN<<"Preprocess and parse: "<<imp->import_id <<" ("<<TrimCWD(imp->path)<<")\n")
                
                imp->preproc_import_id = preprocessor.process(imp->import_id, true);
                map_id(imp->preproc_import_id, imp->import_id);
                
                // TODO: Free memory for old import since we don't need it anymore.
                //   import_id is used in dependencies but I don't think we ever
                //   query the import from the lexer so from the compilers
                //   perspective the import_id still exists but the data from
                //   it doesn't which is fine since the dependencies don't care
                //   about the data of the import, just whether it was loaded
                //   at some point in time. Double check this though.
                // lexer.destroyImport_unsafe(imp->import_id);

                auto intern_imp = lexer.getImport_unsafe(imp->preproc_import_id);
                
                u32 tokens = 0;
                if(intern_imp->chunk_indices.size() > 0) {
                    tokens = (intern_imp->chunk_indices.size()-1) * TOKEN_ORIGIN_TOKEN_MAX + lexer.getChunk_unsafe(intern_imp->chunk_indices.last())->tokens.size();
                }
                
                LOG(LOG_TASKS, log::GRAY
                    << " tokens: "<<tokens
                    << "\n")
                
                if(!options->only_preprocess) {
                    auto my_scope = parser::ParseImport(imp->preproc_import_id, this);
                    
                    // ast->print();
                    imp->scopeId = my_scope->scopeId;

                    imp->state = (TaskType)(imp->state | picked_task.type);
                    picked_task.type = TASK_TYPE_ENUMS;
                    picked_task.import_id = imp->import_id;
                    tasks.add(picked_task); // nocheckin, lock tasks
                }
            } else if(picked_task.type == TASK_TYPE_ENUMS) {
                CompilerImport* imp = imports.get(picked_task.import_id-1);
                auto my_scope = ast->getScope(imp->scopeId);
                LOGD(LOG_TASKS, log::GREEN<<"Type enums: "<<imp->import_id <<" ("<<TrimCWD(imp->path)<<")\n")

                // We share scopes here when we know that all imports have been parsed
                // and have scopes we can throw around.

                // log::out << BriefString(imp->path) << " deps:\n";
                for(int i=0;i<imp->dependencies.size();i++) {
                    auto dep = imports.get(imp->dependencies[i].id-1);
                    auto dep_scope = ast->getScope(dep->scopeId);
                    my_scope->sharedScopes.add(dep_scope);

                    // log::out << "  " << BriefString(dep->path) << "\n";
                }

                if(imp->path == PRELOAD_NAME) {
                    auto global_scope = ast->getScope(ast->globalScopeId);
                    global_scope->sharedScopes.add(my_scope);
                }
                // TODO: Below we assume all files named Lang.btb specify type information which is bad.
                //   Do we check modules/Lang.btb instead?
                auto name = TrimDir(imp->path);
                if(name == TYPEINFO_NAME && typeinfo_import_id == 0) {
                    typeinfo_import_id = imp->import_id;
                    // TODO: Should type information be at global scope.
                    //   Should it be loaded by default? I don't think so.
                    //   Maybe you want to import it once in the main file so
                    //  that you don't have to import it everywhere else though.
                }

                int prev_errors = options->compileStats.errors;
                TypeCheckEnums(ast, my_scope->astScope, this);
                bool had_errors = options->compileStats.errors > prev_errors;

                // TODO: Print amount of checked enums.
                //  LOG(LOG_TASKS, log::GRAY
                //     << " tokens: "<<tokens
                //     << "\n")

                if(!had_errors) {
                    imp->state = (TaskType)(imp->state | picked_task.type);
                    picked_task.type = TASK_TYPE_STRUCTS;
                    picked_task.import_id = imp->import_id;
                    tasks.add(picked_task); // nocheckin, lock tasks
                }
            } else if(picked_task.type == TASK_TYPE_STRUCTS) {
                CompilerImport* imp = imports.get(picked_task.import_id-1); // nocheckin, lock imports, this is not the only spot
                auto my_scope = ast->getScope(imp->scopeId);
                LOGD(LOG_TASKS, log::GREEN<<"Type structs: "<<imp->import_id <<" ("<<TrimCWD(imp->path)<<")\n")
                
                bool ignore_errors = true;
                bool changed = false;
                bool yes = SIGNAL_SUCCESS == TypeCheckStructs(ast, my_scope->astScope, this, ignore_errors, &changed);

                // TODO: Print amount of checked structs.
                //  LOG(LOG_TASKS, log::GRAY
                //     << " tokens: "<<tokens
                //     << "\n")
                // TODO: Print whether we are done with structs or if some remain unchecked.

                if(yes) {
                    imp->state = (TaskType)(imp->state | picked_task.type);
                    picked_task.type = TASK_TYPE_FUNCTIONS;
                    picked_task.import_id = imp->import_id;
                    tasks.add(picked_task); // nocheckin, lock tasks
                } else {
                    if(!changed) {
                        if(picked_task.no_change) {
                            ignore_errors = false;
                            TypeCheckStructs(ast, my_scope->astScope, this, ignore_errors, &changed);
                        }
                    }
                    picked_task.no_change = !changed;
                    if(ignore_errors)
                        tasks.add(picked_task); // nocheckin, lock tasks
                }
            } else if(picked_task.type == TASK_TYPE_FUNCTIONS) {
                CompilerImport* imp = imports.get(picked_task.import_id-1);
                auto my_scope = ast->getScope(imp->scopeId);
                LOGD(LOG_TASKS, log::GREEN<<"Type function signatures: "<<imp->import_id <<" ("<<TrimCWD(imp->path)<<")\n")
                
                int prev_errors = options->compileStats.errors;
                bool is_initial_import = initial_import_id == imp->import_id;
                TypeCheckFunctions(ast, my_scope->astScope, this, is_initial_import);
                bool had_errors = options->compileStats.errors > prev_errors;

                // TODO: Print amount of checked stuff.
                //  LOG(LOG_TASKS, log::GRAY
                //     << " tokens: "<<tokens
                //     << "\n")

                if(!had_errors) {
                    imp->state = (TaskType)(imp->state | picked_task.type);
                    if(is_initial_import) {
                        // The 
                        auto iden = ast->findIdentifier(imp->scopeId,0,"main");
                        if(iden) {
                            Assert(iden->funcOverloads.overloads.size());
                            auto overload = iden->funcOverloads.overloads[0];
                            addTask_type_body(overload.astFunc, overload.funcImpl);
                        } else {
                            addTask_type_body(imp->import_id);
                        }
                    }

                    // We add GEN_BYTECODE now but it won't be processed
                    // untill all TASK_TYPE_BODY tasks are handled first.
                    // We add GEN_BYTECODE here because it will run once per import.
                    // Otherwise we would need some code somewhere to check if all 
                    // TASK_TYPE_BODY have been dealt with and then add one GEN_BYTECODE
                    // per import.
                    imp->state = (TaskType)(imp->state | picked_task.type);
                    picked_task.type = TASK_GEN_BYTECODE;
                    picked_task.import_id = imp->import_id;
                    tasks.add(picked_task); // nocheckin, lock tasks
                }
            } else if(picked_task.type == TASK_TYPE_BODY) {
                auto imp = imports.get(picked_task.import_id-1);
                auto my_scope = ast->getScope(imp->scopeId);
                if(picked_task.astFunc) {
                    LOGD(LOG_TASKS, log::GREEN<<"Type function body: "<<picked_task.astFunc->name <<" (from import: "<<imp->import_id<<", "<<TrimCWD(imp->path)<<")\n")
                } else {
                    LOGD(LOG_TASKS, log::GREEN<<"Type global body: "<< imp->import_id <<" ("<<TrimCWD(imp->path)<<")\n")
                }
                
                int prev_errors = options->compileStats.errors;
                
                ASTScope* import_scope = my_scope->astScope;
                if(imp->type_checked_import_scope)
                    import_scope = nullptr;
                TypeCheckBody(this, picked_task.astFunc,picked_task.funcImpl, import_scope);
                imp->type_checked_import_scope = true;
                
                // TODO: Print some interesting info?
                //  LOG(LOG_TASKS, log::GRAY
                //     << " tokens: "<<tokens
                //     << "\n")

                bool had_errors = options->compileStats.errors > prev_errors;
                // TODO: Do we need to do something with errors?

                // NOTE: TypeCheckBody may call addTask_type_body.
                //   TASK_TYPE_BODY is per function, not per import so
                //   we can't add GEN_BYTECODE task here.
            } else if(picked_task.type == TASK_GEN_BYTECODE) {
                auto imp = imports.get(picked_task.import_id-1);
                auto my_scope = ast->getScope(imp->scopeId);
                LOGD(LOG_TASKS, log::GREEN<<"Gen bytecode: "<<imp->import_id <<" ("<<TrimCWD(imp->path)<<")\n")

                if(!have_generated_global_data) { // cheap quick check, will the compiler optimize it away?
                    lock_miscellaneous.lock();
                    if(!have_generated_global_data) { // thread safe check
                        GenContext c{};
                        c.ast = ast;
                        c.bytecode = bytecode;
                        c.reporter = &reporter;
                        c.compiler = this;
                        c.generateData(); // make sure this function doesn't call lock_miscellaneous
                        have_generated_global_data = true;
                    }
                    lock_miscellaneous.unlock();
                }

                int prev_errors = options->compileStats.errors;

                if(initial_import_id == imp->import_id) {
                    auto yes = GenerateScope(my_scope->astScope, this, imp, &imp->tinycodes, true);
                } else {
                    auto yes = GenerateScope(my_scope->astScope, this, imp, &imp->tinycodes, false);
                }
                // TODO: Print some interesting info?
                //  LOG(LOG_TASKS, log::GRAY
                //     << " tokens: "<<tokens
                //     << "\n")


                bool new_errors = false;
                if(prev_errors < options->compileStats.errors) {
                    new_errors = true;
                }
                
                if(!new_errors) {
                    // @DEBUG
                    // for(auto t : imp->tinycodes) {
                    //     // log::out << log::GOLD << t->name << "\n";
                    //     t->print(0,-1,bytecode,nullptr,true);
                    // }
                    
                    imp->state = (TaskType)(imp->state | picked_task.type);
                    picked_task.type = TASK_GEN_MACHINE_CODE;
                    picked_task.import_id = imp->import_id;
                    tasks.add(picked_task); // nocheckin, lock tasks
                }
            } else if(picked_task.type == TASK_GEN_MACHINE_CODE) {
                auto imp = imports.get(picked_task.import_id-1);
                // auto my_scope = ast->getScope(imp->scopeId);
                LOGD(LOG_TASKS, log::GREEN<<"Gen machine code: "<<imp->import_id <<" ("<<TrimCWD(imp->path)<<")\n")

                if(options->compileStats.errors == 0) {
                    // can't generate if bytecode is messed up.

                    switch(options->target) {
                        case TARGET_BYTECODE: {
                            // do nothing
                        } break;
                        case TARGET_WINDOWS_x64:
                        case TARGET_UNIX_x64: {
                            for(auto t : imp->tinycodes)
                                GenerateX64(this, t);
                        } break;
                    }
                }
                   // TODO: Print some interesting info?
                //  LOG(LOG_TASKS, log::GRAY
                //     << " tokens: "<<tokens
                //     << "\n")

                    
                imp->state = (TaskType)(imp->state | picked_task.type);
            } else {
                Assert(false);
            }
            
            lock_imports.lock();
            // imp->busy = false;
            
            if(!signaled) {
                lock_wait_for_imports.signal();
                signaled = true;
            }
            waiting_threads++;
            lock_imports.unlock();
        }
    }
    #undef LOGD
    
    lock_imports.lock();
    total_threads--;
    lock_imports.unlock();
}
u32 Thread_process(void* self) {
    ((Compiler*)self)->processImports();
    return 0;
}
void Compiler::run(CompileOptions* options) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Gray19);
    auto tp = engone::StartMeasure();

    Assert(!this->options);
    this->options = options;

    // ################################
    // What are we trying to generate?
    // #################################
    
    if(options->useDebugInformation) {
        log::out << log::YELLOW << "Debug information was recently added and is not complete. Only DWARF with COFF (on Windows) and GCC is supported. ELF for Unix systems is on the way.\n";
        // return EXIT_CODE_NOTHING; // debug is used with linker errors (.text+0x32 -> file:line)
    }

    std::string obj_file = "bin/main.o";
    std::string exe_file = "bin/main.exe";
    std::string bc_file = "bin/main.bc";

    bool generate_obj_file = false;
    bool generate_exe_file = false;
    bool generate_bc_file = false;

    if(options->output_file.size() == 0) {
        switch(options->target){
            case TARGET_BYTECODE: {
                options->output_file = bc_file;
            } break;
            case TARGET_WINDOWS_x64:
            case TARGET_UNIX_x64: {
                options->output_file = exe_file;
            } break;
            default: Assert(false);
        }
    }

    int last_dot = options->output_file.find_last_of(".");
    if(last_dot != -1) {
        std::string format = options->output_file.substr(last_dot);
        if(format == ".o" || format == ".obj") {
            obj_file = options->output_file;
            generate_obj_file = true;
        } else if(format == ".exe") {
            generate_exe_file = true;
            exe_file = options->output_file;
        } else if(format == ".bc"){
            Assert(false);
            generate_bc_file = true;
            bc_file = options->output_file;
        } else {
            log::out << "You specified '"<<format<<"' as format for the output file but it I the compiler have no idea what to generate.\n";
            #define PRINT_FORMAT_OPTIONS\
                log::out << log::RED << "  Specify .o (or .obj) for object file\n";             \
                log::out << log::RED << "          .exe for executable\n";                      \
                log::out << log::RED << "          .bc for bytecode\n";                      \
                log::out << log::RED << "  or no output file for the default "<<exe_file<<"\n"; 
            PRINT_FORMAT_OPTIONS
            return;
        }
    } else {
        if(options->target == TARGET_UNIX_x64) {
            exe_file = options->output_file;
        } else {
            log::out << log::RED << "The specified output file '"<<options->output_file<<"' does not have a format. This is assumed to be an executable when targeting Linux but you are not ("<<options->target<<")\n";
            PRINT_FORMAT_OPTIONS
            return;
        }
    }

    importDirectories.add(options->modulesDirectory);
    
    preprocessor.init(&lexer, this);
    ast = AST::Create();
    bytecode = Bytecode::Create();
    bytecode->debugInformation = DebugInformation::Create(ast);
    reporter.lexer = &lexer;
    
    switch(options->target) {
        case TARGET_BYTECODE: {
            // do nothing
        } break;
        case TARGET_WINDOWS_x64:
        case TARGET_UNIX_x64: {
            program = X64Program::Create();
            program->debugInformation = bytecode->debugInformation;
        } break;
        default: Assert(false);
    }

    
    if(options->source_buffer.buffer && options->source_file.size()) {
        log::out << log::RED << "A source buffer and source file was specified. You can only specify one\n";
        options->compileStats.errors++;
        return;
    }
    
    
    bool skip_preload = false;
    // skip_preload = true;
    if(skip_preload) {
        log::out << log::YELLOW << "Preload is skipped!\n";
    }
    if(!skip_preload) {
        // should preload exist in modules?
        StringBuilder preload{};
        preload +=
        "struct Slice<T> {"
        // "struct @hide Slice<T> {"
            "ptr: T*;"
            "len: u64;"
        "}\n"
        // "operator [](slice: Slice<char>, index: u32) -> char {"
        //     "return slice.ptr[index];"
        // "}\n"
        "struct Range {" 
        // "struct @hide Range {" 
            "beg: i32;"
            "end: i32;"
        "}\n"
        "fn @native prints(str: char[]);\n"
        "fn @native printc(str: char);\n"
        "fn @native printi(n: i32);\n"
        // "#macro Assert(X) { prints(#quoted X); }"
        // "#macro Assert(X) { prints(#quoted X); *cast<char>null; }"
        ;
        if(options->target == TARGET_WINDOWS_x64) {
            preload += "#macro OS_WINDOWS #endmacro\n";
        } else if(options->target == TARGET_UNIX_x64) {
            preload += "#macro OS_UNIX #endmacro\n";
        } else if(options->target == TARGET_BYTECODE) {
            preload += "#macro OS_BYTECODE #endmacro\n";
        }
        if(options->linker == LINKER_MSVC) {
            preload += "#macro LINKER_MSVC #endmacro\n";
        } else if(options->linker == LINKER_GCC) {
            preload += "#macro LINKER_GCC #endmacro\n";
        }
        
        auto virtual_path = PRELOAD_NAME;
        lexer.createVirtualFile(virtual_path, &preload); // add before creating import
        preload_import_id = addOrFindImport(virtual_path);
        Assert(preload_import_id); // nocheckin
    }

    if(options->source_buffer.buffer) {
        StringBuilder builder{};
        // We ignore source_buffer.startColumn
        for(int i=0;i<options->source_buffer.startLine-1;i++){
            builder.append("\n");
        }
        builder.append(options->source_buffer.buffer, options->source_buffer.size);
        auto virtual_path = options->source_buffer.origin;
        lexer.createVirtualFile(virtual_path, &builder);
        initial_import_id = addOrFindImport(virtual_path);
    } else {
        // preload is added as dependency automatically
        initial_import_id = addOrFindImport(options->source_file);
    }
    if(initial_import_id == 0) {
        log::out << log::RED << "Could not find '"<<options->source_file << "'\n";
        options->compileStats.errors++;
        return;
    }
    options->threadCount = 1; // TODO: Don't force thread count

    int thread_count = options->threadCount;
    DynamicArray<engone::Thread> threads{};
    threads.resize(thread_count-1);
    
    lock_wait_for_imports.init(1,1);
    signaled = true;

    // Threaded compilation section
    for(int i=0;i<thread_count-1;i++) {
        threads[i].init(Thread_process, this); // Thread_process just calls processImports
    }
    processImports();
    for(int i=0;i<thread_count-1;i++) {
        threads[i].join();
    }

    switch(options->target) {
        case TARGET_BYTECODE: {
            // do nothing
        } break;
        case TARGET_WINDOWS_x64:
        case TARGET_UNIX_x64: {
            GenerateX64_finalize(this);
        } break;
        default: Assert(false);
    }
    
    if(options->compileStats.errors!=0){ 
        if(!options->silent)
            options->compileStats.printFailed();
        return;
    }
    if(options->compileStats.warnings!=0){
        if(!options->silent)
            options->compileStats.printWarnings();
    }

    if(options->only_preprocess) {
        auto& imps = lexer.getImports();
        auto iter = imps.iterator();
        std::unordered_map<std::string,bool> printed_imps;

        while(imps.iterate_reverse(iter)) {
            auto pair = printed_imps.find(iter.ptr->path);
            if(pair == printed_imps.end()) {
                // print first occurence of import (from preprocessor)
                if(TrimDir(iter.ptr->path) == "macros.btb") { // TODO: Temporary if
                    log::out << log::GOLD << iter.ptr->path<<"\n";
                    lexer.print(iter.ptr->file_id);
                }
                printed_imps[iter.ptr->path] = true;
            } else {
                // skip second occurence of import (from lexer)
            }
        }
        return;
    }

    // double time = engone::StopMeasure(tp);
    // engone::log::out << "Compiled in "<<FormatTime(time)<<"\n";

    // bytecode->print();

    if(!options->useDebugInformation && program)
        program->debugInformation = nullptr;
    
    if(compiler_got_stuck) {
        return;
    }

    switch(options->target){
        case TARGET_BYTECODE: {
            // if(generate_obj_file) {
            //     log::out << log::RED << "Options specified object file as output but that is not possible when target is bytecode.\n";
            //     return;
            // } else {

            // }
        } break;
        case TARGET_WINDOWS_x64: {
            ObjectFile::WriteFile(OBJ_COFF, obj_file, program, this);
        } break;
        case TARGET_UNIX_x64: {
            ObjectFile::WriteFile(OBJ_ELF, obj_file, program, this);
        } break;
    }

    if(generate_exe_file && options->target != TARGET_BYTECODE) {
        std::string cmd = "";
        bool outputOtherDirectory = false;
        switch(options->linker) {
        case LINKER_MSVC: {
            Assert(options->target == TARGET_WINDOWS_x64);
            outputOtherDirectory = exe_file.find("/") != std::string::npos;
            
            // TEMPORARY
            if(options->useDebugInformation) {
                log::out << log::RED << "You must use another linker than MSVC when compiling with debug information. Add the flag "<<log::LIME<<"--linker gcc"<<log::RED<<" (make sure to have gcc installed). The compiler does not support PDB debug information, it only supports DWARF. DWARF uses sections with long names but MSVC linker truncates those names. That's why you cannot use MVSC linker.\n";
                return;
            }

            cmd = "link /nologo /INCREMENTAL:NO ";
            if(options->useDebugInformation)
                cmd += "/DEBUG ";
            // else cmd += "/DEBUG "; // force debug info for now
            cmd += obj_file + " ";
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
                cmd += "/OUT:" + exe_file+" ";
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

            cmd += obj_file + " ";
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
            cmd += "-o " + exe_file;
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

        int exitCode = 0;
        {
            ZoneNamedNC(zone0,"Linker",tracy::Color::Blue2, true);
            
            if(!options->silent)
                log::out << log::LIME<<"Linker command: "<<cmd<<"\n";
            // engone::StartProgram((char*)cmd.c_str(),PROGRAM_WAIT, &exitCode, {}, linkerLog, linkerLog);
            engone::StartProgram((char*)cmd.c_str(),PROGRAM_WAIT, &exitCode);
        }
        options->compileStats.end_linker = StartMeasure();
        if(exitCode != 0) {
            log::out << log::RED << "linker failed\n";
            options->compileStats.errors++;
        }    
    } else {
        // TODO: If bytecode is the target and the user specified
        //   app.bc as outputfile should we write out a bytecode file format?
    }
    
    if(options->executeOutput) {
        switch(options->target) {
            case TARGET_WINDOWS_x64: {
                #ifdef OS_WINDOWS
                int exitCode;
                engone::StartProgram("test.exe",PROGRAM_WAIT, &exitCode);
                log::out << log::LIME <<"Exit code: " << exitCode << "\n";

                // Some user friendly information about crashes
                    // from winnt.h
                std::string err = StringFromExitCode(exitCode);
                if(err.size())
                    log::out << log::RED << " " << err << "\n";
                #else
                log::out << log::RED << "You cannot run a Windows program on Linux. Consider changing target when compiling (--target unix-x64)\n";
                #endif
            } break;
            case TARGET_UNIX_x64: {
                #ifdef OS_UNIX
                int exitCode;
                engone::StartProgram("test.exe",PROGRAM_WAIT, &exitCode);
                log::out << log::LIME <<"Exit code: " << exitCode << "\n";
                #else
                log::out << log::RED << "You cannot run a Unix program on Windows. Consider changing target when compiling (--target win-x64)\n";
                #endif
            } break;
            case TARGET_BYTECODE: {
                VirtualMachine vm{};
                vm.execute(bytecode, "main");
            } break;
            default: Assert(false);
        }
    } else {
        if(!options->silent)
            log::out << log::BLACK << "not executing program\n";
    }

    if(options->compileStats.errors==0) {
        if(!options->silent) {
            options->compileStats.printSuccess(options);
        }
    }
}
u32 Compiler::addOrFindImport(const std::string& path, const std::string& dir_of_origin_file, std::string* assumed_path_on_error) {
    Path abs_path = findSourceFile(path, dir_of_origin_file, assumed_path_on_error);
    if(abs_path.text.empty()) {
        return 0; // file does not exist? caller should throw error
    }
    CompilerImport imp{};
    lock_imports.lock();
    BucketArray<CompilerImport>::Iterator iter{};
    while(imports.iterate(iter)) {
        if(iter.ptr->path == abs_path.text) {
            u32 id = iter.ptr->import_id;
            lock_imports.unlock();
            return id;
        }
    }
    
    imp.path = abs_path.text;
    lexer::Import* intern_imp;
    imp.import_id = lexer.createImport(imp.path, &intern_imp);
    Assert(imp.import_id!=0);
    
    auto yes = imports.requestSpot(imp.import_id-1, &imp);
    Assert(yes);

    CompilerTask task{TASK_LEX};
    task.import_id = imp.import_id;
    tasks.add(task);
    
    lock_imports.unlock();

    // preload is shared with global scope.
    // every import has therefore access to the preload
    // we do not need to add dependency here.
    // Haha, slow down there boy. The preprocessor does not have shared scopes.
    // It only has dependencies to work with and if we want imports to access
    // macros from the Preload then we do need to add dependency here.
    // We could do something like shared dependencies perhaps?
    if(preload_import_id != 0) {
        // id is zero if we are adding preload right now.
        addDependency(yes->import_id, preload_import_id);
    }
    
    return imp.import_id;
}
void Compiler::addDependency(u32 import_id, u32 dep_import_id, const std::string& as_name) {
    lock_imports.lock();
    auto imp = imports.get(import_id-1);
    Assert(imp);
    imp->dependencies.add({dep_import_id, as_name});

    lock_imports.unlock();
    // LOG(CAT_PROCESSING,engone::log::CYAN<<"Add depend: "<<import_id<<"->"<<dep_import_id<<" ("<<TrimCWD(imp->path)<<")\n")
}
void Compiler::addTask_type_body(ASTFunction* ast_func, FuncImpl* func_impl) {
    lock_imports.lock();
    defer { lock_imports.unlock(); };
    CompilerTask picked_task{};
    picked_task.type = TASK_TYPE_BODY;
    // function has the preprocessed import but the tasks always assumes the original file import.
    picked_task.import_id = get_map_id(ast_func->getImportId(&lexer));
    picked_task.astFunc = ast_func;
    picked_task.funcImpl = func_impl;
    tasks.add(picked_task); // nocheckin, lock tasks
}
void Compiler::addTask_type_body(u32 import_id) {
    lock_imports.lock();
    defer { lock_imports.unlock(); };
    CompilerTask picked_task{};
    picked_task.type = TASK_TYPE_BODY;
    picked_task.import_id = import_id;
    tasks.add(picked_task); // nocheckin, lock tasks
}
void Compiler::addLibrary(u32 import_id, const std::string& path, const std::string& as_name) {
    using namespace engone;
    lock_imports.lock();
    auto imp = imports.get(import_id-1);
    Assert(imp);
    // imports.requestSpot(dep_import_id-1,nullptr);
    bool found = false;
    for(int i=0;i<imp->libraries.size();i++) {
        auto& lib = imp->libraries[i];
        if(lib.path == path) {
            // Multiple paths could possibly be the same.
            // Imagine "src/math.lib" and "./math.lib" where the dot indicates
            // the directory of the current source file ("src/main.btb")
            found = true;
            break;
        }
        if(lib.named_as == as_name) {
            found = true;
            // TODO: Improve error message
            options->compileStats.errors++;
            log::out << log::RED << "Multiple libraries cannot be named the same ("<<as_name<<").\n";
            log::out << log::RED << " \""<<BriefString(path)<<"\" collides with \""<<lib.path<<"\"\n";
            break;
        }
    }
    if(!found) {
        imp->libraries.add({path, as_name});
    }
    lock_imports.unlock();
}
void Compiler::addLinkDirective(const std::string& text){
    lock_miscellaneous.lock();
    // TODO: Trim whitespace?
    for(int i=0;i<(int)linkDirectives.size();i++){
        if(text == linkDirectives[i]) {
            lock_miscellaneous.unlock();
            return;
        }
    }
    linkDirectives.add(text);
    lock_miscellaneous.unlock();
}
Path Compiler::findSourceFile(const Path& path, const Path& sourceDirectory, std::string* assumed_path_on_error) {
    using namespace engone;
    // absolute
    // ./path always from sourceDirectory
    // relative to cwd OR import directories
    // .btb is implicit

    auto vfile = lexer.findVirtualFile(path.text);
    if(vfile)
        return path; // we don't want to search the file system for a virtual file
        
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
        if(!engone::FileExist(fullPath.text)) {
            if(assumed_path_on_error)
                *assumed_path_on_error = fullPath.text;
            return {};
        }
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
                // no path was assumed
                // if(assumed_path_on_error)
                //     *assumed_path_on_error = fullPath.text;
                fullPath = "";
            }
        }
    }
    return fullPath;
}

void Compiler::addError(const lexer::SourceLocation& location, CompileError errorType) {
    auto src = lexer.getTokenSource_unsafe(location);
    errorTypes.add({errorType, (u32)src->line});
}
void Compiler::addError(const lexer::Token& token, CompileError errorType) {
    auto src = lexer.getTokenSource_unsafe({token});
    errorTypes.add({errorType, (u32)src->line});
}
static const char* annotation_names[]{
    "unknown",
    "custom",

};
static const char* const PRELOAD_NAME = "<preload>";
static const char* const TYPEINFO_NAME = "Lang.btb";