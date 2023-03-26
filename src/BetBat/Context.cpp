#include "BetBat/Context.h"

#include "BetBat/APICalls.h"
#include <string.h>

#ifdef CLOG
#define _CLOG(x) x;
#else
#define _CLOG(x) ;
#endif

const char* RefToString(int type){
    #define REFCASE(x) case x: return #x;
    switch(type){
        REFCASE(REF_NUMBER)
        REFCASE(REF_STRING)
    }
    return "REF_?";
}
const char* RefToString(Ref& ref){
    #define REFCASE(x) case x: return #x;
    switch(ref.type){
        REFCASE(REF_NUMBER)
        REFCASE(REF_STRING)
    }
    return "REF_?";
}
void PrintRef(Context* context, int index){
    engone::log::out << "$"<<index;
}
void PrintRefValue(Context* context, Ref& ref){
    using namespace engone;
    if(ref.type==REF_NUMBER){
        Number* num = context->getNumber(ref.index);
        if(!num)
            log::out << log::RED<<"null"<<log::SILVER;
        else
            log::out << num->value;
    }else if(ref.type==REF_STRING){
        String* str = context->getString(ref.index);
        if(!str)
            log::out <<log::RED<< "null"<<log::SILVER;
        else
            PrintRawString(*str);
    }else{
        log::out<<"?";
    }
}
void Context::load(Bytecode code){
    activeCode = code;
}
Number* Context::getNumber(uint32 index){
    if(numbers.used<=index) return 0;
    return (Number*)numbers.data + index;
}
uint32 Context::makeNumber(){
    if(numbers.max == numbers.used){
        if(!numbers.resize(numbers.max*2 + 10))
            return 0;   
    }
     if(infoValues.max <= numbers.used){
        int oldMax = infoValues.max;
        if(!infoValues.resize(infoValues.max*2 + 10))
            return 0; 
        memset((char*)infoValues.data+oldMax,0,(infoValues.max-oldMax)*infoValues.m_typeSize);
    }
    int index = -1;
    for(uint i=0;i<numbers.used;i++){
        uint8* info = (uint8*)infoValues.data + i;
        if((*info & REF_NUMBER) == 0){
            index = i;
        }
    }
    if(index==-1){
        index = numbers.used++;
    }
    Number* num = (Number*)numbers.data + index;
    new(num)Number();
    uint8* info = (uint8*)infoValues.data + index;
    *info |= REF_NUMBER;
    return index;
}
void Context::deleteNumber(uint32 index){
    uint8* info = (uint8*)infoValues.data + index;
    if((*info) & REF_NUMBER){
        *info = (*info) & (~REF_NUMBER);

        if(index+1==numbers.used){
            numbers.used--;
            while(numbers.used>0){
                uint8* info = (uint8*)infoValues.data + numbers.used-1;
                if(((*info)&REF_NUMBER))
                    break;
                numbers.used--;
            }
        }
    }
}
String* Context::getString(uint32 index){
     if(strings.used<=index) return 0;
    return (String*)strings.data + index;
}
uint32 Context::makeString(){
    if(strings.max == strings.used){
        if(!strings.resize(strings.max*2 + 10))
            return 0;
    }
    if(infoValues.max <= strings.used){
        int oldMax = infoValues.max;
        if(!infoValues.resize(infoValues.max*2 + 10))
            return 0; 
        memset((char*)infoValues.data+oldMax,0,(infoValues.max-oldMax)*infoValues.m_typeSize);
    }
    int index = -1;
    for(uint i=0;i<strings.used;i++){
        uint8* info = (uint8*)infoValues.data + i;
        if((*info & REF_STRING) == 0){
            index = i;
        }
    }
    if(index==-1){
        index = strings.used++;
    }

    String* str = (String*)strings.data + index;
    new(str)String();
    uint8* info = (uint8*)infoValues.data + index;
    *info |= REF_STRING;
    return index;
}
void Context::cleanup(){
    numbers.resize(0);
    for(uint i=0;i<strings.used;i++){
        String* str = (String*)strings.data+i;
        if((*(uint8*)infoValues.data + i)&REF_STRING)
            str->memory.resize(0);
    }
    infoValues.resize(0);
    scopes.resize(0);
    valueStack.resize(0);
}
void Context::deleteString(uint32 index){
    uint8* info = (uint8*)infoValues.data + index;
    if((*info) & REF_STRING){
        *info = (*info) & (~REF_STRING);
        
        String* str = (String*)strings.data + index;
        str->memory.resize(0);

        if(index+1==strings.used){
            strings.used--;
            while(strings.used>0){
                uint8* info = (uint8*)infoValues.data + strings.used-1;
                if(((*info)&REF_STRING))
                    break;
                strings.used--;
            }
        }
    }
}
Scope* Context::getScope(uint index){
    return (Scope*)scopes.data + index;
}
bool Context::ensureScopes(uint depth){
    if(scopes.used<depth){
        if(!scopes.resize(depth))
            return false;
        while(scopes.used<depth){
            Scope* scope = (Scope*)scopes.data+scopes.used;
            new(scope)Scope();
            scopes.used++;
        }
    }
    return true;
}
// void LinkFunction(Bytecode*)
void Context::execute(){
    using namespace engone;
    log::out << "\n####   Execute   ####\n";
    uint programCounter=0;

    uint latestDebugLine=0;
    
    apiCalls["print"]=APIPrint;
    
    ensureScopes(1);

    #define CERR log::out << log::RED<< "ContextError "<<programCounter<<", "<<inst
    #define CWARN log::out << log::YELLOW<< "ContextWarning "<<programCounter<<", "<<inst
    
    auto startTime = MeasureSeconds();

    int limit = activeCode.length()*1.5;
    while(true){
        limit--;
        if(limit==0){
            // prevent infinite loop
            log::out << "REACHED SAFETY LIMIT\n";
            break;
        }
        if(activeCode.length() == programCounter)
            break;

        Bytecode::DebugLine* debugLine = activeCode.getDebugLine(programCounter,&latestDebugLine);
        if(debugLine){
            log::out << *debugLine;
        }

        Instruction inst = activeCode.get(programCounter++);
        Ref* references = getScope(currentScope)->references;

        switch(inst.type){
            case BC_ADD:
            case BC_SUB:
            case BC_MUL:
            case BC_DIV:
            case BC_LESS:
            case BC_GREATER:
            case BC_EQUAL: {
                Ref& r0 = references[inst.reg0];
                Ref& r1 = references[inst.reg1];
                Ref& r2 = references[inst.reg2];
                
                if(r0.type==REF_NUMBER && r1.type == REF_NUMBER && r2.type == REF_NUMBER){
                    Number* v0 = getNumber(r0.index);
                    Number* v1 = getNumber(r1.index);
                    Number* v2 = getNumber(r2.index);
                    
                    if(!v0||!v1||!v2){
                        CERR << ", values were null\n";   
                        continue;
                    }
                    
                    Decimal num;
                    
                    if(inst.type==BC_ADD)
                        num = v0->value + v1->value;
                    else if(inst.type==BC_SUB)
                        num = v0->value - v1->value;
                    else if(inst.type==BC_MUL)
                        num = v0->value * v1->value;
                    else if(inst.type==BC_DIV)
                        num = v0->value / v1->value;
                    else if(inst.type==BC_LESS)
                        num = v0->value < v1->value;
                    else if(inst.type==BC_GREATER)
                        num = v0->value > v1->value;
                    else if(inst.type==BC_EQUAL)
                        num = v0->value == v1->value;
                    else {
                        CERR << ", inst. not available for "<<RefToString(r0.type)<<"\n";   
                    }
                    _CLOG(log::out << inst << ", "<<v0->value<<" ";
                    if(inst.type==BC_ADD)
                        log::out <<"+";
                    else if(inst.type==BC_SUB)
                        log::out <<"-";
                    else if(inst.type==BC_MUL)
                        log::out <<"*";
                    else if(inst.type==BC_DIV)
                        log::out <<"/";
                    else if(inst.type==BC_LESS)
                        log::out <<"<";
                    else if(inst.type==BC_GREATER)
                        log::out <<">";
                    else if(inst.type==BC_EQUAL)
                        log::out <<"==";
                    else {
                        CERR <<", BUG AT "<<__FILE__<<":"<<__LINE__<<"\n";   
                    })
                    
                    _CLOG(log::out << " "<<v1->value<<" = "<<num<<"\n";)
                    v2->value = num;
                } else if(r0.type==REF_STRING && r1.type == REF_STRING && r2.type == REF_STRING){
                    String* v0 = getString(r0.index);
                    String* v1 = getString(r1.index);
                    String* v2 = getString(r2.index);
                    
                    if(!v0||!v1||!v2){
                        CERR <<", values were null\n";   
                        continue;
                    }
                    
                    if(inst.type==BC_ADD){
                        if(!v2->memory.resize(v0->memory.used + v1->memory.used)){
                            CERR <<", allocation failed\n";   
                            continue;
                        }
                        if(v0==v2){
                            // append
                            memcpy((char*)v2->memory.data+v0->memory.used, v1->memory.data,v1->memory.used);
                        } else {
                            // Note: the order of memcpy is important in case v1==v2.
                            //  copying v0 first to v2 would overwrite memory in v1 since they are the same.
                            memcpy((char*)v2->memory.data+v0->memory.used, v1->memory.data, v1->memory.used);
                            memcpy((char*)v2->memory.data,v0->memory.data, v0->memory.used);
                        }
                        v2->memory.used = v0->memory.used + v1->memory.used;
                        _CLOG(log::out << inst <<", \"";PrintRawString(*v2);log::out<<"\"\n";)
                    } else {
                        CERR<<", inst. not available for "<<RefToString(r0.type)<<"\n";   
                    }
                } else if(r0.type==REF_STRING && r1.type == REF_NUMBER && r2.type == REF_STRING){
                    String* v0 = getString(r0.index);
                    Number* v1 = getNumber(r1.index);
                    String* v2 = getString(r2.index);
                    
                    if(!v0||!v1||!v2){
                        CERR <<", one value were null ";PrintRefValue(this,r0);log::out<<" ";
                            PrintRefValue(this,r1);log::out<<" ";PrintRefValue(this,r2);log::out<<"\n";   
                        continue;
                    }
                    
                    if(inst.type==BC_ADD){
                        int numlength = 20; // Todo: how large can a decimal be? should include \0 because of sprintf
                        if(!v2->memory.resize(v0->memory.used + numlength)){
                            CERR <<", allocation failed\n";   
                            continue;
                        }
                        if(v0!=v2) {
                            memcpy((char*)v2->memory.data,v0->memory.data, v0->memory.used);
                        }
                        int written = sprintf((char*)v2->memory.data+v0->memory.used,"%lf",v1->value);
                        if(written>numlength){
                            CERR << ", double turned into "<<written<<" chars (max "<<numlength<<")\n";
                            continue;
                        }
                        v2->memory.used = v0->memory.used + written;
                        _CLOG(log::out << inst <<", \"";PrintRawString(*v2);log::out<<"\"\n";)
                    } else {
                        CERR<<", inst. not available for "<<RefToString(r0.type)<<"\n";   
                    }
                }else{
                    CERR<<", invalid types "<<
                        RefToString(r0.type)<<" "<<RefToString(r1.type)<<" "<<RefToString(r2.type)<<" in registers\n";   
                }
                break;
            }
            case BC_COPY:{
                Ref& r0 = references[inst.reg0];
                Ref& r1 = references[inst.reg1];
                 
                // not redundant since you are copying the value.
                // the original value may still exist on the stack
                // if(inst.reg0==inst.reg1){
                //     CERR << ", redundant move to same register\n";
                //     continue;
                // }
                
                // if(r0.type==r1.type&&r0.index==r1.index){
                //     CWARN << ", redundant copying same value (["<<r1.index<<"] = ["<<r0.index<<"])\n";
                //     continue;
                // }

                if(r0.type==REF_NUMBER){
                    r1.type = r0.type;
                    // if r0==r1 then r1.index=makeNumber will overwrite r0.index
                    //  temp variable will solve it
                    int tempIndex = r0.index;
                    r1.index = makeNumber();
                    // Todo: check if makeNumber failed
                    
                    Number* n0 = getNumber(tempIndex);
                    Number* n1 = getNumber(r1.index);
                    if(!n0||!n1){
                        CERR<< ", nummbers are null\n";
                    }else{
                        n1->value = n0->value;
                        _CLOG(log::out << inst <<" ["<<r1.index<<"], copied "<< n1->value <<"\n";)
                    }
                }else if(r0.type==REF_STRING){
                    r1.type = r0.type;
                    r1.index = makeString();
                    // Todo: check if makeString failed
                    
                    String* v0 = getString(r0.index);
                    String* v1 = getString(r1.index);
                    if(!v0||!v1){
                        CERR << ", values are null\n";
                    }else{
                        v0->copy(v1);
                        _CLOG(log::out << inst <<" ["<<r1.index<<"], copied ";PrintRawString(*v1);log::out<<"\n";)  
                    }
                }else{
                    CERR<< ", cannot copy "<<RefToString(r0.type)<<"\n";   
                }
                break;
            }
            case BC_MOV:{
                Ref& r0 = references[inst.reg0];
                Ref& r1 = references[inst.reg1];
                
                if(inst.reg0==inst.reg1){
                    CERR << ", redundant move to same register\n";
                    continue;
                }

                r1 = r0;
                
                if(r0.type==REF_NUMBER){
                    Number* v0 = getNumber(r0.index);
                    _CLOG(log::out << inst <<", moved " <<v0->value<<"\n";)
                } else if(r0.type==REF_STRING){
                    String* v0 = getString(r0.index);
                    _CLOG(log::out << inst <<", moved \"" ;PrintRawString(*v0);log::out << "\"\n";)
                } else {
                    _CLOG(log::out << inst <<", moved ?\n";)
                    continue;
                }
                break;   
            }
            case BC_NUM: {
                Ref& r0 = references[inst.reg0];
                
                r0.type = REF_NUMBER;
                r0.index = makeNumber();
                
                _CLOG(log::out << inst <<" ["<<r0.index<<"]\n";)
                
                break;
            }
            case BC_STR: {
                Ref& r0 = references[inst.reg0];
                
                r0.type = REF_STRING;
                r0.index = makeString();
                
                _CLOG(log::out << inst <<" ["<<r0.index<<"]\n";)
                
                break;
            }
            case BC_DEL: {
                Ref& r0 = references[inst.reg0];
                
                if(r0.type==REF_NUMBER){
                    deleteNumber(r0.index);
                }else if(r0.type==REF_STRING){
                    deleteString(r0.index);
                }else{
                    CERR << ", invalid type "<<RefToString(r0.type)<<" in registers\n";
                    continue;
                }
                _CLOG(log::out << inst <<" ["<<r0.index<<"]\n";)
                r0 = {};
                break;
            }
            case BC_LOADC: {
                Ref& r0 = references[inst.reg0];
                // uint extraData =  *(uint*)&activeCode.get(programCounter++);
                uint extraData = (uint)inst.reg1|((uint)inst.reg2<<8);
                // Todo: BC_MAKE_NUMBER if ref doesn't have a number? option in the compiler to make numbers when necessary?
                if (r0.type == REF_NUMBER){
                    Number* n0 = getNumber(r0.index);
                    if(!n0){
                       CERR << ", register was null\n";
                       continue;
                    }
                    // auto unresolved = activeCode.getUnresolved(inst.singleReg());
                    // if(unresolved){
                    //     n0->value = unresolved->address;
                    //     log::out << inst << ", ";
                    //     PrintRef(this,LOAD_CONST_REG);
                    //     log::out << " = "<< unresolved->name <<" (unresolved)\n";
                    // }else{
                    Number* num = activeCode.getConstNumber(extraData);
                    if(!num){
                        CERR << ", number constant at "<< extraData<<" does not exist\n";   
                        continue;
                    }
                    n0->value = num->value;
                    _CLOG(log::out << inst << ", reg = " << n0->value<<"\n";)
                    // PrintRef(this,inst.reg0);
                    // log::out << " = "<< n0->value <<"\n";
                    // }
                }else if (r0.type == REF_STRING){
                    String* v0 = getString(r0.index);
                    if(!v0){
                       CERR << ", register was null\n";
                       continue;
                    }
                    String* vc = activeCode.getConstString(extraData);
                    if(!vc){
                       CERR << ", string constant at "<<extraData<<" was null\n";   
                       continue;
                    }
                    vc->copy(v0);
                    _CLOG(log::out << inst << ", reg = ";PrintRawString(*v0);log::out<<"\n";)
                    // PrintRef(this,inst.reg0);
                    // log::out << " = \""; PrintRawString(*v0); log::out<<"\"\n";
                }else{
                    CERR << ", invalid type "<<RefToString(r0.type)<<" in registers\n";   
                }
                break;
            }
            case BC_PUSH:{
                Ref& r0 = references[inst.reg0];
                Ref& sp = references[REG_STACK_POINTER];

                if(sp.type != REF_NUMBER){
                    CERR << ", $sp must be a number\n";
                    continue;
                }
                Number* stackNumber = getNumber(sp.index);
                if(stackNumber->value!=(Decimal)(uint)stackNumber->value){
                    CERR << ", $sp cannot have decimals  ($sp = "<<stackNumber->value<<")\n";
                    continue;
                }
                if(valueStack.max <= (uint)stackNumber->value){
                    if(!valueStack.resize(valueStack.max*2+10)){
                        CERR << ", stack allocation failed\n";
                        continue;
                    }
                }
                if(stackNumber->value<0){
                    CERR<<", $sp cannot be negative ($sp = "<<(uint)stackNumber->value<<")\n";
                    continue;
                }
                uint index = stackNumber->value;
                Ref* ref = (Ref*)valueStack.data+index;
                stackNumber->value++;

                *ref = r0;
                _CLOG(log::out << inst << ", pushed ";PrintRefValue(this,r0);log::out<<" at "<<index<<"\n";)
                    
                break;
            }
            case BC_POP:{
                Ref& r0 = references[inst.reg0];
                Ref& sp = references[REG_STACK_POINTER];

                if(sp.type != REF_NUMBER){
                    CERR << ", $sp must be a number\n";
                    continue;
                }
                Number* stackNumber = getNumber(sp.index);
                if(stackNumber->value!=(Decimal)(uint)stackNumber->value){
                    CERR << ", $sp cannot have decimals ($sp = "<<stackNumber->value<<")\n";
                    continue;
                }
                if(stackNumber->value<1){
                    CERR<<", $sp cannot be negative or zero ($sp = "<<(uint)stackNumber->value<<")\n";
                    continue;
                }
                if(stackNumber->value>=valueStack.max){
                    CERR << ", cannot pop beyond the allocated stack ($sp = "<<(uint)stackNumber->value<<")\n";
                    continue;
                }
                stackNumber->value--;
                Ref* ref = (Ref*)valueStack.data+(uint)stackNumber->value;
                r0 = *ref;
                _CLOG(log::out << inst << ", popped ";PrintRefValue(this, r0);log::out<<"\n";)
                  
                break;
            }
            case BC_LOADV:{
                Ref& r0 = references[inst.reg0];
                // Ref& r1 = references[inst.reg1];
                int offsetFrameIndex = (uint)inst.reg1 | ((uint)inst.reg2<<8);
                Ref& fp = references[REG_FRAME_POINTER];

                if(fp.type != REF_NUMBER){
                    CERR << ", $fp must be a number\n";
                    continue;
                }
                Number* stackNumber = getNumber(fp.index);
                if(!stackNumber){
                    CERR << ", $fp cannot be null\n";
                    continue;
                }
                if(stackNumber->value!=(Decimal)(uint)stackNumber->value){
                    CERR << ", $fp cannot have decimals ($sp = "<<stackNumber->value<<")\n";
                    continue;
                }
                // Number* offset = getNumber(r1.index);
                // if(!offset){
                //     CERR<<", $r1 (offset reg.) cannot be null\n";
                //     continue;
                // }
                // if(offset->value!=(Decimal)(uint)offset->value){
                //     CERR << ", $r1 (offset reg.) cannot have decimals  ($sp = "<<offset->value<<")\n";
                //     continue;
                // }
                // int index = stackNumber->value + offset->value;
                int index = stackNumber->value + offsetFrameIndex;
                if(index<0){
                    CERR << ", $fp + $r1 cannot be negative ($fp+$r1 = "<<index<<")\n";
                    continue;
                }
                if((uint)index>=valueStack.max){
                    CERR << ", $fp + $r1 goes beyond the allocated stack ($fp+$r1 = "<<index<<")\n";
                    continue;
                }
                Ref* ref = (Ref*)valueStack.data+index;
                r0 = *ref;
                _CLOG(log::out << inst << ", loaded ";PrintRefValue(this,r0);log::out<<" from "<<index<<"\n";)
                break;
            }
            case BC_JUMP: {
                Ref& r0 = references[inst.reg0];
                
                if(r0.type==REF_NUMBER){
                    Number* n0 = getNumber(r0.index);
                    uint address = n0->value;
                
                    // note that address is refers to the instruction position/index and not the byte memory address.
                    if(n0->value != (Decimal)(uint)n0->value){
                        CERR<< ", decimal in register not allowed ("<<n0->value<<"!="<<((Decimal)(uint)n0->value)<<")\n";   
                    } else if(activeCode.length()<address){
                        CERR << ", invalid address "<<address<<" (max "<<activeCode.length()<<")\n";   
                    }else if(programCounter == address){
                        _CLOG(log::out << "ContextWarning at "<<programCounter<<", " << inst << ", jumping to next instruction is redundant\n";)
                    }else{
                        programCounter = address;
                        _CLOG(log::out << inst << ", jumped to "<<programCounter<<"\n";)
                    }
                }else{
                    CERR << ", invalid type "<<RefToString(r0.type)<<" in registers\n";   
                }
                break;
            }
            case BC_JUMPIF: {
                Ref& r0 = references[inst.reg0];
                Ref& r1 = references[inst.reg1];
                Ref& r2 = references[inst.reg2];
                
                if(r2.type==REF_NUMBER){
                    Number* n2 = getNumber(r2.index);
                    uint address = n2->value;
                    
                    // note that address is refers to the instruction position/index and not the byte memory address.
                    if(n2->value != (Decimal)(uint)n2->value){
                        CERR<< ", decimal in register not allowed ("<<n2->value<<"!="<<((Decimal)(uint)n2->value)<<")\n";
                        continue;
                    } else if(activeCode.length()<address){
                        CERR << ", invalid address "<<address<<" (max "<<activeCode.length()<<")\n";   
                        continue;
                    }else if(programCounter == address){
                        CWARN << ", jumping to next instruction is redundant\n";   
                        continue;
                    }
                    
                    if(r0.type==REF_NUMBER&&r1.type==REF_NUMBER){
                        Number* n0 = getNumber(r0.index);
                        Number* n1 = getNumber(r1.index);
                        if(!n0||!n1){
                            CERR << ", numbers where null\n";  
                            continue;   
                        }
                        if(n0->value == n1->value){
                            programCounter = address;
                            _CLOG(log::out << inst << ", "<<n0->value<<" == "<<n1->value<<", jumped to "<<programCounter<<"\n";)
                        }else{
                            _CLOG(log::out << inst << ", "<<n0->value<<" != "<<n1->value<<", no jumping\n";)
                        }
                    } else if(r0.type==REF_STRING&&r1.type==REF_STRING){
                        String* v0 = getString(r0.index);
                        String* v1 = getString(r1.index);
                        if(!v0||!v1){
                            CERR << ", numbers where null\n";  
                            continue;   
                        }
                        if(*v0 == *v1){
                            programCounter = address;
                            _CLOG(log::out << inst << ", ";PrintRawString(*v0);log::out<<" == "<<*v1<<", jumped to "<<programCounter<<"\n";)
                        }else{
                            _CLOG(log::out << inst << ", ";PrintRawString(*v0);log::out<<" != ";PrintRawString(*v1);log::out<<", no jumping\n";)
                        }
                    } else{
                        CERR << ", invalid type "<<RefToString(r0.type)<<" "<<RefToString(r1.type)<<"\n";  
                        continue;
                    }
                }else{
                    CERR << ", invalid type "<<RefToString(r0.type)<<" in registers (should be number)\n";   
                }
                break;
            } case BC_ENTERSCOPE: {
                bool yes = ensureScopes(currentScope+1);
                if(yes){
                    currentScope++;
                    _CLOG(log::out << inst << "\n";)
                }else{
                    CERR << ", could not ensure scope (allocation failure)\n";   
                }
                break;
            } case BC_EXITSCOPE: {
                if(currentScope>0){
                    currentScope--;
                    _CLOG(log::out << inst << "\n";)
                }else{
                    CERR << ", cannot exit at global scope (currentScope == "<<currentScope<<")\n";   
                }
                break;
            }case BC_RETURN: {
                Ref& r0 = references[inst.reg0];
                
                if(currentScope<=0){
                    CERR << ", cannot return from global scope (currentScope == "<<currentScope<<")\n";   
                    continue;
                }else{
                    currentScope--;
                    Ref* refs = getScope(currentScope)->references;
                    
                    Ref& rv = refs[REG_RETURN_VALUE];
                    
                    // move return value to scope above
                    rv = r0;
                    
                    // jump to address
                    Ref& ra = refs[REG_RETURN_ADDR];
                    if(ra.type!=REF_NUMBER){
                        CERR<<", $ra must be a number (was ";
                        PrintRefValue(this,ra);
                        log::out << ")\n";
                        continue;
                    }
                    Number* n0 = getNumber(ra.index);
                    if(!n0){
                        CERR<<", value was null\n";
                        continue;
                    }
                    // Todo: error decimal number
                    programCounter = n0->value;
                    _CLOG(log::out << inst << ", jumped to "<<programCounter<<", returned ";
                    PrintRefValue(this,rv);
                    log::out << "\n";)
                    
                }
                break;
            } case BC_RUN: {
                Ref& r0 = references[inst.reg0];
                Ref& r1 = references[inst.reg1];
                
                bool yes = ensureScopes(currentScope+1);
                if(!yes){
                    CERR << ", could not ensure scope (allocation failure)\n";
                    continue;  
                }
                
                Ref* refs = getScope(currentScope+1)->references;
                Ref& rz = refs[REG_ARGUMENT];
                Ref& ra = references[REG_RETURN_ADDR];
                
                if(ra.type!=REF_NUMBER){
                    CERR<<", $ra must be a number (was ";
                    PrintRefValue(this,ra);
                    log::out << ")\n";
                    continue;
                }
                Number* na = getNumber(ra.index);
                if(r1.type==REF_NUMBER){
                    // CERR << ", jump register must be a number (was "<<RefToString(r1)<<")\n";
                    // continue;
                    Number* n1 = getNumber(r1.index);
                    if(!na){
                        CERR<<", $ra was null\n";
                        continue;
                    }
                    if(!n1){
                        CERR << ", $r1 is null\n";
                        continue;
                    }
                    if(n1->value!=(Decimal)(uint)n1->value){
                        CERR << ", $r1 cannot be decimal ("<<n1->value<<")\n";
                        continue;
                    }
                    currentScope++;
                    na->value = programCounter;
                    rz = r0;
                    programCounter = n1->value;
                    
                    // check if rz is number
                    _CLOG(log::out << inst << ", arg: ";
                    PrintRefValue(this,rz);
                    log::out<<", jump to "<<n1->value<<"\n";)
                }else if(r1.type==REF_STRING){
                    String* v1 = getString(r1.index);
                // auto unresolved = activeCode.getUnresolved(n1->value);
                // if(!unresolved){
                // }else{
                    std::string name = *v1;
                    auto find = apiCalls.find(name);
                    if(find!=apiCalls.end()){
                        void* arg=0;
                        if(r0.type==REF_STRING)
                            arg = getString(r0.index);
                        else if(r0.type==REF_NUMBER)
                            arg = getNumber(r0.index);
                        else{
                            CERR << ", unresolved function does not allow "<<RefToString(r0.type)<<" as argument\n";
                            continue;
                        }
                        _CLOG(log::out << inst << ", arg: ";
                        PrintRefValue(this,r0);
                        log::out<<", api call "<<name<<"\n";)
                        if(find->second){
                            find->second(this,r0.type,arg);
                        }else{
                            CERR << ", api call was null\n";
                        }
                    }else{
                        std::string foundPath="";
                        
                        if(FileExist(name)){
                            foundPath = name;
                        }else{
                            std::string paths = EnvironmentVariable("PATH");
                            uint at=0;
                            const char* str=paths.data();
                            bool found=false;
                            while(true){
                                if(at>=paths.length())
                                    break;
                                uint end = paths.find(";",at);
                                if(at>end) {
                                    at = end +1;
                                    continue;
                                }
                                if(end==(uint)-1)
                                    end = paths.length();
                                std::string view = paths.substr(at,end-at);
                                // log::out << view.data()<<"\n";
                                if(view.length()==0)
                                    break;
                                // ((char*)view.data())[end] = 0;
                                at = end+1;
                                
                                std::string exepath = "";
                                exepath += view;
                                exepath += "\\";
                                exepath += name;
                                if(FileExist(exepath)){
                                    foundPath = exepath;
                                    break;
                                }
                            }
                        }
                        if(foundPath.empty()){
                            CERR << ", "<<name<<" could not be found\n";
                        }else{
                            std::string temp=std::move(foundPath);
                            if(r0.type==REF_NUMBER){
                                Number* v0 = getNumber(r0.index);
                                if(v0){
                                    temp += " ";
                                    temp += std::to_string(v0->value);
                                }
                            }else if(r0.type==REF_STRING){
                                String* v0 = getString(r0.index);
                                temp += " ";
                                temp += *v0;
                            }
                            bool yes = StartProgram("",(char*)temp.data(),PROGRAM_WAIT);
                            // bool yes = StartProgram(foundPath.c_str(),buffer,PROGRAM_WAIT);
                            // bool yes = StartProgram(foundPath.c_str(),args,PROGRAM_WAIT|PROGRAM_NEW_CONSOLE);
                            if(!yes){
                                CERR << ", "<<name<<" found but cannot start\n";
                            }else{
                                _CLOG(log::out << inst << ", arg: ";
                                PrintRefValue(this,r0);
                                log::out<<", exe call "<<name<<"\n";)
                            }
                        }
                    }
                }
                break;
            }
            default:{
                log::out << log::RED<<inst << " Not implemented\n";
            }
        }
    }
    double executionTime = StopMeasure(startTime);
    log::out << "Executed in "<<executionTime<<" seconds\n";
    if(numbers.used!=0||strings.used!=0){
        log::out << log::YELLOW<<"Ended with "<<numbers.used << " numbers and "<<strings.used << " strings (based on .used)\n";
    }
}
void Context::Execute(Bytecode code){
    Context context{};
    context.load(code);
    context.execute(); 
    context.cleanup();  
}