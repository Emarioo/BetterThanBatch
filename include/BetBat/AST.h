#pragma once

#include "BetBat/Tokenizer.h"
#include "BetBat/Lexer.h"
#include "BetBat/Signals.h"

typedef u32 ScopeId;
typedef u32 ContentOrder;
#define CONTENT_ORDER_ZERO 0
#define CONTENT_ORDER_MAX (u32)-1
struct ASTStruct;
struct ASTEnum;
struct ASTFunction;
struct AST;
struct FuncImpl;
struct ASTExpression;
struct ASTScope;

enum CallConventions : u8 {
    BETCALL, // The default. Native functions use this.
    STDCALL, // Currently default x64 calling convention.
    INTRINSIC, // Special implementation. Usually a function directly coupled to an instruction.
    CDECL_CONVENTION, // Not implemented yet. CDECL is a macro and unavailable so _CONVENTION was added to the name.
    // https://www.ired.team/miscellaneous-reversing-forensics/windows-kernel-internals/linux-x64-calling-convention-stack-frame
    UNIXCALL, // System V AMD64 ABI calling convention.
};
enum LinkConventions : u8 {
    NONE=0x00, // no linkConvention export/import
    // DLLEXPORT, // .drectve section is needed to pass /EXPORT: to the linker. The exported function must use stdcall convention
    IMPORT=0x80, // external from the source code, linkConvention with static library or object files
    DLLIMPORT=0x81, // linkConvention with dll, function are renamed to __impl_
    VARIMPORT=0x82, // linkConvention with extern global variables (extern FUNCPTRTYPE someFunction;)
    NATIVE=0x10, // for interpreter or other implementation in x64 converter
};

#define IS_IMPORT(X) (X&(u8)0x80)
const char* ToString(CallConventions stuff);
const char* ToString(LinkConventions stuff);

engone::Logger& operator<<(engone::Logger& logger, CallConventions convention);
engone::Logger& operator<<(engone::Logger& logger, LinkConventions convention);

StringBuilder& operator<<(StringBuilder& builder, CallConventions convention);
StringBuilder& operator<<(StringBuilder& builder, LinkConventions convention);


enum PrimitiveType : u16 {
    AST_VOID=0,
    AST_UINT8,
    AST_UINT16,
    AST_UINT32,
    AST_UINT64,
    AST_INT8,
    AST_INT16,
    AST_INT32,
    AST_INT64,
// IMPORTANT: YOU MUST MODIFY primitive_names in AST.cpp IF YOU MAKE CHANGES HERE
    
    AST_BOOL,
    AST_CHAR,
    
    AST_FLOAT32,
    AST_FLOAT64,
    
    AST_TRUE_PRIMITIVES,

    AST_STRING, // converted to char[]
    AST_NULL, // converted to void*

    AST_FUNC_REFERENCE,

    // TODO: should these be moved somewhere else?
    AST_ID, // variable, enum, type
    AST_FNCALL,
    AST_ASM, // inline assembly
    AST_SIZEOF,
    AST_NAMEOF,
    AST_TYPEID,
    
    AST_PRIMITIVE_COUNT, // above types are true with isValue
};
enum OperationType : u16 {
    // TODO: Add BINOP (binary) and UNOP (unary) in the enum names.
    //  If you could arrange binary and unary ops so that you can determine the difference
    //  using a bitmask (op & BINARY_UNARY_MASK), that would signifcantly speed up code which currently has
    //  to check if an op is equal to any of the operations (op == AST_NOT || op == AST_INCREMENT || ...)
    //  
    AST_ADD=AST_PRIMITIVE_COUNT,  
    AST_SUB,
    AST_MUL,
    AST_DIV,
    AST_MODULUS,
    AST_UNARY_SUB,
    
