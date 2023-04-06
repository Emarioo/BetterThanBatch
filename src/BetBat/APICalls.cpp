#include "BetBat/APICalls.h"

#include "BetBat/Context.h"


Ref APIPrint(Context* context, int refType, void* value){
    if(refType==REF_STRING){
        String* str = (String*)value;
        engone::log::out <<*str;
    }else if(refType==REF_NUMBER){
        Number* num = (Number*)value;
        engone::log::out <<num->value;
    }else {
        // nothing?   
    }
    engone::log::out<<"\n";
    int index = context->makeNumber();
    context->getNumber(index)->value=1;
    return {REF_NUMBER,index};
    // return {};
}
Ref APITime(Context* context, int refType, void* value){
    using namespace engone;
    // Arguments are ignored
    if(refType==0||!value){
        int index = context->makeNumber();
        context->getNumber(index)->value=engone::MeasureSeconds();
        return {REF_NUMBER,index};
    }else if(refType==REF_NUMBER) {
        Number* num = (Number*)value;
        int index = context->makeNumber();
        context->getNumber(index)->value=engone::StopMeasure(num->value);
        return {REF_NUMBER,index};
    }else if(refType==REF_STRING){
        String* str = (String*)value;
        static char* buffer=0;
        static const int BUFFER_SIZE=50;
        if(!buffer){
            buffer=(char*)malloc(BUFFER_SIZE+1);
            if(!buffer){
                log::out << log::RED<<"APITime: Alloc failed\n";
                return {};
            }
        }
        if(BUFFER_SIZE<str->memory.used){
            log::out <<log::RED<< "APITime: Buffer to small\n";
            return {};
        }
        memcpy(buffer,str->memory.data,str->memory.used);
        buffer[str->memory.used]=0;
        double number = atof(buffer);
        int index = context->makeNumber();
        context->getNumber(index)->value=engone::StopMeasure(number);
        return {REF_NUMBER,index};
    }else{
        log::out << "APITime: Expected Number or null as argument (got "<<RefToString(refType)<<")\n";
        return {};
    }
}