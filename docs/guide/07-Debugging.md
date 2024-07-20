**NOTE**: This chapter is incomplete. It barely covers debuggers but does cover debugging by printing data. Perhaps logging and printing should be it's own chapter? Or maybe this chapter (Debugging) should be renamed to Printing/Logging.

# Debugging
Compile with debug information like this (-d): `btb -d main.btb`
This generates DWARF debug information. gdb and vscode supports it but you have to setup launch.json if you are using vscode (look it up online, "vscode debugging gdb")

<!-- TODO: Should we throw in some images showing debugging in vscode, also an example of launch.json? -->

# Debugging by printing
Print debugging can be useful if it's difficult to debug using the other options. Perhaps you have networking with multiple processes. Printing information to stdout is useful.

When debugging code by printing, there are two useful modules. One for logging and one for type information (read about type information in a later chapter).
```c++
#import "Logger"
#import "Lang" // IMPORTANT: Lang might be renamed to Types or something else!

// log is a useful macro for printing all kinds of things
log("A number: ",232 + 1 * 9, " a float: ", 23.52, " a string: ", "hi", " a char: ", 'Z')

// Under the hood, log is evaluated to multiple calls to std_print.
// std_print is an overloaded function for many types (floats, integers, strings, pointers)
std_print("A number")
std_print(232 + 1*9)
std_print("A float:")
/* std_print(... */

#import "Array"

// But with type information (Lang module), you can print the data
// inside structs. This saves a lot of time.
arr: Array<i32>
log(&arr)
arr.add(6)
arr.add(9)
log(&arr)

// In other languages (C/C++) you would need to write a function for
// each struct you want to print or type out every member that you
// want to print.
```