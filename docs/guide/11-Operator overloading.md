# Operator overloading
Operations such as addition, equals, and bit shift can be overloaded with an operator function. Any occurances of the operation where the types match the overload will call the function instead of performing the operation.

The most common use case of operator overloading is vector math.

```c++
struct Vector {
    x: f32;
    y: f32;
}
operator +(a: Vector, b: Vector) -> Vector {
    return Vector{
        a.x + b.x,
        a.y + b.y
    }
}
operator *(a: Vector, factor: f32) -> Vector {
    return Vector{
        a.x * factor,
        a.y * factor
    }
}

pos := Vector{1,1}
vel := Vector{40,10}
result := pos + vel * 0.016

```

# Type conversion overloading
Besides overloading operators, you can also overload type conversions with your own functions. An example is shown below.
```c++
#import "Logger"

enum ID {
    INVALID,
}
operator cast(v: ID) -> i32 {
    new_value: i32 = cast_unsafe<i32> v-1
    log("cast ", cast_unsafe<i32>v, " -> ", new_value)
    return new_value
}
operator cast(v: i32) -> ID {
    new_value: ID = cast_unsafe<ID> (v+1)
    log("cast ", v, " -> ", cast_unsafe<i32>new_value)
    return new_value
}

n: ID = 13 // *Implicit* cast and call to overload

log(cast<i32>n) // *Explicit* cast and call to overload

// strict cast where we print the binary value, no calling of overloads
log(cast_unsafe<i32> n)

fn hey(n: i32) { }
hey(ID.INVALID) // implicit cast using overloaded cast
```
When using type conversions you may easily run into infinite recursion since normal casting isn't available. `cast_unsafe` and `cast_normal` can be used instead.
```c++
#import "Logger"

enum ID {
    INVALID,
}
operator cast(v: i32) -> ID {
    return v-1 // implicit cast which would call this overload in an infinite loop

    return cast<ID> (v-1) // same here with an explicit cast

    return cast_normal<ID> (v-1) // cast_normal will use the normal cast that is available in the language, float to integer or enum to integer for example.
    
    return cast_unsafe<ID> (v-1) // cast_unsafe will treat the bits as a different type, this only works with primitive types (those that fit in machine registers, 64-bit values on a 64-bit machine)
    
}
```

**NOTE**: Even though you can overload type conversions, it's not always a good idea. Explicitly calling a function or macro that converts a type conveys an intention in the code. A type conversion overload is not visible in the code and programmers may be confused when a number is converted to a string out of nowhere. Type conversions are however great for converting lists into iterators or a sublist into the list.