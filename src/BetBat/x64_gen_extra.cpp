/*
    Experimental x64 generator
    (well, the whole compiler is experimental)
*/

#include "BetBat/x64_gen.h"
#include "BetBat/x64_defs.h"

#include "BetBat/CompilerEnums.h"
#include "BetBat/Compiler.h"

#include "BetBat/Reformatter.h"

bool X64Builder::generateFromTinycode_v2(Bytecode* code, TinyBytecode* tinycode) {
    using namespace engone;

    this->bytecode = bytecode;
    this->tinycode = tinycode;
    
    bool failed = false;
    for(auto ind : tinycode->required_asm_instances) {
        auto& inst = code->asmInstances[ind];
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
    
    // NOTE: The generator makes assumptions about the bytecode.
    //  - alloc_local isn't called iteratively unless it's scoped
    //  - registers aren't saved between calls and jumps
    
    bool logging = false;
    // logging = true;
    
    DynamicArray<Arg> accessed_params;

    auto& instructions = tinycode->instructionSegment;
    bool find_push = false;
    int push_level = 0;
    bool running = true;
    i64 pc = 0;
    while(running) {
        if(!(pc < instructions.size()))
            break;
        i64 prev_pc = pc;
        InstructionOpcode opcode = (InstructionOpcode)instructions[pc];
        pc++;
        
        if(logging) {
            tinycode->print(prev_pc, prev_pc + 1, bytecode);
            log::out << "\n";
        }
        
        auto n = createInst(opcode); // passing opcode does nothing
        n->bc_index = prev_pc;
        n->base = (InstBase*)&instructions[prev_pc];
        InstBaseType flags = instruction_contents[opcode];
        if(flags & BASE_op1) pc++;
        if(flags & BASE_op2) pc++;
        if(flags & BASE_op3) pc++;
        if(flags & BASE_ctrl) pc++;
        if(flags & BASE_link) pc++;
        if(flags & BASE_call) pc++;
        if(flags & BASE_imm8) pc++;
        if(flags & BASE_imm16) pc+=2;
        if(flags & BASE_imm32) pc+=4;
        if(flags & BASE_imm64) pc+=8;
        insert_inst(n);

        // log::out << n->base->opcode<<"\n";
        
        if(opcode == BC_GET_PARAM) {
            auto base = (InstBase_op1_ctrl_imm16*)n->base;
            int param_index = base->imm16 / 8;
            if(param_index >= accessed_params.size()) {
                accessed_params.resize(param_index+1);
            }
            accessed_params[param_index].control = base->control;
        }
    }

    auto IS_ESSENTIAL=[&](InstructionOpcode t) {
        return t == BC_RET
        || t == BC_JZ
        || t == BC_JNZ
        || t == BC_JMP
        || t == BC_MOV_MR
        || t == BC_MOV_MR_DISP16
        || t == BC_SET_ARG
        || t == BC_SET_RET
        || t == BC_CALL
        || t == BC_ATOMIC_ADD
        || t == BC_ATOMIC_CMP_SWAP
        || t == BC_MEMCPY
        || t == BC_MEMZERO
        || t == BC_ASM
        || t == BC_TEST_VALUE
        ;
    };

    auto IS_GENERAL_REG=[&](BCRegister r) {
        return r != BC_REG_INVALID && r != BC_REG_LOCALS;
    };

    // bad name, initial register, start of register journey, full register overwrite
    auto IS_START=[&](InstructionOpcode t) {
        return t == BC_MOV_RM
        || t == BC_MOV_RM_DISP16
        || t == BC_LI32
        || t == BC_LI64
        || t == BC_MOV_RR
        || t == BC_CAST
        || t == BC_STRLEN
        || t == BC_GET_PARAM
        || t == BC_GET_VAL
        || t == BC_PTR_TO_LOCALS
        || t == BC_PTR_TO_PARAMS
        || t == BC_DATAPTR
        || t == BC_CODEPTR
        || t == BC_RDTSC
        ;
    };
    // auto IS_WALL=[&](InstructionOpcode t) {
    //     return t == BC_JZ
    //     || t == BC_RET
    //     ;
    // };

    // TODO: Option to turn on/off optimizations?
    //   We turn them off for speed or to check if the normal generator works fine while the optimization code has a bug in it.

    // WE CAN'T OPTIMIZE AWAY PUSH AND POP BECAUSE IT'LL MESS UP 16-byte alignment when calling functinos.
    #define ENABLE_X64_OPTIMIZATIONS

    // TODO: If we have this bytecode:
    //     li a, 5
    //     add f, a
    //     mov_mr [locals], f
    //   The generator will just accept it even though f has an undefined value.
    //   We want to assert these cases because it's a bug in the bytecode generator.

    // TODO: Optimize out unecessary push and pop (push/pop in sequence after each other)
    // TODO: Allocate some temporary space to store values instead of pushing and popping them.
    //   We can use MODE_DEREF to access to values.
    bool last_was_pop = false; // used to detect redundant push and pop
    int index_of_last_pop = -1;
    X64Inst* original_recipient = nullptr;
    int original_recipient_regnr = 0;

    DynamicArray<int> insts_to_delete{};
    for(int i=inst_list.size()-1;i>=0;i--) {
        auto n = inst_list[i];
        // log::out <<log::GOLD<< n->base->opcode<<"\n";

        if(n->base->opcode == BC_POP) {
            auto base = (InstBase_op1*)n->base;
            auto& v = bc_register_map[base->op0];
            auto recipient = v.used_by;
            auto reg_nr = v.reg_nr;
            if(recipient) {
                n->reg0 = recipient->regs[reg_nr];
                
                bc_push_list.add({});
                bc_push_list.last().used_by = n;
                bc_push_list.last().reg_nr = 0;
                #ifdef ENABLE_X64_OPTIMIZATIONS
                // turn this off if you suspect bugs
                last_was_pop = true;
                #endif
                index_of_last_pop = i;
                original_recipient = recipient;
                original_recipient_regnr = reg_nr;
                
                // insts_to_delete.add(i);
            } else {
                last_was_pop = false;
                insts_to_delete.add(i);
                bc_push_list.add({});
                bc_push_list.last().used_by = nullptr;
            }
            free_map_reg(n,0);
            continue;
        }
        if(n->base->opcode == BC_PUSH) {
            auto base = (InstBase_op1*)n->base;
            auto& v = bc_push_list.last();  // TODO: What if bytecode is messed up and didn't add a pop?
            auto recipient = v.used_by;
            // auto reg_nr = v.reg_nr;
            if(recipient) {
                if(last_was_pop) {
                    last_was_pop = false;
                    n->reg0 = recipient->reg0; // we know recipient is pop and that it has reg0

                    insts_to_delete.add(index_of_last_pop);
                    insts_to_delete.add(i);
                    
                    // map_reg(original_recipient,original_recipient_regnr);
                    bc_register_map[base->op0].used_by = original_recipient;
                    bc_register_map[base->op0].reg_nr = original_recipient_regnr;
                } else {
                    n->reg0 = alloc_artifical_reg(n->bc_index);
                    map_reg(n,0);
                }
            } else {
                insts_to_delete.add(i);
            }
            last_was_pop = false;
            bc_push_list.pop();
            continue;
        }
        if(n->base->opcode == BC_ASM) {
            auto base = (InstBase_imm32_imm8_2*)n->base;
            
            // base->imm8_0 // inputs
            for(int i=0;i<base->imm8_0;i++) {
                bc_push_list.add({});
                bc_push_list.last().used_by = n;
                bc_push_list.last().reg_nr = -1;
            }
            // base->imm8_1 // outputs
            for(int i=0;i<base->imm8_1;i++) {
                bc_push_list.pop();
            }
        }
        last_was_pop = false;
        if(n->base->opcode == BC_CALL) {
            // TODO: Push list?
            // NOTE: We assume that registers aren't used between calls. The bytecode should have asserts so it doesn't happen.
            clear_register_map();
        }
        // IMPORTANT: We don't clear on JZ or JNZ. This could be really bad.
        //   But if the generator 
        // if(n->base->opcode == BC_JMP || n->base->opcode == BC_JZ || n->base->opcode == BC_JNZ) {
        if(n->base->opcode == BC_JMP) {
            Assert(bc_push_list.size() == 0);
            clear_register_map();
            // can we detect whether the register map is empty?
            // assert if so and detect bugs?
            // not sure if the current logic allows that
        }
        
        if(n->base->opcode == BC_CAST) {
            auto base = (InstBase_op2_ctrl*)n->base;
            auto& v = bc_register_map[base->op0];
            auto recipient = v.used_by;
            auto reg_nr = v.reg_nr;
            if(recipient) {
                // TODO: What if we convert to float but the recipient explicitly was non-float operation?
                n->reg0 = recipient->regs[reg_nr];
                map_reg(n,0);

                // if(IS_CONTROL_FLOAT(base->control)) {
                //     suggest_artifical_float(n->reg1);
                //     suggest_artifical_size(n->reg1, 1 << GET_CONTROL_SIZE(base->control));
                //     Assert(n->reg0 != n->reg1);
                // }
                if(IS_CONTROL_CONVERT_FLOAT(base->control)) {
                    suggest_artifical_float(n->reg0);
                    suggest_artifical_size(n->reg0, 1 << GET_CONTROL_CONVERT_SIZE(base->control));
                    // Assert(n->reg0 != n->reg1);
                    // one artifical register can't store how the value changed and was casted over  time. We must cast from one artifical to another.
                    // Actually, same BC register is fine, we just create a new artifical register
                }

                if(IS_START(n->base->opcode)) {
                    // TODO: Handle mov_rr, delete it if it isn't necessary
                    free_map_reg(n,0);
                }
            } else {
                insts_to_delete.add(i);
                continue; // don't allocate registers
            }
            {
            auto& v = bc_register_map[base->op1];
            auto recipient = v.used_by;
            auto reg_nr = v.reg_nr;
            if(recipient && base->op0 != base->op1) { // we can't share artifical register hence, op0 != op1
                n->reg1 = recipient->regs[reg_nr];
                map_reg(n,1);
            } else {
                n->reg1 = alloc_artifical_reg(n->bc_index);
                if(IS_CONTROL_FLOAT(base->control)) {
                    suggest_artifical_float(n->reg1);
                    suggest_artifical_size(n->reg1, 1 << GET_CONTROL_SIZE(base->control));
                }
                map_reg(n,1);
            }
            }
            continue;
        }

        // TODO: Rework IS_ESSENTIAL, IS_START and so on.
        //   We should group the instructions better.
    
        // TODO: What if two operands refer to the same register
        if(IS_ESSENTIAL(n->base->opcode)) {
            if(instruction_contents[n->base->opcode] & BASE_op1) {
                if(n->base->opcode == BC_SET_RET) {
                    auto base = (InstBase_op1_ctrl_imm16*)n->base;
                    switch(tinycode->call_convention) {
                    case BETCALL: {
                        n->reg0 = alloc_artifical_reg(n->bc_index);
                        map_reg(n,0);
                    } break;
                    case STDCALL:
                    case UNIXCALL: {
                        n->reg0 = alloc_artifical_reg(n->bc_index);
                        // How do we optimally reserve XMM0 and A?
                        // if(IS_CONTROL_FLOAT(base->control)) {
                        //     n->reg0 = alloc_artifical_reg(n->bc_index, X64_REG_XMM0);
                        // } else {
                        //     n->reg0 = alloc_artifical_reg(n->bc_index, X64_REG_A);
                        // }
                        map_reg(n,0);
                    }
                    }
                } else if(n->base->opcode == BC_MOV_MR || n->base->opcode == BC_MOV_MR_DISP16 || n->base->opcode == BC_JNZ || n->base->opcode == BC_JZ) {
                    auto base = (InstBase_op1*)n->base;
                    if(IS_GENERAL_REG(base->op0)) {
                        auto& v = bc_register_map[base->op0];
                        auto recipient = v.used_by;
                        auto reg_nr = v.reg_nr;
                        if(recipient) {
                            n->reg0 = recipient->regs[reg_nr];
                            map_reg(n,0);
                        } else {
                            n->reg0 = alloc_artifical_reg(n->bc_index);
                            map_reg(n,0);
                        }
                    } else if(base->op0 == BC_REG_LOCALS) {
                        n->reg0 = alloc_artifical_reg(-1, X64_REG_BP); // we don't free BP and don't pass bc_index
                    }
                } else {
                    auto base = (InstBase_op1*)n->base;
                    if(IS_GENERAL_REG(base->op0)) {
                        n->reg0 = alloc_artifical_reg(n->bc_index);
                        map_reg(n,0);
                    } else if(base->op0 == BC_REG_LOCALS) {
                        n->reg0 = alloc_artifical_reg(-1, X64_REG_BP); // we don't free BP and don't pass bc_index
                    }
                }
            }
        } else {
            if(instruction_contents[n->base->opcode] & BASE_op1) {
                auto base = (InstBase_op1*)n->base;
                if(IS_GENERAL_REG(base->op0)) {
                    auto& v = bc_register_map[base->op0];
                    auto recipient = v.used_by;
                    auto reg_nr = v.reg_nr;
                    if(recipient) {
                        n->reg0 = recipient->regs[reg_nr];
                        map_reg(n,0);

                        if(IS_START(n->base->opcode)) {
                            // TODO: Handle mov_rr, delete it if it isn't necessary
                            free_map_reg(n,0);
                        }
                    } else {
                        insts_to_delete.add(i);
                        continue; // don't allocate registers
                    }
                } else {
                    // probably side-effect instruction, we do not delete these
                }
            }
        }
        if(instruction_contents[n->base->opcode] & BASE_op2) {
            auto base = (InstBase_op2*)n->base;
            if(IS_GENERAL_REG(base->op1)) {
                auto& v = bc_register_map[base->op1];
                auto recipient = v.used_by;
                auto reg_nr = v.reg_nr;
                if(recipient) {
                    n->reg1 = recipient->regs[reg_nr];
                    map_reg(n,1);
                } else {
                    n->reg1 = alloc_artifical_reg(n->bc_index);
                    map_reg(n,1);
                }
            } else if(base->op1 == BC_REG_LOCALS) {
                n->reg1 = alloc_artifical_reg(-1, X64_REG_BP);
            }
        }
        if(instruction_contents[n->base->opcode] & BASE_op3) {
            auto base = (InstBase_op3*)n->base;
            if(IS_GENERAL_REG(base->op2)) {
                auto& v = bc_register_map[base->op2];
                auto recipient = v.used_by;
                auto reg_nr = v.reg_nr;
                if(recipient) {
                    n->reg2 = recipient->regs[reg_nr];
                    map_reg(n,2);
                } else {
                    n->reg2 = alloc_artifical_reg(n->bc_index);
                    map_reg(n,2);
                }
            } else Assert(false);
        }
        if(n->base->opcode == BC_ROUND || n-> base->opcode == BC_SQRT) {
            suggest_artifical_float(n->reg0);
            suggest_artifical_size(n->reg0, 4);
        }
        if(instruction_contents[n->base->opcode] & BASE_ctrl) {
            int off = 1;
            if(instruction_contents[n->base->opcode] & BASE_op1)
                off++;
            if(instruction_contents[n->base->opcode] & BASE_op2)
                off++;
            if(instruction_contents[n->base->opcode] & BASE_op3)
                off++;
            InstructionControl control = (InstructionControl)*((u8*)n->base + off);
            if(control & CONTROL_FLOAT_OP) {
                // TODO: Is this fine? Should we suggest float for all registers if control is float?
                int size = 1 << GET_CONTROL_SIZE(control);
                if(instruction_contents[n->base->opcode] & BASE_op1) {
                    suggest_artifical_float(n->reg0);
                    suggest_artifical_size(n->reg0, size);
                }
                if(instruction_contents[n->base->opcode] & BASE_op2) {
                    suggest_artifical_float(n->reg1);
                    suggest_artifical_size(n->reg1, size);
                }
                if(instruction_contents[n->base->opcode] & BASE_op3) {
                    suggest_artifical_float(n->reg2);
                    suggest_artifical_size(n->reg2, size);
                }
            }
        }
    }

    for(auto i : insts_to_delete) {
        auto n = inst_list[i];
        // log::out <<log::RED<<"del "<<n->bc_index<<": "<< *n <<"\n";

        // if(n->base->opcode == BC_PUSH || n->base->opcode == BC_POP) {
        //     // Any thing that messes with stack can't be removed because it breaks 16-byte alignemnt for function calls.
            
        // } else {
        // }
        inst_list.removeAt(i);
    }

    // @DEBUG
    // log::out << tinycode->name<<"\n";
    // for(auto n : inst_list) {
    //     log::out << n->bc_index<< " "<< *n << "\n";
    // }
    
    // TODO: Assert that we don't allocate in operands
    #define FIX_PRE_IN_OPERAND(N) auto reg##N = get_artifical_reg(n->reg##N);
    // #define FIX_PRE_IN_OPERAND(N) auto reg##N = get_and_alloc_artifical_reg(n->reg##N);
    #define FIX_PRE_OUT_OPERAND(N) auto reg##N = get_and_alloc_artifical_reg(n->reg##N);

    #define FIX_POST_IN_OPERAND(N) if(reg##N->started_by_bc_index == n->bc_index) free_register(reg##N->reg);
    #define FIX_POST_OUT_OPERAND(N) if(reg##N->started_by_bc_index == n->bc_index) free_register(reg##N->reg);

    DynamicArray<i32> _bc_to_x64_translation;
    _bc_to_x64_translation.resize(tinycode->size());
    int last_pc = 0;
    auto map_translation = [&](i32 bc_addr, i32 asm_addr) {
        while(bc_addr >= 0 && _bc_to_x64_translation[bc_addr] == 0) {
            _bc_to_x64_translation[bc_addr] = asm_addr;
            bc_addr--;
        }
    };
    auto map_strict_translation = [&](i32 bc_addr, i32 asm_addr) {
        _bc_to_x64_translation[bc_addr] = asm_addr;
    };
    auto get_map_translation = [&](i32 bc_addr) {
        if(bc_addr >= _bc_to_x64_translation.size())
            return last_pc;
        return _bc_to_x64_translation[bc_addr];
    };

    struct RelativeRelocation {
        // RelativeRelocation(i32 ip, i32 x64, i32 bc) : currentIP(ip), immediateToModify(x64), bcAddress(bc) {}
        i32 inst_addr=0; // It's more of a nextIP rarther than current. Needed when calculating relative offset
        // Address MUST point to an immediate (or displacement?) with 4 bytes.
        // You can add some flags to this struct for more variation.
        i32 imm32_addr=0; // dword/i32 to modify with correct value
        i32 bc_addr=0; // address in bytecode, needed when looking up absolute offset in bc_to_x64_translation
    };
    DynamicArray<RelativeRelocation> relativeRelocations;
    relativeRelocations._reserve(40);
    auto addRelocation32 = [&](int inst_addr, int imm32_addr, int bc_addr) {
        RelativeRelocation rel{};
        rel.inst_addr = inst_addr;
        rel.imm32_addr = imm32_addr;
        rel.bc_addr = bc_addr;
        relativeRelocations.add(rel);
    };
    
    emit_push(X64_REG_BP);

    emit1(PREFIX_REXW);
    emit1(OPCODE_MOV_REG_RM);
    emit_modrm(MODE_REG, X64_REG_BP, X64_REG_SP);
    
    // Microsoft x64 calling convention
    //    Callee saved: RBX, RBP, RDI, RSI, RSP, R12, R13, R14, R15, and XMM6-XMM15 
    // System V ABI convention
    //    Callee saved: RBX, RBP,           RSP, R12, R13, R14, R15
    
    // TODO: Don't save these unless we use them.
    //   System V ABI doesn't need DI and SI.
    const X64Register callee_saved_regs[]{
        X64_REG_B,
        X64_REG_DI,
        X64_REG_SI,
        X64_REG_R12,
    };
    int callee_saved_regs_len = sizeof(callee_saved_regs)/sizeof(*callee_saved_regs);
    
    
    // We need to save registers if the caller expects that
    //   if we use stdcall, unixcall i guess?
    
    bool enable_callee_saved_registers = false;
    // tinycode->is_used_as_function_pointer;
    // if(tinycode->call_convention == STDCALL || tinycode->call_convention == UNIXCALL) {
    //     enable_callee_saved_registers = true;
    // }
    
    callee_saved_space = 0;
    if(enable_callee_saved_registers) {
        for(int i=0;i<callee_saved_regs_len;i++) {
            emit_push(callee_saved_regs[i]);
            callee_saved_space += 8;
        }
    }
    
    auto pop_callee_saved_regs=[&]() {
        if(enable_callee_saved_registers) {
            for(int i=callee_saved_regs_len-1;i>=0;i--) {
                emit_pop(callee_saved_regs[i]);
            }
        }
    };
    
    const X64Register stdcall_normal_regs[4]{
        X64_REG_C,
        X64_REG_D,
        X64_REG_R8,
        X64_REG_R9,
    };
    const X64Register stdcall_float_regs[4] {
        X64_REG_XMM0,
        X64_REG_XMM1,
        X64_REG_XMM2,
        X64_REG_XMM3,
    };

    const X64Register unixcall_normal_regs[6]{
        X64_REG_DI,
        X64_REG_SI,
        X64_REG_D,
        X64_REG_C,
        X64_REG_R8,
        X64_REG_R9,
    };
    const X64Register unixcall_float_regs[8] {
        X64_REG_XMM0,
        X64_REG_XMM1,
        X64_REG_XMM2,
        X64_REG_XMM3,
        // BC_REG_XMM4,
        // BC_REG_XMM5,
        // BC_REG_XMM6,
        // BC_REG_XMM7,
    };

    DynamicArray<int> misalignments{}; // used by BC_ALLOC_ARGS and BC_FREE_ARGS
    int virtual_stack_pointer = 0; // TODO: abstract into X64Builder?

    // use these when calling intrinsics with call instructions that need 16-byte alignment
    auto push_alignment = [&]() {
        int misalignment = virtual_stack_pointer & 0xf;
        misalignments.add(misalignment);
        int imm = 16 - misalignment;

        if(imm != 0) {
            emit_sub_imm32(X64_REG_SP, (i32)imm);
        }
        virtual_stack_pointer -= imm;
    };
    auto pop_alignment = [&]() {
        int misalignment = misalignments.last();
        misalignments.pop();
        int imm = 16 - misalignment;

        if(imm != 0) {
            emit_add_imm32(X64_REG_SP, (i32)imm);
        }
        virtual_stack_pointer += imm;
    };

    // TODO: If the function (tinycode) has parameters and is stdcall or unixcall
    //   then we need the args will be passed through registers and we must
    //   move them to the stack space.
    if(tinycode->call_convention == STDCALL) {
        // Assert(false);
        for(int i=0;i<accessed_params.size() && i < 4; i++) {
            auto control = accessed_params[i].control;

            int off = i * 8 + FRAME_SIZE;
            X64Register reg_args = X64_REG_BP;
            X64Register reg = stdcall_normal_regs[i];
            if(IS_CONTROL_FLOAT(control))
                reg = stdcall_float_regs[i];

            emit_mov_mem_reg(reg_args,reg,control,off);
        }
    } else if(tinycode->call_convention == UNIXCALL) {
        Assert(false);
    }
    
    struct SPMoment {
        int virtual_sp;
        int pop_at_bc_index;
    };
    DynamicArray<SPMoment> sp_moments{};
    
    // NOTE: If the generator runs out of registers then define the macro below
    //   and run this command: btb -dev > out
    //   Start from the end of the file and see which instruction forgets to 
    //   free their registers.
    // #define DEBUG_REGISTER_USAGE
    
    #ifdef DEBUG_REGISTER_USAGE
    log::out << log::CYAN << tinycode->name<<"\n";
    #endif
    for(auto n : inst_list) {
        auto opcode = n->base->opcode;
        map_translation(n->bc_index, code_size());
        last_pc = code_size();
        
        for(int i=0;i<sp_moments.size();i++) {
            if(n->bc_index == sp_moments[i].pop_at_bc_index) {
                virtual_stack_pointer = sp_moments[i].virtual_sp;
                i--;
                sp_moments.pop();
                break;
            }
        }
        
        #ifdef DEBUG_REGISTER_USAGE
        int log_start = log::out.get_written_bytes();
        log::out <<" "<< n->bc_index<< ": "<< *n;
        int written = log::out.get_written_bytes() - log_start;

        for(int i=written; i < 40; i++) {
            log::out << " ";
        }
        
        log::out << " - ";
        for(auto& pair : registers) {
            if(pair.second.used)
                log::out << pair.first << " ";
        }
        log::out << "\n";
        #endif
        
        switch(n->base->opcode) {
            case BC_HALT: {
                Assert(("HALT not implemented",false));
            } break;
            case BC_NOP: {
                emit1(OPCODE_NOP);
            } break;
            case BC_MOV_RR: {
                FIX_PRE_IN_OPERAND(1)
                FIX_PRE_OUT_OPERAND(0)
                emit_mov_reg_reg(reg0->reg, reg1->reg);
                FIX_POST_OUT_OPERAND(0)
                FIX_POST_IN_OPERAND(1)
            } break;
            case BC_MOV_RM:
            case BC_MOV_RM_DISP16: {
                auto base = (InstBase_op2_ctrl*)n->base;
                i16 imm = 0;
                if(instruction_contents[opcode] & BASE_imm16) {
                    imm = ((InstBase_op2_ctrl_imm16*)n->base)->imm16;
                }
                
                FIX_PRE_IN_OPERAND(1)
                FIX_PRE_OUT_OPERAND(0)
                emit_mov_reg_mem(reg0->reg, reg1->reg, base->control, imm);
                
                // Assert((0 != IS_CONTROL_FLOAT(base->control)) == reg0->floaty);

                if(!IS_CONTROL_FLOAT(base->control) && !reg0->floaty) {
                    if(IS_CONTROL_SIGNED(base->control)) {
                        emit_movsx(reg0->reg,reg0->reg,base->control);
                    } else {
                        emit_movzx(reg0->reg,reg0->reg,base->control);
                    }
                }
                FIX_POST_OUT_OPERAND(0)
                FIX_POST_IN_OPERAND(1)
            } break;
            case BC_MOV_MR:
            case BC_MOV_MR_DISP16: {
                auto base = (InstBase_op2_ctrl*)n->base;
                i16 imm = 0;
                if(instruction_contents[opcode] & BASE_imm16) {
                    imm = ((InstBase_op2_ctrl_imm16*)n->base)->imm16;
                }
                // TODO: Which order are the ops push to the stack?
                FIX_PRE_IN_OPERAND(1)
                FIX_PRE_IN_OPERAND(0)
                emit_mov_mem_reg(reg0->reg, reg1->reg, base->control, imm);
                FIX_POST_IN_OPERAND(0)
                FIX_POST_IN_OPERAND(1)
            } break;
            case BC_PUSH: {
                FIX_PRE_IN_OPERAND(0)
                // if(!n->reg0.on_stack) {
                    // don't push if already on stack
                    emit_push(reg0->reg, reg0->size);
                    virtual_stack_pointer -= 8;
                // }
                FIX_POST_IN_OPERAND(0)
            } break;
            case BC_POP: {
                FIX_PRE_OUT_OPERAND(0)
                // if(!n->reg0.on_stack) {
                    // don't pop if it's supposed to be on stack
                    emit_pop(reg0->reg, reg0->size);
                    virtual_stack_pointer += 8;
                // }
                FIX_POST_OUT_OPERAND(0)
            } break;
            case BC_LI32: {
                auto base = (InstBase_op1_imm32*)n->base;
                
                FIX_PRE_OUT_OPERAND(0)
                
                if(IS_REG_XMM(reg0->reg)) {
                    emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                    emit_modrm_slash(MODE_DEREF_DISP8, 0, X64_REG_SP);
                    emit1((u8)-8);
                    emit4((u32)(i32)base->imm32);

                    emit3(OPCODE_3_MOVSS_REG_RM);
                    emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(reg0->reg), X64_REG_SP);
                    emit1((u8)-8);
                } else {
                    // We do not need a 64-bit move to load a 32-bit into a 64-bit register and ensure that the register is cleared.
                    // A 32-bit move will actually clear the upper bits.
                    // emit_prefix(0,X64_REG_INVALID, reg0->reg);
                    // emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                    // emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg0->reg));
                    // emit4((u32)(i32)base->imm32);
                    
                    u8 reg_field = CLAMP_EXT_REG(reg0->reg) - 1;
                    Assert(reg_field >= 0 && reg_field <= 7);
                    // reg0->reg should be in rm when using MOV_REG_IMM_RD
                    // REXB is used instead of REXR, see intel manual "Register Operand Coded in Opcode Byte"
                    emit_prefix(0, X64_REG_INVALID, reg0->reg);
                    emit1((u8)(OPCODE_MOV_REG_IMM_RD | reg_field));
                    emit4((u32)base->imm32);
                }
                
                FIX_POST_OUT_OPERAND(0)
            } break;
            case BC_LI64: {
                auto base = (InstBase_op1_imm64*)n->base;
                FIX_PRE_OUT_OPERAND(0)

                if(IS_REG_XMM(reg0->reg)) {
                    // Assert(!n->reg0.on_stack);

                    Assert(is_register_free(RESERVED_REG0));
                    X64Register tmp_reg = RESERVED_REG0;

                    u8 reg_field = tmp_reg - 1;
                    Assert(reg_field >= 0 && reg_field <= 7);

                    emit1(PREFIX_REXW);
                    emit1((u8)(OPCODE_MOV_REG_IMM_RD | reg_field));
                    emit8((u64)base->imm64);

                    emit1(PREFIX_REXW);
                    emit1(OPCODE_MOV_RM_REG);
                    emit_modrm(MODE_DEREF_DISP8, tmp_reg, X64_REG_SP);
                    emit1((u8)-8);

                    emit3(OPCODE_3_MOVSD_REG_RM);
                    emit_modrm(MODE_DEREF_DISP8, CLAMP_EXT_REG(reg0->reg), X64_REG_SP);
                    emit1((u8)-8);
                } else {
                    u8 reg_field = CLAMP_EXT_REG(reg0->reg) - 1;
                    Assert(reg_field >= 0 && reg_field <= 7);
                    if(base->imm64 & 0xFFFF'FFFF'0000'0000) {
                        emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg0->reg);
                        emit1((u8)(OPCODE_MOV_REG_IMM_RD | reg_field));
                        emit8((u64)base->imm64);
                    } else {
                        emit_prefix(0, X64_REG_INVALID, reg0->reg);
                        emit1((u8)(OPCODE_MOV_REG_IMM_RD | reg_field));
                        emit4((u32)base->imm64);
                    }
                }
                
                FIX_POST_OUT_OPERAND(0)
            } break;
            case BC_INCR: {
                auto base = (InstBase_op1_imm32*)n->base;
                FIX_PRE_IN_OPERAND(0)
                if((i32)base->imm32 < 0) {
                    emit_sub_imm32(reg0->reg, (i32)-base->imm32);
                } else {
                    emit_add_imm32(reg0->reg, (i32)base->imm32);
                }
                FIX_POST_IN_OPERAND(0)
            } break;
            case BC_ALLOC_LOCAL:
            case BC_ALLOC_ARGS: {
                auto base = (InstBase_op1_imm16*)n->base;
                int imm = (i32)base->imm16; // IMPORTANT: immediate is modified, do not used base->imm16 directly!
                if(opcode == BC_ALLOC_ARGS) {
                    int misalignment = (virtual_stack_pointer + imm) & 0xf;
                    misalignments.add(misalignment);
                    if(misalignment != 0) {
                        imm += 16 - misalignment;
                    }
                }
                if(imm != 0) {
                    if(base->op0 != BC_REG_INVALID) {
                        FIX_PRE_OUT_OPERAND(0)

                        emit_sub_imm32(X64_REG_SP, imm);
                        emit_prefix(PREFIX_REXW, reg0->reg, X64_REG_SP);
                        emit1(OPCODE_MOV_REG_RM);
                        emit_modrm(MODE_REG, CLAMP_EXT_REG(reg0->reg), X64_REG_SP);

                        FIX_POST_OUT_OPERAND(0)                
                    } else {
                        emit_sub_imm32(X64_REG_SP, imm);
                    }
                } else Assert(base->op0 == BC_REG_INVALID); // we can't get pointer if we didn't allocate anything

                virtual_stack_pointer -= imm;
                push_offsets.add(0); // needed for SET_ARG
            } break;
            case BC_FREE_LOCAL:
            case BC_FREE_ARGS: {
                auto base = (InstBase_imm16*)n->base;
                int imm = (i32)base->imm16;
                
                if(opcode == BC_FREE_ARGS) {
                    int misalignment = misalignments.last();
                    misalignments.pop();
                    if(misalignment != 0) {
                        imm += 16 - misalignment;
                    }
                }
                if(imm != 0) {
                    emit_add_imm32(X64_REG_SP, (i32)imm);
                }
                ret_offset -= imm;
                virtual_stack_pointer += imm;
                push_offsets.pop();
            } break;
           case BC_SET_ARG: {
                auto base = (InstBase_op1_ctrl_imm16*)n->base;
                FIX_PRE_IN_OPERAND(0)

                // this code is required with stdcall or unixcall
                // betcall ignores this
                int arg_index = base->imm16 / 8;
                if(arg_index >= recent_set_args.size()) {
                    recent_set_args.resize(arg_index + 1);
                }
                recent_set_args[arg_index].control = base->control;

                Assert(push_offsets.size()); // bytecode is missing ALLOC_ARGS if this fires
                
                int off = base->imm16 + push_offsets.last();
                X64Register reg_args = X64_REG_SP;
                emit_mov_mem_reg(reg_args, reg0->reg, base->control, off);

                FIX_POST_IN_OPERAND(0)
            } break;
            case BC_GET_PARAM: {
                auto base = (InstBase_op1_ctrl_imm16*)n->base;
                FIX_PRE_OUT_OPERAND(0)

                X64Register reg_params = X64_REG_BP;
                int off = base->imm16 + FRAME_SIZE;
                emit_mov_reg_mem(reg0->reg, reg_params, base->control, off);
                
                if(IS_CONTROL_SIGNED(base->control)) {
                    emit_movsx(reg0->reg, reg0->reg, base->control);
                } else if(!IS_CONTROL_FLOAT(base->control)) {
                    emit_movzx(reg0->reg, reg0->reg, base->control);
                }
                
                FIX_POST_OUT_OPERAND(0)
            } break;
            case BC_GET_VAL: {
                auto base = (InstBase_op1_ctrl_imm16*)n->base;
                auto call_base = (InstBase_link_call_imm32*)last_inst_call->base;
                switch(call_base->call) {
                    case BETCALL: {
                        FIX_PRE_OUT_OPERAND(0)

                        X64Register reg_params = X64_REG_SP;
                        int off = base->imm16 - FRAME_SIZE + ret_offset;
                        emit_mov_reg_mem(reg0->reg, reg_params, base->control, off);
                    
                        if(IS_CONTROL_SIGNED(base->control)) {
                            emit_movsx(reg0->reg, reg0->reg, base->control);
                        } else if(!IS_CONTROL_FLOAT(base->control)) {
                            emit_movzx(reg0->reg, reg0->reg, base->control);
                        }
                        
                        FIX_POST_OUT_OPERAND(0)
                    } break;
                    case STDCALL:
                    case UNIXCALL: {
                        // TODO: We hope that an instruction after a call didn't
                        //   overwrite the return value in RAX. We assert if it did.
                        //   Should we reserve RAX after a call until we reach BC_GET_VAL
                        //   and use the return value? No because some function do not
                        //   anything so we would have wasted rax. Then what do we do?

                        // For now we assume RAX has been reserved
                        auto reg0 = get_artifical_reg(n->reg0);
                        if(reg0->floaty) {
                            reg0->reg = alloc_register(X64_REG_XMM0, true); 
                            Assert(reg0->reg != X64_REG_INVALID);
                        } else {
                            reg0->reg = alloc_register(X64_REG_A, false);
                            Assert(reg0->reg != X64_REG_INVALID);
                        }
                        // TODO: movzx, movsx on returned value?
                    } break;
                }
            } break;
            case BC_SET_RET: {
                auto base = (InstBase_op1_ctrl_imm16*)n->base;
                switch(tinycode->call_convention) {
                    case BETCALL: {
                        FIX_PRE_IN_OPERAND(0)
                        
                        X64Register reg_rets = X64_REG_BP;
                        int off = base->imm16 - callee_saved_space;

                        emit_mov_mem_reg(reg_rets, reg0->reg, base->control, off);
                        
                        FIX_POST_IN_OPERAND(0)
                    } break;
                    case STDCALL:
                    case UNIXCALL: {
                        FIX_PRE_IN_OPERAND(0)

                        Assert(base->imm16 == -8);
                        Assert(reg0->floaty == (0 != IS_CONTROL_FLOAT(base->control)));

                        if(reg0->floaty) {
                            if(reg0->reg != X64_REG_XMM0)
                                emit_mov_reg_reg(X64_REG_XMM0, reg0->reg, 1 << GET_CONTROL_SIZE(base->control));
                        } else {
                            if(reg0->reg != X64_REG_A)
                                emit_mov_reg_reg(X64_REG_A, reg0->reg);
                        }

                        FIX_POST_IN_OPERAND(0)
                    } break;
                }
            } break;
            case BC_PTR_TO_LOCALS: {
                auto base = (InstBase_op1_imm16*)n->base;
                FIX_PRE_OUT_OPERAND(0);

                X64Register reg_local = X64_REG_BP;
                int off = base->imm16 + callee_saved_space;
                emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg0->reg);
                emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg0->reg));
                emit4((u32)off);


                emit_prefix(PREFIX_REXW, reg0->reg, reg_local);
                emit1(OPCODE_ADD_REG_RM);
                emit_modrm(MODE_REG, CLAMP_EXT_REG(reg0->reg), reg_local);

                FIX_POST_OUT_OPERAND(0);
            } break;
            case BC_PTR_TO_PARAMS: {
                auto base = (InstBase_op1_imm16*)n->base;
                FIX_PRE_OUT_OPERAND(0);

                X64Register reg_params = X64_REG_BP;

                emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg0->reg);
                emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg0->reg));
                emit4((u32)(base->imm16 + FRAME_SIZE));

                emit_prefix(PREFIX_REXW, reg0->reg, reg_params);
                emit1(OPCODE_ADD_REG_RM);
                emit_modrm(MODE_REG, CLAMP_EXT_REG(reg0->reg), reg_params);

                FIX_POST_OUT_OPERAND(0);
            } break;
            case BC_JMP: {
                auto base = (InstBase_imm32*)n->base;

                emit1(OPCODE_JMP_IMM32);
                int imm_offset = code_size();
                emit4((u32)0);
                
                const u8 BYTE_OF_BC_JMP = 1 + 4; // TODO: DON'T HARDCODE VALUES!
                addRelocation32(imm_offset - 1, imm_offset, n->bc_index + BYTE_OF_BC_JMP + base->imm32);
            } break;
            case BC_CALL: {
                auto base = (InstBase_link_call_imm32*)n->base;
                last_inst_call = n;
                // Setup arguments
                // Arguments are pushed onto the stack by set_arg instructions.
                // Some calling conventions require the first four arguments in registers
                // We move the args on stack to registers.
                // TODO: We could improve by directly moving args to registers
                //  but it's more complicated.
                // Assert(push_offset == 0); push_offset does not have to be zero
                switch(base->call) {
                    case BETCALL: break;
                    case STDCALL: {
                        // recent_set_args
                        for(int i=0;i<recent_set_args.size() && i < 4;i++) {
                            auto& arg = recent_set_args[i];
                            auto control = arg.control;

                            int off = i * 8;
                            X64Register reg_args = X64_REG_SP;
                            X64Register reg = stdcall_normal_regs[i];
                            if(IS_CONTROL_FLOAT(control))
                                reg = stdcall_float_regs[i];

                            emit_mov_reg_mem(reg,reg_args,control,off);
                        }
                    } break;
                    case UNIXCALL: {
                        Assert(false);
                        // unixcall_normal_regs
                    } break;
                    default: Assert(false);
                }
                recent_set_args.resize(0);
                ret_offset = 0;
                if(base->link == LinkConventions::VARIMPORT || base->link == LinkConventions::DLLIMPORT) {
                    // log::out << log::RED << "@varimport is incomplete. It was specified in a function named '"<<tinycode->name<<"'\n";
                    // Assert(("incomplete @varimport",false));
                    emit1(OPCODE_CALL_RM_SLASH_2);

                    emit_modrm_rip32_slash((u8)2, 0);
                    int offset = code_size() - 4;

                    map_strict_translation(n->bc_index + 3, offset);

                } else if(base->link == LinkConventions::IMPORT) {
                    emit1(OPCODE_CALL_IMM);
                    int offset = code_size();
                    emit4((u32)0);
                    
                    // necessary when adding internal func relocations
                    // +3 = 1 (opcode) + 1 (link) + 1 (convention)
                    map_strict_translation(n->bc_index + 3, offset);

                } else if (base->link == LinkConventions::NONE){ // or export
                    // Assert(false);
                    emit1(OPCODE_CALL_IMM);
                    int offset = code_size();
                    emit4((u32)0);
                    
                    // necessary when adding internal func relocations
                    // +3 = 1 (opcode) + 1 (link) + 1 (convention)
                    map_strict_translation(n->bc_index + 3, offset);
                    // bc_to_x64_translation[n->bc_index + 3] = offset;
                    // prog->addInternalFuncRelocation(current_tinyprog_index, offset, n->imm);

                    // RelativeRelocation reloc{};
                    // reloc.currentIP = code_size();
                    // reloc.bcAddress = n->bc_index + n->imm + BYTE_OF_BC_JZ;
                    // reloc.immediateToModify = code_size()-4; // NOTE: RelativeRelocation assumes 32-bit integer JMP_IMM8 would not work without changes
                    // relativeRelocations.add(reloc);

                    // RelativeRelocation reloc{};
                    // reloc.currentIP = prog->size();
                    // reloc.bcAddress = imm;
                    // reloc.immediateToModify = prog->size()-4;
                    // relativeRelocations.add(reloc);
                } else if (base->link == LinkConventions::NATIVE){
                    //-- native function
                    switch((i32)base->imm32) {
                    // You could perhaps implement a platform layer which the language always links too.
                    // That way, you don't need to write different code for each platform.
                    // case NATIVE_malloc: {
                        
                    // }
                    // break;
                    case NATIVE_prints: {
                        // TODO: Check platform target instead
                        #ifdef OS_WINDOWS
                        // ptr = [rsp + 0]
                        // len = [rsp + 8]
                        // char* ptr = *(char**)(fp+argoffset);
                        // u64 len = *(u64*)(fp+argoffset+8);
                        emit_mov_reg_mem(X64_REG_SI, X64_REG_SP, CONTROL_64B, 0); // ptr
                        emit_mov_reg_mem(X64_REG_DI, X64_REG_SP, CONTROL_64B, 8); // Length

                        // TODO: You may want to save registers. This is not needed right
                        //   now since we basically handle everything through push and pop.
                        // TODO: 16-byte alignment on native functions
                        /*
                        sub    rsp,0x38
                        mov    ecx,0xfffffff5
                        call   QWORD PTR [rip+0x0]          # GetStdHandle(-11)
                        mov    QWORD PTR [rsp+0x20],0x0
                        xor    r9,r9
                        mov    r8,rdi                       # rdi = length
                        mov    rdx,rsi                      # rsi = buffer
                        mov    rcx,rax
                        call   QWORD PTR [rip+0x0]          # WriteFile(...)
                        add    rsp,0x38
                        */
                        // rdx should be buffer and r8 length
                        u32 start_addr = code_size();
                        u8 arr[]={ 0x48, 0x83, 0xEC, 0x38, 0xB9, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0x4D, 0x31, 0xC9, 0x49, 0x89, 0xF8, 0x48, 0x89, 0xF2, 0x48, 0x89, 0xC1, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x38 };
                        emit_bytes(arr,sizeof(arr));

                        // C creates these symbol names in it's object file
                        prog->addNamedUndefinedRelocation("__imp_GetStdHandle", start_addr + 0xB, current_tinyprog_index);
                        // prog->addNamedUndefinedRelocation("__imp_GetStdHandle", start_addr + 0xB, current_tinyprog_index);
                        prog->addNamedUndefinedRelocation("__imp_WriteFile", start_addr + 0x26, current_tinyprog_index);
                        // prog->namedUndefinedRelocations.add(reloc0);
                        // prog->namedUndefinedRelocations.add(reloc1);
                    #else
                        // ptr = [rsp + 0]
                        // len = [rsp + 8]
                        // char* ptr = *(char**)(fp+argoffset);
                        // u64 len = *(u64*)(fp+argoffset+8);
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_MOV_REG_RM);
                        emit_modrm(MODE_DEREF, REG_SI, X64_REG_SP);
                        
                        prog->add((u8)(PREFIX_REXW));
                        prog->add(OPCODE_MOV_REG_RM);
                        emit_modrm(MODE_DEREF_DISP8, REG_D, X64_REG_SP);
                        prog->add((u8)8);

                        prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                        prog->addModRM(MODE_REG, 0, REG_DI);
                        prog->add4((u32)1); // 1 = stdout

                        prog->add(OPCODE_CALL_IMM);
                        int reloc_pos = prog->code_size();
                        prog->add4((u32)0);

                        // We call the Unix write system call, altough not directly
                        X64Program::NamedUndefinedRelocation reloc0{};
                        reloc0.name = "write"; // symbol name, gcc (or other linker) knows how to relocate it
                        reloc0.textOffset = reloc_pos;
                        prog->namedUndefinedRelocations.add(reloc0);
                    #endif
                        break;
                    }
                    case NATIVE_printc: {
                    #ifdef OS_WINDOWS
                        // char = [rsp + 7]
                        emit1(PREFIX_REXW);
                        emit1(OPCODE_MOV_REG_RM);
                        emit_modrm(MODE_REG, X64_REG_SI, X64_REG_SP);

                        emit1(PREFIX_REXW);
                        emit1(OPCODE_ADD_RM_IMM_SLASH_0);
                        emit_modrm_slash(MODE_REG, 0, X64_REG_SI);
                        emit4((u32)0);

                        emit1(PREFIX_REXW);
                        emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                        emit_modrm_slash(MODE_REG, 0, X64_REG_DI);
                        emit4((u32)1);

                        // TODO: You may want to save registers. This is not needed right
                        //   now since we basically handle everything through push and pop.

                        /*
                        sub    rsp,0x38
                        mov    ecx,0xfffffff5
                        call   QWORD PTR [rip+0x0]          # GetStdHandle(-11)
                        mov    QWORD PTR [rsp+0x20],0x0
                        xor    r9,r9
                        mov    r8,rdi                       # rdi = length
                        mov    rdx,rsi                      # rsi = buffer (this might be wrong)
                        mov    rcx,rax
                        call   QWORD PTR [rip+0x0]          # WriteFile(...)
                        add    rsp,0x38
                        */
                        int offset = code_size();
                        u8 arr[]={ 0x48, 0x83, 0xEC, 0x38, 0xB9, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0x4D, 0x31, 0xC9, 0x49, 0x89, 0xF8, 0x48, 0x89, 0xF2, 0x48, 0x89, 0xC1, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x38 };
                        emit_bytes(arr,sizeof(arr));

                        // C creates these symbol names in it's object file
                        prog->addNamedUndefinedRelocation("__imp_GetStdHandle",offset + 0xB, current_tinyprog_index);
                        prog->addNamedUndefinedRelocation("__imp_WriteFile",offset + 0x26, current_tinyprog_index);

                    #else
                        // char = [rsp + 7]
                        prog->add(PREFIX_REXW);
                        prog->add(OPCODE_MOV_REG_RM);
                        prog->addModRM(MODE_REG, REG_SI, X64_REG_SP);

                        // add an offset, but not needed?
                        // prog->add(PREFIX_REXW);
                        // prog->add(OPCODE_ADD_RM_IMM_SLASH_0);
                        // prog->addModRM(MODE_REG, 0, REG_SI);
                        // prog->add4((u32)8);

                        // TODO: You may want to save registers. This is not needed right
                        //   now since we basically handle everything through push and pop.

                        prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                        prog->addModRM(MODE_REG, 0, REG_D);
                        prog->add4((u32)1); // 1 byte/char length

                        // prog->add(OPCODE_MOV_RM_REG);
                        // prog->addModRM(MODE_REG, REG_SI, REG_SI); // pointer to buffer

                        prog->add(OPCODE_MOV_RM_IMM32_SLASH_0);
                        prog->addModRM(MODE_REG, 0, REG_DI);
                        prog->add4((u32)1); // stdout

                        prog->add(OPCODE_CALL_IMM);
                        int reloc_pos = prog->code_size();
                        prog->add4((u32)0);

                        // We call the Unix write system call, altough not directly
                        X64Program::NamedUndefinedRelocation reloc0{};
                        reloc0.name = "write"; // symbol name, gcc (or other linker) knows how to relocate it
                        reloc0.textOffset = reloc_pos;
                        prog->namedUndefinedRelocations.add(reloc0);
                    #endif
                        break;
                    }
                    default: {
                        // failure = true;
                        // Assert(bytecode->nativeRegistry);
                        auto nativeRegistry = NativeRegistry::GetGlobal();
                        auto nativeFunction = nativeRegistry->findFunction(base->imm32);
                        // auto* nativeFunction = bytecode->nativeRegistry->findFunction(imm);
                        if(nativeFunction){
                            log::out << log::RED << "Native '"<<nativeFunction->name<<"' (id: "<<base->imm32<<") is not implemented in x64 converter (" OS_NAME ").\n";
                        } else {
                            log::out << log::RED << base->imm32<<" is not a native function (message from x64 converter).\n";
                        }
                    }
                    } // switch
                } else {
                    Assert(false);
                }
                switch(base->call) {
                    case BETCALL: break;
                    case STDCALL:
                    case UNIXCALL: {
                        // nocheckin, TODO: FIXME, do we need to modify the register allocation loop before generation?
                        // auto r = alloc_register(X64_REG_A, false);
                        // Assert(r != X64_REG_INVALID);
                    } break;
                }
                break;
            } break;
            case BC_RET: {
                pop_callee_saved_regs();
                emit_pop(X64_REG_BP);
                emit1(OPCODE_RET);
            } break;
            case BC_JNZ: {
                auto base = (InstBase_op1_imm32*)n->base;
                
                FIX_PRE_IN_OPERAND(0)

                emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg0->reg);
                emit1(OPCODE_CMP_RM_IMM8_SLASH_7);
                emit_modrm_slash(MODE_REG, 7, CLAMP_EXT_REG(reg0->reg));
                emit1((u8)0);
                
                emit2(OPCODE_2_JNE_IMM32);
                int imm_offset = code_size();
                emit4((u32)0);

                const u8 BYTE_OF_BC_JNZ = 1 + 1 + 4; // TODO: DON'T HARDCODE VALUES!
                int jmp_bc_addr = n->bc_index + BYTE_OF_BC_JNZ + base->imm32;
                addRelocation32(imm_offset - 2, imm_offset, jmp_bc_addr);
                
                sp_moments.add({virtual_stack_pointer, jmp_bc_addr});
                
                FIX_POST_IN_OPERAND(0)
            } break;
             case BC_JZ: {
                auto base = (InstBase_op1_imm32*)n->base;
                
                FIX_PRE_IN_OPERAND(0)
                
                emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg0->reg);
                emit1(OPCODE_CMP_RM_IMM8_SLASH_7);
                emit_modrm_slash(MODE_REG, 7, CLAMP_EXT_REG(reg0->reg));
                emit1((u8)0);
                
                emit2(OPCODE_2_JE_IMM32);
                int imm_offset = code_size();
                emit4((u32)0);
                
                const u8 BYTE_OF_BC_JZ = 1 + 1 + 4; // nocheckin TODO: DON'T HARDCODE VALUES!
                int jmp_bc_addr = n->bc_index + BYTE_OF_BC_JZ + base->imm32;
                addRelocation32(imm_offset - 2, imm_offset, jmp_bc_addr);
                
                sp_moments.add({virtual_stack_pointer, jmp_bc_addr});
                
                /*
                  Immediates in bytecode jump instructions are relative to
                    the byte after the final byte in the instruction. That byte offset from
                    start of bytecode instructions PLUS the immediate results in the first
                    byte to execute next cycle. HOWEVER, OPNodes store bc_index based on
                    the first byte in the instruction and we don't have
                    a table of number of bytes per opcode.
                    
                    Either we create a table or use a sneaky hardcoded value here.
                    I am sorry but I choose the lazy option. I have been working on the x64_gen
                    for a while and just want to see it done right now. I will come back
                    and clean it upp later.
                    
                    I do want to add that the hardcoded solution is probably faster than a table
                    unless the compiler does some optimizations on a static const table.
                */
                FIX_POST_IN_OPERAND(0)
            } break;
            case BC_DATAPTR: {
                auto base = (InstBase_op1_imm32*)n->base;
                FIX_PRE_OUT_OPERAND(0)
                
                Assert(!IS_REG_XMM(reg0->reg)); // loading pointer into xmm makes no sense, it's a compiler bug

                emit_prefix(PREFIX_REXW, reg0->reg, X64_REG_INVALID);
                emit1(OPCODE_LEA_REG_M);
                emit_modrm_rip32(CLAMP_EXT_REG(reg0->reg), (u32)0);
                i32 disp32_offset = code_size() - 4; // -4 to refer to the 32-bit immediate in modrm_rip
                prog->addDataRelocation(base->imm32, disp32_offset, current_tinyprog_index);

                FIX_POST_OUT_OPERAND(0)
            } break;
            case BC_CODEPTR: {
                Assert(false);
                // u8 size = DECODE_REG_SIZE(op0);
                // Assert(size==8);

                // prog->add(PREFIX_REXW);
                // prog->add(OPCODE_LEA_REG_M);
                // prog->addModRM_rip(BCToProgramReg(op0,8), (u32)0);
                // RelativeRelocation reloc{};
                // reloc.immediateToModify = prog->size()-4;
                // reloc.currentIP = prog->size();
                // reloc.bcAddress = imm;
                // relativeRelocations.add(reloc);
                // // NamedRelocation reloc{};
                // // reloc.dataOffset = imm;
                // // reloc.
                // // reloc.textOffset = prog->size() - 4;
                // // prog->dataRelocations.add(reloc);
                // // prog->namedRelocations.add();
            } break;
            case BC_ADD:
            case BC_SUB:
            case BC_MUL:
            // case BC_DIV:
            // case BC_MOD:
            
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
            // case BC_LNOT:

            // bitwise operations
            case BC_BAND: 
            case BC_BOR: 
            // case BC_BNOT:
            case BC_BXOR: 
            case BC_BLSHIFT: 
            case BC_BRSHIFT: {
                auto base = (InstBase_op2*)n->base;
                FIX_PRE_IN_OPERAND(1);
                FIX_PRE_IN_OPERAND(0);
                
                InstructionControl control = CONTROL_NONE;
                int size = 8;
                if(instruction_contents[opcode] & BASE_ctrl) {
                    auto base = (InstBase_op2_ctrl*)n->base;
                    control = base->control;
                    size = 1 << GET_CONTROL_SIZE(control);
                }
                bool is_signed = IS_CONTROL_SIGNED(control);
                
                if(IS_CONTROL_FLOAT(control)) {
                    switch(opcode) {
                        case BC_ADD: {
                            if(size == 4)
                                emit3(OPCODE_3_ADDSS_REG_RM);
                            else if(size == 8)
                                emit3(OPCODE_3_ADDSD_REG_RM);
                            else Assert(false);
                            emit_modrm(MODE_REG, CLAMP_XMM(reg0->reg), CLAMP_XMM(reg1->reg));
                        } break;
                        case BC_SUB: {
                            if(size == 4)
                                emit3(OPCODE_3_SUBSS_REG_RM);
                            else if(size == 8)
                                emit3(OPCODE_3_SUBSD_REG_RM);
                            else Assert(false);
                            emit_modrm(MODE_REG, CLAMP_XMM(reg0->reg), CLAMP_XMM(reg1->reg));
                        } break;
                        case BC_MUL: {
                            if(size == 4)
                                emit3(OPCODE_3_MULSS_REG_RM);
                            else if(size == 8)
                                emit3(OPCODE_3_MULSD_REG_RM);
                            else Assert(false);
                            emit_modrm(MODE_REG, CLAMP_XMM(reg0->reg), CLAMP_XMM(reg1->reg));
                        } break;
                        case BC_LAND: {
                            Assert(false);
                        } break;
                        case BC_LOR: {
                            Assert(false);
                        } break;
                        case BC_LNOT: {
                            Assert(false);
                        } break;
                        case BC_BAND: {
                            Assert(false);
                        } break;
                        case BC_BOR: {
                            Assert(false);
                        } break;
                        case BC_BXOR: {
                            Assert(false);
                        } break;
                        case BC_BLSHIFT:
                        case BC_BRSHIFT: {
                            Assert(false);
                            // bool c_was_used = false;
                            // if(!is_register_free(X64_REG_C)) {
                            //     c_was_used = true;
                            //     emit_push(X64_REG_C);

                            //     emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg1->reg);
                            //     emit1(OPCODE_MOV_REG_RM);
                            //     emit_modrm(MODE_REG, X64_REG_C, CLAMP_EXT_REG(reg1->reg));
                            // }

                            // emit1(PREFIX_REXW);
                            // switch(opcode) {
                            //     case BC_BLSHIFT: {
                            //         emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg0->reg);
                            //         emit1(OPCODE_SHL_RM_CL_SLASH_4);
                            //         emit_modrm_slash(MODE_REG, 4, CLAMP_EXT_REG(reg0->reg));
                            //     } break;
                            //     case BC_BRSHIFT: {
                            //         emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg0->reg);
                            //         emit1(OPCODE_SHR_RM_CL_SLASH_5);
                            //         emit_modrm_slash(MODE_REG, 5, CLAMP_EXT_REG(reg0->reg));
                            //     } break;
                            //     default: Assert(false);
                            // }

                            // if(c_was_used) {
                            //     emit_pop(X64_REG_C);
                            // }
                        } break;
                        case BC_EQ:
                        case BC_NEQ:
                        case BC_LT:
                        case BC_LTE:
                        case BC_GT:
                        case BC_GTE: {
                            // TODO: Prefix for XMM8-15
                            if(size == 4) {
                                emit2(OPCODE_2_COMISS_REG_RM);
                            } else {
                                emit3(OPCODE_3_COMISD_REG_RM);
                            }
                            emit_modrm(MODE_REG, CLAMP_XMM(reg0->reg), CLAMP_XMM(reg1->reg));

                            u16 setType = 0;
                            switch(opcode) {
                                case BC_LT: {
                                    setType = is_signed ? OPCODE_2_SETL_RM8 : OPCODE_2_SETB_RM8;
                                } break;
                                case BC_LTE: {
                                    setType = is_signed ? OPCODE_2_SETLE_RM8 : OPCODE_2_SETBE_RM8;
                                } break;
                                case BC_GT: {
                                    setType = is_signed ? OPCODE_2_SETG_RM8 : OPCODE_2_SETA_RM8;
                                } break;
                                case BC_GTE: {
                                    setType = is_signed ? OPCODE_2_SETGE_RM8 : OPCODE_2_SETAE_RM8;
                                } break;
                                case BC_EQ: {
                                    setType = OPCODE_2_SETE_RM8;
                                } break;
                                case BC_NEQ: {
                                    setType = OPCODE_2_SETNE_RM8;
                                } break;
                                default: Assert(false);
                            }
                            
                            // TODO: OH NO, reg0 and reg1 are XMM registers. We can't output bools that way.
                            //   So we modify the artifical register here and free the old XMM and 
                            //   reserve a general register. There may be scenarios I haven't thought about.
                            //   Things might break?
                            free_register(reg0->reg);
                            reg0->reg = X64_REG_INVALID;
                            reg0->floaty = false;
                            reg0->size = 0;
                            FIX_PRE_OUT_OPERAND(0);
                            
                            // WE NEED REX TO SET SIL, DIL and not DH, BH
                            emit_prefix(PREFIX_REX, X64_REG_INVALID, reg0->reg);
                            emit2(setType);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg0->reg));
                            
                            // do we need more stuff or no? I don't think so?
                            emit_prefix(PREFIX_REX, X64_REG_INVALID, reg0->reg);
                            emit2(OPCODE_2_MOVZX_REG_RM8);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(reg0->reg), CLAMP_EXT_REG(reg0->reg));

                        } break;
                        default: Assert(false);
                    }
                } else {
                    switch(opcode) {
                        case BC_ADD: {
                            emit_operation(OPCODE_ADD_REG_RM, reg0->reg, reg1->reg, control);
                        } break;
                        case BC_SUB: {
                            emit_operation(OPCODE_SUB_REG_RM, reg0->reg, reg1->reg, control);
                        } break;
                        case BC_MUL: {
                            // TODO: Handle signed/unsigned multiplication (using InstructionControl)
                            if(is_signed) {
                                auto reg = reg0->reg;
                                auto rm = reg1->reg;
                                // int size = 1 << GET_CONTROL_SIZE(control);
                                if(size == 1) {
                                    emit_movsx(reg, reg, CONTROL_8B);
                                    emit_movsx(rm, rm, CONTROL_8B);
                                    emit1(PREFIX_16BIT);
                                    emit_prefix(0, reg, rm);
                                    emit2(OPCODE_2_IMUL_REG_RM);
                                } else if(size == 2) {
                                    emit1(PREFIX_16BIT);
                                    emit_prefix(0, reg, rm);
                                    emit2(OPCODE_2_IMUL_REG_RM);
                                } else if(size == 4) {
                                    emit_prefix(0, reg, rm);
                                    emit2(OPCODE_2_IMUL_REG_RM);
                                } else if(size == 8) {
                                    emit_prefix(PREFIX_REXW, reg, rm);
                                    emit2(OPCODE_2_IMUL_REG_RM);
                                } else Assert(false);
                                emit_modrm(MODE_REG, CLAMP_EXT_REG(reg), CLAMP_EXT_REG(rm));
                            } else {
                                X64Register real_reg1 = reg1->reg;

                                bool pushed_a = false;
                                bool pushed_d = false;

                                if(reg1->reg == X64_REG_A || reg1->reg == X64_REG_D) {
                                    real_reg1 = RESERVED_REG0;
                                    emit_mov_reg_reg(real_reg1, reg1->reg);
                                }
                                if(reg1->reg != X64_REG_D && reg0->reg != X64_REG_D) {
                                    if(!is_register_free(X64_REG_D)) {
                                        emit_push(X64_REG_D);
                                        pushed_d = true;
                                    }
                                }
                                if(reg1->reg != X64_REG_A && reg0->reg != X64_REG_A) {
                                    if(!is_register_free(X64_REG_A)) {
                                        emit_push(X64_REG_A);
                                        pushed_a = true;
                                    }
                                }
                                if(reg0->reg != X64_REG_A) {
                                    emit_mov_reg_reg(X64_REG_A, reg0->reg);
                                }
                                
                                auto reg = X64_REG_INVALID;
                                auto rm = real_reg1;
                                if(size == 1) {
                                    emit_movzx(reg, reg, CONTROL_8B);
                                    emit_prefix(PREFIX_REX, reg, rm);
                                    emit1(OPCODE_MUL_A8_RM8_SLASH_4);
                                    emit_movzx(reg, reg, CONTROL_8B);
                                } else if(size == 2) {
                                    emit1(PREFIX_16BIT);
                                    emit_prefix(0, reg, rm);
                                    emit1(OPCODE_MUL_A_RM_SLASH_4);
                                } else if(size == 4) {
                                    emit_prefix(0, reg, rm);
                                    emit1(OPCODE_MUL_A_RM_SLASH_4);
                                } else if(size == 8) {
                                    emit_prefix(PREFIX_REXW, reg, rm);
                                    emit1(OPCODE_MUL_A_RM_SLASH_4);
                                } else Assert(false);
                                emit_modrm_slash(MODE_REG, 4, CLAMP_EXT_REG(rm));

                                // emit_prefix(PREFIX_REXW, X64_REG_INVALID, real_reg1);
                                // emit1(OPCODE_MUL_AX_RM_SLASH_4);
                                // emit_modrm_slash(MODE_REG, 4, CLAMP_EXT_REG(real_reg1));
                                
                                if(reg0->reg != X64_REG_A) {
                                    emit_mov_reg_reg(reg0->reg, X64_REG_A);
                                }
                                if(real_reg1 != reg1->reg) {
                                    emit_mov_reg_reg(reg1->reg, real_reg1);
                                }
                                if(pushed_a) {
                                    emit_pop(X64_REG_A);
                                }
                                if(pushed_d) {
                                    emit_pop(X64_REG_D);
                                }
                            }
                        } break;
                        case BC_LAND: {
                            // AND = !(!r0 || !r1)

                            emit_prefix(PREFIX_REXW, reg0->reg, reg0->reg);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(reg0->reg), CLAMP_EXT_REG(reg0->reg));

                            emit1(OPCODE_JZ_IMM8);
                            int offset_jmp0_imm = code_size();
                            emit1((u8)0);

                            emit_prefix(PREFIX_REXW, reg1->reg, reg1->reg);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(reg1->reg), CLAMP_EXT_REG(reg1->reg));
                            
                            emit1(OPCODE_JZ_IMM8);
                            int offset_jmp1_imm = code_size();
                            emit1((u8)0);

                            emit_prefix(0,X64_REG_INVALID, reg0->reg);
                            emit1(OPCODE_MOV_RM8_IMM8_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg0->reg));
                            emit1((u8)1);

                            emit1(OPCODE_JMP_IMM8);
                            int offset_jmp2_imm = code_size();
                            emit1((u8)0);

                            fix_jump_here_imm8(offset_jmp0_imm);
                            fix_jump_here_imm8(offset_jmp1_imm);

                            emit_prefix(0,X64_REG_INVALID, reg0->reg);
                            emit1(OPCODE_MOV_RM8_IMM8_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg0->reg));
                            emit1((u8)0);

                            fix_jump_here_imm8(offset_jmp2_imm);
                            
                            emit_movzx(reg0->reg, reg0->reg, CONTROL_8B);
                        } break;
                        case BC_LOR: {
                            // OR = (r0==0 || r1==0)
                            emit_prefix(PREFIX_REXW, reg0->reg, reg0->reg);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(reg0->reg), CLAMP_EXT_REG(reg0->reg));

                            emit1(OPCODE_JNZ_IMM8);
                            int offset_jmp0_imm = code_size();
                            emit1((u8)0);

                            emit_prefix(PREFIX_REXW, reg1->reg, reg1->reg);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(reg1->reg), CLAMP_EXT_REG(reg1->reg));
                            
                            emit1(OPCODE_JNZ_IMM8);
                            int offset_jmp1_imm = code_size();
                            emit1((u8)0);

                            emit_prefix(0,X64_REG_INVALID, reg0->reg);
                            emit1(OPCODE_MOV_RM8_IMM8_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg0->reg));
                            emit1((u8)0);

                            emit1(OPCODE_JMP_IMM8);
                            int offset_jmp2_imm = code_size();
                            emit1((u8)0);

                            fix_jump_here_imm8(offset_jmp0_imm);
                            fix_jump_here_imm8(offset_jmp1_imm);

                            emit_prefix(0,X64_REG_INVALID, reg0->reg);
                            emit1(OPCODE_MOV_RM8_IMM8_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg0->reg));
                            emit1((u8)1);
                            
                            emit_movzx(reg0->reg, reg0->reg, CONTROL_8B);

                            fix_jump_here_imm8(offset_jmp2_imm);
                        } break;
                        case BC_BAND: {
                            emit_operation(OPCODE_AND_REG_RM, reg0->reg, reg1->reg, control);
                        } break;
                        case BC_BOR: {
                            emit_operation(OPCODE_OR_REG_RM, reg0->reg, reg1->reg, control);
                        } break;
                        case BC_BXOR: {
                            auto base = (InstBase_op2_ctrl*)n->base;
                            if(base->op0 == base->op1) {
                                // BXOR is special if you use the same registers.
                                // It's just an output
                                reg0 = get_and_alloc_artifical_reg(n->reg0);
                                reg1 = reg0;
                                if(IS_REG_XMM(reg0->reg)) {
                                    if(size == 4) {
                                        emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                                        emit_modrm_slash(MODE_DEREF_DISP8, 0, X64_REG_SP);
                                        emit1((u8)-8);
                                        emit4((u32)(i32)0);

                                        emit3(OPCODE_3_MOVSS_REG_RM);
                                        emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(reg0->reg), X64_REG_SP);
                                        emit1((u8)-8);
                                    } else if(size == 8) {
                                        emit_prefix(PREFIX_REXW, X64_REG_INVALID, X64_REG_INVALID);
                                        emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                                        emit_modrm_slash(MODE_DEREF_DISP8, 0, X64_REG_SP);
                                        emit1((u8)-8);
                                        emit4((u32)(i32)0);

                                        emit3(OPCODE_3_MOVSD_REG_RM);
                                        emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(reg0->reg), X64_REG_SP);
                                        emit1((u8)-8);
                                    } else Assert(false);
                                } else {
                                    emit_operation(OPCODE_XOR_REG_RM, reg0->reg, reg1->reg, control);
                                }
                            } else {
                                emit_operation(OPCODE_XOR_REG_RM, reg0->reg, reg1->reg, control);
                            }
                        } break;
                        case BC_BLSHIFT:
                        case BC_BRSHIFT: {
                            bool c_was_used = false;
                            X64Register reg = reg0->reg;
                            if(reg1->reg == X64_REG_C) {

                            } else if(!is_register_free(X64_REG_C)) {
                                c_was_used = true;
                                if(reg0->reg == X64_REG_C) {
                                    reg = RESERVED_REG0;
                                    emit_mov_reg_reg(reg, X64_REG_C);
                                } else {
                                    emit_push(X64_REG_C);
                                }
                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg1->reg);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, X64_REG_C, CLAMP_EXT_REG(reg1->reg));
                            }

                            switch(opcode) {
                                case BC_BLSHIFT: {
                                    emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg);
                                    emit1(OPCODE_SHL_RM_CL_SLASH_4);
                                    emit_modrm_slash(MODE_REG, 4, CLAMP_EXT_REG(reg));
                                } break;
                                case BC_BRSHIFT: {
                                    emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg);
                                    emit1(OPCODE_SHR_RM_CL_SLASH_5);
                                    emit_modrm_slash(MODE_REG, 5, CLAMP_EXT_REG(reg));
                                } break;
                                default: Assert(false);
                            }

                            if(c_was_used) {
                                if(reg0->reg == X64_REG_C) {
                                    emit_mov_reg_reg(X64_REG_C, reg);
                                } else {
                                    emit_pop(X64_REG_C);
                                }
                            }
                        } break;
                        case BC_EQ:
                        case BC_NEQ:
                        case BC_LT:
                        case BC_LTE:
                        case BC_GT:
                        case BC_GTE: {
                            // TODO: There is an optimization for NOT EQUAL where we use XOR.
                            //   BUT, that would kind of be a 64-bit bool so we don't do that.
                            if(GET_CONTROL_SIZE(control) == CONTROL_8B) {
                                // we use rex to access sil, dil NOT dh, bh
                                emit_prefix(PREFIX_REX, reg0->reg, reg1->reg);
                                emit1(OPCODE_CMP_REG8_RM8);
                            } else {
                                if(GET_CONTROL_SIZE(control) == CONTROL_16B) {
                                    emit1(PREFIX_16BIT);
                                    emit_prefix(0, reg0->reg, reg1->reg);
                                } else if(GET_CONTROL_SIZE(control) == CONTROL_32B) {
                                    emit_prefix(0, reg0->reg, reg1->reg);
                                }  else if(GET_CONTROL_SIZE(control) == CONTROL_64B) {
                                    emit_prefix(PREFIX_REXW, reg0->reg, reg1->reg);
                                }
                                emit1(OPCODE_CMP_REG_RM);
                            }
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(reg0->reg), CLAMP_EXT_REG(reg1->reg));

                            u16 setType = 0;
                            switch(opcode) {
                                case BC_LT: {
                                    setType = is_signed ? OPCODE_2_SETL_RM8 : OPCODE_2_SETB_RM8;
                                } break;
                                case BC_LTE: {
                                    setType = is_signed ? OPCODE_2_SETLE_RM8 : OPCODE_2_SETBE_RM8;
                                } break;
                                case BC_GT: {
                                    setType = is_signed ? OPCODE_2_SETG_RM8 : OPCODE_2_SETA_RM8;
                                } break;
                                case BC_GTE: {
                                    setType = is_signed ? OPCODE_2_SETGE_RM8 : OPCODE_2_SETAE_RM8;
                                } break;
                                case BC_EQ: {
                                    setType = OPCODE_2_SETE_RM8;
                                } break;
                                case BC_NEQ: {
                                    setType = OPCODE_2_SETNE_RM8;
                                } break;
                                default: Assert(false);
                            }
                            
                            emit_prefix(PREFIX_REX, X64_REG_INVALID, reg0->reg);
                            emit2(setType);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg0->reg));
                            
                            emit_movzx(reg0->reg, reg0->reg, CONTROL_8B);
                        } break;
                        default: Assert(false);
                    }
                }
                
                FIX_POST_IN_OPERAND(0);
                auto base2 = (InstBase_op2_ctrl*)n->base;
                if(opcode == BC_BXOR && base2->op0 == base2->op1) {
                    // don't free second register, 'BXOR a, a' is special
                } else {
                    FIX_POST_IN_OPERAND(1);
                }
            } break;
            
            case BC_DIV: // these require specific registers
            case BC_MOD: { 
                auto base = (InstBase_op2_ctrl*)n->base;

                FIX_PRE_IN_OPERAND(1);
                FIX_PRE_IN_OPERAND(0);

                bool is_modulo = opcode == BC_MOD;
                bool is_signed = IS_CONTROL_SIGNED(base->control);
                int size = 1 << GET_CONTROL_SIZE(base->control);
                
                if(IS_CONTROL_FLOAT(base->control)) {
                    // Assert(false);
                    int size = 1 << GET_CONTROL_SIZE(base->control);
                    if(opcode == BC_DIV) {
                        if(size ==  4)
                            emit3(OPCODE_3_DIVSS_REG_RM);
                        else if(size == 8)
                            emit3(OPCODE_3_DIVSD_REG_RM);
                        else Assert(false);
                        emit_modrm(MODE_REG, CLAMP_XMM(reg0->reg), CLAMP_XMM(reg1->reg));
                    } else if (opcode == BC_MOD) {
                        X64Register tmp_reg = X64_REG_XMM7;
                        if(!is_register_free(tmp_reg)) {
                            emit_push(tmp_reg, size);
                        }
                        // out = a - round(a/b) * b
                        if(size ==  4) {
                            // movss xmm2, xmm0
                            // divss xmm2, xmm1
                            // roundss xmm2, xmm2, 1
                            // mulss xmm2, xmm1
                            // subss xmm0, xmm2
                            
                            emit3(OPCODE_3_MOVSS_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_XMM(tmp_reg), CLAMP_XMM(reg0->reg));

                            emit3(OPCODE_3_DIVSS_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_XMM(tmp_reg), CLAMP_XMM(reg1->reg));

                            emit4(OPCODE_4_ROUNDSS_REG_RM_IMM8);
                            emit_modrm(MODE_REG,CLAMP_XMM(tmp_reg),CLAMP_XMM(tmp_reg));
                            emit1((u8)1); // determines rounding mode, i chose "1" for a good reason, not sure why right now though (internet is out...)

                            emit3(OPCODE_3_MULSS_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_XMM(tmp_reg), CLAMP_XMM(reg1->reg));

                            emit3(OPCODE_3_SUBSS_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_XMM(reg0->reg), CLAMP_XMM(tmp_reg));
                        } else if(size == 8) {
                            // NOTE: Emit same instructions as "size==4" BUT _SD instead of _SS
                            //   Careful when copy pasting stuff!
                            emit3(OPCODE_3_MOVSD_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_XMM(tmp_reg), CLAMP_XMM(reg0->reg));

                            emit3(OPCODE_3_DIVSD_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_XMM(tmp_reg), CLAMP_XMM(reg1->reg));

                            emit4(OPCODE_4_ROUNDSD_REG_RM_IMM8);
                            emit_modrm(MODE_REG,CLAMP_XMM(tmp_reg),CLAMP_XMM(tmp_reg));
                            emit1((u8)1); // determines rounding mode, i chose "1" for a good reason, not sure why right now though (internet is out...)

                            emit3(OPCODE_3_MULSD_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_XMM(tmp_reg), CLAMP_XMM(reg1->reg));

                            emit3(OPCODE_3_SUBSD_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_XMM(reg0->reg), CLAMP_XMM(tmp_reg));
                        } else Assert(false);
                        if(!is_register_free(tmp_reg)) {
                            emit_pop(tmp_reg, size);
                        }
                    } else Assert(false);
                } else {
                    Assert(reg0->reg != reg1->reg);
                    
                    if(is_signed) {
                        X64Register real_reg1 = reg1->reg;
                        
                        bool pushed_a = false;
                        bool pushed_d = false;

                        if(reg1->reg == X64_REG_A || reg1->reg == X64_REG_D) {
                            real_reg1 = RESERVED_REG0;
                            emit_mov_reg_reg(real_reg1, reg1->reg);
                        }
                        if(reg1->reg != X64_REG_D && reg0->reg != X64_REG_D) {
                            if(!is_register_free(X64_REG_D)) {
                                emit_push(X64_REG_D);
                                pushed_d = true;
                            }
                        }
                        if(reg1->reg != X64_REG_A && reg0->reg != X64_REG_A) {
                            if(!is_register_free(X64_REG_A)) {
                                emit_push(X64_REG_A);
                                pushed_a = true;
                            }
                        }
                        if(reg0->reg != X64_REG_A) {
                            emit_mov_reg_reg(X64_REG_A, reg0->reg);
                        }

                        auto reg = X64_REG_INVALID;
                        auto rm = real_reg1;
                        if(size == 1) {
                            // TODO: Check if this is fine for 8-bit.
                            emit1(PREFIX_16BIT);
                            emit1(OPCODE_CDQ);
                            emit_prefix(PREFIX_REX, reg, rm);
                            emit1(OPCODE_IDIV_A8_RM8_SLASH_7);
                        } else if(size == 2) {
                            emit1(PREFIX_16BIT);
                            emit1(OPCODE_CDQ);
                            emit1(PREFIX_16BIT);
                            emit_prefix(0, reg, rm);
                            emit1(OPCODE_IDIV_A_RM_SLASH_7);
                        } else if(size == 4) {
                            emit1(OPCODE_CDQ);
                            emit_prefix(0, reg, rm);
                            emit1(OPCODE_IDIV_A_RM_SLASH_7);
                        } else if(size == 8) {
                            emit1(PREFIX_REXW);
                            emit1(OPCODE_CDQ);
                            emit_prefix(PREFIX_REXW, reg, rm);
                            emit1(OPCODE_IDIV_A_RM_SLASH_7);
                        } else Assert(false);
                        emit_modrm_slash(MODE_REG, 7, CLAMP_EXT_REG(rm));

                        if(reg0->reg == X64_REG_A && reg1->reg == X64_REG_D) {
                            if(is_modulo) {
                                emit_mov_reg_reg(reg0->reg, X64_REG_D);
                            } else {

                            }
                            emit_mov_reg_reg(reg1->reg, real_reg1);
                        } else if(reg0->reg == X64_REG_D && reg1->reg == X64_REG_A) {
                            if(is_modulo) {

                            } else {
                                emit_mov_reg_reg(reg0->reg, X64_REG_A);
                            }

                            emit_mov_reg_reg(reg1->reg, real_reg1);
                        } else if(reg0->reg == X64_REG_A) {
                            if(is_modulo) {
                                emit_mov_reg_reg(reg0->reg, X64_REG_D);
                            } else {

                            }
                            if(!is_register_free(X64_REG_D)) {
                                emit_pop(X64_REG_D);
                            }
                        } else if(reg0->reg == X64_REG_D) {
                            if(is_modulo) {
                                
                            } else {
                                emit_mov_reg_reg(reg0->reg, X64_REG_A);
                            }
                            if(!is_register_free(X64_REG_A)) {
                                emit_pop(X64_REG_A);
                            }
                        } else if(reg1->reg == X64_REG_A) {
                            if(is_modulo) {
                                emit_mov_reg_reg(reg0->reg, X64_REG_D);
                            } else {
                                emit_mov_reg_reg(reg0->reg, X64_REG_A);
                            }
                            emit_mov_reg_reg(reg1->reg, real_reg1);
                            if(!is_register_free(X64_REG_D)) {
                                emit_pop(X64_REG_D);
                            }
                        } else if(reg1->reg == X64_REG_D) {
                            if(is_modulo) {
                                emit_mov_reg_reg(reg0->reg, X64_REG_D);
                            } else {
                                emit_mov_reg_reg(reg0->reg, X64_REG_A);
                            }
                            emit_mov_reg_reg(reg1->reg, real_reg1);

                            if(!is_register_free(X64_REG_A)) {
                                emit_pop(X64_REG_A);
                            }
                        }  else {
                            if(is_modulo) {
                                emit_mov_reg_reg(reg0->reg, X64_REG_D);
                            } else {
                                emit_mov_reg_reg(reg0->reg, X64_REG_A);
                            }
                            if(!is_register_free(X64_REG_A)) {
                                emit_pop(X64_REG_A);
                            }
                            if(!is_register_free(X64_REG_D)) {
                                emit_pop(X64_REG_D);
                            }
                        }

                        if(is_modulo) {
                            // Most of the time you want (-5 % 3) + 3 = 1
                            // and not -5 % 3 = -2.
                            // so we just do that add for convenience.
                            
                            Assert(size == 4 || size == 8); // TODO: Handle 8-bit and 16-bit compare

                            emit_prefix(size == 8 ? PREFIX_REXW : 0, X64_REG_INVALID, reg0->reg);
                            emit1(OPCODE_CMP_RM_IMM8_SLASH_7);
                            emit_modrm_slash(MODE_REG, 7, reg0->reg);
                            emit1((u8)0);
                            
                            emit1(OPCODE_JGE_REL8);
                            int offset_jmp = code_size();
                            emit1((u8)0);

                            emit_operation(OPCODE_ADD_REG_RM, X64_REG_D, rm, base->control);

                            fix_jump_here_imm8(offset_jmp);
                        }                        

                    } else {
                        X64Register real_reg1 = reg1->reg;

                        // TODO: We should simplifiy this code. The logic is too much for my brain right now.
                        if(reg0->reg == X64_REG_A && reg1->reg == X64_REG_D) {
                            real_reg1 = RESERVED_REG0;
                            emit_mov_reg_reg(real_reg1, reg1->reg);
                        } else if(reg0->reg == X64_REG_D && reg1->reg == X64_REG_A) {
                            real_reg1 = RESERVED_REG0;
                            emit_mov_reg_reg(real_reg1, reg1->reg);

                            emit_mov_reg_reg(X64_REG_A, reg0->reg);
                        } else if(reg0->reg == X64_REG_A) {
                            if(!is_register_free(X64_REG_D)) {
                                emit_push(X64_REG_D);
                            }
                        } else if(reg0->reg == X64_REG_D) {
                            if(!is_register_free(X64_REG_A)) {
                                emit_push(X64_REG_A);
                            }
                            emit_mov_reg_reg(X64_REG_A, reg0->reg);
                        } else if(reg1->reg == X64_REG_A) {
                            if(!is_register_free(X64_REG_D)) {
                                emit_push(X64_REG_D);
                            }
                            real_reg1 = RESERVED_REG0;
                            emit_mov_reg_reg(real_reg1, reg1->reg);

                            emit_mov_reg_reg(X64_REG_A, reg0->reg);
                        } else if(reg1->reg == X64_REG_D) {
                            if(!is_register_free(X64_REG_A)) {
                                emit_push(X64_REG_A);
                            }
                            real_reg1 = RESERVED_REG0;
                            emit_mov_reg_reg(real_reg1, reg1->reg);

                            emit_mov_reg_reg(X64_REG_A, reg0->reg);
                        }  else {
                            if(!is_register_free(X64_REG_D)) {
                                emit_push(X64_REG_D);
                            }
                            if(!is_register_free(X64_REG_A)) {
                                emit_push(X64_REG_A);
                            }
                            emit_mov_reg_reg(X64_REG_A, reg0->reg);
                        }

                        emit_prefix(PREFIX_REXW, X64_REG_INVALID, X64_REG_INVALID);
                        emit1(OPCODE_XOR_REG_RM);
                        emit_modrm(MODE_REG, X64_REG_D, X64_REG_D);

                        // reg0 is dividend
                        // reg1 is divisor
                        // reg0 must be moved to A
                        // we require D to be zeroed
                        
                        auto reg = X64_REG_INVALID;
                        auto rm = real_reg1;
                        if(size == 1) {
                            // 8-bit divide puts result in AL and AH, annoying.
                            // so we just zero upper bits and use 16-bit divide instead.
                            // emit_prefix(PREFIX_REX, reg, rm);
                            // emit1(OPCODE_DIV_A8_RM8_SLASH_6);
                            emit_movzx(X64_REG_A, X64_REG_A, CONTROL_8B);
                            emit_movzx(rm, rm, CONTROL_8B);
                            emit1(PREFIX_16BIT);
                            emit_prefix(0, reg, rm);
                            emit1(OPCODE_DIV_A_RM_SLASH_6);
                        } else if(size == 2) {
                            emit1(PREFIX_16BIT);
                            emit_prefix(0, reg, rm);
                            emit1(OPCODE_DIV_A_RM_SLASH_6);
                        } else if(size == 4) {
                            emit_prefix(0, reg, rm);
                            emit1(OPCODE_DIV_A_RM_SLASH_6);
                        } else if(size == 8) {
                            emit_prefix(PREFIX_REXW, reg, rm);
                            emit1(OPCODE_DIV_A_RM_SLASH_6);
                        } else Assert(false);
                        emit_modrm_slash(MODE_REG, 6, CLAMP_EXT_REG(rm));

                        // emit_prefix(PREFIX_REXW, X64_REG_INVALID, real_reg1);
                        // emit1(OPCODE_DIV_A_RM_SLASH_6);
                        // emit_modrm_slash(MODE_REG, 6, CLAMP_EXT_REG(real_reg1));
                        
                        // A smust be moved to reg0
                        // reg1 is left unchanged
                        // whatever A and D was before,
                        // they should stay the same (unless they were reg0 or reg1)

                        if(reg0->reg == X64_REG_A && reg1->reg == X64_REG_D) {
                            if(is_modulo) {
                                emit_mov_reg_reg(reg0->reg, X64_REG_D);
                            } else {

                            }
                            emit_mov_reg_reg(reg1->reg, real_reg1);
                        } else if(reg0->reg == X64_REG_D && reg1->reg == X64_REG_A) {
                            if(is_modulo) {

                            } else {
                                emit_mov_reg_reg(reg0->reg, X64_REG_A);
                            }

                            emit_mov_reg_reg(reg1->reg, real_reg1);
                        } else if(reg0->reg == X64_REG_A) {
                            if(is_modulo) {
                                emit_mov_reg_reg(reg0->reg, X64_REG_D);
                            } else {

                            }
                            if(!is_register_free(X64_REG_D)) {
                                emit_pop(X64_REG_D);
                            }
                        } else if(reg0->reg == X64_REG_D) {
                            if(is_modulo) {
                                
                            } else {
                                emit_mov_reg_reg(reg0->reg, X64_REG_A);
                            }
                            if(!is_register_free(X64_REG_A)) {
                                emit_pop(X64_REG_A);
                            }
                        } else if(reg1->reg == X64_REG_A) {
                            if(is_modulo) {
                                emit_mov_reg_reg(reg0->reg, X64_REG_D);
                            } else {
                                emit_mov_reg_reg(reg0->reg, X64_REG_A);
                            }
                            emit_mov_reg_reg(reg1->reg, real_reg1);
                            if(!is_register_free(X64_REG_D)) {
                                emit_pop(X64_REG_D);
                            }
                        } else if(reg1->reg == X64_REG_D) {
                            if(is_modulo) {
                                emit_mov_reg_reg(reg0->reg, X64_REG_D);
                            } else {
                                emit_mov_reg_reg(reg0->reg, X64_REG_A);
                            }
                            emit_mov_reg_reg(reg1->reg, real_reg1);

                            if(!is_register_free(X64_REG_A)) {
                                emit_pop(X64_REG_A);
                            }
                        }  else {
                            if(is_modulo) {
                                emit_mov_reg_reg(reg0->reg, X64_REG_D);
                            } else {
                                emit_mov_reg_reg(reg0->reg, X64_REG_A);
                            }
                            if(!is_register_free(X64_REG_A)) {
                                emit_pop(X64_REG_A);
                            }
                            if(!is_register_free(X64_REG_D)) {
                                emit_pop(X64_REG_D);
                            }
                        }
                    }
                }
                
                FIX_POST_IN_OPERAND(0)
                FIX_POST_IN_OPERAND(1)
            } break;
            case BC_LNOT: { // LNOT has one input and output while add has two input and one output, it's special
                FIX_PRE_IN_OPERAND(1);
                FIX_PRE_OUT_OPERAND(0);
                // TODO: What about differently sized values? Should it always be 64-bit operation
                emit_prefix(PREFIX_REXW, reg1->reg, reg1->reg);
                emit1(OPCODE_TEST_RM_REG);
                emit_modrm(MODE_REG, CLAMP_EXT_REG(reg1->reg), CLAMP_EXT_REG(reg1->reg));
                
                emit_prefix(PREFIX_REX, X64_REG_INVALID, reg0->reg);
                emit2(OPCODE_2_SETE_RM8);
                emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg0->reg));

                emit_movzx(reg0->reg, reg0->reg, CONTROL_8B);

                FIX_POST_OUT_OPERAND(0)
                FIX_POST_IN_OPERAND(1)
            } break;
            case BC_BNOT: { // BNOT is special
                FIX_PRE_IN_OPERAND(1);
                FIX_PRE_OUT_OPERAND(0);
                // TODO: Optimize by using one register. No need to allocate an IN and OUT register. We can then skip the MOV instruction.
                emit_prefix(PREFIX_REXW, reg0->reg, reg1->reg);
                emit1(OPCODE_MOV_REG_RM);
                emit_modrm(MODE_REG, CLAMP_EXT_REG(reg0->reg), CLAMP_EXT_REG(reg1->reg));

                emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg0->reg);
                emit1(OPCODE_NOT_RM_SLASH_2);
                emit_modrm_slash(MODE_REG, 2, CLAMP_EXT_REG(reg0->reg));
                
                FIX_POST_OUT_OPERAND(0)
                FIX_POST_IN_OPERAND(1)
            } break;
            case BC_CAST: {
                FIX_PRE_IN_OPERAND(1);
                FIX_PRE_OUT_OPERAND(0);
                
                auto base = (InstBase_op2_ctrl*)n->base;

                int fsize = 1 << GET_CONTROL_SIZE(base->control);
                int tsize = 1 << GET_CONTROL_CONVERT_SIZE(base->control);
                int cast = GET_CONTROL_CAST(base->control);

                u8 minSize = fsize < tsize ? fsize : tsize;
                
                // we only handle 32 and 64-bit floats
                if (IS_CONTROL_FLOAT(base->control)) {
                    Assert(fsize == 4 || fsize == 8);
                }
                if (IS_CONTROL_CONVERT_FLOAT(base->control)) {
                    Assert(tsize == 4 || tsize == 8);
                }

                X64Register from_reg = reg1->reg;
                X64Register to_reg = reg0->reg;
                int pc_before = code_size();
                switch(cast) {
                    case CAST_FLOAT_SINT: {
                        Assert(IS_REG_XMM(from_reg));

                        u8 prefix = construct_prefix(tsize == 8 ? PREFIX_REXW | 0 : 0, to_reg, X64_REG_INVALID);
                        if (fsize == 4) {
                            if(prefix)
                                emit4((OPCODE_4_REXW_CVTTSS2SI_REG_RM & ~0xFF00) | (prefix << 8));
                            else
                                emit3(OPCODE_3_CVTTSS2SI_REG_RM);
                        } else {
                            if(prefix)
                                emit4((OPCODE_4_REXW_CVTTSD2SI_REG_RM & ~0xFF00) | (prefix << 8));
                            else
                                emit3(OPCODE_3_CVTTSD2SI_REG_RM);
                        }
                        emit_modrm(MODE_REG, CLAMP_EXT_REG(to_reg), CLAMP_XMM(from_reg));
                    } break;
                    case CAST_FLOAT_UINT: {
                        

                        if(tsize <= 4) {
                            u8 prefix = construct_prefix(PREFIX_REXW, to_reg, X64_REG_INVALID);
                            if(fsize == 4) {
                                if(prefix) {
                                    emit4((OPCODE_4_REXW_CVTTSS2SI_REG_RM & ~0xFF00) | (prefix << 8));
                                } else {
                                    emit3(OPCODE_3_CVTTSS2SI_REG_RM);
                                }
                            } else {
                                if(prefix) {
                                    emit4((OPCODE_4_REXW_CVTTSD2SI_REG_RM & ~0xFF00) | (prefix << 8));
                                } else {
                                    emit3(OPCODE_3_CVTTSD2SI_REG_RM);
                                }
                            }
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(to_reg), CLAMP_XMM(from_reg));
                        } else {
                            /*
                            movsd  xmm0,QWORD PTR [rbp-0x10]
                            comisd xmm0,QWORD PTR [rip+0x8]        # 37 <main+0x37>
                            jae    3d <main+0x3d>
                            movsd  xmm0,QWORD PTR [rbp-0x10]
                            cvttsd2si rax,xmm0
                            jmp    60 <main+0x60>
                            movsd  xmm0,QWORD PTR [rbp-0x10]
                            movsd  xmm1,QWORD PTR [rip+0x8]        # 52 <main+0x52>
                            subsd  xmm0,xmm1
                            cvttsd2si rax,xmm0
                            movabs rdx,0x8000000000000000
                            xor    rax,rdx
                            */
                            u64 imm = 0x43e0'0000'0000'0000;
                            
                            X64Register tmp_reg = RESERVED_REG0;
                            
                            u8 reg_field = tmp_reg - 1;
                            Assert(reg_field >= 0 && reg_field <= 7);
                            
                            emit1(PREFIX_REXW);
                            emit1((u8)(OPCODE_MOV_REG_IMM_RD | reg_field));
                            emit8((u64)imm);
                            
                            int off = -16; // can't put float immediate at -8 because we push something so we do -16 instead
                            
                            emit_mov_mem_reg(X64_REG_SP, tmp_reg, CONTROL_8B, off);
                            
                            if(fsize == 4)
                                emit2(OPCODE_2_COMISS_REG_RM);
                            else
                                emit3(OPCODE_3_COMISD_REG_RM);
                            emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(from_reg), X64_REG_SP);
                            emit1((u8)off);
                            
                            emit1(OPCODE_JAE_REL8);
                            int jae_offset = code_size();
                            emit1((u8)0);
                            
                            u8 prefix = construct_prefix(PREFIX_REXW, to_reg, X64_REG_INVALID);
                            if(fsize == 4) {
                                emit4((OPCODE_4_REXW_CVTTSS2SI_REG_RM & ~0xFF00) | (prefix << 8));
                            } else {
                                emit4((OPCODE_4_REXW_CVTTSD2SI_REG_RM & ~0xFF00) | (prefix << 8));
                            }
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(to_reg), CLAMP_XMM(from_reg));
                            
                            emit1(OPCODE_JMP_IMM8);
                            int jmp_offset = code_size();
                            emit1((u8)0);
                            
                            fix_jump_here_imm8(jae_offset);
                            
                            emit_push(from_reg);
                            
                            if(fsize == 4) {
                                emit4(OPCODE_3_SUBSS_REG_RM);
                            } else {
                                emit4(OPCODE_3_SUBSD_REG_RM);
                            }
                            emit_modrm(MODE_DEREF_DISP8, from_reg, X64_REG_SP);
                            emit1((u8)off);
                            
                            prefix = construct_prefix(PREFIX_REXW, to_reg, X64_REG_INVALID);
                            if(fsize == 4) {
                                emit4((OPCODE_4_REXW_CVTTSS2SI_REG_RM & ~0xFF00) | (prefix << 8));
                            } else {
                                emit4((OPCODE_4_REXW_CVTTSD2SI_REG_RM & ~0xFF00) | (prefix << 8));
                            }
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(to_reg), CLAMP_XMM(from_reg));
                            
                            reg_field = tmp_reg - 1;
                            Assert(reg_field >= 0 && reg_field <= 7);
                            
                            emit1(PREFIX_REXW);
                            emit1((u8)(OPCODE_MOV_REG_IMM_RD | reg_field));
                            emit8((u64)0x8000'0000'0000'0000);
                            
                            emit_prefix(PREFIX_REXW, to_reg, tmp_reg); 
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(to_reg), CLAMP_EXT_REG(tmp_reg));
                            
                            emit_pop(from_reg);
                            
                            fix_jump_here_imm8(jmp_offset);
                            
                        }
                    } break;
                    case CAST_SINT_FLOAT: {
                        Assert(IS_REG_XMM(to_reg));
                        
                        // TODO: Sign extend
                        u8 prefix = construct_prefix(fsize == 8 ? PREFIX_REXW : 0, X64_REG_INVALID, from_reg);
                        if(tsize == 4) {
                            if(prefix)
                                emit4((OPCODE_4_REXW_CVTSI2SS_REG_RM & ~0xFF00) | (prefix << 8));
                            else
                                emit3(OPCODE_3_CVTSI2SS_REG_RM);
                        } else {
                            if(prefix)
                                emit4((OPCODE_4_REXW_CVTSI2SD_REG_RM & ~0xFF00) | (prefix << 8));
                            else
                                emit3(OPCODE_3_CVTSI2SD_REG_RM);
                        }
                        emit_modrm(MODE_REG, CLAMP_XMM(to_reg), CLAMP_EXT_REG(from_reg));
                    } break;
                    case CAST_UINT_FLOAT: {
                        Assert(IS_REG_XMM(to_reg));
                        
                        if(fsize <= 4) {
                            // TODO: We assume zero extend? I believe that's fine.
                            u8 prefix = construct_prefix(PREFIX_REXW, X64_REG_INVALID, from_reg);
                            if(tsize == 4) {
                                if(prefix)
                                    emit4((OPCODE_4_REXW_CVTSI2SS_REG_RM & ~0xFF00) | (prefix << 8));
                                else
                                    emit3(OPCODE_3_CVTSI2SS_REG_RM);
                            } else {
                                if(prefix)
                                    emit4((OPCODE_4_REXW_CVTSI2SD_REG_RM & ~0xFF00) | (prefix << 8));
                                else
                                    emit3(OPCODE_3_CVTSI2SD_REG_RM);
                            }
                            emit_modrm(MODE_REG, CLAMP_XMM(to_reg), CLAMP_EXT_REG(from_reg));
                        } else {
                            /*
                            test   rax,rax
                            js     29 <main+0x29>
                            pxor   xmm0,xmm0
                            cvtsi2ss xmm0,rax
                            jmp    42 <main+0x42>
                            mov    rdx,rax
                            shr    rdx,1
                            and    eax,0x1
                            or     rdx,rax
                            pxor   xmm0,xmm0
                            cvtsi2ss xmm0,rdx
                            addss  xmm0,xmm0
                            */
                            
                            X64Register tmp_reg0 = RESERVED_REG0;
                            X64Register tmp_reg1 = RESERVED_REG1;

                            emit_prefix(PREFIX_REXW, from_reg, from_reg);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(from_reg), CLAMP_EXT_REG(from_reg));

                            emit1(OPCODE_JS_REL8);
                            u32 js_offset = code_size();
                            emit1((u8)0);

                            emit3(OPCODE_3_SSE_PXOR_RM_REG); // Not sure if it's necessary to zero the xmm register but C compiler do that so we will too just in case.
                            emit_modrm(MODE_REG, CLAMP_XMM(to_reg), CLAMP_XMM(to_reg));

                            u8 prefix = construct_prefix(PREFIX_REXW, X64_REG_INVALID, from_reg);
                            if(tsize == 4) {
                                emit4((OPCODE_4_REXW_CVTSI2SS_REG_RM & ~0xFF00) | (prefix << 8));
                            } else {
                                emit4((OPCODE_4_REXW_CVTSI2SD_REG_RM & ~0xFF00) | (prefix << 8));
                            }
                            emit_modrm(MODE_REG, CLAMP_XMM(to_reg), CLAMP_EXT_REG(from_reg));

                            emit1(OPCODE_JMP_IMM8);
                            u32 jmp_offset = code_size();
                            emit1((u8)0);
                            
                            fix_jump_here_imm8(js_offset);
                            
                            emit_mov_reg_reg(tmp_reg0,from_reg,8);
                            emit_mov_reg_reg(tmp_reg1,from_reg,8);

                            emit1(PREFIX_REXW);
                            emit1(OPCODE_SHR_RM_IMM8_SLASH_5);
                            emit_modrm_slash(MODE_REG, 5, tmp_reg0);
                            emit1((u8)1);
                            
                            emit1(PREFIX_REXW);
                            emit1(OPCODE_AND_RM_IMM8_SLASH_4);
                            emit_modrm_slash(MODE_REG, 4, tmp_reg1);
                            emit1((u8)1);

                            emit1(PREFIX_REXW);
                            emit1(OPCODE_OR_REG_RM);
                            emit_modrm(MODE_REG, tmp_reg0, tmp_reg1);
                            
                            prefix = construct_prefix(PREFIX_REXW, X64_REG_INVALID, tmp_reg0);
                            if(tsize == 4) {
                                emit4((OPCODE_4_REXW_CVTSI2SS_REG_RM & ~0xFF00) | (prefix << 8));
                            } else {
                                emit4((OPCODE_4_REXW_CVTSI2SD_REG_RM & ~0xFF00) | (prefix << 8));
                            }
                            emit_modrm(MODE_REG, CLAMP_XMM(to_reg), CLAMP_EXT_REG(tmp_reg0));

                            if(tsize == 4)
                                emit3(OPCODE_3_ADDSS_REG_RM);
                            else
                                emit3(OPCODE_3_ADDSD_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_XMM(to_reg), CLAMP_XMM(to_reg));

                            fix_jump_here_imm8(jmp_offset);
                        }
                    } break;
                    case CAST_UINT_SINT:
                    case CAST_SINT_UINT: {
                        if(minSize==1){
                            emit_prefix(PREFIX_REXW, to_reg, from_reg);
                            emit2(OPCODE_2_MOVZX_REG_RM8);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(to_reg), CLAMP_EXT_REG(from_reg));
                        } else if(minSize == 2) {
                            emit_prefix(PREFIX_REXW, to_reg, from_reg);
                            emit2(OPCODE_2_MOVZX_REG_RM16);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(to_reg), CLAMP_EXT_REG(from_reg));
                        } else if(minSize == 4) {
                            emit_prefix(0, to_reg, from_reg);
                            emit1(OPCODE_MOV_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(to_reg), CLAMP_EXT_REG(from_reg));
                        } else if(minSize == 8) {
                            if(to_reg != from_reg)
                                emit_mov_reg_reg(to_reg,from_reg);
                        }
                    } break;
                    case CAST_UINT_UINT: {
                        if(to_reg != from_reg)
                            emit_mov_reg_reg(to_reg,from_reg);
                    } break;
                    case CAST_SINT_SINT: {
                        if(minSize == 8) {
                            if(to_reg != from_reg)
                                emit_mov_reg_reg(to_reg,from_reg);
                        } else {
                            emit_movsx(to_reg,from_reg, base->control);
                        }
                    } break;
                    case CAST_FLOAT_FLOAT: {
                        Assert(IS_REG_XMM(to_reg) && IS_REG_XMM(from_reg));
                        // if(IS_REG_XMM(origin_reg)) {
                            if(fsize == 4 && tsize == 8) {
                                emit3(OPCODE_3_CVTSS2SD_REG_RM);
                                emit_modrm(MODE_REG, CLAMP_XMM(to_reg), CLAMP_XMM(from_reg));
                            } else if(fsize == 8 && tsize == 4) {
                                emit3(OPCODE_3_CVTSD2SS_REG_RM);
                                emit_modrm(MODE_REG, CLAMP_XMM(to_reg), CLAMP_XMM(from_reg));
                            } else {
                                // do nothing, sizes are equal
                                if(to_reg != from_reg)
                                    emit_mov_reg_reg(to_reg,from_reg, fsize);
                            }
                        // Code below is for when allocated registers aren't xmm.
                        // The code is not optimal if this happens so we assert above and just don't allow it.
                        // } else {
                        //     Assert(is_register_free(X64_REG_XMM7));
                        //     X64Register temp = X64_REG_XMM7;
                        //     if(fsize == 8)
                        //     emit_prefix(fsize == 8 ? PREFIX_REXW : 0, origin_reg, X64_REG_INVALID);
                        //     emit1(OPCODE_MOV_RM_REG);
                        //     emit_modrm(MODE_DEREF_DISP8, CLAMP_EXT_REG(origin_reg), X64_REG_SP);
                        //     emit1((u8)-8);

                        //     // NOTE: Do we need REXW here?
                        //     if(fsize == 4 && tsize == 8) {
                        //         emit3(OPCODE_3_CVTSS2SD_REG_RM);
                        //     } else if(fsize == 8 && tsize == 4) {
                        //         emit3(OPCODE_3_CVTSD2SS_REG_RM);
                        //     } else if(fsize == 4 && tsize == 4){
                        //         emit3(OPCODE_3_MOVSS_REG_RM);
                        //     } else if(fsize == 8 && tsize == 8) {
                        //         emit3(OPCODE_3_MOVSD_REG_RM);
                        //     }
                        //     emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(temp), X64_REG_SP);
                        //     emit1((u8)-8);

                        //     if(tsize == 4) {
                        //         emit3(OPCODE_3_MOVSS_RM_REG);
                        //         emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(temp), X64_REG_SP);
                        //         emit1((u8)-8);
                                
                        //         emit1(OPCODE_MOV_REG_RM);
                        //         emit_modrm(MODE_DEREF_DISP8, CLAMP_EXT_REG(origin_reg), X64_REG_SP);
                        //         emit1((u8)-8);
                        //     } else if(tsize == 8) {
                        //         emit3(OPCODE_3_MOVSD_RM_REG);
                        //         emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(temp), X64_REG_SP);
                        //         emit1((u8)-8);

                        //         emit1(PREFIX_REXW);
                        //         emit1(OPCODE_MOV_REG_RM);
                        //         emit_modrm(MODE_DEREF_DISP8, CLAMP_EXT_REG(origin_reg), X64_REG_SP);
                        //         emit1((u8)-8);
                        //     }
                        // }
                    } break;
                    default: Assert(("Cast type not implemented in x64 backend, Compiler bug",false));
                }
                // Assert here to make sure that the cast produces an instruction
                // that moves the value from one register to another. There was a bug where this didn't happen.
                Assert(to_reg == from_reg || (code_size() > pc_before));
                
                FIX_POST_OUT_OPERAND(0)
                FIX_POST_IN_OPERAND(1)
            } break;
            case BC_MEMZERO: {
                auto base = (InstBase_op2_imm8*)n->base;
                // which order?
                FIX_PRE_IN_OPERAND(1)
                FIX_PRE_IN_OPERAND(0)

                // ##########################
                //   MEMZERO implementation
                // ##########################
                {
                    u8 batch_size = base->imm8;
                    if(batch_size == 0)
                        batch_size = 1;

                    X64Register reg_cur = reg0->reg;
                    X64Register reg_fin = reg1->reg;

                    u8 prefix = PREFIX_REXW;
                    emit_prefix(PREFIX_REXW, reg_cur, reg_fin);
                    emit1(OPCODE_ADD_RM_REG);
                    emit_modrm(MODE_REG, CLAMP_EXT_REG(reg_cur), CLAMP_EXT_REG(reg_fin));

                    int offset_loop = code_size();

                    emit_prefix(PREFIX_REXW, reg_fin, reg_cur);
                    emit1(OPCODE_CMP_REG_RM);
                    emit_modrm(MODE_REG, CLAMP_EXT_REG(reg_fin), CLAMP_EXT_REG(reg_cur));

                    emit1(OPCODE_JE_IMM8);
                    int offset_jmp_imm = code_size();
                    emit1((u8)0);
                    

                    switch(batch_size) {
                    case 0:
                    case 1:
                        emit_prefix(0, X64_REG_INVALID, reg_cur);
                        emit1(OPCODE_MOV_RM8_IMM8_SLASH_0);
                        break;
                    case 2:
                        emit1(PREFIX_16BIT);
                        emit_prefix(0, X64_REG_INVALID, reg_cur);
                        emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                        break;
                    case 4:
                        emit_prefix(0, X64_REG_INVALID, reg_cur);
                        emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                        break;
                    case 8:
                        emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg_cur);
                        emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                        break;
                    default: Assert(false);
                    }

                    emit_modrm_slash(MODE_DEREF, 0, CLAMP_EXT_REG(reg_cur));

                    switch(batch_size) {
                    case 0:
                    case 1:
                        emit1((u8)0);
                        break;
                    case 2:
                        emit2((u16)0); // note the 16-bit integer here, NOT 32-bit
                        break;
                    case 4:
                        emit4((u32)0);
                        break;
                    case 8:
                        emit4((u32)0);
                        break;
                    default: Assert(false);
                    }

                    emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg_cur);
                    emit1(OPCODE_ADD_RM_IMM8_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg_cur));
                    emit1((u8)batch_size);

                    emit1(OPCODE_JMP_IMM8);
                    emit1((u8)(offset_loop - (code_size()+1)));

                    fix_jump_here_imm8(offset_jmp_imm);
                }
                FIX_POST_IN_OPERAND(0)
                FIX_POST_IN_OPERAND(1)
            } break;
            case BC_MEMCPY: {
                auto base = (InstBase_op3*)n->base;
                // which order?
                FIX_PRE_IN_OPERAND(2)
                FIX_PRE_IN_OPERAND(1)
                FIX_PRE_IN_OPERAND(0)
                
                // ##########################
                //   MEMCPY implementation
                // ##########################
                {
                    /*
                    mov rcx, 4
                    add rcx, rsi
                    loop_:
                    cmp rsi, rcx
                    je loop_end
                    mov al, BYTE PTR [rsi]
                    mov BYTE PTR [rdi], al
                    inc rdi
                    inc rsi
                    jmp loop_
                    loop_end:
                    */

                    X64Register dst = reg0->reg;
                    X64Register src = reg1->reg;
                    X64Register fin = reg2->reg;
                    X64Register tmp = X64_REG_A;

                    emit_prefix(PREFIX_REXW, fin, dst);
                    emit1(OPCODE_ADD_REG_RM);
                    emit_modrm(MODE_REG, CLAMP_EXT_REG(fin), CLAMP_EXT_REG(dst));

                    int offset_loop = code_size();

                    emit_prefix(PREFIX_REXW, fin, dst);
                    emit1(OPCODE_CMP_REG_RM);
                    emit_modrm(MODE_REG, CLAMP_EXT_REG(dst), CLAMP_EXT_REG(fin));

                    emit1(OPCODE_JE_IMM8);
                    int offset_jmp_imm = code_size();
                    emit1((u8)0);

                    int batch_size = 1;
                    InstructionControl control = CONTROL_8B;
                    switch(batch_size) {
                        case 1: control = CONTROL_8B; break;
                        case 2: control = CONTROL_16B; break;
                        case 4: control = CONTROL_32B; break;
                        case 8: control = CONTROL_64B; break;
                        default: Assert(false);
                    }
                    emit_mov_reg_mem(tmp, src, control, 0);
                    emit_mov_mem_reg(dst, tmp, control, 0);

                    if(batch_size == 1) {
                        emit_prefix(PREFIX_REXW, X64_REG_INVALID, dst);
                        emit1(OPCODE_INC_RM_SLASH_0);
                        emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(dst));

                        emit_prefix(PREFIX_REXW, X64_REG_INVALID, src);
                        emit1(OPCODE_INC_RM_SLASH_0);
                        emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(src));
                    } else {
                        emit_add_imm32(dst, batch_size);
                        emit_add_imm32(src, batch_size);
                    }

                    emit1(OPCODE_JMP_IMM8);
                    emit1((u8)(offset_loop - (code_size()+1)));

                    fix_jump_here_imm8(offset_jmp_imm);
                }
                FIX_POST_IN_OPERAND(0)
                FIX_POST_IN_OPERAND(1)
                FIX_POST_IN_OPERAND(2)
            } break;
            case BC_STRLEN: {
                auto base = (InstBase_op2*)n->base;
                FIX_PRE_IN_OPERAND(1)
                FIX_PRE_OUT_OPERAND(0)

                /*
                mov rcx, 0
                cmp rsi, 0
                je loop_end

                loop_:
                mov al, BYTE PTR[rsi]
                cmp al, 0
                je loop_end
                inc ecx
                inc rsi
                jmp loop_
                loop_end:
                */
            
                X64Register counter = reg0->reg;
                X64Register src     = reg1->reg;
                X64Register tmp     = X64_REG_A;

                emit_prefix(PREFIX_REXW, counter, counter);
                emit1(OPCODE_XOR_REG_RM);
                emit_modrm(MODE_REG, CLAMP_EXT_REG(counter), CLAMP_EXT_REG(counter));

                emit_prefix(PREFIX_REXW, tmp, tmp);
                emit1(OPCODE_XOR_REG_RM);
                emit_modrm(MODE_REG, CLAMP_EXT_REG(tmp), CLAMP_EXT_REG(tmp));

                emit_prefix(PREFIX_REXW, X64_REG_INVALID, src);
                emit1(OPCODE_CMP_RM_IMM8_SLASH_7);
                emit_modrm_slash(MODE_REG, 7, CLAMP_EXT_REG(src));
                emit1((u8)0);

                emit1(OPCODE_JE_IMM8);
                int offset_jmp_null = code_size();
                emit1((u8)0);

                int offset_loop = code_size();
                
                emit_mov_reg_mem(tmp, src, CONTROL_8B, 0);

                emit_prefix(PREFIX_REXW, X64_REG_INVALID, tmp);
                emit1(OPCODE_CMP_RM_IMM8_SLASH_7);
                emit_modrm_slash(MODE_REG, 7, CLAMP_EXT_REG(tmp));
                emit1((u8)0);

                emit1(OPCODE_JE_IMM8);
                int offset_jmp = code_size();
                emit1((u8)0);

                emit_prefix(PREFIX_REXW, X64_REG_INVALID, counter);
                emit1(OPCODE_INC_RM_SLASH_0);
                emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(counter));
                
                emit_prefix(PREFIX_REXW, X64_REG_INVALID, src);
                emit1(OPCODE_INC_RM_SLASH_0);
                emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(src));

                emit1(OPCODE_JMP_IMM8);
                emit_jmp_imm8(offset_loop);

                fix_jump_here_imm8(offset_jmp_null);
                fix_jump_here_imm8(offset_jmp);
            
                FIX_POST_OUT_OPERAND(0)
                FIX_POST_IN_OPERAND(1)
            } break;
            case BC_RDTSC: {
                auto reg0 = get_artifical_reg(n->reg0);
                if(is_register_free(X64_REG_D)) {
                    reg0->reg = alloc_register(X64_REG_D);
                } else {
                    reg0->reg = alloc_register();
                    emit_push(X64_REG_D);
                }

                // TODO: We overwrite register A, is that okay?
                Assert(is_register_free(X64_REG_A));
                emit2(OPCODE_2_RDTSC); // sets edx (32-64) and eax (0-32), upper 32-bits of rdx and rax are cleared

                emit1(PREFIX_REXW);
                emit1(OPCODE_SHL_RM_IMM8_SLASH_4);
                emit_modrm_slash(MODE_REG, 4, X64_REG_D);
                emit1((u8)32);

                if(reg0->reg == X64_REG_D) {
                    emit1(PREFIX_REXW);
                    emit1(OPCODE_OR_REG_RM);
                    emit_modrm(MODE_REG, X64_REG_A, X64_REG_D);
                } else {
                    emit1(PREFIX_REXW);
                    emit1(OPCODE_OR_REG_RM);
                    emit_modrm(MODE_REG, X64_REG_D, X64_REG_A);
                    emit_pop(X64_REG_D);
                    
                    emit_mov_reg_reg(reg0->reg, X64_REG_A);
                }
            } break;
            case BC_ATOMIC_ADD: {
                auto base = (InstBase_op2_ctrl*)n->base;
                FIX_PRE_IN_OPERAND(1)
                FIX_PRE_IN_OPERAND(0)

                int prefix = 0;
                if(GET_CONTROL_SIZE(base->control) == CONTROL_32B) {

                } else if(GET_CONTROL_SIZE(base->control) == CONTROL_64B)
                    prefix |= PREFIX_REXW;
                else Assert(false);
                
                emit1(PREFIX_LOCK);
                emit_prefix(prefix, reg0->reg, reg1->reg);
                emit1(OPCODE_ADD_RM_REG);
                emit_modrm(MODE_DEREF, CLAMP_EXT_REG(reg0->reg), CLAMP_EXT_REG(reg1->reg));

                FIX_POST_IN_OPERAND(0)
                FIX_POST_IN_OPERAND(1)
            } break;
            case BC_ATOMIC_CMP_SWAP: {
                FIX_PRE_IN_OPERAND(2)
                FIX_PRE_IN_OPERAND(1)
                FIX_PRE_IN_OPERAND(0)

                // TODO: Add 64-bit version
                // int prefix = 0;
                // if(GET_CONTROL_SIZE(n->control) == CONTROL_32B) {

                // } else if(GET_CONTROL_SIZE(n->control) == CONTROL_64B)
                //     prefix |= PREFIX_REXW;
                // else Assert(false);

                X64Register reg_new = reg2->reg;
                X64Register old =     reg1->reg;
                X64Register ptr =     reg0->reg;

                emit_mov_reg_reg(X64_REG_A, old);
                
                emit1(PREFIX_LOCK);
                emit_prefix(0, reg_new, ptr);
                emit2(OPCODE_2_CMPXCHG_RM_REG);
                emit_modrm(MODE_DEREF, CLAMP_EXT_REG(reg_new), CLAMP_EXT_REG(ptr));
                // RAX will be set to the original value in ptr

                emit_mov_reg_reg(ptr, X64_REG_A);

                FIX_POST_IN_OPERAND(0)
                FIX_POST_IN_OPERAND(1)
                FIX_POST_IN_OPERAND(2)
            } break;
            case BC_ROUND: {
                FIX_PRE_IN_OPERAND(0)

                X64Register reg = reg0->reg;

                emit4(OPCODE_4_ROUNDSS_REG_RM_IMM8);
                emit_modrm(MODE_REG, CLAMP_XMM(reg), CLAMP_XMM(reg));
                emit1((u8)0); // immediate describes rounding mode. 0 means "round to" which I believe will
                // choose the number that is closest to the actual number. There where round toward, up, and toward as well (round to is probably a good default).

                FIX_POST_IN_OPERAND(0)
            } break;
            case BC_SQRT: {
                FIX_PRE_IN_OPERAND(0)

                X64Register reg = reg0->reg;

                emit3(OPCODE_3_SQRTSS_REG_RM);
                emit_modrm(MODE_REG, CLAMP_XMM(reg), CLAMP_XMM(reg));

                FIX_POST_IN_OPERAND(0)
            } break;
            case BC_ASM: {
                InstBase_imm32_imm8_2* base = (InstBase_imm32_imm8_2*)n->base;
                
                if(push_offsets.size())
                    push_offsets.last() += 8 * (-base->imm8_0 + base->imm8_1);
                
                virtual_stack_pointer += (base->imm8_0 - base->imm8_1) * 8; // inputs - outputs
                
                Bytecode::ASM& asmInstance = bytecode->asmInstances.get(base->imm32);
                Assert(asmInstance.generated);
                u32 len = asmInstance.iEnd - asmInstance.iStart;
                if(len != 0) {
                    u8* ptr = bytecode->rawInstructions._ptr + asmInstance.iStart;
                    int pc_start = code_size();
                    emit_bytes(ptr, len);
                    for (int i = 0; i < asmInstance.relocations.size();i++) {
                      auto& it = asmInstance.relocations[i];
                      prog->addNamedUndefinedRelocation(it.name, pc_start + it.textOffset, tinycode->index);
                    }
                } else {
                    // TODO: Better error, or handle error somewhere else?
                    log::out << log::RED << "BC_ASM at "<<n->bc_index<<" was incomplete\n";
                }
                
            } break;
            case BC_TEST_VALUE: {
                #ifdef DISABLE_BC_TEST_VALUE
                FIX_POST_IN_OPERAND(0)
                FIX_POST_IN_OPERAND(1)
                continue;
                #endif

                /* Problem bug
                    We reserve general registers
                        because have static bytes in an array which use those registers
                    We evaluate operands, nodes

                    If we evaluate nodes which want xmm registers we got a problem
                        the nodes should use xmm and ignore what the parent node suggested
                        when the nodes return value, they should provide them in general instead of xmm
                    
                */

                FIX_PRE_IN_OPERAND(0)
                FIX_PRE_IN_OPERAND(1)

                auto base = (InstBase_op2_ctrl_imm32*)n->base;

                bool pushed_di = false;
                bool pushed_si = false;
                if(!is_register_free(X64_REG_SI)) {
                    pushed_si = true;
                    emit_push(X64_REG_SI);
                }
                if(!is_register_free(X64_REG_DI)) {
                    pushed_di = true;
                    emit_push(X64_REG_DI);
                }

                emit_mov_reg_reg(RESERVED_REG0, reg0->reg, reg0->size);
                emit_movzx(RESERVED_REG0, RESERVED_REG0, base->control);
                emit_mov_reg_reg(RESERVED_REG1, reg1->reg, reg1->size);
                emit_movzx(RESERVED_REG1, RESERVED_REG1, base->control);

                push_alignment();

                #ifdef OS_WINDOWS
                /*
                sub rsp, 8
                mov rbx, rsp
                sub rsp, 0x28
                mov DWORD PTR [rbx], 0x99999978
                cmp rsi, rdi
                je hop
                mov BYTE PTR [rbx], 0x5f
                hop:
                mov    ecx,0xfffffff4
                call   QWORD PTR [rip+0x0]
                mov    rcx,rax
                mov    rdx,rbx
                mov    r8,0x4
                xor    r9,r9
                mov    QWORD PTR [rsp+0x20],0x0
                call   QWORD PTR [rip+0x0]    
                add    rsp, 0x30

                0:  48 83 ec 08             sub    rsp,0x8
                4:  48 89 e3                mov    rbx,rsp
                7:  48 83 ec 28             sub    rsp,0x28
                b:  c7 03 5f 99 99 99       mov    DWORD PTR [rbx],0x9999995f
                11: 48 39 fe                cmp    rsi,rdi
                14: 74 03                   je     19 <hop>
                16: c6 03 78                mov    BYTE PTR [rbx],0x78
                0000000000000019 <hop>:
                19: b9 f4 ff ff ff          mov    ecx,0xfffffff4
                1e: ff 15 00 00 00 00       call   QWORD PTR [rip+0x0]        # 24 <hop+0xb>
                24: 48 89 c1                mov    rcx,rax
                27: 48 89 da                mov    rdx,rbx
                2a: 49 c7 c0 04 00 00 00    mov    r8,0x4
                31: 4d 31 c9                xor    r9,r9
                34: 48 c7 44 24 20 00 00    mov    QWORD PTR [rsp+0x20],0x0
                3b: 00 00
                3d: ff 15 00 00 00 00       call   QWORD PTR [rip+0x0]        # 43 <hop+0x2a>
                43: 48 83 c4 30             add    rsp,0x30
                */

                // TODO: Subroutine/function for tests.
                //   100 test instructions would generate 4700 bytes of machine code.
                //   With a subroutine you would get 100*6 (call) + 48 (subroutine) bytes.
                //   A subroutine scales much better.

                int start_addr = code_size();
                const u8 arr[] { 0x48, 0x83, 0xEC, 0x08, 0x48, 0x89, 0xE3, 0x48, 0x83, 0xEC, 0x28, 0xC7, 0x03, 0x78, 0x99, 0x99, 0x99, 0x48, 0x39, 0xFE, 0x74, 0x03, 0xC6, 0x03, 0x5f, 0xB9, 0xF4, 0xFF, 0xFF, 0xFF, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0x89, 0xC1, 0x48, 0x89, 0xDA, 0x49, 0xC7, 0xC0, 0x04, 0x00, 0x00, 0x00, 0x4D, 0x31, 0xC9, 0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x30 };
                emit_bytes(arr, sizeof(arr));

                int imm = base->imm32;
                Assert((imm & ~0xFFFF) == 0);
                set_imm8(start_addr + 0xe, imm&0xFF);
                set_imm8(start_addr + 0xf, (imm>>8)&0xFF);
                set_imm8(start_addr + 0x10, (imm>>16)&0xFF);

                prog->addNamedUndefinedRelocation("__imp_GetStdHandle", start_addr + 0x20, current_tinyprog_index);
                prog->addNamedUndefinedRelocation("__imp_WriteFile", start_addr + 0x3F, current_tinyprog_index);
                #else
                Assert(false);
                // TODO: FIX, look at windows version, I believe the assembly below is wrong.
                /*
                sub    rsp,0x10  # must be 16-byte aligned when calling unix write
                mov    rbx,rsp
                mov    DWORD PTR [rbx],0x99993a57 # temporary data
                cmp    rdx,rax
                je     hop
                mov    BYTE PTR [rbx],0x78 # err character
                hop:
                mov    rdx,0x4 # buffer size
                mov    rsi,rbx # buffer ptr
                mov    rdi,0x2 # stderr
                call   0
                add    rsp,0x10
                */


                u8 arr[]={
                    0x48, 0x83, 0xEC, 0x10, 0x48, 0x89, 0xE3, 0xC7,
                    0x03, 0x57, 0x3A, 0x99, 0x99, 0x48, 0x39, 0xC2,
                    0x74, 0x03, 0xC6, 0x03, 0x78, 0x48, 0xC7, 0xC2, 
                    0x04, 0x00, 0x00, 0x00, 0x48, 0x89, 0xDE, 0x48, 
                    0xC7, 0xC7, 0x02, 0x00, 0x00, 0x00, 0xE8, 0x00, 
                    0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x10 
                };
                Program_x64::NamedUndefinedRelocation reloc0{};
                reloc0.name = "write"; // symbol name, gcc (or other linker) knows how to relocate it
                reloc0.textOffset = prog->size() + 0x27;
                prog->namedUndefinedRelocations.add(reloc0);
                arr[0x0B] = (imm>>8)&0xFF; // set location info
                arr[0x0C] = imm&0xFF;
                prog->addRaw(arr,sizeof(arr));

                #endif

                pop_alignment();

                if(pushed_di) {
                    emit_pop(X64_REG_DI);
                }
                if(pushed_si) {
                    emit_pop(X64_REG_SI);
                }
                
                FIX_POST_IN_OPERAND(1)
                FIX_POST_IN_OPERAND(0)
            } break;
        
            default: Assert(false);
        }
    }

    // TODO: If you have many relocations you can use process them using multiple threads.
    //  It can't be too few because the overhead of managing the threads would be worse
    //  than the performance gain.
    for(int i=0;i<relativeRelocations.size();i++) {
        auto& rel = relativeRelocations[i];
        if(rel.bc_addr == tinycode->size())
            set_imm32(rel.imm32_addr, code_size() - rel.inst_addr);
        else {
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
                    log::out << log::RED << "Bug in compiler when translating BC to x64 addresses. This x64 instruction "<<log::GREEN<<NumberToHex(rel.inst_addr,true) <<log::RED<< " wanted to refer to "<<log::GREEN<<rel.bc_addr<<log::RED<<" (BC address) but ther was no mapping of the BC address."<<"\n";
                // }
            }
            
            Assert(rel.bc_addr>=0); // native functions not implemented
            int addr_of_next_inst = rel.imm32_addr + 4;
            i32 value = get_map_translation(rel.bc_addr) - addr_of_next_inst;
            set_imm32(rel.imm32_addr, value);
            // If you are wondering why the relocation is zero then it is because it
            // is relative and will jump to the next instruction. This isn't necessary so
            // they should be optimised away. Don't do this now since it makes the conversion
            // confusing.
        }
    }
    
    // last instruction should be ret
    Assert(tinyprog->text[tinyprog->head-1] == OPCODE_RET);
    
    for(int i=0;i<tinycode->call_relocations.size();i++) {
        auto& r = tinycode->call_relocations[i];
        if(r.funcImpl->astFunction->linkConvention == NATIVE)
            continue;
        int ind = r.funcImpl->tinycode_id - 1;
        // log::out << r.funcImpl->astFunction->name<<" pc: "<<r.pc<<" codeid: "<<ind<<"\n";
        prog->addInternalFuncRelocation(current_tinyprog_index, get_map_translation(r.pc), ind);
    }

    
    // TODO: Optimize, store external relocations per tinycode instead
    // bool found = bytecode->externalRelocations.size() == 0;
    for(int i=0;i<bytecode->externalRelocations.size();i++) {
        auto& rel = bytecode->externalRelocations[i];
        if(tinycode->index == rel.tinycode_index) {
            int off = get_map_translation(rel.pc);
            prog->addNamedUndefinedRelocation(rel.name, off, rel.tinycode_index, rel.library_path);
            // found = true;
            // break;
        }
    }
    // Assert(found);

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
    
    for(auto i : inst_list) {
        delete i;
    }
    inst_list.cleanup();
    return true;
}

