# What is the tokenizer for?
- **Input** of text, tokens as **output**.
- Each character is checked one by one where words, quotes, comments and special characters are seperated by each other or white space. `hello+2` is not one whole word but three tokens.
- The tokenzier is very straight forward and rarely changes because of it's simple job.

# Structure of the data
The output consists of an **array of tokens** and an **array of characters** which the tokens point to.
Alternative ways to store tokens:
- Each token could have it's own allocated memory but this is slower. Cache and data oriented design is thrown out of the window.
- The two arrays could be combined into one. You pack a token struct and then the characters for the token. Then you pack another token struct and so on. `(Token*)tokens+index` doesn't work since the token structs aren't packed together. You would need to length of current token to get the next token. You can make a function like `getToken(int index)` to do it for you but it would be better if the data was seperated to begin with.

# Adding tokens
You can append characters a token should have and then add a token which points to the start of where you appended the characters. Execept pointers don't work if the array of character is reallocated. You need offset stuff.

# Extra stuff


## Examples
`hello"kay"sup` -> `hello "kay" sup`

`hm "quote \" in string" WOW`. The backslash + quote results in just a quote in the token.

## Comments
Whatever generator you are using (script, bytecode text) comments will act the same. That is, ignored completly by the generator. You could argue that something like `// @optimize math` would be useful to tell the compiler some stuff. But you don't need comments for it. `@optimize math` or `#optimize math` would work too. Therefore comments can be fully handled by the tokenizer leaving less work for the generator.

## Decimals
`261.921` is treated as one token even though `.` is a special character.
The requirement is that a number preceeds `.`. `.23` or `12 .34` does not count as one decimal.

## Steps
Raw -> Tokens