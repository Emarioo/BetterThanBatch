Manually testing code to see if the parser and intepreter works as expected is an awful way to do it. Automatic tests (a.k.a. unit tests) is much better.

# How
The language has features like loops, assignment and functions which you want to test. You write scripts for each feature while including edge cases and possibly ambiguous or strange behaviour. These scripts will be tested by the C++ for the unit testing.

Is testing done in code or in shell?
`parser < sumtest.script > out.txt` and then `diff sumtest.out out.txt` where sumtest.out is the expected output.
Or do you run `parser -test=all` which internally calls a function which does all testing?

Testing using C++ code allows for more control and options. Using diff is very limited and you get a very specific comparision. In code you can test bytecode, tokens individually.

# How do you know if a test succeeded?
You expect certain output from a certain script. The test fails if the output is different.

Can we do tests in a way which ensures that features work. **No**, human error. Ensuring features requires knowing all edge cases which isn't possible. Even if you could know, a human could forget one.

Test cases are there to check common problems. Some problems may still be missed but there is a high chance the compiler works as expected if it passes the tests.

How can you expect output? Look at bytecode, tokens, executed instructions? how does APICalls make this harder?

Bytecode can change with each version so we can't compare it directly. We could however expect certain instructions.

What output can we expect from `sum = 1`? 
- **Instructions** must have `BC_STOREV` and `BC_LOADC`. (only one of each)
- The only **constant** is `1`.
- **Tokens** will be `sum`, `=`, `1`

The above expectations still don't ensure that the code works. What does it mean to say code works when we have just one assignment?

`_test_ sum` is converted to `BC_RUN _test_ sum`. This is handled differently. The value of sum at the time of execution is stored in a list of test values. At the end you can compare the values against expected values.

If there are some tests that fail and we make a change it may be useful to know if the same tests fail or if they are different. The number of tests is one way to know this but otherwise you could generate a unique hexidecimal string based on the failed tests. The same string is generated if the same tests fail. If a different test fails then a different string is made. Knowing how many tests failed is probably enough. A unique string would be going overboard.

## How do we fully test macros?
What examples push the macros to the limit?
Macros has a few features. Test them individually then combined?
`#define A 1,\n2i` what can go wrong without arguments?