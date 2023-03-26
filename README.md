# BetterThanBatch
A compiler and runtime for a new scripting language 

## How it works
- Tokenizer
- Generator/Compiler
- Execution/Runtime

## Example (scripting, not finished)
```
he = 91
var = 23 + (he+3/5)
```

## Example (bytecode)
```
number: 29.12
    num $a
    num $f
    load $f number
    add $f $f $f
```