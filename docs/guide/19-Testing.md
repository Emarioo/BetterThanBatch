The language/compiler has a builtin testing feature, mainly meant for testing the compiler while relying on as few features as possible. It can be used to selectively run tests from a directory, file or individual tests within a file.

There is one flaw which is that failed and succeded tests are printed to stdout which means that you can't use stdout for other things. We may use stderr in the future or write to a temporary file `temp_test.txt`.

You can always develop your own testing environment if the testing system doesn't suit your needs.

# The basics
**WARNING**: The syntax and behaviour is not set in stone and may change in the future!

First of all, this is what you type in the terminal to tell the compiler to run tests.
```sh
btb --test your_file.btb

# Wildcard is also allowed, see 'btb --help' for more information
btb --test tests/*
```

There are two key components to testing. Specifying a test case and tests to perform within the case as seen below.
```c++
@TEST_CASE(my_first_test)             // <- The test case

_test 1; 1; // will always succeed       <- The tests
_test 1; 0; // will always fail

// _test takes two expressions that evaluates to a 64-bit
// value on a 64-bit machine.
_test expr_a; expr_b;

// Yes, separating the expressions with semi-colon is a little strange. Syntax is work in progress.
```

Here is an example with two test cases.
```c++
@TEST_CASE(test_func)

expectations: i32[] { 2, 7, 12, 17 }
for expectations {
    expected := it
    actual := func(nr)
    _test expected; actual;
}

fn func(x: i32) -> i32 { return x * 5 + 2 }

@TEST_CASE(test_diamond)

expectations: i32[] { 2, 7, 12, 17 }
for expectations {
    expected := it
    actual := func(nr)
    _test expected; actual;
}

fn func(x: i32) -> i32 { return x * 5 + 2 }
```

# Implementation details
**TODO:** Provide more information and clarify things.

The compiler tests code with these steps:
- 1. Find `@TEST_CASE` within the files and divide them up into substrings.
- 2. Compile the substrings.
- 3. Execute the compiled program.
- 4. Read stdout from the program, `x` means success, `_` means failure
- 5. Summarize compile errors and the result from stdout.

`_test` is an intrinsic with special syntax that can be represented by the code below. `test_id` refers to a list of locations in the source code where the respective `_test´ exists. That is why the summary can print the location of which test failed.

```c++
fn _test(test_id: u32, a: i64, b: i64) {
    if a == b {
        printc('x')
    } else {
        printc('_')
    }
    // test_id should range from 0 - 16777215 (24 bits)
    printc( test_id        & 0xFFu) //  0-7
    printc((test_id >> 8 ) & 0xFFu) //  8-15
    printc((test_id >> 16) & 0xFFu) // 16-23
}

_test(0, 4, 4)
_test(1, 4, 4)
_test(2, 4, 4)
```

The machine code `_test` is converted to can be found by searching for BC_TEST_VALUE in this file `x64_gen_extra.cpp`.

`TestSuite.cpp` controls the steps during testing.

# Future features
- Improve test time by reusing compiled imports for multiple tests cases. Especially those from the standard library. This should be an optional feature since it could have unforseen side effects.
- Ability to write a section of code that is included in all test cases within a file. Currently, you have to create a separate file and import it to reuse code like that for testing.
