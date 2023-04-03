#include "BetBat/TestGenerator.h"
#include "BetBat/Compiler.h"

int main(int argc, const char** argv){
    using namespace engone;

    bool compileInst=false;
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"-inst")==0){
            compileInst=true;
        }else if(compileInst){
            CompileInstructions(argv[i]);
        }else{
            CompileScript(argv[i]);
        }
    }
    
    if(argc<2){
        // log::out.enableConsole(false);
        // TestVariableLimit(10000);
        // TestMathExpression(100);

        // CompileDisassemble("tests/varlimit.txt");

        // CompileScript("tests/script/vars.txt");
        // CompileScript("tests/script/math.txt");
        // CompileScript("tests/script/funcexe.txt");
        CompileScript("tests/script/if.txt");

        // CompileInstructions("tests/inst/stack.txt");
        // CompileInstructions("tests/inst/func.txt");
        // CompileInstructions("tests/inst/numstr.txt");
        // CompileInstructions("tests/inst/apicalls.txt");
    }

    int finalMemory = GetAllocatedBytes()-log::out.getMemoryUsage();
    if(finalMemory!=0)
        log::out << "Final memory: "<<finalMemory<<"\n";
        
    log::out.cleanup();

    log::out << "Done!\n";
}