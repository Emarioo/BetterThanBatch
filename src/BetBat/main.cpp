#include "BetBat/TestGenerator.h"
#include "BetBat/Compiler.h"
#include "BetBat/TestSuite.h"

int main(int argc, const char** argv){
    using namespace engone;

    // log::out.enableReport(false);

    for(int i=1;i<argc;i++){
        const char* str = argv[i];
        int len = strlen(argv[i]);
        if(len>0 && str[0]=='-'){
            if(!strcmp(argv[i],"-test")){
                TestSuite();
            }
        }else{
            // if(strcmp(argv[i],"-inst")==0){
            //     compileInst=true;
            // }else if(compileInst){
            //     CompileInstructions(argv[i]);
            // }else{
                CompileScript(argv[i]);
            // }
        }
        CompileScript(argv[i]);
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
        CompileScript("example/preproc.txt");

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