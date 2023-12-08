#include "BetBat/DebugInformation.h"

u32 DebugInformation::addOrGetFile(const std::string& file) {
    for(int i=0;i<(int)files.size();i++){
        if(files[i] == file)
            return i;
    }
    files.add(file);
    return files.size() - 1;
}
DebugInformation::Function* DebugInformation::addFunction(const std::string name){
    // TODO: Optimize
    int found = 0;
    for(int i=0;i<(int)functions.size();i++){
        if(functions[i].name == name) {
            found++;
        }
    }
    functions.add({});
    functions.last().name = name;
    if(found>0){
        functions.last().name += "_" + std::to_string(found);
    }
    return &functions.last();
}