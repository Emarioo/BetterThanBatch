**VERY EXPERIMENTAL FEATURE**

When importing a C header, the function declarations in that file will be converted to BTB and available to you. This includes structs, enums, and typedefs.

You can find this example in `examples/cparser`.
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

The purpose of importing C headers in BTB is to eliminate the need to manually write bindings for C libraries. For example, by importing glfw3.h and glad.h, you gain access to all GLFW and OpenGL functions directly. While there are partial bindings in `modules/vendor` many functions are missing because writing complete bindings by hand is time-consuming.

When importing a C header you still need to link against the corresponding library, just like in C. This is done by adding `#load "libs/glfw3-3.8.0/glfw3.lib"` to your source code (assuming you’ve placed the GLFW library in a libs folder). If you forget to link the library you’ll get linker errors, just as you would in a C project.

# Limitations (there's a ton)
This feature is not guarranteed to work especially on different operating systems with different code styles in system headers. You will have to try importing a C header and if it doesn't work then it doesn't work. If it does then great.

The goal is to support headers from most libraries: GLFW, GLAD, STB image, OpenXR, OpenAL, OpenSSL, and C standard headers. None of these work at the time of writing this (2025-06-07).

## Known issues
- We do not handle cdecl calling convention
- We do not support variadic functions
- We have problems preprocessing C standard headers (a lot of attribute, macros and so on)
- We have problems handling includes in C headers
- You cannot import defines

Some of these problems will be fixed in due time but it may take many years depending on how important they are compared to everything else in the compiler.
