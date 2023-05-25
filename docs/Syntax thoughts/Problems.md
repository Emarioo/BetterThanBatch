All conclusions here are not final. Most of them are thought experiments on how to solve some of the problems with the syntax.

## Define different from C
`@define NONE @end` is required for NONE to have no tokens. This is different from C and as a C programmer you will forget this and mess up. `@define NONE` means multiline definition. Multiline definition is fantastic and should exist but is this the right way? Would `@multidefine NONE` or `@define NONE ...` (actual dots) be better?

## Defined or string?
```
var = main // var = "main"
var = str // var = "hello" (assume str = "hello")
var = add 5 9 // var = 14
```
We want the result above. To do this we need to know which tokens are undefined. Each time we see a token we have to search a list of defined tokens and if the token isn't found we can treat it as a string. If we did find it (str for example) we would treat it as a variable (could be a function, number or string).

## Property or string?
```
var = main.c // var = "main.c"
var = str.length // var = 24 (or whatever length str has)
```
The above is how you want dot to work. `file.extension` are often passed to compilers like gcc and you certainly want them to be treated as strings.
`gcc "main.c"` is annoying.
The alternative to `str.length` is `length(str) or len(str) or #len str` but that is rather awful. The dot makes the most sense. You access the length property of the string.
What we can see is that dot should be interpreted differently. If the token preceding dot is a variable then you access a property if the token is undefined then the prefix and suffix of the dot is treated as a whole string.

## Out of scope and functions
A function can be called outside of a scope. Asynchronous functions and global variables are wierd.
```
glob = 1
{
    scoped = 2
    fun = {
        print V: glob, scoped
    }
}
fun
```

## Weak type system and command assumption
There is something wrong with the code below.
```
buffer = "hihi\nhaha"
for chr : buffer {
	if chr=='\n' print hoho
}
```
That's right, for is used for integers not strings. You get ContextError, BC_LESS invalid types... when you do this. Because of the weak types we can't know for sure whether buffer is an integer. We could know but major changes or additional checks need to be made. In general type checking is important to catch these things even if you don't explicitly type `string buffer = "hihi"` and `for int i : 10 {...}`

The code below produces a context error. If you have done the mistake before of using for instead of each then you know context error is thrown. So when you see it that is one of the first things you expect. If you don't know this then how in the world would you know that with `ContextError 12,BC_LESS $10 $11 $12, invalid types REF_NUMBER REF_STRING REF_NUMBER in registers`. Nothing hints at for, each or the list variable being used incorrectly.
```
list = "23 93 28 11 43 61 82 3"
max = 0
for list {
    num = #run tonum #v
	if num > max
		max = num;
}
```
else is treated as a command. You probably forgot to remove else and so a compiler error instead of a runtime error would be better. Once again, assuming command is a bad idea.
```
else if 1 { ... } 
```

## Return values
Executables always return an exit code assuming it is found. It if isn't what is returned? Probably null. The problem is that the context can't deal with null. Let's say print returns null (`Ref(0,0)`) in the case below. `print yes` would try to load the variable but since it's null it fails for various reasons. How to properly deal with this?
```
yes = print "hello"
print yes
```

Code below will be treated as `return` then `a` which isn't caught as undefined but as a command. This is due to attempting parsing since return can be left alone. A sole `return` is the same as `return null`. The second function shows what actually happens. Detecting line feed is a solution but the question is whether there is a more fundamental issue with the language which sneakily allows these things.
```
fun {
	return a
}
fun2 {
	return
	a
}
```

Another issue is this code where when you call the function you don't know if a value will be returned. But an instruction to delete this potential value is necessary. How can you avoid ContextError? You could have an explicity REF_NULL type which doesn't cause an error.
```
fun = { }
fun
```

## Strange syntax
```
func = { return "hello" }
print (#run func;.length)
	-> 5
```

## Changing variables
Memory leaks, crashes?
```
each a : b {
	 a = 9
	 b = 2
}
for i : aha {
	i++
	aha--
}
```

## Runnning function or executables
What we want to happen
```
gcc main.c
print hello
err = gcc main.c // err = 0 (if successful)
err = print hello // err = 1 (if successful)
err = (print hello) // err = 1
err = hello // err = "hello"
```
**Issue 1 (does exe exist?)**
If we don't compile and run the script at the same time we can't possibly know whether gcc exists or not. It may not have existed during compile time but does at runtime. This is more obvious if the script compiles C++ code into an executable which didn't exist at compile time but now does at runtime. The reason we seperate compile and runtime is because we can cache the compiled bytecode so we don't compile it every time (speed is always appreciated).

**Issue 2 ()**
`print` is an internal function. But what if it is an executable we want to run?
`#run print` would run the executable while just `print` would run the internal function.

**Issue 3**
If you want to run the executable `program` you can just type that on a line at it will run. If you at some point decide to define `program = "gcc"` then the variable `program` has would run instead. This can happen without you intending to. In this case the language can be considered to be poorly made since it allows you to make these mistakes. However you can always do `"program"` to ensure that the variable isn't used. So it's fine?

