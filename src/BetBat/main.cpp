#include "BetBat/Compiler.h"

#include <math.h>

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
// 0.025915
// float sine(float x) {
//     const float RAD = 1.57079632679f;
//     int inv = (x<0)<<1;
//     if(inv)
//         x = -x;
//     int t = x/RAD;
//     if (t&1)
//         x = RAD * (1+t) - x;
//     else
//         x = x - RAD * t;

//     // not faster by the way
//     // x = x - RAD * (t+(t&1));
//     // u32 temp0 = (*(u32*)&x ^ ((t&1)<<31)); // flip sign bit
//     // x = *(float*)&temp0;

//     // float taylor = (x - x*x*x/(2*3) + x*x*x*x*x/(2*3*4*5) - x*x*x*x*x*x*x/(2*3*4*5*6*7) + x*x*x*x*x*x*x*x*x/(2*3*4*5*6*7*8*9));
//     float taylor = x - x*x*x/(2*3) * ( 1 - x*x/(4*5) * ( 1 - x*x/(6*7) * (1 - x*x/(8*9))));
//     if ((t&2) ^ inv)
//         taylor = -taylor;
//     return taylor;
//     // not faster by the way
//     // u32 temp = (*(u32*)&out ^ ((t&2)<<30)); // flip sign bit
//     // return *(float*)&temp;
// }
int main(int argc, const char** argv){
    using namespace engone;
    // float yeh = 5.23f;
    // i64 num = 9;
    // float ok = 0.52f + num;
    // auto tp = MeasureTime();
    // float value = 3;
    // float sum = 0;
    // // for(int t=0;t<1000000;t++){
    // for(int t=0;t<100;t++){
    //     value -= 0.1f;
    //     // value += 0.1f;
    //     // 1.57079632679f/8.f;
    //     float f = sine(value);
    //     // float f = sinf(value);
    //     sum += f;
    //     printf("%f: %f\n", value, sine(value) - sinf(value));
    //     // printf("%f: %f = %f ~ %f\n", value, sine(value) - sinf(value), sine(value), sinf(value));
    // }
    // printf("Time %f\n",(float)StopMeasure(tp)*1000.f);
    // printf("sum: %f\n", sum);

    // double out = log10(92.3);

    // return 0;

    // UserProfile profile{};
    // profile.deserialize("myprofile.txt");
    // profile.serialize("myprofile2.txt");

    // return 0;

    // for(int i=0;i<23;i++){
    //     int c = i + 9;
    //     int d = 12;
    //     int a = c + d - (i + 2) - i*d*9;
    // }
    // // ReadObjectFile();
    // int a = 23;
    // int b = 2;
    // int c = a && b;
    // return 0;

    // u8 a = global + 2;

    // auto p = &a;

    // *p = 9;

    // u64 b = a * (192 + a * 9 - 23 / a * 23);
    // u16 c = b;


    // log::out.enableReport(false);

    // PerfTestTokenize("src/BetBat/Generator.cpp", 1);
    // PerfTestTokenize("src/BetBat/Generator.cpp", 15);
    // PerfTestTokenize("src/BetBat/Parser.cpp", 15);
    // return false;

    #define IfArg(X) if(!strcmp(arg,X))
    #define MODE_COMPILE 1
    #define MODE_OUT 2
    #define MODE_RUN 4
    #define MODE_USER_ARGS 5
    // #define MODE_TEST 29
    // #define MODE_LOG 120
    int mode = MODE_COMPILE; // and run unless out files
    
    bool devmode=false;
    
    std::string compilerPath = argv[0];

    std::vector<std::string> tests; // could be const char*
    std::vector<std::string> files;
    std::vector<std::string> outFiles;
    std::vector<std::string> filesToRun;

    TargetPlatform target = BYTECODE;
    #ifdef COMPILE_x64
    target = WINDOWS_x64;
    #endif

    bool onlyPreprocess = false;

    std::vector<std::string> userArgs;

    for(int i=1;i<argc;i++){
        const char* arg = argv[i];
        int len = strlen(argv[i]);
        if(mode==MODE_USER_ARGS){
            userArgs.push_back(arg);
            continue;
        }
        // log::out << "arg["<<i<<"] "<<arg<<"\n";
        if(!strcmp(arg,"--help")||!strcmp(arg,"-help")) {
            print_help();
            return 1;
        } else IfArg("-run") {
            mode = MODE_RUN;
        } else IfArg("-preproc") {
            onlyPreprocess = true;
        } else IfArg("-out") {
            mode = MODE_OUT;
        } else IfArg("-dev") {
            devmode = true;
        } else IfArg("-target"){
            i++;
            // TODO: Print error message if target argument is missing
            if(i<argc){
                arg = argv[i];
                target = ToTarget(arg);
                Assert(target != UNKNOWN_TARGET);
                // TODO: Error message if target is unknown
            } else {
                Assert(false);
            }
        } else IfArg("-user-args") {
            mode = MODE_USER_ARGS;
        } else if(mode==MODE_COMPILE){ 
            files.push_back(arg);
        } else if(mode==MODE_RUN){
            filesToRun.push_back(arg);
        } else if(mode==MODE_OUT){
            outFiles.push_back(arg);
        } else {
            log::out << log::RED << "Invalid argument '"<<arg<<"' (see -help)\n";
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
    CompileOptions compileOptions{};
    compileOptions.compilerDirectory = compilerDir.text;
    compileOptions.userArgs = userArgs;
    compileOptions.target = target;
    for(int i = 0; i < (int)files.size();i++){
        if(onlyPreprocess){
            if(outFiles.size()==0) // TODO: Error message
                continue;
            auto stream = TokenStream::Tokenize(files[i]);
            CompileInfo compileInfo{};
            auto stream2 = Preprocess(&compileInfo, stream);
            stream2->writeToFile(outFiles[i]);
            TokenStream::Destroy(stream);
            TokenStream::Destroy(stream2);
            continue;
        }
        compileOptions.initialSourceFile = files[i].c_str();

        if(outFiles.size()==0){
            log::out << log::GRAY << "Compile and run: "<<files[i] << "\n";
            Assert(target == BYTECODE); // TODO: error message
            CompileAndRun(compileOptions);
        } else {
            CompileAndExport(compileOptions, outFiles[i]);
        }
    }
    for(std::string& file : filesToRun){
        Bytecode* bc = ImportBytecode(file);
        if(bc){
            log::out << log::GRAY<<"Running "<<file << "\n";
            RunBytecode(bc, userArgs);
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
        #ifndef DEV_FILE
        compileOptions.initialSourceFile = "examples/dev.btb";
        #else
        compileOptions.initialSourceFile = DEV_FILE;
        #endif

        #ifndef COMPILE_x64
        // interpreter
        CompileAndRun(compileOptions);
        #else
        #define OBJ_FILE "bin/dev.obj"
        #define EXE_FILE "dev.exe"

        // CompileOptions options{};
        // CompileAndExport({"examples/x64_test.btb"}, EXE_FILE);

        Program_x64* program = nullptr;
        Bytecode* bytecode = CompileSource({DEV_FILE});
        // bytecode->codeSegment.used=0;
        // bytecode->add({BC_DATAPTR, BC_REG_RBX});
        // bytecode->addIm(0);
        // bytecode->add({BC_MOV_MR, BC_REG_RBX, BC_REG_AL, 1});
        
        if(bytecode)
            program = ConvertTox64(bytecode);

        defer { if(bytecode) Bytecode::Destroy(bytecode); if(program) Program_x64::Destroy(program); };
        if(program){
            program->printHex("temp.hex");
            // program = Program_x64::Create();
            // u8 arr[]={ 0x48, 0x83, 0xEC, 0x28, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x28, 0xC3 };
            // program->addRaw(arr,sizeof(arr));
            // NamedRelocation nr{};
            // nr.name = "printme";
            // nr.textOffset = 0x5;
            // program->namedRelocations.add(nr);

            WriteObjectFile(OBJ_FILE,program);

            // auto objFile = ObjectFile::DeconstructFile(OBJ_FILE, false);
            // defer { ObjectFile::Destroy(objFile); };

            // link the object file and run the resulting executable
            // error level is printed because it's the only way to get
            // an output from the executable at the moment.
            // Printing and writing to files require linkConvention to stdio.h or windows.
            i32 errorLevel = 0;
            engone::StartProgram("","link /nologo " OBJ_FILE 
            " bin/NativeLayer.lib"
            " uuid.lib"
            " /DEFAULTLIB:LIBCMT",PROGRAM_WAIT);
            engone::StartProgram("",EXE_FILE,PROGRAM_WAIT,&errorLevel);
            log::out << "Error level: "<<errorLevel<<"\n";
        }
        #endif

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
    int finalMemory = GetAllocatedBytes() - log::out.getMemoryUsage();
    if(finalMemory!=0){
        log::out << log::RED<< "Final memory: "<<finalMemory<<"\n";
        PrintRemainingTrackTypes();
    }
    // log::out << "Total allocated bytes: "<<GetTotalAllocatedBytes() << "\n";
    log::out.cleanup();
    // system("pause");
}