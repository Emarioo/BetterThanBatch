#include "BetBat/NativeRegistry.h"


engone::Logger& operator<<(engone::Logger& logger, Language::Slice<char>& slice){
    logger << std::string(slice.ptr,slice.len);
    return logger;
}

int NativeRegistry::initializations = 0;

NativeRegistry* NativeRegistry::Create(){
    // Assert(false);
    auto ptr = (NativeRegistry*)engone::Allocate(sizeof(NativeRegistry));
    new(ptr)NativeRegistry();
    return ptr;
}
void NativeRegistry::Destroy(NativeRegistry* ptr){
    ptr->~NativeRegistry();
    engone::Free(ptr, sizeof(NativeRegistry));
}

NativeRegistry::NativeFunction* NativeRegistry::findFunction(const Token& name){
    Assert(initialized);
    auto pair = nativeFunctionMap.find(name);
    if(pair == nativeFunctionMap.end()){
        return nullptr;
    }
    return nativeFunctions.getPtr(pair->second);
}
NativeRegistry::NativeFunction* NativeRegistry::findFunction(i64 jumpAddress){
    Assert(initialized);
    return nativeFunctions.getPtr(-jumpAddress - 1);
}
void NativeRegistry::initNativeContent(){
    Assert(!initialized);
    initialized=true;
    if(initializations){
        engone::log::out << engone::log::YELLOW<< "Initializing native registry again ("<<(initializations + 1)<<" time(s) so far).\n";
    }
    initializations++;
    #define ADD(X) _addFunction(#X,{NATIVE_##X});

    ADD(printi)
    ADD(printc)
    ADD(prints)
    ADD(printd)
    ADD(malloc)
    ADD(realloc)
    ADD(free)
    ADD(FileOpen)
    ADD(FileRead)
    ADD(FileWrite)
    ADD(FileClose)

    ADD(DirectoryIteratorCreate)
    ADD(DirectoryIteratorDestroy)
    ADD(DirectoryIteratorNext)
    ADD(DirectoryIteratorSkip)

    ADD(CurrentWorkingDirectory)
    ADD(StartTimePoint)
    ADD(EndTimePoint)

    ADD(CmdLineArgs)

    #undef ADD
}
void NativeRegistry::_addFunction(const std::string& name, const NativeFunction& func){
    nativeFunctions.add(func);
    nativeFunctions.last().name = name;
    // nativeFunctions.last().jumpAddress; // func contains this 
    nativeFunctionMap[name] = nativeFunctions.size()-1;
}