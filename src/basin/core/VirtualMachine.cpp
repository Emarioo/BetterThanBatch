#include "basin/core/VirtualMachine.h"

// needed for FRAME_SIZE
#include "basin/core/Generator.h"
#include "basin/Compiler.h"
#include "basin/CompilerInterface.h"

#include <iostream>

// #define BITS(P,B,E,S) ((P<<(S-E))>>B)

// #define DECODE_OPCODE(I) I->opcode
// #define DECODE_REG0(I) I->op0
// #define DECODE_REG1(I) I->op1
// #define DECODE_REG2(I) I->op2

// #define SP_CHANGE(incr) log::out << "sp "<<(i64)(sp-(u64)stack.data-(incr))<<" -> "<<(i64)(sp-(u64)stack.data)<<"\n";
#define SP_CHANGE(incr)

// #define DECODE_TYPE(ptr) (*((u8*)ptr+1))

void VirtualMachine::cleanup(){
    stack.resize(0);
    states.cleanup();
    reset();
    for(auto& f : bytecode_pointers) {
        if(f.ptr)
            engone::FreeExec(f.ptr, f.size);
    }
    bytecode_pointers.cleanup();
    // cmdArgsBuffer.resize(0);
    // engone::Free(cmdArgs.ptr, sizeof(Language::Slice<char>)*cmdArgs.len);
    // cmdArgs.ptr = nullptr;
    // cmdArgs.len = 0;
}
void VirtualMachine::reset(){
    states.resize(0);
    memset(stack.data(), 0, stack.max);
}
void PrintPointer(volatile void* ptr){
    u64 v = (u64)ptr;
    engone::log::out << "0x";
    bool zeros = true;
    for(int i=60;i>=0;i-=4){
        u64 n = (v >> i) & 0xF;
        if(n == 0 && zeros)
            continue;
        zeros = false;
        char chr = n < 10 ? n + '0' : n - 10 + 'A';
        engone::log::out << chr; 
    }
}
void VirtualMachine::init_stack(int stack_size) {
    stack.resize(stack_size);
    memset(stack.data(), 0, stack.max);
    // memset((void*)registers, 0, sizeof(registers));
}
TinyBytecode* VirtualMachine::fetch_tinycode(Bytecode* bytecode, const std::string& tinycode_name){
    for(int i=0;i<bytecode->tinyBytecodes.size();i++) {
        if(bytecode->tinyBytecodes[i]->name == tinycode_name) {
            return bytecode->tinyBytecodes[i];
        }
    }
    return nullptr;
}

int LevenshteinDistance(const std::string& w0, const std::string& w1) {
    // https://en.wikipedia.org/wiki/Levenshtein_distance#Iterative_with_two_matrix_rows
    
    int m = w0.size();
    int n = w1.size();
    
    // malloc memory once and reuse it.
    int* base = (int*)malloc(2 * (4 * (n+1)));
    int* v0 = base;
    int* v1 = base + n+1;
    
    for (int i=0;i<n+1;i++)
        v0[i] = i;

    for (int i=0;i<m;i++) {
        v1[0] = i+1;
        
        for (int j=0;j<n;j++) {
            int dcost = v0[j+1] + 1;
            int icost = v1[j] + 1;
            int scost = v0[j];
            if (w0[i] != w1[i])
                scost++;
            v1[j+1] = dcost < icost ? (dcost < scost ? dcost : scost) : (icost < scost ? icost : scost);
        }
        int* tmp = v0;
        v0 = v1;
        v1 = tmp;
    }

    int score = v0[n];
    free(base);
    return score;
}

