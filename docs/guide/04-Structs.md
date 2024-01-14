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

## Unions (not implemented)

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