# BetterThanBatch
A compiler for a programming language.

- Good for your every day scripting needs.
- Useful for visualizing data structures, programs...?
- Standard library with essentials for platform independent programs (graphics, audio, file formats, networking)
- Fast compiler, good error messages, no external build system.


# Features
See the [Guide](/docs/guide/00-Introduction.md) for details.

- **Statically typed** - All data types in the program language are statically typed. No dynamic objects.
- **Imports** - No headers, no forward declarations. Circular dependencies between imports are allowed.
- **Function overloading** - Very powerful in combination with macros. `std_print` is the standard print function which can print all sorts of types.
- **Operator overloading** - Overloading for all arithmetic, logical, and relational operations.
- **Polymorphism** - Structures and functions that can be reused for multiple types (useful in arrays and hash maps). Polymorphism is like templates, generics in other languages.
- **Runtime type information** - Not included by default. Import `Lang` to access type information.
- **Print any type** - The `Logger` module implements a polymorphic `std_print` function which takes in a pointer of some type and prints it.
- **Preprocessor** - Conditional sections, recursive macros, and functions inserts that allow you match functions where you want to insert text at the start of them.
- **Compile time execution** - Only globals are evaluated at compile time at this moment.
- **Stack trace and asserts** - `Assert` and `StackTrace` modules that in combination with function inserts give you a stack trace similar to Javascript.
- **x86-64 generation** - Compiler supports x86-64 targets. (32-bit x86 is not supported yet)
- **ARM generation** - Currently **Experimental**, use at your own risk.
- **Inline assembly** - A block of assembly that is inserted into your code. It can be a statement or part of an expression. Requires GCC or MSVC tool chain (as or ml64).
- **Decent error messages** - You are bombarded with them in certain cases and macros certainly makes it harder to figure out what the error message is about but most messages provide the types that are problematic (in polymorphic structs/functions for example). This is how errors look like:
![](/docs/img/err-named-arg.png)

## Standard library
Most modules in the standard library work for Windows and Linux.
- **String**
- **Array, Map** - Basic data structures.
- **Networking** - Sockets wrapper, server/client module, HTTP module.
- **File system** - Beyond the normal open/read/write/close functions there are functions for iterating through files in directories and creating a file watcher with a callback.
- **Sound** - *Work in progress*
- **Graphics** - Function to create a window, draw rectangles, text and images. This works on Linux and Windows. GLAD, GLFW, stb_image is used.
![](/docs/img/game_blobs_0.png)

### Library bindings
These are some modules with bindings for popular libraries. They provide common functions but you may need to declare some functions yourself.
- **OpenSSL** - Encryption
- **GLAD** - Bindings for OpenGL
- **GLFW** - Window and input library
- **STB (stb_image)** - Image loading and writing
- **Windows**
- **Linux**

You are more than welcome to contribute your own bindings!

## Other useful features

- **Linking with external libraries** - Dynamic and static.
- **Exporting functions** - You can compile static and dynamic libraries and export functions.
- **Debug information** - Debug info for Visual Studio is not supported. The compiler generates DWARF and does not support PDB. Visual Studio Code or GDB supports DWARF which you can use. (you need extension and launch.json for vscode, see .vscode/launch.json in this repository or search on the internet on how to set it up)
- **The core of the compiler does not use any libraries** - The compiler parses, type checks, and generates object files with instructions completely by itself. It does however rely on a linker to create executables from the object files. It also uses GCC or MSVC Macro assembler when inline assembly is used. The repository contains GLAD, GLFW, stb_image which is part of the standard library that is distributed. The compiler itself doesn't use them. There is also Tracy which is used for profiling performance.
- **Software/hardware exception handling** - Only for Windows. The plan is to redesign it since one of the goals for the programming language is no platform specific features (or at least VERY few).

