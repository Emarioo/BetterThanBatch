#include "BetBat/Bytecode.h"
#include "BetBat/Compiler.h"

// included to get CallConventions
#include "BetBat/x64_gen.h"

#define ENABLE_BYTECODE_OPTIMIZATIONS

u32 Bytecode::getMemoryUsage(){
    Assert(false);
    return 0;
    // uint32 sum = instructionSegment.max*instructionSegment.getTypeSize()
    //     +debugSegment.max*debugSegment.getTypeSize()
    //     // debugtext?        
    //     ;
    // return sum;
}

bool Bytecode::addExportedFunction(const std::string& name, int tinycode_index) {
    for(int i=0;i<exportedFunctions.size();i++) {
        if(exportedFunctions[i].name == name) {
            return false;
        }
    }
    exportedFunctions.add({});
    exportedFunctions.last().name = name;
    exportedFunctions.last().tinycode_index = tinycode_index;
    return true;
}
void Bytecode::addExternalRelocation(const std::string& name, const std::string& library_path, int tinycode_index, int pc){
    ExternalRelocation tmp{};
    tmp.name = name;
    tmp.library_path = library_path;
    tmp.tinycode_index = tinycode_index;
    tmp.pc = pc;
    externalRelocations.add(tmp);
}
void Bytecode::cleanup(){
    // instructionSegment.resize(0);
    dataSegment.resize(0);
    // debugSegment.resize(0);
    // debugLocations.cleanup();
    if(debugInformation) {
        DebugInformation::Destroy(debugInformation);
        debugInformation = nullptr;
    }
    // if(nativeRegistry && nativeRegistry != NativeRegistry::GetGlobal()){
    //     NativeRegistry::Destroy(nativeRegistry);
    //     nativeRegistry = nullptr;
    // }
    // debugText.clear();
    // debugText.shrink_to_fit();
}

Bytecode* Bytecode::Create(){
    Bytecode* ptr = TRACK_ALLOC(Bytecode);
    new(ptr)Bytecode();
    return ptr;
}
// bool Bytecode::exportSymbol(const std::string& name, u32 instructionIndex){
//     for(int i=0;i<exportedSymbols.size();i++){
//         if(exportedSymbols[i].name == name){
//             return false;
//         }
//     }
//     exportedSymbols.add({name,instructionIndex});

//     return true;
// }
void Bytecode::Destroy(Bytecode* code){
    if(!code)
        return;
    code->cleanup();
    code->~Bytecode();
    TRACK_FREE(code, Bytecode);
    // engone::Free(code, sizeof(Bytecode));
}
void Bytecode::ensureAlignmentInData(int alignment){
    Assert(alignment > 0);
    // TODO: Check that alignment is a power of 2
    lock_global_data.lock();
    int misalign = alignment - (dataSegment.used % alignment);
    if(misalign == alignment) return;
    if(dataSegment.max < dataSegment.used + misalign){
        int oldMax = dataSegment.max;
        bool yes = dataSegment.resize(dataSegment.max*2 + 100);
        Assert(yes);
        memset(dataSegment.data() + oldMax, '_', dataSegment.max - oldMax);
    }
    int index = dataSegment.used;
    memset((char*)dataSegment.data() + index,'_',misalign);
    dataSegment.used+=misalign;
    lock_global_data.unlock();
}
int Bytecode::appendData(const void* data, int size){
    Assert(size > 0);
    lock_global_data.lock();
    if(dataSegment.max < dataSegment.used + size){
        int oldMax = dataSegment.max;
        dataSegment._reserve(dataSegment.max*2 + 2*size);
        memset(dataSegment.data() + oldMax, '_', dataSegment.max - oldMax);
    }
    int index = dataSegment.used;
    if(data) {
        memcpy((char*)dataSegment.data() + index,data,size);
    } else {
        memset((char*)dataSegment.data() + index,'_',size);
    }
    dataSegment.used+=size;
    lock_global_data.unlock();
    return index;
}
// int Bytecode::appendData_late(void** out_ptr, int size) {
//     Assert(size > 0);
//     Assert(out_ptr);
//     if(dataSegment.max < dataSegment.used + size){
//         int oldMax = dataSegment.max;
//         dataSegment.resize(dataSegment.max*2 + 2*size);
//         memset(dataSegment.data + oldMax, '_', dataSegment.max - oldMax);
//     }
//     int index = dataSegment.used;
//     if(data) {
//         memcpy((char*)dataSegment.data + index,data,size);
//     } else {
//         memset((char*)dataSegment.data + index,0,size);
//     }
//     dataSegment.used+=size;
//     return index;
// }
// void Bytecode::addDebugText(Token& token, u32 instructionIndex){
//     addDebugText(token.str,token.length,instructionIndex);
// }
// void Bytecode::addDebugText(const std::string& text, u32 instructionIndex){
//     addDebugText(text.data(),text.length(),instructionIndex);
// }
// void Bytecode::addDebugText(const char* str, int length, u32 instructionIndex){
//     using namespace engone;
//     if(instructionIndex==(u32)-1){
//         instructionIndex = instructionSegment.used;
//     }
//     if(instructionIndex>=debugSegment.max){
//         int newSize = instructionSegment.max*1.5+20;
//         newSize += (instructionIndex-debugSegment.max)*2;
//         int oldmax = debugSegment.max;
//         if(!debugSegment.resize(newSize))
//             return;
//         memset((char*)debugSegment.data + oldmax*debugSegment.getTypeSize(),0,(debugSegment.max-oldmax)*debugSegment.getTypeSize());
//     }
//     int oldIndex = *((u32*)debugSegment.data + instructionIndex);
//     if(oldIndex==0){
//         int index = debugText.size();
//         debugText.push_back(std::string(str,length));
//         // debugText.push_back({});
//         // debugText.resize(length);
        // // Assert(false); MISSING BOUNDS CHECK ON STRCPY
