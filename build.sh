#!/bin/bash

# This file hasn't been updated in a while and probably doesn't
# Compile things properly.

# USE_DEBUG=1

GCC_COMPILE_OPTIONS="-std=c++14"
# GCC_COMPILE_OPTIONS="-std=c++14 -O3"
GCC_INCLUDE_DIRS="-Iinclude -Ilibs/stb/include -Ilibs/glfw-3.3.8/include -Ilibs/glew-2.1.0/include -include include/pch.h "
GCC_DEFINITIONS="-DOS_LINUX"
GCC_WARN="-Wall -Wno-unused-variable -Wno-attributes -Wno-unused-value -Wno-unused-but-set-variable -Wno-nonnull-compare"
GCC_WARN="$GCC_WARN -Wno-sign-compare"

# if $USE_DEBUG; then
#     GCC_COMPILE_OPTIONS="$GCC_COMPILE_OPTIONS -g"
# fi

srcfiles=`find . -regex './src/BetBat/.*.cpp$'; find . -regex './src/Engone/.*.cpp$'; echo src/Native/NativeLayer.cpp`
output=bin/compiler

# echo $srcfiles

echo -n "Compiling..."

startTime=$(($(date +%s%N)))

g++ $GCC_WARN $GCC_COMPILE_OPTIONS $GCC_INCLUDE_DIRS $GCC_DEFINITIONS $srcfiles -o $output

err=$?
endTime=$(($(date +%s%N)))
runtime=$((($endTime - $startTime)/1000000))
echo -e "\rCompiled in $((($runtime) / 1000)).$((($endTime - $startTime) % 1000)) seconds"

if [ "$err" == 0 ]; then
    # echo f | XCOPY /y /q bin/program_linux.exe prog.exe > nul
    ./$output -dev
fi

# gcc -c src/Other/test.c -gdwarf-3 -o test.o
# objdump test.o -W > out