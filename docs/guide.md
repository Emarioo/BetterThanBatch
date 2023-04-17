# Guide to the language
A quick guide which doesn't go much into detail since things might change.
I will have to excuse for some of the informal parts.

## Variables and expressions
```
{name} = {expression}
```
The above is called an assignment. The left side is
the variable name. The right side is an expression.

```
a = 1 + 2 - 3 * 4 / 5
b = 1 + 2 * ( 3 * 4 - 5 ) / 5
c = 1+1 == 2 && (9 || 0)
z = a + b + c
```
Math works as you would expect from other languages like Javascript.
Using variables in expressions also works like Javascript.
There is no modulus operation yet.

```
a = "hello"
b = a + " world"
c = a[0:3] + b[2]
d = "we go home"[3:4]
```
This is how strings are made. You can concatenate with +
and get a substring with \[x:y\]. X is the start index while Y
is the end (inclusive). \[0:0\] is the same as \[0\].

```
a = 3.type
b = "what".type
c = "how long".length
```
These are called properties. Length only works on strings.

If you are wondering where arrays are then I am afraid they
don't exist. You will have to use strings as arrays and
substring to access the "elements".
Use `(#run tonum "3.24")` to convert a string into a number.

## If, while, for and each (not complete)
```
if {expression} {body} else {body}
```
If the expression is true
The above is the general syntax for if and while.
If the expression is true the code in the body will run.
```
while {expression} {body}
```

```
sum=0
i = 0
while i < 10 {
    sum = sum + i
    i = i + 1
}
print sum
```
```
sum=0
for 10 {
    sum = sum + #i
}
print sum
```
```
list = "one two three"
each list {
    print item: #v
}
```


## Functions and executables (not complete)
Some quick examples. More details will come later.
```
gcc.exe main.c     <- runs gcc

print hello        <- print to console (stdout)
```


## Preprocessor directives
Note that these directives remove, add and rearrange tokens.
When testing preprocessor I would recommend only using
the tokenizer and preprocessor. Not the parser. This way the
parser doesn't bother you with error messages.  
`parser.exe -steps preprocessor`

The best part about the preprocessor is the #define/macro part.
If there is one thing you should read it would be that.

```
#ifdef {name} {tokens} #endif
```
Same as C.

### Macros with #define
```
#define {name} {args} {tokens}
```
With the above you can define a macro and wherever the macro's
name shows up, the contents of it will replace the name.

```
#define PI 3.14
PI   ->   3.14
```
PI will be replaced with the number.

```
#define Say(who,msg) [who]: msg
#define Say(who,date,msg) [date, who]: msg

Say(me,Hi you)            ->   [me]: Hi you 
Say(me,01:28,You awake?)    ->    [01:28, me]: You awake?

Say    ->    Error: Macro cannot have 0 arguments
```
Macros can have arguments. You can define multiple macros with the same
name but they need to have different amount of arguments. Redefining
a macro with the same name and argument amount will overwrite the previous one.
If you use a macro name that does exist while the argument amount doesn't you
will get an error.

RootMacro refers to the namespace of existing macros with different argument amounts. CertainMacro refers to a specific defined macro with a certain argument
amount.

```
#define Base(A,...) A = ...
Base(var,well, cool, ?)     ->  var = wellcool?
```
Each macro namespace can have one macro with an infinite amount of arguments.
I have not set a limit but there is of course one. Either the program crashes,
the operating system does something about it or your computer crashes.

```
#undef {macro}
#undef {macro} {argument amount}

#undef Say
#undef Say 1
#undef Say ...
```
Just undef and macro name will undefine the whole macro namespace. If you have a
number afterwards then the macro with the specific argument amount will be removed.
Three dots will remove the infinite argument variant.

```
#define Nom(a,b,c) a + b + c
#define Grub 1,2,3
#define Grub2 1,2

Nom(#unwrap Args)     -> 1 + 2 + 3
Nom(9,#unwrap Grub2)  -> 9 + 1 + 2

```
You can use #unwrap to seperate commas from another macro and pour those
tokens/arguments into another macro.

```
#define Front(T,a,b) T = Back(a,b) - Back(a,b)
#define Back(a,b) [a + b]

Front(var,1,2)    ->    var = [1 + 2] - [1 + 2]
```
Macros can call other macros. Note that macros can be defined
in any order as long as the macro exists at the time of evaluation.

I want to end with the best for last and that is the idea of recursive macros.
I encourage you to explore it for yourself before continuing reading but if
you just want to know what it's about check out [](advanced.md)
