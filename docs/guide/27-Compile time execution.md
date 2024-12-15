The compiler has a feature called compile time execution where code such as expressions can be evaluated at compile time. Compile time execution for global variables were briefly covered in an earlier chapter. This chapter will go more in-depth.

# Compile time execution
There are two features in the language that use compile time execution. Global variables and the run directive. Global variables are evaluated first, then all run directives in the code.

## Run directive
There are two types of run directives. Global run directive which is placed at the global scope of a file and local run directives which are placed inside functions.

```c++
#import "Logger"

#run log("I am global")

fn main() {
    log("main begin")
    
    #run log("I am local")
    
    log("main end")
}

global y: i32 = temp()
fn temp() -> i32 {
    log("I am global variable")
    return 5
}
```

Running the above code will make you realize something about the order of execution.
1. Global variables are evaluated first.
2. Local run directives in functions comes next.
2. Lastly global run directives at the global scope of the file.

**MORE TO COME**

### Technical details
The compiler keeps track of a list of global run directives. The bytecode for those expressions are generated and executed before it's time to generate machine code.

Functions that contain a run directive are delayed until the bytecode for all other functions are generated. Then the bytecode for the delayed functions are generated. When the run directive is encountered a new bytecode generation context is created where the bytecode for run directive's expression is generated. Then that expression is evaluated. And then inserted into the delayed function where the run directive was.

Generate bytecode for functions without `#run` allows us to avoid a dependency tree because if a run directive is executed and a function call refers to a function that hasn't been generated, then we know that function contains a `#run` directive which could cause a circular dependency. In any case, the function call isn't resolved, the virtual machine returns with an error. We print an error message and the call stack and where execution failed (print the name of the function with the run directive we encountered) and we live happy ever after, no bugs.

The compiler uses multiple threads and so does compile time execution. Therefore run directives can be computed in any order.

Compile time evaluation has one restrictions which is returning pointers. Allocating heap at compile time and returning it is not allowed since that pointer isn't available at runtime. Any pointer that points to the data section is allowed however. The compiler will print an error message when this isn't the case. Returning a string is allowed because it lives in the data section. Returning a function pointer to a function defined in the program is also allowed.

## Corner cases

Run directives inside other run directives are ignored.
```c++
fn main() {
    x := #run 3 * #run 2
}
```

Run directive inside global variables don't do anything. The function is not marked as containing a run directive because it will not be executed by the function.
```c++
fn main() {
    global x: i32 = #run 5
}
```

Circular dependency is detected when executing virtual machine. The bytecode for the function we're trying to call is not generated.
```c++
fn first() {
    #run second()
}
fn second() {
    #run first()
}
first()
```

Function pointer from compile time evaluation.
```c++
#import "Logger"

fn first() {
    log("Okay")
}
f := #run first
f()
```

Structures are recursively evaluated.
```c++
#import "Logger"

struct Block {
    x: i32;
    str: char[] = "unnamed";
}

fn first() -> Block {
    return Block{4}
}

b := #run first()
log(&b)
```

The run directives won't run because the bytecode for function they exist in won't be generated since there is no call to them (from the main function)
```c++
#import "Logger"

fn first() {
    log("first")
    #run second()
}
fn second() {
    log("second")
    #run first()
}

// first() // <-- this is required
```
