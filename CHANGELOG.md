
# Change Log
All notable changes to this project will be documented in this file. Every version will not have a release on github but every version that does will be marked.
 
Versioning for this project works like this: [Versioning](/docs/details/05-Versioning.md).
 
## v0.2.1 - ...

### Added
- Added *#function* which is evaluated to the name of the current function, similar to *#line*.
- Added StackTrace.btb
- Added *#function_insert*, see details in guide.
- Added #fileabs which evaluates to absolute path, #file was changed to evaluate to relative path.

## Bug fixes
- Graphics.btb module accidently swapped green and blue color values when rendering text (DrawText).
- STB.btb always linked with dynamic library instead of static library.

### Standard library additions and changes
- Added std_print for colors (LogColor enum)
 
## v0.2.0 - 2024-10-14 (**latest**, **release**)
I added the changelog this last month so I added changes based on commits messages.

**WARNING:** Code you write now may not work in later versions because the compiler is in very early stages. This is especially true with macros since there is a planned redesign of the whole preprocessor and lexing phase.

### Added
- Added Asserts.btb.
- Added CHANGELOG.md.
- Added #global_macro, see Preprocessor chapter in guide.
- Added user-defined iterators.
- Added OpenSSL bindings (in modules/OpenSSL.btb).
- Added HTTP.btb module.
- Added inferred initializers.
- Added support for compiling static and dynamic libraries (not just executables).
- Added Networking library (Net.btb).
- Added socket bindings (Socket.btb).
- Added compile time evaluation of expressions in global variables (they can call functions).
- Added stb_image bindings (STB.btb).
- Added graphics module (Graphics.btb).
- Added GLFW and GLAD bindings (GLFW.btb, GLAD.btb).
- Added runtime type information (Lang.btb will be renamed).
- Added DWARF support.
- Added ELF support.
- Added inline assembly
- Added COFF support.
- Added x86_64 backend.
- Added polymorhism and function overloading.
- And a lot more like lexer, preprocessor, parser, variables, functions, methods, structs, enums, switch, macros, defer, arithmetic operations, two atomic operations, , File.btb, String.btb, Logger.btb, Windows.btb, Linux.btb, OS.btb.

### [0.1.0] - ?
Lost to time