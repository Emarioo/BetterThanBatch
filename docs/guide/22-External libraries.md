**A common mistake** is tangling the calling conventions. Normally, a custom convention is used for functions but if you import or export a function it is implicitly changed to the native standard convention (stdcall or System V ABI). If you are casting function pointers, make sure to annotate them with the correct convention.

# Linking in the compiler
We begin with the basics.
1. The compiler generates an object file (bin/main.o).
2. The compiler provides information to the linker (MSVC, GCC, CLANG) such as the object file, path of output file, and libraries to link with.

Libraries to link with is specified in the program itself. There are two ways of doing this.

`#load "path_to_lib"` - Passes a library to the linker.
`#load "path_to_lib" as Lib` - Passes the library to the linker **if** any function or variable refers to `Lib`.
`#link "path_to_lib"` - Pass any kind of argument to the linker.

`#link` exists in case you want to interact with the linker in a way that the language and compiler doesn't support. This will often require one `#link` per linker since all linkers don't have the same options (MSVC, GCC).

`#load` exists to link with libraries independently from the linker. The compiler uses the right compiler options depending on the linker. `g++ -Ldir_to_lib -llib_file` (GNU Linux or MinGW Windows), `link path_to_lib` (MSVC Windows).

Path after `#load` can be relative to current working directory, relative to folder where the compiler executable is, and relative to import directories specified in arguments passed to the compiler executable. (absolute paths also work of course)

<!-- I don't think this is true anymore
One important thing is that `#load "your.lib"` without slashes is assumed to be a system library. It will be linked like this: `gcc -llib_file`. This `#load "./your.lib"` will be linked as a user library like this:`gcc your.lib`.
-->

## Linking libraries
**WARNING:** You cannot link with shared libraries on Linux yet.

Let's say this is your project structure.
```
project
├─main.btb
├─libs
│ ├─sound.lib
│ ├─sound.dll
```

The purpose of your program is to play the sound file specified
through command line arguments. The static library 'sound.lib' contains
a function that lets you play a sound from a file and you want to call it.
The following code does just that.

```c++
// Tell the compiler to link with 'libs/sound.lib' and let functions
// refer to this library as 'Sound'.
#load "libs/sound.lib" as Sound

// @import tells the compiler that this function comes from a library
// and shouldn't have a body.
fn @import(Sound) PlaySoundFromFile(path: char*);

fn main(argc: i32, argv: char**) {
    // Now we just call the function
    PlaySoundFromFile(argv[1])
}

// If the name 'PlaySoundFromFile' is to long then you can use an alias.
fn @import(Sound, alias = "PlaySoundFromFile") do_sound(path: char*);
// Also useful when importing name mangled functions.
// (in this case, name mangling using GCC Compiler, g++)
fn @import(Sound, alias = "_Z17PlaySoundFromFilePc") do_sound(path: char*);

fn main(argc: i32, argv: char**) {
    do_sound(argv[1])
}
```

Linking with a dynamic library is as easy as changing the path.
```c++
#load "libs/sound.dll" as Sound

fn @import(Sound) PlaySoundFromFile(path: char*);
```

Under the hood, different things are happening but this is hidden to make things more convenient. If you want to link a dll as a static library you can do the following, altough this will crash your program.
```c++
#load "libs/sound.dll" as Sound

// @importlib to link with a static library no matter the
// file Sound refers to, @importdll for dynamic libraries
fn @importlib(Sound) PlaySoundFromFile(path: char*);
```

**NOTE**: When importing (or exporting) the calling convention defaults to `@oscall` (stdcall when  targeting Windows, System V ABI for Linux). You can change the convention by writing `fn @betcall @import(...`.

## Distributing executables and dynamic libraries
When you compile an executable that uses dynamic libraries, you must also distribute the dynamic libraries along with your executable. Otherwise Windows will complain, "sound.dll not found" on the user's computer.

This part of the compiler is work in progress on Linux but on Windows, the dlls are copied to the working directory where you ran the compiler. These dlls need to be distributed.

# Creating libraries
Compile executable: `btb main.btb -o app.exe`

Compile dynamic library: `btb main.btb -o app.dll`

Compile static library: `btb main.btb -o app.lib`

When compiling libraries, the functions you want to export must be annotated with `@export`. Exported functions cannot be polymorphic and you cannot export functions with the same name. You can alias the name.
```c++
fn @export multiply(x: f32, y: f32) -> f32 {
    return x * y
}
fn @export(alias="multiply_int") multiply(x: i32, y: i32) -> i32 {
    return x * y
}
```

A thing to note is that only the functions that are used such as the entry point and exported functions are present in the binary as symbols.

**NOTE**: Exported functions default to @oscall. Write `fn @betcall @import(...)` to force another calling convention.

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
    #if !LINKER_MSVC
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
