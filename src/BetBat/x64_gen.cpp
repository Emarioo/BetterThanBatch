
/*
Hello!
    The x64 generator is based on SSA (static single-assignment).
    The bytecode is converted to a tree of nodes where registers and
    the value on the stack determines the relations between the nodes.
    This results in a couple of root nodes which are instructions with
    side effects such as "mov [rbp], eax" or "add rsp, 8".
    x64 registers are allocated and freed when traversing the nodes and generating x64 instructions.
    There will probably be a refactor in the future for further 
    optimized code generation (reuse of values in registers).

When writing code in this file:
    The code for the generator is messy since this is a first proper
    implementation of a decent generator (x64 is also a bit scuffed,
    but with good documentation).

    I urge you to be in a smart and consistent mood when changing the code.
    It is easy to accidently emit the wrong instruction SO be sure to 
    create GREAT test cases so that all edge cases are covered.

    Good luck!

Notes:
    I tried to think of a linear approach (instead of SSA) where you can perform optimizations (dead code elimination, efficient usage of registers)
    but I could not make it work. A bottom-up and then top-down approach with a tree is much easier.

*/

#include "BetBat/x64_gen.h"
#include "BetBat/x64_defs.h"

#include "BetBat/CompilerEnums.h"
#include "BetBat/Compiler.h"

// DO NOT REMOVE NATIVE REGISTER FUNCTIONS JUST BECAUSE WE ONLY
// HAVE BC_REG_LOCALS. YES it's just one register BUT WHO KNOWS
// IF WE WILL HAVE MORE IN THE FUTURE!
X64Register ToNativeRegister(BCRegister reg) {
    // if(reg == BC_REG_SP) return X64_REG_SP; // nocheckin
    if(reg == BC_REG_LOCALS) return X64_REG_BP;
    return X64_REG_INVALID;
}
bool IsNativeRegister(BCRegister reg) {
    return ToNativeRegister(reg) != X64_REG_INVALID;
}
bool IsNativeRegister(X64Register reg) {
    return reg == X64_REG_BP || reg == X64_REG_SP;
}

void GenerateX64_finalize(Compiler* compiler) {
    Assert(compiler->program); // don't call this function if there is no program
    Assert(compiler->program->debugInformation); // we expect debug information?

    auto prog = compiler->program;
    auto bytecode = compiler->bytecode;

    prog->index_of_main = bytecode->index_of_main;

    // prog->debugInformation = bytecode->debugInformation;
    // bytecode->debugInformation = nullptr;

    if(bytecode->dataSegment.size() != 0) {
        prog->globalData = TRACK_ARRAY_ALLOC(u8, bytecode->dataSegment.size());
        // prog->globalData = (u8*)engone::Allocate(bytecode->dataSegment.used);
        Assert(prog->globalData);
        prog->globalSize = bytecode->dataSegment.size();
        memcpy(prog->globalData, bytecode->dataSegment.data(), prog->globalSize);

        // OutputAsHex("data.txt", (char*)prog->globalData, prog->globalSize);
    }

    for(int i=0;i<compiler->bytecode->exportedFunctions.size();i++) {
        auto& sym = compiler->bytecode->exportedFunctions[i];
        prog->addExportedSymbol(sym.name, sym.tinycode_index);
        // X64Program::ExportedSymbol tmp{};
        // tmp.name = sym.name;
        // tmp.textOffset = addressTranslation[sym.location];
        // prog->exportedSymbols.add(tmp);
    }
    
    Assert(compiler->bytecode->externalRelocations.size() == 0);
}

