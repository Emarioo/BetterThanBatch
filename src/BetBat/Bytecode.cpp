#include "BetBat/Bytecode.h"

// included to get CallConventions
#include "BetBat/x64_Converter.h"

const char* InstToString(int type){
    #define CASE(x,y) case x: return y;
    switch(type){
        CASE(BC_MOV_RR,"mov")
        CASE(BC_MOV_RM,"mov")
        CASE(BC_MOV_RM_DISP32,"mov")
        CASE(BC_MOV_MR,"mov")
        CASE(BC_MOV_MR_DISP32,"mov")
        
        CASE(BC_MODI,"mod")
        CASE(BC_FMOD,"mod")
        CASE(BC_ADDI,"add")
        CASE(BC_FADD,"add")
        CASE(BC_SUBI,"sub")
        CASE(BC_FSUB,"sub")
        CASE(BC_MULI,"mul")
        CASE(BC_FMUL,"mul")
        CASE(BC_DIVI,"div")
        CASE(BC_FDIV,"div")
        CASE(BC_INCR,"inc")
        
        CASE(BC_JMP,"jmp")
        CASE(BC_CALL,"call")
        CASE(BC_RET,"ret")
        CASE(BC_JE,"je")
        CASE(BC_JNE,"jne")
        
        CASE(BC_PUSH,"push")
        CASE(BC_POP,"pop")
        CASE(BC_LI,"li")
        CASE(BC_DATAPTR,"dataptr")
        CASE(BC_CODEPTR,"codeptr")

        CASE(BC_EQ,"eq")
        CASE(BC_NEQ,"neq")
        
        CASE(BC_LT,"lt") 
        CASE(BC_LTE,"lte")
        CASE(BC_GT,"gt") 
        CASE(BC_GTE,"gte")
        CASE(BC_ANDI,"and")
        CASE(BC_ORI,"or") 
        CASE(BC_NOT,"not")
        
        CASE(BC_FEQ,"eq")
        CASE(BC_FNEQ,"neq")
        CASE(BC_FLT,"lt") 
        CASE(BC_FLTE,"lte")
        CASE(BC_FGT,"gt") 
        CASE(BC_FGTE,"gte")
        
        CASE(BC_BAND,"band")
        CASE(BC_BOR,"bor")
        CASE(BC_BXOR,"bxor")
        CASE(BC_BNOT,"bnot")
        CASE(BC_BLSHIFT,"shl")
        CASE(BC_BRSHIFT,"shr")
        
        CASE(BC_CAST,"cast")
        CASE(BC_ASM,"asm")

        CASE(BC_MEMZERO,"memzero")
        CASE(BC_MEMCPY,"memcpy")
        CASE(BC_RDTSC,"rdtsc")
        CASE(BC_STRLEN,"strlen")
        // CASE(BC_RDTSCP)
        CASE(BC_CMP_SWAP,"cmp_swap")
        CASE(BC_ATOMIC_ADD,"atomic_add")
        
        CASE(BC_SQRT,"sqrt")
        CASE(BC_ROUND,"round")
        
        CASE(BC_TEST_VALUE,"test_value")

        // CASE(BC_SIN)
        // CASE(BC_COS)
        // CASE(BC_TAN)
        default: {
            Assert(false);
        }
    }
    #undef CASE
    return "BC_?";
}
int RegBySize(u8 regName, int size){
    if(size==1) return ENCODE_REG_BITS(regName, BC_REG_8);
    else if(size==2) return ENCODE_REG_BITS(regName, BC_REG_16);
    else if(size==4) return ENCODE_REG_BITS(regName, BC_REG_32);
    else if(size==8) return ENCODE_REG_BITS(regName, BC_REG_64);
    else {
        // TypeChecker may fail and give an invalid type with size 0.
        // 
        // Assert(("Bad size, only 1,2,4,8 are allowed", false))
        return 0;
    }
}
bool Bytecode::removeLast(){
    if(instructionSegment.used>0)
        return false;
    instructionSegment.used--;
    return true;
}
engone::Logger& operator<<(engone::Logger& logger, Instruction& instruction){
    instruction.print();
    return logger;
}
uint32 Bytecode::getMemoryUsage(){
    uint32 sum = instructionSegment.max*instructionSegment.getTypeSize()
        +debugSegment.max*debugSegment.getTypeSize()
        // debugtext?        
        ;
    return sum;
}

