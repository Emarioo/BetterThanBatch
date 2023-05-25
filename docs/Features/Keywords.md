# for
For consists of one or two expressions and then a body.
`for [expr] {body}`
`for [expr] : [expr] {body}`
An expression in this case can be a **number**, **variable** or a **variable** with an assignment. The right side of assignment is parsed by `ParseExpression`. 

Increment of 1 is done automatically can cannot be changed. Use **while** if you need special incrementation.

## Range from 0 to ?
**Number** in the expression causes the loop to run for that amount of times.
`for 10 print #i` prints 0 to 9.

**Variable** in expression makes the loop run for whatever number the variable has.
`num=5; for num print #i` 0 to 4 is printed.
`for num=5 print #i` 0 to 4 is printed.

## Range from ? to ?
Simular concept as before but with a `:` to seperate start and end of the range.
The left side specifies the start and the right side specifies the end.

`for hey : 3 print hey` 0 to 3 is printed.
`for hey=1 : 3 print hey` 1 to 3 is printed.
`for hey=2 : max=7 print hey` 2 to 6 is printed.


## Reverse
`#reverse` Can be used to increment by -1.
`#reverse for 10 print #i` prints 9-0

`#reverse for ab=7 : 3 print ab` prints 7-3

# each
## For each substring
`each [special expr] {body}`
If the special expression is a string then the string is split by space and each substring is set as the value.
`each "hej sup" print #v` prints hej then sup
`list="hej sup"`
`each list print #v` prints hej then sup
`each str = "hej sup" print #v` prints hej then sup
`each str : list print str` prints hej then sup
`each str : strs = "hej sup" print str` prints hej then sup

```
each "hey ok sup"
	print #v
```


# if
`if [expression] {body}` as you would expect.
`if [expr] {body} else {body}`

```
if yes
	print ha
else
	print no
```

```
if yes
	print ha;
else
	haa
```
```
test=0
if test!=0
    if test==1
        print yes
    else
        print no
else
    print certain no
```
; marks end of a single line if. else is bad syntax
# while
`while [expression] {body}` expression can be a variable or an expression like i<10.

# break/continue
Normal functionality in loops but also allow breaking out of multiple loops.
`break 2` would break 2 times instead of one.

# true / false / null
true=1, false=0, null=`Ref(0,0)`