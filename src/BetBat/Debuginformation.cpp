#include "BetBat/DebugInformation.h"

u32 DebugInformation::addOrGetFile(const std::string& file) {
    for(int i=0;i<files.size();i++){
        if(files[i] == file)
            return i;
    }
    files.add(file);
    return files.size() - 1;
}