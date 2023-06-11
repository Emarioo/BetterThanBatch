#include "BetBat/AST.h"

const char *OpToStr(OperationType optype) {
#define CASE(A, B) \
    case AST_##A:  \
        return #B;
    switch (optype) {
        CASE(ADD, +)
        CASE(SUB, -)
        CASE(MUL, *)
        CASE(DIV, /)
        CASE(MODULUS, /)

        CASE(EQUAL, ==)
        CASE(NOT_EQUAL, !=)
        CASE(LESS, <)
        CASE(LESS_EQUAL, <=)
        CASE(GREATER, >)
        CASE(GREATER_EQUAL, >=)
        CASE(AND, &&)
        CASE(OR, ||)
        CASE(NOT, !)

        CASE(BAND, &)
        CASE(BOR, |)
        CASE(BXOR, ^)
        CASE(BNOT, ~)
        CASE(BLSHIFT, <<)
        CASE(BRSHIFT, >>)

        CASE(CAST, cast)
        CASE(MEMBER, member)
        CASE(INITIALIZER, initializer)
        CASE(SLICE_INITIALIZER, slice_initializer)
        CASE(FROM_NAMESPACE, namespaced)
        CASE(SIZEOF, sizeof)

        CASE(REFER, &)
        CASE(DEREF, * (dereference))
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
        CASE(PROP_ASSIGN, prop_assign)
        CASE(IF, if)
        CASE(WHILE, while)
        CASE(RETURN, return)
        CASE(CALL, call)
        CASE(USING, using)
        CASE(BODY, body)
        CASE(DEFER, defer)
    }
#undef CASE
    return "?";
}
// const char* TypeIdToStr(int type){
//     #define CASE(A,B) case AST_##A: return #B;
//     switch(type){
//         CASE(FLOAT32,f32)
//         CASE(INT32,i32)
//         CASE(BOOL,bool)
//         CASE(CHAR,char)

//         CASE(VAR,var)
//     }
//     #undef CASE
//     return "?";
// }
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
    new (ast) AST();

    ScopeId scopeId = ast->addScopeInfo();
    ast->globalScopeId = scopeId;
    // initialize default data types
    ast->addTypeInfo(scopeId,Token("void"), AST_VOID);
    ast->addTypeInfo(scopeId,Token("u8"), AST_UINT8, 1);
    ast->addTypeInfo(scopeId,Token("u16"), AST_UINT16, 2);
    ast->addTypeInfo(scopeId,Token("u32"), AST_UINT32, 4);
    ast->addTypeInfo(scopeId,Token("u64"), AST_UINT64, 8);
    ast->addTypeInfo(scopeId,Token("i8"), AST_INT8, 1);
    ast->addTypeInfo(scopeId,Token("i16"), AST_INT16, 2);
    ast->addTypeInfo(scopeId,Token("i32"), AST_INT32, 4);
    ast->addTypeInfo(scopeId,Token("i64"), AST_INT64, 8);
    ast->addTypeInfo(scopeId,Token("f32"), AST_FLOAT32, 4);
    ast->addTypeInfo(scopeId,Token("bool"), AST_BOOL, 1);
    ast->addTypeInfo(scopeId,Token("char"), AST_CHAR, 1);
    ast->addTypeInfo(scopeId,Token("null"), AST_NULL, 8);
    ast->addTypeInfo(scopeId,Token("var"), AST_VAR);
    ast->addTypeInfo(scopeId,Token("member"), AST_MEMBER);
    ast->addTypeInfo(scopeId,Token("call"), AST_FNCALL);

    ast->mainBody = ast->createBody(); {
        // TODO: set size and offset of language structs here instead of letting the compiler do it.
        // ASTStruct* astStruct = ast->createStruct("Slice");
        // auto voidInfo = ast->getTypeInfo("void*");
        // astStruct->members.push_back({"ptr",voidInfo->id});
        // astStruct->members.push_back({"len",ast->getTypeInfo("u64")->id});
        // auto structType = ast->getTypeInfo("Slice");
        // structType->astStruct = astStruct;
        // ast->mainBody->add(astStruct);
    }
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
// FunctionInfo *AST::addFunction(ScopeId scopeId, const std::string &name) {
//     using namespace engone;
//     auto id = addIdentifier(scopeId, name);
//     if(!id)
//         return nullptr;

