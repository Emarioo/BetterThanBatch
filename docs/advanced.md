## Recursive macros
There is a limit to this. The implementation uses recursion which
means that the hard limit is the limit of the call stack.
Except for that there is also a limit set in Config.h.
The value varies depending on what is being tested but a normal value is 100.

```
#define Macro(x,...) Macro(...)
#define Macro(x)
```

The fundamentals of recursive macros is
1. A macro with infinite arguments.
2. Macro calling itself with one argument less.
3. A macro with a fixed length

This is the same with recursion when using functions.
A function calls itself and at some point it reaches a base case
where it will stop.

```
#define Add(x,...) x + Add(...)
#define Add(x) x
Add(1,2,3,4)    ->   1 + 2 + 3 + 4
```
The above does the following
  Add(1,2,3,4) -> 
  1 + Add(2,3,4) ->
  1 + 2 + Add(3,4) ->
  1 + 2 + 3 + Add(4) ->
  1 + 2 + 3 + 4

The reason this works at all is because the macro which fits
the argument amount the best is chosen. For Add(x,...) to be
chosen you must have at least on argument. For Add(x) you must
have one. The infinite macro is always chosen last.
Add(x) is our base case.

```c++
#multidefine ToStr(...)
const char* ToString(int type){
    switch(type){
        ToStrCase(...)
    }
}
#enddef
#define ToStrCase(X,...) case X: return #X; ToStrCase(...)
#define ToStrCase

ToStr(iron,gold,copper) ->

const char* ToString(int type){
    switch(type){
        case iron: "iron";
        case gold: "gold";
        case copper: "copper";
    }
}
```
Imagine the above. This is why recursive macros are powerful.