//         // strcpy((char*)debugText.data(),str,length);
//         *((u32*)debugSegment.data + instructionIndex) = index + 1;
//     }else{
//         Assert((int)debugText.size()<=oldIndex);
//         // debugText[oldIndex-1] += "\n"; // should line feed be forced?
//         debugText[oldIndex-1] += std::string(str,length);
//     }
// }
// const Bytecode::Location* Bytecode::getLocation(u32 instructionIndex, u32* locationIndex){
//     using namespace engone;
//     return nullptr;
//     // if(instructionIndex>=debugSegment.used)
//     //     return nullptr;
//     // u32 index = *((u32*)debugSegment.data() + instructionIndex);
//     // if(locationIndex)
//     //     *locationIndex = index;
//     // return debugLocations.getPtr(index); // may return nullptr;
// }
// Bytecode::Location* Bytecode::setLocationInfo(const char* str, u32 instructionIndex){
//     return setLocationInfo((Token)str);
// }
// Bytecode::Location* Bytecode::setLocationInfo(u32 locationIndex, u32 instructionIndex){
//     using namespace engone;
//     return nullptr;
    
    // if(instructionIndex == (u32)-1){
    //     instructionIndex = instructionSegment.used;
    // }
    // if(instructionIndex>=debugSegment.max) {
    //     int newSize = instructionSegment.max*1.5+20;
    //     newSize += (instructionIndex-debugSegment.max)*2;
    //     int oldmax = debugSegment.max;
    //     bool yes = debugSegment.resize(newSize);
    //     Assert(yes);
    //     memset((char*)debugSegment.data + oldmax*debugSegment.getTypeSize(),0xFF,(debugSegment.max-oldmax)*debugSegment.getTypeSize());
    // }
    // u32& index = *((u32*)debugSegment.data + instructionIndex);
    // if(debugSegment.used <= instructionIndex) {
    //     debugSegment.used = instructionIndex + 1;
    // }
    
    // index = locationIndex;
    // auto ptr = debugLocations.getPtr(index); // may return nullptr;
    // // if(ptr){
    // //     *locationIndex = index;
    // //     if(tokenRange.tokenStream())
    // //         ptr->file = TrimDir(tokenRange.tokenStream()->streamName);
    // //     else
    // //         ptr->desc = tokenRange.firstToken; // TODO: Don't just use the first token.
    // //     ptr->line = tokenRange.firstToken.line;
    // //     ptr->column = tokenRange.firstToken.column;
    // // }
    // return ptr;
// }
// Bytecode::Location* Bytecode::setLocationInfo(const TokenRange& tokenRange, u32 instructionIndex, u32* locationIndex){
//     using namespace engone;
//     return nullptr;
    // if(instructionIndex == (u32)-1){
    //     instructionIndex = instructionSegment.used;
    // }
    // if(instructionIndex>=debugSegment.max) {
    //     int newSize = instructionSegment.max*1.5+20;
    //     newSize += (instructionIndex-debugSegment.max)*2;
    //     int oldmax = debugSegment.max;
    //     bool yes = debugSegment.resize(newSize);
    //     Assert(yes);
    //     memset((char*)debugSegment.data + oldmax*debugSegment.getTypeSize(),0xFF,(debugSegment.max-oldmax)*debugSegment.getTypeSize());
    // }
    // u32& index = *((u32*)debugSegment.data + instructionIndex);
    // if(debugSegment.used <= instructionIndex) {
    //     debugSegment.used = instructionIndex + 1;
    // }
    // if((int)index == -1){
    //     bool yes = debugLocations.add({});
    //     Assert(yes);
    //     index = debugLocations.size()-1;
    // }
    // auto ptr = debugLocations.getPtr(index); // may return nullptr;
    // if(ptr){
    //     if(locationIndex)
    //         *locationIndex = index;
    //     if(tokenRange.tokenStream()) {
    //         // ptr->file = TrimDir(tokenRange.tokenStream()->streamName);
    //         // ptr->line = tokenRange.firstToken.line;
    //         // ptr->column = tokenRange.firstToken.column;
    //     } else {
    //         // ptr->preDesc = tokenRange.firstToken; // TODO: Don't just use the first token.
    //     }
    // }
    // return ptr;
// }
// const char* Bytecode::getDebugText(u32 instructionIndex){
//     using namespace engone;
//     if(instructionIndex>=debugSegment.max){
//         // log::out<<log::RED << "out of bounds on debug text\n";
//         return 0;
//     }
//     u32 index = *((u32*)debugSegment.data + instructionIndex);
//     if(!index)
//         return 0;
//     index = index - 1; // little offset
//     if(index>=debugText.size()){
//         // This would be bad. The debugSegment shouldn't contain invalid values
//         log::out<<log::RED << __FILE__ << ":"<<__LINE__<<", instruction index out of bounds\n";
//         return 0;   
//     }
//     return debugText[index].c_str();
// }


void BytecodeBuilder::init(Bytecode* code, TinyBytecode* tinycode, Compiler* compiler) {
    // Assert(virtualStackPointer == 0);
    // Assert(stackAlignment.size() == 0);
    // virtualStackPointer = 0;
    // stackAlignment.cleanup();
    
    this->code = code;
    this->tinycode = tinycode;
    this->compiler = compiler;
}

void BytecodeBuilder::emit_test(BCRegister to, BCRegister from, u8 size, i32 test_location) {
    emit_opcode(BC_TEST_VALUE);
    emit_operand(to);
    emit_operand(from);
    emit_imm8(size);
    emit_imm32(test_location);
}
void BytecodeBuilder::emit_push(BCRegister reg) {
    emit_opcode(BC_PUSH);
    emit_operand(reg);
    pushed_offset -= 8;
    if(pushed_offset < pushed_offset_max)
        pushed_offset_max = pushed_offset;
}
void BytecodeBuilder::emit_pop(BCRegister reg) {
#ifdef ENABLE_BYTECODE_OPTIMIZATIONS
    int i = get_index_of_previous_instruction();
    if(i != -1) {
        if(tinycode->instructionSegment[i] == BC_PUSH) {
            if(tinycode->instructionSegment[i + 1] == reg) {
                remove_previous_instruction();
                tinycode->instructionSegment.pop();
                tinycode->instructionSegment.pop();
                return;
            }
        }
    }
#endif

    emit_opcode(BC_POP);
    emit_operand(reg);
    pushed_offset += 8;
}
void BytecodeBuilder::emit_li32(BCRegister reg, i32 imm){
    emit_opcode(BC_LI32);
    emit_operand(reg);
    emit_imm32(imm);
}
void BytecodeBuilder::emit_li64(BCRegister reg, i64 imm){
    emit_opcode(BC_LI64);
    emit_operand(reg);
    emit_imm64(imm);
}
void BytecodeBuilder::emit_incr(BCRegister reg, i32 imm) {
    Assert(imm != 0); // incrementing by 0 is dumb

#ifdef ENABLE_BYTECODE_OPTIMIZATIONS
    // if(index_of_last_instruction != -1 && tinycode->instructionSegment[index_of_last_instruction] == BC_INCR && tinycode->instructionSegment[index_of_last_instruction+1] == reg) {
    //     // modify immediate of previous incr
    //     i32* prev_imm = (i32*)&tinycode->instructionSegment[index_of_last_instruction + 2];
    //     *prev_imm += imm;

    //     if(*prev_imm == 0) {
    //         index_of_last_instruction = -1;
    //         tinycode->instructionSegment.used -= 6;
    //     }

    //     Assert(no_modification); // we don't know if previous incr had modification or not, it probably did
    //     if(reg == BC_REG_SP)
    //         virtualStackPointer += imm;
    //     return;
    // }
#endif
    // Assert(reg != BC_REG_SP && reg != BC_REG_BP); // these are deprecated
    emit_opcode(BC_INCR);
    emit_operand(reg);
    emit_imm32(imm);
    
    // if(reg == BC_REG_SP && !no_modification)
    //     virtualStackPointer += imm;
}
void BytecodeBuilder::emit_alloc_local(BCRegister reg, u16 size) {
    if(compiler->options->compileStats.errors == 0) {
        // We would need an array of pushed offsets
        // Assert(pushed_offset == 0);
    }
    Assert(size > 0);
    emit_opcode(BC_ALLOC_LOCAL);
    emit_operand(reg);
    emit_imm16(size);

    has_return_values = false;
    // pushed_offset = 0;
}
void BytecodeBuilder::emit_free_local(u16 size) {
    if(compiler->options->compileStats.errors == 0) {
        // Assert(pushed_offset == 0); // We have some pushed values in the way and can't free local variables
    }
    if(has_return_values)
        ret_offset -= size;
    // If size=0 is passed then it's probably a bug.
    // We should not do "if(size==0) return;" because then we won't catch those bugs.
    Assert(size > 0);
    emit_opcode(BC_FREE_LOCAL);
    emit_imm16(size);
}

