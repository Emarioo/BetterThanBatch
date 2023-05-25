# BetterThanBatch
A compiler and runtime for a new programming language.

## Disclaimer
- Windows only

## Features
- Variables, functions, namespaces, structs, enums and namespaces.
- If, for, while, break, continue and return.
- #define, #multidefine and #undef (macros/defines are recursive)
- #ifdef (exactly like C)
- #unwrap (for macros)
- Concatenation (.. instead of ## from C)

## On the way
- Usage of C++ functions within a script
- Calling executables like gcc.exe
- Polymorphism
- Operator and function overloading
- More things are on the way too.

(guide is not up to date)
See the Guide/Walkthrough [](docs/guide.md)

## The processs
- Tokenizer     (text -> tokens)
- Preprocessor  (tokens -> tokens)
- Parser        (tokens -> AST)
- Generator     (AST -> bytecode)
- Optimizer     (bytecode -> faster and smaller bytecode)
- Interpreter   (runs the bytecode)

## Examples
You can find some examples in the example folder.
[](example/ast.btb)

## Building with vcvars (Visual Studio)
Running the commands below will allow you to compile the project.
vcvars64 can usually be found in `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build`.
```
.../vcvars64.bat
build.bat
```

## Building with g++
If you go into build.bat you can remove SET USE_MSVC=1
and make sure SET USE_GCC=1 is used.
Running build.bat will then compile the project with g++.
Make sure GCC is installed.

More build options may exist in the future.