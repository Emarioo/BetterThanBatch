Exceptions was difficult to implement because there isn't a lot of documentation on how to do it. Now when I have done it is actually not difficult at all. What is difficult is correctly handling the semantics of the language, destroying and cleaning objects. Luckily, this language is not responsible for that so even that part is easy.

There are different kinds of exceptions. This language only handles software/hardware exceptions from the operating system. If the language doesn't support it then it is pretty much impossible to handle them as a user. Other types of exceptions on the language level isn't as difficult to implement but they are also not as useful since they just pass around error values. Catching a null pointer dereference is much more sophisticated.

# Windows
**NOTE:** This is my understanding of how exceptions work on Windows, I may have misunderstood some parts. You can find resources further down where I learnt it all.

Handling hardware/software exceptions on Windows is called Structured Exception Handling (SEH). This document assumes 64-bit system. Windows on 32-bit handle exceptions differently (from my understanding).

<!-- Exceptions implemented at the language level where users can throw their own exceptions is not as useful or important. This is just personal opinion, don't write about this. -->

There are a couple of parts you need to do when implementing exceptions.
- Defining the Windows structures.
- Writing data to .xdata and .pdata sections in the object file.
- Exception handler preferably written in your language.
- And the frontend which parses try-catch and puts it into the AST.

When an exception occurs, let's say division by zero, Windows will search for an appropriate function in the .pdata section. The whole section is a table of function entries (this is the definition of the entry [RUNTIME_FUNCTION](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170#struct-runtime_function)). The entries describe the start and end code address of functions (relative to image base). An exception has information such as type but also the address for which instruction the exception occured at. Windows will pick the entry in the table where the exception address fits inside the function range. The entry also contains an address (relative to image base) which points to a structure in the .xdata section.

The .xdata section stores information about each function such as the address of the exception handler and specific language information like where the try-blocks can be located in the function. The .xdata section is NOT a simple list like the .pdata because each structure could be sized differently. The structure is defined here [UWNIND_INFO](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170#struct-unwind_info).

Now, the function entry contained an address into the .xdata section and now it knows which UNWIND_INFO to use. What Windows does is check if the UNWIND_INFO has an exception handler (EHANDLER) and if so, call it with useful information about the exception and execution context. Definition of function is here [EXCEPTION_ROUTINE](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170#language-specific-handler).

The exception handling is now in your hands. if you want to print something and terminate the process, you can do that. If you want to read some files, you can do that. If you want to start executing at the appropriate catch-block then you can do that too.

I will not go through writing an exception handler in detail but I will link to an example here [exception_handler](/modules/Exception.btb#L26). The important parts to consider is:
- [RtlLookupFunctionEntry](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtllookupfunctionentry) which will find the appropriate function entry based on an instruction pointer.
- [RtlRestoreContext](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtlrestorecontext) which can be used to start execution at a catch-block. This may not be relevant to you who are reading but *RtlRestoreContext* uses *stdcall* calling convention and not *cdecl*. You can disassemble a C file that calls the function and see for yourself.
- ExceptionContinueSearch which is returned when a try-catch doesn't match.
- First argument of exception handler [EXCEPTION_RECORD](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-exception_record) which contains information about the exception, mainly exception code and which instruction exception occured at.
- Third argument [CONTEXT](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-context) which contains the stack pointer and base pointer when the exception occured. This can be used to unwind the stack by accessing the previous base pointer and return address.
- The fourth argument [DISPATCHER_CONTEXT (scroll down a bit)](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-context) which contains information about the function entry and handler data. The execption handler doesn't need to call *RtlLookupFunctionEntry* the first time because DISPATCHER_CONTEXT contains the handler data for the try-blocks.

When creating the .xdata section you can either follow the C standard and use it's language specific handler data structures OR you can create your own. If you want interoperability with C libraries then that is probably a good idea, otherwise you can create your own structure for the try-blocks and even specify zero unwind codes in the UNWIND_INFO structure. However, if your exception handler relies on Windows functions for unwinding the call frames then you do need to specify the unwind codes. Otherwise, you can unwind the stack yourself.

You can find code for writing the section data in [ObjectFile::WriteFile - ObjectFile.cpp](/src/BetBat/ObjectFile.cpp). The code is spread out a bit, you can search for *.xdata* *UNWIND_INFO* and *exception_handler*.

**NOTE**: I know this isn't all information you will need to implement exception handling but it should suffice if you also take a look at the resources further down.

# Linux
**TODO:** Implement exceptions when compiling for Linux.



# Other stuff

Windows with Microsoft C/C++ compiler there is "Structured Exception Handling" with macros GetExceptionCode(), AbnormalTermination, __try, __catch, __finally. 

On Linux there is signals, sigaction(), SIGSEGV, SIGKILL, SIGILL. I think Linux might have some exception handling other than sigaction. There is something about exceptions in DWARF.

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
