**INCOMPLETE**

The compiler has internal **Bytecode** which is generated from the **Abstract Syntax Tree** (**AST**).

The bytecode allows the compiler to have one component for converting **AST** to some code and then multiple components for converting **Bytecode** to **Machine code** (`x86_gen` or `ARM`). Without an intermediate representation we would need to duplicate a lot of logic in the current BytecodeGenerator into X86Generator and ARMGenerator, or we would need to have one MachineGenerator with a bunch of if statements for each target we support (x86_64, ARM). Imagine adding a new target and forgetting to add code generation for switch or for loop statement.

**Bytecode** also allows us to easily execute it (in our **VirtualMachine**). No bytecode means executing a x86 function we generated at runtime and tweaking the code for relocations which is troublesome. We have more issues if we target ARM when our host is x86, then we need to generate it twice if we also want to execute it. Side note: our **VirtualMachine** still needs to generate a x86 stubs at runtime for BTB functions we want to pass as callback to C code (GLFW3s key/mouse callback for example).

As a user of the compiler you do not need to know anything about the **Bytecode**. While the compiler can output .bc files there is no reason to do this other than diagnosing compiler bugs.

# BTB's Bytecode

The bytecode is independent from CPU architectures (x86_64, ARM) and operating systems. This includes calling conventions where bytecode instructions are written the same for all conventions. The `call` instruction does however specify what calling convention should be used by the **Machine Code Generator** (**MCG**). It also specifies whether an internal bytecode function is called, a function from a static library, or a function from a dynamic library.

<!-- Linking with static or dynamic library should be known when generating bytecode, this is specified in BTB source with `#load "mylib.lib"` or `#load "mylib.dll` (`.a` and `.so` on Linux). -->

The instruction set is a mix of a register and stack-based design.

## Information in a Bytecode object
The **Bytecode** struct contains information about the whole compiled program. This struct alone allows you to generate machine code. This information exists in this struct:
- A list of BTB functions (**TinyBytecode**)
- The target (**TargetPlatform**)
- ...

This information is stored in a bytecode file:
**TinyBytecode** structs represent a single BTB function. It contains this information:
- ...

**NOTE:** The target in bytecode should maybe be the *intended target* since we can override it when doing `btb program.bc --target ARM_x86`. Compiling for Windows and changing target to Linux is a bad idea though. Compiling for x86_64 and then switching to ARM also seems strange unless your doing bare metal? Probably should allow overriding it like this. If none is specified we use the one in .bc, if it is specified then they must be equal .

**NOTE:** In the x86_64 generator and other files you will see that **Compiler** struct is available alongside **Bytecode**. This is because of errors, statistics and other metadata. The actual program is located in **Bytecode**.

### File format

## Instruction set
See `src/basin/core/Bytecode.h`.


## Example
alloc_local l0, 16
local_ptr b, 0
mov [b], 9
mov [b+8], 1

alloc_local l1, 32
mov t0, [b]
arg t0
mov t0, [b+8]
arg t0
call WriteFile, l1, stdcall, importdll

mov [b], rax

mov t0, [b]
arg rcx, t0




// alloc local is called at the start of every function
// it specifies how much stack space is necessary (local variables)
alloc_local 256

mov [lp + 0], 5 // access the local space

<!-- param a, 0, size // access first argument
param a, 1, size // access second argument
param_ptr a, 1, disp // get pointer to second argument -->
    
mov a, [p0] // access first parameter
mov a, [p1 + 8] // access second parameter with an offset of 8 into the value of the argument (accessing second field of a struct)


alloc_local 256

alloc_local 32
push 4
push 9

pop
mov [ap], 1
pop
mov [ap + 8], 2
call hey
mov t0, [rv - 8]
push t0

mov [a0], 9
call yoo
mov t0, [r0]
push t0

pop a
mov [a0], a
pop a
mov [a1], a
call WriteFile, stdcall, importdll


