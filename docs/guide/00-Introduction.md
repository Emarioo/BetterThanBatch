**DISCLAIMER**: The language and compiler is incomplete and not ready for serious use. It is perfectly fine for miscellaneous tasks.

**NOTE**: The real guide starts in the next chapter ([Chapter 1: Variables and operations](/docs/guide/01-Variables%20and%20operations.md)).

# Introduction
This is a project about a compiler for a new programming language. The idea for the language is strong fundamental concepts that gives the programmer power to do what they want with what the language offers without the language having a large feature set for every specific little thing a programmer might want.

The language is planned to be used for visualizing data, games, and file formats. The second thing is smaller scripts for every day tasks like renaming files in a directory or doing complicated math which is inconvenient on a calculator.

The name of the language ("Better than batch") is temporary. These are some other ideas.
- Tin (the metal)
- Son (?)
- Compound (language is mix of fundamental features? a compound)

## Language Server
**TODO:** Guide on how to setup extension syntax highlighting, suggestions and so on.

## Compiling code
TODO: How to compile code.
TODO: Test all the code showcased in the guide. I have not actually tested anything so I could have messed up the syntax somewhere.

Compile code with this:
`btb main.btb`

For information about the options you can pass to the compiler
`btb --help`



## Compiling the compiler
**Unix**: `build.sh`      (requires `g++`)

**Windows**: `build.bat`  (requires `cl`, `link`, and `g++` for DWARF debug information)

**NOTE**: On Windows, `g++` can be acquired from MinGW. Installing Visual Studio will give the tools but then
you must make sure `cl`, `link` is available in environment variables. Look into `vcvars64.bat`

## Why I started this project and what it has become
The original idea was a scripting language similar to shell code which you would use instead of Make, CMAKE, bash, and batch. The difference being a general purpose language without the constraints and difficulties of those build systems and shell scripts. Over time I realized that my goals were set to low. Why a scripting language? Why not a real programming language like C/C++, Jai, Rust and Odin that compiles to machine code and that you can use to create games and all kinds of applications. And so the new goal was set and here we are now with a decent programming language with many bugs, issues in the compiler's architecture design, and way more todos than a single person can handle in a small amount of time. Yet, I have not given up and still intend to turn this compiler into high quality software and a programming language that has legitimate proven reasons for why it's designed the way it is. And I will not neglect the documentation.

Will I achieve my goals? Probably not, but I do believe in my ability to create  something that is close to the goals that really matter.

## Summary of interesting features (last updated on 2024-01-13)
- Polymorphism (structs, functions)
- Macros (preprocessor)
- Type information (usable by source code)
- Operator overload (also function overloading)
- Imports and namespaces (no headers)
- Code compiles the same and performs the same on all supported operating systems (only 64-bit Windows and Unix at the moment)
- Standard library with every common thing you need (graphics, sound, networking, hash maps, logging, allocators)
- DWARF debug information
- Inline assembly (intel syntax)

**On the way**
- Bytecode execution at compile time
- Some form of metaprogramming