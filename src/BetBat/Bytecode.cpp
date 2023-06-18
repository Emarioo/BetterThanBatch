#include "BetBat/Bytecode.h"

const char* InstToStringX(int type){
    #define CASE(x) case x: return #x;
    switch(type){
        CASE(BC_MOV_RR)
        CASE(BC_MOV_RM)
        CASE(BC_MOV_MR)
        
        CASE(BC_MODI)
        CASE(BC_MODF)
        CASE(BC_ADDI)
        CASE(BC_ADDF)
        CASE(BC_SUBI)
        CASE(BC_SUBF)
        CASE(BC_MULI)
        CASE(BC_MULF)
        CASE(BC_DIVI)
        CASE(BC_DIVF)
        CASE(BC_INCR)
        
        CASE(BC_JMP)
        CASE(BC_CALL)
        CASE(BC_RET)
        CASE(BC_JE)
        CASE(BC_JNE)
        
        CASE(BC_PUSH)
        CASE(BC_POP)
        CASE(BC_LI)

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

        CASE(BC_ZERO_MEM)
        CASE(BC_MEMCPY)
    }
    #undef CASE
    return "BC_?";
}
int RegBySize(int regName, int size){
    if(size==1) return ENCODE_REG_TYPE(BC_REG_8) | regName;
    else if(size==2) return ENCODE_REG_TYPE(BC_REG_16) | regName;
    else if(size==4) return ENCODE_REG_TYPE(BC_REG_32) | regName;
    else if(size==8) return ENCODE_REG_TYPE(BC_REG_64) | regName;
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
    uint32 sum = codeSegment.max*codeSegment.m_typeSize
        +debugSegment.max*debugSegment.m_typeSize
        // debugtext?        
        ;
    return sum;
}
bool Bytecode::add(Instruction instruction){
    if(codeSegment.max == codeSegment.used){
        if(!codeSegment.resize(codeSegment.max*2 + 100))
            return false;   
    }
    _GLOG(engone::log::out <<length()<< ": "<<instruction<<"\n";)
    *((Instruction*)codeSegment.data + codeSegment.used++) = instruction;
    return true;
}
bool Bytecode::addIm(i32 data){
    if(codeSegment.max == codeSegment.used){
        if(!codeSegment.resize(codeSegment.max*2 + 100))
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
    debugText.clear();
    debugText.shrink_to_fit();
}
const char* RegToStr(u8 reg){
    #define CASE(K,V) case BC_REG_##K: return "$"#V;
    #define CASER(K,V) case BC_REG_R##K##X: return "$r"#V"x";\
    case BC_REG_E##K##X: return "$e"#V "x";\
    case BC_REG_##K##X: return "$"#V "x";\
    case BC_REG_##K##L: return "$"#V "l";\
    case BC_REG_##K##H: return "$"#V "h";
    switch(reg){
        CASER(A,a)
        CASER(B,b)
        CASER(C,c)
        CASER(D,d)

        CASE(SP,sp)
        CASE(FP,fp)
        CASE(DP,dp)
        case 0: return "";
    }
    #undef CASE
    #undef CASER
    return "$?";
}
void Instruction::print(){
    using namespace engone;

    if(opcode==BC_INCR)
        log::out << log::PURPLE<<InstToStringX(opcode) << log::GRAY<<" "<<RegToStr(op0) << " "<< (i8)op1 << log::SILVER;
    else if(opcode==BC_ZERO_MEM)
        log::out << log::PURPLE<<InstToStringX(opcode) << log::GRAY<<" "<<RegToStr(op0) << " "<< (u8)op1 << log::SILVER;
    else if(opcode==BC_CAST)
        log::out << log::PURPLE<<InstToStringX(opcode) << log::GRAY<<" "<<op0<<" "<<RegToStr(op1) << " "<< RegToStr(op2) << log::SILVER;
    else
        log::out << log::PURPLE<<InstToStringX(opcode) << log::GRAY<<" "<<RegToStr(op0) << " "<<RegToStr(op1)<< " "<<RegToStr(op2)<<log::SILVER;
    
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
    Bytecode* ptr = (Bytecode*)engone::Allocate(sizeof(Bytecode));
    new(ptr)Bytecode();
    return ptr;
}
void Bytecode::Destroy(Bytecode* code){
    if(!code)
        return;
    code->cleanup();
    code->~Bytecode();
    engone::Free(code, sizeof(Bytecode));
}
int Bytecode::appendData(const void* data, int size){
    if(dataSegment.max < dataSegment.used + size){
        dataSegment.resize(dataSegment.max*2 + 50);
    }
    int index = dataSegment.used;
    memcpy((char*)dataSegment.data + index,data,size);
    dataSegment.used+=size;
    return index;
}
void Bytecode::addDebugText(Token& token, u32 instructionIndex){
    addDebugText(token.str,token.length,instructionIndex);
}
void Bytecode::addDebugText(const std::string& text, u32 instructionIndex){
    addDebugText(text.data(),text.length(),instructionIndex);
}
void Bytecode::addDebugText(const char* str, int length, u32 instructionIndex){
    using namespace engone;
    if(instructionIndex==(u32)-1){
        instructionIndex = codeSegment.used;
    }
    if(instructionIndex>=debugSegment.max){
        int newSize = codeSegment.max*1.5+20;
        newSize += (instructionIndex-debugSegment.max)*2;
        int oldmax = debugSegment.max;
        if(!debugSegment.resize(newSize))
            return;
        memset((char*)debugSegment.data + oldmax*debugSegment.m_typeSize,0,(debugSegment.max-oldmax)*debugSegment.m_typeSize);
    }
    int oldIndex = *((u32*)debugSegment.data + instructionIndex);
    if(oldIndex==0){
        int index = debugText.size();
        debugText.push_back(std::string(str,length));
        *((u32*)debugSegment.data + instructionIndex) = index + 1;
    }else{
        Assert((int)debugText.size()<=oldIndex)
        // debugText[oldIndex-1] += "\n"; // should line feed be forced?
        debugText[oldIndex-1] += std::string(str,length);
    }
}
const char* Bytecode::getDebugText(u32 instructionIndex){
    using namespace engone;
    if(instructionIndex>=debugSegment.max){
        // log::out<<log::RED << "out of bounds on debug text\n";
        return 0;
    }
    u32 index = *((u32*)debugSegment.data + instructionIndex);
    if(!index)
        return 0;
    index = index - 1; // little offset
    if(index>=debugText.size()){
        // This would be bad. The debugSegment shouldn't contain invalid values
        log::out<<log::RED << __FILE__ << ":"<<__LINE__<<", instruction index out of bounds\n";
        return 0;   
    }
    return debugText[index].c_str();
}