bool Bytecode::addExportedSymbol(const std::string& name,  u32 location) {
    for(int i=0;i<exportedSymbols.size();i++) {
        if(exportedSymbols[i].name == name) {
            return false;
        }
    }
    ExportedSymbol tmp{};
    tmp.name = name;
    tmp.location = location;
    exportedSymbols.add(tmp);
    return true;
}
void Bytecode::addExternalRelocation(const std::string& name, u32 location){
    ExternalRelocation tmp{};
    tmp.name = name;
    tmp.location = location;
    externalRelocations.add(tmp);
}
bool Bytecode::add_notabug(Instruction instruction){
    if(instructionSegment.max == instructionSegment.used){
        if(!instructionSegment.resize(instructionSegment.max*2 + 25*sizeof(Instruction)))
            return false;   
    }
    *((Instruction*)instructionSegment.data + instructionSegment.used++) = instruction;
    _GLOG(printInstruction(length()-1, false);)
    return true;
}

void Bytecode::printInstruction(u32 index, bool printImmediates){
    using namespace engone;
    Assert(index < (u32)length());

    u8 opcode = get(index).opcode;
    log::out << index<< ": "<<get(index)<<" ";
    if(printImmediates){
        u8 immCount = immediatesOfInstruction(index);
        if(immCount>0) {
            if(opcode == BC_LI && get(index).op1 == 2) {
                i64 imm = (i64)getIm(index+1) | ((i64)getIm(index+2) << 32);
                log::out << imm;
            } else { 
                log::out << getIm(index + 1);
            }
        }
    }
    log::out<<"\n";
}
u32 Bytecode::immediatesOfInstruction(u32 index) {
    u8 opcode = get(index).opcode;
    if(opcode == BC_LI || opcode==BC_JMP || opcode==BC_JE || opcode==BC_JNE || opcode==BC_CALL || opcode==BC_DATAPTR||
        opcode == BC_MOV_MR_DISP32 || opcode == BC_MOV_RM_DISP32 || opcode == BC_CODEPTR || opcode==BC_TEST_VALUE){
        
        if(opcode == BC_LI && get(index).op1 == 2){
            return 2;
        } else {
            return 1;
        }
    }
    return 0;
}
bool Bytecode::addIm_notabug(i32 data){
    if(instructionSegment.max == instructionSegment.used){
        if(!instructionSegment.resize(instructionSegment.max*2 + 25*sizeof(Instruction)))
            return false;
    }
    _GLOG(engone::log::out <<length()<< ": "<<data<<"\n";)
    // NOTE: This function works because sizeof Instruction == sizeof u32
    *((i32*)instructionSegment.data + instructionSegment.used++) = data;
    return true;
}
void Bytecode::cleanup(){
    instructionSegment.resize(0);
    dataSegment.resize(0);
    debugSegment.resize(0);
    debugLocations.cleanup();
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
const char* RegToStr(u8 reg){
    #define CASE(K,V) case BC_REG_##K: return ""#V;
    #define CASER(K,V) case BC_REG_R##K##X: return "r"#V"x";\
    case BC_REG_E##K##X: return "e"#V "x";\
    case BC_REG_##K##X: return ""#V "x";\
    case BC_REG_##K##L: return ""#V "l";
    // case BC_REG_##K##H: return ""#V "h";
    switch(reg){
        CASER(A,a)
        CASER(B,b)
        CASER(C,c)
        CASER(D,d)

        CASE(SP,sp)
        CASE(FP,fp)
        // CASE(DP,dp)
        CASE(RDI,rdi)
        CASE(RSI,rsi)

        CASE(R8,r8)
        CASE(R9,r9)

        CASE(XMM0f,xmm0f)
        CASE(XMM1f,xmm1f)
        CASE(XMM2f,xmm2f)
        CASE(XMM3f,xmm3f)
        
        CASE(XMM0d,xmm0d)
        CASE(XMM1d,xmm1d)
        CASE(XMM2d,xmm2d)
        CASE(XMM3d,xmm3d)

        case 0: return "";
    }
    #undef CASE
    #undef CASER
    return "$?";
}
void Instruction::print(){
    using namespace engone;

    if(opcode==BC_INCR){
        i16 offset = (i16)((i16)op1 | ((i16)op2<<8));
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY<<" "<<RegToStr(op0) << " "<< offset  << log::NO_COLOR;
    // else if(opcode==BC_MEMZERO)
    //     log::out << log::PURPLE<<InstToStringX(opcode) << log::GRAY<<" "<<RegToStr(op0) << " "<< (u8)op1 << log::NO_COLOR;
    } else if(opcode==BC_CAST) {
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY<<" ";
        switch(op0){
            case CAST_FLOAT_SINT: log::out << "f->s"; break;
            case CAST_SINT_FLOAT: log::out << "s->f"; break;
            case CAST_UINT_FLOAT: log::out << "u->s"; break;
            case CAST_SINT_SINT: log::out << "s->s"; break;
            case CAST_UINT_SINT: log::out << "u->s"; break;
            case CAST_SINT_UINT: log::out << "s->u"; break;
            case CAST_FLOAT_FLOAT: log::out << "f->f"; break;
        }
        log::out<<" "<<RegToStr(op1) << " "<< RegToStr(op2) << log::NO_COLOR;
    } else if(opcode==BC_LT || opcode==BC_LTE ||opcode==BC_GT ||opcode==BC_GTE) {
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY<<" ";
        switch(op0){
            case CMP_UINT_UINT: log::out << "u-u"; break;
            case CMP_UINT_SINT: log::out << "u-s"; break;
            case CMP_SINT_UINT: log::out << "s-u"; break;
            case CMP_SINT_SINT: log::out << "s-s"; break;
        }
        log::out<<" "<<RegToStr(op1) << " "<< RegToStr(op2) << log::NO_COLOR;
    }
    else if(opcode==BC_MOV_MR) {
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY<<" ["<<RegToStr(op0) << "] "<<RegToStr(op1)<< " "<<op2<<log::NO_COLOR;
    } else if(opcode==BC_MOV_MR_DISP32) {
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY<<" ["<<RegToStr(op0) << "+disp] "<<RegToStr(op1)<< " "<<op2<<log::NO_COLOR;
    } else if(opcode==BC_MOV_RM) {
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY<<" "<<RegToStr(op0) << " ["<<RegToStr(op1)<< "] "<<op2<<log::NO_COLOR;
    } else if(opcode==BC_MOV_RM_DISP32){
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY<<" "<<RegToStr(op0) << " ["<<RegToStr(op1)<< "+disp] "<<op2<<log::NO_COLOR;
    } else if(opcode==BC_CALL) {
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY <<" "<< (LinkConventions)op0 << " "<< (CallConventions)op1 <<log::NO_COLOR;
    } else if(opcode==BC_ASM) {
        int index = ASM_DECODE_INDEX(op0,op1,op2);
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY <<" "<< index <<log::NO_COLOR;
    } else if(opcode==BC_LI) {
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY<<" "<<RegToStr(op0);
        if(op1 == 2)
            log::out << " qword";
         log::out<<log::NO_COLOR;
    } else if(opcode==BC_TEST_VALUE) {
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY<<" "<<op0<<" " << RegToStr(op1)<< " "<<RegToStr(op2);
        log::out<<log::NO_COLOR;
    } else
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY<<" "<<RegToStr(op0) << " "<<RegToStr(op1)<< " "<<RegToStr(op2)<<log::NO_COLOR;
    
    // auto color = log::out.getColor();
    // if(type==BC_NUM||type==BC_STR||type==BC_SUBSTR||type==BC_COPY)
    //     log::out << log::AQUA;
    // if(type==BC_DEL)
    //     log::out << log::PURPLE;
    // if(type==BC_LOADNC||type==BC_LOADSC||type==BC_LOADV||type==BC_STOREV){
    //     log::out << InstToString(type) << " $"<<RegToString(reg0) << " "<<((uint)reg1|((uint)reg2<<8));
    // } else if((type&BC_MASK) == BC_R1){
    //     log::out << InstToString(type);
    //     if(reg1==0&&reg2==0){
    //         log::out << " $"<<RegToString(reg0);
    //     }else{
    //         log::out << " "<<((uint)reg0|((uint)reg1<<8)|((uint)reg2<<8));
    //     }
    // }else if((type&BC_MASK) == BC_R2){
    //     log::out << InstToString(type)<<" $" <<RegToString(reg0);
    //     // if(reg2!=0||type==BC_LOADV){
    //     //     log::out << " "<<((uint)reg1|((uint)reg2<<8));
    //     // }else{
    //         log::out << " $"<<RegToString(reg1);
    //     // }
    // // if((type&BC_MASK) == BC_R3)
    // } else
    //     log::out<<InstToString(type)<<" $"<<RegToString(reg0)<<" $"<<RegToString(reg1)<<" $"<<RegToString(reg2);
        
    // log::out << color;
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
    int misalign = (alignment - (dataSegment.used%alignment)) % alignment;
    if(dataSegment.max < dataSegment.used + misalign){
        int oldMax = dataSegment.max;
        bool yes = dataSegment.resize(dataSegment.max*2 + 100);
        Assert(yes);
        memset(dataSegment.data + oldMax, '_', dataSegment.max - oldMax);
    }
    int index = dataSegment.used;
    memset((char*)dataSegment.data + index,'_',misalign);
    dataSegment.used+=misalign;
}
int Bytecode::appendData(const void* data, int size){
    Assert(size > 0);
    if(dataSegment.max < dataSegment.used + size){
        int oldMax = dataSegment.max;
        dataSegment.resize(dataSegment.max*2 + 2*size);
        memset(dataSegment.data + oldMax, '_', dataSegment.max - oldMax);
    }
    int index = dataSegment.used;
    if(data) {
        memcpy((char*)dataSegment.data + index,data,size);
    } else {
        memset((char*)dataSegment.data + index,0,size);
    }
    dataSegment.used+=size;
    return index;
}
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
const Bytecode::Location* Bytecode::getLocation(u32 instructionIndex, u32* locationIndex){
    using namespace engone;
    if(instructionIndex>=debugSegment.used)
        return nullptr;
    u32 index = *((u32*)debugSegment.data + instructionIndex);
    if(locationIndex)
        *locationIndex = index;
    return debugLocations.getPtr(index); // may return nullptr;
}
Bytecode::Location* Bytecode::setLocationInfo(const char* str, u32 instructionIndex){
    return setLocationInfo((Token)str);
}
Bytecode::Location* Bytecode::setLocationInfo(u32 locationIndex, u32 instructionIndex){
    using namespace engone;
    if(instructionIndex == (u32)-1){
        instructionIndex = instructionSegment.used;
    }
    if(instructionIndex>=debugSegment.max) {
        int newSize = instructionSegment.max*1.5+20;
        newSize += (instructionIndex-debugSegment.max)*2;
        int oldmax = debugSegment.max;
        bool yes = debugSegment.resize(newSize);
        Assert(yes);
        memset((char*)debugSegment.data + oldmax*debugSegment.getTypeSize(),0xFF,(debugSegment.max-oldmax)*debugSegment.getTypeSize());
    }
    u32& index = *((u32*)debugSegment.data + instructionIndex);
    if(debugSegment.used <= instructionIndex) {
        debugSegment.used = instructionIndex + 1;
    }
    
    index = locationIndex;
    auto ptr = debugLocations.getPtr(index); // may return nullptr;
    // if(ptr){
    //     *locationIndex = index;
    //     if(tokenRange.tokenStream())
    //         ptr->file = TrimDir(tokenRange.tokenStream()->streamName);
    //     else
    //         ptr->desc = tokenRange.firstToken; // TODO: Don't just use the first token.
    //     ptr->line = tokenRange.firstToken.line;
    //     ptr->column = tokenRange.firstToken.column;
    // }
    return ptr;
}
Bytecode::Location* Bytecode::setLocationInfo(const TokenRange& tokenRange, u32 instructionIndex, u32* locationIndex){
    using namespace engone;
    if(instructionIndex == (u32)-1){
        instructionIndex = instructionSegment.used;
    }
    if(instructionIndex>=debugSegment.max) {
        int newSize = instructionSegment.max*1.5+20;
        newSize += (instructionIndex-debugSegment.max)*2;
        int oldmax = debugSegment.max;
        bool yes = debugSegment.resize(newSize);
        Assert(yes);
        memset((char*)debugSegment.data + oldmax*debugSegment.getTypeSize(),0xFF,(debugSegment.max-oldmax)*debugSegment.getTypeSize());
    }
    u32& index = *((u32*)debugSegment.data + instructionIndex);
    if(debugSegment.used <= instructionIndex) {
        debugSegment.used = instructionIndex + 1;
    }
    if((int)index == -1){
        bool yes = debugLocations.add({});
        Assert(yes);
        index = debugLocations.size()-1;
    }
    auto ptr = debugLocations.getPtr(index); // may return nullptr;
    if(ptr){
        if(locationIndex)
            *locationIndex = index;
        if(tokenRange.tokenStream()) {
            // ptr->file = TrimDir(tokenRange.tokenStream()->streamName);
            // ptr->line = tokenRange.firstToken.line;
            // ptr->column = tokenRange.firstToken.column;
        } else {
            // ptr->preDesc = tokenRange.firstToken; // TODO: Don't just use the first token.
        }
    }
    return ptr;
}
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