    AST_EQUAL,
    AST_NOT_EQUAL,
    AST_LESS,
    AST_GREATER,
    AST_LESS_EQUAL,
    AST_GREATER_EQUAL,
    AST_AND,
    AST_OR,
    AST_NOT,
    // IMPORTANT: MODIFY operation_names in AST.cpp IF YOU MAKE A CHANGE HERE

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
extern const char* statement_names[];
extern const char* prim_op_names[];
// Preferably use these macros since the way we access
// the names may change. Perhaps prim and op will be split into
// their own tables.
#define PRIM_NAME(X) ((X) >= 0 && (X) < AST_PRIMITIVE_COUNT ? prim_op_names[X] : nullptr)
#define OP_NAME(X) (((PrimitiveType)X) >= AST_PRIMITIVE_COUNT && (X) < AST_OPERATION_COUNT ? prim_op_names[X] : nullptr)
#define STATEMENT_NAME(X) ((X) >= 0 && (X) < ASTStatement::STATEMENT_COUNT ? statement_names[X] : nullptr)
struct ASTNode {
    // TODO: Add a flag which you can check to know whether there are enums or not.
    //  If there aren't then you don't need to go through all the nodes.
    TokenRange tokenRange{};
    // TODO: Some of these do not need to exist in
    // every type of node
    enum Flags : u16 {
        HIDDEN = 0x1,
        REVERSE = 0x2, // for loop
        POINTER = 0x4, // for loop
        NULL_TERMINATED = 0x8, // AST_STRING
        DUMP_ASM = 0x10,
        DUMP_BC = 0x20,
        NO_CODE = 0x40,
    };
    int nodeId = 0;
    u16 flags = 0;
    LinkConventions linkConvention = LinkConventions::NONE;
    CallConventions callConvention = BETCALL;

    #define SET_FLAG(N,F) void set##N(bool on) { flags = on ? flags | F : flags & ~F; } bool is##N() { return flags&F; }
    SET_FLAG(Pointer,POINTER)
    SET_FLAG(Reverse,REVERSE)
    SET_FLAG(Hidden,HIDDEN)
    SET_FLAG(NoCode,NO_CODE)
    #undef SET_FLAG
    // A lot of places need to know whether a function has a body.
    // When function should have a body or not has changed a lot recently
    // and I have needed to rewrite a lot. Having the requirement abstracted in
    // a function will prevent some of the changes you would need to make.
    bool needsBody() { return linkConvention == LinkConventions::NONE;}
};
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
            bool yes = _array.resize(index + 1);
            Assert(yes);
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
    TypeId(PrimitiveType type) : _infoIndex0((u16)type), _infoIndex1(0), _flags(VALID_MASK) {}
    TypeId(OperationType type) : _infoIndex0((u16)type), _infoIndex1(0), _flags(VALID_MASK) {}
    static TypeId Create(u32 id) {
        TypeId out={}; 
        // TODO: ENUM or STRUCT?
        // out._flags = VALID_MASK;
        out.valid = true;
        out._infoIndex0 = id&0xFFFF;
        out._infoIndex1 = id>>16;
        return out; 
    }
    static TypeId CreateString(u32 index) {
        TypeId out = {};
        // out._flags = VALID_MASK | STRING;
        out.valid = true;
        out.string = true;
        out._infoIndex0 = index&0xFFFF;
        out._infoIndex1 = index>>16;
        return out;
    }
    enum TypeType : u32 {
        PRIMITIVE = 0x0,
        VALID_MASK = 0x1,
        STRING = 0x2,
        POISON = 0x4,
        TYPE_MASK = 0x2 | 0x4,
        POINTER_MASK = 0x8 | 0x10,
        POINTER_SHIFT = 3,
    };
    union {
        u16 _infoIndex0 = 0;
        PrimitiveType union_primtive;
        OperationType union_op;
    };
    u8 _infoIndex1 = 0;
    union {
        u8 _flags = 0;
        struct {
            bool valid : 1;
            bool string : 1;
            bool poison : 1;
            u8 pointer_level : 2;
    // u8 _bits_reserved : 3;
        };
    };

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
    bool isPoison() const { return (_flags & TYPE_MASK) == POISON; }
    // bool isVirtual() const { return (_flags & TYPE_MASK) == VIRTUAL; }
    TypeId baseType() const {
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
    // returns true if valid, no string, and no pointer
    bool isNormalType() const { return isValid() && !isString() && !isPointer(); }
    //  && !isVirtual(); }
};
engone::Logger& operator <<(engone::Logger&, TypeId typeId);
struct StructImpl;
struct FnOverloads {
    struct Overload {
        ASTFunction* astFunc=0;
        FuncImpl* funcImpl = 0;
    };
    struct PolyOverload {
        ASTFunction* astFunc=0;
        // DynamicArray<TypeId> argTypes{};
    };
    QuickArray<Overload> overloads{};
    QuickArray<Overload> polyImplOverloads{};
    QuickArray<PolyOverload> polyOverloads{};
    // Do not modify overloads while using the returned pointer
    // TODO: Use BucketArray to allow modifications
    Overload* getOverload(AST* ast, QuickArray<TypeId>& argTypes, ASTExpression* fncall, bool canCast = false);
    // Note that this function becomes complex if parentStruct is polymorphic.
    Overload* getOverload(AST* ast, QuickArray<TypeId>& argTypes, QuickArray<TypeId>& polyArgs, StructImpl* parentStruct, ASTExpression* fncall, bool implicitPoly = false, bool canCast = false);
    // Overload* getOverload(AST* ast, DynamicArray<TypeId>& argTypes, ASTExpression* fncall, bool canCast = false);
    // Overload* getOverload(AST* ast, DynamicArray<TypeId>& argTypes, DynamicArray<TypeId>& polyArgs, ASTExpression* fncall, bool implicitPoly = false, bool canCast = false);
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
    QuickArray<Member> members{};
    
