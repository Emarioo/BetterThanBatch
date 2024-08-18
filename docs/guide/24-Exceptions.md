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
That is why you can also use platform specific exceptions and handling. sigaction() is just a function but with Windows you have the GetExceptionCode() and __try macros you need for specific handling on Windows.s


# Resources
## Windows
https://amitkparida.medium.com/structured-exception-handling-in-visual-c-618b9e792faa
https://learn.microsoft.com/en-us/cpp/cpp/try-except-statement?view=msvc-170
https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170
https://llvm.org/docs/ExceptionHandling.html
http://www.godevtool.com/ExceptFrame.htm
https://limbioliong.wordpress.com/2022/01/09/understanding-windows-structured-exception-handling-part-1/
https://www-user.tu-chemnitz.de/~heha/hsn/chm/Win32SEH.chm/
http://blog.davidegrayson.com/2016/02/windows-32-bit-structured-exception.html
https://www.osronline.com/article.cfm%5earticle=469.htm
https://blog.talosintelligence.com/exceptional-behavior-windows-81-x64-seh/
http://www.nynaeve.net/?p=113

This link explained that RtlRestoreContext should be used in language specific exception handler: https://billdemirkapi.me/abusing-exceptions-for-code-execution-part-2/. Other websites may also have mentioned it but I didn't realize that was the one I should use. I think that's the right one, it works at least.

C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.36.32532\crt\src

_c_specific_handler in https://github.com/killvxk/DisPg/blob/7aaf8861fb6b28987e968c0829a3df00f5723e13/ExceptAMD64.c#L541

**NOTE:** This has been quite the rabbit hole. I wish people (ehem, Microsoft) would document things better.

## Linux (DWARF/GCC)
https://gcc.gnu.org/legacy-ml/gcc/2002-07/msg00391.html