void BytecodeBuilder::emit_set_arg(BCRegister reg, i16 imm, int size, bool is_float) {
    emit_opcode(BC_SET_ARG);
    emit_operand(reg);
    emit_imm16(imm);
    
    InstructionControl control=CONTROL_NONE;
    if(is_float)
        control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(size == 1)      control = (InstructionControl)(control | CONTROL_8B);
    else if(size == 2) control = (InstructionControl)(control | CONTROL_16B);
    else if(size == 4) control = (InstructionControl)(control | CONTROL_32B);
    else if(size == 8) control = (InstructionControl)(control | CONTROL_64B);
    else Assert(false);
    emit_control(control);
}
void BytecodeBuilder::emit_get_param(BCRegister reg, i16 imm, int size, bool is_float){
    emit_opcode(BC_GET_PARAM);
    emit_operand(reg);
    emit_imm16(imm);
    
    InstructionControl control=CONTROL_NONE;
    if(is_float)
        control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(size == 1)      control = (InstructionControl)(control | CONTROL_8B);
    else if(size == 2) control = (InstructionControl)(control | CONTROL_16B);
    else if(size == 4) control = (InstructionControl)(control | CONTROL_32B);
    else if(size == 8) control = (InstructionControl)(control | CONTROL_64B);
    else Assert(false);
    emit_control(control);
}
void BytecodeBuilder::emit_set_ret(BCRegister reg, i16 imm, int size, bool is_float){
    emit_opcode(BC_SET_RET);
    emit_operand(reg);
    emit_imm16(imm);
    
    InstructionControl control=CONTROL_NONE;
    if(is_float)
        control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(size == 1)      control = (InstructionControl)(control | CONTROL_8B);
    else if(size == 2) control = (InstructionControl)(control | CONTROL_16B);
    else if(size == 4) control = (InstructionControl)(control | CONTROL_32B);
    else if(size == 8) control = (InstructionControl)(control | CONTROL_64B);
    else Assert(false);
    emit_control(control);
}
void BytecodeBuilder::emit_get_val(BCRegister reg, i16 imm, int size, bool is_float){
    Assert(has_return_values);
    const int FRAME_SIZE = 16;
    int off = imm + ret_offset - FRAME_SIZE;
    // off = -24
    // push = -8 (ok) -16 (ok) -24 (not ok)
    // engone::log::out << (pushed_offset_max) << " " << (off)<<".."<<(off+size) << "\n";
    // if(pushed_offset_max >= off + size)
    //     engone::log::out << " false\n";
    // THIS IS CORRECT, DON'T CHANGE THE MATH!
    // structs members and return values are pushed in different orders which is why
    // structs can have 3 members without value being overwritten while multiple return values can only have 2.
    // TODO: This is obviously a flaw in the BETBAT calling convention. How do we fix it?
    if(compiler->options->compileStats.errors == 0) {
        Assert(("Multiple return values has limits (at least 3 8-byte values will work). BC_PUSH can overwrite the return values in the frame of the callee.",pushed_offset_max >= off + size)); // Asserts if return value we try to access was overwritten by pushed values
    }

    emit_opcode(BC_GET_VAL);
    emit_operand(reg);
    emit_imm16(imm);
    
    InstructionControl control=CONTROL_NONE;
    if(is_float)
        control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(size == 1)      control = (InstructionControl)(control | CONTROL_8B);
    else if(size == 2) control = (InstructionControl)(control | CONTROL_16B);
    else if(size == 4) control = (InstructionControl)(control | CONTROL_32B);
    else if(size == 8) control = (InstructionControl)(control | CONTROL_64B);
    else Assert(false);
    emit_control(control);
}
void BytecodeBuilder::emit_call(LinkConventions l, CallConventions c, i32* index_of_relocation, i32 imm) {
    emit_opcode(BC_CALL);
    emit_imm8(l);
    emit_imm8(c);
    *index_of_relocation = tinycode->instructionSegment.used;
    emit_imm32(imm);

    has_return_values = true;
    ret_offset = pushed_offset;
    pushed_offset_max = pushed_offset;
}
void BytecodeBuilder::emit_ptr_to_locals(BCRegister reg, int imm16) {
    emit_opcode(BC_PTR_TO_LOCALS);
    emit_operand(reg);
    Assert(imm16 >= -0x8000 && imm16 <= 0x7FFF); // imm16 is int and not i16 so that we catch mistakes
    emit_imm16(imm16);
}
void BytecodeBuilder::emit_ptr_to_params(BCRegister reg, int imm16) {
    emit_opcode(BC_PTR_TO_PARAMS);
    emit_operand(reg);
    Assert(imm16 >= -0x8000 && imm16 <= 0x7FFF); // imm16 is int and not i16 so that we catch mistakes
    emit_imm16(imm16);
}
void BytecodeBuilder::emit_ret() {
    emit_opcode(BC_RET);
}
void BytecodeBuilder::emit_jmp(int pc) {
    emit_opcode(BC_JMP);
    emit_imm32(pc - (get_pc() + 4)); // +4 because of immediate
}
int BytecodeBuilder::emit_jmp() {
    emit_opcode(BC_JMP);
    int imm_offset  = get_pc();
    emit_imm32(0);
    return imm_offset;
}
void BytecodeBuilder::emit_jz(BCRegister reg, int pc) {
    emit_opcode(BC_JZ);
    emit_operand(reg);
    emit_imm32(pc - (get_pc() + 4));  // +4 because of immediate
}
int BytecodeBuilder::emit_jz(BCRegister reg) {
    emit_opcode(BC_JZ);
    emit_operand(reg);
    int imm_offset = get_pc();
    emit_imm32(0);
    return imm_offset;
}
void BytecodeBuilder::emit_jnz(BCRegister reg, int pc) {
    emit_opcode(BC_JNZ);
    emit_operand(reg);
    emit_imm32(pc - (get_pc() + 4)); // +4 because of immediate
}
int BytecodeBuilder::emit_jnz(BCRegister reg) {
    emit_opcode(BC_JNZ);
    emit_operand(reg);
    int tmp = get_pc();
    emit_imm32(0);
    return tmp;
}
void BytecodeBuilder::fix_jump_imm32_here(int imm_index) {
    *(int*)&tinycode->instructionSegment[imm_index] = get_pc() - (imm_index + 4); // +4 because immediate should be relative to the end of the instruction, not relative to the offset within the instruction
}
void BytecodeBuilder::emit_mov_rr(BCRegister to, BCRegister from){
    emit_opcode(BC_MOV_RR);
    emit_operand(to);
    emit_operand(from);
}
void BytecodeBuilder::emit_mov_rm(BCRegister to, BCRegister from, int size){
    emit_opcode(BC_MOV_RM);
    emit_operand(to);
    emit_operand(from);
    
    if(size == 1) emit_control(CONTROL_8B);
    else if(size == 2) emit_control(CONTROL_16B);
    else if(size == 4) emit_control(CONTROL_32B);
    else if(size == 8) emit_control(CONTROL_64B);
    else Assert(false);
}
void BytecodeBuilder::emit_mov_mr(BCRegister to, BCRegister from, int size){
    emit_opcode(BC_MOV_MR);
    emit_operand(to);
    emit_operand(from);
    if(size == 1) emit_control(CONTROL_8B);
    else if(size == 2) emit_control(CONTROL_16B);
    else if(size == 4) emit_control(CONTROL_32B);
    else if(size == 8) emit_control(CONTROL_64B);
    else Assert(false);
}

