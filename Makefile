.SILENT:
all: build build_native

verbose := 0

GCC_COMPILE_OPTIONS := -std=c++14 -g
# GCC_COMPILE_OPTIONS="-std=c++14 -O3"
GCC_INCLUDE_DIRS := -Iinclude -Ilibs/stb/include -Ilibs/glfw-3.3.8/include -Ilibs/glew-2.1.0/include -include include/pch.h 
GCC_DEFINITIONS := -DOS_UNIX 
GCC_WARN := -Wall -Wno-unused-variable -Wno-attributes -Wno-unused-value -Wno-null-dereference -Wno-missing-braces -Wno-unused-private-field -Wno-unknown-warning-option -Wno-unused-but-set-variable -Wno-nonnull-compare 
GCC_WARN := $(GCC_WARN) -Wno-sign-compare 

# CC := clang++
CC := g++

# compiler executable
OUTPUT := bin/btb

# NOTE: bin/int <- stands for intermediates which mostly consists object files

SRC := $(shell find src/BetBat src/Engone src/Native -name "*.cpp" | grep -Po '(?<=/).*')
OBJ := $(patsubst %.cpp,bin/int/%.o,$(SRC))
DEP := $(patsubst bin/int/%.o,bin/int/%.d,$(OBJ))

-include $(DEP)

# This does not check headers
build_native: src/Native/NativeLayer.cpp
# 	cl /c /std:c++14 /nologo /TP /EHsc !MSVC_INCLUDE_DIRS! /DOS_WINDOWS /DNO_PERF /DNO_TRACKER /DNATIVE_BUILD src\Native\NativeLayer.cpp /Fo:bin/NativeLayer.obj
# lib /nologo bin/NativeLayer.obj /ignore:4006 gdi32.lib user32.lib OpenGL32.lib libs/glew-2.1.0/lib/glew32s.lib libs/glfw-3.3.8/lib/glfw3_mt.lib Advapi32.lib /OUT:bin/NativeLayer.lib

	$(CC) -c $(GCC_INCLUDE_DIRS) $(GCC_WARN) -DOS_UNIX -DNO_PERF -DNO_TRACKER -DNATIVE_BUILD src/Native/NativeLayer.cpp -o bin/NativeLayer.o
# glfw, glew, opengl is not linked with here, it should be
	ar rcs -o bin/NativeLayer.lib bin/NativeLayer.o 
	

ifeq ($(verbose),0)
build: $(OBJ)
	mkdir -p bin
	$(CC) -o $(OUTPUT) $(OBJ)
bin/int/%.o: src/%.cpp
	mkdir -p $(shell dirname $@)
	$(CC) $(GCC_WARN) $(GCC_COMPILE_OPTIONS) $(GCC_INCLUDE_DIRS) $(GCC_DEFINITIONS) -c $< -o $@ -MD -MP
else
bin/int/%.o: src/%.cpp
	echo $<
	mkdir -p $(shell dirname $@)
	$(CC) $(GCC_WARN) $(GCC_COMPILE_OPTIONS) $(GCC_INCLUDE_DIRS) $(GCC_DEFINITIONS) -c $< -o $@ -MD -MP
build: $(OBJ)
	echo $(OUTPUT)
	mkdir -p $(shell dirname $(OUTPUT))
	$(CC) -o $(OUTPUT) $(OBJ)
endif

clean:
	rm -rf bin/*
