First of, objects on the stack or in the global data is always initialized to zero. Variables will not have uninitialized memory unless you allocate memory from the heap.

# Initialization and construction
Structures are initialized like this:
```c++
struct Box {
    data: i32;
}

box: Box // initialized implicitly

box = Box{} // manually initialize or overwrite box with default values, zero initialized in this case

struct Box99 {
    data: i32 = 99;
}

box: Box
box = Box{} // construct/initialize box with default values, 99 in this case

struct Box99 {
    data:   i32 = 3;
    data_1: i32 = 5;
    data_2: i32 = 40;
}

box: Box
box = Box{12, 1} // construct/initialize box with some specified and some default values

_test 12; box.data   // specified
_test 1;  box.data_1 // specified
_test 40; box.data_2 // default
```

If you require more complex initialization then you would do this:
```C++
#import "Memory" // Allocate

struct Box<T> {
    data: T*;
    fn init() {
        data = Allocate(sizeof T)
    }
}

box: Box
box.init()
```

Based on this you may think that you can rename `init` to whatever you want which is true unless you want other structures to be able to initialize it. An integer type can easily be initialzed to zero `i32{}` while a structure needs to be initialized with `init()`. The language therefore implements an artifical function `construct` that initializes the data based on it's type. See example below.

```c++
#import "Memory" // Allocate

struct Box<T> {
    data: T*;
    fn init() {
        data = Allocate(sizeof T)

        data = T{}  // default initialization

        data.init() // complex initialization

        construct(data) // default or complex initialization based on the type of data
    }
}

box: Box
box.init()


fn construct<T>(data: T*) // <- signature of 'construct'

```

**NOTE**: There are no move or copy operators/constructors.

# De-initialization and destruction
Once again, there are no destructors like C++. Each member of a struct must be destructed individually (for better or worse).

Primitive types do not require destruction, we simply zero the memory (`a = T{}` or `memzero(&a, sizeof a)`). Structures with allocations is more complicated however. The structures in the standard library has a method called `cleanup` which will free allocations and initialize the memory of the struct to a fresh state.

Similar to `construct`, destruction has `destruct` which will call `cleanup` or zero memory depending on the type to destruct.

```c++
#import "Memory"   // Allocate, Free
#import "Lang"     // print members of structures
#import "Logger"   // log

struct Box<T> {
    ptr: T*;
    fn init(v: T) {
        ptr = Allocate(sizeof T);
        construct(ptr) // initialize
    }
    fn cleanup() {
        destruct(ptr)
        Free(ptr)     
        ptr = null
        // box is now as good as new, a fresh state
    }
}

box: Box
log(&box) // default values, fresh state

box.init();
log(&box) // initialized with an allocation

box.cleanup();
log(&box) // default values, fresh state, allocations have been freed

fn destruct<T>(data: T*) // <- signature of 'destruct'
```

The semantics of construction and destruction is very important in data structures where you can add and remove elements. You want an array of arrays to correctly free memory without memory leaks. You also don't want to have different data structures for correctly destructing structures and primitive types, hence `construct` and `destruct`.

# Future features
- Some way to automatically destructing members of a struct. Remembering to destruct new members you add is difficult.
```c++
// something like this? adding this to every new struct you create is
// kind of annoying too no?
for @ptr this.members
    destruct(it)
```


# Implementation details

Some code for construct, destruct can be found in `Generator.cpp`, function `GenContext::generateSpecialFnCall`.
At the time of writing this is true, may change in the future.