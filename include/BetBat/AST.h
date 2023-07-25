#pragma once

#include "BetBat/Tokenizer.h"
#include "Engone/Alloc.h"
#include "BetBat/Util/Array.h"

#include "BetBat/x64_Converter.h"

typedef u32 ScopeId;
struct ASTStruct;
struct ASTEnum;
struct ASTFunction;
struct AST;
struct FuncImpl;
struct ASTExpression;
struct ASTScope;

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
    
    AST_STRING, // converted to char[]
    AST_NULL, // converted to void*

    AST_TRUE_PRIMITIVES,

    AST_POLY,

    // TODO: should these be moved somewhere else?
    AST_ID, // variable, enum, type
    AST_FNCALL,
    AST_SIZEOF,
    AST_NAMEOF,
    
    AST_PRIMITIVE_COUNT, // above types are true with isValue
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

    AST_RANGE,
    AST_INDEX, // index operator, arr[3] same as *(arr + 3*sizeof Item) unless overloaded
    AST_INCREMENT,
    AST_DECREMENT,

    AST_CAST,
    AST_MEMBER,
    AST_INITIALIZER,
    AST_SLICE_INITIALIZER,
    AST_FROM_NAMESPACE,
    AST_ASSIGN,

    AST_REFER, // reference
    AST_DEREF, // dereference

    AST_OPERATION_COUNT,
};
struct ASTNode {
    TokenRange tokenRange{};
    // TODO: Some of these do not need to exist in
    // every type of node
    bool hidden=false;
    bool reverse=false;
    bool pointer=false;
    LinkConventions linkConvention = LinkConventions::NONE;
    CallConventions callConvention = BETCALL;

    // A lot of places need to know whether a function has a body.
    // When function should have a body or not has changed a lot recently
    // and I have needed to rewrite a lot. Having the requirement abstracted in
    // a function will prevent some of the changes you would need to make.
    bool needsBody() { return linkConvention == LinkConventions::NONE;}
};
typedef u32 PolyId;
template<class T>
struct PolyVersions {
    PolyVersions() = default;
    ~PolyVersions(){ _array.cleanup(); }
    // TODO: Allocations are should be controlled by AST.
    // Using heap for now to make things simple.

    DynamicArray<T> _array{};

    // automatically allocates
    T& get(u32 index) {
        if(index >= _array.used) {
            Assert(("Probably a bug",index < 1000));
            Assert(_array.resize(index + 4));
        }
        return _array.get(index);
    }
    // automatically allocates
    T& operator[](u32 index) {
        return get(index);
    }
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
// struct TypeIds {
//     TypeId* typeIds=nullptr;
//     int len = 0;
//     void cleanup(){
//         if(!typeIds) return;
//         engone::Free(typeIds, len*sizeof(TypeId));
//         typeIds = nullptr;
//         len = 0;
//     }
//     void init(int len){
//         Assert(!typeIds);
//         typeIds = (TypeId*)engone::Allocate(len*sizeof(TypeId));
//         if(!typeIds)
//             return;
//         this->len = len;
//     }
// };
struct FnOverloads {
    struct Overload {
        ASTFunction* astFunc=0;
        FuncImpl* funcImpl = 0;
    };
    struct PolyOverload {
        ASTFunction* astFunc=0;
        // DynamicArray<TypeId> argTypes{};
    };
    DynamicArray<Overload> overloads{};
    DynamicArray<Overload> polyImplOverloads{};
    DynamicArray<PolyOverload> polyOverloads{};
    // Do not modify overloads while using the returned pointer
    // TODO: Use BucketArray to allow modifications
    Overload* getOverload(AST* ast, DynamicArray<TypeId>& argTypes, ASTExpression* fncall, bool canCast = false);
    Overload* getOverload(AST* ast, DynamicArray<TypeId>& argTypes, DynamicArray<TypeId>& polyArgs, ASTExpression* fncall);
    // Get base polymorphic overload which can match with the typeIds.
    // You want to generate the real overload afterwards.
    // ASTFunction* getPolyOverload(AST* ast, DynamicArray<TypeId>& typeIds, DynamicArray<TypeId>& polyTypes);
    
