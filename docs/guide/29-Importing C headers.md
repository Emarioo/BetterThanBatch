**EXPERIMENTAL FEATURE, see _Limitations_ section further down**

You can import a C header with `#import "util.h"`. The functions, structs, and enums in the header and further included files from C #include will then be available in the BTB source code.


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

# Performance
The C to BTB transpiler is slower than just using bindings in BTB. It should be acceptable for an experimental feature. We will make some improvements and provide metrics in the future.

We will implement these to speed it up and make it better:
- Reuse transpiled BTB code to avoid transpiling every compilation. If last write timestamp on any included header changes then we must re-transpile.
- If multiple BTB files import the same header then we shouldn't transpile every time we import it. We can transpile it once and reuse it for all imports. If include directories or macros passed to imported C header is different then we need to transpile multiple times.

# Bindings in vendor

The main purpose of importing C headers is to eliminate the need to manually write bindings for C libraries. For example, by importing glfw3.h and glad.h, you gain access to all GLFW and OpenGL functions directly. While there are partial bindings in `modules/vendor` many functions are missing because writing complete bindings by hand is time-consuming. We still have `modules/vendor` for when the BTB's C parser is buggy or can't handle certain headers for whatever reason.

## Known working library headers
|Library|Lib version|Last checked|BTB version|Note|
|-|-|-|-|-|
|glfw3.h|v3.3.9|2025-07-24|v0.2.1, ce6aba572efe||
|libs/glad/include/glad.h||2025-07-24|v0.2.1, ce6aba572efe||
|libs/stb/include/stb_image.h||2025-07-24|v0.2.1, ce6aba572efe||
|libs/stb/include/stb_image_write.h||2025-07-24|v0.2.1, ce6aba572efe||

Libraries we want to support: `OpenXR`, `OpenSSL`, `libopus`. `Tracy` (more or less).

# Limitations
The BTB Compiler has a C parser which handles most C syntax but not all. That which it cannot handle is skipped an in theory the syntax it can handle is available to you. But since some types are skipped it usually means that function declarations complain about a missing type. For example system headers have *\_\_attribute\_\_* keyword on struct fields which C parser can't handle. The whole struct is therefore skipped and any functions that use it will cause error in BTB compiler about unknown type.

This feature is not guarranteed to work especially on different operating systems with different code styles in system headers. You will have to try importing a C header and if it doesn't work then it doesn't work and you must write bindings. If it does then great.

You can add the `--verbose` flag to get more information about how the C parsing is going. If the error message is confusing, the extra verbosity may give you a clue to what is wrong. You are welcome to report issues you find (github or discord).

## Known issues
- BTB compiler does not support `cdecl` calling convention.
- BTB compiler does not support variadic functions.
- BTB compiler does not handle *\_\_attribute\_\_* on struct fields.
- BTB does not support 128-bit floats, meaning functions, variables, types with `long double` are skipped.
- BTB's C preprocessor may behave differently compared to C compilers when expanding macros and evaluating `#if` expressions.
- Importing the same header in multiple BTB files can cause issues or collisions (might be fine but we have not considered this)

<!--
We should move this to "details/C parsing.md".
Not relevant for the guide.

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

-->