void BytecodeBuilder::emit_mov_rm_disp(BCRegister to, BCRegister from, int size, int displacement){
    Assert(to != BC_REG_T1 && from != BC_REG_T1);
    if(displacement == 0) {
        emit_mov_rm(to, from, size);
        return;
    }
    
    emit_opcode(BC_MOV_RM_DISP16);
    emit_operand(to);
    emit_operand(from);
    
    if(size == 1) emit_control(CONTROL_8B);
    else if(size == 2) emit_control(CONTROL_16B);
    else if(size == 4) emit_control(CONTROL_32B);
    else if(size == 8) emit_control(CONTROL_64B);
    else Assert(false);
    
    Assert(displacement < 0x8000 && displacement >= -0x8000);
    emit_imm16(displacement);
}
void BytecodeBuilder::emit_mov_mr_disp(BCRegister to, BCRegister from, int size, int displacement){
    Assert(to != BC_REG_T1 && from != BC_REG_T1);
    if(displacement == 0) {
        emit_mov_mr(to, from, size);
        return;
    }
    
    emit_opcode(BC_MOV_MR_DISP16);
    emit_operand(to);
    emit_operand(from);
     
    if(size == 1) emit_control(CONTROL_8B);
    else if(size == 2) emit_control(CONTROL_16B);
    else if(size == 4) emit_control(CONTROL_32B);
    else if(size == 8) emit_control(CONTROL_64B);
    else Assert(false);
    
    Assert(displacement < 0x8000 && displacement >= -0x8000);
    emit_imm16(displacement);
}

void BytecodeBuilder::emit_add(BCRegister to, BCRegister from, bool is_float, int size) {
    emit_opcode(BC_ADD);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(size == 1) control = (InstructionControl)(control | CONTROL_8B);
    else if(size == 2) control = (InstructionControl)(control | CONTROL_16B);
    else if(size == 4) control = (InstructionControl)(control | CONTROL_32B);
    else if(size == 8) control = (InstructionControl)(control | CONTROL_64B);
    emit_control(control);
}
void BytecodeBuilder::emit_sub(BCRegister to, BCRegister from, bool is_float, int size) {
    emit_opcode(BC_SUB);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(size == 1) control = (InstructionControl)(control | CONTROL_8B);
    else if(size == 2) control = (InstructionControl)(control | CONTROL_16B);
    else if(size == 4) control = (InstructionControl)(control | CONTROL_32B);
    else if(size == 8) control = (InstructionControl)(control | CONTROL_64B);
    emit_control(control);
}
void BytecodeBuilder::emit_mul(BCRegister to, BCRegister from, bool is_float, int size, bool is_signed) {
    emit_opcode(BC_MUL);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(!is_signed) control = (InstructionControl)(control | CONTROL_UNSIGNED_OP);
    if(size == 1) control = (InstructionControl)(control | CONTROL_8B);
    else if(size == 2) control = (InstructionControl)(control | CONTROL_16B);
    else if(size == 4) control = (InstructionControl)(control | CONTROL_32B);
    else if(size == 8) control = (InstructionControl)(control | CONTROL_64B);
    emit_control(control);
}
void BytecodeBuilder::emit_div(BCRegister to, BCRegister from, bool is_float, int size, bool is_signed) {
    emit_opcode(BC_DIV);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(!is_signed) control = (InstructionControl)(control | CONTROL_UNSIGNED_OP);
    if(size == 1) control = (InstructionControl)(control | CONTROL_8B);
    else if(size == 2) control = (InstructionControl)(control | CONTROL_16B);
    else if(size == 4) control = (InstructionControl)(control | CONTROL_32B);
    else if(size == 8) control = (InstructionControl)(control | CONTROL_64B);
    emit_control(control);
}
void BytecodeBuilder::emit_mod(BCRegister to, BCRegister from, bool is_float, int size, bool is_signed) {
    emit_opcode(BC_MOD);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(!is_signed) control = (InstructionControl)(control | CONTROL_UNSIGNED_OP);
    if(size == 1) control = (InstructionControl)(control | CONTROL_8B);
    else if(size == 2) control = (InstructionControl)(control | CONTROL_16B);
    else if(size == 4) control = (InstructionControl)(control | CONTROL_32B);
    else if(size == 8) control = (InstructionControl)(control | CONTROL_64B);
    emit_control(control);
}

