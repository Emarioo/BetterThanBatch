The language does not support user-made exceptions where a keyword like 'throw' is used to manually cause exceptions. Software and hardware exceptions from the operating system are supported.

# Exceptions
Note that even if you can catch exceptions and continue executing it may not be a good idea. An exception may occur in the middle of the try-block causing memory leaks and files to remain open. You need to take special care when using exceptions to avoid such scenarios. The reason Exceptions are supported is to give the user a neat reason as to why the program crashed instead of a sudden termination with no information at all.

You can catch exceptions with the following syntax:
```c++
#import "Exception" // you must import this file to have access to the enums
try {
    prints("try")
    *cast<char*>cast<i64>0 = 1 // dereference null pointer
    prints("no crash")
} catch EXCEPTION_ANY {
    prints("bad memory access!")
}
```

Try catch can be nested.
```c++
#import "Exception"
try {
    try {
        a := 1 / 0 // zero division
    } catch EXCEPTION_ANY {
        prints("zero division")
    }
    *cast<char*>cast<i64>0 = 0 // dereference null pointer
} catch EXCEPTION_ANY {
    prints("bad memory access!")
}
```

Exceptions that occur in function calls will also be caught.
```c++
#import "Exception"
fn crash() {
    a := 1 / 0 // zero division
}
try {
    crash()
    prints("no crash")
} catch EXCEPTION_ANY {
    prints("crash!")
}
```

Sometimes you may want to catch specific exceptions, that is what the expression next to the catch keyword is for. You can find all the exceptions types in the enum ExceptionType in the file [Exception.btb](/modules/Exception.btb). Also, each try can have multiple catch.

These are some of the exceptions:
- EXCEPTION_MEMORY_VIOLATION (memory access violation, segmentation fault)
- EXCEPTION_INT_ZERO_DIVISION
- EXCEPTION_INVALID_INSTRUCTION (illegal instruction, privileged instruction)
- EXCEPTION_ANY (catches all types of exceptions)
- EXCEPTION_OTHER (catches exceptions not specified in ExpressionType)

```c++
#import "Exception"
try {
    a := 1 / 0;
    *cast<i32*>cast<i64>0 = 9
} catch EXCEPTION_MEMORY_VIOLATION {
    prints("caught access\n");
} catch EXCEPTION_ZERO_DIVISION {
    prints("caught div\n");
}
// If multiple catch-statements accept the same type then the first one will be used.
try {
    *cast<i32*>cast<i64>0 = 9
} catch EXCEPTION_MEMORY_VIOLATION {
    prints("caught access 1\n");
} catch EXCEPTION_MEMORY_VIOLATION {
    prints("caught access 2\n");
}
```

If you want more information about the exception then you can specify a variable similar to for loops which will store a pointer to a structure with exception information. The structure is called **ExceptionInfo** and is defined in *Exception.btb*. At the moment, the structure only contains the exception code and instruction pointer where exception occured. (you can let me know through a github issue what other information you want)
```c++
#import "Exception"
#import "Logger"
try {
    n := 1/0
} catch ex: EXCEPTION_ANY {
    log(ex)
}
```

**NOTE:** Exceptions are implement using a *language specific exception handler*. It's purpose is finding the try-blocks and setting up the hardware registers. It is defined in Exception.btb but you can implement your own by disabling the default one by defining this macro: **DISABLE_DEFAULT_EXCEPTION_HANDLER**, then define your own **exception_handler** with the same signature as the default one. This is not recommended but possible. Maybe you want a global exception handler and more information about the exception.

**NOTE:** Expression.btb does not specify all types of expressions. Just the most common ones. If you are on Windows you can specify Windows exceptions in the catch expression such as WIN_EXCEPTION_ARRAY_BOUNDS_EXCEEDED.

**NOTE:** Some languages have "finally" which executes at the end of a try-block even if a "higher" try-block caught the exception. The language does not currently support it.
