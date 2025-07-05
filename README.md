**COMPILER IS WORK IN PROGRESS, IT WILL CHANGE AND BREAK YOUR CODE**

# BetterThanBatch
A neat compiler.

- Good for your every day scripting needs.
- Useful for visualizing data structures, programs...?
- Standard library with essentials for platform independent programs (graphics, audio, file formats, networking)
- Fast compiler, good error messages, no external build system.

# Features
See the [Guide](/docs/guide/README.md) for details.

- **Statically typed** - All data types in the program language are statically typed. No dynamic objects.
- **Compile time execution** - Run code at compile time. Read and parse a config.txt file to a data structure and place it in your program as a global variable.
- **Imports** - No headers, no forward declarations. Circular dependencies between imports are allowed.
- **Function overloading** - Very powerful in combination with macros. `std_print` is the standard print function which can print all sorts of types.
- **Operator overloading** - Overloading for all arithmetic, logical, and relational operations.
- **Polymorphism** - Structures and functions that can be reused for multiple types (useful in arrays and hash maps). Polymorphism is like templates, generics in other languages.
- **Runtime type information** - Not included by default. Import `Lang` to access type information.
- **Print any type** - The `Logger` module implements a polymorphic `std_print` function which takes in a pointer of some type and prints it.
- **Preprocessor** - Conditional sections, recursive macros, and functions inserts that allow you match functions where you want to insert text at the start of them.
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

### Library bindings (/modules/vendor)
These are some modules with bindings for popular libraries. They provide common functions but you may need to declare some functions yourself.
- **OpenSSL** - Encryption
- **GLAD** - Bindings for OpenGL
- **GLFW** - Window and input library
- **STB (stb_image)** - Image loading and writing
- **Windows**
- **Linux**

You are more than welcome to contribute your own bindings!

There is an **experimental** C import feature which you can try. It allows you to include functions from C headers, such as `#import "GLFW/glfw3.h"`.

## Other useful features

- **Linking with external libraries** - Dynamic and static.
- **Exporting functions** - You can compile static and dynamic libraries and export functions. The compiler will auto-generate a file with import declarations.
- **Debug information** - Debug info for Visual Studio is not supported. The compiler generates DWARF and does not support PDB. Visual Studio Code or GDB supports DWARF which you can use. (you need extension and launch.json for vscode, see .vscode/launch.json in this repository or search on the internet on how to set it up)
- **The core of the compiler does not use any libraries** - The compiler parses, type checks, and generates object files with instructions completely by itself. It does however rely on a linker to create executables from the object files. It also uses GCC or MSVC Macro assembler when inline assembly is used. The repository contains GLAD, GLFW, stb_image which is part of the standard library that is distributed. The compiler itself doesn't use them. There is also Tracy which is used for profiling performance.
- **Software/hardware exception handling** - Only for Windows. Linux uses signals which is very different from what Windows offers. The plan is to redesign the exception feature to work on all major platforms.

