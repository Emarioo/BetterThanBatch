Inline assembly is about writing assembly directly into your program. This is rarely something you need
but it can be useful if you need extra performance or want to experiment. Do note that it is easy to make mistakes. I would recommend debugging while looking at the disassembly to see exactly what your program is doing.

The compiler utilizes third-party assemblers such as `ml64` (Macro Assembler, Windows) `as` (GNU Assembler, Linux). You are required to have access to one of these assemblers in your PATH.

# Inline assembly
The `asm` keyword allows you to write assembly within the following curly braces. Assembly blocks are treated as expressions and can be used wherever expressions are allowed. The unique thing about inline assembly is that it's typeless. The compiler cannot know what type your assembly code outputs, if any. You can specify the type yourself with this syntax: `asm<type>`. Below are some examples.
```c++
#import "Logger"

a := asm<i32> {
    mov eax, 123
    push eax // push value to "return" it
}
log(a) // prints 123

f := asm<f32> {
    // IEEE 754 Floating Point.
    mov eax, 4144CCCDh // Assembler doesn't like 0x prefix.
    push eax
}
log(f); // prints 12.3 (roughly)
```

Structs can be returned by pushing the fields in reverse order as shown below.
```c++

#import "Logger"

struct Cake {
    name: char;
    size: i32
}
struct Plate {
    cake: Cake;
    color: char
}

plate := asm<Plate> {
    push 'C' // color
    push 2   // cake.size
    push 'n' // cake.name
}

log("cake.name: ",plate.cake.name," cake.size: ", plate.cake.size, " color: ", plate.color)
```

If you want to "give" the assembly values then you can use this syntax: `asm<type>(expressions) {...}`. The expressions within the parenethesis can be seen as arguments pushed to stack which the assembly can then pop. Note once again that assembly is typeless and that any amount of expressions with any type is allowed. Pushing large structs is certainly not recommended.
```c++

n := asm<i32>(6, 9) {
    pop rax
    pop rdx
    add rax, rdx
    push rax
}

```

If you are desperate, you can read variables but it is not recommended because you have to know the exact offset.

```c++
#import "Logger"

num := 65 // placed at rbp-8

new_num := asm<i32> {
    mov eax, [rbp - 8] // read variable 'num'
    add eax, 5
    push eax
}
log(new_num) // prints 70
```
These are some things that affect where variables are placed on the stack:
- If non-volatile registers are saved then `rbp-8` should changed by -8 per saved register. Currently, only @stdcall and @unixcall calling conventions save 4 registers. `rbp-48` would be the address of the first variable.
- Return values will offset the address by the size of all return values plus padding for 8-byte alignment. The return values `ì32, i64` requires this `rbp-24`. `i16,i16` requires `rbp-16`.

**DISCLAIMER**: The calculation for offsets to variables may have changed, the text may not be up to date.


## Random implementation details
- asm is parsed after preprocessor and lexer, meaning macros can work.
- Assemblers can be pedantic with newlines after instructions and so you may have to put each instruction on it's own line.'
- Maximum of 255 pushes as input to inline assembly and 255 pops as output. This does not mean 255 expressions if some expressions are structs with multiple members.

## Debugging your inline assembly
- I would recommend a debugger that shows you to the disassembly (most debuggers have this).
- The compiler will generate `bin/inline_asm123.asm` which shows the assembly given to the assembler.
- `@dump_asm` can be used on a scope to see the disassembly of that scope. Wrap your assembly block in one and let the compiler print out the assembly instructions.

## Upcomming features
Safer and less error prone way of reading/writing variables.
```c++
num := 65
new_num := asm<i32> {
    mov eax, [num]
    eax, 123
    push eax
}
```

## Other notes
When passing (not that you ever would) inline assembly to preprocessor macros you must wrap them in parenthesis because the commas within the assembly will be recognized as separators for arguments.

Even if you can use assembly blocks as default arguments to functions doesn't mean you should.