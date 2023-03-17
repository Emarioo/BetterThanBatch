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
String* Context::getString(int index){
    return (String*)strings.data + index;
}
int Context::makeNumber(){
    if(numbers.max == numbers.used){
        if(numbers.resize(numbers.max*2 + 10))
            return 0;   
    }
    int index = numbers.used;
    Number* num = (Number*)numbers.data + index;
    *num = {};
    numbers.used++;
    return index;
}
int Context::makeString(){
    
    return 0;
}
void Context::execute(){
    printf("\n####   Execute   ####\n");
    int index=-1;
    while(true){
        if(activeCode.length() == index)
            break;
        Instruction inst = activeCode.get(index++);
        
        #define PRINT_3 printf(" %s $%hhu, $%hhu, $%hhu: ", InstToString(inst.type), inst.reg0,inst.reg1,inst.reg2);
        #define PRINT_2 printf(" %s $%hhu, [%hu]: ", InstToString(inst.type), inst.reg0,inst.var0);
        #define PRINT_1 printf(" %s $%hhu: ", InstToString(inst.type), inst.reg0);
        
        switch(inst.type){
            case BC_ADD: {
                Ref r0 = references[inst.reg0];
                Ref r1 = references[inst.reg1];
                Ref r2 = references[inst.reg2];
                
                if(r0.type==REF_NUMBER && r1.type == REF_NUMBER && r2.type == REF_NUMBER){
                    Number* n0 = getNumber(r0.index);
                    Number* n1 = getNumber(r1.index);
                    Number* n2 = getNumber(r2.index);
                    
                    n2->value = n0->value + n1->value;
                    PRINT_3 printf("[%d] = [%d]%lf + [%d]%lf\n", r2.index, r0.index, n0->value, r1.index, n1->value );
                }else{
                    int badType = 0;
                    if(r0.type != REF_NUMBER)
                        badType = r0.type;
                    if (r1.type != REF_NUMBER)
                        badType = r1.type;
                    if (r2.type != REF_NUMBER)
                        badType = r2.type;
                    printf("ContextError: %d:%s, invalid type %s in registers\n",index,InstToString(inst.type),RefToString(badType));   
                }
                break;
            }
            case BC_SUB:{
                
            }
            case BC_MAKE_NUMBER: {
                Ref r0 = references[inst.reg0];
                
                r0.type = REF_NUMBER;
                r0.index = makeNumber();
                
                PRINT_1  printf("$%hhu = [%d]\n", inst.reg0, r0.index );
                
                break;
            }
            case BC_LOAD_CONST: {
                Ref r0 = references[inst.reg0];
                
                if (r0.type == REF_NUMBER){
                    Number* n0 = getNumber(r0.index);
                    Number* num = activeCode.getConstNumber(inst.var0);
                    
                    n0->value = num->value;
                    
                    PRINT_2  printf("[%hu] = [%d]\n", inst.reg0, r0.index );    
                }else{
                    printf("ContextError: %d:%s, invalid type %s in registers\n",index,InstToString(inst.type),RefToString(r0.type));   
                }
                
                
                break;   
            }
        }
    }
}