#pragma once

#include "BetBat/Tokenizer.h"
#include "Engone/Alloc.h"

typedef u32 DataType;
enum PrimitiveType : u32 {
    AST_NONETYPE=0,
    AST_FLOAT32=1,
    AST_INT32,
    AST_BOOL8,
    
    AST_VAR,
    AST_FNCALL,
    
    AST_PRIMITIVE_COUNT,
};
enum OperationType : u32 {
    AST_ADD=AST_PRIMITIVE_COUNT,  
    AST_SUB,
    AST_MUL,
    AST_DIV,
    
    AST_EQUAL,
    AST_NOT_EQUAL,
    AST_LESS,
    AST_GREATER,
    AST_LESS_EQUAL,
    AST_GREATER_EQUAL,
    AST_AND,
    AST_OR,
    AST_NOT,

    AST_REFER, // take reference
    AST_DEREF, // dereference
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
            std::string* funcName;
            ASTExpression* inArg;
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
    
    // used for function calls
    ASTExpression* next=0;
    void print(AST* ast, int depth);
};
struct ASTBody;
struct ASTStatement {
    ASTStatement() : name(0), dataType(0), expression(0) {}
    enum Type {
        ASSIGN,
        IF,
        WHILE,
        FOR,
        EACH,
    };
    int type=0;
    union {
        struct { // assign
            std::string* name; // TODO: don't use std::string
            DataType dataType;
        };
    };
    ASTExpression* expression;
    ASTBody* body;
    ASTBody* elseBody;
    // assignment
    // function call
    // return, continue, break
    // for, each, if

    ASTStatement* next=0;

    void print(AST* ast, int depth);
};
struct ASTFunction {
    std::string* name=0;
    struct Arg{
        std::string name;
        DataType dataType;
    };
    std::vector<Arg> arguments;
    std::vector<DataType> returnTypes;

    ASTBody* body=0;

    ASTFunction* next=0;

    void print(AST* ast, int depth);
};
struct ASTBody {
    // functions
    // statements
    ASTFunction* function = 0;
    ASTStatement* statement = 0;

    void print(AST* ast, int depth);
};
struct AST {
    static AST* Create();
    static void Destroy(AST* ast);
    void cleanup();
    /*
    functions
    structs
    global variables
    
    OR
 
    global body
    */
    ASTBody* body=0;

    std::unordered_map<std::string, DataType> dataTypes;
    DataType nextDataTypeId=0x100;
    static const DataType POINTER_BIT = 0x40000000;
    DataType nextPointerTypeId=POINTER_BIT;
    DataType getDataType(const std::string& name);
    // do not save the returned string reference, add a data type and then use the reference.
    // it may be invalid.
    const std::string* getDataType(DataType id);
    DataType addDataType(const std::string& name);

    DataType getOrAddDataType(const std::string& name);

    static bool IsPointer(DataType id);

    // engone::Memory text{1};

    // uint createName(Token& token);

    ASTBody* createBody();
    ASTFunction* createFunction(const std::string& name);
    ASTStatement* createStatement(int type);
    ASTExpression* createExpression(DataType type);

    void print(int depth = 0);
};
const char* DataTypeToStr(int type);