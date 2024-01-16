
#include "BetBat/Compiler.h"
#include "BetBat/Util/TestSuite.h"
#include "BetBat/PDB.h"
#include "BetBat/DWARF.h"

#include "BetBat/Help.h"

#include <math.h>
// #include "BetBat/glfwtest.h"

#include "BetBat/Fuzzer.h"
#include "BetBat/Lexer.h"

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
// #include "Bcrypt.h"
// #include "unistd.h"
int main(int argc, const char** argv){
    using namespace engone;
    log::out.enableReport(false);
    // MeasureInit();
    ProfilerInitialize();
    
    lexer::Lexer lexer{};
    u16 fileid = lexer.tokenize("examples/dev.btb");

    lexer.print(fileid);

    return 0;
    
    // log::out << p.text << "\n";
    // for(int i=0;i<10000;i++) {
    //     log::out << i << "\r";
    //     log::out.flush();
    //     engone::Sleep(0.5);
    // }
    // u64 a = 0x8000'0000'0000'0001;
    // double f = a;

    // auto f = FileCOFF::DeconstructFile("wa.exe", false);
    // auto f = FileCOFF::DeconstructFile("bin/dev.obj");

    // log::out.print(f->stringTableData, f->stringTableSize);

    // DynamicArray<std::string> files{};
    // PatternMatchFiles("*main.cpp|*.h|!libs",&files);
    // log::out << "RESULT:\n";
    // FOR(files)
    //     log::out << it<<"\n";

    // dwarf::LEB128_test();
    // return 0;

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
    bool performTests = false; // could be one or more
    bool show_profiling = false;
    bool search_for_source = false;

    std::string pattern_for_files = ""; // used with --test --search-for-files

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
        } else if (streq(arg,"--test") || streq(arg,"-ts")) {
            performTests = true;
            compileOptions.instant_report = false;
            if(i+1 < argc && argv[i+1][0] != '-'){
                i++;
                arg = argv[i];
                pattern_for_files = arg;
            }
        } else if (streq(arg,"--test-with-errors") || streq(arg,"-twe")) {
            performTests = true;
            compileOptions.instant_report = true;
            if(i+1 < argc && argv[i+1][0] != '-'){
                i++;
                arg = argv[i];
                pattern_for_files = arg;
            }
        } else if (streq(arg,"--debug") || streq(arg, "-g")) {
            compileOptions.useDebugInformation = true;
        } else if (streq(arg,"--silent")) {
            compileOptions.silent = true;
        } else if (streq(arg,"--verbose")) {
            compileOptions.verbose = true;
            log::out << log::RED << "Verbose option (--verbose) is not used anywhere yet\n";
        } else if (streq(arg,"--profiling")) {
            show_profiling = true;
        } else if (streq(arg,"--target") || streq(arg, "-t")){
            i++;
            if(i<argc){
                arg = argv[i];
                compileOptions.target = ToTarget(arg);
                if(compileOptions.target == TARGET_UNKNOWN) {
                    invalidArguments = true;
                    log::out << log::RED << arg << " is not a valid target.\n";
                    // TODO: print list of targets
                }
            } else {
                invalidArguments = true;
                log::out << log::RED << "You must specify a target after '"<<arg<<"'.\n";
                // TODO: print list of targets
                // for(int j=0;j<){
                // }
            }
        } else if (streq(arg,"--linker") || streq(arg, "-l")){
            i++;
            if(i<argc){
                arg = argv[i];
                compileOptions.linker = ToLinker(arg);
                if(compileOptions.linker == LINKER_UNKNOWN) {
                    invalidArguments = true;
                    log::out << log::RED << arg << " is not a valid linker.\n";
                    // TODO: print list of targets
                }
            } else {
                invalidArguments = true;
                log::out << log::RED << "You must specify a linker after '"<<arg<<"'.\n";
                // TODO: print list of targets
            }
        // } else if (streq(arg,"--linker-cmd")){
        //     CUSTOM LINKER is not possible because we don't know what flags to pass to the linker.
        //     Is -o the flag for output or is it -output, /out perhaps /OUT or /Fo:
        //     i++;
        //     if(i<argc){
        //         arg = argv[i];
        //         compileOptions.linker_cmd = arg;
        //     } else {
        //         invalidArguments = true;
        //         log::out << log::RED << "You must specify a command line linker after '"<<arg<<"'.\n";
        //     }
        }else if (streq(arg,"--pattern-match") || streq(arg, "-pm")){
            search_for_source = true;
            i++;
            if(i<argc){
                arg = argv[i];
                pattern_for_files = arg;
            } else {
                invalidArguments = true;
                log::out << log::RED << "You must specify a pattern after '"<<arg<<"'.\n";
                // TODO: print list of targets
            }
        } else if(streq(arg,"--user-args") || streq(arg,"-ua")) {
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
                // arg = argv[i];
                // pattern_for_files = arg;
                compileOptions.sourceFile = arg;
            }
        }
    }
    
    if(!devmode && !performTests && !search_for_source && compileOptions.sourceFile.text.empty()) {
        log::out << log::RED << "Specify a source file\n";
        return EXIT_CODE_NOTHING;
    }

    compileOptions.threadCount = 1;

    if(invalidArguments) {
        return EXIT_CODE_NOTHING; // not a compiler failure so we use "NOTHING" instead of "FAILURE"
    }
    if(compileOptions.useDebugInformation) {
        log::out << log::YELLOW << "Debug information was recently added and is not complete. Only DWARF with COFF (on Windows) and GCC is supported. ELF for Unix systems is on the way.\n";
        // return EXIT_CODE_NOTHING; // debug is used with linker errors (.text+0x32 -> file:line)
    }
    if(search_for_source) {
        DynamicArray<std::string> files{};
        int num = PatternMatchFiles(pattern_for_files, &files);
        if(files.size() == 0) {
            log::out << log::RED << "Pattern '"<<pattern_for_files<<"' did not match any files\n";
            return EXIT_CODE_NOTHING;
        } else if(files.size() > 1) {
            log::out << log::RED << "Pattern '"<<pattern_for_files<<"' matched more than one file:\n";
            for(int i=0;i<files.size();i++) {
                log::out << " "<<files[i]<<"\n";
            }
            return EXIT_CODE_NOTHING;
        } else {
            compileOptions.sourceFile = files[0];
            // ok
            // for(int i=0;i<files.size();i++) {
            //     log::out << log::GRAY<< " "<<files[i]<<"\n";
            // }
        }
        // if(!compileOptions.sourceFile.isAbsolute()) {
        //     const char* path = compileOptions.sourceFile.text.c_str();
        //     int pathlen = compileOptions.sourceFile.text.length();
        //     Assert(pathlen != 0);
        //     auto iter = engone::DirectoryIteratorCreate(".",1);
        //     engone::DirectoryIteratorData data{};
        //     defer { engone::DirectoryIteratorDestroy(iter, &data); };
        //     while(engone::DirectoryIteratorNext(iter, &data)) {
        //         // log::out << data.name<< "\n";
        //         if(data.isDirectory) {
        //             if(data.namelen >= 3 && !strncmp(data.name, "./.", 3)) { // ignore folders like .git, .vscode, .vs
        //                 engone::DirectoryIteratorSkip(iter);
        //             }
        //             continue;
        //         }
        //         if(data.namelen >= pathlen && !strcmp(data.name + data.namelen - pathlen, path)) {
        //             Assert(data.namelen >= 2);
        //             if(data.name[0] == '.' && data.name[1] == '/')
        //                 compileOptions.sourceFile.text = data.name + 2;
        //             else
        //                 compileOptions.sourceFile.text = data.name;
        //             break;
        //         }
        //     }
        // }
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
                // TODO: Don't skip imports.
                if(stream->importList.size() > 0) {
                    log::out << log::RED << "All imports are skipped with the '--preproc' flag.\n";
                    log::out << log::GRAY << "Imports: ";
                    for(int i=0;i<stream->importList.size();i++) {
                        if(i!=0) log::out << ", ";
                        log::out << stream->importList[i].name;
                    }
                    log::out<<"\n";
                }
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
    } else if(performTests) {
        if(pattern_for_files.length() != 0) {
            DynamicArray<std::string> tests;
            int matches = PatternMatchFiles(pattern_for_files, &tests);
            if(matches == 0) {
                log::out << log::RED << "The pattern '"<<pattern_for_files << "' did not match with any files\n";
            } else {
                int failures = VerifyTests(&compileOptions, tests);
                compilerExitCode = failures;
            }
        } else {
            // DynamicArray<std::string> tests;
            // tests.add("tests/simple/operations.btb");
            int failures = TestSuite(&compileOptions, TEST_ALL);
            // int failures = VerifyTests(tests);
            compilerExitCode = failures;
        }
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

        // #define RUN_TESTS "tests/simple/operations.btb"
        // #define RUN_TESTS "tests/simple/assignment.btb"
        // #define RUN_TESTS "tests/flow/defer.btb"
        // #define RUN_TESTS "tests/flow/loops.btb"
        // #define RUN_TESTS "tests/flow/switch.btb"
        // #define RUN_TESTS "tests/funcs/overloading.btb"
        // #define RUN_TESTS "tests/macro/defines.btb"
        // #define RUN_TESTS "tests/macro/recur.btb"
        // #define RUN_TESTS "tests/macro/spacing.btb"
        // #define RUN_TESTS "tests/inline-asm/simple.btb"

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
        if(compileOptions.target == TARGET_BYTECODE){
            compileOptions.executeOutput = true;
            CompileAll(&compileOptions);
        } else {
            #define EXE_FILE "dev.exe"
            
            // FuzzerOptions opts{};
            // opts.node_iterations_per_file = 5000;
            // opts.file_count = 20;
            // GenerateFuzzedFiles(opts,"main.btb");
            
            compileOptions.useDebugInformation = true;
            compileOptions.executeOutput = true;
            compileOptions.outputFile = EXE_FILE;
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
    // if(show_profiling)
    //     PrintMeasures();
    // MeasureCleanup();

    // ProfilerPrint();
    // ProfilerExport("profiled.dat");

    ProfilerCleanup();

    NativeRegistry::DestroyGlobal();
    int finalMemory = GetAllocatedBytes() - log::out.getMemoryUsage() - Tracker::GetMemoryUsage();
    // int finalMemory = GetAllocatedBytes() - log::out.getMemoryUsage() - Tracker::GetMemoryUsage() - MeasureGetMemoryUsage();
    if(finalMemory!=0){
        log::out << log::RED<< "Final memory: "<<finalMemory<<"\n";
        PrintRemainingTrackTypes();

        Tracker::PrintTrackedTypes();
    }
    // log::out << "Total allocated bytes: "<<GetTotalAllocatedBytes() << "\n";
    log::out.cleanup();
    // system("pause");

    Tracker::SetTracking(false); // bad stuff happens when global data of tracker is deallocated before other global structures like arrays still track their allocations afterward.
    Tracker::DestroyGlobal();

    return compilerExitCode;
}