# How to get started
Download a release (follow steps below) or clone the repository and [build](#Building) the compiler. Then read the [Guide](/docs/guide/README.md).

1. Download a release (.zip) from https://github.com/Emarioo/BetterThanBatch/releases
2. Then unzip in a folder of your choice.
3. Add the path to the executable to the environment variable `PATH` so you can call `btb` from a terminal in any directory.
4. Install one of these toolchains: GCC, Clang, or MSVC (Microsoft Visual C/C++ Compiler). The compiler cannot link object files into executables by itself.

Lastly, if you are using vscode then install the BTB Language extension [btb-lang/README.md](/btb-lang/README.md). If you are working with other editors then you would need to write the syntax highlighting yourself. You can look at the vscode extension's syntax grammar and highlighting and reimplement it for your editor's plugin system.

# Building

**Requirements**: Python 3.9+, GCC/Clang/MSVC

The project is written in C++ and does not require any libraries. `build.py` is used when building the compiler. Once compiled, the executable can be found in `bin/btb.exe`. I recommend editing the environment variable `PATH` so that you have access to `btb.exe` from anywhere.


Begin by cloning the repository then follow the instructions based on your operating system. We support `Linux` and `Windows` so if you're on macOS then you're out of luck (compiler doesn't support the Mach-O object file format). If you want to help implement macOS support then join the discord and let's have a chat: https://discord.gg/gVzQhm9pwH.
```
git clone https://github.com/Emarioo/BetterThanBatch
```

You can find more build information here [docs/building.md](/docs/building.md).

While the BTB Compiler doesn't *require* any libraries, you can use [Tracy Profiler](https://github.com/wolfpld/tracy) to profile it. The project also contains libraries such as [GLFW](https://www.glfw.org/), [stb_image](https://github.com/nothings/stb), and [Glad](https://glad.dav1d.de/) which are distributed with the compiler.

## Linux
1. Install `python3` and `g++` or `clang++`:
```bash
# on Ubuntu
sudo apt install python3
sudo apt install clang++
sudo apt install g++
```

2. Run the build script:
```bash
python3 build.py

# gcc is default, add clang to build with clang
python3 build.py clang
```

## Windows
1. Install Python
2. Install one of these toolchains: Visual Studio, GCC, or Clang.
3. Run the build script:
```bash
python build.py

# gcc is default, you can change toolchain like this:
python build.py msvc
```

# Join the community
If you find the compiler and language interesting and want to chat about it or have questions or problems getting started then feel free to join our discord: https://discord.gg/gVzQhm9pwH

## Mini-projects

Simple rendering using GLAD and GLFW: [Rendering test](/examples/graphics/quad.btb)

Rendering textures and text, simple collision and player movement, sockets and networking: [Multiplayer game](/examples/graphics/game.btb)

Reads files and counts newlines using multiple threads: [Line counter](/examples/linecounter.btb)

<!-- (incomplete) Parses and read binary file formats: [Binary viewer](/examples/binary_viewer/main.btb) -->

## Licensing of BTB Compiler source code

This repository contains two separately licensed components:

### Compiler program (in `/src`)
The compiler source code is licensed under the GNU General Public License v3.0 (GPL-3.0).

You are free to use, study, modify, and share the compiler.

If you distribute the compiler or modified versions of it, you must also release the full source code under the same license (GPL-3.0).

Commercial use is allowed, but proprietary forks or closed-source redistributions are not.

See [LICENSE](./LICENSE) for the full terms of the GPL.

### Standard Library (in `/modules`)
The standard library is released under the [MIT License](./modules/LICENSE). You are free to use, modify, and include this library in commercial projects. Do whatever you want with it.

### Everything else
Other folders like `/docs`, `/examples`, `/tests`, and `/btb-lang` (vscode syntax highlighting) are free to use in any way you want.

Content under `libs` is not owned by me.

# A personal note on the present and the future
The compiler is work in progress and bugs in the code generation do occur which are really hard to catch. If you do encounter strange behaviour and are about to pull out your hair then don't hesitate to ask for help on the discord. I (Emarioo) am happy to help.

So far I have worked on the compiler for 1700 hours and I think it really shows with all it's capabilities. For the future I have around 400 todos in my private documents and 600 in the source code. Some of the major things I want to work on are the following:
- ARM code generation, compile BTB for a microprocessor. Mostly for learning and experimentation.
- Optimize and refactor the compiler. Current architecture makes it hard to multi-thread because data is accessed from anywhere at anytime (is what it feels like).
- Move away from the preprocessor and utilize the syntax tree and compile time evaluation more.
- Build system/compiler module allows you to interact with the compiler at compile time.
- Lambda functions and possibly closures.

During the compiler's development I have been inspired by many things, but the person who lead me down this path was Jonathan Blow with his programming language Jai. Huge thanks to him for his amazing and inspiring videos and games. (if you haven't played Braid, Anniversary Edition then I think you should check it out on steam, it's fantastic, just saying)
