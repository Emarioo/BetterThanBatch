Thoughts about preprocessor until (2023-05-25). I noted things in a different document after that. 

Maybe not necessary for the language but it could be a somewhat useful addition as well as interesting to implement.

Preprocessing operates on tokens. Tokens may be moved, duplicated and modified. Preprocessor doesn't do any math or function calls.

Preprocessing should be turing complete. Sequence, Selection, Iteration. Good luck intellisense. 

Preprocessor directives uses # in this document but it may be different in the current state of the language.

Preprocesser comes after **Comments** and **Tokenizer**.
```
// #define LOG(X) X
#define LOG(X) // print OKAY
```

# Features
## Concatenation
Works in macros the same way `##` works in C macros.
```
#define Concat(a,b) a..b
Concat(Hej,Hoo)      -> HejHoo
Concat(A B,C D)      -> A BC D

#define Quad(A) Concat(A..A,A..A)        .. does not work in argument
                                         evaluation. (may change)

```

## define
Two ways to use define.
```
#define {name} {tokens}

#multidefine {name}           new line indicates usage of multiple lines
{tokens}
{tokens}
...
#enddef                     don't forget to end it!

#define {name} #enddef      definition with no tokens
```
The feature works like C/C++.
```
#define Hey Goodbye
Hey	-> Goodbye
```
Redefining is allowed but throws a warning. Cannot be turned off at the moment.
```
#define ONE 1
ONE -> 1
#define ONE 2
ONE -> 2
```

```
#define Yes No
Yes -> No
#define No chicken
Yes -> chicken       "No" exists at the moment of evaluation 
```

```
#define HAPPY sad HAPPY
HAPPY -> sad HAPPY       Notice that there is no recursion

#define TRICK0 might TRICK1
#define TRICK1 work TRICK0

TRICK0 -> might work TRICK0     Infinite recursion is not allowed and this
  output is probably not wanted. An error will therefore be thrown.
```

## Advanced define
Also works with multiple lines the same way as the normal `#define`.
Usage:
```
#define {name}(variables...) {tokens}
```
Also works simular to C/C++.
```
#define ADD(X,Y) X + Y
ADD(9,23) -> X + Y

#define ONE 1
ADD(ONE,2) -> 1 + 2

#define SAM()     <- this turns into
#define SAM       <- this with no arguments
```
Variable argument length and recursion
```
#define List(...) [...]
List(a,b,c) -> [abc]

#define Add(E,...) E + Add(...)
#define AddEnd(E) E
Add(2,6,9) -> 2 + 6 + 9
```

## unwrap
Used to spread tokens from a definition into argument values onto another definition. 
```
#define Vals 1,2
Add(Vals) -> 1,2              since Vals is seen as one argument

Add(#unwrap Vals) -> 1 + 2    unwrap will recognize comma in Vals 
                              as a delimiter 
```

```
#unwrap Add(1,2)       Unwrap not allowed. Args must be unwrapped into
                       another function.

#define He(A,...) #unwrap A = ...      Bad again

#define Test(A,B) A == He(#unwrap B, #unwrap Add(1,2))
	Is allowed since we unwrap arguments or definitions which are
	in paramterers to another definition.
```

## undef
Will remove definitions. 
```
#define Sup(A,...) A = ...
#define Sup(A) A = none
#undef Sup            removes definition with any argument length 
	-> nothing remains
#undef Sup(1)         removes definition with specific argument length
	-> Sup(A) is removed
#undef Sup(...)       removes def. with infinite arguments
	-> Sup(A,...) is removed
```

## ifdef and ifndef
Same as C/C++. It works on any defintion
```
#define YES yes

#ifdef YES
YES is defined
#else
YES is not defined
#endif
```

## if
If as a directive is very useful when you do need it but it isn't often you do. Below is at least one example.
```
#define VERSION 1.1

#if VERSION <= 1.0
...
#elif VERSION == 1.1
...
#endif
```
Something to ponder about is how `#define VER 1.1.5` could work. Does `#if VER == 1.1.5` do a check based on tokens or the actual number? Since you may want `<` or `>` this `1.1.5` is allowed in a define of course but causes an error in `#if` when 1.1.5 is intepreted as a number. Perhaps it is read as `1.1 .5`. First part is the number then `.5` is random stuff. Depends on how the tokenizer works I guess. 

## quoted
`#quoted hello!` becomes `"hello"!`. Note that quoted applies to the next token.
However, with this `#define Q(A) #quoted A` and `Q(hello!)` you get `"hello!"` because `#quoted` applies to all tokens in argument `A`.
How about `#quoted (hello!)` does it become `"hello!"`, `"(hello!)"` or `"("hello!`. Following the rule of the next token the latter would be correct. However, paranthesis could be a special case. A problem occurs if you want to turn paranthesis into a quote. The `#define Q...` example may handle `Q((hello!))` to become `"(hello!)"`.
```
#define Q(X) X
#quoted Q(hey) -> "hey"
#quoted #quoted Q(hey) -> cannot do quoted twice
#quoted Q(#quoted hey sup) -> "\"hey\" sup" or just "hey sup"? do they accumulate?
// What use is there for accumulated quotes?
```

