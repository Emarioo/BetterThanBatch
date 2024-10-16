#include "BetBat/NativeRegistry.h"


// engone::Logger& operator<<(engone::Logger& logger, Language::Slice<char>& slice){
//     logger << std::string(slice.ptr,slice.len);
//     return logger;
// }

int NativeRegistry::initializations = 0;

NativeRegistry* s_globalRegistry = nullptr;
// NativeRegistry s_globalRegistry{};
NativeRegistry* NativeRegistry::GetGlobal(){
    if(!s_globalRegistry){
        s_globalRegistry = TRACK_ALLOC(NativeRegistry);
        new(s_globalRegistry) NativeRegistry();
        s_globalRegistry->initNativeContent();
    }
    // if(!s_globalRegistry.initialized) {
    //     // s_globalRegistry = Create();
    //     // s_globalRegistry->initNativeContent();
    //     s_globalRegistry.initNativeContent();
    // }
    return s_globalRegistry;
    // return &s_globalRegistry;
}
void NativeRegistry::DestroyGlobal(){
    if(!s_globalRegistry)
        return;
    s_globalRegistry->nativeFunctions.cleanup();
    TRACK_FREE(s_globalRegistry,NativeRegistry);
    s_globalRegistry = nullptr;
}

NativeRegistry::NativeFunction* NativeRegistry::findFunction(const std::string& name){
    Assert(initialized);
    auto pair = nativeFunctionMap.find(name);
    if(pair == nativeFunctionMap.end()){
        return nullptr;
    }
    // return nativeFunctions.getPtr(pair->second);
    return &nativeFunctions[pair->second];
}
NativeRegistry::NativeFunction* NativeRegistry::findFunction(i64 jumpAddress){
    Assert(initialized);
    auto pair = nativeFunctionMap2.find(jumpAddress);
    if(pair == nativeFunctionMap2.end()){
        return nullptr;
    }
    // return nativeFunctions.getPtr(pair->second);
    return &nativeFunctions[pair->second];
}
void NativeRegistry::initNativeContent(){
    Assert(!initialized);
    initialized=true;
    if(initializations){
        engone::log::out << engone::log::YELLOW<< "Initializing native registry again (only necessary once, "<<(initializations + 1)<<" time(s) so far).\n";
    }
    initializations++;
    #define ADD(X) _addFunction(#X,{NATIVE_##X});

    ADD(printi)
    ADD(printc)
    ADD(prints)
    ADD(printd)
    ADD(Allocate)
    ADD(Reallocate)
    ADD(Free)

    ADD(FileOpen)
    ADD(FileRead)
    ADD(FileWrite)
    ADD(FileClose)

    ADD(DirectoryIteratorCreate)
    ADD(DirectoryIteratorDestroy)
    ADD(DirectoryIteratorNext)
    ADD(DirectoryIteratorSkip)

    ADD(CurrentWorkingDirectory)
    ADD(StartMeasure)
    ADD(StopMeasure)

    ADD(CmdLineArgs)

    ADD(NativeSleep)

    #undef ADD
}
void NativeRegistry::_addFunction(const std::string& name, const NativeFunction& func){
    // nativeFunctions.push_back(func);
    // nativeFunctions.back().name = name;
    nativeFunctions.add(func);
    nativeFunctions.last().name = name;
    // nativeFunctions.last().jumpAddress; // func contains this 
    nativeFunctionMap[name] = nativeFunctions.size()-1;
    nativeFunctionMap2[func.jumpAddress] = nativeFunctions.size()-1;
}