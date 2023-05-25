Here are some random thoughts

# Compiler
You may want to interact with the different phases. A neat way to do this is have a special character for each phase. If you want the tokenizer to do something out of the ordinary you can use @option which the tokenizer will recognize and do something special. The tokenizer would not pass @option to the next phase. The generator would detect \#option and do something special. The execution phase just runs bytecode.

With errors, it would be useful to know in which file they occured. To do this the text data and the file path has to move through the phases together. In what way to you do that? A script struct?

**Convenience**
You have `script.txt`.
You can compile and run it with `compiler.exe script.txt`.
You can also just compile with `compiler.exe -c script.txt` which will give you `script.btc` (script.bytecode). You can run it with `compiler.exe script.btc`.
If the bytecode script has a different extension you can use `compiler.exe -assume=btc script.txt`.
If you update the script semi-frequently and only want to compile when it's new you can use `compiler.exe -cache script.btc`.

**Cool ideas?**
`compiler -mode=tokenize,compile,runtime "text.btc"="text.tokens"="text.txt" text.btc=text.tokens -when=always,modified`

**Syntax tree?**
There is currently no syntax tree. You never specify a type, there is no (or very minimal )type checking and the language is simple overall. A syntax tree isn't necessary with language's current state. This may change though.

# Tokenizer
Map tokens to integers? `TOK_IF`. This saves some space. Instead of storing `while` in a token you store `#define TOK_WHILE 0x10`. How much space do you really save? If you have many keywords with long names you save a lot. But in this language there are few and small keywords. Most tokens are variables, integers or special characters. Mapping variables to identifiers should not be done in the tokenizer. You rarely use integers so having integers in string format doesn't do much. Special characters only consist of one character. Directives do have names you can turn into integer identifiers so you may save some there but directives aren't very common either.
**Ultimately** mapping tokens to integer identifiers would add unnecessary complexity to the tokenizer and all things considered, tokens as strings work just fine.

Number, string and function type exists. Each type has a dynamic list of all the values. String has a heap allocator for the contents of the string. The lists may exist in each scope. What if their value should exist outside of the scope? Function probably needs some allocater too.

```c++
enum Type{
	NUMBER,
	STRING,
};
struct Number{
	double num;
};
struct String{
	uint32 length;
	// uint16 allows for 65536 characters which is too small. uint32 should be used
	uint32 maxLength;
	char* string;
};
struct Function {
	int functype; // indicates executable, c++ function or bytecode type.
	uint32 length;
	uint32 maxlength;
	char bytecode[];	
};
```

functions, commands can return null and so variables can be null too.

# Reading/Writing files
`readfile text.txt` this would return a string with the contents of the file.
`readfile text.txt 500 100` would start from 500 and read the next 100 characters and return a string. Each readfile would open, read and close the file. This can be improved if during execution the program can see bytecode in advance and recognize readfile instructions.

`writefile text.txt "string or variable"` example of writing

What if you do `readfile text.exe` this would return binary data. For and many other functions doesn't work well with these.

`writefile text.exe ((readfile text0.exe) + (readfile text1.exe))` will combine the contents of text0 with text1 and write it to text.exe.

# Stack/heap/static overhaul
How do we change current design to a better one? registers work more like a stack. ValueStack is more like global variables. This is really odd and has problems.

First of what data do we need and where and how do we store it. Well the normal layout.
**Static section**: constants and global variables
**Stack section**: local variables, function frames
**Heap section**: user structures, dynamic allocations like strings
**Code section**: the bytecode but we already have this

What this means is that we move away from number and string storage which we had before. The registers are special made for that type of storage and won't work with the new system. Same with all instructions which use these registers.

What do we change it too? Very similar to x86 if not the same.

What types of instructions to we need?
`BC_MOV`: copy memory to register, register to memory or register to register.
`BC_ADD`: Arithmetic, bitwise and logic operation on registers.
`BC_CALL`: Jump to address while dealing with stack and frame pointers
`BC_JUMP`: Normal and conditional jump. jump if equal, jump less, jump not equal and such.
`BC_PUSH`: Push value in register to the stack
`BC_POP`: Pop value on stack to register