    // FuncImpl can be null and probably will be most of the time
    // when you call this.
    void addOverload(ASTFunction* astFunc, FuncImpl* funcImpl);
    Overload* addPolyImplOverload(ASTFunction* astFunc, FuncImpl* funcImpl);
    void addPolyOverload(ASTFunction* astFunc);

    // FuncImpl* createImpl();
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
    DynamicArray<Member> members{};
    
    DynamicArray<TypeId> polyArgs;

    // std::unordered_map<std::string, FnOverloads> methods;
    
    // FnOverloads::Overload* getMethod(const std::string& name, std::vector<TypeId>& typeIds);
    // void addPolyMethod(const std::string& name, ASTFunction* func, FuncImpl* funcImpl);
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
    StructImpl* structImpl=0; // nullptr means pure/base poly type 
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
    StructImpl* getImpl();
};
struct ASTFunction;
struct FuncImpl {
    std::string name;
    struct Spot {
        TypeId typeId{};
        int offset=0;
    };
    DynamicArray<Spot> argumentTypes;
    DynamicArray<Spot> returnTypes;
    int argSize=0;
    int returnSize=0;
    i64 address = 0; // Set by generator
    u32 polyVersion=-1;
    DynamicArray<TypeId> polyArgs;
    StructImpl* structImpl = nullptr;
    void print(AST* ast, ASTFunction* astFunc);
    static const u64 ADDRESS_INVALID = 0; // undefined or not address that hasn't been set
    static const u64 ADDRESS_EXTERNAL = 1;
};
struct Identifier {
    Identifier() {}
    enum Type {
        VAR, FUNC
    };
    Type type=VAR;
    std::string name{};
    ScopeId scopeId=0;
    u32 varIndex=0;
    FnOverloads funcOverloads{};
};
struct VariableInfo {
    i32 frameOffset = 0;
    i32 memberIndex = -1; // negative = normal variable, positive = variable in struct
    TypeId typeId=AST_VOID;
    // ScopeId scopeId = 0; // do you really need this variable? can you get the identifier instead?

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
const char* OpToStr(OperationType op, bool null = false);
struct ASTExpression : ASTNode {
    ASTExpression() {
        // printf("hum\n");
        // engone::log::out << "default init\n";
        // *(char*)0 = 9;
    }
    // ASTExpression() { memset(this,0,sizeof(*this)); }
    // ASTExpression() : left(0), right(0), castType(0) { }
    // Token token{};
    bool isValue=false;
    TypeId typeId = {}; // not polymorphic, primitive or operation
    
    union {
        i64 i64Value=0;
        float f32Value;
        bool boolValue;
        char charValue;
    };
    Token name{};
    Token namedValue={}; // Used for named arguments (fncall or initializer). Empty means never specified or used.
    ASTExpression* left=0; // FNCALL has arguments in order left to right
    ASTExpression* right=0;
    TypeId castType={};
    OperationType assignOpType = (OperationType)0;

    // TypeId finalType={}; // evaluated by type checker

    DynamicArray<ASTExpression*>* args=nullptr; // fncall or initialiser
    u32 nonNamedArgs = 0;

    // PolyVersions<DynamicArray<TypeId>> versions_argTypes{};

    // The contents of overload is stored here. This is not a pointer since
    // the array containing the overload may be resized.
    PolyVersions<FnOverloads::Overload> versions_overload{};

    // you could use a union with some of these to save memory
    // outTypeSizeof and castType could perhaps be combined?
    PolyVersions<int> versions_constStrIndex{};
    PolyVersions<TypeId> versions_outTypeSizeof{};
    PolyVersions<TypeId> versions_castType{};

    void printArgTypes(AST* ast, DynamicArray<TypeId>& argTypes);

