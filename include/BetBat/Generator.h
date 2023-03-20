#pragma once

#include "BetBat/Tokenizer.h"
#include "BetBat/Values.h"

// Operators
#define BC_ADD 0x01
#define BC_SUB 0x02
#define BC_MUL 0x03
#define BC_DIV 0x04

// Variable/register management
#define BC_MAKE_NUMBER 0x11
#define BC_MAKE_STRING 0x12
#define BC_LOAD_CONST 0x13

const char* InstToString(int instructionType);

struct Instruction {
    uint8 type=0;
    uint8 reg0;
    union {
        struct {
            uint8 reg1;
            uint8 reg2;
        };
        uint16 var0;
    };
};
Instruction CreateInstruction(uint8 type, uint8 reg0, uint8 reg1, uint8 reg2);
Instruction CreateInstruction(uint8 type, uint8 reg0, uint16 var0);
Instruction CreateInstruction(uint8 type, uint8 reg0);

struct Bytecode {
    engone::Memory constNumbers{sizeof(Number)};
    engone::Memory stringNumbers{sizeof(String)};
    engone::Memory codeSegment{sizeof(Instruction)};
    
    bool add(Instruction instruction);
    Instruction get(int index);
    
    int addConstNumber(double number);
    Number* getConstNumber(int index);
    
    int length();
};


Bytecode Generate(Tokens tokens);