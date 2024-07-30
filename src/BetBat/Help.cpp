#include "BetBat/Help.h"

#include "BetBat/Compiler.h"
#include "Engone/Logger.h"

void print_version(){
    using namespace engone;
    char buffer[100];
    CompilerVersion version = CompilerVersion::Current();
    version.serialize(buffer, sizeof(buffer),CompilerVersion::INCLUDE_AVAILABLE);
    log::out << "BTB Compiler, version: " << log::LIME<< buffer <<"\n";
    log::out << log::GRAY << "(major.minor.patch.revision/name-year.month.day)\n";
    // log::out << log::GRAY << " released "<<version.year << "-"<<version.month << "-"<<version.day <<" (YYYY-MM-DD)";
}
void print_help(){
    using namespace engone;

    #define HEADER(X) log::out << log::BLUE << "# " << X << "\n";
    #define USAGE(X) log::out << log::YELLOW << X << ": " << log::NO_COLOR;
    #define DESC(X) log::out << X;
    #define END log::out << "\n";
    
    log::out << log::GRAY << "More information can be found here:\n"
        "https://github.com/Emarioo/BetterThanBatch/tree/main/docs\n";

    // log::out.enableWrapping(true);
    // defer { log::out.enableWrapping(true); };

    HEADER("Compiling")
    USAGE("btb <file>")
    DESC(
        "Without flags, the compiler will take one file as input and generate an executable "
        "based on the operating system and linker you have. The compiler generates it's own object files "
        "but you must install one of these linkers or C/C++ compilers: g++, clang, link (MSVC). "
        "On Windows, you can install Visual Studio and setup the environment variables so that 'link' is available.\n"
    )
    END
    
    USAGE("-o,--out <file>")
    DESC("Specifies where the executable should be placed. If <file> ends with .o (or .obj), then just the object file will be generated skipping the linker.\n")
    END

    USAGE("-l,--linker <linker>")
    DESC("Specifies the linker to use. These are available: ")
    log::out << log::LIME;
    for(int i=LINKER_START;i<LINKER_END;i++) {
        if(i!=LINKER_START)
            log::out << ", ";
        log::out << ToString((LinkerChoice)i);
    }
    log::out << "\n";
    END

    USAGE("-t,--target <target-platform>")
    DESC("Specifies the target platform to compile for. The default is the same as what the compiler executable was compiled for. Also, compiling for Unix on Windows wouldn't work because of the linker. With a special linker maybe but you can only use a predefined linker.\n")
    log::out << "These are available: " << log::LIME;
    for(int i=TARGET_START;i<TARGET_END;i++) {
        if(i!=TARGET_START)
            log::out << ", ";
        log::out << ToString((TargetPlatform)i);
    }
    log::out << "\n";
    END

    #define GREEN(X) log::LIME << X << log::GRAY

    USAGE("-pm,--pattern-match <pattern>")
    DESC("Allows you to specify a pattern instead of a specific file for compilation. Compiler will reject patterns that matched more than one file.\n")
    log::out << "'|' separates rules. Each rule is a string with characters and wildcards '*'\n";
    log::out << log::GRAY << " Match file extensions: " << GREEN("*.h") << ", "<< GREEN("*.h|*.cpp") << "\n";
    log::out << log::GRAY << " Match directories: "<<GREEN("*/src/*")<<"\n";
    log::out << log::GRAY << " Exclude directories: "<<GREEN("!*/libs/*")<<", "<<GREEN("!*/bin/*|!*/res/*")<<" (.vs, .git, .vscode are excluded by default)\n";
    END

    USAGE("-ua,--user-args")
    DESC("Will pass arguments to the executable. Can only be used with '--run'. All arguments after this flag will be sent to the compiled program instead of the compiler.\n")
    END

    USAGE("-r,--run")
    DESC("Will run the executable after it's been compiled.\n")
    END

    USAGE("--incremental")
    DESC("Compiles source code if any file changed since last compilation.\n")
    END

    USAGE("--stable-globals")
    DESC("Mainly used with hotreloading where the global data is heap allocated to make it non-volatile. Code that refers to global variables will instead use the allocated memory. The global data remains in an initialized state.\n")
    END

    USAGE("-m,--macro")
    DESC("Defines an empty macro visible in all source files\n")
    END

    HEADER("Debugging")

    USAGE("-d,--debug")
    DESC("Will compile with debug information (DWARF). Note that MSVC linker doesn't work with DWARF. You must use g++ or other linker. PDB for Windows is not implemented yet.\n")
    log::out << log::GRAY<<"TODO: -d=DWARF, -d=PDB\n";
    END

    USAGE("-p,--preproc <file>")
    DESC("Will run the preprocesser on the specified file phase and output the result to console (stdout).\n")
    END

    USAGE("--silent")
    DESC("reserved.\n")
    END
    
    
    USAGE("--verbose")
    DESC("reserved.\n")
    END

    USAGE("--profiling")
    DESC("Displays time measurements of the internal parts of the compiler.\n")
    END

    HEADER("Testing")

    USAGE("-ts,--test [pattern]")
    DESC("Will run tests on all files that matched the [pattern]. Some default files will be chosen if a pattern wasn't supplied. See '--pattern-match' for syntax of pattern.\n")
    END
    
    USAGE("-twe,--test-with-errors [pattern]")
    DESC("Same as '--test' but error messages will be printed to stdout. In the future, stderr may be more convenient.\n")
    END

    #undef HEADER
    #undef USAGE
    #undef DESC


    // #define PRINT_USAGE(X) log::out << log::YELLOW << X ": "<<log::NO_COLOR;
    // #define PRINT_DESC(X) log::out << X;
    // #define PRINT_EXAMPLES log::out << log::LIME << " Examples:\n";
    // #define PRINT_EXAMPLE(X) log::out << X;

    // PRINT_USAGE("compiler.exe [file0 file1 ...]")
    // PRINT_DESC("Arguments after the executable specifies source files to compile. "
    //          "They will compile and run seperately. Use #import inside the source files for more files in your "
    //          "projects. The exit code will be zero if the compilation succeeded; non-zero if something failed.\n")
    // PRINT_EXAMPLES
    // PRINT_EXAMPLE("  compiler.exe file0.btb script.txt\n")
    // log::out << "\n";
    // PRINT_USAGE("compiler.exe [file0 ...] -out [file0 ...]")
    // PRINT_DESC("With the "<<log::WHITE<<"out"<<log::WHITE<<" flag, files to the left will be compiled into "
    //          "bytecode and written to the files on the right of the out flag. "
    //          "The amount of files on the left and right must match.\n")
    // PRINT_EXAMPLES
    // PRINT_EXAMPLE("  compiler.exe main.btb -out program.btbc\n")
    // log::out << "\n";
    // PRINT_USAGE("compiler.exe --run [file0 ...]")
    // PRINT_DESC("Runs bytecode files generated with the out flag.\n")
    // PRINT_EXAMPLES
    // PRINT_EXAMPLE("  compiler.exe -run program.btbc\n")
    // log::out << "\n";
    // PRINT_USAGE("compiler.exe --target <target-platform>")
    // PRINT_DESC("Compiles source code to the specified target whether that is bytecode, Windows, Linux, x64, object file, or an executable. "
    //         "All of those in different combinations may not be supported yet.\n")
    // PRINT_EXAMPLES
    // PRINT_EXAMPLE("  compiler.exe main.btb -out main.exe -target win-x64\n")
    // PRINT_EXAMPLE("  compiler.exe main.btb -out main -target linux-x64\n")
    // PRINT_EXAMPLE("  compiler.exe main.btb -out main.btbc -target bytecode\n")
    // log::out << "\n";
    // PRINT_USAGE("compiler.exe -user-args [arg0 ...]")
    // PRINT_DESC("Only works if you run a program. Any arguments after this flag will be passed to the program that is specified to execute\n")
    // PRINT_EXAMPLES
    // PRINT_EXAMPLE("  compiler.exe main.btb -user-args hello there\n")
    // PRINT_EXAMPLE("  compiler.exe -run main.btbc -user-args -some-flag \"TO PASS\"\n")
    // log::out << "\n";
    // #undef PRINT_USAGE
    // #undef PRINT_DESC
    // #undef PRINT_EXAMPLES
    // #undef PRINT_EXAMPLE

}