    // ASTExpression* next=0;
    void print(AST* ast, int depth);
};
struct ASTStatement : ASTNode {
    // ASTStatement() { memset(this,0,sizeof(*this)); }
    enum Type {
        EXPRESSION=0,
        ASSIGN,
        IF,
        WHILE,
        FOR,
        RETURN,
        BREAK,
        CONTINUE,
        USING,
        BODY,
        DEFER,

        STATEMENT_TYPE_COUNT,
    };
    ~ASTStatement(){
        returnValues.~DynamicArray<ASTExpression*>();
    }
    Type type = EXPRESSION;
    // int opType = 0;
    struct VarName {
        Token name{}; // TODO: Does not store info about multiple tokens, error message won't display full string
        TypeId assignString{};
        int arrayLength=-1;
        PolyVersions<TypeId> versions_assignType{};
        // true if variable declares a new variable (it does if it has a type)
        // false if variable implicitly declares a new type OR assigns to an existing variable
        bool declaration(){
            return assignString.isValid();
        }
    };
    bool rangedForLoop=false; // otherwise sliced for loop
    DynamicArray<VarName> varnames;
    std::string* alias = nullptr;
    // TypeId typeId={};

    // true if bodies and expressions can be used.
    // false if returnValues should be used.
    bool hasNodes(){
        // Return is the only one that uses return values
        return type != ASTStatement::RETURN;
    }
    // with a union you have to use hasNodes before using the expressions.
    // this is annoying so I am not using a union but you might want to
    // to save 24 bytes.
    union {
        struct {
            ASTExpression* firstExpression;
            ASTScope* firstBody;
            ASTScope* secondBody;
        };
        // DynamicArray<ASTExpression*> returnValues{};
    };
    DynamicArray<ASTExpression*> returnValues{};

    PolyVersions<DynamicArray<TypeId>> versions_expresssionTypes; // types from firstExpression

    bool sharedContents = false; // this node is not the owner of it's nodes.

    void print(AST* ast, int depth);
};
struct ASTStruct : ASTNode {
    enum State {
        TYPE_EMPTY,
        TYPE_COMPLETE,
        TYPE_CREATED,
        TYPE_ERROR, 
    };
    Token name{};
    std::string polyName="";
    struct Member {
        Token name;
        ASTExpression* defaultValue=0;
        TypeId stringType{};
    };
    DynamicArray<Member> members{};
    struct PolyArg {
        Token name{};
        TypeInfo* virtualType = nullptr;
    };
    DynamicArray<PolyArg> polyArgs;

    StructImpl* createImpl();
    
    State state=TYPE_EMPTY;

    StructImpl* nonPolyStruct = nullptr;

    ScopeId scopeId=0;

    std::unordered_map<std::string, FnOverloads> _methods;
    
    // FnOverloads* addMethod(const std::string& name);
    FnOverloads& getMethod(const std::string& name);
    // void addPolyMethod(const std::string& name, ASTFunction* func, FuncImpl* funcImpl);

    DynamicArray<ASTFunction*> functions{};
    // ASTFunction* functions = 0;
    // ASTFunction* functionsTail = 0;
    // done in parser stage
    void add(ASTFunction* func);

    bool isPolymorphic() {
        return polyArgs.size()!=0;
    }

    void print(AST* ast, int depth);
};
struct ASTEnum : ASTNode {
    TokenRange tokenRange{};
    Token name{};
    struct Member {
        Token name{};
        int enumValue=0;
    };
    DynamicArray<Member> members{};
    
    bool getMember(const Token& name, int* out);

    void print(AST* ast, int depth);  
};
struct ASTFunction : ASTNode {
    Token name{};
    
