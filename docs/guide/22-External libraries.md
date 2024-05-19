# Linking in the compiler
We begin with the basics.
1. The compiler generates an object file (bin/main.o).
2. Then the compiler provides the linker (MSVC, GCC, CLANG) with necessary information such as the object file, path of output file, and libraries to link with.

Libraries to link with is specified in the program itself. There are two ways of doing this.

`#link "path_to_lib"` - Is used to pass arguments directly to the linker
`#load "path_to_lib" as Lib` - Is used to pass libraries to the linker.

`#link` exists in case you want to interact with the linker in a way that the language and compiler doesn't support. Note that the arguments of one linker isn't always compatible with another linker (MSVC, GNU).

`#load` exists to link with libraries independently from the linker. You just specify the path to the library and don't have to worry about `g++ -Ldir_to_lib -llib_file` (GNU Linux or MinGW Windows) and `link path_to_lib` (MSVC Windows).

## Linking a library
Let's say this is your project structure.
```
project
├─main.btb
├─libs
│ ├─sound.lib
```

The purpose of your program is to play the sound file specified
through command line arguments. The static library 'sound.lib' contains
a function that lets you play a sound from a file and you want to call it.
The following code does just that.

```c++
// Tell the compiler to link with 'libs/sound.lib' and let functions
// refer to this library as 'Sound'.
#load "libs/sound.lib" as Sound

// @import tells the compiler that this function does not have a body and is
// an external function that exists in the static library 'Sound'.
fn @import(Sound) PlaySoundFromFile(path: char*);

fn main(argc: i32, argv: char**) {
    // Now we just call the function
    PlaySoundFromFile(argv[1])
}

// If the name 'PlaySoundFromFile' is to long then you can use an alias.
fn @import(Sound, alias = "PlaySoundFromFile") do_sound(path: char*);

fn main(argc: i32, argv: char**) {
    do_sound(argv[1])
}
```

**NOTE**: The calling convention is assumed to be stdcall when targeting Windows and System V ABI when targeting Linux.


# Expterimental features

## @dllimport
Below is an example of linking with the Win32 API. The `@dllimport` differs from `@import` by which call instruction is used.
Normal import uses a relative call while dllimport uses a RIP relative call and "__imp_" is pre-appended to the external function name.

I don't know why we would need `@dllimport`. I tried various methods while struggling to implement calls to external functions and
RIP relative call and __imp_ is one method I tried.

```c++
#load "Bcrypt.lib" as bcrypt

// Both work
fn @import(bcrypt,alias="BCryptGenRandom") BCryptGenRandom_lib(_: void*, buf: void*, len: u32, flags: u32) -> i32;
fn @dllimport(bcrypt,alias="BCryptGenRandom") BCryptGenRandom_dll(_: void*, buf: void*, len: u32, flags: u32) -> i32;

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