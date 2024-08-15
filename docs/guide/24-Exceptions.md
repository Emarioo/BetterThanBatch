**WARNING:** Work in progress, features explained here are not implemented.

# Exceptions
Exceptions in the language refers to operating system or hardware exceptions. The language does not support user-made exceptions.

These are some of the exceptions:
- EXCEPTION_MEMORY (memory access violation, segmentation fault)
- EXCEPTION_ZERO_DIVISION_INT
- EXCEPTION_ZERO_DIVISION_FLOAT
- EXCEPTION_ILLEGAL (illegal instruction)

Catching exceptions is done like this:
```c++
#import "Logger"

try {
    log("Try")
    *cast<char*>cast<i64>0 = 0 // dereference null pointer
    log("All is good")
} catch EXCEPTION_MEMORY {
    log("Bad memory access!")
}
```

Try catch can be nested.
```c++
#import "Logger"

try {
    try {
        a := 1 / 0 // zero division
    } catch EXCEPTION_ZERO_DIVISION_INT {
        log("Zero division")
    }
    *cast<char*>cast<i64>0 = 0 // dereference null pointer
} catch EXCEPTION_MEMORY {
    log("Bad memory access!")
}
```

**NOTE:** Some languages have "finally" which executes

## Implementation details

Note that under the hood, operating systems and hardware handle exceptions differently. On Windows with Microsoft C/C++ compiler there is "Structured Exception Handling" with macros GetExceptionCode(), AbnormalTermination, __try, __catch, __finally. On Linux there is signals, sigaction(), SIGSEGV, SIGKILL, SIGILL. The language tries to be platform independent and you may therefore lose some functionality.

**TODO:** Implement the feature below.
That is why you can also use platform specific exceptions and handling. sigaction() is just a function but with Windows you have the GetExceptionCode() and __try macros you need for specific handling on Windows.


# Resources
## Windows
https://amitkparida.medium.com/structured-exception-handling-in-visual-c-618b9e792faa
https://learn.microsoft.com/en-us/cpp/cpp/try-except-statement?view=msvc-170
https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170
https://llvm.org/docs/ExceptionHandling.html

## Linux (DWARF/GCC)
https://gcc.gnu.org/legacy-ml/gcc/2002-07/msg00391.html
