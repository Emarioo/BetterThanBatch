#pragma once

#include "BetBat/Lexer.h"
#include "BetBat/MessageTool.h"
#include "BetBat/Signals.h"

typedef u32 ScopeId;
typedef u32 ContentOrder;
#define CONTENT_ORDER_ZERO 0
#define CONTENT_ORDER_MAX (u32)-1
struct ASTStruct;
struct ASTStatement;
struct ASTEnum;
struct ASTFunction;
struct AST;
struct FuncImpl;
struct ASTExpression;
struct ASTScope;

enum CallConvention : u8 {
    BETCALL, // The default. Native functions use this.
    STDCALL, // Currently default x64 calling convention.
    INTRINSIC, // Special implementation. Usually a function directly coupled to an instruction.
    // CDECL_CONVENTION, // Not implemented yet. CDECL is a macro and unavailable so _CONVENTION was added to the name.
    // https://www.ired.team/miscellaneous-reversing-forensics/windows-kernel-internals/linux-x64-calling-convention-stack-frame
    UNIXCALL, // System V AMD64 ABI calling convention.
    // ARMCALL,
};
#define STR_STATIC_IMPORT "importlib"
#define STR_DYNAMIC_IMPORT "importdll"
enum LinkConvention : u8 {
    NONE=0x00, // no linkConvention export/import
    // DLLEXPORT, // .drectve section is needed to pass /EXPORT: to the linker. The exported function must use stdcall convention
    IMPORT=0x80, // external from the source code, linkConvention with static library or object files
    STATIC_IMPORT=0x81,  // linkConvention with lib or relative code
    DYNAMIC_IMPORT=0x82, // linkConvention with dll, function are renamed to __impl_
};

#define IS_IMPORT(X) (X&(u8)0x80)
const char* ToString(CallConvention stuff);
const char* ToString(LinkConvention stuff);

engone::Logger& operator<<(engone::Logger& logger, CallConvention convention);
engone::Logger& operator<<(engone::Logger& logger, LinkConvention convention);

StringBuilder& operator<<(StringBuilder& builder, CallConvention convention);
StringBuilder& operator<<(StringBuilder& builder, LinkConvention convention);


// IMPORTANT: YOU MUST MODIFY primitive_names in AST.cpp IF YOU MAKE CHANGES HERE
enum PrimitiveType : u16 {
    TYPE_VOID=0,
    TYPE_UINT8,
    TYPE_UINT16,
    TYPE_UINT32,
    TYPE_UINT64,
    TYPE_INT8,
    TYPE_INT16,
    TYPE_INT32,
    TYPE_INT64,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_FLOAT32,
    TYPE_FLOAT64,
    
    // AST_TRUE_PRIMITIVES,

    // AST_STRING, // converted to char[]
    // AST_NULL, // converted to void*

    // // TODO: should these be moved somewhere else?
    // AST_ID, // variable, enum, type
    // AST_FNCALL,
    // AST_ASM, // inline assembly
    // AST_SIZEOF,
    // AST_NAMEOF,
    // AST_TYPEID,
    
    TYPE_PRIMITIVE_COUNT, // above types are true with isValue
};
enum OperationType : u16 {
    // TODO: Add BINOP (binary) and UNOP (unary) in the enum names.
    //  If you could arrange binary and unary ops so that you can determine the difference
    //  using a bitmask (op & BINARY_UNARY_MASK), that would signifcantly speed up code which currently has
    //  to check if an op is equal to any of the operations (op == AST_NOT || op == AST_INCREMENT || ...)
    //  
    AST_OP_NONE = 0,
    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV,
    AST_MODULO,
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
    AST_PRE_INCREMENT,
    AST_PRE_DECREMENT,
    AST_POST_INCREMENT,
    AST_POST_DECREMENT,

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
extern const char* primitive_type_names[];
extern const char* operation_names[];
// Preferably use these macros since the way we access
// the names may change. Perhaps prim and op will be split into
// their own tables.
#define PRIM_NAME(X) (primitive_type_names[X])
#define OP_NAME(X) (operation_names[X])
#define STATEMENT_NAME(X) ((X) >= 0 && (X) < ASTStatement::STATEMENT_COUNT ? statement_names[X] : nullptr)

struct ASTNode {
    // TODO: Add a flag which you can check to know whether there are enums or not.
    //  If there aren't then you don't need to go through all the nodes.
    // TokenRange tokenRange{};
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

    #define SET_FLAG(N,F) void set##N(bool on) { flags = on ? flags | F : flags & ~F; } bool is##N() { return flags&F; }
    SET_FLAG(Pointer,POINTER)
    SET_FLAG(Reverse,REVERSE)
    SET_FLAG(Hidden,HIDDEN)
    SET_FLAG(NoCode,NO_CODE)
    #undef SET_FLAG
};

extern engone::Mutex g_polyVersions_mutex; // TODO: FIX THIS! I added it because two threads may check the same polymorphic scope with different poly versions. When they resize the array for polyversion types we crash.

template<class T>
struct PolyVersions {
    PolyVersions() = default;
    ~PolyVersions(){ _array.cleanup(); }
    // TODO: Allocations are should be controlled by AST.
    // Using heap for now to make things simple.


