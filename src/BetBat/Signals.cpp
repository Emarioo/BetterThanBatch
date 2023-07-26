#include "BetBat/Signals.h"

#include "Engone/PlatformLayer.h"


SignalDefault CastSignal(SignalAttempt signal){
    #define CASE(A) case SignalAttempt::A: return SignalDefault::A;
    switch(signal){
        CASE(SUCCESS)
        CASE(FAILURE)
        CASE(COMPLETE_FAILURE)
    }
    Assert(("Cannot cast signal",false));
    return SignalDefault::COMPLETE_FAILURE;
    #undef CASE
}

bool operator==(SignalDefault a, SignalDefault b){
    // checking result == FAILURE will not include
    // COMPLETE_FAILURE, if you want to check for failure
    // use result != SUCCESS
    Assert(b != SignalDefault::FAILURE);
    return (u32)a == (u32)b;
}
bool operator==(SignalAttempt a, SignalAttempt b){
    Assert(b != SignalAttempt::FAILURE);
    return (u32)a == (u32)b;
}