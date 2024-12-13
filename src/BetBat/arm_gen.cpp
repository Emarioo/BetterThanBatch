#include "BetBat/arm_gen.h"
#include "BetBat/Compiler.h"

#include "BetBat/arm_defs.h"
#include "BetBat/Reformatter.h"

/*
 https://armconverter.com/?disasm

 @TODO: We may need to add .ARM.attributes section in ELF to get
    correct disassembly from objdump.
    https://github.com/ARM-software/abi-aa/blob/main/addenda32/addenda32.rst

Relocations
https://refspecs.linuxbase.org/elf/ARMELFA08.pdf
Reloc on arm
https://stackoverflow.com/questions/64838776/understanding-arm-relocation-example-str-x0-tmp-lo12zbi-paddr

@TODO: Can we use THUMB instructions?
*/

bool GenerateARM(Compiler* compiler, TinyBytecode* tinycode) {
    using namespace engone;
    TRACE_FUNC()
    
    ZoneScopedC(tracy::Color::SkyBlue1);

    _VLOG(log::out << log::BLUE << "ARM Generator:\n";)


    // make sure dependencies have been fixed first
    bool yes = tinycode->applyRelocations(compiler->bytecode);
    if (!yes) {
        log::out << "Incomplete call relocation\n";
        return false;
    }

    ARMBuilder builder{};
    builder.init(compiler, tinycode);

    yes = builder.generate();
    return yes;
}

