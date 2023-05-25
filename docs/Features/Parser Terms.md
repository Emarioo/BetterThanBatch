return is used in explanations but replaced may be more appropriate.
# index
`#index` (or `#i`) returns the index of the "closest" running for loop.  With multiple loops you can use `#index0` to get the index of the first loop. `#index1` of the second and so on (or `#i0`, `#i1`).

# value
`#value` (or `#v`) returns the current value of the for loop. `for string {print #value}` instead of `for value : string {print value}`. `#value` is the same as `#index` when looping numbers (`for 10 #i==#v`). `#value0`, `#value1` ... when using multiple loops.

# line
`#line` returns the line number

# file
`#file` returns the file name. (just `script.txt` not `C:/users/desktop/script.txt` )

# breakpoint
`#breakpoint` only used when debugging

# globalize/inherit
`#inherit somescript.script` global variables in `somescript` will be "inherited" by the current script. Maybe you want `#remove somevariable` to delete a variable in the current scope (or above scopes).

# localized/functional
`global=2; func = #functional {global = global+1;print global}`. When you declare functional, a function cannot access global variables. global in the function is computed to `global = null + 1` (since global isn't local). This allows the compiler to optimze when you call the function because it knows that a variable's value isn't shared after function call. If so the compiler can remove the variable without worrying about other variables using that value.

# defer
`#defer print hi`. The code after defer will run at the end of the scope. (see Jonathan Blow's compiler for details)

# function
`#function` functions don't have a unique name. the same function value can be referred to by multiple variables. (Ehm, what should it do?) 

# run
`#run` is just to explicitly run a command/executable.
`gcc.exe` works just fine.

# args / arg0 / argX
`#args` in a function is the arguments passed to the function
`#arg0` is the first argument. `#argX` is the X:th argument.
If `#arg0` is used in global scope then it would be the first argument passed when the script was called.
`runtime.exe script.txt hello yoo` hello could be the first argument or the first arg might be script.txt, not really sure yet. 

# executables / functions / strings / numbers
`#executables, #functions, #strings, #numbers` any of these return a string of all the available variables for the different types executables, functions, strings or numbers.