void VirtualMachine::execute(Bytecode* bytecode, const std::string& tinycode_name, bool apply_related_relocations, CompileOptions* options){
    using namespace engone;
    this->bytecode = bytecode;
    this->options = options;
    // IMPORTANT: Put variables in VM struct as fields since local variables
    //   won't be accessible when calling a bytecode function is passed as
    //   function pointer to C function.

    TRACE_FUNC()

    Assert(bytecode);

    TinyBytecode* tinycode = nullptr;
    int tiny_index = -1;

    for(int i=0;i<bytecode->tinyBytecodes.size();i++) {
        if(bytecode->tinyBytecodes[i]->name == tinycode_name) {
            tinycode = bytecode->tinyBytecodes[i];
            tiny_index = i;
            break;
        }
    }
    if(!tinycode) {
        log::out << log::RED << "Tinycode " << tinycode_name << " not found\n";
        return;
    }

    REGISTER_SIZE = bytecode->arch.REGISTER_SIZE;
    FRAME_SIZE = bytecode->arch.FRAME_SIZE;

    // TODO: Appling partial relocations and checking for dependencies (other tinycodes)
    //   may be expensive. Avoiding partial relocation might be best.
    //   Also, if relocations have been applied once, we don't need to do so again.
    checked_codes.resize(0);
    codes_to_check.resize(0);

    codes_to_check.add(tinycode);
    checked_codes.add(tinycode);
    while(codes_to_check.size()) {
        auto t = codes_to_check[0];
        codes_to_check.removeAt(0);

        // log::out << "apply " << t->name<<"\n";

        for (int i=0;i<t->call_relocations.size();i++) {
            auto& rel = t->call_relocations[i];
            if (!rel.funcImpl || rel.funcImpl->tinycode_id <= 0)
                continue;

            auto tcode = bytecode->tinyBytecodes[rel.funcImpl->tinycode_id-1];
            bool found = false;
            for(int j=0;j<checked_codes.size();j++) {
                if (tcode == checked_codes[j]) {
                    found = true;
                    break;
                }
            }
            if(!found) {
                checked_codes.add(tcode);
                codes_to_check.add(tcode);
            }
        }
    }
    if(apply_related_relocations) {
        FuncImpl* unresolved_func = nullptr;
        for(auto& t : checked_codes) {
            bool yes = t->applyRelocations(bytecode, false, &unresolved_func);
            if(!yes) {
                error.type = VM_UNRESOLVED_CALL;
                error.message = unresolved_func->astFunction->name + " called from " + t->name;
                log::out << log::RED << "Incomplete call relocation, "<<t->name<<"\n";
                return;
            }
        }
    }

    // TODO: Reuse loaded dlls and function pointers because we
    //   may run VM again. Mapping function pointers multiple times is unnecessary.
    defer {
        for(auto& pair : libs) {
            for (auto& pair2 : pair.second->functions) {
                // libfunc does not own relocs
                // we don't free the relocs memory
                delete pair2.second;
            }
            delete pair.second;
        }
        libs.clear();
    };
    bool any_failure = false;
    for(int i=0;i<bytecode->externalRelocations.size();i++) {
        auto& r = bytecode->externalRelocations[i];
        if(r.library_index == -1) {
            TinyBytecode* tinycode = bytecode->tinyBytecodes[r.tinycode_index];
            bool required_reloc = false;
            for(auto& t : checked_codes) {
                if(t == tinycode) {
                    required_reloc = true;
                    break;
                }
            }
            if(!required_reloc)
                continue;
            // nocheckin TODO: Show location of compile time run directive so developer can remove or comment it out to avoid this error.
            log::out << log::RED << "VM ERROR:"<<log::NO_COLOR<<" Incomplete external relocation " << log::LIME << r.name << log::NO_COLOR << ", ";
            if(tinycode->debugFunction) {
                const std::string& filename = bytecode->debugInformation->files[tinycode->debugFunction->fileIndex];
                int line = tinycode->lines[tinycode->index_of_lines[r.pc]].line_number;
                log::out << filename << ":" << line << "\n";
                compiler->compile_stats.errors++;
            } else {
                log::out << "no source info\n";
            }
            error.type = VMErrorType::VM_ERROR_ALREADY_PRINTED;
            return;
        }
        auto& proglib = bytecode->libraries->get(r.library_index);
        // log::out << log::LIME << r.name << " " << r.library_path<<"\n";
        if(proglib.path.size() == 0) {
            any_failure = true;
            log::out << log::RED << "Lib path is zero, this lib: "<<proglib.name << " this function call: "<<r.name<<".\n";
            continue;
        }

        // Check if the VM encounters the external relocation, if not then
        // we don't need to load the dynamic library and function pointer.
        bool load_lib = true;
        if(checked_codes.size() != 0) {
            load_lib = false;
            for (auto t : checked_codes) {
                if(t->index == r.tinycode_index) {
                    load_lib = true;
                    break;
                }
            }
        }
        if(!load_lib)
            continue;

        auto pair_lib = libs.find(proglib.path);
        Lib* lib = nullptr;
        if(pair_lib == libs.end()) {
            lib = new Lib();
            libs[proglib.path] = lib;
        } else {
            lib = pair_lib->second;
        }
        auto pair_fn = lib->functions.find(r.name);
        LibFunc* fn = nullptr;
        if(pair_fn == lib->functions.end()) {
            fn = new LibFunc();
            lib->functions[r.name] = fn;
        } else {
            fn = pair_fn->second;
        }
        fn->relocs.add(&r);
        fn->signature = r.signature;
    }
    
    for(auto& pair_lib : libs) {
        const std::string& path = pair_lib.first;
        if(path == "<compiler>") {
            pair_lib.second->dll = nullptr;
        } else {
            // log::out << "VM lib "<< path<<"\n";
            
            // If static library was specified then try to load dynamic library instead.
            // When compiling exe we'll link with static library but in VM we can't and must
            // link with dynamic library.
            // TODO: check out of bounds
            int slash = path.find_last_of("/") + 1;
            int dot = path.substr(slash).find("."); // handles libc.so.3.3
            int dot2 = path.substr(dot+1).find("."); // handles libc.so.3.3
            if (dot == -1) {
                dot = path.size();   
            } else {
                dot += slash;   
            }
            if (dot2 == -1) {
                dot2 = path.size();
            } else {
                dot2 += dot+1;
            }
            
            std::string dir_with_slash = path.substr(0, slash);
            std::string filename = path.substr(slash, dot - slash);
            std::string fileext = path.substr(dot, dot2 - (dot));

            // log::out << "parts '" << dir_with_slash << "' '" << filename << "' '" << fileext << "' "<<dot << " " << dot2 << "\n";

            DynamicArray<std::string> alt_paths;
            alt_paths.add(path);
            if(fileext == ".a" && filename.substr(0,3) == "lib") {
                alt_paths.add(dir_with_slash + filename.substr(3) + ".so");
                alt_paths.add(dir_with_slash + filename + ".so");
                alt_paths.add(dir_with_slash + filename.substr(3) + ".dll");
                alt_paths.add(dir_with_slash + filename + ".dll");
            } else if(fileext == ".lib") { 
                alt_paths.add(dir_with_slash + filename + ".dll");
            }
            
            // If our guesses above didn't work then try to find any dll in
            // the directory of the static lib. One such case is GLFW
            //    libglfw3.a
            //    libglfw.so.3.3  <- filename is 'glfw' not 'glfw3' which our guesses above doesn't handle
            if(dir_with_slash.size() > 0 && ((fileext == ".a" && filename.substr(0,3) == "lib") || fileext == ".lib")) {
                std::string best_path;
                int best_score=999999; // low is better
                auto iter = DirectoryIteratorCreate(dir_with_slash.c_str(), dir_with_slash.size()-1);
                DirectoryIteratorData data;
                while(DirectoryIteratorNext(iter, &data)) {
                    if (data.isDirectory) continue;
                        
                    std::string name = std::string(data.name, data.namelen);
                    int slash = name.find_last_of("/") + 1;
                    int dot = name.substr(slash).find(".");
                    int dot2 = name.substr(dot+1).find("."); // handles libc.so.3.3
                    if (dot == -1) {
                        dot = name.size();   
                    } else {
                        dot += slash;   
                    }
                    if (dot2 == -1) {
                        dot2 = name.size();
                    } else {
                        dot2 += dot+1;
                    }
                    std::string dll_name = name.substr(slash, dot - slash);
                    std::string dll_ext = name.substr(dot, dot2 - (dot));
                    // log::out << "is a thing "<<name << " " << dll_name <<" ext "<< dll_ext << "\n";
                    if(dll_ext == ".so" || dll_ext == ".dll") {
                        int score = LevenshteinDistance(filename, dll_name);
                        // log::out << "check "<<name<< " score" << score << "\n";
                        if (score < best_score) {
                            best_path = name;
                        }
                    }
                }
                DirectoryIteratorDestroy(iter, &data);
                if(best_path.size() > 0) {
                    alt_paths.add(best_path);
                }
            }
            
            for(auto& path : alt_paths) {
                pair_lib.second->dll = LoadDynamicLibrary(path, false);
                if (pair_lib.second->dll) {
                    // log::out << "found " << path<<"\n";
                    break;
                }
            }
            
            if(!pair_lib.second->dll) {
                any_failure = true;
                log::out << log::RED << "VM ERROR:"<<log::NO_COLOR<<" Could not load library "<<log::LIME<<path<<log::NO_COLOR<<". calling "<<tinycode_name<<"\n";
                
                if(alt_paths.size() > 0) {
                    log::out << log::GRAY << " Tried these paths: ";
                    for(auto& path : alt_paths) {
                        log::out << path<<", ";
                    }
                    log::out << "\n";
                }
                
                continue;
            } else {
                // log::out << log::LIME << "Load '"<<alt_path<<"'\n";
            }
        }
        for(auto& pair_fn : pair_lib.second->functions) {
            if(path == "<compiler>") {
                pair_fn.second->func_ptr = lang::get_compiler_function(pair_fn.first.c_str(), pair_fn.first.size());
            } else {
                pair_fn.second->func_ptr = GetFunctionPointer(pair_lib.second->dll, pair_fn.first);
                // log::out << log::LIME << " fn '"<<pair_fn.first<<"'\n";
            }
            if(!pair_fn.second->func_ptr) {
                any_failure = true;
                log::out << log::RED << "Could not load function pointer "<<pair_fn.first<<"\n";
                continue;
            } else {
                int index = dll_functions.size();
                dll_functions.add(pair_fn.second);
                // dll_functions.add(pair_fn.second->func_ptr);
                dll_function_names.add(pair_fn.first);
                // APPLY RELOCATIONS
                for(auto r : pair_fn.second->relocs) {
                    auto& t = bytecode->tinyBytecodes[r->tinycode_index];
                    
                    if(t->instructionSegment[r->pc-3] == BC_EXT_DATAPTR) {
                        // TODO: This is a temporary fix for external global variables.
                        int reli = ((i64)r - (i64)bytecode->externalRelocations._ptr)/sizeof(*bytecode->externalRelocations._ptr);
                        // log::out << r->name << " at "<<r->pc << " val "<<reli<< " " << (void*)pair_fn.second->func_ptr <<"\n";
                        if(reli >= dll_variables.size()) {
                            dll_variables.resize(reli+1);
                        }
                        dll_variables[reli] = (void*)pair_fn.second->func_ptr;
                    } else {
                        // log::out << "Apply reloc "<< t->index <<", "<< r->pc << ", "<<r->name<<"\n";
                        *(i32*)&t->instructionSegment[r->pc] = Bytecode::BEGIN_DLL_FUNC_INDEX + index;
                    }
                }
            }
        }
    }
    if(any_failure){
        error.type = VMErrorType::VM_ERROR_ALREADY_PRINTED;
        return;
    }

    // NOTE: Bytecode function pointers are "stubs" which
    //   C functions from a library can call which "leads back" to the VM.
    if (bytecode_pointers.size() < bytecode->tinyBytecodes.size()) {
        bytecode_pointers.resize(bytecode->tinyBytecodes.size());
    }
    
    // Generate inline assembly for all relevant tinycodes
    bool failed = false;
    for(auto tc : checked_codes) {
        for(auto ind : tc->required_asm_instances) {
            auto& inst = bytecode->asmInstances[ind];
            if(!inst.generated) {
                bool yes = prepare_assembly(compiler, tc, inst);
                if(yes) {
                    inst.generated = true; 
                    if(inst.relocations.size() > 0) {
                        failed = true;   
                        compiler->compile_stats.errors++; // nocheckin add error a different way
                        log::out << log::RED << inst.file << ":"<<inst.lineStart  << ": " <<log::NO_COLOR<<" Inline assembly run in VirtualMachine cannot have relocations.\n";
                        for (int i = 0; i < inst.relocations.size();i++) {
                            log::out << " " << inst.relocations[i].name << " - " << inst.relocations[i].textOffset << "\n";
                        }
                    }
                } else {
                    failed = true;
                }
            }
        }
    }
    if(failed) {
        // prepare_assembly already prints errors
        return;
    }
    
    // log::out << log::GOLD << "VirtualMachine:\n";

    // TODO: Setup argc, argv on the stack with betcall convention
    
    // push_offsets.add(0);

    if(stack.max == 0) {
        // only init stack if we haven't already
        // caller to VM may want to init there own stack with a
        // large size with some pre-initialized content.
        init_stack();
    }

    // Set safety check
    #define VM_STACK_MAGIC 0x91620761
    *(i64*)stack.data() = VM_STACK_MAGIC;

    i64 stack_pointer = 0;
    if(bytecode->target == TARGET_ARM) {
        force_mapping = true;
        u64 stack_start = 0xFFFF'FFFF;
        bool yes = add_memory_mapping(stack_start-stack.max, (u64)stack.data(), stack.max);
        stack_pointer = stack_start;
    } else {
        stack_pointer = (i64)(stack.data() + stack.max);
    }
    
    tp = StartMeasure();

    #ifdef ILOG
    #undef _ILOG
    #define _ILOG(X) if(!silent){X};
    #endif
    
    // _ILOG(log::out << "sp = "<<sp<<"\n";)

    // CALLBACK_ON_ASSERT(
    //     log::out << log::RED << "Dump of bytecode\n";
    //     tinycode->print(0,-1, bytecode);
    //     log::out << "Asserted at instruction " << log::CYAN << states.last().prev_pc << "\n";
    //     tinycode->print(states.last().prev_pc - 10, states.last().prev_pc+50, bytecode);
    // )

    // hardcoded breakpoints when debugging
    // breakpoints.add({945});

    if(options) {
        if(options->interactive_vm)
            interactive = true;
        if(options->logged_vm)
            logging = true;
    }
    // interactive = true;
    // logging = true;

    push_state(tiny_index, stack_pointer);
    execute();
    // don't pop, we want to access the stack and registers afterwards
    // to move data into data section or inline literals or whatever.
    // pop_state();
    auto time = StopMeasure(tp);
    if(!silent){
        log::out << "\n";
        log::out << log::LIME << "Executed "<<executedInstructions<<" insts. in "<<FormatTime(time)<< " ("<<FormatUnit(executedInstructions/time)<< " inst/s)\n";
        #ifdef ILOG_REGS
        printRegisters();
        #endif
    }
}

