/*
    Experimental x64 generator
    (well, the whole compiler is experimental)
*/

#include "BetBat/x64_gen.h"
#include "BetBat/x64_terms.h"

#include "BetBat/CompilerEnums.h"
#include "BetBat/Compiler.h"

void X64Builder::generateFromTinycode_v2(Bytecode* code, TinyBytecode* tinycode) {
    using namespace engone;
    log::out <<log::GOLD<< "RUNNING EXPERIMENTAL X64 GEN.\n";
    this->bytecode = bytecode;
    this->tinycode = tinycode;

    bool logging = false;

    #define CREATE_INST() auto n = createInst(opcode);\
            n->op0 = op0;\
            n->op1 = op1;\
            n->op2 = op2;\
            n->imm = imm;\
            n->cast = cast;\
            n->control=control;\
            insert_inst(n);

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
        
        InstructionType opcode = BC_HALT;
        BCRegister op0=BC_REG_INVALID, op1=BC_REG_INVALID, op2=BC_REG_INVALID;
        InstructionControl control = CONTROL_NONE;
        InstructionCast cast=CAST_UINT_UINT;
        i64 imm = 0;

        opcode = (InstructionType)instructions[pc];
        pc++;
        
        // _ILOG(log::out <<log::GRAY<<" sp: "<< sp <<" fp: "<<fp<<"\n";)
        if(logging) {
            tinycode->print(prev_pc, prev_pc + 1, bytecode);
            log::out << "\n";
        }

        switch (opcode) {
        case BC_MOV_RR: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            CREATE_INST();

        } break;
        case BC_MOV_RM:
        case BC_MOV_RM_DISP16: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            if(opcode == BC_MOV_RM_DISP16) {
                imm = *(i16*)&instructions[pc];
                pc += 2;
            }
            
            CREATE_INST();
            
        } break;
        case BC_MOV_MR:
        case BC_MOV_MR_DISP16: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            if(opcode == BC_MOV_MR_DISP16) {
                imm = *(i16*)&instructions[pc];
                pc += 2;
            }
            
            CREATE_INST();

        } break;
        case BC_ALLOC_LOCAL: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;

            CREATE_INST();
            
        } break;
        case BC_FREE_LOCAL: {
            imm = *(i16*)&instructions[pc];
            pc+=2;
            
            CREATE_INST();

        } break;
        case BC_PUSH: {
            op0 = (BCRegister)instructions[pc++];
            
            CREATE_INST();
        } break;
        case BC_POP: {
            op0 = (BCRegister)instructions[pc++];
            
            CREATE_INST();
        } break;
        case BC_LI32: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            CREATE_INST();

        } break;
        case BC_ADD: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            CREATE_INST();
        } break;
        case BC_JMP: {
            imm = *(i32*)&instructions[pc];
            pc+=4;
              
            CREATE_INST();
        } break;
        case BC_JZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            CREATE_INST();
        } break;
        case BC_RET: {
            CREATE_INST();
        } break;
        default: Assert(("instruction not handled",false));
        }
    }


    auto IS_ESSENTIAL=[&](InstructionType t) {
        return t == BC_RET
        || t == BC_JZ
        || t == BC_MOV_MR
        || t == BC_MOV_MR_DISP16
        // temporary
        // || t == BC_POP
        || t == BC_PUSH
        ;
    };

    auto IS_GENERAL_REG=[&](BCRegister r) {
        return r != BC_REG_INVALID && r != BC_REG_LOCALS;
    };

    auto ALLOC_REG = [&](X64Inst* n, int nr) {
        X64Operand& op = n->regs[nr];
        BCRegister& r = n->ops[nr];
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
    auto IS_START=[&](InstructionType t) {
        return t == BC_MOV_RM
        || t == BC_MOV_RM_DISP16
        || t == BC_LI32
        || t == BC_MOV_RR
        ;
    };
    auto IS_WALL=[&](InstructionType t) {
        return t == BC_JZ
        || t == BC_RET
        ;
    };

    DynamicArray<int> insts_to_delete{};
    for(int i=inst_list.size()-1;i>=0;i--) {
        auto n = inst_list[i];
        log::out <<log::GOLD<< n->opcode<<"\n";

        if(n->opcode == BC_POP) {
            auto& v = bc_register_map[n->op0];
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
        if(n->opcode == BC_PUSH) {
            auto& v = bc_push_list.last();  // TODO: What if bytecode is messed up and didn't add a pop?
            auto recipient = v.used_by;
            auto reg_nr = v.reg_nr;
            if(recipient) {
                ALLOC_REG(n,0);
            } else {
                insts_to_delete.add(i);
            }

            bc_push_list.pop();
            continue;
        }
        // TODO: ignore LOCAL register
        // TODO: What if two operands refer to the same register
        if(IS_ESSENTIAL(n->opcode)) {
            if(IS_GENERAL_REG(n->op0)) {
                ALLOC_REG(n,0);
                map_reg(n,0);
            }
        } else {
            if(IS_GENERAL_REG(n->op0)) {
                auto& v = bc_register_map[n->op0];
                auto recipient = v.used_by;
                auto reg_nr = v.reg_nr;
                if(recipient) {
                    n->reg0 = recipient->regs[reg_nr]; map_reg(n,0);
                    Assert(!n->reg0.invalid());

                    if(IS_START(n->opcode)) {
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
        if(IS_GENERAL_REG(n->op1)) {
            auto& v = bc_register_map[n->op1];
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
        }
        if(IS_GENERAL_REG(n->op2)) {
            auto& v = bc_register_map[n->op2];
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

    for(auto i : insts_to_delete) {
        auto n = inst_list[i];
        log::out <<log::RED<<"del "<< *n <<"\n";

        inst_list.removeAt(i);
    }

    for(auto n : inst_list) {
        log::out << *n << "\n";
        log::out << log::GRAY <<" "<< n->opcode << " ";
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



    for(auto n : inst_list) {
        // generate x64
        // log::out << n->opcode<<"\n";
    }
}

engone::Logger& operator<<(engone::Logger& l, X64Inst& i) {
    l << engone::log::PURPLE << i.opcode << engone::log::NO_COLOR;
    if(i.op0 != X64_REG_INVALID)
        l << " " << i.op0;
    if(i.op1 != X64_REG_INVALID)
        l << ", " << i.op1;
    if(i.op2 != X64_REG_INVALID)
        l << ", " << i.op2;

    if(i.imm != 0)
        l << ", " << engone::log::GREEN<< i.imm << engone::log::NO_COLOR;

    return l;
}