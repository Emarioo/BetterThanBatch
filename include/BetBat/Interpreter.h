#pragma once

#include "BetBat/Parser.h"
#include "BetBat/ExternalCalls.h"

const char* RefToString(int type);
// engone::Logger operator<<(engone::Logger logger, Ref& ref);
// A function scope. StackFrame? not if, for or while scope
struct Scope{
    Ref references[256]{0};
    
    int returnAddress=0;
};
struct Performance {
    int instructions=0;
    double exectime=0;
};
struct UserThread {
    enum State : uint {
        Inactive=0,
        Active,
        Finished, // thread finished but hasn't joined
        Waiting, // waiting for thread to finish or signal 
    };
    int programCounter=0;
    engone::Memory scopes{sizeof(Scope)};
    uint currentScope=0;
    engone::Memory valueStack{sizeof(Ref)}; // holds references to values
    
    Ref outRef{};
    
    State state=Inactive;
    int waitingFor = -1; // thread to wait for, relevant with Waiting
    int inRef = -1;

    // union for os/user thread?

    engone::Thread osThread{};
};

#define USE_INFO_VALUES

struct Context {
    engone::Memory numbers{sizeof(Number)};
    engone::Memory strings{sizeof(String)};
    #ifdef USE_INFO_VALUES
    engone::Memory infoValues{sizeof(uint8)};
    #else
    engone::Memory afreeNumbers{sizeof(int)};
    engone::Memory afreeStrings{sizeof(int)};
    #endif
    int numberCount=0;
    int stringCount=0;

    // engone::Memory scopes{sizeof(Scope)};
    // uint currentScope=0;
    // engone::Memory valueStack{sizeof(Ref)}; // holds references to values
    
    int currentThread = -1; // -1 indicates no thread
    engone::Memory userThreads{sizeof(UserThread)}; // holds references to values

    struct TestValue {
        int type=0;
        union {
            String string;
            Number number;
        };
    };
    std::vector<TestValue> testValues;

    Bytecode activeCode;
    
    void cleanup();

    UserThread* getThread(int index);
    // Making a new thread may cause a reallocation which will
    // invalidate references to members in user thread such as program counter. 
    int makeThread();
    // NOTE: we never destroy or reallocate the thread array as it results in
    //  unnecessary complications.
    
    // will also run the thread
    int makeOSThread(ExternalCall func, int refType, void* value);
    int makeOSThread(const std::string& cmd);

    Scope* getScope(uint index);
    bool ensureScopes(uint depth);

    void enterScope();
    void exitScope();

    uint32 makeNumber();
    void deleteNumber(uint index);
    Number* getNumber(uint index);
    
    uint32 makeString();
    void deleteString(uint index);
    String* getString(uint index);

    void execute(Bytecode& code, Performance* perf=0);
    
    static void Execute(Bytecode& code, Performance* perf=0);
};