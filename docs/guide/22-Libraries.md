
For quick reference:
- Compile dynamic library: `btb main.btb -o main.dll` (or `-o main.so`)
- Compile static library: `btb main.btb -o main.lib` (or `-o libmain.a`)
- Compile executable: `btb main.btb -o main.exe` (or `-o main`)

# Linking with libraries
<!-- We begin with the basics.
1. The compiler generates an object file (bin/main.o).
2. The compiler provides information to the linker (MSVC, GCC, CLANG) such as the object file, path of output file, and libraries to link with.
-->
Libraries to link with are specified in the source code of the program using the `#load` directive. This is different from C compilers where you specify libraries as arguments to the linker.

Functions from a library are marked with `@import(LibName)`. Below are some practical examples.

## Linking with a dynamic library
Let's say this is your project structure. You want to compile two dynamic libraries, sound.so and broadcast.so, which should should be linked when compiling the main program.
```
project
|-main.btb
|-sound.c
|-broadcast.btb
```
**main.btb**
```c++
// btb main.btb --run

// Tell the compiler to link with 'libs/sound.lib' and let functions
// refer to this library as 'Sound'.
#load "./libsound.so" as Sound
#load "./libbroadcast.so" as Broadcast

// @import tells the compiler that this function comes from a library
// and shouldn't have a body.
fn @import(Sound) PlaySoundFromFile(path: char*);
fn @import(Broadcast) BroadcastMsg(text: char*);

PlaySoundFromFile("theme.mp3".ptr)
BroadcastMsg("main is done".ptr)
```
**sound.c**
```c
// gcc sound.c -fpic -shared -o libsound.so

#include <stdio.h>

void PlaySoundFromFile(const char* path){
    printf("Play sound: %s\n", path);
}
```

**broadcast.btb**
```c
// btb broadcast.btb -o libbroadcast.so

#import "Logger"

fn @export BroadcastMsg(text: char*){
    log("Broadcast: ", text);
}
```

Compile and run the files with these commands.
```bash
gcc sound.c -fpic -shared -o libsound.so
btb broadcast.btb -o libbroadcast.so
btb main.btb -o main

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.
./main
```

`export LD_LIBRARY_PATH=.` tells Linux to search current working directory for libsound.so when running `main`.

## Other information

You can use an alias for the imported/exported function like this:
```cpp
fn @import(Sound, alias="PlaySoundFromFile") do_sound(path: char*);
fn @export(Sound, alias="btb_sendmsg") sendmsg(path: char*) { /* ... */ }
```

Exported functions cannot be polymorphic. You cannot export functions with the same name. BTB does not apply name mangling when exporting functions.

You can use `#link` to pass an argument to the linker, useful when the compiler is lacking support for something. This may require one `#link` per toolchain since all linkers don't have the same options (MSVC vs GCC). You may be better off compiling object file from BTB and then calling linker yourself with a bunch of flags in a `build.sh` script (or `build.btb` if that is preferred).
```cpp
#link "nostdlib"
```

`#load` exists to link with libraries independently from the linker. The compiler uses the right compiler options depending on the linker. `g++ -Ldir_to_lib -llib_file` (GNU Linux or MinGW Windows), `link path_to_lib` (MSVC Windows).

By specifying the path to libraries with `#load "path/to/mylib.so" as LIB` the compiler's **Virtual Machine** can find the libraries and therefore fully run your program in the **VM**. `btb src/main.btb -o app.o` + `gcc app.o path/to/mylib.so -o app` where main.btb declares `#load LIB` without a path means that the **VM** can't call external functions from that LIB. If you have compile time code that doesn't call external functions then that's fine, it will still run.

The path specified in the `#load` directive should be one of these:
- Relative to current working directory
- Absolute path
- Relative to the directory of the compiler executable
- Relative to import directories (specified through arguments to the compiler)
- A path without a slash is assumes to be a system library. `#load "lib_file"` -> `gcc -llib_file`. Use `#load "./lib_file"` for relative path.

