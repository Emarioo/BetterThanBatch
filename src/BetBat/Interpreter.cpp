#include "BetBat/Interpreter.h"

#include "BetBat/ExternalCalls.h"
#include "BetBat/Utility.h"
#include <string.h>

#ifdef CLOG
#define _CLOG(x) x;
#else
#define _CLOG(x) ;
#endif

#ifdef CLOG_LEAKS
#define _CLOG_LEAKS(X) X
#else
#define _CLOG_LEAKS(X)
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
            PrintRawString(*str,14);
    }else{
        log::out<<log::RED<<"null"<<log::SILVER;
    }
}
Number* Context::getNumber(uint32 index){
    if(numbers.used<=index) return 0;
    uint8* info = (uint8*)infoValues.data + index;
    if((*info & REF_NUMBER) == 0) return 0; 
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
    for(int i=(int)numbers.used-1;i>-1;i--){
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
    numberCount++;
    return index;
}
void Context::deleteNumber(uint32 index){
    uint8* info = (uint8*)infoValues.data + index;
    if((*info) & REF_NUMBER){
        *info = (*info) & (~REF_NUMBER);
        numberCount--;

        if(index+1==numbers.used){
            numbers.used--;
            while(numbers.used>0){
                uint8* info = (uint8*)infoValues.data + numbers.used-1;
                if(((*info)&REF_NUMBER))
                    break;
                numbers.used--;
            }
        }
    }else{
        // engone::log::out << engone::log::YELLOW<<"Cannot delete number at "<<index<<"\n";
    }
}
String* Context::getString(uint32 index){
    if(strings.used<=index) return 0;
    uint8* info = (uint8*)infoValues.data + index;
    if((*info & REF_STRING) == 0) return 0; 
    return (String*)strings.data + index;
}
uint32 Context::makeString(){
    using namespace engone;
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
    for(int i=(int)strings.used-1;i>-1;i--){
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
    stringCount++;
    // log::out << "string++\n";
    return index;
}
void Context::cleanup(){
    numbers.resize(0);
    for(uint i=0;i<strings.used;i++){
        String* str = (String*)strings.data+i;
        uint8* val = (uint8*)infoValues.data + i;
        if((*(val))&REF_STRING)
            str->memory.resize(0);
    }
    strings.resize(0);
    infoValues.resize(0);
    scopes.resize(0);
    valueStack.resize(0);
    // reseting like this is not very fail safe.
    // what if more variables are added which are managed
    // in functions like makeNumber, deleteNumber.
    // if you don't call those the variables won't reset
    // unless specified here. And you will forget to put them here
    // so call the functions instead of reseting memory like we do above.
    stringCount=0;
    numberCount=0;
}
void Context::deleteString(uint32 index){
    using namespace engone;
    uint8* info = (uint8*)infoValues.data + index;
    if((*info) & REF_STRING){
        *info = (*info) & (~REF_STRING);
        stringCount--;
        // log::out << "string--\n";
        
        String* str = (String*)strings.data + index;
        str->memory.resize(0);
        // Todo: Optimize by not resizing memory since it might be used later.
        //  Unnecessary to deallocate and then allocate again.

        if(index+1==strings.used){
            strings.used--;
            while(strings.used>0){
                uint8* info = (uint8*)infoValues.data + strings.used-1;
                if(((*info)&REF_STRING))
                    break;
                strings.used--;
            }
        }
    }else{
        // engone::log::out << engone::log::YELLOW<<"Cannot delete string at "<<index<<"\n";
    }
}
Scope* Context::getScope(uint index){
    return (Scope*)scopes.data + index;
}
void Context::enterScope(){
    // currentScope++;
    Scope* scope = getScope(currentScope);

    scope->references[REG_ZERO].type = REF_NUMBER;
    scope->references[REG_ZERO].index = makeNumber();

    scope->references[REG_FRAME_POINTER].type = REF_NUMBER;
    scope->references[REG_FRAME_POINTER].index = makeNumber();
}
void Context::exitScope(){
    Scope* scope = getScope(currentScope);

    deleteNumber(scope->references[REG_ZERO].index);
    deleteNumber(scope->references[REG_FRAME_POINTER].index);

    // currentScope--;
}
bool Context::ensureScopes(uint depth){
    if(scopes.max>depth)
        return true;
    if(!scopes.resize(depth+1))
        return false;
    while(scopes.used<=depth){
        Scope* scope = (Scope*)scopes.data+scopes.used;
        new(scope)Scope();
        scopes.used++;
    }
    return true;
}
void Context::execute(Bytecode& code, Performance* perf){
    using namespace engone;
    activeCode = code;
    log::out << log::BLUE<< "\n##   Execute   ##\n";
    int programCounter=0;

    // ProvideDefaultCalls(externalCalls);
    
    ensureScopes(1);

    #define CERR log::out << log::RED<< "ContextError "<<(nowProgramCounter)<<","<<inst
    #define CWARN log::out << log::YELLOW<< "ContextWarning "<<(nowProgramCounter)<<","<<inst
    
    #define LINST " "<<(programCounter-1)<<": "<<inst

    auto startTime = MeasureSeconds();

    int atLine=-1;

    enterScope();

    uint64 executedInsts = 0;
    uint64 executedLines = 0;
    while(true){
        if(executedInsts>=INST_LIMIT){
            // prevent infinite loop
            log::out << log::RED<<"REACHED SAFETY LIMIT. EXPECT INCOMPLETE CLEANUP\n";
            break;
        }
        if(activeCode.length() == programCounter)
            break;
        int nowProgramCounter=programCounter;
        Instruction inst = activeCode.get(programCounter++);
        if(inst.type==0){
            _CLOG(log::out << LINST<<", skipping null instruction\n");
            continue;
        }

        #ifdef USE_DEBUG_INFO
        Bytecode::DebugLine* debugLine = activeCode.getDebugLine(programCounter-1);
        if(debugLine&&debugLine->line!=atLine){
            atLine = debugLine->line;
            executedLines++;
            
            #ifdef PRINT_DEBUG_LINES
            _CLOG(log::out <<"\n";)
            log::out << *debugLine<<"\n";
            #endif
        }
        #endif

        executedInsts++;
        
        bool yes = ensureScopes(currentScope+1);
        if(!yes){
            log::out << log::RED<<"ContextError: Could not allocate scope\n";
            break;
        }
        Ref* references = getScope(currentScope)->references;

        switch(inst.type){
            case BC_AND:
            case BC_OR: {
                Ref& r0 = references[inst.reg0];
                Ref& r1 = references[inst.reg1];
                Ref& r2 = references[inst.reg2];

                bool left=false,right=false;
                if(r0.type==REF_NUMBER){
                    Number* v0 = getNumber(r0.index);
                    left = v0->value!=0;
                } else if(r0.type==REF_STRING){
                    String* v0 = getString(r0.index);
                    left = v0->memory.used!=0;
                }
                if(r1.type==REF_NUMBER){
                    Number* v1 = getNumber(r1.index);
                    right = v1->value!=0;
                } else if(r1.type==REF_STRING){
                    String* v1 = getString(r1.index);
                    right = v1->memory.used!=0;
                }

                bool out=false;
                if(inst.type==BC_AND){
                    out = left && right;
                }else if(inst.type==BC_OR){
                    out = left || right;
                }

                if(out){
                    _CLOG(log::out << LINST <<",  "<<left<<"?"<<right<<" yes\n";)
                }else{
                    _CLOG(log::out << LINST <<", "<<left<<"?"<<right<<"  no\n";)
                }
                
                if(r2.type==REF_NUMBER){
                    Number* v = getNumber(r2.index);
                    v->value = out;
                } else if(r2.type==REF_STRING){
                    String* v = getString(r2.index);
                    *v = out ? "1" : "";
                }

                break;
            }
            case BC_ADD:
            case BC_SUB:
            case BC_MUL:
            case BC_DIV:
            case BC_LESS:
            case BC_GREATER:
            case BC_EQUAL:
            case BC_NOT_EQUAL:{
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
                    else if(inst.type==BC_NOT_EQUAL)
                        num = v0->value != v1->value;
                    else if(inst.type==BC_AND)
                        num = v0->value!=0 && v1->value!=0;
                    else if(inst.type==BC_OR)
                        num = v0->value!=0 || v1->value!=0;
                    else {
                        CERR << ", inst. not available for "<<RefToString(r0.type)<<"\n";   
                    }
                    _CLOG(log::out << LINST<< ", "<<v0->value<<" ";
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
                         else if(inst.type==BC_NOT_EQUAL)
                        log::out <<"!=";
                         else if(inst.type==BC_AND)
                        log::out <<"&&";
                         else if(inst.type==BC_OR)
                        log::out <<"||";
                    else {
                        CERR <<", BUG AT "<<__FILE__<<":"<<__LINE__<<"\n";   
                    })
                    
                    _CLOG(log::out << " ";PrintRefValue(this,r1);log::out<<" = "<<num<<"\n";)
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
                        _CLOG(log::out << LINST <<", \"";PrintRefValue(this,r2);log::out<<"\"\n";)
                    } else if(inst.type==BC_EQUAL){
                        if(v0->memory.used!=v1->memory.used){
                            v2->memory.used=0;
                            _CLOG(log::out << LINST <<",  no\n";)
                            continue;
                        }
                        if(0!=strncmp((char*)v0->memory.data,(char*)v1->memory.data,v0->memory.used)){
                            v2->memory.used=0;
                            _CLOG(log::out << LINST <<",  no\n";)
                            continue;
                        }
                        *v2 = "1";

                        _CLOG(log::out << LINST <<",  yes\n";)
                    }  else if(inst.type==BC_NOT_EQUAL){
                        if(v0->memory.used!=v1->memory.used){
                            *v2 = "1";
                            _CLOG(log::out << LINST <<",  yes\n";)
                            continue;
                        }
                        if(0!=strncmp((char*)v0->memory.data,(char*)v1->memory.data,v0->memory.used)){
                            *v2 = "1";
                            _CLOG(log::out << LINST <<",  yes\n";)
                            continue;
                        }
                        *v2 = "";

                        _CLOG(log::out << LINST <<",  no\n";)
                    }else {
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
                        int written = 0;
                        if(v1->value!=(Decimal)(int64)v1->value)
                            written = sprintf((char*)v2->memory.data+v0->memory.used,"%lf",v1->value);
                        else
                            written = sprintf((char*)v2->memory.data+v0->memory.used,"%lld",(int64)v1->value);
                        if(written>numlength){
                            CERR << ", double turned into "<<written<<" chars (max "<<numlength<<")\n";
                            continue;
                        }
                        v2->memory.used = v0->memory.used + written;
                        _CLOG(log::out << LINST <<", \"";PrintRefValue(this,r2);log::out<<"\"\n";)
                    } else {
                        CERR<<", inst. not available for "<<RefToString(r0.type)<<"\n";   
                    }
                }
                else{
                    CERR<<", invalid types "<<
                        RefToString(r0.type)<<" "<<RefToString(r1.type)<<" "<<RefToString(r2.type)<<" in registers\n";   
                }
                break;
            }
            case BC_SUBSTR: {
                Ref& r0 = references[inst.reg0];
                Ref& r1 = references[inst.reg1];
                Ref& r2 = references[inst.reg2];

                if(r0.type==REF_STRING && r1.type == REF_NUMBER && r2.type == REF_NUMBER){
                    String* v0 = getString(r0.index);
                    Number* v1 = getNumber(r1.index);
                    Number* v2 = getNumber(r2.index);

                    if(!v0||!v1||!v2){
                        CERR <<", one value were null ";PrintRefValue(this,r0);log::out<<" ";
                            PrintRefValue(this,r1);log::out<<" ";PrintRefValue(this,r2);log::out<<"\n";   
                        continue;
                    }
                    
                    if(v1->value>v2->value){
                        CERR << ", r1 <= r2 was false ("<<v1->value<<" <= "<<v2->value<<")\n";
                        continue;
                    }
                    if(0>v1->value){
                        CERR << ", 0 <= r1 was false  (0 <= "<<v1->value<<")\n";
                        continue;
                    }
                    if(v2->value >= v0->memory.used){
                        CERR << ", r2 < r0.length was false  ("<<v2->value<<" < "<<v0->memory.used<<")\n";
                        continue;
                    }
                    
                    // Todo: handle decimals in v1, v2 by throwing error
                    int index0 = v1->value;
                    int index1 = v2->value;
                    int length = v2->value - v1->value + 1; // +1 to be inclusive, "hej"[0:0] = "h"

                    int outIndex = makeString();
                    String* out = getString(outIndex);
                    if(!out){
                        CERR <<", out string could not be allocated\n";
                        continue;
                    }
                    if(!out->memory.resize(length)){
                        CERR <<", out string could not be allocated\n";
                        continue;
                    }

                    memcpy(out->memory.data,(char*)v0->memory.data+index0,length);
                    out->memory.used = length;

                    r0.index = outIndex;
                    
                    _CLOG(log::out _CLOG_LEAKS(<<log::AQUA) << LINST <<", substr '"<<*out<<"'\n";)
                    // now the first register contains the substring of the original
                }else{
                    CERR<<", invalid types "<<
                        RefToString(r0.type)<<" "<<RefToString(r1.type)<<" "<<RefToString(r2.type)<<" in registers\n";   
                }
                break;
            }
            case BC_TYPE: {
                Ref& r0 = references[inst.reg0];
                Ref& r1 = references[inst.reg1];

                if(r1.type==REF_STRING){
                    String* v1 = getString(r1.index);

                    if(!v1){
                        CERR <<", r1 was null ";PrintRefValue(this,r1);log::out<<"\n";   
                        continue;
                    }

                    const char* type = 0;
                    int length = 0;
                    if(r0.type==REF_STRING){
                        type = "string";
                    } else if(r0.type==REF_NUMBER){
                        type = "number";
                    } else
                        type = "null";
                    length = strlen(type);
                    
                    if(!v1->memory.resize(length)){
                        CERR << ", failed resizing\n";
                        continue;
                    }
                    memcpy(v1->memory.data,type,length);
                    v1->memory.used = length;
                    _CLOG(log::out << LINST <<", type '"<<*v1<<"'\n";)
                }else{
                    CERR<<", invalid type "<<RefToString(r1.type)<<" in r1\n";   
                }
                break;
            }
             case BC_LEN: {
                Ref& r0 = references[inst.reg0];
                Ref& r1 = references[inst.reg1];

                if(r0.type==REF_STRING && r1.type==REF_NUMBER){
                    String* v0 = getString(r0.index);
                    Number* v1 = getNumber(r1.index);

                    if(!v0||!v1){
                        CERR <<", r1 was null ";PrintRefValue(this,r1);log::out<<"\n";   
                        continue;
                    }
                    v1->value = v0->memory.used;
                    _CLOG(log::out << LINST <<", len "<<v1->value<<"\n";)
                }else{
                    CERR<<", invalid types "<<RefToString(r0.type)<<", "<<RefToString(r1.type)<<" in r0, r1\n";   
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
                switch(r0.type){
                    case REF_STRING:
                    {
                        r1.type = r0.type;
                        int tempIndex = r0.index;
                        r1.index = makeString();
                        // Todo: check if makeString failed
                        
                        String* v0 = getString(tempIndex);
                        String* v1 = getString(r1.index);
                        if(!v0||!v1){
                            CERR << ", values are null\n";
                        }else{
                            v0->copy(v1);
                            _CLOG(log::out _CLOG_LEAKS(<< log::AQUA)<< LINST <<" ["<<r1.index<<"], copied ";PrintRefValue(this,r1);log::out<<"\n";)  
                        }
                        break;
                    }
                    case REF_NUMBER:
                    {
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
                            _CLOG(log::out _CLOG_LEAKS(<<log::AQUA) << LINST <<" ["<<r1.index<<"], copied "<< n1->value <<"\n";)
                        }
                        break;
                    }
                    default:
                        r1 = r0;
                        _CLOG(log::out << LINST <<" ["<<r1.index<<"], copied null\n";)  
                        // CERR<< ", cannot copy "<<RefToString(r0.type)<<"\n";
                        break;
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
                
                _CLOG(log::out << LINST <<", moved ";)
                if(r0.type==REF_NUMBER){
                    Number* v0 = getNumber(r0.index);
                    if(v0)
                        _CLOG(log::out << v0->value)
                    else
                        _CLOG(log::out << "?")

                } else if(r0.type==REF_STRING){
                    String* v0 = getString(r0.index);
                    if(v0)
                        _CLOG(log::out << *v0)
                    else
                        _CLOG(log::out << "?")

                } else {
                    _CLOG(log::out << "?";)
                }
                _CLOG(log::out<<"\n";)
                break;   
            }
            case BC_NUM: {
                Ref& r0 = references[inst.reg0];
                
                r0.type = REF_NUMBER;
                r0.index = makeNumber();
                
                _CLOG(log::out _CLOG_LEAKS(<< log::AQUA)<< LINST <<" ["<<r0.index<<"]\n";)
                
                break;
            }
            case BC_STR: {
                Ref& r0 = references[inst.reg0];
                
                r0.type = REF_STRING;
                r0.index = makeString();
                
                _CLOG(log::out _CLOG_LEAKS(<< log::AQUA)<< LINST <<" ["<<r0.index<<"]\n";)
                
                break;
            }
            case BC_DEL: {
                Ref& r0 = references[inst.reg0];
                
                if(r0.type==REF_NUMBER){
                    deleteNumber(r0.index);
                }else if(r0.type==REF_STRING){
                    deleteString(r0.index);
                }else if(r0.type==0&&r0.index==0){
                    CWARN << ", cannot delete "<<RefToString(r0.type)<<"\n";
                }else{
                    CERR << ", invalid type "<<RefToString(r0.type)<<" in registers\n";
                    continue;
                }
                _CLOG(log::out _CLOG_LEAKS(<< log::PURPLE)<< LINST <<" ["<<r0.index<<"]\n";)
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
                    //     LINST << ", ";
                    //     PrintRef(this,LOAD_CONST_REG);
                    //     log::out << " = "<< unresolved->name <<" (unresolved)\n";
                    // }else{
                    Number* num = activeCode.getConstNumber(extraData);
                    if(!num){
                        CERR << ", number constant at "<< extraData<<" does not exist\n";   
                        continue;
                    }
                    n0->value = num->value;
                    _CLOG(log::out << LINST << ", reg = " << n0->value<<"\n";)
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
                    _CLOG(log::out << LINST << ", reg = ";PrintRawString(*v0);log::out<<"\n";)
                    // PrintRef(this,inst.reg0);
                    // log::out << " = \""; PrintRawString(*v0); log::out<<"\"\n";
                }else{
                    CERR << ", invalid type "<<RefToString(r0.type)<<" in registers\n";   
                }
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
                    CERR << ", $fp + $r1 cannot be negative ($fp + $r1 = "<<index<<")\n";
                    continue;
                }
                if((uint)index>=valueStack.max){
                    CERR << ", $fp + $r1 goes beyond the allocated stack ($fp + $r1 = "<<index<<" >= "<<valueStack.max<<")\n";
                    continue;
                }
                Ref* ref = (Ref*)valueStack.data+index;
                r0 = *ref;
                _CLOG(log::out << LINST << ", loaded '";PrintRefValue(this,r0);log::out<<"' from "<<index<<"\n";)
                break;
            }
            case BC_STOREV:{
                Ref& r0 = references[inst.reg0];
                int frameOffset = (uint)inst.reg1 | ((uint)inst.reg2<<8);
                Ref& fp = references[REG_FRAME_POINTER];

                if(fp.type != REF_NUMBER){
                    CERR << ", $fp must be a number\n";
                    continue;
                }
                Number* framePointer = getNumber(fp.index);
                if(!framePointer){
                    CERR << ", $fp cannot be null\n";
                    continue;
                }
                if(framePointer->value!=(Decimal)(uint)framePointer->value){
                    CERR << ", $fp cannot have decimals ($sp = "<<framePointer->value<<")\n";
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
                int index = framePointer->value + frameOffset;
                if(index<0){
                    CERR << ", $fp + $r1 cannot be negative ($fp + $r1 = "<<index<<")\n";
                    continue;
                }
                Ref* ref = 0;
                if((uint)index<valueStack.max){
                    ref = (Ref*)valueStack.data+index;
                    // CERR << ", $fp + $r1 goes beyond the allocated stack ($fp + $r1 = "<<index<<" >= "<<valueStack.max<<")\n";
                    // continue;

                    if(ref->index==r0.index && ref->type==r0.type){
                        continue;
                    }
                    // delete previous value
                    if(ref->type==REF_NUMBER){
                        deleteNumber(ref->index);
                    }else if(ref->type==REF_STRING){
                        deleteString(ref->index);
                    }
                }else{
                    int oldMax = valueStack.max;
                    if(!valueStack.resize(valueStack.max+index+10)){
                        CERR << ", stack allocation failed\n";
                        continue;
                    }
                    // zero-initalize the new values. Random memory could indicate
                    for(int i=oldMax;i<(int)valueStack.max;i++){
                        *((Ref*)valueStack.data+i) = {};
                    }
                    ref = (Ref*)valueStack.data+index;
                }
                *ref = r0;
                _CLOG(log::out << LINST << ", stored '";PrintRefValue(this,r0);log::out<<"' at "<<index<<"\n";)
                break;
            }
            case BC_JUMP: {
                Ref& r0 = references[inst.reg0];
                
                if(r0.type==REF_NUMBER){
                    Number* n0 = getNumber(r0.index);
                    int address = n0->value;
                
                    // note that address is refers to the instruction position/index and not the byte memory address.
                    if(n0->value != (Decimal)(uint)n0->value){
                        CERR<< ", decimal in register not allowed ("<<n0->value<<"!="<<((Decimal)(uint)n0->value)<<")\n";   
                    } else if(activeCode.length()<address){
                        CERR << ", invalid address "<<address<<" (max "<<activeCode.length()<<")\n";   
                    }else if(programCounter == address){
                        _CLOG(log::out << "ContextWarning at "<<programCounter<<", " << inst << ", jumping to next instruction is redundant\n";)
                    }else{
                        _CLOG(log::out << LINST << ", jumped to "<<address<<"\n";)
                        programCounter = address;
                    }
                }else{
                    CERR << ", invalid type "<<RefToString(r0.type)<<" in registers\n";   
                }
                break;
            }
            case BC_JUMPNIF: {
                Ref& r0 = references[inst.reg0];
                Ref& r1 = references[inst.reg1];
                
                if(r1.type==REF_NUMBER){
                    Number* n1 = getNumber(r1.index);
                    int address = n1->value;
                    
                    // note that address is refers to the instruction position/index and not the byte memory address.
                    if(n1->value != (Decimal)(uint)n1->value){
                        CERR<< ", decimal in register not allowed ("<<n1->value<<"!="<<((Decimal)(uint)n1->value)<<")\n";
                        continue;
                    } else if(activeCode.length()<address){
                        CERR << ", invalid address "<<address<<" (max "<<activeCode.length()<<")\n";   
                        continue;
                    }else if(programCounter == address){
                        CWARN << ", jumping to next instruction is redundant\n";   
                        continue;
                    }
                    
                    if(r0.type==REF_NUMBER||r0.type==REF_STRING){
                        bool willJump = false;
                        if(r0.type==REF_NUMBER){
                            Number* n0 = getNumber(r0.index);
                            if(!n0){
                                CERR << ", numbers where null\n";  
                                continue;   
                            }
                            willJump = n0->value == 0;
                        }else if(r0.type==REF_STRING){
                            String* n0 = getString(r0.index);
                            willJump = n0->memory.used==0;
                        }
                        if(willJump){
                            _CLOG(log::out << LINST << ", jumped to "<<address<<"\n";)
                            programCounter = address;
                        }else{
                            _CLOG(log::out << LINST << ", no jumping\n";)
                        }
                    } else{
                        CERR << ", invalid type "<<RefToString(r0.type)<<" "<<RefToString(r1.type)<<"\n";  
                        continue;
                    }
                }else{
                    CERR << ", invalid type "<<RefToString(r1.type)<<" in r1 (should be number)\n";   
                }
                break;
            } 
            // case BC_ENTERSCOPE: {
            //     // bool yes = ensureScopes(currentScope+1);
            //     if(yes){
            //         currentScope++;
            //         _CLOG(log::out << LINST << "\n";)
            //     }else{
            //         CERR << ", could not ensure scope (allocation failure)\n";   
            //     }
            //     break;
            // } case BC_EXITSCOPE: {
            //     if(currentScope>0){
            //         currentScope--;
            //         _CLOG(log::out << LINST << "\n";)
            //     }else{
            //         CERR << ", cannot exit at global scope (currentScope == "<<currentScope<<")\n";   
            //     }
            //     break;
            // }
            case BC_RETURN: {
                Ref& r0 = references[inst.reg0];
                
                if(currentScope<=0){
                    CERR << ", cannot return from global scope (currentScope == "<<currentScope<<")\n";   
                    continue;
                }else{
                    programCounter = getScope(currentScope)->returnAddress;
                    exitScope();
                    currentScope--;
                    Ref* refs = getScope(currentScope)->references;
                    
                    Ref& rv = refs[REG_RETURN_VALUE];
                    
                    // move return value to scope above
                    rv = r0;
                    
                    // jump to address
                    // Ref& ra = refs[REG_RETURN_ADDR];
                    // if(ra.type!=REF_NUMBER){
                    //     CERR<<", $ra must be a number (was ";
                    //     PrintRefValue(this,ra);
                    //     log::out << ")\n";
                    //     continue;
                    // }
                    // Number* n0 = getNumber(ra.index);
                    // if(!n0){
                    //     CERR<<", value was null\n";
                    //     continue;
                    // }
                    // Todo: error decimal number
                    // programCounter = n0->value;
                    _CLOG(log::out << LINST << ", jumped to "<<programCounter<<", returned ";
                    PrintRefValue(this,rv);
                    log::out << "\n";)
                    
                }
                break;
            }
            case BC_TEST: {
                Ref& r0 = references[inst.reg0];
                TestValue testValue{r0.type};
                if(r0.type==REF_STRING){
                    String* v0 = getString(r0.index);
                    testValue.string = *v0;
                }else if(r0.type==REF_NUMBER){
                    Number* v0 = getNumber(r0.index);
                    testValue.number = *v0;
                }
                testValues.push_back(testValue);
                log::out << log::MAGENTA<<"_test_ ";
                PrintRefValue(this,r0);
                log::out <<"\n";
                break;
            }
            case BC_WRITE_FILE:
            case BC_APPEND_FILE:
            case BC_READ_FILE: {
                Ref& r0 = references[inst.reg0];
                Ref& r1 = references[inst.reg1];
                
                // write works with number and string, read doesn't
                if(inst.type==BC_READ_FILE&&r0.type!=REF_STRING){
                    CERR << ", r0 must be a string not "<<RefToString(r0.type)<<"\n";
                    return;
                }
                if(r1.type!=REF_STRING){
                    CERR << ", r1 must be a string not "<<RefToString(r1.type)<<"\n";
                    return;
                }
                
                String* v1 = getString(r1.index);
                if(!v1){
                    CERR << ", r1 must be a string not "<<RefToString(r1.type)<<"\n";
                    continue;   
                }
                // Todo: opening and closing files frequently is inefficient.
                //  If we could maintain the file for a few milliseconds and if file hasn't been
                //  accessed, THEN we can close it.
                
                uint64 fileSize=0;
                APIFile* file=0;
                
                // Block .cpp and .h in case you mess up and the compiler is overwritten with garbage
                if(v1->memory.used>4 && inst.type!=BC_READ_FILE){
                    const char* forbidden[]{".cpp",".h",".bat",".btb"};
                    bool bad = false;
                    int i=0;
                    for(;i<(int)(sizeof(forbidden)/sizeof(*forbidden));i++){
                        const char* str = forbidden[i];
                        int len = strlen(str);
                        if(strncmp((const char*)v1->memory.data+v1->memory.used-len,str,len)==0){
                            bad=true;
                            break;
                        }
                    }
                    if(bad){
                        log::out << log::YELLOW << LINST << ", blocked: '"<<*v1<<"' ('"<<forbidden[i]<<"' is forbidden)\n";
                        continue;
                    }
                }
                #define CHECK_V0 if(!v0){CERR << ", r0 must be a string not "<<RefToString(r0.type)<<"\n";continue;}
                // Todo: handle errors and make the code neater
                if(inst.type==BC_WRITE_FILE){
                    uint64 written=0;
                    if(r0.type==REF_STRING){
                        String* v0 = getString(r0.index);
                        CHECK_V0
                        file = FileOpen(*v1,&fileSize,FILE_WILL_CREATE);
                        written = FileWrite(file,v0->memory.data,v0->memory.used);
                    }else if(r0.type==REF_NUMBER){
                        Number* v0 = getNumber(r0.index);
                        CHECK_V0
                        char buf[100];
                        written = sprintf(buf,"%lf",v0->value);
                        file = FileOpen(*v1,&fileSize,FILE_WILL_CREATE);
                        written = FileWrite(file,buf,written);
                    }
                }else if(inst.type==BC_APPEND_FILE){
                    uint64 written=0;
                    if(r0.type==REF_STRING){
                        String* v0 = getString(r0.index);
                        CHECK_V0
                        file = FileOpen(*v1,&fileSize,FILE_CAN_CREATE);
                        bool err = FileSetHead(file,fileSize);
                        written = FileWrite(file,v0->memory.data,v0->memory.used);
                    }else if(r0.type==REF_NUMBER){
                        Number* v0 = getNumber(r0.index);
                        CHECK_V0
                        char buf[100];
                        written = sprintf(buf,"%lf",v0->value);
                        file = FileOpen(*v1,&fileSize,FILE_CAN_CREATE);
                        bool err = FileSetHead(file,fileSize);
                        written = FileWrite(file,buf,written);
                    }
                }else if(inst.type==BC_READ_FILE){
                    String* v0 = getString(r0.index);
                    CHECK_V0
                    file = FileOpen(*v1,&fileSize,FILE_ONLY_READ);
                    // Todo: don't read large files (500 MB)? Maybe use a stream instruction?
                    //   May
                    // log::out << "FILESIZE: "<<fileSize<<"\n";
                    bool yes = v0->memory.resize(fileSize);
                    if(yes){
                        uint64 read = FileRead(file,v0->memory.data,fileSize);
                        v0->memory.used = read;
                        // log::out << "Read bytes: "<<read<<"\n";
                    }
                }
                FileClose(file);
                
                _CLOG(log::out << LINST << ", from: '"<<*v1<<"' content: '";PrintRefValue(this,r0);log::out <<"'\n";)
                break;
            } case BC_RUN: {
                Ref& r0 = references[inst.reg0];
                Ref& r1 = references[inst.reg1];
                
                if(r1.type==REF_NUMBER){
                    Ref* refs = getScope(currentScope+1)->references;
                    Ref& rz = refs[REG_ARGUMENT];
                    Number* n1 = getNumber(r1.index);
                    if(!n1){
                        CERR << ", $r1 is null\n";
                        continue;
                    }
                    if(n1->value!=(Decimal)(uint)n1->value){
                        CERR << ", $r1 cannot be decimal ("<<n1->value<<")\n";
                        continue;
                    }
                    currentScope++;
                    enterScope();
                    // na->value = programCounter;
                    getScope(currentScope)->returnAddress=programCounter;
                    rz = r0;
                    
                    // check if rz is number
                    _CLOG(log::out << LINST << ", arg: ";
                    PrintRefValue(this,rz);
                    log::out<<", jump to "<<n1->value<<"\n";)

                    programCounter = n1->value;
                }else if(r1.type==REF_STRING){
                    String* v1 = getString(r1.index);

                    std::string name = *v1;

                    // auto find = externalCalls.map.find(name);
                    auto func = GetExternalCall(name);
                    // if(find!=externalCalls.map.end()){
                    if(func){
                        void* arg=0;
                        if(r0.type==REF_STRING)
                            arg = getString(r0.index);
                        else if(r0.type==REF_NUMBER)
                            arg = getNumber(r0.index);
                        else{
                            // CERR << ", unresolved function does not allow "<<RefToString(r0.type)<<" as argument\n";
                            // continue;
                        }
                        _CLOG(log::out _CLOG_LEAKS(<< log::AQUA)<< LINST << ", external: '"<<name<<"' arg: '";
                        PrintRefValue(this,r0); log::out<<"'\n";)
                        // if(find->second){
                            // Ref returnValue = find->second(this,r0.type,arg);
                            Ref returnValue = func(this,r0.type,arg);
                            
                            // if(returnValue.type!=0){
                            Ref& rv = references[REG_RETURN_VALUE];
                            rv = returnValue;
                            // }else{
                                   
                            // }
                        // }else{
                        //     CERR << ", api call was null\n";
                        // }
                    }else{
                        // std::string foundPath="";
                        
                        // std::string temp=name;
                        // if(temp.find("\"")==0)
                        //     temp = temp.substr(1);
                        // if(temp.find_last_of("\"")==temp.length()-1)
                        //     temp = temp.substr(0,temp.length()-1);
                        
                        // if(FileExist(temp)){
                        //     foundPath = name;
                        // }else{
                        //     std::string paths = EnvironmentVariable("PATH");
                        //     uint at=0;
                        //     const char* str=paths.data();
                        //     bool found=false;
                        //     while(true){
                        //         if(at>=paths.length())
                        //             break;
                        //         uint end = paths.find(";",at);
                        //         if(at>end) {
                        //             at = end +1;
                        //             continue;
                        //         }
                        //         if(end==(uint)-1)
                        //             end = paths.length();
                        //         std::string view = paths.substr(at,end-at);
                        //         // log::out << view.data()<<"\n";
                        //         if(view.length()==0)
                        //             break;
                        //         // ((char*)view.data())[end] = 0;
                        //         at = end+1;
                                
                        //         std::string exepath = "";
                        //         exepath += view;
                        //         exepath += "\\";
                        //         exepath += name;
                        //         if(FileExist(exepath)){
                        //             foundPath = exepath;
                        //             break;
                        //         }
                        //     }
                        // }
                        // if(foundPath.empty()){
                        //     CERR << ", "<<name<<" could not be found\n";
                        // }else{
                            // std::string finaltemp=std::move(foundPath);
                            // ReplaceChar((char*)finaltemp.data(),finaltemp.length(),'/','\\');
                            
                            int index = name.find("cmd");
                            std::string finaltemp;
                            if(index==0){
                                // Todo: what if name has /k and stuff?
                                if(name.length()>strlen("strlen"))
                                if(name[4])
                                finaltemp = "cmd.exe /K " + name.substr(3);
                            }else{
                                finaltemp = name;
                            }
                            if(r0.type==REF_NUMBER){
                                Number* v0 = getNumber(r0.index);
                                if(v0){
                                    finaltemp += " ";
                                    finaltemp += std::to_string(v0->value);
                                }
                            }else if(r0.type==REF_STRING){
                                String* v0 = getString(r0.index);
                                finaltemp += " ";
                                finaltemp += *v0;
                            }
                            _CLOG(log::out _CLOG_LEAKS(<< log::AQUA)<< LINST << ", exe: '"<<name<<"' arg: '";
                            PrintRefValue(this,r0); log::out<<"'\n";)
                            int exitCode = 0;
                            int flags = PROGRAM_WAIT;
                            
                            
                            log::out << "["<<finaltemp<<"]\n";
                            bool yes = StartProgram("",(char*)finaltemp.data(),flags,&exitCode);
                            if(!yes){
                                CERR << ", "<<name<<" found but cannot start\n";
                            }else{
                                Ref& rv = references[REG_RETURN_VALUE];
                                uint index = makeNumber();
                                if(index!=(uint)-1){
                                    rv.type = REF_NUMBER;
                                    rv.index = index;
                                    Number* rv = getNumber(index);
                                    rv->value = exitCode;
                                }
                            }
                        // }
                    }
                }else{
                     CERR << ", bad registers ";PrintRefValue(this,r0);log::out <<", ";PrintRefValue(this,r1);log::out<<"\n";
                }
                break;
            }
            default:{
                log::out << log::RED<<inst << " Not implemented\n";
            }
        }
    }
    exitScope();

    char temp[30];
    auto formatUnit = [&](double number){
        if(number<1000) sprintf(temp,"%llu",(uint64)number);
        else if(number<1e6) sprintf(temp,"%.2lf K",number/1000.f);
        else if(number<1e9) sprintf(temp,"%.2lf M",number/1e6);
        else sprintf(temp,"%.2lf G",number/1e9);
    };

    bool summary=true;    
    // bool summary=false;

    double executionTime = StopMeasure(startTime);
    
    if(perf){
        perf->instructions = executedInsts;
        perf->runtime = executionTime;   
    }
    if(summary){
        double nsPerInst = executionTime/executedInsts*1e9;
        double nsPerLine = executionTime/executedLines*1e9;
        // Todo: note that APICalls and executables are included. When calling those
        //  you can measure the time in those functions and subtract it from instruction time.
        log::out << log::BLUE<<"##   Summary   ##\n";
        
        // Note: In reality executedLines stands for how often execution switched to a different line.
        // It doesn't represent how many complex lines were executed.
        formatUnit(executedLines);
        log::out << " "<<temp<<" lines in "<<executionTime<<" seconds (avg "<<nsPerLine<<" ns/line)\n";
        
        formatUnit(executedInsts);
        log::out << " "<< temp<<" instructions in "<<executionTime<<" seconds (avg "<<nsPerInst<<" ns/inst)\n";
        
        double instPerS = executedInsts/executionTime;
        formatUnit(instPerS);
        log::out << " "<< temp<<" instructions per second ("<<temp<<"Hz)\n";
        
        // double target = 3e9;
        // log::out.flush(); printf(" %d",(int)(target/instPerS)); log::out<<" times slower than 3 GHz ("<<temp<<"Hz / 3 GHz)\n";
    }
    if(numberCount!=0||stringCount!=0){
        log::out << log::YELLOW<<"Context finished with "<<numberCount << " numbers and "<<stringCount << " strings (n.used "<<numbers.used<<", s.used "<<strings.used<<")\n";
    }
    // auto tp = MeasureSeconds();
    // int sum = 0;
    // int i=0;
    // int j=0;
    // int N=1000;
    // while(i<N){
    //     sum += i;
    //     j=0;
    //     while(j<N-i){
    //         sum += j;
    //         j++;
    //     }
    //     i++;
    // }
    // auto sec = StopMeasure(tp);
    // log::out << "Res "<<sum<<" "<<(sec*1000000)<<" us\n";
}
void Context::Execute(Bytecode& code, Performance* perf){
    Context context{};
    context.execute(code,perf);
    context.cleanup();
}