## eval
Evaluate expressions in the preprocessor. There are some limits when using assigned variables and functions. Math and macros are evaluated properly.

A use case for this would be:
```
#multidefine REPEAT(X,N)
#if (N>=0)
	REPEAT(X, #eval (N-1) )
	X..N
#endif
#enddef

REC(arr,2)
// ^ becomes
arr0
arr1
arr2
```

`#if` uses eval automatically.

# Other
## Use cases
#### Modifications
```
// You can easily turn on/off logging in the script with
#define LOG(X) X
// #define LOG(X)

LOG(print Hello)

// Modify flags
#quoted Atoken -> "Atoken"
#suffix_space Atoken+ -> Atoken +
#non_quoted "AToken" -> Atoken

// What do we do if we want to quote multiple tokens together?
#quoted (A B) -> "(A B)" OR "A B"
#quoted (A B) -> "("A B)

// This might work best without space between quoted and paranthesis
#quoted(A B) -> "A B"

```
#### Selection
```
// Selection
#ifdef LOG
print Yes logging
#else
print No logging
#endif
```

#### Recursion
You can introduce more complex syntax with `...` where you choose every second argument. The limit is your **imagination**.
```
#define REC(A,...) print A; REC(...)
REC(You,Are,Great) ->
	print You; print Are; print Great;
```

How about selection within the recursion? Also what is considered within `#define`?
C uses \\ but that seems annoying. `#begin` and `#end` to mark a region for `#define`. Otherwise the region is from `#define` to line feed?
```
#define REC(A,...) #if A==a REC(b,...) #else print A; REC(...)
REC(a,b,c) ->
	print b; print b; print c;

#define SEL(A) #if A==a print hey
#else print No
SEL(a) -> ?
```

What about **infinite recursion**? There are some cases where the preprocessor can detect this for example: `#define A(a,...) a A(a,...)`. `#define A(a,...) A(...)`  is the proper usage. For any potential edge cases you can have a limit of 1000 recursions. If wanted you can specify a different limit (`-1` for no limit, `0` for no recursion and numbers above 0 defines the limit)

#### Iteration (later, recursion might be enough)
Lists of tokens should be possible.
How does C limit you and fix it.
```
#define LIST Hey,Hallu,Sip
#foreach LIST print #item
```


## What preprocessor can solve
Enums and string conversion. This is difficult in C.
The preproc. is good if it can solve this. Enums in this language doesn't exist but you can define variables and use them as enums.

```c
// abc.h
enum ABC {a,b,c};
// abc.c
const char* ToString(ABC abc){
	switch(abc){
		case a: return "a";
		case b: return "b";
		case c: return "c";
	}
}
```
What you will notice is that you have to specify the types in two places. The enum itself and in the ToString function. C does not have a preprocessor to iteratively generate cases.
```c
enum ABC {a,b,c};
const char* ToString(ABC abc){
#define CASE(T) case case T: return #T;
	switch(abc){
		CASE(a)
		CASE(b)
		CASE(c)
	}
}
```

Here we see the same issue. First the numbers in a, b, c are set manually and not incremented automatically. Second issue is the specification in two places.

```
a = 0
b = 1
c = 2
tostr = {
	if #arg0 == a return "a"
	if #arg0 == b return "b"
	if #arg0 == c return "c"
}
```

Here we use recursive preprocessing to handle a "list" of types. It is somewhat simular to BNP. Also note that `#quoted` isn't evaluated in `#define`. It's first when a usage of the definition occurs that `#quoted` is evaluated.
```
#define ENUMS(I, ...) I=ACC++ ENUMS(...)
#define ENUM(...) ACC=0 ENUMS(...)
#define CASES(I, ...) if #arg0 == I return #quoted I CASES(...)

#define ABC a,b,c 
ENUM(ABC) ->
	ACC=0 a=ACC++ b=ACC++ c=ACC++

tostr = {
	CASES(ABC) ->
		if #arg0 == a return #quoted a
		if #arg0 == b return #quoted b
		if #arg0 == c return #quoted c
}
```

# Additional use cases
## At every function
You have many functions and you want to log when the execution enters and exit each of them. The token where `#any` is in `#replace` is moved or kept in `#with`.
use `#begin` and `#end` for preprocessor directives to work on multiple lines.
Note that scopes in the function is properly handled. The `...` part isn't determined to be `if 1 { a` but rather `if 1 { a }`. Quotes and paranthesis are also handled.
```
#replace #any = { ... }
#with = { print Enter #any; ... print Exit #any; }

a = { print hi; }
b = { if 1 { a } }
b // -> Enter B, Enter A, hi, Exit A, Exit B

```

## Many equals or nots
```
#define ORS(T,Item,...) T==Item || ORS(T,...)
#define ORS(T,Item) T Item
ORS(type,HEY,OK,YES)
	-> a==HEY||a==OK||a==YES

#define NAND(Item,...) ! Item || NAND(...)
#define NAND(Item) ! Item

NAND(v0,v1,v2)
	-> !v0||!v1||!v2

#define MANY(prefix,suffix,delim,Item,...)
	prefix Item suffix delim MANY(...)
#define MANY(prefix,suffix,delim,Item) prefix Item suffix
```
