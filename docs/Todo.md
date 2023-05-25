**THIS IS THE TODO FOR THE FIRST VERSION OF THE LANGUAGE WITH GENPARSER**
**THIS IS NOT UPDATED ANYMORE SINCE GENPARSER IS DEPRECATED. NOTES ABOUT TOKENIZER AND PREPROCESSOR ARE INACCURATE**
## Overall
- [ ] Color coding for instructions, registers, values and addresses.
- [x] addDebugLine should be called when `info.next` is. This means you can't go back a token with `info.index--`. You have to use `info.revert(offset)`. Debug lines should be removed as you revert. **Note**: autmatically doing addDebugLine is less stuff to think about but with `info.next` it sets the debug line a bit offset from where you would want it. SERIOUSLY fix this. Debug lines was so much better placed before. **Note:** Fixed by only calling `info.next` when token IS correct. `info.next` adds debug lines and if you revert than those lines will be incorrect. You can fix it by deleting debug lines when reverting but what you printed to the console cannot be removed.
- [x] `#define DIRECTIVE_SYM "#"` should be defined. It is then easy to change `#` to `@` if wanted.
- [x] Change how debug lines work. Each instruction points to a line. When executing you keep track of what line you are at. When an instruction points to a different line than the current line we print out the new line and move the line index to it. This system uses an extra integer (line index) for each instruction but is easier to work with when executing but also in the generator. **Note:** Instead I made some other changes and so each instructions doesn't need an extra integer. Theoretically, this change should be slower than the other but on the other hand, easier.
- [ ] `for j : undef` where undef isn't defined would cause `unexpected : ParseFlow` which doesn't make sense since `:` is perfectly valid. The parse functions may need to return different errors like `ERR_UNDEFINED` or `ERR_ATTEMPT_FAILED`.
- [ ] The way error works is that if we failed in parse function we abort all parsing until we the first parse function. Initial for, while, assignment, command will be handled independently where errors and instructions don't affect each other except for undefined variables. Then of course will the bytecode not work since  generation of instruction will have stopped abruptly.  The question is how to skip the tokens.
- [ ] Scope isn't dropped during `PARSE_ERROR`. Many places to add `dropScope`. Goto statement to deal with `dropScope` and `return PARSE_ERROR`?
- [ ] Multiple return values? Multiple input variables? One concatenated string is used as argument which isn't good for numbers.
- [ ] When detecting variables, `#i`, `#arg` it is done in `ParseExpression` and `ParseCommand`. Duplicate code isn't good. Whenever a change needs to be made both places needs to change.
- [ ] \_\_debugbreak\_\_ in **Interpreter**?
- [ ] `var = 5 + 2 k \n print hello` This would set var then run k as a command then run print. The k running as a command is not a good thing. You make mistakes like this and may accidently remove or create many files. ParseCommand could only be allowed if explicitly using `#run` or if the previous token has line feed suffix (or if there is no previous token). Another example is `list = 23 93` where 93 is treated as an executable. `list = 23\n 93.exe` may exist. I mean maybe you want to do this. BUT the with the numbers on the same line it's just not good. 
- [x] First few lines can be used to indicate compilation steps. You may only want preprocessor tokenizer or parser. `@only-preprocessor`, `@parser @preprocessor`, `@disable preprocessor`, `@enable parser, optimizer`? Some steps require other steps like all steps require the tokenizer. Does `@parser` use preprocessor by default? If so do you need `@preprocessor off` to disable it?
- [ ] mkdir function doesn't exist. (can use mkdir from cmd.exe though)
- [ ] Import other script files.
- [ ] Make examples using vectors. Show off operator overloading, structs...
- [x] Config arguments to display detailed information about compilation.
- [ ] If you use a macro which causes an error then you would see the result of the macro along the error message. You won't see the original source code where the macro isn't evaluated. Seeing what the macro generates is more helpful.
- [ ] Anonymous underscore variable.

## Major stuff?
- [ ] Can the intepreter and bytecode be leveraged in a way a normal machine instructions cannot? Super instructions like `BC_WRITE_FILE` and `BC_RUN`. These special instructions allow you to do more with less instructions. Ultimately there are machine instruction in the end which makes these special instructions work so performance is lost whatever you do.
- [ ] A generator to convert bytecode into an executable using C and GCC. May be hard since it's hard to identify the functions and if statements.
- [ ] If you have a lot of code and somehow `test` snuck in on one line you are kind of doomed. The reason being that it's caught at runtime. `test` is treated as command and assumes it to be an executable. At least you get a message about `test` not being found and you can do a search in the script file for it so you can find it but it would be nice if you got a better message of where it was. What you could do is use `@enable strict-executables`  or something where you have to be explicit about the executables. Either with `#run` or something else.

