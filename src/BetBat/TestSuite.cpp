#include "BetBat/TestSuite.h"
#include "BetBat/Interpreter.h"
#include "BetBat/Context.h"
#include "BetBat/Parser.h"
#include "BetBat/GenParser.h"
#include "BetBat/Utility.h"

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
bool hasTokens(TokenStream& tokens, int min, int max=-1){
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
        Context::TestValue& val = data->context.testValues[data->testValueIndex++];\
        if(val.type==REF_STRING && !(val.string == S)){\
            return TEST_FAILED;\
        }}

#define TEST_FAILED 0
#define TEST_SUCCESS 1
struct TestData {
    engone::Memory text{0};
    TokenStream tokens{};
    Bytecode bytecode{};
    Context context{};
    int testValueIndex = 0;
    
    void init(const char* path){
        text = ReadFile(path);
        tokens = TokenStream::Tokenize(text);
    }
    ~TestData(){
        text.resize(0);
        tokens.cleanup();
        bytecode.cleanup();
        context.cleanup();
    }
};
typedef int (*TestCase(TestData* data));

int TestSumtest(TestData* data){
    using namespace engone;
    int err=0;

    if(!hasTokens(data->tokens,13))
        return TEST_FAILED;

    data->bytecode = GenerateScript(data->tokens,&err);
    if(err||
        !hasConstraint(data->bytecode,1,1)||
        !hasConstraint(data->bytecode,2,1)||
        !hasNumbers(data->bytecode,2)
    ) return TEST_FAILED;

    data->context.execute(data->bytecode);

    TEST_VALUE("3");
    
    return TEST_SUCCESS;
}
int TestMacro(){
    return 0;
}

bool VectorContains(std::vector<std::string>& list, const std::string& str){
    for(std::string& it : list){
        if (it == str) 
            return true;
    }
    return false;
}

void TestSuite(std::vector<std::string>& tests, bool testall){
    using namespace engone;
    int failedCases = 0;
    int totalCases = 0;

#define RUN_TEST(P,F) {TestData data;data.init(P);int result = F(&data);totalCases++;if(result==TEST_FAILED) failedCases++; }

    if(testall||VectorContains(tests,"sumtest"))
        RUN_TEST("tests/sumtest.btb",TestSumtest)

    int successfulCases = totalCases-failedCases;

    float rate = (float)successfulCases/totalCases;
    if(rate<1.f) log::out << log::GOLD;
    else log::out << log::GREEN;
    log::out << (100.f*rate)<<"% Success ("<<successfulCases<<" correct cases)\n";
    if(rate!=1.f){
        log::out << log::RED << failedCases<<" cases failed\n";
    }
}