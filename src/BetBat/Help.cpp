#include "BetBat/Help.h"

#include "BetBat/Compiler.h"
#include "Engone/Logger.h"

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
        "https://github.com/Emarioo/BetterThanBatch/tree/main/docs\n";
    #define PRINT_USAGE(X) log::out << log::YELLOW << X ": "<<log::NO_COLOR;
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
    PRINT_USAGE("compiler.exe --run [file0 ...]")
    PRINT_DESC("Runs bytecode files generated with the out flag.\n")
    PRINT_EXAMPLES
    PRINT_EXAMPLE("  compiler.exe -run program.btbc\n")
    log::out << "\n";
    PRINT_USAGE("compiler.exe --target <target-platform>")
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