# This file is for Linux
# Not Windows, use build.bat for Windows

.SILENT:
all: build

verbose := 0

GCC_COMPILE_OPTIONS := -std=c++14 -g
# GCC_COMPILE_OPTIONS="-std=c++14 -O3"
GCC_INCLUDE_DIRS := -Iinclude -Ilibs/stb/include -Ilibs/glfw-3.3.8/include -Ilibs/glew-2.1.0/include -Ilibs/tracy-0.10/public -include include/pch.h 
GCC_DEFINITIONS := -DOS_LINUX -DNO_UIMODULE -DCOMPILER_GNU
GCC_WARN := -Wall -Wno-unused-variable -Wno-attributes -Wno-unused-value -Wno-null-dereference -Wno-missing-braces -Wno-unused-private-field -Wno-unknown-warning-option -Wno-unused-but-set-variable -Wno-nonnull-compare -Wno-sequence-point
# GCC complains about dereferencing nullptr (-Wsequence-point)
GCC_WARN := $(GCC_WARN) -Wno-sign-compare 

# CC := clang++
CC := g++

# compiler executable
ifdef path
	OUTPUT := $(path)
else
	OUTPUT := bin/btb
endif

# NOTE: bin/int <- stands for intermediates which mostly consists object files

SRC := $(shell find src/BetBat src/Engone -name "*.cpp" | grep -Po '(?<=/).*')
OBJ := $(patsubst %.cpp,bin/int/%.o,$(SRC))
DEP := $(patsubst bin/int/%.o,bin/int/%.d,$(OBJ))


SRC := $(SRC) BetBat/hacky_sysvcall.s
OBJ := $(OBJ) bin/int/BetBat/hacky_sysvcall.o

# $(info $(SRC))
# $(info $(OBJ))

-include $(DEP)

# This does not check headers
# build_native: src/Native/NativeLayer.cpp
# 	cl /c /std:c++14 /nologo /TP /EHsc !MSVC_INCLUDE_DIRS! /DOS_WINDOWS /DNO_PERF /DNO_TRACKER /DNATIVE_BUILD src\Native\NativeLayer.cpp /Fo:bin/NativeLayer.obj
# lib /nologo bin/NativeLayer.obj /ignore:4006 gdi32.lib user32.lib OpenGL32.lib libs/glew-2.1.0/lib/glew32s.lib libs/glfw-3.3.8/lib/glfw3_mt.lib Advapi32.lib /OUT:bin/NativeLayer.lib

# $(CC) -c $(GCC_INCLUDE_DIRS) $(GCC_WARN) -DOS_UNIX -DNO_PERF -DNO_TRACKER -DNATIVE_BUILD src/Native/NativeLayer.cpp -o bin/NativeLayer_gcc.o
# glfw, glew, opengl is not linked with here, it should be
# ar rcs -o bin/NativeLayer_gcc.lib bin/NativeLayer_gcc.o 

# build_tracy: tracy/public/TracyClient.cpp
# 	$(CC) -c $(GCC_INCLUDE_DIRS) $(GCC_WARN) -DOS_UNIX  ayer.cpp -o bin/NativeLayer_gcc.o
# 	ar rcs -o bin/NativeLayer_gcc.lib bin/NativeLayer_gcc.o 

ifeq ($(verbose),0)
build: $(OBJ)
	mkdir -p bin
	$(CC) -o $(OUTPUT) $(OBJ)
bin/int/%.o: src/%.cpp
	mkdir -p $(shell dirname $@)
	$(CC) $(GCC_WARN) $(GCC_COMPILE_OPTIONS) $(GCC_INCLUDE_DIRS) $(GCC_DEFINITIONS) -c $< -o $@ -MD -MP
bin/int/%.o: src/%.s
	mkdir -p $(shell dirname $@)
	as -c $< -o $@
else
build: $(OBJ)
	echo $(OUTPUT)
	mkdir -p $(shell dirname $(OUTPUT))
	$(CC) -o $(OUTPUT) $(OBJ)
bin/int/%.o: src/%.cpp
	echo $<
	mkdir -p $(shell dirname $@)
	$(CC) $(GCC_WARN) $(GCC_COMPILE_OPTIONS) $(GCC_INCLUDE_DIRS) $(GCC_DEFINITIONS) -c $< -o $@ -MD -MP
bin/int/%.o: src/%.s
	echo $<
	mkdir -p $(shell dirname $@)
	as -c $< -o $@
endif

clean:
	rm -rf bin/*
