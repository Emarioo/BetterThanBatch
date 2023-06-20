# BetterThanBatch
A compiler and interpreter for a new programming language.

It will be better than batch for your average scripting needs.
It will be useful for making neat programs, exploring data structures
and visualising data using a straight forward rendering
library (abstraction of Vulkan or OpenGL).

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
- Polymorphism in structs, functions and methods.
- Usage of C++ functions within a script
- Calling executables like gcc.exe
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
[Random code](examples/ast.btb)

# Usage
You may want to read [Building](#building) first.

The official usage of the compiler isn't established yet.
You can at least do `compiler yourfile.btb` to compile and run the initial file.
`compiler --help` may provide more options.

Here is a short and quick [Guide](docs/guide.md) (also not up to date, sorry).

# Building
## Windows
### Building with vcvars (with Visual Studio installed)
First you need Visual Studio installed and then you should
find vcvars64 in `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build`.
The exact directory depends on the version and location of your installation.

Run `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat`
to setup the commands cl and link.
Then run `build.bat` which will generate an executable in the bin directory.

I would recommend adding the directory of vcvars64 in your user environment variable PATH.
This allows you to type `vcvars64.bat` instead of the long directory path.

### Building with g++ (MinGW or CYGWIN)
First you need to install MinGW-64 (or CYGWIN?).
You can modify `build.bat` to compile with g++ instead of cl (MSVC).
This is done by removing `SET USE_MSVC=1` and adding (or uncommenting) `SET USE_GCC=1`.
Then simply build with `build.bat`. The compiler can once again be found in bin.

## Linux
Tested with Windows Subsystem for Linux. You may therefore experience
some bugs that haven't been found when using Linux for real.

Build with `build.sh`. The compiler can be found in the bin directory found in bin.
