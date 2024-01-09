# BetterThanBatch
A compiler for a new programming language.

It will be better than batch for your average scripting needs.
It will be useful for making neat programs, exploring data structures
and visualising data using a straight forward rendering
library (abstraction of Vulkan or OpenGL).

Inspiration from:
- Jai (by Jonathan Blow)
- Batch file (not for the reason you think)
- C/C++
- and a bit of Rust

Example of errors with named arguments
![](/docs/img/err-named-arg.png)

## Where is the focus
- Actually good somewhat complete standard library (graphics, audio, file formats, networking)
- Fast compiler with a smooth user experience. You don't need to setup a project folder and a build system to compile a single file.
- Code execution at compile time.

## Features
- #import to divide your code into multiple files
- #include to tokenize a file and transfer the tokens into another file.
- Polymorphism in structs, functions and methods (there are some bugs in advances scenarios)
- Function and operator overloading
- x64 code generator (object files)
- Compiling with debug information (DWARF only)
- Linking with C/C++ functions from libraries and object files (symbols and relocations)
- #define, #multidefine, #undef, #unwrap (macros/defines are recursive)
- #ifdef (exactly like C)
- defer, using.
- Type information in the code
- Compiler bugs that waste your time.

## On the way
- Shell-like way of calling executables
- Thorough documentation
- Constant evaluation and compile time execution

## Disclaimer
The compiler has mysterious bugs and as such is very unstable and should not be used for serious projects. Recently, proper tests have been implemented and some major bugs have been resolved making the compiler a little more useful.

You can compile the compiler for Windows and Ubuntu (probably most Unix systems, only tested on Ubuntu though).

The documentation and guide is being worked on but there are examples and `examples/dev.btb` usually contain the new additions in each commit.

## Examples and guide
You can find some examples in the examples folder. Some examples may be old and not work, sorry.

[Rendering test](/examples/graphics/quad.btb) with GLEW, GLFW, and OpenGL

[Line counter](/examples/linecounter.btb) with multiple threads

[Binary viewer](/examples/binary_viewer/main.btb) with multiple threads

[Recent random code](/examples/dev.btb)

Here is a thorough [Full Guide](/docs/guide/00-Introduction.md) (work in progress).

# Usage
You may want to read [Building](#building) first.

The official usage of the compiler has not been established yet.
You can at least do `btb yourfile.btb` to compile and run the initial file.
`btb --help` may provide more options.


# Building
First of all, the project isn't compiled with CMAKE or Visual Studio.
It uses shell scripts to compile as a unity build. This causes the least amount of headaches.
(may change in the future)
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
Tested with Ubuntu every 5-10 commits.

Clone or download files from the repository.
Build with `build.sh`. The compiler can be found in the bin directory found in bin.
