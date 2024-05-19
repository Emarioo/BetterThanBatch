# Annotations
The purpose of annotations is to tell the compiler extra details about structures, statements, or functions. All annotations follow the same syntax: `@name` or `@name(extra content)`.

Annotations usually appear between keyword like `struct` or `fn` and an identifier as shown below:
```c++
struct @no_padding MyData { ... }

fn @stdcall MyFunction() { ... }
```

See other chapters such as [Structs](./04-Structs.md) and [Enums](./06-Enums.md) for specific annotations.

# Future / incomplete features

User defined annotations?