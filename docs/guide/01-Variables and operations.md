# Variables, operations and expressions
Words: declaration, assignment, unary operator, binary operator, primitive types

This is how you declare and use variables.
```c++
a := 5    // declaration
c := a - 9

c = 2 * b  // reassign a value to a variable, note the sole '=' without the ':'
```

**NOTE**: Semi-colon ';' is used to separate statements (like variable assignments/declarations) but it is optional. There is ambiguity in the syntax where semi-colon is required, multiplication and dereference for example.

## Arithmetic operations

```c++
12 :=  3 + 9   // addition
 3 :=  5 - 2   // subtraction
18 :=  9 * 2   // multiplication
12 := 25 / 2   // integer division
 1 :=  6 % 5   // modulo
```

## Primitive types
The language is typed which means that every variable has a **type**. A type describes the kind of data that is stored in a variable and it's size.

These are the (current) primitive types:

**Signed integers**: i8, i16, i32, i64 (1, 2, 4, and 8 bytes respectively)

**Unsigned integers**: u8, u16, u32, u64 (cannot represent a negative number)

**Character**: char (represents a 1-byte character, ASCII)

**Boolean**: bool (represents a 1-byte true or false value)

**Decimal/floating point numbers**: f32, f64 (4 and 8 bytes respectively)

**Pointers**: void*, i32*, char* (8 bytes on a 64-bit computer, the compiler only supports 64-bit)

The type of a variable can be specified between the colon and equal sign. Otherwise, the type is infered from the expression.
```c++
num0: i32 = 23
num1: f32 = 96.5
final := num0 + num1 // type of final is infered from the expression

chr: char = 'A'
yes: bool = true
```

## Literals
Literals refer to the constant numbers, strings, and floats in the code.
Most (or all) literals use the 32-bit variation when available.

```c++
a := 9241           // signed integer literal
a =  9241s          // signed integer literal
a =  9241u          // unsigned integer literal
a =  0x92_39        // integer literal but hexidecimal form
a =  0b1_0110_0101  // integer literal but binary form
// underscore can be used to separate the digits/hexidecimals/bits making it easier to grasp their value

f := 3.144   // float literal
f := 3.144d  // 64-bit float literal

chr: char = 'K'   // character literal
str: Slice<char> = "My string"  // string literal, more about how to use strings further below
```

**NOTE**: `Slice<char>` is a polymorphic struct that contains a pointer and a length. See the chapters about structs and polymorphism for how to create this kind of type yourself.

## Pointers and arrays
The language has pointers like C/C++.
```c++
a: i32 = 82;
ptr: i32* = &a;     // take a pointer to a variable
value: i32 = *ptr;  // dereference the pointer to get the value at the pointer's address

ints: i32[20];             // zero initialized array
ints: i32[20] { 3, 8, 4 }; // initialize with values
// the type of 'ints' is Slice<i32>
ints: i32[.] { 3, 8, 4 };  // array length based on number of expressions

// Arrays have a pointer and a length
ints.ptr[0]  // first element from the pointer
ints[0]      // first element using a predefined operator overload for Slices

// inst.ptr must be used when setting the value of an element (operator overload for it doesn't exist yet)
ints.ptr[ints.len - 1] = 239; // set last element

str: Slice<char> = "String";
str[str.len - 1] // access last character which is 'g'
```

The language has pointer arithmetic which means that you can perform add and subtract operations on
pointers with integers. The **difference** from C/C++ is that there is no automatic scaling. Adding 3 to a 32-bit integer pointer will not scale it up to 12. `sizeof(type or expression)` can be used to retrieve the size of the type and then you can multiply by the integer yourself.
```c++
arr: i32[10];

*(arr.ptr + (arr.len-1) * sizeof(i32)) = 92  // set the value of the last element
// Note that you would use the index operator for things like this.
arr.ptr[arr.len-1] = 92


```

**NOTE**: In the future, not having the automatic scaling may be a nuisance and thus could change. The reason we don't is because we want pointer arithmetic on `void*` but since it is 0 in size, it doesn't make since to scale it by 0 bytes. You could of course see void* as an edge case and use a 1-byte scaling. In C/C++ you are required to do quite a few casts and it would be nice if you didn't need to. Sometimes you can feel as though you are fighting the pointer arithmetic which isn't good.

## More operations
Words: bitwise operator, comparison/equality operator, logical operator

```c++
11 = 9 | 3  (bitwise or)
 1 = 9 & 3  (bitwise and)
10 = 9 ^ 3  (bitwise xor)

true  = 4 < 9  (less than)
true  = 9 <= 9 (less or equal than)
false = 4 > 9  (less than)
true  = 9 >= 9 (less than)
false = 4 == 9 (is equal)
true  = 4 != 9 (is not equal)

false =  true && false    (logical and)
true  = 4 < 9 && 5 != 9
true  = false || true     (logical or)
```

## Type conversions
TODO: Pointer, integer, decimal conversion
TODO: More information about casting
```c++
n: i32 = 5
f: f32 = n // n is implicitly casted to float

f := cast<f32> n // explicit cast to float
```

```c++
i: i32*;

f: f32* = i // you cannot implicitly cast pointers of different types
f: void* = i // unless you are converting to void

f: f32* = cast<f32*> i // forcefully cast the pointer type
```

## Possible mistakes
The language makes it slightly more difficult to create unintentional bugs when mismatching
unsigned and signed integers of various sizes but it is important to note that operations
that can cause integer overflow and underflow should be treated with care.

There is also the issue of converting large integers to small integers where you lose data.
Converting small integers to large integers is fine but sometimes you may write something like this:
```c++
a: u8 = 5
b: u32 = a << 16    
// shifting an 8-bit integer 16 bits will always result in 0, b will not be 0x5_0000

b: u32 = cast<u32> a << 16 // you must cast 'a' first

```