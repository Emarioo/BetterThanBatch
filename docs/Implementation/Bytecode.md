# Instructions
Each instruction is 32 bits (4 bytes). The first byte describes the type (add, jump...). The other bytes depend on the instruction. With add, each byte represents a register.

# What's in the bytecode
- List of instructions which operate on registers
- Number and string constants

# Register defaults
**$0** not used, may be reserved for null.
**$1** accumulator
...

# Abbreviations
`jump $a` is the format of the instruction.
`jump func` is an abbreviation of `load $t0 func; jump $t0`.
`add 1 6 $a` and `add "ok" $a $b` is also possible.

# Calling a function
`run $a $b` call a function. `$a` is the argument (string or number). `$b` is a number of the address where the function is located in the bytecode.

# Calling C++ functions
To call a function type `run $a $b` where `$a` is an argument and `$b` is a string. If the string can be resovled to a function name like `print` then the mapped C++ function will be called. `run "hello" "print"` will print hello. 

# Calling executables
Same as with function but if the string cannot be resolved to a function it  will search for it in working directory and environment variables.
`run "main.c" "gcc.exe"` will run `gcc` if it can be found. If the string has no format then `.exe` is assumed.

# Thoughts / planning
How large is one bytecode? At least one byte for the type. This gives 256 different bytecode types. Should be enough.

If registers are used we may want to use 2 input registers and 1 output for math operations. This results in 3 bytes. The registers probably have variable pointers in them, these are set using a bytecode instruction like **BC_load_var**, **reg_index**, **var_name**. var_name is two bytes. Instructions are 4 bytes which is small and good. (5+9/(x+6)). This expression can be calculated within registers without using any variables. A problem here may be that you aren't going to use all the 256 registers.
Compiler will probably only end up using 16. Instruction of two bytes gives 8 combinations for the 2 inputs and 4 for output (3+3+2 bits).

If use variables 256 is too small, 65536 variables is better. 2 in variables, 1 output gives 6 bytes. Bytecodes would operate on variables directly. Each variable has a number. Variable name to variable number can be computed at compile time. With debug information the variable name should probably still be displayed next to the variable number.

Bytecode instructions have different lengths. Add operation may have 30 bytes. 20 characters for the 2 input variable names, 10 bytes for output variable name. This will be slow because CPU cannot predict as much and this also wastes memory.

# Instructions
```
struct Instruction {
	uint8 type;
	uint8 r0;
	union {
		struct { // math ops use this
			uint8 r1;
			uint8 r2;
		};
		uint16 v0;
	};
};
Instruction CreateInstruction(uint8 type, uint8 r0, uint8, r1, uint8 r2);
Instruction CreateInstruction(uint8 type, uint8 r0, uint16, v0);
```
**Rx** refers to a register. x is just a number to differentiate the registers within the instruction. **Vx** refers to a variable index. **Dx** refers to a static data element. Could be a string or number.

**BC_directive DirType R0** : DirType specifies a directive type like `#index` in number form. The instructions computes a value and puts it into R0.
**BC_add R0 R1 R2**
**BC_load_var R0 V0**
**BC_load_data R0 D0**
**BC_run_func R0 R1** : R0 specifies function, R1 specifies arguments.
**BC_returned R0** : R0 where returned value is put
**BC_if?**
**BC_jump_if?**
**BC_jump**



# api functions
In the generator and context you map addresses to certain functions.
`#define API_ADDR_PRINT 0x40000000`
`#define API_ADDR_PRINT_S "print"`
When you type `load print` in byte language the resolver will see `print` and the same as normal resolve it to an address (which is just a number).
In the context when you `jump` or `run` that address, it will match with `API_ADDR_PRINT` and instead run a C++ function.

# executables
Same as when calling functions but when a constant is unresolved it will create an address for it. maybe?

Consider `gcc main.c`. A constant is made for main.c (`arg: "main.c"`).
`gcc` is not a variable and not a known function. We then map it to an address. In code when we call `gcc` we type `load gcc` (`load "g++"` should be possible too). Register `$lr` will have the address for gcc. `run $a $lr` will run the stuff. The implementation of the context will know that `gcc` must be searched for in cwd or environment variables.

Now... how to do you specify whether you want to wait for gcc to finish. Or if you want to create a new console when calling gcc?