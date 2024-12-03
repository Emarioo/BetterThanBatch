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

## Global macros (experimental, unpredictable)
Normal macros (`#macro`) are defined inside one file and only visible in that file and other files that import that file.

Global macros are visible for all files regardless of where they are defined. If you define them in the first file that is compiled then they will be visible everywhere. if you define them in a file that is imported by many other files then it is very hard to predict which files have access to them, especially when the compiler uses multiple threads and processing multiple files at once.

```c++
// main.btb
#import "Logger"

#global_macro THE_CONST 9

log(get_const())

// util.btb
fn get_const() -> i32 {
    // if THE_CONST is defined as a normal macro then the compiler
    // will tell you "THE_CONST is not defined"
    return THE_CONST
}
```

**NOTE:** In the future, global macros may be **restricted** to the initial source file that is specified on the command line.


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

**DISCLAIMER**: The preprocessor does not handle whitespace very well. The output of recursive macros may have strange indentation and new lines where you don't want them.

Here are some useful extra directives for macros
```c++
#macro add(x,...) x + add(...)
#macro add(x,y) x + y

// unwrap/spread tokens from a macro separated by commas.
#macro LIST 1,2,3,4
add(#unwrap LIST)

// concatenation with ##
#macro LN var ## #line: i32;
LN

// counter, each occurrence of counter increments a temporary
// counter within the macro evaluation. Separate evaluation
// has independent counters. A local version of #unique.
#macro DECL(x,...) var ## #counter: i32; DECL(...)

DECL(a,a)

DECL(a,a,a,a)
```

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
file: char[] = #file      // project/main.btb (relative to current working directory, absolute path if not from current working directory)
file: char[] = #fileabs  // /home/you/project/main.btb
file: char[] = #filename  // main.btb
file: char[] = #date      // YYYY.MM.DD (month and day will be prefixed with zero when necessary)

fn the_func() {
    file: char[] = #function  // the_func
}

// Turns the following token into a quote (mostly inside macros)
str := #quote i_am_a_string

// Increments a global counter at compile time. You are guaranteed to receive a unique unsigned 32-bit integer.
id0 = #unique // 0
id1 = #unique // 1

// #counter evaluates to a local counter and increments it
// the counter exists per macro evaluation, thereby being local.
#macro add(x,...) #counter + add(...)
#macro add(x,y) #counter + #counter

add(a,b,c,d)       // = 6
add(a,b,c,d,e,f,g) // = 21
```
**NOTE:** `#function` is implemented in the parser because the preprocessor is not aware of functions. This means that you cannot concatenate `#function` with other tokens.

# Predefined configuration macros
The compiler pre-defines a couple of macros.
- **OS_WINDOWS** - When targeting Windows
- **OS_UNIX** - When targeting Linux
- **BUILD_EXE** - When building exe
- **BUILD_DLL** - When building dynamic library
- **BUILD_LIB** - When building static library
- **BUILD_OBJ** - When building object file
<!-- - **BUILD_BC** - When building bytecode (not supported) -->
- **BUILT_FOR_VM** - When building and directly running code in virtual machine
- **LINKER_MSVC** - When linking with Microsoft Visual C/C++ Compiler (link)
- **LINKER_GCC** - When linking with GNU Compiler Collection (gcc)

# Function inserts
**Added in 0.2.1**

Function inserts allows you to insert a text string at the top of functions. It can be used to easily profile and trace all or a selection of functions in your programs.

```c++
#import "Logger"

#function_insert "compute"
    log("HI FROM INSERT")
#endinsert

fn compute() {
    log("compute working...")   
}
fn main() {
    log("main start")
    compute()
    log("main end")
}
```

The syntax of function inserts is shown below. Priority is optional.
```c++
#function_insert [priority] <pattern>
    <insert_content>
#endinsert
```

## Details of pattern matching
Function inserts declared in one file is available for every import. Pattern matching allows you to select which functions should have the insert. You can declare as many function inserts as you want.


