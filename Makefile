.SILENT:
all: build

verbose := 0

GCC_COMPILE_OPTIONS := -std=c++14 -g
# GCC_COMPILE_OPTIONS="-std=c++14 -O3"
GCC_INCLUDE_DIRS := -Iinclude -Ilibs/stb/include -Ilibs/glfw-3.3.8/include -Ilibs/glew-2.1.0/include -include include/pch.h 
GCC_DEFINITIONS := -DOS_UNIX 
GCC_WARN := -Wall -Wno-unused-variable -Wno-attributes -Wno-unused-value -Wno-unused-but-set-variable -Wno-nonnull-compare 
GCC_WARN := $(GCC_WARN) -Wno-sign-compare 

# NOTE: bin/int <- stands for intermediates which mostly consists object files

SRC := $(shell find src/BetBat src/Engone src/Native -name "*.cpp" | grep -Po '(?<=/).*')
OBJ := $(patsubst %.cpp,bin/int/%.o,$(SRC))
DEP := $(patsubst bin/int/%.o,bin/int/%.d,$(OBJ))

-include $(DEP)

ifeq ($(verbose),0)
build: $(OBJ)
	mkdir -p bin
	g++ -o bin/btb $(OBJ)
bin/int/%.o: src/%.cpp
	mkdir -p $(shell dirname $@)
	g++ $(GCC_WARN) $(GCC_COMPILE_OPTIONS) $(GCC_INCLUDE_DIRS) $(GCC_DEFINITIONS) -c $< -o $@ -MD -MP
else
bin/int/%.o: src/%.cpp
	echo $<
	mkdir -p $(shell dirname $@)
	g++ $(GCC_WARN) $(GCC_COMPILE_OPTIONS) $(GCC_INCLUDE_DIRS) $(GCC_DEFINITIONS) -c $< -o $@ -MD -MP
build: $(OBJ)
	echo bin/btb
	mkdir -p bin
	g++ -o bin/btb $(OBJ)
endif

clean:
	rm -rf bin/*