    struct PolyArg {
        Token name{};
        TypeInfo* virtualType = nullptr;
    };
    struct Arg {
        Token name{};
        ASTExpression* defaultValue=0;
        TypeId stringType={};
    };
    struct Ret {
        TokenRange valueToken{}; // for error messages
        TypeId stringType;
    };
    DynamicArray<PolyArg> polyArgs;
    DynamicArray<Arg> arguments; // you could rename to parameters
    DynamicArray<Ret> returnValues; // string type
    u32 nonDefaults=0; // used when matching overload, having it here avoids recalculations of it

    DynamicArray<FuncImpl*> _impls{};
    FuncImpl* createImpl();
    const DynamicArray<FuncImpl*>& getImpls(){
        return _impls;
    }

    u32 polyVersionCount=0;
    ASTStruct* parentStruct = nullptr;

    ScopeId scopeId=0;
    ASTScope* body=0;

    // does not consider the method's struct
    bool isPolymorphic(){
        return polyArgs.size()!=0;
    }

    void print(AST* ast, int depth);
};
struct ASTScope : ASTNode {
    std::string* name = 0; // namespace
    ScopeId scopeId=0;
    // ASTScope* next = 0;
    enum Type {
        BODY,
        NAMESPACE,
    };
    Type type = BODY;

    DynamicArray<ASTStruct*> structs{};
    void add(ASTStruct* astStruct, ASTStruct* tail = 0);
    
    DynamicArray<ASTEnum*> enums{};
    void add(ASTEnum* astEnum, ASTEnum* tail = 0);

    DynamicArray<ASTFunction*> functions{};
    void add(ASTFunction* astFunction, ASTFunction* tail = 0);
    
    DynamicArray<ASTScope*> namespaces{};
    void add(ASTScope* astNamespace, AST* ast, ASTScope* tail = 0);
    
    DynamicArray<ASTStatement*> statements{};    
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
    ScopeInfo* createScope(ScopeId parentScope);
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
        return getTypeInfo(convertToTypeId(typeString,scopeId).baseType());
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
    
    std::vector<VariableInfo*> variables;

    //-- Identifiers and variables
    // Searches for identifier with some name. It does so recursively
    Identifier* findIdentifier(ScopeId startScopeId, const Token& name, bool searchParentScopes = true);
    VariableInfo* identifierToVariable(Identifier* identifier);

    // Returns nullptr if variable already exists or if scopeId is invalid
    VariableInfo* addVariable(ScopeId scopeId, const Token& name, bool shadowPreviousVariables=false);
    // Returns nullptr if variable already exists or if scopeId is invalid
    Identifier* addIdentifier(ScopeId scopeId, const Token& name, bool shadowPreviousIdentifier=false);

    void removeIdentifier(ScopeId scopeId, const Token& name);
    void removeIdentifier(Identifier* identifier);
        
    u32 getTypeSize(TypeId typeId);
    u32 getTypeAlignedSize(TypeId typeId);

    bool castable(TypeId from, TypeId to);

    struct ConstString {
        u32 length = 0;
        u32 address = 0;
    };
    std::unordered_map<std::string, u32> _constStringMap;
    DynamicArray<ConstString> _constStrings;

    // outIndex is used with getConstString(u32)
    ConstString& getConstString(const std::string& str, u32* outIndex);
    ConstString& getConstString(u32 index);

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
    ASTFunction* createFunction();
    ASTStruct* createStruct(const Token& name);
    ASTEnum* createEnum(const Token& name);
    ASTStatement* createStatement(ASTStatement::Type type);
    ASTExpression* createExpression(TypeId type);
    // You may also want to call some additional functions to properly deal with
    // named scopes so types are properly evaluated. See ParseNamespace for an example.
    ASTScope* createNamespace(const Token& name);
    
    void destroy(ASTScope* astScope);
    void destroy(ASTFunction* function);
    void destroy(ASTStruct* astStruct);
    void destroy(ASTEnum* astEnum);
    void destroy(ASTStatement* statement);
    void destroy(ASTExpression* expression);

    void print(int depth = 0);
    void printTypesFromScope(ScopeId scopeId, int scopeLimit=-1);
};
const char* TypeIdToStr(int type);