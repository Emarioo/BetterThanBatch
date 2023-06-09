#include "BetBat/Compiler.h"

void print_help(){
    using namespace engone;
    log::out << log::BLUE << "##   Help   ##\n";
    log::out << log::GRAY << "More information can be found here:\n"
        "https://github.com/Emarioo/BetterThanBatch/tree/master/docs\n";
    #define PRINT_USAGE(X) log::out << log::YELLOW << X ": "<<log::SILVER;
    #define PRINT_DESC(X) log::out << X;
    #define PRINT_EXAMPLES log::out << log::LIME << " Examples:\n";
    #define PRINT_EXAMPLE(X) log::out << X;
    PRINT_USAGE("compiler.exe [file0 file1 ...]")
    PRINT_DESC("Arguments after the executable specifies source files to compile. "
             "They will compile and run seperately. Use #import for more files in your "
             "projects.\n")
    PRINT_EXAMPLES
    PRINT_EXAMPLE("  compiler.exe file0.btb script.txt\n")
    log::out << "\n";
    PRINT_USAGE("compiler.exe [file0 ...] -out [file0 ...]")
    PRINT_DESC("With the "<<log::WHITE<<"out"<<log::WHITE<<" flag, files to the left will be compiled into "
             "bytecode and written to the files on the right of the out flag. "
             "The amount of files on the left and right must match.\n")
    PRINT_EXAMPLES
    PRINT_EXAMPLE("  compiler.exe main.btb -out program.btbc\n")
    log::out << "\n";
    PRINT_USAGE("compiler.exe -run [file0 ...]")
    PRINT_DESC("Runs bytecode files generated with the out flag.\n")
    PRINT_EXAMPLES
    PRINT_EXAMPLE("  compiler.exe -run program.btbc\n")
    log::out << "\n";

    #undef PRINT_USAGE
    #undef PRINT_DESC
    #undef PRINT_EXAMPLES
    #undef PRINT_EXAMPLE
    // log::out << log::YELLOW << "compiler.exe -log [type0,type1,...]: "<<log::SILVER;
    // log::out << "Prints debug info. The argument after determines what "
    //          << "type of info to print. Types are tokenizer, preprocessor, parser, "
    //          << "optimizer, interpreter and threads. Comma can be used to specify multiple. "
    //          << "Script files can be specified before and after\n";
    // log::out << log::LIME << " Examples:\n";
    // log::out << "  compiler.exe script.btb -log  "<<log::GRAY<<"(log by itself gives some basic information)\n";
    // log::out << "  -log tok          "<<log::GRAY<<"(extra info about tokenizer)\n";
    // log::out << "  -log pre,par,thr  "<<log::GRAY<<"(extra info about preprocessor, parser and threads)\n";
    // log::out << "  -log opt,int      "<<log::GRAY<<"(extra info about optimizer and interpreter)\n";

    /*
        compiler main.btb -out main.btbc
        compiler -run main.btbc


    */
}

