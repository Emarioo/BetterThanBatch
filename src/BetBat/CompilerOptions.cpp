#include "BetBat/CompilerOptions.h"

#include "BetBat/Util/Utility.h"
#include "BetBat/Help.h"

bool InterpretArguments(const BaseArray<std::string>& commands, CompileOptions* options) {
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
        } else if (arg == "--run-vm" || arg == "-vm") {
            options->execute_in_vm = true;
        } else if (arg == "--int-vm" || arg == "-ivm") {
            options->interactive_vm = true;
            options->execute_in_vm = true;
        } else if (arg == "--log-vm" || arg == "-pvm") { // not abbreviated lvm to avoid confusing with llvm
            options->logged_vm = true;
            options->execute_in_vm = true;
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
        } else if (arg == "--cache-tests" || arg == "-ct") {
            options->cache_tests = true;
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
            // options->verbose = true;
            global_loggingSection = (LoggingSection)(LOG_TASKS);
            // log::out << log::RED << "Verbose option (--verbose) is not used anywhere yet\n";
        // } else if(arg == "--verbose") {
// | LOG_TASKS
// | LOG_BYTECODE

// | LOG_TOKENIZER
// | LOG_PREPROCESSOR
        } else if (arg == "--profiling") {
            options->show_profiling = true;
        } else if (arg == "-idr"||arg=="--import-dir") {
            i++;
            if(i<commands.size()) {
                options->importDirectories.add(commands[i]);
            } else {
                invalidArguments = true;
                log::out << log::RED << "You must specify an import directory after '"<<arg<<"'.\n";
            }
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
