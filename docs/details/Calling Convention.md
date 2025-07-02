# BTB Calling convention
The BTB Calling Convention follows *System V ABI* with a few tweaks to handle multiple return values

This document describes how a BTB function signature relates to a CPU architecture's registers. By reading this document you will understand how
to write assembly that can correctly call functions from a BTB static/dynamic library.

We currently support one architecture: x64

(at the time of writing this we use an old BTB calling convention where we support ARMv7 but it will be some time before i revisit ARM and implement the new convention)

## BTB function signature
A function signature consists of *arguments* and *return values*.

The calling conventions specifies:
- Which arguments are passed in registers and on the stack
- Which return values are passed in registers and on the stack
- Which registers are volatile
- Stack alignment 

## x64
Overview of the call stack.

|Stack|What|
|-|-|
|RBP + 16|start of normal arguments|
|RBP + 8|return address|
|RBP + 0|base pointer (previous RBP)|
|RBP - 8*n_saved_regs|callee saved non-volatile registers (16-byte aligned)|
|RBP - 8*n_saved_regs|start of local variables (grows downwards)|

Arguments are handled exactly like *System V ABI* specifies. The alignment of the stack pointer must be 16 bytes before calling a function.

Return values are handled like *System V ABI* too.
- First two return values of integer type (including char, bool, pointer, 8-byte struct) are placed in `RAX`, `RDX`.
- If first return value is a 16-byte struct it is placed in `RAX`, `RDX`.
- If first return value is larger than 16 bytes a hidden return pointer is used and placed in `RDI` as first argument. All other arguments are shifted to the next argument register.

What differs is that we allow more than 2 return values. With four integers the third and fourth are placed in the return pointer. The return pointer refers to a struct packed with the remaining return values.

Then we have complex situations like this: `fn () -> i32, char[], i32, i32`. The first and third value are placed in `RAX`, `RDX` since the second value is 16 bytes and the doesn't fit in `RDX` and `RAX` is already occupied. The second and fourth value are placed in return pointer `struct { a: char[]; b: i32; }`.

### Non-volatile registers
These registers should maintain their value from a function call: `rbp`, `rbp`, `r12 - r15`.

All float registers, `xmm0 - xmm15`, are volatile.

### A function with zero return values follows this:
First 8 arguments of type `float` are placed in: `xmm0 - xmm7`. The rest are placed on the stack.

First 6 arguments where type size is less or equal to `16 bytes` are placed in: `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`. The rest are placed on the stack. `integer`, `pointer`, `char`, `bool`, and struct


If the general/float registers are filled up then the remaining arguments are placed on the stack.

If the argument type isn't allowed in a register (eg. it's a `struct` or `array`) then it is placed on the stack.

The alignment for arguments placed on the stack are treated as fields in a struct. A 16-bit integer is 2-byte aligned, a 64-bit integer is 8-byte aligned. A struct with two 32-bit integers is 4-byte aligned, a struct with one 32-bit integer and one 64-bit integer is 8-byte aligned.

**nocheckin TODO:** When do we pass a pointer to a struct? const ref

### A function with zero arguments follows this:
First 2 return values of type `integer`, `pointer`, `char`, `bool` are placed in: `rax`, `rdx`.

First 2 return values of type `float` are placed in: `xmm0 - xmm1`.


**nocheckin TODO:** Describe structs, hidden return pointer


### A function with arguments and return values follows this:

**nocheckin TODO:**

### An example
The arguments
```rust
fn complex(a0: i32, a1: f32, a2: char[], a3: i32, a4: i32,
           a5: i32, a6: f32, a7: i32, a8: i32) -> i32, f32[], f32, i32, i32, f32
```

**nocheckin TODO:** Finish up example



**TODO:** Support x86, ARMv7 (ARM 32-bit), ARMv8 (ARM 64-bit)


# Future extensions
## Variadic arguments

BTB currently does **not** support variadic arguments in the language. Instead, the language encourages the use of **function overloading** and **recursive macros** to implement variadic-like behavior with compile-time type safety.

This is the proposed calling convention if we decide to move forward with variadic arguments anyway:

- The variadic arguments space is always placed on the stack after potential normal arguments.
- The start of variadic arguments space is 8-byte aligned. The fields in the space follows classic C struct field alignment.
- Variadic arguments has the following data
    - *rtti_data* refering the the caller's translation units Runtime Type Information (RTTI)
    - *vargs_count* specifying the number of variadic arguments (32-bit integer)
    - *varg_types* a list of type IDs referring to RTTI, each id is a 32-bit integer
    - *varg_values* is the data of the arguments

A struct that represents variadic arguments would look like this:
```c
struct VariadicArguments {
    rtti_data: RTTI*;
    vargs_count: i32;
    varg_types: TypeId[vargs_count];
    varg_values: u8[...];
}
// Defined in modules/Lang.btb (currently defined as three global variables but it will be changed to one global struct)
struct RTTI {
    type_infos: TypeInfo[];
    type_members: TypeMember[];
    type_strings: char[]
}
```

This approach enables passing fully typed variadic arguments at runtime, avoiding the unsafe, promotion-based style of C `...` functions. It also solves cross-module RTTI mismatches by passing a pointer to the caller's RTTI data.

Compared to format strings we use more stack space (with rtti_data, vargs_count, varg_types) but it also let's us skip the hassle and poor type safety of format strings. Well, gcc compiler usually warns you if you messed up the types.

# Resources
[OSDev Wiki - System V ABI](https://wiki.osdev.org/System_V_ABI)

[Microsoft - x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention)