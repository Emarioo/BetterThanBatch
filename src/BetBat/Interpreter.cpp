#include "BetBat/Interpreter.h"
#include "Engone/Logger.h"

#define BITS(P,B,E,S) ((P<<(S-E))>>B)

#define DECODE_OPCODE(I) I->opcode
#define DECODE_REG0(I) I->op0
#define DECODE_REG1(I) I->op1
#define DECODE_REG2(I) I->op2

// #define DECODE_TYPE(ptr) (*((u8*)ptr+1))

void Interpreter::cleanup(){
    stack.resize(0);   
}

void Interpreter::printRegisters(){
    using namespace engone;
    log::out << "RAX: "<<rax<<" "<<*(float*)&rax<<"\n";
    log::out << "RBX: "<<rbx<<" "<<*(float*)&rbx<<"\n";
    log::out << "RCX: "<<rcx<<" "<<*(float*)&rcx<<"\n";
    log::out << "RDX: "<<rdx<<" "<<*(float*)&rdx<<"\n";
    log::out << "SP: "<<sp<<"\n";
    log::out << "FP: "<<fp<<"\n";
    log::out << "PC: "<<pc<<"\n";
}
void* Interpreter::getReg(u8 id){
    switch(id){
        case BC_REG_RAX: return &rax;
        case BC_REG_RBX: return &rbx;
        case BC_REG_RCX: return &rcx;
        case BC_REG_RDX: return &rdx;
        case BC_REG_SP: return &sp;
        case BC_REG_FP: return &fp;
    }
    engone::log::out <<"(RegID: "<<id<<")\n";
    Assert("tried to access bad register")
    return 0;
}

