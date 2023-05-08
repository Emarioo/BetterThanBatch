#include "BetBat/BytecodeX.h"

const char* InstToStringX(int type){
    #define CASE(x) case x: return #x;
    switch(type){
        CASE(BC_MOV_RR)
        CASE(BC_MOV_RM)
        CASE(BC_MOV_MR)
        
        CASE(BC_ADDI)
        CASE(BC_ADDF)
        CASE(BC_SUBI)
        CASE(BC_SUBF)
        CASE(BC_MULI)
        CASE(BC_MULF)
        CASE(BC_DIVI)
        CASE(BC_DIVF)
        
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
        CASE(BC_NOTB)
    }
    #undef CASE
    return "BC_?";
}


// const char* RegToString(uint8 reg){
//     // switch(reg){
//     //     case REG_NULL: return REG_NULL_S;
//     //     case REG_ZERO: return REG_ZERO_S;
//     //     case REG_RETURN_VALUE: return REG_RETURN_VALUE_S;
//     //     case REG_ARGUMENT: return REG_ARGUMENT_S;
//     //     case REG_FRAME_POINTER: return REG_FRAME_POINTER_S;
//     //     // case REG_STACK_POINTER: return REG_STACK_POINTER_S;
//     // }
//     // return crazyMap[reg];
//     return "regtostr broken";
    
//     // static std::unordered_map<uint8,std::string> s_regNameMap;
//     // auto find = s_regNameMap.find(reg);
//     // if(find!=s_regNameMap.end()) return find->second.c_str();
//     // std::string& tmp = s_regNameMap[reg]="a"+std::to_string(reg-REG_ACC0);
//     // return tmp.c_str();
// }

InstructionX* BytecodeX::get(uint index){
    // Assert(index<codeSegment.used);
    return ((InstructionX*)codeSegment.data + index);
}
int BytecodeX::length(){
    return (int)codeSegment.used;
}
bool BytecodeX::removeLast(){
    if(codeSegment.used>0)
        return false;
    codeSegment.used--;
    return true;
}
engone::Logger& operator<<(engone::Logger& logger, InstructionX& instruction){
    instruction.print();
    return logger;
}
// engone::Logger& operator<<(engone::Logger& logger, Bytecode::DebugLine& debugLine){
//     logger << engone::log::GRAY <<"Line "<<debugLine.line<<": ";
//     for(uint i=0;i<debugLine.length;i++){
//         logger<<*(debugLine.str+i);
//     }
//     return logger;
// }
uint32 BytecodeX::getMemoryUsage(){
    uint32 sum = codeSegment.max*codeSegment.m_typeSize
        +debugSegment.max*debugSegment.m_typeSize
        // debugtext?        
        ;
    return sum;
}
bool BytecodeX::add(InstructionX instruction){
    if(codeSegment.max == codeSegment.used){
        if(!codeSegment.resize(codeSegment.max*2 + 100))
            return false;   
    }
    _GLOG(engone::log::out <<length()<< ": "<<instruction<<"\n";)
    *((InstructionX*)codeSegment.data + codeSegment.used++) = instruction;
    return true;
}
bool BytecodeX::addIm(u32 data){
    if(codeSegment.max == codeSegment.used){
        if(!codeSegment.resize(codeSegment.max*2 + 100))
            return false;   
    }
    _GLOG(engone::log::out <<length()<< ": "<<data<<"\n";)
    // NOTE: This function works because sizeof Instruction == sizeof u32
    *((u32*)codeSegment.data + codeSegment.used++) = data;
    return true;
}
// bool Bytecode::add(uint8 type, uint8 reg0, uint8 reg1, uint8 reg2){
//     Instruction inst{};
//     inst.type = type;
//     inst.reg0 = reg0;
//     inst.reg1 = reg1;
//     inst.reg2 = reg2;
//     return add(inst);
// }
// bool Bytecode::add(uint8 type, uint8 reg0, uint16 reg12){
//     Instruction inst{};
//     inst.type = type;
//     inst.reg0 = reg0;
//     inst.reg1 = reg12&0xFF;
//     inst.reg2 = reg12>>8;
//     return add(inst);
// }
// bool Bytecode::add(uint8 type, uint reg012){
//     Instruction inst{};
//     inst.type = type;
//     inst.reg0 = reg012&0xFF;
//     inst.reg1 = (reg012>>8)&0xFF;
//     inst.reg2 = (reg012>>16)&0xFF;
//     return add(inst);
// }
// uint Bytecode::addConstNumber(Decimal number){
//     if(constNumbers.max == constNumbers.used){
//         if(!constNumbers.resize(constNumbers.max*2 + 100))
//             return -1;
//     }
//     int index = constNumbers.used;
//     *((Number*)constNumbers.data + index) = {number};
//     constNumbers.used++;
//     return index;
// }
// Number* Bytecode::getConstNumber(uint index){
//     if(index>=constNumbers.used) return 0;
//     return (Number*)constNumbers.data + index;
// }
// void BytecodeX::finalizePointers(){
//     for(int i=0;i<(int)constStrings.used;i++){
//         String* str = (String*)constStrings.data+i;
//         str->memory.data = (char*)constStringText.data + (uint64)str->memory.data;
//     }
//     for(int i=0;i<(int)debugLines.used;i++){
//         DebugLine* line = (DebugLine*)debugLines.data+i;
//         line->str = (char*)debugLineText.data + (uint64)line->str;
//     }
// }
// uint Bytecode::addConstString(Token& token, const char* padding){
//     int padlen = 0;
//     if(padding)
//         padlen = strlen(padding);
//     int length = token.length + padlen;
//     if(constStrings.max == constStrings.used){
//         if(!constStrings.resize(constStrings.max*2 + 100))
//             return -1;
//     }
//     if(constStringText.max < constStringText.used + length){
//         if(!constStringText.resize(constStringText.max*2 + length*30))
//             return -1;
//     }
//     int index = constStrings.used;
//     String* str = ((String*)constStrings.data + index);
//     new(str)String();
    