## Safety
- [ ] Most safety checks during compile time. Hard with weak typing though.
- [ ] If `text` in `text[5]` is a constant you can do a simple check.
- [ ] During execution, bounds check, memory violation should all be handled and **NO** crashes should occur. The program stops whenever this happens. This is an ideal, not sure how much of I can accomplish.
- [ ] API functions should be checked at compile time. If you misspell one you will find out right away. Same with bytecode functions. With executables the generator will warn you if the executable can't be found. The code will still compile and run since the executable may not exist in the current environment.
- [ ] Memory leaks when executing `BC_NUM` without calling `BC_DEL` afterwards can be detected with some algoritm at compile time. Since there are no circular dependencies this isn't to hard but if you `BC_MOV` then things could become complex. Memory leaks only occur if the parser has a bug in it.
- [ ] APICalls should not modify value argument. The value may be a direct pointer to a variable and changing it would modify the variable itself. Generally a copy is made where you concatenate strings to form the final argument but this isn't always the case with a single variable as argument.
- [ ] Should `BC_RUN` start a cmd.exe process and run the command and arguments in it? dir, date and such would be possible on a single line. Otherwise you would need `cmd.exe /K dir`. An issue with cmd.exe is that `echo non > parser.cpp` would be possible and dangerous. This can't really be detected unless you make code to specifically catch .cpp (annoying with all imaginable edge cases). `cmd.exe /K` you are explicitly saying that you are on your own. A variant would be `cmd dir` where the extra fluff is added automatically unless `cmd.exe` is used. How does this work with bash? You want it to be platform independent. Do you make your own dir, date and time? You can of course call cmd or bash explicitly. Doing it with `cmd` or `bash` might work?

## Management and ownership of memory
Raw text, tokens and bytecode is currently loosely used by functions. They just exist and the functions expect them to exist. It would be better if things were clear with more distinct ownership. `Tokenize` takes raw text and returns `Tokens`. The raw text can now be freed. Then you pass tokens to a generator and get `Bytecode`. `Tokens` can be freed unless you use debug info in which case DebugLines refer to tokens in `Tokens`.
- [ ] DebugLines should refer to startToken and endToken inside `Tokens`.

## Tokenizer
- [ ] Provide a list of errors when finished
- [ ] Should \\n be converted to line feed?
- [ ] Column may be wrong when tab is encountered. Tab is treated as one character but in editors it is treated as 2,3 or 4 (usually). Not sure what to do about this. Enforcing space is on option and assuming tabs are 4 spaces. If tab is found the tokenizer can warn you.
- [x] Optimize by detecting hashtag. If not found then you don't need the preprocessor. If you do find it and it's not in a string or comment then the preprocessor is probably necessary. 
- [x] Handle `//` comments
- [x] Handle `/**/` comments

## Preprocessor
- [x] `#define`
- [x] `#ifdef`, `#else`, `#endif`
- [ ] `#quote`, `@noquote`
- [ ] `#if {expr}` `#if DEF0 && DEF1` instead of C's `#if defined(DEF0)...`. If a macro exists then it is treated as 1 or true.
- [x] `#define Base(A,B) A..B` where `..` combines two tokens. This has to happen when tokens are added to outTokens since inTokens can't be modified. It doesn't have to be `..`. C uses `##` although `..` feels more natural.
- [ ] Test highly recursive definitions. Many vectors are allocated, memory is very inefficient. How slow is it? Is it so slow that you need to fix it right now or after some other language features?
- [ ] Line and column numbers in tokens are messed up after using `#define`. Maybe that's okay because the line and column should represent the tokens position in the original text. With macros, some tokens don't have an original position, just the position they were copied from.
- [x] ifdef with multiple else where each else toggle the section that is active.
- [ ] `#unwrap` works on commas. A directive that works on individual tokens? Same with line feed and space suffix?
- [ ] How does `quoted` and `unwrap` work together.
- [ ] When defining `Rep(X,...) Hey X, Rep(...)` an empty define (`Rep(X)`) could be made if one with that length doesn't exist. It may be common that you want an empty define with recursion so if the preprocessor auto creates one that would be less to type.
- [x] What should happen when you disable preprocessor (`@disable preprocessor`). Should preprocessor not run at all so parser ends up reading `#define Hej` or should they macros and ifdef be intepreted and removed before the parser and any usages of macros will stay as they are. **Note**: The decision I have made is to just not run the `Preprocess` function. This means that the parser will deal with the directives. Letting the preprocessor run and ignoring the directives doesn't make much sense since the code will run differently. With `ifdef` and `else` both sections would work. You otherwise choose a default but who says which one to choose? There are many extra decisions so if you don't want preprocessor, just disable it and don't use any macros or directives. **IF YOU STILL WANT TO DISABLE IT DO IT IN A DIFFERENT WAY.** `@disable` is for completly disabling a layer.
```
#define Test(A,...) A-Test(...)
#define Test(A) A
#define input Hej there, dude what+ is up!
Test(#spread input) -> Hej-there-,-dude-what-+-is-up-!
```
- [ ] Macros should explicitly be called like `add!(1,2)` or `$add(1,2)`. Rust does this it's nice being able to differentiate between normal functions and macros. C/C++ does not do this.