    int size() const {
        // return _array.size();
        return _array.size() + (has_first ? 1 : 0);
    }
    // automatically allocates
    T& get(u32 index, bool skip_mutex = false) {
        if(!skip_mutex)
            g_polyVersions_mutex.lock();
        if(index == 0) {
            has_first = true;
            if(!skip_mutex)
                g_polyVersions_mutex.unlock();
            return first_elem;
        }
        if(index >= _array.size()) {
            Assert(("Probably a bug",index < 1000));
            bool yes = _array.resize(index + 1);
            Assert(yes);
        }
        auto elem = &_array.get(index);
        if(!skip_mutex)
            g_polyVersions_mutex.unlock();
        return *elem;
    }
    void set(u32 index, const T& elem) {
        g_polyVersions_mutex.lock();
        auto& thing = get(index, true);
        thing = elem;
        g_polyVersions_mutex.unlock();
    }
    // automatically allocates
    T& operator[](u32 index) {
    // const T& operator[](u32 index) {
        auto ptr = &get(index);
        return *ptr;
    }
    // only works on types that have method 'steal_from'
    void steal_element_into(u32 index, T& stealer) {
        g_polyVersions_mutex.lock();
        auto& elem = get(index, true);
        stealer.steal_from(elem);
        g_polyVersions_mutex.unlock();
    }
    void steal_element_from(u32 index, T& from) {
        g_polyVersions_mutex.lock();
        auto& elem = get(index, true);
        elem.steal_from(from);
        g_polyVersions_mutex.unlock();
    }
    // T steal_element(u32 index) {
    //     g_polyVersions_mutex.lock();
    //     auto elem = get(index, true);
    //     get(index, true)
    //     g_polyVersions_mutex.unlock();
    //     return elem;
    // }

private:
    // An optimization where first element is stored in struct. This means that node AST's that use PolyVersions won't allocate memory as long as less than one element is needed. Since most code isn't polymorphic and doesn't need more than one element, this is perfect.
    bool has_first = false;
    T first_elem{};
    DynamicArray<T> _array{};
};
struct TypeId {
    TypeId() = default;
    TypeId(PrimitiveType type) : _infoIndex0((u16)type), _infoIndex1(0), _flags(VALID_MASK) {}
    // TypeId(OperationType type) : _infoIndex0((u16)type), _infoIndex1(0), _flags(VALID_MASK) {}
    static TypeId Create(u32 id, int level = 0) {
        TypeId out={}; 
        // TODO: ENUM or STRUCT?
        // out._flags = VALID_MASK;
        out.valid = true;
        out._infoIndex0 = id&0xFFFF;
        out._infoIndex1 = id>>16;
        out.pointer_level = level;
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
    // bool operator==(OperationType t) const {
    //     return *this == TypeId(t);
    // }
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
    TypeId withoutPointer() const {
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
struct OverloadGroup {
    struct Overload {
        ASTFunction* astFunc=nullptr;
        FuncImpl* funcImpl = nullptr;
    };
    struct PolyOverload {
        ASTFunction* astFunc=nullptr;
        // DynamicArray<TypeId> argTypes{};
    };
    QuickArray<Overload> overloads{};
    QuickArray<Overload> polyImplOverloads{};
    QuickArray<PolyOverload> polyOverloads{};
    // Do not modify overloads while using the returned pointer
    // TODO: Use BucketArray to allow modifications
    // Overload* getOverload(AST* ast, ScopeId scopeOfFncall, QuickArray<TypeId>& argTypes, bool implicit_this, ASTExpression* fncall, bool canCast = false, const BaseArray<bool>* inferred_args = nullptr);
    // // Note that this function becomes complex if parentStruct is polymorphic.
    // Overload* getOverload(AST* ast, QuickArray<TypeId>& argTypes, QuickArray<TypeId>& polyArgs, StructImpl* parentStruct, bool implicit_this,ASTExpression* fncall, bool implicitPoly = false, bool canCast = false, const BaseArray<bool>* inferred_args = nullptr);
    
    // // FuncImpl can be null and probably will be most of the time
    // // when you call this.
    // void addOverload(ASTFunction* astFunc, FuncImpl* funcImpl);
    // Overload* addPolyImplOverload(ASTFunction* astFunc, FuncImpl* funcImpl);
    // void addPolyOverload(ASTFunction* astFunc);

    // FuncImpl* createImpl();
};
// ASTStruct can have multiple of these per
// polymorphic instantiation.
struct StructImpl {
    TypeId typeId;
    int size=0;
    int alignedSize=0;
    struct Member {
        TypeId typeId={};
        int offset=0;
    };
    QuickArray<Member> members{};
    
    QuickArray<TypeId> polyArgs;

    bool is_evaluating = false;
};
struct FunctionSignature {
    struct Spot {
        TypeId typeId{};
        int offset=0;
    };
    QuickArray<Spot> argumentTypes;
    QuickArray<Spot> returnTypes;
    int argSize=0; // always divisible by 8
    int returnSize=0; // always divisible by 8
    QuickArray<TypeId> polyArgs;
    CallConvention convention=BETCALL;
};
struct TypeInfo {
    TypeInfo(const StringView& name, TypeId id, int size=0) :  name(name), id(id), originalId(id), _size(size) {}
    TypeInfo(TypeId id, int size=0) : id(id), originalId(id), _size(size) {}
    // StringView name;
    std::string name;
    TypeId id={}; // can be virtual and point to a different type
    
    TypeId originalId={};
    int _size=0;
    // u32 _alignedSize=0;
    int arrlen=0;
    ASTStruct* astStruct=nullptr;
    StructImpl* structImpl=nullptr; // nullptr means pure/base poly type 
    ASTEnum* astEnum=nullptr;
    FunctionSignature* funcType=nullptr;

    ScopeId scopeId = 0;
    // bool isVirtualType = false;
    // bool isVirtual() { return isVirtualType; }
    
    struct MemberData {
        TypeId typeId{};
        int index = 0;
        int offset = 0;
    };

    int getSize();
    // 1,2,4,8
    MemberData getMember(const std::string& name);
    MemberData getMember(int index);
    StructImpl* getImpl();
};
struct FuncImpl {
    // std::string name;
    ASTFunction* astFunction = nullptr;
    int usages = 0;
    bool isUsed() { return usages!=0; }
    
    FunctionSignature signature;

    int _size_of_locals = 0; // These two fields are mostly private, don't access them directly
    int _max_size_of_locals = 0;
    int _max_size_of_arguments = 0; // size of the arguments of the function call with the biggest size for function arguments
    void update_max_arguments(int size) {
        if(_max_size_of_arguments < size)
            _max_size_of_arguments = size;
    }
    void alloc_frame_space(int size) {
        _size_of_locals += size;
        if(_max_size_of_locals < _size_of_locals)
            _max_size_of_locals = _size_of_locals;
    }
    void free_frame_space(int size) {
        _size_of_locals -= size;
    }
    int get_frame_size() const {
        // TODO: 8-byte alignment
        // NOTE: I disabled arguments for now, they are not included in stack frame for now.
        // return signature.returnSize + _max_size_of_locals + _max_size_of_arguments;
        return signature.returnSize + _max_size_of_locals;
    }
   
    int tinycode_id = 0; // 0 is invalid, set by generator
    int polyVersion = -1; // We can catch mistakes if we use -1 as default value
    StructImpl* structImpl = nullptr;
    void print(AST* ast, ASTFunction* astFunc);
};
struct IdentifierVariable;
struct IdentifierFunction;
struct Identifier {
    Identifier() {}
    enum Type {
        LOCAL_VARIABLE,
        ARGUMENT_VARIABLE,
        GLOBAL_VARIABLE,
        MEMBER_VARIABLE,
        FUNCTION,
    };
    Type type=LOCAL_VARIABLE;
    std::string name;
    ScopeId scopeId=0;
    ContentOrder order = 0;

    bool is_fn() { return type == FUNCTION; }
    bool is_var() { return type != FUNCTION; }

    IdentifierVariable* cast_var() { Assert(is_var()); return (IdentifierVariable*)this; }
    IdentifierFunction* cast_fn() { Assert(is_fn()); return (IdentifierFunction*)this; }
};

struct IdentifierVariable : public Identifier {
    int memberIndex = -1; // only used with MEMBER type
    int argument_index = 0;

    PolyVersions<int> versions_dataOffset{};
    PolyVersions<TypeId> versions_typeId{};

    bool is_import_global = false;

    ASTStatement* declaration = nullptr; // needed for linked_library for imported variables

    bool isLocal() { return type == LOCAL_VARIABLE; }
    bool isArgument() { return type == ARGUMENT_VARIABLE; }
    bool isGlobal() { return type == GLOBAL_VARIABLE; }
    bool isMember() { return type == MEMBER_VARIABLE; }
};
struct IdentifierFunction : public Identifier {
    OverloadGroup funcOverloads{};
};
struct ScopeInfo {
    ScopeInfo(ScopeId id) : id(id) {}
    // StringView name; // namespace
    std::string name;
    ScopeId id = 0;
    ScopeId parent = 0;
    ContentOrder contentOrder = CONTENT_ORDER_ZERO; // the index of this scope in the parent content list

    std::unordered_map<std::string, ScopeId> nameScopeMap; // namespace map?
    std::unordered_map<std::string, TypeInfo*> nameTypeMap;

    // We store pointers to identifiers because it may be a derivative class (variable or function). We refer to identifiers in ASTExpression by pointer and don't want unordered_map to invalidate the pointer by reallocating the memory.
    std::unordered_map<std::string, Identifier*> identifierMap;

    QuickArray<ScopeInfo*> sharedScopes;

    ASTScope* astScope = nullptr; // may be null, some scopes don't belong to ASTScope

    bool is_function_scope = false;

    // TODO: Move these elsewhere?
    u32 bc_start = 0; // we need tinycode id too
    u32 bc_end = 0;
    u32 asm_start = 0; // relative to function start
    u32 asm_end = 0;

    void print(AST* ast);

    // Returns the full namespace.
    // Name of parent scopes are concatenated.
    std::string getFullNamespace(AST* ast);
};

enum ASTExpressionType {
    EXPR_NONE,
    EXPR_VALUE,
    EXPR_IDENTIFIER,
    EXPR_STRING,
    EXPR_MEMBER,
    EXPR_ASSEMBLY,
    EXPR_OPERATION,
    EXPR_ASSIGN,
    EXPR_CALL,
    EXPR_INITIALIZER,
    EXPR_CAST,
    EXPR_BUILTIN, // sizeof, cast

    EXPR_NULL,
};
bool operator==(ASTExpressionType,OperationType) = delete;
bool operator==(OperationType,ASTExpressionType) = delete;

struct AST;

struct ASTExpression;
struct ASTExpressionValue;
struct ASTExpressionString;
struct ASTExpressionIdentifier;
struct ASTExpressionMember;
struct ASTExpressionAssembly;
struct ASTExpressionOperation;
struct ASTExpressionAssign;
struct ASTExpressionCall;
struct ASTExpressionInitializer;
struct ASTExpressionCast;
struct ASTExpressionBuiltin;
struct ASTExpression : ASTNode {
    static const ASTExpressionType TYPE = EXPR_NONE;
    ASTExpression() { }
    ASTExpressionType type;
    lexer::SourceLocation location;
    bool uses_cast_operator = false; // needed for all exprs
    StringView namedValue{}; // needed for all exprs
    
    bool computeWhenPossible = false;
    #ifdef gone
    bool isConstant() { return constantValue; }
    // TODO: Bit field for all the bools
    bool constantValue = false;
    #endif

    template<typename T>
    T* as() const {
        Assert(type == T::TYPE);
        return (T*)this;
    }

    void printArgTypes(AST* ast, QuickArray<TypeId>& argTypes);

    void print(AST* ast, int depth);
};

struct ASTExpressionValue : public ASTExpression {
    static const ASTExpressionType TYPE = EXPR_VALUE;
    TypeId typeId = {};
    union {
        i64 i64Value=0;
        float f32Value;
        double f64Value;
        bool boolValue;
        char charValue;
    };
};
struct ASTExpressionString : public ASTExpression {
    static const ASTExpressionType TYPE = EXPR_STRING;
    PolyVersions<int> versions_constStrIndex{};
    std::string name;
};

struct ASTExpressionIdentifier : public ASTExpression {
    static const ASTExpressionType TYPE = EXPR_IDENTIFIER;
    Identifier* identifier;
    std::string name;
    ASTEnum* enum_ast = nullptr;
    int enum_member = -1;
};
struct ASTExpressionMember : public ASTExpression {
    static const ASTExpressionType TYPE = EXPR_MEMBER;
    std::string name;
    ASTExpression* left;
};
struct ASTExpressionAssembly : public ASTExpression {
    static const ASTExpressionType TYPE = EXPR_ASSEMBLY;
    TypeId castType;
    TypeId asmTypeString;
    lexer::TokenRange asm_range{};
    QuickArray<ASTExpression*> args;
    PolyVersions<TypeId> versions_asmType{};
};
struct ASTExpressionOperation : public ASTExpression {
    static const ASTExpressionType TYPE = EXPR_OPERATION;
    OperationType op_type;
    ASTExpression* left;
    ASTExpression* right;
    // operator overloading
    Identifier* identifier;
    int nonNamedArgs = 0;
    PolyVersions<OverloadGroup::Overload> versions_overload{};
    PolyVersions<FunctionSignature*> versions_func_type{};
};
struct ASTExpressionAssign : public ASTExpression {
    static const ASTExpressionType TYPE = EXPR_ASSIGN;
    OperationType assign_op_type; // can be none
    ASTExpression* left;
    ASTExpression* right;
};
struct ASTExpressionCall : public ASTExpression {
    static const ASTExpressionType TYPE = EXPR_CALL;
    std::string name;
    Identifier* identifier;
    QuickArray<ASTExpression*> args;
    int nonNamedArgs;

    bool implicitThisArg = false;
    // true if an implicit this argument should be passed.
    // this happens when a method calls another method in the same struct.
    bool hasImplicitThis() const { return implicitThisArg; }
    void setImplicitThis(bool yes) { implicitThisArg = yes; }

    bool memberCall = false; // rename to method call?
    // true if a call has the shape 'obj.func()'
    // it is different from implicit this in that the parent struct doesn't determine
    // the function we call, the 'obj' does.
    bool isMemberCall() const { return memberCall; }
    void setMemberCall(bool yes) { memberCall = yes; }

    PolyVersions<OverloadGroup::Overload> versions_overload{};
    PolyVersions<FunctionSignature*> versions_func_type{};
};
struct ASTExpressionInitializer : public ASTExpression {
    static const ASTExpressionType TYPE = EXPR_INITIALIZER;
    TypeId castType;
    PolyVersions<TypeId> versions_castType{};
    QuickArray<ASTExpression*> args;
    bool is_inferred_initializer() {
        return !castType.isValid();
    }
};
struct ASTExpressionCast : public ASTExpression {
    static const ASTExpressionType TYPE = EXPR_CAST;
    ASTExpression* left;
    TypeId castType;
    PolyVersions<TypeId> versions_castType{};
    bool unsafeCast = false;
    bool isUnsafeCast() { return unsafeCast; }
    void setUnsafeCast(bool yes) { unsafeCast = yes; }
};
enum BuiltinType {
    AST_SIZEOF,
    AST_TYPEOF,
    AST_NAMEOF,
};  
struct ASTExpressionBuiltin : public ASTExpression {
    static const ASTExpressionType TYPE = EXPR_BUILTIN;
    // QuickArray<ASTExpression*> args;
    BuiltinType builtin_type;
    ASTExpression* left;
    std::string name;
    PolyVersions<TypeId> versions_outTypeSizeof;
    PolyVersions<TypeId> versions_outTypeTypeid; // typeid/sizeof
    PolyVersions<int> versions_constStrIndex{}; // nameof
};
#define CAST_EXPR(V,T) (V->as<ASTExpression##T>())
enum ForLoopType : u8 {
    SLICED_FOR_LOOP,
    RANGED_FOR_LOOP,
    CUSTOM_FOR_LOOP, // user defined with create_iterator, iterate
};
struct ASTStatement : ASTNode {
    // ASTStatement() { memset(this,0,sizeof(*this)); }
    enum Type {
        EXPRESSION=0,
        DECLARATION, // const, global, variable
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
        TRY,

        STATEMENT_COUNT,
    };
    ~ASTStatement(){
        // returnValues.~QuickArray<ASTExpression*>();
    }
    Type type = EXPRESSION;
    // int opType = 0;
    struct VarName {
        std::string name{}; // TODO: Does not store info about multiple tokens, error message won't display full string
        TypeId assignString{};
        int arrayLength=-1;
        bool declaration = false; // multi var. assignment may not declare variables
        PolyVersions<TypeId> versions_assignType{}; // is inferred from expression in type checker
        // true if variable declares a new variable (it does if it has a type)
        // false if variable implicitly declares a new type OR assigns to an existing variable
        // bool declaration(){
        //     return assignString.isValid();
        // }
        
        // The corresponding identifier. Set in type checker.
        // Will be null in first polymorphic check, but will be set for later polymorphic checks
        IdentifierVariable* identifier = nullptr;
        lexer::SourceLocation location;
    };
    lexer::SourceLocation location{};
    DynamicArray<VarName> varnames;
    std::string alias;

    LinkConvention linkConvention; // used with imported global variables

    // bool uses_cast_operator = false;

    ASTExpression* testValue = nullptr;
    bool computeWhenPossible = false;

    std::string linked_alias;
    std::string linked_library;
    bool isImported() { return linked_library.size() != 0; }
   
    // with a union you have to use hasNodes before using the expressions.
    // this is annoying so I am not using a union but you might want to
    // to save 24 bytes.
    // union {
    //     struct {
            ASTExpression* firstExpression = nullptr;
            ASTScope* firstBody  = nullptr;
            ASTScope* secondBody = nullptr;
        // };
        // DynamicArray<ASTExpression*> returnValues{};
    // };
    struct SwitchCase {
        ASTExpression* caseExpr = nullptr;
        ASTScope* caseBody = nullptr;
        bool fall = false;
        // TODO: Optimize by not having these fields for switch case, they are only used for catch cases.
        IdentifierVariable* variable = nullptr;
        lexer::SourceLocation location{};
        std::string catch_exception_name{}; // only for exceptions
    };
    DynamicArray<SwitchCase> switchCases; // used with switch statement and try-catch
    
    // IMPORTANT: If you put an array in a union then you must explicitly
    // do cleanup in the destructor. Destructor in unions isn't called since
    // C++ doesn't know which variable to destruct.
    // union {
        // QuickArray<ASTExpression*> returnValues{};
    QuickArray<ASTExpression*> arrayValues; // for array initialized
    // };
    PolyVersions<QuickArray<TypeId>> versions_expressionTypes; // types from firstExpression

    // used with for loops
    PolyVersions<OverloadGroup::Overload> versions_create_overload{};
    PolyVersions<OverloadGroup::Overload> versions_iterate_overload{};

    bool is_notstable = false;
    ForLoopType forLoopType=SLICED_FOR_LOOP;
    bool globalDeclaration=false; // for variables, indicates whether variable refers to global data in data segment
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
    lexer::SourceLocation location{};
    std::string name;
    struct Member {
        std::string name;
        lexer::SourceLocation location{};
        ASTExpression* defaultValue = nullptr;
        TypeId stringType{};
        int array_length = 0;
    };
    DynamicArray<Member> members{};
    struct PolyArg {
        lexer::SourceLocation location{};
        std::string name;
        TypeInfo* virtualType = nullptr;
    };
    DynamicArray<PolyArg> polyArgs;
    TypeId base_typeId; // base type as in, type without any specified polymorphic arguments

    State state=TYPE_EMPTY;

    bool no_padding = false;

    StructImpl* nonPolyStruct = nullptr;

    ScopeId scopeId=0;

    std::unordered_map<std::string, OverloadGroup> _methods;
    
    // OverloadGroup* addMethod(const std::string& name);
    OverloadGroup* getMethod(const std::string& name, bool create = false);
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

    volatile bool is_being_checked = false; // only one thread can check function at any one time.

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
    // TokenRange tokenRange{};
    lexer::SourceLocation location{}; // name token
    std::string name{};
    struct Member {
        // Token name{};
        std::string name;
        // StringView name;
        i64 enumValue=0;
        bool ignoreRules = false;
        lexer::SourceLocation location;
    };
    DynamicArray<Member> members{};

    enum Rules : u32 {
        NONE = 0,
        UNIQUE         = 0x1,
        SPECIFIED      = 0x2,
        BITFIELD       = 0x4,
        ENCLOSED       = 0x8,
        OPERATION_LESS = 0x10,
        LENIENT_SWITCH = 0x20,
    };
    Rules rules = NONE;

    // abstraction over rules & rule in case something changes in the future
    bool hasRule(Rules rule) {
        return (rules & rule);
    }

    TypeId colonType = TYPE_UINT32; // may be a type string before the type checker, may be a type string after if checker failed.
    TypeId typeId = {};
    
    bool getMember(const StringView& name, int* out);

    void print(AST* ast, int depth);  
};
struct ASTFunction : ASTNode {
    lexer::SourceLocation location;
    std::string name{};
    IdentifierFunction* identifier = nullptr;
    
    struct PolyArg {
        std::string name{};
        TypeInfo* virtualType = nullptr;
        lexer::SourceLocation location;
    };
    struct Arg {
        lexer::SourceLocation location;
        std::string name{};
        ASTExpression* defaultValue=0;
        TypeId stringType={};
        IdentifierVariable* identifier = nullptr;
    };
    struct Ret {
        lexer::SourceLocation location{};
        TypeId stringType{};
    };

    // returns preprocessed id, do not use it for tasks without convertint
    // to original import id.
    u32 getImportId(lexer::Lexer* lexer) const {
        return lexer->getImport_unsafe(location)->file_id;
    }

    QuickArray<IdentifierVariable*> memberIdentifiers; // only relevant with parent structs

    DynamicArray<PolyArg> polyArgs;
    DynamicArray<Arg> arguments; // you could rename to parameters
    DynamicArray<Ret> returnValues; // string type
    u32 nonDefaults=0; // used when matching overload, having it here avoids recalculations of it

    QuickArray<FuncImpl*> _impls{};
    // FuncImpl* createImpl();
    const QuickArray<FuncImpl*>& getImpls(){
        return _impls;
    }

    volatile bool is_being_checked = false; // only one thread can check function at any one time.

    // one function name can link to multiple functions from different libraries
    // if they use different aliases (or real names)
    // linked_alias and -library should be PER FuncImpl instead of PER ASTFunction
    std::string linked_alias;
    std::string linked_library;

    std::string export_alias; // empty means no export, same name as function means no alias, different means alias

    bool is_compiler_func = false;
    bool contains_run_directive = false;

    struct PolyState {
        DynamicArray<TypeId> argTypes;
        DynamicArray<TypeId> structTypes;
    };
    DynamicArray<PolyState> polyStates;
    void pushPolyState(FuncImpl* funcImpl);
    void pushPolyState(const BaseArray<TypeId>* funcPolyArgs, StructImpl* structParent);
    void popPolyState();

    u32 polyVersionCount=0;
    ASTStruct* parentStruct = nullptr;
    bool is_operator = false;
    bool isOperator() const { return is_operator; }

    ScopeId scopeId=0;
    ASTScope* body=0;

    // does not consider the method's struct
    bool isPolymorphic(){
        return polyArgs.size()!=0;
    }
    
    bool blank_body = false; // tells the generator to create no instructions except a return.
    LinkConvention linkConvention = LinkConvention::NONE;
    CallConvention callConvention = BETCALL;
    // A lot of places need to know whether a function has a body.
    // When function should have a body or not has changed a lot recently
    // and I have needed to rewrite a lot. Having the requirement abstracted in
    // a function will prevent some of the changes you would need to make.
    bool needsBody() { return linkConvention == LinkConvention::NONE && callConvention != INTRINSIC && !is_compiler_func; }

    void print(AST* ast, int depth);
};
struct ASTScope : ASTNode {
    std::string name = ""; // namespace
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
    enum SpotType : u8 {
        STRUCT,
        ENUM,
        FUNCTION,
        NAMESPACE,
        STATEMENT,
    };
    struct Spot {
        SpotType spotType = STRUCT;
        u32 index = 0;
    };
    QuickArray<Spot> content{};

    void print(AST* ast, int depth);
};
struct Compiler;
struct AST {
    AST(Compiler* compiler) : compiler(compiler) {}

    Compiler* compiler = nullptr;
    //-- Creation and destruction
    static AST* Create(Compiler* compiler);
    static void Destroy(AST* ast);
    void cleanup();
    
    //-- Scope stuff
    ScopeInfo* createScope(ScopeId parentScope, ContentOrder contentOrder, ASTScope* astScope);
    ScopeInfo* getScope(ScopeId id);
    // Searches scopes for a namespace. First the current scope (scopeId), then inherited/using scopes, and optionally parent scopes of the current scope. Example of name "GameLib::Math". search_parent_scopes SHOULD NOT default to false. You will use findScope thinking it searches parents and whoops, you got a bug.
    // @return Nullptr when scope not found.
    ScopeInfo* findScope(StringView name, ScopeId scopeId, bool search_parent_scopes);
    // Same as findScope but always searches parents. This function has become redundant so maybe remove it?
    // ScopeInfo* findScopeFromParents(StringView name, ScopeId scopeId);

    //-- Types
    
    TypeId getTypeString(const std::string& name);
    StringView getStringFromTypeString(TypeId typeString);
    // Token getTokenFromTypeString(TypeId typeString);
    // typeString must be a string type id.
    TypeId convertToTypeId(TypeId typeString, ScopeId scopeId, bool transformVirtual);

    // DynamicArray<TypeInfo*> _typeInfos; // TODO: Use a bucket array
    TypeInfo* createType(StringView name, ScopeId scopeId);
    TypeInfo* createPredefinedType(StringView name, ScopeId scopeId, TypeId id, int size=0);
    // isValid() of the returned TypeId will be false if
    // type couldn't be converted.
    TypeId convertToTypeId(StringView typeString, ScopeId scopeId, bool transformVirtual);
    // pointers NOT allowed
    TypeInfo* convertToTypeInfo(StringView typeString, ScopeId scopeId, bool transformVirtual) {
        return getTypeInfo(convertToTypeId(typeString, scopeId, transformVirtual).baseType());
    }
    // pointers are allowed
    TypeInfo* getBaseTypeInfo(TypeId id);
    // pointers are NOT allowed
    TypeInfo* getTypeInfo(TypeId id);
    std::string typeToString(TypeId typeId);

    TypeId ensureNonVirtualId(TypeId id);

    //-- OTHER
    int REGISTER_SIZE = -1;
    int FRAME_SIZE = -1;
    ASTScope* globalScope=0;
    MUTEX(lock_globalScope);
    // QuickArray<ASTScope*> importScopes{};

    // QuickArray<std::string*> tempStrings;
    // std::string* createString();

    // static const u32 NEXT_ID = 0x100;
    
    // void find_or_compute_search_scope(ScopeId start_scopeId, ContentOrder contentOrder);
    struct SearchScope {
        struct Entry {
            ScopeInfo* scope;
            ContentOrder order;
            bool crossed_function_boundary;
        };
        DynamicArray<Entry> scopes;
    };
    // search scope for shared scopes
    SearchScope* find_or_compute_search_scope(ScopeId start_scopeId);
    std::unordered_map<ScopeId, SearchScope*> search_scope_map;

    //-- Identifiers and variables
    // Searches for identifier with some name. It does so recursively
    Identifier* findIdentifier(ScopeId startScopeId, ContentOrder contentOrder, const StringView& name, bool* crossed_function_boundary, bool searchParentScopes = true, bool searchSharedScopes = true);
    void findIdentifiers(ScopeId startScopeId, ContentOrder contentOrder, const StringView& name, QuickArray<Identifier*>& out_identifiers, bool* crossed_function_boundary, bool searchParentScopes = true);
    // Identifier* findIdentifier(ScopeId startScopeId, ContentOrder, const StringView& name, bool searchParentScopes = true);
    // VariableInfo* identifierToVariable(Identifier* identifier);

    // Returns nullptr if variable already exists or if scopeId is invalid
    // If out_reused_identical isn't null then an identical variable can be returned it if exists. This can happen in polymorphic scopes.
    IdentifierVariable* addVariable(Identifier::Type type, ScopeId scopeId, const StringView& name, ContentOrder contentOrder, bool* out_reused_identical);
    IdentifierFunction* addFunction(ScopeId scopeId, const StringView& name, ContentOrder contentOrder);
    
    // returns true if successful, out_enum/member contains what was found.
    // returns false if not found. This could also mean that the enum was enclosed, if so the out parameters may tell you the enum that would have matched if it wasn't enclosed (it allows you to be more specific with error messages).
    bool findEnumMember(ScopeId scopeId, const StringView& name, ASTEnum** out_enum, int* out_member);

    void removeIdentifier(ScopeId scopeId, const StringView& name);
    void removeIdentifier(Identifier* identifier);

    struct ScopeIterator {
        ContentOrder next_order = 0;
        ScopeInfo* next_scope = 0;
        bool next_crossed_function_boundary = 0;
    private:
        struct Item {
            ScopeInfo* scope;
            ContentOrder order;
            bool crossed_function_boundary;
        };
        QuickArray<Item> search_items;
        int search_index = 0;
        friend struct AST;
    };
    ScopeIterator createScopeIterator(ScopeId scopeId, ContentOrder order);
    ScopeInfo* iterate(ScopeIterator& iterator, bool searchSharedScopes = true);
        
    int getTypeSize(TypeId typeId);
    int getTypeAlignedSize(TypeId typeId);

    // less_strict will allow u64 -> i32* conversions, integer to pointer is otherwise only allowed with void pointers.
    bool castable(TypeId from, TypeId to, bool less_strict = false);

    bool findCastOperator(ScopeId scopeId, TypeId from_type, TypeId to_type, OverloadGroup::Overload* overload = nullptr);

    void declareUsageOfOverload(OverloadGroup::Overload* overload);

    int aquireGlobalSpace(int size) {
        int offset = globalDataOffset;
        globalDataOffset += size;
        return offset;
    }
    // from type checker
    int preallocatedGlobalSpace() {
        return globalDataOffset;
    }
    
    std::string nameOfFuncImpl(FuncImpl* impl);

    void initLinear(){
        Assert(!linearAllocation);
        linearAllocationMax = 0x1000'000; // tweak this
        linearAllocationUsed = 0;
        linearAllocation = TRACK_ARRAY_ALLOC(char, linearAllocationMax);
         // (char*)engone::Allocate(linearAllocationMax);
        Assert(linearAllocation);
    }
    void* allocate(int size) {
        // const int align = 8;
        // int diff = (align - (linearAllocationUsed % align)) % align;
        // linearAllocationUsed += diff;
        // if(diff) {
        //     engone::log::out << "dif "<<diff<<"\n";
        // }
        lock_linearAllocation.lock();
        // TODO: If we don't have enough space then create a new linear allocator
        //  and allocate stuff there. Buckets of linear allocators basically.
        Assert(linearAllocationUsed + size < linearAllocationMax);
        // lock_linearAllocation.lock();
        // u32 new_index = linearAllocationUsed;
        // linearAllocationUsed+=size;
        // lock_linearAllocation.unlock();
        
        // u32 new_index = engone::atomic_add((volatile i32*)&linearAllocationUsed, size) - size;

        int new_index = linearAllocationUsed; // TODO: Bad
        linearAllocationUsed += size;
        lock_linearAllocation.unlock();
        
        void* ptr = linearAllocation + new_index;
        return ptr;
    }

    struct ConstString {
        u32 length = 0;
        u32 address = 0;
    };
    // outIndex is used with getConstString(u32)
    ConstString& getConstString(const std::string& str, u32* outIndex);
    ConstString& getConstString(u32 index);

    // Must be used after TrimPointer
    static void DecomposePolyTypes(StringView view, StringView* out_base, QuickArray<StringView>* outPolyTypes);
    static void DecomposeNamespace(StringView view, StringView* out_namespace, StringView* out_name);
    static void DecomposePointer(StringView view, StringView* out_name, u32* level);
    // static StringView TrimPointer(StringView& view, u32* level = nullptr);
    static StringView TrimBaseType(StringView view, StringView* outNamespace, u32* level, QuickArray<StringView>* outPolyTypes, StringView* typeName);
    // true if id is one of u8-64, i8-64
    static bool IsInteger(TypeId id);
    // will return false for non number types
    static bool IsSigned(TypeId id);
    static bool IsDecimal(TypeId id);

    int nextNodeId = 1; // start at 1, 0 indicates a non-set id for ASTNode, probably a bug if so
    int getNextNodeId() { return nextNodeId++; }

    StructImpl* createStructImpl(TypeId typeId);
    FuncImpl* createFuncImpl(ASTFunction* astFunc);

    ASTScope* createBody();
    ASTFunction* createFunction();
    ASTStruct* createStruct(const StringView& name);
    ASTEnum* createEnum(const StringView& name);
    ASTStatement* createStatement(ASTStatement::Type type);

    ASTExpression* createExpression(ASTExpressionType type);
    ASTExpressionOperation* createExpressionOperation(OperationType type) {
        auto ptr = (ASTExpressionOperation*)createExpression(EXPR_OPERATION);
        ptr->op_type = type;
        return ptr;
    }
    ASTExpressionValue* createExpressionValue(TypeId type) {
        auto ptr = (ASTExpressionValue*)createExpression(EXPR_VALUE);
        ptr->typeId = type;
        return ptr;
    }

    // You may also want to call some additional functions to properly deal with
    // named scopes so types are properly evaluated. See ParseNamespace for an example.
    ASTScope* createNamespace(const StringView& name);
    
    void destroy(ASTScope* astScope);
    void destroy(ASTFunction* function);
    void destroy(ASTStruct* astStruct);
    void destroy(ASTEnum* astEnum);
    void destroy(ASTStatement* statement);
    void destroy(ASTExpression* expression);

    void print(int depth = 0);
    void printTypesFromScope(ScopeId scopeId, int scopeLimit=-1);

    ScopeId globalScopeId=0; // needs to be public, Parser needs it

    u32 nextTypeId=AST_OPERATION_COUNT;
    // TODO: Use a bucket array, might not make a difference since typeInfos are allocated in a linear allocator. 
    QuickArray<TypeInfo*> _typeInfos; // not private because DWARF uses it and I don't want to use friend

    QuickArray<TypeInfo*> function_types;
    
    // TODO: annotations
    TypeInfo* findOrAddFunctionSignature(const BaseArray<TypeId>& args, const BaseArray<TypeId>& rets, CallConvention conv);
    TypeInfo* findOrAddFunctionSignature(FunctionSignature* signature);

    MUTEX(lock_scopes);
    volatile u32 nextScopeInfoIndex = 0;
    // TODO: Use a bucket array
    QuickArray<ScopeInfo*> _scopeInfos; // not private because x64_Converted needs it (once again, I don't want to use friend)
    
    MUTEX(lock_strings);
    std::unordered_map<std::string, u32> _constStringMap; // used in generator
    QuickArray<ConstString> _constStrings;

    struct GlobalItem {
        ASTStatement* stmt;
        ScopeId scope;
    };
    QuickArray<GlobalItem> globals_to_evaluate;

    
    // NOTE: These functions are methods of the AST instead of OverloadGroup because it's easier to synchronize with multi-threading. (we would need individual mutex for each group or a global variable, it's better to have mutex in the AST)
    OverloadGroup::Overload* getOverload(OverloadGroup* group, ScopeId scopeOfFncall, const BaseArray<TypeId>& argTypes, bool implicit_this, ASTExpression* fncall, bool canCast = false, const BaseArray<bool>* inferred_args = nullptr);
    // Note that this function becomes complex if parentStruct is polymorphic. It only checks computed polymorphic functions
    OverloadGroup::Overload* getPolyOverload(OverloadGroup* group, const BaseArray<TypeId>& argTypes, const BaseArray<TypeId>& polyArgs, StructImpl* parentStruct, bool implicit_this, ASTExpression* fncall, bool implicitPoly = false, bool canCast = false, const BaseArray<bool>* inferred_args = nullptr);
    
    // FuncImpl can be null and probably will be most of the time
    // when you call this.
    void addOverload(OverloadGroup* group, ASTFunction* astFunc, FuncImpl* funcImpl);
    OverloadGroup::Overload* addPolyImplOverload(OverloadGroup* group, ASTFunction* astFunc, FuncImpl* funcImpl);
    void addPolyOverload(OverloadGroup* group, ASTFunction* astFunc);


private:
    MUTEX(lock_overloads);

    MUTEX(lock_linearAllocation);
    char* linearAllocation = nullptr;
    u32 linearAllocationMax = 0;
    volatile u32 linearAllocationUsed = 0;

    QuickArray<ASTExpression*> expressions;
    QuickArray<ASTStruct*> structures;
    QuickArray<ASTEnum*> enums;
    QuickArray<ASTStatement*> statements;
    QuickArray<ASTFunction*> functions;
    QuickArray<ASTScope*> bodies;
    QuickArray<Identifier*> identifiers;

    MUTEX(lock_astnodes);

    int globalDataOffset = 0;

    MUTEX(lock_variables);

    MUTEX(lock_typeTokens);
    DynamicArray<std::string> _typeTokens;

};
const char* TypeIdToStr(int type);