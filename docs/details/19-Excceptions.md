Exceptions was difficult to implement because there isn't a lot of documentation on how to do it. Now when I have done it is actually not difficult at all. What is difficult is correctly handling the semantics of the language, destroying objects for example. Luckily, this language is not responsible for that (because I said so).

There are different kinds of exceptions. This language only handles software/hardware exceptions from the operating system. If the language doesn't support it then it is pretty much impossible to handle them as a user. Other types of exceptions on the language level isn't as difficult to implement but their also not as useful since they just pass around error values. Catching an null pointer dereference is much more sophisticated.

# Windows
Handling hardware/software exceptions on Windows is called Structured Exception Handling (SEH).

<!-- Exceptions implemented at the language level where users can throw their own exceptions is not as useful or important. This is just personal opinion, don't write about this. -->

There are a couple of parts you need to do when implementing exceptions.
- Defining the Windows structures.
- Writing data to .xdata and .pdata sections in the object file.
- Exception handler preferably written in your language.
- And the frontend which parses try-catch and puts it into the AST.

**TODO:** Write more!

# Linux
**TODO:** Implement exceptions when compiling for Linux.