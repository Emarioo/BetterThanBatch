
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
#include "Engone/Win32Includes.h"
int main(int argc, const char** argv){
    using namespace engone;
    
    // DeconstructPDB("test.pdb");
    // DeconstructDebugSymbols(obj->
    // DeconstructPDB("test.pdb");
    // DeconstructPDB("bin/compiler.pdb");
    // DeconstructPDB("bin/dev.pdb");

    // auto obj = ObjectFile::DeconstructFile("test.obj");

    // auto pdb = PDBFile::Deconstruct("test.pdb");
    // PDBFile::WriteFile(pdb, "test2.pdb");
    PDBFile::WriteEmpty("test2.pdb");
    auto pdb2 = PDBFile::Deconstruct("test2.pdb");
    // PDBFile::Destroy(pdb);
    Tracker::SetTracking(false); // bad stuff happens when global data of tracker is deallocated before other global structures like arrays still track their allocations afterward.
    return 0;

    // log::out << "hello\n";

    // SECURITY_ATTRIBUTES sa;
    // sa.nLength = sizeof(sa);
    // sa.lpSecurityDescriptor = NULL;
    // sa.bInheritHandle = TRUE;

    // HANDLE h = CreateFileA("out.log",
    //     GENERIC_WRITE|GENERIC_READ,
    //     // FILE_APPEND_DATA,
    //     FILE_SHARE_WRITE | FILE_SHARE_READ,
    //     &sa,
    //     // OPEN_ALWAYS,
    //     CREATE_ALWAYS,
    //     FILE_ATTRIBUTE_NORMAL,
    //     NULL );

    // PROCESS_INFORMATION pi; 
    // STARTUPINFOA si;
    // BOOL ret = FALSE; 
    // DWORD flags = CREATE_NO_WINDOW;

    // ZeroMemory( &pi, sizeof(PROCESS_INFORMATION) );
    // ZeroMemory( &si, sizeof(STARTUPINFO) );
    // si.cb = sizeof(STARTUPINFOA); 
    // si.dwFlags |= STARTF_USESTDHANDLES;
    // si.hStdInput = NULL;
    // si.hStdError = h;
    // si.hStdOutput = h;

    // char* cmd = "ml64";
    // ret = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, flags, NULL, NULL, &si, &pi);

    // if ( ret ) 
    // {
    //     CloseHandle(pi.hProcess);
    //     CloseHandle(pi.hThread);
    //     return 0;
    // }

    // return -1;


    // auto file = engone::FileOpen("temps.txt", 0, FILE_ALWAYS_CREATE);

    // // auto prev = engone::GetStandardOut();
    // // engone::SetStandardOut(file);

    // engone::StartProgram("ml64 /c", PROGRAM_WAIT, 0, {}, file);
    // // log::out << "Hey!\n";

    // engone::FileClose(file);

    // engone::SetStandardOut(prev);

    // u64 u = 0;
    // i64 s = -23;
    // bool gus = u > s;
    // bool gsu = s > u;

    // bool lsu = s < u;
    // bool lus = u < s;

    // bool gesu = s >= u;
    // bool geus = u >= s;
    
    // bool lesu = s <= u;
    // bool leus = u <= s;
    // float l = n;
    // float l = k;

    // float k = 0.2f;
    // bool a = 0.1f < k;
    // bool b = 0.1f <= k;
    // bool c = 0.1f > k;
    // bool d = 0.1f >= k;
    // bool e = 0.1f == k;
    // bool f = 0.1f != k;

    // TestWindow();
    // return 0;
    // A a;
    // const auto& yeah = typeid(a);
    // log::out << yeah.name()<<"\n";
    MeasureInit();

    ProfilerInitialize();

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
    bool runTests = false;

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
        } else if (ArgIs("-tests")) {
            runTests = true;
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
            }
        }
    } else if(runTests) {
        // DynamicArray<std::string> tests;
        // tests.add("tests/simple/operations.btb");
        TestSuite(TEST_ALL);
    } else if(!devmode){
        if(compileOptions.outputFile.text.size()==0) {
            compileOptions.executeOutput = true;
            CompileSource(&compileOptions);
            // CompileAndRun(&compileOptions);
        } else {
            CompileSource(&compileOptions); // won't execute
            // CompileAndExport(&compileOptions);
        }
    } else if(devmode){
        log::out << log::BLACK<<"[DEVMODE]\n";
        #ifndef DEV_FILE
        compileOptions.initialSourceFile = "examples/dev.btb";
        #else
        compileOptions.initialSourceFile = DEV_FILE;
        #endif

        #ifdef RUN_TEST_SUITE
        DynamicArray<std::string> tests;
        tests.add("tests/simple/operations.btb");
        // tests.add("tests/flow/loops.btb");
        // tests.add("tests/what/struct.btb");
        VerifyTests(tests);
        if(true) {} else
        #endif
        if(compileOptions.target == BYTECODE){
            compileOptions.executeOutput = true;
            CompileSource(&compileOptions);
            // CompileAndRun(&compileOptions);
        } else {
            #define OBJ_FILE "bin/dev.obj"
            #define EXE_FILE "dev.exe"
            compileOptions.outputFile = EXE_FILE;
            compileOptions.useDebugInformation = true;
            compileOptions.executeOutput = true;
            CompileSource(&compileOptions);
            // CompileAndRun(&compileOptions);
        }

        // DeconstructPDB("bin/dev.pdb");
        PDBFile::Deconstruct("bin/dev.pdb");
        // DeconstructPDB("test.pdb");

        // Bytecode::Destroy(bytecode);
        
        // PerfTestTokenize("example/build_fast.btb",200);
    }
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

}