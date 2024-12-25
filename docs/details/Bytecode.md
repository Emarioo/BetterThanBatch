**INCOMPLETE**

# Bytecode
The compiler produces bytecode which can be executed by a virtual machine or further compiled to machine instructions.

The bytecode is independent from computer architecture and operating system. It is a mix of a register-based and stack-based design.

## Instruction set
See `include/BetBat/Bytecode.h`.


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