    QuickArray<TypeId> polyArgs;
};
struct TypeInfo {
    TypeInfo(const std::string& name, TypeId id, u32 size=0) :  name(name), id(id), originalId(id), _size(size) {}
    TypeInfo(TypeId id, u32 size=0) : id(id), originalId(id), _size(size) {}
    std::string name;
    TypeId id={}; // can be virtual and point to a different type
    
    TypeId originalId={};
    u32 _size=0;
    // u32 _alignedSize=0;
    u32 arrlen=0;
    ASTStruct* astStruct=0;
    StructImpl* structImpl=0; // nullptr means pure/base poly type 
    ASTEnum* astEnum=0;

    ScopeId scopeId = 0;
    // bool isVirtualType = false;
    // bool isVirtual() { return isVirtualType; }
    
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
struct FuncImpl {
    std::string name;
    struct Spot {
        TypeId typeId{};
        int offset=0;
    };
    u32 usages = 0;
    bool isUsed() { return usages!=0; }
    QuickArray<Spot> argumentTypes;
    QuickArray<Spot> returnTypes;
    int argSize=0;
    int returnSize=0;
    i64 address = ADDRESS_INVALID; // Set by generator
    u32 polyVersion=-1;
    QuickArray<TypeId> polyArgs;
    StructImpl* structImpl = nullptr;
    void print(AST* ast, ASTFunction* astFunc);
    static const i64 ADDRESS_INVALID = -1; // undefined or not address that hasn't been set
    // static const u64 ADDRESS_EXTERNAL = -21;
};
struct Identifier {
    Identifier() {}
    enum Type {
        VARIABLE, FUNCTION
    };
    Type type=VARIABLE;
    std::string name{};
    ScopeId scopeId=0;
    // struct Pair {
    //     u32 polyVersion;
    //     u32 varIndex;
    // };
    // PolyVersions<u32> versions_varIndex{};
    u32 varIndex=0;
    FnOverloads funcOverloads{};
    ContentOrder order = 0;

    // How to support multiple variables in multiple poly versions
    // multiple poly versions because of methods inherting members of 'this' argument
    // multiple variables because of shadowing
    // Global variables cannot be shadowed? 
    // 

};
struct VariableInfo {
    enum Type : u8 {
        LOCAL, GLOBAL, MEMBER
    };
    // you could do a union on memberIndex and dataOffset if you want to save space
    i32 memberIndex = -1; // only used with MEMBER type

    // i32 dataOffset = 0;
    // TypeId typeId{};
    PolyVersions<i32> versions_dataOffset{};
    PolyVersions<TypeId> versions_typeId{};
    Type type = LOCAL;
    // bool globalData = false; // false
    // ScopeId scopeId = 0; // do you really need this variable? can you get the identifier instead?

    // We use getters here because the implementation changes a lot
    // i32 getDataOffset() { return dataOffset; }
    // TypeId getTypeId() { return typeId; }
    bool isMember() { return type == MEMBER; }
    bool isLocal() { return type == LOCAL; }
    bool isGlobal() { return type == GLOBAL; }
};
struct ScopeInfo {
    ScopeInfo(ScopeId id) : id(id) {}
    std::string name; // namespace
    ScopeId id = 0;
    ScopeId parent = 0;
    ContentOrder contentOrder = CONTENT_ORDER_ZERO; // the index of this scope in the parent content list

