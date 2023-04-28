#include "BetBat/TestGenerator.h"
#include "BetBat/Compiler.h"
#include "BetBat/TestSuite.h"

// #include "Engone/Win32Includes.h"

#include "BetBat/Utility.h"

int main(int argc, const char** argv){
    using namespace engone;

    // const char* hm = R"(
    // // Okay koapjdopaJDOPjAODjoöAJDoajodpJAPdjkpajkdpakPDKpakdpakpakdpAKdpjAPJDpAKDpaKPDkAPKDpaKDkAPKDpAKPDkaPDkpAKDpAKPDkaPDKpAKPDkaPKDpaKDdapdkmpadkpöakldpaklpdkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk___D:ADKPAD_AD__DA_DPA_DP-D-HF_AH_FhaFh-iawHF_ha_F__paklfalflaflak,fp<jkbpm hyO IWHBRTOAWHJOÖBTJMAPÄWJNPTÄAin2åpbtImawäpå-tib,äawåtbiäåaJODjaodjAOJdoajDoaJKOdjaoJDoaJodjaOdjaoJdoaJKODkaopKDpoAKPODkaPKDpkaPDkap Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess Okay i guess
    // )";
    
    // auto mem = ReadFile("tests/benchmark/string.btb");
    // fwrite(mem.data,1,mem.used,stdout);
    // return 0;
    // printf("%s",hm);
    // printf("%s",hm);
    // printf("%s",hm);
    // fwrite(file);
    // HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    // printf("%p\n",h);
    // FileWrite((APIFile*)((uint64)h+1),hm,strlen(hm));
    // FileWrite((APIFile*)((uint64)h+1),hm,strlen(hm));
    // return 0;
    
    // auto pipe = PipeCreate(false,true);
    
    // std::string cmd = "cmd /C \"dir\"";
    // StartProgram("",(char*)cmd.data(),0,0,0,pipe);
    
    // char buffer[1024];
    // while(1){
    //     int bytes = PipeRead(pipe,buffer,sizeof(buffer));
    //     printf("Read %d\n",bytes);
    //     if(!bytes)
    //         break;
    //     for(int i=0;i<bytes;i++){
    //         printf("%c",buffer[i]);
    //     }
    //     printf("\n");
    // }
    // printf("Done\n");
    
    // PipeDestroy(pipe);
    
    // return 0;
    
    // log::out.enableReport(false);

    #define IfArg(X) if(!strcmp(arg,X))
    #define MODE_TEST 1
    #define MODE_RUN 2
    int mode = MODE_RUN;

    std::vector<std::string> tests; // could be const char*
    std::vector<std::string> files;

    for(int i=1;i<argc;i++){
        const char* arg = argv[i];
        int len = strlen(argv[i]);
        // log::out << "arg["<<i<<"] "<<arg<<"\n";
        IfArg("-test") {
            mode = MODE_TEST;
        } else IfArg("-testall") {
            TestSuite(tests,true);
        } else IfArg("-run") {
            mode = MODE_RUN;
        } else if(mode==MODE_RUN){
            files.push_back(arg);
        } else if(mode==MODE_TEST){
            tests.push_back(arg);
        }
    }
    if(!tests.empty()){
        TestSuite(tests);
    }
    for(std::string& file : files){
        CompileScript(file.c_str());
    }
    if(argc<2){
        log::out << "No input files!\n";
        
        // log::out.enableConsole(false);
        // TestVariableLimit(10000);
        // TestMathExpression(100);

        // CompileDisassemble("tests/varlimit.btb");

        // CompileScript("tests/script/vars.btb");
        // CompileScript("tests/script/math.btb");
        // CompileScript("tests/script/funcexe.btb");
        // CompileScript("tests/script/if.btb");
        // CompileScript("tests/script/funcs.btb");
        // CompileScript("tests/benchmark/loop.btb");
        // CompileScript("tests/script/prop.btb");
        // CompileScript("tests/script/addfunc.btb");
        // CompileScript("example/preproc.btb");
        // CompileScript("example/each.btb");
        // CompileScript("example/filter.btb");
        // CompileScript("example/pipes.btb");
        // CompileScript("example/lines.btb");
        // CompileScript("example/typedefify.btb");
        // CompileScript("example/loggifier.btb");
        // CompileScript("example/findmax.btb");
        // CompileScript("example/async.btb");
        // CompileScript("tests/simple/assignment.btb");
        // CompileScript("example/func.btb");
        // CompileScript("example/sumcol.btb");
        // CompileScript("example/cgen.btb");
        CompileScript("example/osthread.btb");
        // CompileScript("example/scopes.btb");
        // CompileScript("tests/constoptim.btb");
        // CompileScript("tests/benchmark/string.btb", 10);
        // CompileScript("tests/script/eh.btb");
        // CompileScript("example/build.btb");
        // CompileScript("tests/simple/ops.btb");

        // CompileInstructions("tests/inst/stack.btb");
        // CompileInstructions("tests/inst/func.btb");
        // CompileInstructions("tests/inst/numstr.btb");
        // CompileInstructions("tests/inst/apicalls.btb");
    }else{
        
    }
    int finalMemory = GetAllocatedBytes() - log::out.getMemoryUsage();
    if(finalMemory!=0)
        log::out << log::RED<< "Final memory: "<<finalMemory<<"\n";            
    log::out.cleanup();
}