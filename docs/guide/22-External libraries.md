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

One important thing is that `#load "your.lib"` without slashes is assumed to be a system library. It will be linked like this: `gcc -llib_file`. This `#load "./your.lib"` will be linked as a user library like this:`gcc your.lib`.

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

# Compiling libraries
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

A thing to note is that only the functions that are used such as the entry point and exported functions are generated. All other functions are skipped and are not present in the binary.

**NOTE**: Exported functions default to @oscall. Write `fn @betcall @import(...` to force another calling convention.

# What does @import do and how does the compiler link stuff?
**NOTE:** The text is based on Windows and x64.

**TODO:** Write some text about Linux.

**TODO:** Include resources (websites) that contains more information about each part.

This section covers details which is useful to know if your program is crashing or you have linker errors. Compile these files and follow along the explanation to better understand linking:

**sound.btb**
```c++
// btb sound.btb -o sound.dll
// btb sound.btb -o sound.lib

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
// btb test_link.btb -o test_link.exe -r

#load "./sound.dll" as Sound_dll
#load "./sound.lib" as Sound_lib

fn @import(Sound_dll) Play_dll(path: char*);
fn @import(Sound_lib) Play_lib(path: char*);

fn main() {
    Play_lib("hi.wav".ptr)
    Play_dll("hi.wav".ptr)
}
```

First, let's figure out what we are dealing with. When we link the final executable `test_link.exe` we have `test_link.o` which contains the `main` function, `sound.lib` which contains `Play_lib`, and `sound.dll` which contains `Play_dll`. These libraries were linked because the execution of the executable the `main` function calls the play functions. If we didn't call any play function, the compiler would not link with the libraries because they weren't used. If only `Play_dll` was used then only `sound.dll` would be linked. I recommend commenting out the functions and testing this.

The executable contains three things (not literally). The x64 code for `main`, the x64 code for `Play_lib`, and an *Import Address Table* (IAT) with an entry for `Play_dll`.

The x64 code for calling a function from the static library is very simple. It is a `call rel32` instruction with a relocation to the `Play_lib` symbol. The linker resolves this at compile-time and things work as we expect. `Play_dll` is a little more sophisticated because it requires the IAT.

To understand the purpose of the *Import Address Table* we must first understand the usage of dynamic libraries. Dynamic libraries can be loaded and called programmatically using functions from the operating system. There are usually three functions: loading a dynamic library, acquiring a pointer to a function, and freeing the library. Below is some code that does this, I urge you to test it yourself.

```c++
#import "OS" // imports LoadDynamicLibrary and DynamicLibrary

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

    dll.cleanup()
}
```

As you might have noticed, you can still link with dlls without writing this code. That is because the linker does it for you by creating an Import Address Table (IAT), loading dynamic libraries, and filling the table with pointers when the executable starts. This is what you call [load-time dynamic linking (Microsoft)](https://learn.microsoft.com/en-us/windows/win32/dlls/load-time-dynamic-linking).

Now we can answer the question of how we call `Play_dll`. The pointer to the function exists in the IAT and the linker creates a symbol to it prefixed with `__imp_`. If we want the pointer to the function we just have to get it from `__imp_Play_dll`. It looks like this in code `mov rax, [__imp_Play_dll]` + `call rax` or just `call [__imp_Play_dll]`.

With all this knowledge we can write a program in assembly.

**TODO:** WORK IN PROGRESS!!!!!!!
```c++
// btb test_link.btb -o test_link.exe -r

#load "./sound.dll" as Sound_dll
#load "./sound.lib" as Sound_lib

fn @import(Sound_dll) Play_dll(path: char*);
fn @import(Sound_lib) Play_lib(path: char*);

fn main() {
    asm("hi.wav".ptr) {
        // NOTE: Assembly assumes GCC syntax with intel flavour

        // we must declare external symbols
        extern Play_lib
        extern __imp_Play_dll

        // NOTE: We assume stdcall convention...
        pop rbx // pop pointer to string
        sub rsp, 32
        
        mov rcx, rbx
        call 
        
        mov rcx, rbx
        call
    }
    // Play_lib("hi.wav".ptr)
    // Play_dll("hi.wav".ptr)
}
```

**NOTE:** There is an extra thing that linkers do which is stubs for functions from dynamic libraries. These are created if the compiler created a `call rel32` (`call Play_dll`) instead of a `call reg` (`call [__imp_Play_dll]`). Since the `call rel32` instruction cannot call functions from dlls and you can't convert it to a `call reg`, a stub is created where the relative call (`call Play_dll`, Play_dll is a symbol to the stub) jumps to the stub which contains a jump instruction that can jump to code in dlls (`jmp [__imp_Play_dll]`). This is not relevant in the BTB Compiler because you have to explicitly state where the function comes from. Therefore, we always know what type of call to use.

**NOTE:** I have not explained the various relocation types and symbols because I barely now what the different types do myself. I have managed to make things work by analyzing the object files created by gcc and msvc but I plan to take some time to better understand the relocation types.

# Experimental features

## @importdll
Below is an example of linking with the Win32 API. The `@importdll` differs from `@import` by which call instruction is used.
Normal import uses a relative call while importdll uses a RIP relative call and "__imp_" is pre-appended to the external function name.

I don't know why we would need `@importdll`. I tried various methods while struggling to implement calls to external functions and
RIP relative call and __imp_ is one method I tried.

```c++
#load "Bcrypt.lib" as bcrypt

// Both work
fn @import(bcrypt,alias="BCryptGenRandom") BCryptGenRandom_lib(_: void*, buf: void*, len: u32, flags: u32) -> i32;
fn @importdll(bcrypt,alias="BCryptGenRandom") BCryptGenRandom_dll(_: void*, buf: void*, len: u32, flags: u32) -> i32;

data, data2: i32;
status  := BCryptGenRandom_lib(null, &data , sizeof data , 2);
status2 := BCryptGenRandom_dll(null, &data2, sizeof data2, 2);
```

## Non-repetitive library import
The syntax needs redesigning.
```c++
#load "math.lib" as Math

// @mass_import applies @import(Math) to all functions within the scope
@mass_import(Math) {
    fn box_inside_box(...) -> bool;
    fn circle_inside_box(...) -> bool;
}

// A general form where any annotation can be applied to all functions.
#apply_annotation @import(Math) @betcall {
    fn box_inside_box(...) -> bool;
    fn circle_inside_box(...) -> bool;
}
```