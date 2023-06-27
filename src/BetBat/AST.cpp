#include "BetBat/AST.h"

const char *OpToStr(OperationType optype) {
#define CASE(A, B) \
    case AST_##A:  \
        return #B;
    switch (optype) {
        CASE(ADD, +)
        CASE(SUB, -)
        CASE(MUL, * (multiplication))
        CASE(DIV, /)
        CASE(MODULUS, %)

        CASE(EQUAL, ==)
        CASE(NOT_EQUAL, !=)
        CASE(LESS, <)
        CASE(LESS_EQUAL, <=)
        CASE(GREATER, >)
        CASE(GREATER_EQUAL, >=)
        CASE(AND, &&)
        CASE(OR, ||)
        CASE(NOT, !)

        CASE(BAND, & (bitwise and))
        CASE(BOR, | (bitwise or))
        CASE(BXOR, ^ (bitwise xor))
        CASE(BNOT, ~ (bitwise not))
        CASE(BLSHIFT, << (bit shift))
        CASE(BRSHIFT, >> (bit shift))

        CASE(CAST, cast)
        CASE(MEMBER, member)
        CASE(INITIALIZER, initializer)
        CASE(SLICE_INITIALIZER, slice_initializer)
        CASE(FROM_NAMESPACE, namespaced)
        CASE(ASSIGN, =)

        CASE(REFER, & (reference))
        CASE(DEREF, * (dereference))
        default: return "?";
    }
#undef CASE
    return "?";
}
const char *StateToStr(int type) {
#define CASE(A, B)        \
    case ASTStatement::A: \
        return #B;
    switch (type) {
        CASE(ASSIGN, assign)
        CASE(IF, if)
        CASE(WHILE, while)
        CASE(RETURN, return)
        CASE(BREAK, break)
        CASE(CONTINUE, continue)
        CASE(EXPRESSION, expression)
        CASE(USING, using)
        CASE(BODY, body)
        CASE(DEFER, defer)
    }
#undef CASE
    return "?";
}

AST *AST::Create() {
    using namespace engone;
// Useful when debugging memory leaks
#ifdef ALLOC_LOG
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
        ADD_TRACKING(std::string)
        // #define LOG_SIZE(X) << #X " "<<sizeof(X)<<"\n"
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

    AST *ast = (AST *)engone::Allocate(sizeof(AST));
    new(ast) AST();

    ScopeId scopeId = ast->createScope()->id;
    ast->globalScopeId = scopeId;
    // initialize default data types
    ast->createPredefinedType(Token("void"),scopeId, AST_VOID);
    ast->createPredefinedType(Token("u8"),scopeId, AST_UINT8, 1);
    ast->createPredefinedType(Token("u16"),scopeId, AST_UINT16, 2);
    ast->createPredefinedType(Token("u32"),scopeId, AST_UINT32, 4);
    ast->createPredefinedType(Token("u64"),scopeId, AST_UINT64, 8);
    ast->createPredefinedType(Token("i8"),scopeId, AST_INT8, 1);
    ast->createPredefinedType(Token("i16"),scopeId, AST_INT16, 2);
    ast->createPredefinedType(Token("i32"),scopeId, AST_INT32, 4);
    ast->createPredefinedType(Token("i64"),scopeId, AST_INT64, 8);
    ast->createPredefinedType(Token("f32"),scopeId, AST_FLOAT32, 4);
    ast->createPredefinedType(Token("bool"),scopeId, AST_BOOL, 1);
    ast->createPredefinedType(Token("char"),scopeId, AST_CHAR, 1);
    ast->createPredefinedType(Token("null"),scopeId, AST_NULL, 8);
    ast->createPredefinedType(Token("ast_string"),scopeId, AST_STRING, 0);
    ast->createPredefinedType(Token("var"),scopeId, AST_VAR);
    ast->createPredefinedType(Token("member"),scopeId, AST_MEMBER);
    ast->createPredefinedType(Token("sizeof"),scopeId, AST_SIZEOF);
    ast->createPredefinedType(Token("call"),scopeId, AST_FNCALL);

    ast->mainBody = ast->createBody();
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
    //     ast->mainBody->add(astStruct);
    // }
    return ast;
}

VariableInfo *AST::addVariable(ScopeId scopeId, const std::string &name) {
    using namespace engone;
    
    auto id = addIdentifier(scopeId, name);
    if(!id)
        return nullptr;
    
    id->type = Identifier::VAR;
    id->varIndex = variables.size();
    variables.push_back({});
    return &variables[id->varIndex];
}

Identifier* AST::addFunction(ScopeId scopeId, const std::string& name, ASTFunction* func) {
    using namespace engone;
    auto id = addIdentifier(scopeId, name);
    if(!id)
        return nullptr;

    id->type = Identifier::FUNC;
    id->astFunc = func;
    return id;
}
Identifier *AST::addIdentifier(ScopeId scopeId, const std::string &name) {
    using namespace engone;
    ScopeInfo* si = getScope(scopeId);
    if(!si)
        return nullptr;

    auto pair = si->identifierMap.find(name);
    if (pair != si->identifierMap.end())
        return nullptr;

    auto ptr = &(si->identifierMap[name] = {});
    ptr->name = name;
    ptr->scopeId = scopeId;
    return ptr;
}
Identifier *AST::getIdentifier(ScopeId scopeId, const std::string &name) {
    using namespace engone;
    Token ns={};
    Token realName = TrimNamespace(Token(name), &ns);
    ScopeId nextScopeId = scopeId;
    while(true) {
        if(ns.str) {
            ScopeInfo* nscope = getScope(ns, nextScopeId);
            if(nscope) {
                auto pair = nscope->identifierMap.find(realName);
                if(pair != nscope->identifierMap.end()){
                    return &pair->second;
                }
            }
        }
        ScopeInfo* si = getScope(nextScopeId);
        if(!si){
            log::out << log::RED <<__FUNCTION__<<" Scope was null (compiler bug)\n";
            break;
        }
        if(!ns.str) { auto pair = si->identifierMap.find(realName);
        if(pair != si->identifierMap.end()){
            return &pair->second;
        } }
        if(nextScopeId == 0 && si->parent == 0){
            // quit when we checked global
            break;
        }
        nextScopeId = si->parent;
    }
    return nullptr;
}
VariableInfo *AST::getVariable(Identifier* id) {
    // TODO: bound check
    return &variables[id->varIndex];
}
ASTFunction *AST::getFunction(Identifier* id) {
    return id->astFunc;
    // TODO: bound check
    // return &functions[index];
}

