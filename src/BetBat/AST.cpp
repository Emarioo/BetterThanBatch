#include "BetBat/AST.h"

const char* ToString(CallConventions stuff){
    #define CASE(X,N) case X: return N;
    switch(stuff){
        CASE(BETCALL,"betcall")
        CASE(STDCALL,"stdcall")
        CASE(INTRINSIC,"intrinsic")
        CASE(CDECL_CONVENTION,"cdecl")
        CASE(UNIXCALL,"unixcall")
    }
    return "<unknown-call>";
    #undef CASE
}
const char* ToString(LinkConventions stuff){
    #define CASE(X,N) case X: return N;
    switch(stuff){
        CASE(NONE,"none")
        CASE(NATIVE,"native")
        CASE(DLLIMPORT,"dllimport")
        CASE(VARIMPORT,"varimport")
        CASE(IMPORT,"import")
    }
    return "<unknown-link>";
    #undef CASE
}
engone::Logger& operator<<(engone::Logger& logger, CallConventions convention){
    return logger << ToString(convention);
}
engone::Logger& operator<<(engone::Logger& logger, LinkConventions convention){
    return logger << ToString(convention);
}

StringBuilder& operator<<(StringBuilder& builder, CallConventions convention){
    return builder << ToString(convention);
}
StringBuilder& operator<<(StringBuilder& builder, LinkConventions convention){
    return builder << ToString(convention);
}

const char* prim_op_names[]{
// PRIMITIVES
    "void",         // AST_VOID
    "u8",           // AST_UINT8
    "u16",          // AST_UINT16
    "u32",          // AST_UINT32
    "u64",          // AST_UINT64
    "i8",           // AST_INT8
    "i16",          // AST_INT16
    "i32",          // AST_INT32
    "i64",          // AST_INT64

    "bool",         // AST_BOOL
    "char",         // AST_CHAR

    "f32",          // AST_FLOAT32
    "f64",          // AST_FLOAT64
    
    nullptr,        // AST_TRUE_PRIMITIVES
    
    "ast_string",    // AST_STRING
    "null",         // AST_NULL

    "fn_ref",        // AST_FUNC_REFERENCE

    "ast_id",        // AST_ID
    "ast_call",     // AST_FNCALL
    "ast_asm",     // AST_ASM
    "ast_sizeof",   // AST_SIZEOF
    "ast_nameof",   // AST_NAMEOF
    "ast_typeid",   // AST_NAMEOF
    
// OPERATIONS
     "+",                   // AST_ADD, AST_PRIMITIVE_COUNT
     "-",                   // AST_SUB
     "*",                   // AST_MUL
     "/",                   // AST_DIV
     "%",                   // AST_MODULUS
     "-",                   // AST_UNARY_SUB

     "==",                  // AST_EQUAL
     "!=",                  // AST_NOT_EQUAL
     "<",                   // AST_LESS
     "<=",                  // AST_LESS_EQUAL
     ">",                   // AST_GREATER
     ">=",                  // AST_GREATER_EQUAL
     "&&",                  // AST_AND
     "||",                  // AST_OR
     "!",                   // AST_NOT

     "&",                   // AST_BAND
     "|",                   // AST_BOR
     "^",                   // AST_BXOR
     "~",                   // AST_BNOT
     "<<",                  // AST_BLSHIFT
     ">>",                  // AST_BRSHIFT

     "..",                  // AST_RANGE
     "[]",                  // AST_INDEX
     "++",                  // AST_INCREMENT
     "--",                  // AST_DECREMENT
     
     "cast",                // AST_CAST
     "member",              // AST_MEMBER
     "initializer",         // AST_INITIALIZER
     "slice_initializer",   // AST_SLICE_INITIALIZER
     "namespaced",          // AST_FROM_NAMESPACE
     "=",                   // AST_ASSIGN

     "& (reference)",       // AST_REFER
     "* (dereference)",     // AST_DEREF               
};
const char* statement_names[] {
    "expression",     // EXPRESSION
    "assign",         // ASSIGN  
    "if",             // IF

    "while",          // WHILE
    "for",            // FOR

    "return",         // RETURN
    "break",          // BREAK
    "continue",       // CONTINUE
    
    "switch",         // SWITCH
    
    "using",          // USING
    "body",           // BODY
    "defer",          // DEFER
    "test",           // TEST
};

AST *AST::Create() {
    using namespace engone;
// Useful when debugging memory leaks
#ifdef LOG_ALLOC
    static bool once = false;
    if (!once) {
        once = true;
#define ADD_TRACKING(X) TrackType(sizeof(X), #X);
        ADD_TRACKING(TypeInfo)
        ADD_TRACKING(ScopeInfo)
        ADD_TRACKING(ASTExpression)
        ADD_TRACKING(ASTEnum)
        ADD_TRACKING(ASTStruct)
        ADD_TRACKING(ASTFunction)
        ADD_TRACKING(ASTStatement)
        ADD_TRACKING(ASTScope)
        ADD_TRACKING(AST)
        ADD_TRACKING(FuncImpl)
        ADD_TRACKING(StructImpl)
        ADD_TRACKING(std::string)
        #define LOG_SIZE(X) << #X " "<<sizeof(X)<<"\n"
        // log::out
        // LOG_SIZE(TypeInfo)
        // LOG_SIZE(ASTExpression)
        // LOG_SIZE(ASTEnum)
        // LOG_SIZE(ASTStruct)
        // LOG_SIZE(ASTFunction)
        // LOG_SIZE(ASTStatement)
        // LOG_SIZE(ASTScope)
        // LOG_SIZE(AST)
        // ;
    }
#endif

    AST *ast = TRACK_ALLOC(AST);
    new(ast) AST();

    ast->initLinear();

    ast->globalScope = ast->createBody();

    ast->_scopeInfos.resize(0x10000); // nocheckin

    ScopeId scopeId = ast->createScope(0,CONTENT_ORDER_ZERO, ast->globalScope)->id;
    ast->globalScopeId = scopeId;
    // initialize default data types
    #define ADD(T, S) ast->createPredefinedType(PRIM_NAME(T), scopeId, T, S);
    ADD(AST_VOID,0);
    ADD(AST_UINT8, 1);
    ADD(AST_UINT16, 2);
    ADD(AST_UINT32, 4);
    ADD(AST_UINT64, 8);
    ADD(AST_INT8, 1);
    ADD(AST_INT16, 2);
    ADD(AST_INT32, 4);
    ADD(AST_INT64, 8);
    ADD(AST_FLOAT32, 4);
    ADD(AST_FLOAT64, 8);
    ADD(AST_BOOL, 1);
    ADD(AST_CHAR, 1);
    ADD(AST_NULL, 8);
    ADD(AST_STRING, 0);
    ADD(AST_FUNC_REFERENCE, 8);
    ADD(AST_ID,0);
    ADD(AST_SIZEOF,0);
    ADD(AST_NAMEOF,0);
    ADD(AST_TYPEID,0);
    ADD(AST_FNCALL,0);
    #undef ADD
    // {
    //     // TODO: set size and offset of language structs here instead of letting the compiler do it.
    //     ASTStruct* astStruct = ast->createStruct("Slice");
    //     astStruct->members.push_back({});
    //     auto& mem = astStruct->members.back();
    //     mem.name = "ptr";
    //     mem.typeId = ast->getTypeId(ast->globalScopeId, Token("void*"));
    //     mem.defaultValue = 0;
    //     mem.polyIndex = -1;
    //     astStruct->members.push_back({});
    //     auto& mem2 = astStruct->members.back();
    //     mem2.name = "len";
    //     mem2.typeId = ast->getTypeId(ast->globalScopeId, Token("u32"));
    //     mem2.defaultValue = 0;
    //     mem2.polyIndex = -1;
    //     ast->getTypeString("Slice");
    //     ast->globalScope->add(astStruct);
    // }
    return ast;
}

// VariableInfo *AST::addVariable(ScopeId scopeId, const Token &name, bool shadowPreviousVariables) {
VariableInfo *AST::addVariable(ScopeId scopeId, const StringView&name, ContentOrder contentOrder, Identifier** identifier, bool* out_reused_identical = nullptr) {
    using namespace engone;
    bool shadowPreviousVariables = false;
    auto id = addIdentifier(scopeId, name, contentOrder, out_reused_identical);
    if(!id) {
        return nullptr;
    }
    VariableInfo* varinfo = nullptr;
    if(identifier)
        *identifier = id;
    if(out_reused_identical && *out_reused_identical) {
        varinfo = getVariableByIdentifier(id);
        Assert(varinfo);
        // If we reused identifier then it must have been a variable
        // and it should have varinfo. Probably bug otherwise.
    } else {
        varinfo = (VariableInfo*)allocate(sizeof(VariableInfo));
        new(varinfo)VariableInfo();
        
        id->type = Identifier::VARIABLE;
        id->order = contentOrder;
        
        lock_variables.lock();
        id->varIndex = variables.size();
        variables.add(varinfo);
        lock_variables.unlock();
    }
    return varinfo;
}
VariableInfo* AST::getVariableByIdentifier(Identifier* identifier) {
    Assert(identifier && identifier->type == Identifier::VARIABLE);
    VariableInfo* ptr = nullptr;
    lock_variables.lock();
    if(identifier->varIndex < variables.size()) {
        ptr = variables[identifier->varIndex];
    }
    lock_variables.unlock();
    return ptr;
}
// VariableInfo* AST::identifierToVariable(Identifier* identifier){
//     Assert(identifier);
//     Assert(identifier->varIndex < variables.size());
//     return variables[identifier->varIndex];
// }
// Identifier *AST::addIdentifier(ScopeId scopeId, const Token &name, bool shadowPreviousIdentifiers) {
Identifier *AST::addIdentifier(ScopeId scopeId, const StringView &name, ContentOrder contentOrder, bool* out_reused_identical) {
    using namespace engone;
    bool shadowPreviousIdentifiers = false;
    ScopeInfo* si = getScope(scopeId);
    if(!si)
        return nullptr;
    std::string sName = std::string(name.ptr,name.len); // string view?
    lock_variables.lock();
    auto pair = si->identifierMap.find(sName);
    if (pair != si->identifierMap.end()){
        if(out_reused_identical) {
            if(scopeId == pair->second.scopeId
            && contentOrder == pair->second.order
            && name == pair->second.name
            && pair->second.type == Identifier::VARIABLE) { // we can only reuse variable identifiers, not function identifiers
                *out_reused_identical = true;
                return &pair->second;
            }
        }
        return nullptr;
    }

    // TODO: Delete variable info of previous identifier when shadowing
    si->identifierMap[sName] = {};
    auto id = &si->identifierMap[sName];
    id->name = name;
    id->scopeId = scopeId;
    id->order = contentOrder;
    lock_variables.unlock();
    return id;
}
Identifier* AST::findIdentifier(ScopeId startScopeId, ContentOrder contentOrder, const StringView& name, bool searchParentScopes){
    using namespace engone;
    if(searchParentScopes){
        StringView ns;
        StringView real_name;
        DecomposeNamespace(name, &ns, &real_name);
        ScopeId nextScopeId = startScopeId;
        ContentOrder nextOrder = contentOrder;
        lock_variables.lock();
        defer { lock_variables.unlock(); };
        WHILE_TRUE_N(1000) {
            if(ns.ptr) {
                Assert(false); // broken with content order
                ScopeInfo* nscope = findScope(ns, nextScopeId, false);
                Assert(nscope);
                auto pair = nscope->identifierMap.find(real_name);
                if(pair != nscope->identifierMap.end()){
                    return &pair->second;
                }
            }
            ScopeInfo* si = getScope(nextScopeId);
            Assert(si);
            if(!ns.ptr) {
                auto pair = si->identifierMap.find(real_name);
                if(pair != si->identifierMap.end() && pair->second.order <= nextOrder){
                // if(pair != si->identifierMap.end() && pair->second.type == Identifier::VARIABLE && pair->second.order < nextOrder){
                    return &pair->second;
                }
            }
            if(nextScopeId == 0 && si->parent == 0){
                // quit when we checked global
                break;
            }
            nextScopeId = si->parent;
            nextOrder = si->contentOrder;
        }
    } else {
        ScopeInfo* si = getScope(startScopeId);
        Assert(si);
        lock_variables.lock();
        defer { lock_variables.unlock(); };
        auto pair = si->identifierMap.find(name);
        if(pair == si->identifierMap.end()){
            return nullptr;
        }
        // if(pair->second.type == Identifier::VARIABLE && pair->second.order < contentOrder) {
        if(pair->second.order <= contentOrder) {
            return &pair->second;
        }
        return nullptr;
    }
    return nullptr;
}

