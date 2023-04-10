# BetterThanBatch
A compiler and runtime for a new scripting language which
doesn't use any libraries.

Currently the things below works but need some tweaks.
- Variables
- Math expressions
- If statements
- For and while loops
- Break and continue
- Functions
- Usage of C++ functions within a script
- Calling executables like gcc.exe

## How it works (or will work)
- Tokenizer (lexer)
- Preprocessor
- Generator (parser and compiler for bytecode)
- Optimizer (optional)
- Execution (intepreter for final bytecode)

## Building (currently only on windows)
build.bat is used to compile the project but
compiling scripts with it doesn't really work
yet. Things will probably work in May.

## Performance (rough measures)
![](docs/img/perf-23-04-08.png)
The statistics comes from the code below.
The compiler and intepreter was compiled with MSVC
and O2 optimizations (no debug). The image
is there to give some idea of how slow the intepreter
is and more thorough results with hardware
specification will be done later.
````
sum = 0
for 1000000 {
    sum = sum + #i
}
```

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

## Example (bytecode)
```
number: 29.12
    num $a
    num $f
    load $f number
    add $f $f $f
```

## La spécialité
```
FILES = main.cpp compiler.cpp
g++ FILES -o program.exe
```
GCC and any other executable found in environment variables
can be run like a shell script.