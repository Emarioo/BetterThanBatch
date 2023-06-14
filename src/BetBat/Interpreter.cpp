#include "BetBat/Interpreter.h"

#define BITS(P,B,E,S) ((P<<(S-E))>>B)

#define DECODE_OPCODE(I) I->opcode
#define DECODE_REG0(I) I->op0
#define DECODE_REG1(I) I->op1
#define DECODE_REG2(I) I->op2

// #define SP_CHANGE(incr) log::out << "sp "<<(i64)(sp-(u64)stack.data-(incr))<<" -> "<<(i64)(sp-(u64)stack.data)<<"\n";
#define SP_CHANGE(incr)

// #define DECODE_TYPE(ptr) (*((u8*)ptr+1))

void Interpreter::cleanup(){
    stack.resize(0);   
}

void Interpreter::printRegisters(){
    using namespace engone;
    log::out << "RAX: "<<(i64)rax<<" "<<*(float*)&rax<<"\n";
    log::out << "RBX: "<<(i64)rbx<<" "<<*(float*)&rbx<<"\n";
    log::out << "RCX: "<<(i64)rcx<<" "<<*(float*)&rcx<<"\n";
    log::out << "RDX: "<<(i64)rdx<<" "<<*(float*)&rdx<<"\n";
    log::out << "SP: "<<sp<<"\n";
    log::out << "FP: "<<fp<<"\n";
    log::out << "PC: "<<pc<<"\n";
}
void* Interpreter::getReg(u8 id){
    // #define CASE(K,V) case BC_REG_##K: return V;
    #define CASER(K,V) case BC_REG_R##K##X: return &r##V##x;\
    case BC_REG_E##K##X: return &r##V##x;\
    case BC_REG_##K##X: return &r##V##x;\
    case BC_REG_##K##L: return &r##V##x;\
    case BC_REG_##K##H: return (void*)((u8*)&r##V##x+1);
    switch(id){
        CASER(A,a)
        CASER(B,b)
        CASER(C,c)
        CASER(D,d)

        case BC_REG_SP: return &sp;
        case BC_REG_FP: return &fp;
        case BC_REG_PC: return &pc;
        case BC_REG_DP: return &dp;
    }
    #undef CASER
    engone::log::out <<"(RegID: "<<id<<")\n";
    Assert("tried to access bad register")
    return 0;
}
void yeah(int reg, void* from, void* to){
    using namespace engone;
    int size = DECODE_REG_TYPE(reg);
    if(size==BC_REG_8 ){
        *((u8* ) to) = *((u8* ) from);
        _ILOG(log::out << (*(i8*)to);)
    }else if(size==BC_REG_16) {
        *((u16*) to) = *((u16*) from);
        _ILOG(log::out << (*(i16*)to);)
    }else if(size==BC_REG_32)   { 
         *((u32*) to) = *((u32*) from);
         _ILOG(log::out << (*(i32*)to);)
    }else if(size==BC_REG_64){
         *((u64*) to) = *((u64*) from);
        _ILOG(log::out << (*(i64*)to);)
    }else 
        log::out <<log::RED <<"bad set to from\n";
        
    // _ILOG(log::out << (*(i64*)to)<<"\n";)
}
void Interpreter::execute(Bytecode* bytecode){
    using namespace engone;
    if(!bytecode){
        log::out << log::RED << __FUNCTION__<<"Cannot execute null bytecode\n";
        return;   
    }
    
    _VLOG(log::out <<log::BLUE<< "##   Interpreter   ##\n";)
    stack.resize(100*1024);
    
    pc = 0;
    sp = (u64)stack.data+stack.max;
    fp = (u64)stack.data+stack.max;
    dp = (u64)bytecode->dataSegment.data;
    
    auto tp = MeasureSeconds();
    
    _ILOG(log::out << "sp = "<<sp<<"\n";)
    
    // u64* savedFp = 0;
    
    u64 length = bytecode->codeSegment.used;
    Instruction* codePtr = (Instruction*)bytecode->codeSegment.data;
    while (true) {
        // check used

        // if(pc>=(u64)bytecode->length())
        if(pc>=(u64)length)
            break;

        // Instruction* inst = bytecode->get(pc);
        Instruction* inst = codePtr + pc;
        
        // _ILOG(log::out <<log::GRAY<<" sp: "<< sp <<" fp: "<<fp;)
        // if(savedFp)
        //     _ILOG(log::out <<" sfp: "<< *savedFp;)
        // _ILOG(log::out <<"\n";)
        _ILOG(
        const char* str = bytecode->getDebugText(pc);
        if(str)
            log::out << log::GRAY<<  str;)
        
        _ILOG(if(inst)
                log::out << pc<<": "<<*inst<<", ";)
        log::out.flush(); // flush the instruction in case a crash occurs.

        pc++;
        u8 opcode = DECODE_OPCODE(inst);
        switch (opcode) {
        case BC_MODI:
        case BC_MODF:
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
        case BC_BAND:
        case BC_BOR:
        case BC_BXOR:
        case BC_BLSHIFT:
        case BC_BRSHIFT:
        {
            u8 r0 = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);
            u8 r2 = DECODE_REG2(inst);

            // if (DECODE_REG_TYPE(r0) != DECODE_REG_TYPE(r1) || DECODE_REG_TYPE(r1) != DECODE_REG_TYPE(r2)){
            //     log::out << log::RED<<"register bit mismatch\n";
            //     continue;   
            // }
            void* xp = getReg(r0);
            void* yp = getReg(r1);
            void* op = getReg(r2);
            
            i64 x = 0;
            i64 y = 0;
            int xs = 1<<DECODE_REG_TYPE(r0);
            int ys = 1<<DECODE_REG_TYPE(r1);
            int os = 1<<DECODE_REG_TYPE(r2);
            if(xs==1){
                x = *(i8*)xp;
            }
            if(xs==2){
                x = *(i16*)xp;
            }
            if(xs==4){
                x = *(i32*)xp;
            }
            if(xs==8){
                x = *(i64*)xp;
            }
            if(ys==1){
                y = *(i8*)yp;
            }
            if(ys==2){
                y = *(i16*)yp;
            }
            if(ys==4){
                y = *(i32*)yp;
            }
            if(ys==8){
                y = *(i64*)yp;
            }
            i64 out = 0;
            
            u64 oldSp = sp;

            if(opcode==BC_ADDF||opcode==BC_SUBF||opcode==BC_MULF||opcode==BC_DIVF
                ||opcode==BC_MODF){
                if(os!=4||xs!=4||ys!=4){
                    log::out << log::RED << "float operation requires 4 byte register";
                    return;
                }
            }
            #define GEN_OP(OP) out = x OP y; _ILOG(log::out << out << " = "<<x <<" "<< #OP << " "<<y;) break;
            #define GEN_OPF(OP) *(float*)&out = *(float*)&x OP *(float*)&y; _ILOG(log::out << *(float*)&out << " = "<<*(float*)&x <<" "<< #OP << " "<<*(float*)&y;) break;
            switch(opcode){
                case BC_MODI: GEN_OP(%)
                case BC_MODF: *(float*)&out = fmod(*(float*)&x,*(float*)&y); _ILOG(log::out << *(float*)&out << " = "<<*(float*)&x <<" % "<<*(float*)&y;) break;
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
                
                case BC_BXOR: GEN_OP(^)
                case BC_BOR: GEN_OP(|)
                case BC_BAND: GEN_OP(&)
                case BC_BLSHIFT: GEN_OP(<<)
                case BC_BRSHIFT: GEN_OP(>>)
            }
            #undef GEN_OP
            
            _ILOG(log::out << "\n";)
            if(r2==BC_REG_SP){
                SP_CHANGE(sp-oldSp)
            }
            if(os==1){
                *(i8*)op = out;
            }
            if(os==2){
                *(i16*)op = out;
            }
            if(os==4){
                *(i32*)op = out;
            }
            if(os==8){
                *(i64*)op = out;
            }
            
            // #define GEN_BIT(B, OP) if(DECODE_REG_TYPE(r0)==BC_REG_##B ) *((u##B* ) out) = *((u##B* ) x) OP *((u##B* ) y);
            // GEN_BIT(8,+)
            // else GEN_BIT(16,+)
            // else GEN_BIT(32,+)
            // else GEN_BIT(64,+)
            // else
            //      log::out << log::RED<< __FILE__<<"unknown reg type\n";
            // #undef GEN_BIT
            break;
        }
        case BC_INCR:{
            u8 r0 = DECODE_REG0(inst);
            i16 offset = (i16)((u16)DECODE_REG1(inst) | ((u16)DECODE_REG2(inst)<<8));
            
            i64* regValue = (i64*)getReg(r0);
            *regValue += offset;
            
            _ILOG(log::out << "\n";)
            break;
        }
        case BC_NOT:
        case BC_BNOT:{
            u8 r0 = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);

            if (DECODE_REG_TYPE(r0) != DECODE_REG_TYPE(r1)){
                log::out << log::RED<<"register bit mismatch\n";
                continue;
            }
            void* xp = getReg(r0);
            void* op = getReg(r1);
            
            i64 x = 0;
            int xs = 1<<DECODE_REG_TYPE(r0);
            int os = 1<<DECODE_REG_TYPE(r1);
            if(xs==1){
                x = *(i8*)xp;
            }
            if(xs==2){
                x = *(i16*)xp;
            }
            if(xs==4){
                x = *(i32*)xp;
            }
            if(xs==8){
                x = *(i64*)xp;
            }
            i64 out = 0;
            u64 oldSp = sp;

            #define GEN_OP(OP) out = OP x; _ILOG(log::out << out << " = "<<#OP <<" "<< x;) break;
            switch(opcode){
                case BC_NOT: GEN_OP(!)
                case BC_BNOT: GEN_OP(~)
            }
            #undef GEN_OP
            
            _ILOG(log::out << "\n";)

            if(os==1){
                *(i8*)op = out;
            }
            if(os==2){
                *(i16*)op = out;
            }
            if(os==4){
                *(i32*)op = out;
            }
            if(os==8){
                *(i64*)op = out;
            }

            break;
        }
        case BC_MOV_MR:{
            u8 r0 = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);
            // u8 offset = DECODE_REG2(inst);

            if(DECODE_REG_TYPE(r0) != BC_REG_64){
                log::out << log::RED<<"r0 (pointer) must use 64 bit registers\n";
                continue;   
            }
            u64* fromptr = (u64*)getReg(r0);
            void* from = (void*)(*fromptr); // NOTE: Program can crash here
            // void* from = (void*)(*(u64*)getReg(r0) + offset); // NOTE: Program can crash here
            void* to = getReg(r1);
            
            // SET_TO_FROM(r1)
            yeah(r1,from,to);
            
            // *((u64*) to) = 0;
            // if(DECODE_REG_TYPE(r1)==BC_REG_8) *((u8* ) to) = *((u8* ) from);
            // if(DECODE_REG_TYPE(r1)==BC_REG_16) *((u16*) to) = *((u16*) from);
            // if(DECODE_REG_TYPE(r1)==BC_REG_32) *((u32*) to) = *((u32*) from);
            // if(DECODE_REG_TYPE(r1)==BC_REG_64) *((u64*) to) = *((u64*) from);

            // _ILOG(log::out << " = "<<(*(u64*)to)<<"\n";)
            _ILOG(log::out <<"\n";)

            break;
        }
        case BC_MOV_RM:{
            u8 r0 = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);
            // u8 offset = DECODE_REG2(inst);

            if(DECODE_REG_TYPE(r1) != BC_REG_64){
                log::out << log::RED<<"r1 (pointer) must use 64 bit registers\n";
                continue;
            }
            void* toptr = getReg(r1);
            int size = 1<<DECODE_REG_TYPE(r0);
            if((*(u64*)toptr)%size!=0){
                log::out << log::RED<<(*(u64*)toptr)<<" does not align to "<<size<<"\n";
                continue;
            }
            
            void* from = getReg(r0); // NOTE: Program can crash here
            void* to = (void*)(*(u64*)toptr);
            // void* to = (void*)(*(u64*)getReg(r1)+offset);

            // SET_TO_FROM(r0)
            
            
            
            yeah(r0,from,to);
            // *((u64*) to) = 0;
            // if(r1&BC_REG_8 ) *((u8* ) to) = *((u8* ) from);
            // if(r1&BC_REG_16) *((u16*) to) = *((u16*) from);
            // if(r1&BC_REG_32) *((u32*) to) = *((u32*) from);
            // if(r1&BC_REG_64) *((u64*) to) = *((u64*) from);

            _ILOG(log::out <<"\n";)
            
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

            // SET_TO_FROM(r1)
            yeah(r1,from,to);

            _ILOG(log::out << " = "<<(*(u64* )to)<<"\n";)
            break;
        }
        case BC_ZERO_MEM:{
            u8 r0 = DECODE_REG0(inst);
            u16 size = (u16)DECODE_REG1(inst) | ((u16)DECODE_REG2(inst)<<8);
            
            void* to = getReg(r0);

            memset(to,0,size);

            _ILOG(log::out << " [0-"<<size<<"]\n";)
            break;
        }
        case BC_LI:{
            u8 r0 = DECODE_REG0(inst);
            
            void* out = getReg(r0);
            
            void* datap = (codePtr + pc);
            i32 data = *(i32*)(codePtr + pc);
            // u32 data = *(u32*)bytecode->get(pc);
            pc++;
            
            _ILOG(log::out << data<<"\n";)
            
            u8 t = DECODE_REG_TYPE(r0);
            if(t==BC_REG_8){
                *((i8*)out) = data;
            } else if(t==BC_REG_16){
                *((i16*)out) = data;   
            } else if(t==BC_REG_32){
                *((i32*)out) = data;   
            } else if(t==BC_REG_64){
                *((i64*)out) = data;   
            }
            break;
        }
        case BC_PUSH:{
            u8 r0 = DECODE_REG0(inst);
            int rsize = 1<<DECODE_REG_TYPE(r0);
            
            if((i64)sp-(i64)stack.data - rsize > (i64)stack.max){
                log::out <<log::RED<<__FUNCTION__<< ": stack pointer was messed with (sp: "<<
                ((i64)sp-(i64)stack.data)<<", max: "<<
                (stack.max)<<")\n";
                return;
            } else if((i64)sp-(i64)stack.data < (i64)0){
                // NOTE: This is not underflow even though sp is less than base stack pointer because
                //  stack grows down and not up.
                log::out<<log::RED <<__FUNCTION__<< ": stack "<<log::GOLD<<"overflow"<<log::RED<<" (sp: "<<
                ((i64)sp-(i64)stack.data)<<", max: "<<
                (stack.max)<<")\n";
                return;
            }

            sp-=rsize;
            void* to = (void*)sp;
            void* from = getReg(r0);
            yeah(r0,from,to);
            
            _ILOG(log::out <<"\n";)
            SP_CHANGE(-rsize)
            break;
        }
        case BC_POP: {
            u8 r0 = DECODE_REG0(inst);
            int rsize = 1<<DECODE_REG_TYPE(r0);
            
            if((i64)sp-(i64)stack.data - rsize > (i64)stack.max){
                // NOTE: stack shrinks upwards and thus we call it underflow
                log::out <<log::RED<<__FUNCTION__<< ": stack "<<log::GOLD<<"underflow"<<log::RED<<" (sp: "<<
                ((i64)sp-(i64)stack.data)<<", max: "<<
                (stack.max)<<")\n";
                return;
            } else if((i64)sp-(i64)stack.data < (i64)0){
                // NOTE: This is not underflow even though sp is less than base stack pointer because
                //  stack grows down and not up.
                log::out<<log::RED <<__FUNCTION__<< ": stack pointer was messed with (sp: "<<
                ((i64)sp-(i64)stack.data)<<", max: "<<
                (stack.max)<<")\n";
                return;
            }

            void* to = getReg(r0);
            void* from = (void*)sp;
            yeah(r0,from,to);
            sp+=rsize;
            
            _ILOG(log::out <<"\n";)
            SP_CHANGE(+rsize)
            break;
        }
        case BC_CALL: {
            u8 r0 = DECODE_REG0(inst);
            
            void* ptr = getReg(r0);
            i64 addr = *((i64*)ptr);
            
            _ILOG(log::out << "\n";)
            if(addr>(int)length){
                log::out << log::GOLD << "Call to instruction beyond all bytecode\n";
            }
            // _ILOG(log::out <<" pc: "<< pc <<" fp: "<<fp<<"\n";)
            
            sp-=8;
            *((u64*) sp) = fp;
            // _ILOG(log::out<<"save fp at "<<sp<<"\n";)
            // savedFp = (u64*)sp;
            // SP_CHANGE(-8)
            sp-=8;
            *((u64*) sp) = pc; // pc points to the next instruction, +1 not needed 
            // SP_CHANGE(-8)
            
            fp = sp;
            pc = addr;
            
            // _ILOG(log::out <<" pc: "<< pc <<" fp: "<<fp<<"\n";)
    
            if(addr<0){
                int argoffset = 16;
                if(addr==BC_EXT_ALLOC){
                    u64 size = *(u64*)(fp+argoffset);
                    void* ptr = engone::Allocate(size);
                    _ILOG(log::out << "alloc "<<size<<" -> "<<ptr<<"\n";)
                    *(u64*)(fp-8) = (u64)ptr;
                } else if (addr==BC_EXT_REALLOC){
                    void* ptr = *(void**)(fp+argoffset+16);
                    u64 oldsize = *(u64*)(fp +argoffset + 8);
                    u64 size = *(u64*)(fp+argoffset);
                    ptr = engone::Reallocate(ptr,oldsize,size);
                    _ILOG(log::out << "realloc "<<size<<" ptr: "<<ptr<<"\n";)
                    *(u64*)(fp-8) = (u64)ptr;
                } else if (addr==BC_EXT_FREE){
                    void* ptr = *(void**)(fp+argoffset + 8);
                    u64 size = *(u64*)(fp+argoffset);
                    engone::Free(ptr,size);
                    _ILOG(log::out << "free "<<size<<" old ptr: "<<ptr<<"\n";)
                } else if (addr==BC_EXT_PRINTI){
                    i64 num = *(i64*)(fp+argoffset);
                    log::out << log::LIME<<"print: "<<num<<"\n";
                    // _ILOG(log::out << "free "<<size<<" old ptr: "<<ptr<<"\n";)
                }else if (addr==BC_EXT_PRINTC){
                    char chr = *(char*)(fp+argoffset + 7); // +7 comes from the alignment after char
                    #ifdef ILOG
                    log::out << log::LIME << "print: "<<chr<<"\n";
                    #else
                    log::out << chr;
                    #endif
                }else {
                    _ILOG(log::out << log::RED << addr<<" is not a special function\n";)
                }
                // bc ret here
                goto TINY_GOTO;
            }
            
            break;
        }
        case BC_RET: {
            _ILOG(log::out <<"\n";)
            
            TINY_GOTO:
            // _ILOG(log::out <<"pc: "<< pc<<" fp: "<< fp<<"\n";)
            sp=fp+16; // will now point to arguments
            pc = *((u64*)(fp));
            // sp+=8;
            // _ILOG(log::out<<"loaded fp from "<<(fp+8)<<"\n";)
            fp = *((u64*)(fp+8));
            // sp+=8;
            // SP_CHANGE(sp-fp)
            // _ILOG(log::out <<"pc: "<< pc<<" fp: "<< fp<<"\n";)
            
            break;
        }
        case BC_JMP: {
            // load immediate
            // u32 data = *(u32*)bytecode->get(pc);
            u32 data = *(u32*)(codePtr + pc);
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

            // u32 data = *(u32*)bytecode->get(pc);
            u32 data = *(u32*)(codePtr + pc);
            pc++;

            void* ptr = getReg(r0);
            int rsize = 1<<DECODE_REG_TYPE(r0);
            u64 testValue = 0;
            if(rsize==1) {
                testValue = *(u8*)ptr;
            } else if(rsize==2) {
                testValue = *(u16*)ptr;
            } else if(rsize==4) {
                testValue = *(u32*)ptr;
            } else if(rsize==8){
                testValue = *(u64*)ptr;
            }

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
        // case BC_CASTF32: {
        //     u8 r0 = DECODE_REG0(inst);
        //     u8 r1 = DECODE_REG1(inst);
            
        //     void* x = getReg(r0);
        //     void* out = getReg(r1);
            
        //     *(float*)out = *(i64*)x;
        //     _ILOG(log::out <<*(float*)out<<"\n";)
        //     break;
        // }
        case BC_CAST: {
            u8 type = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);
            u8 r2 = DECODE_REG2(inst);

            void* xp = getReg(r1);
            void* out = getReg(r2);
            if(type==CAST_FLOAT_SINT){
                int size = 1<<DECODE_REG_TYPE(r1);
                if(size!=4){
                    log::out << log::RED << "float needs 4 byte register\n";
                }
                int size2 = 1<<DECODE_REG_TYPE(r2);
                // TODO: log out
                if(size2==1) {
                    *(i8*)out = *(float*)xp;
                } else if(size2==2) {
                    *(i16*)out = *(float*)xp;
                } else if(size2==4) {
                    *(i32*)out = *(float*)xp;
                } else if(size2==8){
                    *(i64*)out = *(float*)xp;
                }
            } else if(type==CAST_SINT_FLOAT){
                int fsize = 1<<DECODE_REG_TYPE(r1);
                int tsize = 1<<DECODE_REG_TYPE(r2);
                if(tsize!=4){
                    log::out << log::RED << "float needs 4 byte register\n";
                }
                // TODO: log out
                if(fsize==1) {
                    *(float*)out = *(i8*)xp;
                } else if(fsize==2) {
                    *(float*)out = *(i16*)xp;
                } else if(fsize==4) {
                    *(float*)out = *(i32*)xp;
                } else if(fsize==8){
                    *(float*)out = *(i64*)xp;
                }
            } else if(type==CAST_UINT_SINT){
                int fsize = 1<<DECODE_REG_TYPE(r1);
                int tsize = 1<<DECODE_REG_TYPE(r2);
                u64 temp = 0;
                if(fsize==1) {
                    temp = *(u8*)xp;
                } else if(fsize==2) {
                    temp = *(u16*)xp;
                } else if(fsize==4) {
                    temp = *(u32*)xp;
                } else if(fsize==8){
                    temp = *(u64*)xp;
                }
                // TODO: log out
                if(tsize==1) {
                    *(i8*)out = temp;
                } else if(tsize==2) {
                    *(i16*)out = temp;
                } else if(tsize==4) {
                    *(i32*)out = temp;
                } else if(tsize==8){
                    *(i64*)out = temp;
                }
            } else if(type==CAST_SINT_UINT){
                int fsize = 1<<DECODE_REG_TYPE(r1);
                int tsize = 1<<DECODE_REG_TYPE(r2);
                i64 temp = 0;
                if(fsize==1) {
                    temp = *(i8*)xp;
                } else if(fsize==2) {
                    temp = *(i16*)xp;
                } else if(fsize==4) {
                    temp = *(i32*)xp;
                } else if(fsize==8){
                    temp = *(i64*)xp;
                }
                // TODO: log out
                if(tsize==1) {
                    *(u8*)out = temp;
                } else if(tsize==2) {
                    *(u16*)out = temp;
                } else if(tsize==4) {
                    *(u32*)out = temp;
                } else if(tsize==8){
                    *(u64*)out = temp;
                }
            } else if(type==CAST_SINT_SINT){
                int fsize = 1<<DECODE_REG_TYPE(r1);
                int tsize = 1<<DECODE_REG_TYPE(r2);
                i64 temp = 0;
                if(fsize==1) {
                    temp = *(i8*)xp;
                } else if(fsize==2) {
                    temp = *(i16*)xp;
                } else if(fsize==4) {
                    temp = *(i32*)xp;
                } else if(fsize==8){
                    temp = *(i64*)xp;
                }
                // TODO: log out
                if(tsize==1) {
                    *(i8*)out = temp;
                } else if(tsize==2) {
                    *(i16*)out = temp;
                } else if(tsize==4) {
                    *(i32*)out = temp;
                } else if(tsize==8){
                    *(i64*)out = temp;
                }
            }
            
            _ILOG(log::out <<"\n";)
            break;
        }
        case BC_MEMCPY: {
            u8 r0 = DECODE_REG0(inst); // dst
            u8 r1 = DECODE_REG1(inst); // src
            u8 r2 = DECODE_REG2(inst); // size

            void* dstp = getReg(r0);
            void* srcp = getReg(r1);
            void* sizep = getReg(r2);

            int s0 = 1<<DECODE_REG_TYPE(r2);
            int s1 = 1<<DECODE_REG_TYPE(r2);
            int s2 = 1<<DECODE_REG_TYPE(r2);
            u64 size = 0;
            if(s2==1) {
                size = *(u8*)sizep;
            } else if(s2==2) {
                size = *(u16*)sizep;
            } else if(s2==4) {
                size = *(u32*)sizep;
            } else if(s2==8){
                size = *(u64*)sizep;
            }
            if(s0!=8||s1!=8){
                log::out << log::RED << "src and dst must use 8 byte registers";
                log::out << "\n";
                break;
            }
            void* dst = *(void**)dstp;
            void* src = *(void**)srcp;
            memcpy(dst,src,size);
            log::out << "copied "<<size<<" bytes\n";
            break;
        }
        } // for switch
    }
    auto time = StopMeasure(tp);
    log::out << "\n";
    log::out << "Executed in "<<FormatTime(time)<<"\n";
    printRegisters();
       
}