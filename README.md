# BetterThanBatch
A compiler and runtime for a new scripting language which
doesn't use any libraries.

This readme and all docs is based on an earlier version of the language
which doesn't hvae structs, pointers, enums, namespaces or bitwise operations.
It uses two types: Number and String.
For a small shell like scripting language this was fine but I wasn't happy with this.

The earlier version uses the C++ code in GenParser and Context while the new version uses
Parser, Generator and Interpreter. This new version does have structs and pointers.
enums, namespaces and the rest is being worked on.

The language will still support simple calls to executables.

## Disclaimer
- No automatic tests. Expect old broken features.
- Limited functions to to interract with the operating system.
- Windows only.

## Features
- Variables
- Math expressions
- If statements
- For and while loops
- Break and continue
- Functions
- Usage of C++ functions within a script
- Calling executables like gcc.exe
- #define, #multidefine and #undef (macros/defines are recursive)
- #ifdef (exactly like C)
- #unwrap (for macros)
- Concatenation (.. instead of ## from C)
- Asynchronous functions and multithreading (no locks)

Structs and arrays will probably be left out unless a good idea on how
to deal with them comes to mind.
Language has weak typing at the moment (C is strong while Javascript is weak)

See the Guide/Walkthrough [](docs/guide.md)

## The processs
- Tokenizer     (text -> tokens)
- Preprocessor  (manipulates tokens)
- Parser        (tokens -> bytecode)
- Optimizer     (improve speed and size of bytecode)
- Interpreter   (runs the bytecode)

A normal parser doesn't do bytecode generation but it does here.
This may change. Parser (tokens -> AST), Generator (AST -> bytecode)

## Performance (rough measures)
```
sum = 0
for 1000000 {
    sum = sum + #i
}
```
![](docs/img/perf-23-04-08.png)

The statistics comes from the code above.
The compiler and intepreter was compiled with MSVC
and O2 optimizations (no debug). The statistics
is there to give some idea of how slow the intepreter
is. More thorough testing with consideration to hardware
specification will be done later.

## Example (Fibonacci)
```
N = 10
last = 1
now = 1
temp = 0
for N {
    print now
    temp = now
    now = now + last
    last = temp
}
```

## La spécialité
```
FILES = main.cpp compiler.cpp
g++ FILES -o program.exe
```
GCC and any other executable found in environment variables
can be run like a shell script. Just like batch.

## Building (currently only on windows)
build.bat is used to compile the project.
More on this in the future.

## What will not change?
The tokenizer and preprocessor will stay as they are for the most part.
They work wonderfully well.
Calling executables in a simple manner like batch (and bash) while
using pipes is also an amazing feature.

## Might change
The syntax of the language, weak typing and intepreter has been
chosen because they seem easier to implement than their opposites.
Each side has positive and negatives depending on what the
language will be used for. The language will be used
for mundane and repetitive tasks on your own computer which
is why weak typing and the syntax has been chosen. This may change
though since you can use implicit types.