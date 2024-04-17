#include "BetBat/VirtualMachine.h"

// needed for FRAME_SIZE
#include "BetBat/Generator.h"

#define BITS(P,B,E,S) ((P<<(S-E))>>B)

#define DECODE_OPCODE(I) I->opcode
#define DECODE_REG0(I) I->op0
#define DECODE_REG1(I) I->op1
#define DECODE_REG2(I) I->op2

// #define SP_CHANGE(incr) log::out << "sp "<<(i64)(sp-(u64)stack.data-(incr))<<" -> "<<(i64)(sp-(u64)stack.data)<<"\n";
#define SP_CHANGE(incr)

// #define DECODE_TYPE(ptr) (*((u8*)ptr+1))


void VirtualMachine::cleanup(){
    stack.resize(0);
    cmdArgsBuffer.resize(0);
    engone::Free(cmdArgs.ptr, sizeof(Language::Slice<char>)*cmdArgs.len);
    cmdArgs.ptr = nullptr;
    cmdArgs.len = 0;
    reset();
}
void VirtualMachine::reset(){
    memset((void*)registers, 0, sizeof(registers));
    memset(stack.data, 0, stack.max);
}
void VirtualMachine::setCmdArgs(const DynamicArray<std::string>& inCmdArgs){
    using namespace engone;
    Assert(false); // this needs a rewrite
    // cmdArgs.resize(inCmdArgs.size());
    // cmdArgs.ptr = TRACK_ARRAY_ALLOC(Language::Slice<char>);
    cmdArgs.ptr = (Language::Slice<char>*)engone::Allocate(sizeof(Language::Slice<char>)*inCmdArgs.size());
    cmdArgs.len = inCmdArgs.size();
    u64 totalText = 0;
    for(int i=0;i<inCmdArgs.size();i++){
        totalText += inCmdArgs[i].length()+1;
    }
    cmdArgsBuffer.resize(totalText);
    for(int i=0;i<inCmdArgs.size();i++){
        char* ptr = cmdArgsBuffer.data + cmdArgsBuffer.used;
        memcpy(ptr, inCmdArgs[i].data(),inCmdArgs[i].length());
        *(ptr + inCmdArgs[i].length()) = 0;
        cmdArgs.ptr[i].ptr = ptr;
        cmdArgs.ptr[i].len = inCmdArgs[i].length();
        // log::out << cmdArgs.ptr[i].ptr<<" "<<inCmdArgs[i].length()<<"\n";
        // log::out << cmdArgs.ptr[i];
        cmdArgsBuffer.used = inCmdArgs[i].length() + 1;
    }
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

// void VirtualMachine::execute(Bytecode* bytecode){
//     executePart(bytecode, 0, bytecode->length());
// }
void VirtualMachine::execute(Bytecode* bytecode, const std::string& tinycode_name){
    using namespace engone; 

    for(auto& t : bytecode->tinyBytecodes) {
        bool yes = t->applyRelocations(bytecode);
        if(!yes) {
            log::out << "Incomplete call relocation\n";
            return;
        }
    }

    Assert(bytecode);
    auto nativeRegistry = NativeRegistry::GetGlobal();
    // if(!bytecode->nativeRegistry) {
    //     bytecode->nativeRegistry = NativeRegistry::GetGlobal();
    //     // bytecode->nativeRegistry = NativeRegistry::Create();
    //     // bytecode->nativeRegistry->initNativeContent();
    // }

    struct LibFunc {
        DynamicArray<ExternalRelocation*> relocs;
        VoidFunction func_ptr;
    };
    struct Lib {
        std::unordered_map<std::string, LibFunc*> functions;
        DynamicLibrary dll;
    };
    
    bool any_failure = false;
    std::unordered_map<std::string, Lib*> libs;
    DynamicArray<VoidFunction> dll_functions{};
    DynamicArray<std::string> dll_function_names{};
    for(int i=0;i<bytecode->externalRelocations.size();i++) {
        auto& r = bytecode->externalRelocations[i];
        // log::out << log::LIME << r.name << " " << r.library_path<<"\n";
        if(r.library_path.size() == 0) {
            any_failure = true;
            continue;
        }
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
        pair_lib.second->dll = LoadDynamicLibrary(pair_lib.first);
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
                    *(i32*)&t->instructionSegment[r->pc] = Bytecode::BEGIN_DLL_FUNC_INDEX + index;
                }
            }
        }
    }
    if(any_failure){
        return;
    }
    
    // if(bytecode->externalRelocations.size()>0){
    //     log::out << log::RED << "VirtualMachine does not support symbol relocations! Don't use function with @import annotation.\n";
    //     return;
    // }
    // _VLOG(
    //     if(!silent){
    //     if(startInstruction==0 && endInstruction == bytecode->length())
    //         log::out <<log::BLUE<< "##   VirtualMachine  ##\n";
    //     else
    //         log::out <<log::BLUE<< "##   VirtualMachine ("<<startInstruction<<" - "<<endInstruction<<")  ##\n";
    //     }
    // )
    log::out << log::GOLD << "VirtualMachine:\n";

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

    stack.resize(0x10'0000);
    memset(stack.data, 0, stack.max);
    memset((void*)registers, 0, sizeof(registers));

    // TODO: Setup argc, argv on the stack with betcall convention
    
    push_offsets.add(0);

    i64 pc = 0;
    stack_pointer = (i64)(stack.data + stack.max);
    base_pointer = stack_pointer;
    
    auto CHECK_STACK = [&]() {
        if(stack_pointer < (i64)stack.data || stack_pointer > (i64)(stack.data+stack.max)) {
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
    // bool logging = false;
    bool logging = true;

    // auto& instructions = tinycode->instructionSegment;
    #define instructions tinycode->instructionSegment
    i64 userAllocatedBytes=0;
    u64 executedInstructions = 0;
    // u64 length = bytecode->instructionSegment.used;
    // if(length==0)
    //     log::out << log::YELLOW << "VirtualMachine ran bytecode with zero instructions. Bug?\n";
    // Instruction* codePtr = (Instruction*)bytecode->instructionSegment.data;
    // Bytecode::Location* prevLocation = nullptr;
    int prev_tinyindex = -1;
    int prev_line = -1;
    bool running = true;
    while(running) {
        i64 prev_pc = pc;
        if(pc>=(u64)tinycode->instructionSegment.used)
            break;

        InstructionType opcode = (InstructionType)instructions[pc++];
        
        BCRegister op0=BC_REG_INVALID, op1, op2;
        InstructionControl control;
        InstructionCast cast;
        i64 imm;
        
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
            auto line_index = tinycode->index_of_lines[prev_pc] - 1;
            if(line_index != -1) {
                auto& line = tinycode->lines[line_index];

                if(line.line_number != prev_line || tiny_index != prev_tinyindex) {
                    if(tiny_index != prev_tinyindex) {
                        log::out << log::GOLD  << tinycode->name <<"\n";
                        prev_tinyindex = tiny_index;
                    }
                    prev_tinyindex = tiny_index;
                    log::out << log::CYAN << line.line_number << "| " << line.text<<"\n";
                    prev_line = line.line_number;
                }
            } else if(tiny_index != prev_tinyindex) {
                log::out << log::GOLD  << tinycode->name <<"\n";
                prev_tinyindex = tiny_index;
            }
            tinycode->print(prev_pc, prev_pc + 1, bytecode, &dll_function_names);
        }

        void* ptr_from_mov = nullptr;

        executedInstructions++;
        switch (opcode) {
        case BC_HALT: {
            running = false;
            if(logging)
                log::out << "HALT\n";
        } break;
        case BC_NOP: {
        } break;
        case BC_ALLOC_LOCAL: {
            // Assert(push_offset == 0); // Ensure that no push instructions executed before nocheckin
            op0 = (BCRegister)instructions[pc++];
            imm = *(i16*)(instructions.data() + pc);
            pc += 2;
            registers[BC_REG_LOCALS] = base_pointer;
            stack_pointer -= imm;
            
            if(op0 != BC_REG_INVALID)
                registers[op0] = stack_pointer;

            has_return_values_on_stack = false; // alloc_local overwrites return values
            ret_offset = 0;
        } break;
        case BC_FREE_LOCAL: {
            // Assert(push_offset == 0); // Ensure that no push instructions executed before nocheckin
            imm = *(i16*)(instructions.data() + pc);
            pc += 2;
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
            void* ptr = ptr = (void*)(registers[mop] + imm);
            ptr_from_mov = ptr;
            
            int size = GET_CONTROL_SIZE(control);
            if(opcode == BC_MOV_MR || opcode == BC_MOV_MR_DISP16) {
                if(size == CONTROL_8B)       *(i8*) ptr = registers[op1];
                else if(size == CONTROL_16B) *(i16*)ptr = registers[op1];
                else if(size == CONTROL_32B) *(i32*)ptr = registers[op1];
                else if(size == CONTROL_64B) *(i64*)ptr = registers[op1];
            } else {
                if(size == CONTROL_8B)       registers[op0] = *(i8*) ptr;
                else if(size == CONTROL_16B) registers[op0] = *(i16*)ptr;
                else if(size == CONTROL_32B) registers[op0] = *(i32*)ptr;
                else if(size == CONTROL_64B) registers[op0] = *(i64*)ptr;
            }
        } break;
        case BC_SET_ARG: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            control = (InstructionControl)instructions[pc++];
            int size = GET_CONTROL_SIZE(control);

            void* ptr = (void*)(stack_pointer + push_offsets.last() + imm);

            if(size == CONTROL_8B)       *(i8*) ptr = registers[op0];
            else if(size == CONTROL_16B) *(i16*)ptr = registers[op0];
            else if(size == CONTROL_32B) *(i32*)ptr = registers[op0];
            else if(size == CONTROL_64B) *(i64*)ptr = registers[op0];
        } break;
        case BC_GET_PARAM: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            control = (InstructionControl)instructions[pc++];
            int size = GET_CONTROL_SIZE(control);
            
            int FRAME_SIZE = 8 + 8; // nochecking TODO: Do not assume frame size, maybe we disable base pointer!
            void* ptr = (void*)(base_pointer + FRAME_SIZE + imm);

            if(size == CONTROL_8B)       registers[op0] = *(i8*) ptr;
            else if(size == CONTROL_16B) registers[op0] = *(i16*)ptr;
            else if(size == CONTROL_32B) registers[op0] = *(i32*)ptr;
            else if(size == CONTROL_64B) registers[op0] = *(i64*)ptr;
        } break;
         case BC_SET_RET: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            control = (InstructionControl)instructions[pc++];
            int size = GET_CONTROL_SIZE(control);
            
            Assert(imm < 0);
            // int FRAME_SIZE = 8 + 8; // nochecking TODO: Do not assume frame size, maybe we disable base pointer!
            void* ptr = (void*)(base_pointer + imm);
            
            if(size == CONTROL_8B)       *(i8*) ptr = registers[op0];
            else if(size == CONTROL_16B) *(i16*)ptr = registers[op0];
            else if(size == CONTROL_32B) *(i32*)ptr = registers[op0];
            else if(size == CONTROL_64B) *(i64*)ptr = registers[op0];

        } break;
        case BC_GET_VAL: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            control = (InstructionControl)instructions[pc++];
            int size = GET_CONTROL_SIZE(control);
            
            Assert(has_return_values_on_stack);
            Assert(imm < 0);
            int FRAME_SIZE = 8 + 8; // nochecking TODO: Do not assume frame size, maybe we disable base pointer!
            // NOTE: push_offset makes sure push and pop instructions doesn't mess with vals/args registers/references
            void* ptr = (void*)(stack_pointer + ret_offset + push_offsets.last() + imm - FRAME_SIZE);

            if(size == CONTROL_8B)       registers[op0] = *(i8*) ptr;
            else if(size == CONTROL_16B) registers[op0] = *(i16*)ptr;
            else if(size == CONTROL_32B) registers[op0] = *(i32*)ptr;
            else if(size == CONTROL_64B) registers[op0] = *(i64*)ptr;
        } break;
        case BC_GET_LOCAL: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;

            registers[op0] = registers[BC_REG_LOCALS] + imm;
        } break;
        case BC_PUSH: {
            op0 = (BCRegister)instructions[pc++];
            
            stack_pointer -= 8;
            CHECK_STACK();
            *(i64*)stack_pointer = registers[op0];
            push_offsets.last() += 8;

            has_return_values_on_stack = false;
            ret_offset = 0;
        } break;
        case BC_POP: {
            op0 = (BCRegister)instructions[pc++];
            
            registers[op0] = *(i64*)stack_pointer;
            stack_pointer += 8;
            CHECK_STACK();
            push_offsets.last() -= 8;
        } break;
        case BC_LI32: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            registers[op0] = imm;
        } break;
        case BC_LI64: {
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
            
            if(op0 == 0)
                pc += imm;
        } break;
        case BC_JNZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            if(op0 != 0)
                pc += imm;
        } break;
        case BC_CALL: {
            LinkConventions l = (LinkConventions)instructions[pc++];
            CallConventions c = (CallConventions)instructions[pc++];
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
                    Makeshift_stdcall(f, (void*)stack_pointer);
                } else if(c == UNIXCALL) {
                    // Makeshift_unixcall(f, (void*)stack_pointer);
                    Assert(("Virtual machine does not support imported functions when using unixcall (System V ABI convention)",false));
                } else { 
                    // Makeshift function for betcall?
                    Assert(false); 
                }
                has_return_values_on_stack = true; // NOTE: potential return values
                ret_offset = 0;
            } else if(imm < 0) {
                executeNative(imm);
                has_return_values_on_stack = true; // NOTE: potential return values
                ret_offset = 0;
            } else {
                int new_tiny_index = imm-1;
                stack_pointer -= 8;
                CHECK_STACK();
                *(i64*)stack_pointer = pc | ((i64)tiny_index << 32);

                stack_pointer -= 8;
                CHECK_STACK();
                *(i64*)stack_pointer = base_pointer;
                base_pointer = stack_pointer;
                registers[BC_REG_LOCALS] = base_pointer;

                pc = 0;
                tiny_index = new_tiny_index;
                tinycode = bytecode->tinyBytecodes[tiny_index];

                push_offsets.add(0);
            }

        } break;
        case BC_RET: {
            i64 diff = stack_pointer - (i64)stack.data;
            if(diff == (i64)(stack.max)) {
                running = false;
                break;
            }

            base_pointer = *(i64*)stack_pointer;
            stack_pointer += 8;
            CHECK_STACK();

            i64 encoded_pc = *(i64*)stack_pointer;
            stack_pointer += 8;
            CHECK_STACK();

            registers[BC_REG_LOCALS] = base_pointer;

            has_return_values_on_stack = true;
            ret_offset = 0;

            pc = encoded_pc & 0xFFFF'FFFF;
            tiny_index = encoded_pc >> 32;
            tinycode = bytecode->tinyBytecodes[tiny_index];

            push_offsets.pop();
        } break;
        case BC_DATAPTR: {
            // Assert(false);
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;

            Assert(bytecode->dataSegment.size() > imm);
            registers[op0] = (u64)(bytecode->dataSegment.data() + imm);
        } break;
        case BC_CODEPTR: {
            Assert(false);
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
        } break;
        case BC_CAST: {
            op0 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            cast = (InstructionCast)instructions[pc++];
            u8 fsize = 1 << GET_CONTROL_SIZE(control);
            u8 tsize = 1 << GET_CONTROL_CONVERT_SIZE(control);

            switch(cast){
            case CAST_UINT_UINT:
            case CAST_UINT_SINT:
            case CAST_SINT_UINT:
            case CAST_SINT_SINT: {
                u64 tmp = registers[op0];
                if(tsize == 1) *(u8*)&registers[op0] = tmp;
                else if(tsize == 2) *(u16*)&registers[op0] = tmp;
                else if(tsize == 4) *(u32*)&registers[op0] = tmp;
                else if(tsize == 8) *(u64*)&registers[op0] = tmp;
                else Assert(false);
            } break;
            case CAST_UINT_FLOAT: {
                u64 tmp = registers[op0];
                if(tsize == 4) *(float*)&registers[op0] = tmp;
                else if(tsize == 8) *(double*)&registers[op0] = tmp;
                else Assert(false);
            } break;
            case CAST_SINT_FLOAT: {
                i64 tmp = registers[op0];
                if(tsize == 4) *(float*)&registers[op0] = tmp;
                else if(tsize == 8) *(double*)&registers[op0] = tmp;
                else Assert(false);
            } break;
            case CAST_FLOAT_UINT: {
                if(fsize == 4){
                    float tmp = *(float*)&registers[op0];
                    if(tsize == 1) *(u8*)&registers[op0] = tmp;
                    else if(tsize == 2) *(u16*)&registers[op0] = tmp;
                    else if(tsize == 4) *(u32*)&registers[op0] = tmp;
                    else if(tsize == 8) *(u64*)&registers[op0] = tmp;
                    else Assert(false);
                } else if(fsize == 8) {
                    double tmp = *(double*)&registers[op0];
                    if(tsize == 1) *(u8*)&registers[op0] = tmp;
                    else if(tsize == 2) *(u16*)&registers[op0] = tmp;
                    else if(tsize == 4) *(u32*)&registers[op0] = tmp;
                    else if(tsize == 8) *(u64*)&registers[op0] = tmp;
                    else Assert(false);
                } else Assert(false);
            } break;
            case CAST_FLOAT_SINT: {
                if(fsize == 4){
                    float tmp = *(float*)&registers[op0];
                    if(tsize == 1) *(i8*)&registers[op0] = tmp;
                    else if(tsize == 2) *(i16*)&registers[op0] = tmp;
                    else if(tsize == 4) *(i32*)&registers[op0] = tmp;
                    else if(tsize == 8) *(i64*)&registers[op0] = tmp;
                    else Assert(false);
                } else if(fsize == 8) {
                    double tmp = *(double*)&registers[op0];
                    if(tsize == 1) *(i8*)&registers[op0] = tmp;
                    else if(tsize == 2) *(i16*)&registers[op0] = tmp;
                    else if(tsize == 4) *(i32*)&registers[op0] = tmp;
                    else if(tsize == 8) *(i64*)&registers[op0] = tmp;
                    else Assert(false);
                } else Assert(false);
            } break;
            case CAST_FLOAT_FLOAT: {
                if(fsize == 4 && tsize == 4) *(float*)&registers[op0] = *(float*)registers[op0];
                else if(fsize == 4 && tsize == 8) *(double*)&registers[op0] = *(float*)registers[op0];
                else if(fsize == 8 && tsize == 4) *(float*)&registers[op0] = *(double*)registers[op0];
                else if(fsize == 8 && tsize == 8) *(double*)&registers[op0] = *(double*)registers[op0];
                else Assert(false);
            } break;
            default: Assert(false);
            }
            
        } break;
        case BC_MEMZERO: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            memset((void*)registers[op0],0, registers[op1]);
        } break;
        case BC_MEMCPY: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            op2 = (BCRegister)instructions[pc++];
            memcpy((void*)registers[op0],(void*)registers[op1], registers[op2]);
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

            // nocheckin, we assume 32 bit float operation
            
            if(control & CONTROL_FLOAT_OP) {
                switch(opcode){
                    #define OP(P) *(float*)&registers[op0] = *(float*)&registers[op0] P *(float*)&registers[op1]; break;
                    case BC_ADD: OP(+)
                    case BC_SUB: OP(-)
                    case BC_MUL: OP(*)
                    case BC_DIV: OP(/)
                    case BC_MOD: *(float*)&registers[op0] = fmodf(*(float*)&registers[op0], *(float*)&registers[op1]); break;
                    case BC_EQ:  OP(==)
                    case BC_NEQ: OP(!=)
                    case BC_LT:  OP(<)
                    case BC_LTE: OP(<=)
                    case BC_GT:  OP(>)
                    case BC_GTE: OP(>=)
                    default: Assert(false);
                    #undef OP
                }
            } else {
                switch(opcode){
                    #define OP(P) registers[op0] = registers[op0] P registers[op1]; break;
                    case BC_ADD: OP(+)
                    case BC_SUB: OP(-)
                    case BC_MUL: OP(*)
                    case BC_DIV: OP(/)
                    case BC_MOD: OP(%)
                    case BC_EQ:  OP(==)
                    case BC_NEQ: OP(!=)
                    case BC_LT:  OP(<)
                    case BC_LTE: OP(<=)
                    case BC_GT:  OP(>)
                    case BC_GTE: OP(>=)
                    default: Assert(false);
                    #undef OP
                }
            }
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
            
            switch(opcode){
                #define OP(P) registers[op0] = registers[op0] P registers[op1]; break;
                case BC_BXOR: OP(^)
                case BC_BOR: OP(|)
                case BC_BAND: OP(&)
                case BC_BLSHIFT: OP(<<)
                case BC_BRSHIFT: OP(>>)
                case BC_LAND: OP(&&)
                case BC_LOR: OP(||)
                case BC_LNOT: registers[op0] = !registers[op1];
                default: Assert(false);
                #undef OP
            }
        } break;
        #ifdef gone
        // case BC_CALL: {
        //     i32 addr = *(i32*)(codePtr + pc);
        //     pc++;
            
        //     LinkConventions linkConvention = (LinkConventions)DECODE_REG0(inst);
        //     CallConventions callConvention = (CallConventions)DECODE_REG1(inst);
        //     Assert(linkConvention == LinkConventions::NONE || linkConvention==LinkConventions::NATIVE);
        //     // void* ptr = getReg(r0);
        //     // i64 addr = *((i64*)ptr);
            
        //     _ILOG(log::out << addr<<"\n";)
        //     if(addr>(int)length){
        //         log::out << log::YELLOW << "Call to instruction beyond all bytecode\n";
        //     }
        //     _ILOG(log::out <<" pc: "<< pc <<" fp: "<<fp<<" sp: "<<sp<<"\n";)
        //     // _ILOG(log::out<<"save fp at "<<sp<<"\n";)
        //     // savedFp = (u64*)sp;
        //     // SP_CHANGE(-8)
        //     sp-=8;
        //     *((u64*) sp) = pc; // pc points to the next instruction, +1 not needed 
        //     // SP_CHANGE(-8)
        //     pc = addr;
            
        //     _ILOG(log::out <<" pc: "<< pc <<" fp: "<<fp<<" sp: "<<sp<<"\n";)
    
        //     // if((i64)addr<0){
        //     if(linkConvention == LinkConventions::NATIVE){
        //         int argoffset = GenInfo::FRAME_SIZE; // native functions doesn't save
        //         // the frame pointer. FRAME_SIZE is based on that you do so -8 to get just pc being saved
                
        //         // auto* nativeFunction = nativeRegistry->findFunction(addr);
        //         // if(nativeFunction){

        //         // } else {
        //         //     _ILOG(log::out << log::RED << addr<<" is not a native function\n";)
        //         // }
        //         u64 fp = sp - 8;

        //         switch (addr) {
        //         case NATIVE_Allocate:{
        //             u64 size = *(u64*)(fp+argoffset);
        //             void* ptr = engone::Allocate(size);
        //             if(ptr)
        //                 userAllocatedBytes += size;
        //             _ILOG(log::out << "alloc "<<size<<" -> "<<ptr<<"\n";)
        //             *(u64*)(fp-8) = (u64)ptr;
        //             break;
        //         }
        //         break; case NATIVE_Reallocate:{
        //             void* ptr = *(void**)(fp+argoffset);
        //             u64 oldsize = *(u64*)(fp +argoffset + 8);
        //             u64 size = *(u64*)(fp+argoffset+16);
        //             ptr = engone::Reallocate(ptr,oldsize,size);
        //             if(ptr)
        //                 userAllocatedBytes += size - oldsize;
        //             _ILOG(log::out << "realloc "<<size<<" ptr: "<<ptr<<"\n";)
        //             *(u64*)(fp-8) = (u64)ptr;
        //             break;
        //         }
        //         break; case NATIVE_Free:{
        //             void* ptr = *(void**)(fp+argoffset);
        //             u64 size = *(u64*)(fp+argoffset+8);
        //             engone::Free(ptr,size);
        //             userAllocatedBytes -= size;
        //             _ILOG(log::out << "free "<<size<<" old ptr: "<<ptr<<"\n";)
        //             break;
        //         }
        //         break; case NATIVE_printi:{
        //             i64 num = *(i64*)(fp+argoffset);
        //             #ifdef ILOG
        //             log::out << log::LIME<<"print: "<<num<<"\n";
        //             #else                    
        //             log::out << num;
        //             #endif
        //             break;
        //         }
        //         break; case NATIVE_printd:{
        //             float num = *(float*)(fp+argoffset);
        //             #ifdef ILOG
        //             log::out << log::LIME<<"print: "<<num<<"\n";
        //             #else                    
        //             log::out << num;
        //             #endif
        //             break;
        //         }
        //         break; case NATIVE_printc: {
        //             char chr = *(char*)(fp+argoffset); // +7 comes from the alignment after char
        //             #ifdef ILOG
        //             log::out << log::LIME << "print: "<<chr<<"\n";
        //             #else
        //             log::out << chr;
        //             #endif
        //             break;
        //         }
        //         break; case NATIVE_prints: {
        //             char* ptr = *(char**)(fp+argoffset);
        //             u64 len = *(u64*)(fp+argoffset+8);
                    
        //             #ifdef ILOG
        //             log::out << log::LIME << "print: ";
        //             for(u32 i=0;i<len;i++){
        //                 log::out << ptr[i];
        //             }
        //             log::out << "\n";
        //             #else
        //             for(u32 i=0;i<len;i++){
        //                 log::out << ptr[i];
        //             }
        //             #endif
        //             break;
        //         }
        //         break; case NATIVE_FileOpen:{
        //             // The order may seem strange but it's actually correct.
        //             // It's just complicated.
        //             // slice
        //             Language::Slice<char>* path = *(Language::Slice<char>**)(fp+argoffset + 0);
        //             u32 flags = *(u32*)(fp+argoffset+8);
        //             u64* outFileSize = *(u64**)(fp+argoffset+16);

        //             // INCOMPLETE
        //             u64 file = FileOpen(path, flags, outFileSize);

        //             *(u64*)(fp-8) = (u64)file;
        //             break;
        //         }
        //         break; case NATIVE_FileRead:{
        //             // The order may seem strange but it's actually correct.
        //             // It is just complicated.
        //             // slice
        //             u64 file = *(u64*)(fp+argoffset);
        //             void* buffer = *(void**)(fp+argoffset+8);
        //             u64 readBytes = *(u64*)(fp+argoffset+16);

        //             u64 bytes = FileRead(file, buffer, readBytes);
        //             *(u64*)(fp-8) = (u64)bytes;
        //             break;
        //         }
        //         break; case NATIVE_FileWrite:{
        //             // The order may seem strange but it's actually correct.
        //             // It is just complicated.
        //             // slice
        //             u64 file = *(u64*)(fp+argoffset);
        //             void* buffer = *(void**)(fp+argoffset+8);
        //             u64 writeBytes = *(u64*)(fp+argoffset+16);

        //             u64 bytes = FileWrite(file, buffer, writeBytes);
        //             *(u64*)(fp-8) = (u64)bytes;
        //             break;
        //         }
        //         break; case NATIVE_FileClose:{
        //             APIFile file = *(APIFile*)(fp+argoffset);
        //             FileClose(file);
        //             // log::out << log::GRAY<<"FileClose: "<<file<<"\n";
        //             break;
        //         }
        //         break; case NATIVE_DirectoryIteratorCreate:{
        //             // slice
        //             // char* strptr = *(char**)(fp+argoffset + 0);
        //             // u64 len = *(u64*)(fp+argoffset+8);
        //             Language::Slice<char>* rootPath= *(Language::Slice<char>**)(fp + argoffset + 0);

        //             // INCOMPLETE
        //             // std::string rootPath = std::string(strptr,len);
        //             auto iterator = DirectoryIteratorCreate(rootPath);

        //             // log::out << log::GRAY<<"Create dir iterator: "<<rootPath<<"\n";

        //             // auto iterator = (Language::DirectoryIterator*)engone::Allocate(sizeof(Language::DirectoryIterator));
        //             // new(iterator)Language::DirectoryIterator();
        //             // iterator->_handle = (u64)iterHandle;
        //             // iterator->rootPath.ptr = (char*)engone::Allocate(len);
        //             // iterator->rootPath.len = len;
        //             // memcpy(iterator->rootPath.ptr, strptr, len);

        //             *(Language::DirectoryIterator**)(fp-8) = iterator;
        //             break;
        //         }
        //         break; case NATIVE_DirectoryIteratorDestroy:{
        //             Language::DirectoryIterator* iterator = *(Language::DirectoryIterator**)(fp+argoffset + 0);
        //             // INCOMPLETE
        //             DirectoryIteratorDestroy(iterator);

        //             // log::out << log::GRAY<<"Destroy dir iterator: "<<iterator->rootPath<<"\n";
        //             // if(iterator->result.name.ptr) {
        //             //     engone::Free(iterator->result.name.ptr, iterator->result.name.len);
        //             //     iterator->result.name.ptr = nullptr;
        //             //     iterator->result.name.len = 0;
        //             // }
        //             // engone::Free(iterator->rootPath.ptr,iterator->rootPath.len);
        //             // iterator->~DirectoryIterator();
        //             // engone::Free(iterator,sizeof(Language::DirectoryIterator));
        //             break;
        //         }
        //         break; case NATIVE_DirectoryIteratorNext:{
        //             Language::DirectoryIterator* iterator = *(Language::DirectoryIterator**)(fp+argoffset + 0);
                    
        //             auto data = DirectoryIteratorNext(iterator);

        //             // DirectoryIteratorData result{};
        //             // bool success = DirectoryIteratorNext((DirectoryIterator)iterator->_handle, &result);
        //             // INCOMPLETE
        //             // if(iterator->result.name.ptr) {
        //             //     engone::Free(iterator->result.name.ptr, iterator->result.name.len);
        //             //     iterator->result.name.ptr = nullptr;
        //             //     iterator->result.name.len = 0;
        //             // }
        //             // // if(success){
        //             // //     iterator->result.lastWriteSeconds = result.lastWriteSeconds;
        //             // //     iterator->result.fileSize = result.fileSize;
        //             // //     iterator->result.isDirectory = result.isDirectory;
        //             // //     iterator->result.name.ptr = (char*)Allocate((u64)result.name.length());
        //             // //     memcpy(iterator->result.name.ptr, result.name.data(), result.name.length());
        //             // //     iterator->result.name.len = (u64)result.name.length();
        //             // // }
        //             // // if(success)
        //             // // log::out << log::GRAY<<"Next dir iterator: "<<iterator->rootPath<<", file: "<<result.name<<"\n";
        //             // // else
        //             // // log::out << log::GRAY<<"Next dir iterator: "<<iterator->rootPath<<", finished\n";
        //             // if(success)
        //             //     *(Language::DirectoryIteratorData**)(fp-8) = &iterator->result;
        //             // else
        //             // *(Language::DirectoryIteratorData**)(fp-8) = nullptr;
        //             *(Language::DirectoryIteratorData**)(fp-8) = data;
        //             break;
        //         }
        //         break; case NATIVE_DirectoryIteratorSkip:{
        //             Language::DirectoryIterator* iterator = *(Language::DirectoryIterator**)(fp+argoffset + 0);
                    
        //             DirectoryIteratorSkip(iterator);

        //             // // log::out << log::GRAY<<"Skip dir iterator: "<<iterator->result.name<<"\n";
        //             break;
        //         }
        //         break; case NATIVE_CurrentWorkingDirectory:{
                    
        //             std::string temp = GetWorkingDirectory();
        //             ReplaceChar((char*)temp.data(), temp.length(),'\\','/');
        //             Assert(temp.length()<CWD_LIMIT);
        //             memcpy(cwdBuffer,temp.data(),temp.length());
        //             usedCwd = temp.length();

        //             *(char**)(fp-16) = cwdBuffer;
        //             *(u64*)(fp-8) = temp.length();
        //             // log::out << log::GRAY<<"CWD: "<<temp<<"\n";
        //             break;
        //         }
        //         break; case NATIVE_StartMeasure:{
        //             auto timePoint = StartMeasure();
                    
        //             *(u64*)(fp-8) = (u64)timePoint;

        //             // log::out << log::GRAY<<"Start mesaure: "<<(u64)timePoint<<"\n";
        //             break;
        //         }
        //         break; case NATIVE_StopMeasure:{
        //             u64 timePoint = *(u64*)(fp+argoffset + 0);

        //             double time = StopMeasure(timePoint);

        //             *(float*)(fp - 8) = (float)time;
        //             // log::out << log::GRAY<<"Stop measure: "<<(u64)timePoint << ", "<<time<<"\n";
        //             break;
        //         }
        //         break; case NATIVE_CmdLineArgs:{
        //             *(Language::Slice<char>**)(fp - 16) = cmdArgs.ptr;
        //             *(u64*)(fp - 8) = cmdArgs.len;
        //             // log::out << log::GRAY<<"Cmd line args: len: "<<cmdArgs.len<<"\n";
        //             break;
        //         }
        //         break; case NATIVE_NativeSleep:{
        //             float sleepTime = *(float*)(fp + argoffset + 4);
        //             Sleep(sleepTime);
        //             break;
        //         }
        //         break; default:{
        //             // auto* nativeFunction = bytecode->nativeRegistry->findFunction(addr);
        //             auto* nativeFunction = nativeRegistry->findFunction(addr);
        //             if(nativeFunction){
        //                 // _ILOG(
        //                 log::out << log::RED << "Native '"<<nativeFunction->name<<"' ("<<addr<<") has no impl. in interpreter\n";
        //                 // )
        //             } else {
        //                 // _ILOG(
        //                 log::out << log::RED << addr<<" is not a native function\n";
        //                 // )
        //             }
        //             // break from loop after a few times?
        //             // break won't work in switch statement.
        //         }
        //         } // switch
        //         // jump to BC_RET case
        //         goto TINY_GOTO;
        //     }
            
        //     break;
        // }
       
        // break; case BC_TEST_VALUE: {
        //     u8 r0 = DECODE_REG0(inst); // bytes
        //     u8 r1 = DECODE_REG1(inst); // test value
        //     u8 r2 = DECODE_REG2(inst); // computed value
        //     // TODO: Strings don't work yet

        //     u32 data = *(u32*)(codePtr + pc);
        //     pc++;

        //     u8* testValue = (u8*)getReg(r1);
        //     u8* computedValue = (u8*)getReg(r2);

        //     bool same = !strncmp((char*)testValue, (char*)computedValue, r0);

        //     char tmp[]{
        //         same ? '_' : 'x',
        //         '-',
        //         (char)((data>>8)&0xFF),
        //         (char)(data&0xFF)
        //     };
        //     // fwrite(&tmp, 1, 1, stderr);

        //     auto out = engone::GetStandardErr();
        //     engone::FileWrite(out, &tmp, 4);

        //     _ILOG(log::out << "\n";)
        //     break;
        // }
        #endif
        default: {
            log::out << log::RED << "Implement "<< log::PURPLE<< opcode << "\n";
            return;
        }
        } // switch

        if(logging && op0) {
            if(opcode == BC_MOV_MR || opcode == BC_MOV_MR_DISP16) {
                     if(control & CONTROL_8B)  log::out << log::GRAY << ", "<<*(i8*)ptr_from_mov;
                else if(control & CONTROL_16B) log::out << log::GRAY << ", "<<*(i16*)ptr_from_mov;
                else if(control & CONTROL_32B) log::out << log::GRAY << ", "<<*(i32*)ptr_from_mov;
                else if(control & CONTROL_64B) log::out << log::GRAY << ", "<<*(i64*)ptr_from_mov;
            } else {
                log::out << log::GRAY << ", "<<registers[op0];
            }
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
    auto time = StopMeasure(tp);
    if(!silent){
        log::out << "\n";
        log::out << log::LIME << "Executed in "<<FormatTime(time)<< " "<<FormatUnit(executedInstructions/time)<< " inst/s\n";
        #ifdef ILOG_REGS
        printRegisters();
        #endif
    }
}
void VirtualMachine::executeNative(int tiny_index){
    using namespace engone;
    u64 args = stack_pointer;
    switch (tiny_index) {
        case NATIVE_printi:{
            i64 num = *(i64*)(args);
            #ifdef ILOG
            log::out << log::LIME<<"print: "<<num<<"\n";
            #else                    
            log::out << num;
            #endif
        } break;
        case NATIVE_printd:{
            float num = *(float*)(args);
            #ifdef ILOG
            log::out << log::LIME<<"print: "<<num<<"\n";
            #else                    
            log::out << num;
            #endif
        } break;
        case NATIVE_printc: {
            char chr = *(char*)(args); // +7 comes from the alignment after char
            #ifdef ILOG
            log::out << log::LIME << "print: "<<chr<<"\n";
            #else
            log::out << chr;
            #endif
        } break;
        case NATIVE_prints: {
            char* ptr = *(char**)(args);
            u64 len = *(u64*)(args+8);
            
            #ifdef ILOG
            log::out << log::LIME << "print: ";
            for(u32 i=0;i<len;i++){
                log::out << ptr[i];
            }
            log::out << "\n";
            #else
            for(u32 i=0;i<len;i++){
                log::out << ptr[i];
            }
            #endif
        } break;
        // case NATIVE_Allocate:{
        //     u64 size = *(u64*)(fp+argoffset);
        //     void* ptr = engone::Allocate(size);
        //     if(ptr)
        //         userAllocatedBytes += size;
        //     _ILOG(log::out << "alloc "<<size<<" -> "<<ptr<<"\n";)
        //     *(u64*)(fp-8) = (u64)ptr;
        // } break;
        // case NATIVE_Reallocate:{
        //     void* ptr = *(void**)(fp+argoffset);
        //     u64 oldsize = *(u64*)(fp +argoffset + 8);
        //     u64 size = *(u64*)(fp+argoffset+16);
        //     ptr = engone::Reallocate(ptr,oldsize,size);
        //     if(ptr)
        //         userAllocatedBytes += size - oldsize;
        //     _ILOG(log::out << "realloc "<<size<<" ptr: "<<ptr<<"\n";)
        //     *(u64*)(fp-8) = (u64)ptr;
        // } break;
        // case NATIVE_Free:{
        //     void* ptr = *(void**)(fp+argoffset);
        //     u64 size = *(u64*)(fp+argoffset+8);
        //     engone::Free(ptr,size);
        //     userAllocatedBytes -= size;
        //     _ILOG(log::out << "free "<<size<<" old ptr: "<<ptr<<"\n";)
        //     break;
        // } break;
        default: Assert(false);
    }
}