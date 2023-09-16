#include "BetBat/Interpreter.h"

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

void Interpreter::cleanup(){
    stack.resize(0);
    cmdArgsBuffer.resize(0);
    engone::Free(cmdArgs.ptr, sizeof(Language::Slice<char>)*cmdArgs.len);
    cmdArgs.ptr = nullptr;
    cmdArgs.len = 0;
}
void Interpreter::reset(){
    rax=0;
    rbx=0;
    rcx=0;
    rdx=0;
    xmm0d=0;
    xmm1d=0;
    xmm2d=0;
    xmm3d=0;
    sp=0;
    fp=0;
    pc=0;
    rsi=0;
    rdi=0;
    memset(stack.data, 0, stack.max);
}
void Interpreter::setCmdArgs(const DynamicArray<std::string>& inCmdArgs){
    using namespace engone;
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
void Interpreter::printRegisters(){
    using namespace engone;
    void* pa = (void*)&rax;
    void* pb = (void*)&rbx;
    void* pc = (void*)&rcx;
    void* pd = (void*)&rdx;
    log::out << "RAX: "<<(i64)rax<<" "<<*(float*)pa<<"\n";
    log::out << "RBX: "<<(i64)rbx<<" "<<*(float*)pb<<"\n";
    log::out << "RCX: "<<(i64)rcx<<" "<<*(float*)pc<<"\n";
    log::out << "RDX: "<<(i64)rdx<<" "<<*(float*)pd<<"\n";
    log::out << "xmm?\n";
    log::out << "SP: "<<sp<<"\n";
    log::out << "FP: "<<fp<<"\n";
    log::out << "PC: "<<pc<<"\n";
}
volatile void* Interpreter::getReg(u8 id){
    // #define CASE(K,V) case BC_REG_##K: return V;
    #define CASER(K,V) case BC_REG_R##K##X: return &r##V##x;\
    case BC_REG_E##K##X: return &r##V##x;\
    case BC_REG_##K##X: return &r##V##x;\
    case BC_REG_##K##L: return &r##V##x;
    // case BC_REG_##K##H: return (void*)((u8*)&r##V##x+1);
    switch(id){
        CASER(A,a)
        CASER(B,b)
        CASER(C,c)
        CASER(D,d)

        case BC_REG_SP: return &sp;
        case BC_REG_FP: return &fp;
        // case BC_REG_DP: return &dp;
        case BC_REG_RDI: return &rdi;
        case BC_REG_RSI: return &rsi;

        case BC_REG_XMM0f: return &xmm0d;
        case BC_REG_XMM1f: return &xmm1d;
        case BC_REG_XMM2f: return &xmm2d;
        case BC_REG_XMM3f: return &xmm3d;
    }
    #undef CASER
    engone::log::out <<"(RegID: "<<id<<")\n";
    Assert("tried to access bad register");
    return 0;
}
void Interpreter::moveMemory(u8 reg, volatile void* from, volatile void* to){
    using namespace engone;
    int size = DECODE_REG_BITS(reg);
    if(from != to) {
        *((u64*) to) = 0;
    }
    if(size==BC_REG_8){
        *((u8* ) to) = *((u8* ) from);
        _ILOG(if(!silent) {log::out << (*(i8*)to);})
    }else if(size==BC_REG_16) {
        *((u16*) to) = *((u16*) from);
        _ILOG(if(!silent) {log::out << (*(i16*)to);})
    }else if(size==BC_REG_32)   { 
         *((u32*) to) = *((u32*) from);
         _ILOG(if(!silent) {log::out << (*(i32*)to);})
    }else if(size==BC_REG_64){
         *((u64*) to) = *((u64*) from);
        _ILOG(if(!silent) {log::out << (*(i64*)to);})
    }else 
        log::out <<log::RED <<"bad set to from\n";
        
    // _ILOG(log::out << (*(i64*)to)<<"\n";)
}
void PrintPointer(void* ptr){
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

void Interpreter::execute(Bytecode* bytecode){
    executePart(bytecode, 0, bytecode->length());
}
void Interpreter::executePart(Bytecode* bytecode, u32 startInstruction, u32 endInstruction){
    using namespace engone; 
    Assert(bytecode);
    auto nativeRegistry = NativeRegistry::GetGlobal();
    // if(!bytecode->nativeRegistry) {
    //     bytecode->nativeRegistry = NativeRegistry::GetGlobal();
    //     // bytecode->nativeRegistry = NativeRegistry::Create();
    //     // bytecode->nativeRegistry->initNativeContent();
    // }

    if(bytecode->externalRelocations.size()>0){
        log::out << log::RED << "Interpreter does not support symbol relocations! Don't use function with @import annotation.\n";
        return;
    }
    _VLOG(
        if(!silent){
        if(startInstruction==0 && endInstruction == bytecode->length())
            log::out <<log::BLUE<< "##   Interpreter  ##\n";
        else
            log::out <<log::BLUE<< "##   Interpreter ("<<startInstruction<<" - "<<endInstruction<<")  ##\n";
        }
    )
    stack.resize(100*1024);
    memset(stack.data, 0, stack.max);
    
    pc = startInstruction;
    sp = (u64)stack.data+stack.max;
    fp = (u64)stack.data+stack.max;
    
    if(sp % 16 != 8) {
        sp -= 8 - sp % 16;
    }

    u64 expectedStackPointer = sp;

    auto tp = StartMeasure();

    #ifdef ILOG
    #undef _ILOG
    #define _ILOG(X) if(!silent){X};
    #endif
    
    // _ILOG(log::out << "sp = "<<sp<<"\n";)

    i64 userAllocatedBytes=0;
    
    u64 executedInstructions = 0;
    u64 length = bytecode->instructionSegment.used;
    if(length==0)
        log::out << log::YELLOW << "Interpreter ran bytecode with zero instructions. Bug?\n";
    Instruction* codePtr = (Instruction*)bytecode->instructionSegment.data;
    Bytecode::Location* prevLocation = nullptr;
    u32 stopAt = endInstruction;
    WHILE_TRUE_N(99999999) {
        // if(pc>=(u64)bytecode->length())
        if(pc>=(u64)length)
            break;
        if(pc == stopAt)
            break;

        // Instruction* inst = bytecode->get(pc);
        Instruction* inst = codePtr + pc;
        
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

        executedInstructions++;
        pc++;
        u8 opcode = DECODE_OPCODE(inst);
        switch (opcode) {
        case BC_MODI:
        case BC_FMOD:
        case BC_ADDI:
        case BC_SUBI:
        case BC_MULI:
        case BC_DIVI:
        case BC_FADD:
        case BC_FSUB:
        case BC_FMUL:
        case BC_FDIV:
        case BC_EQ:
        case BC_NEQ:
        case BC_LT:
        case BC_LTE:
        case BC_GT:
        case BC_GTE:
        case BC_FEQ:
        case BC_FNEQ:
        case BC_FLT:
        case BC_FLTE:
        case BC_FGT:
        case BC_FGTE:
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

            // if (DECODE_REG_SIZE_TYPE(r0) != DECODE_REG_SIZE_TYPE(r1) || DECODE_REG_SIZE_TYPE(r1) != DECODE_REG_SIZE_TYPE(r2)){
            //     log::out << log::RED<<"register bit mismatch\n";
            //     continue;   
            // }
            volatile void* xp = getReg(r0);
            volatile void* yp = getReg(r1);
            volatile void* op = getReg(r2);
            
            i64 x = 0;
            i64 y = 0;
            float fx = 0;
            float fy = 0;
            int xs = DECODE_REG_SIZE(r0);
            int ys = DECODE_REG_SIZE(r1);
            int os = DECODE_REG_SIZE(r2);
            if(xs==1){
                x = *(i8*)xp;
            }
            if(xs==2){
                x = *(i16*)xp;
            }
            if(xs==4){
                x = *(i32*)xp;
                fx = *(float*)xp;
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
                fy = *(float*)yp;
            }
            if(ys==8){
                y = *(i64*)yp;
            }
            i64 out = 0;
            float fout = 0;
            
            u64 oldSp = sp;
            bool isFloatCmp =  opcode== BC_FEQ || opcode==BC_FNEQ || opcode==BC_FLT
                || opcode== BC_FLTE || opcode==BC_FGT|| opcode==BC_FGTE;
            bool isFloat = opcode==BC_FADD||opcode==BC_FSUB||opcode==BC_FMUL||opcode==BC_FDIV
                ||opcode==BC_FMOD || isFloatCmp;
            if(isFloat){
                if((os!=4&&os!=1)||xs!=4||ys!=4){
                    log::out << log::RED << "float operation requires 4-byte register";
                    return;
                }
            }
            #define GEN_OP(OP) out = x OP y; _ILOG(log::out << out << " = "<<x <<" "<< #OP << " "<<y;) break;
            #define GEN_OPF(OP) fout = fx OP fy; _ILOG(log::out << fout << " = "<< fx <<" "<< #OP << " "<< fy;) break;
            #define GEN_OPIF(OP) out = fx OP fy; _ILOG(log::out << out << " = "<< fx <<" "<< #OP << " "<< fy;) break;
            switch(opcode){
                case BC_MODI: GEN_OP(%)
                case BC_FMOD: fout = fmod(fx,fy); _ILOG(log::out << fout << " = "<< fx <<" % "<< fy;) break;
                case BC_ADDI: GEN_OP(+)   
                case BC_SUBI: GEN_OP(-)   
                case BC_MULI: GEN_OP(*)   
                case BC_DIVI: GEN_OP(/)
                case BC_FADD: GEN_OPF(+)   
                case BC_FSUB: GEN_OPF(-)   
                case BC_FMUL: GEN_OPF(*)   
                case BC_FDIV: GEN_OPF(/)

                case BC_EQ: GEN_OP(==)
                case BC_NEQ: GEN_OP(!=)
                case BC_LT: GEN_OP(<)
                case BC_LTE: GEN_OP(<=)
                case BC_GT: GEN_OP(>)
                case BC_GTE: GEN_OP(>=)
                case BC_ANDI: GEN_OP(&&)
                case BC_ORI: GEN_OP(||)
                
                case BC_FEQ:    GEN_OPIF(==)
                case BC_FNEQ:   GEN_OPIF(!=)
                case BC_FLT:     GEN_OPIF(<)
                case BC_FLTE:   GEN_OPIF(<=)
                case BC_FGT:     GEN_OPIF(>)
                case BC_FGTE:   GEN_OPIF(>=)
                
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
            
            *(u64*)op = 0;
            if(os==1){
                *(i8*)op = out;
            }
            if(os==2){
                *(i16*)op = out;
            }
            if(os==4){
                if(isFloat) {
                    *(float*)op = fout;
                } else {
                    *(i32*)op = out;
                }
            }
            if(os==8){
                *(i64*)op = out;
            }
            
            // #define GEN_BIT(B, OP) if(DECODE_REG_SIZE_TYPE(r0)==BC_REG_##B ) *((u##B* ) out) = *((u##B* ) x) OP *((u##B* ) y);
            // GEN_BIT(8,+)
            // else GEN_BIT(16,+)
            // else GEN_BIT(32,+)
            // else GEN_BIT(64,+)
            // else
            //      log::out << log::RED<< __FILE__<<"unknown reg type\n";
            // #undef GEN_BIT
            break;
        }
        break; case BC_INCR:{
            u8 r0 = DECODE_REG0(inst);
            i16 offset = (i16)((i16)DECODE_REG1(inst) | ((i16)DECODE_REG2(inst)<<8));
            // log::out << "offset: "<<offset<<"\n";
            i64* regValue = (i64*)getReg(r0);
            *regValue += offset;
            
            _ILOG(log::out << "\n";)
            break;
        }
        break; case BC_NOT:
        case BC_BNOT:{
            u8 r0 = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);

            if (DECODE_REG_SIZE(r0) != DECODE_REG_SIZE(r1)){
                log::out << log::RED<<"register bit mismatch\n";
                continue;
            }
            volatile void* xp = getReg(r0);
            volatile void* op = getReg(r1);
            
            i64 x = 0;
            int xs = 1<<DECODE_REG_SIZE(r0);
            int os = 1<<DECODE_REG_SIZE(r1);
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
        break; case BC_MOV_MR:
        case BC_MOV_MR_DISP32:{
            u8 r0 = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);
            i8 operandSize = (i8)DECODE_REG2(inst);
            Assert(operandSize==1 || operandSize==2||operandSize==4||operandSize==8);

            if(DECODE_REG_BITS(r0) != BC_REG_64){
                log::out << log::RED<<"r0 (pointer) must use 64 bit registers\n";
                continue;   
            }
            u64* fromptr = (u64*)getReg(r0);
            volatile void* from = (void*)(*fromptr); // NOTE: Program can crash here
            volatile void* to = getReg(r1);

            i32 disp = 0;
            if(opcode==BC_MOV_MR_DISP32){
                disp = *(i32*)(codePtr + pc);
                pc++;
                from = (void*)((u64)from + disp);
            }
            
            if(((uint64)from % operandSize) != 0){
                log::out << log::RED<<"r0 (pointer: "<<(uint64)from<<") not aligned by "<<operandSize<<" bytes\n";
                continue;
            }
            u64 value = 0;
            if(operandSize==1){
                value = *((u8* ) to) = *((u8* ) from);
                // _ILOG(log::out << (*(i8*)to);)
            }else if(operandSize==2) {
                value = *((u16*) to) = *((u16*) from);
                // _ILOG(log::out << (*(i16*)to);)
            }else if(operandSize==4)   { 
                value = *((u32*) to) = *((u32*) from);
                // _ILOG(log::out << (*(i32*)to);)
            }else if(operandSize==8){
                value = *((u64*) to) = *((u64*) from);
                // _ILOG(log::out << (*(i64*)to);)
            }else 
                log::out <<log::RED <<"bad set to from\n";

            
            _ILOG(log::out << value << " = [";PrintPointer(from);
            if(disp!=0)
                log::out << " + "<<disp;
            log::out <<"]";)

            _ILOG(log::out <<"\n";)

            break;
        }
        break; case BC_MOV_RM:
        case BC_MOV_RM_DISP32:{
            u8 r0 = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);
            i8 operandSize = (i8)DECODE_REG2(inst);
            Assert(operandSize==1 || operandSize==2||operandSize==4||operandSize==8);

            if(DECODE_REG_BITS(r1) != BC_REG_64){
                log::out << log::RED<<"r1 (pointer) must use 64 bit registers\n";
                continue;
            }
            u64* toptr = (u64*)getReg(r1);
            volatile void* from = getReg(r0);
            void* to = (void*)(*toptr); // NOTE: Program can crash here

            i32 disp = 0;
            if(opcode==BC_MOV_RM_DISP32){
                disp = *(i32*)(codePtr + pc);
                pc++;
                to = (void*)((u64)to + disp);
            }

            if(((uint64)to % operandSize) != 0){
                log::out << log::RED<<"r1 (pointer: "<<(uint64)to<<") not aligned by "<<operandSize<<" bytes\n";
                continue;
            }
            u64 value = 0;
            if(operandSize==1){
                value = *((u8* ) to) = *((u8* ) from);
                // _ILOG(log::out << (*(i8*)to);)
            }else if(operandSize==2) {
                value = *((u16*) to) = *((u16*) from);
                // _ILOG(log::out << (*(i16*)to);)
            }else if(operandSize==4)   { 
                value = *((u32*) to) = *((u32*) from);
                // _ILOG(log::out << (*(i32*)to);)
            }else if(operandSize==8){
                value = *((u64*) to) = *((u64*) from);
                // _ILOG(log::out << (*(i64*)to);)
            }else 
                log::out <<log::RED <<"bad set to from\n";
            
            _ILOG(log::out << "[";PrintPointer(to);
            if(disp!=0)
                log::out << " + "<<disp;
            log::out <<"]" << " = " << value;)
            
            _ILOG(log::out <<"\n";)
            
            break;
        }
        break; case BC_MOV_RR:{
            u8 r0 = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);
            if (DECODE_REG_BITS(r0) != DECODE_REG_BITS(r1)){
                log::out << __FILE__<<"register bit mismatch\n";
                continue;
            }
            volatile void* from = getReg(r0);
            volatile void* to = getReg(r1);

            // SET_TO_FROM(r1)
            moveMemory(r1,from,to);

            _ILOG(log::out << " = "<<(*(u64* )to)<<"\n";)
            break;
        }
        break; case BC_LI:{
            u8 r0 = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst); // extra big immediate
            
            volatile void* out = getReg(r0);
            
            i64 data = *(i32*)(codePtr + pc);
            pc++;
            if(r1 == 2){
                i64 data2 = *(i32*)(codePtr + pc);
                pc++;
                data |= data2<<32;
            }
            
            _ILOG(log::out << data<<"\n";)
            
            u8 t = DECODE_REG_BITS(r0);
            *((i64*)out) = 0;
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
        break; case BC_DATAPTR: {
            u8 r0 = DECODE_REG0(inst);
            Assert(DECODE_REG_SIZE(r0) == 8);

            u64* out = (u64*)getReg(r0);
            u32 dataOffset = *(u32*)(codePtr + pc);
            pc++;
            
            *out = (u64)bytecode->dataSegment.data + dataOffset;
            
            _ILOG(log::out << *out<<" = "<< dataOffset<<" + "<< (u64)bytecode->dataSegment.data <<"\n";)
            break;
        }
        break; case BC_CODEPTR: {
            u8 r0 = DECODE_REG0(inst);
            Assert(DECODE_REG_SIZE(r0) == 8);

            u64* out = (u64*)getReg(r0);
            u32 dataOffset = *(u32*)(codePtr + pc);
            pc++;
            
            *out = pc + dataOffset;
            
            _ILOG(log::out << *out<<" = "<< dataOffset<<" + "<< pc <<"\n";)
            break;
        }
        break; case BC_PUSH:{
            u8 r0 = DECODE_REG0(inst);
            int rsize = DECODE_REG_SIZE(r0);
            rsize = 8;
            
            if((i64)sp-(i64)stack.data - rsize > (i64)stack.max){
                log::out <<log::RED<<__FUNCTION__<< ": stack pointer was messed with (sp: "<<
                ((i64)sp-(i64)stack.data)<<", max: "<<
                (stack.max)<<")\n";
                return;
            } else if((i64)sp-(i64)stack.data < (i64)0){
                // NOTE: This is not underflow even though sp is less than base stack pointer because
                //  stack grows down and not up.
                log::out<<log::RED <<__FUNCTION__<< ": stack "<<log::YELLOW<<"overflow"<<log::RED<<" (sp: "<<
                ((i64)sp-(i64)stack.data)<<", max: "<<
                (stack.max)<<")\n";
                return;
            }

            sp-=rsize;
            void* to = (void*)sp;
            volatile void* from = getReg(r0);

            if(((uint64)to % rsize) != 0){
                log::out << log::RED<<"sp (pointer: "<<(uint64)to<<") not aligned by "<<rsize<<" bytes\n";
                continue;
            }
            moveMemory(r0,from,to);
            
            _ILOG(log::out <<"\n";)
            SP_CHANGE(-rsize)
            break;
        }
        break; case BC_POP: {
            u8 r0 = DECODE_REG0(inst);
            int rsize = DECODE_REG_SIZE(r0);
            rsize = 8;
            
            if((i64)sp-(i64)stack.data - rsize > (i64)stack.max){
                // NOTE: stack shrinks upwards and thus we call it underflow
                log::out <<log::RED<<__FUNCTION__<< ": stack "<<log::YELLOW<<"underflow"<<log::RED<<" (sp: "<<
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

            volatile void* to = getReg(r0);
            void* from = (void*)sp;
            if(((uint64)from % rsize) != 0){
                log::out << log::RED<<"sp (pointer: "<<(uint64)from<<") not aligned by "<<rsize<<" bytes\n";
                continue;
            }
            moveMemory(r0,from,to);
            sp+=rsize;
            
            _ILOG(log::out <<"\n";)
            SP_CHANGE(+rsize)
            break;
        }
        break; case BC_CALL: {
            i32 addr = *(i32*)(codePtr + pc);
            pc++;
            
            LinkConventions linkConvention = (LinkConventions)DECODE_REG0(inst);
            CallConventions callConvention = (CallConventions)DECODE_REG1(inst);
            Assert(linkConvention == LinkConventions::NONE || linkConvention==LinkConventions::NATIVE);
            // void* ptr = getReg(r0);
            // i64 addr = *((i64*)ptr);
            
            _ILOG(log::out << addr<<"\n";)
            if(addr>(int)length){
                log::out << log::YELLOW << "Call to instruction beyond all bytecode\n";
            }
            _ILOG(log::out <<" pc: "<< pc <<" fp: "<<fp<<" sp: "<<sp<<"\n";)
            // _ILOG(log::out<<"save fp at "<<sp<<"\n";)
            // savedFp = (u64*)sp;
            // SP_CHANGE(-8)
            sp-=8;
            *((u64*) sp) = pc; // pc points to the next instruction, +1 not needed 
            // SP_CHANGE(-8)
            pc = addr;
            
            _ILOG(log::out <<" pc: "<< pc <<" fp: "<<fp<<" sp: "<<sp<<"\n";)
    
            // if((i64)addr<0){
            if(linkConvention == LinkConventions::NATIVE){
                int argoffset = GenInfo::FRAME_SIZE; // native functions doesn't save
                // the frame pointer. FRAME_SIZE is based on that you do so -8 to get just pc being saved
                
                // auto* nativeFunction = nativeRegistry->findFunction(addr);
                // if(nativeFunction){

                // } else {
                //     _ILOG(log::out << log::RED << addr<<" is not a native function\n";)
                // }
                u64 fp = sp - 8;

                switch (addr) {
                case NATIVE_Allocate:{
                    u64 size = *(u64*)(fp+argoffset);
                    void* ptr = engone::Allocate(size);
                    if(ptr)
                        userAllocatedBytes += size;
                    _ILOG(log::out << "alloc "<<size<<" -> "<<ptr<<"\n";)
                    *(u64*)(fp-8) = (u64)ptr;
                    break;
                }
                break; case NATIVE_Reallocate:{
                    void* ptr = *(void**)(fp+argoffset);
                    u64 oldsize = *(u64*)(fp +argoffset + 8);
                    u64 size = *(u64*)(fp+argoffset+16);
                    ptr = engone::Reallocate(ptr,oldsize,size);
                    if(ptr)
                        userAllocatedBytes += size - oldsize;
                    _ILOG(log::out << "realloc "<<size<<" ptr: "<<ptr<<"\n";)
                    *(u64*)(fp-8) = (u64)ptr;
                    break;
                }
                break; case NATIVE_Free:{
                    void* ptr = *(void**)(fp+argoffset);
                    u64 size = *(u64*)(fp+argoffset+8);
                    engone::Free(ptr,size);
                    userAllocatedBytes -= size;
                    _ILOG(log::out << "free "<<size<<" old ptr: "<<ptr<<"\n";)
                    break;
                }
                break; case NATIVE_printi:{
                    i64 num = *(i64*)(fp+argoffset);
                    #ifdef ILOG
                    log::out << log::LIME<<"print: "<<num<<"\n";
                    #else                    
                    log::out << num;
                    #endif
                    break;
                }
                break; case NATIVE_printd:{
                    float num = *(float*)(fp+argoffset);
                    #ifdef ILOG
                    log::out << log::LIME<<"print: "<<num<<"\n";
                    #else                    
                    log::out << num;
                    #endif
                    break;
                }
                break; case NATIVE_printc: {
                    char chr = *(char*)(fp+argoffset); // +7 comes from the alignment after char
                    #ifdef ILOG
                    log::out << log::LIME << "print: "<<chr<<"\n";
                    #else
                    log::out << chr;
                    #endif
                    break;
                }
                break; case NATIVE_prints: {
                    char* ptr = *(char**)(fp+argoffset);
                    u64 len = *(u64*)(fp+argoffset+8);
                    
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
                    break;
                }
                break; case NATIVE_FileOpen:{
                    // The order may seem strange but it's actually correct.
                    // It's just complicated.
                    // slice
                    Language::Slice<char>* path = *(Language::Slice<char>**)(fp+argoffset + 0);
                    bool readOnly = *(bool*)(fp+argoffset+8);
                    u64* outFileSize = *(u64**)(fp+argoffset+16);

                    // INCOMPLETE
                    u64 file = FileOpen(path, readOnly, outFileSize);

                    *(u64*)(fp-8) = (u64)file;
                    break;
                }
                break; case NATIVE_FileRead:{
                    // The order may seem strange but it's actually correct.
                    // It is just complicated.
                    // slice
                    u64 file = *(u64*)(fp+argoffset);
                    void* buffer = *(void**)(fp+argoffset+8);
                    u64 readBytes = *(u64*)(fp+argoffset+16);

                    u64 bytes = FileRead(file, buffer, readBytes);
                    *(u64*)(fp-8) = (u64)bytes;
                    break;
                }
                break; case NATIVE_FileWrite:{
                    // The order may seem strange but it's actually correct.
                    // It is just complicated.
                    // slice
                    u64 file = *(u64*)(fp+argoffset);
                    void* buffer = *(void**)(fp+argoffset+8);
                    u64 writeBytes = *(u64*)(fp+argoffset+16);

                    u64 bytes = FileWrite(file, buffer, writeBytes);
                    *(u64*)(fp-8) = (u64)bytes;
                    break;
                }
                break; case NATIVE_FileClose:{
                    APIFile file = *(APIFile*)(fp+argoffset);
                    FileClose(file);
                    // #ifdef VLOG
                    // log::out << log::GRAY<<"FileClose: "<<file<<"\n";
                    // #endif
                    break;
                }
                break; case NATIVE_DirectoryIteratorCreate:{
                    // slice
                    // char* strptr = *(char**)(fp+argoffset + 0);
                    // u64 len = *(u64*)(fp+argoffset+8);
                    Language::Slice<char>* rootPath= *(Language::Slice<char>**)(fp + argoffset + 0);

                    // INCOMPLETE
                    // std::string rootPath = std::string(strptr,len);
                    auto iterator = DirectoryIteratorCreate(rootPath);

                    #ifdef VLOG
                    // log::out << log::GRAY<<"Create dir iterator: "<<rootPath<<"\n";
                    #endif

                    // auto iterator = (Language::DirectoryIterator*)engone::Allocate(sizeof(Language::DirectoryIterator));
                    // new(iterator)Language::DirectoryIterator();
                    // iterator->_handle = (u64)iterHandle;
                    // iterator->rootPath.ptr = (char*)engone::Allocate(len);
                    // iterator->rootPath.len = len;
                    // memcpy(iterator->rootPath.ptr, strptr, len);

                    *(Language::DirectoryIterator**)(fp-8) = iterator;
                    break;
                }
                break; case NATIVE_DirectoryIteratorDestroy:{
                    Language::DirectoryIterator* iterator = *(Language::DirectoryIterator**)(fp+argoffset + 0);
                    // INCOMPLETE
                    DirectoryIteratorDestroy(iterator);

                    #ifdef VLOG
                    // log::out << log::GRAY<<"Destroy dir iterator: "<<iterator->rootPath<<"\n";
                    #endif
                    // if(iterator->result.name.ptr) {
                    //     engone::Free(iterator->result.name.ptr, iterator->result.name.len);
                    //     iterator->result.name.ptr = nullptr;
                    //     iterator->result.name.len = 0;
                    // }
                    // engone::Free(iterator->rootPath.ptr,iterator->rootPath.len);
                    // iterator->~DirectoryIterator();
                    // engone::Free(iterator,sizeof(Language::DirectoryIterator));
                    break;
                }
                break; case NATIVE_DirectoryIteratorNext:{
                    Language::DirectoryIterator* iterator = *(Language::DirectoryIterator**)(fp+argoffset + 0);
                    
                    auto data = DirectoryIteratorNext(iterator);

                    // DirectoryIteratorData result{};
                    // bool success = DirectoryIteratorNext((DirectoryIterator)iterator->_handle, &result);
                    // INCOMPLETE
                    // if(iterator->result.name.ptr) {
                    //     engone::Free(iterator->result.name.ptr, iterator->result.name.len);
                    //     iterator->result.name.ptr = nullptr;
                    //     iterator->result.name.len = 0;
                    // }
                    // // if(success){
                    // //     iterator->result.lastWriteSeconds = result.lastWriteSeconds;
                    // //     iterator->result.fileSize = result.fileSize;
                    // //     iterator->result.isDirectory = result.isDirectory;
                    // //     iterator->result.name.ptr = (char*)Allocate((u64)result.name.length());
                    // //     memcpy(iterator->result.name.ptr, result.name.data(), result.name.length());
                    // //     iterator->result.name.len = (u64)result.name.length();
                    // // }
                    // #ifdef VLOG
                    // // if(success)
                    // // log::out << log::GRAY<<"Next dir iterator: "<<iterator->rootPath<<", file: "<<result.name<<"\n";
                    // // else
                    // // log::out << log::GRAY<<"Next dir iterator: "<<iterator->rootPath<<", finished\n";
                    // #endif
                    // if(success)
                    //     *(Language::DirectoryIteratorData**)(fp-8) = &iterator->result;
                    // else
                    // *(Language::DirectoryIteratorData**)(fp-8) = nullptr;
                    *(Language::DirectoryIteratorData**)(fp-8) = data;
                    break;
                }
                break; case NATIVE_DirectoryIteratorSkip:{
                    Language::DirectoryIterator* iterator = *(Language::DirectoryIterator**)(fp+argoffset + 0);
                    
                    DirectoryIteratorSkip(iterator);

                    // #ifdef VLOG
                    // // log::out << log::GRAY<<"Skip dir iterator: "<<iterator->result.name<<"\n";
                    // #endif
                    break;
                }
                break; case NATIVE_CurrentWorkingDirectory:{
                    
                    std::string temp = GetWorkingDirectory();
                    Assert(temp.length()<CWD_LIMIT);
                    memcpy(cwdBuffer,temp.data(),temp.length());
                    usedCwd = temp.length();

                    *(char**)(fp-16) = cwdBuffer;
                    *(u64*)(fp-8) = temp.length();
                    #ifdef VLOG
                    log::out << log::GRAY<<"CWD: "<<temp<<"\n";
                    #endif
                    break;
                }
                break; case NATIVE_StartMeasure:{
                    auto timePoint = StartMeasure();
                    
                    *(u64*)(fp-8) = (u64)timePoint;

                    #ifdef VLOG
                    log::out << log::GRAY<<"Start mesaure: "<<(u64)timePoint<<"\n";
                    #endif
                    break;
                }
                break; case NATIVE_StopMeasure:{
                    u64 timePoint = *(u64*)(fp+argoffset + 0);

                    double time = StopMeasure(timePoint);

                    *(float*)(fp - 8) = (float)time;
                    #ifdef VLOG
                    log::out << log::GRAY<<"Stop measure: "<<(u64)timePoint << ", "<<time<<"\n";
                    #endif
                    break;
                }
                break; case NATIVE_CmdLineArgs:{
                    *(Language::Slice<char>**)(fp - 16) = cmdArgs.ptr;
                    *(u64*)(fp - 8) = cmdArgs.len;
                    #ifdef VLOG
                    log::out << log::GRAY<<"Cmd line args: len: "<<cmdArgs.len<<"\n";
                    #endif
                    break;
                }
                break; case NATIVE_NativeSleep:{
                    float sleepTime = *(float*)(fp + argoffset + 4);
                    Sleep(sleepTime);
                    break;
                }
                break; default:{
                    // auto* nativeFunction = bytecode->nativeRegistry->findFunction(addr);
                    auto* nativeFunction = nativeRegistry->findFunction(addr);
                    if(nativeFunction){
                        // _ILOG(
                        log::out << log::RED << "Native '"<<nativeFunction->name<<"' ("<<addr<<") has no impl. in interpreter\n";
                        // )
                    } else {
                        // _ILOG(
                        log::out << log::RED << addr<<" is not a native function\n";
                        // )
                    }
                    // break from loop after a few times?
                    // break won't work in switch statement.
                }
                } // switch
                // jump to BC_RET case
                goto TINY_GOTO;
            }
            
            break;
        }
        break; case BC_RET: {
            _ILOG(log::out <<"\n";)
            
            TINY_GOTO:
            // _ILOG(log::out <<"pc: "<< pc<<" fp: "<< fp<<" sp: "<< sp<<"\n";)
            
            // sp=fp+16; // will now point to arguments
            pc = *((u64*)(sp));
            sp+=8;

            // sp+=8;
            // _ILOG(log::out<<"loaded fp from "<<(fp+8)<<"\n";)
            // sp+=8;
            // SP_CHANGE(sp-fp)
            // _ILOG(log::out <<"pc: "<< pc<<" fp: "<< fp<<" sp: "<< sp<<"\n";)
            
            break;
        }
        break; case BC_JMP: {
            // load immediate
            // u32 data = *(u32*)bytecode->get(pc);
            u32 data = *(u32*)(codePtr + pc);
            pc++;
            // TODO: relative immediate instead?
            //   can't see any benefit right now.
            _ILOG(log::out << data<<"\n";)
            pc = data;
            break;
        }
        break; case BC_JE:
        case BC_JNE: {
            u8 r0 = DECODE_REG0(inst);

            // u32 data = *(u32*)bytecode->get(pc);
            u32 data = *(u32*)(codePtr + pc);
            pc++;

            volatile void* ptr = getReg(r0);
            int rsize = DECODE_REG_SIZE(r0);
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

            _ILOG(log::out << "testval: "<<testValue<<", jumpto: "<<data<<"\n";)
            
            bool yes=false;
            switch (opcode){
                case BC_JE: yes = testValue!=0; break;
                break; case BC_JNE: yes = testValue==0; break;
            }
            if(yes){
                pc = data;
            }
            break;
        }
        break; case BC_CAST: {
            u8 type = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);
            u8 r2 = DECODE_REG2(inst);

            volatile void* xp = getReg(r1);
            volatile void* out = getReg(r2);
            if(type==CAST_FLOAT_SINT){
                int size = DECODE_REG_SIZE(r1);
                if(size!=4){
                    log::out << log::RED << "float needs 4 byte register\n";
                }
                int size2 = DECODE_REG_SIZE(r2);
                // TODO: log out+
                if(size2==1) {
                    *(i8*)out = *(float*)xp;
                } else if(size2==2) {
                    *(i16*)out = *(float*)xp;
                } else if(size2==4) {
                    *(i32*)out = *(float*)xp;
                } else if(size2==8){
                    *(i64*)out = *(float*)xp;
                }
            // } else if(type==CAST_FLOAT_UINT){
            //     int size = DECODE_REG_SIZE(r1);
            //     if(size!=4){
            //         log::out << log::RED << "float needs 4 byte register\n";
            //     }
            //     int size2 = DECODE_REG_SIZE(r2);
            //     // TODO: log out
            //     if(size2==1) {
            //         *(u8*)out = *(float*)xp;
            //     } else if(size2==2) {
            //         *(u16*)out = *(float*)xp;
            //     } else if(size2==4) {
            //         *(u32*)out = *(float*)xp;
            //     } else if(size2==8){
            //         *(u64*)out = *(float*)xp;
            //     }
            } else if(type==CAST_SINT_FLOAT){
                int fsize = DECODE_REG_SIZE(r1);
                int tsize = DECODE_REG_SIZE(r2);
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
            } else if(type==CAST_UINT_FLOAT){
                int fsize = DECODE_REG_SIZE(r1);
                int tsize = DECODE_REG_SIZE(r2);
                if(tsize!=4){
                    log::out << log::RED << "float needs 4 byte register\n";
                }
                // TODO: log out
                if(fsize==1) {
                    *(float*)out = *(u8*)xp;
                } else if(fsize==2) {
                    *(float*)out = *(u16*)xp;
                } else if(fsize==4) {
                    *(float*)out = *(u32*)xp;
                } else if(fsize==8){
                    *(float*)out = *(u64*)xp;
                }
            } else if(type==CAST_UINT_SINT){
                int fsize = DECODE_REG_SIZE(r1);
                int tsize = DECODE_REG_SIZE(r2);
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
                int fsize = DECODE_REG_SIZE(r1);
                int tsize = DECODE_REG_SIZE(r2);
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
                int fsize = DECODE_REG_SIZE(r1);
                int tsize = DECODE_REG_SIZE(r2);
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
            } else {
                Assert(("Cast type not implemented",false));
            }
            
            _ILOG(log::out <<"\n";)
            break;
        }
        break; case BC_MEMZERO:{
            u8 r0 = DECODE_REG0(inst);
            u8 r1 = DECODE_REG1(inst);
            // u16 size = (u16)DECODE_REG1(inst) | ((u16)DECODE_REG2(inst)<<8);
            
            int r0size = DECODE_REG_SIZE(r0);
            Assert(r0size == 8);

            volatile void* toptr = getReg(r0);
            void* to = (void*)*(u64*)toptr;

            int r1size = DECODE_REG_SIZE(r1);
            volatile void* anysize = getReg(r1);
            u64 size = 0;
            if(r1size==1) {
                size = *(u8*)anysize;
            } else if(r1size==2) {
                size = *(u16*)anysize;
            } else if(r1size==4) {
                size = *(u32*)anysize;
            } else if(r1size==8){
                size = *(u64*)anysize;
            }

            memset(to,0,size);

            _ILOG(log::out << " [0-"<<size<<"]\n";)
            break;
        }
        break; case BC_MEMCPY: {
            u8 r0 = DECODE_REG0(inst); // dst
            u8 r1 = DECODE_REG1(inst); // src
            u8 r2 = DECODE_REG2(inst); // size

            volatile void* dstp = getReg(r0);
            volatile void* srcp = getReg(r1);
            volatile void* sizep = getReg(r2);

            int s0 = DECODE_REG_SIZE(r2);
            int s1 = DECODE_REG_SIZE(r2);
            int s2 = DECODE_REG_SIZE(r2);
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
            _ILOG(log::out << "\n";)
            // log::out << "copied "<<size<<" bytes\n";
            break;
        }
        break; case BC_STRLEN: {
            u8 r0 = DECODE_REG0(inst); // str
            u8 r1 = DECODE_REG1(inst); // not used
            u8 r2 = DECODE_REG2(inst); // out

            volatile void* strp = getReg(r0);
            char* str = *(char**)strp;
            u64* outp = (u64*)getReg(r2);

            if(DECODE_REG_SIZE(r0)!=8){
                log::out << log::RED << "src must use 8 byte registers";
                log::out << "\n";
                break;
            }

            *outp = strlen(str);

            _ILOG(log::out << *outp << "\n";)
            break;
        }
        break; case BC_RDTSC: {
            u8 r0 = DECODE_REG0(inst); // count
            Assert(r0 == BC_REG_RAX);
                
            u64 count = __rdtsc();

            volatile void* countp = getReg(r0);

            *((u64*)countp) = count;

            _ILOG(log::out << "\n";)
            break;
        }
        #ifdef OS_WINDOWS
        break; case BC_CMP_SWAP: {
            u8 r0 = DECODE_REG0(inst); // dst
            u8 r1 = DECODE_REG1(inst); // old
            u8 r2 = DECODE_REG2(inst); // new
            Assert(r0 == BC_REG_RBX && r1 == BC_REG_EAX && r2 == BC_REG_EDX);

            // volatile long* dst = *(volatile long**)getReg(r0);
            // i32 old = *(i32*)getReg(r1);
            // i32 newv = *(i32*)getReg(r2);
            long tmp = rax;
            rax = (long)tmp == _InterlockedCompareExchange((volatile long*)rbx, (long)rdx, tmp);
        }
        #endif
        // break; case BC_ATOMIC_ADD: {

        // }
        break; case BC_SQRT: {
            u8 r0 = DECODE_REG0(inst);
            float* floatp = (float*)getReg(r0);
            *floatp = sqrtf(*floatp);
            _ILOG(log::out << *floatp<<"\n";)
        }
        break; case BC_ROUND: {
            u8 r0 = DECODE_REG0(inst);
            float* floatp = (float*)getReg(r0);
            *floatp = roundf(*floatp);
            _ILOG(log::out << *floatp<<"\n";)
        }
        
#define BC_MEMZERO 100
#define BC_MEMCPY 101
#define BC_STRLEN 102
#define BC_RDTSC 103
// #define BC_RDTSCP 109
// compare and swap, atomic
#define BC_CMP_SWAP 104
#define BC_ATOMIC_ADD 105

#define BC_SQRT 120
#define BC_ROUND 121
        // break; case BC_RDTSCP: {
        //     u8 r0 = DECODE_REG0(inst); // dst
        //     u8 r1 = DECODE_REG1(inst); // src
        //     u8 r2 = DECODE_REG2(inst); // size
        //     Assert(r0 == BC_REG_RAX && r1 == BC_REG_ECX && r2 == BC_REG_RDX);
                
        //     u32 aux=0;
        //     u64 count = __rdtscp(&aux);

        //     void* countp = getReg(r0);
        //     void* auxp = getReg(r1);

        //     *((u64*)countp) = count;
        //     *((u32*)auxp) = aux;

        //     _ILOG(log::out << "\n";)
        //     break;
        // }
        break; case BC_TEST_VALUE: {
            u8 r0 = DECODE_REG0(inst); // bytes
            u8 r1 = DECODE_REG1(inst); // test value
            u8 r2 = DECODE_REG2(inst); // computed value
            // TODO: Strings don't work yet

            u32 data = *(u32*)(codePtr + pc);
            pc++;

            u8* testValue = (u8*)getReg(r1);
            u8* computedValue = (u8*)getReg(r2);

            bool same = !strncmp((char*)testValue, (char*)computedValue, r0);

            char tmp[]{
                same ? '_' : 'x',
                '-',
                (char)((data>>8)&0xFF),
                (char)(data&0xFF)
            };
            // fwrite(&tmp, 1, 1, stderr);

            auto out = engone::GetStandardErr();
            engone::FileWrite(out, &tmp, 4);

            _ILOG(log::out << "\n";)
            break;
        }
        break; default: {
            log::out << log::RED << "Implement "<< log::PURPLE<< InstToString(opcode)<< "\n";
            return;
        }
        } // for switch
    }
    if(!silent){
        log::out << "rax: "<<rax<<"\n";
    }
    if(userAllocatedBytes!=0){
        log::out << log::RED << "User program leaks "<<userAllocatedBytes<<" bytes\n";
    }
    if(!expectValuesOnStack){
        if(sp != expectedStackPointer){
            log::out << log::YELLOW<<"sp was "<<(i64)(sp - ((u64)stack.data+stack.max))<<", should be 0\n";
        }
        if(fp != (u64)stack.data+stack.max){
            log::out << log::YELLOW<<"fp was "<<(i64)(fp - ((u64)stack.data+stack.max))<<", should be 0\n";
        }
    }
    auto time = StopMeasure(tp);
    if(!silent){
        log::out << "\n";
        log::out << log::LIME << "Executed in "<<FormatTime(time)<< " "<<FormatUnit(executedInstructions/time)<< " inst/s\n";
        #ifdef ILOG_REGS
        printRegisters();
        #endif
    }
}