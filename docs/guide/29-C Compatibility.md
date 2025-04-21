The compiler and language is made with some C compatibility in mind.

Summary:
- Integers, floats, booleans and pointers are treated the same (no runtime exceptions or subranges like ADA and Pascal).
- The compiler does not know cdecl. Imported functions (from C libraries) are assumed to be Sys V abi calling convention on Linux and x64 Microsoft C++ calling convention on Windows (stdcall). I have personally never seen cdecl being used on modern computers when compiling with GCC, MVSC.
- You can import function declarations from C headers from a static/dynamic library.

# Importing C headers
That's right. You can import C headers and therefore don't have to create BTB bindings for houndres of functions.

There are limitations:
- C macros from headers do not transfer to BTB.
- Function bodies cannot be parsed. And there shouldn't be any in a header meant to declare functions.
- You have to use `#link` or use compile time build options to provide the library.
- Declarations in the header cannot come from multiple static/dynamic library files.