void AST::removeIdentifier(ScopeId scopeId, const StringView &name) {
    auto si = getScope(scopeId);
    lock_variables.lock();
    
    auto pair = si->identifierMap.find(name);
    if (pair != si->identifierMap.end()) {
        // TODO: The variable the identifier points to cannot be erased since
        //   the variables array will invalidate other identifier's index.
        //   A slot approach with a stack of empty slots for the variables would work.
        si->identifierMap.erase(pair);
    } else {
        // TODO: This can happen if variable was shadowed. If it wasn't then it is a bug
        //   and it would be good to catch that but we would need information about shadowing
        //   which we don't have at the moment.
        // Assert(("cannot remove non-existent identifier, compiler bug?",false));
    }
    lock_variables.unlock();
}
bool AST::findEnumMember(ScopeId scopeId, const StringView& name, ContentOrder contentOrder, ASTEnum** out_enum, int* out_member) {
    StringView ns;
    StringView real_name;
    DecomposeNamespace(name, &ns, &real_name);
    Assert(!ns.ptr); // namespace stuff not implemented, see findIdentifier and convertTypeId for it's implementation

    Assert(out_enum && out_member);

    ScopeId nextScopeId = scopeId;
    ContentOrder nextOrder = contentOrder;
    WHILE_TRUE {
        ScopeInfo* si = getScope(nextScopeId);
        Assert(si);
        if(si->astScope) {
            for(auto& astEnum : si->astScope->enums) {
                if((astEnum->rules & ASTEnum::ENCLOSED) && *out_enum != nullptr) {
                    continue;
                }
                for(int i=0;i<astEnum->members.size();i++){
                    auto& mem = astEnum->members[i];
                    // NOTE: We still check enclosed enums because it is useful to know
                    //  which member/enum could have matched if it wasn't enclosed when no other enum/member matches.
                    if(mem.name == real_name) {
                        *out_enum = astEnum;
                        *out_member = i;
                        if(astEnum->rules & ASTEnum::ENCLOSED) {
                            break;
                        } else {
                            return true;
                        }
                    }
                }
            }
        }
        if(nextScopeId == 0 && si->parent == 0){
            // quit when we checked global
            break;
        }
        nextScopeId = si->parent;
        nextOrder = si->contentOrder;
    }
    return false;
}
bool AST::castable(TypeId from, TypeId to){
    if (from == to)
        return true;
    if(from.isPointer()) {
        if (to == AST_BOOL)
            return true;
        if ((to.baseType() == AST_UINT64 || to.baseType() == AST_INT64) && 
            from.getPointerLevel() - to.getPointerLevel() == 1)
            return true;
        // if(to.baseType() == AST_VOID && from.getPointerLevel() == to.getPointerLevel())
        //     return true;
        if(to.baseType() == AST_VOID && to.getPointerLevel() > 0)
            return true;
    }
    if(to.isPointer()) {
        if ((from.baseType() == AST_UINT64 || from.baseType() == AST_INT64) && 
            to.getPointerLevel() - from.getPointerLevel() == 1)
            return true;
        // if(from.baseType() == AST_VOID && from.getPointerLevel() == to.getPointerLevel())
        //     return true;
        if(from.baseType() == AST_VOID && from.getPointerLevel() > 0)
            return true;
    }
    if (AST::IsDecimal(from) && AST::IsInteger(to)) {
        return true;
    }
    if (AST::IsInteger(from) && AST::IsDecimal(to)) {
        return true;
    }
    if (AST::IsDecimal(from) && AST::IsDecimal(to)) {
        return true;
    }
    if ((AST::IsInteger(from) && to == AST_CHAR) ||
        (from == AST_CHAR && AST::IsInteger(to))) {
        return true;
    }
    if ((AST::IsInteger(from) && to == AST_BOOL) ||
        (from == AST_BOOL && AST::IsInteger(to))) {
        return true;
    }
    if (AST::IsInteger(from) && AST::IsInteger(to)) {
        return true;
    }

    TypeInfo* from_typeInfo = nullptr;
    if(from.isNormalType())
        from_typeInfo = getTypeInfo(from);
    int to_size = getTypeSize(to);
    if(from_typeInfo && from_typeInfo->astEnum && AST::IsInteger(to)) {
        // TODO: Print an error that says big enums can't be casted to small integers.
        if(to_size >= from_typeInfo->getSize())
            return true;
    }

    return false;

}
// FnOverloads::Overload* FnOverloads::getOverload(AST* ast, DynamicArray<TypeId>& argTypes, ASTExpression* fncall, bool canCast){
FnOverloads::Overload* FnOverloads::getOverload(AST* ast, QuickArray<TypeId>& argTypes, ASTExpression* fncall, bool canCast){
    using namespace engone;
    // Assert(!fncall->hasImplicitThis());
    // Assume the only overload. The generator may do implicit casting if needed.
    //   Or not because the generator pushes arguments to get types in order to
    //   get overload. It's first when we have the overload that the casting can happen.
    //   But at that point the arguments to cast have already been pushed. BC_MOV, BC_POP, BC_PUSH to
    //   deal with that is bad for runtime so you would need to use different code
    //   specifically for implicit casting. I don't want more code right now.
    // if(overloads.size()==1)
    //     return &overloads[0];

    FnOverloads::Overload* lastOverload = nullptr;
    FnOverloads::Overload* intOverload = nullptr;
    FnOverloads::Overload* uintOverload = nullptr;
    FnOverloads::Overload* sintOverload = nullptr;
    int validOverloads = 0;
    int intOverloads = 0;
    int uintOverloads = 0;
    int sintOverloads = 0;
    for(int i=0;i<(int)overloads.size();i++){
        auto& overload = overloads[i];
        bool found = true;
        bool found_int = true; // any signedness
        bool found_sint = true; // signed
        bool found_uint = true; // unsigned
        // TODO: Store non defaults in Identifier or ASTStruct to save time.
        //   Recalculating non default arguments here every time you get a function is
        //   unnecessary.
        
        int startOfRealArguments = 0;
        if (fncall->hasImplicitThis()) {
            startOfRealArguments = 1;   
        }
        // NOTE: I refactored here to have less duplicated code. 'this' require special behaviour which is no done cleanly.
        //   BUT, did I break something? - Emarioo, 2023-12-19
            
        if(fncall->nonNamedArgs > overload.astFunc->arguments.size() - startOfRealArguments // can't match if the call has more essential args than the total args the overload has
        || fncall->nonNamedArgs < overload.astFunc->nonDefaults - startOfRealArguments // can't match if the call has less essential args than the overload (excluding defaults)
        || argTypes.size() > overload.astFunc->arguments.size() - startOfRealArguments)
            continue;
            
        if(canCast){
            found_int = false;
            found_sint = false; // we don't use int
            found_uint = false;
            for(int j=0;j<(int)fncall->nonNamedArgs;j++){
                TypeId implArgType = overload.funcImpl->argumentTypes[j+startOfRealArguments].typeId;
                if(!ast->castable(argTypes[j], implArgType)) {
                    found = false;
                    break;
                }
                // log::out << ast->typeToString(overload.funcImpl->argumentTypes[j].typeId) << " = "<<ast->typeToString(argTypes[j])<<"\n";
            }
        } else {
            for(int j=0;j<(int)fncall->nonNamedArgs;j++){
                TypeId implArgType = overload.funcImpl->argumentTypes[j+startOfRealArguments].typeId;
                if(argTypes[j] != implArgType) {
                    found = false;
                    // NOTE: What about implicitly casting float?
                    //  Casting floats and doubles has more overhead than integers.
                    //  It may therefore be a good idea to force the user to explicitly cast.
                    //  Or create two functions for f32 and f64.
                    if(!(AST::IsInteger(implArgType) && AST::IsInteger(argTypes[j]))) {
                        found_sint = false;
                        found_uint = false;
                        found_int = false;
                    } else {
                        if(!(AST::IsSigned(implArgType) && AST::IsSigned(argTypes[j]))) {
                            found_sint = false;
                        }
                        if(!(!AST::IsSigned(implArgType) && !AST::IsSigned(argTypes[j]))) {
                            found_uint = false;
                        }
                    }
                    // break; // We can't break when using foundInt
                }
                // log::out << ast->typeToString(overload.funcImpl->argumentTypes[j].typeId) << " = "<<ast->typeToString(argTypes[j])<<"\n";
            }
        }
        if(found_int) {
            intOverload = &overload;
            intOverloads++;
        }
        if(found_sint) {
            sintOverload = &overload;
            sintOverloads++;
        } 
        if(found_uint) {
            uintOverload = &overload;
            uintOverloads++;
        } 
        if(!found)
            continue;

        // NOTE: @Optimize You can return here because there should only be one matching overload.
        // But we keep going in case we find more matches which would indicate
        // a bug in the compiler. An optimised build would not do this.
        if(validOverloads > 0) {
            if(canCast) {
                return nullptr;
            }
            // log::out << log::RED << __func__ <<" (COMPILER BUG): More than once match!\n";
            Assert(("More than one match!",false));
            return lastOverload;
        }
        lastOverload = &overload;
        validOverloads++;
    }
    if(lastOverload)
        return lastOverload;
    if(uintOverloads == 1)
        return uintOverload;
    if(sintOverloads == 1)
        return sintOverload;
    if(intOverloads == 1)
        return intOverload;
    return nullptr;
}

