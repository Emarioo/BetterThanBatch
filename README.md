# BetterThanBatch
A compiler and runtime for a new programming language.

## Disclaimer
- Windows only

## Features
- Variables, functions, namespaces, structs, enums.
- If, for, while, break, continue, return, defer, using...
- #define, #multidefine and #undef (macros/defines are recursive)
- #ifdef (exactly like C)
- #unwrap (for macros)
- Concatenation (.. instead of ## from C)
- #import which is better than #include when it comes to splitting
  code into multiple files.
- #include also exists

## On the way
- Usage of C++ functions within a script
- Calling executables like gcc.exe
- Polymorphism
- Operator and function overloading

(guide is not up to date)
See the [Guide/Walkthrough](docs/guide.md)

## The processs
The name of each step may not accurately represent what's
actually happening. Type checker infers and creates types for
structs, enums and functions. The generator also does some
more detailed type checking with expressions and when the
values are put in registers and pushed to the stack.

Some preprocessing is done after the parser like
\_FUNCTION\_ which isn't know until after parsing.

- Tokenizer     (text -> tokens)
- Preprocessor  (tokens -> tokens)
- Parser        (tokens -> AST)
- Type checker  (modifies types in AST)
- Generator     (AST -> bytecode)
- Optimizer     (bytecode -> faster and smaller bytecode)
- Interpreter   (runs the bytecode)

## Examples
You can find some examples in the example folder.
[Random code](example/ast.btb)

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