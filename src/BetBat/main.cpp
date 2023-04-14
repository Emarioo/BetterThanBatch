#include "BetBat/TestGenerator.h"
#include "BetBat/Compiler.h"
#include "BetBat/TestSuite.h"

// void split(){
//     const char* str="bool compileInst = false ; for ( int i = 1 ; i < argc ; i++) {";

//     std::vector<std::string> list;
//     int len = strlen(str);
//     list.push_back("");
//     for(int i=0;i<len;i++){
//         char chr = str[i];
//         if(chr==' '){
//             if(list.size()!=0){
//                 if(!list.back().empty()){
//                     list.push_back("");
//                     continue;
//                 }
//             }
//         }
//         list.back()+=chr;
//     }
//     for(int i=0;i<list.size();i++){
//         printf("%s\n",list[i]);
//     }
// }
int main(int argc, const char** argv){
    using namespace engone;

    // master.txt seems to be opened by other program and
    // can't be opened. Not sure why?
    // log::out.enableReport(false);

    bool compileInst=false;
    for(int i=1;i<argc;i++){
        const char* str = argv[i];
        int len = strlen(argv[i]);
        if(len>0 && str[0]=='-'){
            if(!strcmp(argv[i],"-test")){
                TestSuite();
            }
        }else{
            if(strcmp(argv[i],"-inst")==0){
                compileInst=true;
            }else if(compileInst){
                CompileInstructions(argv[i]);
            }else{
                CompileScript(argv[i]);
            }
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
        // CompileScript("tests/script/if.txt");
        // CompileScript("tests/script/funcs.txt");
        // CompileScript("tests/benchmark/loop.txt");
        // CompileScript("tests/script/prop.txt");
        CompileScript("tests/script/addfunc.txt");

        // CompileInstructions("tests/inst/stack.txt");
        // CompileInstructions("tests/inst/func.txt");
        // CompileInstructions("tests/inst/numstr.txt");
        // CompileInstructions("tests/inst/apicalls.txt");
    }

    int finalMemory = GetAllocatedBytes()-log::out.getMemoryUsage();
    if(finalMemory!=0)
        log::out << "Final memory: "<<finalMemory<<"\n";
        
    log::out << "end of main!\n";

    log::out.cleanup();
}