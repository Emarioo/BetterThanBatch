**TODO:** Be more thorough.

**NOTE:** Module name Lang.btb may be renamed to Types.btb or something else.

# Type information
Type information can be included in your program by importing `Lang.btb`.

You can find examples in `tests/lang` and available functions in `modules/Lang.btb`

## Basic information
There are three simple attributes you can acquire from type information: size, name, and a unique type id. You can get information from the type name or the evaluated type from an expression.
```c++
#import "Logger"
#import "Map"
#import "Array"

callback: fn @oscall (Map<char[],i32>, f32)

// size
log("int: ",sizeof i32)      // type from name
log("map: ",sizeof Map<i32,i32>)
log("callback: ",sizeof callback) // type from expression (just a variable in this case)

// name
log("int: ",nameof i32)
log("map: ",nameof Map<i32,i32>)
log("callback: ",nameof callback)

struct A {
    n: Array<i32>
}
a: A
log("a.n: ",nameof(a.n)) // expression
log("a{}: ",nameof(A{})) // expression

// type id
log("i32: ", typeid i32)
log("map: ", typeid Map<i32,i32>)
log("callback: ", typeid callback)
```

## Advanced information
For being a compiled language, this might seem magical and awesome.
```c++
#import "Lang"
#import "Logger"
#import "String"
#import "Array"
#import "Map"

arr: Array<i32>
arr.append(5)
arr.append(9)

log("Array: ", &arr)

str: StringBuilder
map: Map<i32,i32>
map.set(3,6)
map.set(23,92)
str.append(&map)
log("Map: ", str)
```

The above is possible because the compiler includes information about the types when `Lang` is imported. The information is stored in the global data section of the executable. The data can be accessed through the global variables defined in `Lang`. At the time of writing, they are: `lang_typeInfos`, `lang_members`, and `lang_strings`.

This code will print all the types the compiler needed when compiling the code.
```c++
#import "Lang"
#import "Logger"

for @ptr lang_typeInfos {
    // Some types are NONE, reserved for future types
    if it.type == lang_Primitive.NONE
        continue

    name := Slice<char>{
        &lang_strings.ptr[it.name.beg],
        it.name.end - it.name.beg
    }
    log(name)
    log(" id: ", nr)
    log(" type: ", &it.type)
    log(" size: ", it.size) // some types have zero in size, usually indicates a polymorphic type

    members := Slice<lang_TypeMember>{
        &lang_members.ptr[it.members.beg],
        it.members.end - it.members.beg
    }
    if it.type == lang_Primitive.STRUCT || it.type == lang_Primitive.ENUM {
        log(" members:")
        for @ptr members {
            name := Slice<char>{
                &lang_strings.ptr[it.name.beg],
                it.name.end - it.name.beg
            }
            log("  ",name)
        }
    }
    log()
}
```

**NOTE:** The global variables are implicitly initialized in the main function. If you wish to do so manually (must be done in dynamic libraries) call `init_preload`.

## Experimental/future features
### User defined method from type to string
Since the standard logging uses StringBuilder to print types. All you need to implement user defined type to string methods is a feature that allows you to add methods to structs from outside the struct.

How would such syntax look like?
```c++
struct String { }

// like this?
fn String.append(n: Array<i32>) {}
fn String::append(n: Array<i32>) {}

// or this?
methods String { // based on trait from Rust
    fn append()
}
struct @additional String { 
    // field members would not be allowed, only methods
    fn append()
}
```

Then there is the question of where those methods are visible. If we add a method to StringBuilder then would any module that import StringBuilder have access to the additional methods? Probably not since the task dependency system would need to be modified to account for the additional methods.

To have access to the additional methods, you must import where those were defined. Also, when defining additional methods the struct those new methods belong to must be defined.