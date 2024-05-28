# Functions and global variables

A function is declared with a name, parameters, and return values. Functions can then be called with some arguments.
```c++
#import "Logger"

fn Increment(firstArg: i32, secondArg: f32) -> i32, f32 {
    return firstArg + 5, secondArg + 5.0;   
}

a, b := Increment(92,13);
log(a,b);
```

# Main function
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

# Function pointers
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