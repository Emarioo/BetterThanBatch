# BetterThanBatch
A compiler for a new programming language.

- Good for your every day scripting needs.
- Useful for visualizing data structures, programs...?
- Standard library with essentials for platform independent programs (graphics, audio, file formats, networking)
- Fast compiler, good error messages, no external build system.

Inspiration from:
- Jai (by Jonathan Blow)
- C/C++
- and a bit of Rust

Example of errors with named arguments
![](/docs/img/err-named-arg.png)

Example of unfinished multiplayer game
![](/docs/img/game_blobs_0.png)

**DISCLAIMER**: The compiler is not ready for serious projects. Both because the bug list isn't empty but also because the language isn't complete. New features will be added and old features will be removed.

## State of the compiler/language
|Project|Status|
|-|-|
|Windows build|Yes|
|Linux (Ubuntu) build|Mostly works, still fixing things|
|Documentation|Half complete guide|
|x86-64|Yes|
|ARM-64|No|
|Test cases|Covers simple cases|

|Features|Status|
|-|-|
|Preprocessor|Yes, #macro, #if, #line, #file|
|Polymorphism|Yes, but some issues with matching overloaded functions|
|Function overloading|Yes|
|Operator overloading|Yes|
|Namespaces|No, incomplete|
|Linking with external functions|Yes, with #load and @import but could be better|
|Type information|Yes, but needs a refactor/redesign|
|Compile time execution|Global variables are computed at compile time, nothing else yet|
|x64 backend|Yes|
|Debug information|Only DWARF|
|x64 inline assembly|Yes|

# How to get started
**NOTE**: There are no releases yet so you have to compile it yourself. The compiler is just not stable enough yet.

**Option 1:** Clone the repo and build the compiler executable yourself, see [Building](#building). Then have a look at [A small but very important guide](/docs/guide/00-Introduction.md).

**Option 2 (coming soon):** Download a release of `BTB Compiler` at https://github.com/Emarioo/BetterThanBatch/releases. Then unzip it in a folder of your choice and edit your environment variable `PATH` with the path to the compiler executable (for convenience). Then have a look at [A small but very important guide](/docs/guide/00-Introduction.md).

<!-- TODO: Swap option 1 and 2 so that download release is first option, the recommended option. Compiling project is first option right now because there are no releases -->

# Building
The project is built using the Python script `build.py`. Python version 3.9 and higher should work without problems. Once compiled, the executable can be found in `bin/btb.exe`. I recommend editing the environment variable `PATH` so that you have access to `btb.exe` from anywhere.

## Linux
**Requirements**: `g++`

Make sure `g++` is available and run `python3 build.py` in a terminal.

## Windows
**Requirements**: `cl` and `link` OR `g++`.

If you plan to compile the project once, I would recommend installing MinGW to get `g++` because it's easy. Then you can run `python build.py gcc` in a terminal. This may take between 30 and 60 seconds for a complete rebuild (gcc is slow).

If you plan on compiling the project often, I would recommend using Visual Studio Developer Tools (cl, link) because it compiles the project quicker than GCC (6-12 seconds for a complete rebuild). Before you can run `python build.py msvc` in a terminal, you must make sure `cl` and `link` are available. You can do this by installing Visual Studio and in the launcher selecting something named `C/C++ desktop development` (if you have used C++ in Visual Studio then you probably have this). Then hit the windows key and search for `VS Developer Command Prompt`. This opens a terminal, type `cl` and you'll see that it's available. Now you need to cd into the project where `build.py` is and run `python build.py msvc`.

**NOTE:** Compiling the project with Visual Studio may seem like a lot of work but you just have to open the VS developer terminal once. If you use VSCode as your editor than you can type `code` in the VS terminal and all terminals inside VSCode will have `cl` available. Another alternative is to add vcvars64.bat (look it up on the internet what it is) to your PATH and then you can run that in any terminal to make `cl` available. Personally, I have made `cl` available at all times by manually making the changes vcvars64 would do to environment variables.

## Extra
If you want to compile with debug information, tracy profiler or optimizations then you can toggle these options inside `build.py`. You can also pass them as arguments to the script like this: `python build.py use_debug use_optimizations use_tracy`.

You can change the path of the executable like this: `python build.py output=bin/release/btb.exe`.
All object files will still end up in `bin`, this cannot be changed at the moment.

# Examples
**NOTE:** These have been tested on my computer with my environment which means that it might not work on your computer. You may need to clone the repo, build the compiler with MSVC, and run `btb.exe --run examples/graphics/quad.btb` to make it work. You have to build the repo because there is no release yet.

[Rendering test](/examples/graphics/quad.btb) - Rendering with GLEW, GLFW, and OpenGL

[Multiplayer game](/examples/graphics/game.btb) - Rendering textures and text, simple collision and player movement, sockets and networking.

[Line counter](/examples/linecounter.btb) - Reading files using multiple threads. (probably broken)

<!-- incomplete [Binary viewer](/examples/binary_viewer/main.btb) parsing/reading binary files, lexing -->