int main(int argc, const char** argv){
    using namespace engone;

    log::out.enableReport(false);

    #define IfArg(X) if(!strcmp(arg,X))
    #define MODE_COMPILE 1
    #define MODE_OUT 2
    #define MODE_RUN 4
    // #define MODE_TEST 29
    // #define MODE_LOG 120
    int mode = MODE_COMPILE;
    
    bool devmode=false;
    
    std::string compilerPath = argv[0];

    std::vector<std::string> tests; // could be const char*
    std::vector<std::string> files;
    std::vector<std::string> outFiles;
    std::vector<std::string> filesToRun;

    for(int i=1;i<argc;i++){
        const char* arg = argv[i];
        int len = strlen(argv[i]);
        // log::out << "arg["<<i<<"] "<<arg<<"\n";
        IfArg("--help") {
            print_help();
            return 0;
        } else IfArg("-run") {
            mode = MODE_RUN;
        } else IfArg("-out") {
            mode = MODE_OUT;
        } else IfArg("-dev") {
            devmode = true;
        } else if(mode==MODE_COMPILE){
            files.push_back(arg);
        } else if(mode==MODE_RUN){
            filesToRun.push_back(arg);
        } else if(mode==MODE_OUT){
            outFiles.push_back(arg);
        } 
        /*
        else if(mode==MODE_TEST){
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
        */
    }
    if(files.size() != outFiles.size() && outFiles.size()!=0){
        log::out << log::RED <<"The amount of input and output files must match!\n";
        int index = 0;
        // TODO: Prettier formatting. What about really long file names.
        WHILE_TRUE {
            if(index<(int)files.size()){
                log::out << files[index];
            } else {
                log::out << "?";
            }
            log::out << " - ";
            if(index<(int)outFiles.size()){
                log::out << outFiles[index];
            } else {
                log::out << "?";
            }
            log::out << "\n";
        }
        return 0;
    }
    // if(!tests.empty()){
    //     // TestSuite(tests);
    // }
    Path compilerDir = compilerPath;
    compilerDir = compilerDir.getAbsolute().getDirectory();
    if(compilerDir.text.length()>4){
        if(compilerDir.text.substr(compilerDir.text.length()-5,5) == "/bin/")
            compilerDir = compilerDir.text.substr(0,compilerDir.text.length() - 4);
    }
    for(int i = 0; i < (int)files.size();i++){
        if(outFiles.size()==0){
            log::out << log::GRAY << "Compile and run: "<<files[i] << "\n";
            CompileAndRun({files[i].c_str(), compilerDir.text});
        } else {
            Bytecode* bc = CompileSource({files[i].c_str(), compilerDir.text});
            if(bc){
                bool yes = ExportBytecode(outFiles[i], bc);
                Bytecode::Destroy(bc);
                if(yes)
                    log::out << log::GRAY<<"Exported "<<files[i] << " into bytecode in "<<outFiles[i]<<"\n";
                else
                    log::out <<log::RED <<"Failed exporting "<<files[i] << " into bytecode in "<<outFiles[i]<<"\n";
            }
        }
    }
    for(std::string& file : filesToRun){
        Bytecode* bc = ImportBytecode(file);
        if(bc){
            log::out << log::GRAY<<"Running "<<file << "\n";
            RunBytecode(bc);
            Bytecode::Destroy(bc);
        } else {
            log::out <<log::RED <<"Failed importing "<<file <<"\n";
        }
    }
    if(!devmode && files.empty()
        && filesToRun.empty()){
        print_help();
        // log::out << "No input files!\n";
    } else if(devmode){
        log::out << log::BLACK<<"[DEVMODE]\n";
        // PerfTestTokenize("example/build_fast.btb",200);

        // CompileAndRun({"examples/strings.btb", compilerDir.text});
        CompileAndRun({"examples/test.btb", compilerDir.text});
        // CompileAndRun({"examples/macro-bench.btb", compilerDir.text});
        // RunBytecode({"a.btbc", compilerDir.text});
        // Bytecode* bc = ImportBytecode(std::string("a.btbc"));
        // RunBytecode(bc);
        // Bytecode::Destroy(bc);
        // CompileAndRun({"strings.btb", compilerDir.text});
        // CompileAndRun("example/ast.btb", compilerPath);
        // CompileAndRun("tests/benchmark/loop.btb");
        // CompileAndRun("tests/benchmark/loop2.btb");
        
        // log::out.enableConsole(false);
        // TestVariableLimit(10000);
        // TestMathExpression(100);

        // auto tp = MeasureTime();
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

    }
    // std::string msg = "I am a rainbow, wahoooo!";
    // for(int i=0;i<(int)msg.size();i++){
    //     char chr = msg[i];
    //     log::out << (log::Color)(i%16);
    //     log::out << chr;
    // }
    int finalMemory = GetAllocatedBytes() - log::out.getMemoryUsage();
    if(finalMemory!=0){
        log::out << log::RED<< "Final memory: "<<finalMemory<<"\n";
        PrintRemainingTrackTypes();
    }
    log::out.cleanup();
    // system("pause");
}