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
    int str_length = strlen(str);
    Assert(str_length <= MAX_STRING_VERSION_LENGTH);
    if(str_length > MAX_STRING_VERSION_LENGTH) return; // in case you disable asserts for whatever reason
    // "0.1.0/0-2023.8.29"
    // "65535.65535.65535/65535-65535.256.256"
    // "9999.9999.9999/9999-9999.99.99"
    
    // good defaults
    memset(this, 0, sizeof(CompilerVersion));
    
    // char buffer[MAX_STRING_VERSION_LENGTH+1];
    // strcpy(buffer, str); // buffer and string length is already checked
    // int slashIndex = -1, dashIndex = -1;
    
    struct {
        union {
            int vers[4]{-1,-1,-1,-1};
            struct {
                int major;
                int minor;
                int patch;
                int revision;
            };
        };
        int name = -1;
        union {
            int date[3]{-1,-1,-1};
            struct {
                int year;
                int month;
                int day;
            };
        };
    } positions;
    
    int dot_count=0;
    int slash_count=0;
    int dash_count=0;
    positions.vers[dot_count] = 0;
    dot_count++;
    for(int i=0;i<str_length;i++){
        if(str[i] == '/') { // indicates state/name
            Assert(("too many slashes in version string", slash_count < 1));
            positions.name = i+1;
            slash_count++;
            continue;
        }
        if(str[i] == '.') {
            Assert(("too many dots in version string", dot_count < 4));
            positions.vers[dot_count] = i + 1;
            dot_count++;
            continue;
        }
        if(str[i] == '-') { // indicates date
            Assert(("too many dashes in version string", dash_count < 3));
            positions.date[dash_count] = i + 1;
            dash_count++;
            continue;
        }
    }
    
    if (positions.major != -1)
        major = atoi(str + positions.major);
    if (positions.minor != -1)
        minor = atoi(str + positions.minor);
    if (positions.patch != -1)
        patch = atoi(str + positions.patch);
    if (positions.revision != -1)
        revision = atoi(str + positions.revision);
        
    if (positions.name != -1) {
        int len = 0;
        if(positions.year != -1) {
            len = positions.year - positions.name - 1; // -1 for dash
        } else {
            len = str_length - positions.name;
        }
        Assert(len-1 < sizeof(name));
        strncpy(name, str + positions.name, len);
        name[len] = '\0';
    }
        
    if (positions.year != -1)
        year = atoi(str + positions.year);
    if (positions.month != -1)
        month = atoi(str + positions.month);
    if (positions.day != -1)
        day = atoi(str + positions.day);
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
            offset += snprintf(outBuffer + offset, bufferSize - offset, "-%d-%d-%d", (int)year, (int)month, (int)day);
    } else {
        if(flags & INCLUDE_REVISION)
            offset += snprintf(outBuffer + offset, bufferSize - offset, ".%d", (int)revision);
        if(flags & INCLUDE_NAME) {
            name[sizeof(name)-1] = '\0'; // just in case
            offset += snprintf(outBuffer + offset, bufferSize - offset, "/%s", name);
        }
        if(flags & INCLUDE_DATE)
            offset += snprintf(outBuffer + offset, bufferSize - offset, "-%d-%d-%d", (int)year, (int)month, (int)day);
    }
}

