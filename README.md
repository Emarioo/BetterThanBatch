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

**NOTE:** A rewrite of the compiler is in progress. See rewrite-2.1 for the latest work. The dev branch is the most stable however.

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
- Some examples: Astroids like game

## Disclaimer
The compiler is not ready for use in serious projects. Both because the bug list isn't empty but also because the language isn't complete. New features will be added, old features will be removed and so on. Recently, a convenient system for testing was implemented which has ensured that the compiler is working in simple situations.

You can compile the compiler for Windows and Ubuntu (probably most Unix systems, only tested on Ubuntu though).

The documentation and guide is being worked on but there are examples and `examples/dev.btb` usually contain the new additions in each commit.

## Guide
[Full Guide](/docs/guide/00-Introduction.md) (work in progress).

## Examples
**NOTE:** These have been tested on my computer with my environment which means that it might not work on your computer. You may need to clone the repo, build the compiler with MSVC, and run `compiler.exe --run examples/graphics/quad.btb` to make it work. You have to build the repo because there is no release yet.

[Rendering test](/examples/graphics/quad.btb) rendering with GLEW, GLFW, and OpenGL

[Line counter](/examples/linecounter.btb) reading files, multiple threads

[Binary viewer](/examples/binary_viewer/main.btb) parsing/reading binary files, lexing

[Recent random code](/examples/dev.btb)

# Usage
The official usage of the compiler has not been established yet. `btb --help` will show the current way of compiling files and describe useful flags.

These commands will most likely work.
`btb main.btb --run` (`--run` will also run the executable after it's been built)


# Building
On Linux/Unix you only to run `build.sh`.

On Windows however, you need to go into `build.bat` and comment or uncomment a variable at the top depending on whether you are compiling using `g++` or with Visual Studio tools (MSVC). Then you can run the script.

The compiler executable can be found at this path `bin/btb.exe` (skip the `.exe` on Unix).

**NOTE**: The script assumes that `link` and `cl` (with MSVC) are accesible from the command line. Running `vcvars64.bat` will set the appropriate environment variables (vcvars can be found here `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build`). Personally, I have set the environment variables manually instead of having vcvars64.bat do it everytime because the script can be rather slow.

**NOTE**: The Linux/Unix version is not tested after every commit but you can expect every 5-10 commits to be tested with Ubuntu.
