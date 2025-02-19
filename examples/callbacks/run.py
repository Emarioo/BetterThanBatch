#!/usr/bin/python3
import os

os.system("gcc examples/callbacks/util.c -g -shared -o examples/callbacks/util.dll")
os.system("btb examples/callbacks/main.btb -r")