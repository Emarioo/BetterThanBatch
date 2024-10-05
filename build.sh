#!/bin/bash

# BTB_DIR=$(pwd)/bin
# if [[ "$PATH" != *"$BTB_DIR"* ]]; then
#     export PATH=$PATH:$BTB_DIR
#     echo Set PATH: $PATH
# fi

if [ ! "$1" = "run" ]; then
    export LD_LIBRARY_PATH=.
    python3 build.py
else
    export LD_LIBRARY_PATH=.
    ./bin/btb -dev
fi
