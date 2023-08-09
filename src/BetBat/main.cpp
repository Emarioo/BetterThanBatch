#include "BetBat/Compiler.h"
#include "BetBat/Util/TestSuite.h"

#include <math.h>

void print_help(){
    using namespace engone;
    log::out << log::BLUE << "##   Help (outdated)   ##\n";
    log::out << log::GRAY << "More information can be found here:\n"
        "https://github.com/Emarioo/BetterThanBatch/tree/master/docs\n";
    #define PRINT_USAGE(X) log::out << log::YELLOW << X ": "<<log::SILVER;
    #define PRINT_DESC(X) log::out << X;
    #define PRINT_EXAMPLES log::out << log::LIME << " Examples:\n";
    #define PRINT_EXAMPLE(X) log::out << X;
    PRINT_USAGE("compiler.exe [file0 file1 ...]")
    PRINT_DESC("Arguments after the executable specifies source files to compile. "
             "They will compile and run seperately. Use #import inside the source files for more files in your "
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
    PRINT_USAGE("compiler.exe -target <target-platform>")
    PRINT_DESC("Compiles source code to the specified target whether that is bytecode, Windows, Linux, x64, object file, or an executable. "
            "All of those in different combinations may not be supported yet.\n")
    PRINT_EXAMPLES
    PRINT_EXAMPLE("  compiler.exe main.btb -out main.exe -target win-x64\n")
    PRINT_EXAMPLE("  compiler.exe main.btb -out main -target linux-x64\n")
    PRINT_EXAMPLE("  compiler.exe main.btb -out main.btbc -target bytecode\n")
    log::out << "\n";
    PRINT_USAGE("compiler.exe -user-args [arg0 ...]")
    PRINT_DESC("Only works if you run a program. Any arguments after this flag will be passed to the program that is specified to execute\n")
    PRINT_EXAMPLES
    PRINT_EXAMPLE("  compiler.exe main.btb -user-args hello there\n")
    PRINT_EXAMPLE("  compiler.exe -run main.btbc -user-args -some-flag \"TO PASS\"\n")
    log::out << "\n";
    #undef PRINT_USAGE
    #undef PRINT_DESC
    #undef PRINT_EXAMPLES
    #undef PRINT_EXAMPLE
}
// #include <typeinfo>
// struct A {
//     int ok;
// };
int main(int argc, const char** argv){
    using namespace engone;
    
    // A a;
    // const auto& yeah = typeid(a);
    // log::out << yeah.name()<<"\n";

    // return 1;
    CompileOptions compileOptions{};

    std::string compilerPath = argv[0];
    Path compilerDir = compilerPath;
    compilerDir = compilerDir.getAbsolute().getDirectory();
    if(compilerDir.text.length()>4){
        if(compilerDir.text.substr(compilerDir.text.length()-5,5) == "/bin/")
            compilerDir = compilerDir.text.substr(0,compilerDir.text.length() - 4);
    }
    compileOptions.resourceDirectory = compilerDir.text;
    
    bool devmode=false;
    
    // UserProfile* userProfile = UserProfile::CreateDefault();

    #ifdef CONFIG_DEFAULT_TARGET
    compileOptions.target = CONFIG_DEFAULT_TARGET;
    #endif

    bool onlyPreprocess = false;
    bool runFile = false;

    if (argc < 2) {
        print_help();
        return 1;
    }

    bool invalidArguments = false;
    #define ArgIs(X) !strcmp(arg,X)
    #define MODE_COMPILE 1
    #define MODE_OUT 2
    #define MODE_RUN 4
    int mode = MODE_COMPILE; // and run unless out files
    for(int i=1;i<argc;i++){
        const char* arg = argv[i];
        int len = strlen(argv[i]);
        // log::out << "arg["<<i<<"] "<<arg<<"\n";
        if(!strcmp(arg,"--help")||!strcmp(arg,"-help")) {
            print_help();
            return 1;
        } else if (ArgIs("-run")) {
            runFile = true;
        } else if (ArgIs("-preproc")) {
            onlyPreprocess = true;
        } else if (ArgIs("-out")) {
            i++;
            if(i<argc){
                arg = argv[i];
                compileOptions.outputFile = arg;
                // TODO: Disallow paths that start with a dash since they resemble arguments
            } else {
                invalidArguments = true;
                log::out << log::RED << "You must specify a file path after '-out'.\n";
                // TODO: print list of targets
                // for(int j=0;j<){
                // }
            }
        } else if (ArgIs("-dev")) {
            devmode = true;
        } else if (ArgIs("-target")){
            i++;
            if(i<argc){
                arg = argv[i];
                compileOptions.target = ToTarget(arg);
                if(compileOptions.target == UNKNOWN_TARGET) {
                    invalidArguments = true;
                    log::out << log::RED << arg << " is not a valid target.\n";
                    // TODO: print list of targets
                }
            } else {
                invalidArguments = true;
                log::out << log::RED << "You must specify a target after '-target'.\n";
                // TODO: print list of targets
                // for(int j=0;j<){
                // }
            }
        } else if(ArgIs("-user-args")) {
            i++;
            for(;i<argc;i++) {
                const char* arg = argv[i];
                int len = strlen(argv[i]);
                compileOptions.userArgs.add(arg);
            }
        } else {
            if(*arg == '-') {
                log::out << log::RED << "Invalid argument '"<<arg<<"' (see -help)\n";
            } else {
                compileOptions.initialSourceFile = arg;
            }
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

    if(onlyPreprocess){
        if (compileOptions.initialSourceFile.text.size() == 0) {
            log::out << log::RED << "You must specify a file when using -preproc\n";
        } else {
            if(compileOptions.outputFile.text.size() == 0) {
                log::out << log::RED << "You must specify an output file (use -out) when using -preproc.\n";
            } else{
                auto stream = TokenStream::Tokenize(compileOptions.initialSourceFile.text);
                CompileInfo compileInfo{};
                auto stream2 = Preprocess(&compileInfo, stream);
                stream2->writeToFile(compileOptions.outputFile.text);
                TokenStream::Destroy(stream);
                TokenStream::Destroy(stream2);
            }
        }
    }
    else if(!devmode){
        if(compileOptions.outputFile.text.size()==0) {
            CompileAndRun(&compileOptions);
        } else {
            CompileAndExport(&compileOptions);
        }
    } else if(devmode){
        log::out << log::BLACK<<"[DEVMODE]\n";
        #ifndef DEV_FILE
        compileOptions.initialSourceFile = "examples/dev.btb";
        #else
        compileOptions.initialSourceFile = DEV_FILE;
        #endif

        // DynamicArray<std::string> tests;
        // tests.add("tests/what/struct.btb");
        // TestSuite(tests);

        if(compileOptions.target == BYTECODE){
            CompileAndRun(&compileOptions);
        } else {
            #define OBJ_FILE "bin/dev.obj"
            #define EXE_FILE "dev.exe"
            compileOptions.outputFile = EXE_FILE;
            CompileAndRun(&compileOptions);
            // CompileAndExport(&compileOptions);

            // for(int i=0;i<100;i++) {
            //     auto opts = compileOptions;
            //     CompileAndRun(&opts);
            // }

            // // CompileOptions options{};
            // // CompileAndExport({"examples/x64_test.btb"}, EXE_FILE);

            // Program_x64* program = nullptr;
            // Bytecode* bytecode = CompileSource(&compileOptions);
            // // bytecode->codeSegment.used=0;
            // // bytecode->add({BC_DATAPTR, BC_REG_RBX});
            // // bytecode->addIm(0);
            // // bytecode->add({BC_MOV_MR, BC_REG_RBX, BC_REG_AL, 1});
            // #ifdef LOG_MEASURES
            // PrintMeasures();
            // #endif

            // if(bytecode)
            //     program = ConvertTox64(bytecode);

            // defer { if(bytecode) Bytecode::Destroy(bytecode); if(program) Program_x64::Destroy(program); };
            // if(program){
            //     program->printHex("temp.hex");
            //     // program = Program_x64::Create();
            //     // u8 arr[]={ 0x48, 0x83, 0xEC, 0x28, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x28, 0xC3 };
            //     // program->addRaw(arr,sizeof(arr));
            //     // NamedRelocation nr{};
            //     // nr.name = "printme";
            //     // nr.textOffset = 0x5;
            //     // program->namedRelocations.add(nr);

            //     WriteObjectFile(OBJ_FILE,program);

            //     // auto objFile = ObjectFile::DeconstructFile(OBJ_FILE, false);
            //     // defer { ObjectFile::Destroy(objFile); };

            //     // link the object file and run the resulting executable
            //     // error level is printed because it's the only way to get
            //     // an output from the executable at the moment.
            //     // Printing and writing to files require linkConvention to stdio.h or windows.
            //     i32 errorLevel = 0;
            //     engone::StartProgram("","link /nologo " OBJ_FILE 
            //     " bin/NativeLayer.lib"
            //     " uuid.lib"
            //     // " Kernel32.lib"
            //     " shell32.lib"
            //     " /DEFAULTLIB:LIBCMT",PROGRAM_WAIT);
            //     std::string hoho{};
            //     hoho += EXE_FILE;
            //     for(auto& arg : compileOptions.userArgs){
            //         hoho += " ";
            //         hoho += arg;
            //     }
            //     engone::StartProgram("",(char*)hoho.data(),PROGRAM_WAIT,&errorLevel);
            //     log::out << "Error level: "<<errorLevel<<"\n";
            // }
        }

        // {
        //     auto objFile = ObjectFile::DeconstructFile("bin/obj_test.obj", true);
        //     defer { ObjectFile::Destroy(objFile); };
        //     objFile->writeFile("bin/obj_min.obj");
            // auto objFile2 = ObjectFile::DeconstructFile("bin/obj_min.obj", false);
            // defer { ObjectFile::Destroy(objFile2); };
        // }

        // Bytecode::Destroy(bytecode);
        
        // PerfTestTokenize("example/build_fast.btb",200);
    }
    // std::string msg = "I am a rainbow, wahoooo!";
    // for(int i=0;i<(int)msg.size();i++){
    //     char chr = msg[i];
    //     log::out << (log::Color)(i%16);
    //     log::out << chr;
    // }
    NativeRegistry::DestroyGlobal();
    int finalMemory = GetAllocatedBytes() - log::out.getMemoryUsage() - Tracker::GetMemoryUsage();
    if(finalMemory!=0){
        log::out << log::RED<< "Final memory: "<<finalMemory<<"\n";
        PrintRemainingTrackTypes();

        Tracker::PrintTrackedTypes();
    }
    // log::out << "Total allocated bytes: "<<GetTotalAllocatedBytes() << "\n";
    log::out.cleanup();
    // system("pause");
}