void BytecodeBuilder::emit_band(BCRegister to, BCRegister from) {
    emit_opcode(BC_BAND);
    emit_operand(to);
    emit_operand(from);   
}
void BytecodeBuilder::emit_bor(BCRegister to, BCRegister from) {
    emit_opcode(BC_BOR);
    emit_operand(to);
    emit_operand(from);   
}
void BytecodeBuilder::emit_bxor(BCRegister to, BCRegister from) {
    emit_opcode(BC_BXOR);
    emit_operand(to);
    emit_operand(from);
}
void BytecodeBuilder::emit_bnot(BCRegister to, BCRegister from) {
    emit_opcode(BC_BNOT);
    emit_operand(to);
    emit_operand(from);   
}
void BytecodeBuilder::emit_blshift(BCRegister to, BCRegister from) {
    emit_opcode(BC_BLSHIFT);
    emit_operand(to);
    emit_operand(from);   
}
void BytecodeBuilder::emit_brshift(BCRegister to, BCRegister from) {
    emit_opcode(BC_BRSHIFT);
    emit_operand(to);
    emit_operand(from);   
}

void BytecodeBuilder::emit_eq(BCRegister to, BCRegister from, bool is_float,int size){
    emit_opcode(BC_EQ);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(size == 1) control = (InstructionControl)(control | CONTROL_8B);
    else if(size == 2) control = (InstructionControl)(control | CONTROL_16B);
    else if(size == 4) control = (InstructionControl)(control | CONTROL_32B);
    else if(size == 8) control = (InstructionControl)(control | CONTROL_64B);
    emit_control(control);
}
void BytecodeBuilder::emit_neq(BCRegister to, BCRegister from, bool is_float,int size){
    emit_opcode(BC_NEQ);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(size == 1) control = (InstructionControl)(control | CONTROL_8B);
    else if(size == 2) control = (InstructionControl)(control | CONTROL_16B);
    else if(size == 4) control = (InstructionControl)(control | CONTROL_32B);
    else if(size == 8) control = (InstructionControl)(control | CONTROL_64B);
    emit_control(control);
}
void BytecodeBuilder::emit_lt(BCRegister to, BCRegister from, bool is_float, int size, bool is_signed){
    emit_opcode(BC_LT);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(!is_signed) control = (InstructionControl)(control | CONTROL_UNSIGNED_OP);
    if(size == 1) control = (InstructionControl)(control | CONTROL_8B);
    else if(size == 2) control = (InstructionControl)(control | CONTROL_16B);
    else if(size == 4) control = (InstructionControl)(control | CONTROL_32B);
    else if(size == 8) control = (InstructionControl)(control | CONTROL_64B);
    emit_control(control);
}
void BytecodeBuilder::emit_lte(BCRegister to, BCRegister from, bool is_float,int size, bool is_signed){
    emit_opcode(BC_LTE);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(!is_signed) control = (InstructionControl)(control | CONTROL_UNSIGNED_OP);
    if(size == 1) control = (InstructionControl)(control | CONTROL_8B);
    else if(size == 2) control = (InstructionControl)(control | CONTROL_16B);
    else if(size == 4) control = (InstructionControl)(control | CONTROL_32B);
    else if(size == 8) control = (InstructionControl)(control | CONTROL_64B);
    emit_control(control);
}
void BytecodeBuilder::emit_gt(BCRegister to, BCRegister from, bool is_float,int size, bool is_signed){
    emit_opcode(BC_GT);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(!is_signed) control = (InstructionControl)(control | CONTROL_UNSIGNED_OP);
    if(size == 1) control = (InstructionControl)(control | CONTROL_8B);
    else if(size == 2) control = (InstructionControl)(control | CONTROL_16B);
    else if(size == 4) control = (InstructionControl)(control | CONTROL_32B);
    else if(size == 8) control = (InstructionControl)(control | CONTROL_64B);
    emit_control(control);
}
void BytecodeBuilder::emit_gte(BCRegister to, BCRegister from, bool is_float,int size, bool is_signed){
    emit_opcode(BC_GTE);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(!is_signed) control = (InstructionControl)(control | CONTROL_UNSIGNED_OP);
    if(size == 1) control = (InstructionControl)(control | CONTROL_8B);
    else if(size == 2) control = (InstructionControl)(control | CONTROL_16B);
    else if(size == 4) control = (InstructionControl)(control | CONTROL_32B);
    else if(size == 8) control = (InstructionControl)(control | CONTROL_64B);
    emit_control(control);
}
 
void BytecodeBuilder::emit_land(BCRegister to, BCRegister from) {
    emit_opcode(BC_LAND);
    emit_operand(to);
    emit_operand(from);
}
void BytecodeBuilder::emit_lor(BCRegister to, BCRegister from) {
    emit_opcode(BC_LOR);
    emit_operand(to);
    emit_operand(from);
}
void BytecodeBuilder::emit_lnot(BCRegister to, BCRegister from) {
    emit_opcode(BC_LNOT);
    emit_operand(to);
    emit_operand(from);
}
void BytecodeBuilder::emit_dataptr(BCRegister reg, i32 imm) {
    emit_opcode(BC_DATAPTR);
    emit_operand(reg);
    emit_imm32(imm);
}
void BytecodeBuilder::emit_codeptr(BCRegister reg, i32 imm) {
    emit_opcode(BC_CODEPTR);
    emit_operand(reg);
    emit_imm32(imm);
}
void BytecodeBuilder::emit_memzero(BCRegister ptr_reg, BCRegister size_reg, u8 batch) {
    emit_opcode(BC_MEMZERO);
    emit_operand(ptr_reg);
    emit_operand(size_reg);   
    emit_imm8(batch);
}
void BytecodeBuilder::emit_memcpy(BCRegister dst, BCRegister src, BCRegister size_reg) {
    emit_opcode(BC_MEMCPY);
    emit_operand(dst);
    emit_operand(src);   
    emit_operand(size_reg);
}
void BytecodeBuilder::emit_strlen(BCRegister len_reg, BCRegister src_reg){
    emit_opcode(BC_STRLEN);
    emit_operand(len_reg);
    emit_operand(src_reg);
}
void BytecodeBuilder::emit_rdtsc(BCRegister to){
    emit_opcode(BC_RDTSC);
    emit_operand(to);
}
void BytecodeBuilder::emit_atomic_cmp_swap(BCRegister ptr, BCRegister old, BCRegister new_reg){
    emit_opcode(BC_ATOMIC_CMP_SWAP);
    emit_operand(ptr);
    emit_operand(old);
    emit_operand(new_reg);
}
void BytecodeBuilder::emit_atomic_add(BCRegister to, BCRegister from, InstructionControl control){
    Assert(!IS_CONTROL_FLOAT(control));
    emit_opcode(BC_ATOMIC_ADD);
    emit_operand(to);
    emit_operand(from);
    emit_control(control);
}
void BytecodeBuilder::emit_sqrt(BCRegister to){
    emit_opcode(BC_SQRT);
    emit_operand(to);
}
void BytecodeBuilder::emit_round(BCRegister to){
    emit_opcode(BC_ROUND);
    emit_operand(to);
}