    std::unordered_map<std::string, ScopeId> nameScopeMap; // namespace map?
    std::unordered_map<std::string, TypeInfo*> nameTypeMap;

    std::unordered_map<std::string, Identifier> identifierMap;

    QuickArray<ScopeInfo*> usingScopes;

    ASTScope* astScope = nullptr; // some scopes don't belong to ASTScope

    // TODO: Move these elsewhere
    u32 bc_start;
    u32 bc_end;
    u32 asm_start;
    u32 asm_end;

    void print(AST* ast);

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
struct ASTExpression : ASTNode {
    ASTExpression() {
        // printf("hum\n");
        // engone::log::out << "default init\n";
        // *(char*)0 = 9;
    }
    ~ASTExpression() {
        versions_outTypeSizeof.~PolyVersions();
        // versions_outTypeTypeid.~PolyVersions(); // union with ...Sizeof
    }
    // enum Type: u8 {
    //     NORMAL = 0,
    //     INLINE_ASSEMBLY = 1,

    // };
    // ASTExpression() { memset(this,0,sizeof(*this)); }
    // ASTExpression() : left(0), right(0), castType(0) { }
    // Token token{};
    bool isValue = false;
    TypeId typeId = {}; // not polymorphic, primitive or operation
    // Type expressionType = NORMAL;
    // enum ConstantType {
    //     NONE,
    //     CONSTANT
    // };
    bool isConstant() { return constantValue; }
    // TODO: Bit field for all the bools
    bool constantValue = false;
    bool computeWhenPossible = false;

    bool implicitThisArg = false;
    bool memberCall = false;
    bool postAction = false;
    bool unsafeCast = false;
    bool hasImplicitThis() const { return implicitThisArg; }
    void setImplicitThis(bool yes) { implicitThisArg = yes; }

    bool isMemberCall() const { return memberCall; }
    void setMemberCall(bool yes) { memberCall = yes; }

    bool isPostAction() const { return postAction; }
    void setPostAction(bool yes) { postAction = yes; }
    
    bool isUnsafeCast() { return unsafeCast; }
    void setUnsafeCast(bool yes) { unsafeCast = yes; }

    // bool isInlineAssembly() const { return expressionType & INLINE_ASSEMBLY; }
    // void setInlineAssembly(bool yes) { expressionType = (Type)((expressionType&~INLINE_ASSEMBLY)); if (yes) expressionType = (Type)(expressionType|INLINE_ASSEMBLY); }

    union {
        i64 i64Value=0;
        float f32Value;
        double f64Value;
        bool boolValue;
        char charValue;
    };

    Token name{};
    Token namedValue={}; // Used for named arguments (fncall or initializer). Empty means never specified or used.
    ASTExpression* left=0; // FNCALL has arguments in order left to right
    ASTExpression* right=0;
    TypeId castType={};
    OperationType assignOpType = (OperationType)0;

    // Used if expression type is AST_ID
    Identifier* identifier = nullptr;

    // TODO: Is okay to fill ASTExpression with more data? It's quite large already.
    //  How else would we store this information because we don't want to recalculate it
    //  in Generator. I guess some unions?
    ASTEnum* enum_ast = nullptr;
    int enum_member = -1;


    QuickArray<ASTExpression*> args; // fncall or initialiser
    u32 nonNamedArgs = 0;

    // PolyVersions<DynamicArray<TypeId>> versions_argTypes{};

    // The contents of overload is stored here. This is not a pointer since
    // the array containing the overload may be resized.
    PolyVersions<FnOverloads::Overload> versions_overload{};

    // you could use a union with some of these to save memory
    // outTypeSizeof and castType could perhaps be combined?
    PolyVersions<int> versions_constStrIndex{};
    union {
        PolyVersions<TypeId> versions_outTypeSizeof{};
        PolyVersions<TypeId> versions_outTypeTypeid;
    };
    PolyVersions<TypeId> versions_castType{};

    void printArgTypes(AST* ast, QuickArray<TypeId>& argTypes);

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
        SWITCH,
        USING,
        BODY,
        DEFER,
        TEST,