bool ARMBuilder::generate() {
    using namespace engone;
    
    TRACE_FUNC()

    CALLBACK_ON_ASSERT(
        tinycode->print(0,-1, code);
    )
    
    // Useful when debugging
    // int arr[]{
    //     0x04b02de5, 0x00b08de2, 0x0cd04de2, 0x1730a0e3,
    //     0x08300be5, 0x08301be5, 0x0330e0e1, 0x0c300be5,
    //     0x0000a0e1, 0x00d08be2, 0x04b09de4, 0x1eff2fe1,
    // };
    // for(int i=0;i<sizeof(arr)/sizeof(*arr);i++) {
    //     arr[i] = (arr[i] << 24) | (arr[i] >> 24) | ((arr[i]&0xFF00) << 8) | ((arr[i] & 0xFF0000) >> 8);
    // }
    // emit_bytes((u8*)arr, sizeof(arr));
    // return true;
    
    bool failed = false;
    for(auto ind : tinycode->required_asm_instances) {
        auto& inst = bytecode->asmInstances[ind];
        if(!inst.generated) {
            bool yes = prepare_assembly(inst);
            if(yes) {
               inst.generated = true; 
            } else {
                failed = true;
                // error should have been printed
            }
        }
    }
    if(failed)
        return false;
    
    bc_to_machine_translation.resize(tinycode->size());
    
    REGISTER_SIZE = compiler->arch.REGISTER_SIZE;
    FRAME_SIZE = compiler->arch.FRAME_SIZE;
    
    auto& instructions = tinycode->instructionSegment;
    
     // TODO: What about WinMain?
    bool is_entry_point = tinycode->name == compiler->entry_point; // TODO: temporary, we should let the user specify entry point, the whole compiler assumes "main" as entry point...

    const ARMRegister arm_callee_saved_regs[]{
        ARM_REG_R4,
        ARM_REG_R5,
        ARM_REG_R6,
        ARM_REG_R7,
        ARM_REG_R8,
        ARM_REG_R9,
        ARM_REG_R10,
    };
    const ARMRegister* callee_saved_regs = nullptr;

    if(compiler->force_default_entry_point) {
        is_entry_point = false;
    }

    bool is_blank = false;
    if(tinycode->debugFunction->funcAst) {
        is_blank = tinycode->debugFunction->funcAst->blank_body; // TODO: We depend on debugFunction, change this
    }

    if(!is_blank) {
        // BAD TEXT
        // if(!is_entry_point) {
        // entry point on unix systems don't need BP restored
        // the stack is also already 16-byte aligned.
        // } else {
        //     // still need to push link register
        //     emit_push(ARM_REG_LR);
        // }
        // GOOD TEXT: We always push fp,lr to get right alignment?
        emit_push_fp_lr();
        emit_mov(ARM_REG_FP, ARM_REG_SP);
    }
    
    int callee_saved_regs_len = 0;
    
    bool enable_callee_saved_registers = false;
    // enable_callee_saved_registers = true;
    callee_saved_regs = arm_callee_saved_regs;
    callee_saved_regs_len = sizeof(arm_callee_saved_regs)/sizeof(*arm_callee_saved_regs);

    if(is_blank || (is_entry_point && compiler->options->target != TARGET_WINDOWS_x64)) {
        // entry point on unix systems don't need non-volatile registers restored
        enable_callee_saved_registers = false;
    }
    
    callee_saved_space = 0;
    if(enable_callee_saved_registers) {
        // TODO: We can use one instruction to push all registers we want
        // for(int i=0;i<callee_saved_regs_len;i++) {
        //     emit_push(callee_saved_regs[i]);
        //     callee_saved_space += REGISTER_SIZE;
        // }
        emit_push_reglist(callee_saved_regs, callee_saved_regs_len);
        callee_saved_space += REGISTER_SIZE * callee_saved_regs_len;
    }
    auto pop_callee_saved_regs=[&]() {
        if(enable_callee_saved_registers) {
            // for(int i=callee_saved_regs_len-1;i>=0;i--) {
            //     emit_pop(callee_saved_regs[i]);
            // }
            emit_pop_reglist(callee_saved_regs, callee_saved_regs_len);
        }
    };
    
    virtual_stack_pointer -= callee_saved_space; // if callee saved space is misaligned then we need to consider that when 16-byte alignment is required. (BC_ALLOC_ARGS)
    
    auto push_alignment = [&]() {
        int misalignment = virtual_stack_pointer % FRAME_SIZE;
        misalignments.add(misalignment);
        int imm = FRAME_SIZE - misalignment;

        if(imm != 0) {
            emit_sub_imm(ARM_REG_SP, ARM_REG_SP, imm);
        }
        virtual_stack_pointer -= imm;
    };
    auto pop_alignment = [&]() {
        int misalignment = misalignments.last();
        misalignments.pop();
        int imm = FRAME_SIZE - misalignment;

        if(imm != 0) {
            emit_add_imm(ARM_REG_SP, ARM_REG_SP, imm);
        }
        virtual_stack_pointer += imm;
    };
    
    // #define DEBUG_ARM_GEN
    
    DynamicArray<Arg> accessed_params;
    int args_offset = 0;
    
    i64 pc = 0;
    while(true) {
        if(!(pc < instructions.size()))
            break;
        i64 prev_pc = pc;
        InstructionOpcode opcode = (InstructionOpcode)instructions[pc];
        pc++;
        
        InstBase* base = (InstBase*)&instructions[prev_pc];
        pc += instruction_contents[opcode].size - 1; // -1 because we incremented pc earlier
        
        if(opcode == BC_GET_PARAM) {
            auto inst = (InstBase_op1_ctrl_imm16*)base;
            int param_index = inst->imm16 / REGISTER_SIZE;
            if(param_index >= accessed_params.size()) {
                accessed_params.resize(param_index+1);
            }
            // log::out << "param "<<param_index<<" " << base->control<<"\n";
            accessed_params[param_index].control = inst->control;
        } else if(opcode == BC_PTR_TO_PARAMS) {
            // TODO: Dude, sometimes I just am a cat playing around with stupid things. accessed_params is A TERRIBLE IDEA and should not have been created EVER. It relies on the user using the parameters in order to know what the parameters are, their size and if they are float, signed or unsigned. What if the user doesn't use all passed arguments? What if we add new instructions that touch parameters without modifying accessed_params. PLEASE FOR THE LOVE OF THE CAT GOD FIX THIS GARBAGE.
            auto inst = (InstBase_op1_imm16*)base;
            int param_index = inst->imm16 / REGISTER_SIZE;
            if(param_index >= accessed_params.size()) {
                accessed_params.resize(param_index+1);
            }
            // log::out << "param "<<param_index<<" " << base->control<<"\n";
            if(REGISTER_SIZE == 4)
                // we don't know the size, if it's float, if it's unsigned so we just assume unsigned 64-bit type.
                accessed_params[param_index].control = CONTROL_32B;
            else
                accessed_params[param_index].control = CONTROL_64B;
        }
    }
    // #define DECL_SIZED_PARAM(CONTROL) InstructionControl control = (InstructionControl)(((CONTROL) & ~CONTROL_MASK_SIZE) | CONTROL_64B);
    if(tinycode->call_convention == UNIXCALL) {
        args_offset = callee_saved_space;
        
        // If the function only accesses arguments through inline assembly then the user must save the registers manually.
        // unless we provide a special get_arg instruction in the inline assembly?
        if(is_blank && accessed_params.size()) {
            log::out << log::RED << "ERROR in " << tinycode->name << log::NO_COLOR<< ": the function accesses parameters which have not been setup due to @blank!\n";
            log::out << "  Don't use @blank or limit yourself to inline assembly.\n";
            compiler->compile_stats.errors++; // nocheckin, TODO: call some function instead
        }
        if (is_entry_point) {
            // entry point has it's arguments put on the stack, not in rdi, rsi...
            int count = accessed_params.size();
            if(count != 0) {
                // TODO: Print where entry point is defined.
                // TODO: What's the calling convention for entry point on Unix on ARM computer.
                log::out << log::RED << "Entry point should have zero arguments when targeting ARM (we don't support any because what would the calling conventin be?)\n";
                // Assert(("entry point shouldn't have more than 3 args",count <= 3));
                // Assert(("entry point shouldn't have more than 3 args",count <= 3));
                // int num = count-1;
                // emit_sub_imm(ARM_REG_SP, ARM_REG_SP, num*REGISTER_SIZE);
                // callee_saved_space += REGISTER_SIZE * num;
                // virtual_stack_pointer -= REGISTER_SIZE * num;
            }
            // ARMRegister tmp_reg = ARM_REG_R4;
            // if(accessed_params.size() > 0) {
            //     auto& param = accessed_params[0];
            //     param.offset_from_rbp = 0;
            // }
            // if(accessed_params.size() > 1) {
            //     auto& param = accessed_params[1];
            //     // DECL_SIZED_PARAM(param.control)
            //     param.offset_from_rbp = -REGISTER_SIZE;
                
            //     emit_mov_reg_reg(tmp_reg, X64_REG_BP);
            //     emit_add_imm32(tmp_reg, REGISTER_SIZE);
            //     emit_mov_mem_reg(X64_REG_BP,tmp_reg,control,param.offset_from_rbp);
            // }
            // if(accessed_params.size() > 2) {
            //     auto& param = accessed_params[2];
            //     DECL_SIZED_PARAM(param.control)
            //     param.offset_from_rbp = -FRAME_SIZE;
            //     // emit_mov_reg_reg(tmp_reg, X64_REG_BP); reuse rax from second param
            //     emit_add_imm32(tmp_reg, 8); // +8 (null of argv)

            //     X64Register extra_reg = X64_REG_D;
            //     disable_modrm_asserts = true;
            //     emit_mov_reg_mem(extra_reg,X64_REG_BP,CONTROL_32B,0); // argc
            //     disable_modrm_asserts = false;

            //     emit1(PREFIX_REXW);
            //     emit1(OPCODE_SHL_RM_IMM8_SLASH_4);
            //     emit_modrm_slash(MODE_REG, 4, extra_reg);
            //     emit1((u8)3);

            //     emit1(PREFIX_REXW);
            //     emit1(OPCODE_ADD_REG_RM);
            //     emit_modrm(MODE_REG, tmp_reg, extra_reg);

            //     emit_mov_mem_reg(X64_REG_BP,tmp_reg,control,param.offset_from_rbp);
            // }
        } else {
            int count = accessed_params.size();
            if(count != 0) {
                emit_sub_imm(ARM_REG_SP, ARM_REG_SP, count * REGISTER_SIZE);
                callee_saved_space += count * REGISTER_SIZE;
                virtual_stack_pointer -= REGISTER_SIZE * count;
            }
            int normal_count = 0;
            int float_count = 0;
            int stacked_count = 0;
            for(int i=0;i<accessed_params.size();i++) {
                auto& param = accessed_params[i];
                // DECL_SIZED_PARAM(param.control)

                int off = -args_offset - (1+i) * REGISTER_SIZE;
                param.offset_from_rbp = off;

                ARMRegister reg_args = ARM_REG_FP;
                ARMRegister reg = ARM_REG_INVALID;
                // if(IS_CONTROL_FLOAT(control)) {
                //     if(float_count < 8) {
                //         // NOTE: Type checker provides a better error, this assert is here as a reminder to fix this code.
                //         Assert(("x64 generator can't handle more than 4 floats",float_count < 4));
                //         reg = unixcall_float_regs[float_count];
                //         emit_mov_mem_reg(reg_args,reg,control,param.offset_from_rbp);
                //     } else {
                //         Assert(("x64 generator can't handle more than 8 floats",false));
                //         reg = X64_REG_XMM7; // is it safe to use xmm7 register?
                //         int src_off = FRAME_SIZE + stacked_count * 8;
                //         emit_mov_reg_mem(reg,reg_args,control,src_off);
                //         emit_mov_mem_reg(reg_args,reg,control,param.offset_from_rbp);
                //         stacked_count++;
                //     }
                //     float_count++;
                // } else {
                    if(normal_count < 4) {
                        reg = (ARMRegister)(ARM_REG_R0 + normal_count);
                        // reg = unixcall_normal_regs[normal_count];
                        emit_str(reg, ARM_REG_FP, param.offset_from_rbp);
                        // emit_mov_mem_reg(reg_args,reg,control,param.offset_from_rbp);
                    } else {
                        reg = ARM_REG_R0;
                        int src_off = FRAME_SIZE + stacked_count * REGISTER_SIZE;
                        emit_ldr(reg, ARM_REG_FP, src_off);
                        emit_str(reg, ARM_REG_FP, param.offset_from_rbp);
                        // emit_mov_reg_mem(reg,reg_args,control,src_off);
                        // emit_mov_mem_reg(reg_args,reg,control,param.offset_from_rbp);
                        stacked_count++;
                    }
                    normal_count++;
                // }
            }
        }
    } else Assert(false);

    struct DataptrReloc {
        int pc_offset; // arm code offset (not bytecode)
        int data_offset;
    };
    DynamicArray<DataptrReloc> dataptr_relocs{};

    struct CodeptrReloc {
        int pc_offset;
        // int bc_imm_index;
        int tinycode_index;
    };
    DynamicArray<CodeptrReloc> codeptr_relocs{};
    
    InstBase* last_inst_call = nullptr;
    
    BytecodePrintCache print_cache{};
    bool logging = true;
    bool running = true;
    // running = false;
    pc = 0;
    while(running) {
        if(!(pc < instructions.size()))
            break;
        i64 prev_pc = pc;
        map_translation(prev_pc, code_size());
        InstructionOpcode opcode = (InstructionOpcode)instructions[pc];
        pc++;
        
        InstBase* base = (InstBase*)&instructions[prev_pc];
        pc += instruction_contents[opcode].size - 1; // -1 because we incremented pc earlier
        
        #ifdef DEBUG_ARM_GEN
        if(logging) {
            tinycode->print(prev_pc, prev_pc + 1, bytecode, nullptr, false, &print_cache);
            log::out << "\n";
        }
        #endif

        for(int i=0;i<sp_moments.size();i++) {
            auto& moment = sp_moments[i];
            // log::out << "check " << moment.pop_at_bc_index<<"\n";
            if(prev_pc == moment.pop_at_bc_index) {
                pop_stack_moment(i);
                i--;
                // break; do not break, there may be more moment to pop on the at the same bc index
            }
        }
        
        switch(opcode) {
            case BC_HALT: {
                Assert(("HALT not implemented",false));
            } break;
            case BC_NOP: {
                // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/NOP
                emit4((u32)0b11110011101011111000000000000000);
            } break;
            case BC_MOV_RR: {
                auto inst = (InstBase_op2*)base;
                
                ARMRegister reg_op = find_register(inst->op1);
                ARMRegister reg_dst = reg_op;
                if (inst->op0 != inst->op1)
                    reg_dst = alloc_register(inst->op0);
                emit_mov(reg_dst, reg_op);
            } break;
            case BC_MOV_RM:
            case BC_MOV_RM_DISP16: {
                auto inst = (InstBase_op2_ctrl_imm16*)base;
                
                i16 imm = 0;
                if(opcode == BC_MOV_RM_DISP16)
                    imm = inst->imm16;
                ARMRegister reg_op = find_register(inst->op1);
                ARMRegister reg_dst = reg_op;
                if (inst->op0 != inst->op1)
                    reg_dst = alloc_register(inst->op0);
                int size = GET_CONTROL_SIZE(inst->control);
                switch(size){
                    case CONTROL_8B:  emit_ldrb(reg_dst, reg_op, imm); break;
                    case CONTROL_16B: emit_ldrh(reg_dst, reg_op, imm); break;
                    case CONTROL_32B: emit_ldr (reg_dst, reg_op, imm); break;
                    default: Assert(size != CONTROL_64B);
                }
            } break;
            case BC_MOV_MR:
            case BC_MOV_MR_DISP16: {
                auto inst = (InstBase_op2_ctrl_imm16*)base;
                
                i16 imm = 0;
                if(opcode == BC_MOV_MR_DISP16)
                    imm = inst->imm16;
                ARMRegister reg_dst = find_register(inst->op0);
                ARMRegister reg_op = find_register(inst->op1);
                int size = GET_CONTROL_SIZE(inst->control);
                switch(size){
                    // NOTE: first arg 'rt' is register, second arg 'rn' is memory address
                    case CONTROL_8B:  emit_strb(reg_op, reg_dst, imm); break;
                    case CONTROL_16B: emit_strh(reg_op, reg_dst, imm); break;
                    case CONTROL_32B: emit_str (reg_op, reg_dst, imm); break;
                    default: Assert(size != CONTROL_64B);
                }
            } break;
            case BC_PUSH: {
                auto inst = (InstBase_op1*)base;
                
                ARMRegister reg_op = find_register(inst->op0);
                emit_push(reg_op);
                virtual_stack_pointer -= REGISTER_SIZE;
            } break;
            case BC_POP: {
                auto inst = (InstBase_op1*)base;
                
                ARMRegister reg_dst = alloc_register(inst->op0);
                emit_pop(reg_dst);
                virtual_stack_pointer += REGISTER_SIZE;
            } break;
            case BC_LI32: {
                auto inst = (InstBase_op1_imm32*)base;
                
                ARMRegister reg_dst = alloc_register(inst->op0);
                int imm = inst->imm32;
                // imm = flip_bits(imm);
                emit_movw(reg_dst, imm & 0xFFFF);
                if((imm >> 16) != 0)
                    emit_movt(reg_dst, imm >> 16);
            } break;
            case BC_LI64: {
                Assert(("BC_LI64 not allowed in ARM gen",false));
            } break;
            case BC_INCR: {
                auto inst = (InstBase_op1_imm16*)base;
                
                ARMRegister reg_dst = find_register(inst->op0);
                if(inst->imm16 < 0) {
                    emit_sub_imm(reg_dst, reg_dst, -inst->imm16);
                } else {
                    emit_add_imm(reg_dst, reg_dst, inst->imm16);
                }
            } break;
            case BC_ALLOC_ARGS:
            case BC_ALLOC_LOCAL: {
                auto inst = (InstBase_op1_imm16*)base;
                int imm = inst->imm16;
                
                Assert(inst->op0 == BC_REG_INVALID);
                
                if(opcode == BC_ALLOC_ARGS) {
                    int misalignment = (virtual_stack_pointer + imm) % FRAME_SIZE;
                    misalignments.add(misalignment);
                    if(misalignment != 0) {
                        imm += FRAME_SIZE - misalignment;
                    }
                }
                
                if(imm != 0) {
                    ARMRegister reg_dst = ARM_REG_SP;
                    emit_sub_imm(reg_dst, reg_dst, imm);
                }
                virtual_stack_pointer -= imm;
                push_offsets.add(0); // needed for SET_ARG
            } break;
            case BC_FREE_ARGS:
            case BC_FREE_LOCAL: {
                auto inst = (InstBase_imm16*)base;
                int imm = inst->imm16;
                
                if(opcode == BC_FREE_ARGS) {
                    int misalignment = misalignments.last();
                    misalignments.pop();
                    if(misalignment != 0) {
                        imm += FRAME_SIZE - misalignment;
                    }
                }
                
                if(imm != 0) {
                    ARMRegister reg_dst = ARM_REG_SP;
                    emit_add_imm(reg_dst, reg_dst, imm);
                }
                
                ret_offset -= imm;
                virtual_stack_pointer += imm;
                push_offsets.pop(); // needed for SET_ARG
            } break;
            case BC_SET_ARG: {
                auto inst = (InstBase_op1_ctrl_imm16*)base;

                ARMRegister reg_op = find_register(inst->op0);

                // this code is required with stdcall or unixcall
                // betcall ignores this
                int arg_index = inst->imm16 / REGISTER_SIZE;
                if(arg_index >= recent_set_args.size()) {
                    recent_set_args.resize(arg_index + 1);
                }
                recent_set_args[arg_index].control = inst->control;

                Assert(push_offsets.size()); // bytecode is missing ALLOC_ARGS if this fires
                
                int off = inst->imm16 + push_offsets.last();
                // Assert(GET_CONTROL_SIZE(inst->control) == CONTROL_32B);
                // emit_str(reg_op, ARM_REG_SP, off);
                
                int size = GET_CONTROL_SIZE(inst->control);
                switch(size){
                    case CONTROL_8B:  emit_strb(reg_op, ARM_REG_SP, off); break;
                    case CONTROL_16B: emit_strh(reg_op, ARM_REG_SP, off); break;
                    case CONTROL_32B: emit_str (reg_op, ARM_REG_SP, off); break;
                    default: Assert(size != CONTROL_64B);
                }
            } break;
            case BC_GET_PARAM: {
                auto inst = (InstBase_op1_ctrl_imm16*)base;
                
                ARMRegister reg_op = find_register(inst->op0);

                int off = inst->imm16 + FRAME_SIZE;
                if(tinycode->call_convention == UNIXCALL) {
                    int param_index = inst->imm16 / REGISTER_SIZE;
                    Assert(param_index < accessed_params.size());
                    off = accessed_params[param_index].offset_from_rbp;
                }
                int size = GET_CONTROL_SIZE(inst->control);
                switch(size){
                    case CONTROL_8B:  emit_ldrb(reg_op, ARM_REG_FP, off); break;
                    case CONTROL_16B: emit_ldrh(reg_op, ARM_REG_FP, off); break;
                    case CONTROL_32B: emit_ldr (reg_op, ARM_REG_FP, off); break;
                    default: Assert(size != CONTROL_64B);
                }
            } break;
            case BC_GET_VAL: {
                auto inst = (InstBase_op1_ctrl_imm16*)base;
                auto call_base = (InstBase_link_call_imm32*)last_inst_call;
                auto call_base_r = (InstBase_op1_link_call*)last_inst_call;
                CallConvention conv = call_base->call;
                if(last_inst_call->opcode == BC_CALL_REG)
                    conv = call_base_r ->call;
                switch(conv) {
                    // case BETCALL: {
                    //     FIX_PRE_OUT_OPERAND(0)

                    //     X64Register reg_params = X64_REG_SP;
                    //     int off = base->imm16 - FRAME_SIZE + ret_offset;
                    //     emit_mov_reg_mem(reg0->reg, reg_params, base->control, off);
                    
                    //     if(IS_CONTROL_SIGNED(base->control)) {
                    //         emit_movsx(reg0->reg, reg0->reg, base->control);
                    //     } else if(!IS_CONTROL_FLOAT(base->control)) {
                    //         emit_movzx(reg0->reg, reg0->reg, base->control);
                    //     }
                        
                    //     FIX_POST_OUT_OPERAND(0)
                    // } break;
                    // case STDCALL:
                    case UNIXCALL: {
                        // TODO: We hope that an instruction after a call didn't
                        //   overwrite the return value in RAX. We assert if it did.
                        //   Should we reserve RAX after a call until we reach BC_GET_VAL
                        //   and use the return value? No because some function do not
                        //   anything so we would have wasted rax. Then what do we do?

                        ARMRegister reg_dst = alloc_register(inst->op0);
                        
                        emit_mov(reg_dst, ARM_REG_R0);

                        // auto reg0 = get_artifical_reg(n->reg0);
                        // if(reg0->floaty) {
                        //     reg0->reg = alloc_register(n->reg0, X64_REG_XMM0, true); 
                        //     Assert(reg0->reg != X64_REG_INVALID);
                        // } else {
                        //     // X64_REG_A is used by mul and rdtsc, we can't reserve it
                        //     // (well, we could but then we would need to temporarily push/pop rax before using it, instead we move A to a new register)
                        //     FIX_PRE_OUT_OPERAND(0)
                            
                        //     if(IS_CONTROL_SIGNED(base->control)) {
                        //         emit_movsx(reg0->reg, X64_REG_A, base->control);
                        //     } else if(!IS_CONTROL_FLOAT(base->control)) {
                        //         emit_movzx(reg0->reg, X64_REG_A, base->control);
                        //     } else Assert(false);
                            
                        //     Assert(reg0->reg != X64_REG_INVALID);
                        // }
                    } break;
                    default: Assert(false);
                }
            } break;
            case BC_SET_RET: {
                auto inst = (InstBase_op1_ctrl_imm16*)base;
                switch(tinycode->call_convention) {
                    // case BETCALL: {
                        
                    //     X64Register reg_rets = X64_REG_BP;
                    //     int off = base->imm16 - callee_saved_space;

                    //     emit_mov_mem_reg(reg_rets, reg0->reg, base->control, off);
                        
                    // } break;
                    // case STDCALL:
                    case UNIXCALL: {

                        ARMRegister reg_op = find_register(inst->op0);

                        Assert(inst->imm16 == -REGISTER_SIZE);
                        // Assert(reg0->floaty == (0 != IS_CONTROL_FLOAT(base->control)));
                        
                        emit_mov(ARM_REG_R0, reg_op);
                        // if(reg0->floaty) {
                        //     if(reg0->reg != X64_REG_XMM0)
                        //         emit_mov_reg_reg(X64_REG_XMM0, reg0->reg, 1 << GET_CONTROL_SIZE(base->control));
                        // } else {
                        //     if(reg0->reg != X64_REG_A)
                        //         emit_mov_reg_reg(X64_REG_A, reg0->reg);
                        // }
                    } break;
                    default: Assert(false);
                }
            } break;
            case BC_PTR_TO_LOCALS: {
                auto inst = (InstBase_op1_imm16*)base;
                
                int off = inst->imm16 - callee_saved_space;
                ARMRegister reg_dst = alloc_register(inst->op0);
                if(off < 0) {
                    emit_sub_imm(reg_dst, ARM_REG_FP, -off);
                } else {
                    emit_add_imm(reg_dst, ARM_REG_FP, off);
                }
            } break;
            case BC_PTR_TO_PARAMS: {
                auto inst = (InstBase_op1_imm16*)base;

                int off = inst->imm16 + FRAME_SIZE;
                if(tinycode->call_convention == UNIXCALL) {
                    // In STDCALL, caller makes space for args and they are put there.
                    // Sys V ABI convention does not so we make space for them in this call frame.
                    // The offset is therefore not 'imm+FRAME_SIZE'.
                    // if(is_entry_point) {
                    //     off -= 16; // args on stack, no return address or previous rbp
                    // } else {
                        // TODO: Is the hardcoded 4 okay?
                        if(inst->imm16 < 4 * REGISTER_SIZE) {
                            off = -args_offset - inst->imm16 - REGISTER_SIZE;
                        }
                    // }
                }
                
                ARMRegister reg_dst = alloc_register(inst->op0);
                if(off < 0) {
                    emit_sub_imm(reg_dst, ARM_REG_FP, -off);
                } else {
                    emit_add_imm(reg_dst, ARM_REG_FP, off);
                }
            } break;
            case BC_CALL:
            case BC_CALL_REG: {
                auto inst = (InstBase_link_call_imm32*)base;
                auto base_r = (InstBase_op1_link_call*)base;
                
                last_inst_call = base;
                
                CallConvention conv = inst->call;
                LinkConvention link = inst->link;
                ARMRegister ptr_reg = ARM_REG_INVALID;
                if(opcode == BC_CALL_REG) {
                    ptr_reg = ARM_REG_R4;
                    ARMRegister reg_op = find_register(base_r->op0);
                    if(ptr_reg != reg_op)
                        emit_mov(ptr_reg, reg_op);
                    conv = base_r->call;
                    link = base_r->link;
                }
                Assert(link == LinkConvention::NONE);

                switch(conv) {
                    // case BETCALL: break;
                    // case STDCALL: {
                    //     // recent_set_args
                    //     for(int i=0;i<recent_set_args.size() && i < 4;i++) {
                    //         auto& arg = recent_set_args[i];
                    //         auto control = arg.control;

                    //         int off = i * 8;
                    //         X64Register reg_args = X64_REG_SP;
                    //         X64Register reg = stdcall_normal_regs[i];
                    //         if(IS_CONTROL_FLOAT(control))
                    //             reg = stdcall_float_regs[i];
                    //         else {
                    //             emit_prefix(0, reg, reg);
                    //             emit1(OPCODE_XOR_REG_RM);
                    //             emit_modrm(MODE_REG, CLAMP_EXT_REG(reg), CLAMP_EXT_REG(reg));
                    //         }
                    //         emit_mov_reg_mem(reg,reg_args,control,off);
                    //     }
                    // } break;
                    case UNIXCALL: {
                        // if(recent_set_args.size() > 6) {
                        //     Assert(("x64 gen can't handle Sys V ABI with more than 6 arguments.", false));
                        // }
                        int normal_count = 0;
                        int float_count = 0;
                        int stacked_count = 0;
                        for(int i=0;i<recent_set_args.size();i++) {
                            auto& arg = recent_set_args[i];
                            auto control = arg.control;
                            // Assert(GET_CONTROL_SIZE(control) == CONTROL_32B);

                            int off = i * REGISTER_SIZE;
                            ARMRegister reg_args = ARM_REG_SP;
                            // ARMRegister reg_args = ARM_REG_FP;
                            // log::out << "control " <<(InstructionControl)control << "\n";
                            
                            // if(IS_CONTROL_FLOAT(control)) {
                            //     if(float_count < 8) {
                            //         // NOTE: We provide a better error message in the type checker.
                            //         Assert(("x64 generator doesn't support more than 4 float arguments",float_count < 4));
                                    
                            //         X64Register reg = unixcall_float_regs[float_count];
                            //         emit_mov_reg_mem(reg,reg_args,control,off);
                            //     } else {
                            //         X64Register tmp = X64_REG_A;
                            //         emit_mov_reg_mem(X64_REG_A,reg_args,control, off);
                            //         int dst_off = stacked_count * 8;
                            //         emit_mov_mem_reg(reg_args,X64_REG_A,control, dst_off);
                            //         stacked_count++;
                            //     }
                            //     float_count++;
                            // } else {
                            if(normal_count < 4) {
                                ARMRegister reg = (ARMRegister)(ARM_REG_R0 + normal_count);
                                // X64Register reg = unixcall_normal_regs[normal_count];
                                emit_ldr(reg, reg_args, off);
                            } else {
                                ARMRegister tmp = ARM_REG_R4;
                                int dst_off = stacked_count * REGISTER_SIZE;
                                
                                emit_ldr(tmp, reg_args, off);
                                emit_str(tmp, reg_args, dst_off);
                                stacked_count++;
                            }
                            normal_count++;
                            // }
                        }
                    } break;
                    default: Assert(false);
                }
                
                recent_set_args.resize(0);
                ret_offset = 0;
                if(opcode == BC_CALL_REG) {
                    emit_mov(ARM_REG_LR, ARM_REG_PC);
                    emit_bx(ptr_reg);
                    break;
                }


                int offset = code_size();
                emit_bl(0);
                // necessary when adding internal func relocations
                // +3 = 1 (opcode) + 1 (link) + 1 (convention)
                map_strict_translation(prev_pc + 3, offset);
            } break;
            case BC_RET: {
                 // NOTE: We should not modify sp_moments / virtual_stack_pointer because BC_RET_ may exist in a conditional block. This is fine since we only need sp_moment if we have instructions that require alignment, if we return then there are no more instructions.
                
                int total = 0;
                if(callee_saved_space - args_offset > 0) {
                    total += callee_saved_space - args_offset;
                }
                if(total != 0)
                    emit_add_imm(ARM_REG_SP, ARM_REG_SP, (total));

                if(is_blank) {
                    // user handles the rest
                    emit_pop(ARM_REG_LR);
                    emit_bx(ARM_REG_LR);
                } else if(is_entry_point) {
                    Assert(tinycode->call_convention == UNIXCALL);
                    // emit1(OPCODE_MOV_REG_RM);
                    // emit_modrm(MODE_REG, X64_REG_DI, X64_REG_A);

                    // // we have to manually exit when linking with -nostdlib
                    // emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                    // emit_modrm_slash(MODE_REG, 0, X64_REG_A);
                    // emit4((u32)SYS_exit);
                    // emit2(OPCODE_2_SYSCALL);
                    // emit_pop(ARM_REG_LR);
                    // the startup assembly script starts with an 8-byte aligned stack
                    // so we might as well push and pop fp and lr to keep that
                    // alignment
                    emit_pop_fp_lr();
                    emit_bx(ARM_REG_LR);
                } else {
                    pop_callee_saved_regs();
                    emit_pop_fp_lr();
                    emit_bx(ARM_REG_LR);
                }
            } break;
            case BC_JNZ: {
                auto inst = (InstBase_op1_imm32*)base;
                ARMRegister reg = find_register(inst->op0);
                emit_cmp_imm(reg, 0);
                
                int imm_offset = code_size();
                emit_b(0, ARM_COND_NE);
                
                const u8 BYTE_OF_BC_JNZ = 1 + 1 + 4; // TODO: DON'T HARDCODE VALUES!
                int jmp_bc_addr = prev_pc + BYTE_OF_BC_JNZ + inst->imm32;
                addRelocation32(imm_offset, imm_offset, jmp_bc_addr);

                push_stack_moment(virtual_stack_pointer, jmp_bc_addr);
            } break;
            case BC_JZ: {
                auto inst = (InstBase_op1_imm32*)base;
                ARMRegister reg = find_register(inst->op0);
                emit_cmp_imm(reg, 0);
                int imm_offset = code_size();
                emit_b(0, ARM_COND_EQ);
                
                const u8 BYTE_OF_BC_JZ = 1 + 1 + 4; // TODO: DON'T HARDCODE VALUES!
                int jmp_bc_addr = prev_pc + BYTE_OF_BC_JZ + inst->imm32;
                addRelocation32(imm_offset, imm_offset, jmp_bc_addr);
                
                push_stack_moment(virtual_stack_pointer, jmp_bc_addr);
            } break;
            case BC_JMP: {
                auto inst = (InstBase_imm32*)base;
                
                int imm_offset = code_size();
                
                emit_b(0, ARM_COND_AL);
                
                const u8 BYTE_OF_BC_JMP = 1 + 4; // TODO: DON'T HARDCODE VALUES!
                int jmp_bc_addr = prev_pc + BYTE_OF_BC_JMP + inst->imm32;
                addRelocation32(imm_offset, imm_offset, jmp_bc_addr);
                
                // Don't mess with moments if we jump back, aka continue and loop statements
                if (inst->imm32 > 0) {
                    // Only mess with moments if we have an associated conditional jump (and if we have any moments at all)
                    // We determine association if the jmp hops to an instruction after the on the conditional one
                    // hops too.
                    // There may be flaws, what if we have nested jmp in some sketchy configuration?
                    // What about break statements. They don't jump backwards.
                    if (sp_moments.size() > 0) {
                        auto& last = sp_moments.last();
                        if (jmp_bc_addr >= last.pop_at_bc_index) {
                            pop_stack_moment();
                            push_stack_moment(virtual_stack_pointer, jmp_bc_addr);
                        }
                    }
                }
            } break;
            case BC_DATAPTR: {
                auto inst = (InstBase_op1_imm32*)base;
                ARMRegister reg = alloc_register(inst->op0);
                int imm_offset = code_size();
                emit_ldr(reg, ARM_REG_PC, 0);

                DataptrReloc rel{};
                rel.pc_offset = imm_offset;
                rel.data_offset = inst->imm32;
                dataptr_relocs.add(rel);
            } break;
            case BC_EXT_DATAPTR: {
                Assert(false);
            } break;
            case BC_CODEPTR: {
                auto inst = (InstBase_op1_imm32*)base;
                int tinycode_index = inst->imm32 - 1;
                ARMRegister reg = alloc_register(inst->op0);
                i32 imm_offset = code_size();

                emit_ldr(reg, ARM_REG_PC, 0);
                emit_add(reg, reg, ARM_REG_PC);

                CodeptrReloc rel{};
                rel.pc_offset = imm_offset;
                rel.tinycode_index = tinycode_index;
                // rel.bc_imm_index = prev_pc + 2;
                codeptr_relocs.add(rel);
            } break;
            case BC_ADD:
            case BC_SUB:
            case BC_MUL:
            case BC_DIV:
            // comparisons
            case BC_EQ:
            case BC_NEQ:
            case BC_LT:
            case BC_LTE:
            case BC_GT:
            case BC_GTE:
            // logical operations
            case BC_LAND:
            case BC_LOR:
            // bitwise ops
            case BC_BAND:
            case BC_BOR:
            case BC_BXOR:
            case BC_BLSHIFT:
            case BC_BRSHIFT:
            {
                auto inst = (InstBase_op2_ctrl*)base;
                
                Assert(!IS_CONTROL_FLOAT(inst->control));
                
                ARMRegister reg_dst = find_register(inst->op0);
                ARMRegister reg_op = find_register(inst->op1);
                
                switch(opcode) {
                    case BC_ADD: emit_add(reg_dst, reg_dst, reg_op); break;
                    case BC_SUB: emit_add(reg_dst, reg_dst, reg_op); break;
                    case BC_MUL: {
                        ARMRegister reg_temp = alloc_register();
                        if(IS_CONTROL_SIGNED(inst->control)) {
                            emit_smull(reg_dst, reg_temp, reg_dst, reg_op);
                        } else {
                            emit_umull(reg_dst, reg_temp, reg_dst, reg_op);
                        }
                        free_register(reg_temp);
                    } break;
                    case BC_DIV: {
                        if(IS_CONTROL_SIGNED(inst->control)) {
                            emit_sdiv(reg_dst, reg_dst, reg_op);
                        } else {
                            emit_udiv(reg_dst, reg_dst, reg_op);
                        }
                    } break;
                    case BC_EQ: {
                        emit_cmp(reg_dst, reg_op);
                        emit_movw(reg_dst, 1, ARM_COND_EQ);
                        emit_movw(reg_dst, 0, ARM_COND_NE);
                    } break;
                    case BC_NEQ: {
                        emit_cmp(reg_dst, reg_op);
                        emit_movw(reg_dst, 0, ARM_COND_EQ);
                        emit_movw(reg_dst, 1, ARM_COND_NE);
                    } break;
                    case BC_LT: {
                        emit_cmp(reg_dst, reg_op);
                        emit_movw(reg_dst, 1, ARM_COND_LT);
                        emit_movw(reg_dst, 0, ARM_COND_GE);
                    } break;
                    case BC_GT: {
                        emit_cmp(reg_dst, reg_op);
                        emit_movw(reg_dst, 1, ARM_COND_GT);
                        emit_movw(reg_dst, 0, ARM_COND_LE);
                    } break;
                    case BC_LTE: {
                        emit_cmp(reg_dst, reg_op);
                        emit_movw(reg_dst, 1, ARM_COND_LE);
                        emit_movw(reg_dst, 0, ARM_COND_GT);
                    } break;
                    case BC_GTE: {
                        emit_cmp(reg_dst, reg_op);
                        emit_movw(reg_dst, 1, ARM_COND_GE);
                        emit_movw(reg_dst, 0, ARM_COND_LT);
                    } break;
                    case BC_LAND: {
                        // AND = r0 != 0 && r1 != 0
                        emit_cmp_imm(reg_dst, 0);
                        emit_movw(reg_dst, 1, ARM_COND_NE);
                        emit_movw(reg_dst, 0, ARM_COND_EQ);
                        
                        emit_cmp_imm(reg_op, 0);
                        emit_and_imm(reg_dst, reg_op, 1, ARM_COND_EQ);
                        emit_and_imm(reg_dst, reg_op, 0, ARM_COND_NE);
                    } break;
                    case BC_LOR: {
                        emit_orr(reg_dst, reg_dst, reg_op, true);
                        emit_movw(reg_dst, 1, ARM_COND_EQ);
                        emit_movw(reg_dst, 0, ARM_COND_NE);
                    } break;
                    case BC_BAND: {
                        emit_and(reg_dst, reg_dst, reg_op);
                    } break;
                    case BC_BOR: {
                        emit_orr(reg_dst, reg_dst, reg_op);
                    } break;
                    case BC_BXOR: {
                        emit_eor(reg_dst, reg_dst, reg_op);
                    } break;
                    case BC_BLSHIFT: {
                        emit_lsl(reg_dst, reg_dst, reg_op);
                    } break;
                    case BC_BRSHIFT: {
                        emit_lsr(reg_dst, reg_dst, reg_op);
                    } break;
                }
                
                //  TODO: less than, equal, bitwise ops
            } break;
            case BC_BNOT: {
                auto inst = (InstBase_op2_ctrl*)base;
                
                ARMRegister reg_op = find_register(inst->op1);
                ARMRegister reg_dst = reg_op;
                if (inst->op0 != inst->op1) {
                    reg_dst = alloc_register(inst->op0);
                }
                emit_mvn(reg_dst, reg_op);
            } break;
            case BC_MOD: {
                auto inst = (InstBase_op2_ctrl*)base;
                ARMRegister reg_dst = find_register(inst->op0);
                ARMRegister reg_op = find_register(inst->op1);
                
                ARMRegister reg_temp = alloc_register();
                ARMRegister reg_temp2 = alloc_register();
                
                Assert(!IS_CONTROL_FLOAT(inst->control));
                
                /*
                op0 - (op0 / op1) * op1
                mov r0
                mov r1
                div r2 r0 r1
                mul r2 r2 r1
                sub r0 r0 r2
                */
                if(IS_CONTROL_SIGNED(inst->control)) {
                    emit_sdiv(reg_temp, reg_dst, reg_op);
                    emit_smull(reg_temp, reg_temp2, reg_dst, reg_op);
                    emit_sub(reg_dst, reg_dst, reg_temp);
                }
                free_register(reg_temp);
                free_register(reg_temp2);
            } break;
            case BC_CAST: {
               auto inst = (InstBase_op2_ctrl *)base;

                // TODO: Do casts, if we cast to same register then we do nothing
                ARMRegister reg_op = find_register(inst->op1);
                ARMRegister reg_dst = reg_op;
                if (inst->op0 != inst->op1) {
                    reg_dst = alloc_register(inst->op0);
                    emit_mov(reg_dst, reg_op);
                }
            } break;
            case BC_ASM: {
                InstBase_imm32_imm8_2* inst = (InstBase_imm32_imm8_2*)base;
                
                if(push_offsets.size())
                    push_offsets.last() += 8 * (-inst->imm8_0 + inst->imm8_1);
                
                virtual_stack_pointer += (inst->imm8_0 - inst->imm8_1) * 8; // inputs - outputs
                
                Bytecode::ASM& asmInstance = bytecode->asmInstances.get(inst->imm32);
                Assert(asmInstance.generated);
                u32 len = asmInstance.iEnd - asmInstance.iStart;
                if(len != 0) {
                    u8* ptr = bytecode->rawInstructions._ptr + asmInstance.iStart;
                    int pc_start = code_size();
                    emit_bytes(ptr, len);
                    for (int i = 0; i < asmInstance.relocations.size();i++) {
                      auto& it = asmInstance.relocations[i];
                      program->addNamedUndefinedRelocation(it.name, pc_start + it.textOffset, tinycode->index);
                    }
                } else {
                    // TODO: Better error, or handle error somewhere else?
                    log::out << log::RED << "BC_ASM at "<<prev_pc<<" was incomplete\n";
                }
            } break;
            default: {
                log::out << log::RED << "TODO: Implement BC instrtuction in ARM generator: "<< log::PURPLE<< opcode << "\n";
                Assert(false);
            }
        }
    }
    // emit_movw(ARM_REG_R0, 5);
    // emit_movw(ARM_REG_R1, 6);
    // emit_add(ARM_REG_R0, ARM_REG_R0, ARM_REG_R1);
    // emit_bx(ARM_REG_LR);
    
    // NOTE: We should have emitted a ret instruction and any instructions we emit here will never execute.
    
    for(int i=0;i<codeptr_relocs.size();i++) {
        auto& rel = codeptr_relocs[i];

        i32 disp32_offset = code_size();
        emit4((u32)(disp32_offset - rel.pc_offset));
        set_imm12(rel.pc_offset, disp32_offset - rel.pc_offset-8);
        // map_strict_translation(rel.bc_imm_index, disp32_offset);
                
        program->addInternalFuncRelocation(current_funcprog_index, disp32_offset, rel.tinycode_index, true, disp32_offset - rel.pc_offset);
    }

    for(int i=0;i<dataptr_relocs.size();i++) {
        auto& rel = dataptr_relocs[i];

        i32 disp32_offset = code_size();
        emit4((u32)rel.data_offset);
        set_imm12(rel.pc_offset, disp32_offset - rel.pc_offset - 8);
        program->addDataRelocation(rel.data_offset, disp32_offset, current_funcprog_index);
    }

     for(int i=0;i<tinycode->call_relocations.size();i++) {
        auto& r = tinycode->call_relocations[i];
        if(r.funcImpl->astFunction->linkConvention == NATIVE)
            continue;
        int ind = r.funcImpl->tinycode_id - 1;
        program->addInternalFuncRelocation(current_funcprog_index, get_map_translation(r.pc), ind);
        // log::out << r.funcImpl->astFunction->name<<" pc: "<<r.pc<<" codeid: "<<ind<<"\n";
    }
    
    for(int i=0;i<relativeRelocations.size();i++) {
        auto& rel = relativeRelocations[i];
        if(rel.bc_addr == tinycode->size()) {
            set_imm24(rel.imm32_addr, code_size() - rel.inst_addr);
        } else {
            if(get_map_translation(rel.bc_addr) == 0) {
                // The BC instruction we refer to was skipped by the x64 generator.
                // Instead we take the next closest translation.
                // TODO: Optimize. Preferably don't iterate at all.
                //   If we must then check 64-bit integers for 0 at a time instead of 8-bit integers.
                // int closest_translation = 0;
                // for(int i=rel.bc_addr;i<_bc_to_x64_translation.size();i++) {
                //     if(_bc_to_x64_translation[i] != 0) { // check 64-bit values instead
                //         closest_translation = _bc_to_x64_translation[i];
                //         break;
                //     }
                // }
                // if(closest_translation != 0) {
                //     map_translation(rel.bc_addr, closest_translation);
                // } else {
                    log::out << log::RED << "Bug in compiler when translating BC to ARM addresses. This ARM instruction "<<log::GREEN<<NumberToHex(rel.inst_addr,true) <<log::RED<< " wanted to refer to "<<log::GREEN<<rel.bc_addr<<log::RED<<" (BC address) but ther was no mapping of the BC address."<<"\n";
                // }
            }
            
            Assert(rel.bc_addr>=0); // native functions not implemented
            int addr_of_next_inst = rel.imm32_addr + 8;
            i32 value = get_map_translation(rel.bc_addr) - addr_of_next_inst;
            set_imm24(rel.imm32_addr, value/4);
            // BL (immediate) instruction expected a relative offset divided by 4.
            // If you are wondering why the relocation is zero then it is because it
            // is relative and will jump to the next instruction. This isn't necessary so
            // they should be optimised away. Don't do this now since it makes the conversion
            // confusing.
        }
    }
    Assert(bytecode->externalRelocations.size() == 0);
    // TODO: Optimize, store external relocations per tinycode instead
    // bool found = bytecode->externalRelocations.size() == 0;
    // for(int i=0;i<bytecode->externalRelocations.size();i++) {
    //     auto& rel = bytecode->externalRelocations[i];
    //     if(tinycode->index == rel.tinycode_index) {
    //         int off = get_map_translation(rel.pc);
    //         program->addNamedUndefinedRelocation(rel.name, off, rel.tinycode_index, rel.library_path, rel.type == BC_REL_GLOBAL_VAR);
    //         // found = true;
    //         // break;
    //     }
    // }
    
    
    // funcprog->printHex();
    
    Assert(("Size of generated ARM is not divisible by 4",code_size() % 4 == 0));
    
    // TODO: Don't iterate like this
    auto di = bytecode->debugInformation;
    DebugFunction* fun = nullptr;
    for(int i=0;i<di->functions.size();i++) {
        if(di->functions[i]->tinycode == tinycode) {
            fun = di->functions[i];
            break;
        }
    }
    Assert(fun);

    fun->offset_from_bp_to_locals = callee_saved_space;
    fun->args_offset = args_offset;

    for(int i=0;i<fun->lines.size();i++) {
        auto& ln = fun->lines[i];
        
        ln.asm_address = get_map_translation(ln.bc_address);
        if(ln.asm_address == 0) {
            log::out << log::RED << "Bug in compiler when translating BC to x64 addresses. Line "<<log::LIME<<ln.lineNumber<<log::RED<<" refers to BC addr "<<ln.bc_address<<log::RED <<" which wasn't mapped.\n";
        }
        // Assert(ln.asm_address != 0); // very suspicious if it is zero
    }

    DynamicArray<ScopeInfo*> scopes{};

    for(auto& v : fun->localVariables) {
        auto now = di->ast->getScope(v.scopeId);
        now->asm_start = get_map_translation(now->bc_start);
        now->asm_end = get_map_translation(now->bc_end);
    }
    
    return true;
}