// FnOverloads::Overload* FnOverloads::getOverload(AST* ast, DynamicArray<TypeId>& argTypes, DynamicArray<TypeId>& polyArgs, ASTExpression* fncall, bool implicitPoly, bool canCast){
FnOverloads::Overload* FnOverloads::getOverload(AST* ast, QuickArray<TypeId>& argTypes, QuickArray<TypeId>& polyArgs, StructImpl* parentStruct, ASTExpression* fncall, bool implicitPoly, bool canCast){
    using namespace engone;
    // nocheckin IMPORTANT BUG: We compare poly args of a function BUT NOT the parent struct.
    // That means that we match if two parent structs have different args.

    // Assert(!fncall->hasImplicitThis()); // copy code from other getOverload
    FnOverloads::Overload* outOverload = nullptr;
    // TODO: Check all overloads in case there are more than one match.
    //  Good way of finding a bug in the compiler.
    //  An optimised build would not do this.
    for(int i=0;i<(int)polyImplOverloads.size();i++){
        auto& overload = polyImplOverloads[i];
        bool doesPolyArgsMatch = true;
        // The number of poly args must match. Using 1 poly arg when referring to a function with 2
        // does not make sense.
        if(overload.funcImpl->polyArgs.size() != polyArgs.size() && !implicitPoly)
            continue;
        if((overload.funcImpl->structImpl == nullptr) != (parentStruct == nullptr))
            continue;
        if(parentStruct) {
            if(overload.funcImpl->structImpl->polyArgs.size() != parentStruct->polyArgs.size() /* && !implicitPoly */)
                continue;
            // I don't think implicitPoly should affects this
            for(int j=0;j<(int)parentStruct->polyArgs.size();j++){
                if(parentStruct->polyArgs[j] != overload.funcImpl->structImpl->polyArgs[j]){
                    doesPolyArgsMatch = false;
                    break;
                }
            }
            if(!doesPolyArgsMatch)
                    continue;
        }
        bool found = true;

        if(!implicitPoly){
            // The args must match exactly. Otherwise, a new implementation should be generated.
            for(int j=0;j<(int)polyArgs.size();j++){
                if(polyArgs[j] != overload.funcImpl->polyArgs[j]){
                    doesPolyArgsMatch = false;
                    break;
                }
            }
            if(!doesPolyArgsMatch)
                continue;
        }

        if (fncall->hasImplicitThis()) {
            // Implicit this means that the arguments the function call has won't have the this argument (this = the object the method is called from)
            // But the function implementation uses a this argument so we do -1 and +1 in some places in the code below
            // to account for this.
            if(fncall->nonNamedArgs > overload.astFunc->arguments.size()-1 // can't match if the call has more essential args than the total args the overload has
                || fncall->nonNamedArgs < overload.astFunc->nonDefaults-1 // can't match if the call has less essential args than the overload (excluding defaults)
                || argTypes.size() > overload.astFunc->arguments.size()-1
                )
                continue;

            if(canCast) {
                for(int j=0;j<(int)fncall->nonNamedArgs;j++){
                    if(!ast->castable(argTypes[j], overload.funcImpl->argumentTypes[j+1].typeId)) {
                        found = false;
                        break;
                    }
                    // log::out << ast->typeToString(overload.funcImpl->argumentTypes[j].typeId) << " = "<<ast->typeToString(argTypes[j])<<"\n";
                }
            } else {
                for(int j=0;j<(int)fncall->nonNamedArgs;j++){
                    if(argTypes[j] != overload.funcImpl->argumentTypes[j+1].typeId) {
                        // TODO: foundInt, see non-poly version of getOverload
                        found = false;
                        break;
                    }
                }
            }
        } else {
            if(fncall->nonNamedArgs > overload.astFunc->arguments.size() // can't match if the call has more essential args than the total args the overload has
                || fncall->nonNamedArgs < overload.astFunc->nonDefaults // can't match if the call has less essential args than the overload (excluding defaults)
                || argTypes.size() > overload.astFunc->arguments.size()
                )
                continue;

            if(canCast) {
                for(int j=0;j<(int)fncall->nonNamedArgs;j++){
                    if(!ast->castable(argTypes[j], overload.funcImpl->argumentTypes[j].typeId)) {
                        found = false;
                        break;
                    }
                    // log::out << ast->typeToString(overload.funcImpl->argumentTypes[j].typeId) << " = "<<ast->typeToString(argTypes[j])<<"\n";
                }
            } else {
                for(int j=0;j<(int)fncall->nonNamedArgs;j++){
                    if(argTypes[j] != overload.funcImpl->argumentTypes[j].typeId) {
                        // TODO: foundInt, see non-poly version of getOverload
                        found = false;
                        break;
                    }
                }
            }

        }
        if(!found)
            continue;
        if(canCast)
            return &overload;
        // NOTE: You can return here because there should only be one matching overload.
        // But we keep going in case we find more matches which would indicate
        // a bug in the compiler. An optimised build would not do this.
        if(outOverload) {
            // log::out << log::RED << __func__ <<" (COMPILER BUG): More than once match!\n";
            Assert(("More than one match!",false));
            return outOverload;
        }
        outOverload = &overload;
    }
    return outOverload;
}
// void ASTFunction::pushPolyState(QuickArray<TypeId>* funcPolyArgs, QuickArray<TypeId>* structPolyArgs){
void ASTFunction::pushPolyState(FuncImpl* funcImpl) {
    pushPolyState(&funcImpl->polyArgs, funcImpl->structImpl);
}
void ASTFunction::pushPolyState(QuickArray<TypeId>* funcPolyArgs, StructImpl* structParent) {
    Assert((parentStruct == nullptr) == (structParent == nullptr));
    polyStates.add({});
    auto& state = polyStates.last();
    if(parentStruct){
        Assert(parentStruct->polyArgs.size() == structParent->polyArgs.size());
        state.structTypes.resize(parentStruct->polyArgs.size());
        for(int j=0;j<(int)parentStruct->polyArgs.size();j++){
            state.structTypes[j] = parentStruct->polyArgs[j].virtualType->id;
            parentStruct->polyArgs[j].virtualType->id = structParent->polyArgs.get(j);
        }
    }
    Assert(polyArgs.size() == funcPolyArgs->size());
    state.argTypes.resize(polyArgs.size());
    for(int j=0;j<(int)polyArgs.size();j++){
        state.argTypes[j] = polyArgs[j].virtualType->id;
        polyArgs[j].virtualType->id = funcPolyArgs->get(j);
    }
}
void ASTFunction::popPolyState(){
    Assert(polyStates.size()>0);
    auto& state = polyStates.last();
    for(int j=0;j<(int)polyArgs.size();j++){
        polyArgs[j].virtualType->id = state.argTypes[j];
    }
    if(parentStruct){
        for(int j=0;j<(int)parentStruct->polyArgs.size();j++){
            parentStruct->polyArgs[j].virtualType->id = state.structTypes[j];
        }
    }
    polyStates.pop();
}
void ASTStruct::pushPolyState(StructImpl* structImpl) {
    polyStates.add({});
    auto& state = polyStates.last();
    Assert(polyArgs.size() == structImpl->polyArgs.size());
    state.structPolyArgs.resize(polyArgs.size());
    for(int j=0;j<(int)polyArgs.size();j++){
        state.structPolyArgs[j] = polyArgs[j].virtualType->id;
        polyArgs[j].virtualType->id = structImpl->polyArgs.get(j);
    }
}
void ASTStruct::popPolyState(){
    Assert(polyStates.size()>0);
    auto& state = polyStates.last();
    for(int j=0;j<(int)polyArgs.size();j++){
        polyArgs[j].virtualType->id = state.structPolyArgs[j];
    }
    polyStates.pop();
}
// ASTFunction* FnOverloads::getPolyOverload(AST* ast, DynamicArray<TypeId>& typeIds, DynamicArray<TypeId>& polyTypes){
//     using namespace engone;
//     ASTFunction* outFunc = nullptr;
//     for(int i=0;i<(int)polyOverloads.size();i++){
//         PolyOverload& overload = polyOverloads[i];
//         if(overload.astFunc->polyArgs.size() != polyTypes.size())
//             continue;// unless implicit polymorphic types
//         for(int j=0;j<(int)overload.astFunc->polyArgs.size();j++){
//             overload.astFunc->polyArgs[j].virtualType->id = polyTypes[j];
//         }
//         defer {
//             for(int j=0;j<(int)overload.astFunc->polyArgs.size();j++){
//                 overload.astFunc->polyArgs[j].virtualType->id = {};
//             }
//         };
//         // continue if more args than possible
//         // continue if less args than minimally required
//         if(typeIds.size() > overload.astFunc->arguments.size() || typeIds.size() < overload.argTypes.size())
//             continue;
//         bool found = true;
//         for (int j=0;j<(int)overload.argTypes.size();j++){
//             TypeId argType = ast->ensureNonVirtualId(overload.argTypes[j]);
//             if(argType != typeIds[j]
//             //  && overload.argTypes[j] != AST_POLY
//              ){
//                 found = false;
//                 break;
//             }
//         }
//         if(found){
//             // return &overload;
//             // NOTE: You can return here because there should only be one matching overload.
//             // But we keep going in case we find more matches which would indicate
//             // a bug in the compiler. An optimised build would not do this.
//             if(outFunc) {
//                 // log::out << log::RED << __func__ <<" (COMPILER BUG): More than once match!\n";
//                 Assert(("More than one match!",false));
//                 return outFunc;
//             }
//             outFunc = overload.astFunc;
//         }
//     }
//     return outFunc;
// }

void FnOverloads::addOverload(ASTFunction* astFunc, FuncImpl* funcImpl){
    overloads.add({astFunc,funcImpl});
}
FnOverloads::Overload* FnOverloads::addPolyImplOverload(ASTFunction* astFunc, FuncImpl* funcImpl){
    polyImplOverloads.add({astFunc,funcImpl});
    return &polyImplOverloads[polyImplOverloads.size()-1];
}
void FnOverloads::addPolyOverload(ASTFunction* astFunc){
    polyOverloads.add({astFunc});
    // auto& po = polyOverloads[polyOverloads.size()-1];
    // po.argTypes.stealFrom(typeIds);
}

FnOverloads* ASTStruct::getMethod(const std::string& name, bool create){
    auto pair = _methods.find(name);
    if(pair == _methods.end()){
        if(create)
            return &(_methods[name] = {});
        
        return nullptr;
    }
    return &pair->second;
}

void AST::shareWithGlobalScope(ASTScope* body) {
    lock_globalScope.lock();
    auto scope = getScope(globalScope->scopeId);
    scope->sharedScopes.add(getScope(body->scopeId));
    lock_globalScope.unlock();
}
// void AST::appendToMainBody(ASTScope *body) {
//     Assert(body);
    
//     // if(globalScope->tokenRange.tokenStream() == nullptr) {
//     //     // IMPORTANT TODO: This is so horribly bad and I will regret this later.
//     //     //    The main body has multiple scopes appended to it. They all come from different token streams.
//     //     //    Adding to the madness, we assume that the preloaded import doesn't have a tokenStream to skip it.
//     //     //    We may not have a token stream if the import is empty.
//     //     if (body->tokenRange.tokenStream() && body->tokenRange.tokenStream()->streamName != "<base>") {
//     //         globalScope->tokenRange = body->tokenRange;
//     //     }
//     // }

