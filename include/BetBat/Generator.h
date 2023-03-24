#pragma once

#include "BetBat/Tokenizer.h"
#include "BetBat/Value.h"
#include "Engone/Typedefs.h"

#include <unordered_map>

// 2 bits to indicate instruction structure
// 3 register OR 1 register + 1 variable OR 1 register
// #define BC_MASK (0b11<<6)
// #define BC_R3 (0b00<<6)
// #define BC_R2 (0b01<<6)
// #define BC_R1 (0b10<<6)

#define BC_MASK          (0xC0)
#define BC_R1            (0x40)
#define BC_R2            (0x80)
#define BC_R3            (0xC0)

#define BC_ADD           (BC_R3|0x01)
#define BC_SUB           (BC_R3|0x02)
#define BC_MUL           (BC_R3|0x03)
#define BC_DIV           (BC_R3|0x04)
#define BC_LESS          (BC_R3|0x05)
#define BC_GREATER       (BC_R3|0x06)
#define BC_EQUAL         (BC_R3|0x07)

#define BC_JUMPIF        (BC_R3|0x30)

#define BC_COPY          (BC_R2|0x01)
#define BC_MOV           (BC_R2|0x02)
#define BC_RUN           (BC_R2|0x03)

#define BC_NUM           (BC_R1|0x01)
#define BC_STR           (BC_R1|0x02)
#define BC_DEL           (BC_R1|0x03)

#define BC_LOAD          (BC_R1|0x10)

#define BC_RETURN        (BC_R1|0x15)
#define BC_ENTERSCOPE    (BC_R1|0x16)
#define BC_EXITSCOPE     (BC_R1|0x17)

#define BC_JUMP          (BC_R1|0x30)

#define DEFAULT_REG_ZERO 0
#define DEFAULT_REG_ACC 1
#define DEFAULT_REG_TEMP0 4
#define DEFAULT_REG_TEMP1 5
#define DEFAULT_REG_TEMP2 6

// #define DEFAUlT_REG_NTEMP0 1
// #define DEFAUlT_REG_NTEMP1 2
// #define DEFAUlT_REG_NTEMP2 3
// #define DEFAUlT_REG_STEMP0 4
// #define DEFAUlT_REG_STEMP1 5
// #define DEFAUlT_REG_STEMP2 6
// #define DEFAUlT_REG_NTEMP0_s "n0"
// #define DEFAUlT_REG_NTEMP1_s "n1"
// #define DEFAUlT_REG_NTEMP2_s "n2"
// #define DEFAULT_REG_STEMP0_S "s0"
// #define DEFAULT_REG_STEMP1_S "s1"
// #define DEFAULT_REG_STEMP2_S "s2"

#define DEFAULT_REG_RETURN_ADDR 7
#define DEFAULT_REG_RETURN_VALUE 8
#define DEFAULT_REG_ARGUMENT 9
#define DEFAULT_REG_RETURN_ADDR_S "ra"
#define DEFAULT_REG_RETURN_VALUE_S "rv"
#define DEFAULT_REG_ARGUMENT_S "rz"

const char* InstToString(int type);

struct Instruction {
    uint8 type=0;
    union {
        uint8 regs[3]{0};
        struct{
            uint8 reg0;
            uint8 reg1;
            uint8 reg2;
        };
    };

    uint32 singleReg(){return (uint32)reg0|((uint32)reg1<<8)|((uint32)reg2<<16);}
    void print();
};
engone::Logger& operator<<(engone::Logger& logger, Instruction& instruction);

class Context;
typedef void(*APICall)(Context*, int refType, void*);
struct Bytecode {
    engone::Memory codeSegment{sizeof(Instruction)};
    engone::Memory constNumbers{sizeof(Number)};
    engone::Memory constStrings{sizeof(String)};
    //Todo: One array which contains the memory for all strings instead of
    //      each string having it's own allocation.
    
    struct Unresolved{
        uint address=-1;
        std::string name;
        APICall func=0;
    };
    engone::Memory unresolveds{sizeof(Unresolved)};
    bool addUnresolved(Token& token, uint address);
    Unresolved* getUnresolved(uint address);
    Unresolved* getUnresolved(const std::string& name);
    bool linkUnresolved(const std::string& name, APICall funcPtr);
    
    // std::unordered_map<uint, uint> unresolvedConstants;
    uint nextUnresolvedAddress=0x400000; // note that BC_LOAD_CONST has 24 bits of address space.

    bool add(Instruction inst);
    bool add(uint8 type, uint8 reg0, uint8 reg1, uint8 reg2);
    bool add(uint8 type, uint8 reg0, uint16 reg12);
    bool add(uint8 type, uint reg012);
    Instruction& get(uint index);
    uint length();
    
    uint addConstNumber(double number);
    Number* getConstNumber(uint index);
    
    uint addConstString(Token& token);
    String* getConstString(uint index);
};
struct GenerationInfo {
    Bytecode code{};
    uint index=0;
    Tokens tokens{};
    
    int baseIndex=-1;

    // std::unordered_map<double,uint> constNumberMap;
    
    // Todo: converting from Token to std::string can be slow since it may
    //   require memory to be allocated. Make a custom hash map?
    std::unordered_map<std::string,uint> nameOfNumberMap;
    std::unordered_map<std::string,uint> nameOfStringMap;

    struct IncompleteInstruction{
        uint instIndex=0;
        Token token;
    };
    std::vector<IncompleteInstruction> instructionsToResolve;

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

std::string Disassemble(Bytecode code);