engone::Logger& operator<<(engone::Logger& l, X64Inst& i) {
    
    l << engone::log::PURPLE << i.base->opcode << engone::log::NO_COLOR;
    if(i.reg0 != 0)
        l << " " << i.reg0;
    if(i.reg1 != 0)
        l << ", " << i.reg1;
    if(i.reg2 != 0)
        l << ", " << i.reg2;
    
    int flags = instruction_contents[i.base->opcode];
    int offset = 1 + ((flags & BASE_op1) != 0) + ((flags & BASE_op2) != 0) + ((flags & BASE_op3) != 0)
        + ((flags & BASE_ctrl) != 0) + ((flags & BASE_link) != 0)
        + ((flags & BASE_call) != 0);
    u8* ptr = (u8*)i.base + offset;
    if(flags & BASE_imm8) {
        auto imm = *(i8*)ptr;
        l << ", " << engone::log::GREEN<< imm << engone::log::NO_COLOR;
    } else if(flags & BASE_imm16) {
        auto imm = *(i16*)ptr;
        l << ", " << engone::log::GREEN<< imm << engone::log::NO_COLOR;
    } else if(flags & BASE_imm32) {
        auto imm = *(i32*)ptr;
        l << ", " << engone::log::GREEN<< imm << engone::log::NO_COLOR;
    } else if(flags & BASE_imm64) {
        auto imm = *(i64*)ptr;
        l << ", " << engone::log::GREEN<< imm << engone::log::NO_COLOR;
    }

    return l;
}