void Interpreter::execute(BytecodeX* bytecode){
    using namespace engone;
    _VLOG(log::out <<log::BLUE<< "##   Interpreter   ##\n";)
    stack.resize(100*1024);
    
    pc = 0;
    sp = (u64)stack.data;
    fp = (u64)stack.data;
    
    auto tp = MeasureSeconds();

    while (true) {
        // check used

        if(pc>=(u64)bytecode->length())
            break;

        InstructionX* inst = bytecode->get(pc);
        if(inst)
            _ILOG(log::out << pc<<": "<<*inst<<", ";)
        pc++;
        u8 opcode = DECODE_OPCODE(inst);
        switch (opcode) {
        case BC_ADDI:
        case BC_SUBI:
        case BC_MULI:
        case BC_DIVI:
        case BC_ADDF:
        case BC_SUBF:
        case BC_MULF:
        case BC_DIVF:
        case BC_EQ:
        case BC_NEQ:
        case BC_LT:
        case BC_LTE:
        case BC_GT:
        case BC_GTE:
        case BC_ANDI:
        case BC_ORI:
        {
            u8 r0 = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);
            u8 r2 = DECODE_REG2(inst);

            if (DECODE_REG_TYPE(r0) != DECODE_REG_TYPE(r1) || DECODE_REG_TYPE(r1) != DECODE_REG_TYPE(r2)){
                log::out << log::RED<<"register bit mismatch\n";
                continue;   
            }
            void* xp = getReg(r0);
            void* yp = getReg(r1);
            void* outp = getReg(r2);
            
            u64 x = *(u64*)xp;
            u64 y = *(u64*)yp;
            u64& out = *(u64*)outp;
            
            #define GEN_OP(OP) out = x OP y; _ILOG(log::out << out << " = "<<x <<" "<< #OP << " "<<y;) break;
            #define GEN_OPF(OP) *(float*)&out = *(float*)&x OP *(float*)&y; _ILOG(log::out << *(float*)&out << " = "<<*(float*)&x <<" "<< #OP << " "<<*(float*)&y;) break;
            switch(opcode){
                case BC_ADDI: GEN_OP(+)   
                case BC_SUBI: GEN_OP(-)   
                case BC_MULI: GEN_OP(*)   
                case BC_DIVI: GEN_OP(/)
                case BC_ADDF: GEN_OPF(+)   
                case BC_SUBF: GEN_OPF(-)   
                case BC_MULF: GEN_OPF(*)   
                case BC_DIVF: GEN_OPF(/)

                case BC_EQ: GEN_OP(==)
                case BC_NEQ: GEN_OP(!=)
                case BC_LT: GEN_OP(<)
                case BC_LTE: GEN_OP(<=)
                case BC_GT: GEN_OP(>)
                case BC_GTE: GEN_OP(>=)
                case BC_ANDI: GEN_OP(&&)
                case BC_ORI: GEN_OP(||)
            }
            #undef GEN_OP
            
            // #define GEN_BIT(B, OP) if(DECODE_REG_TYPE(r0)==BC_REG_##B ) *((u##B* ) out) = *((u##B* ) x) OP *((u##B* ) y);
            // GEN_BIT(8,+)
            // else GEN_BIT(16,+)
            // else GEN_BIT(32,+)
            // else GEN_BIT(64,+)
            // else
            //      log::out << log::RED<< __FILE__<<"unknown reg type\n";
            // #undef GEN_BIT
            _ILOG(log::out << "\n";)
            break;
        }
        case BC_NOTB:{
            u8 r0 = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);

            if (DECODE_REG_TYPE(r0) != DECODE_REG_TYPE(r1)){
                log::out << log::RED<<"register bit mismatch\n";
                continue;   
            }
            void* xp = getReg(r0);
            void* outp = getReg(r1);
            
            u64 x = *(u64*)xp;
            u64& out = *(u64*)outp;
            
            #define GEN_OP(OP) out = OP x; _ILOG(log::out << out << " = "<<#OP <<" "<< x;) break;
            switch(opcode){
                case BC_NOTB: GEN_OP(!)
            }
            #undef GEN_OP
            
            _ILOG(log::out << "\n";)
            break;
        }
        case BC_MOV_MR:{
            u8 r0 = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);

            if(DECODE_REG_TYPE(r0) != BC_REG_64){
                log::out << log::RED<<"mov mr must use 64 bit regs\n";
                continue;   
            }
            void* from = (void*)(*(u64*)getReg(r0)); // NOTE: Program can crash here
            void* to = getReg(r1);
            
            if(DECODE_REG_TYPE(r1)==BC_REG_8 ) *((u8* ) to) = *((u8* ) from);
            if(DECODE_REG_TYPE(r1)==BC_REG_16) *((u16*) to) = *((u16*) from);
            if(DECODE_REG_TYPE(r1)==BC_REG_32) *((u32*) to) = *((u32*) from);
            if(DECODE_REG_TYPE(r1)==BC_REG_64) *((u64*) to) = *((u64*) from);

            _ILOG(log::out << " = "<<(*(u64* )to)<<"\n";)

            break;
        }
        case BC_MOV_RM:{
            u8 r0 = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);

            if(DECODE_REG_TYPE(r0) != BC_REG_64){
                log::out << log::RED<<"mov mr must use 64 bit regs\n";
                continue;
            }
            void* from = getReg(r0); // NOTE: Program can crash here
            void* to = (void*)(*(u64*)getReg(r1));

            *((u64*) to)= *(u64*)from;

            // if(r1&BC_REG_8 ) *((u8* ) to) = *((u8* ) from);
            // if(r1&BC_REG_16) *((u16*) to) = *((u16*) from);
            // if(r1&BC_REG_32) *((u32*) to) = *((u32*) from);
            // if(r1&BC_REG_64) *((u64*) to) = *((u64*) from);

            _ILOG(log::out << " = "<<(*(u64* )to)<<"\n";)
            
            break;
        }
        case BC_MOV_RR:{
            u8 r0 = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);
            if (DECODE_REG_TYPE(r0) != DECODE_REG_TYPE(r1)){
                log::out << __FILE__<<"register bit mismatch\n";
                continue;
            }
            void* from = getReg(r0);
            void* to = getReg(r1);
            if(r0&BC_REG_8 ) *((u8* ) to) = *((u8* ) from);
            if(r0&BC_REG_16) *((u16*) to) = *((u16*) from);
            if(r0&BC_REG_32) *((u32*) to) = *((u32*) from);
            if(r0&BC_REG_64) *((u64*) to) = *((u64*) from);

            _ILOG(log::out << " = "<<(*(u64* )to)<<"\n";)
            break;
        }
        case BC_LI:{
            u8 r0 = DECODE_REG0(inst);
            
            void* out = getReg(r0);
            
            u32 data = *(u32*)bytecode->get(pc);
            pc++;
            
            _ILOG(log::out << data<<"\n";)
            
            u8 t = DECODE_REG_TYPE(r0);
            if(t==BC_REG_8){ // higher 8 bit register doesn't align properly.
                log::out <<__FILE__<< "register bit mismatch\n";
                continue;   
            }
            
            *((u64*) out) = data; // NOTE: what if register is 16 bit?
            break;
            
        }
        case BC_PUSH:{
            u8 r0 = DECODE_REG0(inst);
            
            if(sp-8>=(uint64)stack.data + stack.max){
                log::out <<__FILE__<< "stack overflow (sp: "<<sp<<", max: "<<((u64)stack.data+stack.max)<<")\n";
                return;
            }
            
            void* ptr = getReg(r0);
            void* stackPtr = (void*)sp;
            
            sp+=8; // +=1 may not work if you do +=4 after since memory would be unaligned.
            
            *((u64*) stackPtr) = *(u64*)ptr;
            _ILOG(log::out << (*(u64*)stackPtr)<<"\n";)
            break;
        }
        case BC_POP: {
            u8 r0 = DECODE_REG0(inst);
            
            sp-=8; // +=1 may not work if you do +=4 after since memory would be unaligned.
            // log::out << " "<<(void*)sp<<" "<<stack.data <<"\n";
            void* ptr = getReg(r0);
            void* stackPtr = (void*)sp;
            
            *(u64*)ptr = *((u64*) stackPtr);
            _ILOG(log::out << (*(u64*)ptr)<<"\n";)
            break;
        }
        case BC_JMP: {
            // load immediate
            u32 data = *(u32*)bytecode->get(pc);
            pc++;
            // TODO: relative immediate instead?
            //   can't see any benefit right now.
            _ILOG(log::out << "\n";)
            pc = data;
            break;
        }
        case BC_JE:
        case BC_JNE: {
            u8 r0 = DECODE_REG0(inst);

            u32 data = *(u32*)bytecode->get(pc);
            pc++;

            void* ptr = getReg(r0);
            u64 testValue = *((u64*)ptr);

            _ILOG(log::out << testValue<<"\n";)
            
            bool yes=false;
            switch (opcode){
                case BC_JE: yes = testValue!=0; break;
                case BC_JNE: yes = testValue==0; break;
            }
            if(yes){
                pc = data;
            }
            break;
        }
        } // for switch
    }
    printRegisters();
    auto time = StopMeasure(tp);
    log::out << "Time: "<<time<<"\n";
       
}