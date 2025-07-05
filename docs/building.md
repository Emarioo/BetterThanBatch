
You need Python and a C++ compiler to build the BTB Compiler. `build.py` is used to build and make releases of the compiler.

Basic steps can be found here: [README.md - Building](/README.md#building)

Here are options you can pass to `build.py`:

- `build.py msvc/gcc/clang` - Select C++ toolchain/compiler. `gcc` by default.
- `build.py debug=true/false` - Toggle debug information. `true` by default.
- `build.py optimized=true/false` - Toggle optimizations. `false` by default.
- `build.py tracy=true/false` - Include Tracy Profiler which lets you measure compiler performance. `false` by default.
- `build.py output=<path>` - Change path of compiler executable. `bin/btb.exe` by default.

All object files will end up in `bin/int` while the BTB executable ends up in `bin`.

You can make a release with `build.py release <version>`.

The `main` branch is always well tested and works on Linux and Windows all the time. The `dev` branch contains the latest commits where I may have broken something.


# Installing toolchains on Windows

## Installing/using MSVC
1. Install Visual Studio and select `C/C++ desktop development` in the Visual Studio Installer.
2. Navigate to the cloned repository and run `python build.py`.

If that didn't work and you get a message like this: "MSVC could not be configured". Then the automatic setup of temporary environment variables for MSVC failed.

To setup the environment you can hit the windows key and search for `VS Developer Command Prompt`. This will open up a terminal where `cl` and `link` are available. Navigate to the cloned repo folder and run `python build.py`.

If you have Visual Studio Code then you can type `code` in the *VS Developer* terminal. From there you can `Open Folder` and choose the cloned repo folder. Then in VSCode `Create New Terminal` and `cl` should be available. Now run `python build.py`.

If you want MSVC on the command line (without starting the *VS Developer* terminal) then run a command similar to this: `"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"`. The path may be different if you installed Visual Studio elsewhere or if you have a different version. You can add the path to the environment variable PATH which allows you to just type `vcvars64.bat`.
<!-- This needs more explaination.
- (manually adding environment variables from vcvars64) If you are tired of *VS Developer* and *vcvars64.bat* then you can check the content and which environment paths you have before running vcvars64.bat and compare them with the ones you have after running vcvars64.bat. Then you add the missing content to environment variables permanently.
-->

**NOTE:** When using the compiler you also need to setup the environment variables. The compiler will try to do it automatically but may fail if you are using an older version of Visual Studio or if you changed the default install location.

## Installing GCC
Look for a tutorial on the internet. This link may be helpful: https://sourceforge.net/projects/mingw/files/Installer/

## Installing clang
Look for a tutorial on the internet. 

# C++ compile time for the curios
Measurements for fully rebuilding the compiler with debug information and optimizations disabled.

|Time (s)|Compiler|OS|CPU|Drive|
|-|-|-|-|-|
|7-12|MSVC|Windows 11|Intel CPU (8 threads, released around 2013)|Some HDD from 2013|
|25-50|GCC|Windows 11|Intel CPU (8 threads, released around 2013)|Some HDD from 2013|
|2|MSVC|Windows 11|Ryzen 9 9950x (32 threads, released 2024)|NVMe WD Black SN850X Gen 4, read 7 300 MB/s|
|5|GCC|Windows 11|Ryzen 9 9950x (32 threads, released 2024)|NVMe WD Black SN850X Gen 4, read 7 300 MB/s|

Not a full rebuild takes about 1-3 seconds unless you modified C++ headers.
