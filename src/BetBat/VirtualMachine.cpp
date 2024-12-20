#include "BetBat/VirtualMachine.h"

// needed for FRAME_SIZE
#include "BetBat/Generator.h"

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
    reset();
    // cmdArgsBuffer.resize(0);
    // engone::Free(cmdArgs.ptr, sizeof(Language::Slice<char>)*cmdArgs.len);
    // cmdArgs.ptr = nullptr;
    // cmdArgs.len = 0;
}
void VirtualMachine::reset(){
    memset((void*)registers, 0, sizeof(registers));
    memset(stack.data(), 0, stack.max);
}
// void VirtualMachine::setCmdArgs(const DynamicArray<std::string>& inCmdArgs){
//     using namespace engone;
//     Assert(false); // this needs a rewrite
//     // cmdArgs.resize(inCmdArgs.size());
//     // cmdArgs.ptr = TRACK_ARRAY_ALLOC(Language::Slice<char>);
//     cmdArgs.ptr = (Language::Slice<char>*)engone::Allocate(sizeof(Language::Slice<char>)*inCmdArgs.size());
//     cmdArgs.len = inCmdArgs.size();
//     u64 totalText = 0;
//     for(int i=0;i<inCmdArgs.size();i++){
//         totalText += inCmdArgs[i].length()+1;
//     }
//     cmdArgsBuffer.resize(totalText);
//     for(int i=0;i<inCmdArgs.size();i++){
//         char* ptr = cmdArgsBuffer.data + cmdArgsBuffer.used;
//         memcpy(ptr, inCmdArgs[i].data(),inCmdArgs[i].length());
//         *(ptr + inCmdArgs[i].length()) = 0;
//         cmdArgs.ptr[i].ptr = ptr;
//         cmdArgs.ptr[i].len = inCmdArgs[i].length();
//         // log::out << cmdArgs.ptr[i].ptr<<" "<<inCmdArgs[i].length()<<"\n";
//         // log::out << cmdArgs.ptr[i];
//         cmdArgsBuffer.used = inCmdArgs[i].length() + 1;
//     }
// }
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

// void VirtualMachine::execute(Bytecode* bytecode){
//     executePart(bytecode, 0, bytecode->length());
// }
void VirtualMachine::init_stack(int stack_size) {
    stack.resize(stack_size);
    memset(stack.data(), 0, stack.max);
    memset((void*)registers, 0, sizeof(registers));
}
TinyBytecode* VirtualMachine::fetch_tinycode(Bytecode* bytecode, const std::string& tinycode_name){
    for(int i=0;i<bytecode->tinyBytecodes.size();i++) {
        if(bytecode->tinyBytecodes[i]->name == tinycode_name) {
            return bytecode->tinyBytecodes[i];
        }
    }
    return nullptr;
}