//     // engone::log::out << engone::log::RED << "Content order is ruined in appendToMainBody\n";

//     // How do we maintain it.
//     // Content order of existing children don't need to be recalculated.
//     // Content order of all child scopes must recalculated.
//     {
//         // ZoneNamedN(zone0, "lock_main", true);
//         lock_globalScope.lock();
//     }
//     for(auto it : body->content) {
//         switch(it.spotType) {
//         case ASTScope::STRUCT: {
//             globalScope->add(this, body->structs[it.index]);
//             break;
//         }
//         case ASTScope::ENUM: {
//             globalScope->add(this, body->enums[it.index]);
//             break; 
//         }
//         case ASTScope::FUNCTION: {
//             globalScope->add(this, body->functions[it.index]);
//             break; 
//         }
//         case ASTScope::STATEMENT: {
//             globalScope->add(this, body->statements[it.index]);
//             // if(body->statements[it.index]->globalDeclaration) {
//             //     globalScope->add_at(this, body->statements[it.index], CONTENT_ORDER_ZERO);
//             // } else {
//             // }
//             break; 
//         }
//         case ASTScope::NAMESPACE: {
//             globalScope->add(this, body->namespaces[it.index]);
//             break;
//         }
//         default: Assert(false);
//         }
//     }
//     body->structs.cleanup();
//     body->enums.cleanup();
//     body->functions.cleanup();
//     body->statements.cleanup();
//     body->namespaces.cleanup();
//     body->content.cleanup();
//     // #define _ADD(X) for(auto it : body->X) { globalScope->add(this, it); } body->X.cleanup();
//     // _ADD(enums)
//     // _ADD(functions)
//     // _ADD(structs)
//     // _ADD(statements)
//     // // for(auto it : body->statements) { 
//     // //     // if(it->firstBody) {

//     // //     // } else if() {

//     // //     // }
//     // //     globalScope->add(it);
//     // // }
//     // _ADD(namespaces)
//     // // for(auto it : body->namespaces) { globalScope->add(it, this); } body->namespaces.cleanup();
//     // #undef ADD
//     destroy(body);
//     lock_globalScope.unlock();
// }

ASTScope *AST::createBody() {
    ZoneScopedC(tracy::Color::Gold);
    auto ptr = (ASTScope *)allocate(sizeof(ASTScope));
    new(ptr) ASTScope();
    ptr->nodeId = getNextNodeId();
    ptr->isNamespace = false;
    return ptr;
}
ASTStatement *AST::createStatement(ASTStatement::Type type) {
    ZoneScopedC(tracy::Color::Gold);
    auto ptr = (ASTStatement *)allocate(sizeof(ASTStatement));
    new(ptr) ASTStatement();
    ptr->nodeId = getNextNodeId();
    ptr->type = type;
    return ptr;
}
ASTStruct *AST::createStruct(const StringView& name) {
    auto ptr = (ASTStruct *)allocate(sizeof(ASTStruct));
    new(ptr) ASTStruct();
    ptr->nodeId = getNextNodeId();
    ptr->name = name;
    return ptr;
}
ASTEnum *AST::createEnum(const StringView &name) {
    auto ptr = (ASTEnum *)allocate(sizeof(ASTEnum));
    new(ptr) ASTEnum();
    ptr->nodeId = getNextNodeId();
    ptr->name = name;
    return ptr;
}
ASTFunction *AST::createFunction() {
    auto ptr = (ASTFunction *)allocate(sizeof(ASTFunction));
    new(ptr) ASTFunction();
    ptr->nodeId = getNextNodeId();
    return ptr;
}
ASTExpression *AST::createExpression(TypeId type) {
    ZoneScopedC(tracy::Color::Gold);
    auto ptr = (ASTExpression *)allocate(sizeof(ASTExpression));
    new(ptr) ASTExpression();
    ptr->nodeId = getNextNodeId();
    ptr->isValue = (u32)type.getId() < AST_PRIMITIVE_COUNT;
    ptr->typeId = type;
    return ptr;
}
ASTScope *AST::createNamespace(const StringView& name) {
    auto ptr = (ASTScope *)allocate(sizeof(ASTScope));
    new(ptr) ASTScope();
    ptr->nodeId = getNextNodeId();
    ptr->isNamespace = true;
    ptr->name = (std::string*)allocate(sizeof(std::string));
    new(ptr->name)std::string(name);
    return ptr;
}
// #define TAIL_ADD(M, S)     
//     if (!M)                
//         M##Tail = M = S;   
//     else                   
//         M##Tail->next = S; 
//     if (tail) M##Tail = tail; else while (M##Tail->next)  
//         M##Tail = M##Tail->next;

#define TAIL_ADD(M, S) Assert(("should not add null", S)); M.add(S);

void ASTScope::add(AST* ast, ASTStatement *astStatement) {
    TAIL_ADD(statements, astStatement)
    content.add({STATEMENT, statements.size()-1});
    if(astStatement->firstBody) {
        ast->getScope(astStatement->firstBody->scopeId)->contentOrder = content.size()-1;
    }
    if(astStatement->secondBody) {
        ast->getScope(astStatement->secondBody->scopeId)->contentOrder = content.size()-1;
    }
    FOR(astStatement->switchCases) {
        // nocheckin IMPORTANT: Will this work, the switch case scopes having the same order?
        ast->getScope(it.caseBody->scopeId)->contentOrder = content.size()-1;
    }
}
// void ASTScope::add_at(AST* ast, ASTStatement *astStatement, ContentOrder contentOrder) {
//     TAIL_ADD(statements, astStatement)
    
//     // if(contentOrder == CONTENT_ORDER_ZERO) {
//     //     // nocheckin, This is used to put globals at the beginning and making sure they are checked
//     //     // first. This was the easiest way to implement that but probably not the best.
//     //     // This is only used for the root scope or wherever you appendToMainBody.
//     //     content.add({});
//     //     memmove(content.data() + 1, content.data(), (content.size()-1) * sizeof(Spot));
//     //     content[0] = {STATEMENT, statements.size()-1};
//     // } else {
//     //     Assert(("ASTScope::add_at, contentOrder must be zero, incomplete",false));
//     // }
//     content.add({STATEMENT, statements.size() -1 });

//     if(astStatement->firstBody) {
//         ast->getScope(astStatement->firstBody->scopeId)->contentOrder = content.size()-1;
//     }
//     if(astStatement->secondBody) {
//         ast->getScope(astStatement->secondBody->scopeId)->contentOrder = content.size()-1;
//     }
//     FOR(astStatement->switchCases) {
//         // nocheckin IMPORTANT: Will this work, the switch case scopes having the same order?
//         ast->getScope(it.caseBody->scopeId)->contentOrder = content.size()-1;
//     }
// }
void ASTScope::add(AST* ast, ASTStruct *astStruct) {
    TAIL_ADD(structs, astStruct)
    content.add({STRUCT, structs.size()-1});
    ast->getScope(astStruct->scopeId)->contentOrder = content.size()-1;
}
void ASTScope::add(AST* ast, ASTFunction *astFunction) {
    TAIL_ADD(functions, astFunction)
    content.add({FUNCTION, functions.size()-1});
    ast->getScope(astFunction->scopeId)->contentOrder = content.size()-1;
}
void ASTScope::add(AST* ast, ASTEnum *astEnum) {
    TAIL_ADD(enums, astEnum)
    content.add({ENUM, enums.size()-1});
}
void ASTScope::add(AST* ast, ASTScope* astNamespace){
    Assert(("should not add null",astNamespace));
    for(auto ns : namespaces){
        // ASTScope* ns = namespaces.get(i);

        if(*ns->name == *astNamespace->name){
            for(auto it : astNamespace->content) {
                switch(it.spotType) {
                case ASTScope::STRUCT: {
                    ns->add(ast, astNamespace->structs[it.index]);
                }
                break; case ASTScope::ENUM: {
                    ns->add(ast, astNamespace->enums[it.index]);
                }
                break; case ASTScope::FUNCTION: {
                    ns->add(ast, astNamespace->functions[it.index]);
                }
                break; case ASTScope::STATEMENT: {
                    ns->add(ast, astNamespace->statements[it.index]);
                }
                break; case ASTScope::NAMESPACE: {
                    ns->add(ast, astNamespace->namespaces[it.index]);
                }
            }
            }
            // for(auto it : astNamespace->enums)
            //     ns->add(ast, it);
            // for(auto it : astNamespace->functions)
            //     ns->add(ast, it);
            // for(auto it : astNamespace->structs)
            //     ns->add(ast, it);
            // for(auto it : astNamespace->namespaces)
            //     ns->add(ast, it);
            astNamespace->enums.cleanup();
            astNamespace->functions.cleanup();
            astNamespace->structs.cleanup();
            astNamespace->namespaces.cleanup();
            ast->destroy(astNamespace);
            return;
        }
    }
    namespaces.add(astNamespace);
    content.add({NAMESPACE, namespaces.size()-1});
    ast->getScope(astNamespace->scopeId)->contentOrder = content.size()-1;
}
void AST::Destroy(AST *ast) {
    if (!ast)
        return;
    ast->cleanup();
    ast->~AST();
    // engone::Free(ast, sizeof(AST));
    TRACK_FREE(ast, AST);
}

