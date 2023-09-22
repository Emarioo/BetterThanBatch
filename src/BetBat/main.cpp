
#include "BetBat/Compiler.h"
#include "BetBat/Util/TestSuite.h"
#include "BetBat/PDBWriter.h"

#include <math.h>
// #include "BetBat/glfwtest.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#undef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_WRITE_IMPLEMENTATION

void print_version(){
    using namespace engone;
    char buffer[100];
    CompilerVersion version = CompilerVersion::Current();
    version.serialize(buffer, sizeof(buffer),CompilerVersion::INCLUDE_AVAILABLE);
    log::out << "BTB Compiler, version: " << log::LIME<< buffer <<"\n";
    log::out << log::GRAY << " (major.minor.patch.revision/name-year.month.day)\n";
    // log::out << log::GRAY << " released "<<version.year << "-"<<version.month << "-"<<version.day <<" (YYYY-MM-DD)";
}
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
             "projects. The exit code will be zero if the compilation succeeded; non-zero if something failed.\n")
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
#define EXIT_CODE_NOTHING 0
#define EXIT_CODE_SUCCESS 0
#define EXIT_CODE_FAILURE 1

int main(int argc, const char** argv){
    using namespace engone;
    
    // u32 a = 1;
    // u32 b = 32;
    // u32 c = a << b;
    
    // #define A(a,...) a + __VA_ARGS__ + __VA_ARGS__
    
    // A(a,0,2);


    auto f = PDBFile::Deconstruct("test.pdb");
    f->writeFile("test.pdb");
    // PDBFile::Destroy(f);
    // DeconstructDebugSymbols(obj->
    // DeconstructPDB("test.pdb");
    // DeconstructPDB("bin/compiler.pdb");
    // DeconstructPDB("bin/dev.pdb");

    Tracker::SetTracking(false); // bad stuff happens when global data of tracker is deallocated before other global structures like arrays still track their allocations afterward.
    return 0;

    MeasureInit();

    ProfilerInitialize();

    CompileOptions compileOptions{};
    int compilerExitCode = EXIT_CODE_NOTHING;

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
    bool runTests = false;

    if (argc < 2) {
        print_version();
        print_help();
        return EXIT_CODE_NOTHING;
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
            return EXIT_CODE_NOTHING;
        } else if (ArgIs("-run")) {
            compileOptions.executeOutput = true;
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
        } else if (ArgIs("-tests")) {
            runTests = true;
        } else if (ArgIs("-debug")) {
            compileOptions.useDebugInformation = true;
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
                invalidArguments = true;
            } else {
                compileOptions.initialSourceFile = arg;
            }
        }
    }

    if(invalidArguments) {
        return EXIT_CODE_NOTHING; // not a compiler failure so we use "NOTHING" instead of "FAILURE"
    }

    if(onlyPreprocess){
        if (compileOptions.initialSourceFile.text.size() == 0) {
            log::out << log::RED << "You must specify a file when using -preproc\n";
        } else {
            if(compileOptions.outputFile.text.size() == 0) {
                // TODO: Output to a default file like preproc.btb
                log::out << log::RED << "You must specify an output file (use -out) when using -preproc.\n";
            } else{
                auto stream = TokenStream::Tokenize(compileOptions.initialSourceFile.text);
                CompileInfo compileInfo{};
                compileInfo.compileOptions = &compileOptions;
                auto stream2 = Preprocess(&compileInfo, stream);
                stream2->writeToFile(compileOptions.outputFile.text);
                TokenStream::Destroy(stream);
                TokenStream::Destroy(stream2);

                compilerExitCode = compileOptions.compileStats.errors;
            }
        }
    } else if(runTests) {
        // DynamicArray<std::string> tests;
        // tests.add("tests/simple/operations.btb");
        int failures = TestSuite(TEST_ALL);
        compilerExitCode = failures;
    } else if(!devmode){
        if(compileOptions.outputFile.text.size()==0) {
            CompileAll(&compileOptions);
        } else {
            CompileAll(&compileOptions);
        }
        compilerExitCode = compileOptions.compileStats.errors;
    } else if(devmode){
        log::out << log::BLACK<<"[DEVMODE]\n";
        #ifndef DEV_FILE
        compileOptions.initialSourceFile = "examples/dev.btb";
        #else
        compileOptions.initialSourceFile = DEV_FILE;
        #endif

        #ifdef RUN_TEST_SUITE
        DynamicArray<std::string> tests;
        // tests.add("tests/simple/operations.btb");
        tests.add("tests/flow/switch.btb");
        // tests.add("tests/simple/garb.btb");
        // tests.add("tests/flow/loops.btb");
        // tests.add("tests/what/struct.btb");
        VerifyTests(tests);
        #elif defined(RUN_TESTS)
        auto strs = {RUN_TESTS};
        DynamicArray<std::string> tests;
        for(auto s : strs) {
            tests.add(s);
        }
        VerifyTests(tests);
        #else
        if(compileOptions.target == BYTECODE){
            compileOptions.executeOutput = true;
            CompileAll(&compileOptions);
        } else {
            #define EXE_FILE "dev.exe"
            compileOptions.outputFile = EXE_FILE;
            compileOptions.useDebugInformation = true;
            compileOptions.executeOutput = true;
            CompileAll(&compileOptions);
        }
        compilerExitCode = compileOptions.compileStats.errors;
        #endif

        // DeconstructPDB("bin/dev.pdb");
        // auto pdb = PDBFile::Deconstruct("bin/dev.pdb");
        // PDBFile::Destroy(pdb);
        // DeconstructPDB("test.pdb");

        // Bytecode::Destroy(bytecode);
        
        // PerfTestTokenize("example/build_fast.btb",200);
    }
    
    compileOptions.~CompileOptions(); // options has memory which needs to be freed before checking for memory leaks.
    // std::string msg = "I am a rainbow, wahoooo!";
    // for(int i=0;i<(int)msg.size();i++){
    //     char chr = msg[i];
    //     log::out << (log::Color)(i%16);
    //     log::out << chr;
    // }
    // ProfilerPrint();
    // ProfilerExport("profiled.dat");
    ProfilerCleanup();

    NativeRegistry::DestroyGlobal();
    int finalMemory = GetAllocatedBytes() - log::out.getMemoryUsage() - Tracker::GetMemoryUsage() - MeasureGetMemoryUsage();
    if(finalMemory!=0){
        log::out << log::RED<< "Final memory: "<<finalMemory<<"\n";
        PrintRemainingTrackTypes();

        Tracker::PrintTrackedTypes();
    }
    // log::out << "Total allocated bytes: "<<GetTotalAllocatedBytes() << "\n";
    log::out.cleanup();
    // system("pause");
    Tracker::SetTracking(false); // bad stuff happens when global data of tracker is deallocated before other global structures like arrays still track their allocations afterward.

    return compilerExitCode;
}