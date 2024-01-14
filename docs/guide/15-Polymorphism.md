# Polymorphism
Polymorphism (templates/generics) allows you to reuse code for multiple types. Arrays and maps needs to contain items of some specified type which is where polymorphism comes in.

Polymorphic structure.
```c++
struct Basket<T> {
    content: T;   
}

bask: Basket<i32>;
bask.content = 23;
std_prints(b.content)

bask_bask: Basket<Basket<i32>>;
bask_bask.content.content = 923;
std_prints(bask_bask.content.content)
```
Polymorphic function.
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