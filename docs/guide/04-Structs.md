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


## Arrays in structs
Structs can have arrays which are declared similarily to arrays on the stack. The size of the struct is exactly that which you specified. With arrays on the stack, both a slice (16 bytes) and the elements are put on the stack. The slice being additional memory. This is not the case in the struct.

```c++
#import "Logger"

struct Name {
    str: char[20]
}

name: Name // zero initialized

s := "okay"
memcpy(name.str, s.ptr, s.len + 1)

log(name.str)

for 0..name.str.len-1 {
    name.str[nr] = 'a' + nr
}

log(name.str)
```

## Special annotations

```c++
// @no_padding will disable alignment calculations
struct @no_padding Data {
    a: i16
    // 2 bytes would normally be reserved here to align
    // the following 32-bit integer. The no_padding stops that.
    b: i32
} // sizeof Data == 6 (not 8 as we would expect with alignment and padding)
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