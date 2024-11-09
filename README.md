# BetterThanBatch
A compiler for a programming language.

- Good for your every day scripting needs.
- Useful for visualizing data structures, programs...?
- Standard library with essentials for platform independent programs (graphics, audio, file formats, networking)
- Fast compiler, good error messages, no external build system.

Inspiration from:
- C/C++
- Jai by Jonathan Blow
- and a bit of Rust

Also, many thanks to Jonathan Blow for his amazing and motivating content.

Example of errors with named arguments
![](/docs/img/err-named-arg.png)

Example of unfinished multiplayer game
![](/docs/img/game_blobs_0.png)

**DISCLAIMER**: The compiler is not ready for serious projects. Both because the bug list isn't empty but also because the language isn't complete. New features will be added and old features will be removed.

## State of the compiler/language
|Part|Status|
|-|-|
|Windows build|Yes|
|Linux (Ubuntu) build|Yes|
|Documentation|Half complete guide|
|x86-64|Yes|
|ARM|Work in progress|
|Test cases|Covers simple cases|

|Feature|Status|
|-|-|
|Preprocessor|Yes, #macro, #if, #line, #file|
|Polymorphism|Yes, but some issues with matching overloaded functions|
|Multi-threading in the compiler|Work in progress|
|Function overloading|Yes|
|Operator overloading|Yes|
|Namespaces|No, incomplete|
|Linking with external functions|Yes, with #load and @import but could be better|
|Type information|Yes, but needs a redesign|
|Compile time execution|Global variables are computed at compile time, nothing else yet|
|Custom x64 backend|Yes, llvm is not used resulting in a fast but unoptimized compilation|
|Debug information|Only DWARF|
|x64 inline assembly|Yes|
|Software/hardware exception handling|Only Windows, Linux is WIP|

# How to get started

## Downloading a release
You can download a release of the *BTB Compiler* at https://github.com/Emarioo/BetterThanBatch/releases.
- First download the zip.
- Then unzip in a folder of your choice.
- (optional) Edit your environment variable `PATH` to contain the path to the compiler executable.
- You also need a linker because the compiler produces object files. You can use gcc, clang, or msvc (Microsoft Visual C/C++ Compiler). I would recommend looking up a tutorial on Youtube on how to install them. If you run into `gcc` not found then you haven't set up your environment variables. If you run into `link` for msvc then you must run VS Developer Command Prompt terminal or vcvars64.bat (find a youtube tutorial).
- Try running
- All that's left is taking a look at the guide for the language: [A small but very important guide](/docs/guide/00-Introduction.md).

**Option 2:** Clone the repo and build the compiler executable yourself, see [Building](#building). Then have a look at [A small but very important guide](/docs/guide/00-Introduction.md).


# Building
**Requirements** `python`

The project is built using the Python script `build.py`. Python version 3.9 and higher should work without problems. Once compiled, the executable can be found in `bin/btb.exe`. I recommend editing the environment variable `PATH` so that you have access to `btb.exe` from anywhere.

## Linux
**Requirements**: `g++` or `clang++`

Make sure `g++` or `clang++` is available and run `python3 build.py`. If you want to use a specific compiler then do `python3 build.py clang` in a terminal. (gcc is the default)

On Ubuntu you can install `g++` using `sudo apt install gcc`.

## Windows
**Requirements**: `cl/link`, `clang++` or `g++`.

If you plan to compile the project often than `cl/link` commands (msvc, Microsoft Visual C/C++) are recommended because they compile the project faster than the others. If you compile the project once in a while then `clang++` or `g++` are also viable options. Then you can run `python build.py` in a terminal. You can specify a specific compiler like this: `python build.py gcc` (msvc is the default).

For a full rebuild, compiling may take between 5 and 10 seconds with msvc. gcc and clang++ can take around 30-60 seconds (30 seconds on newer computers).

### Installing msvc
- Install Visual Studio.
. In the Visual Studio Installer, select and install something named `C/C++ desktop development` (if you have used C++ in Visual Studio before then you probably have this).
- Once installed hit the windows key and search for `VS Developer Command Prompt`.
- That opens a terminal in which you type `cl` where it print prints out the Microsoft C/C++ compiler version.
- Now inside the terminal you need to cd into the BetterThanBatch project where `build.py` is and run `python build.py`.
- (vscode) If you have Visual Studio Code then you can type `code` in the *VS Developer* terminal. From there you can `Open Folder` and choose the BetterThanBatch folder. Then in VSCode `Create New Terminal` and `cl` should be available.
- (vcvars64.bat) If you want access to `cl` without starting *VS Developer* terminal then you can run vcvars64.bat. It can be found in: `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build` but it depends on where you installed Visual Studio (and which version). The full command you have to type in the terminal is `"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"`. If you plan on doing this then I would recommend adding the path to Build to the environment variable PATH.
<!-- This needs more explaination.
- (manually adding environment variables from vcvars64) If you are tired of *VS Developer* and *vcvars64.bat* then you can check the content and which environment paths you have before running vcvars64.bat and compare them with the ones you have after running vcvars64.bat. Then you add the missing content to environment variables permanently.
-->

**NOTE:** Compiling the project with Visual Studio may seem like a lot of work but you just have to open the VS developer terminal once. If you use VSCode as your editor than you can type `code` in the VS terminal and all terminals inside VSCode will have `cl` available. Another alternative is to add vcvars64.bat (look it up on the internet what it is) to your PATH and then you can run that in any terminal to make `cl` available. Personally, I have made `cl` available at all times by manually making the changes vcvars64 would do to environment variables.
### Installing gcc
https://www.mingw-w64.org/downloads/#mingw-builds

### Installing clang
**TODO:** I have never installed clang on Windows before. So I don't know. I should test it out though.


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

# TODO
A temporary todo list because I don't have access to internet at the moment...

- Write a script that takes out and compiles code snippets from .md files. Perhaps we can include it in `btb --test` somehow. We don't need to check that the snippets to what they are supposed to. It is enough to know that they compile without errors. The other tests should cover all functionality in the snippets.
- The code below does not print a good error message:
    ```c++
    fn hi(a: i32, b: i32) {}
    hi(5, b = 23) // named argument not allowed
    ```
    
- Tests that fail should print the expected value and the incorrect value (if possible). This could be hard but it's good information.