#include "BetBat/Compiler.h"
#include "BetBat/TestSuite.h"
#include "BetBat/PDB.h"
#include "BetBat/DWARF.h"

#include "BetBat/Help.h"

#include "tracy/Tracy.hpp"

#include <math.h>

#include "BetBat/Fuzzer.h"
#include "BetBat/Lexer.h"

#undef FILE_READ_ONLY // bye bye Windows defined flag
#undef IMAGE_REL_AMD64_REL32
#undef coff

bool InterpretCommands(const DynamicArray<std::string>& commands, CompileOptions* out_options);

int main(int argc, const char** argv){
    using namespace engone;
    #define EXIT_CODE_SUCCESS 0
    #define EXIT_CODE_FAILURE 1

    auto main_start = StartMeasure();

    InitAssertHandler();
    ProfilerInitialize();

    // FileCOFF::Destroy(FileCOFF::DeconstructFile("wa.o", false));

    // return 0;
    
    DynamicArray<std::string> arguments{};
    for(int i=1;i<argc;i++) // is the first argument always the executable?
        arguments.add(argv[i]);

    // NOTE: I have not moved InterpretCommands and compiler option parsing to it's own file
    //   it's rather temporary. In the future, you will specify compile options such as
    //   target, debug, output path, incremental linking, and linker exe in a build.btb script.
    //   I would rather implement the real solution than mess around with compile options here.
    CompileOptions options{};
    bool valid = InterpretCommands(arguments, &options);

    int exit_code = EXIT_CODE_SUCCESS;

    if(!valid)
        return EXIT_CODE_FAILURE;

    if(options.quit)
        return EXIT_CODE_SUCCESS;

    arguments.cleanup();

    if(options.devmode) {
        // This code is only used during development
        log::out << log::BLACK<<"[DEVMODE]\n";

        // FuzzerOptions opts{};
        // opts.node_iterations_per_file = 5000;
        // opts.file_count = 20;
        // GenerateFuzzedFiles(opts,"main.btb");

        options.output_file = "main.exe";
        options.source_file = "examples/dev.btb";
        options.threadCount = 2;
        // options.disable_multithreading = false;
        // options.target = TARGET_BYTECODE;
        // options.target = TARGET_WINDOWS_x64;
        // options.linker = LINKER_MSVC;
        // options.linker = LINKER_GCC;
        options.executeOutput = true;
        // options.incremental_build = true;
        // options.disable_preload = true;
        // options.only_preprocess = true;
        options.useDebugInformation = true;
        Compiler compiler{};
        compiler.run(&options);

        if(compiler.options->compileStats.errors > 0)
            exit_code = EXIT_CODE_FAILURE;
        else
            exit_code = EXIT_CODE_SUCCESS;

        // VirtualMachine vm{};
        // vm.execute(compiler.bytecode,"main");

        // DynamicArray<std::string> tests;
        // tests.add("tests/simple/operations.btb");
        // tests.add("tests/flow/switch.btb");
        // tests.add("tests/funcs/overloading.btb");
        // tests.add("tests/simple/garb.btb");
        // tests.add("tests/flow/loops.btb");
        // tests.add("tests/what/struct.btb");
        // VerifyTests(&options, tests);
        
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
        VerifyTests(&options, tests);
        #else
        // if(options.target == TARGET_BYTECODE){
        //     options.executeOutput = true;
        //     // CompileAll(&options);
        // } else {
        //     #define EXE_FILE "dev.exe"
            
        //     // FuzzerOptions opts{};
        //     // opts.node_iterations_per_file = 5000;
        //     // opts.file_count = 20;
        //     // GenerateFuzzedFiles(opts,"main.btb");
            
        //     options.useDebugInformation = true;
        //     options.executeOutput = true;
        //     options.output_file = EXE_FILE;
        //     // CompileAll(&options);
        // }
        // compilerExitCode = options.compileStats.errors;
        #endif

        // return EXIT_CODE_SUCCESS;
    } else if(options.performTests) {
        if(options.pattern_for_files.length() != 0) {
            DynamicArray<std::string> tests;
            int matches = PatternMatchFiles(options.pattern_for_files, &tests);
            if(matches == 0) {
                log::out << log::RED << "The pattern '"<<options.pattern_for_files << "' did not match with any files\n";
            } else {
                int failures = VerifyTests(&options, tests);
                // compilerExitCode = failures;
                return failures != 0;
            }
        } else {
            // DynamicArray<std::string> tests;
            // tests.add("tests/simple/operations.btb");
            int failures = TestSuite(&options);
            // int failures = VerifyTests(tests);
            return failures != 0;
        }
    } else {
        Compiler compiler{};
        compiler.run(&options);

        // if(options.executeOutput) {
        //     switch(options.target) {
        //         case TARGET_BYTECODE: {
        //             VirtualMachine vm{};
        //             vm.execute(compiler.bytecode,"main");
        //         } break;
        //     }
        // }
        
        if(compiler.options->compileStats.errors > 0)
            exit_code = EXIT_CODE_FAILURE;
        else
            exit_code = EXIT_CODE_SUCCESS;
    }

    // ###### CLEANUP STUFF ######

    // log::out << "allocs "<<GetTotalNumberAllocations()<<"\n";

    options.cleanup();
    ProfilerCleanup();

    NativeRegistry::DestroyGlobal();
    int finalMemory = GetAllocatedBytes() - log::out.getMemoryUsage();
    // int finalMemory = GetAllocatedBytes() - log::out.getMemoryUsage() - Tracker::GetMemoryUsage() - MeasureGetMemoryUsage();
    if(finalMemory!=0){
        log::out << log::RED<< "Final memory: "<<finalMemory<<"\n";
        // PrintRemainingTrackTypes();
        GlobalHeapAllocator()->tracker.printAllocations();
    }

    // #ifdef TRACY_ENABLE
    // {
    //     ZoneNamedN(zone0,"sleep",true);
    //     engone::Sleep(0.5); // give time for program to connect and send data to tracy profiler
    // }
    // #endif

    // log::out << "Finished\n";

    auto main_time = StopMeasure(main_start);
    // log::out << "Main time: " << main_time<<"\n";

    return exit_code;
}
bool InterpretCommands(const DynamicArray<std::string>& commands, CompileOptions* options) {
    using namespace engone;
    if (commands.size() == 0) {
        print_version();
        log::out << log::GOLD << "The compiler suggests 'btb.exe -help'.\n";
        // print_help();
        options->quit = true;
        return true;
    }

    std::string path_to_exe = TrimLastFile(engone::GetPathToExecutable());
    std::string dir_of_exe = TrimLastFile(path_to_exe);
    // log::out << path_to_exe<<"\n";

    if(dir_of_exe.size() >= 5 &&
    dir_of_exe.substr(dir_of_exe.length()-5,5) == "/bin/") {
        // TODO: If exe exists in a bin directory then we assume the modules
        //   directory can be found in the parent directory. This is true
        //   in the repository right now but maybe not in the future.
        //   If a user wants to put their compiler executable in a bin
        //   folder along with the modules folder then they are out of luck.
        std::string without_bin = dir_of_exe.substr(0,dir_of_exe.size() - 4);
        options->modulesDirectory = without_bin + "modules/";
    } else {
        options->modulesDirectory = dir_of_exe + "modules/";
    }

    // TODO: User profiles
    // UserProfile* userProfile = UserProfile::CreateDefault();

    bool search_for_source = false;
    bool invalidArguments = false;

    for(int i=0;i<commands.size();i++){
        const std::string& arg = commands[i];
        // log::out << "arg["<<i<<"] "<<arg<<"\n";
        if(arg == "--help" || arg == "-help" || arg == "-?") {
            options->quit = true;
            print_help();
            return true;
        } else if (arg == "--run" || arg == "-r") {
            options->executeOutput = true;
        } else if (arg == "--preproc" || arg == "-p") {
            options->only_preprocess = true;
        } else if (arg == "--out" || arg == "-o") {
            i++;
            if(i<commands.size()){
                // TODO: Disallow paths that start with a dash since they resemble arguments
                options->output_file = commands[i];
            } else {
                invalidArguments = true;
                log::out << log::RED << "You must specify a file path after '-out'.\n";
            }
        } else if (arg == "-dev") {
            options->devmode = true;
        } else if (arg == "--test" || arg == "-ts") {
            options->performTests = true;
            options->instant_report = false;
            if(i+1 < commands.size() && commands[i+1][0] != '-'){
                i++;
                options->pattern_for_files = commands[i];
            }
        } else if (arg == "--test-with-errors" || arg == "-twe") {
            options->performTests = true;
            options->instant_report = true;
            if(i+1 < commands.size() && commands[i+1][0] != '-'){
                i++;
                options->pattern_for_files = commands[i];
            }
        } else if (arg == "--debug" || arg == "-d") {
            options->useDebugInformation = true;
        } else if (arg == "--incremental" || arg == "-i") {
            options->incremental_build = true;
        } else if (arg == "--stable-globals") {
            options->stable_global_data = true;
        } else if (arg == "--macro" || arg == "-m") {
            i++;
            if(i<commands.size()){
                options->defined_macros.add(commands[i]);
            } else {
                invalidArguments = true;
                log::out << log::RED << "You must specify a macro name after '"<<arg<<"'.\n";
            }
        } else if (arg == "--silent") {
            options->silent = true;
        } else if (arg == "--verbose") {
            options->verbose = true;
            log::out << log::RED << "Verbose option (--verbose) is not used anywhere yet\n";
        } else if (arg == "--profiling") {
            options->show_profiling = true;
        } else if (arg == "--target" || arg == "-t"){
            i++;
            bool print_targets = false;
            if(i<commands.size()){
                options->target = ToTarget(commands[i]);
                if(options->target == TARGET_UNKNOWN) {
                    invalidArguments = true;
                    log::out << log::RED << arg << " is not a valid target.\n";
                    // TODO: print list of targets
                    print_targets = true;
                }
            } else {
                invalidArguments = true;
                log::out << log::RED << "You must specify a target after '"<<arg<<"'.\n";
                // TODO: print list of targets
                // for(int j=0;j<){
                // }
                print_targets = true;
            }
            if (print_targets) {
                log::out << "These targets are available:\n";
                for(int j=TARGET_START;j<TargetPlatform::TARGET_END;j++) {
                    log::out << " " << ToString((TargetPlatform)j);
                }
            }
        } else if (arg == "--linker" || arg == "-l") {
            i++;
            bool print_linkers = false;
            if(i<commands.size()){
                options->linker = ToLinker(commands[i]);
                if(options->linker == LINKER_UNKNOWN) {
                    invalidArguments = true;
                    log::out << log::RED << arg << " is not a valid linker.\n";
                    // TODO: print list of targets
                }
            } else {
                invalidArguments = true;
                log::out << log::RED << "You must specify a linker after '"<<arg<<"'.\n";
                // TODO: print list of targets
            }
             if (print_linkers) {
                log::out << "These linkers are available:\n";
                for(int j=LINKER_START;j<LinkerChoice::LINKER_END;j++) {
                    log::out << " " << ToString((LinkerChoice)j);
                }
            }
        } else if (arg == "--pattern-match" || arg == "-pm") {
            search_for_source = true;
            i++;
            if(i<commands.size()){
                options->pattern_for_files = commands[i];
            } else {
                invalidArguments = true;
                log::out << log::RED << "You must specify a pattern after '"<<arg<<"'.\n";
            }
        } else if(arg == "--user-args" || arg == "-ua" || arg == "--") {
            i++;
            for(;i<commands.size();i++) {
                options->userArguments.add(commands[i]);
            }
        } else if(arg == "-nothreads") {
            options->disable_multithreading = true;
        } else if(arg == "-threads") {
            options->disable_multithreading = false;
        } else if(arg == "-nopreload") {
            options->disable_preload = true;
        } else {
            if(arg[0] == '-') {
                log::out << log::RED << "Invalid argument '"<<arg<<"' (see -help)\n";
                invalidArguments = true;
            } else {
                // arg = argv[i];
                // pattern_for_files = arg;
                if(options->source_file.size() != 0) {
                    log::out << log::RED << "You cannot specify initial source file twice ("<<arg<<")\n";
                    return false;
                }
                options->source_file = arg;
            }
        }
    }
    if(invalidArguments) {
        return false;
    }

    if(!options->devmode && !options->performTests && !search_for_source && options->source_file.empty()) {
        log::out << log::RED << "Specify a source file\n";
        return false;
    }

    if(search_for_source) {
        DynamicArray<std::string> files{};
        int num = PatternMatchFiles(options->pattern_for_files, &files);
        if(files.size() == 0) {
            log::out << log::RED << "Pattern '"<<options->pattern_for_files<<"' did not match any files\n";
            return false;
        } else if(files.size() > 1) {
            log::out << log::RED << "Pattern '"<<options->pattern_for_files<<"' matched more than one file:\n";
            for(int i=0;i<files.size();i++) {
                log::out << " "<<files[i]<<"\n";
            }
            return false;
        } else {
            options->source_file = files[0];
            // for(int i=0;i<files.size();i++) {
            //     log::out << log::GRAY<< " "<<files[i]<<"\n";
            // }
        }
    }

    if(options->only_preprocess && options->source_file.size() == 0) {
        // this should never run because we detect missing source file earlier
        log::out << log::RED << "You must specify a file when using --preproc\n";
    }

    return true;
}