        STATEMENT_COUNT,
    };
    ~ASTStatement(){
        // returnValues.~QuickArray<ASTExpression*>();
    }
    Type type = EXPRESSION;
    // int opType = 0;
    struct VarName {
        Token name{}; // TODO: Does not store info about multiple tokens, error message won't display full string
        TypeId assignString{};
        int arrayLength=-1;
        bool declaration = false;
        PolyVersions<TypeId> versions_assignType{}; // is inferred from expression in type checker
        // true if variable declares a new variable (it does if it has a type)
        // false if variable implicitly declares a new type OR assigns to an existing variable
        // bool declaration(){
        //     return assignString.isValid();
        // }
        
        // The corresponding identifier. Set in type checker.
        // Will be null in first polymorphic check, but will be set for later polymorphic checks
        Identifier* identifier = nullptr;
    };
    DynamicArray<VarName> varnames;
    std::string* alias = nullptr;

    ASTExpression* testValue = nullptr;
    // Token testValueToken{};
    // int bytesToTest = 0;
    // TypeId typeId={};

    // true if bodies and expressions can be used.
    // false if returnValues should be used.
    // use this to determine which part of a union to
    // initialize or cleanup
    // Not used!
    // bool hasNodes(){
    //     // Return is the only one that uses return values
    //     return type != ASTStatement::RETURN;
    // }
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
    struct SwitchCase {
        ASTExpression* caseExpr = nullptr;
        ASTScope* caseBody = nullptr;
        bool fall = false;
    };
    QuickArray<SwitchCase> switchCases; // used with switch statement
    
    // IMPORTANT: If you put an array in a union then you must explicitly
    // do cleanup in the destructor. Destructor in unions isn't called since
    // C++ doesn't know which variable to destruct.
    // union {
        // QuickArray<ASTExpression*> returnValues{};
    QuickArray<ASTExpression*> arrayValues; // for array initialized
    // };
    PolyVersions<DynamicArray<TypeId>> versions_expressionTypes; // types from firstExpression


    bool rangedForLoop=false; // otherwise sliced for loop
    bool globalAssignment=false; // for variables, indicates whether variable refers to global data in data segment
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
    // std::string name{};
    Token name{};
    std::string polyName="";
    struct Member {
        Token name;
        ASTExpression* defaultValue=0;
        TypeId stringType{};
    };
    QuickArray<Member> members{};
    struct PolyArg {
        Token name{};
        TypeInfo* virtualType = nullptr;
    };
    QuickArray<PolyArg> polyArgs;

    // StructImpl* createImpl();
    
    State state=TYPE_EMPTY;

    StructImpl* nonPolyStruct = nullptr;

    ScopeId scopeId=0;

    std::unordered_map<std::string, FnOverloads> _methods;
    
    // FnOverloads* addMethod(const std::string& name);
    FnOverloads* getMethod(const std::string& name, bool create = false);
    // void addPolyMethod(const std::string& name, ASTFunction* func, FuncImpl* funcImpl);

    // TODO: Be more efficient with allocations. You don't have to pop the poly states once allocated. Use an integer to know which depth you are at. Then once the compiler is done it can clean up the arrays.
    struct PolyState {
        DynamicArray<TypeId> structPolyArgs;
    };
    DynamicArray<PolyState> polyStates;
    // void pushPolyState(QuickArray<TypeId>* funcPolyArgs, QuickArray<TypeId>* structPolyArgs);
    // void pushPolyState(QuickArray<TypeId>* funcPolyArgs, StructImpl* structParent);
    void pushPolyState(StructImpl* structImpl);
    void popPolyState();

    QuickArray<ASTFunction*> functions{};
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
        bool ignoreRules = false;
    };
    QuickArray<Member> members{};

    enum Rules : u32 {
        NONE = 0,
        UNIQUE            = 0x1,
        SPECIFIED         = 0x2,
        BITFIELD          = 0x4,
        ENCLOSED          = 0x8,
        OPERATION_LESS    = 0x10,
    };
    Rules rules = NONE;

    TypeId colonType = AST_UINT32; // may be a type string before the type checker, may be a type string after if checker failed.
    TypeId actualType = {};
    
    bool getMember(const Token& name, int* out);

    void print(AST* ast, int depth);  
};
struct ASTFunction : ASTNode {
    Token name{};
    Identifier* identifier = nullptr;
    
    struct PolyArg {
        Token name{};
        TypeInfo* virtualType = nullptr;
    };
    struct Arg {
        Token name{};
        ASTExpression* defaultValue=0;
        TypeId stringType={};
        Identifier* identifier = nullptr;
    };
    struct Ret {
        TokenRange valueToken{}; // for error messages
        TypeId stringType;
    };

