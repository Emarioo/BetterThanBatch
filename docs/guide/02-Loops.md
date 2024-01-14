# Loops
There are two loops, for and while.

## For
The *for* loop is used to iterate through an array or range of values

```c++
#import "Logger" // See chapter about "Imports and namespaces"
// Logger provides a macro/function called 'log' which can print numbers, strings, and floats among other types.

TODO: Explain it, nr
TODO: Explain that it, nr uses the same memory with ranges but not with slices

for 0..5
    log(it);

for i: 0..5 {
    log(i);
}

arr: i32[10] { 5, 9, 2, 4, 23, 1982, 18};

for arr {
    log(it);
}
    
for num: arr
    log(num);
    
```
For loops have two annotations which are `@pointer` and `@reverse`. They can be abbreviated to `@ptr` and `@rev` (annotations are explained in another chapter).

```c++
// loop in reverse printing 4, 3, 2, 1, 0
for @rev 0..5
    log(it)
```
`@pointer` is only allowed with slices. Instead of `it` being the actual type, it is a pointer to the value in the slice.
```c++
arr: i32[5]{4,7,9,13,25}
for @ptr arr {
    log("value:",*it, " relative address:", it - arr.ptr)
}
```

## While
The *while* loop is used when looping/iterating as long as a condition is true.

```c++
i := 0
while i < 5 {
    log(i);
    i++;
}

i := 0
while { // infinte loop
    log(i);
    i++;
    if i == 5
        break;
}
```

## Break and continue
All loops support *break* and *continue*.

```c++
TODO: break does what
TODO: continue does what


while {
    
}

```

With while loops, you can skip the expression to get an infinite loop. However, the compiler will complain
unless you have a break statement inside the while's body.
```c++
n = 0
while {
    log(n)
    n++
    if n == 5
        break
}
```