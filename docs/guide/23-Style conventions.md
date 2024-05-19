You can skip this chapter without missing much.

# Coding style in this language
The language and compiler do not impose a coding style. You are free to do as you wish. There is however a standard coding style that should preferably be used so that programmers can read each others' code with ease. The standard library is consistent with the coding style defined in this document.

**NOTE:** The style is work in progress. To be honest, we might just skip this entirely. It's just a few things that should be standardized.

**TODO:** Give numbers to the areas of coding style. Example: *1 Project structure*, *2 Naming convention* *2.1 Function names*


## Terminology
- **Pascal case**: ILikeChicken

- **Snake case**: i_like_chicken

- **Camel case**: iLikeChicken

## Naming convention
- **Variables/Parameters**: All variables/parameters should be **Camel Case** or **Snake Case**. Global variables should be prefixed with `global_` or `g_` if it is not obvious that the global's name is a global and not a local variable. Variables/arguments that represent an amount of something should be named `count`, `length`, or `len` like `entity_count` and **NOT** `entity_size`. `size` represents an amount of bytes like `array_size` is the allocated bytes for an array while `array_count` represents the amount of elements in the array.

- **Constants**: Actual constants or macro constants should be all upper case with underscores.

- **Functions**: Normal/global functions (non-methods) should be **Pascal case**. Methods should be **Camel case** or **Snake Case**. The words in the name should consist of a verb or at least indicate some kind of action.

- **Structures**: **Pascal case**.

- **Namespaces**: **Camel case** or **Snake case**. The name should indicate some kind of module or namespace. For example, a namespace named `chicken` or `manager` would be assumed to a structure. `math`, `utility`, or `animals` is more reasonable.

- Long descriptive names is a good thing.

- **Macros**: Macros should preferably be all upper case unless you need a specific name that resembles a function.

## Styles of little importance
- Nesting of scopes
- Lines of code per function

# TEMPORARY
Text after this point are old thoughts. May be brought back, or maybe not.

The coding style has two levels of categorization. The first level describes the importance where **High** should rarely (preferably never) be broken while **"Who cares"** can be fully ignored with few consequences.

## High importance
**High** importance is identified as: "If you break this coding style, people reading your code WILL be confused and annoyed about your code."

## Low importance

## "Who cares" importance
Importance identified as: "It would be nice if people followed these conventions but no one really cares because at the end of the day, it's just a personal preference."