StructImpl::Method StructImpl::getMethod(const std::string& name){
    auto pair = methods.find(name);
    if(pair == methods.end())
        return {};
    return pair->second;
    // ASTFunction* next = functions;
    // while(next){
    //     ASTFunction* now = next;
    //     next = next->next;
    //     if(now->name == name){
    //         return now;
    //     }
    // }
    // return nullptr;
}

void AST::removeIdentifier(ScopeId scopeId, const std::string &name) {
    auto si = getScope(scopeId);
    auto pair = si->identifierMap.find(name);
    if (pair != si->identifierMap.end()) {
        // TODO: The variable identifier points to cannot be erased since
        //   the variables array will invalidate other identifier's index.
        //   This means that the array will keep growing. Fix this.
        si->identifierMap.erase(pair);
    }
}
// std::string TypeInfo::getFullType(AST* ast){
//     ScopeInfo* scope = ast->getScope(scopeId);
//     std::string ns = scope->getFullNamespace(ast);
//     if(ns.empty()){
//         return name;
//     }else {
//         return ns + "::" + name;   
//     }
// }
void AST::appendToMainBody(ASTScope *body) {
    Assert(body);
    if (body->enums) {
        mainBody->add(body->enums, body->enumsTail);
        body->enums = 0;
        body->enumsTail = 0;
    }
    if (body->functions) {
        mainBody->add(body->functions, body->functionsTail);
        body->functions = 0;
        body->functionsTail = 0;
    }
    if (body->structs) {
        mainBody->add(body->structs, body->structsTail);
        body->structs = 0;
        body->structsTail = 0;
    }
    if (body->statements) {
        mainBody->add(body->statements, body->statementsTail);
        body->statements = 0;
        body->statementsTail = 0;
    }
    if (body->namespaces) {
        mainBody->add(body->namespaces, this, body->namespacesTail);
        body->namespaces = 0;
        body->namespacesTail = 0;
    }
    destroy(body);
}

ASTScope *AST::createBody() {
    auto ptr = (ASTScope *)engone::Allocate(sizeof(ASTScope));
    new(ptr) ASTScope();
    ptr->type = ASTScope::BODY;
    return ptr;
}
ASTStatement *AST::createStatement(int type) {
    auto ptr = (ASTStatement *)engone::Allocate(sizeof(ASTStatement));
    new(ptr) ASTStatement();
    ptr->type = type;
    return ptr;
}
ASTStruct *AST::createStruct(const std::string &name) {
    auto ptr = (ASTStruct *)engone::Allocate(sizeof(ASTStruct));
    new(ptr) ASTStruct();
    ptr->name = name;
    return ptr;
}
ASTEnum *AST::createEnum(const std::string &name) {
    auto ptr = (ASTEnum *)engone::Allocate(sizeof(ASTEnum));
    new(ptr) ASTEnum();
    ptr->name = name;
    return ptr;
}
ASTFunction *AST::createFunction(const std::string &name) {
    auto ptr = (ASTFunction *)engone::Allocate(sizeof(ASTFunction));
    new(ptr) ASTFunction();
    ptr->name = name;
    return ptr;
}
ASTExpression *AST::createExpression(TypeId type) {
    auto ptr = (ASTExpression *)engone::Allocate(sizeof(ASTExpression));
    new(ptr) ASTExpression();
    ptr->isValue = (u32)type.getId() < AST_PRIMITIVE_COUNT;
    ptr->typeId = type;
    return ptr;
}
ASTScope *AST::createNamespace(const std::string& name) {
    auto ptr = (ASTScope *)engone::Allocate(sizeof(ASTScope));
    new(ptr) ASTScope();
    ptr->type = ASTScope::NAMESPACE;
    ptr->name = (std::string*)engone::Allocate(sizeof(std::string));
    new(ptr->name)std::string(name);
    return ptr;
}
#define TAIL_ADD(M, S)     \
    if (!M)                \
        M##Tail = M = S;   \
    else                   \
        M##Tail->next = S; \
    if (tail) M##Tail = tail; else while (M##Tail->next)  \
        M##Tail = M##Tail->next;