AST::ConstString& AST::getConstString(const std::string& str, u32* outIndex){
    Assert(outIndex);
    auto pair = _constStringMap.find(str);
    if(pair != _constStringMap.end()){
        *outIndex = pair->second;
        return _constStrings[pair->second];
    }
    AST::ConstString tmp={};
    tmp.length = str.length();
    _constStrings.add(tmp);
    *outIndex = _constStrings.size()-1;
    _constStringMap[str] = _constStrings.size()-1;
    return _constStrings[_constStrings.size()-1];
}
AST::ConstString& AST::getConstString(u32 index){
    Assert(index < _constStrings.size());
    return _constStrings[index];
}
void AST::cleanup() {
    using namespace engone;

    for (auto &scope : _scopeInfos) {
        scope->nameTypeMap.clear();
        scope->~ScopeInfo();
        // engone::Free(scope, sizeof(ScopeInfo));
    }
    _scopeInfos.cleanup();

    // for(auto& ti : _typeInfos){
    for(int i=0;i<(int)_typeInfos.size();i++){
        auto ti = _typeInfos[i];
        // typeInfos is resized and filled with nullptrs
        // when types are created
        if(!ti) continue;

        if(ti->structImpl){
            // log::out << pair->astStruct->name << " CLEANED\n";
            ti->structImpl->~StructImpl();
            // engone::Free(pair->structImpl, sizeof(StructImpl));
        }
        ti->~TypeInfo();
        // engone::Free(pair, sizeof(TypeInfo));
    }
    _typeInfos.cleanup();
    _constStringMap.clear();
    _constStrings._reserve(0);


    // for(auto& str : tempStrings){
    //     str->~basic_string<char>();
    //     // engone::Free(str, sizeof(std::string));
    // }
    // tempStrings.cleanup();

    for(auto ptr : variables){
        ptr->~VariableInfo();
        // engone::Free(ptr, sizeof(VariableInfo));
    }
    variables.cleanup();

    nextTypeId = AST_OPERATION_COUNT;

    destroy(globalScope);
    globalScope = nullptr;

    // make sure allocated nodes and other objects in linear allocator
    // have been destroyed before freeing it.
    if(linearAllocation) {
        TRACK_ARRAY_FREE(linearAllocation, char, linearAllocationMax);
        // engone::Free(linearAllocation, linearAllocationMax);
    }
    // log::out << "Linear: "<<linearAllocationUsed<<"\n";
    linearAllocation = nullptr;
    linearAllocationMax = 0;
    linearAllocationUsed = 0;
}
ScopeInfo* AST::createScope(ScopeId parentScope, ContentOrder contentOrder, ASTScope* astScope) {
    ZoneScopedC(tracy::Color::Gold);

    auto ptr = (ScopeInfo *)allocate(sizeof(ScopeInfo));
    u32 id = 0;
    
    // id = nextScopeInfoIndex++; // nocheckin

    id = engone::atomic_add((volatile i32*)&nextScopeInfoIndex, 1) - 1;
    _scopeInfos[id] = ptr;
    
    // lock_scopes.lock(); // too slow
    // id = _scopeInfos.size();
    // _scopeInfos.add(ptr);
    // lock_scopes.unlock();
    
    new(ptr) ScopeInfo{id};
    
    ptr->parent = parentScope;
    ptr->contentOrder = contentOrder;
    ptr->astScope = astScope;

    return ptr;
}
ScopeInfo* AST::getScope(ScopeId id){
    ScopeInfo* ptr = nullptr;
    // lock_scopes.lock(); // too slow
    if(_scopeInfos.size() > id)
        ptr = _scopeInfos[id];
    // lock_scopes.unlock();
    return ptr;
}
ScopeInfo* AST::findScope(StringView name, ScopeId scopeId, bool search_parent_scopes){
    using namespace engone;
    StringView nextName = name;
    ScopeId nextScopeId = scopeId;
    
    WHILE_TRUE {
        ScopeInfo* scope = getScope(nextScopeId);
        if(!scope) return nullptr;
        int splitIndex = -1;
        for(int i=0;i<nextName.len-1;i++){
            if(nextName.ptr[i] == ':' && nextName.ptr[i+1] == ':'){
                splitIndex = i;
                break;
            }
        }
        if(splitIndex == -1){
            auto pair = scope->nameScopeMap.find(std::string(nextName.ptr,nextName.len));
            if(pair != scope->nameScopeMap.end()){
                return getScope(pair->second);
            }
            for(int i=0;i<(int)scope->sharedScopes.size();i++){
                ScopeInfo* usingScope = scope->sharedScopes[i];
                auto pair = usingScope->nameScopeMap.find(std::string(nextName.ptr,nextName.len));
                if(pair != usingScope->nameScopeMap.end()){
                    return getScope(pair->second);
                }
            }
            if(search_parent_scopes) {
                if(nextScopeId != scope->parent) { // Check infinite loop. Mainly for global scope.
                    nextScopeId = scope->parent;
                    continue;
                }
            }
            return nullptr;
        } else {
            StringView first = {};
            first.ptr = nextName.ptr;
            first.len = splitIndex;
            StringView rest = {};
            rest.ptr = nextName.ptr += splitIndex+2;
            rest.len = nextName.len - (splitIndex + 2);
            auto pair = scope->nameScopeMap.find(std::string(first.ptr,first.len));
            if(pair != scope->nameScopeMap.end()){
                nextName = rest;
                nextScopeId = pair->second;
                continue;
            }
            bool cont = false;
            for(int i=0;i<(int)scope->sharedScopes.size();i++){
                ScopeInfo* usingScope = scope->sharedScopes[i];
                auto pair = usingScope->nameScopeMap.find(std::string(first.ptr,first.len));
                if(pair != usingScope->nameScopeMap.end()){
                    nextName = rest;
                    nextScopeId = pair->second;
                    cont = true;
                    break;
                }
            }
            if(cont) continue;
            if(search_parent_scopes) {
                if(nextScopeId != scope->parent) {
                    nextScopeId = scope->parent;
                    continue;
                }
            }
            return nullptr;
        }
    }
    return nullptr;
}
// ScopeInfo* AST::findScopeFromParents(StringView name, ScopeId scopeId){
//     return findScope(name,scopeId,true);
// }

// TypeId AST::getTypeString(Token name){
//     // converts char[] into Slice<char> (or any type, not just char)
//     if(name.length>2&&!strncmp(name.str+name.length-2,"[]",2)){
//         std::string* str = createString();
//         *str = "Slice<";
//         str->append(std::string(name.str,name.length-2));
//         (*str)+=">";
//         name = *str;
//     }
//     for(int i=0;i<(int)_typeTokens.size();i++){
//         if(name == _typeTokens[i])
//             return TypeId::CreateString(i);
//     }
//     _typeTokens.add(name);
//     return TypeId::CreateString(_typeTokens.size()-1);
// }
TypeId AST::getTypeString(const std::string& name){
    // return TypeId::Create(AST_INT32); // nocheckin
    // ZoneScoped;

    // converts char[] into Slice<char> (or any type, not just char)
    u32 index = 0;
    if(name.length()>2&&!strncmp(name.c_str()+name.length()-2,"[]",2)){
        std::string str = "Slice<" + name + ">";
        lock_typeTokens.lock(); // slow
        for(int i=0;i<(int)_typeTokens.size();i++){
            if(str == _typeTokens[i]) {
                lock_typeTokens.unlock();
                return TypeId::CreateString(i);
            }
        }
        index = _typeTokens.size();
        _typeTokens.add(str);
        lock_typeTokens.unlock();
    } else {
        lock_typeTokens.lock(); // slow
        for(int i=0;i<(int)_typeTokens.size();i++){
            if(name == _typeTokens[i]) {
                lock_typeTokens.unlock();
                return TypeId::CreateString(i);
            }
        }
        index = _typeTokens.size();
        _typeTokens.add(name);
        lock_typeTokens.unlock();
    }
    return TypeId::CreateString(index);
}
StringView AST::getStringFromTypeString(TypeId typeId){
    Assert(typeId.isString());
    if(!typeId.isString())
        return {};

    u32 index = typeId.getId();
    if(index >= _typeTokens.size())
        return {};
    
    // return _typeTokens[index];
    auto& s = _typeTokens[index];
    return { s.c_str() };
}
TypeId AST::convertToTypeId(TypeId typeString, ScopeId scopeId, bool transformVirtual){
    Assert(typeString.isString());
    StringView tstring = getStringFromTypeString(typeString);
    return convertToTypeId(tstring, scopeId, transformVirtual);
}
// void ll(AST* ast){
//     for(int i=0;i<ast->_typeInfos.size();i++){
//         auto ti = ast->_typeInfos[i];
//         // typeInfos is resized and filled with nullptrs
//         // when types are created
//         // if(!ti) continue;

//         if(ti->structImpl){
//             // log::out << pair->astStruct->name << " CLEANED\n";
//             // ti->structImpl->~StructImpl();
//             // engone::Free(pair->structImpl, sizeof(StructImpl));
//         }
//         // ti->~TypeInfo();
//         // engone::Free(pair, sizeof(TypeInfo));
//     }
// }
TypeInfo* AST::createType(StringView name, ScopeId scopeId){
    using namespace engone;
    auto scope = getScope(scopeId);
    if(!scope) return nullptr;
    
    auto nearPair = scope->nameTypeMap.find(name);
    if(nearPair != scope->nameTypeMap.end()) {
        return nullptr; // type already exists
    }
    // You could check if name already exists in parent
    // scopes to but I think it is fine for now.

    auto ptr = (TypeInfo *)allocate(sizeof(TypeInfo));
    // Assert(ptr);
    new(ptr) TypeInfo{name, TypeId::Create(nextTypeId++)};
    ptr->scopeId = scopeId;
    scope->nameTypeMap[name] = ptr;
    if(ptr->id.getId() >= _typeInfos.size()) {
        _typeInfos.resize(ptr->id.getId() + AST_PRIMITIVE_COUNT);

    }
    _typeInfos[ptr->id.getId()] = ptr;
    // ll(this);
    return ptr;
}
engone::Logger& operator <<(engone::Logger& logger, TypeId typeId) {
    if(typeId.getId() < AST_PRIMITIVE_COUNT) {
        logger << PRIM_NAME(typeId.getId());
    } else if(typeId.getId() < AST_OPERATION_COUNT) {
        logger << OP_NAME(typeId.getId());
    } else {
        logger << "TypeId{";
        if(typeId.isValid()) {
            logger << typeId.getId();
            if(typeId.string)
                logger << ",string";
            if(typeId.string)
                logger << ",poison";
            if(typeId.pointer_level > 0)
                logger << ",ptr:"<<typeId.pointer_level;
        }
        else
            logger << "invalid";
        logger <<"}";
    }
    return logger;
}
TypeInfo* AST::createPredefinedType(StringView name, ScopeId scopeId, TypeId id, u32 size) {
    auto ptr = (TypeInfo *)allocate(sizeof(TypeInfo));
    new(ptr) TypeInfo{name, id, size};
    ptr->scopeId = scopeId;
    auto scope = getScope(scopeId);
    if(!scope) return nullptr;
    
    if(ptr->id.getId() >= _typeInfos.size()) {
        _typeInfos.resize(ptr->id.getId() + AST_PRIMITIVE_COUNT);
    }
    _typeInfos[ptr->id.getId()] = ptr;
    scope->nameTypeMap[ptr->name] = ptr;
    // ll(this);
    return ptr;
}
void AST::printTypesFromScope(ScopeId scopeId, int scopeLimit){
    using namespace engone;
    log::out << __func__<<":\n";
    int parentLevel = 0;
    Assert(scopeLimit==-1);
    ScopeId nextScopeId = scopeId;
    WHILE_TRUE {
        ScopeInfo* scope = getScope(nextScopeId);
        if(!scope) return;

        // if(scope->nameTypeMap.size)
        log::out << " Scope "<<nextScopeId<<":\n";
        for(auto& pair : scope->nameTypeMap){
            log::out << "  "<<pair.first << ", \n";
        }
        // bool brea=false;
        Assert(scope->sharedScopes.size()==0);
        // for(int i=0;i<(int)scope->usingScopes.size();i++){
        //     ScopeInfo* usingScope = scope->usingScopes[i];
        //     auto pair = usingScope->nameTypeMap.find(typeName);
        //     if(pair != usingScope->nameTypeMap.end()){
        //         typeInfo = pair->second;
        //         brea = true;
        //         break;
        //     }
        // }
        // if(brea) break;
        if(nextScopeId==scope->parent) // prevent infinite loop
            break;
        nextScopeId = scope->parent;
    }
}
TypeId AST::convertToTypeId(StringView typeString, ScopeId scopeId, bool transformVirtual) {
    using namespace engone;
    u32 pointerLevel = 0;
    StringView namespacing = {};
    StringView typeName;
    DecomposePointer(typeString, &typeName, &pointerLevel);
    DecomposeNamespace(typeName, &namespacing, &typeName);
    
    // Token baseType = TrimBaseType(typeString, &namespacing, &pointerLevel, &polyTypes, &typeName);
    // if(polyTypes.size()!=0)
    //     log::out << log::RED << "Polymorphic types are not handled ("<<typeString<<")\n";
    // TODO: Polymorphism!s

    ScopeInfo* scope = nullptr;
    TypeInfo* typeInfo = nullptr;
    if(namespacing.ptr){
        scope = findScope(namespacing, scopeId, true);
        if(!scope) return {};
        auto firstPair = scope->nameTypeMap.find(typeName);
        if(firstPair != scope->nameTypeMap.end()){
            typeInfo = firstPair->second;
        }
    } else {
        // Find base type in parent scopes
        ScopeId nextScopeId = scopeId;
        // log::out << "find "<<typeString<<"\n";
        WHILE_TRUE_N(1000) {
            ScopeInfo* scope = getScope(nextScopeId);
            if(!scope) return {};
            // for(auto& pair : scope->nameTypeMap){
            //     log::out <<" "<<pair.first << " : " <<pair.second->id.getId() << "\n";
            // }
            auto pair = scope->nameTypeMap.find(typeName);
            if(pair!=scope->nameTypeMap.end()){
                typeInfo = pair->second;
                break;
            }
            bool brea=false;
            for(int i=0;i<(int)scope->sharedScopes.size();i++){
                ScopeInfo* usingScope = scope->sharedScopes[i];
                auto pair = usingScope->nameTypeMap.find(typeName);
                if(pair != usingScope->nameTypeMap.end()){
                    typeInfo = pair->second;
                    brea = true;
                    break;
                }
            }
            if(brea) break;
            if(nextScopeId==scope->parent) // prevent infinite loop
                break;
            nextScopeId = scope->parent;
        }
    }
    if(!typeInfo) {
        return {};
    }
    if(transformVirtual) {
        if(typeInfo->id != typeInfo->originalId) {
            WHILE_TRUE {
                TypeInfo* virtualInfo = getTypeInfo(typeInfo->id);
                if(virtualInfo && typeInfo != virtualInfo){
                    typeInfo = virtualInfo;
                } else {
                    break;
                }
            }
            if(!typeInfo->id.isValid()) {
                MSG_CODE_LOCATION;
                log::out << log::RED << "Compiler Bug: Transformed virtual had TypeInfo but it's virtual TypeId was invalid (all zeros)\n";
            }
        }
        TypeId out = typeInfo->id;
        out.setPointerLevel(pointerLevel);
        return out;
    } else {
        TypeId out = typeInfo->originalId;
        out.setPointerLevel(pointerLevel);
        return out;
    }

}

