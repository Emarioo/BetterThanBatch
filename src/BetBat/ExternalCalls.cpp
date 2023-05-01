#include "BetBat/ExternalCalls.h"
#include "BetBat/Interpreter.h"
#include "BetBat/Utility.h"

#include <random>
#include <chrono>

Ref ExtPrint(Context* context, int refType, void* value){
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
}
Ref ExtRandom(Context* context, int refType, void* value){
    static std::uniform_real_distribution<double> dist(0.,1.);
    static std::mt19937 thing(std::chrono::system_clock::now().time_since_epoch().count());
    
    int index = context->makeNumber();
    context->getNumber(index)->value = dist(thing);
    return {REF_NUMBER,index};
}
double Numify(String* str){
    using namespace engone;
    static char* buffer=0;
    static const int BUFFER_SIZE=50;
    if(!buffer){
        buffer=(char*)malloc(BUFFER_SIZE+1);
        if(!buffer){
            log::out << log::RED<<__FUNCTION__<<": Alloc failed\n";
            return {};
        }
    }
    if(BUFFER_SIZE<str->memory.used){
        log::out <<log::RED<< __FUNCTION__<<": Buffer to small\n";
        return {};
    }
    memcpy(buffer,str->memory.data,str->memory.used);
    buffer[str->memory.used]=0;
    return atof(buffer);
}
Ref ExtFloor(Context* context, int refType, void* value){
    if(!value) return {};
    double num = 0;
    if(refType==REF_NUMBER)
        num = ((Number*)value)->value;
    else if(refType==REF_STRING)
        num = Numify((String*)value);
    // engone::log::out << refType << " "<<value<<"\n";
    int index = context->makeNumber();
    context->getNumber(index)->value = floor(num);
    return {REF_NUMBER,index};
}
Ref ExtToNum(Context* context, int refType, void* value){
    using namespace engone;
    double number=0;
    if(refType==REF_STRING){
        String* str = (String*)value;
        number = Numify(str);
    }else if(refType==REF_NUMBER){
        Number* num = (Number*)value;
        number = num->value;
    } else {
        return {}; // failed
    }
    int index = context->makeNumber();
    context->getNumber(index)->value=number;
    return {REF_NUMBER,index};
}
Ref ExtTime(Context* context, int refType, void* value){
    using namespace engone;
    // Arguments are ignored
    if(refType==0||!value){
        int index = context->makeNumber();
        context->getNumber(index)->value=engone::MeasureSeconds();
        return {REF_NUMBER,index};
    }else if(refType==REF_NUMBER) {
        Number* num = (Number*)value;
        int index = context->makeNumber();
        context->getNumber(index)->value=engone::StopMeasure(num->value);
        return {REF_NUMBER,index};
    }else if(refType==REF_STRING){
        String* str = (String*)value;
        static char* buffer=0;
        static const int BUFFER_SIZE=50;
        if(!buffer){
            buffer=(char*)malloc(BUFFER_SIZE+1);
            if(!buffer){
                log::out << log::RED<<__FUNCTION__<<": Alloc failed\n";
                return {};
            }
        }
        if(BUFFER_SIZE<str->memory.used){
            log::out <<log::RED<< __FUNCTION__<<": Buffer to small\n";
            return {};
        }
        memcpy(buffer,str->memory.data,str->memory.used);
        buffer[str->memory.used]=0;
        double number = atof(buffer);
        // Todo: atof evaluates "214) hello" to 214.
        //   If pass a combination of number and string it's probably a bug.
        //   Print an error message if so.
        int index = context->makeNumber();
        context->getNumber(index)->value=engone::StopMeasure(number);
        return {REF_NUMBER,index};
    }else{
        log::out << __FUNCTION__<<": Expected Number or null as argument (got "<<RefToString(refType)<<")\n";
        return {};
    }
}
Ref ExtFilterFiles(Context* context, int refType, void* value){
    using namespace engone;
    if(refType!=REF_STRING)
        return {};

    std::vector<std::string> list;
    
    String* str = (String*)value;
    
    int begin = 0;
    int end=0;
    uint64 i=0;
    while(i<str->memory.used){
        char chr = ((char*)str->memory.data)[i];
        i++;
        if (chr==' ') {
            end = i-2;
        } else if (i == str->memory.used) {
            end = i-1;
        } else {
            continue;
        }
        if (end-begin+1>0) {
            std::string tmp;
            tmp.resize(end-begin+1,0);
            memcpy((char*)tmp.data(),(char*)str->memory.data+begin,end-begin+1);
            list.push_back(tmp);
            // log::out << "Item "<<tmp<<"\n";
        }
        begin = i;
    }

    std::string cwd = GetWorkingDirectory();
    RecursiveDirectoryIterator* iter = RecursiveDirectoryIteratorCreate(cwd);

    int outIndex = context->makeString();
    String* out = context->getString(outIndex);
    // Todo: check valid outIndex
    DirectoryIteratorData result;
    while(1){
        bool yes = RecursiveDirectoryIteratorNext(iter,&result);
        if(!yes)
            break;

        if(result.isDirectory)
            continue;

        // log::out << "path "<<result.name<<"\n";

        bool allowed=false;
        for(std::string& tmp : list){
            if(tmp[0]=='*'){
                if(tmp.length()==1u){
                    allowed=true;
                    break;
                }
                int correct = 0;
                for(int i=result.name.length()-1;i>-1;i--){
                    int ti = tmp.length()-1-correct;
                    if(result.name[i] == tmp[ti]){
                        correct++;
                        // log::out << "Right "<<result.name[i]<<" "<<result.name<<"\n";
                        if(correct==(int)tmp.length()-1){
                            break;
                        }
                    }else{
                        break;
                        // correct=0;
                    }
                }
                if(correct==(int)tmp.length()-1){
                    allowed = true;
                    break;
                }
            } else {
                if (result.name==tmp){
                    allowed = true;
                    break;
                }
            }
        }
        if(allowed){
            if(out->memory.max < out->memory.used + result.name.length() + 1){
                out->memory.resize(out->memory.max + 2*result.name.length() + 20);
            }
            memcpy((char*)out->memory.data+out->memory.used, result.name.data(),result.name.length());
            out->memory.used += result.name.length();
            *((char*)out->memory.data+out->memory.used) = ' ';
            out->memory.used += 1;
            // log::out << "Added "<<result.name<<"\n";
        }
    }
    RecursiveDirectoryIteratorDestroy(iter);

    ReplaceChar((char*)out->memory.data,out->memory.used,'\\','/');

    return {REF_STRING,outIndex};
}

ExternalCall GetExternalCall(const std::string& name){
    #define GETEXT(X,Y) if(name==X) return Y;
    GETEXT("print",ExtPrint)
    GETEXT("time",ExtTime)
    GETEXT("filterfiles",ExtFilterFiles)

    GETEXT("tonum",ExtToNum)
    GETEXT("random",ExtRandom)
    GETEXT("floor",ExtFloor)
    
    return 0;
}
// void ProvideDefaultCalls(ExternalCalls& calls){
//     calls.map["print"]=ExtPrint;
//     calls.map["time"]=ExtTime;
//     calls.map["tonum"]=ExtToNum;
//     calls.map["filterfiles"]=ExtFilterFiles;
// }