void ASTScope::add(ASTStatement *astStatement, ASTStatement* tail) {
    TAIL_ADD(statements, astStatement)
}
void ASTScope::add(ASTStruct *astStruct, ASTStruct* tail) {
    TAIL_ADD(structs, astStruct)
}
void ASTScope::add(ASTFunction *astFunction, ASTFunction* tail) {
    TAIL_ADD(functions, astFunction)
}
void ASTScope::add(ASTEnum *astEnum, ASTEnum* tail) {
    TAIL_ADD(enums, astEnum)
}
void ASTScope::add(ASTScope* astNamespace, AST* ast, ASTScope* tail){
    if(!namespaces){
        namespacesTail = namespaces = astNamespace;
        if(tail)
            namespacesTail = tail;
        else
            while (namespacesTail->next)
                namespacesTail = namespacesTail->next;
        return;
    }
    // NOTE: This code can be confusing and mistakes
    //  are easy to make. Keep a clear mind when
    //  working here and don't change things
    //  in the spur of the moment!
    // NOTE: Change things in ASTScope::add WHEN DOING CHANGES HERE!
    ASTScope* nextInsert = astNamespace;
    while(nextInsert){
        ASTScope* now = nextInsert;
        nextInsert = nextInsert->next;

        bool appended=false;
        ASTScope* nextNS = namespaces;
        while(nextNS){
            ASTScope* ns = nextNS;
            nextNS = nextNS->next;

            if(*ns->name == *astNamespace->name){
                appended=true;
                if(now->enums)
                    ns->add(now->enums);
                if(now->functions)
                    ns->add(now->functions);
                if(now->structs)
                    ns->add(now->structs);
                if(now->namespaces)
                    ns->add(now->namespaces, ast);
                now->enums=0;
                now->functions=0;
                now->structs=0;
                now->namespaces=0;
                now->next = 0;
                ast->destroy(now);
                break;
            }
        }
        if(!appended){
            namespacesTail->next = now;
            now->next = 0;
            namespacesTail = namespacesTail->next;
        }
    }
}
void AST::Destroy(AST *ast) {
    if (!ast)
        return;
    ast->cleanup();
    ast->~AST();
    engone::Free(ast, sizeof(AST));
}
void AST::cleanup() {
    using namespace engone;

    for (auto &scope : _scopeInfos) {
        scope->nameTypeMap.clear();
        scope->~ScopeInfo();
        engone::Free(scope, sizeof(ScopeInfo));
    }
    _scopeInfos.clear();

    for(auto& pair : _typeInfos){
        // typeInfos is resized and filled with nullptrs
        // when types are created
        if(!pair) continue;

        if(pair->structImpl)
            engone::Free(pair->structImpl, sizeof(StructImpl));
        pair->~TypeInfo();
        engone::Free(pair, sizeof(TypeInfo));
    }
    _typeInfos.clear();
    constStrings.clear();

    for(auto& str : tempStrings){
        str->~basic_string<char>();
        engone::Free(str, sizeof(std::string));
    }
    tempStrings.clear();

    nextTypeId = AST_OPERATION_COUNT;

    destroy(mainBody);
    mainBody = nullptr;
}
ScopeInfo* AST::createScope() {
    auto ptr = (ScopeInfo *)engone::Allocate(sizeof(ScopeInfo));
    new(ptr) ScopeInfo{(u32)_scopeInfos.size()};
    _scopeInfos.push_back(ptr);
    return ptr;
}
ScopeInfo* AST::getScope(ScopeId id){
    if(_scopeInfos.size() <= id)
        return nullptr; // out of bounds
    return _scopeInfos[id];
}
ScopeInfo* AST::getScope(Token name, ScopeId scopeId){
    using namespace engone;
    Token nextName = name;
    ScopeId nextScopeId = scopeId;
    int safetyLimit = 100;
    while(true) {
        if(!safetyLimit--) {
            log::out << log::RED << __func__ << ": while safety limit broken\n";
            break;
        }
        ScopeInfo* scope = getScope(nextScopeId);
        if(!scope) return nullptr;
        int splitIndex = -1;
        for(int i=0;i<nextName.length-1;i++){
            if(nextName.str[i] == ':' && nextName.str[i+1] == ':'){
                splitIndex = i;
                break;
            }
        }
        if(splitIndex == -1){
            auto pair = scope->nameScopeMap.find(nextName);
            if(pair != scope->nameScopeMap.end()){
                return getScope(pair->second);
            }
            for(int i=0;i<(int)scope->usingScopes.size();i++){
                ScopeInfo* usingScope = scope->usingScopes[i];
                auto pair = usingScope->nameScopeMap.find(nextName);
                if(pair != usingScope->nameScopeMap.end()){
                    return getScope(pair->second);
                }
            }
            return nullptr;
        } else {
            Token first = {};
            first.str = nextName.str;
            first.length = splitIndex;
            Token rest = {};
            rest.str = nextName.str += splitIndex+2;
            rest.length = nextName.length - (splitIndex + 2);
            auto pair = scope->nameScopeMap.find(first);
            if(pair != scope->nameScopeMap.end()){
                nextName = rest;
                nextScopeId = pair->second;
                continue;
            }
            bool cont = false;
            for(int i=0;i<(int)scope->usingScopes.size();i++){
                ScopeInfo* usingScope = scope->usingScopes[i];
                auto pair = usingScope->nameScopeMap.find(first);
                if(pair != usingScope->nameScopeMap.end()){
                    nextName = rest;
                    nextScopeId = pair->second;
                    cont = true;
                    break;
                }
            }
            if(cont) continue;
            return nullptr;
        }
    }
    return nullptr;
}
ScopeInfo* AST::getScopeFromParents(Token name, ScopeId scopeId){
    using namespace engone;

    Token nextName = name;
    ScopeId nextScopeId = scopeId;
    int safetyLimit = 100;
    while(true) {
        if(!safetyLimit--) {
            log::out << log::RED << __func__ << ": while safety limit broken\n";
            break;
        }
        ScopeInfo* scope = getScope(nextScopeId);
        if(!scope) return nullptr;
        int splitIndex = -1;
        for(int i=0;i<nextName.length-1;i++){
            if(nextName.str[i] == ':' && nextName.str[i+1] == ':'){
                splitIndex = i;
                break;
            }
        }
        if(splitIndex == -1){
            auto pair = scope->nameScopeMap.find(nextName);
            if(pair != scope->nameScopeMap.end()){
                return getScope(pair->second);
            }
            for(int i=0;i<(int)scope->usingScopes.size();i++){
                ScopeInfo* usingScope = scope->usingScopes[i];
                auto pair = usingScope->nameScopeMap.find(nextName);
                if(pair != usingScope->nameScopeMap.end()){
                    return getScope(pair->second);
                }
            }
            if(nextScopeId != scope->parent) { // Check infinite loop. Mainly for global scope.
                nextScopeId = scope->parent;
                continue;
            }
            return nullptr;
        } else {
            Token first = {};
            first.str = nextName.str;
            first.length = splitIndex;
            Token rest = {};
            rest.str = nextName.str += splitIndex+2;
            rest.length = nextName.length - (splitIndex + 2);
            auto pair = scope->nameScopeMap.find(first);
            if(pair != scope->nameScopeMap.end()){
                nextName = rest;
                nextScopeId = pair->second;
                continue;
            }
            bool cont = false;
            for(int i=0;i<(int)scope->usingScopes.size();i++){
                ScopeInfo* usingScope = scope->usingScopes[i];
                auto pair = usingScope->nameScopeMap.find(nextName);
                if(pair != usingScope->nameScopeMap.end()){
                    nextName = rest;
                    nextScopeId = pair->second;
                    break;
                }
            }
            if(cont) continue;
            if(nextScopeId != scope->parent) {
                nextScopeId = scope->parent;
                continue;
            }
            return nullptr;
        }
    }
    return nullptr;
}

