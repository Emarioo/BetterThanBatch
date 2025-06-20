# Structs
Structs are types that can contain multiple types. Each member in a struct has a type and a name. Members also have a relative offset into the struct but that is handled by the compiler.

This is how you declare and use a struct.
```c++
#import "Logger"

struct Apples {
    count: i32;
    size: f32;
}

your_apples = Apples{ 5, 92.1 };

// The type of the struct initializer can also be inferred from
// assignments, declarations, and function paramaters.
your_apples = { 5, 92.1 };

log(your_apples.count, your_apples.size);
```

The type of members cannot be that of their own struct as that would create a paradox. You can however use a pointer of it's own struct since pointers have a fixed size.
```c++
struct Paradox {
    n: i32;
    hm: Paradox; // what is the size of Paradox? infinite?
}
struct NotAParadox {
    n: i32;
    ptr: Paradox*; // this is fine   
}
```

## Special annotations

### @no_padding
This will disable alignment calculations and padding on fields. Useful if you are working with file formats that
are based on C structs that have `#pragma pack(push,1)`.
```c++
struct @no_padding Data {
    a: i16
    // 2 bytes would normally be reserved here to align
    // the following 32-bit integer. The no_padding stops that.
    b: i32
} // sizeof Data == 6 (not 8 as we would expect with alignment and padding)
```

### @no_pointers
This will cause compiler error if struct contains a pointer (we check fields of fields of structs recursively). It is useful when you want to ensure a struct is serializable.
Otherwise you may forget that you are serializing this struct in your code and add a pointer field to support a new feature. This prevents that.
```c++
struct @no_pointers Data {
    fine: i32;
    data_ptr: i32*; // ERROR
}
```

# Incomplete features

## Unions

TO EXPLAIN:
- First member of union can have a default value. The rest cannot not even if they are bigger (will be zero initialized though). Initializer affects the first member.
- The active/first member can be changed using an annotation. @active, @first, @front
- Nested unions are flattened
- 

```
struct Bee {
    size: i32;
    union {
        cool: i32;
        taste: f32;   
    }   
}
```