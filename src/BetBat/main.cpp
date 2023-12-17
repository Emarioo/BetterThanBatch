
#include "BetBat/Compiler.h"
#include "BetBat/Util/TestSuite.h"
#include "BetBat/PDBWriter.h"
#include "BetBat/Dwarf.h"

#include "BetBat/Help.h"

#include <math.h>
// #include "BetBat/glfwtest.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#undef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_WRITE_IMPLEMENTATION

#define EXIT_CODE_NOTHING 0
#define EXIT_CODE_SUCCESS 0
#define EXIT_CODE_FAILURE 1

void test_garbage() {
    using namespace engone;
    dwarf::LEB128_test();
    ByteStream::Test_iterator();
    
    u32 a = 1;
    u32 b = 32;
    u32 c = a << b;
    
    #define A(a,...) a + __VA_ARGS__ + __VA_ARGS__
    
    A(a,0,2);


    // auto f = PDBFile::Deconstruct("wa.pdb");
    // log::out << "---\n";
    // f->writeFile("wa.pdb");
    
    // PDBFile::Destroy(f);
    // DeconstructDebugSymbols(obj->
    // DeconstructPDB("test.pdb");
    // DeconstructPDB("bin/compiler.pdb");
    // DeconstructPDB("bin/dev.pdb");

    Tracker::SetTracking(false); // bad stuff happens when global data of tracker is deallocated before other global structures like arrays still track their allocations afterward.
}
bool streq(const char* a, const char* b) {
    int len_a = strlen(a);
    int len_b = strlen(b);
    if(len_a != len_b)
        return false;
    for(int i=0;i<len_a;i++) {
        if(a[i] != b[i])
            return false;
    }
    return true;
}
int main(int argc, const char** argv){
    using namespace engone;
    
    // test_garbage();
    // return 0;
    
    /*
        INITIALIZE
    */
    log::out.enableReport(false);
    MeasureInit();
    ProfilerInitialize();

    CompileOptions compileOptions{};
    compileOptions.threadCount = 1;
    int compilerExitCode = EXIT_CODE_NOTHING;
    std::string compilerPath = argv[0];
    Path compilerDir = compilerPath;
    compilerDir = compilerDir.getAbsolute().getDirectory();
    if(compilerDir.text.length()>4){
        if(compilerDir.text.substr(compilerDir.text.length()-5,5) == "/bin/")
            compilerDir = compilerDir.text.substr(0,compilerDir.text.length() - 4);
    }
    compileOptions.modulesDirectory = compilerDir.text + "modules/";
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
    for(int i=1;i<argc;i++){
        const char* arg = argv[i];
        int len = strlen(argv[i]);
        // log::out << "arg["<<i<<"] "<<arg<<"\n";
        if(streq(arg,"--help")||streq(arg,"-help")) {
            print_help();
            return EXIT_CODE_NOTHING;
        } else if (streq(arg,"--run") || streq(arg,"-r")) {
            compileOptions.executeOutput = true;
        } else if (streq(arg,"--preproc") || streq(arg,"-p")) {
            onlyPreprocess = true;
        } else if (streq(arg,"--out") || streq(arg,"-o")) {
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
        } else if (streq(arg,"-dev")) {
            devmode = true;
        } else if (streq(arg,"--tests")) {
            runTests = true;
        } else if (streq(arg,"--debug") || streq(arg, "-g")) {
            compileOptions.useDebugInformation = true;
        } else if (streq(arg,"--silent")) {
            compileOptions.silent = true;
        } else if (streq(arg,"--verbose")) {
            compileOptions.verbose = true;
            log::out << log::RED << "Verbose option (--verbose) is not implemented\n";
        } else if (streq(arg,"--target") || streq(arg, "-t")){
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
        } else if(streq(arg,"--user-args")) {
            i++;
            for(;i<argc;i++) {
                const char* arg = argv[i];
                int len = strlen(argv[i]);
                compileOptions.userArguments.add(arg);
            }
        } else {
            if(*arg == '-') {
                log::out << log::RED << "Invalid argument '"<<arg<<"' (see -help)\n";
                invalidArguments = true;
            } else {
                compileOptions.sourceFile = arg;
            }
        }
    }

    compileOptions.threadCount = 1;

    if(invalidArguments) {
        return EXIT_CODE_NOTHING; // not a compiler failure so we use "NOTHING" instead of "FAILURE"
    }
    
    if(compileOptions.useDebugInformation) {
        log::out << log::RED << "Debug information is not supported yet\n";
        return EXIT_CODE_NOTHING;
    }
    // #ifdef gone
    if(onlyPreprocess){
        if (compileOptions.sourceFile.text.size() == 0) {
            log::out << log::RED << "You must specify a file when using --preproc\n";
        } else {
            auto stream = TokenStream::Tokenize(compileOptions.sourceFile.text);
            if(!stream) {
                log::out << log::RED << "Cannot read file '"<< compileOptions.sourceFile.text<<"'\n";
            } else {
                CompileInfo compileInfo{};
                compileInfo.compileOptions = &compileOptions;
                auto stream2 = Preprocess(&compileInfo, stream);
                Assert(stream2);
                if(compileOptions.outputFile.text.size() == 0) {
                    log::out << log::AQUA << "## "<<compileOptions.sourceFile.text<<" ##\n";
                    stream2->print();
                    // TODO: Output to a default file like preproc.btb
                    // log::out << log::RED << "You must specify an output file (use -out) when using -preproc.\n";
                    compilerExitCode = compileOptions.compileStats.errors;
                } else{
                    log::out << "Preprocessor output written to '"<<compileOptions.outputFile.text<<"'\n";
                    stream2->writeToFile(compileOptions.outputFile.text);
                    compilerExitCode = compileOptions.compileStats.errors;
                }
                TokenStream::Destroy(stream);
                TokenStream::Destroy(stream2);
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
        compileOptions.sourceFile = "examples/dev.btb";
        #else
        compileOptions.sourceFile = DEV_FILE;
        #endif

        #ifdef RUN_TEST_SUITE
        DynamicArray<std::string> tests;
        // tests.add("tests/simple/operations.btb");
        // tests.add("tests/flow/switch.btb");
        tests.add("tests/funcs/overloading.btb");
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
            // compileOptions.useDebugInformation = true;
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
    // #endif
    // compileOptions.compileStats.generatedFiles.add("Yoo!");
    // {
    //     Path yoo = "haha";
    //     yoo.~Path();
    // }
    compileOptions.cleanup();
    // compileOptions.~CompileOptions(); // options has memory which needs to be freed before checking for memory leaks.
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