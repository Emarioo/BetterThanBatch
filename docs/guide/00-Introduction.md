# Introduction
TODO: What is this language about?

## Compiling code
TODO: How to compile code.
TODO: Test all the code showcased in the guide. I have not actually tested anything so I could have messed up the syntax somewhere.

Compile code with this:
`btb main.btb`

For information about the options you can pass to the compiler
`btb --help`



## Compiling the compiler
**Unix**: `build.sh`      (requires `g++`)

**Windows**: `build.bat`  (requires `cl`, `link`, and `g++` for DWARF debug information)

**NOTE**: On Windows, `g++` can be acquired from MinGW. Installing Visual Studio will give the tools but then
you must make sure `cl`, `link` is available in environment variables. Look into `vcvars64.bat`