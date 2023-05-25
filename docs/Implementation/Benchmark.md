Some thoughts about benchmarking.

Compare script against python, C, Javascript (NodeJS).
```
f = {return 2*x + x^2 - 5};
result = ""
for x : 1000 {
	result = result " " (f x)
}
```

Compare language against a build.bat script. What is the difference in simplicity and speed? (how well does the language interract with msvc compiler and how fast can it compile Project Unknown)

# Benchmarking bigger things
Measuring small pieces of code many times gives you some kind of measurement. But a large program may behave differently. Performance may be different as it does more complex things. More unexpected IO, wider variety of instructions, more allocated numbers and strings at once. With a large program you can test the limit of the parser and intepreter. How many numbers and strings can you have before things break. How much can the tokenizer, preprocessor and parser handle.

A large program is clearly very useful to test the language with. But making a large program is difficult. Mostly because it takes time away from working on features for the language but also because you need an idea which fits into a large program. A game is one such thing but the language isn't made to make games with (you can make inefficient ones if you create some external functions for rendering).

The language is made to be simple and useful for smaller programs.

# Results
## Laptop - 2023-03-31
Test with `TestVariableLimit(10000)`. Most of the time, compilation took 0.053 seconds and execution 0.83 seconds. Idle CPU usage was roughly 6% so other programs did probably not effect the time to much.