void BytecodeBuilder::emit_cast(BCRegister reg, InstructionCast castType, u8 from_size, u8 to_size) {
    InstructionControl control = InstructionControl::CONTROL_NONE;
    switch(from_size) {
        case 1: control = (InstructionControl)(control | CONTROL_8B); break;
        case 2: control = (InstructionControl)(control | CONTROL_16B); break;
        case 4: control = (InstructionControl)(control | CONTROL_32B); break;
        case 8: control = (InstructionControl)(control | CONTROL_64B); break;
        Assert(false);
    }
    switch(to_size) {
        case 1: control = (InstructionControl)(control | CONTROL_CONVERT_8B); break;
        case 2: control = (InstructionControl)(control | CONTROL_CONVERT_16B); break;
        case 4: control = (InstructionControl)(control | CONTROL_CONVERT_32B); break;
        case 8: control = (InstructionControl)(control | CONTROL_CONVERT_64B); break;
        Assert(false);
    }
    
    emit_opcode(BC_CAST);
    emit_operand(reg);
    emit_imm8(control);
    emit_imm8(castType);
}

void BytecodeBuilder::emit_opcode(InstructionType type) {
    append_previous_instruction(tinycode->instructionSegment.size());
    tinycode->instructionSegment.add((u8)type);
    tinycode->index_of_lines.resize(tinycode->instructionSegment.size());
    tinycode->index_of_lines[tinycode->instructionSegment.size()-1] = tinycode->lines.size();
}
void BytecodeBuilder::emit_operand(BCRegister reg) {
    tinycode->instructionSegment.add((u8)reg);
}
void BytecodeBuilder::emit_control(InstructionControl control) {
    tinycode->instructionSegment.add((u8)control);
}

void BytecodeBuilder::emit_imm8(i8 imm) {
    tinycode->instructionSegment.add(0);
    // get the pointer after add() because of reallocations
    i8* ptr = (i8*)(tinycode->instructionSegment.data() + tinycode->instructionSegment.size() - 1);
    *ptr = imm;
}
void BytecodeBuilder::emit_imm16(i16 imm) {
    tinycode->instructionSegment.add(0);
    tinycode->instructionSegment.add(0);
    // get the pointer after add() because of reallocations
    i16* ptr = (i16*)(tinycode->instructionSegment.data() + tinycode->instructionSegment.size() - 2);
    *ptr = imm;
}
void BytecodeBuilder::emit_imm32(i32 imm) {
    tinycode->instructionSegment.add(0); // TODO: OPTIMIZE
    tinycode->instructionSegment.add(0);
    tinycode->instructionSegment.add(0);
    tinycode->instructionSegment.add(0);
    // get the pointer after add() because of reallocations
    i32* ptr = (i32*)(tinycode->instructionSegment.data() + tinycode->instructionSegment.size() - 4);
    *ptr = imm;
}
void BytecodeBuilder::emit_imm64(i64 imm) {
    tinycode->instructionSegment.add(0); // TODO: OPTIMIZE
    tinycode->instructionSegment.add(0);
    tinycode->instructionSegment.add(0);
    tinycode->instructionSegment.add(0);
    tinycode->instructionSegment.add(0);
    tinycode->instructionSegment.add(0);
    tinycode->instructionSegment.add(0);
    tinycode->instructionSegment.add(0);
    // get the pointer after add() because of reallocations
    i64* ptr = (i64*)(tinycode->instructionSegment.data() + tinycode->instructionSegment.size() - 8);
    *ptr = imm;
}
//  void BytecodeBuilder::restoreStackMoment(int saved_sp, bool no_modification, bool no_instruction) {
//     using namespace engone;
//     int offset = saved_sp - virtualStackPointer;
//     if (offset == 0)
//         return;
//     if(!no_modification || no_instruction) {
//         // int at = saved_sp - virtualStackPointer;
//         // while (at > 0 && stackAlignment.size() > 0) {
//         //     auto align = stackAlignment.last();
//         //     // log::out << "pop stackalign "<<align.diff<<":"<<align.size<<"\n";
//         //     stackAlignment.pop();
//         //     at -= align.size;
//         //     at -= align.diff;
//         //     if(!hasErrors()) {
//         //         Assert(at >= 0);
//         //     }
//         // }
//     } 
//     // else {
//     //     _GLOG(log::out << "relsp "<<moment<<"\n";)
//     // }
//     if(!no_instruction){
//         emit_incr(BC_REG_SP, offset, no_modification);
//     } else {
//         virtualStackPointer = saved_sp;
//     }
//     if(no_modification) {
//         _GLOG(log::out << "relsp (temp) "<<saved_sp<<"\n";)
//     } else {
//         _GLOG(log::out << "relsp "<<saved_sp<<"\n";)
//     }
// }
bool TinyBytecode::applyRelocations(Bytecode* code) {
    bool suc = true;
    for(int i=0;i<call_relocations.size();i++) {
        auto& rel = call_relocations[i];
        Assert(rel.funcImpl && rel.funcImpl->tinycode_id);
        *(i32*)&instructionSegment[rel.pc] = rel.funcImpl->tinycode_id;

        // if(rel.funcImpl && rel.funcImpl->tinycode_id) {
        // } else if(rel.func_name.size()) {
        //     bool found = false;
        //     for(int j=0;j<code->tinyBytecodes.size();j++){
        //         if(rel.func_name == code->tinyBytecodes[j]->name) {
        //             *(i32*)&instructionSegment[rel.pc] = j + 1;
        //             found = true;
        //             break;
        //         }
        //     }
        //     if(!found) suc = false;
        // } else {
        //     suc = false;
        // }
    }
    return suc;
}

// extern const char* control_names[] {
    
