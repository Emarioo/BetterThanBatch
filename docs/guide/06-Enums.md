# Enums
An enum is a type that contains members of names and values. The value of an enum type is the same as an unsigned or signed integer with a few differences with type conversions and allowed operations. The value of the members automatically increment starting from 0.
```c++
enum Flavour {
    STRAWBERRY,    // 0, starts at zero
    CHOCOLATE,     // 1, auto incremented
    CHICKEN = 82,  // user defined
    TASTELESS,     // 83, auto incremented
}

a: Flavour = Flavour.CHICKEN;
b := CHOCOLATE;                 // if you want to be brief


```c++
An enum is a i32 by default but can be changed like this (only integers allowed):
```
enum Hi : u8 { }
```

TODO: explain enum annotations