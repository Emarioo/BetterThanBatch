This is the initial document where if figured out what the language should have been.
Note that the other markdown files are very unorganized. I have not bothered cleaning it up since this version of the language is abandoned.

**What does it take to make a good scripting language?**
**What is a good scripting language?**
**How do you run a scripting language?**

**NOTE: When reading docs and thoughts the syntax may be old. In those cases focus on the meaning behind the syntax instead.** 
## What should a scripting language be?
Scripting language in this scenario refers to languages like Batch, Bash with a little bit of Javascript.

- A scripting language is different from C, Java, C++ in that it is minimal, easy to use and straight forward. Batch is neither of these. It is just a pain. You should be happy and satisifed about how simple it is to rename many files or get a list of files in a folder. You don't need to be a fully fledged programmer to do some minimal scripting.

- Bash is pretty good. If the new language is worse than bash in terms of convenience then the project has failed. If the language has some missing features then that's fine (I can't implement everything). So the good features of bash should be implemented in terms of how little code and complexity you need for simple things. Another thing is if bash does something well in terms of small code but at the cost of confusion then the new language should use a little more code but be very clear and certain about what is going on. Small code at the expense of clearness is bad (usually).

- If you are doing complex things and would greatly benefit from some odd syntax then the language should provide that. But it must also provide a clearer alternative. 

- Scripting is used to automate an action which a user wants to do many times.

- Scripting should have good support when moving, copying, reading and writing files (also piping). It should also be able to run executables and other scripts. Running executables should be simple. The syntax should allow the common `gcc main.c -o main.exe`.
`files = "main.c"`
`gcc "main.c"`

- If you are editing a file maybe .txt or markdown and you need to do some repetitive stuff this language comes in handy. Get the path of the file, write a little script which writes to the file, run it and you have saved yourself a lot of time. This isn't unique or anything. You can write a quick little script in python too.

# Why is batch bad
```
rem New line
set nl=^


echo Wow^%nl%


rem modulus
set /a hm=19%%3
set /a x=19%3 <- does not work! setlocal /? may have explanation

set min=%time:~3,2%  <- set tim to 08 if time is 14:08:32

you want to perform math on the minute?

set /a he=%min% + 1  <- this seems fine right?

noo 08 is treated as an octal, 8 is invalid

set /a he=(100%min% % 100) + 1 <- this is what you have to do

CODE BELOW is wierd, This is an attempt at aquiring source includes from a directory. But variables has a limit. Scripting language will NOT have this limit. Instead of pouring includes into a variable you would do it to a file (maybe preferable anyway though).
SET files=
for /r %%i in (*.cpp) do (
    SET file=%%i
    if not "x!file:reactphysics3d\src=!"=="x!file!" (
        SET files=!files! #include ^"!file:!^"^%NL%

  
    )
)
```

# Is bash good
Yes, it is very good and many people like it. But there must be some bad parts about. We should fix those.

# Garbage collection
You don't want to deal with memory when scripting. Since there are no objects this is rather simple. If a value, variable is out of scope then it is deleted. In any case, mark and sweep algorithm is rather easy to implement (in it's simplest form).

Now the way you make memory is by creating variables `num = 19` or `str = "hello"`. Calling `#free num/src` is not a big deal. So you could have an option where you can disable garbage collection. The reason you want to is because the algorithms for garbage collection can be unpredictable. Probably not a big deal in this case. This is more of an issue with languages like Javascript. 

# Running a script
Tokenization -> ByteCode -> Optimizer (optional) -> Runtime
You could interpret text and run it right away but compiling it into efficient instructions would be faster. Having these steps should make things easier. Instead of one spidernet of complexity you have 4 smaller modules which solves a more specific task.

**Scripting language (.btb)**
**Bytecode instructions (.btbi)**

## Tokenization (phase 1)
The script is turned into tokens. For, if, strings, numbers, commands. This becomes a list of tokens. The reason why this step is useful is because however the bytecode is calculated it will always in one way or another identify strings and depending on what they contain transform it into bytecode. If you skip this step and make a change in the script -> bytecode conversion then you would need to reimplement the string identification.

## Bytecode generation (phase 2)
Tokens are interpreted and turned into bytecode. Constant number, variable names and string could be put into a static data segment in the final bytecode. The actual code is put into the text/code segment.

## Execution (phase 3)
The bytecode is executed. Tokenizer and bytecode parser can be run during execution because of eval.

## Optimization (phase 4)
Optimize bytecode making it faster and smaller. Efficiency I say!

You can do each step independently by running
`compile.exe script.txt -tokens -o script.tks`
`compile.exe script.tks -bytecode -o script.btc`
`compile.exe script.btc -execute -o ?`
-o is optional
