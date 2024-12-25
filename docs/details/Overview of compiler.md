# Overview of the compiler
These chapters, in `docs/details`, describes the implementation of the compiler. The chapters cover the most noteworthy parts.

**NOTE**: CHAPTERS ARE WORK IN PROGRESS

## The phases
The name of each step may not accurately represent what's
actually happening. Type checker infers and creates types for
structs, enums and functions. The generator also does some
more detailed type checking with expressions and when the
values are put in registers and pushed to the stack.

Some preprocessing is done after the parser like
\_FUNCTION\_ which isn't know until after parsing.

- Lexer             (text -> tokens)
- Preprocessor      (tokens -> tokens)
- Parser            (tokens -> AST)
- Type checker      (modifies types in AST)
- Generator         (AST -> bytecode)
- Virtual Machine   (runs the bytecode, compile time execution)
- x64 Generator     (bytecode -> object file -> executable)
<!-- - Optimizer         (bytecode -> faster and smaller bytecode) -->


## First step
The first step in the compiler parses the command line arguments.
- Which file to compile?
- What file to output, what platform?
- Running tests?
- Should the executable (or bytecode) be executed after compilation?
- How much log information? (verbose, silent)


## My idea how the usage of the compiler works
All that is shown does not work.
```c++
// main.cpp

// Decide how you want to compile your source.
CompileOptions options;
options.source = "src/main.btb";
options.output = "bin/main.exe";
options.optimizations = OPTIM_SPEED;

// Compile the source.
Compiler compiler;
compiler.run(&options)

// If you want to compile again with in-memory caching
// then just compile again. You would probably have a loop
// and a tcp server or inter process communication to tell the
// compiler when to compile again.
compiler.run(&options)

// here you can access stats, lines and bytes of source code.
compiler.stats

compiler.cleanup(); // clean things up if you need to.
```