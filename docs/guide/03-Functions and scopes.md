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
Functions can have default arguments and named arguments.
```c++
#import "Logger"

fn print_msg(msg: char[], sender: char[] = {}) {
    if sender.ptr
        log(msg)
    else
        log(sender,": ", msg)
}

print_msg("Someone says hi")
print_msg("Hello someone!", "Cat")
print_msg("Hello cat and someone.", sender = "Frog")
```
**NOTE:** You cannot name an argument that doesn't have a default. On the implementation side of the compiler we would need to rewrite the overload matching system.
```c++
fn hi(a: i32, b: i32) {}

hi(5, b = 23) // named argument not allowed
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

There is also a builtin function `global_slice()` which returns a slice to the global data. If you need to copy the data for whatever reason than you can.
```c++
global_data := global_slice()
log(global_data)

// The function is defined in a preload script like this:
fn @compiler global_slice() -> Slice<char>

// This code is pretty fun
#import "OS"
#import "Logger"

x := global_slice()
for x {
    std_print(it)
    ProcessSleep(0.001)
}
```

## Advanced (stable globals for hotreloading)
**WARNING:** This feature is rather buggy and can crash a lot if you don't know what you are doing.

Stable global data is mainly used with hotreloading to implement global data that persists between dll reloads. This is done by allocating heap memory for global data. Code in dll that accesses global data will access the heap memory instead of the global data. Global data would be reset upon dll reload while the heap memory stays the same.

However, you should probably avoid using globals but when you are prototyping and just need some globals to store some arrays for testing then this feature can be quite useful.

These are the steps to use stable global data:
- Compile dll with stable globals flag: `btb --stable-globals main.btb -o main.dll`
- At start of application, allocate heap memory the same size as global_size().
- Upon fist load of dll, copy global data (the initial state of global data) to the allocated memory.
- Then every time the dll is reloaded (including first time), execute this code inside the dll code: `stable_global_memory = ptr_to_allocated_memory`. Pointer to allocated memory needs to be passed from the executable that loaded the dll. You could export a function in the dll that sets the stable_global_memory. Or you have this line inside the hotreloading_event function which you create in the dll when using hotreloading based on `Hotreloading.btb`. `AppInstance` could store pointer to allocated memory.

Note that if you recompile the dll and it has added globals, removed globals, or type changes then the application will probably crash because the heap memory is based on the previous dlls global data layout. This is similar to if you change structs since hotreloading only works with code changes, not data layout, at least if the data persists from dll reload.

Note that stable global data won't work if the dll imports static libraries. Static libraries do not support stable global data, especially if they were compiled from C.

**TODO:** Provide small example on how to use stable global data.