How big are the instructions? They vary some that uses addresses are bigger.
If we follow x86_64 there would be a-d for low and high (8 bit). 8 registers. ax-dx (16 bit), 12 registers eax-edx (32 bit), 16 registers. Then there are stack and frame pointer registers and some others so we need more than 16 registers (4 bits), so 5 bits can represent 32 registers and that is probably enough.

Since we have different types of instructions which needs different information and a different layout we would begin with an opcode. 5 bits gives us 32 types.

## Arithmetic instruction
3 registers, 15 bits. opcode, 20 bits. 4 bits for type (add,sub...)
{opcode|type|reg0|reg1|reg2} = 24 bits

float and non float?

ADD,SUB,MUL,DIV,MOD,
OR,AND,XOR,NOT,NEGATE,
ShiftLEft,SHIFTright,

## Move instruction
register to register : {opcode|type|reg0|reg1}
memory to register : {opcode|type|reg0|reg1}
register to memory : {opcode|type|reg0|reg1}

## Stack stuff
push : {opcode|type|reg0} increment stack pointer
pop : {opcode|type|reg0}

## Jump
jump : {opcode|type|reg0}
jumpeq : {opcode|type|reg0|reg1|reg2}

## Call
call : {opcode|type|reg0}
ret : {opcode|type|reg0} // return address on stack?

`BC_CALLEXT`: Special instruction to call external functions
`BC_CALLEXE`: Special instruction to call executable

Other instructions, `BC_THREAD`, `BC_JOIN`, `BC_LOAD_IMMEDIATE`,`BC_MEMCPY`

# Piping
Shell languages can use pipes. `ls > out.txt`
This is very useful. It's not a proper shell language without this.
How to do it in this language?
The syntax with `<>` is good but may be recognized as less and greater comparisons. As long as they aren't used in an expression it should be fine?
`CreatePipe` can be used from Windows to do pipes. Not sure how Linux does it.
Usage of pipes
`cat < file.txt` content in filename is routed to stdin of cmd 
`date > file.txt` stdout of cmd is routed to filename
`date | cat` stdout of left command is stdin of right command
`date | cat < file.txt` here,`date |` is ignored,  `< file.txt` overwrites?

`variable | file.txt`
`file.txt < variable` is not allowed.

# Name for the language
**BTB** : Better than batch
SBTB : Significantly better than batch

# Testing compiler
Some algorithm to spit out garbage code and test it against the compiler?
`for 0..1000 { buffer[i] = RandomChar() }` generate many random characters and let the tokenizer fail because of bad characters.

`for 0..1000 { buffer[i] = RandomValidChar() }`
the compiler should fail here.

`for 0..1000 { token[i] = RandomValidString/Char()}`
This is more sophisticated. random variable names, {, ), +, 9, 2 are generated. Interesting to see how the compiler performs. You could go beyond and make some machine learning algorithm to generate useless code. Give it a set of strings to choose from and it will learn the the compiler complains.

# Expression evalutes to something
`let hey = {println!("hey"); 92}` is possible in Rust where `hey` becomes `92`. This allows you to do this with macros `let hey = macro!(1,2)` where `#define macro(...) { other stuff}`. Macros can't be used in assignment if curly brackets aren't evaluated in this way.

```
#define vec_rec(X...) temp.push(X); vec_rec(...)
#define vec_rec(X)
#multidefine vec(...) {
    let mut temp = Vec::new();
    vec_rec(...)
    temp
}
#enddef
  
#define vec_rec(X...) temp.push(X); vec_rec(...)
#define vec_rec(X)
#multidefine map(...) {
    let mut temp = Vec::new();
    vec_rec(...)
    temp
}
#enddef
```