    QuickArray<Identifier*> memberIdentifiers; // only relevant with parent structs

    QuickArray<PolyArg> polyArgs;
    QuickArray<Arg> arguments; // you could rename to parameters
    QuickArray<Ret> returnValues; // string type
    u32 nonDefaults=0; // used when matching overload, having it here avoids recalculations of it

    QuickArray<FuncImpl*> _impls{};
    // FuncImpl* createImpl();
    const QuickArray<FuncImpl*>& getImpls(){
        return _impls;
    }

    struct PolyState {
        DynamicArray<TypeId> argTypes;
        DynamicArray<TypeId> structTypes;
    };
    DynamicArray<PolyState> polyStates;
    void pushPolyState(FuncImpl* funcImpl);
    void pushPolyState(QuickArray<TypeId>* funcPolyArgs, StructImpl* structParent);
    void popPolyState();

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
    bool isNamespace = false;

    QuickArray<ASTStruct*> structs{};
    void add(AST* ast, ASTStruct* astStruct);
    
    QuickArray<ASTEnum*> enums{};
    void add(AST* ast, ASTEnum* astEnum);

    QuickArray<ASTFunction*> functions{};
    void add(AST* ast, ASTFunction* astFunction);
    
    QuickArray<ASTScope*> namespaces{};
    void add(AST* ast, ASTScope* astNamespaces);
    
    QuickArray<ASTStatement*> statements{};
    void add(AST* ast, ASTStatement* astStatement);
    // void add_at(AST* ast, ASTStatement* astStatement, ContentOrder ContentOrder);

    // Using doesn't affect functions or structs because
    // functions and structs doesn't have an order while
    // using statements do. The code below should
    // maintain the order necessary to make using
    // work with functions and structs.
    enum SpoType : u8 {
        STRUCT,
        ENUM,
        FUNCTION,
        NAMESPACE,
        STATEMENT,
    };
    struct Spot {
        SpoType spotType;
        u32 index;
    };
    QuickArray<Spot> content{};

    void print(AST* ast, int depth);
};
struct AST {
    //-- Creation and destruction
    static AST* Create();
    static void Destroy(AST* ast);
    void cleanup();
    
    //-- Scope stuff
    ScopeId globalScopeId=0;
    QuickArray<ScopeInfo*> _scopeInfos; // TODO: Use a bucket array
    ScopeInfo* createScope(ScopeId parentScope, ContentOrder contentOrder, ASTScope* astScope);
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
    QuickArray<Token> _typeTokens;
    TypeId getTypeString(Token name);
    Token getTokenFromTypeString(TypeId typeString);
    // typeString must be a string type id.
    TypeId convertToTypeId(TypeId typeString, ScopeId scopeId, bool transformVirtual);

    QuickArray<TypeInfo*> _typeInfos; // TODO: Use a bucket array, might not make a difference since typeInfos are allocated in a linear allocator
    // DynamicArray<TypeInfo*> _typeInfos; // TODO: Use a bucket array
    TypeInfo* createType(Token name, ScopeId scopeId);
    TypeInfo* createPredefinedType(Token name, ScopeId scopeId, TypeId id, u32 size=0);
    // isValid() of the returned TypeId will be false if
    // type couldn't be converted.
    TypeId convertToTypeId(Token typeString, ScopeId scopeId, bool transformVirtual);
    // pointers NOT allowed
    TypeInfo* convertToTypeInfo(Token typeString, ScopeId scopeId, bool transformVirtual) {
        return getTypeInfo(convertToTypeId(typeString, scopeId, transformVirtual).baseType());
    }
    // pointers are allowed
    TypeInfo* getBaseTypeInfo(TypeId id);
    // pointers are NOT allowed
    TypeInfo* getTypeInfo(TypeId id);
    std::string typeToString(TypeId typeId);

    TypeId ensureNonVirtualId(TypeId id);

    //-- OTHER
    ASTScope* mainBody=0;

    QuickArray<std::string*> tempStrings;
    std::string* createString();

    // static const u32 NEXT_ID = 0x100;
    u32 nextTypeId=AST_OPERATION_COUNT;
    
    QuickArray<VariableInfo*> variables;

