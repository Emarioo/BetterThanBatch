# Introduction
TODO: What is this language about?

## Compiling code
TODO: How to compile code.

Compile code with this:
`btb main.btb`

For information about the options you can pass to the compiler
`btb --help`



## Compiling the compiler
**Unix**: `build.sh`      (requires `gcc`)

**Windows**: `build.bat`  (requires `cl`, `link`, and `gcc`)

**NOTE**: On Windows, `gcc` can be acquired from MinGW. Installing Visual Studio will give the tools but then
you must make sure `cl`, `link` is available in environment variables. Look into `vcvarsall.bat`