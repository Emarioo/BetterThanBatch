#include "BetBat/AST.h"

const char *OpToStr(int optype) {
#define CASE(A, B) \
    case AST_##A:  \
        return #B;
    switch (optype) {
        CASE(ADD, +)
        CASE(SUB, -)
        CASE(MUL, *)
        CASE(DIV, /)

        CASE(EQUAL, ==)
        CASE(NOT_EQUAL, !=)
        CASE(LESS, <)
        CASE(LESS_EQUAL, <=)
        CASE(GREATER, >)
        CASE(GREATER_EQUAL, >=)
        CASE(AND, &&)
        CASE(OR, ||)
        CASE(NOT, !)

        CASE(CAST, cast)
        CASE(MEMBER, member)
        CASE(INITIALIZER, initializer)
        CASE(SLICE_INITIALIZER, slice_initializer)
        CASE(FROM_NAMESPACE, namespaced)
        CASE(SIZEOF, sizeof)

        CASE(REFER, &)
        CASE(DEREF, *(deref))
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
        ADD_TRACKING(ASTBody)
        ADD_TRACKING(AST)
        // #define LOG_SIZE(X) << #X " "<<sizeof(X)<<"\n"
        // log::out
        // LOG_SIZE(TypeInfo)
        // LOG_SIZE(ASTExpression)
        // LOG_SIZE(ASTEnum)
        // LOG_SIZE(ASTStruct)
        // LOG_SIZE(ASTFunction)
        // LOG_SIZE(ASTStatement)
        // LOG_SIZE(ASTBody)
        // LOG_SIZE(AST)
        // ;
    }
#endif

    AST *ast = (AST *)engone::Allocate(sizeof(AST));
    new (ast) AST();

    ScopeId scopeId = ast->addScopeInfo();
    ast->globalScopeId = scopeId;
    // initialize default data types
    ast->addTypeInfo(scopeId,"void", AST_VOID);
    ast->addTypeInfo(scopeId,"u8", AST_UINT8, 1);
    ast->addTypeInfo(scopeId,"u16", AST_UINT16, 2);
    ast->addTypeInfo(scopeId,"u32", AST_UINT32, 4);
    ast->addTypeInfo(scopeId,"u64", AST_UINT64, 8);
    ast->addTypeInfo(scopeId,"i8", AST_INT8, 1);
    ast->addTypeInfo(scopeId,"i16", AST_INT16, 2);
    ast->addTypeInfo(scopeId,"i32", AST_INT32, 4);
    ast->addTypeInfo(scopeId,"i64", AST_INT64, 8);
    ast->addTypeInfo(scopeId,"f32", AST_FLOAT32, 4);
    ast->addTypeInfo(scopeId,"bool", AST_BOOL, 1);
    ast->addTypeInfo(scopeId,"char", AST_CHAR, 1);
    ast->addTypeInfo(scopeId,"null", AST_NULL, 8);
    ast->addTypeInfo(scopeId,"var", AST_VAR);
    ast->addTypeInfo(scopeId,"member", AST_MEMBER);
    ast->addTypeInfo(scopeId,"call", AST_FNCALL);

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
void AST::appendToMainBody(ASTBody *body) {
    if (body->enums) {
        mainBody->add(body->enums);
        body->enums = 0;
        body->enumsTail = 0;
    }
    if (body->functions) {
        mainBody->add(body->functions);
        body->functions = 0;
        body->functionsTail = 0;
    }
    if (body->structs) {
        mainBody->add(body->structs);
        body->structs = 0;
        body->structsTail = 0;
    }
    if (body->statements) {
        mainBody->add(body->statements);
        body->statements = 0;
        body->statementsTail = 0;
    }
    destroy(body);
}
ASTBody *AST::createBody() {
    auto ptr = (ASTBody *)engone::Allocate(sizeof(ASTBody));
    new (ptr) ASTBody();
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
    ptr->name = new std::string(name);
    return ptr;
}
ASTEnum *AST::createEnum(const std::string &name) {
    auto ptr = (ASTEnum *)engone::Allocate(sizeof(ASTEnum));
    new (ptr) ASTEnum();
    ptr->name = new std::string(name);
    return ptr;
}
ASTFunction *AST::createFunction(const std::string &name) {
    auto ptr = (ASTFunction *)engone::Allocate(sizeof(ASTFunction));
    new (ptr) ASTFunction();
    ptr->name = new std::string(name);
    return ptr;
}
ASTExpression *AST::createExpression(TypeId type) {
    auto ptr = (ASTExpression *)engone::Allocate(sizeof(ASTExpression));
    new (ptr) ASTExpression();
    ptr->isValue = (u32)type < AST_PRIMITIVE_COUNT;
    ptr->typeId = type;
    return ptr;
}
#define TAIL_ADD(M, S)     \
    if (!M)                \
        M##Tail = M = S;   \
    else                   \
        M##Tail->next = S; \
    while (M##Tail->next)  \
        M##Tail = M##Tail->next;
void ASTBody::add(ASTStruct *astStruct) {
    TAIL_ADD(structs, astStruct)
}
void ASTBody::add(ASTStatement *astStatement) {
    TAIL_ADD(statements, astStatement)
    // if(!statements)
    //     statementsTail = statements = astStatement;
    // else
    //     statementsTail->next = astStatement;
    // while(statementsTail->next)
    //     statementsTail = statementsTail->next;
}
void ASTBody::add(ASTFunction *astFunction) {
    TAIL_ADD(functions, astFunction)
}
void ASTBody::add(ASTEnum *astEnum) {
    TAIL_ADD(enums, astEnum)
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
    nextTypeIdId = NEXT_ID;
    nextPointerTypeId = POINTER_BIT;
    nextSliceTypeId = SLICE_BIT;

    destroy(mainBody);
    mainBody = 0;
}

void AST::destroy(ASTBody *body) {
    if (body->structs)
        destroy(body->structs);
    if (body->enums)
        destroy(body->enums);
    if (body->functions)
        destroy(body->functions);
    if (body->statements)
        destroy(body->statements);

    body->~ASTBody();
    engone::Free(body, sizeof(ASTBody));
}
void AST::destroy(ASTStruct *astStruct) {
    if (astStruct->next)
        destroy(astStruct->next);
    if (astStruct->name)
        delete astStruct->name;
    for (auto &mem : astStruct->members) {
        if (mem.defaultValue)
            destroy(mem.defaultValue);
    }
    astStruct->~ASTStruct();
    engone::Free(astStruct, sizeof(ASTStruct));
}
void AST::destroy(ASTFunction *function) {
    if (function->next)
        destroy(function->next);
    if (function->name)
        delete function->name;
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
    if (astEnum->name)
        delete astEnum->name;
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
int TypeInfo::size() {
    if (astStruct)
        return astStruct->size;
    return _size;
}
int TypeInfo::alignedSize() {
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
void AST::addTypeInfo(ScopeId scopeId, const std::string &name, TypeId id, int size) {
    auto ptr = (TypeInfo *)engone::Allocate(sizeof(TypeInfo));
    new (ptr) TypeInfo{name, id, size};
    ptr->scopeId = scopeId;
    auto scope = getScopeInfo(scopeId);
    Assert(scope);
    
    typeInfos[ptr->id] = ptr;
    scope->nameTypeMap[ptr->name] = ptr;
}
TypeInfo *AST::getTypeInfo(ScopeId scopeId, const std::string &name, bool dontCreate) {
    auto scope = getScopeInfo(scopeId);
    if(!scope) return 0;
    auto basePair = scope->nameTypeMap.find(name);
    if(basePair != scope->nameTypeMap.end()){
        return basePair->second;
    }

    ScopeId nextParent = scope->parent;
    if(scopeId!=globalScopeId){ // global scope does not have parent
        while(true){
            auto parentScope = getScopeInfo(nextParent);
            if(!parentScope)
                break;

            auto pair = parentScope->nameTypeMap.find(name);
            if(pair!=parentScope->nameTypeMap.end())
                return pair->second;
            
            if(nextParent==globalScopeId)
                break;
            nextParent = parentScope->parent;
        }
    }

    if (dontCreate)
        return 0;
    Token tok;
    tok.str = (char *)name.c_str();
    tok.length = name.length();
    if (AST::IsPointer(tok)) {
        auto ptr = (TypeInfo *)engone::Allocate(sizeof(TypeInfo));
        new (ptr) TypeInfo{name, nextPointerTypeId++, 8}; // NOTE: fixed pointer size of 8
        ptr->scopeId = scopeId;
        scope->nameTypeMap[name] = ptr;
        typeInfos[ptr->id] = ptr;
        return ptr;
    } else if (AST::IsSlice(tok)) {
        // TODO: slice should exist in global scope so use that 
        //  instead of scopeId to save some time.
        return getTypeInfo(scopeId,"Slice");
        // return (typeInfos[name] = new TypeInfo{nextSliceTypeId++, 16}); // NOTE: fixed pointer size of 8
    } 
    auto ptr = (TypeInfo *)engone::Allocate(sizeof(TypeInfo));
    new (ptr) TypeInfo{name, nextTypeIdId++};
    ptr->scopeId = scopeId;
    scope->nameTypeMap[name] = ptr;
    typeInfos[ptr->id] = ptr;
    return ptr;
}
TypeInfo *AST::getTypeInfo(TypeId id) {
    auto pair = typeInfos.find(id);
    if (pair != typeInfos.end()) {
        return pair->second;
    }
    return 0;

    // auto scope = getScopeInfo(scopeId);
    // if(!scope) return 0;
    // auto pair = scope->typeInfos.find(id);
    // if (pair != scope->typeInfos.end()) {
    //     return pair->second;
    // }
    
    // ScopeId nextParent = scope->parent; // 0 means no parent
    // while(nextParent!=0){
    //     auto parentScope = getScopeInfo(nextParent);
    //     if(!parentScope)
    //         break;

    //     auto pair = parentScope->typeInfos.find(id);
    //     if(pair!=parentScope->typeInfos.end())
    //         return pair->second;
        
    //     nextParent = parentScope->parent;
    // }

    // return 0;
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
            return {0, -1};
        }
        int pi = members[index].polyIndex;
        if (pi != -1) {
            int typ = polyTypes[pi];
            return {typ, index};
        }

        return {members[index].typeId, index};
    } else {
        return {0, -1};
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
bool AST::IsPointer(Token &token) {
    if (!token.str || token.length < 2)
        return false;
    return token.str[token.length - 1] == '*';
}
bool AST::IsPointer(TypeId id) {
    return id & POINTER_BIT;
}
bool AST::IsSlice(TypeId id) {
    return id & SLICE_BIT;
}
bool AST::IsSlice(Token &token) {
    if (!token.str || token.length < 3)
        return false;
    return token.str[token.length - 2] == '[' && token.str[token.length - 1] == ']';
}
bool AST::IsInteger(TypeId id) {
    return AST_UINT8 <= id && id <= AST_INT64;
}
bool AST::IsSigned(TypeId id) {
    return AST_INT8 <= id && id <= AST_INT64;
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
void ASTBody::print(AST *ast, int depth) {
    using namespace engone;
    PrintSpace(depth);
    log::out << "Body\n";
    if (structs)
        structs->print(ast, depth + 1);
    if (enums)
        enums->print(ast, depth + 1);
    if (functions)
        functions->print(ast, depth + 1);
    if (statements)
        statements->print(ast, depth + 1);
}
void ASTFunction::print(AST *ast, int depth) {
    using namespace engone;
    PrintSpace(depth);
    log::out << "Func " << *name << " (";
    for (int i = 0; i < (int)arguments.size(); i++) {
        auto &arg = arguments[i];
        auto &dtname = ast->getTypeInfo(arg.typeId)->name;
        log::out << arg.name << ": " << dtname;
        if (i + 1 != (int)arguments.size()) {
            log::out << ", ";
        }
    }
    log::out << ")";
    if (!returnTypes.empty())
        log::out << "->";
    for (auto &ret : returnTypes) {
        auto &dtname = ast->getTypeInfo(ret.typeId)->name;
        log::out << dtname << ", ";
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
    log::out << "Struct " << *name;
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
        auto typeInfo = ast->getTypeInfo(member.typeId);
        if (typeInfo)
            log::out << member.name << ": " << typeInfo->name;
        if (member.defaultValue) {
            log::out << " = ";
            member.defaultValue->tokenRange.print();
        }
        if (i + 1 != (int)members.size()) {
            log::out << ", ";
        }
    }
    log::out << " }\n";
    if (next)
        next->print(ast, depth);
}
void ASTEnum::print(AST *ast, int depth) {
    using namespace engone;
    PrintSpace(depth);
    log::out << "Enum " << *name << " {";
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
        auto &dtname = ast->getTypeInfo(typeId)->name;
        if (typeId != 0)
            // log::out << " "<<TypeIdToStr(dataType)<<"";
            log::out << " " << dtname << "";
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
    } else if (type == BODY) {
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
        auto dtname = ast->getTypeInfo(typeId)->name;
        // log::out << "Expr "<<TypeIdToStr(dataType)<<" ";
        log::out << "Expr ";
        // if(dtname)
        log::out << dtname;
        log::out << " ";
        if (typeId == AST_FLOAT32)
            log::out << f32Value;
        else if (typeId >= AST_UINT8 && typeId <= AST_INT64)
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
        log::out << "Expr " << OpToStr(typeId) << " ";
        if (typeId == AST_CAST) {
            log::out << ast->getTypeInfo(castType)->name;
            log::out << "\n";
            left->print(ast, depth + 1);
        } else if (typeId == AST_MEMBER) {
            log::out << *name;
            log::out << "\n";
            left->print(ast, depth + 1);
        } else if (typeId == AST_INITIALIZER) {
            log::out << *name;
            log::out << "\n";
            if(left)
                left->print(ast, depth + 1);
        } else if (typeId == AST_FROM_NAMESPACE) {
            // log::out << *name<<" "<<*member;
            log::out << "\n";
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