// };
extern const char* cast_names[] {
    "uint->uint",
    "uint->sint",
    "sint->uint",
    "sint->sint",
    "float->uint",
    "float->sint",
    "uint->float",
    "sint->float",
    "float->float",
};
extern const char* instruction_names[] {
    "halt", // BC_HALT
    "nop", // BC_NOP
    "mov_rr", // BC_MOV_RR
    "mov_rm", // BC_MOV_RM
    "mov_mr", // BC_MOV_MR
    "mov_rm_disp", // BC_MOV_RM_DISP16
    "mov_mr_disp", // BC_MOV_MR_DISP16
    "push", // BC_PUSH
    "pop", // BC_POP
    "li32", // BC_LI
    "li64", // BC_LI
    "incr", // BC_INCR
    "alloc_local", // BC_ALLOC_LOCAL
    "free_local", // BC_FREE_LOCAL
    "set_arg", // 
    "get_param", // 
    "get_val", // 
    "set_ret", // 
    "ptr_to_locals",
    "ptr_to_params",
    "jmp", // BC_JMP
    "call", // BC_CALL
    "ret", // BC_RET
    "jnz", // BC_JNZ
    "jz", // BC_JZ
    "dataptr", // BC_DATAPTR
    "codeptr", // BC_CODEPTR
    "add", // BC_ADD
    "sub", // BC_SUB
    "mul", // BC_MUL
    "div", // BC_DIV
    "mod", // BC_MOD
    "eq", // BC_EQ
    "neq", // BC_NEQ
    "lt", // BC_LT
    "lte", // BC_LTE
    "gt", // BC_GT
    "gte", // BC_GTE
    "land", // BC_LAND
    "lor", // BC_LOR
    "lnot", // BC_LNOT
    "band", // BC_BAND
    "bor", // BC_BOR
    "bnot", // BC_BNOT
    "bxor", // BC_BXOR
    "blshift", // BC_BLSHIFT
    "brshift", // BC_BRSHIFT
    "cast", // BC_CAST
    "memzero", // BC_MEMZERO
    "memcpy", // BC_MEMCPY
    "strlen", // BC_STRLEN
    "rdtsc", // BC_RDTSC
    "cmp_swap", // BC_CMP_SWAP
    "atom_add", // BC_ATOMIC_ADD
    "sqrt", // BC_SQRT
    "round", // BC_ROUND
    "asm", // BC_ASM
    "test", // BC_TEST_VALUE
};
extern const char* register_names[] {
    "invalid", // BC_REG_INVALID
    "a", // BC_REG_A
    "b", // BC_REG_B
    "c", // BC_REG_C
    "d", // BC_REG_D
    "e", // BC_REG_E
    "f", // BC_REG_F
    "t0", // BC_REG_T0
    "t1", // BC_REG_T1
    "locals",
    // "args",
    // "vals",
    // "params",
    // "rets",

    // "sp", // BC_REG_SP
    // "bp", // BC_REG_BP
    // "rax", // BC_REG_RAX
    // "rbx", // BC_REG_RBX
    // "rcx", // BC_REG_RCX
    // "rdx", // BC_REG_RDX
    // "rsi", // BC_REG_RSI
    // "rdi", // BC_REG_RDI
    // "r8", // BC_REG_R8
    // "r9", // BC_REG_R9
    // "xmm0", // BC_REG_XMM0
    // "xmm1", // BC_REG_XMM1
    // "xmm2", // BC_REG_XMM2
    // "xmm3", // BC_REG_XMM3
};

