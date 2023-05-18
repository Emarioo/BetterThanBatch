#include "BetBat/TestGenerator.h"
#include "BetBat/Compiler.h"
#include "BetBat/TestSuite.h"

#include "BetBat/Utility.h"

void print_help(){
    using namespace engone;
    log::out << log::BLUE << "##   HELP   ##\n";
    log::out << log::GOLD << "compiler.exe [file0 file1 ...]: "<<log::SILVER;
    log::out << "Arguments after the executable specifies script files to compile. "
             << "They will be compiled and run seperately.\n";
    log::out << log::LIME << " Examples:\n";
    log::out << "  compiler.exe file0.btb script.txt\n";
    log::out << "\n";
    log::out << log::GOLD << "compiler.exe -log [type0,type1,...]: "<<log::SILVER;
    log::out << "Prints debug info. The argument after determines what "
             << "type of info to print. Types are tokenizer, preprocessor, parser, "
             << "optimizer, interpreter and threads. Comma can be used to specify multiple. "
             << "Script files can be specified before and after\n";
    log::out << log::LIME << " Examples:\n";
    log::out << "  compiler.exe script.btb -log  "<<log::GRAY<<"(log by itself gives some basic information)\n";
    log::out << "  -log tok          "<<log::GRAY<<"(extra info about tokenizer)\n";
    log::out << "  -log pre,par,thr  "<<log::GRAY<<"(extra info about preprocessor, parser and threads)\n";
    log::out << "  -log opt,int      "<<log::GRAY<<"(extra info about optimizer and interpreter)\n";
}

int main(int argc, const char** argv){
    using namespace engone;

    // uint64 a = 1;
    // int b = 2;
    // int c = 3;
    // uint64 d = 4;
    // auto p0 = &a;
    // auto p1 = &b;
    // auto p2 = &c;
    // auto p3 = &d;
    
    // return 0;

    log::out.enableReport(false);

    #define IfArg(X) if(!strcmp(arg,X))
    #define MODE_TEST 1
    #define MODE_RUN 2
    #define MODE_LOG 3
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
        } else IfArg("-log") {
            mode = MODE_LOG;
            SetLog(LOG_OVERVIEW,true);
        }else if(mode==MODE_RUN){
            files.push_back(arg);
        } else if(mode==MODE_TEST){
            mode = MODE_RUN;
            tests.push_back(arg);
        } else if(mode==MODE_LOG) {
            mode = MODE_RUN;
            
            int corrects[6]{0};
            const char* strs[]{"tokenizer","preprocessor","parser","optimizer","interpreter","threads"};
            
            for (int j=0;j<len;j++){
                char chr = arg[j];
                if(chr!=','){
                    for (int k=0;k<6;k++){
                        // log::out << "T "<<k<<" "<<chr<<"\n";
                        if(strs[k][corrects[k]] == chr){
                            // log::out << "R "<<k<<" "<<chr<<"\n";
                            corrects[k]++;
                            if(corrects[k] == (int)strlen(strs[k])){
                                corrects[k] = 0;
                                SetLog(1<<k,true);
                            }
                        }else{
                            corrects[k]=0;
                        }
                    }
                }
                if(chr == ',' || j+1 == len){
                    int max = 0;
                    int index = -1;
                    for (int k=0;k<6;k++){
                        if(corrects[k] >= max){
                            index = k;
                            max = corrects[k];
                        }
                        corrects[k] = 0;
                    }
                    // log::out << "on "<<index<<"\n";
                    if(index!=-1){
                        SetLog(1<<index,true);
                    }
                    continue;
                }
            }
        }
    }
    if(!tests.empty()){
        TestSuite(tests);
    }
    for(std::string& file : files){
        CompileScript(file.c_str());
    }
    if(files.size()==0){
        // print_help();
        // log::out << "No input files!\n";

        // PerfTestTokenize("example/build_fast.btb",200);

        CompileScript("example/v2/ast.btb");
        // CompileScript("tests/benchmark/loop.btb");
        // CompileScript("tests/benchmark/loop2.btb");
        
        // log::out.enableConsole(false);
        // TestVariableLimit(10000);
        // TestMathExpression(100);

        // auto tp = MeasureSeconds();
        // int sum = 0;
        // // int sum1 = 0;
        // // int sum2 = 0;
        // for (int i=0;i<1000000;i++) {
        //     sum = sum + i;
        //     // sum2 = sum1 + i*2;
        //     // sum2 = sum2 + i*3;
        // }
        // log::out << (StopMeasure(tp)*1e3)<<"\n";

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
        // CompileScript("example/osthread.btb");
        // CompileScript("example/recursion.btb");
        // CompileScript("tests/constoptim.btb");
        // CompileScript("tests/benchmark/string.btb", 10);
        // CompileScript("tests/script/eh.btb");
        // CompileScript("example/build.btb");
        // CompileScript("example/build_fast.btb");
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