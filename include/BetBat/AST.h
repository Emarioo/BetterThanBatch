#pragma once

#include "BetBat/Tokenizer.h"
#include "Engone/Alloc.h"

typedef u32 ScopeId;
struct ASTStruct;
struct ASTEnum;
struct ASTFunction;
struct AST;
enum PrimitiveType : u32 {
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
enum OperationType : u32 {
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
    // AST_DEFER, // defer
};
struct TypeId {
    TypeId() = default;
    TypeId(PrimitiveType type) : _infoIndex0((u32)type), _flags(VALID_MASK | PRIMITIVE) {}
    TypeId(OperationType type) : _infoIndex0((u32)type), _flags(VALID_MASK | PRIMITIVE) {}
    static TypeId Create(u32 id) {
        TypeId out={}; 
        out._flags = VALID_MASK; // TODO: ENUM or STRUCT?
        out._infoIndex0 = id&0xFFFF;
        out._infoIndex1 = id>>16;
        return out; 
    }
    static TypeId CreateString(u32 index) {
        TypeId out = {};
        out._flags = VALID_MASK | STRING;
        out._infoIndex0 = index&0xFFFF;
        out._infoIndex1 = index>>16;
        return out;
    }
    enum TypeType : u32 {
        PRIMITIVE = 0x0,
        STRING = 0x1,
        STRUCT = 0x2,
        ENUM = 0x3,
        TYPE_MASK = 0x1 | 0x2,
        POINTER_MASK = 0x4 | 0x8,
        POINTER_BIT = 3,
        VALID_MASK = 0x80,
    };
    u16 _infoIndex0 = 0;
    u8 _infoIndex1 = 0;
    u8 _flags = 0;

    bool operator!=(TypeId type) const {
        return !(*this == type);
    }
    bool operator==(TypeId type) const {
        return _flags == type._flags && _infoIndex0 == type._infoIndex0 && _infoIndex1 == type._infoIndex1;
    }
    bool operator==(PrimitiveType primitiveType) const {
        return *this == TypeId(primitiveType);
    }
    bool operator==(OperationType t) const {
        return *this == TypeId(t);
    }
    bool isValid() {
        return (_flags & VALID_MASK) != 0;
    }
    // TODO: Rename to something better
    bool isNormalType() { return (_flags & TYPE_MASK) != STRING && !isPointer(); }
    TypeId baseType() {
        TypeId out = *this;
        out._flags = out._flags & ~POINTER_MASK;
        return out; 
    }
    bool isPointer() const {
        return (_flags & POINTER_MASK) != 0;
    }
    void setPointerLevel(u32 level) {
        Assert(level<=(POINTER_MASK>>POINTER_BIT)); 
        _flags = (_flags & ~POINTER_MASK) | ((level<<POINTER_BIT) & POINTER_MASK);
    }
    bool isString() const { return (_flags & TYPE_MASK) == STRING; }
    u32 getPointerLevel() const { return (_flags & POINTER_MASK)>>POINTER_BIT; }
    u32 getId() const { return (u32)_infoIndex0 | ((u32)_infoIndex1<<8); }
};
struct TypeInfo {
    TypeInfo(const std::string& name, TypeId id, u32 size=0) :  name(name), id(id), _size(size) {}
    TypeInfo(TypeId id, u32 size=0) : id(id), _size(size) {}
    std::string name;
    TypeId id={};
    u32 _size=0;
    u32 _alignedSize=0;
    u32 arrlen=0;
    ASTStruct* astStruct=0;
    ASTEnum* astEnum=0;
    std::vector<TypeId> polyTypes;
    TypeInfo* polyOrigin=0;

    std::string getFullType(AST* ast); // namespace too
    
    ScopeId scopeId = 0;
    
    struct MemberOut { TypeId typeId;int index;};

    u32 getSize();
    // 1,2,4,8
    u32 alignedSize();
    MemberOut getMember(const std::string& name);
};
struct Identifier {
    Identifier() {}
    enum Type {
        VAR, FUNC
    };
    Type type=VAR;
    std::string name{};
    union {
        int varIndex;
        ASTFunction* astFunc=0;
    };
    static const u64 INVALID_FUNC_ADDRESS = 0;
};
struct VariableInfo {
    i32 frameOffset = 0;
    i32 memberIndex = -1; // negative = normal variable, positive = variable in struct
    TypeId typeId=AST_VOID;
};
struct ScopeInfo {
    ScopeInfo(ScopeId id) : id(id) {}
    std::string name; // namespace
    ScopeId id = 0;
    std::unordered_map<std::string, TypeInfo*> nameTypeMap;
    std::unordered_map<std::string, ScopeInfo*> nameScopeMap;

    std::unordered_map<std::string, Identifier> identifierMap;

    std::string getFullNamespace(AST* ast);
    
    ScopeId parent = 0;
};
template<>
struct std::hash<TypeId> {
    std::size_t operator()(TypeId const& s) const noexcept {
        return s.getId();
    }
};
struct AST;
const char* OpToStr(OperationType op);
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
    TypeId typeId = {};
    
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
    TypeId castType={};
    