TypeId AST::getTypeString(Token name){
    for(int i=0;i<(int)_typeTokens.size();i++){
        if(name == _typeTokens[i])
            return TypeId::CreateString(i);
    }
    _typeTokens.push_back(name);
    return TypeId::CreateString(_typeTokens.size()-1);
}
Token AST::getTokenFromTypeString(TypeId typeId){
    static const Token non={};
    if(!typeId.isString())
        return non;

    u32 index = typeId.getId();
    if(index >= _typeTokens.size())
        return non;
    
    return _typeTokens[index];
}
TypeId AST::convertToTypeId(TypeId typeString, ScopeId scopeId){
    Assert(typeString.isString())
    Token tstring = getTokenFromTypeString(typeString);
    return convertToTypeId(tstring, scopeId);
}

TypeInfo* AST::createType(Token name, ScopeId scopeId){
    using namespace engone;
    auto scope = getScope(scopeId);
    if(!scope) return nullptr;
    
    auto nearPair = scope->nameTypeMap.find(name);
    if(nearPair != scope->nameTypeMap.end()) {
        return nullptr; // type already exists
    }
    // You could check if name already exists in parent
    // scopes to but I think it is fine for now.

    auto ptr = (TypeInfo *)engone::Allocate(sizeof(TypeInfo));
    Assert(ptr);
    new(ptr) TypeInfo{name, TypeId::Create(nextTypeId++)};
    ptr->scopeId = scopeId;
    scope->nameTypeMap[name] = ptr;
    if(ptr->id.getId() >= _typeInfos.size()) {
        _typeInfos.resize(ptr->id.getId() + 20);
    }
    _typeInfos[ptr->id.getId()] = ptr;
    return ptr;
}
TypeInfo* AST::createPredefinedType(Token name, ScopeId scopeId, TypeId id, u32 size) {
    auto ptr = (TypeInfo *)engone::Allocate(sizeof(TypeInfo));
    new(ptr) TypeInfo{name, id, size};
    ptr->scopeId = scopeId;
    auto scope = getScope(scopeId);
    if(!scope) return nullptr;
    
    if(ptr->id.getId() >= _typeInfos.size()) {
        _typeInfos.resize(ptr->id.getId() + 20);
    }
    _typeInfos[ptr->id.getId()] = ptr;
    scope->nameTypeMap[ptr->name] = ptr;
    return ptr;
}
TypeId AST::convertToTypeId(Token typeString, ScopeId scopeId) {
    using namespace engone;
    Token namespacing = {};
    u32 pointerLevel = 0;
    std::vector<Token> polyTypes;
    Token typeName = {}; // base name + poly names

    typeName = TrimPointer(typeString, &pointerLevel);
    typeName = TrimNamespace(typeName, &namespacing);
    
    // Token baseType = TrimBaseType(typeString, &namespacing, &pointerLevel, &polyTypes, &typeName);
    // if(polyTypes.size()!=0)
    //     log::out << log::RED << "Polymorphic types are not handled ("<<typeString<<")\n";
    // TODO: Polymorphism!s

    ScopeInfo* scope = nullptr;
    TypeInfo* typeInfo = nullptr;
    if(namespacing.str){
        scope = getScopeFromParents(namespacing, scopeId);
        if(!scope) return {};
        auto firstPair = scope->nameTypeMap.find(typeName);
        if(firstPair != scope->nameTypeMap.end()){
            typeInfo = firstPair->second;
        }
    } else {
        // Find base type in parent scopes
        ScopeId nextScopeId = scopeId;
        while(true){
            ScopeInfo* scope = getScope(nextScopeId);
            if(!scope) return {};

            auto pair = scope->nameTypeMap.find(typeName);
            if(pair!=scope->nameTypeMap.end()){
                typeInfo = pair->second;
                break;
            }
            bool brea=false;
            for(int i=0;i<(int)scope->usingScopes.size();i++){
                ScopeInfo* usingScope = scope->usingScopes[i];
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
    while(true){
        TypeInfo* virtualInfo = getTypeInfo(typeInfo->id);
        if(virtualInfo && typeInfo != virtualInfo){
            typeInfo = virtualInfo;
        } else {
            break;
        }
    }

    TypeId out = typeInfo->id;
    out.setPointerLevel(pointerLevel);
        
    return out;
}
TypeInfo *AST::getBaseTypeInfo(TypeId id) {
    Assert(!id.isString());
    if(!id.isValid() && !id.isString()) return nullptr;
    if(id.getId() >= _typeInfos.size()) return nullptr;
    return _typeInfos[id.getId()];
}
TypeInfo *AST::getTypeInfo(TypeId id) {
    if(!id.isNormalType()) {
        return nullptr;
    }
    Assert(id.isNormalType());
    if(id.getId() >= _typeInfos.size()) return nullptr;
    return _typeInfos[id.getId()];
}
std::string AST::typeToString(TypeId typeId){
    using namespace engone;
    if(typeId.isString())
        return std::string((Token)getTokenFromTypeString(typeId));
    const char* cstr = OpToStr((OperationType)typeId.getId());
    if(strcmp(cstr,"?") != 0) {
        return cstr;
    }
    std::string out="";
    TypeInfo* ti = getBaseTypeInfo(typeId);
    if(!ti)
        return Token("");

    ScopeInfo* scope = getScope(ti->scopeId);
    std::string ns = scope->getFullNamespace(this);
    if(ns.empty()){
        out = ti->name;
    } else {
        out = ns + "::" + ti->name;
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
    // log::out << log::YELLOW << __func__ << ": Warning! polymorphism is ignored!\n";
    for(int i=0;i<(int)typeId.getPointerLevel();i++){
        out+="*";
    }
    return out;
}
TypeId AST::ensureNonVirtualId(TypeId id){
    if(!id.isValid() || id.isString()) return id;
    u32 level = id.getPointerLevel();
    id = id.baseType();
    while(true){
        TypeInfo* ti = getTypeInfo(id);
        if(!ti || ti->id == id)
            break;
        id = ti->id;
    }
    id.setPointerLevel(level);
    return id;
}
std::string* AST::createString(){
    std::string* ptr = (std::string*)engone::Allocate(sizeof(std::string));
    new(ptr)std::string();
    tempStrings.push_back(ptr);
    return ptr;
}
void AST::destroy(ASTScope *scope) {
     if (scope->next)
        destroy(scope->next);
    if (scope->name) {
        scope->name->~basic_string<char>();
        engone::Free(scope->name, sizeof(std::string));
        scope->name = nullptr;
    }
    if (scope->structs)
        destroy(scope->structs);
    if (scope->enums)
        destroy(scope->enums);
    if (scope->functions)
        destroy(scope->functions);
    if (scope->statements)
        destroy(scope->statements);
    if(scope->namespaces)
        destroy(scope->namespaces);

    scope->~ASTScope();
    engone::Free(scope, sizeof(ASTScope));
}
void AST::destroy(ASTStruct *astStruct) {
    if (astStruct->next)
        destroy(astStruct->next);
    for (auto &mem : astStruct->members) {
        if (mem.defaultValue)
            destroy(mem.defaultValue);
    }
    astStruct->members.clear();
    if (astStruct->functions)
        destroy(astStruct->functions);
    astStruct->~ASTStruct();
    engone::Free(astStruct, sizeof(ASTStruct));
}
void AST::destroy(ASTFunction *function) {
    if (function->next)
        destroy(function->next);
    if (function->body)
        destroy(function->body);
    for (auto &arg : function->arguments) {
        if (arg.defaultValue)
            destroy(arg.defaultValue);
    }
    for(auto ptr : function->polyImpls){
        ptr->~FuncImpl();
        engone::Free(ptr,sizeof(FuncImpl));
    }
    function->~ASTFunction();
    engone::Free(function, sizeof(ASTFunction));
}
void AST::destroy(ASTEnum *astEnum) {
    if (astEnum->next)
        destroy(astEnum->next);
    astEnum->~ASTEnum();
    engone::Free(astEnum, sizeof(ASTEnum));
}
void AST::destroy(ASTStatement *statement) {
    if (statement->next)
        destroy(statement->next);
    if (statement->name){
        statement->name->~basic_string<char>();
        engone::Free(statement->name, sizeof(std::string));
        statement->name = nullptr;
    }
    if (statement->alias){
        statement->alias->~basic_string<char>();
        engone::Free(statement->alias, sizeof(std::string));
        statement->alias = nullptr;
    }
    // if (statement->lvalue)
    //     destroy(statement->lvalue);
    if (statement->rvalue)
        destroy(statement->rvalue);
    if (statement->body)
        destroy(statement->body);
    if (statement->elseBody)
        destroy(statement->elseBody);
    statement->~ASTStatement();
    engone::Free(statement, sizeof(ASTStatement));
}
void AST::destroy(ASTExpression *expression) {
    if (expression->next)
        destroy(expression->next);
    if (expression->name) {
        expression->name->~basic_string<char>();
        engone::Free(expression->name, sizeof(std::string));
        expression->name = nullptr;
    }
    if (expression->namedValue){
        expression->namedValue->~basic_string<char>();
        engone::Free(expression->namedValue, sizeof(std::string));
        expression->namedValue = nullptr;
    }
    if (expression->left)
        destroy(expression->left);
    if (expression->right)
        destroy(expression->right);
    expression->~ASTExpression();
    engone::Free(expression, sizeof(ASTExpression));
}
u32 TypeInfo::getSize() {
    if (astStruct) {
        if(structImpl) {
            return structImpl->size;
        } else {
            return astStruct->baseImpl.size;
        }
    }
    return _size;
}
StructImpl* TypeInfo::getImpl(){
    return !astStruct ? nullptr : (astStruct->isPolymorphic() ? structImpl : &astStruct->baseImpl);
}
u32 TypeInfo::alignedSize() {
    if (astStruct) {
        if(structImpl)
            return structImpl->alignedSize;
        else
            return astStruct->baseImpl.alignedSize;
    }
    return _size > 8 ? 8 : _size;
}
Token AST::TrimNamespace(Token typeString, Token* outNamespace){
    if(!typeString.str || typeString.length < 3) {
        if(outNamespace)           
           *outNamespace = {};
        return typeString;
    }
    // TODO: Line and column is ignored but it should also be separated properly.
    // TODO: ":::" should cause an error but isn't considered at all here.
    // TODO: This code doesn't work if poly names have namespaces.
    // int index = -1; // index of the left colon
    // for(int i=typeString.length-1;i>0;i--){
    //     if(typeString.str[i-1] == ':' && typeString.str[i] == ':') {
    //         index = i-1;
    //         break;
    //     }
    // }
    // if(index==-1){
    //     if(outNamespace)
    //         *outNamespace = {};
    //     return typeString;
    // }
    // if(outNamespace){
    //     outNamespace->str = typeString.str;
    //     outNamespace->length = index;
    // }
    // out.str = typeString.str + (index+2);
    // out.length = typeString.length - (index+2);

    int lastColonIndex = -1;
    int depth = 0;
    for(int i=typeString.length-1;i>=0;i--){
        if(typeString.str[i] == '>'){
            depth++;
        }
        if(typeString.str[i] == '<'){
            depth--;
        }
        if(depth==0 && i < typeString.length - 1){
            if(typeString.str[i] == ':' && typeString.str[i+1] == ':') {
                lastColonIndex = i;
                break;
            }
        }
    }
    if(lastColonIndex!=-1){
        outNamespace->str = typeString.str;
        outNamespace->length = lastColonIndex;
        typeString.str = typeString.str + lastColonIndex + 2;
        typeString.length = typeString.length - (lastColonIndex + 2);
    }
    return typeString;
}
Token AST::TrimPointer(Token& token, u32* outLevel){
    Token out = token;
    for(int i=token.length-1;i>=0;i--){
        if(token.str[i]=='*') {
            if(outLevel)
                (*outLevel)++;
            continue;
        } else {
            out.length = i + 1;
            break;
        }
    }
    return out;
}
Token AST::TrimBaseType(Token typeString, Token* outNamespace, 
    u32* level, std::vector<Token>* outPolyTypes, Token* typeName)
{
    if(!typeString.str || !outNamespace || !level || !outPolyTypes) {
        return typeString;
    }
    // TODO: Line and column in tokens are ignored but should
    //   be seperated into the divided tokens.

    //-- Trim pointers
    for(int i=typeString.length-1;i>=0;i--){
        if(typeString.str[i]=='*') {
            (*level)++;
            continue;
        } else {
            typeString.length = i + 1;
            break;
        }
    }

    //-- Trim namespaces
    int lastColonIndex = -1;
    int depth = 0;
    for(int i=typeString.length-1;i>=0;i--){
        if(typeString.str[i] == '>'){
            depth++;
        }
        if(typeString.str[i] == '<'){
            depth--;
        }
        if(depth==0 && i < typeString.length - 1){
            if(typeString.str[i] == ':' && typeString.str[i+1] == ':') {
                lastColonIndex = i;
                break;
            }
        }
    }
    if(lastColonIndex!=-1){
        outNamespace->str = typeString.str;
        outNamespace->length = lastColonIndex;
        typeString.str = typeString.str + lastColonIndex + 2;
        typeString.length = typeString.length - (lastColonIndex + 2);
    }

    *typeName = typeString;

    //-- Trim poly types
    if(typeString.length < 2 || typeString.str[typeString.length-1] != '>') {
        return typeString;
    }
    int leftArrow = -1;
    int rightArrow = typeString.length-1;
    // Right arrow must be at the end of typeString because we can't cut out
    // the polyTypes from typeString and then amend the two ends.
    // We would need to allocate a new string for typeString to reference
    // and we should avoid that if possible.
    for(int i=0;i<typeString.length;i++) {
        if(typeString.str[i] == '<'){
            leftArrow = i;
            break;
        }
    }
    if(leftArrow == -1){
        return typeString;
    }
    typeString.length = leftArrow;

    Token acc = {};
    for(int i=leftArrow + 1; i < rightArrow; i++){
        if(typeString.str[i] != ',') {
            if(!acc.str)
                acc.str = typeString.str + i;
            acc.length++;
        }
        if(typeString.str[i] == ',' || i == rightArrow - 1){
            // We don't push acc if it's empty because we can then
            // check the lengh of outPolyTypes to verify if we
            // have valid types. If empty types are allowed then
            // we would need to go through the array to find
            // empty tokens.
            if(acc.str)
                outPolyTypes->push_back(acc);
            acc = {};
        }
    }
    
    return typeString;
}
Token AST::TrimPolyTypes(Token typeString, std::vector<Token>* outPolyTypes) {
    //-- Trim poly types
    Assert(typeString.str[typeString.length-1] != '*');
    if(typeString.length < 2 || typeString.str[typeString.length-1] != '>') {
        return typeString;
    }
    int leftArrow = -1;
    int rightArrow = typeString.length-1;
    // Right arrow must be at the end of typeString because we can't cut out
    // the polyTypes from typeString and then amend the two ends.
    // We would need to allocate a new string for typeString to reference
    // and we should avoid that if possible.
    for(int i=0;i<typeString.length;i++) {
        if(typeString.str[i] == '<'){
            leftArrow = i;
            break;
        }
    }
    if(leftArrow == -1){
        return typeString;
    }
    typeString.length = leftArrow;

    Token acc = {};
    for(int i=leftArrow + 1; i < rightArrow; i++){
        if(typeString.str[i] != ',') {
            if(!acc.str)
                acc.str = typeString.str + i;
            acc.length++;
        }
        if(typeString.str[i] == ',' || i == rightArrow - 1){
            // We don't push acc if it's empty because we can then
            // check the lengh of outPolyTypes to verify if we
            // have valid types. If empty types are allowed then
            // we would need to go through the array to find
            // empty tokens.
            if(acc.str)
                outPolyTypes->push_back(acc);
            acc = {};
        }
    }
    
    return typeString;

    // if(!typeString.str || typeString.length < 2 || typeString.str[typeString.length-1] != '>') { // <>
    //     return typeString;
    // }
    // // TODO: Line and column is ignored but it should also be separated properly.
    // // TODO: ":::" should cause an error but isn't considered at all here.
    // int leftArrow = -1;
    // int rightArrow = typeString.length-1;
    // for(int i=0;i<typeString.length;i++) {
    //     if(typeString.str[i] == '<'){
    //         leftArrow = i;
    //         break;
    //     }
    // }
    // // for(int i=typeString.length-1;i>=0;i--) {
    // //     if(typeString.str[i] == '>'){
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
    //     if(typeString.str[i] != ',') {
    //         if(!acc.str)
    //             acc.str = typeString.str + i;
    //         acc.length++;
    //     }
    //     if(typeString.str[i] == ',' || i == rightArrow - 1){
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
            return ti->astStruct->baseImpl.alignedSize;
    }
    return ti->_size > 8 ? 8 : ti->_size;
}
// TypeId AST::getTypeId(ScopeId scopeId, Token typeString, bool* success){
//     Token theNamespace = {};
//     u32 plevel = 0;
//     Token typeName = TrimNamespace(typeString,&theNamespace);
//     std::vector<Token> polyTypes;
//     typeName = TrimPointer(typeName,&plevel);
//     typeName = TrimPolyTypes(typeName, &polyTypes);
//     if(theNamespace.str){
//         ScopeId nextScopeId = scopeId;
//         bool quit = false;
//         // namespaced
//         while(!quit){
//             ScopeInfo* scope = getScope(nextScopeId);
//             if(nextScopeId==0)
//                 quit=true;
//             nextScopeId = scope->parent;
//             if(!scope)
//                 break;
                
//             ScopeInfo* nscope = getNamespace(scope->id, theNamespace);
//             if(nscope){
//                 auto pair = nscope->nameTypeMap.find(typeName);
//                 if(pair != nscope->nameTypeMap.end()){
//                     TypeId out = pair->second->id;
//                     out.setPointerLevel(plevel);
//                     if(success) *success = true;
//                     return out;
//                 } else {
//                     if(success) *success = false;
//                     return {};
//                 }   
//             }
//         }
//         if(success) *success = false;
//         return {};
//     }
//     auto scope = getScope(scopeId);
//     if(!scope) { 
//         if(success) *success = false;
//         return {};
//     }
    
//     auto basePair = scope->nameTypeMap.find(typeName);
//     if(basePair != scope->nameTypeMap.end()){
//         TypeId out = basePair->second->id;
//         out.setPointerLevel(plevel);
//         if(success) *success = true;
//         return out;
//     }

//     ScopeId nextParent = scope->parent;
//     if(scopeId!=globalScopeId){ // global scope does not have parent
//         while(true){
//             auto parentScope = getScope(nextParent);
//             if(!parentScope)
//                 break;

//             auto pair = parentScope->nameTypeMap.find(typeName);
//             if(pair!=parentScope->nameTypeMap.end()){
//                 TypeId out = pair->second->id;
//                 out.setPointerLevel(plevel);
//                 if(success) *success = true;
//                 return out;
//             }
//             if(nextParent==globalScopeId)
//                 break;
//             nextParent = parentScope->parent;
//         }
//     }
//     if(success) *success = false;
//     return {};
// }

void ASTStruct::add(ASTFunction* func){
    // NOTE: Is this code necessary? Did something break.
    auto pair = baseImpl.methods.find(func->name);
    Assert(pair == baseImpl.methods.end());
    baseImpl.methods[func->name] = {func, &func->baseImpl};

    if(!functions){
        functions = func;
        functionsTail = func;
    } else {
        functionsTail->next = func;
    }
    while(functionsTail->next){
        functionsTail = functionsTail->next;
    }
}

void StructImpl::addPolyMethod(const std::string& name, ASTFunction* func, FuncImpl* funcImpl){
    auto pair = methods.find(name);
    Assert(pair == methods.end());

    methods[name] = {func, funcImpl};
}
std::string ScopeInfo::getFullNamespace(AST* ast){
    std::string ns = "";
    ScopeInfo* scope = ast->getScope(id);
    while(scope){
        if(ns.empty() && !scope->name.empty())
            ns = scope->name;
        else if(!scope->name.empty())
            ns = scope->name + "::"+ns;
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
        StructImpl* impl = structImpl ? structImpl : &astStruct->baseImpl;
        return {impl->members[index].typeId, index, impl->members[index].offset};
    } else {
        return {{}, -1};
    }
}
bool ASTEnum::getMember(const std::string &name, int *out) {
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
    return AST_UINT8 <= id.getId() && id.getId() <= AST_INT64;
}
bool AST::IsSigned(TypeId id) {
    return AST_INT8 <= id.getId() && id.getId() <= AST_INT64;
}
/* #region  */
void PrintSpace(int count) {
    using namespace engone;
    for (int i = 0; i < count; i++)
        log::out << "  ";
}

void AST::print(int depth) {
    using namespace engone;
    if (mainBody) {
        PrintSpace(depth);
        log::out << "AST\n";
        mainBody->print(this, depth + 1);
    }
}
void ASTScope::print(AST *ast, int depth) {
    using namespace engone;
    PrintSpace(depth);
    if(type == BODY)
        log::out << "Body\n";
    if(type == NAMESPACE){
        log::out << "Namespace ";
        if(name)
            log::out << " "<<*name;
        log::out<<"\n";
           
    }
    if (structs)
        structs->print(ast, depth + 1);
    if (enums)
        enums->print(ast, depth + 1);
    if (functions)
        functions->print(ast, depth + 1);
    if (statements)
        statements->print(ast, depth + 1);
    if (namespaces)
        namespaces->print(ast, depth + 1);
    if(next)
        next->print(ast,depth);
}
void ASTFunction::print(AST *ast, int depth) {
    using namespace engone;
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
    log::out << "(";
    for (int i = 0; i < (int)arguments.size(); i++) {
        auto &arg = arguments[i];
        auto &argImpl = baseImpl.arguments[i];
        log::out << arg.name << ": ";
        log::out << ast->typeToString(argImpl.typeId);
        if (i + 1 != (int)arguments.size()) {
            log::out << ", ";
        }
    }
    log::out << ")";
    if (!baseImpl.returnTypes.empty())
        log::out << "->";
    for (auto &ret : baseImpl.returnTypes) {
        // auto dtname = ast->getTypeInfo(ret.typeId)->getFullType(ast);
        // log::out << dtname << ", ";
        log::out << ast->typeToString(ret.typeId) << ", ";
    }
    log::out << "\n";
    if (body) {
        body->print(ast, depth + 1);
    }
    if (next)
        next->print(ast, depth);
}
void ASTStruct::print(AST *ast, int depth) {
    using namespace engone;
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
    log::out << " { ";
    for (int i = 0; i < (int)members.size(); i++) {
        auto &member = members[i];
        auto &implMem = baseImpl.members[i];
        auto str = ast->typeToString(implMem.typeId);
        log::out << member.name << ": " << str;
        if (member.defaultValue) {
            log::out << " = ";
            member.defaultValue->tokenRange.print();
        }
        if (i + 1 != (int)members.size()) {
            log::out << ", ";
        }
    }
    if(functions){
        log::out << "\n";
        functions->print(ast,depth);
        PrintSpace(depth);
    }
    log::out << " }\n";
    if (next)
        next->print(ast, depth);
}
void ASTEnum::print(AST *ast, int depth) {
    using namespace engone;
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
    if (next)
        next->print(ast, depth);
}
void ASTStatement::print(AST *ast, int depth) {
    using namespace engone;
    PrintSpace(depth);
    log::out << "Statement " << StateToStr(type);

    if(typeId.isValid())
        log:: out << " "<< ast->typeToString(typeId);
    if(name)
        log::out << " " << *name;
    if(alias)
        log::out << " as " << *alias;
    // if(opType!=0)
    //     log::out << " "<<OpToStr((OperationType)opType)<<"=";
    log::out << "\n";
    // if (lvalue) {
    //     lvalue->print(ast, depth + 1);
    // }
    if (rvalue) {
        rvalue->print(ast, depth + 1);
    }
    if (body) {
        body->print(ast, depth + 1);
    }
    if (elseBody) {
        elseBody->print(ast, depth + 1);
    }
    if (next) {
        next->print(ast, depth);
    }
}
void ASTExpression::print(AST *ast, int depth) {
    using namespace engone;
    PrintSpace(depth);

    if (isValue) {
        log::out << "Expr ";
        log::out << ast->typeToString(typeId);
        log::out << " ";
        if (typeId == AST_FLOAT32)
            log::out << f32Value;
        else if (AST::IsInteger(typeId))
            log::out << i64Value;
        else if (typeId == AST_BOOL)
            log::out << boolValue;
        else if (typeId == AST_CHAR)
            log::out << charValue;
        else if (typeId == AST_VAR)
            log::out << *name;
        // else if(typeId==AST_STRING) log::out << ast->constStrings[constStrIndex];
        else if (typeId == AST_FNCALL)
            log::out << *name;
        else if (typeId == AST_NULL)
            log::out << "null";
        else if(typeId == AST_STRING)
            log::out << *name;
        else if(typeId == AST_SIZEOF)
            log::out << *name;
        else
            log::out << "missing print impl.";
        if (typeId == AST_FNCALL) {
            if (left) {
                log::out << " args:\n";
                left->print(ast, depth + 1);
            } else {
                log::out << " no args\n";
            }
        } else
            log::out << "\n";
    } else {
        log::out << "Expr ";
        if(typeId == AST_ASSIGN) {
            if(castType.getId()!=0){
                log::out << OpToStr((OperationType)castType.getId());
            }
        }
        log::out << OpToStr((OperationType)typeId.getId()) << " ";
        if (typeId == AST_CAST) {
            log::out << ast->typeToString(castType);
            log::out << "\n";
            left->print(ast, depth + 1);
        } else if (typeId == AST_MEMBER) {
            log::out << *name;
            log::out << "\n";
            left->print(ast, depth + 1);
        } else if (typeId == AST_INITIALIZER) {
            log::out << ast->typeToString(castType);
            log::out << "\n";
            if(left)
                left->print(ast, depth + 1);
        } else if (typeId == AST_FROM_NAMESPACE) {
            log::out << *name;
            log::out << "\n";
            if(left)
                left->print(ast, depth + 1);
        } else if(typeId == AST_ASSIGN) {
            // if(castType.getId()!=0){
            //     log::out << OpToStr((OperationType)castType.getId());
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
    if (next) {
        next->print(ast, depth);
    }
}
/* #endregion */