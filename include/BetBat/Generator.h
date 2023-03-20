#pragma once

#include "BetBat/Tokenizer.h"
#include "BetBat/Value.h"
#include "Engone/Typedefs.h"

#include <unordered_map>

// 2 bits to indicate instruction structure
// 3 register OR 1 register + 1 variable OR 1 register
#define BC_MASK (0b11<<6)
#define BC_R3 (0b00<<6)
#define BC_R2 (0b01<<6)
#define BC_R1 (0b10<<6)

#define BC_ADD (BC_R3|0x01)
#define BC_SUB (BC_R3|0x02)
#define BC_MUL (BC_R3|0x03)
#define BC_DIV (BC_R3|0x04)

#define BC_DIV (BC_R3|0x05)

#define BC_LOAD_CONST (BC_R2|0x01)

#define BC_MAKE_NUMBER (BC_R1|0x01)
#define BC_JUMP (BC_R1|0x02)


const char* InstToString(int type);

struct Instruction {
    uint8 type;
    uint8 reg0;
    union {
        struct {
            uint8 reg1;
            uint8 reg2;
        };
        uint16 reg12;
    };

    void print();
};
engone::Logger& operator<<(engone::Logger& logger, Instruction& instruction);

struct Bytecode {
    engone::Memory codeSegment{sizeof(Instruction)};
    engone::Memory constNumbers{sizeof(Number)};

    bool add(Instruction inst);
    bool add(uint8 type, uint8 reg0, uint8 reg1, uint8 reg2);
    bool add(uint8 type, uint8 reg0, uint16 reg12);
    bool add(uint8 type, uint reg012);
    Instruction& get(uint index);
    
    uint addConstNumber(double number);
    Number* getConstNumber(uint index);
    uint length();
};
struct GenerationInfo {
    Bytecode code{};
    uint index=0;
    Tokens tokens{};
    
    int baseIndex=-1;

    std::unordered_map<double,uint> constNumberMap;

    std::unordered_map<std::string,uint> nameNumberMap;

    std::unordered_map<std::string,uint> addressMap;

    struct AddressInst{
        uint instIndex=0;
        Token addrName;
    };
    std::vector<AddressInst> instructionsToResolve;

    // Does not handle out of bounds
    Token prev();
    Token next();
    Token now();
    int at();
    bool end();
    void finish();

    void nextLine();
};

Bytecode CompileScript(Tokens tokens);
Bytecode CompileInstructions(Tokens tokens);