## Parser (rename to BytecodeGenerator or just generator?)
- [x] Math expressions
- [x] Variables
- [x] Calling functions/executables
- [x] If
- [x] While, For, `#i`
- [x] Scoped variables
- [x] substring and property from variables (`str.length`, `var.type`, `str[0:3]`)
- [x] substr and property on constants (`"hm"[0:2]`,  `("ok"+"ha")[1:2]`) 
- [x] break, continue
- [x] Each, `#v`
- [x] const variables? it can replace enums. `@define ENUM_HEY` or some other preprocessor magic could be used otherwise. **Note**: no need for const variables, if you do use a macro.
- [ ] Creating and calling functions defined in the script. (also think about recursion?)
- [ ] Number as argument
- [ ] `DebugLines` doesn't work with `info.nextLine` which is called during errors.
- [ ] Assignment on asynchronous function/executable should not be allowed since it can't return whether it failed. You would need some promise or other way to check failure.
- [ ] Make sure `acc0+regCount` doesn't overflow. **Side note:** checking if it is less than 256 every where with macros is not a good solution.
- [ ] `BC_COPY $10 $10 ; BC_DEL $10 $10` is generated at the end when parsing assignment. This is completly unnecessary. Optimizer could handle it but it's better if the generator does it. Otherwise optimizer has a lot of work to do for each assignment. **Note**: Was it fixed with `PARSE_NO_VALUE`?.
- [ ] Can you somehow specify that you don't want a return value from a function? return value from `gcc main.c` is just deleted and so there isn't a need to make one at all. The optimizer deals with some of it for now but it's better if the generator does it from the beginning.
- [ ] Operator table and symbol table to keep track of types. `string == string` should maybe become a number. `("a"=="a")==1` if the equals between strings return another string then `string == number` will always be false. Unless you use a strict equals (`===`) which do return false and a normal equals where `"1"==1` is true. The issue here is that the language is on it's way to javascript which isn't good. **A bigger issue** is this `(func "yup")==1` where you don't know what the function returns. An operator table doesn't help in this case. You would need a symbol table for each variable and function. Executables always returns a number.
- [ ] If something fails ParseCommand usually pick things up. `i : 10` may be run as a command if you misspelled `for`. To catch this at compile time you can specify the allowed commands. `#allow g++` `#allow gcc`.
- [ ] `#hidden` to stop debug lines and instructions from showing up during execution. Each line could have `bool hidden;` to indicate which set of instructions will and won't show. Hide works on function bodies. Toggle on off?
- [ ] Should if parsing in `ParseFlow` detect else if instead of calling `ParseBody` after else and then detecting if `ParseFlow`. Good because less function calls? Is there a difference in bytecode?
- [ ] Constant evaluation. If an expression consists of constant values, variables and functions then the expression itself becomes a constant. This means that you don't need to evaluate this at runtime. A function is constant if it doesn't have functions like time, random and doesn't call executables.
- [ ] Deterministic functions. A function where some input always give the same output. The result can be cached and if you ever run a deterministic function again you would look in the cache for the deterministic output
- [ ] `each`, `for` are great. But we also need one to iterate characters in a string. Maybe `chars`? `eachchar`.`eachword` instead of `each`?
- [ ] `var < file.txt` if var is not a variable then it should become one. This is another form of assignment. `var > file.txt` here we should detect `>` and assume var is a variable? you sure though? `dir > file.txt` is dir really a variable? `gcc > file.txt` is this really a variable?
- [ ] Some way to only compile a certain part of the script. You may have a script doing various things and you write a function which you want to test. But you don't want to the rest of the code just this function. `ifdef endif` could work but you need put those in the right places. ifdef at the top, endif at the bottom and some else and maybe other's ifs here and there. This takes some time to put in the code and could become very annoying. `@entry funcname` and then some paramaters to it. Only this function will be called. A `BC_RUN` and `BC_JUMP` is placed at the top to run function and then jump to end of program. **Note:** For now, commenting out sections of code works just fine. This is what I  usually do in other languages.
- [ ] Differentiate between `ParseCommand` and `ParseFunction`. Command is for executables which takes in a string. Function use actual arguments seperated by comma. Does paranthesis work?
- [ ] Properly set frame pointer when jumping to a function. Is it done before or after function call?
- [x] Reallocation of string data isn't considered when adding string constants. Pointers of old strings will be invalidated. `finishPointers` at the end would work.
- [ ] `src = #raw bin\he.cpp` where things after raw are parsed as a string like arguments in `ParseCommand`.
- [ ] `func = { each #arg print #v }` <- `#arg` doesn't work in each. Each only works for a variable.
- [x] Parsing extra operators like `+=` and `-=`. Don't forget about not operator
- [ ] Alias for functions. Both external and user functions. `ff = filterfiles` should be possible. I believe `ff = "filterfiles"` might be possible but... yeah.
- [ ] Are variables outside function allowed inside. This is called side effects and certainly has it's uses. The issue is with asynchronous functions. User thread doesn't share value stack in current implementation. If they would they may overwrite each others values. Moving the frame pointer isn't an option since it needs to move when you switch between variables outside and inside. How do you know which offset in which frame you are refering too? You could solely allow global variables with 0 as frame pointer. `STOREV` and `LOADV` would need a bit to know whether it is a global or non global variable they are referring to. 

