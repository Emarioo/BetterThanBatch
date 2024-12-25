**WORK IN PROGRESS**

The compiler provides a set of functions that affect the compilation. Some examples are: adding and changing static/dynamic libraries, changing output directory, compiling for multiple targets at once.

You can find the functions in `modules/Compiler.btb`. They are marked with `@compiler`. These functions have to be called by a run directive. The entry point or an exported function cannot call a `@compiler` function (or a function that eventually calls `@compiler`). That is because you can't change the path of a library, or change the target you are compiling for at runtime, that just doesn't make any sense.

# The best way to showcase this is with examples

You can use `set_library_path` to change the path of a library that was created through the #load directive.
```c++
#load "invalid/path.lib" as MATH
fn @import(MATH) add(x: i32, y: i32) -> i32;

#import "Compiler"

#run {
    u := current_buildunit()
    u.set_library_path("MATH","./math.lib")
}
```