//     // if(!str->memory.resize(token.length)){
//     //     return -1;   
//     // }
//     // str->memory.used = token.length;

//     str->memory.data = (char*)constStringText.used;
//     // can't use consStringText.data since it may be reallocated
//     // str->memory.data = (char*)constStringText.data + constStringText.used;
//     str->memory.max = str->memory.used = length;
//     str->memory.m_typeSize=0;
    
//     char* ptr = (char*)constStringText.data + constStringText.used;
//     memcpy(ptr,token.str,token.length);
//     if(padding&&padlen!=0)
//         memcpy(ptr + token.length,padding,padlen);

//     constStrings.used++;
//     constStringText.used+=length;
//     return index;
// }
// bool Bytecode::addLoadNC(uint8 reg0, uint constIndex){
//     if((constIndex>>16)!=0)
//         engone::log::out << engone::log::RED<< "Bytecode::addLoadNC (at inst "<<length()<<"), TO SMALL FOR 32 bit integer ("<<constIndex<<")\n";
//     if(!add(BC_LOADNC,reg0,constIndex))
//         return false;
//     // if(!add(*(Instruction*)&constIndex)) return false;
//     return true;
// }
// bool Bytecode::addLoadV(uint8 reg0, uint stackIndex, bool global){
//     if((stackIndex>>15)!=0)
//         engone::log::out << engone::log::RED<< "Bytecode::addLoadC (at inst "<<length()<<"), TO SMALL FOR 15 bit integer ("<<stackIndex<<")\n";
//     int index = stackIndex;
//     if(global)
//         index |= 1<<15;
//     if(!add(BC_LOADV,reg0,index))
//         return false;
//     return true;
// }
// bool Bytecode::addLoadSC(uint8 reg0, uint constIndex){
//     if((constIndex>>16)!=0)
//         engone::log::out << engone::log::RED<< "Bytecode::addLoadSC (at inst "<<length()<<"), TO SMALL FOR 32 bit integer ("<<constIndex<<")\n";
//     if(!add(BC_LOADSC,reg0,constIndex))
//         return false;
//     // if(!add(*(Instruction*)&constIndex)) return false;
//     return true;
// }

