# Tools
**C++** makes up the compiler's implementation.

**Assembly/machine code** is used when generating x64 programs. Virtual Machine creates machine code to execute non-bytecode (inline assembly, stubs for bytecode functions, transitioning from VM to C functions from dynamic libraries).

**Python** is used for building the compiler and anything script related.


# Do nots
- **No batch or bash scripts** because we want a project that compiles and runs on Linux and Windows (maybe MacOS in the future).


# Making a release
- Clone repo
- Clean project with `build.py clean`, compile project with `build.py`
- Run tests
- Change version number in compiler
- Make commit, push it, and tag it with version number
- Run `release.py` on Windows and Linux
- Run `vsce package` in **btb-lang** directory
- Create a release on github, add relevant info from `CHANGELOG.md`, look at previous release, provide the vscode btb-lang extension and the tar/zip files from Windows and Linux.