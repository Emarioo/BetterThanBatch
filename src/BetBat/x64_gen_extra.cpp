/*
    Experimental x64 generator
    (well, the whole compiler is experimental)
*/

#include "BetBat/x64_gen.h"
#include "BetBat/x64_defs.h"

#include "BetBat/CompilerEnums.h"
#include "BetBat/Compiler.h"

void X64Builder::generateFromTinycode_v2(Bytecode* code, TinyBytecode* tinycode) {
    using namespace engone;
    log::out <<log::GOLD<< "RUNNING EXPERIMENTAL X64 GEN.\n";
    this->bytecode = bytecode;
    this->tinycode = tinycode;
    
    // NOTE: The generator makes assumptions about the bytecode.
    //  - alloc_local isn't called iteratively unless it's scoped
    //  - registers aren't saved between calls and jumps
    
    

    bool logging = false;

    // #define CREATE_INST() auto n = createInst(opcode);\
    //         n->op0 = op0;\
    //         n->op1 = op1;\
    //         n->op2 = op2;\
    //         n->imm = imm;\
    //         n->cast = cast;\
    //         n->control=control;\
    //         insert_inst(n);

    // #define GET_INST(TYPE) (TYPE*)&instructions[pc]; pc += sizeof(TYPE);

    
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
        // i64 inst_index = next_inst_index;
        
        InstructionOpcode opcode = BC_HALT;
        // BCRegister op0=BC_REG_INVALID, op1=BC_REG_INVALID, op2=BC_REG_INVALID;
        // InstructionControl control = CONTROL_NONE;
        // InstructionCast cast=CAST_UINT_UINT;
        // i64 imm = 0;

        opcode = (InstructionOpcode)instructions[pc];
        pc++;
        
        // _ILOG(log::out <<log::GRAY<<" sp: "<< sp <<" fp: "<<fp<<"\n";)
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
        if(flags & BASE_cast) pc++;
        if(flags & BASE_link) pc++;
        if(flags & BASE_call) pc++;
        if(flags & BASE_imm8) pc++;
        if(flags & BASE_imm16) pc+=2;
        if(flags & BASE_imm32) pc+=4;
        if(flags & BASE_imm64) pc+=8;
        insert_inst(n);
        
        if(opcode == BC_GET_PARAM) {
            auto base = (InstBase_op1_ctrl_imm16*)n->base;
            int param_index = base->imm16 / 8;
            if(param_index >= accessed_params.size()) {
                accessed_params.resize(param_index+1);
            }
            accessed_params[param_index].control = base->control;
        }
    }
#pragma region
    //     switch (opcode) {
    //     case BC_HALT: {
            
    //     } break;
    //     case BC_NOP: {
    //         Assert(("NOP not implemnted",false));
    //     } break;
    //     case BC_MOV_RR: {
    //         // auto inst = GET_INST(InstBase_op2);
    //         op0 = (BCRegister)instructions[pc++];
    //         op1 = (BCRegister)instructions[pc++];
            
    //         CREATE_INST();

    //     } break;
    //     case BC_MOV_RM:
    //     case BC_MOV_RM_DISP16: {
    //         op0 = (BCRegister)instructions[pc++];
    //         op1 = (BCRegister)instructions[pc++];
    //         control = (InstructionControl)instructions[pc++];
    //         if(opcode == BC_MOV_RM_DISP16) {
    //             imm = *(i16*)&instructions[pc];
    //             pc += 2;
    //         }
            
    //         CREATE_INST();
            
    //     } break;
    //     case BC_MOV_MR:
    //     case BC_MOV_MR_DISP16: {
    //         op0 = (BCRegister)instructions[pc++];
    //         op1 = (BCRegister)instructions[pc++];
    //         control = (InstructionControl)instructions[pc++];
    //         if(opcode == BC_MOV_MR_DISP16) {
    //             imm = *(i16*)&instructions[pc];
    //             pc += 2;
    //         }
            
    //         CREATE_INST();

    //     } break;
    //     case BC_ALLOC_LOCAL: {
    //         op0 = (BCRegister)instructions[pc++];
    //         imm = *(i16*)&instructions[pc];
    //         pc+=2;

    //         CREATE_INST();
            
    //     } break;
    //     case BC_FREE_LOCAL: {
    //         imm = *(i16*)&instructions[pc];
    //         pc+=2;
            
    //         CREATE_INST();

    //     } break;
    //     case BC_PUSH: {
    //         op0 = (BCRegister)instructions[pc++];
            
    //         CREATE_INST();
    //     } break;
    //     case BC_POP: {
    //         op0 = (BCRegister)instructions[pc++];
            
    //         CREATE_INST();
    //     } break;
    //     case BC_LI32: {
    //         op0 = (BCRegister)instructions[pc++];
    //         imm = *(i32*)&instructions[pc];
    //         pc+=4;
            
    //         CREATE_INST();
    //     } break;
    //     case BC_INCR: {
    //         op0 = (BCRegister)instructions[pc++];
    //         imm = *(i32*)&instructions[pc];
    //         pc+=4;
            
    //         CREATE_INST();
    //     } break;
    //     case BC_ADD: {
    //         op0 = (BCRegister)instructions[pc++];
    //         op1 = (BCRegister)instructions[pc++];
    //         control = (InstructionControl)instructions[pc++];
            
    //         CREATE_INST();
    //     } break;
    //     case BC_JMP: {
    //         imm = *(i32*)&instructions[pc];
    //         pc+=4;
              
    //         CREATE_INST();
    //     } break;
    //     case BC_JZ: {
    //         op0 = (BCRegister)instructions[pc++];
    //         imm = *(i32*)&instructions[pc];
    //         pc+=4;
            
    //         CREATE_INST();
    //     } break;
    //     case BC_RET: {
    //         CREATE_INST();
    //     } break;
    //     case BC_CALL: {
    //         LinkConventions link = (LinkConventions)instructions[pc++];
    //         CallConventions call = (CallConventions)instructions[pc++];
    //         imm = *(i32*)&instructions[pc];
    //         pc+=4;
            
    //         CREATE_INST();
    //         n->link = link;
    //         n->call = call;
    //     } break;
    //     case BC_SET_ARG: {
    //         op0 = (BCRegister)instructions[pc++];
    //         control = (InstructionControl)instructions[pc++];
    //         imm = *(i16*)&instructions[pc];
    //         pc+=2;
            
    //         CREATE_INST();
    //     } break;
    //     case BC_GET_VAL: {
    //         op0 = (BCRegister)instructions[pc++];
    //         control = (InstructionControl)instructions[pc++];
    //         imm = *(i16*)&instructions[pc];
    //         pc+=2;
            
    //         CREATE_INST();
    //     } break;
    //     default: Assert(("instruction not handled",false));
    //     }
    // }
