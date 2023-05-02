#pragma once

#include "BetBat/Tokenizer.h"
#include "Engone/Alloc.h"

#define AST_MEM_OFFSET 8

struct AST;
struct ASTArg {
    // expression
    ASTArg* next;
};
struct ASTFuncCall {
    // name
    // ASTExpression
};
struct ASTExpression {
    ASTExpression() : left(0), right(0) {}
    enum Type : int {
        VALUE,
        F32,

        OPERATION,
        ADD,
        SUB,
        MUL,
        DIV,
    };
    int type = 0;
    int innerType = 0;
    union {
        struct {
            float f32Value;
        };
        struct {
            ASTExpression* left;
            ASTExpression* right;
        };
    };
    // f32, u16, i64, struct, string, pointer, function call
    // operation, +-/
    // left input (optional)
    // right input
    
    void print(AST* ast, int depth);
};
struct ASTFunction {
    // name
    // args
    // return types
    // body
};
struct ASTStatement {
    ASTStatement() : name(0), expression(0) {}
    enum Type {
        IF,
        FOR,
        EACH,
        ASSIGN,
    };
    int type=0;
    union {
        struct {
            std::string* name; // TODO: don't use std::string
            ASTExpression* expression;
        };
    };
    // assignment
    // function call
    // return, continue, break
    // for, each, if

    ASTStatement* next=0;

    void print(AST* ast, int depth);
};
struct ASTBody {
    // functions
    // statements
    
    ASTStatement* statement = 0;

    void print(AST* ast, int depth);
};
struct AST {
    /*
    functions
    structs
    global variables
    
    OR
 
    global body
    */
    ASTBody* body=0;

    engone::Memory memory{1};
    // engone::Memory text{1};

    uint createName(Token& token);

    void init();
    static AST* Create();
    static void Destroy(AST* ast);
    void cleanup();
    ASTBody* createBody();
    ASTStatement* createStatement(int type);
    ASTExpression* createExpression(int type);

    void print(int depth = 0);

    template<typename T>
    T* relocate(T* offset){
        return offset;
        // return (T*)memory.data + ((u64)offset - AST_MEM_OFFSET);
    }
};