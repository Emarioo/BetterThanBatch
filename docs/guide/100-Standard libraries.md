Chapter 100 and above is designated for informations and guides for the standard library. These guides are aimed at helping you find what you are looking for. They are not meant to explain every detail about those libraries, for details, see the comments in the actual code.

**NOTE**: Most libraries are work in progress and bound to change. That is why documentation is missing.

# Standard
- Logging/printing
- String
- Arrays
- Maps
- Miscellaneous data structures (BucketArray, ...)
- Math
- Graphics
- Files
- Memory
- Sockets (OS wrapper)
- Networking (library on top of sockets)
- Threads
- General OS stuff (creating process, sleeping)
- Hotreloading (for long running applications, games)

# Compiler/language specific
- Lang (type information)
- Context (?)

# Platform
- Windows
- Linux

# Third party libraries
These are bindings/wrappers for third party libraries. The binaries for the libraries can be found in `libs`. In the case of glad, static and dynamic libraries can be found in `libs/glad/lib-mingw-w64`. If they don't exist then you can run `build.py`.

- GLFW (window and user input API)
- stb image
- GLAD (OpenGL wrapper)