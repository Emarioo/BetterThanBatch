#include "BetBat/Context.h"

const char* RefToString(int type){
    #define REFCASE(x) case x: return #x;
    switch(type){
        REFCASE(REF_NUMBER)
        REFCASE(REF_STRING)
    }
    return "REF_?";
}
void Context::load(Bytecode code){
    activeCode = code;
}
Number* Context::getNumber(int index){
    return (Number*)numbers.data + index;
}
int Context::makeNumber(){
    if(numbers.max == numbers.used){
        if(!numbers.resize(numbers.max*2 + 10))
            return 0;   
    }
    int index = numbers.used;
    Number* num = (Number*)numbers.data + index;
    *num = {};
    numbers.used++;
    return index;
}
// String* Context::getString(int index){
//     return (String*)strings.data + index;
// }
// int Context::makeString(){
    
//     return 0;
// }
void Context::execute(){
    using namespace engone;
    log::out << "\n####   Execute   ####\n";
    int index=0;
    while(true){
        Sleep(0.001); // prevent CPU from overheating if there is a bug with infinite loop
        if(activeCode.length() == index)
            break;
        Instruction inst = activeCode.get(index++);
        
        // #define PRINT_3 printf(" %s $%hhu, $%hhu, $%hhu: ", InstToString(inst.type), inst.reg0,inst.reg1,inst.reg2);
        // #define PRINT_2 printf(" %s $%hhu, [%hu]: ", InstToString(inst.type), inst.reg0,inst.var0);
        // #define PRINT_1 printf(" %s $%hhu: ", InstToString(inst.type), inst.reg0);
        
        // #define PRINT_BC printf();

        switch(inst.type){
            case BC_ADD:
            case BC_SUB:
            case BC_MUL:
            case BC_DIV: {
                Ref& r0 = references[inst.reg0];
                Ref& r1 = references[inst.reg1];
                Ref& r2 = references[inst.reg2];
                
                if(r0.type==REF_NUMBER && r1.type == REF_NUMBER && r2.type == REF_NUMBER){
                    Number* n0 = getNumber(r0.index);
                    Number* n1 = getNumber(r1.index);
                    Number* n2 = getNumber(r2.index);
                    
                    double num;
                    
                    if(inst.type==BC_ADD)
                        num = n0->value + n1->value;
                    if(inst.type==BC_SUB)
                        num = n0->value - n1->value;
                    if(inst.type==BC_MUL)
                        num = n0->value * n1->value;
                    if(inst.type==BC_DIV)
                        num = n0->value / n1->value;
                    
                    inst.print(); log::out << "["<<r2.index<<"]"<<num<<" = ["<<r0.index<<"]"<<n0->value;
                    
                    if(inst.type==BC_ADD)
                        log::out <<" + [";
                    if(inst.type==BC_SUB)
                        log::out <<" - [";
                    if(inst.type==BC_MUL)
                        log::out <<" * [";
                    if(inst.type==BC_DIV)
                        log::out <<" / [";
                    
                    log::out << r1.index<<"]"<<n1->value<<"\n";
                    n2->value = num;
                }else{
                    int badType = 0;
                    if(r0.type != REF_NUMBER)
                        badType = r0.type;
                    if (r1.type != REF_NUMBER)
                        badType = r1.type;
                    if (r2.type != REF_NUMBER)
                        badType = r2.type;
                    log::out << "ContextError at "<<index<< " ["; inst.print(); log::out<<"] invalid type "<<RefToString(badType)<<" in registers\n";   
                }
                break;
            }
            case BC_MAKE_NUMBER: {
                Ref& r0 = references[inst.reg0];
                
                r0.type = REF_NUMBER;
                r0.index = makeNumber();
                
                inst.print(); log::out<<" ["<<r0.index<<"]\n";
                
                break;
            }
            case BC_LOAD_CONST: {
                Ref& r0 = references[inst.reg0];
                
                if (r0.type == REF_NUMBER){
                    Number* n0 = getNumber(r0.index);
                    Number* num = activeCode.getConstNumber(inst.reg12);
                    
                    n0->value = num->value;
                    
                    inst.print(); log::out << "["<<r0.index<<"] = "<<n0->value<<"\n";
                }else{
                    log::out << "ContextError at "<<index<<" ["; inst.print(); log::out << "] invalid type "<<RefToString(r0.type)<<" in registers\n";   
                }
                
                break;
            }
            case BC_JUMP: {
                uint address = (uint)inst.reg0 | ((uint)inst.reg12<<8);
                
                // note that address is refers to the instruction position/index and not the byte memory address.

                if(activeCode.length()<address){
                    log::out << "ContextError at "<<index<<" ["; inst.print(); log::out << "] invalid address "<<address<<" (max "<<activeCode.length()<<")\n";   
                }else if(index == address){
                    log::out << "ContextWarning at "<<index<<" ["; inst.print(); log::out << "] jumping to next instruction is redundant\n";   
                }else{
                    index = address;
                    log::out << inst << "\n";
                }
                break;
            }
            default:{
                log::out << inst << "Not implemented\n";
            }
        }
    }
}