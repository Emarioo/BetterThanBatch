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
    
    AST_STRING, // converted to another type, probably char[]
    AST_NULL, // converted to void*
    
    // TODO: should these be moved somewhere else?
    AST_VAR, // TODO: Also used when refering to enums. Change the name?
    AST_FNCALL,
    AST_SIZEOF,
    
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

    AST_REFER, // reference
    AST_DEREF, // dereference

    AST_OPERATION_COUNT,
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
        VIRTUAL = 0x4, // polymorphic, may be pointless
        TYPE_MASK = 0x1 | 0x2 | 0x4,
        POINTER_MASK = 0x8 | 0x10,
        POINTER_SHIFT = 3,
        VALID_MASK = 0x80,
    };
    u16 _infoIndex0 = 0;
    u8 _infoIndex1 = 0;
    u8 _flags = 0;

    bool operator!=(TypeId type) const {
        return !(*this == type);
    }
    bool operator==(TypeId type) const {
        // Assert(!isVirtual() && !type.isVirtual());
        return _flags == type._flags && _infoIndex0 == type._infoIndex0 && _infoIndex1 == type._infoIndex1;
    }
    bool operator==(PrimitiveType primitiveType) const {
        return *this == TypeId(primitiveType);
    }
    bool operator==(OperationType t) const {
        return *this == TypeId(t);
    }
    bool isValid() const {
        return (_flags & VALID_MASK) != 0;
    }
    bool isString() const { return (_flags & TYPE_MASK) == STRING; }
    bool isVirtual() const { return (_flags & TYPE_MASK) == VIRTUAL; }
    TypeId baseType() {
        TypeId out = *this;
        out._flags = out._flags & ~POINTER_MASK;
        return out; 
    }
    bool isPointer() const {
        return (_flags & POINTER_MASK) != 0;
    }
    void setPointerLevel(u32 level) {
        Assert(level<=(POINTER_MASK>>POINTER_SHIFT)); 
        _flags = (_flags & ~POINTER_MASK) | ((level<<POINTER_SHIFT) & POINTER_MASK);
    }
    u32 getPointerLevel() const { return (_flags & POINTER_MASK)>>POINTER_SHIFT; }
    u32 getId() const { return (u32)_infoIndex0 | ((u32)_infoIndex1<<8); }
    // TODO: Rename to something better
    bool isNormalType() const { return isValid() && !isString() && !isPointer() && !isVirtual(); }
};
// ASTStruct can have multiple of these per
// polymorphic instantiation.
struct StructImpl {
    int size=0;
    int alignedSize=0;
    struct Member {
        TypeId typeId={};
        int offset=0;
    };
    std::vector<Member> members{};
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
    StructImpl* structImpl=0;
    ASTEnum* astEnum=0;

    ScopeId scopeId = 0;
    
    struct MemberData {
        TypeId typeId;
        int index; 
        int offset;
    };

    u32 getSize();
    // 1,2,4,8
    u32 alignedSize();
    MemberData getMember(const std::string& name);
    MemberData getMember(int index);
};
struct FuncImpl {
    std::string name;
    struct Arg {
        TypeId typeId;
        int offset=0;
    };
    std::vector<Arg> arguments;
    int argSize=0;

    struct ReturnValue{
        TypeId typeId;
        int offset=0;
    };
    std::vector<ReturnValue> returnTypes;
    int returnSize=0;
    i64 address = 0; // Set by generator
    std::vector<TypeId> polyIds;
};
struct Identifier {
    Identifier() {}
    enum Type {
        VAR, FUNC
    };
    Type type=VAR;
    std::string name{};
    ScopeId scopeId;
    int varIndex;
    ASTFunction* astFunc=0;
    FuncImpl* funcImpl = 0;
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
    ScopeId parent = 0;

    std::unordered_map<std::string, ScopeId> nameScopeMap;
    std::unordered_map<std::string, TypeInfo*> nameTypeMap;

    std::unordered_map<std::string, Identifier> identifierMap;

    std::vector<ScopeInfo*> usingScopes;

