#pragma once
/*
    Signals in this case refers to the return values from
    parse, check, and generate functions. The returned value
    indicates success or an error which may need to be handled
    in a special way.

    The purpose of the signals is to allow functions to communicate
    with the caller about what happened and if the caller needs
    to do something special.
*/

#include "Engone/Typedefs.h"

enum class SignalDefault : u32 {
    SUCCESS = 0,
    FAILURE, // general failure
    COMPLETE_FAILURE, // really bad failure, you have to stop right away
};
enum class SignalAttempt : u32 {
    SUCCESS = 0,
    FAILURE, // general failure
    COMPLETE_FAILURE, // really bad failure, you have to stop right away
    BAD_ATTEMPT, // the function you called couldn't do what you wanted but it
    // didn't modify data and you can try another function
};
enum class SignalDetailed : u32 {
    SUCCESS = 0,
    FAILURE, // general failure
    COMPLETE_FAILURE, // really bad failure, you have to stop right away
    // UNKNOWN_TYPE,
    // INVALID_TYPE,
};

SignalDefault CastSignal(SignalAttempt signal);

// used to catch mistakes
bool operator==(SignalDefault, SignalDefault);
bool operator==(SignalAttempt, SignalAttempt);

// these are meant to be passed to functions but
// it might be a bad idea because not all functions will
// handle these cases. You could pass a flag that isn't handled
// by mistake and thinking it does but it doesn't and then 
// something bad happens
// enum class SignalFlags : u32 {
//     ATTEMPT,
// };