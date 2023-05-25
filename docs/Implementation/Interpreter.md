Happens in `Context`

We have bytecode. It has a list of instructions and constant data.

We begin executing code at the first instruction. We perform whatever the instructions tells us. And that's it.

## What can the instructions do?
- Operators like add, less, mul, pow, sqrt...
- Jump to addresses. (also conditional jump to represent `if` and `while`)
- Run functions
- Enter/Exit scope

## What are functions?
Basically jump with some added management when it comes to scope, arguments and return values.

The body of the function is just a part of instructions in the whole bytecode. You can call the function by jumping to the first instruction altough you would use the run instruction since it handles some extra stuff.

A function will alter registers and when function returns the registers should return to their original values. This is where scopes come in. With the instructions EnterScope and ExitScope you can change which scope's registers you are modifying.

Note that return doesn't return from just a scope. It can return multiple scopes in a function which has loops and if statements.

**Instructions which transfers registers between scopes?**
Run, return?

Combined 9164.43 ms time (avg 91.64 ms)
Combined 9277.29 ms time (avg 92.77 ms)
Combined 9171.63 ms time (avg 91.72 ms)

Combined 9325.21 ms time (avg 90 ms)
Combined 9325.21 ms time (avg 93.25 ms)