TypeInfo *AST::getBaseTypeInfo(TypeId id) {
    Assert(!id.isString());
    if(!id.isValid() && !id.isString()) return nullptr;
    if(id.getId() >= _typeInfos.size()) return nullptr;
    return _typeInfos[id.getId()];
}
TypeInfo *AST::getTypeInfo(TypeId id) {
    // These if checks have caught many mistakes.
    // Do not remove them. Explicitly use TypeId.baseType() to get the type info.
    if(!id.isValid())
        return nullptr;
    Assert(id.isNormalType());
    Assert(id.getId() < _typeInfos.size());
    // if(!id.isNormalType())
    //     return nullptr;
    if(id.getId() >= _typeInfos.size())
        return nullptr;
    return _typeInfos[id.getId()];
}
void ScopeInfo::print(AST* ast) {
    using namespace engone;
    log::out << log::LIME << "Scope " << id<<"\n";
    if(nameTypeMap.size() > 0) {
        log::out << " Types: " << log::GRAY;
        for(auto& pair : nameTypeMap) {
            engone::log::out << pair.first << " {"<<pair.second->id.getId()<<"}, ";
        }
        log::out << log::NO_COLOR << "\n";
    }
    if(identifierMap.size() > 0) {
        log::out << " Identifiers: " << log::GRAY;
        for(auto& pair : identifierMap) {
            engone::log::out << pair.first << ", ";
        }
        log::out << log::NO_COLOR <<"\n";
    }
    ScopeInfo* sc = ast->getScope(parent);
    if(sc != this) {
        log::out << "Parent-";
        sc->print(ast);
    }
}
std::string AST::typeToString(TypeId typeId){
    using namespace engone;
    if(typeId.isString())
        return std::string(getStringFromTypeString(typeId));
    const char* cstr = OP_NAME(typeId.getId());
    if(cstr)
        return cstr;
    cstr = PRIM_NAME(typeId.getId());
    if(cstr)
        return cstr;
    std::string out="";
    TypeInfo* ti = getBaseTypeInfo(typeId);
    if(!ti)
        return "";

    ScopeInfo* scope = getScope(ti->scopeId);
    std::string ns = scope->getFullNamespace(this);
    if(ns.empty()){
        out = ti->name;
    } else {
        out = ns + "::" + std::string(ti->name);
    }
    if(ti->astStruct && !ti->structImpl && ti->astStruct->polyArgs.size()!=0){
        out +="<";
        for(int i=0;i<(int)ti->astStruct->polyArgs.size();i++){
            if(i!=0)
                out += ",";
            out += ti->astStruct->polyArgs[i].name;
        }
        out +=">";
    }
    for(int i=0;i<(int)typeId.getPointerLevel();i++){
        out+="*";
    }
    return out;
}
TypeId AST::ensureNonVirtualId(TypeId id){
    if(!id.isValid() || id.isString()) return id;
    u32 level = id.getPointerLevel();
    id = id.baseType();
    WHILE_TRUE {
        TypeInfo* ti = getTypeInfo(id);
        if(!ti || ti->id == id)
            break;
        id = ti->id;
    }
    id.setPointerLevel(level);
    return id;
}
// std::string* AST::createString(){
//     std::string* ptr = (std::string*)allocate(sizeof(std::string));
//     new(ptr)std::string();
//     tempStrings.add(ptr);
//     return ptr;
// }
void AST::destroy(ASTScope *scope) {
    // if (scope->next)
    //     destroy(scope->next);
    if (scope->name) {
        scope->name->~basic_string<char>();
        // engone::Free(scope->name, sizeof(std::string));
        scope->name = nullptr;
    }
    #define DEST(X) for(auto it : scope->X) destroy(it);
    DEST(structs)
    DEST(enums)
    DEST(functions)
    DEST(statements)
    DEST(namespaces)
    #undef DEST
    scope->~ASTScope();
    // not done with linear allocator
    // engone::Free(scope, sizeof(ASTScope));
}
void AST::destroy(ASTStruct *astStruct) {
    for (auto &mem : astStruct->members) {
        if (mem.defaultValue)
            destroy(mem.defaultValue);
        mem.defaultValue = nullptr;
    }

    // if(astStruct->nonPolyStruct) // destroyed with typeInfo
    //     engone::Free(astStruct->nonPolyStruct, sizeof(StructImpl));
    for(auto f : astStruct->functions){
        destroy(f);
    }
    astStruct->~ASTStruct();
    // engone::Free(astStruct, sizeof(ASTStruct));
}
void AST::destroy(ASTFunction *function) {
    // if (function->next)
    //     destroy(function->next);
    if (function->body)
        destroy(function->body);
    for (auto &arg : function->arguments) {
        if (arg.defaultValue)
            destroy(arg.defaultValue);
    }
    for(auto ptr : function->_impls){
        ptr->~FuncImpl();
        // engone::Free(ptr,sizeof(FuncImpl));
    }
    function->~ASTFunction();
    // engone::Free(function, sizeof(ASTFunction));
}
void AST::destroy(ASTEnum *astEnum) {
    // if (astEnum->next)
    //     destroy(astEnum->next);
    astEnum->~ASTEnum();
    // engone::Free(astEnum, sizeof(ASTEnum));
}
void AST::destroy(ASTStatement *statement) {
    // if (statement->next)
    //     destroy(statement->next);
    if(!statement->sharedContents){
        if (statement->alias){
            statement->alias->~basic_string<char>();
            TRACK_FREE(statement->alias,std::string);
            // engone::Free(statement->alias, sizeof(std::string));
            statement->alias = nullptr;
        }
        // NOTE: hasNodes isn't used properly anywhere.
        // if(statement->hasNodes()){
        if (statement->firstExpression)
            destroy(statement->firstExpression);
        if (statement->firstBody)
            destroy(statement->firstBody);
        if (statement->secondBody)
            destroy(statement->secondBody);
        if (statement->testValue)
            destroy(statement->testValue);
        // } else {
        // assign may use arrayValues (union, returnValues)
        // we must destroy does values
        for(ASTExpression* expr : statement->arrayValues){
            destroy(expr);
        }
        for(auto& it : statement->switchCases){
            destroy(it.caseExpr);
            destroy(it.caseBody);
        }
        // }
    }
    statement->~ASTStatement();
    // engone::Free(statement, sizeof(ASTStatement));
}
void AST::destroy(ASTExpression *expression) {
    // if (expression->next)
    //     destroy(expression->next);
    for(ASTExpression* expr : expression->args){
        destroy(expr);
    }
    // if(expression->args){
    //     for(ASTExpression* expr : *expression->args){
    //         destroy(expr);
    //     }
    //     expression->args->~DynamicArray<ASTExpression*>();
    //     engone::Free(expression->args,sizeof(DynamicArray<ASTExpression*>));
    // }
    // if (expression->name) {
    //     expression->name->~basic_string<char>();
    //     engone::Free(expression->name, sizeof(std::string));
    //     expression->name = nullptr;
    // }
    // if (expression->namedValue){
    //     expression->namedValue->~basic_string<char>();
    //     engone::Free(expression->namedValue, sizeof(std::string));
    //     expression->namedValue = nullptr;
    // }
    if (expression->left)
        destroy(expression->left);
    if (expression->right)
        destroy(expression->right);
    expression->~ASTExpression();
    // engone::Free(expression, sizeof(ASTExpression));
}
u32 TypeInfo::getSize() {
    if (astStruct) {
        if(structImpl) {
            return structImpl->size;
        } else {
            return 0;
        }
    }
    return _size;
}
StructImpl* TypeInfo::getImpl(){
    return structImpl;
}
u32 TypeInfo::alignedSize() {
    if (astStruct) {
        if(structImpl)
            return structImpl->alignedSize;
        else
            return 0;
    }
    return _size > 8 ? 8 : _size;
}
void AST::DecomposeNamespace(StringView view, StringView* out_namespace, StringView* out_name){
    *out_namespace = {};
    *out_name = {};
    if(!view.ptr || view.len < 3) {
        *out_name = view;
        return;
    }

    // TODO: ":::" should cause an error but isn't considered at all here.
    
    // Works
    // Math::Vector<Math::float32> -> Math  Vector<Math::float32>

    // Should not work
    // Math::Floats<Math::float32>::f32 -> Math::Floats<Math::float32>  f32
    // Math::Floats<Math::float32>:::f32 -> Math::Floats<Math::float32>:  f32

    int index_lastColon = -1;
    int depth = 0;
    for(int i=view.len-1;i>=1;i--){
        if(view.ptr[i] == '>'){
            depth++;
        }
        if(view.ptr[i] == '<'){
            depth--;
        }
        if(depth==0){
            if(view.ptr[i-1] == ':' && view.ptr[i] == ':') {
                index_lastColon = i;
                break;
            }
        }
    }
    if(index_lastColon!=-1){
        out_namespace->ptr = view.ptr;
        out_namespace->len = index_lastColon - 1;
        out_name->ptr = view.ptr + index_lastColon + 1;
        out_name->len = view.len - (index_lastColon + 1);
    } else {
        *out_name = view;
    }
    return ;
}
void AST::DecomposePointer(StringView view, StringView* out_name, u32* outLevel){
    out_name->ptr = view.ptr;
    out_name->len = view.len;
    *outLevel = 0;
    
    for(int i=view.len-1;i>=0;i--){
        if(view.ptr[i]=='*') {
            if(outLevel)
                (*outLevel)++;
            continue;
        } else {
            out_name->len = i + 1;
            break;
        }
    }
}
StringView AST::TrimBaseType(StringView typeString, StringView* outNamespace, 
    u32* level, QuickArray<StringView>* outPolyTypes, StringView* typeName)
{
    if(!typeString.ptr || !outNamespace || !level || !outPolyTypes) {
        return typeString;
    }
    // TODO: Line and column in tokens are ignored but should
    //   be seperated into the divided tokens.

    //-- Trim pointers
    for(int i=typeString.len-1;i>=0;i--){
        if(typeString.ptr[i]=='*') {
            (*level)++;
            continue;
        } else {
            typeString.len = i + 1;
            break;
        }
    }

    //-- Trim namespaces
    int lastColonIndex = -1;
    int depth = 0;
    for(int i=typeString.len-1;i>=0;i--){
        if(typeString.ptr[i] == '>'){
            depth++;
        }
        if(typeString.ptr[i] == '<'){
            depth--;
        }
        if(depth==0 && i < typeString.len - 1){
            if(typeString.ptr[i] == ':' && typeString.ptr[i+1] == ':') {
                lastColonIndex = i;
                break;
            }
        }
    }
    if(lastColonIndex!=-1){
        outNamespace->ptr = typeString.ptr;
        outNamespace->len = lastColonIndex;
        typeString.ptr = typeString.ptr + lastColonIndex + 2;
        typeString.len = typeString.len - (lastColonIndex + 2);
    }

    *typeName = typeString;

    //-- Trim poly types
    if(typeString.len < 2 || typeString.ptr[typeString.len-1] != '>') {
        return typeString;
    }
    int leftArrow = -1;
    int rightArrow = typeString.len-1;
    // Right arrow must be at the end of typeString because we can't cut out
    // the polyTypes from typeString and then amend the two ends.
    // We would need to allocate a new string for typeString to reference
    // and we should avoid that if possible.
    for(int i=0;i<typeString.len;i++) {
        if(typeString.ptr[i] == '<'){
            leftArrow = i;
            break;
        }
    }
    if(leftArrow == -1){
        return typeString;
    }
    typeString.len = leftArrow;

    StringView acc = {};
    for(int i=leftArrow + 1; i < rightArrow; i++){
        if(typeString.ptr[i] != ',') {
            if(!acc.ptr)
                acc.ptr = typeString.ptr + i;
            acc.len++;
        }
        if(typeString.ptr[i] == ',' || i == rightArrow - 1){
            // We don't push acc if it's empty because we can then
            // check the lengh of outPolyTypes to verify if we
            // have valid types. If empty types are allowed then
            // we would need to go through the array to find
            // empty tokens.
            if(acc.ptr)
                outPolyTypes->add(acc);
            acc = {};
        }
    }
    
    return typeString;
}
void AST::DecomposePolyTypes(StringView typeString, StringView* out_base, QuickArray<StringView>* outPolyTypes) {
    //-- Trim poly types
    Assert(typeString.ptr[typeString.len-1] != '*');
    if(typeString.len < 2 || typeString.ptr[typeString.len-1] != '>') {
        *out_base = typeString;
        return;
    }
    int leftArrow = -1;
    int rightArrow = typeString.len-1;
    // Right arrow must be at the end of typeString because we can't cut out
    // the polyTypes from typeString and then amend the two ends.
    // We would need to allocate a new string for typeString to reference
    // and we should avoid that if possible.
    for(int i=0;i<typeString.len;i++) {
        if(typeString.ptr[i] == '<'){
            leftArrow = i;
            break;
        }
    }
    if(leftArrow == -1){
        *out_base = typeString;
        return;
    }
    typeString.len = leftArrow;

    StringView acc = {};
    int depth = 0;
    for(int i=leftArrow + 1; i < rightArrow; i++){
        char chr = typeString.ptr[i];
        if(depth!=0 || chr != ',') {
            if(!acc.ptr)
                acc.ptr = typeString.ptr + i;
            acc.len++;
        }
        if(chr=='<'){
            depth++;
        }
        if(chr=='>'){
            depth--;
        }
        if((depth==0 && chr == ',') || i == rightArrow - 1){
            // We don't push acc if it's empty because we can then
            // check the lengh of outPolyTypes to verify if we
            // have valid types. If empty types are allowed then
            // we would need to go through the array to find
            // empty tokens.
            if(acc.ptr)
                outPolyTypes->add(acc);
            acc = {};
        }
    }
    
    *out_base = typeString;
    return;

    // if(!typeString.ptr || typeString.len < 2 || typeString.ptr[typeString.len-1] != '>') { // <>
    //     return typeString;
    // }
    // // TODO: Line and column is ignored but it should also be separated properly.
    // // TODO: ":::" should cause an error but isn't considered at all here.
    // int leftArrow = -1;
    // int rightArrow = typeString.len-1;
    // for(int i=0;i<typeString.len;i++) {
    //     if(typeString.ptr[i] == '<'){
    //         leftArrow = i;
    //         break;
    //     }
    // }
    // // for(int i=typeString.len-1;i>=0;i--) {
    // //     if(typeString.ptr[i] == '>'){
    // //         rightArrow = i;
    // //         break;
    // //     }
    // // }
    // if(leftArrow == -1 || rightArrow == -1){
    //     return typeString;
    // }
    // Token out = typeString;
    // out.length = leftArrow;

    // Token acc = {};
    // for(int i=leftArrow + 1; i < rightArrow; i++){
    //     if(typeString.ptr[i] != ',') {
    //         if(!acc.str)
    //             acc.str = typeString.ptr + i;
    //         acc.length++;
    //     }
    //     if(typeString.ptr[i] == ',' || i == rightArrow - 1){
    //         // We don't push acc if it's empty because we can then
    //         // check the lengh of outPolyTypes to verify if we
    //         // have valid types. If empty types are allowed then
    //         // we would need to go through the array to find
    //         // empty tokens.
    //         if(!acc.str)
    //             outPolyTypes->push_back(acc);
    //         acc = {};
    //     }
    // }
    // return out;
}
u32 AST::getTypeSize(TypeId typeId){
    if(typeId.isPointer()) return 8; // TODO: Magic number! Is pointer always 8 bytes? Probably but who knows!
    if(!typeId.isNormalType()) return 0;
    auto ti = getTypeInfo(typeId);
    if(!ti)
        return 0;
    return ti->getSize();
}
u32 AST::getTypeAlignedSize(TypeId typeId) {
    if(typeId.isPointer()) return 8; // TODO: Magic number! Is pointer always 8 bytes? Probably but who knows!
    if(!typeId.isNormalType()) return 0;
    auto ti = getTypeInfo(typeId);
    if(!ti)
        return 0;
    // if(ti == typeInfos.end())
    //     return 0;
    // return ti->second->size();
    if (ti->astStruct) {
        if(ti->structImpl)
            return ti->structImpl->alignedSize;
        else
            return 0;
    }
    return ti->_size > 8 ? 8 : ti->_size;
}
void ASTExpression::printArgTypes(AST* ast, QuickArray<TypeId>& argTypes){
    using namespace engone;
    Assert(args.size() == argTypes.size());
    for(int i=0;i<(int)args.size();i++){
        if(i!=0) log::out << ", ";
        if(args.get(i)->namedValue.size()!=0)
            log::out << args.get(i)->namedValue <<"=";
        log::out << log::LIME << ast->typeToString(argTypes[i]) << log::NO_COLOR;
    }
    
    // Assert(args);
    // Assert(args->size() == argTypes.size());
    // for(int i=0;i<(int)args->size();i++){
    //     if(i!=0) log::out << ", ";
    //     if(args->get(i)->namedValue.str)
    //         log::out << args->get(i)->namedValue <<"=";
    //     log::out << ast->typeToString(argTypes[i]);
    // }
}
StructImpl* AST::createStructImpl(){
    auto ptr = (StructImpl*)allocate(sizeof(StructImpl));
    new(ptr)StructImpl();
    return ptr;
}
void FuncImpl::print(AST* ast, ASTFunction* astFunc){
    using namespace engone;
    log::out << astFunction->name <<"(";
    for(int i=0;i<(int)argumentTypes.size();i++){
        if(i!=0) log::out << ", ";
        if(astFunc)
            log::out << astFunc->arguments[i].name <<": ";
        log::out << ast->typeToString(argumentTypes[i].typeId);
    }
    log::out << ")";
}
FuncImpl* AST::createFuncImpl(ASTFunction* astFunc){
    FuncImpl* ptr = (FuncImpl*)allocate(sizeof(FuncImpl));
    new(ptr)FuncImpl();
    ptr->astFunction = astFunc;
    bool nonAllocationFailure = astFunc->_impls.add(ptr);
    Assert(nonAllocationFailure);
    return ptr;
}
void ASTStruct::add(ASTFunction* func){
    // NOTE: Is this code necessary? Did something break.
    // auto pair = baseImpl.methods.find(func->name);
    // Assert(pair == baseImpl.methods.end());
    // baseImpl.methods[func->name] = {func, &func->baseImpl};
    
    functions.add(func);
}