void ARMBuilder::set_imm24(int index, int imm) {
    // @TODO: Make sure immediate fits within 24 bits
    funcprog->text[index+0] = imm & 0xFF;
    funcprog->text[index+1] = (imm>>8) & 0xFF;
    funcprog->text[index+2] = (imm>>16) & 0xFF;
}
void ARMBuilder::set_imm12(int index, int imm) {
    // @TODO: Make sure immediate fits within 12 bits
    funcprog->text[index+0] = imm & 0xFF;
    funcprog->text[index+1] = ((imm>>8) & 0xF) | (funcprog->text[index+1] & 0xF0);
}
ARMRegister ARMBuilder::alloc_register(BCRegister reg) {
    if(reg == BC_REG_LOCALS)
        return ARM_REG_FP;
    int max_used = 0;
    int arm_reg = 1;
    for(int i=1;i<ARM_REG_GENERAL_MAX;i++) {
        auto& info = arm_registers[i];
        if(info.used) {
            if(max_used < info.last_used) {
                max_used = info.last_used;
                arm_reg = i;
            }
            continue;
        }
        info.used = true;
        info.bc_reg = reg;
        info.last_used = 0;
        
        auto& bc_info = bc_registers[reg];
        bc_info.arm_reg = (ARMRegister)i;
        return (ARMRegister)i;
    }
    auto& info = arm_registers[arm_reg];
    info.used = true;
    info.bc_reg = reg;
    info.last_used = 0;
    
    auto& bc_info = bc_registers[reg];
    bc_info.arm_reg = (ARMRegister)arm_reg;
    return (ARMRegister)arm_reg;
}
ARMRegister ARMBuilder::find_register(BCRegister reg) {
    if(reg == BC_REG_LOCALS)
        return ARM_REG_FP;
    auto& bc_info = bc_registers[reg];
    Assert(bc_info.arm_reg != ARM_REG_INVALID);
    for(int i=1;i<ARM_REG_GENERAL_MAX;i++) {
        if(i == bc_info.arm_reg) {
            arm_registers[i].last_used = 0;
        } else {
            arm_registers[i].last_used++;
        }
    }
    return bc_info.arm_reg;
}
void ARMBuilder::free_register(ARMRegister reg) {
    if(reg == ARM_REG_FP)
        return;
    auto& info = arm_registers[reg-1];
    Assert(info.used);
    
    if(info.bc_reg != BC_REG_INVALID) {
        auto& bc_info = bc_registers[info.bc_reg];
        bc_info.arm_reg = ARM_REG_INVALID;
    }
    info.used = false;
    info.bc_reg = BC_REG_INVALID;
}

