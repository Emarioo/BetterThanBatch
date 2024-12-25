**NOTE:** I encourage you to fully read the introduction because it contains vital information for a good experience. If you have a bad experience with the language because you were to lazy to read the introduction, that's on you. Everything else however, the bugs, the bad design, and the incomplete documentation, is most certainly my fault.

# Introduction
This is a project about a compiler for a new programming language. The idea for the language is strong fundamental concepts that gives the programmer power to do what they want with what the language offers without the language having a large feature set for every specific little thing a programmer might want.

The language is planned to be used for making games, visualizing data and file formats. The second thing is smaller scripts for every day tasks like renaming files in a directory or doing complicated math which is inconvenient on a calculator.

## Where to start?
If you haven't built or downloaded the compiler, take a look at [README](/README.md#How-to-get-started).

Your workflow with the compiler will look something like this:
```bash
btb main.btb -o main.exe
main.exe

# or if you want to compile and execute right away
btb main.btb -r

# more information about options
btb --help
```

Now when you know how to compile, your next step is here [Chapter 1: Variables and operations](/docs/guide/01-Variables%20and%20operations.md).

You can find some project ideas here [Suggestions](/docs/misc/suggestions.md).

**NOTE:** The compiler takes only one code file as input and through import directives finds and compiles all other files the program depend on.


## Quality of life
When programming you want a couple of other things such as syntax highlighting, debugger and the ability to easily look at the standard library to see the available functions.

**Syntax highlighting for Visual Studio Code**

The language has a vscode extension called `BTB Lang` which you can find in the marketplace ("Search Extensions in Marketplace"). Installing will apply syntax highlighting to all .btb files. Intellisense, suggestions, go to definition and similar things is not supported, may not ever be.

**Debugging**

For debugging, add the `-d` flag to compile executable with debug information. The format of the information is always DWARF and not PDB. You can therefore not use Visual Studio to debug your program (it uses PDB). GDB or VSCode can be used instead. With VSCode you need to create `.vscode/launch.json`. If you don't know what to put in the launch file, look at the `BTB debug` configuration in github repo ([launch.json](/.vscode/launch.json)) or search for more information online.

**Go to standard library file (vscode)**

For the `Go to file` command to find standard library file you can add the compiler folder to your workspace. VSCode will then search that folder including the modules folder for files such as `Array.btb` and `Graphics.btb`.

**Syntax highlighting for other editors**

For other editors you need to make your own syntax highlighting. Highlighting keywords and number literals may be enough.

