The compiler has a feature called **Compile-time Execution** where a chosen piece of code can be executed during the compilation. The two features that run code at compile-time are global variables and the `#run` directive. Global variables were covered in an earlier chapter, this chapter will cover the `#run` directive and its synergy with global variables.

## The **#run** directive
There are two types of run directives referred to as auxiliary and inline run directive. The Auxiliary run directive is a statement of code that will be executed after all global variables have
been evaluated. It can be placed in any scope such as the global level, in a function, or in a for loop. It's placement does not affect the surrounding code. The only reason you want to place a run directive inside a function is if the executed code is relevant to that function or if it modifies a global variable only accessible from that scope.

This code example shows the execution order of the auxiliary run directive and the global variable. Global variables are evaluated first, run directives are evaluated second, local run directives run last.
```c++
#import "Logger"

fn main() {
    log("main begin")
    
    #run log("I am auxiliary but local")
    
    log("main end")
}

#run log("I am auxiliary and global")

global y: i32 = temp()
fn temp() -> i32 {
    log("I am global variable")
    return 5
}
/*
OUTPUT at compile time:
  I am global variable
  I am auxiliary and global
  I am ALSO auxiliary and global
 
OUTPUT at runtime:
  main begin
  main end
*/
```

The inline run directive is shown below. Even though we call `say_hi` twice the function `compute` is only called once because it's evaluated at compile-time and replaced by a literal integer.
```c++
#import "Logger"

say_hi()
say_hi()

fn say_hi() {
    result := #run compute(5)
    log("hi = ", result)
}
fn compute(value: i32) -> i32 {
    log("compute")
    return value * 2
}
```

Structs and pointers to the global data section (such as strings) can also be returned from the run directive shown below. 
```c++
#import "Logger"

struct Stuff {
    str: char[];
    x: i32;
}
x := fun()
y := fun()
log(&x," ", &y)

fn fun() -> Stuff {
    res := #run comp()
    return res
}
fn comp() -> Stuff {
    log("comp")
    return { "hello", 23 }
}
```


Even function pointers are allowed. If they are defined in the program, operating system functions are not allowed.
```c++
#import "Logger"

struct Funcs {
    start: fn();
    finish: fn();
}

for 0..2 {
    x := #run get_funcs()
    
    x.start()
    x.finish()
}

fn get_funcs() -> Funcs {
    log("Get funcs")
    return { Start, Finish }
}

fn Start() {
    log("START")
}
fn Finish() {
    log("FINISH")
}
```

However, you cannot allocate heap memory and return such pointers to it since that memory only exists at compile time. Returning StringBuilder or Array data structures is therefore not allowed (since they allocate heap memory). During compile-time you can allocate all the heap you want and call all the functions you want. Just be careful when returning structs, especially those from the standard library because they may contain pointers.
```c++
#import "Memory"

x := #run Allocate(4) // cannot return pointer to heap

#run Allocate(4) // does not inline a literal so this is fine albeit a memory leak
```

## Corner cases

Run directives inside other run directives don't do anything since you are already running at compile-time.
```c++
fn main() {
    x := #run 3 * #run 2
}
```

Run directive inside global variables don't do anything, once again, you are already running at compile-time. The function is not marked as containing a run directive because it will not be executed by the function.
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

This is somewhat strange behaviour but the compiler will not check the bodies of functions that aren't called by any one (thus improving performance when compiling). This means that the run directives aren't called either. (this may be changed in the future but be wary of this for now)
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


## Technical details (may be useful if you run in to issues)
Functions that contain an inline run directive are delayed until the bytecode for all other functions are generated. Then the bytecode for the delayed functions are generated. When the run directive is encountered a new bytecode generation context is created where the bytecode for run directive's expression is generated. Then that expression is evaluated. And then inserted into the delayed function where the run directive was.

Functions with inline run directive are marked and delayed because it let's us avoid a dependency graph which simplifies the compiler. This imposes some restrictions. Marked functions with run directive cannot call other marked functions because their bytecode hasn't been generated yet. This is detected in the VM when relocations to functions are resolved.

A run directive may still be able to call a marked function if the bytecode generation for it was tasked before it's own function's bytecode generation task. However, you must not rely on this, especially in the future when the compiler utilizes multi-threading.

The run directives currently execute in a deterministic but not so obvious order. A recommendation is to not write run directives that requires to be executed in a certain order. Even though the compiler is will deterministically execute the same run directives in the same order every time you compile the same program, this will not be the case in the future when the compiler utilizes multi-threading.

An important restriction for Compile time evaluation has one restrictions which is returning pointers. Allocating heap at compile time and returning it is not allowed since that pointer isn't available at runtime. Any pointer that points to the data section is allowed however. The compiler will print an error message when this isn't the case. Returning a string is allowed because it lives in the data section. Returning a function pointer to a function defined in the program is also allowed.

**NOTE:** When compiler is multi-threaded, you can use mutexes to prevent synchronization problems if multiple run directives accesses the same globals. Fortunately, all calls to the compiler session functions are thread-safe so you don't have to worry about that (the functions may not be foolproof from logical errors though).

# Future improvements
- Temporary global data for compile-time execution. For storing temporary mutexes.