    ASTExpression* next=0;
    void print(AST* ast, int depth);
};
struct ASTScope;
struct ASTFunction;
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
        DEFER,
    };
    TokenRange tokenRange{};
    int type = 0;
    int opType = 0;
    // assign
    std::string* name=0;
    std::string* alias=0;
    TypeId typeId={};
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
        TYPE_CREATED,
        TYPE_ERROR, 
    };
    TokenRange tokenRange{};
    std::string name="";
    struct Member {
        std::string name;
        TypeId typeId={};
        int offset=0;
        int polyIndex=-1;
        ASTExpression* defaultValue=0;
    };
    std::vector<Member> members{};
    int size=0;
    int alignedSize=0;
    State state=TYPE_EMPTY;
    std::vector<std::string> polyNames;

    ASTFunction* functions = 0;
    ASTFunction* functionsTail = 0;

    void add(ASTFunction* func);
    ASTFunction* getFunction(const std::string& name);

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
    i64 address = 0; // calculated in Generator

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
    
    // void convertToNamespace(const std::string& name);

    ASTStruct* structs = 0;
    ASTStruct* structsTail = 0;
    void add(ASTStruct* astStruct, ASTStruct* tail = 0);
    
    ASTEnum* enums = 0;
    ASTEnum* enumsTail = 0;
    void add(ASTEnum* astEnum, ASTEnum* tail = 0);

    ASTFunction* functionsTail = 0;
    ASTFunction* functions = 0;
    void add(ASTFunction* astFunction, ASTFunction* tail = 0);
    
    ASTScope* namespaces = 0;
    ASTScope* namespacesTail = 0;
    void add(ASTScope* astNamespace, AST* ast, ASTScope* tail = 0);
    
    ASTStatement* statements = 0;
    ASTStatement* statementsTail = 0;
    void add(ASTStatement* astStatement, ASTStatement* tail = 0);

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

    static const u32 NEXT_ID = 0x100;
    // static const TypeId SLICE_BIT = 0x08000000;
    u32 nextTypeId=NEXT_ID;
    // TypeId nextSliceTypeId=SLICE_BIT;
    
    std::vector<VariableInfo> variables;
    // std::vector<FunctionInfo> functions;

    // Returns nullptr if variable already exists or if scopeId is invalid
    VariableInfo* addVariable(ScopeId scopeId, const std::string& name);
    // Returns nullptr if variable already exists or if scopeId is invalid
    // FunctionInfo* addFunction(ScopeId scopeId, const std::string& name);
    Identifier* addFunction(ScopeId scopeId, ASTFunction* astFunc);
    // Returns nullptr if variable already exists or if scopeId is invalid
    Identifier* addIdentifier(ScopeId scopeId, const std::string& name);
    
    // Pointer may become invalid if calling other functions interracting with
    // the map of identifiers
    // name argument works with namespacing
    Identifier* getIdentifier(ScopeId scopeId, const std::string& name);
    VariableInfo* getVariable(Identifier* id);
    // Function returns: id->astFunction
    // The reason it's used is abstract away how it works in case it changes in the future.
    ASTFunction* getFunction(Identifier* id);
    
    void removeIdentifier(ScopeId scopeId, const std::string& name);

    std::unordered_map<TypeId, TypeInfo*> typeInfos;
    void addTypeInfo(ScopeId scopeId, Token name, TypeId id, u32 size=0);
    TypeInfo* addType(ScopeId scopeId, Token name);
    // Creates a new type if needed
    // scopeId is the current scope. Parent scopes will also be searched.
    TypeInfo* getTypeInfo(ScopeId scopeId, Token name);
    // scopeId is the current scope. Parent scopes will also be searched.
    TypeInfo* getTypeInfo(TypeId id);

    // FunctionInfo* addFunction(ScopeId scopeId, ASTFunction* func);
    // FunctionInfo* getFunction(ScopeId scopeId, Token name);

    u32 getTypeSize(TypeId typeId);
    u32 getTypeAlignedSize(TypeId typeId);

    std::string typeToString(TypeId typeId);

    TypeId getTypeId(ScopeId scopeId, Token typeString, bool* success=0);

    // Retrieves namespace of specified scopeId. Parents are not checked.
    // Example of what name can be: GameLibrary::Math::Vector
    ScopeInfo* getNamespace(ScopeId scopeId, const std::string name);
    
    std::vector<std::string> typeStrings;
    TypeId addTypeString(Token name);
    const std::string& getTypeString(TypeId typeId);

    // static bool IsPointer(TypeId id);
    // static bool IsPointer(Token& token);
    // static u32 GetPointerLevel(Token& token);
    static Token TrimNamespace(Token token, Token* outNamespace = nullptr);
    static Token TrimPointer(Token& token, u32* level = nullptr);
    // static bool IsSlice(TypeId id);
    // static bool IsSlice(Token& token);
    // true if id is one of u8-64, i8-64
    static bool IsInteger(TypeId id);
    // will return false for non number types
    static bool IsSigned(TypeId id);

    // content in body is moved and the body is destroyed. DO NOT USE IT AFTERWARDS.
    void appendToMainBody(ASTScope* body);

    // std::vector<std::string> constStrings;

    ASTScope* createBody();
    ASTFunction* createFunction(const std::string& name);
    ASTStruct* createStruct(const std::string& name);
    ASTEnum* createEnum(const std::string& name);
    ASTStatement* createStatement(int type);
    ASTExpression* createExpression(TypeId type);
    // You may also want to call some additional functions to properly deal with
    // named scopes so types are properly evaluated. See ParseNamespace for an example.
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