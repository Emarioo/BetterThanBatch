# GLFW binaries for 64-bit Windows

## Visual studio tool chain
`lib-vc2022` contains binaries compiled using the Microsoft Visual C/C++ tool chain.

To use GLFW as a DLL, link against the `glfw3dll.lib` file for your
environment.  This will add a load time dependency on `glfw3.dll`.  The
remaining files in the same directory are not needed.

This DLL is built in release mode for the Multithreaded DLL runtime library.

There is also a GLFW DLL and import library pair in the `lib-static-ucrt`
directory.  These are built with Visual C++ 2019 and the static Multithreaded
runtime library.

To use GLFW as a static library, link against `glfw3.lib` if your application
is using the Multithreaded DLL runtime library, or `glfw3_mt.lib` if it is
using the static Multithreaded runtime library.  The remaining files in the same
directory are not needed.

The static libraries are built in release mode and do not contain debug
information but can still be linked with the debug versions of the runtime
library.

# #Mingw

To use GLFW as a DLL, link against the `libglfw3dll.a` file for your
environment.  This will add a load time dependency on `glfw3.dll`.  The
remaining files in the same directory are not needed.

# GLFW binaries for 64-bit Ubuntu
`lib-ubuntu` contains binaries compiled with **X11** (libglfw3.a libglfw.so.3.3).

There is also libglfw-wl.so.3.3 and libglfw3-wl.a which are compiled with **Wayland**. The compiler and language assumes you want to use X11 by default since Wayland doesn't have buttons to minimize, maximize and close the window.

The binaries have been tested on Ubuntu 22.04.4 in VirtualBox 7.0.20.
