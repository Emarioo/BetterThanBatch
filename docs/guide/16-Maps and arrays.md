# Maps and arrays
Data structures are not internal concepts of the compiler. Like C and C++ you create them yourself. Luckily you do not have to because the standard library provides the most common structures for you.


Here is a dynamically sized array
```c++
#import "Array"

arr: DynamicArray<i32>;

arr.add(2);
arr.add(4);
arr.add(6);
for arr.sliced_unsafe()
    log(it);
arr.remove(1);
for arr.sliced_unsafe()
    log(it)
```

Here is a hash map for integers:
```c++
#import "Map"
#import "Logger"

map: Map<i32,i32>;
map.init(3)

map.set(3,1);
map.set(52,2);
map.set(92,3);
map.set(32,4);
map.print();

map.remove(32);
map.remove(3);
map.print();
```