void BytecodeX::cleanup(){
    codeSegment.resize(0);
    debugSegment.resize(0);
    debugText.clear();
    debugText.shrink_to_fit();
    // constNumbers.resize(0);
    // constStrings.resize(0);
    // constStringText.resize(0);
    // debugLines.resize(0);
    // debugLineText.resize(0);
}
// String* Bytecode::getConstString(uint index){
//     if(index>=constStrings.used) return 0;
//     return (String*)constStrings.data + index;
// }
// bool Bytecode::remove(int index){
//     if(index<0||index>=(int)codeSegment.used){
//         return false;
//     }
//     Instruction& inst = *((Instruction*)codeSegment.data + index);
//     inst = {};
//     return true;
// }
// Bytecode::DebugLine* Bytecode::getDebugLine(int instructionIndex, int* latestIndex){
//     if(latestIndex==0)
//         return 0;
//     int index = *latestIndex;
//     DebugLine* line = 0;
//     while(true){
//         if((int)index>=(int)debugLines.used)
//             return 0;
//         index++;
//         line = (DebugLine*)debugLines.data + index;
//         if(line->instructionIndex==instructionIndex)
//             break;
//     }
//     if(latestIndex)
//         *latestIndex = index;
//     return line;
// }
// Bytecode::DebugLine* Bytecode::getDebugLine(int instructionIndex){
//     for(int i=0;i<(int)debugLines.used;i++){
//         DebugLine* line = (DebugLine*)debugLines.data + i;
        
//         if(instructionIndex==line->instructionIndex){
//             return line;
//         }else if(instructionIndex<line->instructionIndex){
//             if(i==0)
//                 return 0;
//             return (DebugLine*)debugLines.data + i-1;
//         }
//     }
//     return 0;
// }

const char* RegToStr(u8 reg){
    #define CASE(K,V) case BC_REG_##K: return "$"#V;
    switch(reg){
        CASE(RAX,rax)
        CASE(RBX,rbx)
        CASE(RCX,rcx)
        CASE(RDX,rdx)
        CASE(SP,sp)
        CASE(FP,fp)
        case 0: return "";
    }
    #undef CASE
    return "$?";
}
void InstructionX::print(){
    using namespace engone;
    
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

BytecodeX* BytecodeX::Create(){
    return new BytecodeX();
}
void BytecodeX::Destroy(BytecodeX* code){
    if(!code)
        return;
    code->cleanup();
    delete code;
}
void BytecodeX::addDebugText(const std::string& text, u32 instructionIndex){
    using namespace engone;
    if(instructionIndex==(u32)-1){
        instructionIndex = codeSegment.used-1;
    }
    if(instructionIndex>=debugSegment.max){
        log::out <<log::RED<< __FILE__<<":"<<__LINE__<<", can't add debug text at "<<instructionIndex<<", debug segment to small ("<<debugSegment.max<<").\n";   
        return;
    }
    int index = debugText.size();
    debugText.push_back(text);
    *((u32*)debugSegment.data + instructionIndex) = index;
}
const char* BytecodeX::getDebugText(u32 instructionIndex){
    using namespace engone;
    if(instructionIndex>=debugSegment.max){
        log::out<<log::RED << __FILE__<<":"<<__LINE__<<", can't add debug text at "<<instructionIndex<<", debug segment to small ("<<debugSegment.max<<").\n";   
        return "";
    }
    u32 index = *((u32*)debugSegment.data + instructionIndex);
    if(index>=debugText.size()){
        log::out<<log::RED << __FILE__ << ":"<<__LINE__<<", instruction index out of bounds\n";
        return "";   
    }
    return debugText[index].c_str();
}