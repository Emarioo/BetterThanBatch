#pragma once

#include "BetBat/Tokenizer.h"
#include "Engone/Alloc.h"

typedef int TypeId;
struct ASTStruct;
struct ASTEnum;
struct TypeInfo {
    TypeInfo(TypeId id, int size=0) : id(id), size(size) {}
    // name
    TypeId id;
    int size=0;
    ASTStruct* astStruct=0;
    ASTEnum* astEnum=0;
};

enum PrimitiveType : int {
    AST_VOID=0,
    AST_UINT8,
    AST_UINT16,
    AST_UINT32,
    AST_UINT64,
    AST_INT8,
    AST_INT16,
    AST_INT32,
    AST_INT64,
    
    AST_BOOL8,
    
    AST_FLOAT32,
    
    AST_NULL, // usually converted to cast<void*> 0
    
    // TODO: should probably be moved
    AST_VAR,
    AST_FNCALL,
    
    AST_PRIMITIVE_COUNT,
};
enum OperationType : int {
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
    
    AST_CAST,
    AST_PROP,
    AST_INITIALIZER,
    AST_FROM_NAMESPACE,
    // AST_SIZEOF,

    AST_REFER, // take reference
    AST_DEREF, // dereference
};
struct TokenRange{
    Token firstToken{};
    int startIndex=0;
    int endIndex=0;
};
struct AST;
const char* OpToStr(int op);
struct ASTExpression {
    ASTExpression() {
        // printf("hum\n");
        // engone::log::out << "default init\n";
        // *(char*)0 = 9;
    }
    // ASTExpression() { memset(this,0,sizeof(*this)); }
    // ASTExpression() : left(0), right(0), castType(0) { }
    TokenRange tokenRange{};
    // Token token{};
    bool isValue=false;
    TypeId typeId = 0;
    
    float f32Value=0;
    i64 i64Value=0;
    bool b8Value=0;
    std::string* name=0;
    std::string* member=0;
    ASTExpression* left=0;
    ASTExpression* right=0;
    TypeId castType=0;
    
    ASTExpression* next=0;
    void print(AST* ast, int depth);
};
struct ASTBody;
struct ASTStatement {
    // ASTStatement() { memset(this,0,sizeof(*this)); }
    enum Type {
        ASSIGN, // a = 9
        PROP_ASSIGN, // *a = 9   a[1]=2   (*(a+8)[2]) = 92
        IF,
        WHILE,
        RETURN,
        CALL,
    };
    TokenRange tokenRange{};
    int type=0;
    // assign
    std::string* name=0;
    TypeId typeId=0;
    ASTExpression* lvalue=0;
    ASTExpression* rvalue=0;
    ASTBody* body=0;
    ASTBody* elseBody=0;
    // assignment
    // function call
    // return, continue, break
    // for, each, if

    ASTStatement* next=0;

    void print(AST* ast, int depth);
};
struct ASTStruct {
    TokenRange tokenRange{};
    std::string* name=0;
    struct Member {
        std::string name;
        TypeId typeId;
        int offset=0;
    };
    std::vector<Member> members{};
    int size=0;

    ASTStruct* next=0;

    void print(AST* ast, int depth);
};
struct ASTEnum {
    TokenRange tokenRange{};
    std::string* name=0;
    struct Member {
        std::string name;
        int id=0;
    };
    std::vector<Member> members{};
    
    ASTEnum* next=0;
    void print(AST* ast, int depth);  
};
struct ASTFunction {
    TokenRange tokenRange{};
    std::string* name=0;
    struct Arg{
        std::string name;
        TypeId typeId;
    };
    std::vector<Arg> arguments;
    std::vector<TypeId> returnTypes;

    ASTBody* body=0;

    ASTFunction* next=0;

    void print(AST* ast, int depth);
};
struct ASTBody {
    TokenRange tokenRange{};
    ASTStruct* structs = 0;
    ASTEnum* enums = 0;
    ASTFunction* functions = 0;
    ASTStatement* statements = 0;

    void print(AST* ast, int depth);
};
struct AST {
    static AST* Create();
    static void Destroy(AST* ast);
    void cleanup();
    
    ASTBody* body=0;
    
    std::unordered_map<std::string, TypeInfo*> typeInfos;
    TypeId nextTypeIdId=0x100;
    static const TypeId POINTER_BIT = 0x40000000;
    TypeId nextPointerTypeId=POINTER_BIT;
    // creates a new type if name doesn't exist 
    TypeId getTypeId(const std::string& name);
    // creates a new type if needed
    TypeInfo* getTypeInfo(const std::string& name);
    TypeInfo* getTypeInfo(TypeId id);
    // do not save the returned string reference, add a data type and then use the reference.
    // it may be invalid.
    const std::string* getTypeId(TypeId id);

    static bool IsPointer(TypeId id);
    static bool IsPointer(Token& token);
    
    // true if id is one of u8-64, i8-64
    static bool IsInteger(TypeId id);
    // will return false for non number types
    static bool IsSigned(TypeId id);

    // engone::Memory text{1};

    // uint createName(Token& token);

    ASTBody* createBody();
    ASTFunction* createFunction(const std::string& name);
    ASTStruct* createStruct(const std::string& name);
    ASTEnum* createEnum(const std::string& name);
    ASTStatement* createStatement(int type);
    ASTExpression* createExpression(TypeId type);

    void print(int depth = 0);
};
const char* TypeIdToStr(int type);