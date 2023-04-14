#include "BetBat/TestSuite.h"
#include "BetBat/Context.h"
#include "BetBat/Generator.h"
#include "BetBat/Utility.h"

#include <functional>

#define SERR engone::log::out << engone::log::RED<<"Error: "

#define _SLOG(x) x;


int findConstant(Bytecode& code, Decimal number){
    int count = 0;
    for(int i=0;i<(int)code.constNumbers.used;i++){
        Number* num = code.getConstNumber(i);
        if(num->value == number){
            count++;
        }
    }
    return count;
}
int findConstant(Bytecode& code, const char* str){
    int count = 0;
    String temp{};
    temp.memory.data = (void*)str;
    temp.memory.used = strlen(str);
    for(int i=0;i<(int)code.constStrings.used;i++){
        String* string = code.getConstString(i);
        if(*string == temp){
            count++;
        }
    }
    return count;
}
bool hasConstraint(Bytecode& code, Decimal number, int min, int max = -1){
    int count = findConstant(code,number);
    bool yes=false;
    if(max==-1) yes = count==min;
    else yes = count>=min&&count<=max;
    if(!yes){
        if(max==-1){
            _SLOG(SERR << "Incorrect count of "<<number<<" ("<<min<<" == "<<count<<")\n";)
        }else{
            _SLOG(SERR << "Incorrect count of "<<number<<" ("<<min<<" <= "<<count<<" <= "<<max<<")\n";)
        }
    }
    return yes;
}
bool hasNumbers(Bytecode& code, int min, int max=-1){
    int count = code.constNumbers.used;
    bool yes=false;
    if(max==-1) yes = count==min;
    else yes = count>=min&&count<=max;
    if(!yes){
        if(max==-1){
            _SLOG(SERR << "Incorrect total numbers ("<<min<<" == "<<count<<")\n";)
        }else{
            _SLOG(SERR << "Incorrect total numbers ("<<min<<" <= "<<count<<" <= "<<max<<")\n";)
        }
    }
    return yes;
}
bool hasTokens(Tokens& tokens, int min, int max=-1){
    int count = tokens.length();
    bool yes=false;
    if(max==-1) yes = count==min;
    else yes = count>=min&&count<=max;
    if(!yes){
        if(max==-1){
            _SLOG(SERR << "Incorrect token count ("<<min<<" == "<<count<<")\n";)
        }else{
            _SLOG(SERR << "Incorrect token count ("<<min<<" <= "<<count<<" <= "<<max<<")\n";)
        }
    }
    return yes;
}


#define TEST_VALUE(S) {\
        Context::TestValue& val = context.testValues[testValueIndex++];\
        if(val.type==REF_STRING && !(val.string == S)){\
            error = TEST_FAILED;\
            goto GOTO_NAME;\
        }}

#define TEST_FAILED 0
#define TEST_SUCCESS 1
int TestSumtest(){
    using namespace engone;
    const char* path = "tests/sumtest.txt";
    int error=TEST_SUCCESS;
    int err=0;
    int testValueIndex=0;
#define GOTO_NAME clean_117

    engone::Memory text{0};
    Tokens tokens{};
    Bytecode bytecode{};
    Context context{};

    text = ReadFile(path);

    tokens = Tokenize(text);

    if(!hasTokens(tokens,13)){
        error = TEST_FAILED;
        goto GOTO_NAME;
    }

    bytecode = GenerateScript(tokens,&err);

    if(
        err||
        !hasConstraint(bytecode,1,1)||
        !hasConstraint(bytecode,2,1)||
        !hasNumbers(bytecode,2)
    ){
        error = TEST_FAILED;
        goto GOTO_NAME;
    }

    context.execute(bytecode);

    TEST_VALUE("3");
    
    GOTO_NAME:
    text.resize(0);
    tokens.cleanup();
    bytecode.cleanup();
    context.cleanup();
    return error;
}
void TestSuite(){
    using namespace engone;
    int failedCases = 0;
    int totalCases = 0;

#define RUN_TEST(F) {int result = F();totalCases++;if(result==TEST_FAILED) failedCases++; }

#define PLURAL(N,S) (N==1 ? S : S "s")

    RUN_TEST(TestSumtest)

    if(failedCases==0){
        log::out << log::GREEN << "Success with ";
    }else{
        log::out << log::RED << "Failed with ";
    }
    int successfulCases = totalCases-failedCases;
    log::out << (totalCases-failedCases)<<" / "<<totalCases<< PLURAL(successfulCases," test")<< "\n";
}