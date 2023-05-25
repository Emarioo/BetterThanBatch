# THIS GUIDE IS FOR A DIFFERENT VERSION OF THE LANGUAGE
I haven't deleted this because the guide for the new
version will probably have the same sections with
some differences.

# Guide to the language
A quick guide which doesn't go much into detail since things might change.

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
z = a + b % c
```
Math works as you would expect from other languages like Javascript.
Using variables in expressions also works like Javascript.

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
a = "123"
a[1] = "-"
```
With the above you can copy a character into the string.

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

## External functions
These are implemented in C++. There are two places you can call them.
In a body or where a statement begins like below.
```
print hello
if 1 {
    print Hello
}
```
Or within an expression
```
result = #run tonum "911"
if 1 == #run tonum "1"
    print result
```

#run is used when calling functions inside an expression.
```
rand = random        ->   ERROR: undefined random
rand = #run random   ->   SUCCESS
```

Functions currently only allow one string as argument.
This will certainly change at some point. Most tokens
after the function name will be treated as a string (even if not quoted)
unless it is a variable name. Tokens inside paranthesis
will be treated as an expression.
```
print Milkshake: (is) tasty    ->     Error, undefined is
```
Line feed (\n) or semi-colon marks the end of the arguments.
```
print Okay; a = #run random   ->   STDOUT: 'Okay'  ASSIGN: a = some random number
print Okay a = #run random    ->   STDOUT: 'Okay a = 0.2561'
```

Functions return a value as you may have noticed with '#run random'.
The function below gives you a "list" of files from the current working directory
based on the arguments. This is recursive meaning files in subfolder as well.
```
files = filterfiles *.cpp
```

You can find all external functions in
[](src/BetBat/ExternalCalls.cpp)
At the bottom you will find a function called GetExternalCall.
This is what the interpreter uses to map strings to C++ functions.

## Calling executables
Simular to how it works in a shell.
```
gcc.exe main.c     <- runs gcc
```

The first part is the path to the executable.
It can be a quoted token or a few tokens without a space.
'gcc.exe' has three tokens and no space. Everything
after are the command line arguments.

You have to use .exe at the end. (may change but this is how it works right now)

cmd is an abbreviation of 'cmd.exe /K'. No .exe is needed here.
```
cmd gcc main.c
```

## User functions
These are defined like this:
```
log = {
    print hey from log
}
```
You can call them as you would with other functions
```
log
a = 1 + (#run log)
```

The argument you pass is once again just one string.
It is used like this:
```
add = {
    sum = 0
    each #arg {         #arg is "2 3"
        sum += #tonum #v
    }
    return sum
}
print (#run add 2 3)   ->  STDOUT: '5'
```
#arg is the argument passed to the function.
A function normally returns null but you can 
return your own value.
(read up on each in a previous section if you are wondering
what it does)

You can make functions inside functions and access
variables outside the function.

NOTE:
How functions should access variables outside isn't decided yet.
There are some problems when using asynchronous functions.

## Asynchronous functions
You can run a function asynchronously.
```
fun = { for 10 print #arg }
#async fun 1
fun 2
```
#async fun will create a new user thread and
the interpreter will switch between the active threads.
The output of the code above will be mashed up ones and twos.

The interpreter itself runs on one thread and does a round
robin on which user thread to run so you will not gain
any performance boost by using asynchronous functions.

However, executables and external functions can be
run on another thread.
```
numThread = #async tonum "5"
print TI: numThread
print (10 + #join he)

gccThread = #async gcc
print Before GCC [gccThread]
print Exit code: (#join g)
```

## Reading and writing to files
This is done using <, > and >>.
Code below reads a file and prints the content
```
content = ""
content < file.txt
print content
```

Write to a file
```
content = "Hello from a script"
content > file.txt
```

Append to a file. > will overwrite the content of the file while >> will append.
```
content = "Hello from a script\n"
content >> file.txt
content = "A second message"
content >> file.txt
```

The file path must be a string but it can also come from an expression
```
path = "file.txt"
content = ""
content < path

content < ("file" + ".txt")
```

## Preprocessor directives
Note that these directives remove, add and rearrange tokens.
You can use @disable and @enable as shown below to disable certain steps in the compiler.
This is useful when experimenting with the preprocessor as the parser won't bother you with error messages.
```
@disable all
@enable preprocessor
```


```
#ifdef {name} {tokens} #endif
```
Simular to C.
```
#ifndef A
print will print
#else
print won't print
#else
print will print
#endif
```
The difference is that #else "toggles" which section will be ignored.

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
Say()    ->    equivalent to the above
```
Macros can have arguments.
You can define multiple macros with the same name but different amount of argument.
Redefining a macro with the same name and argument amount will overwrite the previous one.
If you use a macro name with an amount of arguments that doesn't exist you will get an error.

```
#define Base(A,...) A = ...
Base(var,well, cool, ?)     ->  var = wellcool?
```
Each macro name can have one macro with an unspecified amount of arguments. (VA_ARGS in C)
There isn't a set limit. To many arguments may cause an allocation to fail which may crash the compiler.

```
#undef {macro}
#undef {macro} {argument amount}

#undef Say
#undef Say 1
#undef Say ...
```
Undef will undefine all macros of that name.
A number afterwards specifies which macro to remove.
Three dots will remove the unspecified argument variant.

```
#define Nom(a,b,c) a + b + c
#define Grub 1,2,3
#define Grub2 1,2

Nom(Args)            -> not enough arguments,   1,2,3 + ? + ?

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
you just want to know what it's about check out [](docs/recursive-macros.md)
