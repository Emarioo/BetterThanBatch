#include "BetBat/Bytecode.h"
#include "BetBat/Compiler.h"

// included to get CallConvention
#include "BetBat/x64_gen.h"

#define ENABLE_BYTECODE_OPTIMIZATIONS

InstructionControl operator |(InstructionControl a, InstructionControl b) {
    return (InstructionControl)((int)a | (int)b);
}

u32 Bytecode::getMemoryUsage(){
    Assert(false);
    return 0;
    // uint32 sum = instructionSegment.max*instructionSegment.getTypeSize()
    //     +debugSegment.max*debugSegment.getTypeSize()
    //     // debugtext?        
    //     ;
    // return sum;
}

InstructionControl apply_size(InstructionControl c, int size) {
         if(size == 1) return c | CONTROL_8B;
    else if(size == 2) return c | CONTROL_16B;
    else if(size == 4) return c | CONTROL_32B;
    else if(size == 8) return c | CONTROL_64B;
    else Assert(false);
    return c;
}

bool Bytecode::addExportedFunction(const std::string& name, int tinycode_index, int* existing_tinycode_index) {
    for(int i=0;i<exportedFunctions.size();i++) {
        if(exportedFunctions[i].name == name) {
            if(tinycode_index == exportedFunctions[i].tinycode_index) {
                engone::log::out << engone::log::YELLOW << "Exporting function '"<<name <<"' again, compiler should not do that\n";
                return true;
            }
            if(existing_tinycode_index)
                *existing_tinycode_index = exportedFunctions[i].tinycode_index;
            return false;
        }
    }
    exportedFunctions.add({});
    exportedFunctions.last().name = name;
    exportedFunctions.last().tinycode_index = tinycode_index;
    return true;
}
void Bytecode::addExternalRelocation(const std::string& name, const std::string& library_path, int tinycode_index, int pc, ExternalRelocationType rel_type){
    ExternalRelocation tmp{};
    tmp.name = name;
    tmp.library_path = library_path;
    tmp.tinycode_index = tinycode_index;
    tmp.pc = pc;
    tmp.type = rel_type;
    externalRelocations.add(tmp);
}
void Bytecode::cleanup(){
    // instructionSegment.resize(0);
    dataSegment.resize(0);
    for(auto c : tinyBytecodes) {
        c->~TinyBytecode();
        TRACK_FREE(c, TinyBytecode);
    }
    // debugSegment.resize(0);
    // debugLocations.cleanup();
    // if(debugInformation) {
    //     DebugInformation::Destroy(debugInformation);
    //     debugInformation = nullptr;
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
    code->~Bytecode();
    TRACK_FREE(code, Bytecode);
    // engone::Free(code, sizeof(Bytecode));
}
#define DEFAULT_DATA_BYTE '\0'
// #define DEFAULT_DATA_BYTE '_'
void Bytecode::ensureAlignmentInData(int alignment){
    Assert(alignment > 0);
    // TODO: Check that alignment is a power of 2
    lock_global_data.lock();
    defer { lock_global_data.unlock(); };
    int misalign = alignment - (dataSegment.used % alignment);
    if(misalign == alignment) {
        return;
    }
    if(dataSegment.max < dataSegment.used + misalign){
        int oldMax = dataSegment.max;
        bool yes = dataSegment.resize(dataSegment.max*2 + 100);
        Assert(yes);
        memset(dataSegment.data() + oldMax, DEFAULT_DATA_BYTE, dataSegment.max - oldMax);
    }
    int index = dataSegment.used;
    memset((char*)dataSegment.data() + index,DEFAULT_DATA_BYTE,misalign);
    dataSegment.used+=misalign;
}
int Bytecode::appendData(const void* data, int size){
    Assert(size > 0);
    lock_global_data.lock();
    defer { lock_global_data.unlock(); };
    if(dataSegment.max < dataSegment.used + size){
        int oldMax = dataSegment.max;
        dataSegment.reserve(dataSegment.max*2 + 2*size);
        memset(dataSegment.data() + oldMax, DEFAULT_DATA_BYTE, dataSegment.max - oldMax);
    }
    int index = dataSegment.used;
    if(data) {
        memcpy((char*)dataSegment.data() + index,data,size);
    } else {
        memset((char*)dataSegment.data() + index,DEFAULT_DATA_BYTE,size);
    }
    dataSegment.used+=size;
    return index;
}
int Bytecode::add_assembly(const char* text, int len, const std::string& file, int line_start, int line_end) {
    lock_global_data.lock();
    defer { lock_global_data.unlock(); };
    
    asmInstances.add({});
    int asm_index = asmInstances.size() - 1;
    auto& inst = asmInstances.last();
    inst.file = file;
    inst.lineStart = line_start;
    inst.lineEnd = line_end;
    
    if(rawInlineAssembly.max < rawInlineAssembly.used + len){
        int oldMax = rawInlineAssembly.max;
        rawInlineAssembly.reserve(rawInlineAssembly.max*2 + 2*len);
        memset(rawInlineAssembly.data() + oldMax, ' ', rawInlineAssembly.max - oldMax);
    }
    inst.start = rawInlineAssembly.size();
    
    memcpy((char*)rawInlineAssembly.data() + inst.start, text, len);
    rawInlineAssembly.used += len;
    
    inst.end = rawInlineAssembly.size();
    
    return asm_index;
}

void BytecodeBuilder::init(Bytecode* code, TinyBytecode* tinycode, Compiler* compiler) {
    this->code = code;
    this->tinycode = tinycode;
    this->compiler = compiler;
    
    // TODO: If we add new variables then we can't forget to reset them here.
    //   We will forget to do so though so should we do something like
    //   this->cleanup(); new(this)BytecodeBuilder().;
    //   We want to keep some allocations though, we don't want to needlessly free and allocate.
    previous_instructions_head = 0;
    previous_instructions_count = 0;
    disable_code_gen = false;
    pushed_offset = 0;
    pushed_offset_max = 0;
    ret_offset = 0;
    has_return_values = false;
    virtual_stack_pointer = 0;
    
}

