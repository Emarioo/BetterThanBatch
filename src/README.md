The file structure is as follows:

- `basin` the compiler source code
- `Engone` the platform layer

- `basin/core` core components of the compiler like parser, type checker, bytecode, virtual machine.
- `basin/machine` machine code generators and object file formats
- `basin/extension` extra features like C header parser, function/variable declaration generator, assembly error message reformatter
- `basin/util` data structures and profiling not covered by `Engone`

Note that public headers when using compiler as a static library is located in `/include/basin`