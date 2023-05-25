(Bytecode generator)

**Steps**
- Read tokens
- Put variable names in a map (based on scope)
- Put constants (numbers and strings) in the data segment.
- Produce bytecode based on expressions

# Thoughts
Instead of ParseAssignment trying to see if `name =` exists you can have a MatchAssignment. But you might as well just have one function I guess to no match?

White space is mostly ignored. You can do a math expression on multiple lines. Functions is the only exception. ParseCommand checks for arguments. It stops when line feed is reached. ParseCommand does not stop if it runs within an paranthesis. The end parenthesis will mark end of arguments.

In ParseExpression and # is encountered you do
```
if (token == "#"){
	if(exprInfo.regCount == 0){
		if(token.flags&TOKEN_SUFFIX_SPACE){
			ERRTOK << "expected a directive\n";
			return 0;
		}
		token = info.next();
		if(token == "run"){
			ParseCommand()
		}else{
			ERRTOK << "undefined directive\n";
		}
	}else{
		ERRTOK << "directive not allowed\n";
	}
}
```
This is code makes run directive work.
Then you need normal function calls. This is done by identifying function definition instead of variables.

## Variables
When executing there is a list of numbers and strings. These are the values you can operate on. Registers refer to those values. Instructions operates on the values in the registers.

A variable refers to a value. When and what value? Are values scoped?

How can the interpreter be leveraged in a way the CPU can't?

## Asynchronous functions
UserThreads...

How do we access variables in bytecode?
Variables point to values like registers. The difference is that variables have a human readable name and you can have many of them. Registers have a limit (256 at the time of writing this).

### Problem: Allow many variables
Let's say you have 300 variables in a scope. 256 is not enough registers for it. What do we do? We can consider the problem solved if we can create and refer to 300 values.
```
var1 = 1
var2 = 2
...
var300 = 300
sum = var1 + var2 + ... + var300
```

- You don't need to refer to all values all the time. There is a "window" of values you refer to and you can move this window to change what you can refer to. Even if the total referred values can be large the window is still limited by the register limit. The code above cannot be solved with this method.
- In other languages you have a stack which you can push and pop to. There you can store and refer to many values.

**Yeah... solution is definitely a stack**
```
// loadc = load constant, loadv = load value
// $sp = stack pointer, $fp = frame pointer (used by push/loadv)
loadc $sp 0
loadc $fp 0
num $a
loadc $a 1
push $a // index of value in register is stored in stack
num $a
loadc $a 2
push $a
...
num $a
loadc $a 300
push $a

num $a
loadv $b 0
add $a $b $a
loadv $b 1
add $a $b $a
...
loadv $b 299
add $a $b $a
```

```
str = ""
for i : 10 {
	str += i
}
// str = "0123456789"
```