int Compiler::addTestLocation(lexer::SourceLocation loc, lexer::Lexer* lexer){
    auto imp = lexer->getImport_unsafe(loc);
    auto info = lexer->getTokenSource_unsafe(loc);
    
    testLocations.add({});
    testLocations.last().line = info->line;
    testLocations.last().column = info->column;
    testLocations.last().file = imp->path;
    return testLocations.size()-1;
}
TestLocation* Compiler::getTestLocation(int index) {
    return testLocations.getPtr(index);
}
void CompileStats::printSuccess(CompileOptions* opts){
    using namespace engone;
    double time_compile = DiffMeasure(end_compile - start_compile);
    double time_linker = DiffMeasure(end_linker - start_linker);
    
    log::out << "Compiled " << log::AQUA << FormatUnit((u64)lines)<<log::NO_COLOR << " lines, "<<log::AQUA<<FormatBytes(readBytes)<<log::NO_COLOR<<"\n";
    
    log::out << " total time: "<<log::AQUA<< FormatTime(time_compile)<<log::NO_COLOR;
    if(time_linker != 0) {
        log::out << ", link time: "<<log::AQUA<<FormatTime(time_linker)<<"\n";
    } else {
        log::out << "\n";
    }
    
    if(opts) {
        log::out << " options: "<<log::AQUA<<opts->target<<log::NO_COLOR<<", " << log::AQUA<< opts->linker<<log::NO_COLOR << "\n";
        if(generatedFiles.size() != 0) {
            log::out << " output: ";
            for(int i=0;i<(int)generatedFiles.size();i++){
                if(i!=0)
                    log::out << ", ";
                log::out << log::AQUA << generatedFiles[i] << log::NO_COLOR;
            }
            log::out << "\n";
        }
    }
}
void CompileStats::printFailed(){
    using namespace engone;
    log::out << log::RED<<"Compiler failed with "<<errors<<" error(s)\n";
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

    #define SCOPE_LOCK lock_imports.lock(); defer { lock_imports.unlock(); };

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
        int task_index=tasks.size()-1;
        while(task_index < tasks.size()) {
            // NOTE: We process tasks backwards so that most macro dependencies are evaluated first
            int cur_task_index = task_index;
            task_index--;
            
            CompilerTask& task = tasks[cur_task_index];
            
            // TODO: Optimize dependency checking
            // TODO: Some checks don't respect nested dependencies.
            bool missing_dependency = false;
            if(task.type == TASK_PREPROCESS_AND_PARSE) {
                // Before preprocessing we must make sure all imports that are linked in any way
                // are lexed before preprocessing.
                
                // Macros can be defined in #if and can be created at TASK_LEX or at TASK_PREPROCESS_AND_PARSE
                // Unless imports depend on each other we should preprocess and parse the dependencies first so
                // that macros exist when we expect them to. The code below calculates those dependencies.
                // In the case of a circular dependencies, macros may not be defined.

                // Step 1. Which dependency depends on myself.
                CompilerImport* im = imports.get(task.import_id-1);
                for(auto& dep : im->dependencies) {
                    // if(!dep.disabled) continue;
                    // TODO: Reuse allocation instead of allocating and freeing the array for each dependency.
                    // TODO: Would this be faster with one array and a "head" variable keeping track of the next index to check?
                    DynamicArray<u32> checked_ids{};
                    DynamicArray<u32> ids_to_check{};
                    ids_to_check.add(dep.id);
                    
                    // TODO: We detect circular dependencies multiple times per task. Is it possible
                    //   to cache the results for next time so we don't have to check this again?
                    //   It might not be because of #import inside #if. Who knows when a new import pops up.
                    
                    if(dep.circular_dependency_to_myself) continue;
                    
                    while(ids_to_check.size()) {
                        u32 id = ids_to_check.last();
                        CompilerImport* imp_dep = imports.get(id-1);
                        ids_to_check.pop();
                        checked_ids.add(id);
                        
                        for(auto& it : imp_dep->dependencies) {
                            bool checked = false;
                            for(auto& it2 : checked_ids) {
                                if(it.id == it2) {
                                    checked = true;
                                    break;
                                }
                            }
                            if(checked) continue;
                            ids_to_check.add(it.id);
                            if(it.id == im->import_id) {
                                dep.circular_dependency_to_myself = true;
                                break;
                            }
                        }
                        if(dep.circular_dependency_to_myself)
                            break;
                    }
                }
                // Step 2. Wait for the dependencies that don't depend on me to parse first.
                DynamicArray<u32> checked_ids{};
                DynamicArray<u32> ids_to_check{};
                if(!options->only_preprocess) { // only_preprocess skips parsing so we have to assume that everything has circular dependencies
                    for(auto& dep : im->dependencies) {
                        if(dep.circular_dependency_to_myself)
                            continue;
                        ids_to_check.add(dep.id);
                    }
                    while(ids_to_check.size()) {
                        u32 id = ids_to_check.last();
                        CompilerImport* imp_dep = imports.get(id-1);
                        ids_to_check.pop();
                        checked_ids.add(id);

                        if(!(imp_dep->state & TASK_PREPROCESS_AND_PARSE)) {
                            if(ids_to_check.size() == 0)  {
                                // direct dependency
                                LOG(LOG_TASKS, log::GRAY<<" depend: "<<im->import_id<<"->"<<id<<" ("<<TrimCWD(im->path)<<"->"<<(imp_dep?TrimCWD(imp_dep->path):"?")<<")\n")
                            } else {
                                // inherited dependency
                                LOG(LOG_TASKS, log::GRAY<<" depend: "<<im->import_id<<"-->"<<id<<" ("<<TrimCWD(im->path)<<"->"<<(imp_dep?TrimCWD(imp_dep->path):"?")<<")\n")
                            }
                            missing_dependency = true;
                            break;
                        }

                        for(auto& it : imp_dep->dependencies) {
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
                }
                // Step 3. Process the circular dependencies.
                if(!missing_dependency) {
                    checked_ids.resize(0);
                    ids_to_check.resize(0);
                    
                    for(auto& dep : im->dependencies) {
                        if(!dep.circular_dependency_to_myself && !options->only_preprocess)
                            continue;
                        ids_to_check.add(dep.id);
                    }
                    while(ids_to_check.size()) {
                        u32 id = ids_to_check.last();
                        CompilerImport* imp_dep = imports.get(id-1);
                        ids_to_check.pop();
                        checked_ids.add(id);

                        if(!(imp_dep->state & TASK_LEX)) {
                            if(ids_to_check.size() == 0)  {
                                // direct dependency
                                LOG(LOG_TASKS, log::GRAY<<" depend: "<<im->import_id<<"->"<<id<<" ("<<TrimCWD(im->path)<<"->"<<(imp_dep?TrimCWD(imp_dep->path):"?")<<")\n")
                            } else {
                                // inherited dependency
                                LOG(LOG_TASKS, log::GRAY<<" depend: "<<im->import_id<<"-->"<<id<<" ("<<TrimCWD(im->path)<<"->"<<(imp_dep?TrimCWD(imp_dep->path):"?")<<")\n")
                            }
                            missing_dependency = true;
                            break;
                        }

                        for(auto& it : imp_dep->dependencies) {
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
                }
            } else if (task.type == TASK_TYPE_ENUMS) {
                
                // for(int i=0;i<tasks.size();i++) {
                //     CompilerTask& t = tasks[i];
                //     // TODO: We may only have tasks to generate bytecodes but a thread
                //     //   may have taken out a task and be working on it right now.
                //     //   Redesign this
                //     if(t.type < TASK_TYPE_BODY) {
                //         LOG(LOG_TASKS, log::GRAY<<" depend on task "<<i<< " (import_id: "<<t.import_id<<")\n")
                //         missing_dependency = true;
                //         // we miss a dependency in that all other imports have
                //         // not been type checked yet
                //         break;
                //     }
                // }
                CompilerImport* im = imports.get(task.import_id-1);
                for(int j=0;j<im->dependencies.size();j++) {
                    CompilerImport* dep = imports.get(im->dependencies[j].id-1);
                    // TODO: I had a thought about a potential problem here but the bug I thought caused it didn't. So maybe there isn't a problem waiting for lexed and preprocessed? Of course, we will need to add parsed, type checked and generated in the future.
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
                    // TODO: I had a thought about a potential problem here but the bug I thought caused it didn't. So maybe there isn't a problem waiting for lexed and preprocessed? Of course, we will need to add parsed, type checked and generated in the future.
                    if(!dep || !(dep->state & TASK_TYPE_ENUMS)) {
                        LOG(LOG_TASKS, log::GRAY<<" depend: "<<im->import_id<<"->"<<im->dependencies[j].id<<" ("<<TrimCWD(im->path)<<"->"<<(dep?TrimCWD(dep->path):"?")<<")\n")
                        missing_dependency = true;
                        break;
                    }
                }
            } else if (task.type == TASK_TYPE_FUNCTIONS) {
                // when checking signatures... all structs should be generated, almost.

                // TODO: Optimize, store an integer of how many non-type checked tasks
                //   we have left instead of iterating all tasks every time.
                for(int i=0;i<tasks.size();i++) {
                    CompilerTask& t = tasks[i];
                    // TODO: We may only have tasks to generate bytecodes but a thread
                    //   may have taken out a task and be working on it right now.
                    //   Redesign this
                    if(t.type < TASK_TYPE_FUNCTIONS) {
                        LOG(LOG_TASKS, log::GRAY<<" depend on task "<<i<< " (import_id: "<<t.import_id<<")\n")
                        missing_dependency = true;
                        // we miss a dependency in that all other imports have
                        // not been type checked yet
                        break;
                    }
                }
                if(options->incremental_build && !missing_dependency && last_modified_time == 0.0) {
                    // NOTE: incremental_build is an experimental feature.
                    //   It doesn't consider linked libraries. Only .btb source files.
                    last_modified_time = compute_last_modified_time(); // should run once
                    double output_time = 0.0;
                    bool yes = FileLastWriteSeconds(options->output_file, &output_time);
                    if(yes) {
                        // log::out << last_modified_time << " < " << output_time << " ("<<options->output_file<<")\n";
                        if (last_modified_time < output_time) {
                            tasks.resize(0);
                            output_is_up_to_date = true;
                            break;
                        }
                    }
                }
            } else if (task.type == TASK_TYPE_BODY) {
                // when we check bodies, functions and globals from other imports must be available

                if(task.astFunc && (task.astFunc->is_being_checked || (task.astFunc->parentStruct && task.astFunc->parentStruct->is_being_checked))) {
                    missing_dependency = true;
                } else {
                    // TODO: Optimize, store an integer of how many non-type checked tasks
                    //   we have left instead of iterating all tasks every time.
                    for(int i=0;i<tasks.size();i++) {
                        CompilerTask& t = tasks[i];
                        // TODO: We may only have tasks to generate bytecodes but a thread
                        //   may have taken out a task and be working on it right now.
                        //   Redesign this
                        if(t.type < TASK_TYPE_BODY) {
                            LOG(LOG_TASKS, log::GRAY<<" depend on task "<<i<< " (import_id: "<<t.import_id<<")\n")
                            missing_dependency = true;
                            // we miss a dependency in that all other imports have
                            // not been type checked yet
                            break;
                        }
                    }
                }
            } else if (task.type == TASK_GEN_BYTECODE) {
                // global data must have been generated before we can generate data
                // (in the current implementation)

                // TODO: Optimize, store an integer of how many non-type checked tasks
                //   we have left instead of iterating all tasks every time.
                for(int i=0;i<tasks.size();i++) {
                    CompilerTask& t = tasks[i];
                    // log::out << "type " << t.type << "\n";
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
            } else if (task.type == TASK_GEN_BYTECODE_RUNDIR) {
                // functions without run directives should have been generated before
                // we start running those with.

                // TODO: Optimize, store an integer of how many non-type checked tasks
                //   we have left instead of iterating all tasks every time.
                for(int i=0;i<tasks.size();i++) {
                    CompilerTask& t = tasks[i];
                    // log::out << "type " << t.type << "\n";
                    // TODO: We may only have tasks to generate bytecodes but a thread
                    //   may have taken out a task and be working on it right now.
                    //   Redesign this
                    if(t.type < TASK_GEN_BYTECODE_RUNDIR) {
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
                
                // When generating global data, we need to make sure the calls to functions 
                // in global expressions have been generated. Otherwise we would crash in applyRelocations
                // We could ensure all functions have been generated OR we could just ensure
                // the dependencies while applying partial relocations for the relevant tinycode (_comp_time_).

                // NOTE: I have been messing around with dependencies and task types.
                //   I have found lots of issues were circular dependencies of structs across imports doesn't, applying relocations triggers an assert, and more problems.
                //   Right now, most task types require all task types/compiler phases to be processed in waves (we make sure all tasks have been processed by the earlier step)
                //    INSTEAD OF, checking for import dependencies.
                // CompilerImport* im = imports.get(task.import_id-1);
                // for(int j=0;j<im->dependencies.size();j++) {
                //     CompilerImport* dep = imports.get(im->dependencies[j].id-1);
                //     if(!dep || !(dep->state & TASK_GEN_BYTECODE)) {
                //         LOG(LOG_TASKS, log::GRAY<<" depend: "<<im->import_id<<"->"<<im->dependencies[j].id<<" ("<<TrimCWD(im->path)<<"->"<<(dep?TrimCWD(dep->path):"?")<<")\n")
                //         missing_dependency = true;
                //         break;
                //     }
                // }

                for(int i=0;i<tasks.size();i++) {
                    CompilerTask& t = tasks[i];
                    // TODO: We may only have tasks to generate bytecodes but a thread
                    //   may have taken out a task and be working on it right now.
                    //   Redesign this
                    if(t.type < TASK_GEN_MACHINE_CODE) {
                        LOG(LOG_TASKS, log::GRAY<<" depend on task "<<i<< " (import_id: "<<t.import_id<<")\n")
                        missing_dependency = true;
                        // we miss a dependency in that all other imports have
                        // not been type checked yet
                        break;
                    }
                }
            }
            if(missing_dependency) {
                finished=false;
                continue;
            }
            picked_task = task;
            tasks.removeAt(cur_task_index);
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
            if(compile_stats.errors > 0) {
                // error should have been reported
                lock_imports.unlock();
                break;
            } else {
                if(!options->only_preprocess) {
                    log::out << log::RED << "ERROR: Some tasks have dependencies that cannot be computed. Bug in compiler?\n";
                }
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
        
        CompilerImport* compiler_imp = nullptr;
        if(found) {
            compiler_imp = imports.get(picked_task.import_id-1);
            if(picked_task.astFunc && picked_task.type == TASK_TYPE_BODY) {
                Assert(!picked_task.astFunc->is_being_checked);
                Assert(!picked_task.astFunc->parentStruct || !picked_task.astFunc->parentStruct->is_being_checked);
                picked_task.astFunc->is_being_checked = true;
                if(picked_task.astFunc->parentStruct)
                    picked_task.astFunc->parentStruct->is_being_checked = true;
            }
        }
        lock_imports.unlock();
        
        if(found) {
            if(picked_task.type == TASK_LEX) {
                LOGD(LOG_TASKS, log::GREEN<<"Lexing and preprocessing: "<<compiler_imp->import_id <<" ("<<TrimCWD(compiler_imp->path)<<")\n")
                // compiler_imp->import_id may zero but may also be pre-created
                u32 old_id = compiler_imp->import_id;
                compiler_imp->import_id = lexer.tokenize(compiler_imp->path, old_id);
                if(!compiler_imp->import_id) {
                    compile_stats.errors++; // TODO: Temporary. We should call a function that reports an error since we may need to do more things than just incrementing a counter.
                    log::out << log::RED << "ERROR: Could not find "<<compiler_imp->path << "\n"; // TODO: Improve error message
                } else {
                    u32 new_id = preprocessor.process(compiler_imp->import_id,false);
                    Assert(new_id == 0); // first preprocessor phase should not create an import
                    
                    // lexer.print(old_id);
                    
                    auto intern_imp = lexer.getImport_unsafe(compiler_imp->import_id);
                    
                    engone::atomic_add(&compile_stats.lines, intern_imp->lines);
                    engone::atomic_add(&compile_stats.readBytes, intern_imp->fileSize);
                    
                    u32 tokens = 0;
                    if(intern_imp->chunk_indices.size() > 0) {
                        tokens = (intern_imp->chunk_indices.size()-1) * TOKEN_ORIGIN_TOKEN_MAX + lexer.getChunk_unsafe(intern_imp->chunk_indices.last())->tokens.size();
                    }
                    
                    LOG(LOG_TASKS, log::GRAY
                        << " lines: "<<intern_imp->lines
                        << ", tokens: "<<tokens
                        << ", file_size: "<<FormatBytes(intern_imp->fileSize)
                        << "\n")
                    
                    compiler_imp->state = (TaskType)(compiler_imp->state | picked_task.type);
                    picked_task.type = TASK_PREPROCESS_AND_PARSE;
                    picked_task.import_id = compiler_imp->import_id;
                    
                    SCOPE_LOCK

                    tasks.add(picked_task);
                }
            } else if(picked_task.type == TASK_PREPROCESS_AND_PARSE) {
                
                LOGD(LOG_TASKS, log::GREEN<<"Preprocess and parse: "<<compiler_imp->import_id <<" ("<<TrimCWD(compiler_imp->path)<<")\n")
                
                compiler_imp->preproc_import_id = preprocessor.process(compiler_imp->import_id, true);
                map_id(compiler_imp->preproc_import_id, compiler_imp->import_id);
                
                // TODO: Free memory for old import since we don't need it anymore.
                //   import_id is used in dependencies but I don't think we ever
                //   query the import from the lexer so from the compilers
                //   perspective the import_id still exists but the data from
                //   it doesn't which is fine since the dependencies don't care
                //   about the data of the import, just whether it was loaded
                //   at some point in time. Double check this though.
                // lexer.destroyImport_unsafe(compiler_imp->import_id);

                auto intern_imp = lexer.getImport_unsafe(compiler_imp->preproc_import_id);
                
                u32 tokens = 0;
                if(intern_imp->chunk_indices.size() > 0) {
                    tokens = (intern_imp->chunk_indices.size()-1) * TOKEN_ORIGIN_TOKEN_MAX + lexer.getChunk_unsafe(intern_imp->chunk_indices.last())->tokens.size();
                }
                
                LOG(LOG_TASKS, log::GRAY
                    << " tokens: "<<tokens
                    << "\n")
                
                if(!options->only_preprocess) {
                    auto my_scope = parser::ParseImport(compiler_imp->preproc_import_id, this);
                    
                    // ast->print();
                    compiler_imp->scopeId = my_scope->scopeId;

                    compiler_imp->state = (TaskType)(compiler_imp->state | picked_task.type);
                    picked_task.type = TASK_TYPE_ENUMS;
                    picked_task.import_id = compiler_imp->import_id;

                    SCOPE_LOCK

                    tasks.add(picked_task);
                }
            } else if(picked_task.type == TASK_TYPE_ENUMS) {
                auto my_scope = ast->getScope(compiler_imp->scopeId);
                LOGD(LOG_TASKS, log::GREEN<<"Type enums: "<<compiler_imp->import_id <<" ("<<TrimCWD(compiler_imp->path)<<")\n")

                // We share scopes here when we know that all imports have been parsed
                // and have scopes we can throw around.

                // log::out << BriefString(compiler_imp->path) << " deps:\n";
                for(int i=0;i<compiler_imp->dependencies.size();i++) {
                    if(compiler_imp->dependencies[i].disabled)
                        continue;

                    auto dep = imports.get(compiler_imp->dependencies[i].id-1);
                    auto dep_scope = ast->getScope(dep->scopeId);
                    my_scope->sharedScopes.add(dep_scope);

                    // log::out << "  " << BriefString(dep->path) << "\n";
                }

                if(compiler_imp->path == PRELOAD_NAME) {
                    auto global_scope = ast->getScope(ast->globalScopeId);
                    global_scope->sharedScopes.add(my_scope);
                }
                // TODO: Below we assume all files named Lang.btb specify type information which is bad.
                //   Do we check modules/Lang.btb instead?
                auto name = TrimDir(compiler_imp->path);
                if(name == TYPEINFO_NAME && typeinfo_import_id == 0) {
                    typeinfo_import_id = compiler_imp->import_id;
                    // TODO: Should type information be at global scope.
                    //   Should it be loaded by default? I don't think so.
                    //   Maybe you want to import it once in the main file so
                    //  that you don't have to import it everywhere else though.
                }

                int prev_errors = compile_stats.errors;
                TypeCheckEnums(ast, my_scope->astScope, this);
                bool had_errors = compile_stats.errors > prev_errors;

                // TODO: Print amount of checked enums.
                //  LOG(LOG_TASKS, log::GRAY
                //     << " tokens: "<<tokens
                //     << "\n")

                if(!had_errors) {
                    compiler_imp->state = (TaskType)(compiler_imp->state | picked_task.type);
                    picked_task.type = TASK_TYPE_STRUCTS;
                    picked_task.import_id = compiler_imp->import_id;

                    SCOPE_LOCK
                    tasks.add(picked_task);
                }
            } else if(picked_task.type == TASK_TYPE_STRUCTS) {
                auto my_scope = ast->getScope(compiler_imp->scopeId);
                LOGD(LOG_TASKS, log::GREEN<<"Type structs: "<<compiler_imp->import_id <<" ("<<TrimCWD(compiler_imp->path)<<")\n")
                
                // Checking structs is hard.
                // We keep checking until it's done or
                // no more progress is made.

                // how do we know when we have done a full loop?
                // How do we know we have with multiple threads?

                bool ignore_errors = true;
                bool changed = false;
                bool yes = SIGNAL_SUCCESS == TypeCheckStructs(ast, my_scope->astScope, this, ignore_errors, &changed);

                // TODO: Print amount of checked structs.
                //  LOG(LOG_TASKS, log::GRAY
                //     << " tokens: "<<tokens
                //     << "\n")
                // TODO: Print whether we are done with structs or if some remain unchecked.
                if(changed)
                    struct_tasks_since_last_change=0;
                else
                    struct_tasks_since_last_change++;

                if(yes) {
                    compiler_imp->state = (TaskType)(compiler_imp->state | picked_task.type);
                    picked_task.type = TASK_TYPE_FUNCTIONS;
                    picked_task.import_id = compiler_imp->import_id;

                    SCOPE_LOCK
                    tasks.add(picked_task);
                } else {
                    // log::out << "fail, "<<struct_tasks_since_last_change << ", tasks: " << tasks.size() << " " << BriefString(compiler_imp->path)<<"\n";
                    if(struct_tasks_since_last_change > tasks.size() * 2) {
                        ignore_errors = false;
                        TypeCheckStructs(ast, my_scope->astScope, this, ignore_errors, &changed);
                    }
                    // if(!changed) {
                    //     if(picked_task.no_change) {
                    //         ignore_errors = false;
                    //         TypeCheckStructs(ast, my_scope->astScope, this, ignore_errors, &changed);
                    //     }
                    // }
                    picked_task.no_change = !changed;
                    if(ignore_errors) {
                        lock_imports.lock();
                        // When tasks are processed backwards, 
                        // we must put task first so that we
                        // process the other struct check tasks first
                        tasks.insert(0, picked_task);
                        // tasks.add(picked_task);
                        lock_imports.unlock();
                    }
                }
            } else if(picked_task.type == TASK_TYPE_FUNCTIONS) {
                auto my_scope = ast->getScope(compiler_imp->scopeId);
                LOGD(LOG_TASKS, log::GREEN<<"Type function signatures: "<<compiler_imp->import_id <<" ("<<TrimCWD(compiler_imp->path)<<")\n")
                
                int prev_errors = compile_stats.errors;
                bool is_initial_import = initial_import_id == compiler_imp->import_id;
                TypeCheckFunctions(ast, my_scope->astScope, this, is_initial_import);
                bool had_errors = compile_stats.errors > prev_errors;

                // TODO: Print amount of checked stuff.
                //  LOG(LOG_TASKS, log::GRAY
                //     << " tokens: "<<tokens
                //     << "\n")

                if(!had_errors) {
                    compiler_imp->state = (TaskType)(compiler_imp->state | picked_task.type);
                    
                    if (entry_point.size() != 0) {
                        auto iden = ast->findIdentifier(compiler_imp->scopeId,0,entry_point,nullptr, true, false);
                        if(iden && iden->is_fn()) {
                            auto fun = iden->cast_fn();
                            Assert(fun->funcOverloads.overloads.size());
                            auto overload = fun->funcOverloads.overloads[0];
                            addTask_type_body(overload.astFunc, overload.funcImpl);
                        } else {
                            if(is_initial_import) {
                                addTask_type_body(compiler_imp->import_id);
                            }
                        }
                    }

                    // We add GEN_BYTECODE now but it won't be processed
                    // untill all TASK_TYPE_BODY tasks are handled first.
                    // We add GEN_BYTECODE here because it will run once per import.
                    // Otherwise we would need some code somewhere to check if all 
                    // TASK_TYPE_BODY have been dealt with and then add one GEN_BYTECODE
                    // per import.
                    compiler_imp->state = (TaskType)(compiler_imp->state | picked_task.type);
                    picked_task.type = TASK_GEN_BYTECODE;
                    picked_task.import_id = compiler_imp->import_id;

                    SCOPE_LOCK
                    tasks.add(picked_task);
                }
            } else if(picked_task.type == TASK_TYPE_BODY) {
                auto my_scope = ast->getScope(compiler_imp->scopeId);
                if(picked_task.astFunc) {
                    LOGD(LOG_TASKS, log::GREEN<<"Type function body: "<<picked_task.astFunc->name <<" (from import: "<<compiler_imp->import_id<<", "<<TrimCWD(compiler_imp->path)<<")\n")
                } else {
                    LOGD(LOG_TASKS, log::GREEN<<"Type global body: "<< compiler_imp->import_id <<" ("<<TrimCWD(compiler_imp->path)<<")\n")
                }
                
                int prev_errors = compile_stats.errors;
                
                ASTScope* import_scope = my_scope->astScope;
                if(compiler_imp->type_checked_import_scope)
                    import_scope = nullptr;
                TypeCheckBody(this, picked_task.astFunc,picked_task.funcImpl, import_scope);
                compiler_imp->type_checked_import_scope = true;
                
                if(picked_task.astFunc) {
                    lock_imports.lock();
                    picked_task.astFunc->is_being_checked = false;
                    if(picked_task.astFunc->parentStruct)
                        picked_task.astFunc->parentStruct->is_being_checked = false;
                    lock_imports.unlock();
                }

                // TODO: Print some interesting info?
                //  LOG(LOG_TASKS, log::GRAY
                //     << " tokens: "<<tokens
                //     << "\n")

                bool had_errors = compile_stats.errors > prev_errors;
                // TODO: Do we need to do something with errors?

                // NOTE: TypeCheckBody may call addTask_type_body.
                //   TASK_TYPE_BODY is per function, not per import so
                //   we can't add GEN_BYTECODE task here.
            } else if(picked_task.type == TASK_GEN_BYTECODE || picked_task.type == TASK_GEN_BYTECODE_RUNDIR) {
                auto my_scope = ast->getScope(compiler_imp->scopeId);
                LOGD(LOG_TASKS, log::GREEN<<"Gen bytecode: "<<compiler_imp->import_id <<" ("<<TrimCWD(compiler_imp->path)<<")\n")

                // NOTE: Type checker reserves all require global data.
                //   The generator phase does not create new global data.
                if(!have_prepared_global_data) { // cheap quick check
                    lock_miscellaneous.lock();
                    if(!have_prepared_global_data) { // thread safe check
                        GenContext c{};
                        c.init_context(this);
                        c.generateData(); // make sure this function doesn't call lock_miscellaneous
                        have_prepared_global_data = true;
                    }
                    lock_miscellaneous.unlock();
                }
                if(!have_generated_comp_time_global_data && picked_task.type == TASK_GEN_BYTECODE_RUNDIR) { // cheap quick check, will the compiler optimize it away?
                    lock_miscellaneous.lock();
                    if(!have_generated_comp_time_global_data) { // thread safe check
                        // We need to generate global data here when
                        // all functions have been generated.
                        GenContext c{};
                        c.init_context(this);
                        c.generateGlobalData();
                        have_generated_comp_time_global_data = true;
                    }
                    lock_miscellaneous.unlock();
                }
                if(!have_run_global_run_directives && picked_task.type == TASK_GEN_BYTECODE_RUNDIR) { // cheap quick check
                    lock_miscellaneous.lock();
                    if(!have_run_global_run_directives) { // thread safe check
                        GenContext c{};
                        c.init_context(this);
                        for(int i=0;i<global_run_directives.size();i++) {
                            auto& rundir = global_run_directives[i];
                            c.executeGlobalRunDirective(&rundir);
                        }
                        have_run_global_run_directives = true;
                    }
                    lock_miscellaneous.unlock();
                }

                int prev_errors = compile_stats.errors;
                
                bool is_initial_import = initial_import_id == compiler_imp->import_id;
                bool gen_func_with_run_directives = picked_task.type == TASK_GEN_BYTECODE_RUNDIR;
                auto yes = GenerateScope(my_scope->astScope, this, compiler_imp, &compiler_imp->tinycodes, is_initial_import, gen_func_with_run_directives);
                // TODO: Print some interesting info?
                //  LOG(LOG_TASKS, log::GRAY
                //     << " tokens: "<<tokens
                //     << "\n")

                bool new_errors = false;
                if(prev_errors < compile_stats.errors) {
                    new_errors = true;
                }
                
                if(!new_errors) {
                    if(bytecode->debugDumps.size() != 0) {
                        for(int i=0;i<(int)bytecode->debugDumps.size();i++) {
                            // TODO: Mutex lock when popping debug dump?
                            auto& dump = bytecode->debugDumps[i];
                            bool found = false;
                            for(int j=0;j<compiler_imp->tinycodes.size();j++) {
                                auto t = compiler_imp->tinycodes[j];
                                if (t->index == dump.tinycode_index){
                                    found = true;
                                    break;
                                }
                            }
                            if(!found)
                                continue;
                            if(dump.dumpBytecode) {
                                if(dump.description.empty()) {
                                    log::out << log::LIME << "## Dump:\n";
                                } else {
                                    log::out << log::LIME << "## Dump: "<<log::GOLD<<dump.description<<"\n";
                                }
                                auto tinycode = bytecode->tinyBytecodes[dump.tinycode_index];
                                tinycode->print(dump.bc_startIndex, dump.bc_endIndex, bytecode);
                            }
                            if (dump.dumpAsm) {
                                // TODO: Write to object file, then run disassembler?
                            }
                            
                            bytecode->debugDumps.removeAt(i);
                            i--;
                        }
                    }
                    
                    // @DEBUG
                    LOG_CODE(LOG_BYTECODE,
                        for(auto t : compiler_imp->tinycodes) {
                            // log::out << log::GOLD << t->name << "\n";
                            t->print(0,-1,bytecode,nullptr,true);
                        }
                    )
                    
                    compiler_imp->state = (TaskType)(compiler_imp->state | picked_task.type);
                    if(picked_task.type == TASK_GEN_BYTECODE)
                        picked_task.type = TASK_GEN_BYTECODE_RUNDIR;
                    else
                        picked_task.type = TASK_GEN_MACHINE_CODE;
                    picked_task.import_id = compiler_imp->import_id;

                    SCOPE_LOCK
                    tasks.add(picked_task);
                }
            } else if(picked_task.type == TASK_GEN_MACHINE_CODE) {
                // auto my_scope = ast->getScope(compiler_imp->scopeId);
                LOGD(LOG_TASKS, log::GREEN<<"Gen machine code: "<<compiler_imp->import_id <<" ("<<TrimCWD(compiler_imp->path)<<")\n")

                // if(!have_generated_comp_time_global_data) { // cheap quick check, will the compiler optimize it away?
                //     lock_miscellaneous.lock();
                //     if(!have_generated_comp_time_global_data) { // thread safe check
                //         // We need to generate global data here when
                //         // all functions have been generated.
                //         GenContext c{};
                //         c.init_context(this);
                //         c.generateGlobalData();
                //         have_generated_comp_time_global_data = true;
                //     }
                //     lock_miscellaneous.unlock();
                // }
                // if(!have_run_global_run_directives) { // cheap quick check
                //     lock_miscellaneous.lock();
                //     if(!have_run_global_run_directives) { // thread safe check
                //         GenContext c{};
                //         c.init_context(this);
                //         for(int i=0;i<global_run_directives.size();i++) {
                //             auto& rundir = global_run_directives[i];
                //             c.executeGlobalRunDirective(&rundir);
                //         }
                //         have_run_global_run_directives = true;
                //     }
                //     lock_miscellaneous.unlock();
                // }

                if(compile_stats.errors == 0) {
                    // can't generate if bytecode is messed up.

                    switch(options->target) {
                        case TARGET_BYTECODE: {
                            // do nothing
                        } break;
                        case TARGET_WINDOWS_x64:
                        case TARGET_LINUX_x64: {
                            for(auto t : compiler_imp->tinycodes)
                                GenerateX64(this, t);
                        } break;
                        case TARGET_ARM: {
                            for(auto t : compiler_imp->tinycodes)
                                GenerateARM(this, t);
                        } break;
                        default: Assert(false);
                    }
                }
                   // TODO: Print some interesting info?
                //  LOG(LOG_TASKS, log::GRAY
                //     << " tokens: "<<tokens
                //     << "\n")

                    
                compiler_imp->state = (TaskType)(compiler_imp->state | picked_task.type);
            } else {
                Assert(false);
            }
            
            lock_imports.lock();
            
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
    InitAssertHandler();
    Compiler* compiler = (Compiler*)self;
    compiler->processImports();
    return 0;
}
void Compiler::run(CompileOptions* options) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Gray19);
    // auto tp = engone::StartMeasure();

    if(options->linker == LINKER_MSVC) {
        if(!is_msvc_configured()) {
            bool yes = configure_msvc();
            if(!yes) {
                log::out << log::RED << "MSVC as linker was specified but it it is not configured correctly and automatic configuration failed, have you installed Visual Studio?\n";
                compile_stats.errors += 1;
                return;
            } else if(options->verbose) {
                log::out << log::GRAY << "MSVC toolchain configured automatically.\n";
            }
        }
    }
    
    if(options->target == TARGET_ARM) {
        log::out << log::YELLOW << "ARM support is experimental and does not fully work.\n";
    }
    if(options->target == TARGET_AARCH64) {
        log::out << log::RED << "ARM-64 bit (aarch64) is not supported.";
        return;
    }

    std::string path_to_exe = TrimLastFile(engone::GetPathToExecutable());
    std::string dir_of_exe = TrimLastFile(path_to_exe);

    compiler_executable_dir = dir_of_exe;

    compile_stats.start_compile = engone::StartMeasure();

    Assert(!this->options);
    this->options = options;

    // ################################
    // What are we trying to generate?
    // #################################
    
    #ifdef OS_WINDOWS
    // Linux developers only care about DWARF (probably)
    // It's Windows developers that expect PDB which we don't provide.
    // This text clutters the console so I commented it out. - Emarioo, 2024-05-26
    // if(options->useDebugInformation) {
    //     log::out << log::YELLOW << "Debug information was recently added and is not complete. Only DWARF with COFF (on Windows) and GCC is supported. ELF for Unix systems is on the way.\n";
    //     // return EXIT_CODE_NOTHING; // debug is used with linker errors (.text+0x32 -> file:line)
    // }
    #endif
    if(options->source_file.size() == 0 && !options->source_buffer.buffer) {
        log::out << log::RED << "No source file was specified.\n";
        return;
    }

    std::string output_path = options->output_file;
    if(options->output_file.size() == 0) {
        switch(options->target){
            case TARGET_BYTECODE: {
                output_path = "main.bc";
            } break;
            case TARGET_WINDOWS_x64:
            case TARGET_LINUX_x64: {
                output_path = "main.exe"; // skip .exe on Linux?
            } break;
            case TARGET_ARM: {
                output_path = "main.elf";
            } break;
            default: Assert(false);
        }
    }
    
    int slash_index = output_path.find_last_of("/");
    int dot_index = output_path.find_last_of(".");
    if (dot_index == -1) {
        dot_index = output_path.size();
    }

    enum OutputType {
        OUTPUT_INVALID,
        OUTPUT_OBJ,
        OUTPUT_EXE,
        OUTPUT_ELF,
        OUTPUT_LIB,
        OUTPUT_DLL,
        OUTPUT_BC,
    };
    OutputType output_type = OUTPUT_INVALID;

    bool obj_write_success = false;

    switch(options->target){
        case TARGET_BYTECODE: {
            arch = ARCH_x86_64;
        } break;
        case TARGET_WINDOWS_x64:
        case TARGET_LINUX_x64: {
            arch = ARCH_x86_64;
        } break;
        case TARGET_ARM: {
            arch = ARCH_arm;
        } break;
        case TARGET_AARCH64: {
            arch = ARCH_aarch64;
            log::out << log::RED << "ARM 64-bit not supported\n";
            return;
        } break;
        default: Assert(false);
    }

    std::string output_filename = output_path.substr(slash_index+1, dot_index - (slash_index+1));
    std::string output_extension = ExtractExtension(output_path);
    
    std::string object_path = "bin/" + output_filename + ".o";
    // std::string object_path = output_filename + ".o";
    std::string temp_path{};
    bool perform_copy = false;

    if(output_extension.size() != 0) {
        if(output_extension == ".o" || output_extension == ".obj") {
            output_type = OUTPUT_OBJ;
        } else if(output_extension == ".exe") {
            output_type = OUTPUT_EXE;
        } else if(output_extension == ".bc"){
            Assert(false);
            output_type = OUTPUT_BC;
        } else if(output_extension == ".dll" || output_extension == ".so") {
            output_type = OUTPUT_DLL;
            // NOTE: dll for windows, so for linux.
            //   We shouldn't allow the user to use .so on windows
        } else if(output_extension == ".lib" || output_extension == ".a") {
            output_type = OUTPUT_LIB;
            // NOTE: Linux uses libNAME.a but we just check .a, should we warn the user if they forgot lib at the start of the file?
        } else if(output_extension == ".elf") {
            output_type = OUTPUT_ELF;
            // NOTE: We assume the user intends to compile a "kernel" image for qemu.
        }
    } else {
        if(options->target == TARGET_LINUX_x64) {
            output_type = OUTPUT_EXE;
        }
    }
    if(output_type == OUTPUT_INVALID) {
        if(output_extension.size() == 0) {
            log::out << log::RED << "The output file '"<<options->output_file<<"' does not have a format. This is assumed to be an executable when targeting Linux but you are targeting "<<options->target<<"\n";
        } else {
            log::out << log::RED << "You specified '"<<output_extension<<"' as the file extension which the compiler does not support.\n";
        }
        log::out << log::GOLD << "These are the valid file extensions:\n";
        log::out << "  .obj .o  - object file\n";
        log::out << "  .exe     - executable\n";
        log::out << "  .dll .so - dynamic library\n";
        log::out << "  .lib .a  - static library\n";
        log::out << "  .bc      - bytecode\n";
        log::out << "  .elf     - kernel image (mainly meant for qemu)\n";
        return;
    }

    if(options->target == TARGET_ARM && output_type != OUTPUT_OBJ && output_type != OUTPUT_ELF && output_type != OUTPUT_BC) {
        log::out << log::RED << "The compiler can only generate "<<log::NO_COLOR<<".o .elf .bc "<<log::RED<<" when targeting ARM, not '"<<output_extension<<"'.\n";
        return;
    }
    if(options->target != TARGET_ARM && output_type == OUTPUT_ELF) {
        log::out << log::RED << "Compiler will only output an .elf file when targeting ARM. ELF extension is meant for kernel image. We may support kernel image for 'x86_64' target in the future.\n";
        return;
    }

    if (output_type == OUTPUT_DLL || output_type == OUTPUT_LIB) {
        // NOTE: No entry point for object files?
        entry_point = ""; // no entry point for dlls
    }

    importDirectories.add(options->modulesDirectory);
    
    preprocessor.init(&lexer, this);
    ast = AST::Create(this);
    bytecode = Bytecode::Create();
    bytecode->debugInformation = DebugInformation::Create(ast);
    reporter.lexer = &lexer;

    
    bytecode->target = options->target;
    bytecode->arch = arch;
    
    program = Program::Create();
    program->debugInformation = bytecode->debugInformation;
    
    if(options->source_buffer.buffer && options->source_file.size()) {
        log::out << log::RED << "A source buffer and source file was specified. You can only specify one\n";
        compile_stats.errors++;
        return;
    }
    
    bool skip_preload = false;
    skip_preload = options->disable_preload;
    // skip_preload = true;
    if(skip_preload) {
        log::out << log::YELLOW << "Preload is skipped!\n";
    }
    if(!skip_preload) {
        // should preload exist in modules?
        StringBuilder preload{};
        preload +=
        "struct Slice<T> {\n"
        // "struct @hide Slice<T> {"
        "    ptr: T*;\n"
        "    len: i64;\n"
        "}\n"
        "operator []<T>(slice: Slice<T>, index: i32) -> T {\n"
        "    return slice.ptr[index];\n"
        "}\n"
        "fn @compiler init_preload()\n" // init global data and stuff
        "fn @compiler global_slice() -> Slice<char>\n" // retrieves a slice of global data

        "struct Range {\n"
        // "struct @hide Range {" 
        "    beg: i32;\n"
        "    end: i32;\n"
        "}\n"
        "fn @intrinsic prints(str: char[])\n"
        "fn @intrinsic printc(str: char);\n"
        "fn @intrinsic rdtsc() -> i64;\n"
        "fn @intrinsic strlen(str: char*) -> i32;\n"

        // "#macro Assert(X) { prints(#quoted X); }"
        // "#macro Assert(X) { prints(#quoted X); *cast<char>null; }"
        ;
        if(options->target == TARGET_WINDOWS_x64) {
            preload += "#macro OS_WINDOWS #endmacro\n";
        } else if(options->target == TARGET_LINUX_x64) {
            preload += "#macro OS_LINUX #endmacro\n";
        } else if(options->target == TARGET_BYTECODE) {
            preload += "#macro OS_BYTECODE #endmacro\n";
        }
        if (output_type == OUTPUT_EXE) preload += "#macro BUILD_EXE #endmacro\n";
        if (output_type == OUTPUT_DLL) preload += "#macro BUILD_DLL #endmacro\n";
        if (output_type == OUTPUT_LIB) preload += "#macro BUILD_LIB #endmacro\n";
        if (output_type == OUTPUT_OBJ) preload += "#macro BUILD_OBJ #endmacro\n";
        if (output_type == OUTPUT_BC)  preload += "#macro BUILD_BC  #endmacro\n";
        if (options->execute_in_vm)    preload += "#macro BUILT_FOR_VM #endmacro\n";

        if(options->stable_global_data) {
            preload += "global @notstable stable_global_memory: void*;\n";
            preload += "#macro ENABLED_STABLE_GLOBALS #endmacro\n";
        }

        for(auto& s : options->defined_macros) {
            preload += "#macro " + s + " #endmacro\n";
        }
        
        if(options->linker == LINKER_MSVC) {
            preload += "#macro LINKER_MSVC #endmacro\n";
        } else if(options->linker == LINKER_GCC) {
            preload += "#macro LINKER_GCC #endmacro\n";
        }
        
        auto virtual_path = PRELOAD_NAME;
        lexer.createVirtualFile(virtual_path, &preload); // add before creating import
        preload_import_id = addOrFindImport(virtual_path);
        Assert(preload_import_id);
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
        compile_stats.errors++;
        return;
    }
    
    if(options->disable_multithreading)
        options->threadCount = 1;
    if(options->threadCount == 0) {
        options->threadCount = GetCPUCoreCount();
        // log::out << "Core count "<<options->threadCount<<"\n";
    }
    if(options->threadCount > 1) {
        log::out << log::YELLOW << "WARNING: Multithreading is enabled even though it's an unstable feature. Turn off with "<<log::LIME<<"-nothreads"<<log::NO_COLOR<<".\n";
    }

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

    if(output_is_up_to_date) {
        if(!options->silent) {
            // We finished, don't print anything?
            log::out << log::GOLD << "Output is up to date with the source files.\n";
            log::out << log::GOLD << "Stopping compilation.\n";
        }
        goto JUMP_TO_EXEC;
        // return;
    }
    
    switch(options->target) {
        case TARGET_BYTECODE: {
            // do nothing
        } break;
        case TARGET_WINDOWS_x64:
        case TARGET_LINUX_x64: {
            this->program->finalize_program(this);
        } break;
        case TARGET_ARM: {
            this->program->finalize_program(this);
        } break;
        default: Assert(false);
    }
    
    if(compile_stats.errors!=0){ 
        if(!options->silent)
            compile_stats.printFailed();
        return;
    }
    if(compile_stats.warnings!=0){
        if(!options->silent)
            compile_stats.printWarnings();
    }

    if(options->only_preprocess) {
        auto& imps = lexer.getImports();
        auto iter = imps.iterator();
        std::unordered_map<std::string,bool> printed_imps;

        while(imps.iterate_reverse(iter)) {
            auto pair = printed_imps.find(iter.ptr->path);
            if(pair == printed_imps.end()) {
                // print first occurrence of import (from preprocessor)
                // if(TrimDir(iter.ptr->path) == "macros.btb") { // TODO: Temporary if-statement
                    log::out << log::GOLD << iter.ptr->path<<"\n";
                    // log::out << log::GOLD << "pre: " << iter.ptr->path<<"\n";
                    lexer.print(iter.ptr->file_id);
                    log::out << "\n";
                // }
                printed_imps[iter.ptr->path] = true;
            } else {
                // log::out << log::GOLD << "lex: " << iter.ptr->path<<"\n";
                // lexer.print(iter.ptr->file_id);
                // log::out << "\n";
                // skip second occurrence of import (from lexer)
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
            obj_write_success = ObjectFile::WriteFile(OBJ_COFF, object_path, program, this);
            compile_stats.generatedFiles.add(object_path);
        } break;
        case TARGET_LINUX_x64: {
            obj_write_success = ObjectFile::WriteFile(OBJ_ELF, object_path, program, this);
            compile_stats.generatedFiles.add(object_path);
        } break;
        case TARGET_ARM: {
            obj_write_success = ObjectFile::WriteFile(OBJ_ELF, object_path, program, this);
            compile_stats.generatedFiles.add(object_path);
        } break;
        default: Assert(false);
    }
    {
        int at = output_path.find_last_of("/");
        if(at != -1) {
            std::string output_dir = output_path.substr(0,at);
            DirectoryCreate(output_dir);
        }
    }
    
    if(!obj_write_success) {
        // error message should have been printed.
        // TODO: We should return error codes and print messages here. the ObjectFile functions shouldn't decide how and what we print.
        compile_stats.errors++;
        return;
        // log::out << log::RED << "Could not write object file '"<<object_path<<"'. Perhaps a bad path, perhaps due to compilation error?\n";
    } else if((output_type == OUTPUT_EXE || output_type == OUTPUT_DLL || output_type == OUTPUT_LIB) && options->target != TARGET_BYTECODE) {
        bool has_winmain = false;
        if (output_type == OUTPUT_EXE) {
            // dll doesn't care about entry point
            // well, it has it's own entry point on windows
            if (options->target == TARGET_WINDOWS_x64 && entry_point == "WinMain") {
                for(auto it : bytecode->tinyBytecodes) {
                    // log::out  << it->name << "\n";
                    if (it->name == "WinMain") {
                        has_winmain = true;
                        break;
                    }
                }
            }
        }
        std::string cmd = "";
        bool outputOtherDirectory = false;
        switch(options->linker) {
        case LINKER_MSVC: {
            Assert(options->target == TARGET_WINDOWS_x64);
            
            outputOtherDirectory = options->output_file.find("/") != std::string::npos;

            if(options->useDebugInformation) {
                // TODO: TEMPORARY text
                log::out << log::GOLD << "Debug information (DWARF) cannot be linked using the MSVC linker. Plrease choose a different linker like GNU/g++ using the flag "<<log::LIME<<"--linker gnu"<<log::GOLD<<" (make sure to have gcc installed).\n";
                // The compiler does not support PDB debug information, it only supports DWARF. DWARF
                // uses sections with long names but MSVC linker truncates those names.
                // That's why you cannot use MVSC linker.
            }

            if(output_type == OUTPUT_LIB) {
                cmd = "lib /nologo ";
            } else {
                cmd = "link /nologo /INCREMENTAL:NO ";
                if(options->useDebugInformation)
                    cmd += "/DEBUG ";

                if (output_type == OUTPUT_EXE) {
                    if (has_winmain)
                        cmd += "/SUBSYSTEM:WINDOWS ";
                    else {
                        cmd += "/entry:" + entry_point + " ";
                    }
                } else if (output_type == OUTPUT_DLL) {
                    cmd += "/DLL ";
                } else Assert(false);
                // turn off because parsing command line arguments must be done manually when using our own entry point
                // cmd += "/NODEFAULTLIB "; // entry point must be set manually with NODEFAULTLIB
                // cmd += "/ENTRY:main ";
                // this runtime instead
                cmd += "/DEFAULTLIB:MSVCRT ";
            }
            if (output_type == OUTPUT_DLL && bytecode->exportedFunctions.size() > 0) {
                // When exporting dll we need to specify which functions are exported.
                // We can do this on the command line using /EXPORT but it won't work with lots of exports.
                // We therefore create a .def file which contains all exports.
                std::string def_path = "bin/exports.def";
                cmd += "/DEF:" + def_path + " ";
                int dot_index = output_path.find_last_of(".");
                Assert(dot_index != -1);
                std::string implib_path = output_path.substr(0,dot_index) + "dll.lib";
                
                // When linking with dlls at load-time we can't link with .dll when linking executable.
                // We must use an import library that contains exports and dll name at compile-time (link time).
                // Then at run time, the actual .dll will be linked.
                // I would like to add that GCC is not this complex.
                cmd += "/IMPLIB:" + implib_path + " ";
                auto file = FileOpen(def_path, FILE_CLEAR_AND_WRITE);
                std::string content="";
                std::string dll_name = output_filename;
                for (int i=0;i<dll_name.size();i++) {
                    ((char*)dll_name.data())[i] &= ~32; // swap to upper case
                }

                content += "LIBRARY   " + dll_name + "\n";
                content += "EXPORTS\n";
                for (auto& f : bytecode->exportedFunctions) {
                    content += std::string("   ") + f.name + "\n";
                }

                FileWrite(file, content.data(),content.size());
                FileClose(file);
            }

            cmd += object_path + " ";
            // TODO: Don't link with default libraries. Try to get the executable as small as possible.
            
            // cmd += "/DEFAULTLIB:LIBCMT ";
            cmd += "Kernel32.lib "; // _test and prints uses WriteFile so we must link with kernel32
            
            for (int i = 0;i<(int)linkDirectives.size();i++) {
                auto& dir = linkDirectives[i];
                cmd += dir + " ";
            }
            for(auto& path : program->libraries) {
                std::string p = path;
                if (p.find("./") == 0)
                    p = p.substr(2);
                if (p.find(".dll") == p.size() - 4) {
                    // we can't link with dlls at compile-time
                    // we must link with the corresponding import library.
                    int dot_index = p.find_last_of(".");
                    Assert(dot_index != -1);
                    std::string implib_path = p.substr(0,dot_index) + "dll.lib";
                    cmd += implib_path + " ";
                } else {
                    cmd += p + " ";
                }
            }
            #define LINK_TEMP_EXE "temp_382.exe"
            #define LINK_TEMP_DLL "temp_382.dll"
            #define LINK_TEMP_LIB "temp_382.lib"
            #define LINK_TEMP_PDB "temp_382.pdb"
            if(outputOtherDirectory) {
                perform_copy = true;
                // MSVC linker cannot output to another directory than the current.
                if (output_type == OUTPUT_EXE) {
                    temp_path = LINK_TEMP_EXE;
                } else if(output_type == OUTPUT_DLL) {
                    temp_path = LINK_TEMP_DLL;
                } else if(output_type == OUTPUT_LIB) {
                    temp_path = LINK_TEMP_LIB;
                }
                cmd += "/OUT:" + temp_path;
            } else {
                // dll, lib or exe can all use /OUT
                cmd += "/OUT:" + output_path+" ";
            }
        } break;
        case LINKER_CLANG:
        case LINKER_GCC: {
            // TODO: If on Linux, we need to provide paths to shared libraries somehow. How does linux know where to
            //   find them. We can't just put them in /usr/lib. We can't just modify LD_LIBRARY_PATH because user may
            //   compile restart the terminal (resetting variable) and then run executable which will fail cause it can't find libraries.
            
            if(output_type == OUTPUT_LIB) {
                cmd += "ar rcs ";
                cmd += output_path + " ";
            } else {
                switch(options->linker) {
                    case LINKER_GCC: cmd += "gcc "; break;
                    case LINKER_CLANG: cmd += "clang "; break;
                    default: break;
                }
                if(options->useDebugInformation)
                    cmd += "-g ";

                // TODO: Don't link with default libraries. Try to get the executable as small as possible.
                
                if (output_type == OUTPUT_EXE) {
                    if(!force_default_entry_point) {
                        if(options->target == TARGET_WINDOWS_x64) {
                            // Always use cruntime entry point on Windows because
                            // parsing command line arguments is though.
                            // TODO: Look into manually parsing it. Maybe an entry.btb file with parsing code and other global initialization?
                        } else {
                            // IMPORTANT: 'aligned_16_byte_on_entry_point' needs to be set when using nostdlib!
                            // cmd += "-nostdlib ";
                            // if (entry_point == "main")
                            //     cmd += "--entry main "; // we must explicitly set entry point with nodstdlib even if main is used
                        }
                    }
                    
                    if (entry_point != "main") // no need to set entry if main is used since it is the default
                        cmd += "--entry " + entry_point + " ";
                    
                    if (has_winmain) {
                        cmd += "-Wl,-subsystem,windows "; // https://stackoverflow.com/questions/73676692/what-do-the-subsystem-windows-options-do-in-gcc
                    }
                } else if(output_type == OUTPUT_DLL) {
                    // TODO: Add -nostdlib?
                    // cmd += "-shared ";
                    cmd += "-shared -fPIC ";
                } else Assert(false);

                // cmd += "-ffreestanding "; // these do not make a difference (with mingw on windows at least)
                // cmd += "-Os ";
                // cmd += "-nostartfiles ";
                // cmd += "-nodefaultlibs ";
                // cmd += "-s ";
            }

            cmd += object_path + " ";

            if(options->target == TARGET_WINDOWS_x64) {
                bool has_kernel = false;
                for(auto& path : program->libraries) {
                    if (path == "Kernel32.lib" || path == "Kernel32.dll" || path == "Kernel32") {
                        has_kernel = true;
                        break;
                    }
                }
                if(!has_kernel) {
                    cmd += "-lKernel32 "; // _test and prints uses WriteFile so we must link with kernel32
                }
            } else if(options->target == TARGET_LINUX_x64) {
                // cmd += "-lc "; // link clib because it has wrappers for syscalls, NOTE: We actually don't need this, we use syscalls in assembly.
            }
            
            for (int i = 0;i<(int)linkDirectives.size();i++) {
                auto& dir = linkDirectives[i];
                cmd += dir + " ";
            }

            // TODO: We need to improve the way we find libraries.
            //   For system libraries -lKernel32 should be used.
            //   For relative user libraries libs/glad/glad.dll should be used, no -L or -l.
            //   How does /load indicate this though?

            DynamicArray<std::string> dynamic_libs{};
            for(auto& path : program->libraries) {
                if (path.find("/") != -1) {
                    // User library
                    std::string good_path = path;
                    if (good_path.find("./") == 0)
                        good_path = good_path.substr(2);
                    cmd += good_path + " ";
                    if (good_path.find(".dll") == good_path.size() - 4 || good_path.find(".so") == good_path.size() - 3) {
                        dynamic_libs.add(good_path);
                    }
                } else {
                    // System library
                    std::string file = path;
                    if(options->target == TARGET_WINDOWS_x64) {
                        int pos = file.find_last_of(".");
                        if(pos != file.npos)
                            file = file.substr(0,file.find_last_of("."));
                    } else {
                        int pos = file.find(".");
                        if(file.size() > 3 && file.substr(0,3) == "lib" && pos != -1) {
                            file = file.substr(3, pos-3);
                        }
                    }
                    if(file != "")
                        cmd += "-l" + file + " ";
                }
            }
            std::string output_dir = TrimLastFile(options->output_file);
            if (output_dir == "/")
                output_dir = "";
            for(auto& path : dynamic_libs) {
                std::string file = TrimDir(path);
                std::string new_path = output_dir + file;

                if (path != new_path) {
                    // log::out << "copy " << path << " -> " << new_path << "\n";
                    FileCopy(path, new_path, false); // we copy even if file exists since the user may have compiled the dll and so we should update it
                    // we can check last modified time to avoid copy if nothing has changed
                }
            }


            // ar for static library uses output file as first/second argument.
            // linking/compiling dll or exe can use -o
            if(output_type != OUTPUT_LIB)
                cmd += "-o " + output_path;
        } break;
        default: {
            Assert(false);
            break;
        }
        }

        int exitCode = 0;
        bool failed = false;
        {
            ZoneNamedNC(zone0,"Linker",tracy::Color::Blue2, true);
            
            if(!options->silent)
                log::out << log::LIME<<"Linker command: "<<cmd<<"\n";
            // engone::StartProgram((char*)cmd.c_str(),PROGRAM_WAIT, &exitCode, {}, linkerLog, linkerLog);
            
            compile_stats.start_linker = engone::StartMeasure();
            bool yes = engone::StartProgram((char*)cmd.c_str(),PROGRAM_WAIT, &exitCode);
            if(yes && perform_copy) {
                bool success = FileCopy(temp_path, options->output_file);
                if(!success) {
                    log::out << log::RED << "Could not copy '"<<temp_path<<"' to '"<<options->output_file<<"'\n";
                }
            }
            failed = !yes;
            compile_stats.end_linker = engone::StartMeasure();
            compile_stats.generatedFiles.add(output_path);
        }
        compile_stats.end_linker = StartMeasure();
        if(exitCode != 0 || failed) {
            log::out << log::RED << "Linker failed: '"<<cmd<<"'\n";
            compile_stats.errors++;
            return;
        }    
    } else if (output_type == OUTPUT_ELF){
        if(program->libraries.size() > 0) {
            // TODO: Better error messages that shows where libraries came from.
            log::out << log::RED << "Libraries cannot be linked when compiling for ARM." << log::NO_COLOR;
            for(auto& path : program->libraries) {
                log::out << path<<" ";
            }
            log::out << "\n";
            compile_stats.errors++;
            return;
        }
        
         // TODO: Compiler option for arm linker, what about clang?
        std::string toolchain = "arm-none-eabi";
        std::string as = toolchain+"-as";
        std::string linker = toolchain+"-ld";
        
        std::string cmd = "";
        cmd += linker + " ";
        
        if(options->useDebugInformation)
            cmd += "-g ";
        cmd += object_path + " ";
        
        const char* startup = 
            ".global start\n"
            "start:\n"
            "    LDR sp, =_stack_top\n"
            "    BL main\n"
            "    # angel_SWIreason_ReportException\n"
            "    mov r0, #0x18\n"
            "\n"
            "    # mov r1, #20026 # ADP_Stopped_ApplicationExit\n"
            "    mov r1, #0x2\n"
            "    lsl r1, r1, #16\n"
            "    orr r1, r1, #0x26\n"
            "    # interrupt\n"
            "    svc 0x00123456\n"
            "\n"
            "    # infinite loop if interrupt didn't work\n"
            "    B .\n";
        const char* linker_script = 
            "ENTRY(start)\n"
            // "MEMORY {"
            // "   FLASH (rx) : ORIGIN = 0x00010000, LENGTH = 512K "
            // "   RAM (rw) : ORIGIN = 0x0x "
            // "}"
            "SECTIONS {\n"
            "    . = 0x10000;"
            "    .text : {\n"
            "        bin/arm_startup.o(.text)\n"
            "        *(.text)\n"
            "    }\n"
            "    .data : { *(.data) }\n"
            "    .bss : { *(.bss COMMON) }\n"
            "    . = ALIGN(8);\n"
            "    _stack_top = . + 0x10000; /* 64kB of stack memory */\n"
            "}\n";
            
        auto file_startup = FileOpen("bin/arm_startup.s", FILE_CLEAR_AND_WRITE);
        FileWrite(file_startup, startup, strlen(startup));
        FileClose(file_startup);
        
        std::string cmd_startup = as + " bin/arm_startup.s -o bin/arm_startup.o"; // NOTE: linker script needs to know path of arm_startup.o
        if(options->useDebugInformation)
            cmd_startup += " -g";
        int as_exit_code=0;
        bool yes = StartProgram(cmd_startup.c_str(), PROGRAM_WAIT, &as_exit_code);
        if(!yes || as_exit_code != 0) {
            log::out << log::RED << "Assembler failed: '"<<cmd_startup<<"'\n";
            compile_stats.errors++;
            return;
        }
        
        auto file_lscript = FileOpen("bin/arm_lscript.ld", FILE_CLEAR_AND_WRITE);
        FileWrite(file_lscript, linker_script, strlen(linker_script));
        FileClose(file_lscript);
        
        cmd += " bin/arm_startup.o -T bin/arm_lscript.ld";
        
        for (int i = 0;i<(int)linkDirectives.size();i++) {
            auto& dir = linkDirectives[i];
            cmd += " " + dir;
        }
        cmd += " -o " + output_path;
        int exitCode = 0;
        bool failed = false;
        {
            ZoneNamedNC(zone0,"Linker",tracy::Color::Blue2, true);
            
            if(!options->silent)
                log::out << log::LIME<<"Linker command: "<<cmd<<"\n";
            // engone::StartProgram((char*)cmd.c_str(),PROGRAM_WAIT, &exitCode, {}, linkerLog, linkerLog);
            
            compile_stats.start_linker = engone::StartMeasure();
            bool yes = engone::StartProgram((char*)cmd.c_str(),PROGRAM_WAIT, &exitCode);
            if(yes && perform_copy) {
                bool success = FileCopy(temp_path, options->output_file);
                if(!success) {
                    log::out << log::RED << "Could not copy '"<<temp_path<<"' to '"<<options->output_file<<"'\n";
                }
            }
            failed = !yes;
            compile_stats.end_linker = engone::StartMeasure();
            compile_stats.generatedFiles.add(output_path);
        }
        compile_stats.end_linker = StartMeasure();
        if(exitCode != 0 || failed) {
            log::out << log::RED << "Linker failed: '"<<cmd<<"'\n";
            compile_stats.errors++;
            return;
        }
    } else {
        // TODO: If bytecode is the target and the user specified
        //   app.bc as outputfile should we write out a bytecode file format?
    }
    
    if(output_type == OUTPUT_DLL || output_type == OUTPUT_LIB || output_type == OUTPUT_OBJ) {
        WriteDeclFiles(options->output_file, bytecode, ast, output_type == OUTPUT_DLL);
    }
    
    compile_stats.end_compile = engone::StartMeasure();
    
    if(compile_stats.errors==0) {
        if(!options->silent) {
            compile_stats.printSuccess(options);
        }
    }
    
    // We dump while compiling, sometimes we want to know bytecode because x64_gen crashes. We won't see dump if we print it out at the end.
    // if(bytecode->debugDumps.size() != 0) {
    //     for(int i=0;i<(int)bytecode->debugDumps.size();i++) {
    //         auto& dump = bytecode->debugDumps[i];
    //         if(dump.dumpBytecode) {
    //             if(dump.description.empty()) {
    //                 log::out << "Dump:\n";
    //             } else {
    //                 log::out << "Dump: "<<log::GOLD<<dump.description<<"\n";
    //             }
    //             auto tinycode = bytecode->tinyBytecodes[dump.tinycode_index];
    //             tinycode->print(dump.bc_startIndex, dump.bc_endIndex, bytecode);
    //         }
    //         if (dump.dumpAsm) {
    //             // TODO: Write to object file, then run disassembler?
    //         }
    //     }
    // }
JUMP_TO_EXEC:
    
    if(compile_stats.errors != 0) {
        return;   
    }
    if(options->execute_in_vm)  {
        VirtualMachine vm{};
        vm.execute(bytecode, entry_point, false, options);
        return;
    } 
    if(options->executeOutput && (output_type == OUTPUT_EXE || output_type == OUTPUT_ELF)) {
        switch(options->target) {
            case TARGET_WINDOWS_x64: {
                #ifdef OS_WINDOWS
                int exitCode;

                std::string cmd = output_path;
                cmd = ".\\" + cmd; // TODO: Assumes exe path is relative, not good
                // We need it because when we start program windows will look for executable
                // in the current processe's executable directory instead of the
                // current working directory. relative path ensures we always use current working directory.
                // This matters if you call compiler like this: ..\BetterThanBatch\bin\btb.exe src/main -r
                
                ReplaceChar((char*)cmd.data(), cmd.size(), '/', '\\');
                for (auto& a : options->userArguments) {
                    cmd += " " + a;
                }
                
                // log::out << "CWD: " << GetWorkingDirectory()<<"\n";

                log::out << log::GRAY << "running: " << cmd<<"\n";
                engone::StartProgram(cmd.c_str(),PROGRAM_WAIT, &exitCode);
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
            case TARGET_LINUX_x64: {
                #ifdef OS_LINUX
                int exitCode;
                bool some_crash = false;
                std::string cmd = "./"+output_path; // TODO: Assumes exe path is relative, not good

                for (auto& a : options->userArguments) {
                    cmd += " " + a;
                }

                engone::StartProgram(cmd.c_str(),PROGRAM_WAIT, &exitCode, &some_crash);
                if(some_crash) {
                    log::out << log::RED <<"Crash, exit code: " << exitCode << "\n";
                } else {
                    log::out << log::LIME <<"Exit code: " << exitCode << "\n";
                }
                #else
                log::out << log::RED << "You cannot run a Unix program on Windows. Consider changing target when compiling (--target win-x64)\n";
                #endif
            } break;
            case TARGET_BYTECODE: {
                log::out << log::RED << "Bytecode as target is not supported. Use --run-vm to execute in virtual machine.\n";
            } break;
            case TARGET_ARM: {
                // TODO: Don't hardcode processor for QEMU.
                std::string cmd = "qemu-system-arm "
                    "-semihosting -nographic -serial mon:stdio -M "
                    "xilinx-zynq-a9 -cpu cortex-a9 ";
                cmd += "-kernel " + output_path;
                
                if(options->debug_qemu_with_gdb) {
                    cmd += " -S -gdb tcp::"+options->qemu_gdb_port;
                }
                
                ReplaceChar((char*)cmd.data(), cmd.size(), '/', '\\');
                for (auto& a : options->userArguments) {
                    cmd += " " + a;
                }
                
                // StartProgram("arm-none-eabi-objdump bin/main.o -W", PROGRAM_WAIT);
                
                log::out << log::GRAY << "running: " << cmd<<"\n";
                log::out << log::GRAY << "Ctrl+A <release> X (to exit QEMU)\n";
                int exitCode;
                bool yes = engone::StartProgram(cmd.c_str(),PROGRAM_WAIT, &exitCode);
                if(!yes) {
                    log::out << log::RED << "Could not run: " << cmd<<"\n";
                }
                log::out << log::LIME <<"Exit code from QEMU: " << exitCode << "\n";
            } break;
            default: Assert(false);
        }
        return;
    }
    if(!options->silent)
        log::out << log::GRAY << "not executing program\n";
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
void Compiler::addDependency(u32 import_id, u32 dep_import_id, const std::string& as_name, bool disabled) {
    lock_imports.lock();
    auto imp = imports.get(import_id-1);
    Assert(imp);
    bool found = false;
    for(int i=0;i<imp->dependencies.size();i++) {
        // TODO: Multiple imports, named differently won't work.
        if(imp->dependencies[i].id == dep_import_id) {
            found = true;
            imp->dependencies[i].disabled = disabled; // update
            break;
        }
    }
    if(!found)
        imp->dependencies.add({dep_import_id, as_name, disabled});

    lock_imports.unlock();
    // LOG(CAT_PROCESSING,engone::log::CYAN<<"Add depend: "<<import_id<<"->"<<dep_import_id<<" ("<<TrimCWD(imp->path)<<")\n")
}
void Compiler::addTask_type_body(ASTFunction* ast_func, FuncImpl* func_impl) {
    using namespace engone;
    // log::out << "add "<<ast_func->name<<"\n";
    lock_imports.lock();
    defer { lock_imports.unlock(); };
    CompilerTask picked_task{};
    picked_task.type = TASK_TYPE_BODY;
    // function has the preprocessed import but the tasks always assumes the original file import.
    picked_task.import_id = get_map_id(ast_func->getImportId(&lexer));
    picked_task.astFunc = ast_func;
    picked_task.funcImpl = func_impl;
    tasks.add(picked_task); // TODO: lock tasks
}
void Compiler::addTask_type_body(u32 import_id) {
    lock_imports.lock();
    defer { lock_imports.unlock(); };
    CompilerTask picked_task{};
    picked_task.type = TASK_TYPE_BODY;
    picked_task.import_id = import_id;
    tasks.add(picked_task); // TODO: lock tasks
}
void Compiler::addLibrary(u32 import_id, const std::string& path, const std::string& as_name) {
    using namespace engone;
    if(options->target == TARGET_LINUX_x64) {
        // TODO: When on Windows, use default entry point if a C runtime was specified.
        if(path.find("libc") != -1) { // libc
            if(has_generated_entry_point) {
                // TODO: Improve error message, although it shouldn't happen.
                log::out << log::RED << "COMPILER BUG: "<<log::NO_COLOR <<"When using libc, your program should no longer be the entry point. The libc's entry point should be used instead (things might break otherwise). However, the entry point was generated before libc library was detected. This should not happen, contact developer for a quick fix. (path: '"<<path<<"')\n";
            }
            force_default_entry_point = true;
        }
    }
    lock_imports.lock();
    auto imp = imports.get(import_id-1);
    Assert(imp);
    // imports.requestSpot(dep_import_id-1,nullptr);
    bool found = false;
    if(as_name.size() != 0) {
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
                // TODO: Improve error message, which source file the library came from
                compile_stats.errors++;
                LOG_MSG_LOCATION
                log::out << log::RED << "Multiple libraries cannot be named the same ("<<as_name<<").\n";
                log::out << log::RED << " \""<<BriefString(path)<<"\" collides with \""<<lib.path<<"\"\n";
                break;
            }
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
    #if OS_LINUX
    if(fullPath.text.size() > 0 && fullPath.text[0] == '~') {
        std::string value = GetEnvVar("HOME");
        if(value.size() != 0) {
            // log::out << "HOME="<<value<<"\n";
            // check traling slashes, join them, make sure value and path is separated by ONE slash
            fullPath.text = std::string(value) + fullPath.text.substr(1);
        }
    }
    #endif

    bool keep_searching = true;

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
    // if(keep_searching){
    //     if(engone::FileExist(fullPath.text)){
    //         // if(!fullPath.isAbsolute())
    //         fullPath = fullPath.getAbsolute();
    //     }
    // }
    if (keep_searching) {
        Path temp{}; // = sourceDirectory.text;
        // if(!sourceDirectory.text.empty() && sourceDirectory.text[sourceDirectory.text.size()-1]!='/')
        //     temp.text += "/";
        // temp.text += fullPath.text;
        for(int i=0;i<(int)importDirectories.size();i++){
            const Path& dir = importDirectories[i];
            Assert(dir.isDir() && dir.isAbsolute());
            if(dir.text.size()>0 && dir.text[dir.text.size()-1] == '/')
                temp = dir.text + fullPath.text;
            else
                temp = dir.text + "/" + fullPath.text;

            if(FileExist(temp.text)) {
                fullPath = temp.getAbsolute();
                keep_searching = false;
                break;
            }
        }
    }
    if (keep_searching) {
        if(engone::FileExist(fullPath.text)){
            fullPath = fullPath.getAbsolute();
            keep_searching = false;
        }
    }
    if (keep_searching) {
        Path temp = sourceDirectory.text;
        if(!sourceDirectory.text.empty() && sourceDirectory.text[sourceDirectory.text.size()-1]!='/')
            temp.text += "/";
        temp.text += fullPath.text;
        if(FileExist(temp.text)){ // search directory of current source file again but implicit ./
            fullPath = temp.getAbsolute();
            keep_searching = false;
        }
    }
    if(keep_searching) {
        fullPath = ""; // failure, file not found
    }
    return fullPath;
}

void Compiler::addError(const lexer::SourceLocation& loc, CompileError errorType) {
    u32 cindex,tindex;
    lexer.decode_origin(loc.tok.origin, &cindex, &tindex);

    auto chunk = lexer.getChunk_unsafe(cindex);
    lexer::TokenInfo* info=nullptr;
    lexer::TokenSource* src=nullptr;
    if(loc.tok.type == lexer::TOKEN_EOF) {
        info = &chunk->tokens[tindex-1];
        src = &chunk->sources[tindex-1];
    } else {
        info = &chunk->tokens[tindex];
        src = &chunk->sources[tindex];
    }

    // auto src = lexer.getTokenSource_unsafe(location); // returns null if EOF
    errorTypes.add({errorType, (u32)src->line});
}
void Compiler::addError(const lexer::Token& token, CompileError errorType) {
    addError(lexer::SourceLocation{token}, errorType);
}
CompilerImport* Compiler::getImport(u32 import_id){
    u32 id = get_map_id(import_id);
    lock_imports.lock();
    auto imp = imports.get(id-1);
    lock_imports.unlock();
    return imp;
}
double Compiler::compute_last_modified_time() {
    using namespace engone;
    auto iter = imports.iterator();
    double time = 0.0;
    while (imports.iterate(iter)) {
        auto it = iter.ptr;
        if (it->preproc_import_id == 0)
            continue;

        auto imp = lexer.getImport_unsafe(it->import_id-1);
        // preload doesn't have import, imp will be null then
        if (imp) {
            // log::out << imp->path << " " << imp->last_modified_time<<"\n";
            if (imp->last_modified_time > time) {
                time = imp->last_modified_time;
            }
        }
    }
    return time;
}

bool Compiler::is_msvc_configured() {
    using namespace engone;
    // TODO: Checking for HostX64 which is the path where link.exe should exist
    //   Is not a great idea. What if another application uses the same path structure?
    std::string env_path = GetEnvVar("PATH");
    bool is_configured = env_path.find("\\bin\\HostX64\\x64") != -1;
    return is_configured;
}
bool Compiler::configure_msvc() {
    using namespace engone;
    #ifndef OS_WINDOWS
    return false;
    #endif
    
    bool log_versions = false;
    
    auto find_first_folder = [&](const std::string& path) {
        // TODO: Pick the latest version?
        std::string name = "";
        auto iter = DirectoryIteratorCreate(path.c_str(), path.size());
        DirectoryIteratorData data;
        while(DirectoryIteratorNext(iter, &data)) {
            if(!data.isDirectory)
                continue; // don't care about files
            
            std::string dir_path = std::string(data.name,data.namelen);
            int at = dir_path.find_last_of("/");
            if(at != -1) {
                name = dir_path.substr(at+1);
                break;
            }
            DirectoryIteratorSkip(iter);
        }
        DirectoryIteratorDestroy(iter, &data);
        return name;
    };
    
    // Find Visual Studio toolchain versions
    // std::string version_vs = "2022";
    // std::string version_msvc = "14.33.31629";
    // std::string version_kits = "10.0.20348.0";
    
    std::string base_vs = "C:\\Program Files\\Microsoft Visual Studio";
    std::string base_kits = "C:\\Program Files (x86)\\Windows Kits\\10\\";
    
    std::string version_vs = find_first_folder(base_vs);
    if(log_versions)
        log::out << "Found VS version: "<<version_vs<<"\n";
    if(version_vs.size() == 0) {
        return false;
    }
    
    std::string base_tools_nov = base_vs + "\\" + version_vs + "\\Community\\VC\\Tools\\MSVC";
    std::string version_msvc = find_first_folder(base_tools_nov);
    if(log_versions)
        log::out << "Found MSVC version: "<<version_msvc<<"\n";
    if(version_msvc.size() == 0) {
        return false;
    }
    std::string base_tools = base_tools_nov + "\\" + version_msvc;
    
    std::string version_kits = find_first_folder(base_kits + "\\include");
    if(log_versions)
        log::out << "Found kits version: "<<version_kits<<"\n";
    if(version_kits.size() == 0) {
        return false;
    }
    std::string base_kits_include = base_kits + "\\include\\" + version_kits;
    std::string base_kits_lib = base_kits + "\\lib\\" + version_kits;
    
    // Calculate paths
    std::string add_PATH = ";"+base_tools + "\\bin\\HostX64\\x64;";

    std::string add_LIBPATH = ";"+base_tools + "\\lib\\x64;";

    std::string add_LIB = ";"+base_kits_lib + "\\ucrt\\x64;"
        + base_kits_lib + "\\um\\x64;"
        + base_tools + "\\lib\\x64;";

    std::string add_INCLUDE = ";"+base_tools+"\\include;"
        + base_kits_include+"\\ucrt;"
        + base_kits_include+"\\um;"
        + base_kits_include+"\\shared;"
        + base_kits_include+"\\winrt;"
        + base_kits_include+"\\cppwinrt;";
    
    // Set environment paths
    add_PATH = GetEnvVar("PATH") + add_PATH;
    add_LIBPATH = GetEnvVar("LIBPATH") + add_LIBPATH;
    add_LIB = GetEnvVar("LIB") + add_LIB;
    add_INCLUDE = GetEnvVar("INCLUDE") + add_INCLUDE;
    SetEnvVar("PATH", add_PATH);
    SetEnvVar("LIBPATH", add_LIBPATH);
    SetEnvVar("LIB", add_LIB);
    SetEnvVar("INCLUDE", add_INCLUDE);
    
    // TODO: Check if linker works?
    
    return true;
}

const char* annotation_names[]{
    "unknown",
    "custom",
};
const char* const PRELOAD_NAME = "<preload>";
const char* const TYPEINFO_NAME = "Lang.btb";