**Solutions**
- Internal functions are prioritized. Defined variables comes next.
- Internal functions cannot be assigned to. `print = 24` is not allowed.
- Tokens without assignment is assumed to be an executable. (unless the token is defined as something else)
- `#run` is just to explicitly run an executable.

## Executable or string?
```
var = hello there // var = "hello" "there"
var = gcc main.c // var = 1 (to indicate success or failure)
```
Undefined tokens like `hello` are treated as strings while `gcc` should be treated as an executable. Previously, we had a list to know whether a token was defined but not in this case since `gcc` is an executable. To know if gcc is an executable we need to search for it in working directory and environment variables.

To improve speed we can cache the result of the search but storing the names of all the files could use up an unnecessary amount of memory. Computers have a lot of memory though so it might be fine.

A potential problem is searching during compile time and not runtime. We may compile a script into bytecode on one computer and run it on another. The path to gcc could be totally different. Now, you will probably always compile and run a script at the same time but when you don't the issue remains. In any case if you use the `cd` command then the files to search will change. The search probably needs to be a bytecode instruction with some  if and jump instructions whether the token (`gcc`) was found or not.

```
// incomplete psuedo instructions
	FIND_EXE "gcc" R0
	JUMP_EQUAL YES R0 1
NO:
	SUBTRACT "gcc" "o" R0
	JUMP END
YES:
	SET R0 "-o"
	APPEND R0 " app.exe"
	RUNEXE "gcc" R0
END:
```

At this point we can conclude that we need a data structure to access defined tokens quickly. Hash map is probably enough. Below is some other data structure. It's a bad one but good for inspiration.
```c++
struct List {...}
struct Map {
	List map[256][256]
	bool find(char* str){
		List& list = map[str[0]][str[1]];
		return list.find(str+2)
	}
}
```

## Odd executable names
```
g++ main.c
```
Honestly, I really don't know what to do here. g++ should compile main.c but `g++` would be tokenized to individual characters (`g`, `+`, `+`). To allow this we may need to change how the tokenizer work. If not then whenever we encounter a string and operator token we have to search for files.
`compiler-64` and `1-sad main.c` needs to be possible as well.

Actually, the tokenzier can do it's job. Instead we can put that extra work on the compiler. The tokenizer gives the compiler `g`, `+` and `+`. The compiler's job is to intepret this. In fact, that is what the compiler does normally. The only difference is that the variable or executable name uses multiple tokens. It makes things harder but that is the compiler's job.

What we do need to do though is somehow let the compiler know that `g++` are tokens not seperated by space. A boolean in the token struct could indicate this.

Another thing is that the tokenizer probably shouldn't output `++` as one token but two seperate. This should be true for all operators and special characters (probably).

```
num+num- 9
C:/app.exe // how do we run this exe :/
```
If `num = 2` the above would become -5 but what if `num+num-` is an executable. Do we prioritize the variable or the executable?
How do we know that `C:/app.exe` is a file path?

```
path/ with/space.exe   // I'm done 

// batch/bash
"path/ with/space.exe"
```
What about spaces in the file name. Batch and bash solves it with quotes. Can we do that? I guess a lonely string can be treated as a file to execute. 

This paragraph shouldn't be here but `game.txt` should be started with the default text editor. I believe Windows does this by default. (move this paragraph to an appropriate location)

A lot of these problems have the same issues and if they are solved we will be in good shape.

**HOW TO SOLVE INTEPRETATION OF g++**
- Pre-define `g++`,`clang++`, `compiler-64` as tokens in compiler.
- Define them in the script like `#define_exe "g++"`.
- `@g++` whenever you want to run an executable or just in special scenarios?
- We can do a pre-search of just `g`. If any token begins with that then we know that `g++` may exist. Then we could do another search with `g+`. If none matched then we know that `g++` doesn't exist. This is kind of dumb since we should just search for `g++` right away.

**How to solve it for real**
Whenever the compiler sees some token with a name and there are operator tokens without space boolean flag after it, the compiler can combine the tokens into `g++` and search it. If it succeded or failed we can handle it as normal. Voilá, problem solved.

**Haha or not we need more details**
`kekw = one two` becomes `kekw = "one two"`.
`one two` same thing here without assignment but how do we know that one isn't an executable. `gcc two` shouldn't become `"gcc two"`.
We could assume the first token (`one` or `gcc` in this case) is an executable unless it's defined as a variable.

**Actual solution**
`gcc main.c` and `one two` is treated as a command in the terminal.
`kekw = gcc main.c` `kekw = one two` is also treated as command in the terminal. The result ends up in `kekw`. If the first token (`gcc` / `one`) isn't a constant string/number or defined variable or function then it is assumed to be an api function or executable.
```
print main
err = print main
"print hey"
kekw = ("ok"+sup)
err = (#cli "print" hey)
err = "sup" "ok"
```

