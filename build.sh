#!/bin/bash

# USE_DEBUG=1

GCC_COMPILE_OPTIONS="-std=c++14 -O3"
GCC_INCLUDE_DIRS="-Iinclude"
GCC_DEFINITIONS="-DOS_LINUX"
GCC_WARN="-Wall -Werror -Wno-unused-variable -Wno-unused-value -Wno-unused-but-set-variable"

# if $USE_DEBUG; then
#     GCC_COMPILE_OPTIONS="$GCC_COMPILE_OPTIONS -g"
# fi

srcfiles=`find . -regex './src/.*/.*\.cpp$'`
output=bin/compiler

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