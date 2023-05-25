# print
`print arg0 arg1`
The arguments are seperated by spaces.
`print   arg0   arg1` is the same as the first example.
`print hello " " "my boy"` is the same as `print "hello my boy"`

A string which has binary data in it like char code 0 5, 211, 254 will be printed as ? or space. Good or bad?

# list (maybe flist)
`list *.cpp` get a list of cpp files in working directory.

# split
`split text delim` split text with delim.
`split "hello yes" " "` becomes `"hello"` `"yes"` but there are no lists?

# reverse
`reverse "a b c"` reverse a strings. `"c b a"`

# filter

# skip
`skip "yes no hm" 2` will return starting index of the next word starting at some index. index `2` is touching yes, `4` is returned since it is the index of first character in no.

# token
`token "hello a yes" 2` will get a token in `"hello a yes"` starting from index `2`. Since index `2` is in the middle of hello, it is skipped. a is the token that is returned.

# tokenat
`tokenat "long list of strings" 2` will get the token at index 2. `of` in this case.

# findall
`findall " " "a b c"` will find all `" "` in `"a b c"` and return a list of indices. `"1 3"` is the result in this case.

# insert
`insert "hi" "uhm,  there" 5` will insert "hi" in "uhm..." at index 5

# filemodified
lastwritten, lastmodified are other potential names.
`filemodified file.txt` returns when the file was last modified in seconds since 1970 (probably 1970).

# replace
`replace delim string other` replace delim in string with other.
`replace " " "hello there dude" "\n"` becomes `hello\nthere\ndude`

# find
`find delim string` is interpreted as **find** first occurance of **delim** in **string** and returns the index where delim starts. number variables are converted to strings.

`findlast delim string` will do the same but in reverse.

# numcast / strcast
`numcast string` or `numcast "152"` will convert the string to a number. null is returned if string can't be converted. strcast works the same way.

# eval
`eval "5 + 7"` will run the code in quotes and return 12.
`eval "print hello"` will run the code in quotes.
A tokenizer and token -> bytecode conversion is required on the string to run the code.

Another options is `hello = funccast "5 + 5"`. just hello would run the function. But what about local variables in this case? Don't do this?


# floor / ceil
?