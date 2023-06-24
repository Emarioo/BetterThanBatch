# BetterThanBatch
A compiler and runtime for a new programming language.
Inspiration from:
- Jai (by Jonathan Blow)
- Batch file (not for the reason you think)
- C
- A bit of Rust

## Disclaimer
The documentation is old, minimal and outright lacking.
This is due to me prioritising the features and stability
of the compiler. Once some of the hard features that would
require major alterations to the compiler's architecture if
not done early has been completed, partly at least, then
I shall spend time on examples, documentation and most
importantly proper testing.

## Features
- Variables, functions, namespaces, structs, enums.
- If, for, while, break, continue, return, defer, using...
- #define, #multidefine and #undef (macros/defines are recursive)
- #ifdef (exactly like C)
- #unwrap (for macros)
- Concatenation with .. in macros.
- #import to divide your code into multiple files
- #include to tokenize a file and transfer the tokens into another file.

## On the way
- Usage of C++ functions within a script
- Calling executables like gcc.exe
- Polymorphism
- Operator and function overloading
- Thorough documentation.

## The processs
The name of each step may not accurately represent what's
actually happening. Type checker infers and creates types for
structs, enums and functions. The generator also does some
more detailed type checking with expressions and when the
values are put in registers and pushed to the stack.

Some preprocessing is done after the parser like
\_FUNCTION\_ which isn't know until after parsing.

- Tokenizer     (text -> tokens)
- Preprocessor  (tokens -> tokens)
- Parser        (tokens -> AST)
- Type checker  (modifies types in AST)
- Generator     (AST -> bytecode)
- Optimizer     (bytecode -> faster and smaller bytecode)
- Interpreter   (runs the bytecode)

## Examples
You can find some examples in the example folder.
[Random code](examples/ast.btb) (some are broken and not up to date)

# Usage
You may want to read [Building](#building) first.

The official usage of the compiler has not been established yet.
You can at least do `compiler yourfile.btb` to compile and run the initial file.
`compiler --help` may provide more options.

Here is a short and quick [Guide](docs/guide.md) (also not up to date, sorry).

# Building
## Windows
### Building with vcvars (with Visual Studio installed)
First you need Visual Studio installed. Then you need to find vcvars64 in
`C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build`.
The exact directory depends on the version and location of your installation.

Clone the repository through git or download the files from github.
Start a new terminal inside the project (the folder where `build.bat` exists).
Type in the path to `vcvars64.bat`. This will setup the command cl which is
used to compile the compiler.
Then run `build.bat` which will generate an executable in the bin directory.
That is the compiler compiled and ready for compilation.

I would recommend adding the directory of vcvars64 in your user environment variable PATH.
With this, you can type `vcvars64.bat` instead of the long directory path.

### Building with g++ (MinGW or CYGWIN)
First you need to install MinGW-64 (or CYGWIN?).
You can modify `build.bat` to compile with g++ instead of cl (MSVC).
This is done by removing `SET USE_MSVC=1` and adding (or uncommenting) `SET USE_GCC=1`.
Then simply build with `build.bat`. The compiler can be found in the folder bin.

## Linux
Tested with Windows Subsystem for Linux. You may therefore experience
some bugs that have not been found when using Linux for real.

Clone or download files from the repository.
Build with `build.sh`. The compiler can be found in the bin directory found in bin.