<!-- TODO: Cover variations of #load, without as and so on -->

When importing (or exporting) the calling convention defaults to `@oscall` (stdcall when targeting Windows, System V ABI for Linux). You can change the convention by writing `fn @betcall @import(Lib) func(...)`. (betcall is the language's own calling convention which allows multiple return values and structs passed as values)

## Auto-generated declarations
When compiling a library, a `libname_decl.btb` file is automatically created which contains the import declarations for that specific library along with enum and struct types.

```c++
// math.btb
fn @export add(x: i32, y: f32) -> i32 {
    return x + y
}

// app.btb
#import "math_decl.btb"
#import "Logger"

add(9,10)

// math_decl.btb (auto-generated)
#load "math.lib" as math

fn @import(math) add(x: i32, y: f32) -> i32;
```

Assuming the two files above is in the same directory you can run `btb math.btb -o math.lib` which
will create `math.lib` and `math_decl.btb`. Then you can run `btb app.btb -r` to compile and execute the program.

If you take a look at *math_decl.btb* you will see a *#load* directive at the top. The path is based on the relative path to the library. `btb math.btb -o libs/math.lib` will result in `#load "./libs/math.lib" as math`. The name of the library (*math*) is derived from the output path. `btb math.btb -o MyMaTH.lib` results in `#load "MyMaTH.lib" as MyMaTH`.

The compiler will also generate `math_decl.h` for C/C++ but there are a few considerations.
- Exported functions with *betcall* calling convention will be skipped.
- Polymoprhic struct types will "specialized" (**Array<i32>** -> **Array_i32**).

**KNOWN PROBLEMS:** Compiler will generate types for structs in the standard library so you may unfortunately experience type mismatch with types from standard library.

# What does @import do and how does the compiler link stuff?
**TODO:** Write some text about Linux.
**TODO:** Include resources (websites) that contains more information about each part.

[James McNellis "Everything You Ever Wanted to Know about DLLs"](https://www.youtube.com/watch?v=JPQWQfDhICA)

Before we begin it is important to know that a library linked with one compiler tool chain (MSVC, GCC) will be compatible with another. When testing things you must make sure to recompile every library when changing which linker you are using. Therefore, you should probably not change the linker (GCC is recommended because the compiler doesn't support debug information with MSVC).
Also, the text is based on Windows and x64. Things described may not be true on Linux.

This section covers details which is useful to know if your program is crashing or you have linker errors. Compile these files and follow along the explanation to better understand linking:

**sound.btb**
```c++
// Compile with:
//   btb sound.btb -o sound.dll
//   btb sound.btb -o sound.lib

#import "Logger"

#if BUILD_DLL
    fn @export Play_dll(path: char*) {
        log("dll - Play sound: ", path)
    }
#else
    fn @export Play_lib(path: char*) {
        log("lib - Play sound: ", path)
    }
#endif
```

**test_link.btb**
```c++
// Compiler and run with:
//   btb test_link.btb -o test_link.exe -r

#load "./sound.dll" as Sound_dll
#load "./sound.lib" as Sound_lib

fn @import(Sound_dll) Play_dll(path: char*);
fn @import(Sound_lib) Play_lib(path: char*);

fn main() {
    Play_lib("hi.wav".ptr)
    Play_dll("hi.wav".ptr)
}
```

First, let's figure out what we are dealing with. When we link the final executable `test_link.exe` we have `test_link.o` which contains the `main` function, `sound.lib` which contains `Play_lib`, and `sound.dll` which contains `Play_dll`. These libraries were linked because the `main` function calls the play functions. If we didn't call any play function, the compiler would not link with the libraries because they weren't used. If only `Play_dll` was used then only `sound.dll` would be linked. I recommend commenting out the functions and testing this.

Before we move on it is important to remember that different compilers do things differently. For example, you cannot link dlls to an executable using MSVC linker. There you have to link with a static import library. When compiling `btb sound.btb -o sound.dll --linker msvc`, a `sounddll.lib` file is created alongside `sound.dll`. Internally, the compiler will automatically link with `sounddll.lib` when using MSVC instead of `sound.dll` while with the GCC linker, `sound.dll` will be linked directly.

```
// Linker command when using MSVC
link /nologo /INCREMENTAL:NO /entry:main /DEFAULTLIB:MSVCRT bin/test_link.o Kernel32.lib sound.lib sounddll.lib /OUT:test_link.exe

// Command when using gcc
gcc bin/test_link.o -lKernel32 sound.lib sound.dll -o test_link.exe
```

The executable contains three things (not literally). The x64 code for `main`, the x64 code for `Play_lib`, and an *Import Address Table* (IAT) with an entry for `Play_dll`.

The x64 code for calling a function from the static library is very simple. It is a `call rel32` instruction with a relocation to the `Play_lib` symbol. The linker resolves this at compile-time and things work as we expect. `Play_dll` is a little more sophisticated because it requires the IAT.

To understand the purpose of the *Import Address Table* we must first understand the usage of dynamic libraries. Dynamic libraries can be loaded and called programmatically using functions from the operating system. There are usually three functions: loading a dynamic library, acquiring a pointer to a function, and freeing the library. Below is some code that does this, I urge you to test it yourself.

```c++
#import "OS" // imports LoadDynamicLibrary and the struct DynamicLibrary
             // On windows, LoadDynamicLibrary = LoadLibrary and DynamicLibrary.get_pointer = GetProcAddress

fn main() {
    dll := LoadDynamicLibrary("sound.dll")
    
    ptr := dll.get_pointer("Play_dll") // Play_dll can alternatively be a global variable, it's not limited to functions

    // When we acquire the pointer we don't know which type it is
    // so void* is assumed.
    // To call the function we must cast it to the correct function type.
    // We use @oscall because when we export sound.dll and use @export on Play_dll,
    // the calling convention defaults to @oscall (stdcall on Windows, sys V abi on Linux)
    Play_dll := cast_unsafe< fn @oscall (char*) > ptr

    Play_dll("it works".ptr)

    dll.cleanup() // FreeLibrary on Windows
}
```

As you might have noticed, you can still link with dlls without writing this code. That is because the linker does it for you by creating an Import Address Table (IAT), loading dynamic libraries, and filling the table with pointers when the executable starts. This is what you call [load-time dynamic linking (Microsoft)](https://learn.microsoft.com/en-us/windows/win32/dlls/load-time-dynamic-linking).

Now we can answer the question of how we call `Play_dll`. The pointer to the function exists in the IAT and the linker creates a symbol to it prefixed with `__imp_`. If we want the pointer to the function we just have to get it from `__imp_Play_dll`. It looks like this in code `mov rax, [__imp_Play_dll]` + `call rax` or just `call [__imp_Play_dll]`.

With all this knowledge we can write a program in assembly.

```c++
// Compile with:
//   btb test_link.btb -o test_link.exe -r

// The compiler doesn't know that the assembly uses
// these libraries so we must forcefully link them
#load @force "./sound.dll" as Sound_dll
#load @force "./sound.lib" as Sound_lib

fn @import(Sound_dll) Play_dll(path: char*);
fn @import(Sound_lib) Play_lib(path: char*);

fn main() {
    asm("cool".ptr) {
    #if !LINK_MSVC
        // GCC syntax with intel flavour
            
        // declare external symbols
        .extern Play_lib
        .extern __imp_Play_dll

        // NOTE: We assume stdcall convention
        pop rbx // pop pointer to string which was passed to inline assembly

        sub rsp, 32 // alloc space for args, stdcall needs 32 bytes
        
        mov rcx, rbx // Set first argument
        call Play_lib
        
        mov rcx, rbx // Set first argument again because rcx is a volatile register while rbx isn't
        call qword ptr [rip+__imp_Play_dll]
        // rip+ tells the assembler to use rip relative instruction
        // We get the wrong instruction and relocation without it

        add rsp, 32 // free space for args
    }
    Play_lib("hi.wav".ptr)
    Play_dll("hi.wav".ptr)
}
```

Same assembly with syntax for MSVC assembler (MASM)
```c++
// btb test_link.btb -o test_link.exe -r

// The compiler doesn't know that the assembly uses
// these libraries so we must forcefully link them
#load @force "./sound.dll" as Sound_dll
#load @force "./sound.lib" as Sound_lib

fn @import(Sound_dll) Play_dll(path: char*);
fn @import(Sound_lib) Play_lib(path: char*);

fn main() {
    asm("cool".ptr) {
        // MSVC syntax with intel flavour
        extern Play_lib : proto
        extern __imp_Play_dll : proto

        pop rbx

        sub rsp, 32
        
        mov rcx, rbx
        call Play_lib
        
        mov rcx, rbx
        call qword ptr [__imp_Play_dll]
        // MSVC assembler generate the call we want, no need for rip+ (rip+ isn't even valid syntax)

        add rsp, 32 // free space for args
    }
    Play_lib("hi.wav".ptr)
    Play_dll("hi.wav".ptr)
}
```


**NOTE:** There is an extra thing that linkers do which is stubs for functions from dynamic libraries. These are created if the compiler created a `call rel32` (`call Play_dll`) instead of a `call reg` (`call [__imp_Play_dll]`). Since the `call rel32` instruction cannot call functions from dlls and you can't convert it to a `call reg`, a stub is created where the relative call (`call Play_dll`, Play_dll is a symbol to the stub) jumps to the stub which contains a jump instruction that can jump to code in dlls (`jmp [__imp_Play_dll]`). This is not relevant in the BTB Compiler because you have to explicitly state where the function comes from (dynamic library or not). Therefore, we always know what type of call to use.

**NOTE:** I have not explained the various relocation types and symbols because I barely now what the different types do myself. I have managed to make things work by analyzing the object files created by gcc and msvc but I plan to take some time to better understand the relocation types.


## Compiling stuff on NixOS
When compiling BTB projects (and C/C++ projects) that depend on libraries such as OpenGL and X11 then you must do this in NixOS:
```bash
nix-shell -p libGL xorg.libX11
btb examples/graphics/quad.btb --run
```
Above, `quad.btb` uses GLFW and GLAD which need libGL.so and libX11.so. In NixOS you need to be explicit about which libraries and linker directories you need. BTB will generate `bin/int/gcc_includes.txt` to find include directories in case you import C headers. In there you will find somthing like `/nix/store/SOME_HASH-libgl/include` and same for X11. Same for linker directories. 

**TODO:** Provide a clearer explanation?

# Experimental features

## Non-repetitive library import
If you have a library with many functions which you want to import like GLAD or GLFW it would be nice if you didn't have to use @import(Math) on every single function. Below are some examples:
```c++
#load "math.lib" as Math

// Everything in the namespace receives @import(Math) to all functions within the scope
namespace @import(Math) {
    fn box_inside_box(...) -> bool;
    fn circle_inside_box(...) -> bool;
}

// A general form where any annotation can be applied to all functions.
#apply_annotation @import(Math) @betcall {
    fn box_inside_box(...) -> bool;
    fn circle_inside_box(...) -> bool;
}
```

Even though that's great, we still have a problem with alias. What if every function is prefixed with `math_`. We would then want something like this:
```c++
#apply_annotation @import(Math, alias="math_*") { /* ... */ }

#apply_annotation @import(Math, prefix_alias="math_") { /* ... */ }
```

<!-- **A common mistake** is tangling the calling conventions. Normally, a custom convention is used for functions but if you import or export a function it is implicitly changed to the native standard convention (stdcall or System V ABI). If you are casting function pointers, make sure to annotate them with the correct convention. -->