    //-- Identifiers and variables
    // Searches for identifier with some name. It does so recursively
    // Identifier* findIdentifier(ScopeId startScopeId, const Token& name, bool searchParentScopes = true);
    Identifier* findIdentifier(ScopeId startScopeId, ContentOrder, const Token& name, bool searchParentScopes = true);
    // VariableInfo* identifierToVariable(Identifier* identifier);

    // Returns nullptr if variable already exists or if scopeId is invalid
    // If out_reused_identical isn't null then an identical variable can be returned it if exists. This can happen in polymorphic scopes.
    VariableInfo* addVariable(ScopeId scopeId, const Token& name, ContentOrder contentOrder, Identifier** identifier, bool* out_reused_identical);
    // We don't overload addVariable because we may want to change the name or 
    // behaviour of this function and it's very useful to have descriptive name
    // when we do.
    VariableInfo* getVariableByIdentifier(Identifier* identifier);
    // VariableInfo* addVariable(ScopeId scopeId, const Token& name, bool shadowPreviousVariables=false, Identifier** identifier = nullptr);
    // Returns nullptr if variable already exists or if scopeId is invalid
    // If out_reused_identical isn't null then an identical identifier can be returned it if exists. This can happen in polymorphic scopes.
    Identifier* addIdentifier(ScopeId scopeId, const Token& name, ContentOrder contentOrder, bool* out_reused_identical);
    // Identifier* addIdentifier(ScopeId scopeId, const Token& name, bool shadowPreviousIdentifier=false);

    // returns true if successful, out_enum/member contains what was found.
    // returns false if not found. This could also mean that the enum was enclosed, if so the out parameters may tell you the enum that would have matched if it wasn't enclosed (it allows you to be more specific with error messages).
    bool findEnumMember(ScopeId scopeId, const Token& name, ContentOrder contentOrder, ASTEnum** out_enum, int* out_member);

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
    QuickArray<ConstString> _constStrings;

    u32 globalDataOffset = 0;
    u32 aquireGlobalSpace(int size) {
        u32 offset = globalDataOffset;
        globalDataOffset += size;
        return offset;
    }
    // from type checker
    u32 preallocatedGlobalSpace() {
        return globalDataOffset;
    }

    char* linearAllocation = nullptr;
    u32 linearAllocationMax = 0;
    u32 linearAllocationUsed = 0;
    void initLinear(){
        Assert(!linearAllocation);
        linearAllocationMax = 0x10000000;
        linearAllocationUsed = 0;
        linearAllocation = TRACK_ARRAY_ALLOC(char, linearAllocationMax);
         // (char*)engone::Allocate(linearAllocationMax);
        Assert(linearAllocation);
    }
    void* allocate(u32 size) {
        // const int align = 8;
        // int diff = (align - (linearAllocationUsed % align)) % align;
        // linearAllocationUsed += diff;
        // if(diff) {
        //     engone::log::out << "dif "<<diff<<"\n";
        // }

        // TODO: If we don't have enough space then create a new linear allocator
        //  and allocate stuff there. Buckets of linear allocators basically.
        Assert(linearAllocationUsed + size < linearAllocationMax);
        void* ptr = linearAllocation + linearAllocationUsed;
        linearAllocationUsed += size;
        return ptr;
    }

    // outIndex is used with getConstString(u32)
    ConstString& getConstString(const std::string& str, u32* outIndex);
    ConstString& getConstString(u32 index);

    // Must be used after TrimPointer
    static Token TrimPolyTypes(Token token, QuickArray<Token>* outPolyTypes = nullptr);
    static Token TrimNamespace(Token token, Token* outNamespace = nullptr);
    static Token TrimPointer(Token& token, u32* level = nullptr);
    static Token TrimBaseType(Token token, Token* outNamespace, u32* level, QuickArray<Token>* outPolyTypes, Token* typeName);
    // true if id is one of u8-64, i8-64
    static bool IsInteger(TypeId id);
    // will return false for non number types
    static bool IsSigned(TypeId id);
    static bool IsDecimal(TypeId id);

    // content in body is moved and the body is destroyed. DO NOT USE IT AFTERWARDS.
    void appendToMainBody(ASTScope* body);

    int nextNodeId = 1; // start at 1, 0 indicates a non-set id for ASTNode, probably a bug if so
    int getNextNodeId() { return nextNodeId++; }

    StructImpl* createStructImpl();
    FuncImpl* createFuncImpl(ASTFunction* astFunc);

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