void GenerateX64(Compiler* compiler, TinyBytecode* tinycode) {
    using namespace engone;
    ZoneScopedC(tracy::Color::SkyBlue1);
    
    _VLOG(log::out <<log::BLUE<< "x64 Converter:\n";)
    
    // steal debug information
    // prog->debugInformation = bytecode->debugInformation;
    // bytecode->debugInformation = nullptr;
    
    // make sure dependencies have been fixed first
    bool yes = tinycode->applyRelocations(compiler->bytecode);
    if(!yes) {
        log::out << "Incomplete call relocation\n";
        return;
    }

    X64Builder builder{};
    builder.init(compiler->program);
    builder.bytecode = compiler->bytecode;
    builder.tinycode = tinycode;
    builder.tinyprog = compiler->program->createProgram(tinycode->index);
    builder.current_tinyprog_index = tinycode->index;

    // builder.generateFromTinycode(builder.bytecode, builder.tinycode);
    builder.generateFromTinycode_v2(builder.bytecode, builder.tinycode);

    for(auto n : builder.all_nodes) {
        builder.destroyNode(n);
    }
    builder.all_nodes.cleanup();
}
#ifdef gone
void X64Builder::generateFromTinycode(Bytecode* bytecode, TinyBytecode* tinycode) {
    using namespace engone;
    this->bytecode = bytecode;
    this->tinycode = tinycode;
    // instruction_indices.clear();

    auto& instructions = tinycode->instructionSegment;
    bool logging = false;
    
    // log::out << log::GOLD << "Code gen\n";
    
    DynamicArray<Arg> accessed_params;
    int test_instructions = 0;

    bool find_push = false;
    int push_level = 0;
    bool complete = false;
    i64 pc = 0;
    while(!complete) {
        if(!(pc < instructions.size()))
            break;
        i64 prev_pc = pc;
        // i64 inst_index = next_inst_index;
        
        InstructionOpcode opcode{};
        BCRegister op0=BC_REG_INVALID, op1=BC_REG_INVALID, op2=BC_REG_INVALID;
        InstructionControl control;
        i64 imm = 0;

        opcode = (InstructionOpcode)instructions[pc];
        pc++;
        
        // _ILOG(log::out <<log::GRAY<<" sp: "<< sp <<" fp: "<<fp<<"\n";)
        if(logging) {
            tinycode->print(prev_pc, prev_pc + 1, bytecode);
            log::out << "\n";
        }

        switch (opcode) {
        case BC_HALT: {
            INCOMPLETE
        } break;
        case BC_NOP: {
            INCOMPLETE
        } break;
        case BC_MOV_RR: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->op1 = op1;

            if(IsNativeRegister(op0)) {
                // setting a native register is a side effect
                // we can't use reorder the action
                
                if(!IsNativeRegister(op1)) {
                    node->in1 = reg_values[op1];
                }
                nodes.add(node);
            } else {
                if(!IsNativeRegister(op1)) {
                    node->in1 = reg_values[op1];
                }
                reg_values[op0] = node;
            }
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
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->op1 = op1;
            node->control = control;
            node->imm = imm;
            
            if(!IsNativeRegister(op1))
                node->in1 = reg_values[op1];
            reg_values[op0] = node;
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
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->op1 = op1;
            node->control = control;
            node->imm = imm;
            
            if(!IsNativeRegister(op0)) {
                node->in0 = reg_values[op0];
                reg_values[op0] = nullptr;
            }
            node->in1 = reg_values[op1];
            // reg_values[op1] = nullptr; // nocheckin, fixed with computed_env?
            
            nodes.add(node);
        } break;
        case BC_ALLOC_LOCAL: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->imm = imm;
            if(op0 == BC_REG_INVALID) {
                nodes.add(node);
            } else {
                // Assert(false);
                // nochecking, alloc local is a side effect and must be ordered correctly
                // but it gives an output so we can't really?
                reg_values[op0] = node;
            }
        } break;
        case BC_FREE_LOCAL: {
            imm = *(i16*)&instructions[pc];
            pc+=2;
            
            auto node = createNode(prev_pc, opcode);
            node->imm = imm;
            
            nodes.add(node);
        } break;
        case BC_SET_ARG:
        case BC_SET_RET: {
            op0 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->imm = imm;
            node->control = control;

            node->in0 = reg_values[op0];
            
            nodes.add(node);
        } break;
        case BC_GET_PARAM:
        case BC_GET_VAL: {
            op0 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->imm = imm;
            node->control = control;

            reg_values[op0] = node;

            // this code is only relevant for STDCALL and UNIXCALL
            if(opcode == BC_GET_PARAM) {
                int param_index = imm / 8;
                if(param_index >= accessed_params.size()) {
                    accessed_params.resize(param_index+1);
                }
                accessed_params[param_index].control = control;
            }
        } break;
        case BC_PTR_TO_LOCALS:
        case BC_PTR_TO_PARAMS: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i16*)&instructions[pc];
            pc+=2;
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->imm = imm;

            reg_values[op0] = node;
        } break;
        case BC_PUSH: {
            op0 = (BCRegister)instructions[pc++];
            Assert(!IsNativeRegister(op0));
            
            auto n = reg_values[op0];
            stack_values.add(n);
        } break;
        case BC_POP: {
            op0 = (BCRegister)instructions[pc++];
            Assert(!IsNativeRegister(op0));
            
            auto n = stack_values.last();
            stack_values.pop();
            reg_values[op0] = n;
        } break;
        case BC_LI32: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = createNode(prev_pc, opcode);
            node->imm = imm;
            reg_values[op0] = node;
        } break;
        case BC_LI64: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i64*)&instructions[pc];
            pc+=8;
            
            auto node = createNode(prev_pc, opcode);
            node->imm = imm;
            reg_values[op0] = node;
        } break;
        case BC_INCR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->imm = imm;
            
            node->in0 = reg_values[op0];
            reg_values[op0] = node;
        } break;
        case BC_JMP: {
            imm = *(i32*)&instructions[pc];
            pc+=4;
              
            auto node = createNode(prev_pc, opcode);
            node->imm = imm;
            nodes.add(node);
        } break;
        case BC_JZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->imm = imm;
            
            node->in0 = reg_values[op0];

            reg_values[op0] = nullptr;
            
            nodes.add(node);
        } break;
        case BC_JNZ: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->imm = imm;
            
            node->in0 = reg_values[op0];

            reg_values[op0] = nullptr;
            
            nodes.add(node);
        } break;
        case BC_CALL: {
            LinkConventions l = (LinkConventions)instructions[pc++];
            CallConventions c = (CallConventions)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;

            auto node = createNode(prev_pc, opcode);
            node->link = l;
            node->call = c;
            node->imm = imm;
            
            nodes.add(node);
        } break;
        case BC_RET: {
            auto node = createNode(prev_pc, opcode);
            
            nodes.add(node);
        } break;
        case BC_DATAPTR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->imm = imm;
            
            reg_values[op0] = node;
        } break;
        case BC_CODEPTR: {
            op0 = (BCRegister)instructions[pc++];
            imm = *(i32*)&instructions[pc];
            pc+=4;
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->imm = imm;
            
            reg_values[op0] = node;
        } break;
        case BC_CAST: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];

            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->op1 = op1;
            node->control = control;
            
            node->in0 = reg_values[op0];
            reg_values[op0] = node;
        } break;
        case BC_MEMZERO: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            imm = (u8)instructions[pc++];
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->op1 = op1;
            node->imm = imm;
            
            node->in0 = reg_values[op0];
            node->in1 = reg_values[op1];

            reg_values[op0] = nullptr;
            reg_values[op1] = nullptr;
            
            nodes.add(node);
        } break;
        case BC_MEMCPY: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            op2 = (BCRegister)instructions[pc++];
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->op1 = op1;
            node->op2 = op2;
            
            node->in0 = reg_values[op0];
            node->in1 = reg_values[op1];
            node->in2 = reg_values[op2];

            reg_values[op0] = nullptr;
            reg_values[op1] = nullptr;
            reg_values[op2] = nullptr;
            
            nodes.add(node);
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
            
            auto node = createNode(prev_pc, opcode);
            node->op0 = op0;
            node->op1 = op1;
            node->control = control;
            node->in0 = reg_values[op0];
            node->in1 = reg_values[op1];
            
            reg_values[op0] = node;
        } break;
        case BC_LAND:
        case BC_LOR:
        case BC_BAND:
        case BC_BOR:
        case BC_BXOR:
        case BC_BLSHIFT:
        case BC_BRSHIFT: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            auto node = createNode(prev_pc, opcode);
            node->opcode = opcode;
            node->op0 = op0;
            node->op1 = op1;
            node->in0 = reg_values[op0];
            node->in1 = reg_values[op1];
            
            reg_values[op0] = node;
        } break;
        case BC_LNOT:
        case BC_BNOT: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            auto n = createNode(prev_pc, opcode);
            n->opcode = opcode;
            n->op0 = op0;
            n->op1 = op1;
            // n->in0 = reg_values[op0];
            n->in1 = reg_values[op1];
            
            reg_values[op0] = n;
        } break;
        case BC_RDTSC: {
            op0 = (BCRegister)instructions[pc++];
            
            auto n = createNode(prev_pc, opcode);
            n->opcode = opcode;
            n->op0 = op0;
            
            reg_values[op0] = n;
        } break;
        case BC_ROUND:
        case BC_SQRT: {
            op0 = (BCRegister)instructions[pc++];
            
            auto n = createNode(prev_pc, opcode);
            n->opcode = opcode;
            n->op0 = op0;
            n->in0 = reg_values[op0];
            
            reg_values[op0] = n;
        } break;
        case BC_STRLEN: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            
            auto n = createNode(prev_pc, opcode);
            n->opcode = opcode;
            n->op0 = op0;
            n->op1 = op1;
            n->in1 = reg_values[op1];
            
            reg_values[op0] = n;
        } break;
        case BC_ATOMIC_ADD: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)instructions[pc++];
            
            auto n = createNode(prev_pc, opcode);
            n->opcode = opcode;
            n->op0 = op0;
            n->op1 = op1;
            n->control = control;
            n->in0 = reg_values[op0];
            n->in1 = reg_values[op1];
            
            reg_values[op0] = n;
        } break;
         case BC_ATOMIC_CMP_SWAP: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            op2 = (BCRegister)instructions[pc++];
            
            auto n = createNode(prev_pc, opcode);
            n->opcode = opcode;
            n->op0 = op0;
            n->op1 = op1;
            n->op2 = op2;
            n->in0 = reg_values[op0];
            n->in1 = reg_values[op1];
            n->in2 = reg_values[op2];
            
            reg_values[op0] = n;
        } break;
        case BC_TEST_VALUE: {
            op0 = (BCRegister)instructions[pc++];
            op1 = (BCRegister)instructions[pc++];
            control = (InstructionControl)(u8)instructions[pc++];
            imm = *(u32*)&instructions[pc];
            pc+=4;
            
            auto n = createNode(prev_pc, opcode);
            n->opcode = opcode;
            n->op0 = op0;
            n->op1 = op1;
            n->control = control;
            n->imm = imm;

            n->in0 = reg_values[op0];
            n->in1 = reg_values[op1];
            nodes.add(n);
            test_instructions++;
        } break;
        default: Assert(false);
        }
    }
    
    // Generate x64 from OPNode tree

    // DynamicArray<i32> _bc_to_x64_translation_direct;
    // _bc_to_x64_translation.resize(tinycode->size());
    
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
    
   
    QuickArray<X64Env*> envs;

    // for(auto& n : nodes) {
    //     log::out << "Root " << n->bc_index << " "<<instruction_names[n->opcode] << "\n";
    // }
    
    int node_i = 0;

    bool pop_env = true;
    X64Env* env = nullptr;
    OPNode* n = nullptr;

    auto createEnv = [&]() {
        auto ptr = TRACK_ALLOC(X64Env);
        new(ptr)X64Env();
        return ptr;
    };
    auto destroyEnv = [&](X64Env* env) {
        env->~X64Env();
        TRACK_FREE(env, X64Env);
    };

    auto push_env0 = [&]() {
        Assert(n->in0);
        // This may happen when to two root nodes refer to the same register.
        // You can remove these lines: "value_registers[op0] = nullptr" somewhere
        //  above but then the produced code may not execute properly.
        // You can also fix it by making sure the generator doesn't produce
        //  such cases.
        // Ideally, we want a better representation than the tree for the code.

        auto e = createEnv();
        e->node = n->in0;
        env->env_in0 = e;

        envs.add(e);
        pop_env = false;
        return e;
    };
    auto push_env1 = [&]() {
        Assert(n->in1);

        auto e = createEnv();
        e->node = n->in1;

        env->env_in1 = e;

        envs.add(e);
        pop_env = false;
        return e;
    };
     auto push_env2 = [&]() {
        Assert(n->in2);
        auto e = createEnv();
        env->env_in2 = e;
        e->node = n->in2;

        envs.add(e);
        pop_env = false;
        return e;
    };

        // if(e->reg0.invalid()) {                                           
    #define INHERIT_REG(R)                                                \
            e->reg0 = env->R;                                                 \
            if(e->reg0.invalid()) {                                           \
                if(IS_CONTROL_FLOAT(n->control)) {                            \
                    e->reg0.reg = alloc_register(X64_REG_INVALID, true);      \
                    if(e->reg0.reg == X64_REG_INVALID)                        \
                        e->reg0.on_stack = true;                              \
                } else {                                                      \
                }                                                             \
            }                                                                 
        // }                                                                                                                      

    auto REQUEST_REG0 = [&](bool is_float = false) {
        if(env->reg0.invalid()) {
            env->reg0.reg = alloc_register(X64_REG_INVALID, is_float);
            if(env->reg0.reg == X64_REG_INVALID) {
                env->reg0.on_stack = true;
                env->reg0.reg = RESERVED_REG0;
            }
        } else {
            // already allocated
        }
    };

    const int INPUT0 = 0x1;
    const int INPUT1 = 0x2;
    const int INPUT2 = 0x4;
    #define COMPUTE_INPUTS(N) if(!m_COMPUTE_INPUTS(N)) break
    auto m_COMPUTE_INPUTS = [&](int inputs) {
        // TODO: Node depth?
        // int depth0 = get_node_depth(n->in0);
        // int depth1 = get_node_depth(n->in1);
        if(inputs & INPUT0)
            if(!env->env_in0) {
                if(n->in0->computed_env) {
                    env->env_in0 = n->in0->computed_env;
                } else {
                    auto e = push_env0();
                    INHERIT_REG(reg0)
                    return false;
                }
            }
        if(inputs & INPUT1)
            if(!env->env_in1) {
                if(n->in1->computed_env) {
                    env->env_in1 = n->in1->computed_env;
                } else {
                    auto e = push_env1();
                    INHERIT_REG(reg1)
                    return false;
                }
            }
        if(inputs & INPUT2)
            if(!env->env_in2) {
                if(n->in2->computed_env) {
                    env->env_in2 = n->in2->computed_env;
                } else {
                    auto e = push_env2();
                    INHERIT_REG(reg2)
                    return false;
                }
            }

        if(inputs & INPUT0)
            env->reg0 = env->env_in0->reg0;
        if(inputs & INPUT1)
            env->reg1 = env->env_in1->reg0;
        if(inputs & INPUT2)
            env->reg2 = env->env_in2->reg0;
        
        if(inputs & INPUT0)
            if(env->reg0.on_stack) {
                Assert(!n->in0->computed_env); // values are not pushed with computed_env 
                env->reg0.reg = RESERVED_REG0;
                emit_pop(env->reg0.reg);
            }
        if(inputs & INPUT1)
            if(env->reg1.on_stack) {
                Assert(!n->in1->computed_env); // values are not pushed with computed_env 
                env->reg1.reg = RESERVED_REG2;
                emit_pop(env->reg1.reg);
            }
        if(inputs & INPUT2)
            if(env->reg2.on_stack) {
                Assert(!n->in2->computed_env); // values are not pushed with computed_env 
                env->reg2.reg = RESERVED_REG2;
                emit_pop(env->reg2.reg);
            }
        return true;
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

    int root_pc = 0;
    while(true) {
        bool root = false;
        if(envs.size() == 0) {
            if(node_i >= nodes.size())
                break;
                
            envs.add(createEnv());
            envs.last()->node = nodes[node_i];
            node_i++;

            root_pc = code_size();
            root = true;
        }
        
        env = envs.last();
        n = env->node;
        pop_env = true;
        last_pc = code_size();

        Assert(!n->computed_env);
        
        defer {
            if(pop_env || root) {
                if(get_map_translation(n->bc_index) == 0) {
                    map_translation(n->bc_index, root_pc);
                    if(root)
                        log::out << log::CYAN << "Root";
                    // log::out << " map " <<n->opcode << " "<<n->bc_index<<" -> "<<NumberToHex(size(),true)<<"\n";
                    log::out << " map " <<n->opcode << " "<<n->bc_index<<" -> "<<NumberToHex(get_map_translation(n->bc_index),true)<<"\n";
                } else {
                    log::out <<log::GRAY<< " prevmap " <<n->opcode << " "<<n->bc_index<<" -> "<<NumberToHex(get_map_translation(n->bc_index),true)<<"\n";
                }
            }
        };

        // li a, 5
        // li b, 3
        // li c, 2
        // add a, c
        // add b, c
        // mov [bp], a
        // mov [bp], b

        // mov [bp], a -> add a, c -> li a, 5 -> li c, 2

        // mov [bp], b- > add b, c -> li b, 5 -> li c, 2
        
        switch(n->opcode) {
            case BC_BXOR: {
                if(n->opcode == BC_BXOR && n->op0 == n->op1) {
                    // xor clear on register
                    REQUEST_REG0();

                    emit_prefix(PREFIX_REXW,env->reg0.reg, env->reg0.reg);
                    emit1(OPCODE_XOR_REG_RM);
                    emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg0.reg));
                    
                    if(env->reg0.on_stack) {
                        emit_push(env->reg0.reg);
                    }
                    break;
                }
                // fall through
            }
            case BC_ADD:
            case BC_SUB:
            case BC_MUL: 
            
            case BC_LAND:
            case BC_LOR:
            // case BC_LNOT: // unary operator, "reg = !reg" instead of "reg = reg ! reg"
            case BC_BAND:
            case BC_BOR:
            // case BC_BNOT: // unary operator
            case BC_BLSHIFT:
            case BC_BRSHIFT:

            case BC_EQ:
            case BC_NEQ:
            case BC_LT:
            case BC_LTE:
            case BC_GT:
            case BC_GTE:
            {
                Assert(!(n->opcode == BC_BXOR && n->op0 == n->op1)); // just making sure we don't do a bxor because we don't handle those here
                
                COMPUTE_INPUTS(INPUT0|INPUT1);
                
                bool is_unsigned = !IS_CONTROL_SIGNED(n->control);
                if(IS_CONTROL_FLOAT(n->control)) {
                    switch(n->opcode) {
                        case BC_ADD: {
                            InstructionControl size = GET_CONTROL_SIZE(n->control);
                            // Assert(size == 4 || size == 8);
                            Assert(size == CONTROL_32B);
                            // TODO: We assume 32-bit float NOT GOOD
                            emit3(OPCODE_3_ADDSS_REG_RM);
                            // TODO: verify float register
                            emit_modrm(MODE_REG, CLAMP_XMM(env->reg1.reg), CLAMP_XMM(env->reg0.reg));
                        } break;
                        case BC_SUB: {
                            Assert(false);
                            // emit1(PREFIX_REXW);
                            // emit1(OPCODE_SUB_REG_RM);
                            // emit_modrm(MODE_REG, env->reg0, env->reg1);
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
                            //     emit_modrm(MODE_REG, X64_REG_A, env->reg0);
                                
                            //     emit1(PREFIX_REXW);
                            //     emit1(OPCODE_MUL_AX_RM_SLASH_4);
                            //     emit_modrm_slash(MODE_REG, 4, env->reg1);
                                
                            //     emit1(PREFIX_REXW);
                            //     emit1(OPCODE_MOV_REG_RM);
                            //     emit_modrm(MODE_REG, env->reg0, X64_REG_A);
                                
                            //     if(!a_is_free) {
                            //         emit_pop(X64_REG_A);
                            //     }
                            //     if(!d_is_free) {
                            //         emit_pop(X64_REG_D);
                            //     }
                            // } else {
                            //     emit1(PREFIX_REXW);
                            //     emit2(OPCODE_2_IMUL_REG_RM);
                            //     emit_modrm(MODE_REG, env->reg1, env->reg0);
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

                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg1.reg);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, X64_REG_C, CLAMP_EXT_REG(env->reg1.reg));
                            }

                            emit1(PREFIX_REXW);
                            switch(n->opcode) {
                                case BC_BLSHIFT: {
                                    emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                                    emit1(OPCODE_SHL_RM_CL_SLASH_4);
                                    emit_modrm_slash(MODE_REG, 4, CLAMP_EXT_REG(env->reg0.reg));
                                } break;
                                case BC_BRSHIFT: {
                                    emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                                    emit1(OPCODE_SHR_RM_CL_SLASH_5);
                                    emit_modrm_slash(MODE_REG, 5, CLAMP_EXT_REG(env->reg0.reg));
                                } break;
                                default: Assert(false);
                            }

                            if(c_was_used) {
                                emit_pop(X64_REG_C);
                            }
                        } break;
                        case BC_EQ: {
                            Assert(false);
                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_CMP_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                            
                            emit_prefix(0,X64_REG_INVALID, env->reg0.reg);
                            emit2(OPCODE_2_SETE_RM8);
                            emit_modrm_slash(MODE_REG, 0, env->reg0.reg);

                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg0.reg);
                            emit2(OPCODE_2_MOVZX_REG_RM8);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg0.reg));
                        } break;
                        case BC_NEQ: {
                            Assert(false);
                            // IMPORTANT: THERE MAY BE BUGS IF YOU COMPARE OPERANDS OF DIFFERENT SIZES.
                            //  SOME BITS MAY BE UNINITIALZIED.
                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                        } break;
                        case BC_LT:
                        case BC_LTE:
                        case BC_GT:
                        case BC_GTE: {
                            Assert(false);
                            emit_prefix(PREFIX_REXW,env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_CMP_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));

                            u16 setType = 0;
                            if(!is_unsigned) {
                                switch(n->opcode) {
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
                                switch(n->opcode) {
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
                            
                            emit_prefix(0, X64_REG_INVALID, env->reg0.reg);
                            emit2(setType);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));
                            
                            // do we need more stuff or no? I don't think so?
                            // prog->add2(OPCODE_2_MOVZX_REG_RM8);
                            // emit_modrm(MODE_REG, reg2, reg2);

                        } break;
                        default: Assert(false);
                    }
                } else {
                    switch(n->opcode) {
                        case BC_ADD: {
                            emit_prefix(PREFIX_REXW,env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_ADD_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                        } break;
                        case BC_SUB: {
                            emit_prefix(PREFIX_REXW,env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_SUB_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
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
                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, X64_REG_A, CLAMP_EXT_REG(env->reg0.reg));
                                
                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg1.reg);
                                emit1(OPCODE_MUL_AX_RM_SLASH_4);
                                emit_modrm_slash(MODE_REG, 4, CLAMP_EXT_REG(env->reg1.reg));
                                
                                emit_prefix(PREFIX_REXW, env->reg0.reg, X64_REG_INVALID);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), X64_REG_A);
                                
                                if(!a_is_free) {
                                    emit_pop(X64_REG_A);
                                }
                                if(!d_is_free) {
                                    emit_pop(X64_REG_D);
                                }
                            } else {
                                emit_prefix(PREFIX_REXW, env->reg1.reg, env->reg0.reg);
                                emit2(OPCODE_2_IMUL_REG_RM);
                                emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg1.reg), CLAMP_EXT_REG(env->reg0.reg));
                            }
                        } break;
                        case BC_LAND: {
                            // AND = !(!r0 || !r1)

                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg0.reg);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg0.reg));

                            emit1(OPCODE_JZ_IMM8);
                            int offset_jmp0_imm = size();
                            emit1((u8)0);

                            emit_prefix(PREFIX_REXW, env->reg1.reg, env->reg1.reg);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg1.reg), CLAMP_EXT_REG(env->reg1.reg));
                            
                            emit1(OPCODE_JZ_IMM8);
                            int offset_jmp1_imm = size();
                            emit1((u8)0);

                            emit_prefix(0,X64_REG_INVALID, env->reg0.reg);
                            emit1(OPCODE_MOV_RM_IMM8_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));
                            emit1((u8)1);

                            emit1(OPCODE_JMP_IMM8);
                            int offset_jmp2_imm = size();
                            emit1((u8)0);

                            fix_jump_here_imm8(offset_jmp0_imm);
                            fix_jump_here_imm8(offset_jmp1_imm);

                            emit_prefix(0,X64_REG_INVALID, env->reg0.reg);
                            emit1(OPCODE_MOV_RM_IMM8_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));
                            emit1((u8)0);

                            fix_jump_here_imm8(offset_jmp2_imm);
                        } break;
                        case BC_LOR: {
                            // OR = (r0==0 || r1==0)

                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg0.reg);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg0.reg));

                            emit1(OPCODE_JNZ_IMM8);
                            int offset_jmp0_imm = size();
                            emit1((u8)0);

                            emit_prefix(PREFIX_REXW, env->reg1.reg, env->reg1.reg);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg1.reg), CLAMP_EXT_REG(env->reg1.reg));
                            
                            emit1(OPCODE_JNZ_IMM8);
                            int offset_jmp1_imm = size();
                            emit1((u8)0);

                            emit_prefix(0,X64_REG_INVALID, env->reg0.reg);
                            emit1(OPCODE_MOV_RM_IMM8_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));
                            emit1((u8)0);

                            emit1(OPCODE_JMP_IMM8);
                            int offset_jmp2_imm = size();
                            emit1((u8)0);

                            fix_jump_here_imm8(offset_jmp0_imm);
                            fix_jump_here_imm8(offset_jmp1_imm);

                            emit_prefix(0,X64_REG_INVALID, env->reg0.reg);
                            emit1(OPCODE_MOV_RM_IMM8_SLASH_0);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));
                            emit1((u8)1);

                            fix_jump_here_imm8(offset_jmp2_imm);
                        } break;
                        case BC_LNOT: {
                            emit_prefix(PREFIX_REXW, env->reg1.reg, env->reg1.reg);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg1.reg), CLAMP_EXT_REG(env->reg1.reg));

                            emit2(OPCODE_2_SETE_RM8); // SETZ/SETE
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));
                        } break;
                        case BC_BAND: {
                            emit_prefix(PREFIX_REXW, env->reg1.reg, env->reg0.reg);
                            Assert(false);
                            // emit1(OPCODE_AND_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg1.reg), CLAMP_EXT_REG(env->reg0.reg));
                        } break;
                        case BC_BOR: {
                            emit_prefix(PREFIX_REXW, env->reg1.reg, env->reg0.reg);
                            Assert(false);
                            // emit1(OPCODE_OR_RM_REG);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg1.reg), CLAMP_EXT_REG(env->reg0.reg));
                        } break;
                        case BC_BXOR: {
                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                        } break;
                        case BC_BLSHIFT:
                        case BC_BRSHIFT: {
                            bool c_was_used = false;
                            X64Register reg = env->reg0.reg;
                            if(env->reg1.reg == X64_REG_C) {

                            } else if(!is_register_free(X64_REG_C)) {
                                c_was_used = true;
                                if(env->reg0.reg == X64_REG_C) {
                                    reg = RESERVED_REG0;
                                    emit_mov_reg_reg(reg, X64_REG_C);
                                } else {
                                    emit_push(X64_REG_C);
                                }
                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg1.reg);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, X64_REG_C, CLAMP_EXT_REG(env->reg1.reg));
                            }

                            switch(n->opcode) {
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
                                if(env->reg0.reg == X64_REG_C) {
                                    emit_mov_reg_reg(X64_REG_C, reg);
                                } else {
                                    emit_pop(X64_REG_C);
                                }
                            }
                        } break;
                        case BC_EQ: {
                            if(GET_CONTROL_SIZE(n->control) == CONTROL_8B) {
                                emit_prefix(0, env->reg0.reg, env->reg1.reg);
                                emit1(OPCODE_CMP_REG8_RM8);
                            } else {
                                if(GET_CONTROL_SIZE(n->control) == CONTROL_16B) {
                                    emit1(PREFIX_16BIT);
                                    emit_prefix(0, env->reg0.reg, env->reg1.reg);
                                } else if(GET_CONTROL_SIZE(n->control) == CONTROL_32B) {
                                    emit_prefix(0, env->reg0.reg, env->reg1.reg);
                                }  else if(GET_CONTROL_SIZE(n->control) == CONTROL_64B) {
                                    emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                                }
                                emit1(OPCODE_CMP_REG_RM);
                            }
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));

                            emit_prefix(0,X64_REG_INVALID, env->reg0.reg);
                            emit2(OPCODE_2_SETE_RM8);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));

                            emit_movzx(env->reg0.reg,env->reg0.reg,CONTROL_8B);
                        } break;
                        case BC_NEQ: {
                            if(GET_CONTROL_SIZE(n->control) == CONTROL_8B) {
                                emit_prefix(0, env->reg0.reg, env->reg1.reg);
                                emit1(OPCODE_XOR_REG8_RM8);
                            } else {
                                if(GET_CONTROL_SIZE(n->control) == CONTROL_16B) {
                                    emit1(PREFIX_16BIT);
                                    emit_prefix(0, env->reg0.reg, env->reg1.reg);
                                } else if(GET_CONTROL_SIZE(n->control) == CONTROL_32B) {
                                    emit_prefix(0, env->reg0.reg, env->reg1.reg);
                                }  else if(GET_CONTROL_SIZE(n->control) == CONTROL_64B) {
                                    emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                                }
                                emit1(OPCODE_XOR_REG_RM);
                            }
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                        } break;
                        case BC_LT:
                        case BC_LTE:
                        case BC_GT:
                        case BC_GTE: {
                            if(GET_CONTROL_SIZE(n->control) == CONTROL_8B) {
                                // we used REXW so that we access low bits of R8-R15 and not high bits of ax, cx (ah,ch)
                                emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                                emit1(OPCODE_CMP_REG8_RM8);
                                emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                            } else {
                                if(GET_CONTROL_SIZE(n->control) == CONTROL_16B) {
                                    emit1(PREFIX_16BIT);
                                    emit_prefix(0, env->reg0.reg, env->reg1.reg);
                                } else if(GET_CONTROL_SIZE(n->control) == CONTROL_32B) {
                                    emit_prefix(0, env->reg0.reg, env->reg1.reg);
                                }  else if(GET_CONTROL_SIZE(n->control) == CONTROL_64B) {
                                    emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                                }
                                emit1(OPCODE_CMP_REG_RM);
                                emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));
                            }

                            u16 setType = 0;
                            if(!is_unsigned) {
                                switch(n->opcode) {
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
                                switch(n->opcode) {
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
                            
                            emit_prefix(0, X64_REG_INVALID, env->reg0.reg);
                            emit2(setType);
                            emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));
                            
                            // do we need more stuff or no? I don't think so?
                            emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg0.reg);
                            emit2(OPCODE_2_MOVZX_REG_RM8);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg0.reg));
                        } break;
                        default: Assert(false);
                    }
                }
                
                if(!env->reg1.on_stack)
                    free_register(env->reg1.reg);
                    
                if(env->reg0.on_stack) {
                    emit_pop(env->reg0.reg);
                }
            } break;
            case BC_DIV:
            case BC_MOD: {
                COMPUTE_INPUTS(INPUT0|INPUT1);
                // int depth0 = get_node_depth(n->in0);
                // int depth1 = get_node_depth(n->in1);
                // if(!env->env_in0 && !env->env_in1) {
                //     // NOTE: Evaluate the node with the most calculations and register allocations first so that we don't unnecesarily allocate registers and push values to the stack. We do the easiest work first.
                //     // TODO: Optimize get_node_depth, recalculating depth like this is expensive. (calculate once and reuse for parent and child nodes)

                //     if(depth0 < depth1) {
                //         if(!env->env_in0) {
                //             auto e = push_env0();
                //             INHERIT_REG(reg0)

                //         }
                //         if(!env->env_in1) {
                //             auto e = push_env1();
                //             INHERIT_REG(reg1)
                //         }
                //     } else {
                //         if(!env->env_in1) {
                //             auto e = push_env1();
                //             INHERIT_REG(reg1)
                //         }
                //         if(!env->env_in0) {
                //             auto e = push_env0();
                //             INHERIT_REG(reg0)
                //         }
                //     }
                //     Assert(!pop_env);
                //     break;
                // }
                
                // env->reg0 = env->env_in0->reg0;
                // env->reg1 = env->env_in1->reg0;

                // if(depth0 < depth1) {
                //     if(env->reg0.on_stack) {
                //         env->reg0.reg = RESERVED_REG1;
                //         emit_pop(env->reg0.reg);
                //     }
                //     if(env->reg1.on_stack) {
                //         env->reg1.reg = RESERVED_REG0;
                //         emit_pop(env->reg1.reg);
                //     }
                // } else {
                //     if(env->reg1.on_stack) {
                //         env->reg1.reg = RESERVED_REG0;
                //         emit_pop(env->reg1.reg);
                //     }
                //     if(env->reg0.on_stack) {
                //         env->reg0.reg = RESERVED_REG1;
                //         emit_pop(env->reg0.reg);
                //     }
                // }
                
                if(IS_CONTROL_FLOAT(n->control)) {
                    Assert(false);
                } else {
                    switch(n->opcode) {
                        case BC_DIV: {
                            // TODO: Handled signed/unsigned

                            bool d_is_free = is_register_free(X64_REG_D);
                            if(!d_is_free) {
                                emit_push(X64_REG_D);
                            }
                            emit_prefix(PREFIX_REXW, X64_REG_INVALID, X64_REG_INVALID);
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, X64_REG_D, X64_REG_D);
                            
                            bool a_is_free = true;
                            if(env->reg0.reg != X64_REG_A) {
                                a_is_free = is_register_free(X64_REG_A);
                                if(!a_is_free) {
                                    emit_push(X64_REG_A);
                                }
                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, X64_REG_A, CLAMP_EXT_REG(env->reg0.reg));
                            }

                            emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg1.reg);
                            emit1(OPCODE_DIV_AX_RM_SLASH_6);
                            emit_modrm_slash(MODE_REG, 6, CLAMP_EXT_REG(env->reg1.reg));
                            
                            if(env->reg0.reg != X64_REG_A) {
                                emit_prefix(PREFIX_REXW, env->reg0.reg, X64_REG_INVALID);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), X64_REG_A);
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

                            bool d_is_free = is_register_free(X64_REG_D);
                            if(!d_is_free) {
                                emit_push(X64_REG_D);
                            }
                            emit_prefix(PREFIX_REXW, X64_REG_INVALID, X64_REG_INVALID);
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, X64_REG_D, X64_REG_D);
                            
                            bool a_is_free = true;
                            if(env->reg0.reg != X64_REG_A) {
                                a_is_free = is_register_free(X64_REG_A);
                                if(!a_is_free) {
                                    emit_push(X64_REG_A);
                                }
                                emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_REG, X64_REG_A, CLAMP_EXT_REG(env->reg0.reg));
                            }

                            emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg1.reg);
                            emit1(OPCODE_DIV_AX_RM_SLASH_6);
                            emit_modrm_slash(MODE_REG, 6, CLAMP_EXT_REG(env->reg1.reg)); 
                            
                            emit_prefix(PREFIX_REXW, env->reg0.reg, X64_REG_INVALID);
                            emit1(OPCODE_MOV_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), X64_REG_D);
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
                
                if(!env->reg1.on_stack)
                    free_register(env->reg1.reg);
                    
                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
            } break;
            case BC_LNOT:
            case BC_BNOT: {
                COMPUTE_INPUTS(INPUT1);
                REQUEST_REG0();
                
                switch(n->opcode) {
                    case BC_LNOT: {
                        emit_prefix(PREFIX_REXW, env->reg1.reg, env->reg1.reg);
                        emit1(OPCODE_TEST_RM_REG);
                        emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg1.reg), CLAMP_EXT_REG(env->reg1.reg));
                        
                        emit_prefix(0, env->reg0.reg, X64_REG_INVALID);
                        emit2(OPCODE_2_SETE_RM8);
                        emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));

                        // not necessary?
                        // emit1(PREFIX_REXW);
                        // emit2(OPCODE_2_MOVZX_REG_RM8);
                        // emit_modrm(MODE_REG, env->reg0, env->reg0);
                    } break;
                    case BC_BNOT: {
                        // TODO: Optimize by using one register. No need to allocate an IN and OUT register. We can then skip the MOV instruction.
                        emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                        emit1(OPCODE_MOV_REG_RM);
                        emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));

                        emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                        emit1(OPCODE_NOT_RM_SLASH_2);
                        emit_modrm_slash(MODE_REG, 2, CLAMP_EXT_REG(env->reg0.reg));
                    } break;
                    default: Assert(false);
                }
                
                if(!env->reg1.on_stack)
                    free_register(env->reg1.reg);
                    
                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
            } break;
            case BC_MOV_RR: {
                COMPUTE_INPUTS(INPUT0);
                REQUEST_REG0(IS_REG_XMM(env->reg1.reg));

                if(IS_REG_XMM(env->reg0.reg)) {
                    if(IS_REG_XMM(env->reg1.reg)) {
                        Assert(false);
                        // How do we know whether a 32-bit float
                        // or 64-bit float should be moved
                        // emit3(OPCODE_3_MOVSS_REG_RM);
                        // emit3(OPCODE_3_MOVSD_REG_RM);
                    } else {
                        Assert(false);
                    }
                } else {
                    if(IS_REG_XMM(env->reg1.reg)) {
                        Assert(false);
                    } else {
                        emit_prefix(PREFIX_REXW, env->reg0.reg, env->reg1.reg);
                        emit1(OPCODE_MOV_REG_RM);
                        emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));

                        if(env->reg0.on_stack) {
                            emit_push(env->reg0.reg);
                        }
                    }

                }
            } break;
            case BC_MOV_RM:
            case BC_MOV_RM_DISP16: {
                if(n->op1 != BC_REG_LOCALS) {
                    COMPUTE_INPUTS(INPUT1);
                } else {
                    env->reg1.reg = ToNativeRegister(n->op1);
                }
                REQUEST_REG0(IS_CONTROL_FLOAT(n->control));

                emit_mov_reg_mem(env->reg0.reg, env->reg1.reg, n->control, n->imm);
                
                if(!env->reg1.on_stack && !IsNativeRegister(env->reg1.reg))
                    free_register(env->reg1.reg);
                
                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
            } break;
            case BC_MOV_MR:
            case BC_MOV_MR_DISP16: {
                int inputs = INPUT1;
                if(n->op0 != BC_REG_LOCALS)
                    inputs |= INPUT0;
                else
                    env->reg0.reg = ToNativeRegister(n->op0);
                COMPUTE_INPUTS(inputs);

                emit_mov_mem_reg(env->reg0.reg, env->reg1.reg, n->control, n->imm);
                
                if(!env->reg0.on_stack && !IsNativeRegister(env->reg0.reg)) free_register(env->reg0.reg);
                if(!env->reg1.on_stack) free_register(env->reg1.reg);
            } break;
            case BC_ALLOC_LOCAL: {
                // Assert(push_offset == 0); // we need array of push_offsets to very equal amounts of push and pops since pushes can happen between alloc locals
                if(n->op0 != BC_REG_INVALID) {
                    REQUEST_REG0();
                }

                emit_sub_imm32(X64_REG_SP, (i32)n->imm);
                if(n->op0 != BC_REG_INVALID) {
                    emit_prefix(PREFIX_REXW, env->reg0.reg, X64_REG_SP);
                    emit1(OPCODE_MOV_REG_RM);
                    emit_modrm(MODE_REG, env->reg0.reg, X64_REG_SP);
                    if(env->reg0.on_stack) {
                        Assert(false);
                        // Potential bug here. If alloc_local is pushed and some expression is
                        // evaluated and more values are pushed then that expression has
                        // to be popped before instructions can get the value from alloc_local
                        // I don't know if this will ever happen since things are generally pushed
                        // and popped in the right order. Alloc local is also a root instruction
                        // so there will always be registers available. But still, possibly a bug.

                        emit_push(env->reg0.reg);
                    }
                }
                push_offset = 0;
            } break;
            case BC_FREE_LOCAL: {
                // Assert(push_offset == 0); nocheckin
                emit_add_imm32(X64_REG_SP, (i32)n->imm);
                ret_offset -= n->imm;
            } break;
            case BC_SET_ARG: {
                COMPUTE_INPUTS(INPUT0);

                // this code is required with stdcall or unixcall
                // betcall ignores this
                int arg_index = n->imm / 8;
                if(arg_index >= recent_set_args.size()) {
                    recent_set_args.resize(arg_index + 1);
                }
                recent_set_args[arg_index].control = n->control;

                int off = n->imm + push_offset;
                X64Register reg_args = X64_REG_SP;
                emit_mov_mem_reg(reg_args, env->reg0.reg, n->control, off);
                
                if(!env->reg0.on_stack)
                    free_register(env->reg0.reg);
            } break;
            case BC_SET_RET: {
                switch(tinycode->call_convention) {
                    case BETCALL: {
                        COMPUTE_INPUTS(INPUT0);
                        
                        X64Register reg_rets = X64_REG_BP;
                        int off = n->imm;

                        emit_mov_mem_reg(reg_rets, env->reg0.reg, n->control, off);
                        
                        if(!env->reg0.on_stack)
                            free_register(env->reg0.reg);
                    } break;
                    case STDCALL:
                    case UNIXCALL: {
                        if(!env->env_in0) {
                            Assert(!env->node->in0->computed_env); // See COMPUTED_INPUTS
                            auto e = push_env0();
                            if(IS_CONTROL_FLOAT(n->control)) {
                                e->reg0.reg = alloc_register(X64_REG_XMM0, true);
                                Assert(e->reg0.reg != X64_REG_INVALID);
                                // if(e->reg0.reg == X64_REG_INVALID)
                                //     e->reg0.on_stack = true;
                            } else {
                                // e->reg0.reg = alloc_register(X64_REG_A, false);
                                e->reg0.reg = X64_REG_A;
                                // Assert(e->reg0.reg != X64_REG_INVALID);
                                    // e->reg0.on_stack = true;
                                // let someone else allocate register
                            }
                            break;
                        }

                        env->reg0 = env->env_in0->reg0;
                        
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
            case BC_GET_PARAM: {
                REQUEST_REG0(IS_CONTROL_FLOAT(n->control));

                X64Register reg_params = X64_REG_BP;
                int off = n->imm + FRAME_SIZE;
                emit_mov_reg_mem(env->reg0.reg, reg_params, n->control, off);
                
                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
            } break;
            case BC_GET_VAL: {
                switch(last_call->call) {
                    case BETCALL: {
                        REQUEST_REG0(IS_CONTROL_FLOAT(n->control));

                        X64Register reg_params = X64_REG_SP;
                        int off = n->imm + push_offset - FRAME_SIZE + ret_offset;
                        emit_mov_reg_mem(env->reg0.reg, reg_params, n->control, off);
                        
                        if(env->reg0.on_stack) {
                            emit_push(env->reg0.reg);
                        }
                    } break;
                    case STDCALL:
                    case UNIXCALL: {
                        // TODO: We hope that an instruction after a call didn't
                        //   overwrite the return value in RAX. We assert if it did.
                        //   Should we reserve RAX after a call until we reach BC_GET_VAL
                        //   and use the return value? No because some function do not
                        //   anything so we would have wasted rax. Then what do we do?

                        // For now we assume RAX has been reserved
                        env->reg0.reg = X64_REG_A;
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
            case BC_PTR_TO_LOCALS: {
                REQUEST_REG0(IS_CONTROL_FLOAT(n->control));

                X64Register reg_local = X64_REG_BP;

                emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));
                emit4((u32)n->imm); // TODO: Need to offset if we pushed non-volatile registers


                emit_prefix(PREFIX_REXW, env->reg0.reg, reg_local);
                emit1(OPCODE_ADD_REG_RM);
                emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), reg_local);

                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
            } break;
            case BC_PTR_TO_PARAMS: {
                REQUEST_REG0(IS_CONTROL_FLOAT(n->control));

                X64Register reg_params = X64_REG_BP;

                emit_prefix(PREFIX_REXW, X64_REG_INVALID, env->reg0.reg);
                emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));
                emit4((u32)(n->imm + FRAME_SIZE));

                emit_prefix(PREFIX_REXW, env->reg0.reg, reg_params);
                emit1(OPCODE_ADD_REG_RM);
                emit_modrm(MODE_REG, CLAMP_EXT_REG(env->reg0.reg), reg_params);

                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
            } break;
            case BC_INCR: {
                COMPUTE_INPUTS(INPUT0);

                if(n->imm < 0) {
                    emit_sub_imm32(env->reg0.reg, (i32)-n->imm);
                } else {
                    emit_add_imm32(env->reg0.reg, (i32)n->imm);
                }
                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
            } break;
            case BC_LI32: {
                REQUEST_REG0();
                
                if(IS_REG_XMM(env->reg0.reg)) {
                    Assert(!env->reg0.on_stack);
                    emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                    emit_modrm_slash(MODE_DEREF_DISP8, 0, X64_REG_SP);
                    emit1((u8)-8);
                    emit4((u32)(i32)n->imm);

                    emit3(OPCODE_3_MOVSS_REG_RM);
                    emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(env->reg0.reg), X64_REG_SP);
                    emit1((u8)-8);

                    Assert(!env->reg0.on_stack);
                } else {
                    // We do not need a 64-bit move to load a 32-bit into a 64-bit register and ensure that the register is cleared.
                    // A 32-bit move will actually clear the upper bits.
                    emit_prefix(0,X64_REG_INVALID, env->reg0.reg);
                    emit1(OPCODE_MOV_RM_IMM32_SLASH_0);
                    emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(env->reg0.reg));
                    emit4((u32)(i32)n->imm);

                    if(env->reg0.on_stack) {
                        emit_push(env->reg0.reg);
                    }
                }
            } break;
            case BC_LI64: {
                REQUEST_REG0();

                if(IS_REG_XMM(env->reg0.reg)) {
                    Assert(!env->reg0.on_stack);

                    Assert(is_register_free(RESERVED_REG0));
                    X64Register tmp_reg = RESERVED_REG0;

                    u8 reg_field = tmp_reg - 1;
                    Assert(reg_field >= 0 && reg_field <= 7);

                    emit1(PREFIX_REXW);
                    emit1((u8)(OPCODE_MOV_REG_IMM_RD_IO | reg_field));
                    emit8((u64)n->imm);

                    emit1(PREFIX_REXW);
                    emit1(OPCODE_MOV_RM_REG);
                    emit_modrm(MODE_DEREF_DISP8, tmp_reg, X64_REG_SP);
                    emit1((u8)-8);

                    emit3(OPCODE_3_MOVSD_REG_RM);
                    emit_modrm(MODE_DEREF_DISP8, CLAMP_EXT_REG(env->reg0.reg), X64_REG_SP);
                    emit1((u8)-8);
                } else {
                    u8 reg_field = CLAMP_EXT_REG(env->reg0.reg) - 1;
                    Assert(reg_field >= 0 && reg_field <= 7);

                    emit_prefix(PREFIX_REXW, env->reg0.reg, X64_REG_INVALID);
                    emit1((u8)(OPCODE_MOV_REG_IMM_RD_IO | reg_field));
                    emit8((u64)n->imm);

                    if(env->reg0.on_stack) {
                        emit_push(env->reg0.reg);
                    }
                }
            } break;
            case BC_CALL: {
                last_call = n;
                // Setup arguments
                // Arguments are pushed onto the stack by set_arg instructions.
                // Some calling conventions require the first four arguments in registers
                // We move the args on stack to registers.
                // TODO: We could improve by directly moving args to registers
                //  but it's more complicated.
                Assert(push_offset == 0);
                switch(n->call) {
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
                if(n->link == LinkConventions::DLLIMPORT || n->link == LinkConventions::VARIMPORT) {
                    Assert(false);
                    // Assert(bytecode->externalRelocations[imm].location == bcIndex);
                    // prog->add(OPCODE_CALL_RM_SLASH_2);
                    // prog->addModRM_rip(2,(u32)0);
                    // X64Program::NamedUndefinedRelocation namedReloc{};
                    // namedReloc.name = bytecode->externalRelocations[imm].name;
                    // namedReloc.textOffset = prog->size() - 4;
                    // prog->namedUndefinedRelocations.add(namedReloc);
                } else if(n->link == LinkConventions::IMPORT) {
                    Assert(false);
                    // Assert(bytecode->externalRelocations[imm].location == bcIndex);
                    // prog->add(OPCODE_CALL_IMM);
                    // X64Program::NamedUndefinedRelocation namedReloc{};
                    // namedReloc.name = bytecode->externalRelocations[imm].name;
                    // namedReloc.textOffset = prog->size();
                    // prog->namedUndefinedRelocations.add(namedReloc);
                    // prog->add4((u32)0);
                } else if (n->link == LinkConventions::NONE){ // or export
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
                } else if (n->link == LinkConventions::NATIVE){
                    //-- native function
                    switch(n->imm) {
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
                        auto nativeFunction = nativeRegistry->findFunction(n->imm);
                        // auto* nativeFunction = bytecode->nativeRegistry->findFunction(imm);
                        if(nativeFunction){
                            log::out << log::RED << "Native '"<<nativeFunction->name<<"' (id: "<<n->imm<<") is not implemented in x64 converter (" OS_NAME ").\n";
                        } else {
                            log::out << log::RED << n->imm<<" is not a native function (message from x64 converter).\n";
                        }
                    }
                    } // switch
                } else {
                    Assert(false);
                }
                switch(n->call) {
                    case BETCALL: break;
                    case STDCALL:
                    case UNIXCALL: {
                        auto r = alloc_register(X64_REG_A, false);
                        Assert(r != X64_REG_INVALID);
                    } break;
                }
                break;
            } break;
            case BC_RET: {
                // TODO: push_offset gets messed up on return, continue and break inside while, for, if statements. We need to store a push_offset per code path
                emit_pop(X64_REG_BP);
                emit1(OPCODE_RET);

                switch(tinycode->call_convention) {
                    case BETCALL: break;
                    case STDCALL:
                    case UNIXCALL:
                        if(!is_register_free(X64_REG_A))
                            free_register(X64_REG_A);
                        if(!is_register_free(X64_REG_XMM0))
                            free_register(X64_REG_XMM0);
                    break;
                    default: Assert(false);
                }
            } break;
            case BC_JZ: {
                COMPUTE_INPUTS(INPUT0);
                
                u8 prefix = PREFIX_REXW;
                if(IS_REG_EXTENDED(env->reg0.reg)) {
                    prefix |= PREFIX_REXB;
                }
                emit1(prefix);
                // TODO: We could replace CMP with TEST, no need for 8-bit immediate
                emit1(OPCODE_CMP_RM_IMM8_SLASH_7);
                emit_modrm_slash(MODE_REG, 7, CLAMP_EXT_REG(env->reg0.reg));
                emit1((u8)0);
                
                emit2(OPCODE_2_JE_IMM32);
                int imm_offset = size();
                emit4((u32)0);
                const u8 BYTE_OF_BC_JZ = 1 + 1 + 4; // nocheckin TODO: DON'T HARDCODE VALUES!
                addRelocation32(imm_offset - 2, imm_offset, n->bc_index + BYTE_OF_BC_JZ + n->imm);
                
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
                
                if(!env->reg0.on_stack)
                    free_register(env->reg0.reg);
            } break;
            case BC_JNZ: {
                COMPUTE_INPUTS(INPUT0);

                u8 prefix = PREFIX_REXW;
                if(IS_REG_EXTENDED(env->reg0.reg)) {
                    prefix |= PREFIX_REXB;
                }
                emit1(prefix);
                // TODO: We could replace CMP with TEST, no need for 8-bit immediate
                emit1(OPCODE_CMP_RM_IMM8_SLASH_7);
                emit_modrm_slash(MODE_REG, 7, CLAMP_EXT_REG(env->reg0.reg));
                emit1((u8)0);
                
                emit2(OPCODE_2_JNE_IMM32);
                int imm_offset = size();
                emit4((u32)0);

                const u8 BYTE_OF_BC_JNZ = 1 + 1 + 4; // nocheckin TODO: DON'T HARDCODE VALUES!
                addRelocation32(imm_offset - 2, imm_offset, n->bc_index + BYTE_OF_BC_JNZ + n->imm);
                
                if(!env->reg0.on_stack && !IsNativeRegister(env->reg0.reg))
                    free_register(env->reg0.reg);
            } break;
            case BC_JMP: {
                emit1(OPCODE_JMP_IMM32);
                int imm_offset = size();
                emit4((u32)0);
                
                const u8 BYTE_OF_BC_JMP = 1 + 4; // nocheckin TODO: DON'T HARDCODE VALUES!
                addRelocation32(imm_offset - 1, imm_offset, n->bc_index + BYTE_OF_BC_JMP + n->imm);
            } break;
            case BC_CAST: {
                // TODO: Check node depth, do the most register allocs first
                if(!env->env_in0) {
                    Assert(!env->node->in0->computed_env); // See COMPUTED_INPUTS
                    auto e = push_env0();
                    INHERIT_REG(reg0)
                    break;
                }
                Assert(false); // cast now uses two operands, this code needs to be fixed
                
                env->reg0 = env->env_in0->reg0;

                if(env->reg0.on_stack) {
                    env->reg0.reg = RESERVED_REG1;
                    emit_pop(env->reg0.reg);
                }

                int fsize = 1 << GET_CONTROL_SIZE(n->control);
                int tsize = 1 << GET_CONTROL_CONVERT_SIZE(n->control);

                u8 minSize = fsize < tsize ? fsize : tsize;
                
                X64Register origin_reg = env->reg0.reg;
                #ifdef gone
                switch(n->cast) {
                    case CAST_FLOAT_SINT: {
                        Assert(fsize == 4 || fsize == 8);
                        Assert(!IS_REG_XMM(origin_reg));

                        emit_prefix(fsize == 8 ? PREFIX_REXW : 0, origin_reg, X64_REG_SP);
                        emit1(OPCODE_MOV_RM_REG);
                        emit_modrm(MODE_DEREF_DISP8, CLAMP_EXT_REG(origin_reg), X64_REG_SP);
                        emit1((u8)-8);

                        if (fsize == 4 && tsize == 4) {
                            emit3(OPCODE_3_CVTTSS2SI_REG_RM);
                        } else if (fsize == 4 && tsize == 8) {
                            emit4(OPCODE_4_REXW_CVTTSS2SI_REG_RM);
                        } else if (fsize == 8 && tsize == 4) {
                            emit3(OPCODE_3_CVTTSD2SI_REG_RM);
                        } else if (fsize == 8 && tsize == 8) {
                            emit4(OPCODE_4_REXW_CVTTSD2SI_REG_RM);
                        }
                        // TODO: Fix r8-15 registers
                        emit_modrm(MODE_DEREF_DISP8, origin_reg, X64_REG_SP);
                        emit1((u8)-8);
                    } break;
                    case CAST_FLOAT_UINT: {
                        Assert(fsize == 4 || fsize == 8);

                        Assert(false); // TODO: float to uint64 is very special, needs to be handled properly

                        if(tsize == 8) {
                            if(fsize == 4) {
                                /*
                                movss  xmm0,DWORD PTR [rip+0x0]        # 10 <main+0x10>

                                movss  DWORD PTR [rbp-0xc],xmm0
                                movss  xmm0,DWORD PTR [rbp-0xc]
                                comiss xmm0,DWORD PTR [rip+0x0]        # 21 <main+0x21>
                                jae    unsigned
                                movss  xmm0,DWORD PTR [rbp-0xc]
                                cvttss2si rax,xmm0
                                mov    QWORD PTR [rbp-0x8],rax
                                jmp    end
                            unsigned:
                                movss  xmm0,DWORD PTR [rbp-0xc]
                                movss  xmm1,DWORD PTR [rip+0x0]        # 40 <main+0x40>

                                subss  xmm0,xmm1
                                cvttss2si rax,xmm0
                                mov    QWORD PTR [rbp-0x8],rax
                                movabs rax,0x8000000000000000

                                xor    QWORD PTR [rbp-0x8],rax
                            end:
                                mov    rax,QWORD PTR [rbp-0x8]
                                mov    QWORD PTR [rbp-0x8],rax
                                */
                            } else if(fsize == 8) {
                                /*
                                movsd  xmm0,QWORD PTR [rip+0x0]        # 10 <main+0x10>

                                movsd  QWORD PTR [rbp-0x10],xmm0
                                movsd  xmm0,QWORD PTR [rbp-0x10]
                                comisd xmm0,QWORD PTR [rip+0x0]        # 22 <main+0x22>

                                jae    unsigned
                                movsd  xmm0,QWORD PTR [rbp-0x10]
                                cvttsd2si rax,xmm0
                                mov    QWORD PTR [rbp-0x8],rax
                                jmp    end
                            unsigned:
                                movsd  xmm0,QWORD PTR [rbp-0x10]
                                movsd  xmm1,QWORD PTR [rip+0x0]        # 41 <main+0x41>

                                subsd  xmm0,xmm1
                                cvttsd2si rax,xmm0
                                mov    QWORD PTR [rbp-0x8],rax
                                movabs rax,0x8000000000000000
                            end:
                                xor    QWORD PTR [rbp-0x8],rax
                                mov    rax,QWORD PTR [rbp-0x8]
                                mov    QWORD PTR [rbp-0x8],rax
                                */
                            }
                        } else {

                        }

                        // if(fromXmm) {
                        //     prog->add3(OPCODE_3_CVTTSS2SI_REG_RM);
                        //     emit_modrm(MODE_REG, treg, freg);
                        // } else {
                        //     emit1(OPCODE_MOV_RM_REG);
                        //     emit_modrm(MODE_DEREF_DISP8, freg, X64_REG_SP);
                        //     emit1((u8)-8);

                        //     if(tsize == 8)
                        //         prog->add4(OPCODE_4_REXW_CVTTSS2SI_REG_RM);
                        //     else
                        //         prog->add3(OPCODE_3_CVTTSS2SI_REG_RM);

                        //     emit_modrm(MODE_DEREF_DISP8, treg, X64_REG_SP);
                        //     emit1((u8)-8);
                        // }
                    } break;
                    case CAST_SINT_FLOAT: {
                        Assert(!IS_REG_XMM(origin_reg));
                        Assert(tsize == 4 || tsize == 8);
                        if(fsize == 1){
                            // TODO: we might need to sign extend
                            if(tsize == 4)
                                emit3(OPCODE_3_CVTSI2SS_REG_RM);
                            else
                                emit3(OPCODE_3_CVTSI2SD_REG_RM);
                        } else if(fsize == 2){
                            // TODO: we might need to sign extend
                            if(tsize == 4)
                                emit3(OPCODE_3_CVTSI2SS_REG_RM);
                            else
                                emit3(OPCODE_3_CVTSI2SD_REG_RM);
                        } else if(fsize == 4) {
                            if(tsize == 4)
                                emit3(OPCODE_3_CVTSI2SS_REG_RM);
                            else
                                emit3(OPCODE_3_CVTSI2SD_REG_RM);
                        } else if(fsize == 8) {
                            if(tsize == 4)
                                emit4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                            else
                                emit4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                        }
                        // BUG: Hoping that xmm7 isn't used is considered bad programming.
                        X64Register temp = X64_REG_XMM7;
                        Assert(is_register_free(temp));
                        emit_modrm(MODE_REG, CLAMP_XMM(temp), origin_reg);

                        if(tsize == 4)
                            emit3(OPCODE_3_MOVSS_RM_REG);
                        else
                            emit3(OPCODE_3_MOVSD_RM_REG);
                        emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(temp), X64_REG_SP);
                        emit1((u8)-8);

                        if(tsize == 8)
                            emit1(PREFIX_REXW);
                        emit1(OPCODE_MOV_REG_RM);
                        // We don't CLAMP_EXT_REG because of rex being put inside of OPCODE_4_REXW_CVTSI2SS_REG_RM
                        emit_modrm(MODE_DEREF_DISP8, origin_reg, X64_REG_SP);
                        emit1((u8)-8);
                    } break;
                    case CAST_UINT_FLOAT: {
                        Assert(false);
                        Assert(tsize == 4 || tsize == 8);
                        // Assert(!fromXmm);
                        
                        if(fsize == 8) {
                            // // We have freg but the code may change so to ensure no future bugs we get
                            // // the register again with BCToReg and ensure that it isn't extended (with R8-R11) and that
                            // // it is a 64-bit register.
                            // u8 from_reg = BCToProgramReg(op1, 8);

                            // u8 xmm_reg = REG_XMM7; // nocheckin, use xmm7 if treg is rax,rbx.. otherwise we can actually just use xmm register

                            // if(toXmm) {
                            //     xmm_reg = treg;
                            // }

                            // if(tsize == 4) {
                            //     /*
                            //     test   rax,rax
                            //     js     unsigned
                            //     pxor   xmm0,xmm0
                            //     cvtsi2ss xmm0,rax
                            //     jmp    end
                            // unsigned:
                            //     mov    rdx,rax
                            //     shr    rdx,1
                            //     and    eax,0x1
                            //     or     rdx,rax
                            //     pxor   xmm0,xmm0
                            //     cvtsi2ss xmm0,rdx
                            //     addss  xmm0,xmm0
                            // end:
                            //     */

                            //     u8 temp_reg = REG_DI;
                            //     emit_push(temp_reg);

                            //     // Check signed bit
                            //     emit1(PREFIX_REXW);
                            //     emit1(OPCODE_TEST_RM_REG);
                            //     emit_modrm(MODE_REG, from_reg, from_reg);

                            //     emit1(OPCODE_JS_REL8);
                            //     int jmp_offset_unsigned = prog->size();
                            //     emit1((u8)0); // set later

                            //     // Normal conversion since the signed bit is off
                            //     prog->add3(OPCODE_3_PXOR_RM_REG);
                            //     emit_modrm(MODE_REG, xmm_reg, xmm_reg);

                            //     prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                            //     emit_modrm(MODE_REG, xmm_reg, from_reg);

                            //     emit1(OPCODE_JMP_IMM8);
                            //     int jmp_offset_end = prog->size();
                            //     emit1((u8)0);

                            //     prog->set(jmp_offset_unsigned, prog->size() - (jmp_offset_unsigned +1)); // +1 gives the offset after the jump instruction (immediate is one byte)
                                
                            //     // Special conversion because the signed bit is on but since our conversion is unsigned
                            //     // we need some special stuff so that our float isn't negative and half the number we except

                            //     emit1(PREFIX_REXW);
                            //     emit1(OPCODE_MOV_REG_RM);
                            //     emit_modrm(MODE_REG, temp_reg, from_reg);

                            //     emit1(PREFIX_REXW);    
                            //     emit1(OPCODE_SHR_RM_ONCE_SLASH_5);
                            //     emit_modrm(MODE_REG, 5, temp_reg);

                            //     emit1(OPCODE_AND_RM_IMM8_SLASH_4);
                            //     emit_modrm(MODE_REG, 4, from_reg);
                            //     emit1((u8)1);

                            //     emit1(PREFIX_REXW);
                            //     emit1(OPCODE_OR_RM_REG);
                            //     emit_modrm(MODE_REG, from_reg, temp_reg);

                            //     prog->add3(OPCODE_3_PXOR_RM_REG);
                            //     emit_modrm(MODE_REG, xmm_reg, xmm_reg);

                            //     prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                            //     emit_modrm(MODE_REG, xmm_reg, temp_reg);

                            //     prog->add3(OPCODE_3_ADDSS_REG_RM);
                            //     emit_modrm(MODE_REG, xmm_reg, xmm_reg);

                            //     prog->set(jmp_offset_end, prog->size() - (jmp_offset_end +1));

                            //     emit_pop(temp_reg);

                            //     if(!toXmm) {
                            //         prog->add3(OPCODE_3_MOVSS_RM_REG);
                            //         emit_modrm(MODE_DEREF_DISP8, xmm_reg, X64_REG_SP);
                            //         emit1((u8)-8);

                            //         emit1(OPCODE_MOV_REG_RM);
                            //         emit_modrm(MODE_DEREF_DISP8, treg, X64_REG_SP);
                            //         emit1((u8)-8);
                            //     }
                            // } else if(tsize == 8) {
                            //     /*
                            //     test   rax,rax
                            //     js     unsigned
                            //     pxor   xmm0,xmm0
                            //     cvtsi2sd xmm0,rax
                            //     jmp    end
                            // unsigned:
                            //     mov    rdx,rax
                            //     shr    rdx,1
                            //     and    eax,0x1
                            //     or     rdx,rax
                            //     pxor   xmm0,xmm0
                            //     cvtsi2sd xmm0,rdx
                            //     addsd  xmm0,xmm0
                            // end:
                            //     */
                            //     u8 temp_reg = REG_DI;
                            //     emit_push(temp_reg);

                            //     // Check signed bit
                            //     emit1(PREFIX_REXW);
                            //     emit1(OPCODE_TEST_RM_REG);
                            //     emit_modrm(MODE_REG, from_reg, from_reg);

                            //     emit1(OPCODE_JS_REL8);
                            //     int jmp_offset_unsigned = prog->size();
                            //     emit1((u8)0); // set later

                            //     // Normal conversion since the signed bit is off
                            //     prog->add3(OPCODE_3_PXOR_RM_REG);
                            //     emit_modrm(MODE_REG, xmm_reg, xmm_reg);

                            //     prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                            //     emit_modrm(MODE_REG, xmm_reg, from_reg);

                            //     emit1(OPCODE_JMP_IMM8);
                            //     int jmp_offset_end = prog->size();
                            //     emit1((u8)0);

                            //     prog->set(jmp_offset_unsigned, prog->size() - (jmp_offset_unsigned +1)); // +1 gives the offset after the jump instruction (immediate is one byte)
                                
                            //     // Special conversion because the signed bit is on but since our conversion is unsigned
                            //     // we need some special stuff so that our float isn't negative and half the number we except

                            //     emit1(PREFIX_REXW);
                            //     emit1(OPCODE_MOV_REG_RM);
                            //     emit_modrm(MODE_REG, temp_reg, from_reg);

                            //     emit1(PREFIX_REXW);    
                            //     emit1(OPCODE_SHR_RM_ONCE_SLASH_5);
                            //     emit_modrm(MODE_REG, 5, temp_reg);

                            //     emit1(OPCODE_AND_RM_IMM8_SLASH_4);
                            //     emit_modrm(MODE_REG, 4, from_reg);
                            //     emit1((u8)1);

                            //     emit1(PREFIX_REXW);
                            //     emit1(OPCODE_OR_RM_REG);
                            //     emit_modrm(MODE_REG, from_reg, temp_reg);

                            //     prog->add3(OPCODE_3_PXOR_RM_REG);
                            //     emit_modrm(MODE_REG, xmm_reg, xmm_reg);

                            //     prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                            //     emit_modrm(MODE_REG, xmm_reg, temp_reg);

                            //     prog->add3(OPCODE_3_ADDSD_REG_RM);
                            //     emit_modrm(MODE_REG, xmm_reg, xmm_reg);

                            //     prog->set(jmp_offset_end, prog->size() - (jmp_offset_end +1));

                            //     emit_pop(temp_reg);

                            //     if(!toXmm) {
                            //         prog->add3(OPCODE_3_MOVSD_RM_REG);
                            //         emit_modrm(MODE_DEREF_DISP8, xmm_reg, X64_REG_SP);
                            //         emit1((u8)-8);

                            //         emit1(PREFIX_REXW);
                            //         emit1(OPCODE_MOV_REG_RM);
                            //         emit_modrm(MODE_DEREF_DISP8, treg, X64_REG_SP);
                            //         emit1((u8)-8);
                            //     }
                            // }


                            #ifdef gone
                            // TODO: THIS IS FLAWED! The convert int to float instruction assumes signed integers but
                            //  we are dealing with unsigned ones. That means that large unsigned 64-bit numbers would
                            //  be converted to negative slighly smaller numbers.
                            if(tsize==4)
                                prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                            else
                                prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);

                            if(toXmm) {
                                emit_modrm(MODE_REG, treg, freg);
                            } else {
                                emit_modrm(MODE_REG, REG_XMM7, freg);

                                if(tsize==4)
                                    prog->add3(OPCODE_3_MOVSS_RM_REG);
                                else
                                    prog->add3(OPCODE_3_MOVSD_RM_REG);
                                emit_modrm(MODE_DEREF_DISP8, REG_XMM7, X64_REG_SP);
                                emit1((u8)-8);

                                if(tsize==8)
                                    emit1(PREFIX_REXW);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_DEREF_DISP8, treg, X64_REG_SP);
                                emit1((u8)-8);
                            }

                            // Assert(false);
                            // This code is bugged, doesn't work.
                            /*
                            t = cast<f32>cast<u64>23
                            println(t)
                            */
                            

                            /*
                            test rax, rax
                            jl signed
                            cvtsi2ss xmm0, r13
                            jmp end
                            signed:
                            mov r13d, eax
                            and r13d, 1
                            shr rax, 1
                            or r13, rax
                            cvtsi2ss xmm0, r13
                            mulss xmm0, 2
                            end:
                            */

                        // TODO: This is way to many instructions to convert an unsigned integer to a float.
                        //   Users who don't realize this will unknowningly have slow code because of casts
                        //   they didn't pay attention to. Can we minimize the amount of instructions?
                        //   Can we be less correct and generate fewer instructions?

                            // Assert(false);
                            // We have to do some extra work since we use unsigned 64-bit integer but
                            // the x64 instruction expects a signed 64-bit integer.

                            // TODO: C/C++ compilers use a jump instruction to differentiate between
                            //  the negative and non-negative case. We don't at the moment but probably should.

                            // IMPORTANT: USING A TEMPORARY REG LIKE THIS IS BAD IF OTHER INSTRUCTIONS USES IT.
                            //  You won't expect this register to be modified when you look at the bytecode instructions.
                            // IMPORTANT: The code below modifies the input register!
                            u8 tempReg = REG_R15;
                            u8 xmm = REG_XMM6;
                            if(toXmm) {
                                xmm = treg;
                                Assert(treg != REG_XMM7);
                                // IMPORTANT: XMM7 is in use. More importantly, we assume registers
                                // above xmm3 are unused by the bytecode. We will have many bugs
                                // if the assumption is wrong.
                            }

                            emit1(PREFIX_REXW);
                            emit1(OPCODE_TEST_RM_REG);
                            emit_modrm(MODE_REG, freg, freg);

                            emit1(OPCODE_JL_REL8);
                            u32 jumpSign = prog->size();
                            emit1((u8)0);

                            // emit1((u8)0xF3);
                            // emit1((u8)(PREFIX_REXW));
                            // prog->add2((u16)0x2A0F);
                            if(tsize == 4) {
                                prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                            } else {
                                prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                            }
                            emit_modrm(MODE_REG, xmm, freg);

                            emit1(OPCODE_JMP_IMM8);
                            u32 jumpEnd = prog->size();
                            emit1((u8)0);

                            prog->set(jumpSign, prog->size() - (jumpSign+1)); // +1 because jumpSign points to the byte to change in the jump instruction, we want the end of the instruction.

                            // emit1((u8)(PREFIX_REXW|PREFIX_REXR));
                            emit1((u8)(PREFIX_REXR)); // 64-bit operands not needed, we just want 1 bit from freg
                            emit1(OPCODE_MOV_REG_RM);
                            emit_modrm(MODE_REG, tempReg, freg);

                            emit1((u8)(PREFIX_REXW|PREFIX_REXB));
                            emit1(OPCODE_AND_RM_IMM8_SLASH_4);
                            emit_modrm(MODE_REG, 4, tempReg);
                            emit1((u8)1);


                            emit1((u8)(PREFIX_REXW));
                            emit1(OPCODE_SHR_RM_IMM8_SLASH_5);
                            emit_modrm(MODE_REG, 5, freg);
                            emit1((u8)1);

                            emit1((u8)(PREFIX_REXW|PREFIX_REXB));
                            emit1(OPCODE_OR_RM_REG);
                            emit_modrm(MODE_REG, freg, tempReg);
                            
                            // F3 REX.W 0F 2A  <- the CVTSI2SS_REG_RM requires the rex byte smashed inside it
                            if(tsize == 4) {
                                emit1((u8)0xF3);
                                emit1((u8)(PREFIX_REXW|PREFIX_REXB));
                                prog->add2((u16)0x2A0F);
                            } else {
                                emit1((u8)0xF2);
                                emit1((u8)(PREFIX_REXW|PREFIX_REXB));
                                prog->add2((u16)0x2A0F);
                            }
                            emit_modrm(MODE_REG, xmm, tempReg);
                            
                            emit1((u8)(PREFIX_REXW|PREFIX_REXB|PREFIX_REXR));
                            emit1(OPCODE_XOR_REG_RM);
                            emit_modrm(MODE_REG, tempReg, tempReg);

                            // TODO: REXW may be needed here
                            emit1(PREFIX_REXB);
                            emit1(OPCODE_MOV_RM_IMM8_SLASH_0);
                            emit_modrm(MODE_REG, 0, tempReg);
                            emit1((u8)2);

                            if(tsize == 4)
                                prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                            else
                                prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                            emit_modrm(MODE_REG, REG_XMM7, tempReg);
                            if(tsize == 4) 
                                prog->add3(OPCODE_3_MULSS_REG_RM);
                            else
                                prog->add3(OPCODE_3_MULSD_REG_RM);
                            emit_modrm(MODE_REG, xmm, REG_XMM7);

                            // END
                            prog->set(jumpEnd, prog->size() - (jumpEnd+1)); // +1 because jumpEnd points to the byte to change in the jump instruction, we want the end of the instruction.

                            if (!toXmm) {
                                if(tsize==4)
                                    prog->add3(OPCODE_3_MOVSS_RM_REG);
                                else
                                    prog->add3(OPCODE_3_MOVSD_RM_REG);
                                emit_modrm(MODE_DEREF_DISP8, xmm, X64_REG_SP);
                                emit1((u8)-8);


                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_DEREF_DISP8, treg, X64_REG_SP);
                                emit1((u8)-8);
                            } else {
                                // xmm register is already loaded with correct value
                            }
                            #endif
                        } else {
                            // // nocheckin, rexw when converting u32 to f32/f64 otherwise the last bit will
                            // // be treated as a signed bit by CVTSI2SS
                            // if(fsize == 1){
                            //     // emit1(PREFIX_REXW);
                            //     // emit1(OPCODE_AND_RM_IMM_SLASH_4);
                            //     // emit_modrm(MODE_REG, 4, freg);
                            //     // prog->add4((u32)0xFF);
                            //     if(tsize==4)
                            //         prog->add3(OPCODE_3_CVTSI2SS_REG_RM);
                            //     else
                            //         prog->add3(OPCODE_3_CVTSI2SD_REG_RM);
                            // } else if(fsize == 2){
                            //     // emit1(PREFIX_REXW);
                            //     // emit1(OPCODE_AND_RM_IMM_SLASH_4);
                            //     // emit_modrm(MODE_REG, 4, freg);
                            //     // prog->add4((u32)0xFFFF);
                            //     if(tsize==4)
                            //         prog->add3(OPCODE_3_CVTSI2SS_REG_RM);
                            //     else
                            //         prog->add3(OPCODE_3_CVTSI2SD_REG_RM);
                            // } else if(fsize == 4) {
                            //     // It is probably safe to use rexw so that rax is used
                            //     // when eax was specified. Most instructions zero the upper bits.
                            //     // There may be an edge case though.
                            //     // We must use rexw with this operation since it assumes signed values
                            //     // but we have an unsigned so we must use 64-bit values.
                            //     emit1(PREFIX_REXW);
                            //     emit1(OPCODE_MOV_REG_RM);
                            //     emit_modrm(MODE_REG, freg, freg); // clear upper 32 bits
                            //     if(tsize==4)
                            //         prog->add4(OPCODE_4_REXW_CVTSI2SS_REG_RM);
                            //     else
                            //         prog->add4(OPCODE_4_REXW_CVTSI2SD_REG_RM);
                            //     // if(tsize==4)
                            //     //     prog->add3(OPCODE_3_CVTSI2SS_REG_RM);
                            //     // else
                            //     //     prog->add3(OPCODE_3_CVTSI2SD_REG_RM);
                            // }

                            // if(toXmm) {
                            //     emit_modrm(MODE_REG, treg, freg);
                            // } else {
                            //     emit_modrm(MODE_REG, REG_XMM7, freg);

                            //     if(tsize==4)
                            //         prog->add3(OPCODE_3_MOVSS_RM_REG);
                            //     else
                            //         prog->add3(OPCODE_3_MOVSD_RM_REG);
                            //     emit_modrm(MODE_DEREF_DISP8, REG_XMM7, X64_REG_SP);
                            //     emit1((u8)-8);

                            //     if(tsize==8)
                            //         emit1(PREFIX_REXW);
                            //     emit1(OPCODE_MOV_REG_RM);
                            //     emit_modrm(MODE_DEREF_DISP8, treg, X64_REG_SP);
                            //     emit1((u8)-8);

                            //     // emit1(OPCODE_NOP);
                            // }
                        }
                    } break;
                    case CAST_UINT_SINT:
                    case CAST_SINT_UINT: {
                        if(minSize==1){
                            emit_prefix(PREFIX_REXW, origin_reg, origin_reg);
                            emit2(OPCODE_2_MOVZX_REG_RM8);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(origin_reg), CLAMP_EXT_REG(origin_reg));
                        } else if(minSize == 2) {
                            emit_prefix(PREFIX_REXW, origin_reg, origin_reg);
                            emit2(OPCODE_2_MOVZX_REG_RM16);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(origin_reg), CLAMP_EXT_REG(origin_reg));
                        } else if(minSize == 4) {
                            emit_prefix(0, origin_reg, origin_reg);
                            emit1(OPCODE_MOV_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(origin_reg), CLAMP_EXT_REG(origin_reg));
                            
                        } else if(minSize == 8) {
                            // nothing needs to be done
                        }
                    } break;
                    case CAST_UINT_UINT: {
                        // do nothing
                    } break;
                    case CAST_SINT_SINT: {
                        // TODO: Sign extend properly
                        if(minSize==1){
                            emit_prefix(PREFIX_REXW, origin_reg, origin_reg);
                            emit2(OPCODE_2_MOVSX_REG_RM8);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(origin_reg), CLAMP_EXT_REG(origin_reg));
                        } else if(minSize == 2) {
                            emit_prefix(PREFIX_REXW, origin_reg, origin_reg);
                            emit1(PREFIX_16BIT);
                            emit2(OPCODE_2_MOVSX_REG_RM8);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(origin_reg), CLAMP_EXT_REG(origin_reg));
                        } else if(minSize == 4) {
                            emit_prefix(0, origin_reg, origin_reg);
                            emit1(OPCODE_MOVSXD_REG_RM);
                            emit_modrm(MODE_REG, CLAMP_EXT_REG(origin_reg), CLAMP_EXT_REG(origin_reg));
                        } else if(minSize == 8) {
                            // nothing needs to be done
                        }
                    } break;
                    case CAST_FLOAT_FLOAT: {
                        Assert((fsize == 4 || fsize == 8) && (tsize == 4 || tsize == 8));

                        if(IS_REG_XMM(origin_reg)) {
                            if(fsize == 4 && tsize == 8) {
                                emit3(OPCODE_3_CVTSS2SD_REG_RM);
                                emit_modrm(MODE_REG, CLAMP_XMM(origin_reg), CLAMP_XMM(origin_reg));
                            } else if(fsize == 8 && tsize == 4) {
                                emit3(OPCODE_3_CVTSD2SS_REG_RM);
                                emit_modrm(MODE_REG, CLAMP_XMM(origin_reg), CLAMP_XMM(origin_reg));
                            } else {
                                // do nothing
                            }
                        } else {
                            Assert(is_register_free(X64_REG_XMM7));
                            X64Register temp = X64_REG_XMM7;
                            if(fsize == 8)
                            emit_prefix(fsize == 8 ? PREFIX_REXW : 0, origin_reg, X64_REG_INVALID);
                            emit1(OPCODE_MOV_RM_REG);
                            emit_modrm(MODE_DEREF_DISP8, CLAMP_EXT_REG(origin_reg), X64_REG_SP);
                            emit1((u8)-8);

                            // NOTE: Do we need REXW here?
                            if(fsize == 4 && tsize == 8) {
                                emit3(OPCODE_3_CVTSS2SD_REG_RM);
                            } else if(fsize == 8 && tsize == 4) {
                                emit3(OPCODE_3_CVTSD2SS_REG_RM);
                            } else if(fsize == 4 && tsize == 4){
                                emit3(OPCODE_3_MOVSS_REG_RM);
                            } else if(fsize == 8 && tsize == 8) {
                                emit3(OPCODE_3_MOVSD_REG_RM);
                            }
                            emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(temp), X64_REG_SP);
                            emit1((u8)-8);

                            if(tsize == 4) {
                                emit3(OPCODE_3_MOVSS_RM_REG);
                                emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(temp), X64_REG_SP);
                                emit1((u8)-8);
                                
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_DEREF_DISP8, CLAMP_EXT_REG(origin_reg), X64_REG_SP);
                                emit1((u8)-8);
                            } else if(tsize == 8) {
                                emit3(OPCODE_3_MOVSD_RM_REG);
                                emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(temp), X64_REG_SP);
                                emit1((u8)-8);

                                emit1(PREFIX_REXW);
                                emit1(OPCODE_MOV_REG_RM);
                                emit_modrm(MODE_DEREF_DISP8, CLAMP_EXT_REG(origin_reg), X64_REG_SP);
                                emit1((u8)-8);
                            }
                        }
                    } break;
                    default: Assert(("Cast type not implemented in x64 backend, Compiler bug",false));
                }
                #endif
                
                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
            } break;
            case BC_DATAPTR: {
                REQUEST_REG0();
                
                Assert(!IS_REG_XMM(env->reg0.reg)); // loading pointer into xmm makes no sense, it's a compiler bug

                emit_prefix(PREFIX_REXW, env->reg0.reg, X64_REG_INVALID);
                emit1(OPCODE_LEA_REG_M);
                emit_modrm_rip32(CLAMP_EXT_REG(env->reg0.reg), (u32)0);
                i32 disp32_offset = size() - 4; // -4 to refer to the 32-bit immediate in modrm_rip
                prog->addDataRelocation(n->imm, disp32_offset, current_tinyprog_index);

                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
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
            case BC_MEMZERO: {
                COMPUTE_INPUTS(INPUT0|INPUT1);

                // ##########################
                //   MEMZERO implementation
                // ##########################
                {
                    u8 batch_size = n->imm;
                    if(batch_size == 0)
                        batch_size = 1;

                    X64Register reg_cur = env->reg0.reg;
                    X64Register reg_fin = env->reg1.reg;

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
                
                if(!env->reg0.on_stack)
                    free_register(env->reg0.reg);

                if(!env->reg1.on_stack)
                    free_register(env->reg1.reg);
            } break;
            case BC_MEMCPY: {
                COMPUTE_INPUTS(INPUT0|INPUT1|INPUT2);
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

                    X64Register dst = env->reg0.reg;
                    X64Register src = env->reg1.reg;
                    X64Register fin = env->reg2.reg;
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
                
                if(!env->reg0.on_stack)
                    free_register(env->reg0.reg);

                if(!env->reg1.on_stack)
                    free_register(env->reg1.reg);

                if(!env->reg2.on_stack)
                    free_register(env->reg2.reg);
            } break;
            case BC_STRLEN: {
                if(!env->env_in1) {
                    Assert(!env->node->in1->computed_env); // See COMPUTED_INPUTS
                    auto e = push_env1();
                    INHERIT_REG(reg1)
                    break;
                }
                REQUEST_REG0();

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
            
                X64Register counter = env->reg0.reg;
                X64Register src = env->reg1.reg;

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

                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
                if(!env->reg1.on_stack)
                    free_register(env->reg1.reg);
            } break;
            case BC_RDTSC: {
                if(is_register_free(X64_REG_D)) {
                    env->reg0.reg = X64_REG_D;
                } else {
                    REQUEST_REG0();
                    emit_push(X64_REG_D);
                }

                emit2(OPCODE_2_RDTSC); // sets edx (32-64) and eax (0-32), upper 32-bits of rdx and rax are cleared

                emit1(PREFIX_REXW);
                emit1(OPCODE_SHL_RM_IMM8_SLASH_4);
                emit_modrm_slash(MODE_REG, 4, X64_REG_D);
                emit1((u8)32);

                if(env->reg0.reg == X64_REG_D) {
                    emit1(PREFIX_REXW);
                    emit1(OPCODE_OR_REG_RM);
                    emit_modrm(MODE_REG, X64_REG_A, X64_REG_D);
                } else {
                    emit1(PREFIX_REXW);
                    emit1(OPCODE_OR_REG_RM);
                    emit_modrm(MODE_REG, X64_REG_D, X64_REG_A);
                    emit_pop(X64_REG_D);
                    if(env->reg0.on_stack)
                        emit_push(X64_REG_A);
                    else {
                        emit_mov_reg_reg(env->reg0.reg, X64_REG_A);
                    }
                }
            } break;
            case BC_ATOMIC_ADD: {
                COMPUTE_INPUTS(INPUT0|INPUT1);

                int prefix = 0;
                if(GET_CONTROL_SIZE(n->control) == CONTROL_32B) {

                } else if(GET_CONTROL_SIZE(n->control) == CONTROL_64B)
                    prefix |= PREFIX_REXW;
                else Assert(false);
                
                emit1(PREFIX_LOCK);
                emit_prefix(prefix, env->reg0.reg, env->reg1.reg);
                emit1(OPCODE_ADD_RM_REG);
                emit_modrm(MODE_DEREF, CLAMP_EXT_REG(env->reg0.reg), CLAMP_EXT_REG(env->reg1.reg));

                if(env->reg0.on_stack) emit_pop(env->reg0.reg);

                if(!env->reg1.on_stack) free_register(env->reg1.reg);
            } break;
            case BC_ATOMIC_CMP_SWAP: {
                COMPUTE_INPUTS(INPUT0|INPUT1|INPUT2);

                // TODO: Add 64-bit version
                // int prefix = 0;
                // if(GET_CONTROL_SIZE(n->control) == CONTROL_32B) {

                // } else if(GET_CONTROL_SIZE(n->control) == CONTROL_64B)
                //     prefix |= PREFIX_REXW;
                // else Assert(false);

                X64Register reg_new = env->reg2.reg;
                X64Register old = env->reg1.reg;
                X64Register ptr = env->reg0.reg;

                emit_mov_reg_reg(X64_REG_A, old);
                
                emit1(PREFIX_LOCK);
                emit2(OPCODE_2_CMPXCHG_RM_REG);
                emit_modrm(MODE_DEREF, reg_new, ptr);
                // RAX will be set to the original value in ptr

                emit_mov_reg_reg(ptr, X64_REG_A);

                if(env->reg0.on_stack) emit_pop(env->reg0.reg);

                if(!env->reg1.on_stack) free_register(env->reg1.reg);
                if(!env->reg2.on_stack) free_register(env->reg2.reg);
            } break;
            case BC_ROUND:
            case BC_SQRT: {
                n->control = (InstructionControl)(n->control + CONTROL_FLOAT_OP);
                COMPUTE_INPUTS(INPUT1);

                // REQUEST_REG0(true);

                X64Register reg = env->reg0.reg;

                emit4(OPCODE_4_ROUNDSS_REG_RM_IMM8);
                emit_modrm(MODE_REG, reg, reg);
                emit1((u8)0); // immediate describes rounding mode. 0 means "round to" which I believe will
                // choose the number that is closest to the actual number. There where round toward, up, and toward as well (round to is probably a good default).
                emit3(OPCODE_3_SQRTSS_REG_RM);
                emit_modrm(MODE_REG, reg, reg);

                if(env->reg0.on_stack) {
                    emit_push(env->reg0.reg);
                }
            } break;
            case BC_TEST_VALUE: {
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
            } break;
            default: Assert(false);
        }
        if(pop_env) {
            Assert(!env->node->computed_env);
            env->node->computed_env = env;
            envs.pop();
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

    for(auto e : envs)
        destroyEnv(e);
    envs.cleanup();

    // last instruction should be a return
    Assert(tinyprog->text[tinyprog->head-1] == OPCODE_RET);
}
#endif
void X64Builder::free_all_registers() {
    registers.clear();
}
X64Register X64Builder::alloc_register(X64Register reg, bool is_float) {
    using namespace engone;
    // #define DO_LOG(X) X
    #define DO_LOG(X)
    if(!is_float) {
        if(reg != X64_REG_INVALID) {
            auto pair = registers.find(reg);
            if(pair == registers.end()) {
                registers[reg] = {};
                registers[reg].used = true;
                DO_LOG(log::out << "alloc " << reg<<"\n";)
                return reg;
            } else if(!pair->second.used) {
                pair->second.used = true;
                DO_LOG(log::out << "alloc " << reg<<"\n";)
                return reg;
            }
        } else {
            static const X64Register regs[]{
                // X64_REG_A,
                X64_REG_C,
                X64_REG_D,
                X64_REG_B,
                // X64_REG_SI,
                // X64_REG_DI,

                X64_REG_R8,
                X64_REG_R9,
                X64_REG_R10,
                // X64_REG_R11,
                // X64_REG_R12,
                // X64_REG_R13,
                // X64_REG_R14,
                // X64_REG_R15,
            };
            for(int i = 0; i < sizeof(regs)/sizeof(*regs);i++) {
                X64Register reg = regs[i];
                auto pair = registers.find(reg);
                if(pair == registers.end()) {
                    registers[reg] = {};
                    registers[reg].used = true;
                    Assert(reg != RESERVED_REG0 && reg != RESERVED_REG1);
                    DO_LOG(log::out << "alloc " << reg<<"\n";)
                    return reg;
                } else if(!pair->second.used) {
                    pair->second.used = true;
                    Assert(reg != RESERVED_REG0 && reg != RESERVED_REG1);
                    DO_LOG(log::out << "alloc " << reg<<"\n";)
                    return reg;
                }
            }
        }
    } else {
        if(reg != X64_REG_INVALID) {
            auto pair = registers.find(reg);
            if(pair == registers.end()) {
                registers[reg] = {};
                registers[reg].used = true;
                DO_LOG(log::out << "alloc " << reg<<"\n";)
                return reg;
            } else if(!pair->second.used) {
                pair->second.used = true;
                DO_LOG(log::out << "alloc " << reg<<"\n";)
                return reg;
            }
        } else {
            static const X64Register regs[]{
                X64_REG_XMM0,
                X64_REG_XMM1,
                X64_REG_XMM2,
                X64_REG_XMM3,
            };
            for(int i = 0; i < sizeof(regs)/sizeof(*regs);i++) {
                X64Register reg = regs[i];
                auto pair = registers.find(reg);
                if(pair == registers.end()) {
                    registers[reg] = {};
                    registers[reg].used = true;
                    DO_LOG(log::out << "alloc " << reg<<"\n";)
                    return reg;
                } else if(!pair->second.used) {
                    pair->second.used = true;
                    DO_LOG(log::out << "alloc " << reg<<"\n";)
                    return reg;
                }
            }
        }
    }
    // Assert(("out of registers",false));
    return X64_REG_INVALID;
}
bool X64Builder::is_register_free(X64Register reg) {
    auto pair = registers.find(reg);
    if(pair == registers.end()) {
        return true;
    }
    return false;
}
void X64Builder::free_register(X64Register reg) {
    auto pair = registers.find(reg);
    if(pair == registers.end()) {
        Assert(false);
    }
    pair->second.used = false;
}
bool X64Builder::_reserve(u32 newAllocationSize){
    if(newAllocationSize==0){
        if(tinyprog->_allocationSize!=0){
            TRACK_ARRAY_FREE(tinyprog->text, u8, tinyprog->_allocationSize);
            // engone::Free(text, _allocationSize);
        }
        tinyprog->text = nullptr;
        tinyprog->_allocationSize = 0;
        tinyprog->head = 0;
        return true;
    }
    if(!tinyprog->text){
        // text = (u8*)engone::Allocate(newAllocationSize);
        tinyprog->text = TRACK_ARRAY_ALLOC(u8, newAllocationSize);
        Assert(tinyprog->text);
        // initialization of elements is done when adding them
        if(!tinyprog->text)
            return false;
        tinyprog->_allocationSize = newAllocationSize;
        return true;
    } else {
        TRACK_DELS(u8, tinyprog->_allocationSize);
        u8* newText = (u8*)engone::Reallocate(tinyprog->text, tinyprog->_allocationSize, newAllocationSize);
        TRACK_ADDS(u8, newAllocationSize);
        Assert(newText);
        if(!newText)
            return false;
        tinyprog->text = newText;
        tinyprog->_allocationSize = newAllocationSize;
        if(tinyprog->head > newAllocationSize){
            tinyprog->head = newAllocationSize;
        }
        return true;
    }
    return false;
}

void X64Builder::emit1(u8 byte){
    ensure_bytes(1);
    *(tinyprog->text + tinyprog->head) = byte;
    tinyprog->head++;
}
void X64Builder::emit2(u16 word){
    ensure_bytes(2);
    
    auto ptr = (u8*)&word;
    *(tinyprog->text + tinyprog->head + 0) = *(ptr + 0);
    *(tinyprog->text + tinyprog->head + 1) = *(ptr + 1);
    tinyprog->head+=2;
}
void X64Builder::emit3(u32 word){
    if(tinyprog->head>0){
        // This is not how you use rex prefix
        // cvtsi2ss instructions use smashed in between it's other opcode bytes
        Assert(*(tinyprog->text + tinyprog->head - 1) != PREFIX_REXW);
    }
    Assert(0==(word&0xFF000000));
    ensure_bytes(3);
    
    auto ptr = (u8*)&word;
    *(tinyprog->text + tinyprog->head + 0) = *(ptr + 0);
    *(tinyprog->text + tinyprog->head + 1) = *(ptr + 1);
    *(tinyprog->text + tinyprog->head + 2) = *(ptr + 2);
    tinyprog->head+=3;
}
void X64Builder::emit4(u32 dword){
    // wish we could assert to find bugs but immediates may be zero and won't work.
    // we could make a nother functionn specifcially for immediates though.
    // Assert(dword & 0xFF00'0000);
    ensure_bytes(4);
    auto ptr = (u8*)&dword;

    // deals with non-alignment
    *(tinyprog->text + tinyprog->head + 0) = *(ptr + 0);
    *(tinyprog->text + tinyprog->head + 1) = *(ptr + 1);
    *(tinyprog->text + tinyprog->head + 2) = *(ptr + 2);
    *(tinyprog->text + tinyprog->head + 3) = *(ptr + 3);
    tinyprog->head+=4;
}
void X64Builder::emit8(u64 qword){
    // wish we could assert to find bugs but immediates may be zero and won't work.
    // we could make a nother functionn specifcially for immediates though.
    // Assert(dword & 0xFF00'0000);
    ensure_bytes(8);
    auto ptr = (u8*)&qword;

    // deals with non-alignment
    *(tinyprog->text + tinyprog->head + 0) = *(ptr + 0);
    *(tinyprog->text + tinyprog->head + 1) = *(ptr + 1);
    *(tinyprog->text + tinyprog->head + 2) = *(ptr + 2);
    *(tinyprog->text + tinyprog->head + 3) = *(ptr + 3);
    *(tinyprog->text + tinyprog->head + 4) = *(ptr + 4);
    *(tinyprog->text + tinyprog->head + 5) = *(ptr + 5);
    *(tinyprog->text + tinyprog->head + 6) = *(ptr + 6);
    *(tinyprog->text + tinyprog->head + 7) = *(ptr + 7);
    tinyprog->head += 8;
}
void X64Builder::fix_jump_here_imm8(u32 offset) {
    *(u8*)(tinyprog->text + offset) = (code_size() - (offset + 1));
}
void X64Builder::emit_jmp_imm8(u32 offset) {
    emit1((u8)(offset - (code_size() + 1)));
}
void X64Builder::set_imm32(u32 offset, u32 value) {
    *(u32*)(tinyprog->text + offset) = value;
}
void X64Builder::set_imm8(u32 offset, u8 value) {
    *(u8*)(tinyprog->text + offset) = value;
}
void X64Builder::emit_bytes(const u8* arr, u64 len){
    ensure_bytes(len);
    
    memcpy(tinyprog->text + tinyprog->head, arr, len);
    tinyprog->head += len;
}
void X64Builder::emit_modrm_slash(u8 mod, u8 reg, X64Register _rm){
    if(_rm == X64_REG_SP && mod != MODE_REG) {
        // SP register is not allowed with standard modrm byte, we must use a SIB
        emit_modrm_sib_slash(mod, reg, SIB_SCALE_1, SIB_INDEX_NONE, _rm);
        return;
    }
    u8 rm = _rm - 1;
    Assert((mod&~3) == 0 && (reg&~7)==0 && (rm&~7)==0);
    // You probably made a mistake and used REG_BP thinking it works with just ModRM byte.
    Assert(("Use addModRM_SIB instead",!(mod!=0b11 && rm==0b100)));
    // X64_REG_SP isn't available with mod=0b00, see intel x64 manual about 32 bit addressing for more details
    Assert(("Use addModRM_disp32 instead",!(mod==0b00 && rm==0b101)));
    // Assert(("Use addModRM_disp32 instead",(mod!=0b10)));
    emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
}
void X64Builder::emit_modrm(u8 mod, X64Register _reg, X64Register _rm){
    if(_rm == X64_REG_SP && mod != MODE_REG) {
        // SP register is not allowed with standard modrm byte, we must use a SIB
        emit_modrm_sib(mod, _reg, SIB_SCALE_1, SIB_INDEX_NONE, _rm);
        return;
    }
    u8 reg = _reg - 1;
    u8 rm = _rm - 1;
    Assert((mod&~3) == 0 && (reg&~7)==0 && (rm&~7)==0);
    // You probably made a mistake and used REG_BP thinking it works with just ModRM byte.
    Assert(("Use addModRM_SIB instead",!(mod!=0b11 && rm==0b100)));
    // X64_REG_BP isn't available with mod=0b00, You must use a displacement. see intel x64 manual about 32 bit addressing for more details
    Assert(("Use addModRM_disp32 instead",!(mod==0b00 && rm==0b101)));
    // Assert(("Use addModRM_disp32 instead",(mod!=0b10)));
    emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
}
void X64Builder::emit_modrm_rip32(X64Register _reg, u32 disp32){
    u8 reg = _reg - 1;
    u8 mod = 0b00;
    u8 rm = 0b101;
    Assert((mod&~3) == 0 && (reg&~7)==0 && (rm&~7)==0);
    emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
    emit4(disp32);
}
void X64Builder::emit_modrm_sib(u8 mod, X64Register _reg, u8 scale, u8 index, X64Register _base){
    u8 base = _base - 1;
    u8 reg = _reg - 1;
    //  register to register (mod = 0b11) doesn't work with SIB byte
    Assert(("Use addModRM instead",mod!=0b11));

    Assert(("Ignored meaning in SIB byte. Look at intel x64 manual and fix it.",base != 0b101));

    u8 rm = 0b100;
    Assert((mod&~3) == 0 && (reg&~7)==0 && (rm&~7)==0);
    Assert((scale&~3) == 0 && (index&~7)==0 && (base&~7)==0);

    emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
    emit1((u8)(base | (index << (u8)3) | (scale << (u8)6)));
}
void X64Builder::emit_movsx(X64Register reg, X64Register rm, InstructionControl control) {
    Assert(!IS_REG_XMM(reg) && !IS_REG_XMM(rm));
    Assert(!IS_CONTROL_FLOAT(control));
    Assert(!IS_CONTROL_UNSIGNED(control)); // why would we sign extend unsigned operand?

    emit_prefix(PREFIX_REXW, reg, rm);
    if(GET_CONTROL_SIZE(control) == CONTROL_8B) {
        emit2(OPCODE_2_MOVSX_REG_RM8);
    } else if(GET_CONTROL_SIZE(control) == CONTROL_16B) {
        emit1(PREFIX_16BIT);
        emit2(OPCODE_2_MOVSX_REG_RM8);
    }  else if(GET_CONTROL_SIZE(control) == CONTROL_32B) {
        emit1(OPCODE_MOVSXD_REG_RM);
    } else {
        emit1(OPCODE_MOV_REG_RM);
    }
    emit_modrm(MODE_REG, CLAMP_EXT_REG(reg), CLAMP_EXT_REG(rm));
}
void X64Builder::emit_operation(u8 opcode, X64Register reg, X64Register rm, InstructionControl control) {
    int size = 1 << GET_CONTROL_SIZE(control);
    if(size == 1) {
        emit_prefix(0, reg, rm);
        // 8-bit instructions are usually one less than the regular instruction
        // assuming this is perhaps not a great idea
        u8 opcode_8bit = opcode - 1;
        emit1(opcode_8bit);
    } else if(size == 2) {
        emit1(PREFIX_16BIT);
        emit_prefix(0, reg, rm);
        emit1(opcode);
    } else if(size == 4) {
        emit_prefix(0, reg, rm);
        emit1(opcode);
    } else if(size == 8) {
        emit_prefix(PREFIX_REXW, reg, rm);
        emit1(opcode);
    } else Assert(false);
    emit_modrm(MODE_REG, CLAMP_EXT_REG(reg), CLAMP_EXT_REG(rm));
}
void X64Builder::emit_movzx(X64Register reg, X64Register rm, InstructionControl control) {
    Assert(!IS_REG_XMM(reg) && !IS_REG_XMM(rm));
    Assert(!IS_CONTROL_FLOAT(control));
    Assert(!IS_CONTROL_SIGNED(control)); // It's not good to zero extend signed value

    if(GET_CONTROL_SIZE(control) == CONTROL_64B)
        return; // do nothing

    emit_prefix(PREFIX_REXW, reg, rm);
    if(GET_CONTROL_SIZE(control) == CONTROL_8B) {
        emit2(OPCODE_2_MOVZX_REG_RM8);
    } else if(GET_CONTROL_SIZE(control) == CONTROL_16B) {
        emit2(OPCODE_2_MOVZX_REG_RM16);
    }  else if(GET_CONTROL_SIZE(control) == CONTROL_32B) {
        emit1(OPCODE_MOV_REG_RM);
    }
    emit_modrm(MODE_REG, CLAMP_EXT_REG(reg), CLAMP_EXT_REG(rm));
}
void X64Builder::emit_modrm_sib_slash(u8 mod, u8 reg, u8 scale, u8 index, X64Register _base){
    u8 base = _base - 1;
    //  register to register (mod = 0b11) doesn't work with SIB byte
    Assert(("Use addModRM instead",mod!=0b11));

    Assert(("Ignored meaning in SIB byte. Look at intel x64 manual and fix it.",base != 0b101));

    u8 rm = 0b100;
    Assert((mod&~3) == 0 && (reg&~7)==0 && (rm&~7)==0);
    Assert((scale&~3) == 0 && (index&~7)==0 && (base&~7)==0);

    emit1((u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
    emit1((u8)(base | (index << (u8)3) | (scale << (u8)6)));
}
#ifdef gone
void X64Program::printHex(const char* path){
    using namespace engone;
    Assert(this);
    if(path) {
        OutputAsHex(path, (char*)text, head);
    } else {
        #define HEXIFY(X) (char)(X<10 ? '0'+X : 'A'+X - 10)
        log::out << log::LIME << "HEX:\n";
        for(int i = 0;i<head;i++){
            u8 a = text[i]>>4;
            u8 b = text[i]&0xF;
            log::out << HEXIFY(a) << HEXIFY(b) <<" ";
        }
        log::out << "\n";
        #undef HEXIFY
    }
}
void X64Program::printAsm(const char* path, const char* objpath){
    using namespace engone;
    Assert(this);
    if(!objpath) {
        #ifdef OS_WINDOWS
        FileCOFF::WriteFile("bin/garb.obj", this);
        #else
        FileELF::WriteFile("bin/garb.o", this);
        #endif
    }

    auto file = FileOpen(path, FILE_CLEAR_AND_WRITE);

    #ifdef OS_WINDOWS
    std::string cmd = "dumpbin /NOLOGO /DISASM:BYTES ";
    #else
    // -M intel for intel syntax
    std::string cmd = "objdump -M intel -d ";
    #endif
    if(!objpath)
        cmd += "bin/garb.obj";
    else
        cmd += objpath;

    engone::StartProgram((char*)cmd.c_str(), PROGRAM_WAIT, nullptr, {}, file);

    engone::FileClose(file);
}
#endif

/* dumpbin
Dump of file bin\dev.obj

File Type: COFF OBJECT

main:
  0000000000000000: FF F5              push        rbp
  0000000000000002: 48 8B EC           mov         rbp,rsp
  0000000000000005: 48 81 EC 08 00 00  sub         rsp,8
                    00
  000000000000000C: 33 C0              xor         eax,eax
  000000000000000E: FF F0              push        rax
  0000000000000010: 8F C0              pop         rax
  0000000000000012: 89 45 F8           mov         dword ptr [rbp-8],eax
  0000000000000015: 48 81 EC 08 00 00  sub         rsp,8
                    00
  000000000000001C: 48 33 C0           xor         rax,rax
  000000000000001F: C7 C0 41 00 00 00  mov         eax,41h
  0000000000000025: FF F0              push        rax
  0000000000000027: 48 33 C0           xor         rax,rax
  000000000000002A: 8F C0              pop         rax
  000000000000002C: 88 45 F0           mov         byte ptr [rbp-10h],al
  000000000000002F: 48 81 C4 10 00 00  add         rsp,10h
                    00
  0000000000000036: 8F C5              pop         rbp
  0000000000000038: C3                 ret

  Summary

          12 .debug_abbrev
          4B .debug_info
          31 .debug_line
          39 .text
*/
/* objdump
wa.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <_Z3heyii>:
   0:   f3 0f 1e fa             endbr64 
   4:   55                      push   %rbp
   5:   48 89 e5                mov    %rsp,%rbp
   8:   89 7d fc                mov    %edi,-0x4(%rbp)
   b:   89 75 f8                mov    %esi,-0x8(%rbp)
   e:   8b 55 fc                mov    -0x4(%rbp),%edx
  11:   8b 45 f8                mov    -0x8(%rbp),%eax
  14:   01 d0                   add    %edx,%eax
  16:   5d                      pop    %rbp
  17:   c3                      ret    

0000000000000018 <main>:
  18:   f3 0f 1e fa             endbr64 
  1c:   55                      push   %rbp
  1d:   48 89 e5                mov    %rsp,%rbp
  20:   be 05 00 00 00          mov    $0x5,%esi
  25:   bf 02 00 00 00          mov    $0x2,%edi
  2a:   e8 00 00 00 00          call   2f <main+0x17>
  2f:   b8 00 00 00 00          mov    $0x0,%eax
  34:   5d                      pop    %rbp
  35:   c3                      ret   
*/
void X64Program::Destroy(X64Program* program) {
    using namespace engone;
    Assert(program);
    program->~X64Program();
    TRACK_FREE(program, X64Program);
    // engone::Free(program,sizeof(X64Program));
}
X64Program* X64Program::Create() {
    using namespace engone;

    // auto program = (X64Program*)engone::Allocate(sizeof(X64Program));
    auto program = TRACK_ALLOC(X64Program);
    new(program)X64Program();
    return program;
}

void X64Builder::emit_push(X64Register reg, int size) {
    if(IS_REG_XMM(reg)) {
        emit_sub_imm32(X64_REG_SP, 8);
        if(size == 4) emit3(OPCODE_3_MOVSS_RM_REG);
        else if(size == 8) emit3(OPCODE_3_MOVSD_RM_REG);
        else Assert(false);
        emit_modrm(MODE_DEREF, CLAMP_XMM(reg), X64_REG_SP);
    } else {
        emit_prefix(0, X64_REG_INVALID, reg);
        emit1(OPCODE_PUSH_RM_SLASH_6);
        emit_modrm_slash(MODE_REG, 6, CLAMP_EXT_REG(reg));
    }
    push_offset += 8;
}
void X64Builder::emit_pop(X64Register reg, int size) {
    if(IS_REG_XMM(reg)) {
        if(size == 4) emit3(OPCODE_3_MOVSS_REG_RM);
        else if(size == 8) emit3(OPCODE_3_MOVSD_REG_RM);
        else Assert(false);
        emit_modrm(MODE_DEREF, CLAMP_XMM(reg), X64_REG_SP);
        emit_add_imm32(X64_REG_SP, 8);
    } else {
        emit_prefix(0, X64_REG_INVALID, reg);
        emit1(OPCODE_POP_RM_SLASH_0);
        emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg));
    }
    push_offset -= 8;
}
void X64Builder::emit_add_imm32(X64Register reg, i32 imm32) {
    emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg);
    emit1(OPCODE_ADD_RM_IMM_SLASH_0);
    emit_modrm_slash(MODE_REG, 0, CLAMP_EXT_REG(reg));
    emit4((u32)imm32); // NOTE: cast from i16 to i32 to u32, should be fine
}
void X64Builder::emit_sub_imm32(X64Register reg, i32 imm32) {
    emit_prefix(PREFIX_REXW, X64_REG_INVALID, reg);
    emit1(OPCODE_SUB_RM_IMM_SLASH_5);
    emit_modrm_slash(MODE_REG, 5, CLAMP_EXT_REG(reg));
    emit4((u32)imm32);
}
void X64Builder::emit_prefix(u8 inherited_prefix, X64Register reg, X64Register rm) {
    inherited_prefix = construct_prefix(inherited_prefix, reg, rm);
    if(inherited_prefix)
        emit1(inherited_prefix);
}
u8 X64Builder::construct_prefix(u8 inherited_prefix, X64Register reg, X64Register rm) {
    if(reg != X64_REG_INVALID) {
        if(IS_REG_EXTENDED(reg)) {
            inherited_prefix |= PREFIX_REXR;
        } else if(IS_REG_NORM(reg)) {
            
        } else Assert(false); // float register not allowed like this
    }
    if(rm != X64_REG_INVALID) {
        if(IS_REG_EXTENDED(rm)) {
            inherited_prefix |= PREFIX_REXB;
        } else if(IS_REG_NORM(rm)) {
            
        } else Assert(false); // float register not allowed like this
    }
    return inherited_prefix;
}
void X64Builder::emit_mov_reg_mem(X64Register reg, X64Register rm, InstructionControl control, int disp) {
    int size = GET_CONTROL_SIZE(control);
    // if (IS_CONTROL_FLOAT(control)) {
    if (IS_REG_XMM(reg)) {
        emit_prefix(0, X64_REG_INVALID, rm);
        if(size == CONTROL_32B)
            emit3(OPCODE_3_MOVSS_REG_RM);
        else if(size == CONTROL_64B)
            emit3(OPCODE_3_MOVSD_REG_RM);
        else Assert(false);

        u8 mode = MODE_DEREF_DISP32;
        if(disp == 0) {
            mode = MODE_DEREF;
        } else if(disp >= -0x80 && disp < 0x7F) {
            mode = MODE_DEREF_DISP8;
        }
        emit_modrm(mode, CLAMP_XMM(reg), CLAMP_EXT_REG(rm));
        if(mode == MODE_DEREF) {

        } else if(mode == MODE_DEREF_DISP8)
            emit1((u8)(i8)disp);
        else
            emit4((u32)(i32)disp);
    } else {
        Assert(!IS_CONTROL_FLOAT(control));
        if(size == CONTROL_16B) {
            emit1(PREFIX_16BIT);
        }
        if(size == CONTROL_64B) {
            emit_prefix(PREFIX_REXW, reg, rm);
        } else {
            emit_prefix(0, reg, rm);
        }
        if(size == CONTROL_8B) {
            emit1(OPCODE_MOV_REG8_RM8);
        } else {
            emit1(OPCODE_MOV_REG_RM);
        }

        u8 mode = MODE_DEREF_DISP32;
        if(disp == 0) {
            mode = MODE_DEREF;
        } else if(disp >= -0x80 && disp < 0x7F) {
            mode = MODE_DEREF_DISP8;
        }
        emit_modrm(mode, CLAMP_EXT_REG(reg), CLAMP_EXT_REG(rm));
        if(mode == MODE_DEREF) {

        } else if(mode == MODE_DEREF_DISP8)
            emit1((u8)(i8)disp);
        else
            emit4((u32)(i32)disp);
    }
}
void X64Builder::emit_mov_mem_reg(X64Register rm, X64Register reg, InstructionControl control, int disp) {
    int size = GET_CONTROL_SIZE(control);
    // if (IS_CONTROL_FLOAT(control)) {
    if (IS_REG_XMM(reg)) {
        emit_prefix(0, X64_REG_INVALID, rm);
        if(size == CONTROL_32B)
            emit3(OPCODE_3_MOVSS_RM_REG);
        else if(size == CONTROL_64B)
            emit3(OPCODE_3_MOVSD_RM_REG);
        else Assert(false);

        u8 mode = MODE_DEREF_DISP32;
        if(disp == 0) {
            mode = MODE_DEREF;
        } else if(disp >= -0x80 && disp < 0x7F) {
            mode = MODE_DEREF_DISP8;
        }
        emit_modrm(mode, CLAMP_XMM(reg), CLAMP_EXT_REG(rm));
        if(mode == MODE_DEREF) {

        } else if(mode == MODE_DEREF_DISP8)
            emit1((u8)(i8)disp);
        else
            emit4((u32)(i32)disp);
    } else {
        Assert(!IS_CONTROL_FLOAT(control));
        if(size == CONTROL_16B) {
            emit1(PREFIX_16BIT);
        }
        if(size == CONTROL_64B) {
            emit_prefix(PREFIX_REXW, reg, rm);
        } else {
            emit_prefix(0, reg, rm);
        }
        if(size == CONTROL_8B) {
            emit1(OPCODE_MOV_RM_REG8);
        } else {
            emit1(OPCODE_MOV_RM_REG);
        }

        u8 mode = MODE_DEREF_DISP32;
        if(disp == 0) {
            mode = MODE_DEREF;
        } else if(disp >= -0x80 && disp < 0x7F) {
            mode = MODE_DEREF_DISP8;
        }
        emit_modrm(mode, CLAMP_EXT_REG(reg), CLAMP_EXT_REG(rm));
        if(mode == MODE_DEREF) {

        } else if(mode == MODE_DEREF_DISP8)
            emit1((u8)(i8)disp);
        else
            emit4((u32)(i32)disp);
    }
}
void X64Builder::emit_mov_reg_reg(X64Register reg, X64Register rm, int size) {
    if(IS_REG_XMM(reg) && IS_REG_XMM(rm)) {
        if(size == 4) emit3(OPCODE_3_MOVSS_RM_REG);
        else if(size == 8) emit3(OPCODE_3_MOVSD_RM_REG);
        else Assert(false);
        emit_modrm(MODE_REG, CLAMP_XMM(reg), CLAMP_XMM(rm));
    } else if(IS_REG_XMM(reg) && !IS_REG_XMM(rm)) {
        Assert(false); // did you mean do move xmm to general?
        emit_prefix(PREFIX_REXW, rm, X64_REG_SP);
        emit1(OPCODE_MOV_RM_REG);
        emit_modrm(MODE_DEREF_DISP8, CLAMP_EXT_REG(rm), X64_REG_SP);
        emit1((u8)-8);

        emit3(OPCODE_3_MOVSD_REG_RM);
        emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(reg), X64_REG_SP);
        emit1((u8)-8);
    } else if(!IS_REG_XMM(reg) && IS_REG_XMM(rm)) {
        Assert(false); // did you mean do move xmm to general?
        emit3(OPCODE_3_MOVSD_RM_REG);
        emit_modrm(MODE_DEREF_DISP8, CLAMP_XMM(rm), X64_REG_SP);
        emit1((u8)-8);

        emit_prefix(PREFIX_REXW, reg, X64_REG_SP);
        emit1(OPCODE_MOV_REG_RM);
        emit_modrm(MODE_DEREF_DISP8, CLAMP_EXT_REG(reg), X64_REG_SP);
        emit1((u8)-8);
    } else if(!IS_REG_XMM(reg) && !IS_REG_XMM(rm)) {
        emit_prefix(PREFIX_REXW,reg,rm);
        emit1(OPCODE_MOV_REG_RM);
        emit_modrm(MODE_REG, CLAMP_EXT_REG(reg), CLAMP_EXT_REG(rm));
    }
}

static const char* x64_register_names[]{
    "INVALID",// X64_REG_INVALID = 0,
    "A",// X64_REG_A,
    "C",// X64_REG_C,
    "D",// X64_REG_D,
    "B",// X64_REG_B,
    "SP",// X64_REG_SP,
    "BP",// X64_REG_BP,
    "SI",// X64_REG_SI,
    "DI",// X64_REG_DI,
    "R8",// X64_REG_R8, 
    "R9",// X64_REG_R9, 
    "R10",// X64_REG_R10,
    "R11",// X64_REG_R11,
    "R12",// X64_REG_R12,
    "R13",// X64_REG_R13,
    "R14",// X64_REG_R14,
    "R15",// X64_REG_R15,
    "XMM0",// X64_REG_XMM0,
    "XMM1",// X64_REG_XMM1,
    "XMM2",// X64_REG_XMM2,
    "XMM3",// X64_REG_XMM3,
    "XMM4",// X64_REG_XMM4,
    "XMM5",// X64_REG_XMM5,
    "XMM6",// X64_REG_XMM6,
    "XMM7",// X64_REG_XMM7,
};
engone::Logger& operator <<(engone::Logger& l, X64Register r) {
    l << x64_register_names[r];
    return l;
}