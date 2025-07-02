**VERY EXPERIMENTAL FEATURE**

Importing C headers allows you to call functions and use structs and enums from those headers. Note that you will need to link with the library or object file that contains the implementation for the functions, otherwise you will get linker errors.

You can find an example in `examples/cparser`.
```c++
// util.c
int calculate(int x, int y) {
    return x*2 + y*3
}

// util.h
int calculate(int x, int y);

// main.btb
#load "./util.lib"
#import "./util.h"

#import "Logger"

x: i32 = calculate(2, 7)
log(x)
```

Linking is done by adding `#load "libs/glfw3-3.8.0/glfw3.lib"` to your source code (assuming you’ve placed the GLFW library in a libs folder).

The main purpose of importing C headers is to eliminate the need to manually write bindings for C libraries. For example, by importing glfw3.h and glad.h, you gain access to all GLFW and OpenGL functions directly. While there are partial bindings in `modules/vendor` many functions are missing because writing complete bindings by hand is time-consuming. We still have `modules/vendor` for when the BTB's C parser is buggy or can't handle certain headers for whatever reason.

# Limitations (there's a ton)
This feature is not guarranteed to work especially on different operating systems with different code styles in system headers. You will have to try importing a C header and if it doesn't work then it doesn't work. If it does then great.

You can add the `--verbose` flag to get more information about how the C parsing is going. If the error message is confusing, the extra verbosity may give you a clue to what is wrong. You are welcome to report issues you find (github or discord).

The goal is to support headers from most libraries: GLFW, GLAD, STB image, OpenXR, OpenAL, OpenSSL, and C standard headers. None of these work at the time of writing this (2025-06-07).

## Known issues
- We do not handle cdecl calling convention
- We do not support variadic functions
- We have problems preprocessing C standard headers (a lot of attribute, macros and so on)
- We have problems handling includes in C headers
- You cannot import defines

Some of these problems will be fixed in due time but it may take many months depending on how important they are compared to everything else in the compiler.

# Implementation and details

Convert function declarations, structs, enums, typedefs in C to BTB.

## Problems to solve

1. We must expand macros because we can't possibly parse a C header and it's declarations if they are obfuscated by macros. Macros usually expand to declspec(dllimport) and attribute.

2. C headers have includes. Do we treat includes as their own import or expand all includes? Either way we need to get the macros from them and preprocess.

### Example problem
What if you have import two different headers that include the same base header with declarations. If we expand all includes the two imports would have the same declarations. If we import both of them in the same BTB file we would get
duplicate declarations. We can do `#import "stdio.h" as STDIO` and then `STDIO.fopen()` but you shouldn't have too.

If we treat includes as an import then we won't get the macros from it. Unless C macros carry over into the BTB language. BTB and C macros behave slightly differently but C macros should be compatible with BTB (not the other way).

An include could carry over the macros only and then we also treat it as an import? We'll need extra work and code to purely gather the macros from that file though.


## How we parse C headers
It begins with `#import "stdio.h"`.

We first fully preprocess the file. Macros and includes recursively.

Then we parse the flattened C header and ignore anything that isn't typedef, struct, enum or function. We parse it into a temporary AST.

We then write out a BTB file based on the AST. typedefs become macros, and struct, enum, function become the BTB equivalent.