void ARMBuilder::emit_add(ARMRegister rd, ARMRegister rn, ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/ADD--register--ARM-?lang=en
    ARM_CLAMP_dnm()
    int S = 0;
    int cond = ARM_COND_AL;
    int imm5 = 0;
    int type = ARM_SHIFT_TYPE_LSL;
    int inst = BITS(cond, 4, 28) | BITS(0b0000100, 7, 21) | BITS(S, 1, 20)
        | BITS(Rn,4,16) | BITS(Rd,4,12) | BITS(imm5, 5, 7) | BITS(type, 2, 5)
         | BITS(Rm, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_add_imm(ARMRegister rd, ARMRegister rn, int imm, int cond) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/ADD--immediate--ARM-?lang=en
    // imm == 0 could be a bug so we assert
    Assert(imm > 0 && imm <= 0xFFF);
    ARM_CLAMP_dn()
    int S = 0;
    int imm12 = imm;
    int inst = BITS(cond, 4, 28) | BITS(0b0010100, 7, 21) | BITS(S, 1, 20)
        | BITS(Rn,4,16) | BITS(Rd,4,12) | BITS(imm12, 12, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_sub(ARMRegister rd, ARMRegister rn, ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/SUB--register-?lang=en
    ARM_CLAMP_dnm()
    int S = 0;
    int cond = ARM_COND_AL;
    int imm5 = 0;
    int type = ARM_SHIFT_TYPE_LSL;
    int inst = BITS(cond, 4, 28) | BITS(0b0000010, 7, 21) | BITS(S, 1, 20)
        | BITS(Rn,4,16) | BITS(Rd,4,12) | BITS(imm5, 5, 7) | BITS(type, 2, 5)
         | BITS(Rm, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_sub_imm(ARMRegister rd, ARMRegister rn, int imm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/SUB--immediate--ARM-?lang=en
    // imm == 0 could be a bug so we assert
    Assert(imm > 0 && imm <= 0xFFF);
    ARM_CLAMP_dn()
    int S = 0;
    int cond = ARM_COND_AL;
    int imm12 = imm;
    int inst = BITS(cond, 4, 28) | BITS(0b0010010, 7, 21) | BITS(S, 1, 20)
        | BITS(Rn,4,16) | BITS(Rd,4,12) | BITS(imm12, 12, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_smull(ARMRegister rdlo, ARMRegister rdhi, ARMRegister rn, ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/SMULL?lang=en
    ARM_CLAMP_nm()
    Assert(rdlo > 0 && rdlo < ARM_REG_MAX);
    Assert(rdhi > 0 && rdhi < ARM_REG_MAX);
    u8 RdLo = rdlo-1;
    u8 RdHi = rdhi-1;
    int inst = BITS(0b111110111000, 12, 20) | BITS(Rn, 4, 16)
        | BITS(RdLo,4,12) | BITS(RdHi,4, 8) | BITS(Rm, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_umull(ARMRegister rdlo, ARMRegister rdhi, ARMRegister rn, ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/UMULL
    ARM_CLAMP_nm()
    Assert(rdlo > 0 && rdlo < ARM_REG_MAX);
    Assert(rdhi > 0 && rdhi < ARM_REG_MAX);
    u8 RdLo = rdlo-1;
    u8 RdHi = rdhi-1;
    int inst = BITS(0b111110111010, 12, 20) | BITS(Rn, 4, 16)
        | BITS(RdLo,4,12) | BITS(RdHi,4, 8) | BITS(Rm, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_sdiv(ARMRegister rd, ARMRegister rn, ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/SDIV
    ARM_CLAMP_dnm()
    int inst = BITS(0b111110111001, 12, 20) | BITS(Rn, 4, 16)
        | BITS(0xF,4,12) | BITS(Rd,4, 8) | BITS(0xF,4, 4) | BITS(Rm, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_udiv(ARMRegister rd, ARMRegister rn, ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/UDIV
    ARM_CLAMP_dnm()
    int inst = BITS(0b111110111011, 12, 20) | BITS(Rn, 4, 16)
        | BITS(0xF,4,12) | BITS(Rd,4, 8) | BITS(0xF,4, 4) | BITS(Rm, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_and(ARMRegister rd, ARMRegister rn, ARMRegister rm, int cond) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/AND--register-
    ARM_CLAMP_dnm()
    int S = 0;
    int imm5 = 0;
    int type = ARM_SHIFT_TYPE_LSL;
    int inst = BITS(cond, 4, 28) | BITS(0b0000000, 7, 21) | BITS(S,1,20) | BITS(Rn, 4, 16)
        | BITS(Rd,4, 12) | BITS(imm5,5, 7) | BITS(type,2,5) | BITS(Rm, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_and_imm(ARMRegister rd, ARMRegister rn, int imm, int cond) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/AND--register-
    ARM_CLAMP_dn()
    // @TODO: Check range of 12-bit immediate
    int S = 0;
    int imm12 = imm & 0xFFF;
    int inst = BITS(cond, 4, 28) | BITS(0b0010000, 7, 21) | BITS(S,1,20) | BITS(Rn, 4, 16)
        | BITS(Rd,4, 12) | BITS(imm12,12,0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_orr(ARMRegister rd, ARMRegister rn, ARMRegister rm, bool set_flags) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/ORR--register-
    ARM_CLAMP_dnm()
    int cond = ARM_COND_AL;
    int S = set_flags ? 1 : 0;
    int imm5 = 0;
    int type = ARM_SHIFT_TYPE_LSL;
    int inst = BITS(cond, 4, 28) | BITS(0b0001100, 7, 21) | BITS(S,1,20) | BITS(Rn, 4, 16)
        | BITS(Rd,4, 12) | BITS(imm5,5, 7) | BITS(type,2,5) | BITS(Rm, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_eor(ARMRegister rd, ARMRegister rn, ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/EOR--register-
    ARM_CLAMP_dnm()
    int cond = ARM_COND_AL;
    int S = 0;
    int imm5 = 0;
    int type = ARM_SHIFT_TYPE_LSL;
    int inst = BITS(cond, 4, 28) | BITS(0b0000001, 7, 21) | BITS(S,1,20) | BITS(Rn, 4, 16)
        | BITS(Rd,4, 12) | BITS(imm5,5, 7) | BITS(type,2,5) | BITS(Rm, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_lsl(ARMRegister rd, ARMRegister rn, ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/LSL--register-
    ARM_CLAMP_dnm()
    int cond = ARM_COND_AL;
    int S = 0;
    int imm5 = 0;
    int type = ARM_SHIFT_TYPE_LSL;
    int inst = BITS(cond, 4, 28) | BITS(0b0001101, 7, 21) | BITS(S,1,20) | BITS(Rd, 4, 12)
        | BITS(Rm,4, 8) | BITS(0b0001,4,4) | BITS(Rn, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_lsr(ARMRegister rd, ARMRegister rn, ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/LSR--register-
    ARM_CLAMP_dnm()
    int cond = ARM_COND_AL;
    int S = 0;
    int imm5 = 0;
    int type = ARM_SHIFT_TYPE_LSL;
    int inst = BITS(cond, 4, 28) | BITS(0b0001101, 7, 21) | BITS(S,1,20) | BITS(Rd, 4, 12)
        | BITS(Rm,4, 8) | BITS(0b0011,4,4) | BITS(Rn, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_mvn(ARMRegister rd, ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/MVN--register-
    ARM_CLAMP_dm()
    int cond = ARM_COND_AL;
    int S = 0;
    int imm5 = 0;
    int type = ARM_SHIFT_TYPE_LSL;
    int inst = BITS(cond, 4, 28) | BITS(0b0001111, 7, 21) | BITS(S,1,20)
        | BITS(Rd,4, 12) | BITS(imm5,5, 7) | BITS(type,2,5) | BITS(Rm, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_cmp(ARMRegister rn, ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/CMP--register-
    ARM_CLAMP_nm()
    int cond = ARM_COND_AL;
    int imm5 = 0;
    int type = ARM_SHIFT_TYPE_LSL;
    int inst = BITS(cond, 4, 28) | BITS(0b00010101, 8, 20) | BITS(Rn, 4, 16)
        | BITS(imm5,5,7) | BITS(type, 2, 5) | BITS(Rm, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_cmp_imm(ARMRegister rn, int imm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/CMP--immediate-
    Assert(rn > 0 && rn < ARM_REG_MAX);
    // @TODO: Fix 12-bit immediate range check
    // Assert(imm& >= 0x800 && imm <= 0x7FF);
    u8 Rn = rn-1;
    int cond = ARM_COND_AL;
    int imm12 = imm & 0xFFF;
    int inst = BITS(cond, 4, 28) | BITS(0b00110101, 8, 20) | BITS(Rn, 4, 16)
        | BITS(imm, 12, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_mov(ARMRegister rd, ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/MOV--register--ARM-?lang=en
    ARM_CLAMP_dm()
    int S = 0;
    int cond = ARM_COND_AL;
    int inst = BITS(cond, 4, 28) | BITS(0b0001101, 7, 21) | BITS(S, 1, 20)
        | BITS(Rd,4,12) | BITS(Rm, 4, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_movw(ARMRegister rd, u16 imm16, int cond) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/MOV--immediate-
    ARM_CLAMP_d()
    int imm4 = imm16 >> 12;
    int imm12 = imm16 & 0x0FFF;
    int inst = BITS(cond, 4, 28) | BITS(0b00110000, 8, 20) | BITS(imm4, 4, 16)
        | BITS(Rd,4,12) | BITS(imm12, 12, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_mov_imm(ARMRegister rd, u16 imm, int cond) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/MOVT
    ARM_CLAMP_d()
    int S = 0;
    int imm12 = imm & 0x0FFF;
    int inst = BITS(cond, 4, 28) | BITS(0b0011101, 7, 21) | BITS(S, 1, 20) | 
                BITS(Rd, 4, 12) | BITS(imm12, 12, 0);

    emit4((u32)inst);
}
void ARMBuilder::emit_movt(ARMRegister rd, u16 imm16) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/MOVT
    ARM_CLAMP_d()
    int cond = ARM_COND_AL;
    int imm4 = imm16 >> 12;
    int imm12 = imm16 & 0x0FFF;
    int inst = BITS(cond, 4, 28) | BITS(0b00110100, 8, 20) | BITS(imm4, 4, 16)
        | BITS(Rd,4,12) | BITS(imm12, 12, 0);
    
    emit4((u32)inst);
}
void ARMBuilder::emit_push(ARMRegister r) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/PUSH?lang=en
    Assert(r > 0 && r < ARM_REG_MAX);
    u8 reg = r-1;
    int cond = ARM_COND_AL;
    int reglist = 1 << reg;
    int inst = BITS(cond, 4, 28) | BITS(0b100100101101, 12, 16) | BITS(reglist, 16, 0);
    
    emit4((u32)inst);
    
    if (push_offsets.size())
        push_offsets.last() += REGISTER_SIZE;
    ret_offset += REGISTER_SIZE;
}
void ARMBuilder::emit_pop(ARMRegister r) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/POP--ARM-?lang=en
    Assert(r > 0 && r < ARM_REG_MAX);
    u8 reg = r-1;
    int cond = ARM_COND_AL;
    int reglist = 1 << reg;
    int inst = BITS(cond, 4, 28) | BITS(0b100010111101, 12, 16) | BITS(reglist, 16, 0);
    
    emit4((u32)inst);
    
    if (push_offsets.size())
        push_offsets.last() -= REGISTER_SIZE;
    ret_offset -= REGISTER_SIZE;
}
void ARMBuilder::emit_push_reglist(const ARMRegister* regs, int count) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/PUSH?lang=en
    Assert(count > 0);
    int cond = ARM_COND_AL;
    int reglist = 0;
    for(int i=0;i<count;i++) {
        ARMRegister r = regs[i];
        u8 reg = r-1;
        Assert(r > 0 && r < ARM_REG_MAX);
        reglist |= 1<<reg;
    }
    int inst = BITS(cond, 4, 28) | BITS(0b100100101101, 12, 16) | BITS(reglist, 16, 0);
    
    emit4((u32)inst);
    
    if (push_offsets.size())
        push_offsets.last() += REGISTER_SIZE * count;
    ret_offset += REGISTER_SIZE * count;
}
void ARMBuilder::emit_pop_reglist(const ARMRegister* regs, int count) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/POP--ARM-?lang=en
    Assert(count > 0);
    int cond = ARM_COND_AL;
    int reglist = 0;
    for(int i=0;i<count;i++) {
        ARMRegister r = regs[i];
        u8 reg = r-1;
        Assert(r > 0 && r < ARM_REG_MAX);
        reglist |= 1<<reg;
    }
    int inst = BITS(cond, 4, 28) | BITS(0b100010111101, 12, 16) | BITS(reglist, 16, 0);
    
    emit4((u32)inst);
    
    if (push_offsets.size())
        push_offsets.last() -= REGISTER_SIZE * count;
    ret_offset -= REGISTER_SIZE * count;
}
void ARMBuilder::emit_ldr(ARMRegister rt, ARMRegister rn, i16 imm16) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/LDR--immediate--ARM-?lang=en
    Assert(rt > 0 && rt < ARM_REG_MAX);
    Assert(rn > 0 && rn < ARM_REG_MAX);
    // immediate can't be larger than 12-bits. it can be signed, not including the 12 bits
    Assert(((imm16 & 0x8000) && (imm16 & 0x7000) == 0x7000) || (imm16 & 0xF000) == 0);
    u8 Rt = rt-1;
    u8 Rn = rn-1;
    
    int cond = ARM_COND_AL;
    int P = 1; // P=0 would write back (Rn=Rn+imm) which we don't want
    int U = !(imm16 >> 15);
    int W = 0; // W=1 would write back
    int imm12 = imm16 & 0xFFF;
    if(!U)
        imm12 = -imm12;
    int inst = BITS(cond, 4, 28) | BITS(0b010, 3, 25) | BITS(P, 1, 24)
        | BITS(U, 1, 23) | BITS(W, 1, 21) | BITS(1, 1, 20) | BITS(Rn, 4, 16)
        | BITS(Rt, 4, 12) | BITS(imm12, 12, 0);
    // NOTE: Differs from str at bit 20, bit is 0 in str and 1 in ldr
    emit4((u32)inst);
}

void ARMBuilder::emit_ldrb(ARMRegister rt, ARMRegister rn, i16 imm16) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/LDR--immediate--ARM-?lang=en
    Assert(rt > 0 && rt < ARM_REG_MAX);
    Assert(rn > 0 && rn < ARM_REG_MAX);
    // immediate can't be larger than 12-bits. it can be signed, not including the 12 bits
    Assert(((imm16 & 0x8000) && (imm16 & 0x7000) == 0x7000) || (imm16 & 0xF000) == 0);
    u8 Rt = rt-1;
    u8 Rn = rn-1;
    
    int cond = ARM_COND_AL;
    int P = 1; // P=0 would write back (Rn=Rn+imm) which we don't want
    int U = !(imm16 >> 15);
    int W = 0; // W=1 would write back
    int imm12 = imm16 & 0xFFF;
    if(!U)
        imm12 = -imm12;
    int inst = BITS(cond, 4, 28) | BITS(0b010, 3, 25) | BITS(P, 1, 24)
        | BITS(U, 1, 23) | BITS(1, 1, 22) | BITS(W, 1, 21) | BITS(1, 1, 20) | BITS(Rn, 4, 16)
        | BITS(Rt, 4, 12) | BITS(imm12, 12, 0);
    // NOTE: Differs from str at bit 20, bit is 0 in str and 1 in ldr
    emit4((u32)inst);
}
void ARMBuilder::emit_ldrh(ARMRegister rt, ARMRegister rn, i16 imm16) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/LDR--immediate--ARM-?lang=en
    Assert(rt > 0 && rt < ARM_REG_MAX);
    Assert(rn > 0 && rn < ARM_REG_MAX);
    // immediate can't be larger than 8-bits. it can be signed, not including the 12 bits
    Assert(((imm16 & 0x8000) && (imm16 & 0x7000) == 0x7000) || (imm16 & 0xF000) == 0);
    u8 Rt = rt-1;
    u8 Rn = rn-1;
    
    int cond = ARM_COND_AL;
    int P = 1; // P=0 would write back (Rn=Rn+imm) which we don't want
    int U = !(imm16 >> 15);
    int W = 0; // W=1 would write back
    int imm4h = (imm16 >> 4) & 0xF;
    int imm4l = imm16 & 0xF;
    if(!U)
        imm4h = -imm4h;
    int inst = BITS(cond, 4, 28) | BITS(0b000, 3, 25) | BITS(P, 1, 24)
        | BITS(U, 1, 23) | BITS(1, 1, 22) | BITS(W, 1, 21) | BITS(1, 1, 20) | BITS(Rn, 4, 16)
        | BITS(Rt, 4, 12) | BITS(imm4h, 4, 8) | BITS(0b1011, 4, 4) | BITS(imm4l, 4, 0);
    // NOTE: Differs from str at bit 20, bit is 0 in str and 1 in ldr
    emit4((u32)inst);
}
void ARMBuilder::emit_str(ARMRegister rt, ARMRegister rn, i16 imm16) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/LDR--immediate--ARM-?lang=en
    Assert(rt > 0 && rt < ARM_REG_MAX);
    Assert(rn > 0 && rn < ARM_REG_MAX);
    // immediate can't be larger than 12-bits. it can be signed, not including the 12 bits
    Assert(((imm16 & 0x8000) && (imm16 & 0x7000) == 0x7000) || (imm16 & 0xF000) == 0);
    u8 Rt = rt-1;
    u8 Rn = rn-1;
    
    int cond = ARM_COND_AL;
    int P = 1; // P=0 would write back (Rn=Rn+imm) which we don't want
    int U = !(imm16 >> 15);
    int W = 0; // W=1 would write back
    int imm12 = imm16 & 0xFFF;
    if(!U)
        imm12 = -imm12;
    int inst = BITS(cond, 4, 28) | BITS(0b010, 3, 25) | BITS(P, 1, 24)
        | BITS(U, 1, 23) | BITS(W, 1, 21) | BITS(Rn, 4, 16)
        | BITS(Rt, 4, 12) | BITS(imm12, 12, 0);
    // NOTE: Differs from ldr at bit 20, bit is 0 in str and 1 in ldr
    emit4((u32)inst);
}
void ARMBuilder::emit_strh(ARMRegister rt, ARMRegister rn, i16 imm16) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/LDR--immediate--ARM-?lang=en
    Assert(rt > 0 && rt < ARM_REG_MAX);
    Assert(rn > 0 && rn < ARM_REG_MAX);
    // immediate can't be larger than 8-bits. it can be signed, not including the 12 bits
    Assert(((imm16 & 0x8000) && (imm16 & 0x7000) == 0x7000) || (imm16 & 0xF000) == 0);
    u8 Rt = rt-1;
    u8 Rn = rn-1;
    
    int cond = ARM_COND_AL;
    int P = 1; // P=0 would write back (Rn=Rn+imm) which we don't want
    int U = !(imm16 >> 15);
    int W = 0; // W=1 would write back
    int imm4h = (imm16 >> 4) & 0xF;
    int imm4l = imm16 & 0xF;
    if(!U)
        imm4h = -imm4h;
    int inst = BITS(cond, 4, 28) | BITS(0b000, 3, 25) | BITS(P, 1, 24)
        | BITS(U, 1, 23) | BITS(1, 1, 22) | BITS(W, 1, 21) | BITS(0, 1, 20) | BITS(Rn, 4, 16)
        | BITS(Rt, 4, 12) | BITS(imm4h, 4, 8) | BITS(0b1011, 4, 4) | BITS(imm4l, 4, 0);
    // NOTE: Differs from str at bit 20, bit is 0 in str and 1 in ldr
    emit4((u32)inst);
}

void ARMBuilder::emit_strb(ARMRegister rt, ARMRegister rn, i16 imm16) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/LDR--immediate--ARM-?lang=en
    Assert(rt > 0 && rt < ARM_REG_MAX);
    Assert(rn > 0 && rn < ARM_REG_MAX);
    // immediate can't be larger than 12-bits. it can be signed, not including the 12 bits
    Assert(((imm16 & 0x8000) && (imm16 & 0x7000) == 0x7000) || (imm16 & 0xF000) == 0);
    u8 Rt = rt-1;
    u8 Rn = rn-1;
    
    int cond = ARM_COND_AL;
    int P = 1; // P=0 would write back (Rn=Rn+imm) which we don't want
    int U = !(imm16 >> 15);
    int W = 0; // W=1 would write back
    int imm12 = imm16 & 0xFFF;
    if(!U)
        imm12 = -imm12;
    int inst = BITS(cond, 4, 28) | BITS(0b010, 3, 25) | BITS(P, 1, 24)
        | BITS(U, 1, 23) | BITS(1, 1, 22) | BITS(W, 1, 21) | BITS(0, 1, 20) | BITS(Rn, 4, 16)
        | BITS(Rt, 4, 12) | BITS(imm12, 12, 0);
    // NOTE: Differs from str at bit 20, bit is 0 in str and 1 in ldr
    emit4((u32)inst);
}
void ARMBuilder::emit_b(int imm, int cond) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/B
    // TODO: double check these bit checks with maximum,minimum allowed immediates.
    Assert(((imm & 0x8000'0000) && (imm & 0x7F00'0000) == 0x7F00'0000) || (imm & 0xFF00'0000) == 0);
    
    int imm24 = imm & 0xFF'FFFF;
    int inst = BITS(cond, 4, 28) | BITS(0b1010, 4, 24) | BITS(imm24, 24, 0);
    emit4((u32)inst);
}
void ARMBuilder::emit_bl(int imm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/B
    // TODO: double check these bit checks with maximum,minimum allowed immediates.
    Assert(((imm & 0x8000'0000) && (imm & 0x7F00'0000) == 0x7F00'0000) || (imm & 0xFF00'0000) == 0);
    int cond = ARM_COND_AL;
    int imm24 = imm & 0xFF'FFFF;
    int inst = BITS(cond, 4, 28) | BITS(0b1011, 4, 24) | BITS(imm24, 24, 0);
    emit4((u32)inst);
}
void ARMBuilder::emit_blx(ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/BLX--register-
    Assert(rm > 0 && rm < ARM_REG_MAX);
    u8 Rm = rm-1;
    int cond = ARM_COND_AL;
    int inst = BITS(cond, 4, 28) | BITS(0b00010010, 8, 20) 
        | BITS(0xFFF, 12, 8) | BITS(0b0011,4,4) | BITS(Rm,4,0);
    emit4((u32)inst);
}
void ARMBuilder::emit_bx(ARMRegister rm) {
    // https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/BX?lang=en
    Assert(rm > 0 && rm < ARM_REG_MAX);
    u8 Rm = rm-1;
    int cond = ARM_COND_AL;
    int inst = BITS(cond, 4, 28) | BITS(0b00010010, 8, 20)
        | BITS(0xFFF, 12, 8) | BITS(0b0001, 4, 4) | BITS(Rm, 4, 0);
    emit4((u32)inst);
}

bool ARMBuilder::prepare_assembly(Bytecode::ASM& asmInst) {
    using namespace engone;
    
    #define SEND_ERROR() compiler->compile_stats.errors++;
    
    // TODO: Use one inline assembly file per thread and reuse them.
    //   bin path gets full of assembly files otherwise.
    //   The only downside is that if there was a problem, the user can't take a look at
    //   the assembly because it might have been overwritten so spamming assembly files will do for the time being.
    std::string asm_file = "bin/inline_asm"+std::to_string(tinycode->index)+".s";
    std::string obj_file = "bin/inline_asm"+std::to_string(tinycode->index)+".o";
    auto file = engone::FileOpen(asm_file,FILE_CLEAR_AND_WRITE);
    defer { if(file) engone::FileClose(file); };
    if(!file) {
        SEND_ERROR();
        log::out << log::RED << "Could not create " << asm_file << "!\n";
        return false;
    }
    const char* pretext = nullptr;
    const char* posttext = nullptr;
    int asm_line_offset = 0;
    std::string cmd = "";
    std::string assembler = "arm-none-eabi-as";
    // We assume arm-none-eabi
    pretext = ".text\n";
    posttext = "\n"; // assembler complains about not having a newline so we add one here
    asm_line_offset = 3;
    cmd = assembler + " -o " + obj_file + " " + asm_file;
    
    bool yes = engone::FileWrite(file, pretext, strlen(pretext));
    Assert(yes);
    char* asmText = bytecode->rawInlineAssembly._ptr + asmInst.start;
    u32 asmLen = asmInst.end - asmInst.start;
    yes = engone::FileWrite(file, asmText, asmLen);
    Assert(yes);
    yes = engone::FileWrite(file, posttext, strlen(posttext));
    Assert(yes);

    engone::FileClose(file);
    file = {};

    // TODO: Turn off logging from ml64? or at least pipe into somewhere other than stdout.
    //  If ml64 failed then we do want the error messages.
    // TODO: ml64 can compile multiple assembly files into multiple object files. Not sure how we
    //    separate them out later. This is probably faster than doing one by one. Altough, be wary 
    //    of command line character limit.
    //  
    auto asmLog = engone::FileOpen("bin/asm.log",FILE_CLEAR_AND_WRITE);
    defer { if(asmLog) engone::FileClose(asmLog); };
    
    int exitCode = 0;
    yes = engone::StartProgram((char*)cmd.data(), PROGRAM_WAIT, &exitCode, nullptr, {}, asmLog, asmLog);
    if(exitCode != 0 || !yes) {
        u64 fileSize = engone::FileGetSize(asmLog);
        if(fileSize != 0) {
            engone::FileSetHead(asmLog, 0);
            QuickArray<char> errorMessages{};
            errorMessages.resize(fileSize);
            
            yes = engone::FileRead(asmLog, errorMessages.data(), errorMessages.size());
            Assert(yes);
            if(yes) {
                // log::out << std::string(errorMessages.data(), errorMessages.size());
                ReformatAssemblerError(LINKER_GCC, asmInst, errorMessages, -asm_line_offset);
            }
        } else {
            // If asmLog is empty then we probably misused it
            // and assembler printed to the compiler's stdout.
            // We print a message anyway in case the user just
            // got "Compiler failed with 5 errors". Debugging that would be rough without this message.
            log::out << log::RED << "Assembly error\n";
        }
        SEND_ERROR();
        return false;
    }
    engone::FileClose(asmLog);
    asmLog = {};

    // TODO: DeconstructFile isn't optimized and we deconstruct symbols and segments we don't care about.
    //  Write a specific function for just the text segment.
    Assert(compiler->options->target == TARGET_ARM);
    if(compiler->options->target == TARGET_ARM) {
        // TODO: What about arch64?
        auto objfile = FileELF::DeconstructFile(obj_file);
        defer { FileELF::Destroy(objfile); };
        if(!objfile) {
            log::out << log::RED << "Could not find " << obj_file  << "!\n";
            SEND_ERROR();
            return false;
        }
        int textSection_index = objfile->sectionIndexByName(".text");
        if(textSection_index == 0) {
            log::out << log::RED << "Text section could not be found for the compiled inline assembly. Compiler bug?\n";
            SEND_ERROR();
            return false;
        }
        int relocations_count=0;
        if(objfile->is_32bit) {
            auto relocations = objfile->relaOfSection32(textSection_index, &relocations_count);
        } else {
            auto relocations = objfile->relaOfSection(textSection_index, &relocations_count);
        }
        if(relocations_count!=0){
            log::out << log::RED << "Relocations are not supported in inline assembly.\n";
            SEND_ERROR();
            return false;
        }
        
        int textSize = 0;
        const u8* textData = objfile->dataOfSection(textSection_index, &textSize);
        Assert(textData);
        
        // u8* ptr = objfile->_rawFileData + section->PointerToRawData;
        // u32 len = section->SizeOfRawData;
        // log::out << "Inline assembly "<<i << " size: "<<len<<"\n";
        asmInst.iStart = bytecode->rawInstructions.used;
        bytecode->rawInstructions.reserve(bytecode->rawInstructions.used + textSize);
        memcpy(bytecode->rawInstructions._ptr + bytecode->rawInstructions.used, textData, textSize);
        bytecode->rawInstructions.used += textSize;
        asmInst.iEnd = bytecode->rawInstructions.used;
    }
    return true;
}



const char* arm_register_names[]{
    "INVALID", // ARM_REG_INVALID = 0,
    "r0",      // ARM_REG_R0,
    "r1",      // ARM_REG_R1,
    "r2",      // ARM_REG_R2,
    "r3",      // ARM_REG_R3,
    "r4",      // ARM_REG_R4,
    "r5",      // ARM_REG_R5,
    "r6",      // ARM_REG_R6,
    "r7",      // ARM_REG_R7,
    "r8",      // ARM_REG_R8,
    "r9",      // ARM_REG_R9,
    "r10",     // ARM_REG_R10,
    "fp",     // ARM_REG_R11,
    "ip",     // ARM_REG_R12,
    "sp",     // ARM_REG_R13,
    "lr",     // ARM_REG_R14,
    "pc",     // ARM_REG_R15,
};
engone::Logger &operator<<(engone::Logger &l, ARMRegister r) {
    l << arm_register_names[r];
    return l;
}