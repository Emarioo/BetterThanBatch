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