void VirtualMachine::execute(){
    using namespace engone;
    
    const u64 SANITY_MEMORY_ADDRESS_LOW = 0x10000;

    {
        CallFrame frame{};
        frame.func = bytecode->tinyBytecodes[states.last().tiny_index];
        frame.return_address = 0;
        states.last().call_stack.add(frame);
    }
    
    states.last().push_offsets.add(0);

    auto& push_offsets = states.last().push_offsets;
    auto& stack_pointer = states.last().stack_pointer;
    auto& base_pointer = states.last().base_pointer;
    auto& pc = states.last().pc;
    auto& prev_pc = states.last().prev_pc;
    auto& running = states.last().running;
    auto& tiny_index = states.last().tiny_index;
    auto& tinycode = states.last().tinycode;
    auto& registers = states.last().registers;
    auto& has_return_values_on_stack = states.last().has_return_values_on_stack;
    auto& ret_offset = states.last().ret_offset;
    auto& call_stack = states.last().call_stack;

    CALLBACK_ON_ASSERT(
        log::out << log::RED << "Dump of bytecode\n";
        tinycode->print(0,-1, bytecode);
        log::out << "Asserted at instruction " << log::CYAN << states.last().prev_pc << "\n";
        tinycode->print(states.last().prev_pc - 10 >= 0 ? states.last().prev_pc - 10 : 0, states.last().prev_pc+50 < tinycode->instructionSegment.size() ? states.last().prev_pc+50 : tinycode->instructionSegment.size(), bytecode);
    )

    // #define CHECK_PTR_MAPPED(PTR) if(force_mapping && !temp_ptr_was_mapped) { log::out << log::RED << "PTR "<<PTR<<" was not mapped\n"; return; }
    auto CHECK_PTR_MAPPED = [this](void* PTR) {
        if(force_mapping && !temp_ptr_was_mapped) { 
            log::out << log::RED << "PTR "<<PTR<<" was not mapped\n"; 
            return;
        }
    };
    
    auto CHECK_STACK = [&]() {
        void* ptr = map_pointer(stack_pointer, temp_ptr_was_mapped);
        CHECK_PTR_MAPPED(ptr);
        if((i64)ptr < (i64)stack.data() || (i64)ptr > (i64)(stack.data()+stack.max)) {
            log::out << log::RED << "VirtualMachine: Stack overflow\n";
        }
    };

    auto init_frame = [&](int value = -1) {
        if(value == -1)  value = tinycode->frame_size;

        registers[BC_REG_LOCALS] = base_pointer;
        stack_pointer -= value;
        has_return_values_on_stack = false; // alloc_local overwrites return values
        ret_offset = 0;
    };
    auto deinit_frame = [&](int value = -1) {
        if(value == -1)  value = tinycode->frame_size;

        stack_pointer += value;
        if(has_return_values_on_stack) {
            ret_offset -= value;
        }
    };

    init_frame();
    
    #define instructions tinycode->instructionSegment
    while(running) {
        for(int i=0;i<breakpoints.size();i++) {
            if (breakpoints[i].pc == pc) {
                log::out << log::GOLD << "Breakpoint " << log::GREEN << breakpoints[i].pc << " "<< log::CYAN << tinycode->name << "\n";
                tinycode->print(pc, pc + 2, bytecode);
                interactive = true;
                break;
            }
        }

        if(interactive) {
            printf("> ");
            std::string line;
            std::getline(std::cin, line);
            
            // Remove what the user just typed  (it bloats the screen)
            #ifdef OS_WINDOWS
            int w,h;
            GetConsoleSize(&w,&h);
            int x,y;
            GetConsoleCursorPos(&x,&y);
            x = 0;
            y--;
            SetConsoleCursorPos(x,y);
            FillConsoleAt(' ', x, y, w);
            #elif defined(OS_LINUX)
            // getting cursor pos doesn't seem easy on linux ):
            // int new_y_pos = ?;
            // printf("\033[0,%d", new_y_pos);
            #endif
            
            if(line.empty()) {
                // nothing
            } else if(line == "l") {
                // print_registers(true);
            } else if(line == "f") {
                // print_frame(4,4);
            } else if(line == "s") {
                // print_stack();
            } else if(line == "c") {
                interactive = false;
            } else if(line == "q") {
                interactive = false;
            } else if(line == "help") {
                // printf("log registers\n");
            } else if(line == "p") {
                tinycode->print(pc, pc + 2, bytecode);
                continue;
            }
        }

        prev_pc = pc;
        if(pc>=(u64)tinycode->instructionSegment.used)
            break;

        InstructionOpcode opcode = (InstructionOpcode)instructions[pc++];
        executedInstructions++;
        BCRegister op0=BC_REG_INVALID, op1=BC_REG_INVALID, op2=BC_REG_INVALID;
        InstructionControl control=CONTROL_NONE;
        i64 imm=0;
        
        // _ILOG(log::out <<log::GRAY<<" sp: "<< sp <<" fp: "<<fp<<"\n";)
        
        #ifdef ILOG
        if(!silent) {
            auto location = bytecode->getLocation(pc);
            if(location && prevLocation != location){
                if(location->preDesc.size()!=0)
                    log::out << log::GRAY << location->preDesc<<"\n";
                if(!location->file.empty())
                    log::out << log::GRAY << location->file << ":"<<location->line<<":"<<location->column<<": "<<location->desc<<"\n";
                //     log::out << log::GRAY << location->desc<<"\n";
                // else
            }
            prevLocation = location;

            // const char* str = bytecode->getDebugText(pc);
            // if(str)
            //     log::out << log::GRAY<<  str;
        
            if(inst)
                log::out << pc<<": "<<*inst<<", ";
            log::out.flush(); // flush the instruction in case a crash occurs.
        }
        #endif
        bool printed_newline = false;
        if(logging) {
            // auto line_index = tinycode->index_of_lines[prev_pc] - 1;
            // if(line_index != -1) {
            //     auto& line = tinycode->lines[line_index];

            //     if(line.line_number != prev_line || tiny_index != prev_tinyindex) {
            //         if(tiny_index != prev_tinyindex) {
            //             log::out << log::GOLD  << tinycode->name <<"\n";
            //             prev_tinyindex = tiny_index;
            //         }
            //         prev_tinyindex = tiny_index;
            //         log::out << log::CYAN << line.line_number << "| " << line.text<<"\n";
            //         prev_line = line.line_number;
            //     }
            // } else if(tiny_index != prev_tinyindex) {
            //     log::out << log::GOLD  << tinycode->name <<"\n";
            //     prev_tinyindex = tiny_index;
            // }
            tinycode->print(prev_pc, prev_pc + 1, bytecode, &dll_function_names, false, &print_cache);
        }

        void* ptr_from_mov = nullptr;

        // VerifyAllocHeap();

        executedInstructions++;
        switch (opcode) {
        case BC_HALT: {
            running = false;
            // if(logging)
            log::out << log::GREEN << "HALT (instruction)\n";
        } break;
        case BC_NOP: {
        } break;
        case BC_ALLOC_ARGS: {
            imm = *(i16*)(instructions.data() + pc);
            pc += 2;
            
            push_offsets.add(0);
            i64 misalignment = (stack_pointer + imm) % FRAME_SIZE;
            misalignments.add(misalignment);
            if(misalignment != 0) {
                imm += FRAME_SIZE - misalignment;
            }

            init_frame(imm);
        } break;
        case BC_FREE_ARGS: {
            imm = *(i16*)(instructions.data() + pc);
            pc += 2;
            
            push_offsets.pop();
            i64 misalignment = misalignments.last();
            misalignments.pop();
            if(misalignment != 0) {
                imm += FRAME_SIZE - misalignment;
            }
            
            deinit_frame(imm);
        } break;
        case BC_MOV_RR: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            registers[op0] = registers[op1];
        } break;
        case BC_MOV_RM:
        case BC_MOV_RM_DISP16:
        case BC_MOV_MR:
        case BC_MOV_MR_DISP16: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            BCRegister mop, rop;
            if(opcode == BC_MOV_MR || opcode == BC_MOV_MR_DISP16) {
                mop = op0; rop = op1;
            } else {
                rop = op0; mop = op1;
            }
            if(opcode == BC_MOV_RM_DISP16 || opcode == BC_MOV_MR_DISP16) {
                imm = *(i16*)(instructions.data() + pc);
                pc += 2;
            } else {
                imm = 0;
            }
            void* ptr = map_pointer(registers[mop] + imm, temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);
            ptr_from_mov = ptr;
            Assert(ptr);
            
            int size = GET_CONTROL_SIZE(control);
            if(opcode == BC_MOV_MR || opcode == BC_MOV_MR_DISP16) {
                int real_size = 1 << size;
                memcpy(ptr, &registers[op1], real_size);
                // if(size == CONTROL_8B)       *(i8*) ptr = registers[op1];
                // else if(size == CONTROL_16B) *(i16*)ptr = registers[op1];
                // else if(size == CONTROL_32B) *(i32*)ptr = registers[op1];
                // else if(size == CONTROL_64B) *(i64*)ptr = registers[op1];
                int x = 0; // dumb variable because debugger jumps next instruction and out of scope so I can't see the local variables.
            } else {
                int real_size = 1 << size;
                registers[op0] = 0;
                memcpy(&registers[op0], ptr, real_size);
                // if(size == CONTROL_8B)       registers[op0] = *(i8*) ptr;
                // else if(size == CONTROL_16B) registers[op0] = *(i16*)ptr;
                // else if(size == CONTROL_32B) registers[op0] = *(i32*)ptr;
                // else if(size == CONTROL_64B) registers[op0] = *(i64*)ptr;
                int x = 0; // dumb variable because debugger jumps next instruction and out of scope so I can't see the local variables.
                // Thank you variable, you saved me some time - Emarioo, 2024-12-15
            }
        } break;
        case BC_SET_ARG: {
            op0 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            int arg_index = *(i8*)&instructions[pc++];
            int size = GET_CONTROL_SIZE(control);

            Assert(false); // nocheckin we put return value elsewhere.

            void* ptr = map_pointer(stack_pointer + push_offsets.last() + imm, temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);
            ptr_from_mov = ptr;

            // log::out << "SET_ARG " << *(float*)&registers[op0] << "\n";

            if(size == CONTROL_8B)       *(i8*) ptr = registers[op0];
            else if(size == CONTROL_16B) *(i16*)ptr = registers[op0];
            else if(size == CONTROL_32B) *(i32*)ptr = registers[op0];
            else if(size == CONTROL_64B) *(i64*)ptr = registers[op0];
        } break;
        case BC_GET_PARAM: {
            op0 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            int size = GET_CONTROL_SIZE(control);
            int arg_index = *(i8*)&instructions[pc++];

            Assert(false); // nocheckin we put return value elsewhere.

            void* ptr = map_pointer(base_pointer + FRAME_SIZE + imm, temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);

            if(size == CONTROL_8B)       registers[op0] = *(i8*) ptr;
            else if(size == CONTROL_16B) registers[op0] = *(i16*)ptr;
            else if(size == CONTROL_32B) registers[op0] = *(i32*)ptr;
            else if(size == CONTROL_64B) registers[op0] = *(i64*)ptr;
        } break;
         case BC_SET_RET: {
            op0 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            int arg_index = *(i8*)&instructions[pc++];
            int size = GET_CONTROL_SIZE(control);
            
            Assert(imm < 0);
            void* ptr = map_pointer(base_pointer + imm, temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);
            ptr_from_mov = ptr;

            Assert(false); // nocheckin we put return value elsewhere.

            if(size == CONTROL_8B)       *(i8*) ptr = registers[op0];
            else if(size == CONTROL_16B) *(i16*)ptr = registers[op0];
            else if(size == CONTROL_32B) *(i32*)ptr = registers[op0];
            else if(size == CONTROL_64B) *(i64*)ptr = registers[op0];
            // log::out << "SET RET " << (void*)(ptr) << " " << registers[op0]<<"\n";
        } break;
        case BC_GET_VAL: {
            auto inst = (InstBase_op1_ctrl_imm16_imm8*)&instructions[pc];
            pc += sizeof(InstBase_op1_ctrl_imm16_imm8);
            op0 = inst->op0;
            control = inst->control;
            imm = inst->imm16; 
            int arg_index = inst->imm8;
            // op0 = (BCRegister)instructions[pc++];
            // control = (InstructionControl)instructions[pc++];
            // imm = *(i16*)&instructions[pc];
            // pc+=2;
            int size = GET_CONTROL_SIZE(control);
            Assert(false); // nocheckin we put return value elsewhere.
            Assert(has_return_values_on_stack);
            Assert(imm < 0);
            // NOTE: push_offset makes sure push and pop instructions doesn't mess with vals/args registers/references
            void* ptr = map_pointer(stack_pointer + ret_offset + imm - FRAME_SIZE, temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);

            if(size == CONTROL_8B)       registers[op0] = *(i8*) ptr;
            else if(size == CONTROL_16B) registers[op0] = *(i16*)ptr;
            else if(size == CONTROL_32B) registers[op0] = *(i32*)ptr;
            else if(size == CONTROL_64B) registers[op0] = *(i64*)ptr;
        } break;
        case BC_PTR_TO_LOCALS: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;

            registers[op0] = registers[BC_REG_LOCALS] + imm;
        } break;
        case BC_PTR_TO_PARAMS: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            int arg_index = *(i8*)&instructions[pc++];

            Assert(false); // TODO: We need to calculate parameter offset.
                           // immediate is 0 for non-structs. I'ts arg_index that's interesting
            registers[op0] = base_pointer + FRAME_SIZE + imm;
        } break;
        case BC_PUSH: {
            op0 = (BCRegister)instructions[pc++];
            
            stack_pointer -= REGISTER_SIZE;
            CHECK_STACK();

            auto ptr = map_pointer(stack_pointer, temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);
            memcpy(ptr, &registers[op0], REGISTER_SIZE);
            push_offsets.last() += REGISTER_SIZE;
            ret_offset += REGISTER_SIZE;

            // TODO: We have to be careful not to overwrite returned values and then accessing them
            //   with get_val. This can happen if we return 8 1-byte -integers and push those values 8 times.
            //   each push is 8 byte which equals ot 64 bytes which would overwrite the location of the return values.
            //   It would be nice if we can detect it statically in the bytecode builder.
            // has_return_values_on_stack = false;
            // ret_offset = 0;
        } break;
        case BC_POP: {
            op0 = (BCRegister)instructions[pc++];
            
            void* ptr = map_pointer(stack_pointer, temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);
            memcpy(&registers[op0], ptr, REGISTER_SIZE);
            stack_pointer += REGISTER_SIZE;
            CHECK_STACK();
            ret_offset -= REGISTER_SIZE;
            push_offsets.last() -= REGISTER_SIZE;
        } break;
        case BC_LI32: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            registers[op0] = 0;
            *(i32*)&registers[op0] = imm;
        } break;
        case BC_LI64: {
            if(REGISTER_SIZE != 8) {
                log::out << log::RED <<"VM: BC_LI64 not available on non 64-bit systems.\n";
                return;
            }
            op0 = (BCRegister)instructions[pc++];
            imm = *(i64*)&instructions[pc];
            pc+=8;
            registers[op0] = imm;
        } break;
        case BC_INCR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            registers[op0] += imm;
        } break;
        case BC_JMP: {
            imm = *(i32*)&instructions[pc];
            pc+=4;
            pc += imm;
        } break;
        case BC_JZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            if(registers[op0] == 0)
                pc += imm;
        } break;
        case BC_JNZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            if(registers[op0] != 0)
                pc += imm;
        } break;
        case BC_CALL: {
            LinkConvention l = (LinkConvention)instructions[pc++];
            CallConvention c = (CallConvention)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;

            // Finish printing the instruction so that the function
            // we call doesn't print it's own stuff within the instruction.
            if(logging)
                log::out << "\n";
            log::out.flush();
            printed_newline = true;
                int new_tiny_index = imm-1;
            
            if(new_tiny_index >= 0 && new_tiny_index < bytecode->tinyBytecodes.size() && bytecode->tinyBytecodes[new_tiny_index]->asm_index != -1) {
                auto& tc = bytecode->tinyBytecodes[new_tiny_index];
                auto& asmInstance = bytecode->asmInstances[tc->asm_index];
                Assert(asmInstance.generated);
                int size = asmInstance.iEnd - asmInstance.iStart;
                VoidFunction func_ptr = (VoidFunction)AllocateExec(size);
                memcpy((void*)func_ptr, bytecode->rawInstructions.data() + asmInstance.iStart, size);
                FnMakeshift mk_func = compiler->get_makeshift(&tc->funcImpl->signature);
                mk_func(func_ptr, (void*)stack_pointer);
                FreeExec((void*)func_ptr, size);
                
                has_return_values_on_stack = true; // NOTE: potential return values
                ret_offset = 0;
            } else if(imm >= Bytecode::BEGIN_DLL_FUNC_INDEX) {
                Assert((stack_pointer & 0xF) == 0); // ensure aligned stack

                int index = imm - Bytecode::BEGIN_DLL_FUNC_INDEX;
                auto f = dll_functions[index];
                // fix arguments?
                if(c == STDCALL) {
                    // log::out << "Calling " << dll_function_names[index] << "\n";
                    // float a0 = *(float*)(stack_pointer + 0);
                    // float a1 = *(float*)(stack_pointer + 8);
                    // float a2 = *(float*)(stack_pointer + 16);
                    // float a3 = *(float*)(stack_pointer + 24);
                    // log::out << " " << a0 << " " << a1 << " " << a2 << " " << a3 << "\n";

                    // Makehshift is a bad name
                    // it's more like a StackSwitcher_stdcall
                    #ifdef OS_WINDOWS
                    // log::out << "Calling "<<dll_function_names[index]<<"\n";
                    // IMPORTANT: Debugging DLL requires compiling DLL and BTB with the same toolchain! GCC or MSVC for both.
                    FnMakeshift mk_func = compiler->get_makeshift(f->signature);
                    mk_func(f->func_ptr, (void*)stack_pointer);
                    
                    // Makeshift_stdcall(f->func_ptr, (void*)stack_pointer);
                    // Float values are returned in xmm0. Since Makeshift_stdcall is general for all types of functions we don't know the argument or return types.
                    // Hence it returnes both the rax and xmm0 values.
                    // if (f->signature->returnTypes.size()>0&&f->signature->returnTypes[0].typeId == TYPE_FLOAT32) {
                    //     // TODO: Handle 64-bit floats
                    //     *(float*)(stack_pointer-24) = *(float*)(stack_pointer-32);
                    //     log::out << "MOVE "<<*(float*)(stack_pointer-32)<< " to "<<*(float*)(stack_pointer-24)<<"\n";
                    // }
                    #else
                    Assert(("Virtual machine does not support imported functions when using unixcall (System V ABI convention)",false));
                    #endif
                } else if(c == UNIXCALL) {
                    // Makeshift_unixcall(f, (void*)stack_pointer);
                    #ifdef OS_LINUX
                    // TODO: Use get_makeshift and add sysvcall convention to it.
                    //   Same when generating tinycode stub function. Remove the Makeshift_x.s assembly files since we generate the specific machine code we need.
                    // TODO: Handle 32-bit float returned value...
                    FnMakeshift mk_func = compiler->get_makeshift(f->signature);
                    mk_func(f->func_ptr, (void*)stack_pointer);
                    // Makeshift_sysvcall(f->func_ptr, (void*)stack_pointer);
                    #else
                    Assert(("Virtual machine does not support imported functions when using unixcall (System V ABI convention)",false));
                    #endif
                } else { 
                    // Makeshift function for betcall?
                    Assert(false); 
                }

                if(*(i64*)stack.data() != VM_STACK_MAGIC) {
                    // If we wrote beyond the stack than malloc, calloc, free functions
                    // may cause an exception because we messed with stuff we shouldn't have.
                    // Since log::out may call Reallocate, we use normal printf so
                    // we don't crash when printing the error.
                    fprintf(stderr,"VirtualMachine: Stack overflow from external function, needs more than %d bytes for stack!\n", (int)stack.max);
                    fflush(stderr);
                    // log::out << log::RED << "VirtualMachine: Stack overflow from external function, you need more than "<<stack.max<<" bytes for the stack!\n";
                    error.type = VM_STACK_VIOLATION;
                    return;
                }

                has_return_values_on_stack = true; // NOTE: potential return values
                ret_offset = 0;
            } else {
                if(new_tiny_index < 0 || new_tiny_index >= bytecode->tinyBytecodes.size()) {
                    error.type = VM_UNRESOLVED_CALL;
                    return;
                }
                
                stack_pointer -= REGISTER_SIZE;
                CHECK_STACK();
                void* ptr = map_pointer(stack_pointer, temp_ptr_was_mapped);
                CHECK_PTR_MAPPED(ptr);
                if(REGISTER_SIZE == 4) {
                    *(i32*)ptr = pc | ((i64)tiny_index << 16);
                } else {
                    *(i64*)ptr = pc | ((i64)tiny_index << 32);
                }
                stack_pointer -= REGISTER_SIZE;
                CHECK_STACK();
                ptr = map_pointer(stack_pointer, temp_ptr_was_mapped);
                CHECK_PTR_MAPPED(ptr);
                memcpy(ptr, &base_pointer, REGISTER_SIZE);
                base_pointer = stack_pointer;
                registers[BC_REG_LOCALS] = base_pointer;

                CallFrame frame{};
                frame.func = bytecode->tinyBytecodes[tiny_index];
                frame.return_address = pc;
                call_stack.add(frame);
                
                pc = 0;
                tiny_index = new_tiny_index;
                tinycode = bytecode->tinyBytecodes[tiny_index];

                init_frame(); // allocate stack space for local variables of new function
                
                if(enable_fncall_logging) {
                    auto pair = number_of_fncalls.find(tinycode->name);
                    if (pair == number_of_fncalls.end())
                        number_of_fncalls[tinycode->name] = 1;
                    else
                        pair->second += 1;
                }
            }
        } break;
        case BC_PRINTS: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            char* ptr = (char*)registers[op0];
            int len = (int)registers[op1];
            log::out.print(ptr, len);
            log::out.flush();
        } break;
        case BC_PRINTC: {
            op0 = (BCRegister)instructions[pc++];
            char chr = *(char*)&registers[op0];
            log::out.print(&chr, 1);
            log::out.flush();
        } break;
        case BC_CALL_REG: {
            op0 = (BCRegister)instructions[pc++];
            LinkConvention l = (LinkConvention)instructions[pc++];
            CallConvention c = (CallConvention)instructions[pc++];

            // Finish printing the instruction so that the function
            // we call doesn't print it's own stuff within the instruction.
            if(logging)
                log::out << "\n";
            log::out.flush();
            printed_newline = true;
            
            int new_tiny_index = registers[op0] - 1; // -1 very important
            
            if (!(new_tiny_index >= 0 && new_tiny_index < bytecode->tinyBytecodes.size())) {
                // Not a valid index, we assume it's a function pointer from
                // ext_dataptr, will crash if bytecode instructions
                // are wrong and we call call_reg on an immediate.

                Assert((stack_pointer & 0xF) == 0); // ensure aligned stack

                // int index = imm - Bytecode::BEGIN_DLL_FUNC_INDEX;
                auto f = (void(*)(void))registers[op0];
                auto signature = tinycode->pc_signature_map[prev_pc];
                Assert(f);
                // fix arguments?
                if(c == STDCALL) {
                    // Makehshift is a bad name
                    // it's more like a StackSwitcher_stdcall
                    #ifdef OS_WINDOWS
                    // log::out << "Calling "<<dll_function_names[index]<<"\n";
                    // IMPORTANT: Debugging DLL requires compiling DLL and BTB with the same toolchain! GCC or MSVC for both.
                    FnMakeshift mk_func = compiler->get_makeshift(signature);
                    mk_func(f, (void*)stack_pointer);
                    #else
                    Assert(("Virtual machine does not support imported functions when using unixcall (System V ABI convention)",false));
                    #endif
                } else if(c == UNIXCALL) {
                    #ifdef OS_LINUX
                    FnMakeshift mk_func = compiler->get_makeshift(signature);
                    mk_func(f, (void*)stack_pointer);
                    #else
                    Assert(("Virtual machine does not support imported functions when using unixcall (System V ABI convention)",false));
                    #endif
                } else { 
                    // Makeshift function for betcall?
                    Assert(false); 
                }

                if(*(i64*)stack.data() != VM_STACK_MAGIC) {
                    // If we wrote beyond the stack than malloc, calloc, free functions
                    // may cause an exception because we messed with stuff we shouldn't have.
                    // Since log::out may call Reallocate, we use normal printf so
                    // we don't crash when printing the error.
                    fprintf(stderr,"VirtualMachine: Stack overflow from external function, needs more than %d bytes for stack!\n", (int)stack.max);
                    fflush(stderr);
                    // log::out << log::RED << "VirtualMachine: Stack overflow from external function, you need more than "<<stack.max<<" bytes for the stack!\n";
                    error.type = VM_STACK_VIOLATION;
                    return;
                }

                has_return_values_on_stack = true; // NOTE: potential return values
                ret_offset = 0;
            } else {

                // if (!(new_tiny_index >= 0 && new_tiny_index < bytecode->tinyBytecodes.size())) {
                //     // not valid index
                //     running = false;
                //     break;
                // }
                
                stack_pointer -= REGISTER_SIZE;
                CHECK_STACK();
                void* ptr = map_pointer(stack_pointer, temp_ptr_was_mapped);
                CHECK_PTR_MAPPED(ptr);
                if(REGISTER_SIZE == 4) {
                    *(i32*)ptr = pc | ((i64)tiny_index << 16);
                } else {
                    *(i64*)ptr = pc | ((i64)tiny_index << 32);
                }

                stack_pointer -= REGISTER_SIZE;
                CHECK_STACK();
                ptr = map_pointer(stack_pointer, temp_ptr_was_mapped);
                CHECK_PTR_MAPPED(ptr);
                memcpy(ptr, &base_pointer, REGISTER_SIZE);
                base_pointer = stack_pointer;
                registers[BC_REG_LOCALS] = base_pointer;

                pc = 0;
                tiny_index = new_tiny_index;
                tinycode = bytecode->tinyBytecodes[tiny_index];
                
                if(enable_fncall_logging) {
                    auto pair = number_of_fncalls.find(tinycode->name);
                    if (pair == number_of_fncalls.end())
                        number_of_fncalls[tinycode->name] = 1;
                    else
                        pair->second += 1;
                }
            }
        } break;
        case BC_RET: {
            deinit_frame(); // free local variables

            call_stack.pop();
            if(call_stack.size() == 0) {
                running = false;
                break;
            }
            
            i64 diff = stack_pointer - (i64)stack.data();
            if(diff == (i64)(stack.max)) {
                // no previous call frame so we quit
                running = false;
                break;
            }

            void* ptr = map_pointer(stack_pointer, temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);
            if(REGISTER_SIZE == 4) {
                base_pointer = *(i32*)ptr;
            } else {
                base_pointer = *(i64*)ptr;
            }
            stack_pointer += REGISTER_SIZE;
            CHECK_STACK();

            ptr = map_pointer(stack_pointer, temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);
            i64 encoded_pc;
            if(REGISTER_SIZE == 4) {
                encoded_pc = *(i32*)ptr;
            } else {
                encoded_pc = *(i64*)ptr;
            }
            stack_pointer += REGISTER_SIZE;
            CHECK_STACK();

            registers[BC_REG_LOCALS] = base_pointer;


            has_return_values_on_stack = true;
            ret_offset = 0;
            if(REGISTER_SIZE == 4) {
                pc = encoded_pc & 0xFFFF;
                tiny_index = (encoded_pc >> 16) && 0xFFFF;
            } else {
                pc = encoded_pc & 0xFFFF'FFFF;
                tiny_index = encoded_pc >> 32;
            }
            Assert(tiny_index < bytecode->tinyBytecodes.size());
            tinycode = bytecode->tinyBytecodes[tiny_index];
        } break;
        case BC_DATAPTR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;

            Assert(bytecode->dataSegment.size() > imm);
            registers[op0] = (u64)(bytecode->dataSegment.data() + imm);
        } break;
        case BC_EXT_DATAPTR: {
            // Assert(false);
            op0 = (BCRegister)instructions[pc++];
            LinkConvention l = (LinkConvention)instructions[pc++];

            int reli = -1;
            for(int i=0;i<bytecode->externalRelocations.size();i++) {
                auto& rel = bytecode->externalRelocations[i];
                if (rel.pc == pc && rel.tinycode_index == tiny_index) {
                    reli = i;
                    break;
                }
            }
            if(reli != -1) {
                void* ptr = dll_variables[reli];
                registers[op0] = (i64)ptr;
            } else {
                log::out << log::RED << "Cannot access imported variable in virtual machine (not implemented)\n";
                running = false;
                break;
            }
        } break;
        case BC_CODEPTR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            int new_tiny_index = imm-1;
            if(new_tiny_index < 0 || new_tiny_index >= bytecode->tinyBytecodes.size()) {
                error.type = VM_UNRESOLVED_CALL;
                return;
            }
            
            registers[op0] = (i64)get_bytecode_pointer(imm-1);
        } break;
        case BC_CAST: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            u8 fsize = 1 << GET_CONTROL_SIZE(control);
            u8 tsize = 1 << GET_CONTROL_CONVERT_SIZE(control);

            if(!IS_CONTROL_FLOAT(control) && !IS_CONTROL_CONVERT_FLOAT(control)) {
                if(op0 == op1) {
                    if(tsize < 8)
                    memset((char*)&registers[op0] + tsize, 0, 8 - tsize);
                } else {
                    registers[op0] = 0;
                }
                if (IS_CONTROL_CONVERT_SIGNED(control)) {
                    if (IS_CONTROL_SIGNED(control)) {
                        // i16 -> i32
                        // 0xffff -> 0xffff_ffff
                        
                        i64 tmp = registers[op1];
                        
                        bool issigned = tmp >> (8 * fsize - 1);
                        
                        if(issigned)
                            memset((char*)&tmp + fsize, 0xFF, 8-fsize);
                        else
                            memset((char*)&tmp + fsize, 0x0, 8-fsize);
                        
                        if(tsize == 1) *(i8*)&registers[op0] = tmp;
                        else if(tsize == 2) *(i16*)&registers[op0] = tmp;
                        else if(tsize == 4) *(i32*)&registers[op0] = tmp;
                        else if(tsize == 8) *(i64*)&registers[op0] = tmp;
                        else Assert(false);
                    } else {
                        u64 tmp = registers[op1];
                        if(tsize == 1) *(i8*)&registers[op0] = tmp;
                        else if(tsize == 2) *(i16*)&registers[op0] = tmp;
                        else if(tsize == 4) *(i32*)&registers[op0] = tmp;
                        else if(tsize == 8) *(i64*)&registers[op0] = tmp;
                        else Assert(false);
                    }
                } else {
                    if (IS_CONTROL_SIGNED(control)) {
                        // i16 -> i32
                        // 0xffff -> 0xffff_ffff
                        
                        i64 tmp = registers[op1];
                        bool issigned = tmp >> (8 * fsize - 1);
                        if(issigned)
                            memset((char*)&tmp + fsize, 0xFF, 8-fsize);
                        else
                            memset((char*)&tmp + fsize, 0x0, 8-fsize);
                        
                        if(tsize == 1) *(u8*)&registers[op0] = tmp;
                        else if(tsize == 2) *(u16*)&registers[op0] = tmp;
                        else if(tsize == 4) *(u32*)&registers[op0] = tmp;
                        else if(tsize == 8) *(u64*)&registers[op0] = tmp;
                        else Assert(false);
                    } else {
                        u64 tmp = registers[op1];
                        if(tsize == 1) *(u8*)&registers[op0] = tmp;
                        else if(tsize == 2) *(u16*)&registers[op0] = tmp;
                        else if(tsize == 4) *(u32*)&registers[op0] = tmp;
                        else if(tsize == 8) *(u64*)&registers[op0] = tmp;
                        else Assert(false);
                    }
                }
            } else if(IS_CONTROL_UNSIGNED(control) && IS_CONTROL_CONVERT_FLOAT(control)) {
                u64 tmp = registers[op1];
                if(tsize == 4) *(float*)&registers[op0] = tmp;
                else if(tsize == 8) *(double*)&registers[op0] = tmp;
                else Assert(false);
            } else if(IS_CONTROL_SIGNED(control) && IS_CONTROL_CONVERT_FLOAT(control)) {
                i64 tmp = registers[op1];
                if(tsize == 4) *(float*)&registers[op0] = tmp;
                else if(tsize == 8) *(double*)&registers[op0] = tmp;
                else Assert(false);
            } else if(IS_CONTROL_FLOAT(control) && IS_CONTROL_CONVERT_UNSIGNED(control)) {
                if(fsize == 4){
                    float tmp = *(float*)&registers[op1];
                    if(tsize == 1) *(u8*)&registers[op0] = tmp;
                    else if(tsize == 2) *(u16*)&registers[op0] = tmp;
                    else if(tsize == 4) *(u32*)&registers[op0] = tmp;
                    else if(tsize == 8) *(u64*)&registers[op0] = tmp;
                    else Assert(false);
                } else if(fsize == 8) {
                    double tmp = *(double*)&registers[op1];
                    if(tsize == 1) *(u8*)&registers[op0] = tmp;
                    else if(tsize == 2) *(u16*)&registers[op0] = tmp;
                    else if(tsize == 4) *(u32*)&registers[op0] = tmp;
                    else if(tsize == 8) *(u64*)&registers[op0] = tmp;
                    else Assert(false);
                } else Assert(false);
            } else if(IS_CONTROL_FLOAT(control) && IS_CONTROL_CONVERT_SIGNED(control)) {
                if(fsize == 4){
                    float tmp = *(float*)&registers[op1];
                    registers[op0] = 0;
                    if(tsize == 1) *(i8*)&registers[op0] = tmp;
                    else if(tsize == 2) *(i16*)&registers[op0] = tmp;
                    else if(tsize == 4) *(i32*)&registers[op0] = tmp;
                    else if(tsize == 8) *(i64*)&registers[op0] = tmp;
                    else Assert(false);
                } else if(fsize == 8) {
                    double tmp = *(double*)&registers[op1];
                    if(tsize == 1) *(i8*)&registers[op0] = tmp;
                    else if(tsize == 2) *(i16*)&registers[op0] = tmp;
                    else if(tsize == 4) *(i32*)&registers[op0] = tmp;
                    else if(tsize == 8) *(i64*)&registers[op0] = tmp;
                    else Assert(false);
                } else Assert(false);
            } else if(IS_CONTROL_FLOAT(control) && IS_CONTROL_CONVERT_FLOAT(control)) {
                if(fsize == 4 && tsize == 4)      *(float*)&registers[op0] = *(float*)&registers[op1];
                else if(fsize == 4 && tsize == 8) *(double*)&registers[op0] = *(float*)&registers[op1];
                else if(fsize == 8 && tsize == 4) *(float*)&registers[op0] = *(double*)&registers[op1];
                else if(fsize == 8 && tsize == 8) *(double*)&registers[op0] = *(double*)&registers[op1];
                else Assert(false);
            }  else Assert(false);
        } break;
        case BC_MEMZERO: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            u8 batchsize = (u8)instructions[pc++];

            void* ptr = map_pointer(registers[op0], temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);

            Assert((u64)ptr > SANITY_MEMORY_ADDRESS_LOW); // we rarely access memory below this address, nice way to catch bugs
            Assert(registers[op1] < 0x100000); // we rarely memzero memory larger than this
            memset(ptr, 0, registers[op1]);
        } break;
        case BC_MEMCPY: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            op2 = (BCRegister)instructions[pc++];
            void* ptr = map_pointer(registers[op0], temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);
            void* ptr1 = map_pointer(registers[op1], temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr1);
            Assert((u64)ptr > SANITY_MEMORY_ADDRESS_LOW); // we rarely access memory below this address, nice way to catch bugs
            Assert((u64)ptr1 > SANITY_MEMORY_ADDRESS_LOW); // we rarely access memory below this address, nice way to catch bugs
            Assert(registers[op2] < 0x100000); // we rarely memzero memory larger than this
            memcpy(ptr, ptr1, registers[op2]);
        } break;
        case BC_ASM: {
            u8 inputs = (u8)instructions[pc++];
            u8 outputs = (u8)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            if(push_offsets.size())
                push_offsets.last() += 8 * (outputs - inputs);
            
            BytecodeASM& asmInstance = bytecode->asmInstances.get(imm);
            Assert(asmInstance.generated);
            u32 len = asmInstance.iEnd - asmInstance.iStart;
            
            if(len != 0) {
                u8* ptr = bytecode->rawInstructions._ptr + asmInstance.iStart;
                
                
                // TODO: Reuse allocated executable memory
                int max = 1024;
                u8* f = (u8*)AllocateExec(max);
                int head = 0;
                /*
                    push rbp
                    push rdi
                    # rdi is VM stack pointer
                    mov rbp, rsi
                    sub rsp, 16 # size based on inputs of ASM

                    mov rax, [rdi + 0]
                    mov [rsp+0], rax
                    mov rax, [rdi + 8]
                    mov [rsp+8], rax

                    # body
                    pop rax
                    pop rcx
                    add rax, rcx
                    push rax

                    add rsp, 16
                    pop rdi
                    mov rax, [rsp - 24]
                    mov [rdi - 8], rax
                    pop rbp
                    ret
                */
                
                // u8 PRELUDE[]{
                //     /* push rbp */ 0x55,
                //     /* push rdi */ 0x57,
                //     /* mov rbp, rsi */ 0x48, 0x89, 0xF5,
                //     /* sub rsp, 16 */ 0x48, 0x83, 0xEC, 0x10,
                //     /* mov rax, [rdi + 0] */ 0x48, 0x8B, 0x47, 0x01,
                //     /* mov [rsp+0], rax */ 0x48, 0x89, 0x44, 0x24, 0x01,
                //     /* mov rax, [rdi + 8] */ 0x48, 0x8B, 0x47, 0x08,
                //     /* mov [rsp+8], rax */ 0x48, 0x89, 0x44, 0x24, 0x08,
                //     /*  */ /* 0x58, 0x59, 0x48, 0x01, 0xC8, 0x50,*/
                // };
                // u8 EPILOG[]{
                //     /* add rsp */ 0x48, 0x83, 0xC4, 0x10,
                //     /* pop rdi */ 0x5F,
                //     /*  */ 0x48, 0x8B, 0x44, 0x24, 0xE8,
                //     /*  */ 0x48, 0x89, 0x47, 0xF8,
                //     /* pop rbp */ 0x5D,
                //     /* ret */ 0xC3
                // };
                
                // int stackspace = inputs * 8; // TODO: 16-byte alignment
                // if(outputs*8 > stackspace) {
                //     stackspace = outputs*8;
                // }
                {
                #ifdef OS_WINDOWS
                    Assert(inputs*8 >= -128 && inputs*8 <= 127);
                    u8 code[] {
                        /* push rbp     */ 0x55,
                        /* push rcx     */ 0x51,
                        /* mov rbp, rdx */ 0x48, 0x89, 0xd5,
                        /* sub rsp, 16  */ 0x48, 0x83, 0xEC, (u8)(inputs*8), // TODO: 16-byte alignment
                    };
                #elif OS_LINUX
                    Assert(inputs*8 >= -128 && inputs*8 <= 127);
                    u8 code[] {
                        /* push rbp     */ 0x55,
                        /* push rdi     */ 0x57,
                        /* mov rbp, rsi */ 0x48, 0x89, 0xF5,
                        /* sub rsp, 16  */ 0x48, 0x83, 0xEC, (u8)(inputs*8), // TODO: 16-byte alignment
                    };
                    
                #else
                    Assert(false);
                #endif
                    memcpy(f+head, code, sizeof(code));
                    head += sizeof(code);
                }
                
                for(int i=0;i<inputs;i++) {
                    #ifdef OS_WINDOWS
                    Assert(i*8 >= -128 && i*8 <= 127);
                    u8 code[] {
                        /* mov rax, [rcx + 0] */ 0x48, 0x8B, 0x41, (u8)(i*8),
                        /* mov [rsp+0], rax   */ 0x48, 0x89, 0x44, 0x24, (u8)(i*8),
                    };
                    #elif OS_LINUX
                    Assert(i*8 >= -128 && i*8 <= 127);
                    u8 code[] {
                        /* mov rax, [rdi + 0] */ 0x48, 0x8B, 0x47, (u8)(i*8),
                        /* mov [rsp+0], rax   */ 0x48, 0x89, 0x44, 0x24, (u8)(i*8),
                    };
                    #else
                    Assert(false);
                    #endif
                    memcpy(f+head, code, sizeof(code));
                    head += sizeof(code);
                }
                
                memcpy(f+head, ptr, len);
                head += len;
                
                {
                #ifdef OS_WINDOWS
                    Assert(outputs*8 >= -128 && outputs*8 <= 127);
                    u8 code[] {
                        /* add rsp */ 0x48, 0x83, 0xC4, (u8)(outputs*8),
                        /* pop rcx */ 0x59,
                    };
                    #elif OS_LINUX
                    Assert(outputs*8 >= -128 && outputs*8 < 127);
                    u8 code[] {
                        /* add rsp */ 0x48, 0x83, 0xC4, (u8)(outputs*8),
                        /* pop rdi */ 0x5F,
                    };
                    #else
                    Assert(false);
                    #endif
                    memcpy(f+head, code, sizeof(code));
                    head += sizeof(code);
                }
                
                for(int i=0;i<outputs;i++) {
                    #ifdef OS_WINDOWS
                    Assert(-i*8-16 >= -128 && -i*8-16 <= 127);
                    Assert(inputs*8-i*8-8 >= -128 && inputs*8-i*8-8 <= 127);
                    u8 code[] {
                        /* mov rax, [rsp - 24] */ 0x48, 0x8B, 0x44, 0x24, (u8)(-i*8-16),
                        /* mov [rcx - 8], rax  */ 0x48, 0x89, 0x41, (u8)(inputs*8-i*8-8),
                    };
                #elif OS_LINUX
                    Assert(-i*8-16 >= -128 && -i*8-16 <= 127);
                    Assert(inputs*8-i*8-8 >= -128 && inputs*8-i*8-8 <= 127);
                    u8 code[] {
                        /* mov rax, [rsp - 24] */ 0x48, 0x8B, 0x44, 0x24, (u8)(-i*8-16),
                        /* mov [rdi - 8], rax  */ 0x48, 0x89, 0x47, (u8)(inputs*8-i*8-8),
                    };
                #else
                    Assert(false);
                #endif
                    memcpy(f+head, code, sizeof(code));
                    head += sizeof(code);
                }
                
                {
                    u8 code[] {
                        /* pop rbp */ 0x5D,
                        /* ret     */ 0xC3
                    };
                    memcpy(f+head, code, sizeof(code));
                    head += sizeof(code);
                }
                
                // OutputAsHex("inl_asm.log", f, head);
                
                auto func = (void(*)(i64, i64))f;
                func(stack_pointer, base_pointer);
                
                FreeExec(f, max);
            } else {
                log::out << log::YELLOW << asmInstance.file <<":"<<asmInstance.lineStart<< ": "<<log::NO_COLOR <<" was incomplete or just empty?\n";
            }
            
            stack_pointer += (inputs - outputs) * 8;
        } break;
        case BC_ADD:
        case BC_SUB:
        case BC_MUL:
        case BC_DIV:
        case BC_MOD:
        case BC_EQ:
        case BC_NEQ:
        case BC_LT:
        case BC_LTE:
        case BC_GT:
        case BC_GTE: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            u64 raw_value0 = registers[op0];
            u64 raw_value1 = registers[op1];
            registers[op0] = 0; // we reset first operand because we don't want any dirty bits remaining
            // we don't reset second operand because other instructions may reuse the value
            // we copy the second operand in case first and second operand point to the same register

            if(IS_CONTROL_FLOAT(control)) {
                if(GET_CONTROL_SIZE(control) == CONTROL_32B) {
                    switch(opcode){
                        #define OP(P) *(float*)&registers[op0] = *(float*)&raw_value0 P *(float*)&raw_value1; break;
                        #define OP_BOOL(P) registers[op0] = *(float*)&raw_value0 P *(float*)&raw_value1; break;
                        case BC_ADD: OP(+)
                        case BC_SUB: OP(-)
                        case BC_MUL: OP(*)
                        case BC_DIV: OP(/)
                        case BC_MOD: {
                            // modulo no negative numbers
                            auto tmp = fmodf(*(float*)&raw_value0, *(float*)&raw_value1);
                            if (tmp<0.f)
                                tmp += *(float*)&raw_value1;
                            *(float*)&registers[op0] = tmp;
                            break;
                        }
                        case BC_EQ:  OP_BOOL(==)
                        case BC_NEQ: OP_BOOL(!=)
                        case BC_LT:  OP_BOOL(<)
                        case BC_LTE: OP_BOOL(<=)
                        case BC_GT:  OP_BOOL(>)
                        case BC_GTE: OP_BOOL(>=)
                        default: Assert(false);
                        #undef OP
                        #undef OP_BOOL
                    }
                } else if(GET_CONTROL_SIZE(control) == CONTROL_64B) {
                    switch(opcode){
                        #define OP(P) *(double*)&registers[op0] = *(double*)&raw_value0 P *(double*)&raw_value1; break;
                        #define OP_BOOL(P) registers[op0] = *(double*)&raw_value0 P *(double*)&raw_value1; break;
                        case BC_ADD: OP(+)
                        case BC_SUB: OP(-)
                        case BC_MUL: OP(*)
                        case BC_DIV: OP(/)
                        case BC_MOD: {
                            // modulo no negative numbers
                            auto tmp = fmodl(*(double*)&raw_value0, *(double*)&raw_value1);
                            if (tmp<0.f)
                                tmp += *(double*)&raw_value1;
                            *(double*)&registers[op0] = tmp;
                            break;
                        }
                        case BC_EQ:  OP_BOOL(==)
                        case BC_NEQ: OP_BOOL(!=)
                        case BC_LT:  OP_BOOL(<)
                        case BC_LTE: OP_BOOL(<=)
                        case BC_GT:  OP_BOOL(>)
                        case BC_GTE: OP_BOOL(>=)
                        default: Assert(false);
                        #undef OP
                        #undef OP_BOOL
                        
                    }
                }
            } else {
                #define OP(P) *(TYPE*)&registers[op0] = *(TYPE*)&raw_value0 P *(TYPE*)&raw_value1; break;
                #define SWITCH_OPS                      \
                     switch(opcode){                    \
                        case BC_ADD: OP(+)              \
                        case BC_SUB: OP(-)              \
                        case BC_MUL: OP(*)              \
                        case BC_DIV: OP(/)              \
                        case BC_MOD: {                   \
                            auto tmp = *(TYPE*)&raw_value0 % *(TYPE*)&raw_value1; \
                            if(tmp<0)                   \
                                tmp += *(TYPE*)&raw_value1; \
                            *(TYPE*)&registers[op0] = tmp;       \
                        } break; /* positive modulo    */   \
                        case BC_EQ:  OP(==)             \
                        case BC_NEQ: OP(!=)             \
                        case BC_LT:  OP(<)              \
                        case BC_LTE: OP(<=)             \
                        case BC_GT:  OP(>)              \
                        case BC_GTE: OP(>=)             \
                        default: Assert(false);         \
                    }
                if(IS_CONTROL_SIGNED(control)) {
                    if(GET_CONTROL_SIZE(control) == CONTROL_8B) {
                        #define TYPE i8
                        SWITCH_OPS
                        #undef TYPE
                    } else if(GET_CONTROL_SIZE(control) == CONTROL_16B) {
                        #define TYPE i16
                        SWITCH_OPS
                        #undef TYPE
                    } else if(GET_CONTROL_SIZE(control) == CONTROL_32B) {
                        #define TYPE i32
                        SWITCH_OPS
                        #undef TYPE
                    } else if(GET_CONTROL_SIZE(control) == CONTROL_64B) {
                        #define TYPE i64
                        SWITCH_OPS
                        #undef TYPE
                    }
                } else {
                    if(GET_CONTROL_SIZE(control) == CONTROL_8B) {
                        #define TYPE u8
                        SWITCH_OPS
                        #undef TYPE
                    } else if(GET_CONTROL_SIZE(control) == CONTROL_16B) {
                        #define TYPE u16
                        SWITCH_OPS
                        #undef TYPE
                    } else if(GET_CONTROL_SIZE(control) == CONTROL_32B) {
                        #define TYPE u32
                        SWITCH_OPS
                        #undef TYPE
                    } else if(GET_CONTROL_SIZE(control) == CONTROL_64B) {
                        #define TYPE u64
                        SWITCH_OPS
                        #undef TYPE
                    }
                }
                #undef OP
            }
            
            // if(op0 != op1 && IS_CONTROL_UNSIGNED(control)) {
            //     // Clear upper bits of register to avoid problems
            //     int size = 1 << GET_CONTROL_SIZE(control);
            //     if(size != 8)
            //         memset((char*)&registers[op0] + size, 0, 8-size); // reset
            // }
        } break;
        case BC_LAND:
        case BC_LOR:
        case BC_LNOT:
        case BC_BAND:
        case BC_BOR:
        case BC_BXOR:
        case BC_BNOT:
        case BC_BLSHIFT:
        case BC_BRSHIFT: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            u64 raw_value0 = registers[op0];
            u64 raw_value1 = registers[op1];
            registers[op0] = 0;

            // TODO: Handle sizes, don't always to 64-bit operation
            switch(opcode){
                #define OP(P) registers[op0] = raw_value0 P raw_value1; break;
                case BC_BXOR: OP(^)
                case BC_BOR: OP(|)
                case BC_BAND: OP(&)
                case BC_BLSHIFT: OP(<<)
                case BC_BRSHIFT: OP(>>)
                case BC_LAND: OP(&&)
                case BC_LOR: OP(||)
                case BC_LNOT: registers[op0] = !raw_value1; break;
                case BC_BNOT: registers[op0] = ~raw_value1; break;
                default: Assert(false);
                #undef OP
            }
            // Clear upper bits of register to avoid problems
            int size = 1 << GET_CONTROL_SIZE(control);
            if(size != 8)
                memset((char*)&registers[op0] + size, 0, 8-size); // reset
        } break;
        case BC_RDTSC: {
            op0 = (BCRegister)instructions[pc++];
            #ifdef OS_WINDOWS
            auto time = __rdtsc();
            registers[op0] = time;
            #else
            Assert(("RDTSC not implemented in VM on Linux",false));
            #endif
        } break;
        case BC_STRLEN: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];

            void* ptr = map_pointer(registers[op1], temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);
            registers[op0] = strlen((char*)ptr);
        } break;
        case BC_ATOMIC_ADD: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];

            void* ptr = map_pointer(registers[op0], temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);
            Assert(GET_CONTROL_SIZE(control) == CONTROL_32B);
            Assert(!IS_CONTROL_FLOAT(control));
            registers[op0] = atomic_add((volatile i32*)ptr, registers[op1]);
        } break;
        case BC_ATOMIC_CMP_SWAP: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            op2 = (BCRegister)instructions[pc++];

            void* ptr = map_pointer(registers[op0], temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);
            #ifdef OS_WINDOWS
            // NOTE: op1 is old value (comparand), op2 is new value (exchange)
            long original_value = _InterlockedCompareExchange((volatile long*)ptr, registers[op2], registers[op1]);
            registers[op0] = original_value;
            #else
            Assert(("ATOMIC_CMP_SWAP not implemented in VM on Linux",false));
            #endif
        } break;
        case BC_SQRT: {
            op0 = (BCRegister)instructions[pc++];
            float t = sqrtf(*(float*)&registers[op0]);
            registers[op0] = *(i32*)&t;
        } break;
        case BC_ROUND: {
            op0 = (BCRegister)instructions[pc++];
            float t = roundf(*(float*)&registers[op0]);
            registers[op0] = *(i32*)&t;
        } break;
        case BC_TEST_VALUE: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            u32 data = *(u32*)&instructions[pc];
            pc+=4;
            
            u8* testValue = (u8*)&registers[op0];
            u8* computedValue = (u8*)&registers[op1];

            int size = 1 << GET_CONTROL_SIZE(control);

            bool same = !strncmp((char*)testValue, (char*)computedValue, size);

            char tmp[]{
                same ? 'x' : '_',
                (char)((data)&0xFF),
                (char)((data>>8)&0xFF),
                (char)((data>>16)&0xFF)
            };
            // fwrite(&tmp, 1, 1, stderr);

            auto out = engone::GetStandardErr();
            engone::FileWrite(out, &tmp, 4);

            _ILOG(log::out << "\n";)
        } break;
        default: {
            log::out << log::RED << "Implement "<< log::PURPLE<< opcode << "\n";
            running = false;
            break;
        }
        } // switch

        if(logging && op0 && opcode != BC_LI32 && opcode != BC_LI64 && opcode != BC_CALL) {
            i64 val = registers[op0];
            if(ptr_from_mov) {
                     if(GET_CONTROL_SIZE(control) == CONTROL_8B)  val = *(i8*)ptr_from_mov;
                else if(GET_CONTROL_SIZE(control) == CONTROL_16B) val = *(i16*)ptr_from_mov;
                else if(GET_CONTROL_SIZE(control) == CONTROL_32B) val = *(i32*)ptr_from_mov;
                else if(GET_CONTROL_SIZE(control) == CONTROL_64B) val = *(i64*)ptr_from_mov;
            }
            bool was_float = false;
            if((val & 0xFFFF'FFFF'0000'0000) == 0) {
                int expo = (0xFF & (val >> 23)) - 127; // 8 bits exponent (0xFF), 23 mantissa, 127 bias
                if(expo > -24 && expo < 24) {
                    was_float = true;
                    // probably a float
                    log::out << log::GREEN << ", "<<*(float*)&val << log::GRAY << " (" << val << ")";
                }
            } else {
                int expo = (0x7FF & (val >> 52)) - 1023; // 11 bits exponent (0x7FF), 52 mantissa, 1023 bias
                if(expo > -24 && expo < 24) {
                    was_float = true;
                    // probably a 64-bit float
                    log::out << log::GREEN << ", "<<*(double*)&val << log::GRAY << " (" << val << ")";
                }
            }
            if(!was_float)
                log::out << log::GRAY << ","<< val;
        }

        if(logging && !printed_newline)
            log::out << "\n";
        #undef instructions
    }
    // if(!silent){
    //     log::out << "reg_t0: "<<registers[BC_REG_T0]<<"\n";
    //     log::out << "reg_a: "<<registers[BC_REG_A]<<"\n";
    // }
    // if(userAllocatedBytes!=0){
    //     log::out << log::RED << "User program leaks "<<userAllocatedBytes<<" bytes\n";
    // }
    // if(!expectValuesOnStack){
        // if(sp != expectedStackPointer){
        //     log::out << log::YELLOW<<"sp was "<<(i64)(sp - ((u64)stack.data+stack.max))<<", should be 0\n";
        // }
        // if(fp != (u64)stack.data+stack.max){
        //     log::out << log::YELLOW<<"fp was "<<(i64)(fp - ((u64)stack.data+stack.max))<<", should be 0\n";
        // }
    // }
    // silent = false;
}
bool VirtualMachine::add_memory_mapping(u64 start, u64 physical, u64 size) {
    // check for overlap
    for(int i=0;i<memory_map.size();i++) {
        auto& map = memory_map[i];
        if(start + size > map.start_address && start < map.start_address + map.size)
            return false;
    }
    MemoryMapping map{};
    map.start_address = start;
    map.physical_start_address = physical;
    map.size = size;
    memory_map.add(map);
    return true;
}
void* VirtualMachine::map_pointer(u64 virtual_pointer, bool& was_mapped) {
    using namespace engone;
    // find mapping
    for(int i=0;i<memory_map.size();i++) {
        auto& map = memory_map[i];
        if(virtual_pointer >= map.start_address && virtual_pointer < map.start_address + map.size) {
            was_mapped = true;
            return (void*)(map.physical_start_address + virtual_pointer - map.start_address);
        }
    }
    was_mapped = false;
    // If you crash and are accesing a pointer from global data at compile time
    // then perhaps it wasn't initialized. Runtime type information for example.
    // suspicious pointer
    if(((i64)virtual_pointer >= 0x10000 && (i64)virtual_pointer < 0x0010'0000'0000'0000) || (i64)virtual_pointer == 0) {
        // ok
    } else{
        log::out << log::RED << "ERROR:"<<log::NO_COLOR<<" VM Access violation, ptr = " << (void*)virtual_pointer << "\n";
        Assert(false);
    }
    return (void*)virtual_pointer;
}
void VirtualMachine::push_state(int index, i64 sp) {
    if(states.size() == 0) {
        states.reserve(6);
    } else {
        // Prepare 4 states and crash if we use more than that.
        // We reference fields of the last state in the main execute function
        // and if we resize allocation that memory would be invalidated.
        // We can use bucket array or something else but I
        // choose to set a limit of 4. We can increase if we run into problems.
        Assert(states.size() + 1 <= states.capacity());
    }
    states.add({});
    auto& state = states.last();
    state.stack_pointer = sp;
    Assert(index >= 0 && index < bytecode->tinyBytecodes.size());
    state.tiny_index = index;
    state.tinycode = bytecode->tinyBytecodes[index];
    state.base_pointer = sp;
    state.registers[BC_REG_LOCALS] = state.base_pointer;
}
void VirtualMachine::pop_state() {
    states.pop();
}
void BaseBytecodeStubFunction(VirtualMachine* vm, i64 sp, int index) {
    using namespace engone;
    // log::out << "Hello " << sp << " " << index << "\n";
    vm->push_state(index, sp);

    auto prev = vm->is_callback_from_stub;
    vm->is_callback_from_stub = true;
    vm->execute();
    vm->is_callback_from_stub = prev;

    vm->pop_state();
    // log::out << "Leave " << sp << " " << index << "\n";
}
engone::VoidFunction VirtualMachine::get_bytecode_pointer(int index) {
    using namespace engone;
    auto& ptr = bytecode_pointers[index];

    if(!ptr.ptr) {
        // NOTE: If we're smart, we could allocate a big chunk of
        //   executable memory for all the function pointers instead
        //   of many small once.
        ptr.size = 1024;
        u8* f = (u8*)AllocateExec(ptr.size);
        ptr.ptr = f;
        
        /*
            push rbp
            mov rdx, rsp
            sub rdx, 0x4000
            
            # prepare args to rdx

            mov rcx, 0x1000200030004000 # VM pointer
            mov r8d, 0x10002000 # tinycode index
            mov rax, 0x1000200030004000 # stub function pointer
            sub rsp, 32
            call rax
            add rsp, 32

            # prepare return values
            # mov eax, 55
            
            pop rbp
            ret
        */
        
        // TODO: Doesn't work on LINUX! Different calling convention (we should allocate 32 stack space for arguments, arguments are passed in different registers)
        
        const u32 MINI_VM_STACK_LIMIT = 0x4000;
        const u8 PROLOG[] {
            /* push rbx        */ 0x53,
            /* mov rbx, rsp    */ 0x48, 0x89, 0xe3,
            /* sub rbx, 0x4000 */ 0x48, 0x81, 0xeb, (MINI_VM_STACK_LIMIT>>0)&0xFF, (MINI_VM_STACK_LIMIT>>8)&0xFF, (MINI_VM_STACK_LIMIT>>16)&0xFF, (MINI_VM_STACK_LIMIT>>24)&0xFF,
        };
        const u8 MAIN_BODY[] { // Windows x64 calling convention (stdcall?)
            /* mov rcx, 0x1000200030004000 # VM pointer            */ 0x48, 0xB9, 0x00, 0x40, 0x00, 0x30, 0x00, 0x20, 0x00, 0x10,
            /* mov rdx, rbx                                        */ 0x48, 0x89, 0xDA,
            /* mov r8d, 0x10002000         # tinycode index        */ 0x41, 0xB8, 0x00, 0x20, 0x00, 0x10,
            /* mov rax, 0x1000200030004000 # stub function pointer */ 0x48, 0xB8, 0x00, 0x40, 0x00, 0x30, 0x00, 0x20, 0x00, 0x10,
            /* sub rsp, 32                                         */ 0x48, 0x83, 0xEC, 0x20,
            /* call rax                                            */ 0xFF, 0xD0,
            /* add rsp, 32                                         */ 0x48, 0x83, 0xC4, 0x20,
        };
        const u8 MAIN_BODY_SYSVABI[] {
            /* mov rdi, 0x1000200030004000 # VM pointer            */ 0x48, 0xBF, 0x00, 0x40, 0x00, 0x30, 0x00, 0x20, 0x00, 0x10,
            /* mov rsi, rbx                                        */ 0x48, 0x89, 0xDE,
            /* mov edx, 0x10002000         # tinycode index        */ 0xBA, 0x00, 0x20, 0x00, 0x10,
            /* mov rax, 0x1000200030004000 # stub function pointer */ 0x48, 0xB8, 0x00, 0x40, 0x00, 0x30, 0x00, 0x20, 0x00, 0x10,
            /* call rax                                            */ 0xFF, 0xD0,
        };
        const u8 EPILOG[] {
            /* pop rbx */ 0x5b,
            /* ret     */ 0xC3,
        };
        
        int head = 0;
        memcpy(f+head, PROLOG, sizeof(PROLOG));
        head += sizeof(PROLOG);
        
        // Prepare arguments
        
        //  TODO: We are always using 64 bit registers, maybe a problem? Should we be casting signed unsigned 64/32 bit integers?
        
        auto emit_mov=[&](TypeId type, int regnr, int offset){
            Assert(offset >= -128 && offset <= 127);
            if(type == TYPE_FLOAT32 || type == TYPE_FLOAT64) {
                Assert(regnr >= 0 && regnr <= 7);
                if(type == TYPE_FLOAT32) {
                    f[head++] = 0xF3; // movss
                    f[head++] = 0x0F;
                    f[head++] = 0x11;
                } else if(type == TYPE_FLOAT64) {
                    f[head++] = 0xF2;  // movsd
                    f[head++] = 0x0F;
                    f[head++] = 0x11;
                }
                f[head++] = 0x43 | (regnr<<3);
                f[head++] = offset;
            } else {
                Assert(regnr >= 0 && regnr <= 5);
                if(regnr >= 4)
                    f[head++] = 0x4C;
                else
                    f[head++] = 0x48;
                f[head++] = 0x89;
                const u8 reg_values[]{
                    //   rdi,  rsi,  rdx,  rcx,   r8,   r9
                        0x7b, 0x73, 0x53, 0x4b, 0x43, 0x4b
                };
                f[head++] = reg_values[regnr];
                f[head++] = offset;
            }
        };
        
        auto& tc = bytecode->tinyBytecodes[index];
        auto impl = tc->funcImpl;
        Assert(impl);
        int arg_offset = 16;
        int float_nr = 0;
        int norm_nr = 0;
        for (int i=0;i<impl->signature.argumentTypes.size();i++) {
            auto& arg = impl->signature.argumentTypes[i];
            // TODO: Handle 64-bit floats
            // NOTE: Haha, have fun reading this code :D
        #ifdef OS_WINDOWS
            const i32 float_mov_stride = 5;
            const u8 float_mov[]{
                /* movss [rbx+16], xmm0 */ 0xF3, 0x0F, 0x11, 0x43, 0x10, 
                /* movsd [rbx+16], xmm0 */ 0xF2, 0x0F, 0x11, 0x43, 0x10,
                /* movss [rbx+24], xmm1 */ 0xF3, 0x0F, 0x11, 0x4B, 0x18,
                /* movsd [rbx+24], xmm1 */ 0xF2, 0x0F, 0x11, 0x4B, 0x18,
                /* movss [rbx+32], xmm2 */ 0xF3, 0x0F, 0x11, 0x53, 0x20,
                /* movsd [rbx+32], xmm2 */ 0xF2, 0x0F, 0x11, 0x53, 0x20,
                /* movss [rbx+40], xmm3 */ 0xF3, 0x0F, 0x11, 0x5B, 0x28,
                /* movsd [rbx+40], xmm3 */ 0xF2, 0x0F, 0x11, 0x5B, 0x28,
            };
            const u8 norm_mov[] {
                /* mov [rbx+16], rcx */ 0x48, 0x89, 0x4B, 0x10,
                /* mov [rbx+24], rdx */ 0x48, 0x89, 0x53, 0x18, 
                /* mov [rbx+32], r8  */ 0x4C, 0x89, 0x43, 0x20,
                /* mov [rbx+40], r9  */ 0x4C, 0x89, 0x4B, 0x28,
            };
            if (i >= 0 && i <= 3 && (arg.typeId == TYPE_FLOAT32 || arg.typeId == TYPE_FLOAT64)) {
                memcpy(f+head, float_mov + i*2*float_mov_stride + (arg.typeId == TYPE_FLOAT64 ? 1 : 0), float_mov_stride);
                head += float_mov_stride;
            } else if (i >= 0 && i <= 3) {
                memcpy(f+head, norm_mov + i*4, 4);
                head+=4;
        #elif OS_LINUX
            if(((arg.typeId == TYPE_FLOAT32 || arg.typeId == TYPE_FLOAT64) && float_nr <= 7) || (norm_nr <= 5)) {
                if (arg.typeId == TYPE_FLOAT32 || arg.typeId == TYPE_FLOAT64) {
                    emit_mov(arg.typeId, float_nr, arg_offset);
                    float_nr++;
                } else {
                    emit_mov(arg.typeId, norm_nr, arg_offset);
                    norm_nr++;
                }
                arg_offset += 8;
        #else
            if(true) {
                Assert(("OS neither windows or linux?",false));
        #endif
            } else {
                // mov rax, [rsp+0x8]
                // mov [rbx+0x8], rax
                Assert(16 + i*8 >= -128 && 16 + i*8 <= 127);
                u8 mova[] { 0x48, 0x8B, 0x44, 0x24, (u8)(16 + i*8), };
                u8 movb[] { 0x48, 0x89, 0x43, (u8)(16 + i*8), };
                memcpy(f+head, mova, sizeof(mova));
                head+=sizeof(mova);
                memcpy(f+head, movb, sizeof(movb));
                head+=sizeof(movb);
            }
        }
        
        #if OS_WINDOWS
            memcpy(f+head, MAIN_BODY, sizeof(MAIN_BODY));
            *(i64*)(f + head + 0x0+2)  = (i64)this;
            *(i32*)(f + head + 0xd+2)  = (i32)index;
            *(i64*)(f + head + 0x13+2) = (i64)(void*)BaseBytecodeStubFunction;
            head += sizeof(MAIN_BODY);
        #elif OS_LINUX
            memcpy(f+head, MAIN_BODY_SYSVABI, sizeof(MAIN_BODY_SYSVABI));
            *(i64*)(f + head + 0x0+2)  = (i64)this;
            *(i32*)(f + head + 0xd+1)  = (i32)index; // NOTE: offset to immediate differs from MAIN_BODY by one byte
            *(i64*)(f + head + 0x12+2) = (i64)(void*)BaseBytecodeStubFunction;
            head += sizeof(MAIN_BODY_SYSVABI);
        #else
            Assert(("what abi to use for this OS? we handle windows and linux",false));
        #endif
        
        // Prepare return values
        if (impl->signature.returnTypes.size() > 0) {
            Assert(impl->signature.returnTypes.size() <= 1);
            if (impl->signature.returnTypes[0].typeId == TYPE_FLOAT32) {
                // movss xmm0, [rbx-0x8]
                u8 mov[] { 0xF3, 0x0F, 0x10, 0x43, 0xF8, };
                memcpy(f+head, mov, sizeof(mov));
                head+=sizeof(mov);
            } else  if (impl->signature.returnTypes[0].typeId == TYPE_FLOAT64) {
                // movsd xmm0, [rbx-0x8]
                u8 mov[] { 0xF2, 0x0F, 0x10, 0x43 };
                memcpy(f+head, mov, sizeof(mov));
                head+=sizeof(mov);
            } else {
                // mov rax, [rbx-8]
                u8 mov[] { 0x48, 0x8b, 0x43, 0xf8, };
                memcpy(f+head, mov, sizeof(mov));
                head+=sizeof(mov);
            }
        }
        
        memcpy(f+head, EPILOG, sizeof(EPILOG));
        head += sizeof(EPILOG);
        
        Assert(head < ptr.size);
        
        // OutputAsHex("asm.log", (u8*)f, head);
    }

    return (engone::VoidFunction)ptr.ptr;
}