# How to get started
Download a release (follow steps below) or [build](#Building) the compiler and then read the [Guide](/docs/guide/00-Introduction.md).

1. Download a release (.zip) from https://github.com/Emarioo/BetterThanBatch/releases
2. Then unzip in a folder of your choice.
3. (optional) Add the path to the executable to the environment variable `PATH` so you can call `btb` from a terminal in any directory.
4. Install a linker from one of these toolchains: GCC, Clang, or MSVC (Microsoft Visual C/C++ Compiler). The building section describes how to setup MSVC.

The compiler is capable of generating object files but you need a linker to turn them into executables.

Lastly, if you are using vscode then install the BTB Language extension [btb-lang/README.md](/btb-lang/README.md). If you are working with other editors then you would need to write the syntax highlighting yourself. You can look at the vscode extension's syntax grammar and highlighting and reimplement it for your editor's plugin system.

# Building

**Requirements**: Python 3.9+, GCC/Clang/MSVC

The project is written in C++ and uses a python script (`build.py`) to build the compiler. Once compiled, the executable can be found in `bin/btb.exe`. I recommend editing the environment variable `PATH` so that you have access to `btb.exe` from anywhere.

Begin by cloning the repository then follow the instructions based on your operating system. If you're on macOS then you're out of luck. (if you want to be the one implementing macOS support then join the discord and let's have a chat: https://discord.gg/gVzQhm9pwH)
```
git clone https://github.com/Emarioo/BetterThanBatch
```

## Linux
1. Install `g++` or `clang++`:
```bash
# on Ubuntu
sudo apt install g++
sudo apt install clang
```

2. Run the build script:
```bash
python3 build.py

# gcc is default, add clang to build with clang
python3 build.py clang
```

## Windows
1. Install one of these toolchains: Visual Studio, GCC, Clang.
2. Run the build script:
```bash
python build.py

# msvc is default, you can change toolchain like this:
python build.py gcc
```

A full rebuild can take 4-7 seconds with MSVC, 25-50 seconds with gcc or clang.

### Installing/using MSVC
1. Install Visual Studio and select `C/C++ desktop development` in the Visual Studio Installer.
2. Navigate to the cloned repository and run `python build.py`.

If that didn't work and you get a message like this: "MSVC could not be configured". Then the automatic setup of temporary environment variables for MSVC failed.

To setup the environment you can hit the windows key and search for `VS Developer Command Prompt`. This will open up a terminal where `cl` and `link` are available. Navigate to the cloned repo folder and run `python build.py`.

If you have Visual Studio Code then you can type `code` in the *VS Developer* terminal. From there you can `Open Folder` and choose the cloned repo folder. Then in VSCode `Create New Terminal` and `cl` should be available. Now run `python build.py`.

If you want MSVC on the command line (without starting the *VS Developer* terminal) then run a command similar to this: `"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"`. The path may be different if you installed Visual Studio elsewhere or if you have a different version. You can add the path to the environment variable PATH which allows you to just type `vcvars64.bat`.
<!-- This needs more explaination.
- (manually adding environment variables from vcvars64) If you are tired of *VS Developer* and *vcvars64.bat* then you can check the content and which environment paths you have before running vcvars64.bat and compare them with the ones you have after running vcvars64.bat. Then you add the missing content to environment variables permanently.
-->

**NOTE:** When using the compiler you also need to setup the environment variables. The compiler will try to do it automatically but may fail if you are using an older version of Visual Studio or if you changed the default install location.

### Installing GCC
Look for a tutorial on the internet. This link may be helpful: https://sourceforge.net/projects/mingw/files/Installer/

### Installing clang
Look for a tutorial on the internet. 

## Other build information
If you want to compile with debug information, tracy profiler or optimizations then you can toggle these options inside `build.py`. You can also pass them as arguments to the script like this: `python build.py use_debug use_optimizations use_tracy`.

You can change the path of the executable like this: `python build.py output=bin/release/btb.exe`.
All object files will still end up in `bin`, this cannot be changed at the moment.

The `main` branch is always well tested and works on Linux and Windows all the time. The `dev` branch contains the latest commits where I may have broken something.

# Join the community
If you find the compiler and language interesting and want to chat about it or have questions or problems getting started then feel free to join our discord: https://discord.gg/gVzQhm9pwH

## Mini-projects

Simple rendering using GLAD and GLFW: [Rendering test](/examples/graphics/quad.btb)

Rendering textures and text, simple collision and player movement, sockets and networking: [Multiplayer game](/examples/graphics/game.btb)

Reads files and counts newlines using multiple threads: [Line counter](/examples/linecounter.btb)

<!-- (incomplete) Parses and read binary file formats: [Binary viewer](/examples/binary_viewer/main.btb) -->

# A personal note on the present and the future
The compiler is work in progress and bugs in the code generation do occur which are really hard to catch. If you do encounter strange behaviour and are about to pull out your hair then don't hesitate to ask for help on the discord. I (Emarioo) am happy to help.

So far I have worked on the compiler for 1700 hours and I think it really shows with all it's capabilities. For the future I have around 400 todos in my private documents and 600 in the source code. Some of the major things I want to work on are the following:
- ARM code generation, compile BTB for Arduino and other chips. Mostly for learning and experimentation.
- Optimize and refactor the compiler. Current architecture makes it hard to multi-thread because data is accessed from anywhere at anytime (is what it feels like).
- Move away from the preprocessor and utilize the syntax tree and compile time evaluation more.
- Build system module.
- Lambda functions and possibly closures.

During the compiler's development I have been inspired by many things, but the person who lead me down this path was Jonathan Blow with his programming language Jai. Huge thanks to him for his amazing and inspiring videos and games. (if you haven't played Braid, Anniversary Edition then I think you should check it out on steam, it's fantastic, just saying)
