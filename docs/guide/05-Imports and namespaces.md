# Imports and namespaces

## Importing files and modules
Source files and modules are the same thing, files. Every compilation starts with an initial file and only one file. This file imports other files creating a spider net of dependencies (all which the compiler handles mind you).

```c++
// cake.btb
fn hey() {
    log("Hey! I like cake.");
}

// main.btb
#import "cake.btb"

hey();
```

The import directive has some rules when looking for the file you specified in the quotes. You can always skip `.btb` if you want. The compiler will append it for you.
```c++
// Import from an absolute path
#import "/usr/home/your_name/cake.btb"          // Unix
#import "C:/Users/your_name/Desktop/cake.btb"   // Windows

// Import from the relative path of based on the current source file
#import "./cake.btb"        // Same for Unix and Windows

src
  main.btb -> #import "./cake.btb"   imports cake.btb from the same directory (src)
  cake.btb
 
ProjectFolder (where your current working directory is, assuming it is)
  src
    main.btb -> #import "./cake.btb"   tries to import cake.btb but can't be found in src
  cake.btb
```
If you are not using the relative or absolute format the compiler will search for the files in both. But also the current working directory and the import directories supplied as compiler options when running the compiler.
```c++
// main.btb
#import "cookie.btb"
#import "util/cake.btb"

// cake.btb
#import "cookie.btb"

// Project structure
ProjectFolder
  main.btb -> imports cookie and util/cake
  cookie.btb
  util
    cake.btb -> imports cookie.btb
```


## Standard modules
The compiler has a set of standard modules that provide functions and structures such as:
- Dynamic arrays, hash maps
- Reading, writing, creating, iterating files and directories
- Sockets/networking
- Threads
- Logging
- Math and random utilities
- File formats (JSON?)
- Type information (from the compiler)

```c++
TODO: Update these!
#import "Array"
#import "Map"
#import "OS"      // File I/O, Sockets,...
#import "Threads"
#import "Logger"
#import "Math"
#import "Lang"    // Type information
```

## Namespaces
**NAMESPACES ARE BROKEN RIGHT NOW**

TODO: import file as namespace