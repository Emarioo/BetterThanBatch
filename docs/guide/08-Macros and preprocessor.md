# Macros
Macros allows you to modify tokens of the source code before it is parsed.

```c++
// Note that the language doesn't support the ternary if operator (x ? y : z).
#macro min(a,b) (a < b ? a : b)

min(3,7) -> (3 < 7 ? 3 : 7)

#macro GAME_VERSION "0.4.6/beta"

GAME_VERSION -> "0.4.6/beta"
```

**NOTE**: The arrow `->` is used in this guide to show the result of a macro (`macro -> result`).

The content of a macro can exist on multiple lines. This is done by not having any tokens after the parameters where the macro was defined.
```c++
#macro large(X,Y) // <- no tokens
if(X == Y) {
    std_print("Value was not equal")
} else {
    std_print("Value was equal")
}
#endmacro // don't forget to mark the end
```
Because a multiline macro is defined with zero tokens on the same line as the definition you mustn't forget an `#endmacro` when defining empty macros.
```c++
#macro ENABLE_LOGGING #endmacro

// #ifdef is covered later
#ifdef ENABLE_LOGGING
    std_print("we are logging")
#endif
```

## Variadic and recursive macros
One macro name can have multiple definitions based on the amount of arguments. Macros to be expanded are matched with the definition which matches the argument count. The collection of macro definitions under one macro name is called macro namespace (I am open for other ideas as namespace already is a term and a keyword).
```c++
#macro add(x,y)   x + y
#macro add(x,y,z) x + y + z

add(1,2)   -> 1 + 2
add(1,2,3) -> 1 + 2 + 3
```
Variadic macros allows you to pass an arbitrary amount of arguments during macro expansion. Each macro name can only have one variadic macro definition. The argument matching would be ambiguous otherwise.
```c++
#macro add(x,...) x + ...

add(1)     -> 1 +
add(1,2)   -> 1 + 2
add(1,2,3) -> 1 + 2, 3

#macro sub(x, ..., y) x + ... + y

sub(1,2,3,4,5) -> 1 + 2,3,4 + 5
sub(1,5)       -> 1 + + 5
```
The combination of argument matching and variadic definitions is the basis for recursive macros (in this language). All recursive macros needs a **base case** where a macro is matched with a definition that doesn't recurse. An empty macro definition for zero arguments is automatically created when defining a variadic macro.

This is a recursive version of the `add` macro and the compilers imaginary expansion algorithm.
```c++
#macro add(x,...) x + add(...)

add(1,2,3) -> 1 + 2 + 3 +

// Expansion phase 1
add(1,2,3) -> 1 + add(2,3)
add(2,3)   -> 2 + add(3)
add(3)     -> 3 + add()
add()      ->  // <- empty macro that was created automatically
// Expansion phase 2
add(1,2,3)        -> 1 + add(2,3)
1 + add(2,3)      -> 1 + 2 + add(3)
1 + 2 + add(3)    -> 1 + 2 + 3 + add()
1 + 2 + 3 + add() -> 1 + 2 + 3 +
```

To skip the remaining `+` we are required to define our own **base case**.
```c++
#macro add(x,...) x + add(...)
#macro add(x)     x
add(1,2,3) -> 1 + 2 + 3

// Phase 1 and 2 combined
add(1,2,3)     -> 1 + add(2,3)   // add(x,...)
1 + add(2,3)   -> 1 + 2 + add(3) // add(x,...)
1 + 2 + add(3) -> 1 + 2 + 3      // add(x)
```

Another example
```c++
#macro equals(...) ( equals_inner(...) )
#macro equals_inner(X,Y,...) X == Y && equal(...)
#macro equals_inner(X,Y)     X == Y

equals(a,1)   -> ( a == 1 )
equals(a,1,4) -> ( a == 1 && a == 4 )
```

TODO: Explain recursion limit (100 or something, but can be increased per macro).

**DISCLAIMER**: The preprocessor does not handle whitespace very well. The output of recursive macros may have space and new lines where you don't want them. Normal macros are usually fine.

## Conditional directives
Conditional directives can leave out tokens from the source code based on conditions.
```c++
#macro YES
#macro MAYBE

#ifdef YES
    std_print("yes")
#elif MAYBE
    std_print("maybe")
#else
    std_print("no")
#endif

#ifndef YES
    std_print("not yes")

#elnif MAYBE
    std_print("not maybe")
#endif
```

**DISCLAIMER**: The only supported condition right now is whether a macro is defined or not.

# Utilities from the preprocessor
The preprocessor which is the name of the system that is in charge of macros also provides other directives to retrieve token information, modify tokens, or perform some useful specific logic.
```c++
// These directives are replaced with number and string literals
line: i32    = #line
column: i32  = #column
file: char[] = #file       //  /usr/home/you/main.btb
file: char[] = #filename   //  main.btb
file: char[] = #date       //  YY.MM.DD

// Turns the following token into a quote (mostly inside macros)
str := #quote i_am_a_string

// Increments a global counter at compile time. You are guaranteed to receive a unique unsigned 32-bit integer.
id0 = #unique // 0
id1 = #unique // 1
```


TODO: Default definitions, OS_WINDOWS, OS_UNIX