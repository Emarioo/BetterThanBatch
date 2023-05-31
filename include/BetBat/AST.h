#pragma once

#include "BetBat/Tokenizer.h"
#include "Engone/Alloc.h"

typedef int TypeId;
typedef int ScopeId;
struct ASTStruct;
struct ASTEnum;
struct AST;
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

    std::string getFullType(AST* ast); // namespace too
    
    ScopeId scopeId = 0;
    
    struct MemberOut { TypeId typeId;int index;};

    int size();
    // 1,2,4,8
    int alignedSize();
    MemberOut getMember(const std::string& name);
};
struct ScopeInfo {
    ScopeInfo(ScopeId id) : id(id) {}
    std::string name; // namespace
    ScopeId id = 0;
    std::unordered_map<std::string,TypeInfo*> nameTypeMap;
    std::unordered_map<std::string,ScopeInfo*> nameScopeMap;

    std::string getFullNamespace(AST* ast);
    
    ScopeId parent = 0;
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
    
    // AST_STRING, // converted to another type, probably char[]
    AST_NULL, // usually converted to cast<void*> 0
    
    // TODO: should these be moved somewhere else?
    AST_VAR, // Also used when refering to enums. Change the name?
    AST_FNCALL,
    
    AST_PRIMITIVE_COUNT,
};
enum OperationType : int {
    AST_ADD=AST_PRIMITIVE_COUNT,  
    AST_SUB,
    AST_MUL,
    AST_DIV,
    AST_MODULUS,
    
    AST_EQUAL,
    AST_NOT_EQUAL,
    AST_LESS,
    AST_GREATER,
    AST_LESS_EQUAL,
    AST_GREATER_EQUAL,
    AST_AND,
    AST_OR,
    AST_NOT,

    AST_BAND,
    AST_BOR,
    AST_BXOR,
    AST_BNOT,
    AST_BLSHIFT,
    AST_BRSHIFT,
    
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
struct ASTScope;
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
    int type = 0;
    int opType = 0;
    // assign
    std::string* name=0;
    std::string* alias=0;
    TypeId typeId=0;
    ASTExpression* lvalue=0;
    ASTExpression* rvalue=0;
    ASTScope* body=0;
    ASTScope* elseBody=0;
    // assignment
    // function call
    // return, continue, break
    // for, each, if

    ASTStatement* next=0;

    void print(AST* ast, int depth);
};
struct ASTStruct {
    enum State {
        TYPE_EMPTY,
        TYPE_COMPLETE,
        TYPE_CREATED     
    };
    TokenRange tokenRange{};
    std::string name="";
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
    State state=TYPE_EMPTY;
    std::vector<std::string> polyNames;

    ASTStruct* next=0;

    void print(AST* ast, int depth);
};
struct ASTEnum {
    TokenRange tokenRange{};
    std::string name="";
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
    std::string name="";
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

    ASTScope* body=0;

    ASTFunction* next=0;

    void print(AST* ast, int depth);
};
struct ASTScope {
    TokenRange tokenRange{};
    std::string* name = 0; // namespace
    ScopeId scopeId=0;
    ASTScope* next = 0;    
    enum Type {
        BODY,
        NAMESPACE,   
    };
    Type type = BODY;

    ASTStruct* structs = 0;
    ASTStruct* structsTail = 0;
    void add(ASTStruct* astStruct);
    
    ASTEnum* enums = 0;
    ASTEnum* enumsTail = 0;
    void add(ASTEnum* astEnum);

    ASTFunction* functionsTail = 0;
    ASTFunction* functions = 0;
    void add(ASTFunction* astFunction);
    
    ASTScope* namespaces = 0;
    ASTScope* namespacesTail = 0;
    void add(ASTScope* astNamespace, AST* ast);
    
    ASTStatement* statements = 0;
    ASTStatement* statementsTail = 0;
    void add(ASTStatement* astStatement);

    void print(AST* ast, int depth);
};
struct AST {
    static AST* Create();
    static void Destroy(AST* ast);
    void cleanup();
    
    ASTScope* mainBody=0;
    
    std::unordered_map<ScopeId,ScopeInfo*> scopeInfos;
    ScopeId globalScopeId=0;
    ScopeId nextScopeId=0;
    ScopeId addScopeInfo();
    ScopeInfo* getScopeInfo(ScopeId id);

    static const TypeId NEXT_ID = 0x100;
    static const TypeId POINTER_BIT = 0x04000000;
    static const TypeId SLICE_BIT = 0x08000000;
    TypeId nextTypeIdId=NEXT_ID;
    TypeId nextPointerTypeId=POINTER_BIT;
    TypeId nextSliceTypeId=SLICE_BIT;
    
    static const TypeId STRING_BIT = 0x02000000;

    std::unordered_map<TypeId, TypeInfo*> typeInfos;
    void addTypeInfo(ScopeId scopeId, const std::string& name, TypeId id, int size=0);
    // TypeInfo* addTypeInfo(ScopeId scopeId, const std::string& name);
    // Creates a new type if needed
    // scopeId is the current scope. Parent scopes will also be searched.
    TypeInfo* getTypeInfo(ScopeId scopeId, const std::string& name, bool dontCreate=false, bool forceCreate=false);
    // scopeId is the current scope. Parent scopes will also be searched.
    TypeInfo* getTypeInfo(TypeId id);

    ScopeInfo* getNamespace(ScopeId scopeId, const std::string name);
    
    std::vector<std::string> typeStrings;
    TypeId addTypeString(const std::string& name);
    const std::string& getTypeString(TypeId typeId);

    static bool IsPointer(TypeId id);
    static bool IsPointer(Token& token);
    static bool IsSlice(TypeId id);
    static bool IsSlice(Token& token);
    // true if id is one of u8-64, i8-64
    static bool IsInteger(TypeId id);
    // will return false for non number types
    static bool IsSigned(TypeId id);
    
    // body is now owned by AST. Do not use it.
    void appendToMainBody(ASTScope* body);

    // std::vector<std::string> constStrings;

    ASTScope* createBody();
    ASTFunction* createFunction(const std::string& name);
    ASTStruct* createStruct(const std::string& name);
    ASTEnum* createEnum(const std::string& name);
    ASTStatement* createStatement(int type);
    ASTExpression* createExpression(TypeId type);
    ASTScope* createNamespace(const std::string& name);
    
    void destroy(ASTScope* astScope);
    void destroy(ASTFunction* function);
    void destroy(ASTStruct* astStruct);
    void destroy(ASTEnum* astEnum);
    void destroy(ASTStatement* statement);
    void destroy(ASTExpression* expression);

    void print(int depth = 0);
};
const char* TypeIdToStr(int type);