## Undefined tokens and operators
```
var = hell - sup // var = "hell" "-" "there"
var = func -verbose sup // var = func "-verbose" "sup"
var = before + after // var = 5 (before = 3, after = 2)
```
Operators are treated as strings if they can't operate on the previous or next token. +-\*/ generally only work on numbers. If `before` and `after` are number variables then the add instruction would be used.
```
var = str + after // var = "hello2" (str = "hello", after = 2)
var = str - after // var = "hello" "-" 2
```
Since `str` is a string we use a concatenation instruction instead of add.
Subtract operator is treated as a string because we haven't defined what should happen when we subtract a number from a string.

## Arguments and flags
```
var = hell -o there // var = "hell" "-o" "there"
var = gcc -o app.exe // runs gcc as you would expect
```
Is there any problem here? I think they have been covered in previous sections.

## Code and assignment in parantheses
```
var = 1 + (ok = 9) // var = 10, ok = 9
```
Is it allowed? I believe there are cases where you want to write quick and minimal code. The above would be of benefit in those cases.

```
var = 5 + (max 9 3) // var =  14
```
The above where you run code like a `max` function in parantheses should definitely be allowed. 

```
var = (print hey, 9 + 2) // var = 11 (hey is also printed)
```
The above is allowed in C/C++. The only benefits I can think of is asserts `Assert(("Something is wrong",threads>1))` and writing minimal code. I have personally only used the above for asserts. A side question here is how comma should be handled. It is currently not handled at all in any part of the language (2023-03-16).

## bash vs us (minimal code)

```
// Bash, very good
echo *.cpp

// us, not good
files = list *.cpp
for v : files print v \n

// Better, brackets are annoying
for list *.cpp { print #v \n }

for (list *.cpp) print #v \n

// Some seperator is required ; is wierd but this is probably
// the best we can do.
for list *.cpp; print #value \n

// another option?
print (replace " " (list *.cpp) \n)
```

```
// Simple (batch)
echo "large text" > hello.txt

// Good enough? at least more clear?
writefile "large text" hello.txt
```

## Scope or function
`local=2;if 5==5 {print local}` if {...} is a function then how would scopes work? how can functions access variables? if a function is created in scope 3 and it can access the local variables there, sweet. What happens if a global variable is set to refer to the function. does the function still have access to local variable? and if so what happens with the local variable's lifetime?

```
global = 0
if true {
	localvar = 5
	localfunc = {
		print localvar
	}
	localfunc // run the function
	global = localfunc
}
global // <- run previously local function, what happens with local variable? is it null?
```

## Pipes
When running commands like mkdir, gcc you may not want anything printed to the console or you may want the program's output put into a file. You can redirect output of a program using pipes.
```
// Silently run a command using pipes (batch)
mkdir /d hello > nul
// Pipe output to a file
gcc main.c > out.txt
// Pipe output to input of another program
echo y | mkdir hello
```
This is incredibly useful. `>` cannot be used in the new language since it represents greater than but `|` is available. It could serve both purposes of redirecting to stdin of a program and write into a file. Does it append or overwrite the file?

## Repetition
```
//-- Imagine this
gcc a.c
gcc b.c
gcc c.c
...
//-- You want to check error for each and stop compiling if so but you would need something like this
if (gcc a.c) return false
if (gcc b.c) return false
...
//-- A lot of repetition, rarely good
#oneachline if(...) return false
gcc a.c
gcc b.c
...
#stoponeachline
//-- Something like the above would be great. You can imagine the return false part being a function or something else. The ... part or the names for the directives should probably be different.
```


## Other stuff

```
// Check if process is active
tasklist /fi "IMAGENAME eq game.exe" | find /I "game.exe"
if %errorlevel% == 0 (echo Active) else (echo Inactive)

// In new language? note that tasklist can give much more
// information. How would we compete?
found = isProcActive "game.exe"
if found print Yes; else print No
```

```
// how are for and return treated?
list = for hello return

// as strings or would return be an error since it is a keyword?
list = "for" hello "return"
```

```
// This is useful
nums = (for 10 #i) // nums = 0 1 2 ... 9
nums = (0..9)      // nums = 0 1 2 ... 9, should 9 be included?
nums = (5..1)      // nums = 5 4 ... 1
nums = 0 .. 9 OR 0..9  // does the parantheses matter?
nums = a..b    // a a+1 ... b-1 b, IF a<b

// Is this allowed?
nums = (for 10 {print hi; #i}) // print runs 10 times, nums = 0..9

// Is multiple "lines" of code in parantheses a good idea?
nums = (print hi, 5) // this is allowed in C: (printf(""),5)

// What happens here?
nums = for 10 #i // nums = "for" 10 null OR same as previous nums?

// How about this? for acts on 10 and 5 while "+ #i" is left out
//  causing a syntax error?
nums = for 10 5 + #i

// Hmmm...
sum = 0
kekw = for 10 (5 + (sum += #i))
```