    // Returns the full namespace.
    // Name of parent scopes are concatenated.
    std::string getFullNamespace(AST* ast);
    
};
// template<>
// struct std::hash<TypeId> {
//     std::size_t operator()(TypeId const& s) const noexcept {
//         return s.getId();
//     }
// };
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
        BREAK,
        CONTINUE,
        CALL,
        USING,
        // ALIAS,
        // TYPEALIAS,
        BODY,
        DEFER,
    };
    TokenRange tokenRange{};
    int type = 0;
    int opType = 0;

    std::string* name=0;
    std::string* alias=0;
    TypeId typeId={};
    ASTExpression* lvalue=0;
    ASTExpression* rvalue=0;
    ASTScope* body=0;
    ASTScope* elseBody=0;

    ASTStatement* next=0;

    int _; // offset size to avoid collision with ASTSTatement

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

        ASTExpression* defaultValue=0;
    };
    std::vector<Member> members{};
    struct PolyArg {
        Token name{};
        TypeInfo* virtualType = nullptr;
    };
    std::vector<PolyArg> polyArgs;
    
    StructImpl baseImpl{};
    State state=TYPE_EMPTY;

    ScopeId scopeId=0;

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
    struct Arg {
        std::string name;
        ASTExpression* defaultValue=0;
    };
    std::vector<Arg> arguments;

    struct PolyArg {
        Token name{};
        TypeInfo* virtualType = nullptr;
    };
    std::vector<PolyArg> polyArgs;
    FuncImpl baseImpl;
    std::vector<FuncImpl*> polyImpls;

    ScopeId scopeId=0;
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
    //-- Creation and destruction
    static AST* Create();
    static void Destroy(AST* ast);
    void cleanup();
    
    //-- Scope stuff
    ScopeId globalScopeId=0;
    std::vector<ScopeInfo*> _scopeInfos; // TODO: Use a bucket array
    ScopeInfo* createScope();
    ScopeInfo* getScope(ScopeId id);
    // Searches specified scope for a scope with a certain name.
    // Does not check parent scopes.
    // Argument name can consist of multiple named scopes with :: (GameLib::Math::Vector)
    // Returns nullptr pointer if named scope wasn't found or if scopeId isn't associated with a scope.
    ScopeInfo* getScope(Token name, ScopeId scopeId);
    // Same as getScope but searches curent and parent scopes in order. (scopeId -> scopeId.parent -> scopeId.parent.parent -> globalScope)
    // Returns nullptr pointer if named scope wasn't found or if any encountered scopeId isn't associated with a scope.
    ScopeInfo* getScopeFromParents(Token name, ScopeId scopeId);

    //-- Types
    std::vector<Token> _typeTokens;
    TypeId getTypeString(Token name);
    Token getTokenFromTypeString(TypeId typeString);
    // typeString must be a string type id.
    TypeId convertToTypeId(TypeId typeString, ScopeId scopeId);

    std::vector<TypeInfo*> _typeInfos; // TODO: Use a bucket array
    TypeInfo* createType(Token name, ScopeId scopeId);
    TypeInfo* createPredefinedType(Token name, ScopeId scopeId, TypeId id, u32 size=0);
    // isValid() of the returned TypeId will be false if
    // type couldn't be converted.
    TypeId convertToTypeId(Token typeString, ScopeId scopeId);
    // pointers NOT allowed
    TypeInfo* convertToTypeInfo(Token typeString, ScopeId scopeId) {
        return getTypeInfo(convertToTypeId(typeString,scopeId));
    }
    // pointers are allowed
    TypeInfo* getBaseTypeInfo(TypeId id);
    // pointers are NOT allowed
    TypeInfo* getTypeInfo(TypeId id);
    std::string typeToString(TypeId typeId);

    TypeId ensureNonVirtualId(TypeId id);

    //-- OTHER
    ASTScope* mainBody=0;

    std::vector<std::string*> tempStrings;
    std::string* createString();

    // static const u32 NEXT_ID = 0x100;
    u32 nextTypeId=AST_OPERATION_COUNT;
    
    std::vector<VariableInfo> variables;

    // Returns nullptr if variable already exists or if scopeId is invalid
    VariableInfo* addVariable(ScopeId scopeId, const std::string& name);
    // Returns nullptr if variable already exists or if scopeId is invalid
    // FunctionInfo* addFunction(ScopeId scopeId, const std::string& name);
    Identifier* addFunction(ScopeId scopeId, const std::string& name, ASTFunction* astFunc);
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

    u32 getTypeSize(TypeId typeId);
    u32 getTypeAlignedSize(TypeId typeId);

    struct ConstString {
        u32 length = 0;
        u32 address = 0;
    };
    std::unordered_map<std::string, ConstString> constStrings;

    // Must be used after TrimPointer
    static Token TrimPolyTypes(Token token, std::vector<Token>* outPolyTypes = nullptr);
    static Token TrimNamespace(Token token, Token* outNamespace = nullptr);
    static Token TrimPointer(Token& token, u32* level = nullptr);
    static Token TrimBaseType(Token token, Token* outNamespace, u32* level, std::vector<Token>* outPolyTypes, Token* typeName);
    // true if id is one of u8-64, i8-64
    static bool IsInteger(TypeId id);
    // will return false for non number types
    static bool IsSigned(TypeId id);

    // content in body is moved and the body is destroyed. DO NOT USE IT AFTERWARDS.
    void appendToMainBody(ASTScope* body);
    
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