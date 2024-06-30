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

**Option 1:** Clone the repo and build the compiler executable yourself, see [Building](#building). Then have a look at [A little guide](/docs/guide/00-Introduction.md).

**Option 2 (coming soon):** Download a release of `BTB Compiler` at https://github.com/Emarioo/BetterThanBatch/releases. Then unzip it in a folder of your choice and edit your environment variable `PATH` with the path to the compiler executable (for convenience). Then have a look at [A little guide](/docs/guide/00-Introduction.md).

<!-- TODO: Swap option 1 and 2 so that download release is first option, the recommended option. Compiling project is first option right now because there are no releases -->

# Building

## Linux
**Requirements**: `g++`

Run `build.sh`. Once built, the executable can be found in `bin/btb`. I recommend editing environment variable `PATH` so that you have access to `btb` from anywhere.

## Windows
**Requirements**: `cl` and `link` OR `g++`.

In `build.bat`, comment/uncomment the way you want to build the compiler.
If you are using Visual Studio Tools (cl, link) then open the VS Developer Command Prompt and `cd` to the cloned repository. Then run `build.bat`.
If you are using MinGW and `g++` then you can simply run `build.bat` (as long as `g++` is in your PATH env. variable).

Once built, the executable can be found in `bin/btb.exe`. I recommend editing environment variable `PATH` so that you have access to `btb.exe` from anywhere.

# Examples
**NOTE:** These have been tested on my computer with my environment which means that it might not work on your computer. You may need to clone the repo, build the compiler with MSVC, and run `compiler.exe --run examples/graphics/quad.btb` to make it work. You have to build the repo because there is no release yet.

[Rendering test](/examples/graphics/quad.btb) - Rendering with GLEW, GLFW, and OpenGL

[Multiplayer game](/examples/graphics/game.btb) - Rendering textures and text, simple collision and player movement, sockets and networking.

[Line counter](/examples/linecounter.btb) - Reading files using multiple threads. (probably broken)

<!-- incomplete [Binary viewer](/examples/binary_viewer/main.btb) parsing/reading binary files, lexing -->


