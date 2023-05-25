Some examples I wrote to figure out the syntax.

print files in cwd with relative paths
```
hello = list -w **.cpp // -w is for whitelisting, good or bad?
for value : hello {
	if value.length > #cwd.length {
		relvalue = value[0:#cwd.length]
		print #index relvalue
	}	
}
```

```
a = 9
print Hey a // prints "Hey 9"
print "Hey"a // prints "Hey9"
print a a // prints "9 9"
print a+a // prints "99", or should it print 18? ("9+9" is the result)
// current implementation doesn't deal with operations when parsing
// functions or commands.
```

```
string = "C:/users/desktop/me/file.txt"
print (findlast / file.txt)
print "(" (findlast / file.txt) ")"
```

```
content = #run readfile hehe.cpp
result = #run preprocessor content
writefile content _hehe.cpp
```

```
latest = 0
for list *.cpp {
	tmp = filemodified #value
	if tmp>latest
		latest = tmp
}
```

```
// Find max value
list = 23 93 28 11 43 61 82 3
sum = 0
for list
	if #value > sum
		sum = #v;

// example of function
findmax = { for #arg0 if #v > sum sum = #v; return }
```

```
str = "struct A { } ; struct B { } ;"
// will turn into "typedef struct {} A; typedef struct {} B;"
nums = findall "struct" str // index numbers are seperated by a space, 0 14
#reverse for index : nums {
	insert "typedef " str index
	index = skip str index // skip typedef
	index = skip str index // skip struct
	name = token str index
	// cut removes text from start to some length
	cut str index name.length
	depth = 0
	while index < str.length {
		chr = str[index]
		if chr == "{"
			depth++
		if chr == "}"{
			depth--
			if depth == 0 {
				insert name str (index+1)
				break
			}				
		}
		index++
	}
}
```

If the language can solve this problem with minimal code then it is a good language.
```
// Problem replace
printf("Hello %s, you have %d %s",name,lives,Type(2,6))
// with
logger << "Hello "<<name<<", you have "<<Type(2,6)<<" lives"

// Solution
text = "printf(\"Hello %s, you have %d %s\", name,
		Type(2,6),\"lives\")"
text0=replace "printf" text "logger"
text1=replace "(" text "<<"
// logger<<"Hello %s, you have %d %s",name,lives,Type(2,6))
text2 = text1[0:text1.length-1]
text3 = topsplit text2 ","
// logger<<"Hello %s, you have %d %s" name lives Type(2,6)
FAIL: kind of need a list and if you don't things become very complex with for loops and keeping track of indices.
```

```
// You have 10 files in which each line looks like
// "Hello 27 19"
// How do you sum up the second "column" of each file?
files = f0.txt f1.txt ...
sum = 0
for files for (readlines #value) sum += (split #value " ")[1]

for 10 {
	files += f #index ".txt"
}
```

```
buffer=
count=10
buffer+="void main(){"
for count buffer+="var"+#i+"="+#i+";"
buffer+="sum="
for count-1 buffer+="var"+#i+" +"
buffer+="var"+#i+";"
buffer+="}"
```

```
err = #run "stuff ok/gcc.exe"
err = "hello"
err = print "hello!"
```

## Bytecode
```
arg0: "-w"
arg1: " **.cpp"
	str $a
	load arg0
	append $a $9
	load arg1
	append $a $9
	
	load wee
	mov $9 $ra
	load func
	run $9 $a
wee:

func:
// $z is the argument
	append $z "??"

	mov $z $rv // $rv is return value
	jump $ra // $ra is return address
```

A script to count lines within a project. Then write that value to README.md. This way you can keep an updated number of how many lines a project has in the readme. Code below is just an example on a way to do it. A lot of memory is moved around making it (probably) very slow.
```
lines = 0
files = listfiles *.cpp *.h
for file : files {
	buffer = readfile file // streaming when reading gigabytes?
	for chr : buffer {
		if chr=="\n"
			lines = lines + 1
	}
}

readme = README.md
content = readfile README.md
string = "Total Lines: "
index = find string content
endindex = find "\n" content index // find "delim" in "str" after "index"
half1 = content[0:index+string.length]
half2 = content[index+string.length:-1]
final = half2 + lines + half2

```

```
fun = {
	for i : 10 {
		print #arg: i
	}
}
#async fun 1
fun 2
```