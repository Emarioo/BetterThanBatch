## General enum generation
Make one or more macros which allows you to generate enums
and a function to conver enum to const char*.

```
#define EnumTypes iron,gold,diamond,cobble
#define EnumName Items

const char* ToStr(int type);

#define DeclareEnum(EnumName,#unwrap EnumTypes)
#define DefineToStr(EnumName,#unwrap EnumTypes)
```


## Switch case mapping with two lists
```
const char* map(int type){
    switch(type){
        case a: return "b";
        case c: return "d";
        ...
    }
}
```
The left and right side can be any integer and string literal.

```
#define LValues 1,7,3,5
#define RValues sugar,sand,snow,wood
```

<details>
<summary>Hint 1</summary>
#define Rev(x,...) Rev(...), x
#define Rev(x) x

#define Cone(x,...,y) x,y,Cone(...)
#define Cone(x,y) x,y

Cone(#unwrap LValues,#unwrap Rev(RValues))

</details>
<details>
<summary>Hint 2</summary>
#define Case
#define Case(x,y,...) case x: return y; Case(...)
//#define Case(x,...,y) case x: return y; Case(...)

Case( ? )
</details>