## Interpreter
- [ ] How to specify whether a new console should be created or to run `gcc` parallel to the bytecode? `#async/#await`?
- [ ] When running a script in the script using `run` the script needs to be compiled. Instead of compiling it every time you run it you can cache the bytecode. If the file was modified since last time it was compiled then it should be compiled again.
- [x] Optimize by not freeing memory when deleting strings. The strings can be kept since there is a high chance they will be used again. **Test performance. There should be an increase.** **Note:** There was a small increase.
- [x] Mark and sweep. May not be necessary since the generator produces code in such a way where any created values are deleted when not used. Not implemented for scoped variables yet since scopes needs to be fixed. **Note**: yup not necessary.
- [ ] Extend the limit of constants with a **constant pointer** like the **frame pointer**.
- [ ] Debug info for the variable names. "loaded at 3", "stored at 4" is hard to understand as a human.
- [x] Check null instruction in switch statement instead of it's own if statement. **Note**: 10% speed improvement (roughly).
- [x] infoValues is used to know which values are used in the arrays of numbers and strings. This is because when we remove a value we don't want to rearrange memory and for that holes will appear. infoValues stores these holes. A better approach would probably be a list of free number and string spots. **Note**: An array of free indices improves speed by 11% (14 ms was saved at the time of testing). I tried with std::vector first which was 11% slower. The improvements came when using engone::Memory (which is really just a malloc and then manipulating the data yourself).
- [x] Debug information when executing. It should be seperate from the codeSegment. An example of debug info print the script line before the bytecode for it.
- [x] Test `makeNumber/String` and `deleteNumber/String` to see if they are doing what they are supposed.
- [ ] Monitoring files. If a file updates a callback is fired. This requires the language to be asynchronous. Registers and callstack needs to exist in some `UserThread` struct. The interpreter will do a round robin on the user threads after a few instructions. Then what about shutting down a thread or waiting for it? `BC_JOIN` would wait on a thread (number in register identifies which), `BC_THREAD` creates a new thread which runs a user function. `BC_WAIT` where a thread waits on another thread until it calls `BC_SIGNAL`. All waiting threads will resume.
- [ ] With asynchronous functions another thread could delete a variable outside the function which the thread running the function uses. Functions aren't deleted after like variables. You can still use them. not good.
- [ ] Calling join on a non thread does wierd things

## Debugging
- [ ] Apart from debug lines you can have regions. Each token belongs to a region (maybe multiple) and each region has a color. Each region could represent a parse function. Any token parsed in `ParseExpression` may belong to a blue region while tokens from `ParseAssignment` becomes green. When generator/compiler is finished you can print out the tokens and regions.
- [ ] Attach messages to the instructions so the interpreter can display them.
- [ ] log output from parser should be dumped into a file.
- [ ] OpenGL and GLFW to make a program for debugging compilation and execution?


