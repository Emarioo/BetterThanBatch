#!/bin/bash

# BTB_DIR=$(pwd)/bin
# if [[ "$PATH" != *"$BTB_DIR"* ]]; then
#     export PATH=$PATH:$BTB_DIR
#     echo Set PATH: $PATH
# fi

if [ "$1" = "run" ]; then
    btb -dev
    exit
fi

if [ "$1" = "release" ]; then
    python3 build.py output=$2
else
    python3 build.py
fi

err=$?

if [ "$1" != "release" ]; then
    if [ "$err" = 0 ]; then
        # cp bin/btb btb
        if [ "$#" = 0 ]; then
            ./bin/btb -dev
            # ./bin/btb --test tests/flow/defer.btb
            # ./bin/btb tests/flow/defer.btb
            # ./bin/btb -pm *dev.btb
            # ./bin/btb tests/simple/operations.btb
            # ./bin/btb --test
            # ./bin/btb -ss dev.btb  -p
            # ./bin/btb -p examples/dev.btb
            # ./bin/btb -r ma.btb
        else
            ./bin/btb $@
        fi
    fi 
fi

exit
