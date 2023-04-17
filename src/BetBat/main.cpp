#include "BetBat/TestGenerator.h"
#include "BetBat/Compiler.h"
#include "BetBat/TestSuite.h"

int main(int argc, const char** argv){
    using namespace engone;

    log::out.enableReport(false);



    #define IfArg(X) if(!strcmp(arg,X))
    #define MODE_TEST 1
    #define MODE_RUN 2
    int mode = MODE_RUN;

    for(int i=1;i<argc;i++){
        const char* arg = argv[i];
        int len = strlen(argv[i]);
        log::out << "arg["<<i<<"] "<<arg<<"\n";
        IfArg("-test") {
            mode = MODE_TEST;
        } else IfArg("-testall") {
            TestSuite();
        } else IfArg("-run") {
            mode = MODE_RUN;
        } else if(mode==MODE_RUN){
            CompileScript(argv[i]);
        } else if(mode==MODE_TEST){
            // test specific?
            log::out << log::YELLOW<< "No named tests yet\n";
        }
    }
    if(argc<2){
        log::out << "No input files!\n";
        
        // log::out.enableConsole(false);
        // TestVariableLimit(10000);
        // TestMathExpression(100);

        // CompileDisassemble("tests/varlimit.txt");

        // CompileScript("tests/script/vars.txt");
        // CompileScript("tests/script/math.txt");
        // CompileScript("tests/script/funcexe.txt");
        // CompileScript("tests/script/if.txt");
        // CompileScript("tests/script/funcs.txt");
        // CompileScript("tests/benchmark/loop.txt");
        // CompileScript("tests/script/prop.txt");
        // CompileScript("tests/script/addfunc.txt");
        // CompileScript("example/preproc.txt");
        // CompileScript("example/each.txt");
        CompileScript("example/filter.txt");

        // CompileInstructions("tests/inst/stack.txt");
        // CompileInstructions("tests/inst/func.txt");
        // CompileInstructions("tests/inst/numstr.txt");
        // CompileInstructions("tests/inst/apicalls.txt");
    }else{
        int finalMemory = GetAllocatedBytes()-log::out.getMemoryUsage();
        if(finalMemory!=0)
            log::out << "Final memory: "<<finalMemory<<"\n";            
        log::out.cleanup();
    }
}