//     id->type = Identifier::FUNC;
//     id->index = functions.size();
//     functions.push_back({});
//     return &functions[id->index];
// }
Identifier* AST::addFunction(ScopeId scopeId, ASTFunction* func) {
    using namespace engone;
    auto id = addIdentifier(scopeId, func->name);
    if(!id)
        return nullptr;

    id->type = Identifier::FUNC;
    id->astFunc = func;
    return id;
}
Identifier *AST::addIdentifier(ScopeId scopeId, const std::string &name) {
    using namespace engone;
    ScopeInfo* si = getScopeInfo(scopeId);
    if(!si)
        return nullptr;

    auto pair = si->identifierMap.find(name);
    if (pair != si->identifierMap.end())
        return nullptr;

    auto ptr = &(si->identifierMap[name] = {});
    ptr->name = name;
    return ptr;
}
Identifier *AST::getIdentifier(ScopeId scopeId, const std::string &name) {
    using namespace engone;
    Token ns={};
    Token realName = TrimNamespace(Token(name), &ns);
    ScopeId nextScopeId = scopeId;
    while(true) {
        if(ns.str) {
            ScopeInfo* nscope = getNamespace(nextScopeId, ns);
            if(nscope) {
                auto pair = nscope->identifierMap.find(realName);
                if(pair != nscope->identifierMap.end()){
                    return &pair->second;
                }
            }
        }
        ScopeInfo* si = getScopeInfo(nextScopeId);
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

ASTFunction* ASTStruct::getFunction(const std::string& name){
    ASTFunction* next = functions;
    while(next){
        ASTFunction* now = next;
        next = next->next;
        if(now->name == name){
            return now;
        }
    }
    return nullptr;
}
// Namespace *::getNamespace(int index) {
//     // TODO: bound check
//     return &namespaces[index];
// }
void AST::removeIdentifier(ScopeId scopeId, const std::string &name) {
    auto si = getScopeInfo(scopeId);
    auto pair = si->identifierMap.find(name);
    if (pair != si->identifierMap.end()) {
        // TODO: The variable identifier points to cannot be erased since
        //   the variables array will invalidate other identifier's index.
        //   This means that the array will keep growing. Fix this.
        si->identifierMap.erase(pair);
    }
}
std::string TypeInfo::getFullType(AST* ast){
    ScopeInfo* scope = ast->getScopeInfo(scopeId);
    std::string ns = scope->getFullNamespace(ast);
    if(ns.empty()){
        return name;
    }else {
        return ns + "::" + name;   
    }
}
void AST::appendToMainBody(ASTScope *body) {
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

// void ASTScope::convertToNamespace(const std::string& name){
//     this->name = new std::string(name);
//     type = NAMESPACE;
    
    
// }
ASTScope *AST::createBody() {
    auto ptr = (ASTScope *)engone::Allocate(sizeof(ASTScope));
    new (ptr) ASTScope();
    ptr->type = ASTScope::BODY;
    return ptr;
}
ASTStatement *AST::createStatement(int type) {
    auto ptr = (ASTStatement *)engone::Allocate(sizeof(ASTStatement));
    new (ptr) ASTStatement();
    ptr->type = type;
    return ptr;
}
ASTStruct *AST::createStruct(const std::string &name) {
    auto ptr = (ASTStruct *)engone::Allocate(sizeof(ASTStruct));
    new (ptr) ASTStruct();
    ptr->name = name;
    // new std::string(name);
    return ptr;
}
ASTEnum *AST::createEnum(const std::string &name) {
    auto ptr = (ASTEnum *)engone::Allocate(sizeof(ASTEnum));
    new (ptr) ASTEnum();
    ptr->name = name;
    // new std::string(name);
    return ptr;
}
ASTFunction *AST::createFunction(const std::string &name) {
    auto ptr = (ASTFunction *)engone::Allocate(sizeof(ASTFunction));
    new (ptr) ASTFunction();
    ptr->name = name;
    // new std::string(name);
    return ptr;
}
ASTExpression *AST::createExpression(TypeId type) {
    auto ptr = (ASTExpression *)engone::Allocate(sizeof(ASTExpression));
    new (ptr) ASTExpression();
    ptr->isValue = (u32)type.getId() < AST_PRIMITIVE_COUNT;
    ptr->typeId = type;
    return ptr;
}
ASTScope *AST::createNamespace(const std::string& name) {
    auto ptr = (ASTScope *)engone::Allocate(sizeof(ASTScope));
    new (ptr) ASTScope();
    ptr->type = ASTScope::NAMESPACE;
    ptr->name = new std::string(name);
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

    for (auto &pair : scopeInfos) {
        pair.second->nameTypeMap.clear();
        // for(auto& pair : pair.second->nameFuncMap){
        //     engone::Free(pair.second, sizeof(FunctionInfo));
        // }
        // pair.second->nameFuncMap.clear();
        
        pair.second->~ScopeInfo();
        engone::Free(pair.second, sizeof(ScopeInfo));
    }
    scopeInfos.clear();
    for(auto& pair : typeInfos){
        pair.second->~TypeInfo();
        engone::Free(pair.second, sizeof(TypeInfo));
    }
    typeInfos.clear();
    

    nextScopeId = 0;
    nextTypeId = NEXT_ID;
    // nextPointerTypeId = POINTER_BIT;
    // nextSliceTypeId = SLICE_BIT;

    destroy(mainBody);
    mainBody = 0;
}

void AST::destroy(ASTScope *scope) {
     if (scope->next)
        destroy(scope->next);
    if (scope->name)
        delete scope->name;
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
    if (astStruct->functions)
        destroy(astStruct->functions);
    astStruct->~ASTStruct();
    engone::Free(astStruct, sizeof(ASTStruct));
}
void AST::destroy(ASTFunction *function) {
    if (function->next)
        destroy(function->next);
    // if (function->name)
    //     delete function->name;
    if (function->body)
        destroy(function->body);
    for (auto &arg : function->arguments) {
        if (arg.defaultValue)
            destroy(arg.defaultValue);
    }
    function->~ASTFunction();
    engone::Free(function, sizeof(ASTFunction));
}
void AST::destroy(ASTEnum *astEnum) {
    if (astEnum->next)
        destroy(astEnum->next);
    // if (astEnum->name)
    //     delete astEnum->name;
    astEnum->~ASTEnum();
    engone::Free(astEnum, sizeof(ASTEnum));
}
void AST::destroy(ASTStatement *statement) {
    if (statement->next)
        destroy(statement->next);
    if (statement->name)
        delete statement->name;
    if (statement->alias)
        delete statement->alias;
    if (statement->lvalue)
        destroy(statement->lvalue);
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
    if (expression->name)
        delete expression->name;
    if (expression->namedValue)
        delete expression->namedValue;
    if (expression->left)
        destroy(expression->left);
    if (expression->right)
        destroy(expression->right);
    expression->~ASTExpression();
    engone::Free(expression, sizeof(ASTExpression));
}
u32 TypeInfo::getSize() {
    if (astStruct)
        return astStruct->size;
    return _size;
}
u32 TypeInfo::alignedSize() {
    if (astStruct) {
        return astStruct->alignedSize;
    }
    return _size > 8 ? 8 : _size;
}
ScopeId AST::addScopeInfo() {
    auto ptr = (ScopeInfo *)engone::Allocate(sizeof(ScopeInfo));
    new (ptr) ScopeInfo{nextScopeId++};
    scopeInfos[ptr->id] = ptr;
    return ptr->id;
}
ScopeInfo* AST::getScopeInfo(ScopeId id){
    auto pair = scopeInfos.find(id);
    if(pair==scopeInfos.end()){
        return 0;
    }
    return pair->second;
}
// TypeInfo* addTypeInfo(ScopeId scopeId, const std::string& name){
//     auto ptr = (TypeInfo *)engone::Allocate(sizeof(TypeInfo));
//     new (ptr) TypeInfo{name, , 0};
//     ptr->scopeId = scopeId;
//     auto scope = getScopeInfo(scopeId);
//     Assert(scope);
    
//     typeInfos[ptr->id] = ptr;
//     scope->nameTypeMap[ptr->name] = ptr;
// }
void AST::addTypeInfo(ScopeId scopeId, Token name, TypeId id, u32 size) {
    auto ptr = (TypeInfo *)engone::Allocate(sizeof(TypeInfo));
    new (ptr) TypeInfo{name, id, size};
    ptr->scopeId = scopeId;
    auto scope = getScopeInfo(scopeId);
    Assert(scope);
    
    typeInfos[ptr->id] = ptr;
    scope->nameTypeMap[ptr->name] = ptr;
}
Token AST::TrimNamespace(Token typeString, Token* outNamespace){
    if(!typeString.str || typeString.length < 3) {
        if(outNamespace)           
           *outNamespace = {};
        return typeString;
    }
    // TODO: Line and column is ignored but it should also be separated properly.
    // TODO: ":::" should cause an error but isn't considered at all here.
    int index = -1; // index of the left colon
    for(int i=typeString.length-1;i>0;i--){
        if(typeString.str[i-1] == ':' && typeString.str[i] == ':') {
            index = i-1;
            break;
        }
    }
    if(index==-1){
        if(outNamespace)
            *outNamespace = {};
        return typeString;
    }
    if(outNamespace){
        outNamespace->str = typeString.str;
        outNamespace->length = index;
    }
    Token out = typeString;
    out.str = typeString.str + (index+2);
    out.length = typeString.length - (index+2);
    return out;
}
ScopeInfo* AST::getNamespace(ScopeId scopeId, const std::string name){
    auto scope = getScopeInfo(scopeId);
    if(!scope) return nullptr;
    
    int split = name.find("::");
    std::string view = "";
    std::string rest = "";
    if(split==-1)
        view = name;
    else {
        view = name.substr(0,split);
        rest = name.substr(split+2);
    }
    
    auto pair = scope->nameScopeMap.find(view);
    if(pair == scope->nameScopeMap.end()){
        return nullptr;   
    } else {
        if(rest.empty())
            return pair->second;
        return getNamespace(pair->second->id, rest);
    }
}
TypeId AST::addTypeString(Token name){
    for(int i=0;i<(int)typeStrings.size();i++){
        if(name == typeStrings[i])
            return TypeId::CreateString(i);
    }
    typeStrings.push_back(name);
    return TypeId::CreateString(typeStrings.size()-1);
}

// FunctionInfo* AST::addFunction(ScopeId scopeId, ASTFunction* func){
//     auto scopeInfo = getScopeInfo(scopeId);
//     if(!scopeInfo)
//         return nullptr;
    
//     auto pair = scopeInfo->nameFuncMap.find(func->name);
//     if(pair != scopeInfo->nameFuncMap.end())
//         return 0; // already exists

//     auto ptr = (FunctionInfo*)engone::Allocate(sizeof(FunctionInfo));
//     ptr->func = func;
//     scopeInfo->nameFuncMap[func->name] = ptr;
//     return ptr;
// }
// FunctionInfo* AST::getFunction(ScopeId scopeId, Token name){
//     using namespace engone;
//     Token theNamespace = {};
//     u32 plevel = 0;
//     Token tempName = TrimNamespace(name,&theNamespace);
//     if(theNamespace.str) {
//         log::out << log::RED << "Cannot get function name with namespace: "<< name << "\n";
//         return nullptr;
//     }
//     // Token typeName = TrimPointer(tempName,&plevel);

//     if(plevel != 0) {
//         // log::out << log::RED << "Cannot use "<<__FUNCTION__<<" on pointer type!\n";
//         log::out << log::GOLD << "Warning! Using "<<__FUNCTION__<<" on pointer type!\n";
//         // return nullptr;
//     }
//     auto scope = getScopeInfo(scopeId);
//     if(!scope)
//         return nullptr;
    
//     // u32 plevel = AST::GetPointerLevel(name);
//     // if (plevel != 0) {
//     //     Token typeName = AST::TrimPointer(name);
        
//     // }

//     auto basePair = scope->nameFuncMap.find(name);
//     if(basePair != scope->nameFuncMap.end()){
//         return basePair->second;
//     }

//     ScopeId nextParent = scope->parent;
//     if(scopeId!=globalScopeId){ // global scope does not have parent
//         while(true){
//             auto parentScope = getScopeInfo(nextParent);
//             if(!parentScope)
//                 break;

//             auto pair = parentScope->nameFuncMap.find(name);
//             if(pair!=parentScope->nameFuncMap.end())
//                 return pair->second;
            
//             if(nextParent==globalScopeId)
//                 break;
//             nextParent = parentScope->parent;
//         }
//     }
//     return nullptr;
// }
const std::string& AST::getTypeString(TypeId typeId){
    // TODO: Use typeToString if typeId isn't string type from parser?
    //  It might be better to explicitly use typeToString.
    static const std::string teaesta="";
    if(typeId.isString()) {
        int index = typeId.getId();
        if(index < 0 || index > (int)typeStrings.size())
            return teaesta;
        
        return typeStrings[index];
    }
    return teaesta;
}

u32 AST::getTypeSize(TypeId typeId){
    if(typeId.isPointer()) return 8; // TODO: Magic number! Is pointer always 8 bytes? Probably but who knows!
    auto ti = typeInfos.find(typeId);
    if(ti == typeInfos.end())
        return 0;
    return ti->second->getSize();
}
u32 AST::getTypeAlignedSize(TypeId typeId) {
    if(typeId.isPointer()) return 8; // TODO: Magic number! Is pointer always 8 bytes? Probably but who knows!
    auto ti = typeInfos.find(typeId);
    if(ti == typeInfos.end())
        return 0;
    // return ti->second->size();
    if (ti->second->astStruct) {
        return ti->second->astStruct->alignedSize;
    }
    return ti->second->_size > 8 ? 8 : ti->second->_size;
}
TypeId AST::getTypeId(ScopeId scopeId, Token typeString, bool* success){
    Token theNamespace = {};
    u32 plevel = 0;
    Token tempName = TrimNamespace(typeString,&theNamespace);
    Token typeName = TrimPointer(tempName,&plevel);
    if(theNamespace.str){
        // if(forceCreate)
        //     return 0;
        ScopeId nextScopeId = scopeId;
        bool quit = false;
        // namespaced
        while(!quit){
            ScopeInfo* scope = getScopeInfo(nextScopeId);
            if(nextScopeId==0)
                quit=true;
            nextScopeId = scope->parent;
            if(!scope)
                break;
                
            ScopeInfo* nscope = getNamespace(scope->id, theNamespace);
            if(nscope){
                auto pair = nscope->nameTypeMap.find(typeName);
                if(pair != nscope->nameTypeMap.end()){
                    TypeId out = pair->second->id;
                    out.setPointerLevel(plevel);
                    if(success) *success = true;
                    return out;
                } else {
                    if(success) *success = false;
                    return {};
                }   
            }
        }
        if(success) *success = false;
        return {};
    }
    auto scope = getScopeInfo(scopeId);
    if(!scope) { 
        if(success) *success = false;
        return {};
    }
    
    auto basePair = scope->nameTypeMap.find(typeName);
    if(basePair != scope->nameTypeMap.end()){
        TypeId out = basePair->second->id;
        out.setPointerLevel(plevel);
        if(success) *success = true;
        return out;
    }

    ScopeId nextParent = scope->parent;
    if(scopeId!=globalScopeId){ // global scope does not have parent
        while(true){
            auto parentScope = getScopeInfo(nextParent);
            if(!parentScope)
                break;

            auto pair = parentScope->nameTypeMap.find(typeName);
            if(pair!=parentScope->nameTypeMap.end()){
                TypeId out = pair->second->id;
                out.setPointerLevel(plevel);
                if(success) *success = true;
                return out;
            }
            if(nextParent==globalScopeId)
                break;
            nextParent = parentScope->parent;
        }
    }
    if(success) *success = false;
    return {};
}
TypeInfo* AST::addType(ScopeId scopeId, Token typeString){
    using namespace engone;
    Token theNamespace = {};
    u32 plevel = 0;
    Token tempName = TrimNamespace(typeString,&theNamespace);
    Token typeName = TrimPointer(tempName,&plevel);

    if(plevel != 0) {
        log::out << log::RED << __FUNCTION__<<": Pointer types should not be added to type list (bug in compiler)\n";
        return nullptr;
    }

    if(theNamespace.str){
        log::out << log::RED << __FUNCTION__ <<": Cannot create types of other namespaces\n";
        return nullptr;
    }
    auto scope = getScopeInfo(scopeId);
    if(!scope)
        return nullptr;
    
    auto basePair = scope->nameTypeMap.find(typeName);
    if(basePair != scope->nameTypeMap.end()) {
        return nullptr;
    }

    ScopeId nextParent = scope->parent;
    if(scopeId!=globalScopeId){ // global scope does not have parent
        while(true){
            auto parentScope = getScopeInfo(nextParent);
            if(!parentScope)
                break;

            auto pair = parentScope->nameTypeMap.find(typeName);
            if(pair!=parentScope->nameTypeMap.end())
                return nullptr; // type exists in parent scope
            
            if(nextParent==globalScopeId)
                break;
            nextParent = parentScope->parent;
        }
    }
    // Token tok;
    // tok.str = (char *)name.c_str();
    // tok.length = name.length();
    auto ptr = (TypeInfo *)engone::Allocate(sizeof(TypeInfo));
    new (ptr) TypeInfo{typeName, TypeId::Create(nextTypeId++)};
    ptr->scopeId = scopeId;
    scope->nameTypeMap[typeName] = ptr;
    typeInfos[ptr->id] = ptr;
    return ptr;
}
void ASTStruct::add(ASTFunction* func){
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
std::string AST::typeToString(TypeId typeId){
    const char* cstr = OpToStr((OperationType)typeId.getId());
    if(strcmp(cstr,"?")) {
        return cstr;
    }
    TypeInfo* ti = getTypeInfo(typeId.baseType());
    if(!ti)
        return Token("");
    std::string str = ti->getFullType(this);
    for(int i=0;i<(int)typeId.getPointerLevel();i++){
        str+="*";
    }
    return str;
}
TypeInfo *AST::getTypeInfo(ScopeId scopeId, Token typeString) {
    using namespace engone;
    // TODO: This code can be compressed into fewer lines.
    //   It has been done with getIdentifier which has similar functionality.
    Token theNamespace = {};
    u32 plevel = 0;
    Token tempName = TrimNamespace(typeString,&theNamespace);
    Token typeName = TrimPointer(tempName,&plevel);
    if(plevel != 0) {
        // log::out << log::RED << "Cannot use "<<__FUNCTION__<<" on pointer type!\n";
        log::out << log::GOLD << "Warning! Using "<<__FUNCTION__<<" on pointer type!\n";
        // return nullptr;
    }

    if(theNamespace.str){
        // if(forceCreate)
        //     return 0;
        ScopeId nextScopeId = scopeId;
        bool quit = false;
        // namespaced
        while(!quit){
            ScopeInfo* scope = getScopeInfo(nextScopeId);
            if(nextScopeId==0)
                quit=true;
            nextScopeId = scope->parent;
            if(!scope)
                break;
                
            ScopeInfo* nscope = getNamespace(scope->id, theNamespace);
            if(nscope){
                auto pair = nscope->nameTypeMap.find(typeName);
                if(pair != nscope->nameTypeMap.end()){
                    return pair->second;
                } else {
                    return 0;
                }   
            }
        }
        return 0;
    }
    auto scope = getScopeInfo(scopeId);
    if(!scope) return 0;
    
    // u32 plevel = AST::GetPointerLevel(name);
    // if (plevel != 0) {
    //     Token typeName = AST::TrimPointer(name);
        
    // }

    auto basePair = scope->nameTypeMap.find(typeName);
    if(basePair != scope->nameTypeMap.end()){
        // if(forceCreate) return 0;
        return basePair->second;
    }

    ScopeId nextParent = scope->parent;
    if(scopeId!=globalScopeId){ // global scope does not have parent
        while(true){
            auto parentScope = getScopeInfo(nextParent);
            if(!parentScope)
                break;

            auto pair = parentScope->nameTypeMap.find(typeName);
            if(pair!=parentScope->nameTypeMap.end())
                return pair->second;
            
            if(nextParent==globalScopeId)
                break;
            nextParent = parentScope->parent;
        }
    }
    return nullptr;
    // if (dontCreate)
    //     return 0;
    // Token tok;
    // tok.str = (char *)name.c_str();
    // tok.length = name.length();
    // if (plevel!=0) {
    //     auto ptr = (TypeInfo *)engone::Allocate(sizeof(TypeInfo));
    //     new (ptr) TypeInfo{name, nextPointerTypeId++, 8}; // NOTE: fixed pointer size of 8
    //     ptr->scopeId = scopeId;
    //     scope->nameTypeMap[name] = ptr;
    //     typeInfos[ptr->id] = ptr;
    //     return ptr;
    // } 
    // else if (AST::IsSlice(tok)) {
    //     // NOTE: slice should exist in global scope so use that 
    //     //  instead of scopeId to save some time.
    //     return getTypeInfo(globalScopeId,"Slice");
    //     // return (typeInfos[name] = new TypeInfo{nextSliceTypeId++, 16}); // NOTE: fixed pointer size of 8
    // } 
    // auto ptr = (TypeInfo *)engone::Allocate(sizeof(TypeInfo));
    // new (ptr) TypeInfo{name, nextTypeIdId++};
    // ptr->scopeId = scopeId;
    // scope->nameTypeMap[name] = ptr;
    // typeInfos[ptr->id] = ptr;
    // return ptr;
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
std::string ScopeInfo::getFullNamespace(AST* ast){
    std::string ns = "";
    ScopeInfo* scope = ast->getScopeInfo(id);
    while(scope){
        if(ns.empty())
            ns = scope->name;
        else
            ns = ns +"::"+scope->name;
        if(scope->parent==0) // end
            return ns;
        scope = ast->getScopeInfo(scope->parent);
    }
    return ns;
}
TypeInfo *AST::getTypeInfo(TypeId id) {
    if(!id.isNormalType()) return nullptr;
    auto pair = typeInfos.find(id);
    if (pair != typeInfos.end()) {
        return pair->second;
    }
    return nullptr;
}
TypeInfo::MemberOut TypeInfo::getMember(const std::string &name) {
    if (astStruct) {
        auto &members = astStruct->members;
        int index = -1;
        for (int i = 0; i < (int)members.size(); i++) {
            if (members[i].name == name) {
                index = i;
                break;
            }
        }
        if (index == -1) {
            return {{}, -1};
        }
        int pi = members[index].polyIndex;
        if (pi != -1) {
            TypeId typ = polyTypes[pi];
            return {typ, index};
        }

        return {members[index].typeId, index};
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
// bool AST::IsPointer(Token &token) {
//     if (!token.str || token.length < 2)
//         return false;
//     return token.str[token.length - 1] == '*';
// }
// bool AST::IsPointer(TypeId id) {
//     return id & POINTER_BIT;
// }
// u32 AST::GetPointerLevel(Token& token){
//     if (!token.str || token.length < 2)
//         return 0;
//     u32 level = 0;
//     for(int i=token.length-1;i>=0;i--){
//         if(token.str[i] == '*')
//             level++;
//         else break;
//     }
//     return level;
// }
// bool AST::IsSlice(TypeId id) {
//     return id & SLICE_BIT;
// }
// bool AST::IsSlice(Token &token) {
//     if (!token.str || token.length < 3)
//         return false;
//     return token.str[token.length - 2] == '[' && token.str[token.length - 1] == ']';
// }
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
    log::out << "Func " << name << " (";
    for (int i = 0; i < (int)arguments.size(); i++) {
        auto &arg = arguments[i];
        log::out << arg.name << ": ";
        if(arg.typeId.isString()){
            log:: out << " "<< ast->getTypeString(arg.typeId);
        } else {
            log::out << ast->typeToString(arg.typeId);
            // log::out <<  ast->getTypeInfo(arg.typeId)->getFullType(ast);
        }
        if (i + 1 != (int)arguments.size()) {
            log::out << ", ";
        }
    }
    log::out << ")";
    if (!returnTypes.empty())
        log::out << "->";
    for (auto &ret : returnTypes) {
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
    if (polyNames.size() != 0) {
        log::out << "<";
    }
    for (int i = 0; i < (int)polyNames.size(); i++) {
        if (i != 0) {
            log::out << ", ";
        }
        log::out << polyNames[i];
    }
    if (polyNames.size() != 0) {
        log::out << ">";
    }
    log::out << " { ";
    for (int i = 0; i < (int)members.size(); i++) {
        auto &member = members[i];
        auto str = ast->typeToString(member.typeId);
        // auto typeInfo = ast->getTypeInfo(member.typeId);
        // if (typeInfo)
        log::out << member.name << ": " << str;
            // log::out << member.name << ": " << typeInfo->getFullType(ast);
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

    if (type == ASSIGN) {
        if(typeId.isString()){
              log:: out << " "<< ast->getTypeString(typeId);
        } else {
            log::out << " " << ast->typeToString(typeId);
            // auto ti = ast->getTypeInfo(typeId);
            // if(ti){
            //     auto dtname = ti->getFullType(ast);
            //     if (typeId.getId() != 0)
            //         // log::out << " "<<TypeIdToStr(dataType)<<"";
            //         log::out << " " << dtname;
            // } else {
            //     log::out << " TYPE?";
            // }
        }
        log::out << " " << *name << "\n";
        if (rvalue) {
            rvalue->print(ast, depth + 1);
        }
    } else if (type == PROP_ASSIGN) {
        log::out << "\n";
        if (lvalue) {
            lvalue->print(ast, depth + 1);
        }
        if (rvalue)
            rvalue->print(ast, depth + 1);
    } else if (type == IF) {
        log::out << "\n";
        if (rvalue) {
            rvalue->print(ast, depth + 1);
        }
        if (body) {
            body->print(ast, depth + 1);
        }
        if (elseBody) {
            elseBody->print(ast, depth + 1);
        }
    } else if (type == WHILE) {
        log::out << "\n";
        if (rvalue) {
            rvalue->print(ast, depth + 1);
        }
        if (body) {
            body->print(ast, depth + 1);
        }
    } else if (type == RETURN) {
        log::out << "\n";
        if (rvalue) {
            rvalue->print(ast, depth + 1);
        }
    } else if (type == CALL) {
        log::out << "\n";
        if (rvalue) {
            rvalue->print(ast, depth + 1);
        }
    } else if (type == USING) {
        log::out << " " << *name << " as " << *alias << "\n";
    } else if (type == DEFER) {
        log::out << "\n";
        if (body) {
            body->print(ast, depth + 1);
        }
    }  else if (type == BODY) {
        log::out << "\n";
        if (body) {
            body->print(ast, depth + 1);
        }
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
        if(typeId.isString()){
            log::out << ast->getTypeString(typeId);
        } else {
            log::out << ast->typeToString(typeId);
            // log::out << ast->getTypeInfo(typeId)->getFullType(ast);
        }
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
        log::out << "Expr " << OpToStr((OperationType)typeId.getId()) << " ";
        if (typeId == AST_CAST) {
            if(castType.isString()){
                log::out << ast->getTypeString(castType);
            } else {
                log::out << ast->typeToString(castType);
                // log::out << ast->getTypeInfo(castType)->getFullType(ast);
            }
            log::out << "\n";
            left->print(ast, depth + 1);
        } else if (typeId == AST_MEMBER) {
            log::out << *name;
            log::out << "\n";
            left->print(ast, depth + 1);
        } else if (typeId == AST_INITIALIZER) {
            log::out << ast->getTypeString(castType);
            log::out << "\n";
            if(left)
                left->print(ast, depth + 1);
        } else if (typeId == AST_FROM_NAMESPACE) {
            log::out << *name;
            log::out << "\n";
            if(left)
                left->print(ast, depth + 1);
        } else {
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