void TinyBytecode::print(int low_index, int high_index, Bytecode* code, DynamicArray<std::string>* dll_functions, bool force_newline) {
    using namespace engone;
    if(high_index == -1 || high_index > instructionSegment.size())
        high_index = instructionSegment.size();
    
    auto& instructions = instructionSegment;

    int pc=low_index;
    while(pc<high_index) {
        int prev_pc = pc;
        InstructionType opcode = (InstructionType)instructionSegment[pc++];
        
        BCRegister op0, op1, op2;
        InstructionControl control;
        InstructionCast cast;
        i64 imm;
        
        char buf[8];
        sprintf(buf, "%3d", prev_pc);
        log::out << log::GRAY << " " << buf << log::PURPLE << " " << instruction_names[opcode];
        log::out << log::NO_COLOR;
        
        switch(opcode) {
        case BC_HALT: break;
        case BC_NOP: break;
        case BC_RET: break;
        case BC_MOV_RR: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            log::out << " " << register_names[op0] << ", " << register_names[op1];
        } break;
        case BC_MOV_RM:
        case BC_MOV_MR:
        case BC_MOV_RM_DISP16:
        case BC_MOV_MR_DISP16: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            if(opcode == BC_MOV_RM_DISP16 || opcode == BC_MOV_MR_DISP16) {
                imm = *(i16*)(instructions.data() + pc);
                pc += 2;
            }
            
            switch(opcode) {
                case BC_MOV_RM: log::out << " " << register_names[op0] << ", [" << register_names[op1] << "]"; break;
                case BC_MOV_MR: log::out << " [" << register_names[op0] << "], " << register_names[op1]; break;
                case BC_MOV_RM_DISP16: log::out << " " << register_names[op0] << ", [" << register_names[op1]; if(imm >= 0) log::out << "+"; log::out << imm << "]"; break;
                case BC_MOV_MR_DISP16: log::out << " [" << register_names[op0]; if(imm >= 0) log::out << "+"; log::out << imm << "], " << register_names[op1]; break;
                default: Assert(false);
            }
            
            int size = GET_CONTROL_SIZE(control);
            if(size == CONTROL_8B)       log::out << ", byte";
            else if(size == CONTROL_16B) log::out << ", word";
            else if(size == CONTROL_32B) log::out << ", dword";
            else if(size == CONTROL_64B) log::out << ", qword";
        } break;
        case BC_SET_ARG:
        case BC_GET_PARAM:
        case BC_GET_VAL:
        case BC_SET_RET: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            control = (InstructionControl)instructions[pc++];
            
            // switch(opcode) {
            //     case BC_GET_VAL:   log::out << " " << register_names[op0] << ", [val"; if(imm >= 0) log::out << "+"; log::out << imm << "]"; break;
            //     case BC_GET_PARAM: log::out << " " << register_names[op0] << ", [param"; if(imm >= 0) log::out << "+"; log::out << imm << "]"; break;
            //     case BC_SET_ARG:   log::out << " [arg"; if(imm >= 0) log::out << "+"; log::out << imm << "], " << register_names[op0]; break;
            //     case BC_SET_RET:   log::out << " [ret"; if(imm >= 0) log::out << "+"; log::out << imm << "], " << register_names[op0]; break;
            //     default: Assert(false);
            // }
            switch(opcode) {
                case BC_GET_VAL:   log::out << " " << register_names[op0] << ", ["; if(imm > 0) log::out << "+"; log::out << imm << "]"; break;
                case BC_GET_PARAM: log::out << " " << register_names[op0] << ", ["; if(imm > 0) log::out << "+"; log::out << imm << "]"; break;
                case BC_SET_ARG:   log::out << " ["; if(imm > 0) log::out << "+"; log::out << imm << "], " << register_names[op0]; break;
                case BC_SET_RET:   log::out << " ["; if(imm > 0) log::out << "+"; log::out << imm << "], " << register_names[op0]; break;
                default: Assert(false);
            }
            
            int size = GET_CONTROL_SIZE(control);
            if(size == CONTROL_8B)       log::out << ", byte";
            else if(size == CONTROL_16B) log::out << ", word";
            else if(size == CONTROL_32B) log::out << ", dword";
            else if(size == CONTROL_64B) log::out << ", qword";
        } break;
        case BC_PTR_TO_LOCALS:
        case BC_PTR_TO_PARAMS: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            log::out << " " << register_names[op0] << ", " << log::GREEN <<  imm;
        } break;
        case BC_PUSH:
        case BC_POP: {
            op0 = (BCRegister)instructions[pc++];
            log::out << " " << register_names[op0];
        } break;
        case BC_LI32: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            log::out << " " << register_names[op0] << ", "<<log::GREEN;
            int expo = (0xFF & (imm >> 23)) - 127; // 8 bits exponent (0xFF), 23 mantissa, 127 bias
            if(expo > -24 && expo < 24) {
                // probably a float
                log::out << *(float*)&imm << log::GRAY << " (" << imm << ")";
            } else
                log::out << imm;
        } break;
        case BC_LI64: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i64*)&instructions[pc];
            pc+=8;
            log::out << " " << register_names[op0] << ", "<<log::GREEN;
            int expo = (0x7FF & (imm >> 52)) - 1023; // 11 bits exponent (0x7FF), 52 mantissa, 1023 bias
            if(expo > -24 && expo < 24) {
                // probably a 64-bit float
                log::out << *(double*)&imm << log::GRAY << " (" << imm << ")";
            } else
                log::out << imm;
        } break;
        case BC_INCR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            log::out << " " << register_names[op0] << ", " << log::GREEN << imm;
        } break;
        case BC_ALLOC_LOCAL: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(u16*)&instructions[pc];
            pc+=2;
            if(op0 == BC_REG_INVALID)
                log::out << " " << log::GREEN << imm;
            else
                log::out << " " << register_names[op0]<<", "<<log::GREEN << imm;
        } break;
        case BC_FREE_LOCAL: {
            imm = *(u16*)&instructions[pc];
            pc+=2;
            log::out << " " << log::GREEN << imm;
        } break;
        case BC_CALL: {
            LinkConventions l = (LinkConventions)instructions[pc++];
            CallConventions c = (CallConventions)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            log::out << " " << l << ", " << c <<  ", ";
            if(imm >= Bytecode::BEGIN_DLL_FUNC_INDEX && dll_functions) {
                int ind = imm - Bytecode::BEGIN_DLL_FUNC_INDEX;
                log::out << log::LIME << dll_functions->get(ind);
            } else if(imm < 0) {
                auto r = NativeRegistry::GetGlobal();
                auto f = r->findFunction(imm);
                if(f) {
                    log::out << log::LIME << f->name;
                } else
                    log::out << log::LIME << "?";
            } else {
                int ind = imm - 1;
                if(imm != 0)
                    log::out << log::LIME << code->tinyBytecodes[ind]->name;
                else
                    log::out << log::LIME << "?";
            }
        } break;
        case BC_JMP: {
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            int addr = pc + imm;
            
            log::out << log::GRAY << " :"<< addr;
        } break;
        case BC_JNZ:
        case BC_JZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            int addr = pc + imm;
            
            log::out << " "<<register_names[op0] <<", "<< log::GRAY << ":"<< addr;
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
        case BC_GTE:
        {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            log::out << " "<<register_names[op0] <<", "<< register_names[op1];
            // we don't care about sizes
            auto c = log::out.getColor();
            log::out << log::CYAN;
            if(IS_CONTROL_FLOAT(control))
                log::out << ", float";
            if(!IS_CONTROL_SIGNED(control))
                log::out << ", unsigned";
            int size = GET_CONTROL_SIZE(control);
            if(size == CONTROL_8B)       log::out << ", byte";
            else if(size == CONTROL_16B) log::out << ", word";
            else if(size == CONTROL_32B) log::out << ", dword";
            else if(size == CONTROL_64B) log::out << ", qword";
            log::out << c;
        } break;
        case BC_BAND:
        case BC_BOR:
        case BC_BXOR:
        case BC_BNOT:
        case BC_BLSHIFT:
        case BC_BRSHIFT:
        case BC_LAND:
        case BC_LOR:
        case BC_LNOT:
        {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            log::out << " "<<register_names[op0] <<", "<< register_names[op1];
        } break;
        case BC_DATAPTR:
        case BC_CODEPTR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            log::out << " " << register_names[op0] << ", "<<log::GREEN<<imm;
        } break;
        case BC_CAST: {
            op0 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            cast = (InstructionCast)instructions[pc++];
            
            u8 from_size = 1 << GET_CONTROL_SIZE(control);
            u8 to_size = 1 << GET_CONTROL_CONVERT_SIZE(control);
            
            log::out << " " << register_names[op0] << ", "<< cast_names[cast] <<", "<<from_size<<"b->"<<to_size<<"b";
        } break;
        case BC_MEMZERO: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            imm = *(i8*)&instructions[pc]; pc+=1;
            log::out << " " << register_names[op0] << ", "<< register_names[op1] << ", "<<log::GREEN<<imm;
        } break;
        case BC_MEMCPY: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            op2 = (BCRegister)instructions[pc++];
            log::out << " " << register_names[op0] << ", "<< register_names[op1] << ", " << register_names[op2];
        } break;
        case BC_RDTSC:
        case BC_SQRT:
        case BC_ROUND: {
            op0 = (BCRegister)instructions[pc++];
            log::out << " " << register_names[op0];
        } break;
        case BC_ATOMIC_CMP_SWAP: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            op2 = (BCRegister)instructions[pc++];
            log::out << " " << register_names[op0] << ", "<< register_names[op1] << ", " << register_names[op2];
        } break;
        case BC_STRLEN:
        case BC_ATOMIC_ADD: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            log::out << " " << register_names[op0] << ", "<< register_names[op1] << ", " << control;
        } break;
        case BC_TEST_VALUE: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            u8 size = (u8)instructions[pc++];
            imm = *(u32*)&instructions[pc];
            pc+=4;

            log::out << " " << register_names[op0] << ", "<< register_names[op1] << ", " << log::GREEN << size << ", "<<imm;
        } break;
        default: Assert(false);
        }
        
        if(high_index - low_index > 1 || force_newline)
            log::out << "\n";
    }
}
void Bytecode::print() {
    using namespace engone;
    for(auto& t : tinyBytecodes) {
        log::out <<log::GOLD<< "Tinycode: "<<t->name<<"\n";
        t->print(0,t->instructionSegment.size(),this, nullptr, true);
    }
}