The pattern consists of strings (with wildcards) and logical operations.
Here are some strings:
```c++
// these strings will try to match functions in every file
"compute" // will match all functions named 'compute'
"get_*" // will match all functions that begin with 'get_'
"*_unsafe" // will match all functions that end with '_unsafe'
"*list*" // will match all functions that contains 'list'
"get*int*" // THIS IS NOT ALLOWED!

// These will match all function names in the specified files
f"src/main.btb" // will match every function in the file 'src/main.btb'
f"*/util.btb" // will match every function in all the files named 'util.btb'
f"*test*" // will match every function in the files that contain `test` in their file path

#file // will match all functions in the current file where the function insert was specified
```
**NOTE:** File paths you match with may be relative to current working directory if possible (./src/code.btb).

<!--
Not sure how to formulate this well. These good practises was more relevant when we matched with absolute paths. Since we always do relative paths we are kind of forced to use good practise.

A good practise is to use a wildcard as the first character (`*/src/code.btb`) since it's more likely to work for people wherother people with different file structures.

**NOTE:** Another good practise is to never use absolute paths in the string patterns since another person who wants to compile your code may have your source code files in a different directory.
-->

Matching function names and files paths is great but what if you want to match a specific function in certain files. Logical operations allow you to be more creative.

```c++
f"*main.btb" and "compute" // will match all functions named 'compute' in the files named 'main.btb'
f"*test*", f"*example*" // will match all functions in the files that contain 'test' or 'example' in their path.

#file and ("main", "*compute*") // will match functions in current file that are named 'main' or contains 'compute'
```

The final component in pattern matching for function inserts is `not`. It is used like this:
```c++
not "*get*" // will match all functions that do NOT contain 'get'
not ("*test*", "*example*") // will match all functions that do not exist in file paths containing 'test' or 'example'.

not "hi" and f"*test*" // this matches all functions not named 'hi' in file paths containing 'test'
not ("hi" and f"*test*") // this will match all functions that aren't named 'hi' inside file paths containing 'test'
// a function named 'hi' in 'main.btb' would match
// 'get' in 'test.btb' would match
// 'hi' in 'test.btb' would NOT match
```

## Ordered function inserts
When multiple inserts matches one function then all inserts will be inserted into the function. The order of inserts depend on when function inserts are preprocessed which depends on which order files are imported.

If you require a certain order then you can specify a priority on each function insert. The default priorty is zero and inserts that have the same number depends on when inserts are imported and preprocessed.

```c++
#import "Logger"

#function_insert -1 "go"
    log("I am last")
#endinsert
#function_insert 1 "go"
    log("I am first")
#endinsert

fn go() {}

go()
```

## Inserts in conjuction with asserts and stack trace
The standard library has an Assert module which will crash the program if a condition isn't true. The file and line where the assert was triggered will be printed out. In other languages where exception are thrown (Javascript, Python) the stack trace is printed out. Function inserts allow you to achieve a similar feature.

The difference is that the compiler and standard library will not print a stack trace of source locations where functions were called from but instead the location of the function definitions.

By pushing and popping the name of the function we enter in every function we can then print out that list of functions when an assert is triggered. The code below shows how you can do this.

```c++
#import "Logger"

// This import and function insert is all you need for asserts and stack trace.
#import "Asserts"
#function_insert #file and not "main"
    TRACE_STACK_FRAME()
#endinsert

// The rest of the code is an example

fn get() -> i32 {
    PrintStackTrace()
    return 23
}

fn compute(depth: i32) {
    if depth == 0
        return;
    
    fn call(d: void*) {
        de := *cast<i32*>d
        log(" callback: ", de)
    }
    
    RegisterTraceCallback(call, &depth)
    n := get()
    
    compute(depth-1)
    
    log("---------------")
    Assert(false)
}
fn handle_request() {
    compute(3)
}
fn main() {
    handle_request()
}
```

## Future
As mentioned before the location of the function calls is not present in the stack trace. The location of the function definitions are.

To achieve a trace of function calls we would need to run some code before every function call which requires another function insert feature. This also increases program code size. The affect on performance would be similar since in either case we run some code, whether that is before a function call or inside the function shouldn't make much of a difference. (at least from what I know, maybe there is an affect because of instruction caching or something else I have missed)

Another way to achive function call traces is converting the return address to a file and line number. Not sure how that would be done. It's enough to do this when we assert instead of wasting time calculating it when we may not assert.