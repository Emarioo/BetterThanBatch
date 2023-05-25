Optimizer would be a function/module which takes in bytecode and makes it faster and smaller.
The reason the optimizer is a seperate phase is to keep things simple. If one phase does multiple things then that phase will be much harder to implement.

## Theory of optimizing instructions
Recognizing the more complex scenarios is very difficult. You could write algorithms for each scenario but this will take time and running each algorithm separately is inefficient.
Crafting a **theory** for how the instructions depend on each other is a better alternative in the long run. It also more difficult.

#### Deconstructing operations
An instruction can create, delete and modify values.
Each instruction can perform varying types of operations on each register (r0,r1,r2). Each register of an instruction is marked as `OP_READ` (const read), `OP_MOVE` (replace reference), `OP_MODIY`, `OP_DEL` or `OP_NEW`.

- **Creation**: `BC_NUM`, `BC_STR`, `BC_COPY`, `BC_SUBSTR` and such.
- **Deletion**: `BC_DEL`, `BC_STOREV` (sometimes).
- **Required**: `BC_RUN`. This may have an external effect.
- **Modified**: `BC_MOV`, `BC_ADD` and such which modifies and moves data.
- 

## What to optimize?
- Redundant instructions like `mov $5 $5`, `add $0 $0 $0`, `add $a $0 $a`
- Scale down allocations. The arrays holding constants and instructions are resized exponentially. Once compilation is done these arrays will not change. After optimizations are done you can resize to the actual array size.
- While optimizing you can also find bugs like `BC_NUM $13` and then `BC_MOV $12 13`. The number created by `BC_NUM` is lost due to `BC_MOV`. This is a bug in the compiler and optimzer should see this and throw an error. Ideally the compiler is perfect and doesn't have these bugs but that isn't the reality so the optimizer can make up for it. Catching the complex cases where values are lost isn't necessary. As long as you catch the simple ones and save time you can call this feature a success.
- Jump instructions with if, else, break and continue can be optimized.
```
for i : 5 {
	if i == 2 {
        print hey
    } else {
        continue
    }
}
```
Instead of JUMPNIF at `if 1 == 2` jumping to continue you can directly jump to `for`.

## Removing/adding instructions
The bytecode is an array of instructions. Simply removing an instruction will leave gaps in the array. Adding instructions won't work since the array is packed. You could always do a memmove but if the bytecode is 50 kB and you need to remove/add many instructions then it's very inefficient to do so multiple times.
- When it comes to leaving gaps you could pack everything to the left when optimization is complete. You would then only minimum required memmoves to fill the gaps.
- Adding instructions is thougher. You could split the bytecode into chunks and at the end combine them. Doing a memmove on the individual chunks doesn't require you to move the rest of the bytecode. Splitting into chunks should probably be done at the beginning.
**Final say**
Doing memmove each time you modify the bytecode is fine for now. It adds some extra compile time but ultimately the code will be small and fast.

## Optimization based tokens and bytecode
The more information the optimizer has the more it can optimize. Knowing just bytecode may limit the potential. If optimizer has access to `Tokens` then you may be able to do more? Altough I am not sure how much you would actually gain so this can be implemented later.

## Enable/disable certain optimizations
`OPTIMIZE_REDUNDANT`

```
BC_LOADV $20 1
BC_COPY $20 $20
BC_NUM $21
BC_LOADC $21 10
BC_ADD $20 $21 $20
BC_DEL $21

Can be optimize to

BC_LOADV $21 1
BC_NUM $20
BC_LOADC $20 10
BC_ADD $20 $21 $20
```

105 lines in 0.00 seconds (avg 1440.92 ns/line)
 2.29 K instructions in 0.00 seconds (avg 66.13 ns/inst)
 15.12 M instructions per second (15.12 MHz)