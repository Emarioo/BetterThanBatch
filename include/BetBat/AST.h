#pragma once

#include "BetBat/Tokenizer.h"
#include "Engone/Alloc.h"

#define AST_MEM_OFFSET 8

typedef u32 DataType;
enum PrimitiveType : u32 {
    AST_NONETYPE=0,
    AST_FLOAT32=1,
    AST_INT32,
    AST_BOOL8,
    
    AST_VAR,
    
    AST_PRIMITIVE_COUNT,
};
enum OperationType : u32 {
    AST_ADD=AST_PRIMITIVE_COUNT,  
    AST_SUB,  
    AST_MUL,  
    AST_DIV,
};
struct AST;
const char* OpToStr(int op);
struct ASTExpression {
    Token token{};
    bool isValue=false;
    DataType dataType = 0;
    
    // NOTE: Variables in union are not zero initialized. Only use the 
    //   variables indicated by isValue and type
    union {
        float f32Value;
        int i32Value;
        bool b8Value;
        std::string* varName;
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
struct ASTStatement {
    ASTStatement() : name(0), dataType(0), expression(0) {}
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
            int dataType;
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
    ASTExpression* createExpression(DataType type);

    void print(int depth = 0);

    template<typename T>
    T* relocate(T* offset){
        return offset;
        // return (T*)memory.data + ((u64)offset - AST_MEM_OFFSET);
    }
};
const char* DataTypeToStr(int type);