void BytecodeBuilder::emit_test(BCRegister to, BCRegister from, u8 size, i32 test_location) {
    emit_opcode(BC_TEST_VALUE);
    emit_operand(to);
    emit_operand(from);
    
    if(size == 1) emit_control(CONTROL_8B);
    else if(size == 2) emit_control(CONTROL_16B);
    else if(size == 4) emit_control(CONTROL_32B);
    else if(size == 8) emit_control(CONTROL_64B);
    else Assert(false);
    emit_imm32(test_location);
}
void BytecodeBuilder::emit_push(BCRegister reg) {
    // IMPORTANT: DON'T FORGET TO CHANGE emit_fake_push WHEN MODIFYING THIS FUNCTION!
    if(disable_code_gen) return;

    emit_opcode(BC_PUSH);
    emit_operand(reg);
    pushed_offset -= 8;
    if(pushed_offset < pushed_offset_max)
        pushed_offset_max = pushed_offset;
    virtual_stack_pointer -= 8;
}
void BytecodeBuilder::emit_fake_push() {
    if(disable_code_gen) return;

    // emit_opcode(BC_PUSH);
    // emit_operand(reg);
    // we dont emit any instructions, we just change the state of the builder
    pushed_offset -= 8;
    if(pushed_offset < pushed_offset_max)
        pushed_offset_max = pushed_offset;
    virtual_stack_pointer -= 8;
}
void BytecodeBuilder::emit_fake_pop() {
    if(disable_code_gen) return;

    // emit_opcode(BC_PUSH);
    // emit_operand(reg);
    // we dont emit any instructions, we just change the state of the builder
    pushed_offset += 8;
    virtual_stack_pointer += 8;
}
void BytecodeBuilder::emit_pop(BCRegister reg) {
    if(disable_code_gen) return;

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

    // TODO: Disable assert if there was an error.
    // Assert(pushed_offset < 0); // we have a bug if we popped a value that didn't exist

    emit_opcode(BC_POP);
    emit_operand(reg);
    pushed_offset += 8;
    virtual_stack_pointer += 8;
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
void BytecodeBuilder::emit_li(BCRegister reg, i64 imm, int size){
    if(size == 4) {
        emit_opcode(BC_LI32);
        emit_operand(reg);
        emit_imm32(imm);
    } else {
        emit_opcode(BC_LI64);
        emit_operand(reg);
        emit_imm64(imm);
    }
}
void BytecodeBuilder::emit_incr(BCRegister reg, i32 imm) {
    Assert(imm != 0); // incrementing by 0 is dumb

    emit_opcode(BC_INCR);
    emit_operand(reg);
    emit_imm32(imm);
}
void BytecodeBuilder::emit_asm(int asm_instance, int inputs, int outputs) {
    emit_opcode(BC_ASM);
    emit_imm8(inputs);
    emit_imm8(outputs);
    emit_imm32(asm_instance);
    
    tinycode->required_asm_instances.add(asm_instance);
}
void BytecodeBuilder::emit_alloc_local(BCRegister reg, u16 size) {
    if(disable_code_gen) return;
    if(compiler->compile_stats.errors == 0) {
        // We would need an array of pushed offsets
        // Assert(pushed_offset == 0);
    }
    Assert(size > 0);
    emit_opcode(BC_ALLOC_LOCAL);
    emit_operand(reg);
    emit_imm16(size);

    has_return_values = false;
    // pushed_offset = 0;
    // virtual_stack_pointer -= size;
}
void BytecodeBuilder::emit_alloc_local(BCRegister reg, int* index_to_size) {
    if(disable_code_gen) return;
    if(compiler->compile_stats.errors == 0) {
        // We would need an array of pushed offsets
        // Assert(pushed_offset == 0);
    }
    emit_opcode(BC_ALLOC_LOCAL);
    emit_operand(reg);
    *index_to_size = get_pc();
    emit_imm16(0);

    has_return_values = false;
    // pushed_offset = 0;
    // virtual_stack_pointer -= size;
}
void BytecodeBuilder::fix_local_imm(int index, u16 size) {
    if(disable_code_gen) return;
    *(u16*)&tinycode->instructionSegment[index] = size;
}
void BytecodeBuilder::emit_free_local(u16 size) {
    if(disable_code_gen) return;

    if(compiler->compile_stats.errors == 0) {
        // Assert(pushed_offset == 0); // We have some pushed values in the way and can't free local variables
    }
    // If size=0 is passed then it's probably a bug.
    // We should not do "if(size==0) return;" because then we won't catch those bugs.
    Assert(size > 0);
    emit_opcode(BC_FREE_LOCAL);
    emit_imm16(size);

    if(has_return_values)
        ret_offset -= size;
    // virtual_stack_pointer += size;
}
void BytecodeBuilder::emit_free_local(int* index_to_size) {
    if(disable_code_gen) return;

    if(compiler->compile_stats.errors == 0) {
        // Assert(pushed_offset == 0); // We have some pushed values in the way and can't free local variables
    }
    // If size=0 is passed then it's probably a bug.
    // We should not do "if(size==0) return;" because then we won't catch those bugs.
    emit_opcode(BC_FREE_LOCAL);
    *index_to_size = get_pc();
    emit_imm16(0);

    // if(has_return_values)
    //     ret_offset -= size;
    // virtual_stack_pointer += size;
}
void BytecodeBuilder::emit_alloc_args(BCRegister reg, u16 size) {
    if(disable_code_gen) return;
    if(compiler->compile_stats.errors == 0) {

    }
    // Assert(size > 0); zero size = zero arguments can happen and we can't skip instruction because
    // we want alignment
    emit_opcode(BC_ALLOC_ARGS);
    emit_operand(reg);
    emit_imm16(size);

    has_return_values = false;
    // pushed_offset = 0;
    // virtual_stack_pointer -= size;
}
void BytecodeBuilder::emit_empty_alloc_args(int* out_size_offset) {
    if(disable_code_gen) return;
    if(compiler->compile_stats.errors == 0) {

    }
    Assert(out_size_offset);
    // Assert(size > 0); zero size = zero arguments can happen and we can't skip instruction because
    // we want alignment
    emit_imm8(BC_NOP);
    emit_imm8(BC_NOP);
    // emit_opcode(BC_ALLOC_ARGS);
    // emit_operand(BC_REG_INVALID);
    *out_size_offset = get_pc();
    emit_imm8(BC_NOP);
    emit_imm8(BC_NOP);
    // emit_imm16(0);

    has_return_values = false;
    // pushed_offset = 0;
    // VP needs fixing? or not?
    // virtual_stack_pointer -= size;
}
void BytecodeBuilder::fix_alloc_args(int index, u16 size) {
    auto ptr = tinycode->instructionSegment.data();
    *(u8*)(ptr + index - 2) = BC_ALLOC_ARGS;
    *(u8*)(ptr + index - 1) = BC_REG_INVALID;
    *(u16*)(ptr + index) = size;
    
    // virtual_stack_pointer -= size;
}
void BytecodeBuilder::emit_free_args(u16 size) {
    if(disable_code_gen) return;

    if(compiler->compile_stats.errors == 0) {
        // Assert(pushed_offset == 0); // We have some pushed values in the way and can't free local variables
    }
    emit_opcode(BC_FREE_ARGS);
    emit_imm16(size);

    if(has_return_values)
        ret_offset -= size;
    // virtual_stack_pointer += size;
}

void BytecodeBuilder::emit_set_arg(BCRegister reg, i16 imm, int size, bool is_float, bool is_signed) {
    emit_opcode(BC_SET_ARG);
    emit_operand(reg);
    
    InstructionControl control=CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(is_signed) control = (InstructionControl)(control | CONTROL_SIGNED_OP);
    control = apply_size(control, size);
    emit_control(control);
    
    emit_imm16(imm);
}
void BytecodeBuilder::emit_get_param(BCRegister reg, i16 imm, int size, bool is_float, bool is_signed){
    emit_opcode(BC_GET_PARAM);
    emit_operand(reg);
    
    InstructionControl control=CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(is_signed) control = (InstructionControl)(control | CONTROL_SIGNED_OP);
    control = apply_size(control, size);
    emit_control(control);
    emit_imm16(imm);
}
void BytecodeBuilder::emit_set_ret(BCRegister reg, i16 imm, int size, bool is_float, bool is_signed){
    if(tinycode->call_convention == CallConvention::STDCALL||tinycode->call_convention == CallConvention::UNIXCALL) {
        Assert(imm == -8 || imm == -4);
    }
    
    emit_opcode(BC_SET_RET);
    emit_operand(reg);
    
    InstructionControl control=CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(is_signed) control = (InstructionControl)(control | CONTROL_SIGNED_OP);
    control = apply_size(control, size);
    emit_control(control);
    emit_imm16(imm);
}
void BytecodeBuilder::emit_get_val(BCRegister reg, i16 imm, int size, bool is_float, bool is_signed){
    Assert(has_return_values);
    const int FRAME_SIZE = 16;
    // int off = imm + ret_offset - FRAME_SIZE;
    // off = -24
    // push = -8 (ok) -16 (ok) -24 (not ok)
    // engone::log::out << (pushed_offset_max) << " " << (off)<<".."<<(off+size) << "\n";
    // if(pushed_offset_max >= off + size)
    //     engone::log::out << " false\n";
    // THIS IS CORRECT, DON'T CHANGE THE MATH!
    // structs members and return values are pushed in different orders which is why
    // structs can have 3 members without value being overwritten while multiple return values can only have 2.
    // TODO: This is obviously a flaw in the BETBAT calling convention. How do we fix it?
    if(compiler->compile_stats.errors == 0) {
        // Assert(("Multiple return values has limits (at least 3 8-byte values will work). BC_PUSH can overwrite the return values in the frame of the callee.",pushed_offset_max >= off + size)); // Asserts if return value we try to access was overwritten by pushed values
    }

    emit_opcode(BC_GET_VAL);
    emit_operand(reg);
    
    InstructionControl control=CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(is_signed) control = (InstructionControl)(control | CONTROL_SIGNED_OP);
    control = apply_size(control, size);
    emit_control(control);
    emit_imm16(imm);
}
void BytecodeBuilder::emit_call(LinkConvention l, CallConvention c, i32* index_of_relocation, i32 imm) {
    if(disable_code_gen) return;

    Assert(l != LinkConvention::IMPORT);

    emit_opcode(BC_CALL);
    emit_imm8(l);
    emit_imm8(c);
    *index_of_relocation = tinycode->instructionSegment.used;
    emit_imm32(imm);

    has_return_values = true;
    ret_offset = pushed_offset;
    pushed_offset_max = pushed_offset;
}
void BytecodeBuilder::emit_call_reg(BCRegister reg, LinkConvention l, CallConvention c) {
    if(disable_code_gen) return;

    emit_opcode(BC_CALL_REG);
    emit_operand(reg);
    emit_imm8(l);
    emit_imm8(c);

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
    // We have a bug if we pushed more or less values than we popped.
    // TODO: We need to notify builder that an error occured because we shouldn't assert if so.
    // Assert(pushed_offset == 0);
    emit_opcode(BC_RET);
}
void BytecodeBuilder::emit_jmp(int pc) {
    emit_opcode(BC_JMP);
    emit_imm32(pc - (get_pc() + 4)); // +4 because of immediate
}
void BytecodeBuilder::emit_jmp(int* out_imm_offset) {
    emit_opcode(BC_JMP);
    if(out_imm_offset)
        *out_imm_offset = get_pc();
    emit_imm32(0);
}
void BytecodeBuilder::emit_jz(BCRegister reg, int pc) {
    emit_opcode(BC_JZ);
    emit_operand(reg);
    emit_imm32(pc - (get_pc() + 4));  // +4 because of immediate
}
void BytecodeBuilder::emit_jz(BCRegister reg, int* out_imm_offset) {
    emit_opcode(BC_JZ);
    emit_operand(reg);
    if(out_imm_offset)
        *out_imm_offset = get_pc();
    emit_imm32(0);
}
void BytecodeBuilder::emit_jnz(BCRegister reg, int pc) {
    emit_opcode(BC_JNZ);
    emit_operand(reg);
    emit_imm32(pc - (get_pc() + 4)); // +4 because of immediate
}
void BytecodeBuilder::emit_jnz(BCRegister reg, int* out_imm_offset) {
    emit_opcode(BC_JNZ);
    emit_operand(reg);
    if(out_imm_offset)
        *out_imm_offset = get_pc();
    emit_imm32(0);
}
void BytecodeBuilder::fix_jump_imm32_here(int imm_index) {
    if(disable_code_gen) return;

    *(int*)&tinycode->instructionSegment[imm_index] = get_pc() - (imm_index + 4); // +4 because immediate should be relative to the end of the instruction, not relative to the offset within the instruction
    // engone::log::out << "fixed "<< (*(int*)&tinycode->instructionSegment[imm_index])<<"\n";
}
void BytecodeBuilder::emit_mov_rr(BCRegister to, BCRegister from){
    emit_opcode(BC_MOV_RR);
    emit_operand(to);
    emit_operand(from);
}

void BytecodeBuilder::emit_mov_rm(BCRegister to, BCRegister from, int size){
    Assert(from != BC_REG_LOCALS);

    emit_opcode(BC_MOV_RM);
    emit_operand(to);
    emit_operand(from);
    
    InstructionControl control = CONTROL_NONE;
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_mov_mr(BCRegister to, BCRegister from, int size){
    Assert(to != BC_REG_LOCALS);

    emit_opcode(BC_MOV_MR);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    control = apply_size(control, size);
    emit_control(control);
}

void BytecodeBuilder::emit_mov_rm_disp(BCRegister to, BCRegister from, int size, int displacement){
    Assert(!(from == BC_REG_LOCALS && displacement == 0));
    // Assert(to != BC_REG_T1 && from != BC_REG_T1); // Is this important?
    if(displacement == 0) {
        emit_mov_rm(to, from, size);
        return;
    }
    
    emit_opcode(BC_MOV_RM_DISP16);
    emit_operand(to);
    emit_operand(from);
    
    InstructionControl control = CONTROL_NONE;
    control = apply_size(control, size);
    emit_control(control);
    
    Assert(displacement < 0x8000 && displacement >= -0x8000);
    emit_imm16(displacement);
}
void BytecodeBuilder::emit_mov_mr_disp(BCRegister to, BCRegister from, int size, int displacement){
    Assert(!(to == BC_REG_LOCALS && displacement == 0));
    // Assert(to != BC_REG_T1 && from != BC_REG_T1); // Is this important?
    if(displacement == 0) {
        emit_mov_mr(to, from, size);
        return;
    }
    
    emit_opcode(BC_MOV_MR_DISP16);
    emit_operand(to);
    emit_operand(from);
     
    InstructionControl control = CONTROL_NONE;
    control = apply_size(control, size);
    emit_control(control);
    
    Assert(displacement < 0x8000 && displacement >= -0x8000);
    emit_imm16(displacement);
}

void BytecodeBuilder::emit_add(BCRegister to, BCRegister from, int size, bool is_float, bool is_signed) {
    emit_opcode(BC_ADD);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(is_signed) control = (InstructionControl)(control | CONTROL_SIGNED_OP);
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_add_imm32(BCRegister to, BCRegister from, int imm, int size) {
    if(size == 8)
        emit_li64(to, imm);
    else
        emit_li32(to, imm);
    emit_add(to, from, size, false, true);
}
void BytecodeBuilder::emit_sub(BCRegister to, BCRegister from, int size, bool is_float, bool is_signed) {
    emit_opcode(BC_SUB);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(is_signed) control = (InstructionControl)(control | CONTROL_SIGNED_OP);
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_mul(BCRegister to, BCRegister from, int size, bool is_float, bool is_signed) {
    emit_opcode(BC_MUL);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(is_signed) control = (InstructionControl)(control | CONTROL_SIGNED_OP);
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_div(BCRegister to, BCRegister from, int size, bool is_float, bool is_signed) {
    emit_opcode(BC_DIV);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(is_signed) control = (InstructionControl)(control | CONTROL_SIGNED_OP);
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_mod(BCRegister to, BCRegister from, int size, bool is_float, bool is_signed) {
    emit_opcode(BC_MOD);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(is_signed) control = (InstructionControl)(control | CONTROL_SIGNED_OP);
    control = apply_size(control, size);
    emit_control(control);
}

void BytecodeBuilder::emit_band(BCRegister to, BCRegister from, int size) {
    emit_opcode(BC_BAND);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_bor(BCRegister to, BCRegister from, int size) {
    emit_opcode(BC_BOR);
    emit_operand(to);
    emit_operand(from);   
    
    InstructionControl control = CONTROL_NONE;
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_bxor(BCRegister to, BCRegister from, int size) {
    emit_opcode(BC_BXOR);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_bnot(BCRegister to, BCRegister from, int size) {
    emit_opcode(BC_BNOT);
    emit_operand(to);
    emit_operand(from);   
    InstructionControl control = CONTROL_NONE;
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_blshift(BCRegister to, BCRegister from, int size) {
    emit_opcode(BC_BLSHIFT);
    emit_operand(to);
    emit_operand(from);   
    InstructionControl control = CONTROL_NONE;
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_brshift(BCRegister to, BCRegister from, int size) {
    emit_opcode(BC_BRSHIFT);
    emit_operand(to);
    emit_operand(from);   
    InstructionControl control = CONTROL_NONE;
    control = apply_size(control, size);
    emit_control(control);
}

void BytecodeBuilder::emit_eq(BCRegister to, BCRegister from, int size, bool is_float){
    emit_opcode(BC_EQ);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_neq(BCRegister to, BCRegister from, int size, bool is_float){
    emit_opcode(BC_NEQ);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_lt(BCRegister to, BCRegister from, int size, bool is_float, bool is_signed){
    emit_opcode(BC_LT);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(is_signed) control = (InstructionControl)(control | CONTROL_SIGNED_OP);
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_lte(BCRegister to, BCRegister from, int size, bool is_float, bool is_signed){
    emit_opcode(BC_LTE);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(is_signed) control = (InstructionControl)(control | CONTROL_SIGNED_OP);
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_gt(BCRegister to, BCRegister from, int size, bool is_float, bool is_signed){
    emit_opcode(BC_GT);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(is_signed) control = (InstructionControl)(control | CONTROL_SIGNED_OP);
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_gte(BCRegister to, BCRegister from, int size, bool is_float, bool is_signed){
    emit_opcode(BC_GTE);
    emit_operand(to);
    emit_operand(from);
    InstructionControl control = CONTROL_NONE;
    if(is_float) control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    if(is_signed) control = (InstructionControl)(control | CONTROL_SIGNED_OP);
    control = apply_size(control, size);
    emit_control(control);
}
 
void BytecodeBuilder::emit_land(BCRegister to, BCRegister from, int size) {
    emit_opcode(BC_LAND);
    emit_operand(to);
    emit_operand(from);
    
    InstructionControl control = CONTROL_NONE;
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_lor(BCRegister to, BCRegister from, int size) {
    emit_opcode(BC_LOR);
    emit_operand(to);
    emit_operand(from);
    
    InstructionControl control = CONTROL_NONE;
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_lnot(BCRegister to, BCRegister from, int size) {
    emit_opcode(BC_LNOT);
    emit_operand(to);
    emit_operand(from);
    
    InstructionControl control = CONTROL_NONE;
    control = apply_size(control, size);
    emit_control(control);
}
void BytecodeBuilder::emit_dataptr(BCRegister reg, i32 imm) {
    emit_opcode(BC_DATAPTR);
    emit_operand(reg);
    emit_imm32(imm);
}
void BytecodeBuilder::emit_ext_dataptr(BCRegister reg, LinkConvention link) {
    Assert(link != LinkConvention::IMPORT);
    emit_opcode(BC_EXT_DATAPTR);
    emit_operand(reg);
    emit_imm8(link);
}
void BytecodeBuilder::emit_codeptr(BCRegister reg, i32 imm) {
    emit_opcode(BC_CODEPTR);
    emit_operand(reg);
    emit_imm32(imm);
}
void BytecodeBuilder::emit_memzero(BCRegister ptr_reg, BCRegister size_reg, u8 batch) {
    Assert(ptr_reg != BC_REG_LOCALS);
    Assert(ptr_reg != size_reg);
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

void BytecodeBuilder::emit_cast(BCRegister to, BCRegister from, InstructionControl _control, u8 from_size, u8 to_size) {
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
    if(IS_CONTROL_FLOAT(_control))
        control = (InstructionControl)(control | CONTROL_FLOAT_OP);
    else if(IS_CONTROL_SIGNED(_control))
        control = (InstructionControl)(control | CONTROL_SIGNED_OP);
    if(IS_CONTROL_CONVERT_FLOAT(_control))
        control = (InstructionControl)(control | CONTROL_CONVERT_FLOAT_OP);
    else if(IS_CONTROL_CONVERT_SIGNED(_control))
        control = (InstructionControl)(control | CONTROL_CONVERT_SIGNED_OP);
    
    emit_opcode(BC_CAST);
    emit_operand(to);
    emit_operand(from);
    emit_control(control);
}

void BytecodeBuilder::emit_opcode(InstructionOpcode type) {
    if(disable_code_gen) return;
    append_previous_instruction(tinycode->instructionSegment.size());
    tinycode->instructionSegment.add((u8)type);
    tinycode->index_of_lines.resize(tinycode->instructionSegment.size());
    tinycode->index_of_lines[tinycode->instructionSegment.size()-1] = tinycode->lines.size();
}
void BytecodeBuilder::emit_operand(BCRegister reg) {
    if(disable_code_gen) return;
    tinycode->instructionSegment.add((u8)reg);
}
void BytecodeBuilder::emit_control(InstructionControl control) {
    if(disable_code_gen) return;
    tinycode->instructionSegment.add((u8)control);
    bool is_float = IS_CONTROL_FLOAT(control);
    bool is_signed = IS_CONTROL_SIGNED(control);
    Assert((!is_float && !is_signed) || (!is_float || !is_signed));
    Assert(!is_float || (GET_CONTROL_SIZE(control) == CONTROL_32B || GET_CONTROL_SIZE(control) == CONTROL_64B));

    is_float = IS_CONTROL_CONVERT_FLOAT(control);
    is_signed = IS_CONTROL_CONVERT_SIGNED(control);
    Assert((!is_float && !is_signed) || (!is_float || !is_signed));
    Assert(!is_float || (GET_CONTROL_CONVERT_SIZE(control) == CONTROL_32B || GET_CONTROL_CONVERT_SIZE(control) == CONTROL_64B));
}

void BytecodeBuilder::emit_imm8(i8 imm) {
    if(disable_code_gen) return;
    tinycode->instructionSegment.add(0);
    // get the pointer after add() because of reallocations
    i8* ptr = (i8*)(tinycode->instructionSegment.data() + tinycode->instructionSegment.size() - 1);
    *ptr = imm;
}
void BytecodeBuilder::emit_imm16(i16 imm) {
    if(disable_code_gen) return;
    tinycode->instructionSegment.add(0);
    tinycode->instructionSegment.add(0);
    // get the pointer after add() because of reallocations
    i16* ptr = (i16*)(tinycode->instructionSegment.data() + tinycode->instructionSegment.size() - 2);
    *ptr = imm;
}
void BytecodeBuilder::emit_imm32(i32 imm) {
    if(disable_code_gen) return;
    tinycode->instructionSegment.add(0); // TODO: OPTIMIZE
    tinycode->instructionSegment.add(0);
    tinycode->instructionSegment.add(0);
    tinycode->instructionSegment.add(0);
    // get the pointer after add() because of reallocations
    i32* ptr = (i32*)(tinycode->instructionSegment.data() + tinycode->instructionSegment.size() - 4);
    *ptr = imm;
}
void BytecodeBuilder::emit_imm64(i64 imm) {
    if(disable_code_gen) return;
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
bool TinyBytecode::applyRelocations(Bytecode* code, bool assert_on_failure, FuncImpl** unresolved_func) {
    using namespace engone;
    TRACE_FUNC()

    CALLBACK_ON_ASSERT(
        for(int i=0;i<call_relocations.size();i++) {
            auto& rel = call_relocations[i];
            if (!rel.funcImpl) {
                log::out << "bad reloc " << rel.pc<<"\n";
            } else if (!rel.funcImpl->tinycode_id) {
                log::out << "bad reloc pc:" << rel.pc <<", " << rel.funcImpl->astFunction->name << " at ?" <<"\n";
            }
        }
    )

    bool suc = true;

    for(int i=0;i<call_relocations.size();i++) {
        auto& rel = call_relocations[i];
        // Assert may fire if function wasn't generated.
        // Perhaps no one declared usage of the function even
        // though we should have.
        if (!rel.funcImpl->tinycode_id) {
            if(rel.funcImpl->usages == 0) {
                log::out << log::RED << "COMPILER BUG! "<<log::NO_COLOR<<"Function '"<<log::LIME<<rel.funcImpl->astFunction->name<<log::NO_COLOR<<"' had zero declared usages but generator emitted relocations.\n";
                Assert(false);
            } else {
                if(assert_on_failure) {
                    Assert((rel.funcImpl && rel.funcImpl->tinycode_id));
                }
                if(unresolved_func)
                    *unresolved_func = rel.funcImpl;
                suc = false;
                break;
            }
        }
        *(i32*)&instructionSegment[rel.pc] = rel.funcImpl->tinycode_id;
    }
    return suc;
}

const char* cast_names[] {
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
const char* instruction_names[] {
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
    "alloc_args", // BC_ALLOC_ARGS
    "free_args", // BC_FREE_ARGS
    "set_arg", // 
    "get_param", // 
    "get_val", // 
    "set_ret", // 
    "ptr_to_locals",
    "ptr_to_params",
    "jmp", // BC_JMP
    "call", // BC_CALL
    "call_reg", // BC_CALL
    "ret", // BC_RET
    "jnz", // BC_JNZ
    "jz", // BC_JZ
    "dataptr", // BC_DATAPTR
    "ext_dataptr", // BC_EXT_DATAPTR
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
    "prints", // BC_PRINTS
    "printc", // BC_PRINTC
    "test", // BC_TEST_VALUE
};
InstBaseType operator|(InstBaseType a, InstBaseType b) {
    return (InstBaseType)((int)a | (int)b);
}
#define BASE_op3 (BASE_op3 | BASE_op2 | BASE_op1)
#define BASE_op2 (BASE_op2 | BASE_op1)
BCInstructionInfo instruction_contents[256] {
    { 1, BASE_NONE }, // BC_HALT
    { 1, BASE_NONE }, // BC_NOP
    
    { 3, BASE_op2 },                           // BC_MOV_RR,
    { 4, BASE_op2 | BASE_ctrl },               // BC_MOV_RM,
    { 4, BASE_op2 | BASE_ctrl },               // BC_MOV_MR,
    { 6, BASE_op2 | BASE_ctrl | BASE_imm16 },  // BC_MOV_RM_DISP16,
    { 6, BASE_op2 | BASE_ctrl | BASE_imm16 },  // BC_MOV_MR_DISP16,
    
    { 2, BASE_op1 },                   // BC_PUSH,
    { 2, BASE_op1 },                   // BC_POP,
    { 6, BASE_op1 | BASE_imm32 },      // BC_LI32,
    { 10, BASE_op1 | BASE_imm64 },      // BC_LI64,
    { 6, BASE_op1 | BASE_imm32 },      // BC_INCR,
    { 4, BASE_op1 | BASE_imm16 },      // BC_ALLOC_LOCAL,
    { 3, BASE_imm16 },                   // BC_FREE_LOCAL,
    
    { 4, BASE_op1 | BASE_imm16 },        // BC_ALLOC_ARGS,
    { 3, BASE_imm16 },                   // BC_FREE_ARGS,

    { 5, BASE_op1 | BASE_ctrl | BASE_imm16 },  // BC_SET_ARG,
    { 5, BASE_op1 | BASE_ctrl | BASE_imm16 },  // BC_GET_PARAM,
    { 5, BASE_op1 | BASE_ctrl | BASE_imm16 },  // BC_GET_VAL,
    { 5, BASE_op1 | BASE_ctrl | BASE_imm16 },  // BC_SET_RET,
    { 4, BASE_op1 | BASE_imm16 },  // BC_PTR_TO_LOCALS,
    { 4, BASE_op1 | BASE_imm16 },  // BC_PTR_TO_PARAMS,

    { 5, BASE_imm32 },                         // BC_JMP,
    { 7, BASE_link | BASE_call | BASE_imm32 }, // BC_CALL
    { 4, BASE_op1 | BASE_link | BASE_call }, // BC_CALL
    { 1, BASE_NONE },                          // BC_RET,
    { 6, BASE_op1 | BASE_imm32 },              // BC_JNZ,
    { 6, BASE_op1 | BASE_imm32 },              // BC_JZ,

    { 6, BASE_op1 | BASE_imm32 },  // BC_DATAPTR,
    { 3, BASE_op1 | BASE_link },   // BC_EXT_DATAPTR,
    { 6, BASE_op1 | BASE_imm32 },  // BC_CODEPTR,

    { 4, BASE_op2 | BASE_ctrl },   // BC_ADD,
    { 4, BASE_op2 | BASE_ctrl },   // BC_SUB,
    { 4, BASE_op2 | BASE_ctrl },   // BC_MUL,
    { 4, BASE_op2 | BASE_ctrl },   // BC_DIV,
    { 4, BASE_op2 | BASE_ctrl },   // BC_MOD,
    
    { 4, BASE_op2 | BASE_ctrl },   // BC_EQ,
    { 4, BASE_op2 | BASE_ctrl },   // BC_NEQ,
    { 4, BASE_op2 | BASE_ctrl },   // BC_LT,
    { 4, BASE_op2 | BASE_ctrl },   // BC_LTE,
    { 4, BASE_op2 | BASE_ctrl },   // BC_GT,
    { 4, BASE_op2 | BASE_ctrl },   // BC_GTE,

    { 4, BASE_op2 | BASE_ctrl },   // BC_LAND,
    { 4, BASE_op2 | BASE_ctrl },   // BC_LOR,
    { 4, BASE_op2 | BASE_ctrl },   // BC_LNOT,

    { 4, BASE_op2 | BASE_ctrl },   // BC_BAND,
    { 4, BASE_op2 | BASE_ctrl },   // BC_BOR,
    { 4, BASE_op2 | BASE_ctrl },   // BC_BNOT,
    { 4, BASE_op2 | BASE_ctrl },   // BC_BXOR,
    { 4, BASE_op2 | BASE_ctrl },   // BC_BLSHIFT,
    { 4, BASE_op2 | BASE_ctrl },   // BC_BRSHIFT,

    { 4, BASE_op2 | BASE_ctrl }, // BC_CAST,

    { 4, BASE_op2 | BASE_ctrl },   // BC_MEMZERO,
    { 4, BASE_op3 },               // BC_MEMCPY,
    { 3, BASE_op2 },               // BC_STRLEN,
    { 2, BASE_op1 },               // BC_RDTSC,
    { 4, BASE_op3 },               // BC_ATOMIC_CMP_SWAP,
    { 4, BASE_op2 | BASE_ctrl },   // BC_ATOMIC_ADD,

    { 2, BASE_op1 }, // BC_SQRT,
    { 2, BASE_op1 }, // BC_ROUND,

    { 7, BASE_imm32 | BASE_imm16 }, // BC_ASM,
    { 3, BASE_op2 },                // BC_PRINTS,
    { 2, BASE_op1 },                // BC_PRINTC,

    { 8, BASE_op2 | BASE_ctrl | BASE_imm32 }, // BC_TEST_VALUE,

    // BASE_NONE, // BC_EXTEND1 = 253,
    // BASE_NONE, // BC_EXTEND2 = 254,
    // BASE_NONE, // BC_RESERVED = 255,
};
#undef BASE_op2
#undef BASE_op3
const char* register_names[] {
    "invalid", // BC_REG_INVALID
    "a", // BC_REG_A
    "b", // BC_REG_B
    "c", // BC_REG_C
    "d", // BC_REG_D
    "e", // BC_REG_E
    "f", // BC_REG_F
    "g",
    "h",
    "i",
    "j",
    "k",
    "l",
    "m",
    "n",
    "o",
    "p",
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
engone::Logger& operator<<(engone::Logger& l, InstructionOpcode t) {
    int len = sizeof(instruction_names)/sizeof(*instruction_names);
    if((int)t < len)
        return l << instruction_names[t];
    else
        return l << "{" << (int)t << "}";
}
engone::Logger& operator<<(engone::Logger& l, BCRegister r){
    return l << register_names[r];
}
engone::Logger& operator <<(engone::Logger& logger, InstructionControl c) {
    logger << "{";
    int size = GET_CONTROL_SIZE(c);
         if(size == CONTROL_8B)  logger << "1";
    else if(size == CONTROL_16B) logger << "2";
    else if(size == CONTROL_32B) logger << "4";
    else if(size == CONTROL_64B) logger << "8";
    if(IS_CONTROL_FLOAT(c))
        logger << "f";
    else if (IS_CONTROL_UNSIGNED(c))
        logger << "u";
    else
        logger << "s";
    
    logger << "}";
    return logger;
}
void TinyBytecode::print(int low_index, int high_index, Bytecode* code, DynamicArray<std::string>* dll_functions, bool force_newline, BytecodePrintCache* print_cache) {
    using namespace engone;
    bool print_one_inst = high_index - low_index == 1;
    if(high_index == -1 || high_index > instructionSegment.size())
        high_index = instructionSegment.size();
    
    auto& instructions = instructionSegment;

    bool print_debug_info = true;
    auto tinycode = this;
    auto tiny_index = index;
    
    BytecodePrintCache _local_print_cache{};
    bool specified_cache = print_cache;
    if (!print_cache)
        print_cache = &_local_print_cache;
    
    auto print_control = [](InstructionControl control) {
        auto prev_color = log::out.getColor();
        log::out << log::CYAN;
        int size = GET_CONTROL_SIZE(control);
        if(size == CONTROL_8B)       log::out << ", byte";
        else if(size == CONTROL_16B) log::out << ", word";
        else if(size == CONTROL_32B) log::out << ", dword";
        else if(size == CONTROL_64B) log::out << ", qword";
        if(IS_CONTROL_FLOAT(control))
            log::out << ", float";
        if(IS_CONTROL_UNSIGNED(control))
            log::out << ", unsigned";
        log::out << prev_color;
    };

    int pc=low_index;
    while(pc<high_index) {
        int prev_pc = pc;
        InstructionOpcode opcode = (InstructionOpcode)instructionSegment[pc++];
        
        BCRegister op0, op1, op2;
        InstructionControl control;
        i64 imm;

        if(print_debug_info) {
            // TODO: Code was copied from Virtual Machine. Can we abstract debug printing
            //   to a function which we can use in both places?
            //   Perhaps an argument 'enable_debug_printing'
            auto line_index = tinycode->index_of_lines[prev_pc] - 1;
            if(line_index != -1) {
                auto& line = tinycode->lines[line_index];

                if(line.line_number != print_cache->prev_line || tiny_index != print_cache->prev_tinyindex) {
                    if(tiny_index != print_cache->prev_tinyindex) {
                        if(!print_one_inst ||specified_cache) {
                            log::out << log::GOLD  << tinycode->name <<"\n";
                            print_cache->prev_tinyindex = tiny_index;
                        }
                    }
                    print_cache->prev_tinyindex = tiny_index;
                    log::out << log::CYAN << line.line_number << "| " << line.text<<"\n";
                    print_cache->prev_line = line.line_number;
                }
            } else if(tiny_index != print_cache->prev_tinyindex) {
                if(!print_one_inst || specified_cache) {
                    log::out << log::GOLD  << tinycode->name <<"\n";
                    print_cache->prev_tinyindex = tiny_index;
                }
            }
            
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
        }
        
        char buf[8];
        sprintf(buf, "%3d", prev_pc);
        log::out << log::GRAY << " " << buf << log::PURPLE << " " << opcode;
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
            imm = 0;
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
            
            print_control(control);
        } break;
        case BC_SET_ARG:
        case BC_GET_PARAM:
        case BC_GET_VAL:
        case BC_SET_RET: {
            op0 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            
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
            
            print_control(control);
                
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
        case BC_ALLOC_LOCAL:
        case BC_ALLOC_ARGS: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(u16*)&instructions[pc];
            pc+=2;
            if(op0 == BC_REG_INVALID)
                log::out << " " << log::GREEN << imm;
            else
                log::out << " " << register_names[op0]<<", "<<log::GREEN << imm;
        } break;
        case BC_FREE_LOCAL:
        case BC_FREE_ARGS: {
            imm = *(u16*)&instructions[pc];
            pc+=2;
            log::out << " " << log::GREEN << imm;
        } break;
        case BC_CALL: {
            LinkConvention l = (LinkConvention)instructions[pc++];
            CallConvention c = (CallConvention)instructions[pc++];
            int reloc_pos = pc;
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            log::out << " " << l << ", " << c <<  ", ";
            if(imm >= Bytecode::BEGIN_DLL_FUNC_INDEX && dll_functions) {
                int ind = imm - Bytecode::BEGIN_DLL_FUNC_INDEX;
                log::out << log::LIME << dll_functions->get(ind);
            // } else if(imm < 0) {
            //     auto r = NativeRegistry::GetGlobal();
            //     auto f = r->findFunction(imm);
            //     if(f) {
            //         log::out << log::LIME << f->name;
            //     } else
            //         log::out << log::LIME << imm;
            } else {
                int ind = imm - 1;
                if(ind > 0 && ind < code->tinyBytecodes.size())
                    log::out << log::LIME << code->tinyBytecodes[ind]->name;
                else {
                    bool found = false;
                    if (ind == -1) {
                        for(auto& rel : call_relocations) {
                            if (reloc_pos == rel.pc) {
                                log::out << log::LIME << rel.funcImpl->astFunction->name;
                                found = true;
                                break;
                            }
                        }
                    }
                    if(!found)
                        log::out << log::LIME << imm;
                }
            }
        } break;
        case BC_CALL_REG: {
            op0 = (BCRegister)instructions[pc++];
            LinkConvention l = (LinkConvention)instructions[pc++];
            CallConvention c = (CallConvention)instructions[pc++];
            
            log::out << " " << register_names[op0] << ", " << l << ", " << c <<  ", ";
        } break;
        case BC_JMP: {
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            int addr = pc + imm;
            
            log::out << log::GRAY << " :"<< addr << " ("<<imm<<")";
        } break;
        case BC_JNZ:
        case BC_JZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            int addr = pc + imm;
            
            log::out << " "<<register_names[op0] <<", "<< log::GRAY << ":"<< addr << " ("<<imm<<")";
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
            
            print_control(control);
        } break;
        case BC_BAND:
        case BC_BOR:
        case BC_BXOR:
        case BC_BNOT:
        case BC_BLSHIFT:
        case BC_BRSHIFT: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            log::out << " "<<register_names[op0] <<", "<< register_names[op1] << ", "<<control;
        } break;
        case BC_LAND:
        case BC_LOR:
        case BC_LNOT: {
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
        case BC_EXT_DATAPTR: {
            op0 = (BCRegister)instructions[pc++];
            LinkConvention link = *(LinkConvention*)&instructions[pc++];
            log::out << " " << register_names[op0] << ", "<<link;
        } break;
        case BC_CAST: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            u8 from_size = 1 << GET_CONTROL_SIZE(control);
            u8 to_size = 1 << GET_CONTROL_CONVERT_SIZE(control);
            
            std::string out;
            out += std::to_string(from_size);
            if(IS_CONTROL_FLOAT(control)) {
                out += "f";
            } else if(IS_CONTROL_SIGNED(control)) {
                out += "s";
            } else {
                out += "u";
            }
            out += "->";
            out += std::to_string(to_size);
            if(IS_CONTROL_CONVERT_FLOAT(control)) {
                out += "f";
            } else if(IS_CONTROL_CONVERT_SIGNED(control)) {
                out += "s";
            } else {
                out += "u";
            }
            
            log::out << " " << register_names[op0] << ", "<< register_names[op1] << ", "<< out;
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
        case BC_PRINTC:
        case BC_ROUND: {
            op0 = (BCRegister)instructions[pc++];
            log::out << " " << register_names[op0];
        } break;
        case BC_PRINTS: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            log::out << " " << register_names[op0] << ", " << register_names[op1];
        } break;
        case BC_ASM: {
            u8 inputs = (u8)instructions[pc++];
            u8 outputs = (u8)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            auto& inst = code->asmInstances[imm];
            log::out << " "<< inputs << " "<<outputs<<" " << imm << log::GRAY << " (" << BriefString(inst.file)<<":"<<inst.lineStart<<")" << log::NO_COLOR;
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
            control = (InstructionControl)instructions[pc++];
            imm = *(u32*)&instructions[pc];
            pc+=4;

            log::out << " " << register_names[op0] << ", "<< register_names[op1] << ", " << log::GREEN << control << ", "<<imm;
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