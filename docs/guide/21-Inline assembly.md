**TODO**: Move document into a document of miscellaneous features?

**DISCLAIMER**: This document describes the current version of inline assembly. It may change in the future to solve inconveniences.

# Inline assembly
The `asm` keyword allows you to write assembly code within curly braces. The assembly block is treated as an expression and can be used wherever expressions are allowed. The unique thing about inline assembly is that it's typeless. The compiler cannot know what type your assembly code outputs if it even is. You must specify the type yourself using `cast_unsafe<your_type>`.
```c++
a := cast_unsafe<i32>asm {
    mov eax, 123
    push eax
}
log(a) // prints 123

// prints 12.3~
std_print(cast_unsafe<f32>asm {
    // IEEE 754 Floating Point.
    mov eax, 4144CCCDh // Assembler doesn't like 0x prefix.
    push eax
});
std_print('\n');
```

## Debugging your inline assembly
- I would recommend a debugger that shows you to the disassembly (most debuggers have this).
- The compiler will generate `bin/inline_asm.asm` which shows the actual assembly given to the assembler.
- Copy assembly block and it's code into a new file and only compile that.
- `@dump_asm` can be used on a scope to see the disassembly of that scope. Wrap your assembly block in one and let the compiler print out the assembly instructions.

## Be careful
Commas as an argument token to macros does not work. It's detected as a separation of macro arguments. That means: you can't pass assembly blocks as arguments to macros.

Even if you can use assembly blocks as default arguments to functions doesn't mean you should. Writting assembly code is difficult because it's easy to make mistakes which you may not notice until later. It's also difficult to read assembly as one statement with various math operations requires many machine instruction to resemble the same thing. Inline assembly is discourage for but it's there for those moments where you really need it.