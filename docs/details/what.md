What the .... am i makin

I'm making Basin for myself because i want control.

What I need it to be is extendable in the future.

At the moment bytecode, x86, program, debug info, lexer, parser, compiler task system is all integrated and nested. No clear seperation.
Implementing multithreading is hard. Implementing new features is tedious, a lot to keep track of.

The Basin compiler has a public API you can use to compile files or pieces of code. You can decide to parse just the AST investigate and modify it and then generate executable.
As i said, extendable.

For this the compiler needs clear architecture around memory, compile steps and threading, global memory.
The compiler should be built upon primitives.


**First primitive is source code**, comes in the form of a file, a piece of text passed to compiler from C/C++ or metaprogamming.

**Second primitive is AST**, It represents the program using a tree structure.

**Third primitive is Bytecode**, Created from AST, can run in VM or be converted to machine code.

**Fourth primitive is machine code**, A struct containing raw machine code and relocations. What's up with debug information.

