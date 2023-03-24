#include "BetBat/Context.h"

#include "BetBat/APICalls.h"

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
    if(ref.type==REF_NUMBER){
        Number* num = context->getNumber(ref.index);
        if(!num)
            engone::log::out << "null";
        else
            engone::log::out << num->value;
    }else if(ref.type==REF_STRING){
        String* str = context->getString(ref.index);
        if(!str)
            engone::log::out << "null";
        else
            PrintRawString(*str);
    }else{
        engone::log::out<<"?";
    }
}
void Context::load(Bytecode code){
    activeCode = code;
}
Number* Context::getNumber(uint32 index){
    return (Number*)numbers.data + index;
}
uint32 Context::makeNumber(){
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
void Context::deleteNumber(uint32 index){
    engone::log::out << __FUNCTION__<<" not implemented\n";
}
String* Context::getString(uint32 index){
    return (String*)strings.data + index;
}
uint32 Context::makeString(){
    if(strings.max == strings.used){
        if(!strings.resize(strings.max*2 + 10))
            return 0;   
    }
    int index = strings.used;
    String* num = (String*)strings.data + index;
    *num = {};
    strings.used++;
    return index;
}
void Context::deleteString(uint32 index){
    engone::log::out << __FUNCTION__<<" not implemented\n";
}
Scope* Context::getScope(int index){
    return (Scope*)scopes.data + index;
}
bool Context::ensureScopes(int depth){
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
    int programCounter=0;
    int limit=100;
    
    // activeCode.linkUnresolved("print",APIPrint);
    apiCalls["print"]=APIPrint;
    
    ensureScopes(1);
    
    #define CERR log::out << "ContextError "<<programCounter<<", "<<inst
    #define CWARN log::out << "ContextWarning "<<programCounter<<", "<<inst
        
    while(true){
        limit--;
        if(limit==0)
            break;
        Sleep(0.001); // prevent CPU from overheating if there is a bug with infinite loop
        if(activeCode.length() == programCounter)
            break;
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
                    
                    double num;
                    
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
                    log::out << inst << ", "<<v0->value<<" ";
                    
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
                    }
                    
                    log::out << " "<<v1->value<<" = "<<num<<"\n";
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
                            memcpy((char*)v2->memory.data+v1->memory.used, v1->memory.data,v1->memory.used);
                        } else {
                            // Note: the order of memcpy is important in case v1==v2.
                            //  copying v0 first to v2 would overwrite memory in v1 since they are the same.
                            memcpy((char*)v2->memory.data+v0->memory.used, v1->memory.data, v1->memory.used);
                            memcpy((char*)v2->memory.data,v0->memory.data, v0->memory.used);
                        }
                        v2->memory.used = v0->memory.used + v1->memory.used;
                        log::out << inst <<", \"";PrintRawString(*v2);log::out<<"\"\n";
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
                
                if(r0.type==REF_NUMBER){
                    r1.type = r0.type;
                    r1.index = makeNumber();
                    // Todo: check if makeNumber failed
                    
                    Number* n0 = getNumber(r0.index);
                    Number* n1 = getNumber(r1.index);
                    if(!n0||!n1){
                        CERR<< ", nummbers are null\n";   
                    }else{
                        n1->value = n0->value;
                        log::out << inst <<", copied "<< n1->value <<"\n";
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
                        log::out << inst <<", copied ";PrintRawString(*v1);log::out<<"\n";
                    }
                }else{
                    CERR<< ", cannot copy "<<RefToString(r0.type)<<"\n";   
                }
                break;
            }
            case BC_MOV:{
                Ref& r0 = references[inst.reg0];
                Ref& r1 = references[inst.reg1];
                
                r1 = r0;
                
                if(r0.type==REF_NUMBER){
                    Number* v0 = getNumber(r0.index);
                    log::out << inst <<", moved " <<v0->value<<"\n";
                } else if(r0.type==REF_STRING){
                    String* v0 = getString(r0.index);
                    log::out << inst <<", moved \"" ;PrintRawString(*v0);log::out << "\"\n";
                } else {
                    log::out << inst <<", moved ?\n";
                    continue;
                }
                break;   
            }
            case BC_NUM: {
                Ref& r0 = references[inst.reg0];
                
                r0.type = REF_NUMBER;
                r0.index = makeNumber();
                
                log::out << inst <<"\n";
                
                break;
            }
            case BC_STR: {
                Ref& r0 = references[inst.reg0];
                
                r0.type = REF_STRING;
                r0.index = makeString();
                
                log::out << inst <<"\n";
                
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
                log::out << inst <<" ["<<r0.index<<"]\n";
                
                break;
            }
            case BC_LOAD: {
                Ref& r0 = references[inst.reg0];
                uint extraData =  *(uint*)&activeCode.get(programCounter++);
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
                    //     PrintRef(this,DEFAULT_LOAD_CONST_REG);
                    //     log::out << " = "<< unresolved->name <<" (unresolved)\n";
                    // }else{
                    
                    Number* num = activeCode.getConstNumber(extraData);
                    if(!num){
                        CERR << ", number constant at "<< extraData<<" does not exist\n";   
                        continue;
                    }
                    n0->value = num->value;
                    log::out << inst << ", ";
                    PrintRef(this,inst.reg0);
                    log::out << " = "<< n0->value <<"\n";
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
                    log::out << inst << ", ";
                    PrintRef(this,inst.reg0);
                    log::out << " = \""; PrintRawString(*v0); log::out<<"\"\n";
                }else{
                    CERR << ", invalid type "<<RefToString(r0.type)<<" in registers\n";   
                }
                break;
            }
            case BC_JUMP: {
                Ref& r0 = references[inst.reg0];
                
                if(r0.type==REF_NUMBER){
                    Number* n0 = getNumber(r0.index);
                    uint address = n0->value;
                
                    // note that address is refers to the instruction position/index and not the byte memory address.
                    if(n0->value != (double)(uint)n0->value){
                        CERR<< ", decimal in register not allowed ("<<n0->value<<"!="<<((double)(uint)n0->value)<<")\n";   
                    } else if(activeCode.length()<address){
                        CERR << ", invalid address "<<address<<" (max "<<activeCode.length()<<")\n";   
                    }else if(programCounter == address){
                        log::out << "ContextWarning at "<<programCounter<<", " << inst << ", jumping to next instruction is redundant\n";   
                    }else{
                        programCounter = address;
                        log::out << inst << ", jumped to "<<programCounter<<"\n";
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
                    if(n2->value != (double)(uint)n2->value){
                        CERR<< ", decimal in register not allowed ("<<n2->value<<"!="<<((double)(uint)n2->value)<<")\n";
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
                            log::out << inst << ", "<<n0->value<<" == "<<n1->value<<", jumped to "<<programCounter<<"\n";
                        }else{
                            log::out << inst << ", "<<n0->value<<" != "<<n1->value<<", no jumping\n";
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
                            log::out << inst << ", ";PrintRawString(*v0);log::out<<" == "<<*v1<<", jumped to "<<programCounter<<"\n";
                        }else{
                            log::out << inst << ", ";PrintRawString(*v0);log::out<<" != ";PrintRawString(*v1);log::out<<", no jumping\n";
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
                    log::out << inst << "\n";
                }else{
                    CERR << ", could not ensure scope (allocation failure)\n";   
                }
                break;
            } case BC_EXITSCOPE: {
                if(currentScope>0){
                    currentScope--;
                    log::out << inst << "\n";
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
                    
                    Ref& rv = refs[DEFAULT_REG_RETURN_VALUE];
                    
                    // move return value to scope above
                    rv = r0;
                    
                    // jump to address
                    Ref& ra = refs[DEFAULT_REG_RETURN_ADDR];
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
                    log::out << inst << ", jumped to "<<programCounter<<", returned ";
                    PrintRefValue(this,rv);
                    log::out << "\n";
                    
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
                Ref& rz = refs[DEFAULT_REG_ARGUMENT];
                Ref& ra = references[DEFAULT_REG_RETURN_ADDR];
                
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
                    if(n1->value!=(double)(uint)n1->value){
                        CERR << ", $r1 cannot be decimal ("<<n1->value<<")\n";
                        continue;
                    }
                    currentScope++;
                    na->value = programCounter;
                    rz = r0;
                    programCounter = n1->value;
                    
                    // check if rz is number
                    log::out << inst << ", arg: ";
                    PrintRefValue(this,rz);
                    log::out<<", jump to "<<n1->value<<"\n";
                }else if(r1.type=REF_STRING){
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
                        log::out << inst << ", arg: ";
                        PrintRefValue(this,r0);
                        log::out<<", api call "<<name<<"\n";
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
                            int at=0;
                            const char* str=paths.data();
                            bool found=false;
                            while(true){
                                if(at>=paths.length())
                                    break;
                                int end = paths.find(";",at);
                                if(at>end) {
                                    at = end +1;
                                    continue;
                                }
                                if(end==-1)
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
                                log::out << inst << ", arg: ";
                                PrintRefValue(this,r0);
                                log::out<<", exe call "<<name<<"\n";
                            }
                        }
                    }
                }
                break;   
            }
            default:{
                log::out << inst << " Not implemented\n";
            }
        }
    }
}
void Context::Execute(Bytecode code){
    Context context;
    context.load(code);
    context.execute();   
}