#include "BetBat/Generator.h"

const char* InstToString(int type){
    #define INSTCASE(x) case x: return #x;
    switch(type){
        INSTCASE(BC_ADD)
        INSTCASE(BC_SUB)
        INSTCASE(BC_MUL)
        INSTCASE(BC_DIV)
        
        INSTCASE(BC_LOAD_CONST)
    }
    return "BC_?";
}

Instruction CreateInstruction(uint8 type, uint8 reg0, uint8 reg1, uint8 reg2){
    Instruction inst{};
    inst.type = type;
    inst.reg0 = reg0;
    inst.reg1 = reg1;
    inst.reg2 = reg2;
    return inst;
}
Instruction CreateInstruction(uint8 type, uint8 reg0, uint16 var0){
    Instruction inst{};
    inst.type = type;
    inst.reg0 = reg0;
    inst.reg0 = var0;
    return inst;
}
Instruction CreateInstruction(uint8 type, uint8 reg0){
    Instruction inst{};
    inst.type = type;
    inst.reg0 = reg0;
    return inst;
}
bool Bytecode::add(Instruction instruction){
    if(codeSegment.max == codeSegment.used){
        if(!codeSegment.resize(codeSegment.max*2 + 100))
            return false;   
    }
    *((Instruction*)codeSegment.data + codeSegment.used) = instruction;
    codeSegment.used++;
    return true;
}
int Bytecode::addConstNumber(double number){
    if(constNumbers.max == constNumbers.used){
        if(!constNumbers.resize(constNumbers.max*2 + 100))
            return false;   
    }
    int index = constNumbers.used;
    *((Number*)constNumbers.data + index) = {number};
    constNumbers.used++;
    return index;
}
Number* Bytecode::getConstNumber(int index){
    return (Number*)constNumbers.data + index;
}
int Bytecode::length(){
    return codeSegment.used;   
}
Instruction Bytecode::get(int index){
    if(index<0 || index >= codeSegment.used)
        return {};
    return *((Instruction*)codeSegment.data + index);
}

Bytecode Generate(Tokens tokens){
    printf("\n#### Generate ####\n");
    
    Bytecode bytecode{};
    #define GENADD(ARGS) bytecode.add(CreateInstruction ARGS);
    
    GENADD((BC_MAKE_NUMBER,0))
    GENADD((BC_MAKE_NUMBER,1))
    GENADD((BC_MAKE_NUMBER,2))
    
    int a = bytecode.addConstNumber(5);
    int b = bytecode.addConstNumber(9);
    
    GENADD((BC_LOAD_CONST,0,a))
    GENADD((BC_LOAD_CONST,1,b))
    
    GENADD((BC_ADD,0,1,2))
    GENADD((BC_ADD,2,1,2))
    
    return bytecode;
}