## Instructions
- [ ] `BC_LOADNC` and `BC_LOADSC` which makes a new value while loading the const. This saves an instruction.
- [ ] `BC_JUMPC` instead of `BC_LOADC` and `BC_JUMP`.
- [ ] Atomic increment and decrement on register and value in value stack. `BC_INCR`, `BC_INCV`, `BC_DECR`, `BC_DECV`.
- [x] `BC_PUSHC`. `BC_LOADC` + `BC_PUSH`. **NOTE**: won't be using push type instructions for now.
- [x] `BC_RUN0` or something which doesn't return an argument. APICall functions has an extra argument `returnAValue`. If function doesn't handle it then the return value can be deleted automatically if returned reference isn't `{0,0}`. **Note**: it can return REF_NULL.
- [x] Remove `BC_PUSH` and `BC_POP`.
- [x] `BC_MOD` modulus
- [x] `BC_LESSEQ` less or equal, `BC_GREATEREQ` greater or equal
- [x] `BC_STOREV` which sets a variable. Currently `BC_LOADC`, `BC_PUSH` and some instructions to move and modify the stack pointer is required. An instruction to do all this is very useful. `BC_STOREV` can be considered to be the opposite of `BC_LOADV`.

## Executable flags
- [ ] `-dir` to internally change working directory?
- [ ] `runtime -code "for 10 print #i"` to run code without a file.
- [ ] `runtime -cli` the runtime takes over the command line. Works simular to python terminal.

## Optimizer
- [ ] Remove simple redundant instructions. eg. `BC_MOV $a $a`
- [ ] Remove complex redundant instructions like `BC_MOV $a $b` but then you never use `$b`. Instruction that modify registers which don't get used by a function can be removed. Same with variables. Registers only matter if a function uses them. This should be an option though since you may want to see if `a = 1 + 2` does what it is supposed to and not optimize it away.
- [ ] Combine instructions into one. `BC_NUM, BC_LOADC` can be combined into `BC_LOADNC`.
- [ ] Detect instruction which is guarranteed to never run and remove them. Also throw a warning since this could be a bug either in the compiler or for the one who made the script. Instructions which doesn't run could be a `for 0` or `for count` where `count` is guarranteed to be 0.
- [ ] Dividing the code into **basic blocks** would make it easier to optimize because there are less relevant registers and instructions to consider.
- [ ] Instructions that jump to the next instruction can be removed. If the register was loaded with a constant number which refers to the instruction below the jump instruction we can safely remove it. This scenario happens with empty if, else and loop bodies.
- [x] Combine constants **Note** done for numbers, not really necessary for strings. Actually, I did it for strings too.


## Potential bugs
- [x] When coding the execution I haven't thought about `r0==r1==r2`. Since they are references they will overwrite each other which isn't handled. Temporary variables would fix it.
- [ ] `BC_STOREV` may replace a value and if registers point to that value and tries to use it there would be a bug. 
- [ ] Because values are stored in one list, if a value is deleted and a new one is created at the same index, any operation on that index wouldn't throw an error saying "Value was deleted" but rather silently work since a value exists. One way to accomodate this is to choose a random spot in the list instead of reusing old spots. This is probably not cache friendly so instead you can have a variable `furthestSpot` which you use. When spot is at the end you can pick a random empty spot within the list or allocate more spots.

## Bugs
- [x] If a token in a block comment has line feed suffix then that line feed will not exist in the final tokens since the characters/tokens in the comment were removed. That suffix needs to be applied to the token before the comment. 
- [ ] `#define Q(X) X`, `print Q(#unwrap (hello!,9))`. What does `#unwrap` do. Nothing on paranthesis? if so shouldn't it throw an error since unwrap is used for unwrapping other macros?
- [ ] `f = "bin/" + ( #run floor (index / BATCH))+".cpp"` error add invalid ref types number string number. it's like .cpp is being added to the number from floor but the number should be added to bin/ first. then .cpp should be added to that.
- [x] Tokenizer logging is bugged out. Something to do with \\r? **Note**: printf was used instead of log::out

## Major bugs
- [x] A token has a pointer to tokenData. When the data is resized the pointer will become invalid. The pointer should instead hold the offset while tokens are added and data resized and when finished you can combine the base pointer with the offset. `token.str += tokenData.data` for all tokens.
- [ ] Break and continue causes loads of issues with loops. Unforseen scenarios and memory leaks. This if anything should be heavily tested with the test suite.