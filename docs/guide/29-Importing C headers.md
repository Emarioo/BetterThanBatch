**VERY EXPERIMENTAL FEATURE**

You can import a C header with `#import "util.h"`. The functions, structs, and enums in the header and further included files from C #include will then be available in the BTB source code. Internally the compiler has it's own C parser which parses the headers and converts them to BTB code. The C parser is not complete and will therefore ignore parts it cannot parse such as complex function pointers (function pointers from the GLFW library does work). This is why you see "Function/Type/variable does not exist" because the parser skipped it.

When compiling code that imports C headers you must not forget to link with the C library using `#load "util.lib"`. You can also link with object files this way.

You can find an example in `examples/cparser`. Here is a smaller version:
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

x := calculate(2, 7)
log(x)
```

# Passing macros to imported C headers
When importing C headers you probably want to define some macros for the C header to use. At the moment the following way is how that is done but it may improve in the future.

If you want to use the dynamic library version of GLFW then you should do the following:
```c++
// main.btb
#load "libs/glfw-3.3.9/lib-mingw-w64/glfw3.dll"

@define(BUILD_DLL)
#import "GLFW/glfw3.h"

#import "Logger"

glfwInit() // would crash without GLFW_DLL
```

`@define` will define a macro to `1` and C imports after it will have the macro defined. C imports preceding a @define will not have the macro defined. Once defined there is no way to undefine it.


# Bindings in vendor

The main purpose of importing C headers is to eliminate the need to manually write bindings for C libraries. For example, by importing glfw3.h and glad.h, you gain access to all GLFW and OpenGL functions directly. While there are partial bindings in `modules/vendor` many functions are missing because writing complete bindings by hand is time-consuming. We still have `modules/vendor` for when the BTB's C parser is buggy or can't handle certain headers for whatever reason.

# Limitations
This feature is not guarranteed to work especially on different operating systems with different code styles in system headers. You will have to try importing a C header and if it doesn't work then it doesn't work and you must write bindings. If it does then great.

You can add the `--verbose` flag to get more information about how the C parsing is going. If the error message is confusing, the extra verbosity may give you a clue to what is wrong. You are welcome to report issues you find (github or discord).

The goal is to support headers from most libraries: GLFW, GLAD, STB image, OpenXR, OpenAL, OpenSSL, and C standard headers. None of these work at the time of writing this (2025-06-07).

## Known issues
- BTB compiler does not support `cdecl` calling convention.
- BTB compiler does not support variadic functions.
- BTB's C preprocessor may behave differently compared to C compilers when expanding macros and evaluation `#if` expressions.
- The C parser will skip most of the extension and extra content in C standard headers specific to GCC and MSVC compilers. Functions like `strlen` and `atoi` works just fine.

# Implementation and details (old)

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