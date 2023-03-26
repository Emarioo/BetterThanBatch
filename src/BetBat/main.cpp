#include "BetBat/TestGenerator.h"
#include "BetBat/Compiler.h"

int main(int argc, const char** argv){
    using namespace engone;
    for(int i=1;i<argc;i++){
        CompileScript(argv[i]);
    }
    if(argc<2){
        TestVariableLimit(10);
        // TestMathExpression(100);

        // CompileDisassemble("tests/varlimit.txt");

        // CompileScript("tests/script/vars.txt");
        // CompileScript("tests/script/math.txt");

        // CompileInstructions("tests/inst/stack.txt");
        // CompileInstructions("tests/inst/func.txt");
        // CompileInstructions("tests/inst/numstr.txt");
        // CompileInstructions("tests/inst/apicalls.txt");
    }

    int finalMemory = GetAllocatedBytes()-log::out.getMemoryUsage();
    if(finalMemory!=0)
        log::out << "Final memory: "<<finalMemory<<"\n";
}