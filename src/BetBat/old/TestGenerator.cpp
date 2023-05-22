#include <random>
#include <time.h>
#include <string.h>
#include "Engone/Logger.h"

#include "BetBat/Compiler.h"

#define ADD(...) buffer.used += sprintf((char*)buffer.data+buffer.used,__VA_ARGS__);

float Random(){
    static bool once=false;
    if(!once){
        once=true;
        time_t now = time(0);
        srand(now);
    }
    return (float)rand()/RAND_MAX;
}

void TestMathExpression(int length){
    using namespace engone;
    Memory buffer{1};
    int padding=20;
    if(!buffer.resize(length+padding))
        return;
    memset(buffer.data,0,buffer.max);
    
    #define TYPE_NUM 0
    #define TYPE_OP 1
    #define TYPE_PAR 2
    
    // Todo: make a buggy version with uneven ops and paranthesis.
    #define UNEVEN_PAR 1
    #define UNEVEN_OP 2
    #define UNEVEN_NUM 4

    int bugs =
        // UNEVEN_PAR|
        // UNEVEN_OP|
        // UNEVEN_NUM|
    0;

    const char* ops = "+-*/";
    const char* parsStr = "()";

    float parChance = 0.2;
    int numberRange=9;
    int pars=0;
    while(true){
        float potentialPar=Random();
        // printf("%f",potentialPar);
        int parType = Random()*2;
        if(parType==1&&pars==0)
            parType=0;
        
        if(potentialPar<parChance && 0==(bugs&UNEVEN_PAR)){
            if(parType==0)
                pars++;
            else
                pars--;
        }
        if(potentialPar<parChance && parType==0){
            ADD("%c",parsStr[parType]);
        }
        
        if(0==(bugs&UNEVEN_NUM)||Random()<0.7){
            int num = 1+Random()*(numberRange-1);
            ADD("%d ",num);
        }
        if(potentialPar<parChance && parType==1){
            ADD("%c",parsStr[parType]);
        }
        if(buffer.used>buffer.max-padding){
            for(int i=0;i<pars;i++){
                ADD(")");
                
                if(bugs&UNEVEN_PAR){
                    if(Random()<0.3)
                        break;
                }
            }
            break;
        }
        if(0==(bugs&UNEVEN_OP)||Random()<0.7){
            int op = Random()*4;
            ADD("%c ",ops[op]);
        }
    }

    const char* path = "tests/mathexpr.txt";
    WriteFile(path,buffer);
    CompileScript(path);
}
void TestVariableLimit(int length){
    using namespace engone;

    Memory buffer{1};
    if(!buffer.resize(length*50))
        return;

    for(int i=0;i<length;i++){
        ADD("var%d = %d\n",i,i);
    }
    ADD("sum = ");
    for(int i=0;i<length;i++){
        if(i!=0)
            ADD("+ ");
        ADD("var%d ",i);
    }
    ADD("\n");

    const char* path = "tests/varlimit.txt";
    WriteFile(path,buffer);
    CompileScript(path);

    buffer.resize(0);
}