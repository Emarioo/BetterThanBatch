Basic idea. A few parse functions. Body, Assignment, Expression, Flow, Command.

**Body** is where parsing begins. This function handles the "lines of code" or the body of a function, if or while loop.

**Flow** parses code that changes the flow of the code such as if, while and other conditional keywords.

**Expression** is parsing which evaluates math with numbers, functions and variables.

**Assignment** parses variable on the left and expression on the right of an equals sign. 

**Command** is used with executables or functions where arguments are implicitly concatenated with +.

Most parsing functions can lead to other parsing functions (**Body** leads to **Flow**, **Assignment** or **Command**). Since the syntax isn't explicit with keywords about which function to call the function first tests to see if the following tokens CAN be of the certain parse type. **Assignment** requires = as the second next token and variable name as the next (there is more logic to make it work in practise).

# String manipulation
Assume `string` is a string variable
`string.length`
`string[5]` (get character at)
`string[0:5]` (substring) is 5 the length or the index position?
`string + "hello"` appends "hello" to string. if string is not a variable then the result would be "stringhello"
some sort of crash happens if you do out of bounds.

Quotes represent strings.
`"hello".length` is valid so is the other types of string manipulation.
Quotes are interpreted as one token. use \\\" inside quotes to get a quote in the actual string.

# Variables
`print hello` hello is treated as "hello"
`hello = goodbye` the variable hello is set to the string goodbye
`print hello` goodbye is printed because hello is a variable
print = "hey" will fail because print is a function

multiple return values?
`a, b = div 5 2` 5/2=2.5, a is 2, b is 1 

# Type
`hello.type == "number"` to check if hello is of type string.
`"agh".type == "string"` to check string
`print.type == "function"` to check function
`sour.type == "null"` sour is not a defined variable so .type is therefore null.

# new line and ;
`print hey\n print me` will print "hey" on the first line and "printme" on second line.
`print hey; print me` will print "heyme" on the same line.
; is used to seperate/end commands. ";" if you want to print the actual character.
`print "hey you"\n;print "hey me"` will print "hey you" on first line, "hey me" on second line.

# Math / boolean expressions
These are as you would expect in any language.
hello + 6 < 10    2 && hello    2/5\==hello^2    2\*(x+2^5)/9\*x
hello || (yes && chicken)
yes.type == null

bitwise operators does not exist.

# Nested code
assume there is function `add 5 3` which returns the result.
`hello = (add 5 3)` is the same as `hello = add 5 3`. hello is set to 8.
`add add 1 2 3` will throw an error since the second add isn't a number.
`add (add 1 2) 3` is valid and returns 6.
`print (5+5)` prints 10.
`print "(5+5)"` prints (5+5).
`print ( print hello; findlast h hello )`

# Scopes
```
global = 9
{
	local = 23
	print global local
}
print local
```
brackets in this case specify a scope. the last print will print null since local is undefined out of the scope. A scope is created if it's alone.

# Function
**First how to call them**
`print hello` prints hello to the console.
A string without assignment (not `var = print hello`) will be interpreted as a function/

`name = { print #arg0; print #arg1; print #args; return 25 }`
function will return 25. `{...; 25 }`
`name ey wee` is how you call the function. `name` without any arguments works too.

`{print (#arg0 + #arg1)}` is a scope.
`print name`  if name is function, the bytecode is turned into a string and printed. is the original code printed or the actual bytecode? The bytecode is probably big, more interested in the code.