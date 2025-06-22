
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
The language is typed which means that every variable has a **type**. A type describes the kind of data that is stored in a variable and it's size. These are the primitive types:

**Signed integers**: `i8`, `i16`, `i32`, `i64` (1, 2, 4, and 8 bytes respectively)

**Unsigned integers**: `u8`, `u16`, `u32`, `u64` (cannot represent a negative number)

**Word integers**: `uword`, `iword` (the size depends on the target architecture, uword = u32 on a 32-bit system while uword is u64 on a 64-bit system)

**Character**: `char` (represents a 1-byte character, ASCII)

**Boolean**: `bool` (represents a 1-byte true or false value)

**Decimal/floating point numbers**: `f32`, `f64` (4 and 8 bytes respectively)

**Pointers**: `void*`, `i32*`, `char*` (8 bytes on a 64-bit computer)

**Function pointers**: `fn()`, `fn(i32*)->i32` (8 bytes on a 64-bit computer, function pointers are covered in the chapter about functions)

The type of a variable can be specified between the colon and equal sign. Otherwise, the type is infered from the expression.
```c++
num0: i32 = 23
num1: f32 = 96.5
final := num0 + num1 // type of final is infered from the expression

chr: char = 'A'
yes: bool = true
```

**NOTE:** uword/iword is an alias for the specific integer type and will show up as i64 instead of iword in error messages.

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
str: char[] = "My string"  // string literal, more about how to use strings further below
```

**NOTE**: `char[]` is equivalent to `Slice<char>` which is a polymorphic struct that contains a pointer and a length. See the chapters about structs and polymorphism for how to create this kind of type yourself.

## Pointers
The language has pointers like C/C++.
```c++
a: i32 = 82;
ptr: i32* = &a;     // take the address of a variable
value: i32 = *ptr;  // dereference the address to get the value
```

You can add and subtract integers from a pointer value. The **difference** from C/C++ is that there is no scaling.
Adding 3 to a 32-bit integer pointer will not scale it up to 12. `sizeof(type or expression)` can be used to retrieve
the size of the type and then you can multiply by the integer yourself.
```c++
arr: i32[10];

scaled_ptr := arr.ptr + 9 * sizeof(i32)
*scaled_ptr = 92

// Use the index operator for automatic scaling
scaled_ptr = &arr.ptr[9]
*scaled_ptr = 52
```

## Slices
A *slice* is a built-in type (struct) that combines a pointer and a length. It is recommended to use slices instead of raw pointers and a length when working with multiple elements. The compiler gives you better support and you avoid bugs from missing length information (this happens in C since pointer and length is separate).

```c++
// we cover imports and functions later, we keep these here to have a complete example
#import "Memory" // imports the 'Allocate' function
#import "Logger" // imports the 'log' macro which prints different types to the terminal

slice: i32[];
slice.len = 4
slice.ptr = Allocate(slice.len * sizeof i32)

slice[0] = 23
slice[2] = 2
log(slice[0] + slice[2]) // prints 25
```

## Arrays (fixed size)
Arrays behave a lot like slices with the difference of representing a contiguous list of elements in memory. They have `ptr` and `len` fields for consistency but this is just syntactic suger.

```c++
ints: i32[20]             // zero initialized array on stack
ints: i32[20] = { 3, 8, 4 }; // initialize with values
ints: i32[.] = { 3, 8, 4 };  // array length is set based on number of expressions in the initializer

ints.ptr[0]  // first element from the pointer
ints.len     // length of array type 
ints[0]      // also first element
ints[ints.len - 1] = 239; // set last element
```

Arrays are implicitly casted to slices and pointers of the same element/base type.

## Strings
There is not a primitive type for strings. `char[]` is used for string views and `StringBuilder` from the `String` module is used when concatenating and transforming strings.

```c++
// char[] usually passed to functions, opening a file requires a path which would usually be char[]
str: char[] = "String";
str[str.len - 1] // access last character which is 'g'

// StringBuilder when creating and appending strings and numbers
#import "String"

string: StringBuilder
string.append("Hello, I have")
string.append(5)
item := "cookies"
string.append(item)

// There is also a convenient macro to avoid repeating yourself
msg: StringBuilder
appends(msg, "This is the string: ", string)
```

For the future we will add more convenient string support in the form of operator overloading and user language features which you could modify and make yourself (`String` will implement these).
```c++
a := "Hello" + " Sailor!"  // <- this is not possible but would be nice and convenient
```

Quotes and backslash in literal strings are special.
```c++
// quotes in string
s0 := "Hello \"cat\" and \'dog\'"

// special characters
s1 := "newline: \n"
s2 := "tab: \t"
s3 := "carriage return: \r"
s4 := "null char: \0"
s5 := "escape char: \e"
s6 := "backslash: \\"

// hexidecimal
s7 := "hex: \x1f" // escape character
s8 := "hex: \x00" // null character
s9 := "hex: \x20" // space character
sa := "hex: \x41" // 'A'
```

There is also a text block which puts the exact characters into the text literal. Backslash and quotes do not require escaping with a backslash.
```c++
text := @strbeg
Hello "cat" and 'dog'. Backslash \x23 does not do anything.
Newline is handled correctly.
@strend
```

<!-- Work in progress

This is useful when printing.
```
x := 11
y := 23
prints("$x + $y = $(x+y)")
```
-->
## More operations
bitwise operator, comparison/equality operator, logical operator

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