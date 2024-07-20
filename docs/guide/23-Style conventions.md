You can skip this chapter without missing much.

# Coding style in this language
The language and compiler do not impose a coding style. You are free to do as you wish. There is however a standard coding style that should preferably be used so that programmers can read each others' code with ease. The standard library is consistent with the coding style defined in this document.

**NOTE:** The style is work in progress. To be honest, we might just skip this entirely. It's just a few things that should be standardized.

**TODO:** Give numbers to the areas of coding style. Example: *1 Project structure*, *2 Naming convention* *2.1 Function names*


## Terminology
- **Pascal case**: ILikeChicken

- **Snake case**: i_like_chicken

- **Camel case**: iLikeChicken

## Naming convention
- **Variables/Parameters**: All variables/parameters should be **Camel Case** or **Snake Case**. Global variables should be prefixed with `global_` or `g_` if it is not obvious that the global's name is a global and not a local variable. Variables/arguments that represent an amount of something should be named `count`, `length`, or `len` like `entity_count` and **NOT** `entity_size`. `size` represents an amount of bytes like `array_size` is the allocated bytes for an array while `array_count` represents the amount of elements in the array.

- **Constants**: Actual constants or macro constants should be all upper case with underscores.

- **Functions**: Normal/global functions (non-methods) should be **Pascal case**. Methods should be **Camel case** or **Snake Case**. The words in the name should consist of a verb or at least indicate some kind of action.

- **Structures**: **Pascal case**.

- **Namespaces**: **Camel case** or **Snake case**. The name should indicate some kind of module or namespace. For example, a namespace named `chicken` or `manager` would be assumed to a structure. `math`, `utility`, or `animals` is more reasonable.

- Long descriptive names is a good thing.

- **Macros**: Macros should preferably be all upper case unless you need a specific name that resembles a function.

## Styles of little importance
- Nesting of scopes
- Lines of code per function

# Design conventions
When the standard library (`/modules`) creates an abstraction from the operating system it follows two designs: A handle + global functions, an global create/init function + a struct with methods.

Early modules of the library used a handle which was created and passed to functions but there is a flaw with this. The handle is passed as a value to functions and must therefore be zeroed by the user after it is destroyed. With a struct and methods, the methods can modify the struct and properly zero it making the struct as good as new.

```c++
enum Handle {} // We use enum instead of a macro for i32 because we
               // get proper type checking. Otherwise we could pass
               // any integer to the functions.

fn open(path: char[]) -> Handle { /* ... */ }
fn write(h: Handle, buf: void*, len: i32) { /* ... */ }
fn close(h: Handle) { /* ... */ }

h := open("hi") // Create handle no problem
close(h)
h = cast<Handle>0
// Above however, we pass handle as a value to close
// which means that we manually have to zero the handle.
// Forgetting to zero can cause bugs and it's very tedious
// to zero it if you close it in many places.

// Here is the better design with structs and a global function
fn open(path: char[]) -> File { /* ... */ }
struct File {
    handle: void*;
    
    fn valid() -> bool { return handle }

    fn write(buf: void*, len: i32) { /* ... */ }
    fn close() {
        CloseHandle(handle) // Win32 API function
        handle = null // We properly reset the handle
    }
}

// Now we have no problem
f := open("hi")
f.write("tree",4)
f.close()
// The handle is zero, we can't forget to zero it
```

With that said (or written), you may think that handles or integers is a better abstraction because they are trivial to pass around. The extra data belonging to the handle is abstracted. A struct with methods on the other hand isn't as trivial to copy around, you may want to store path and file size as well as the handle. However, while that's true, no one forces you to store the extra data in the struct the user has access to. That struct can just contain a handle while the methods abstract where the extra data is stored just as we did before. In the compiler, a struct with a single integer is as easily moved around as any other integer (should be unless there is a bug).

# TEMPORARY
Text after this point are old thoughts. May be brought back, or maybe not.

The coding style has two levels of categorization. The first level describes the importance where **High** should rarely (preferably never) be broken while **"Who cares"** can be fully ignored with few consequences.

## High importance
**High** importance is identified as: "If you break this coding style, people reading your code WILL be confused and annoyed about your code."

## Low importance

## "Who cares" importance
Importance identified as: "It would be nice if people followed these conventions but no one really cares because at the end of the day, it's just a personal preference."
