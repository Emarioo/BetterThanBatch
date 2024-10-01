Here are the essential features of any language: conditional and iterative execution.

# If
If-statement. What more is there to say.

```c++
if true {
    // code
} else if 63 == 9 * 7 {
    // code
} else {
    // code
}
```

# Loops
The language supports two types of loops: for-loop and while-loop.

## While-loop
The *while* loop is the most basic form of iteration. It keeps executing a block of code as long as the condition is true.

```c++
i := 0
while i < 5 {
    log(i);
    i++;
}
```


## For-loop
The *for* loop is used to iterate through an array or range of values, or more recently user-defined iterators (covered further down). The for-loop has an expression which should result in a **Slice** or **Range** type.

**NOTE:** The language does *NOT* support C-like for loops `for(int i=0;i<5;i++)`. Use while-loops for more control. For loops are meant to iterator over a list or range of things.

```c++
#import "Logger"

// TODO: Explain it, nr
// TOOD: Explain Range
// TODO: Explain that it, nr uses the same memory with ranges but not with slices

// Most basic form of the for-loop
for 0..5 {
    log(nr);
}
 
// curly braces are optional   
for 0..5
    log(nr);

// For-loops has the default name 'nr' for the index-variable.
// You can change it like this. (nr was chosen because developers rarely use nr for variable names)
for i: 3..5 {
    log(i);
}

```
This is how you iterate through a *Slice*.
```c++
#import "Logger"

array: i32[4] { 5, 9, 2, 4 };

// Iterating through slices gives you the two variables 'it' and 'nr'.
for array {
    log("array[",nr,"] = ",it);
}
log("----")
// You can rename them like this.
for v,i : array {
    log(v, " at ", i);
}
```
You can iterate through slices or ranges in reverse using the `@reverse` or `@rev` annotation.

**NOTE**: Annotations are described in detail in another chapter. They are basically hints and extra options for the compiler.

```c++
#import "Logger"

// loop in reverse printing 4, 3, 2, 1, 0
for @rev 0..5
    log(nr)

log("----")
// loop in reverse printing 4, 3, 2, 1, 0
array: i32[10] { 2, 4, 8, 100};

for @rev array
    log(it," at ", nr)

```
For-loops also have `@pointer` or `@ptr` which causes the 'it' variable to be a pointer to the real value in the slice. The details of this is that the full value in the slice is copied to the local variable 'it' which is fine for small types like integers but really bad for large structures.

```c++
#import "Logger"

array: i32[4]{4,7,9}

// Not very useful for integers
for @ptr array {
    log(*it, ", relative address: ", cast<i64>(it - array.ptr))
}

log("----")

range_array: Range[4] { {0,3}, {10,13}, {100,130} }

// Useful for structures, better performance because of no unnecessary copies.
for @ptr range_array {
    log(it, ", relative address: ", cast<i64>(it - range_array.ptr))
    // Note that log() is very special and can print structures. 'it' is still a pointer.
}
```

## Break and continue
All loops support *break* and *continue*. *break* causes the execution to jump out of the loop. *continue* jumps to the start of the iteration for another round.

```c++
#import "Logger"

for 0..5 {
    log(nr)
    if nr == 1
        break // we break out early, code will only print numbers 0, 1
}

log("----")

for 0..5 {
    if nr > 0 && nr < 4
        continue // we skip some numbers and only print  0, 4
    log(nr)
}
```

Sometimes you may prefer to use break to get out of a loop instead of the condition. Thus, you can skip the expression to get an infinite loop. The compiler will complain if you don't have a break-statement.
```c++
#import "Logger"

n := 0
while {
    log(n)
    n++
    if n == 5
        break
}
```

## For-loop with user-defined iterators
**NOTE:** This part will use structures but not explain them. I recommend reading the chapter about structs before reading this.

User-defined iterator is about the user creating a structure that implements the methods **create_iterator** and **iterate** as well as an iterator struct containing relevant information for iteration. A reference to an instance of struct can then be used in a for-loop expression.

Here is a basic example.
```c++
#import "Logger"
#import "Memory"

struct ListIterator {
    // iterator struct must have these two members named exactly like this
    value: i32* = null;
    index: i32 = -1;
}
struct List {
    data: i32*;
    fn init() {
        data = Allocate(5 * 4)
        data[0] = 10
        data[1] = 20
        data[2] = 33
        data[3] = 44
        data[4] = 0
    }
    // method should have zero arguments and return one struct
    fn create_iterator() -> ListIterator {
        return {}
    }
    // method should have one argument (pointer to struct) and return a boolean
    fn iterate(iterator: ListIterator*) -> bool {
        iterator.index++
        if !data[iterator.index] {
            iterator.value = null
            return false
        }
        iterator.value = &data[iterator.index]
        return true
    }
}

list: List
list.init()

for list {
    log(nr, ": ",*it) // it is a pointer to an integer so we dereference it, don't forget this
}
```

**NOTE:** You cannot use @ptr or @rev. You can change the type of *ptr* in the iterator struct to whatever you want so @ptr isn't exactly necessary. It should preferably be a pointer for consistency. @rev is explained further down.

As you can imagine, you can implement the iteration logic however you want. You can define the iterator struct with however many members you want as long as the required ones are present (value and index).

Another nice thing about this feature is that it is compatible with while-loops too.

```c++
// ...previous code...

list: List
list.init()

iterator := list.create_iterator()
while list.iterate(&iterator) {
    it := iterator.value
    nr := iterator.index
    log(nr, ": ",it)
}
```

**DISCLAIMER:** You cannot iterate in reverse (@rev) yet. I am not sure what is the best way to do it. Either an extra method `iterate_reverse` or an extra argument to iterate `iterate(iter: Iter*, reverse: bool = false)`. I don't want both because it means more work for mainting it. It may not be a lot of work but if all features have multiple ways of doings things then it will eventually become a lot of work. Keep it minimalistic while I can.