# Other stuff
- `var = "hello"` is compiled faster than `var = hello` because the compiler doesn't have to search for the variable hello before realizing that it doesn't exist. With `"hello"` the compiler knows instantly that it should create a string.

```
scopeval = {
	print scoped code
	return "returned from scope"
}
print scopeval

func = (arg0, arg1) {
	print a function
	return arg0 + arg1
}
func = () { print another function }
typedfunc = (int ok, float yeah) -> double { print indeed }
```

If bitwise operations are allowed and types exist. Then a bool is a char. If negative you have false. If positive you have true. Most significant bit is checked. This allows you to encode error messages. In C you can't since there only is one false value (only zero). Down sides to negative being false?

If classes or structs exist then operator and cast overloading is a must. I like C++ operator overloading because you can do a lot. Since you can do a lot there are things you probably shouldn't do like \* as dot product between math vectors. With powerful overloading there is a responsibility and code becomes awful if you don't take it seriously but it is sad for people who know how to utilitze these features to their utmost capabilities.
Example of cast overloading. This is good because when using TokenRef you can write `info.get(tokenRef)` instead of `info.get(tokenRef.index)`. Less typing easier to read since tokenRef really is just index. The flags part is just extra info you may occasionally want. Some may say you will forget what TokenRef is and use it as an int and somewhere along the way you will do `TokenRef ref = (int)tokenRef` and the flags part will be lost. Very true but as I said before. You have the responsiblity of knowing when these features are good and bad. Someone who knows what they are doing will only use this feature in situations where issues can be avoided and if there is a chance to forget or make such a mistake that someone will not use the feature at all (unfortunately you may think you know but you actually don't so you mess up anyway). The amount of programmers who don't know this may be greater than those who do but I kind of want to help good programmers and make their and my life easier or at least more comfortable. **In any case I just really enjoy overloading even if it bites you in the end. I haven't seen the extreme cases where it bites really hard so I might change my mind when I experience it**.
```c++
struct TokenRef{
    uint16 index=0;
    uint16 flags=0;
    operator int() { return index; };
};
```

# Multithreading
Parallel processing needs to be possible as well as some way to wait for a process to finish. Something like this.
```
wait_obj = #async gcc main.cpp
print waiting
#await wait_obj

// batch can do parallel processing
start /b gcc main.cpp
// you can use tasklist to know if a process is running and then timeout for a minimum of 1 second but it may only take 0.4s so you waste 0.6s. This heavily affects compilation speed in a build.  
```
Syntax should probably be different. wait_obj would be some new type like thread or process handle. This would complicate the language with more types. You do need a process handle in the implementation but it would be nice to hide it in a neat way? Having one Handle type for all types of handles might be nice? Thread, process, mutex, file handle?

You can speed up running scripts with multithreading. Multithreading introduces deadlocks and mutexes to solve them. Therefore, it may be hard to implement in a good way. You want the scripting language to be mostly safe where you don't need to worry about deadlocks and data races to much.
```
func = { process things }
isdone = #async func arg0
```

`#async` can be used to run a function in a seperate thread.

If a thread is modifying global variables while another does the same then you will have unexpected results. You need to limit a threads access to variables. You can say that threads cannot access global variables unless another thread gives them access. Not sure how the syntax would look like. You may need more types. String and number may not be enough. A thing to note is that there is no reason to have more threads than the computer can simultaneously run. Doing things in parallel with the same speed as iteratively has no benefit (not in this language probably).

You could stop threads from accessing global variables and only allow them to work on the arguments. The question is how the thread would give back that data.

Multithreading in the runtime introduces complexity with deadlocks and mutexes. Therefore, the one programmer doing scripting cannot create threads but they could maybe speed up the script if it has multiple scripts in it. `run -thread script.txt` could be run in a new thread. Or you could run a function in a new thread like javascript (async, await). But how do you deal with global variables? Some intense functions may be multithreaded. Having a thread pool could be a good idea in that case to prevent the overhead of creating and destroying threads everytime you call the function.

User threads for user functions but what about executables and external functions.
They need kernel threads. The interpreter may have a list of extra threads that are running. They can be mapped to user threads and once they finish they will mark their user thread as finished. Join should work as normal. To make it easier, a function like `runThread(funcname,arg,userThreadIndex)` should deal with the complexity.

# How to do each?
Each for strings can be made with the language as shown below. We can convert the code into raw instructions and let the parser generate those. Luckily we don't need to add any new instructions.
Calling an external C++ function would be faster. But you would need to pass in the list string, the last index you where at and you would need to get either a substring and the new last index or two numbers indicating the start and end of the substring (the end number also indicates the last index). **Multiple return values** is required. You can return a string which has these combined but we need to move forward with a long term solution.
```
begin = 0
i = 0
while i < list.length {
    chr = list[i]
    if chr==" " || i==list.length-1 {
        if(i-begin>0){
            str = list[begin:i-1]
            //-- Body of each 
            print ("\"" + str + "\"")
            //-- end of body
        }
        begin = i+1
    }
    i = i + 1
}
```

Here we arrange`i=i+1`. May decrease the amount of instructions.
```
begin = 0
i = 0
while i < list.length {
    chr = list[i]
    i = i + 1
    if chr==" " || i==list.length {
        if(i-1-begin>0){
            str = list[begin:i-2]
            //-- Body of each 
            print ("\"" + str + "\"")
            //-- end of body
        }
        begin = i
    }
}
```

```
begin = 0
end = 0
i = 0
while i < list.length {
    chr = list[i]
    i = i + 1

    if chr==" " || chr=="\n" {
        end = i-2
    } else if i == list.length {
        end = i-1
    } else {
        continue
    }

    if end-begin+1>0 {
        str = list[begin:end]
        print ("\""+str+"\"")
    }
    begin = i
}
```

```
each list {body}
each v : list {body}
```

# Parsing complex cases of define
- Recursive solution is easier than iterative.
- One function to parse macros. ParseMacro. Can return BAD_ATTEMPT if macro doesn't exist.

## Where does recursion begin?
- **Find root macro.**
- **Evaluate arguments** following the macro name into `Arguments`.
- Evaluation consists of adding token indices to `Arguments` but we also try the first step (**Find root macro**) and if successful we would have another macro being evaluated. This is why we do recursion and not an iterative solution.
- **Match argument length** resulting in `CertainMacro`. Then evaluate and append the tokens of the macro to `outTokens`.
- Tokens in macro that match with the macro's `argumentNames` will instead use appropriate `ArgToken` from `Arguments` (based on argumentIndex and such...).
- Tokens that match with root macro will perform the first step.


This takes care of parsing but not evaluation and output making it completly useless.
```
<base> ::= <macro> | <macro> ( <args> )
<macro> ::= string
<args> ::= <tokens> | <tokens> , <args>
<tokens> ::= <token> | <token> <tokens>
<token> ::= <base> | <token>
```


## Generator (text bytecode)
- [ ] ; should mark the end of an instruction like \\n does.
- [ ] At the end of the file \\n does not exist. If you require it when finishing an instruction at the last line then an error will occur saying unexpected end of file. The fact that this happens when coding the generator is not good. This case will be forgotten many times. Luckily, you receive an error but how do you deal with it so it doesn't happen at all.
- [ ] `copy` and `mov` has important distinctions. mov copies a register's reference to a value. Meaning, both registers point to the same value. It is important to know this since you may modifiy the copied register and affect the original without you intending to. Use copy to duplicate the value. The question is, do you really need mov? or should mov just be copy?
- [x] Constants
- [x] Instructions/mapping of them and appropriate registers for each one
- [x] Jump addresses
- [x] Log error for each line (not limited to one error when compiling)
- [x] Allow instructions to access constants before they are defined
- [x] `run` should work on files in working directory and on files in environment variables. If jump register is a string then it assumes it to be a file to run.
