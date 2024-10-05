**WARNING:** Virtual machine is a bit unstable because it's not tested often or prioritized. Calling functions from external libraries works with operating system libraries like Kernel32.dll. Other libraries such as GLAD and GLFW currently fails.

# Why is there bytecode and a virtual machine?
Bytecode and a VM allows you to execute code at compile time with ease. The compiler has control over what and how things are executing and it is possible to implement whatever features you want. If you had to compile to machine code and then execute then it would be harder (not impossible) to run smaller expressions and harder to interract with the rest of the compiler.

The second reason for bytecode is an intermediate representation. The language has many features and writing a machine code generator that converts an abstract syntax tree (AST) to machine code for every architecture is more work than necessary. With bytecode, we can write one generator from AST to bytecode and then a generator from bytecode to machine code for every architecture. This is a common approach, LLVM (a backend for generating machine code) has it's own intermediate representation.

The compiler will generate executables (machine code) by default because there is only a couple of reasons you want to run your whole program in a virtual machine.
- The compiler doesn't support your architecture or operating system. If so, then you can compile the compiler your self and then run code in the VM. You won't have any executable but you can at least run small scripts that doesn't require high performance. In reality, the compiler only supports two operating systems and x86 because the whole compiler assumes little endian architecture.
- Testing, compiling, and running large programs in the VM successfully means that compile time execution is very likely to work.


# How to run bytecode in VM?
Right now you cannot export bytecode to a file and then run it. You always have to compile and run bytecode directly. (i don't have a good reason to export bytecode to file because you might as well recompile everytime, the compiler is fast after all)

Use -vm flag to run in virtual machine.
```bash
btb examples/random/defer -vm
```

Use -pvm flag to log all instructions virtual machine. Source code information is included.
```bash
btb examples/random/defer -pvm
```

Use -ivm flag to step through each instruction. This is **experimental** and does not work all the time. (in fact, it's broken as I am writing this but it worked a couple of months ago when I implemented it)
```bash
btb examples/random/defer -ivm
```

# How fast is the VM?

This code ran on a computer with this hardware: Windows, i7-4790K, 4GHz, 4/8 cores/threads.
The BTB compiler was compiled with MSVC compiler with highest speed optimizations (VM ran 3.3x faster than normal) and ASSERT_HANDLER off (VM ran 6x times faster).

```cpp
// examples/dev.btb
#import "examples/map_perf"

TestMap(1000,4000);
```

```bash
btb examples/dev.btb -vm

Linker command: gcc -g bin/main.o -lKernel32 -lBcrypt -o main.exe
Compiled 5.10 K lines, 167.01 KB
 total time: 0.27 s, link time: 84.84 ms
 options: win-x64, gcc
 output: bin/main.o, main.exe

Executed 1975827702 insts. in 9.41 s (209.86 M inst/s)
```

It seems to be around 207-210 million bytecode instructions per second (each instruction is roughly 2-8 bytes).