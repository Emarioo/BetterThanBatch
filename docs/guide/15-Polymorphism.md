Polymorphism (generics, templates) is a feature where code can be reused for multiple types.

# The basics
The most common use case for polymorphism is dynamic arrays and other structures that contain elements.

Here is a basic container. You can define a struct with polymorphic arguments within `<>` and fields of the struct that refer to the polymorphic arguments will use the chosen type at instantiation.
```c++
// general definition of struct
struct Basket<T> {
    content: T;
}
// instantiation the struct and specializing the types.
bask: Basket<i32>;
bask.content = 23;
std_prints(b.content)

bask_bask: Basket<Basket<i32>>;
bask_bask.content.content = 923;
std_prints(bask_bask.content.content)
```

Besides polymorphic structures you can have functions.
```c++
fn size<T>() {
    std_prints("Size of ",nameof T," is ",sizeof T,"\n")
}

struct Fruit<T>{
    size: f32;   
    content: T;
}

size<i32>()
size<i64>()
size<Fruit<i32>>()
size<Fruit<Fruit<i32>>>()
```

When calling a polymorphic function, it is possible in non-ambiguous cases to leave out the polymorphic types. A case where this isn't possible is if the function doesn't have any arguments, the compiler uses the function arguments to infer polymorphic types to use.
```c++
fn hi<T>() -> T {
    return {5}
}

hi<i32>() // have to be explicit, there are no arguments

fn hello<T>(x: T) -> T {
    return 5 + x
}

hello(5) // T will be determined to be i32 because of argument.
```

Functions within structures can also be polymorphic and they will inherit the polymorphic arguments from the struct.
```c++
struct Number<T> {
    x: T;
    fn add(y: T) -> T {
        new_num: T = x + y
        x = new_num
        return new_num
    }
    fn fill_add_10<StructToFill>(to_fill: StructToFill*) {
        for 0..to_fill.size() {
            num: T = x + 10
            to_fill.set(nr, num)
        }
    }
}

#import "Array"
num: Number<i32>
arr: Array<f32>

num.add(8)

arr.resize(7)
num.fill_add_10(&arr)
```

# Future features
- A way to restrict which polymorphic types are allowed. Only numbers, or only structs for example.
- A way to execute code conditionally based on a polymorphic type. The code would be chosen at compile time to not affect performance.