void VirtualMachine::execute(Bytecode* bytecode, const std::string& tinycode_name, bool apply_related_relocations, CompileOptions* options){
    using namespace engone;

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

    {
        CallFrame frame{};
        frame.func = bytecode->tinyBytecodes[tiny_index];
        frame.return_address = 0;
        call_stack.add(frame);
    }

    DynamicArray<TinyBytecode*> checked_codes{};
    bool compute_related_codes = true; // we compute so that we only load relevant libraries

    if (apply_related_relocations || compute_related_codes) {
        // TODO: Appling partial relocations and checking for dependencies (other tinycodes)
        //   may be expensive. Avoiding partial relocation might be best.
        //   Also, if relocations have been applied once, we don't need to do so again.
        DynamicArray<TinyBytecode*> codes_to_check{};
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
                    // log::out << log::RED << "Incomplete call relocation, "<<t->name<<"\n";
                    return;
                }
            }
        }
    } else {
        FuncImpl* unresolved_func = nullptr;
        for(auto& t : bytecode->tinyBytecodes) {
            bool yes = t->applyRelocations(bytecode, false, &unresolved_func);
            FuncImpl* unresolved_func = nullptr;
            if(!yes) {
                error.type = VM_UNRESOLVED_CALL;
                error.message = unresolved_func->astFunction->name + " called from " + t->name;
                // log::out << log::RED << "Incomplete call relocation, "<<t->name<<"\n";
                return;
            }
        }
    }

    struct LibFunc {
        DynamicArray<ExternalRelocation*> relocs;
        VoidFunction func_ptr;
    };
    struct Lib {
        std::unordered_map<std::string, LibFunc*> functions;
        DynamicLibrary dll;
    };
    
    // TODO: Reuse loaded dlls and function pointers because we
    //   may run VM again. Mapping function pointers multiple times is unnecessary.
    bool any_failure = false;
    std::unordered_map<std::string, Lib*> libs;
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
    DynamicArray<VoidFunction> dll_functions{};
    DynamicArray<std::string> dll_function_names{};
    for(int i=0;i<bytecode->externalRelocations.size();i++) {
        auto& r = bytecode->externalRelocations[i];
        // log::out << log::LIME << r.name << " " << r.library_path<<"\n";
        if(r.library_path.size() == 0) {
            any_failure = true;
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

        auto pair_lib = libs.find(r.library_path);
        Lib* lib = nullptr;
        if(pair_lib == libs.end()) {
            lib = new Lib();
            libs[r.library_path] = lib;
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
    }
    
    for(auto& pair_lib : libs) {
        // log::out << "VM lib "<< pair_lib.first<<"\n";
        pair_lib.second->dll = LoadDynamicLibrary(pair_lib.first, false); // false = don't log error
        if(!pair_lib.second->dll) {
            // TODO: This solution is cheeky.
            int at = pair_lib.first.find_last_of(".");
            std::string alt_path = pair_lib.first.substr(0, at);
            alt_path += ".dll";
            pair_lib.second->dll = LoadDynamicLibrary(alt_path, false);
        }
        if(!pair_lib.second->dll) {
            any_failure = true;
            log::out << log::RED << "Could not load library "<<pair_lib.first<<"\n";
            continue;
        }
        for(auto& pair_fn : pair_lib.second->functions) {
            pair_fn.second->func_ptr = GetFunctionPointer(pair_lib.second->dll, pair_fn.first);
            if(!pair_fn.second->func_ptr) {
                any_failure = true;
                log::out << log::RED << "Could not load function pointer "<<pair_fn.first<<"\n";
                continue;
            } else {
                int index = dll_functions.size();
                dll_functions.add(pair_fn.second->func_ptr);
                dll_function_names.add(pair_fn.first);
                // APPLY RELOCATIONS
                for(auto r : pair_fn.second->relocs) {
                    auto& t = bytecode->tinyBytecodes[r->tinycode_index];
                    // log::out << "Apply reloc "<< r->pc << ", "<<r->name<<"\n";
                    *(i32*)&t->instructionSegment[r->pc] = Bytecode::BEGIN_DLL_FUNC_INDEX + index;
                }
            }
        }
    }
    if(any_failure){
        return;
    }
    
    // log::out << log::GOLD << "VirtualMachine:\n";

    // TODO: Setup argc, argv on the stack with betcall convention
    
    push_offsets.add(0);

    if(stack.max == 0) {
        // only init stack if we haven't already
        // caller to VM may want to init there own stack with a
        // large size with some pre-initialized content.
        init_stack();
    }

    bool force_mapping = false;
    bool temp_ptr_was_mapped = false;
    if(bytecode->target == TARGET_ARM) {
        force_mapping = true;
        u64 stack_start = 0xFFFF'FFFF;
        bool yes = add_memory_mapping(stack_start-stack.max, (u64)stack.data(), stack.max);
        stack_pointer = stack_start;
        base_pointer = stack_pointer;
    } else {
        stack_pointer = (i64)(stack.data() + stack.max);
        base_pointer = stack_pointer;
    }
    registers[BC_REG_LOCALS] = base_pointer;
    
    // #define CHECK_PTR_MAPPED(PTR) if(force_mapping && !temp_ptr_was_mapped) { log::out << log::RED << "PTR "<<PTR<<" was not mapped\n"; return; }
    auto CHECK_PTR_MAPPED = [&](void* PTR) {
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

    u64 expectedStackPointer = stack_pointer;


    auto tp = StartMeasure();

    #ifdef ILOG
    #undef _ILOG
    #define _ILOG(X) if(!silent){X};
    #endif
    
    // _ILOG(log::out << "sp = "<<sp<<"\n";)

    // auto& instructions = tinycode->instructionSegment;
    #define instructions tinycode->instructionSegment
    i64 userAllocatedBytes=0;
    u64 executedInstructions = 0;
    bool enable_fncall_logging = true;
    std::unordered_map<std::string, u64> number_of_fncalls{};
    // u64 length = bytecode->instructionSegment.used;
    // if(length==0)
    //     log::out << log::YELLOW << "VirtualMachine ran bytecode with zero instructions. Bug?\n";
    // Instruction* codePtr = (Instruction*)bytecode->instructionSegment.data;
    // Bytecode::Location* prevLocation = nullptr;
    BytecodePrintCache print_cache{};
    bool running = true;

    DynamicArray<int> misalignments{}; // used by BC_ALLOC_ARGS and BC_FREE_ARGS

    // TODO: x64 had a bug with push_offsets, ALLOC_ARGS and SET_ARG
    //   I applied the same here but didn't test it.

    // NOTE: We don't need to track relative stack pointer and stack moments like we do in x64 generator because
    //   the VM is running in real time. We have the actual stack pointer and can align it to 16-bytes whenever we want.

    i64 pc = 0;
    i64 prev_pc;
    InstructionOpcode opcode;
    BCRegister op0=BC_REG_INVALID, op1, op2;
    InstructionControl control;
    i64 imm;

    CALLBACK_ON_ASSERT(
        log::out << log::RED << "Dump of bytecode\n";
        tinycode->print(0,-1, bytecode);
        log::out << "Asserted at instruction " << log::CYAN << prev_pc << "\n";
    )

    struct Breakpoint {
        int pc;
        // TODO: break on tinycode name
    };
    DynamicArray<Breakpoint> breakpoints{};

    // hardcoded breakpoints when debugging
    // breakpoints.add({945});

    bool logging = false;
    bool interactive = false;
    if(options) {
        if(options->interactive_vm)
            interactive = true;
        if(options->logged_vm)
            logging = true;
    }
    // interactive = true;
    // logging = true;
    
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

        opcode = (InstructionOpcode)instructions[pc++];
        executedInstructions++;
        
        // BCRegister op0=BC_REG_INVALID, op1, op2;
        // InstructionControl control;
        // i64 imm;
        
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

        executedInstructions++;
        switch (opcode) {
        case BC_HALT: {
            running = false;
            // if(logging)
            log::out << log::GREEN << "HALT (instruction)\n";
        } break;
        case BC_NOP: {
        } break;
        case BC_ALLOC_LOCAL:
        case BC_ALLOC_ARGS: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i16*)(instructions.data() + pc);
            pc += 2;

            // if (imm == 0) // nop
            //     break;

            registers[BC_REG_LOCALS] = base_pointer;

            if(opcode == BC_ALLOC_ARGS) {
                push_offsets.add(0);
                i64 misalignment = (stack_pointer + imm) % FRAME_SIZE;
                misalignments.add(misalignment);
                if(misalignment != 0) {
                    imm += FRAME_SIZE - misalignment;
                }
            }
            stack_pointer -= imm;
            
            if(op0 != BC_REG_INVALID)
                registers[op0] = stack_pointer;
            
            has_return_values_on_stack = false; // alloc_local overwrites return values
            ret_offset = 0;
        } break;
        case BC_FREE_LOCAL:
        case BC_FREE_ARGS: {
            imm = *(i16*)(instructions.data() + pc);
            pc += 2;
            if(opcode == BC_FREE_ARGS) {
                push_offsets.pop();
                i64 misalignment = misalignments.last();
                misalignments.pop();
                if(misalignment != 0) {
                    imm += FRAME_SIZE - misalignment;
                }
            }
            stack_pointer += imm;

            if(has_return_values_on_stack) {
                ret_offset -= imm;
            }
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
            int size = GET_CONTROL_SIZE(control);

            void* ptr = map_pointer(stack_pointer + push_offsets.last() + imm, temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);
            ptr_from_mov = ptr;

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
            int size = GET_CONTROL_SIZE(control);
            
            Assert(imm < 0);
            // int FRAME_SIZE = 8 + 8; // nocheckin TODO: Do not assume frame size, maybe we disable base pointer!
            void* ptr = map_pointer(base_pointer + imm, temp_ptr_was_mapped);
            CHECK_PTR_MAPPED(ptr);
            ptr_from_mov = ptr;

            if(size == CONTROL_8B)       *(i8*) ptr = registers[op0];
            else if(size == CONTROL_16B) *(i16*)ptr = registers[op0];
            else if(size == CONTROL_32B) *(i32*)ptr = registers[op0];
            else if(size == CONTROL_64B) *(i64*)ptr = registers[op0];

        } break;
        case BC_GET_VAL: {
            op0 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            int size = GET_CONTROL_SIZE(control);
            
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

            if(imm >= Bytecode::BEGIN_DLL_FUNC_INDEX) {
                auto f = dll_functions[imm - Bytecode::BEGIN_DLL_FUNC_INDEX];
                // fix arguments?
                if(c == STDCALL) {
                    // Makehshift is a bad name
                    // it's more like a StackSwitcher_stdcall
                    #ifdef OS_WINDOWS
                    Makeshift_stdcall(f, (void*)stack_pointer);
                    #else
                    Assert(("Virtual machine does not support imported functions when using unixcall (System V ABI convention)",false));
                    #endif
                } else if(c == UNIXCALL) {
                    // Makeshift_unixcall(f, (void*)stack_pointer);
                    #ifdef OS_LINUX
                    Makeshift_sysvcall(f, (void*)stack_pointer);
                    #else
                    Assert(("Virtual machine does not support imported functions when using unixcall (System V ABI convention)",false));
                    #endif
                } else { 
                    // Makeshift function for betcall?
                    Assert(false); 
                }
                has_return_values_on_stack = true; // NOTE: potential return values
                ret_offset = 0;
            } else {
                int new_tiny_index = imm-1;
                if(new_tiny_index < 0 && new_tiny_index >= bytecode->tinyBytecodes.size()) {
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
                // not valid index
                running = false;
                break;
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
        } break;
        case BC_RET: {
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

            log::out << log::RED << "Cannot access imported variable in virtual machine (not implemented)\n";
            running = false;
            break;
            // Assert(bytecode->dataSegment.size() > imm);
            // registers[op0] = (u64)(bytecode->dataSegment.data() + imm);
        } break;
        case BC_CODEPTR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;

            registers[op0] = imm;
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

            Assert((u64)ptr > 0x8000000); // we rarely access memory below this address, nice way to catch bugs
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
            Assert((u64)ptr > 0x8000000); // we rarely access memory below this address, nice way to catch bugs
            Assert((u64)ptr1 > 0x8000000); // we rarely access memory below this address, nice way to catch bugs
            Assert(registers[op2] < 0x100000); // we rarely memzero memory larger than this
            memcpy(ptr, ptr1, registers[op2]);
        } break;
        case BC_ASM: {
            u8 inputs = (u8)instructions[pc++];
            u8 outputs = (u8)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            if(!silent) {
                log::out << log::RED << "VirtualMachine cannot execute inline assembly!\n";
            }
            // TODO: Print what assembly we tried to execute.
            //   Show call stack too?
            running = false;
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
    auto time = StopMeasure(tp);
    if(!silent){
        log::out << "\n";
        log::out << log::LIME << "Executed "<<executedInstructions<<" insts. in "<<FormatTime(time)<< " ("<<FormatUnit(executedInstructions/time)<< " inst/s)\n";
        #ifdef ILOG_REGS
        printRegisters();
        #endif
    }
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
    // find mapping
    for(int i=0;i<memory_map.size();i++) {
        auto& map = memory_map[i];
        if(virtual_pointer >= map.start_address && virtual_pointer < map.start_address + map.size) {
            was_mapped = true;
            return (void*)(map.physical_start_address + virtual_pointer - map.start_address);
        }
    }
    was_mapped = false;
    // suspicious pointer
    Assert((i64)virtual_pointer >= 0x100000 && (i64)virtual_pointer < 0x0010'0000'0000'0000);
    return (void*)virtual_pointer;
}