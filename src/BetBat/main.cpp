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

// returns true if no command was matched or if command should be executed together with the compiler
// returns false if program should quit, or if something failed
bool CheckDeveloperCommand(const BaseArray<std::string>& args);

int main(int argc, const char** argv){
    using namespace engone;
    #define EXIT_CODE_SUCCESS 0
    #define EXIT_CODE_FAILURE 1

    auto main_start = StartMeasure();

    InitAssertHandler();
    ProfilerInitialize();
    
    DynamicArray<std::string> arguments{};
    for(int i=1;i<argc;i++) // is the first argument always the executable?
        arguments.add(argv[i]);

    // arguments.add("decode");
    // arguments.add("wa.o");

    bool dev_cmd_success = CheckDeveloperCommand(arguments);
    if(!dev_cmd_success) {
        // error message should have been printed
        return 1;
    }

    // NOTE: I have not moved InterpretCommands and compiler option parsing to it's own file
    //   it's rather temporary. In the future, you will specify compile options such as
    //   target, debug, output path, incremental linking, and linker exe in a build.btb script.
    //   I would rather implement the real solution than mess around with compile options here.
    CompileOptions options{};
    bool valid = InterpretArguments(arguments, &options);

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
        // options.output_file = "main.elf";
        options.source_file = "examples/dev.btb";
        options.threadCount = 1;
        // options.disable_multithreading = false;
        // options.target = TARGET_BYTECODE;
        options.target = TARGET_WINDOWS_x64;
        // options.linker = LINKER_MSVC;
        // options.linker = LINKER_GCC;
        // options.target = TARGET_ARM;
        options.executeOutput = true;
        // options.incremental_build = true;
        // options.disable_preload = true;
        // options.only_preprocess = true;
        options.useDebugInformation = true;
        // options.debug_qemu_with_gdb = true;
        // options.useDebugInformation = false;
        Compiler compiler{};
        compiler.run(&options);

        if(compiler.compile_stats.errors > 0)
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
        // compilerExitCode = compiler.compile_stats.errors;
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
        
        if(compiler.compile_stats.errors > 0)
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
bool CheckDeveloperCommand(const BaseArray<std::string>& args) {
    using namespace engone;
    auto contains = [&](const BaseArray<std::string>& arr, const std::string& str) {
        int index = -1;
        for(int i=0;i<arr.size();i++) {
            if(arr[i] == str) {
                index = i;
                break;
            }
        }
        return index;
    };
    
    // TODO: Should developer commands be described in help messages?
    int index = contains(args, "decode");
    if(index != -1) {
        if(args.size() > index + 1) {
            std::string path = args[index + 1];
            FileCOFF::Destroy(FileCOFF::DeconstructFile(path, false));
            return false;
        } else {
            log::out << log::RED << "You forgot an argument after 'decode'. If this message comes as a suprise, 'decode' is a special command for developers which deconstructs and prints the content of COFF files. \n";
            return false;
        }
    }

    // args do not contain a developer command

    return true;
}