bool X64Builder::prepare_assembly(Bytecode::ASM& asmInst) {
    using namespace engone;
    
    #define SEND_ERROR() compiler->options->compileStats.errors++;
    
    // TODO: Make this thread safe
    std::string asm_file = "bin/inline_asm"+std::to_string(tinycode->index)+".asm";
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
    
    if(compiler->options->linker == LINKER_MSVC) {
        pretext = ".code\n";
        posttext = "END\n";
        asm_line_offset = 1;
        cmd = "ml64 /nologo /Fo " + obj_file + " /c ";
    } else {
        pretext = ".intel_syntax noprefix\n.text\n";
        posttext = "\n"; // assembler complains about not having a newline so we add one here
        asm_line_offset = 2;
        cmd = "as -o " + obj_file + " ";
    }
    cmd += asm_file;
    
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
    yes = engone::StartProgram((char*)cmd.data(), PROGRAM_WAIT, &exitCode, {}, asmLog, asmLog);
    Assert(yes);
    if(exitCode != 0) {
        u64 fileSize = engone::FileGetSize(asmLog);
        engone::FileSetHead(asmLog, 0);
        
        QuickArray<char> errorMessages{};
        errorMessages.resize(fileSize);
        
        yes = engone::FileRead(asmLog, errorMessages.data(), errorMessages.size());
        Assert(yes);
        
        // log::out << std::string(errorMessages.data(), errorMessages.size());
        ReformatAssemblerError(compiler->options->linker, asmInst, errorMessages, -asm_line_offset);
        SEND_ERROR();
        return false;
    }
    engone::FileClose(asmLog);
    asmLog = {};

    // TODO: DeconstructFile isn't optimized and we deconstruct symbols and segments we don't care about.
    //  Write a specific function for just the text segment.
    if(compiler->options->target == TARGET_WINDOWS_x64) {
        auto objfile = FileCOFF::DeconstructFile(obj_file);
        defer { FileCOFF::Destroy(objfile); };
        if(!objfile) {
            log::out << log::RED << "Could not find " << obj_file << "!\n";
            SEND_ERROR();
            return false;
        }
        int sectionIndex = -1;
        for(int j=0;j<objfile->sections.size();j++){
            std::string name = objfile->getSectionName(j);
            if(name == ".text" || (name.length()>=6 && !strncmp(name.c_str(), ".text$",6))) {
                // auto section = objfile->sections[j];
                // Assert(section->NumberOfRelocations == 0); // not handled at the moment
                sectionIndex = j;
                // u8* ptr = objfile->_rawFileData + section->PointerToRawData;
                // u32 len = section->SizeOfRawData;
            }
        }
        if(sectionIndex == -1){
            log::out << log::RED << "Text section in object file could not be found for the compiled inline assembly. Compiler bug?\n";
            SEND_ERROR();
            return false;
        }
        auto section = objfile->sections[sectionIndex];
        // coff::Symbol_Record* text_symbol=nullptr;
        // for(int j=0;j<objfile->symbols.size();j++){
        //     std::string name = objfile->getSymbolName(j);
        //     if(name == ".text" || (name.length()>=6 && !strncmp(name.c_str(), ".text$",6))) {
        //         // auto section = objfile->sections[j];
        //         // Assert(section->NumberOfRelocations == 0); // not handled at the moment
        //         sectionIndex = j;
        //         // u8* ptr = objfile->_rawFileData + section->PointerToRawData;
        //         // u32 len = section->SizeOfRawData;
        //     }
        // }
        // objfile->findSymbolName("")
        // if(section->NumberOfRelocations!=0){
        //     // TODO: Tell the user where the relocation was? Which instruction?
        //     //  user may type 'pop rac' instead of 'pop rax'. pop rac is a valid instruction if rac is a symbol.
        //     //  This mistake is difficult to detect.
        //     log::out << log::RED << "Relocations are not supported when using inline assembly.\n";
        //     SEND_ERROR();
        //     return false;
        // }
        if (section->NumberOfRelocations != 0) {
            Assert(objfile->text_relocations.size() == section->NumberOfRelocations);

            for (int i = 0; i < objfile->text_relocations.size();i++) {
                auto it = objfile->text_relocations[i];
                switch(it->Type) {
                    case coff::IMAGE_REL_AMD64_REL32: {
                        // ok
                    } break;
                    default: {
                        // TODO: Handle other relocations. What kinds though?
                        log::out << "Inline assembly used this kind of relocation: " << it->Type << " which isn't handled\n";
                        SEND_ERROR();
                        return false;
                    }
                }

                auto symbol = objfile->symbols[it->SymbolTableIndex];
                std::string fn_name = objfile->getSymbolName(it->SymbolTableIndex);

                Bytecode::ASM::ExternalNamedReloc rel{};
                rel.name = fn_name;
                rel.textOffset = it->VirtualAddress;
                asmInst.relocations.add(rel);
            }   
        }

        u8* textData = objfile->_rawFileData + section->PointerToRawData;
        i32 textSize = section->SizeOfRawData;
        // log::out << "Inline assembly "<<i << " size: "<<len<<"\n";
        asmInst.iStart = bytecode->rawInstructions.used;
        bytecode->rawInstructions._reserve(bytecode->rawInstructions.used + textSize);
        memcpy(bytecode->rawInstructions._ptr + bytecode->rawInstructions.used, textData, textSize);
        bytecode->rawInstructions.used += textSize;
        asmInst.iEnd = bytecode->rawInstructions.used;
    } else {
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
        auto relocations = objfile->relaOfSection(textSection_index, &relocations_count);
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
        bytecode->rawInstructions._reserve(bytecode->rawInstructions.used + textSize);
        memcpy(bytecode->rawInstructions._ptr + bytecode->rawInstructions.used, textData, textSize);
        bytecode->rawInstructions.used += textSize;
        asmInst.iEnd = bytecode->rawInstructions.used;
    }
    return true;
}