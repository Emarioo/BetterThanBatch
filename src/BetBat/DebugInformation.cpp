#include "BetBat/DebugInformation.h"

#include "BetBat/Bytecode.h"

u32 DebugInformation::addOrGetFile(const std::string& file, bool skip_mutex) {
    if(!skip_mutex)
        mutex.lock();
    for(int i=0;i<(int)files.size();i++){
        if(files[i] == file) {
            if(!skip_mutex)
                mutex.unlock();
            return i;
        }
    }
    int index = files.size();
    files.add(file);
    if(!skip_mutex)
        mutex.unlock();
    return index;
}
DebugFunction* DebugInformation::addFunction(FuncImpl* impl, TinyBytecode* tinycode, const std::string& from_file, int declared_at_line){
    std::string name = "";
    // if(impl)
    //     name = ast->nameOfFuncImpl(impl); // main does not have FuncImpl
    // else
    name = tinycode->name; // so we take name from tinycode instead

    mutex.lock();
    for(int i=0;i<(int)functions.size();i++){
        if(functions[i]->name == name) {
            // Assert(false);
            // TODO: Uniquely name functions better
            name = tinycode->name + "_" + std::to_string(functions.size());
            break;
        }
    }
    u32 fi = addOrGetFile(from_file, true);
    auto ptr = TRACK_ALLOC(DebugFunction);
    new(ptr)DebugFunction(impl,tinycode,fi);
    ptr->name = name;
    ptr->declared_at_line = declared_at_line;
    functions.add(ptr);
    tinycode->debugFunction = ptr;
    mutex.unlock();
    return ptr;
}
void DebugInformation::print() {
    using namespace engone;
    for(auto& fun : functions){
        log::out << fun->name <<"\n";
        for(auto& line : fun->lines) {
            log::out << " ln "<<line.lineNumber << ": 0x" << NumberToHex(line.asm_address)<<"\n";
        }
    }
}