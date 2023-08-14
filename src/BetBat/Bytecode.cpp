#include "BetBat/Bytecode.h"

// included to get CallConventions
#include "BetBat/x64_Converter.h"

const char* InstToString(int type){
    #define CASE(x) case x: return #x;
    switch(type){
        CASE(BC_MOV_RR)
        CASE(BC_MOV_RM)
        CASE(BC_MOV_RM_DISP32)
        CASE(BC_MOV_MR)
        CASE(BC_MOV_MR_DISP32)
        
        CASE(BC_MODI)
        CASE(BC_FMOD)
        CASE(BC_ADDI)
        CASE(BC_FADD)
        CASE(BC_SUBI)
        CASE(BC_FSUB)
        CASE(BC_MULI)
        CASE(BC_FMUL)
        CASE(BC_DIVI)
        CASE(BC_FDIV)
        CASE(BC_INCR)
        
        CASE(BC_JMP)
        CASE(BC_CALL)
        CASE(BC_RET)
        CASE(BC_JE)
        CASE(BC_JNE)
        
        CASE(BC_PUSH)
        CASE(BC_POP)
        CASE(BC_LI)
        CASE(BC_DATAPTR)
        CASE(BC_CODEPTR)

        CASE(BC_EQ)
        CASE(BC_NEQ)
        CASE(BC_LT) 
        CASE(BC_LTE)
        CASE(BC_GT) 
        CASE(BC_GTE)
        CASE(BC_ANDI)
        CASE(BC_ORI) 
        CASE(BC_NOT)
        
        CASE(BC_BAND)
        CASE(BC_BOR)
        CASE(BC_BXOR)
        CASE(BC_BNOT)
        CASE(BC_BLSHIFT)
        CASE(BC_BRSHIFT)
        
        CASE(BC_CAST)

        CASE(BC_MEMZERO)
        CASE(BC_MEMCPY)
        CASE(BC_RDTSC)
        // CASE(BC_RDTSCP)
        CASE(BC_CMP_SWAP)
        CASE(BC_ATOMIC_ADD)

        // CASE(BC_SIN)
        // CASE(BC_COS)
        // CASE(BC_TAN)
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
    if(codeSegment.used>0)
        return false;
    codeSegment.used--;
    return true;
}
engone::Logger& operator<<(engone::Logger& logger, Instruction& instruction){
    instruction.print();
    return logger;
}
uint32 Bytecode::getMemoryUsage(){
    uint32 sum = codeSegment.max*codeSegment.getTypeSize()
        +debugSegment.max*debugSegment.getTypeSize()
        // debugtext?        
        ;
    return sum;
}
void Bytecode::addExternalRelocation(const std::string& name, u32 location){
    ExternalRelocation tmp{};
    tmp.name = name;
    tmp.location = location;
    externalRelocations.add(tmp);
}
bool Bytecode::add(Instruction instruction){
    if(codeSegment.max == codeSegment.used){
        if(!codeSegment.resize(codeSegment.max*2 + 25*sizeof(Instruction)))
            return false;   
    }
    _GLOG(engone::log::out <<length()<< ": "<<instruction<<"\n";)
    *((Instruction*)codeSegment.data + codeSegment.used++) = instruction;
    return true;
}
bool Bytecode::addIm(i32 data){
    if(codeSegment.max == codeSegment.used){
        if(!codeSegment.resize(codeSegment.max*2 + 25*sizeof(Instruction)))
            return false;
    }
    _GLOG(engone::log::out <<length()<< ": "<<data<<"\n";)
    // NOTE: This function works because sizeof Instruction == sizeof u32
    *((i32*)codeSegment.data + codeSegment.used++) = data;
    return true;
}
void Bytecode::cleanup(){
    codeSegment.resize(0);
    dataSegment.resize(0);
    debugSegment.resize(0);
    debugLocations.cleanup();
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
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY<<" "<<RegToStr(op0) << " "<< offset  << log::SILVER;
    // else if(opcode==BC_MEMZERO)
    //     log::out << log::PURPLE<<InstToStringX(opcode) << log::GRAY<<" "<<RegToStr(op0) << " "<< (u8)op1 << log::SILVER;
    } else if(opcode==BC_CAST)
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY<<" "<<op0<<" "<<RegToStr(op1) << " "<< RegToStr(op2) << log::SILVER;
    else if(opcode==BC_MOV_MR||opcode==BC_MOV_RM||opcode==BC_MOV_MR_DISP32||opcode==BC_MOV_RM_DISP32)
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY<<" "<<RegToStr(op0) << " "<<RegToStr(op1)<< " "<<op2<<log::SILVER;
    else if(opcode==BC_CALL)
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY <<" "<< (LinkConventions)op0 << " "<< (CallConventions)op1 <<log::SILVER;
    else
        log::out << log::PURPLE<<InstToString(opcode) << log::GRAY<<" "<<RegToStr(op0) << " "<<RegToStr(op1)<< " "<<RegToStr(op2)<<log::SILVER;
    
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
    // Bytecode* ptr = (Bytecode*)engone::Allocate(sizeof(Bytecode));
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
        bool yes = dataSegment.resize(dataSegment.max*2 + 100);
        Assert(yes);
    }
    int index = dataSegment.used;
    memset((char*)dataSegment.data + index,'_',misalign);
    dataSegment.used+=misalign;
}
int Bytecode::appendData(const void* data, int size){
    Assert(size > 0);
    if(dataSegment.max < dataSegment.used + size){
        dataSegment.resize(dataSegment.max*2 + 2*size);
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
//         instructionIndex = codeSegment.used;
//     }
//     if(instructionIndex>=debugSegment.max){
//         int newSize = codeSegment.max*1.5+20;
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
//         // strncpy((char*)debugText.data(),str,length);
//         *((u32*)debugSegment.data + instructionIndex) = index + 1;
//     }else{
//         Assert((int)debugText.size()<=oldIndex);
//         // debugText[oldIndex-1] += "\n"; // should line feed be forced?
//         debugText[oldIndex-1] += std::string(str,length);
//     }
// }
Bytecode::Location* Bytecode::getLocation(u32 instructionIndex, u32* locationIndex){
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
        instructionIndex = codeSegment.used;
    }
    if(instructionIndex>=debugSegment.max) {
        int newSize = codeSegment.max*1.5+20;
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
        instructionIndex = codeSegment.used;
    }
    if(instructionIndex>=debugSegment.max) {
        int newSize = codeSegment.max*1.5+20;
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
    if(index == -1){
        bool yes = debugLocations.add({});
        Assert(yes);
        index = debugLocations.size()-1;
    }
    auto ptr = debugLocations.getPtr(index); // may return nullptr;
    if(ptr){
        if(locationIndex)
            *locationIndex = index;
        if(tokenRange.tokenStream()) {
            ptr->file = TrimDir(tokenRange.tokenStream()->streamName);
            ptr->line = tokenRange.firstToken.line;
            ptr->column = tokenRange.firstToken.column;
        } else
            ptr->preDesc = tokenRange.firstToken; // TODO: Don't just use the first token.
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