// void StructImpl::addPolyMethod(const std::string& name, ASTFunction* func, FuncImpl* funcImpl){
//     // auto pair = methods.find(name);
//     // Assert(pair == methods.end());

//     // methods[name] = {func, funcImpl};
// }
std::string ScopeInfo::getFullNamespace(AST* ast){
    std::string ns = "";
    ScopeInfo* scope = ast->getScope(id);
    while(scope){
        if(ns.empty() && scope->name.size()!=0)
            ns = scope->name;
        else if(scope->name.size()!=0)
            ns = std::string(scope->name) + "::"+ns;
        if(scope->id == scope->parent)
            return ns;
        scope = ast->getScope(scope->parent);
    }
    return ns;
}

TypeInfo::MemberData TypeInfo::getMember(const std::string &name) {
    if (astStruct) {
        for (int i = 0; i < (int)astStruct->members.size(); i++) {
            if (astStruct->members[i].name == name) {
                return getMember(i);
            }
        }
    }
    return {{}, -1};
}
TypeInfo::MemberData TypeInfo::getMember(int index) {
    if (astStruct) {
        StructImpl* impl = structImpl;
        return {impl->members[index].typeId, index, impl->members[index].offset};
    } else {
        return {{}, -1};
    }
}
bool ASTEnum::getMember(const StringView& name, int *out) {
    int index = -1;
    for (int i = 0; i < (int)members.size(); i++) {
        if (members[i].name == name) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        return false;
    }
    if (out) {
        *out = members[index].enumValue;
    }
    return true;
}

