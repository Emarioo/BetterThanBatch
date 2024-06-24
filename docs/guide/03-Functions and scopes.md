# Functions

A function is declared with a name, parameters, and return values. Functions can then be called with some arguments.
```c++
#import "Logger"

fn Increment(firstArg: i32, secondArg: f32) -> i32, f32 {
    return firstArg + 5, secondArg + 5.0;   
}

a, b := Increment(92,13);
log(a,b);
```

## main function
Languages like C/C++ have a main function while Python doesn't need one. This language is similar. Code can be executed outside any function at the global level where all statements are implicitly inserted into a main function. But you can also specify a main function like this:
```c++
#import "Logger"

fn main() {
    log("Hello world!")
}

// or this if you want the arguments
fn main(argc: u32, argv: char**) {
    for 0..argc {
        log(argv[nr])
    }
    // Note that argv is read only. However, you can write modify and produce unexpected behaviour.
}
```

TODO: Functions and overloading
TODO: Multiple return values
TODO: Args by value/pointer

## Function pointers
Function pointers are a thing.
```c++
fn sum(x: i32, y: i32) ->  i32 {
    return x + y
}

s := sum // function pointer, inferred type

log(s(1, 3))

s2: fn(i32,i32)->i32 // explicit type
s2 = sum

var: fn @stdcall (void*)->i32 // function pointer with a different calling convention
```
Necessary for callback with external libraries.
```c++
fn map(arr: i32[], callback: fn(i32)) {
    for arr
        callback(it)
}
arr: i32[10]{45,6,2,49,24,2}

fn hi(x: i32) {
    log("Hi: ",x)
}
map(arr, hi)

```

# Scopes
TODO: Explain scopes

## Global variables
Global variables differ from local variables in that they can be accessed at all times in the program. The lifetime of a local variable is limited to a function call while a global variable has a lifetime of the whole program.

Local vs global
```c++
#import "Logger"

global top_var: i32

fn main() {
    fn hi() {
        local_var: i32 = 0
        local_var++
        top_var++
        log("top_var: ",top_var)
        log("local_var: ",local_var)
    }
    for 0..5
        hi()
}
```

Global variables are initialized to zero by default. Expressions assigned to global variables are evaluated at compile time. You can therefore not allocate memory and use it at runtime.

```c++
#import "Memory"
#import "Logger"
#import "File"

// this is really bad
global var: i32* = Allocate(4)

// but you can do fun stuff like this
global size_of_file: i32 = file_size()
fn file_size() -> i32 {
    size: i64
    FileOpen("Makefile", FILE_READ_ONLY, &size)
    return size
}
fn main() {
    log(size_of_file)
    log(var)
    log(*var) // crash
}
```

<!-- Sometimes you may want uninitialized global memory. You rarely do and the compiler will initialize the global data section to zero no matter what before globals are evaluated during compilation. However, thereif you have many global structs you can speed up the compiler by not performing unecessary compile time evaluations (compiler should be smart enough to not do unnecessary computations thouh).
```
global hi: Slice<char> = ---
``` -->

Members of global structs are initialized to zero unless they have default values in which case the default values are evaluated at compile time.
```c++
struct Config {
    ok: i32 = 23 + 23
    nom: i32 = 2 * 3
}
global config: Config // initialized with default values in struct
```