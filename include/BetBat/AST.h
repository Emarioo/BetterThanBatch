#pragma once

#include "BetBat/Tokenizer.h"
#include "Engone/Alloc.h"

typedef int TypeId;
struct ASTStruct;
struct ASTEnum;
struct TypeInfo {
    TypeInfo(const std::string& name, TypeId id, int size=0) :  name(name), id(id), _size(size) {}
    TypeInfo(TypeId id, int size=0) : id(id), _size(size) {}
    std::string name;
    TypeId id=0;
    int _size=0;
    int _alignedSize=0;
    int arrlen=0;
    ASTStruct* astStruct=0;
    ASTEnum* astEnum=0;
    std::vector<TypeId> polyTypes;
    TypeInfo* polyOrigin=0;
    
    struct MemberOut { TypeId typeId;int index;};

    int size();
    // 1,2,4,8
    int alignedSize();
    MemberOut getMember(const std::string& name);
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
    
    AST_BOOL,
    AST_CHAR,
    
    AST_FLOAT32,
    
    AST_STRING, // converted to another type, probably char[]
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
    AST_MEMBER,
    AST_INITIALIZER,
    AST_SLICE_INITIALIZER,
    AST_FROM_NAMESPACE,
    AST_SIZEOF,

    AST_REFER, // take reference
    AST_DEREF, // dereference
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
    
    union {
        i64 i64Value=0;
        float f32Value;
        bool boolValue;
        char charValue;
    };
    std::string* name=0;
    int constStrIndex=0;
    std::string* namedValue=0; // used for named arguments
    ASTExpression* left=0; // FNCALL has arguments in order left to right
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
        USING,
        BODY,
    };
    TokenRange tokenRange{};
    int type=0;
    // assign
    std::string* name=0;
    std::string* alias=0;
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
        TypeId typeId=0;
        int offset=0;
        int polyIndex=-1;
        ASTExpression* defaultValue=0;
    };
    std::vector<Member> members{};
    int size=0;
    int alignedSize=0;
    bool typeComplete=false;
    std::vector<std::string> polyNames;

    ASTStruct* next=0;

    void print(AST* ast, int depth);
};
struct ASTEnum {
    TokenRange tokenRange{};
    std::string* name=0;
    struct Member {
        std::string name;
        int enumValue=0;
    };
    std::vector<Member> members{};
    
    bool getMember(const std::string& name, int* out);

    ASTEnum* next=0;
    void print(AST* ast, int depth);  
};
struct ASTFunction {
    TokenRange tokenRange{};
    std::string* name=0;
    struct Arg{
        std::string name;
        TypeId typeId;
        int offset=0;
        ASTExpression* defaultValue=0;
    };
    std::vector<Arg> arguments;
    int argSize=0;

    struct ReturnType{
        ReturnType(TypeId id) : typeId(id) {}
        TypeId typeId;
        int offset=0;
    };
    std::vector<ReturnType> returnTypes;
    int returnSize=0;

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
    
    ASTStruct* structsTail = 0;
    ASTEnum* enumsTail = 0;
    ASTFunction* functionsTail = 0;
    ASTStatement* statementsTail = 0;

    void add(ASTStruct* astStruct);
    void add(ASTStatement* astStatement);
    void add(ASTFunction* astFunction);
    void add(ASTEnum* astEnum);

    void print(AST* ast, int depth);
};
struct AST {
    static AST* Create();
    static void Destroy(AST* ast);
    void cleanup();
    
    ASTBody* mainBody=0;
    
    std::unordered_map<std::string, TypeInfo*> typeInfos;
    std::unordered_map<int, TypeInfo*> typeInfosMap;
    static const TypeId NEXT_ID = 0x100;
    static const TypeId POINTER_BIT = 0x04000000;
    static const TypeId SLICE_BIT = 0x08000000;
    TypeId nextTypeIdId=NEXT_ID;
    TypeId nextPointerTypeId=POINTER_BIT;
    TypeId nextSliceTypeId=SLICE_BIT;
    
    void addTypeInfo(const std::string& name, TypeId id, int size=0);
    // creates a new type if needed
    TypeInfo* getTypeInfo(const std::string& name, bool dontCreate=false);
    TypeInfo* getTypeInfo(TypeId id);

    static bool IsPointer(TypeId id);
    static bool IsPointer(Token& token);
    static bool IsSlice(TypeId id);
    static bool IsSlice(Token& token);
    // true if id is one of u8-64, i8-64
    static bool IsInteger(TypeId id);
    // will return false for non number types
    static bool IsSigned(TypeId id);
    
    // body is now owned by AST. Do not use it.
    void appendToMainBody(ASTBody* body);

    // std::vector<std::string> constStrings;

    ASTBody* createBody();
    ASTFunction* createFunction(const std::string& name);
    ASTStruct* createStruct(const std::string& name);
    ASTEnum* createEnum(const std::string& name);
    ASTStatement* createStatement(int type);
    ASTExpression* createExpression(TypeId type);
    
    void destroy(ASTBody* body);
    void destroy(ASTFunction* function);
    void destroy(ASTStruct* astStruct);
    void destroy(ASTEnum* astEnum);
    void destroy(ASTStatement* statement);
    void destroy(ASTExpression* expression);

    void print(int depth = 0);
};
const char* TypeIdToStr(int type);