#pragma endregion

    auto IS_ESSENTIAL=[&](InstructionOpcode t) {
        return t == BC_RET
        || t == BC_JZ
        || t == BC_MOV_MR
        || t == BC_MOV_MR_DISP16
        || t == BC_SET_ARG
        || t == BC_CALL
        // temporary
        // || t == BC_POP
        || t == BC_PUSH
        ;
    };

    auto IS_GENERAL_REG=[&](BCRegister r) {
        return r != BC_REG_INVALID && r != BC_REG_LOCALS;
    };

    auto ALLOC_REG = [&](X64Inst* n, int nr) {
        InstBase_op3* base = (InstBase_op3*)n->base;
        
        Assert(nr != 0 || (instruction_contents[base->opcode] & BASE_op1));
        Assert(nr != 1 || (instruction_contents[base->opcode] & BASE_op2));
        Assert(nr != 2 || (instruction_contents[base->opcode] & BASE_op3));
        
        X64Operand& op = n->regs[nr];
        BCRegister& r = base->ops[nr];
        Assert(r != BC_REG_LOCALS);
        op.reg = alloc_register();
        if(op.invalid()) {
            op.on_stack = true;
            log::out << " map "<<r<<" -> stack\n";
        } else {
            log::out << " map "<<r<<" -> "<<op.reg<<"\n";
        }
    };

    // bad name, initial register, start of register journey, full register overwrite
    auto IS_START=[&](InstructionOpcode t) {
        return t == BC_MOV_RM
        || t == BC_MOV_RM_DISP16
        || t == BC_LI32
        || t == BC_MOV_RR
        ;
    };
    auto IS_WALL=[&](InstructionOpcode t) {
        return t == BC_JZ
        || t == BC_RET
        ;
    };

    DynamicArray<int> insts_to_delete{};
    for(int i=inst_list.size()-1;i>=0;i--) {
        auto n = inst_list[i];
        log::out <<log::GOLD<< n->base->opcode<<"\n";

        if(n->base->opcode == BC_POP) {
            auto base = (InstBase_op1*)n->base;
            // auto base = (InstBase_op*)n->base;
            auto& v = bc_register_map[base->op0];
            auto recipient = v.used_by;
            auto reg_nr = v.reg_nr;
            if(recipient) {
                n->reg0 = recipient->regs[reg_nr];
                Assert(!n->reg0.invalid());
                bc_push_list.add({});
                bc_push_list.last().used_by = n;
                bc_push_list.last().reg_nr = 0;
            } else {
                insts_to_delete.add(i);
                bc_push_list.add({});
                bc_push_list.last().used_by = nullptr;
            }

            continue;
        }
        if(n->base->opcode == BC_PUSH) {
            // auto base = (InstBase_op1*)n->base;
            auto& v = bc_push_list.last();  // TODO: What if bytecode is messed up and didn't add a pop?
            auto recipient = v.used_by;
            auto reg_nr = v.reg_nr;
            if(recipient) {
                ALLOC_REG(n,0);
                map_reg(n,0);
            } else {
                insts_to_delete.add(i);
            }

            bc_push_list.pop();
            continue;
        }
        if(n->base->opcode == BC_CALL) {
            // TODO: Push list?
            // NOTE: We assume that registers aren't used between calls. The bytecode should have asserts so it doesn't happen.
            clear_register_map();
        }
        if(n->base->opcode == BC_JMP || n->base->opcode == BC_JZ || n->base->opcode == BC_JNZ) {
            Assert(bc_push_list.size() == 0);
            clear_register_map();
            // can we detect whether the register map is empty?
            // assert if so and detect bugs?
            // not sure if the current logic allows that
        }
    
        // TODO: ignore LOCAL register
        // TODO: What if two operands refer to the same register
        if(IS_ESSENTIAL(n->base->opcode)) {
            if(instruction_contents[n->base->opcode] & BASE_op1) {
                auto base = (InstBase_op1*)n->base;
                if(IS_GENERAL_REG(base->op0)) {
                    ALLOC_REG(n,0);
                    map_reg(n,0);
                } else if(base->op0 == BC_REG_LOCALS) {
                    n->reg0.reg = X64_REG_BP;
                }
            }
        } else {
            // if(n->opcode == BC_INCR) {
                
            // } else 
            if(instruction_contents[n->base->opcode] & BASE_op1) {
                auto base = (InstBase_op1*)n->base;
                if(IS_GENERAL_REG(base->op0)) {
                    auto& v = bc_register_map[base->op0];
                    auto recipient = v.used_by;
                    auto reg_nr = v.reg_nr;
                    if(recipient) {
                        n->reg0 = recipient->regs[reg_nr]; map_reg(n,0);
                        Assert(!n->reg0.invalid());

                        if(IS_START(n->base->opcode)) {
                            // TODO: Handle mov_rr, delete it if it isn't necessary
                            free_map_reg(n,0);
                            if(!n->reg0.on_stack)
                                free_register(n->reg0.reg);
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
                    Assert(!n->reg1.invalid());
                    map_reg(n,1);
                } else {
                    ALLOC_REG(n,1);
                    map_reg(n,1);
                }
            } else if(base->op1 == BC_REG_LOCALS) {
                n->reg1.reg = X64_REG_BP;
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
                    Assert(!n->reg2.invalid());
                    map_reg(n,2);
                } else {
                    ALLOC_REG(n,2);
                    map_reg(n,2);
                }
            }
        }
    }

    for(auto i : insts_to_delete) {
        auto n = inst_list[i];
        log::out <<log::RED<<"del "<< *n <<"\n";

        inst_list.removeAt(i);
    }

    for(auto n : inst_list) {
        log::out << *n << "\n";
        log::out << log::GRAY <<" "<< n->base->opcode << " ";
        for(int i=0;i<3;i++){
            if(n->regs[i].invalid()) continue;
            if(i!=0) log::out << ", ";
            if(n->regs[i].on_stack)
                log::out << "op"<<i<<" on stack";
            else
                log::out << n->regs[i].reg;
        }
        log::out << "\n";
    }

    #define FIX_IN_OPERAND(N) if(n->reg##N.on_stack) { n->reg##N.reg = RESERVED_REG##N; emit_pop(n->reg##N.reg); }
    #define FIX_IN_OUT_OPERAND(N) if(n->reg##N.on_stack) { n->reg##N.reg = RESERVED_REG##N; }
    #define FIX_OUT_OPERAND(N) if(n->reg##N.on_stack) { n->reg##N.reg = RESERVED_REG##N; emit_push(n->reg##N.reg); }

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
    // push_offset -= 8; // BP does not count

    emit1(PREFIX_REXW);
    emit1(OPCODE_MOV_REG_RM);
    emit_modrm(MODE_REG, X64_REG_BP, X64_REG_SP);
    
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

    for(auto n : inst_list) {
        auto opcode = n->base->opcode;
        map_translation(n->bc_index, size());
        last_pc = size();
        
        switch(n->base->opcode) {
            case BC_HALT: {
                Assert(("HALT not implemented",false));
            } break;
            case BC_NOP: {
                emit1(OPCODE_NOP);
            } break;
            case BC_MOV_RR: {
                FIX_IN_OPERAND(1)
                FIX_IN_OUT_OPERAND(0)
                emit_mov_reg_reg(n->reg0.reg, n->reg1.reg);
                FIX_OUT_OPERAND(0)
            } break;
            case BC_MOV_RM:
            case BC_MOV_RM_DISP16: {
                auto base = (InstBase_op2_ctrl*)n->base;
                i16 imm = 0;
                if(instruction_contents[opcode] & BASE_imm16) {
                    imm = ((InstBase_op2_ctrl_imm16*)n->base)->imm16;
                }
                FIX_IN_OPERAND(1)
                FIX_IN_OUT_OPERAND(0)
                emit_mov_reg_mem(n->reg0.reg, n->reg1.reg, base->control, imm);
                FIX_OUT_OPERAND(0)
            } break;
            case BC_MOV_MR:
            case BC_MOV_MR_DISP16: {
                auto base = (InstBase_op2_ctrl*)n->base;
                i16 imm = 0;
                if(instruction_contents[opcode] & BASE_imm16) {
                    imm = ((InstBase_op2_ctrl_imm16*)n->base)->imm16;
                }
                // TODO: Which order are the ops push to the stack?
                FIX_IN_OPERAND(1)
                FIX_IN_OPERAND(0)
                emit_mov_mem_reg(n->reg0.reg, n->reg1.reg, base->control, imm);
            } break;
            case BC_PUSH: {
                if(!n->reg0.on_stack) {
                    // don't push if already on stack
                    emit_push(n->reg0.reg);
                }
            } break;
            case BC_POP: {
                if(!n->reg0.on_stack) {
                    // don't pop if it's supposed to be on stack
                    emit_pop(n->reg0.reg);
                }
            } break;
            case BC_LI32: {
                auto base = (InstBase_op1_imm32*)n->base;
                
                FIX_IN_OUT_OPERAND(0)
                
                if(IS_REG_XMM(n->reg0.reg)) {
                    Assert(!n->reg0.on_stack);
                    emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                    emit_modrm_slash(MODE_DEREF_DISP8, 0, X64_REG_SP);
                    emit1((u8)-8);
                    emit4((u32)(i32)base->imm32);

                    emit3(OPCODE_3_MOVSS_REG_RM);
                    emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(n->reg0.reg), X64_REG_SP);
                    emit1((u8)-8);
                } else {
                    // We do not need a 64-bit move to load a 32-bit into a 64-bit register and ensure that the register is cleared.
                    // A 32-bit move will actually clear the upper bits.
                    emit_prefix(0,X64_REG_INVALID, n->reg0.reg);
                    emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(n->reg0.reg));
                    emit4((u32)(i32)base->imm32);
                }
                
                FIX_OUT_OPERAND(0)
            } break;
            case BC_LI64: {
                auto base = (InstBase_op1_imm64*)n->base;
                FIX_IN_OUT_OPERAND(0)

                if(IS_REG_XMM(n->reg0.reg)) {
                    Assert(!n->reg0.on_stack);

                    Assert(is_register_free(RESERVED_REG0));
                    X64Register tmp_reg = RESERVED_REG0;

                    u8 reg_field = tmp_reg - 1;
                    Assert(reg_field >= 0 && reg_field <= 7);

                    emit1(PREFIX_REXW);
                    emit1((u8)(OPCODE_MOV_REG_IMM_RD_IO | reg_field));
                    emit8((u64)base->imm64);

                    emit1(PREFIX_REXW);
                    emit1(OPCODE_MOV_RM_REG);
                    emit_modrm(MODE_DEREF_DISP8, tmp_reg, X64_REG_SP);
                    emit1((u8)-8);

                    emit3(OPCODE_3_MOVSD_REG_RM);
                    emit_modrm(MODE_DEREF_DISP8, CLAMP_EXT_REG(n->reg0.reg), X64_REG_SP);
                    emit1((u8)-8);
                } else {
                    u8 reg_field = CLAMP_EXT_REG(n->reg0.reg) - 1;
                    Assert(reg_field >= 0 && reg_field <= 7);

                    emit_prefix(PREFIX_REXW, n->reg0.reg, X64_REG_INVALID);
                    emit1((u8)(OPCODE_MOV_REG_IMM_RD_IO | reg_field));
                    emit8((u64)base->imm64);
                }
                
                FIX_OUT_OPERAND(0)
            } break;
            case BC_INCR: {
                auto base = (InstBase_op1_imm32*)n->base;
                FIX_IN_OPERAND(0)
                if((i32)base->imm32 < 0) {
                    emit_sub_imm32(n->reg0.reg, (i32)-base->imm32);
                } else {
                    emit_add_imm32(n->reg0.reg, (i32)base->imm32);
                }
                FIX_OUT_OPERAND(0)
            } break;
            case BC_ALLOC_LOCAL: {
                auto base = (InstBase_op1_imm16*)n->base;
                Assert(base->op0 == BC_REG_INVALID); // TODO: Handle reg in alloc_local
                

                emit_sub_imm32(X64_REG_SP, (i32)base->imm16);
                if(base->op0 != BC_REG_INVALID) {
                    // emit_prefix(PREFIX_REXW, env->reg0.reg, X64_REG_SP);
                    // emit1(OPCODE_MOV_REG_RM);
                    // emit_modrm(MODE_REG, env->reg0.reg, X64_REG_SP);
                    // if(env->reg0.on_stack) {
                    //     Assert(false);
                    //     // Potential bug here. If alloc_local is pushed and some expression is
                    //     // evaluated and more values are pushed then that expression has
                    //     // to be popped before instructions can get the value from alloc_local
                    //     // I don't know if this will ever happen since things are generally pushed
                    //     // and popped in the right order. Alloc local is also a root instruction
                    //     // so there will always be registers available. But still, possibly a bug.

                    //     emit_push(env->reg0.reg);
                    // }
                }
                push_offset = 0;
            } break;
            case BC_FREE_LOCAL: {
                auto base = (InstBase_imm16*)n->base;
                // Assert(push_offset == 0); nocheckin
                emit_add_imm32(X64_REG_SP, (i32)base->imm16);
                ret_offset -= base->imm16;
            } break;
           case BC_SET_ARG: {
                auto base = (InstBase_op1_ctrl_imm16*)n->base;
                FIX_IN_OPERAND(0)

                // this code is required with stdcall or unixcall
                // betcall ignores this
                int arg_index = base->imm16 / 8;
                if(arg_index >= recent_set_args.size()) {
                    recent_set_args.resize(arg_index + 1);
                }
                recent_set_args[arg_index].control = base->control;

                int off = base->imm16 + push_offset;
                X64Register reg_args = X64_REG_SP;
                emit_mov_mem_reg(reg_args, n->reg0.reg, base->control, off);
                
            } break;
            case BC_GET_PARAM: {
                auto base = (InstBase_op1_ctrl_imm16*)n->base;
                FIX_IN_OUT_OPERAND(0)

                X64Register reg_params = X64_REG_BP;
                int off = base->imm16 + FRAME_SIZE;
                emit_mov_reg_mem(n->reg0.reg, reg_params, base->control, off);
                
                FIX_OUT_OPERAND(0)
            } break;
            case BC_GET_VAL: {
                auto base = (InstBase_op1_ctrl_imm16*)n->base;
                auto call_base = (InstBase_link_call_imm32*)last_inst_call->base;
                switch(call_base->call) {
                    case BETCALL: {
                        FIX_IN_OUT_OPERAND(0)

                        X64Register reg_params = X64_REG_SP;
                        int off = base->imm16 + push_offset - FRAME_SIZE + ret_offset;
                        emit_mov_reg_mem(n->reg0.reg, reg_params, base->control, off);
                        
                        FIX_OUT_OPERAND(0)
                    } break;
                    case STDCALL:
                    case UNIXCALL: {
                        // TODO: We hope that an instruction after a call didn't
                        //   overwrite the return value in RAX. We assert if it did.
                        //   Should we reserve RAX after a call until we reach BC_GET_VAL
                        //   and use the return value? No because some function do not
                        //   anything so we would have wasted rax. Then what do we do?

                        // For now we assume RAX has been reserved
                        Assert(false); // TODO: FIXME
                        // env->reg0.reg = X64_REG_A;
                        // if(env->reg0.invalid()) {
                        //     if(IS_CONTROL_FLOAT(n->control)) {
                        //         env->reg0.reg = alloc_register(X64_REG_XMM0, true);
                        //         Assert(!env->reg0.invalid());
                        //     } else {
                                
                        //         env->reg0.reg = alloc_register(X64_REG_A, false);
                        //         Assert(!env->reg0.invalid());
                        //     }
                        // }
                    } break;
                }
            } break;
            case BC_SET_RET: {
                auto base = (InstBase_op1_ctrl_imm16*)n->base;
                switch(tinycode->call_convention) {
                    case BETCALL: {
                        
                        FIX_IN_OPERAND(0)
                        
                        X64Register reg_rets = X64_REG_BP;
                        int off = base->imm16;

                        emit_mov_mem_reg(reg_rets, n->reg0.reg, base->control, off);
                        
                    } break;
                    case STDCALL:
                    case UNIXCALL: {
                        // if(!env->env_in0) {
                        //     Assert(!env->node->in0->computed_env); // See COMPUTED_INPUTS
                        //     auto e = push_env0();
                        //     if(IS_CONTROL_FLOAT(n->control)) {
                        //         e->reg0.reg = alloc_register(X64_REG_XMM0, true);
                        //         Assert(e->reg0.reg != X64_REG_INVALID);
                        //         // if(e->reg0.reg == X64_REG_INVALID)
                        //         //     e->reg0.on_stack = true;
                        //     } else {
                        //         // e->reg0.reg = alloc_register(X64_REG_A, false);
                        //         e->reg0.reg = X64_REG_A;
                        //         // Assert(e->reg0.reg != X64_REG_INVALID);
                        //             // e->reg0.on_stack = true;
                        //         // let someone else allocate register
                        //     }
                        //     break;
                        // }
                        // TODO: FIXME
                        Assert(false);

                        // env->reg0 = env->env_in0->reg0;
                        
                        // RAX has been reserved as return value until we return
                        // when ret is encountered we free RAX because the return
                        // may have exist inside while- or if-statement in which case
                        // RAX will be used for other rets
                        // if(!env->reg0.on_stack && !IsNativeRegister(env->reg0.reg))
                        //     free_register(env->reg0.reg);
                    } break;
                }
                // YOU CANNOT PUT STUFF HERE BECAUSE
                // CODE IN THE SWITCH ABOVE MAY BREAK
                // AND THEN START EXECUTING HERE WHEN IT 
                // EXPECTS TO EXIT ALL THE SWITCHES
            } break;
            case BC_PTR_TO_LOCALS: {
                auto base = (InstBase_op1_imm16*)n->base;
                FIX_IN_OUT_OPERAND(0);

                X64Register reg_local = X64_REG_BP;

                emit_prefix(PREFIX_REXW, X64_REG_INVALID, n->reg0.reg);
                emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(n->reg0.reg));
                emit4((u32)base->imm16); // TODO: Need to offset if we pushed non-volatile registers


                emit_prefix(PREFIX_REXW, n->reg0.reg, reg_local);
                emit1(OPCODE_ADD_REG_RM);
                emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), reg_local);

                FIX_OUT_OPERAND(0);
            } break;
            case BC_PTR_TO_PARAMS: {
                auto base = (InstBase_op1_imm16*)n->base;
                FIX_IN_OUT_OPERAND(0);

                X64Register reg_params = X64_REG_BP;

                emit_prefix(PREFIX_REXW, X64_REG_INVALID, n->reg0.reg);
                emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(n->reg0.reg));
                emit4((u32)(base->imm16 + FRAME_SIZE));

                emit_prefix(PREFIX_REXW, n->reg0.reg, reg_params);
                emit1(OPCODE_ADD_REG_RM);
                emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), reg_params);

                FIX_OUT_OPERAND(0);
            } break;
            case BC_JMP: {
                emit1(OPCODE_JMP_IMM32);
                int imm_offset = size();
                emit4((u32)0);
                
                // const u8 BYTE_OF_BC_JMP = 1 + 4; // nocheckin TODO: DON'T HARDCODE VALUES!
                // addRelocation32(imm_offset - 1, imm_offset, n->bc_index + BYTE_OF_BC_JMP + n->imm);
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
                Assert(push_offset == 0);
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
                if(base->link == LinkConventions::DLLIMPORT || base->link == LinkConventions::VARIMPORT) {
                    Assert(false);
                    // Assert(bytecode->externalRelocations[imm].location == bcIndex);
                    // prog->add(OPCODE_CALL_RM_SLASH_2);
                    // prog->addModRM_rip(2,(u32)0);
                    // X64Program::NamedUndefinedRelocation namedReloc{};
                    // namedReloc.name = bytecode->externalRelocations[imm].name;
                    // namedReloc.textOffset = prog->size() - 4;
                    // prog->namedUndefinedRelocations.add(namedReloc);
                } else if(base->link == LinkConventions::IMPORT) {
                    Assert(false);
                    // Assert(bytecode->externalRelocations[imm].location == bcIndex);
                    // prog->add(OPCODE_CALL_IMM);
                    // X64Program::NamedUndefinedRelocation namedReloc{};
                    // namedReloc.name = bytecode->externalRelocations[imm].name;
                    // namedReloc.textOffset = prog->size();
                    // prog->namedUndefinedRelocations.add(namedReloc);
                    // prog->add4((u32)0);
                } else if (base->link == LinkConventions::NONE){ // or export
                    // Assert(false);
                    emit1(OPCODE_CALL_IMM);
                    int offset = size();
                    emit4((u32)0);
                    
                    // necessary when adding internal func relocations
                    // +3 = 1 (opcode) + 1 (link) + 1 (convention)
                    map_strict_translation(n->bc_index + 3, offset);
                    // bc_to_x64_translation[n->bc_index + 3] = offset;
                    // prog->addInternalFuncRelocation(current_tinyprog_index, offset, n->imm);

                    // RelativeRelocation reloc{};
                    // reloc.currentIP = size();
                    // reloc.bcAddress = n->bc_index + n->imm + BYTE_OF_BC_JZ;
                    // reloc.immediateToModify = size()-4; // NOTE: RelativeRelocation assumes 32-bit integer JMP_IMM8 would not work without changes
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
                        u32 start_addr = size();
                        u8 arr[]={ 0x48, 0x83, 0xEC, 0x38, 0xB9, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0x4D, 0x31, 0xC9, 0x49, 0x89, 0xF8, 0x48, 0x89, 0xF2, 0x48, 0x89, 0xC1, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x38 };
                        emit_bytes(arr,sizeof(arr));

                        // C creates these symbol names in it's object file
                        prog->addNamedUndefinedRelocation("__imp_GetStdHandle", start_addr + 0xB, current_tinyprog_index);
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
                        int reloc_pos = prog->size();
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
                        int offset = size();
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
                        int reloc_pos = prog->size();
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
                emit_pop(X64_REG_BP);
                emit1(OPCODE_RET);

                // nocheckin, TODO: FIXME
                // switch(tinycode->call_convention) {
                //     case BETCALL: break;
                //     case STDCALL:
                //     case UNIXCALL:
                //         if(!is_register_free(X64_REG_A))
                //             free_register(X64_REG_A);
                //         if(!is_register_free(X64_REG_XMM0))
                //             free_register(X64_REG_XMM0);
                //     break;
                //     default: Assert(false);
                // }
            } break;
            case BC_JNZ: {
                auto base = (InstBase_op1_imm32*)n->base;
                
                FIX_IN_OPERAND(0)

                emit_prefix(PREFIX_REXW, X64_REG_INVALID, n->reg0.reg);
                emit1(OPCODE_CMP_RM_IMM8_SLASH_7);
                emit_modrm_slash(MODE_REG, 7, CLAMP_EXT_REG(n->reg0.reg));
                emit1((u8)0);
                
                emit2(OPCODE_2_JNE_IMM32);
                int imm_offset = size();
                emit4((u32)0);

                const u8 BYTE_OF_BC_JNZ = 1 + 1 + 4; // nocheckin TODO: DON'T HARDCODE VALUES!
                addRelocation32(imm_offset - 2, imm_offset, n->bc_index + BYTE_OF_BC_JNZ + base->imm32);
                
            } break;
             case BC_JZ: {
                auto base = (InstBase_op1_imm32*)n->base;
                
                FIX_IN_OPERAND(0)
                
                emit_prefix(PREFIX_REXW, X64_REG_INVALID, n->reg0.reg);
                emit1(OPCODE_CMP_RM_IMM8_SLASH_7);
                emit_modrm_slash(MODE_REG, 7, CLAMP_EXT_REG(n->reg0.reg));
                emit1((u8)0);
                
                emit2(OPCODE_2_JE_IMM32);
                int imm_offset = size();
                emit4((u32)0);
                const u8 BYTE_OF_BC_JZ = 1 + 1 + 4; // nocheckin TODO: DON'T HARDCODE VALUES!
                addRelocation32(imm_offset - 2, imm_offset, n->bc_index + BYTE_OF_BC_JZ + base->imm32);
                
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
                
            } break;
            case BC_DATAPTR: {
                auto base = (InstBase_op1_imm32*)n->base;
                FIX_IN_OUT_OPERAND(0)
                
                Assert(!IS_REG_XMM(n->reg0.reg)); // loading pointer into xmm makes no sense, it's a compiler bug

                emit_prefix(PREFIX_REXW, n->reg0.reg, X64_REG_INVALID);
                emit1(OPCODE_LEA_REG_M);
                emit_modrm_rip32(CLAMP_EXT_REG(n->reg0.reg), (u32)0);
                i32 disp32_offset = size() - 4; // -4 to refer to the 32-bit immediate in modrm_rip
                prog->addDataRelocation(base->imm32, disp32_offset, current_tinyprog_index);

                FIX_OUT_OPERAND(0)
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
                FIX_IN_OPERAND(1);
                FIX_IN_OPERAND(0);
                
                InstructionControl control = CONTROL_NONE;
                if(instruction_contents[opcode] & BASE_ctrl) {
                    auto base = (InstBase_op2_ctrl*)n->base;
                    control = base->control;
                }
                bool is_unsigned = !IS_CONTROL_SIGNED(control);
                
                if(IS_CONTROL_FLOAT(control)) {
                    switch(opcode) {
                        case BC_ADD: {
                            InstructionControl size = GET_CONTROL_SIZE(control);
                            // Assert(size == 4 || size == 8);
                            Assert(size == CONTROL_32B);
                            // TODO: We assume 32-bit float NOT GOOD
                            emit3(OPCODE_3_ADDSS_REG_RM);
                            // TODO: verify float register
                            emit_modrm(MODE_REG, CLAMP_XMM(n->reg1.reg), CLAMP_XMM(n->reg0.reg));
                        } break;
                        case BC_SUB: {
                            Assert(false);
                            // emit1(PREFIX_REXW);
                            // emit1(OPCODE_SUB_REG_RM);
                            // emit_modrm(MODE_REG, n->reg0, n->reg1);
                        } break;
                        case BC_MUL: {
                            Assert(false);
                            // // TODO: Handle signed/unsigned multiplication (using InstructionControl)
                            // if(is_unsigned) {
                            //     bool d_is_free = is_register_free(X64_REG_D);
                            //     bool a_is_free = is_register_free(X64_REG_A);
                            //     if(!d_is_free) {
                            //         emit_push(X64_REG_D);
                            //     }
                            //     if(!a_is_free) {
                            //         emit_push(64_REG_A);
                            //     }
                            //     emit1(PREFIX_REXW);
                            //     emit1(OPCODE_MOV_REG_RM);
                            //     emit_modrm(MODE_REG, X64_REG_A, n->reg0);
                                
                            //     emit1(PREFIX_REXW);
                            //     emit1(OPCODE_MUL_AX_RM_SLASH_4);
                            //     emit_modrm_slash(MODE_REG, 4, n->reg1);
                                
                            //     emit1(PREFIX_REXW);
                            //     emit1(OPCODE_MOV_REG_RM);
                            //     emit_modrm(MODE_REG, n->reg0, X64_REG_A);
                                
                            //     if(!a_is_free) {
                            //         emit_pop(X64_REG_A);
                            //     }
                            //     if(!d_is_free) {
                            //         emit_pop(X64_REG_D);
                            //     }
                            // } else {
                            //     emit1(PREFIX_REXW);
                            //     emit2(OPCODE_2_IMUL_REG_RM);
                            //     emit_modrm(MODE_REG, n->reg1, n->reg0);
                            // }
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
                            bool c_was_used = false;
                            if(!is_register_free(X64_REG_C)) {
                                c_was_used = true;
                                emit_push(X64_REG_C);

                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, n->reg1.reg);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, X64_REG_C, CLAMP_EXT_REG(n->reg1.reg));
                            }

                            emit1(PREFIX_REXW);
                            switch(opcode) {
                                case BC_BLSHIFT: {
                                    emit_prefix(PREFIX_REXW, X64_REG_INVALID, n->reg0.reg);
                                    emit1(OPCODE_SHL_RM_CL_SLASH_4);
                                    emit_modrm_slash(MODE_REG, 4, CLAMP_EXT_REG(n->reg0.reg));
                                } break;
                                case BC_BRSHIFT: {
                                    emit_prefix(PREFIX_REXW, X64_REG_INVALID, n->reg0.reg);
                                    emit1(OPCODE_SHR_RM_CL_SLASH_5);
                                    emit_modrm_slash(MODE_REG, 5, CLAMP_EXT_REG(n->reg0.reg));
                                } break;
                                default: Assert(false);
                            }

                            if(c_was_used) {
                                emit_pop(X64_REG_C);
                            }
                        } break;
                        case BC_EQ: {
                            Assert(false);
                            emit_prefix(PREFIX_REXW, n->reg0.reg, n->reg1.reg);
                            emit1(OPCODE_CMP_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg1.reg));
                            
                            emit_prefix(0,X64_REG_INVALID, n->reg0.reg);
                            emit2(OPCODE_2_SETE_RM8);
                            emit_modrm_slash(MODE_REG, 0, n->reg0.reg);

                            emit_prefix(PREFIX_REXW, n->reg0.reg, n->reg0.reg);
                            emit2(OPCODE_2_MOVZX_REG_RM8);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg0.reg));
                        } break;
                        case BC_NEQ: {
                            Assert(false);
                            // IMPORTANT: THERE MAY BE BUGS IF YOU COMPARE OPERANDS OF DIFFERENT SIZES.
                            //  SOME BITS MAY BE UNINITIALZIED.
                            emit_prefix(PREFIX_REXW, n->reg0.reg, n->reg1.reg);
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg1.reg));
                        } break;
                        case BC_LT:
                        case BC_LTE:
                        case BC_GT:
                        case BC_GTE: {
                            Assert(false);
                            emit_prefix(PREFIX_REXW,n->reg0.reg, n->reg1.reg);
                            emit1(OPCODE_CMP_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg1.reg));

                            u16 setType = 0;
                            if(!is_unsigned) {
                                switch(opcode) {
                                    case BC_LT: {
                                        setType = OPCODE_2_SETL_RM8;
                                    } break;
                                    case BC_LTE: {
                                        setType = OPCODE_2_SETLE_RM8;
                                    } break;
                                    case BC_GT: {
                                        setType = OPCODE_2_SETG_RM8;
                                    } break;
                                    case BC_GTE: {
                                        setType = OPCODE_2_SETGE_RM8;
                                    } break;
                                    default: Assert(false);
                                }
                            } else {
                                switch(opcode) {
                                    case BC_LT: {
                                        setType = OPCODE_2_SETB_RM8;
                                    } break;
                                    case BC_LTE: {
                                        setType = OPCODE_2_SETBE_RM8;
                                    } break;
                                    case BC_GT: {
                                        setType = OPCODE_2_SETA_RM8;
                                    } break;
                                    case BC_GTE: {
                                        setType = OPCODE_2_SETAE_RM8;
                                    } break;
                                    default: Assert(false);
                                }
                            }
                            
                            emit_prefix(0, X64_REG_INVALID, n->reg0.reg);
                            emit2(setType);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(n->reg0.reg));
                            
                            // do we need more stuff or no? I don't think so?
                            // prog->add2(OPCODE_2_MOVZX_REG_RM8);
                            // emit_modrm(MODE_REG, reg2, reg2);

                        } break;
                        default: Assert(false);
                    }
                } else {
                    switch(opcode) {
                        case BC_ADD: {
                            emit_prefix(PREFIX_REXW,n->reg0.reg, n->reg1.reg);
                            emit1(OPCODE_ADD_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg1.reg));
                        } break;
                        case BC_SUB: {
                            emit_prefix(PREFIX_REXW,n->reg0.reg, n->reg1.reg);
                            emit1(OPCODE_SUB_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg1.reg));
                        } break;
                        case BC_MUL: {
                            // TODO: Handle signed/unsigned multiplication (using InstructionControl)
                            if(is_unsigned) {
                                bool d_is_free = is_register_free(X64_REG_D);
                                bool a_is_free = is_register_free(X64_REG_A);
                                if(!d_is_free) {
                                    emit_push(X64_REG_D);
                                }
                                if(!a_is_free) {
                                    emit_push(X64_REG_A);
                                }
                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, n->reg0.reg);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, X64_REG_A, CLAMP_EXT_REG(n->reg0.reg));
                                
                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, n->reg1.reg);
                                emit1(OPCODE_MUL_AX_RM_SLASH_4);
                                emit_modrm_slash(MODE_REG, 4, CLAMP_EXT_REG(n->reg1.reg));
                                
                                emit_prefix(PREFIX_REXW, n->reg0.reg, X64_REG_INVALID);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), X64_REG_A);
                                
                                if(!a_is_free) {
                                    emit_pop(X64_REG_A);
                                }
                                if(!d_is_free) {
                                    emit_pop(X64_REG_D);
                                }
                            } else {
                                emit_prefix(PREFIX_REXW, n->reg1.reg, n->reg0.reg);
                                emit2(OPCODE_2_IMUL_REG_RM);
                                emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg1.reg), CLAMP_EXT_REG(n->reg0.reg));
                            }
                        } break;
                        case BC_LAND: {
                            // AND = !(!r0 || !r1)

                            emit_prefix(PREFIX_REXW, n->reg0.reg, n->reg0.reg);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg0.reg));

                            emit1(OPCODE_JZ_IMM8);
                            int offset_jmp0_imm = size();
                            emit1((u8)0);

                            emit_prefix(PREFIX_REXW, n->reg1.reg, n->reg1.reg);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg1.reg), CLAMP_EXT_REG(n->reg1.reg));
                            
                            emit1(OPCODE_JZ_IMM8);
                            int offset_jmp1_imm = size();
                            emit1((u8)0);

                            emit_prefix(0,X64_REG_INVALID, n->reg0.reg);
                            emit1(OPCODE_MOV_RM_IMM8_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(n->reg0.reg));
                            emit1((u8)1);

                            emit1(OPCODE_JMP_IMM8);
                            int offset_jmp2_imm = size();
                            emit1((u8)0);

                            fix_jump_here_imm8(offset_jmp0_imm);
                            fix_jump_here_imm8(offset_jmp1_imm);

                            emit_prefix(0,X64_REG_INVALID, n->reg0.reg);
                            emit1(OPCODE_MOV_RM_IMM8_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(n->reg0.reg));
                            emit1((u8)0);

                            fix_jump_here_imm8(offset_jmp2_imm);
                        } break;
                        case BC_LOR: {
                            // OR = (r0==0 || r1==0)

                            emit_prefix(PREFIX_REXW, n->reg0.reg, n->reg0.reg);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg0.reg));

                            emit1(OPCODE_JNZ_IMM8);
                            int offset_jmp0_imm = size();
                            emit1((u8)0);

                            emit_prefix(PREFIX_REXW, n->reg1.reg, n->reg1.reg);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg1.reg), CLAMP_EXT_REG(n->reg1.reg));
                            
                            emit1(OPCODE_JNZ_IMM8);
                            int offset_jmp1_imm = size();
                            emit1((u8)0);

                            emit_prefix(0,X64_REG_INVALID, n->reg0.reg);
                            emit1(OPCODE_MOV_RM_IMM8_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(n->reg0.reg));
                            emit1((u8)0);

                            emit1(OPCODE_JMP_IMM8);
                            int offset_jmp2_imm = size();
                            emit1((u8)0);

                            fix_jump_here_imm8(offset_jmp0_imm);
                            fix_jump_here_imm8(offset_jmp1_imm);

                            emit_prefix(0,X64_REG_INVALID, n->reg0.reg);
                            emit1(OPCODE_MOV_RM_IMM8_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(n->reg0.reg));
                            emit1((u8)1);

                            fix_jump_here_imm8(offset_jmp2_imm);
                        } break;
                        case BC_LNOT: {
                            emit_prefix(PREFIX_REXW, n->reg1.reg, n->reg1.reg);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg1.reg), CLAMP_EXT_REG(n->reg1.reg));

                            emit2(OPCODE_2_SETE_RM8); // SETZ/SETE
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(n->reg0.reg));
                        } break;
                        case BC_BAND: {
                            emit_prefix(PREFIX_REXW, n->reg1.reg, n->reg0.reg);
                            emit1(OPCODE_AND_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg1.reg), CLAMP_EXT_REG(n->reg0.reg));
                        } break;
                        case BC_BOR: {
                            emit_prefix(PREFIX_REXW, n->reg1.reg, n->reg0.reg);
                            emit1(OPCODE_OR_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg1.reg), CLAMP_EXT_REG(n->reg0.reg));
                        } break;
                        case BC_BXOR: {
                            emit_prefix(PREFIX_REXW, n->reg0.reg, n->reg1.reg);
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg1.reg));
                        } break;
                        case BC_BLSHIFT:
                        case BC_BRSHIFT: {
                            bool c_was_used = false;
                            X64Register reg = n->reg0.reg;
                            if(n->reg1.reg == X64_REG_C) {

                            } else if(!is_register_free(X64_REG_C)) {
                                c_was_used = true;
                                if(n->reg0.reg == X64_REG_C) {
                                    reg = RESERVED_REG0;
                                    emit_mov_reg_reg(reg, X64_REG_C);
                                } else {
                                    emit_push(X64_REG_C);
                                }
                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, n->reg1.reg);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, X64_REG_C, CLAMP_EXT_REG(n->reg1.reg));
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
                                if(n->reg0.reg == X64_REG_C) {
                                    emit_mov_reg_reg(X64_REG_C, reg);
                                } else {
                                    emit_pop(X64_REG_C);
                                }
                            }
                        } break;
                        case BC_EQ: {
                            if(GET_CONTROL_SIZE(control) == CONTROL_8B) {
                                emit_prefix(0, n->reg0.reg, n->reg1.reg);
                                emit1(OPCODE_CMP_REG8_RM8);
                            } else {
                                if(GET_CONTROL_SIZE(control) == CONTROL_16B) {
                                    emit1(PREFIX_16BIT);
                                    emit_prefix(0, n->reg0.reg, n->reg1.reg);
                                } else if(GET_CONTROL_SIZE(control) == CONTROL_32B) {
                                    emit_prefix(0, n->reg0.reg, n->reg1.reg);
                                }  else if(GET_CONTROL_SIZE(control) == CONTROL_64B) {
                                    emit_prefix(PREFIX_REXW, n->reg0.reg, n->reg1.reg);
                                }
                                emit1(OPCODE_CMP_REG_RM);
                            }
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg1.reg));

                            emit_prefix(0,X64_REG_INVALID, n->reg0.reg);
                            emit2(OPCODE_2_SETE_RM8);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(n->reg0.reg));

                            emit_movzx(n->reg0.reg,n->reg0.reg,CONTROL_8B);
                        } break;
                        case BC_NEQ: {
                            if(GET_CONTROL_SIZE(control) == CONTROL_8B) {
                                emit_prefix(0, n->reg0.reg, n->reg1.reg);
                                emit1(OPCODE_XOR_REG8_RM8);
                            } else {
                                if(GET_CONTROL_SIZE(control) == CONTROL_16B) {
                                    emit1(PREFIX_16BIT);
                                    emit_prefix(0, n->reg0.reg, n->reg1.reg);
                                } else if(GET_CONTROL_SIZE(control) == CONTROL_32B) {
                                    emit_prefix(0, n->reg0.reg, n->reg1.reg);
                                }  else if(GET_CONTROL_SIZE(control) == CONTROL_64B) {
                                    emit_prefix(PREFIX_REXW, n->reg0.reg, n->reg1.reg);
                                }
                                emit1(OPCODE_XOR_REG_RM);
                            }
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg1.reg));
                        } break;
                        case BC_LT:
                        case BC_LTE:
                        case BC_GT:
                        case BC_GTE: {
                            if(GET_CONTROL_SIZE(control) == CONTROL_8B) {
                                // we used REXW so that we access low bits of R8-R15 and not high bits of ax, cx (ah,ch)
                                emit_prefix(PREFIX_REXW, n->reg0.reg, n->reg1.reg);
                                emit1(OPCODE_CMP_REG8_RM8);
                                emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg1.reg));
                            } else {
                                if(GET_CONTROL_SIZE(control) == CONTROL_16B) {
                                    emit1(PREFIX_16BIT);
                                    emit_prefix(0, n->reg0.reg, n->reg1.reg);
                                } else if(GET_CONTROL_SIZE(control) == CONTROL_32B) {
                                    emit_prefix(0, n->reg0.reg, n->reg1.reg);
                                }  else if(GET_CONTROL_SIZE(control) == CONTROL_64B) {
                                    emit_prefix(PREFIX_REXW, n->reg0.reg, n->reg1.reg);
                                }
                                emit1(OPCODE_CMP_REG_RM);
                                emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg1.reg));
                            }

                            u16 setType = 0;
                            if(!is_unsigned) {
                                switch(opcode) {
                                    case BC_LT: {
                                        setType = OPCODE_2_SETL_RM8;
                                    } break;
                                    case BC_LTE: {
                                        setType = OPCODE_2_SETLE_RM8;
                                    } break;
                                    case BC_GT: {
                                        setType = OPCODE_2_SETG_RM8;
                                    } break;
                                    case BC_GTE: {
                                        setType = OPCODE_2_SETGE_RM8;
                                    } break;
                                    default: Assert(false);
                                }
                            } else {
                                switch(opcode) {
                                    case BC_LT: {
                                        setType = OPCODE_2_SETB_RM8;
                                    } break;
                                    case BC_LTE: {
                                        setType = OPCODE_2_SETBE_RM8;
                                    } break;
                                    case BC_GT: {
                                        setType = OPCODE_2_SETA_RM8;
                                    } break;
                                    case BC_GTE: {
                                        setType = OPCODE_2_SETAE_RM8;
                                    } break;
                                    default: Assert(false);
                                }
                            }
                            
                            emit_prefix(0, X64_REG_INVALID, n->reg0.reg);
                            emit2(setType);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(n->reg0.reg));
                            
                            // do we need more stuff or no? I don't think so?
                            emit_prefix(PREFIX_REXW, n->reg0.reg, n->reg0.reg);
                            emit2(OPCODE_2_MOVZX_REG_RM8);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg0.reg));
                        } break;
                        default: Assert(false);
                    }
                }
                
                FIX_OUT_OPERAND(0);
            } break;
            
            case BC_DIV: // these require specific registers
            case BC_MOD: { 
                FIX_IN_OPERAND(1);
                FIX_IN_OPERAND(0);
                
                auto base = (InstBase_op2_ctrl*)n->base;
                if(IS_CONTROL_FLOAT(base->control)) {
                    Assert(false);
                } else {
                    switch(base->opcode) {
                        case BC_DIV: {
                            // TODO: Handled signed/unsigned
                            
                            // TODO: I think there are edge cases not taken care of here and in BC_MOD
                            bool d_is_free = false; // is_register_free(X64_REG_D);
                            if(!d_is_free) {
                                emit_push(X64_REG_D);
                            }
                            emit_prefix(PREFIX_REXW, X64_REG_INVALID, X64_REG_INVALID);
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, X64_REG_D, X64_REG_D);
                            
                            bool a_is_free = true;
                            if(n->reg0.reg != X64_REG_A) {
                                a_is_free = false; is_register_free(X64_REG_A);
                                if(!a_is_free) {
                                    emit_push(X64_REG_A);
                                }
                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, n->reg0.reg);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, X64_REG_A, CLAMP_EXT_REG(n->reg0.reg));
                            }

                            emit_prefix(PREFIX_REXW, X64_REG_INVALID, n->reg1.reg);
                            emit1(OPCODE_DIV_AX_RM_SLASH_6);
                            emit_modrm_slash(MODE_REG, 6, CLAMP_EXT_REG(n->reg1.reg));
                            
                            if(n->reg0.reg != X64_REG_A) {
                                emit_prefix(PREFIX_REXW, n->reg0.reg, X64_REG_INVALID);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), X64_REG_A);
                            }
                            if(!a_is_free) {
                                emit_pop(X64_REG_A);
                            }
                            if(!d_is_free) {
                                emit_pop(X64_REG_D);
                            }
                        } break;
                        case BC_MOD: {
                            // TODO: Handled signed/unsigned

                            bool d_is_free = false; // is_register_free(X64_REG_D);
                            if(!d_is_free) {
                                emit_push(X64_REG_D);
                            }
                            emit_prefix(PREFIX_REXW, X64_REG_INVALID, X64_REG_INVALID);
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, X64_REG_D, X64_REG_D);
                            
                            bool a_is_free = true;
                            if(n->reg0.reg != X64_REG_A) {
                                a_is_free = false; // is_register_free(X64_REG_A);
                                if(!a_is_free) {
                                    emit_push(X64_REG_A);
                                }
                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, n->reg0.reg);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, X64_REG_A, CLAMP_EXT_REG(n->reg0.reg));
                            }

                            emit_prefix(PREFIX_REXW, X64_REG_INVALID, n->reg1.reg);
                            emit1(OPCODE_DIV_AX_RM_SLASH_6);
                            emit_modrm_slash(MODE_REG, 6, CLAMP_EXT_REG(n->reg1.reg)); 
                            
                            emit_prefix(PREFIX_REXW, n->reg0.reg, X64_REG_INVALID);
                            emit1(OPCODE_MOV_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), X64_REG_D);
                            // }
                            if(!a_is_free) {
                                emit_pop(X64_REG_A);
                            }
                            if(!d_is_free) {
                                emit_pop(X64_REG_D);
                            }
                        } break;
                        default: Assert(false);
                    }
                }
                
                FIX_OUT_OPERAND(0);
            } break;
            case BC_LNOT: { // LNOT has one input and output while add has two input and one output, it's special
                FIX_IN_OPERAND(1);
                FIX_IN_OUT_OPERAND(0);
                // TODO: What about differently sized values? Should it always be 64-bit operation
                emit_prefix(PREFIX_REXW, n->reg1.reg, n->reg1.reg);
                emit1(OPCODE_TEST_RM_REG);
                emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg1.reg), CLAMP_EXT_REG(n->reg1.reg));
                
                emit_prefix(0, n->reg0.reg, X64_REG_INVALID);
                emit2(OPCODE_2_SETE_RM8);
                emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(n->reg0.reg));
                FIX_OUT_OPERAND(0);
            } break;
            case BC_BNOT: { // BNOT is special
                FIX_IN_OPERAND(1);
                FIX_IN_OUT_OPERAND(0);
                // TODO: Optimize by using one register. No need to allocate an IN and OUT register. We can then skip the MOV instruction.
                emit_prefix(PREFIX_REXW, n->reg0.reg, n->reg1.reg);
                emit1(OPCODE_MOV_REG_RM);
                emit_modrm(MODE_REG, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg1.reg));

                emit_prefix(PREFIX_REXW, X64_REG_INVALID, n->reg0.reg);
                emit1(OPCODE_NOT_RM_SLASH_2);
                emit_modrm_slash(MODE_REG, 2, CLAMP_EXT_REG(n->reg0.reg));
                
                FIX_OUT_OPERAND(0);
            } break;
            
            case BC_MEMZERO: {
                auto base = (InstBase_op2_imm8*)n->base;
                // which order?
                FIX_IN_OPERAND(1)
                FIX_IN_OPERAND(0)

                // ##########################
                //   MEMZERO implementation
                // ##########################
                {
                    u8 batch_size = base->imm8;
                    if(batch_size == 0)
                        batch_size = 1;

                    X64Register reg_cur = n->reg0.reg;
                    X64Register reg_fin = n->reg1.reg;

                    u8 prefix = PREFIX_REXW;
                    emit_prefix(PREFIX_REXW, reg_cur, reg_fin);
                    emit1(OPCODE_ADD_RM_REG);
                    emit_modrm(MODE_REG, CLAMP_EXT_REG(reg_cur), CLAMP_EXT_REG(reg_fin));

                    int offset_loop = size();

                    emit_prefix(PREFIX_REXW, reg_fin, reg_cur);
                    emit1(OPCODE_CMP_REG_RM);
                    emit_modrm(MODE_REG, CLAMP_EXT_REG(reg_fin), CLAMP_EXT_REG(reg_cur));

                    emit1(OPCODE_JE_IMM8);
                    int offset_jmp_imm = size();
                    emit1((u8)0);
                    

                    switch(batch_size) {
                    case 0:
                    case 1:
                        emit_prefix(0, X64_REG_INVALID, reg_cur);
                        emit1(OPCODE_MOV_RM_IMM8_SLASH_0);
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
                        emit4((u32)0);
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
                    emit1((u8)(offset_loop - (size()+1)));

                    fix_jump_here_imm8(offset_jmp_imm);
                    break;
                }
            } break;
            case BC_MEMCPY: {
                auto base = (InstBase_op3*)n->base;
                // which order?
                FIX_IN_OPERAND(2)
                FIX_IN_OPERAND(1)
                FIX_IN_OPERAND(0)
                // TODO: Check node depth, do the most register allocs first
                // if(!env->env_in0) {
                //     auto e = push_env0();
                //     INHERIT_REG(reg0)
                //     break;
                // }
                // if(!env->env_in1) {
                //     auto e = push_env1();
                //     INHERIT_REG(reg1)
                //     break;
                // }
                // if(!env->env_in0) {
                //     auto e = push_env2();
                //     INHERIT_REG(reg2)
                //     break;
                // }

                // env->reg0 = env->env_in0->reg0;
                // env->reg1 = env->env_in1->reg0;
                // env->reg2 = env->env_in2->reg0;
                
                // if(env->reg2.on_stack) {
                //     env->reg2.reg = RESERVED_REG2;
                //     emit_pop(env->reg2.reg);
                // }

                // if(env->reg1.on_stack) {
                //     env->reg1.reg = RESERVED_REG1;
                //     emit_pop(env->reg1.reg);
                // }

                // if(env->reg0.on_stack) {
                //     env->reg0.reg = RESERVED_REG0;
                //     emit_pop(env->reg0.reg);
                // }
                
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

                    X64Register dst = n->reg0.reg;
                    X64Register src = n->reg1.reg;
                    X64Register fin = n->reg2.reg;
                    X64Register tmp = X64_REG_A;

                    u8 prefix = PREFIX_REXW;
                    emit1(PREFIX_REXW);
                    emit1(OPCODE_ADD_RM_REG);
                    emit_modrm(MODE_REG, fin, dst);

                    int offset_loop = size();

                    emit1(PREFIX_REXW);
                    emit1(OPCODE_CMP_REG_RM);
                    emit_modrm(MODE_REG, dst, fin);

                    emit1(OPCODE_JE_IMM8);
                    int offset_jmp_imm = size();
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
                        emit_modrm_slash(MODE_REG, 0, dst);

                        emit_prefix(PREFIX_REXW, X64_REG_INVALID, src);
                        emit1(OPCODE_INC_RM_SLASH_0);
                        emit_modrm_slash(MODE_REG, 0, src);
                    } else {
                        emit_add_imm32(dst, batch_size);
                        emit_add_imm32(src, batch_size);
                    }

                    emit1(OPCODE_JMP_IMM8);
                    emit1((u8)(offset_loop - (size()+1)));

                    fix_jump_here_imm8(offset_jmp_imm);
                    break;
                }
            } break;
            case BC_STRLEN: {
                auto base = (InstBase_op2*)n->base;
                FIX_IN_OPERAND(1)
                FIX_IN_OUT_OPERAND(0)

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
            
                X64Register counter = n->reg0.reg;
                X64Register src =     n->reg1.reg;

                emit_prefix(PREFIX_REXW, counter, counter);
                emit1(OPCODE_XOR_REG_RM);
                emit_modrm(MODE_REG, CLAMP_EXT_REG(counter), CLAMP_EXT_REG(counter));

                emit_prefix(PREFIX_REXW, X64_REG_INVALID, src);
                emit1(OPCODE_CMP_RM_IMM8_SLASH_7);
                emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(src));
                emit1((u8)0);

                emit1(OPCODE_JE_IMM8);
                int offset_jmp_null = size();
                emit1((u8)0);

                int offset_loop = size();
                
                X64Register tmp = X64_REG_A;
                emit_mov_reg_mem(tmp, src, CONTROL_8B, 0);

                emit_prefix(PREFIX_REXW, X64_REG_INVALID, tmp);
                emit1(OPCODE_CMP_RM_IMM8_SLASH_7);
                emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(tmp));
                emit1((u8)0);

                emit1(OPCODE_JE_IMM8);
                int offset_jmp = size();
                emit1((u8)0);

                emit_prefix(PREFIX_REXW, X64_REG_INVALID, counter);
                emit1(OPCODE_INC_RM_SLASH_0);
                emit_modrm_slash(MODE_REG, 0, counter);
                
                emit_prefix(PREFIX_REXW, X64_REG_INVALID, src);
                emit1(OPCODE_INC_RM_SLASH_0);
                emit_modrm_slash(MODE_REG, 0, src);

                emit1(OPCODE_JMP_IMM8);
                emit_jmp_imm8(offset_loop);

                fix_jump_here_imm8(offset_jmp_null);
                fix_jump_here_imm8(offset_jmp);
            
                FIX_OUT_OPERAND(0)
            } break;
            case BC_RDTSC: {
                Assert(false);
                
                // if(is_register_free(X64_REG_D)) {
                //     env->reg0.reg = X64_REG_D;
                // } else {
                //     REQUEST_REG0();
                //     emit_push(X64_REG_D);
                // }

                // emit2(OPCODE_2_RDTSC); // sets edx (32-64) and eax (0-32), upper 32-bits of rdx and rax are cleared

                // emit1(PREFIX_REXW);
                // emit1(OPCODE_SHL_RM_IMM8_SLASH_4);
                // emit_modrm_slash(MODE_REG, 4, X64_REG_D);
                // emit1((u8)32);

                // if(env->reg0.reg == X64_REG_D) {
                //     emit1(PREFIX_REXW);
                //     emit1(OPCODE_OR_RM_REG);
                //     emit_modrm(MODE_REG, X64_REG_D, X64_REG_A);
                // } else {
                //     emit1(PREFIX_REXW);
                //     emit1(OPCODE_OR_RM_REG);
                //     emit_modrm(MODE_REG, X64_REG_A, X64_REG_D);
                //     emit_pop(X64_REG_D);
                //     if(env->reg0.on_stack)
                //         emit_push(X64_REG_A);
                //     else {
                //         emit_mov_reg_reg(env->reg0.reg, X64_REG_A);
                //     }
                // }
            } break;
            case BC_ATOMIC_ADD: {
                auto base = (InstBase_op2_ctrl*)n->base;
                FIX_IN_OPERAND(1)
                FIX_IN_OPERAND(0)

                int prefix = 0;
                if(GET_CONTROL_SIZE(base->control) == CONTROL_32B) {

                } else if(GET_CONTROL_SIZE(base->control) == CONTROL_64B)
                    prefix |= PREFIX_REXW;
                else Assert(false);
                
                emit1(PREFIX_LOCK);
                emit_prefix(prefix, n->reg0.reg, n->reg1.reg);
                emit1(OPCODE_ADD_RM_REG);
                emit_modrm(MODE_DEREF, CLAMP_EXT_REG(n->reg0.reg), CLAMP_EXT_REG(n->reg1.reg));

                FIX_OUT_OPERAND(0)
            } break;
            case BC_ATOMIC_CMP_SWAP: {
                FIX_IN_OPERAND(2)
                FIX_IN_OPERAND(1)
                FIX_IN_OPERAND(0)

                // TODO: Add 64-bit version
                // int prefix = 0;
                // if(GET_CONTROL_SIZE(n->control) == CONTROL_32B) {

                // } else if(GET_CONTROL_SIZE(n->control) == CONTROL_64B)
                //     prefix |= PREFIX_REXW;
                // else Assert(false);

                X64Register reg_new = n->reg2.reg;
                X64Register old =     n->reg1.reg;
                X64Register ptr =     n->reg0.reg;

                emit_mov_reg_reg(X64_REG_A, old);
                
                emit1(PREFIX_LOCK);
                emit2(OPCODE_2_CMPXCHG_RM_REG);
                emit_modrm(MODE_DEREF, reg_new, ptr);
                // RAX will be set to the original value in ptr

                emit_mov_reg_reg(ptr, X64_REG_A);

                FIX_IN_OPERAND(0)
            } break;
            case BC_ROUND:
            case BC_SQRT: {
                FIX_IN_OPERAND(0)

                X64Register reg = n->reg0.reg;

                emit4(OPCODE_4_ROUNDSS_REG_RM_IMM8);
                emit_modrm(MODE_REG, reg, reg);
                emit1((u8)0); // immediate describes rounding mode. 0 means "round to" which I believe will
                // choose the number that is closest to the actual number. There where round toward, up, and toward as well (round to is probably a good default).
                emit3(OPCODE_3_SQRTSS_REG_RM);
                emit_modrm(MODE_REG, reg, reg);

                FIX_OUT_OPERAND(0)
            } break;
            case BC_TEST_VALUE: {
                Assert(false);
                #ifdef DISABLE_BC_TEST_VALUE
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
                #ifdef gone
                X64Register r0 = X64_REG_C;
                X64Register r1 = X64_REG_D;

                if(!env->env_in0 && !env->env_in1) {
                    // NOTE: Evaluate the node with the most calculations and register allocations first so that we don't unnecesarily allocate registers and push values to the stack. We do the easiest work first.
                    // TODO: Optimize get_node_depth, recalculating depth like this is expensive. (calculate once and reuse for parent and child nodes)
                    int depth0 = get_node_depth(n->in0);
                    int depth1 = get_node_depth(n->in1);

                    if(depth0 < depth1) {
                        if(!env->env_in0) {
                            Assert(!env->node->in0->computed_env); // See COMPUTED_INPUTS
                            auto e = push_env0();
                            // e->reg0.reg = alloc_register(r0, false);
                            // Assert(!e->reg0.invalid());
                        }
                        if(!env->env_in1) {
                            Assert(!env->node->in1->computed_env); // See COMPUTED_INPUTS
                            auto e = push_env1();
                            // e->reg0.reg = alloc_register(r1, false);
                            // Assert(!e->reg0.invalid());
                        }
                    } else {
                        if(!env->env_in1) {
                            Assert(!env->node->in1->computed_env); // See COMPUTED_INPUTS
                            auto e = push_env1();
                            // e->reg0.reg = alloc_register(r1, false);
                            // Assert(!e->reg0.invalid());
                        }
                        if(!env->env_in0) {
                            Assert(!env->node->in0->computed_env); // See COMPUTED_INPUTS
                            auto e = push_env0();
                            // e->reg0.reg = alloc_register(r0, false);
                            // Assert(!e->reg0.invalid());
                        }
                    }
                    Assert(!pop_env);
                    break;
                }

                env->reg0 = env->env_in0->reg0;
                env->reg1 = env->env_in1->reg0;

                // NOTE: We leverage the fact that TEST_VALUE is a root node where we don't
                //   need to save registers, because non are used. This assumption may be wrong in the future.
                Assert(envs.size() == 1);
                emit_mov_reg_reg(RESERVED_REG0, env->reg0.reg);
                emit_mov_reg_reg(RESERVED_REG1, env->reg1.reg);

                #ifdef OS_WINDOWS
                /*
                sub rsp, 8
                mov rbx, rsp
                sub rsp, 0x28
                mov DWORD PTR [rbx], 0x99993057
                cmp rsi, rdi
                je hop
                mov BYTE PTR [rbx], 0x78
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
                b:  c7 03 57 30 99 99       mov    DWORD PTR [rbx],0x99993057
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

                int start_addr = size();
                const u8 arr[] { 0x48, 0x83, 0xEC, 0x08, 0x48, 0x89, 0xE3, 0x48, 0x83, 0xEC, 0x28, 0xC7, 0x03, 0x57, 0x30, 0x99, 0x99, 0x48, 0x39, 0xFE, 0x74, 0x03, 0xC6, 0x03, 0x78, 0xB9, 0xF4, 0xFF, 0xFF, 0xFF, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0x89, 0xC1, 0x48, 0x89, 0xDA, 0x49, 0xC7, 0xC0, 0x04, 0x00, 0x00, 0x00, 0x4D, 0x31, 0xC9, 0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x30 };
                emit_bytes(arr, sizeof(arr));

                Assert((n->imm & ~0xFFFF) == 0);
                set_imm8(start_addr + 0xd, n->imm&0xFF);
                set_imm8(start_addr + 0xe, (n->imm>>8)&0xFF);

                prog->addNamedUndefinedRelocation("__imp_GetStdHandle", start_addr + 0x20, current_tinyprog_index);
                prog->addNamedUndefinedRelocation("__imp_WriteFile", start_addr + 0x3F, current_tinyprog_index);
                #else
                Assert(false);
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

                
                if(!env->reg0.on_stack)
                    free_register(env->reg0.reg);
                    
                if(!env->reg1.on_stack)
                    free_register(env->reg1.reg);
                #endif
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
            set_imm32(rel.imm32_addr, size() - rel.inst_addr);
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
}

engone::Logger& operator<<(engone::Logger& l, X64Inst& i) {
    
    l << engone::log::PURPLE << i.base->opcode << engone::log::NO_COLOR;
    if(i.reg0.reg != X64_REG_INVALID)
        l << " " << i.reg0.reg;
    if(i.reg1.reg != X64_REG_INVALID)
        l << ", " << i.reg1.reg;
    if(i.reg2.reg != X64_REG_INVALID)
        l << ", " << i.reg2.reg;
    
    int flags = instruction_contents[i.base->opcode];
    int offset = 1 + ((flags & BASE_op1) != 0) + ((flags & BASE_op2) != 0) + ((flags & BASE_op3) != 0)
        + ((flags & BASE_ctrl) != 0) + ((flags & BASE_cast) != 0) + ((flags & BASE_link) != 0)
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