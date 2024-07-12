**NOTE:** I encourage you to fully read the introduction because it contains vital information for a good experience. If you have a bad experience with the language because you were to lazy to read the introduction, then that's on you. Everything else however, the bugs, the bad design, and the incomplete documentation, is most certainly my fault.

# Introduction
This is a project about a compiler for a new programming language. The idea for the language is strong fundamental concepts that gives the programmer power to do what they want with what the language offers without the language having a large feature set for every specific little thing a programmer might want.

The language is planned to be used for making games, visualizing data and file formats. The second thing is smaller scripts for every day tasks like renaming files in a directory or doing complicated math which is inconvenient on a calculator.

## Where to start?
If you haven't built or downloaded the compiler, take a look at [README](/README.md).

I recommend you keep the compiler binaries/assets (the stuff you downloaded) in one folder and your own projects and code in a separate folder.

You compile files with this command `btb main.btb -o main.exe` and that is pretty much it. You can change target, linker, and whether you want to execute the program right away but the default settings are fine when starting out.

Run `btb --help` for more information about the compiler options.

Your next step is here [Chapter 1: Variables and operations](/docs/guide/01-Variables%20and%20operations.md).

Also, if you want a challange, purpose or project to work on, have a look at [Suggestions](/docs/suggestions.md).

**NOTE*: You may want to start with a test file where you write random throw away code for learning purposes when following the guide.

## Quality of life
When programming you want a couple of other things such as syntax highlighting, debugger and the ability to easily look at the standard library to see the available functions.

For Visual Studio Code (VSCode), I have made an experimental extension for syntax highlighting. In the github repo it exists in a folder called btb-lang and you can activate it by cloning the repo and doing this action `Developer: Install extension from location...` (ctrl+shift+p and search for it, assuming you have the default keybindings). All .btb files will now have the proper syntax highlighting. Intellisense, suggestions, go to definition and similar things is not supported, I am not an extension or TypeScript developer.

For debugging, add the `-d` flag to compile executable with debug information. The format of the information is DWARF and not PDB (PDB is complicated format, it's work in progress). You can therefore not use Visual Studio to debug your program. GDB or VSCode can be used instead. With VSCode you need to create `.vscode/launch.json`. If you don't know what to put in the launch file, look at the `BTB debug` configuration in github repo ([launch.json](/.vscode/launch.json))

If you use the `Go to file` feature in VSCode and want to look up standard library modules then you can add the compiler folder to your workspace. VSCode will then search that folder including the modules folder for files such as `Array.btb` and `Graphics.btb`. There may be a better way of doing it but this will at least work.

I know this seems like a lot of work and I wish I knew of a better way of solving these problems but unfortunately I don't. I am open to improvements.

# Background (not important)

The name current name of the language, Better Than Batch, is temporary. These are some other ideas.
- Tin (the metal)
- Son (?)
- Compound (language is mix of fundamental features? a compound)

## Why I started this project
The original idea was a scripting language similar to shell code which you would use instead of Make, CMAKE, bash, and batch. The difference being a general purpose language without the constraints and difficulties of those build systems and shell scripts. Over time I realized that my goals were set to low. Why a scripting language? Why not a real programming language like C/C++, Jai, Rust and Odin that compiles to machine code and that you can use to create games and all kinds of applications. 

And so the new goal was set and here we are now with a decent programming language with many bugs and way more todos than a single person can handle. I still intend to turn this compiler into high quality software and a programming language that has legitimate practical reasons for features and language decisions. And I will not neglect the documentation so that people can actually use the language.
