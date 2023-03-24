#include "BetBat/APICalls.h"

#include "BetBat/Context.h"

void APIPrint(Context* context, int refType, void* value){
    if(refType==REF_STRING){
        String* str = (String*)value;
        engone::log::out << "PRINT: "<<*str;
    }else if(refType==REF_NUMBER){
        Number* num = (Number*)value;
        engone::log::out <<"PRINT: " <<num->value;
    }else {
        // nothing?   
    }
}