bool AST::IsInteger(TypeId id) {
    if(!id.isNormalType()) return false;
    return AST_UINT8 <= id.getId() && id.getId() <= AST_INT64;
}
bool AST::IsSigned(TypeId id) {
    if(!id.isNormalType()) return false;
    return AST_INT8 <= id.getId() && id.getId() <= AST_INT64; // AST_CHAR is not signed
}
bool AST::IsDecimal(TypeId id){
    // TODO: Add float64
    return id == AST_FLOAT32 || id == AST_FLOAT64;
}
/* #region  */
void PrintSpace(int count) {
    using namespace engone;
    for (int i = 0; i < count; i++)
        log::out << "  ";
}

void AST::print(int depth) {
    using namespace engone;
    if (globalScope) {
        PrintSpace(depth);
        log::out << "AST\n";
        // globalScope->print(this, depth + 1);
        auto s = getScope(globalScope->scopeId);
        for(auto n : s->sharedScopes) {
            n->astScope->print(this, depth+1);
        }
    }

}
void ASTScope::print(AST *ast, int depth) {
    using namespace engone;
    if(isHidden()) return;

    PrintSpace(depth);
    if(!isNamespace)
        log::out << "Body ";
    else {
        log::out << "Namespace ";
        if(name)
            log::out << " "<<*name;
    }
    log::out << "(scope: "<<scopeId<<", order: "<<ast->getScope(scopeId)->contentOrder<<")";
    log::out<<"\n";
    // #define MUL_PRINT(X,...) for(auto it : X) it->print(ast,depth+1); MUL_PRINT(...)
    // MUL_PRINT(structs, enums, functions, statements, namespaces);
    
    for(auto& it : content) {
        switch(it.spotType) {
        case STRUCT: {
            structs[it.index]->print(ast, depth+1);
        }
        break; case ENUM: {
            enums[it.index]->print(ast, depth+1);
        }
        break; case FUNCTION: {
            functions[it.index]->print(ast, depth+1);
        }
        break; case STATEMENT: {
            statements[it.index]->print(ast, depth+1);
        }
        break; case NAMESPACE: {
            namespaces[it.index]->print(ast, depth+1);
        }
        }
    }
    // #define PR(X) for(auto it : X) it->print(ast,depth+1);
    // PR(structs)
    // PR(enums)
    // PR(functions)
    // PR(statements)
    // PR(namespaces)
    // #undef PR
}
void ASTFunction::print(AST *ast, int depth) {
    using namespace engone;
    // if(!hidden && (!body || !body->nativeCode)) {
    if(!isHidden()) {
        PrintSpace(depth);
        log::out << "Func " << name;
        if(polyArgs.size()!=0){
            log::out << "<";
            for(int i=0;i<(int)polyArgs.size();i++){
                if(i!=0)
                    log::out << ",";
                log::out << polyArgs[i].name;
            }
            log::out << ">";
        }
        log::out << " (scope: "<<scopeId<<", order: "<<ast->getScope(scopeId)->contentOrder<<") ";
        log::out << "\n";
        for (int i = 0; i < (int)arguments.size(); i++) {
            auto &arg = arguments[i];
            PrintSpace(depth+1);
            // auto &argImpl = baseImpl.arguments[i];
            // if (i != 0) {
            //     log::out << ", ";
            // }
            log::out << arg.name << ": ";
            log::out << ast->typeToString(arg.stringType);
            log::out << "\n";
        }
        // log::out << ")";
        if (returnValues.size()!=0){
            PrintSpace(depth+1);
            log::out << "-> ";
            for (int i=0;i<(int)returnValues.size();i++){
                auto& retType = returnValues[i].stringType;
                if(i!=0)
                    log::out<<", ";
                // auto dtname = ast->getTypeInfo(ret.typeId)->getFullType(ast);
                // log::out << dtname << ", ";
                log::out << ast->typeToString(retType);
            }
            log::out << "\n";
        }
        if(linkConvention != LinkConventions::NONE){
            PrintSpace(depth+1);
            log::out << "Native/External\n";
        }else{
            if(body){
                body->print(ast, depth + 1);
            }
        }
    }
}
void ASTStruct::print(AST *ast, int depth) {
    using namespace engone;
    if(!isHidden()){
        PrintSpace(depth);
        log::out << "Struct " << name;
        if (polyArgs.size() != 0) {
            log::out << "<";
        }
        for (int i = 0; i < (int)polyArgs.size(); i++) {
            if (i != 0) {
                log::out << ", ";
            }
            log::out << polyArgs[i].name;
        }
        if (polyArgs.size() != 0) {
            log::out << ">";
        }
        log::out << " (scope: "<<scopeId<<")";
        log::out << "\n";
        // log::out << " { ";
        for (int i = 0; i < (int)members.size(); i++) {
            auto &member = members[i];
            // auto &implMem = baseImpl.members[i];
            PrintSpace(depth + 1);
            auto str = ast->typeToString(member.stringType);
            log::out << member.name << ": " << str;
            if (member.defaultValue) {
                log::out << " = ";
                // member.defaultValue->tokenRange.print();
            }
            log::out << "\n";
            // if (i + 1 != (int)members.size()) {
            //     log::out << ", ";
            // }
        }
        // log::out << " }\n";
        for(auto f : functions){
            // log::out << "\n";
            f->print(ast,depth+1);
            // PrintSpace(depth);
        }
    }
}
void ASTEnum::print(AST *ast, int depth) {
    using namespace engone;
    if(!isHidden()){
        PrintSpace(depth);
        log::out << "Enum " << name << " { ";
        for (int i = 0; i < (int)members.size(); i++) {
            auto &member = members[i];
            log::out << member.name << "=" << member.enumValue;
            if (i + 1 != (int)members.size()) {
                log::out << ", ";
            }
        }
        log::out << "}\n";
    }
}
void ASTStatement::print(AST *ast, int depth) {
    using namespace engone;
    PrintSpace(depth);
    log::out << "Statement " << STATEMENT_NAME(type);

    for(int i=0;i<(int)varnames.size();i++){
        auto& vn = varnames[i];
        if(i!=0)
            log::out << ",";
        log::out << " " << vn.name;
        if(vn.assignString.isValid())
            log:: out << ": "<< ast->typeToString(vn.assignString);
    }
    if(alias)
        log::out << " as " << *alias;
    // if(opType!=0)
    //     log::out << " "<<OP_NAME((OperationType)opType)<<"=";
    log::out << "\n";
    // if (lvalue) {
    //     lvalue->print(ast, depth + 1);
    // }
    
    // if(hasNodes()){
        if (firstExpression) {
            firstExpression->print(ast, depth + 1);
        }
        if (firstBody) {
            if(type == ASTStatement::SWITCH) {
                PrintSpace(depth+1);
                log::out << "Default case:\n";
            }
            firstBody->print(ast, depth + 1);
        }
        if (secondBody) {
            secondBody->print(ast, depth + 1);
        }
    // } else {
        for(ASTExpression* expr : arrayValues){
            if (expr) {
                expr->print(ast, depth+1);
            }
        }
        for(auto& it : switchCases){
            if (it.caseExpr) {
                it.caseExpr->print(ast, depth+1);
            }
            if (it.caseBody) {
                it.caseBody->print(ast, depth+1);
            }
        }
    // }
}
void ASTExpression::print(AST *ast, int depth) {
    using namespace engone;
    PrintSpace(depth);

    log::out << "Expr ";
    if(namedValue.size()!=0){
        log::out << namedValue<<"= ";
    }
    if (isValue) {
        log::out << ast->typeToString(typeId);
        log::out << " ";
        log::out.flush();
        if (typeId == AST_FLOAT32)
            log::out << f32Value;
        else if (typeId == AST_FLOAT64)
            log::out << f64Value;
        else if (AST::IsInteger(typeId))
            log::out << i64Value;
        else if (typeId == AST_BOOL)
            log::out << boolValue;
        else if (typeId == AST_CHAR)
            log::out << charValue;
        else if (typeId == AST_ID)
            log::out << name;
        // else if(typeId==AST_STRING) log::out << ast->constStrings[constStrIndex];
        else if (typeId == AST_FNCALL)
            log::out << name;
        else if (typeId == AST_NULL)
            log::out << "null";
        else if(typeId == AST_STRING)
            log::out <<name;
        else if(typeId == AST_SIZEOF)
            log::out << name;
        else if(typeId == AST_NAMEOF)
            log::out << name;
        else if(typeId == AST_TYPEID)
            log::out << name;
        else if(typeId == AST_ASM)
            log::out << "?";
        else
            log::out << "missing print impl.";
        if (typeId == AST_FNCALL) {
            // if (args && args->size()!=0) {
            if (args.size()!=0) {
                log::out << " args:\n";
                // for(auto arg : *args){
                //     arg->print(ast, depth + 1);
                // }
            } else {
                log::out << " no args\n";
            }
        } else
            log::out << "\n";
    } else {
        if(typeId == AST_ASSIGN) {
            if(castType.getId()!=0){
                log::out << OP_NAME((OperationType)castType.getId());
            }
        }
        log::out << OP_NAME((OperationType)typeId.getId()) << " ";
        if (typeId == AST_CAST) {
            log::out << ast->typeToString(castType);
            log::out << "\n";
            left->print(ast, depth + 1);
        } else if (typeId == AST_MEMBER) {
            log::out << name;
            log::out << "\n";
            left->print(ast, depth + 1);
        } else if (typeId == AST_INITIALIZER) {
            log::out << ast->typeToString(castType);
            log::out << "\n";
            if(left)
                left->print(ast, depth + 1);
        } else if (typeId == AST_FROM_NAMESPACE) {
            log::out << name;
            log::out << "\n";
            if(left)
                left->print(ast, depth + 1);
        } else if (typeId == AST_INDEX) {
            log::out << "\n";
            if(left)
                left->print(ast, depth + 1);
            if(right)
                right->print(ast, depth + 1);
        } else if (typeId == AST_INCREMENT) {
            log::out << "\n";
            if(left)
                left->print(ast, depth + 1);
        } else if (typeId == AST_DECREMENT) {
            log::out << "\n";
            if(left)
                left->print(ast, depth + 1);
        } else if(typeId == AST_ASSIGN) {
            // if(castType.getId()!=0){
            //     log::out << OP_NAME((OperationType)castType.getId());
            // }
            log::out << "\n";
            if (left) {
                left->print(ast, depth + 1);
            }
            if (right) {
                right->print(ast, depth + 1);
            }
        }  else {
            log::out << "\n";
            if (left) {
                left->print(ast, depth + 1);
            }
            if (right) {
                right->print(ast, depth + 1);
            }
        }
    }
    if(typeId == AST_FNCALL || typeId == AST_INITIALIZER){
        // for(ASTExpression* expr : *args){
        for(ASTExpression* expr : args){
            if (expr) {
                expr->print(ast, depth+1);
            }
        }
    }
}
/* #endregion */