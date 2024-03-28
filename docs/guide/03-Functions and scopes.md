# Functions and global variables

A function is declared with a name, parameters, and return values. Functions can then be called with some arguments.
Functions can then be 
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


TODO: Global variables