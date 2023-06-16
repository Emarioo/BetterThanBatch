#include "BetBat/TypeChecker.h"

#undef ERRT
#undef ERR
// #undef ERRTYPE
#undef ERRTOKENS
#undef TOKENINFO
#undef LOGAT

// Remember to wrap macros in {} when using if or loops
// since macros have multiple statements.
#define ERR() info.errors++;engone::log::out << engone::log::RED <<"TypeError, "
#define ERRT(R) info.errors++;engone::log::out << engone::log::RED <<"TypeError "<<(R.firstToken.line)<<":"<<(R.firstToken.column)<<", "
// #define ERRTYPE(R,LT,RT) ERRT(R) << "Type mismatch "<<info.ast->getTypeInfo(LT)->name<<" - "<<info.ast->getTypeInfo(RT)->name<<" "

#define LOGAT(R) R.firstToken.line <<":"<<R.firstToken.column

#define ERRTOKENS(R) log::out <<log::RED<< "LN "<<R.firstToken.line <<": "; R.print();log::out << "\n";

#define TOKENINFO(R) {std::string temp="";R.feed(temp);info.code->addDebugText(std::string("Ln ")+std::to_string(R.firstToken.line)+ ": ");info.code->addDebugText(temp+"\n");}

int CheckEnums(CheckInfo& info, ASTScope* scope){
    using namespace engone;
    Assert(scope);
    MEASURE;
    _TC_LOG(FUNC_ENTER)
    
    int error = true;
    ASTEnum* nextEnum = scope->enums;
    while(nextEnum){
        ASTEnum* aenum = nextEnum;
        nextEnum = nextEnum->next;
        
        TypeInfo* typeInfo = info.ast->createType(Token(aenum->name), scope->scopeId);
        if(typeInfo){
            _TC_LOG(log::out << "Defined enum "<<info.ast->typeToString(typeInfo->id)<<"\n";)
            typeInfo->_size = 4; // i32
            typeInfo->astEnum = aenum;
        } else {
             ERRT(aenum->tokenRange) << aenum->name << " is taken\n";
        }
    }
    
    ASTScope* nextScope = scope->namespaces;
    while(nextScope){
        ASTScope* ascope = nextScope;
        nextScope = nextScope->next;
        
        int result = CheckEnums(info,ascope);
        if(!result)
            error = false;
    }
    
    ASTStatement* nextState = scope->statements;
    while(nextState) {
        ASTStatement* astate = nextState;
        nextState = nextState->next;
        
        if(astate->body){
            int result = CheckEnums(info, astate->body);   
            if(!result)
                error = false;
        }
        if(astate->elseBody){
            int result = CheckEnums(info, astate->elseBody);
            if(!result)
                error = false;
        }
    }
    
    ASTFunction* nextFunc = scope->functions;
    while(nextFunc) {
        ASTFunction* afunc = nextFunc;
        nextFunc = nextFunc->next;
        
        if(afunc->body){
            int result = CheckEnums(info, afunc->body);
            if(!result)
                error = false;
        }
    }
    return error;
}
TypeId CheckType(CheckInfo& info, ScopeId scopeId, TypeId typeString, TokenRange& tokenRange, bool* printedError);
TypeId CheckType(CheckInfo& info, ScopeId scopeId, Token typeString, TokenRange& tokenRange, bool* printedError);
bool CheckStructImpl(CheckInfo& info, ASTStruct* astStruct, TypeInfo* structInfo, StructImpl* structImpl){
    using namespace engone;
    _TC_LOG(FUNC_ENTER)
    int offset=0;
    int alignedSize=0; // offset will be aligned to match this at the end
    /* This case is dealt with using biggestType
    struct A {
        a: bool,
        a: i64,
        b: bool,
    }
    sizeof A = 24
    */
    bool success = true;
    log::out << "Check struct impl "<<info.ast->typeToString(structInfo->id)<<"\n";
    //-- Check members
    for (int i = 0;i<(int)astStruct->members.size();i++) {
        auto& member = astStruct->members[i];
        auto& baseMem = astStruct->baseImpl.members[i];
        auto& implMem = structImpl->members[i];

        if(baseMem.typeId.isString()){
            bool printedError = false;
            TypeId tid = CheckType(info, astStruct->scopeId, baseMem.typeId, astStruct->tokenRange, &printedError);
            if(!tid.isValid() && !printedError){
                if(info.showErrors) {
                    ERRT(astStruct->tokenRange) << "type "<< info.ast->getTokenFromTypeString(implMem.typeId) << " in "<< astStruct->name<<"."<<member.name << " is not a type\n";
                }
                success = false;
                implMem.typeId = tid;
                break;
            }
            implMem.typeId = tid;
            _TC_LOG(log::out << " checked member["<<i<<"] "<<info.ast->typeToString(tid)<<"\n";)
        }
    }
    for (int i = 0;i<(int)astStruct->members.size();i++) {
        auto& member = astStruct->members[i];
        auto& implMem = structImpl->members[i];

        i32 size = info.ast->getTypeSize(implMem.typeId);
        i32 asize = info.ast->getTypeAlignedSize(implMem.typeId);
        // log::out << " "<<typeInfo->name << " "<<typeInfo->size()<<"\n";
        if(size==0 || asize == 0){
            // Assert(size != 0 && asize != 0);
            // astStruct->state = ASTStruct::TYPE_ERROR;
            if(implMem.typeId == structInfo->id){
                if(info.showErrors) {
                    // NOTE: pointers to the same struct are okay.
                    ERRT(astStruct->tokenRange) << "member "<<member.name << "'s type uses the parent struct. (recursive struct does not work)\n";
                }
                // TODO: phrase the error message in a better way
                // TOOD: print some column and line info.
                // TODO: message is printed twice
                // log::out << log::RED <<"Member "<< member.name <<" in struct "<<*astStruct->name<<" cannot be it's own type\n";
            } else {
                // astStruct->state = ASTStruct::TYPE_ERROR;
                if(info.showErrors) {
                    ERRT(astStruct->tokenRange) << "type "<< info.ast->typeToString(implMem.typeId) << " in "<< astStruct->name<<"."<<member.name << " is not a type\n";
                }
            }
            success = false;
            break;
        }
        if(alignedSize<asize)
            alignedSize = asize;

        // int misalign = offset % size;
        // if(misalign!=0){
        //     offset+=size-misalign;
        // }
        // if(i-1>=0){
        //     auto& implMema = structImpl->members[i-1];
        //     i32 nsize = info.ast->getTypeSize(implMema.typeId);
        //     // i32 asize = info.ast->getTypeAlignedSize(implMem.typeId);
        //     int misalign = (offset + nsize) % size;
        //     if(misalign!=0){
        //         offset+=size-misalign;
        //     }
        // }
        if(i+1<(int)astStruct->members.size()){
            auto& implMema = structImpl->members[i+1];
            i32 nsize = info.ast->getTypeSize(implMema.typeId);
            // i32 asize = info.ast->getTypeAlignedSize(implMem.typeId);
            int misalign = (offset + size) % nsize;
            if(misalign!=0){
                offset+=misalign;
            }
        }
        
        _TC_LOG(log::out << " "<<offset<<": "<<member.name<<" ("<<size<<" bytes)\n";)
        implMem.offset = offset;
        offset+=size;

        /*
        Current push and pop works by aligning and then pushing.
        To initialize a struct we push the last member first.
        Then we push the first member. If the first member
        is bigger we must align it before pushing. The old
        way does not take this into account. It begins
        with the first member instead of the last
        and aligns incorrectly.
        What I might do is change something so that
        the offsets become {8, 0} instead of {12, 0}

            struct { char[8]; char[4] }
            offset 12  :  push 4    - push last member
                            addi sp 4 - align
            offset 0   :  push 8    - push first member
            * pointer to struct begins here
        */
        if(i+1<(int)astStruct->members.size()){
            auto& implMema = structImpl->members[i+1];
            i32 nsize = info.ast->getTypeSize(implMema.typeId);
            // i32 asize = info.ast->getTypeAlignedSize(implMem.typeId);
            int misalign = (offset + nsize) % size;
            if(misalign!=0){
                offset+=size-misalign;
            }
        }
    }

    // if (polymorphic){
    //     _TC_LOG(log::out << astStruct->name << " was polymorphic and cannot be evaluated yet\n";)
    // }else {
        // align the end so the struct can be stacked together
    if(offset != 0){
        structImpl->alignedSize = alignedSize;
        int misalign = offset % alignedSize;
        if(misalign!=0){
            offset+=alignedSize-misalign;
        }
    }
    structImpl->size = offset;
    // _TC_LOG(log::out << info.ast->typeToString << " was evaluated to "<<offset<<" bytes\n";)
    // }
    return success;
}
TypeId CheckType(CheckInfo& info, ScopeId scopeId, TypeId typeString, TokenRange& tokenRange, bool* printedError){
    using namespace engone;
    Assert(typeString.isString())
    if(!typeString.isString()) {
        _TC_LOG(log::out << "check type typeid wasn't string type\n";)
        return typeString;
    }
    Token token = info.ast->getTokenFromTypeString(typeString);
    return CheckType(info, scopeId, token, tokenRange, printedError);
}
TypeId CheckType(CheckInfo& info, ScopeId scopeId, Token typeString, TokenRange& tokenRange, bool* printedError){
    using namespace engone;
    _TC_LOG(FUNC_ENTER)
    _TC_LOG(log::out << "check "<<typeString<<"\n";)

    TypeId typeId = {};

    Token ns={};
    u32 plevel=0;
    std::vector<Token> polyTypes;
    Token noPointers = AST::TrimPointer(typeString, &plevel);
    Token baseType = AST::TrimPolyTypes(noPointers, &polyTypes);
    Token typeName = AST::TrimNamespace(noPointers, &ns);
    std::vector<TypeId> polyIds;
    std::string* realTypeName = nullptr;

    if(polyTypes.size() != 0) {
        realTypeName = info.ast->createString();
        *realTypeName += baseType;
        *realTypeName += "<";
        // TODO: Virtual poly arguments does not work with multithreading. Or you need mutexes at least.
        for(int i=0;i<(int)polyTypes.size();i++){
            // TypeInfo* polyInfo = info.ast->convertToTypeInfo(polyTypes[i], scopeId);
            // TypeId id = info.ast->convertToTypeId(polyTypes[i], scopeId);
            bool printedError = false;
            TypeId id = CheckType(info, scopeId, polyTypes[i], tokenRange, &printedError);
            if(i!=0)
                *realTypeName += ",";
            *realTypeName += info.ast->typeToString(id);
            polyIds.push_back(id);
            if(id.isValid()){
            //     baseInfo->astStruct->polyArgs[i].virtualType->id = id;
            } else if(!printedError) {
                ERRT(tokenRange) << "Type for polymorphic argument was not valid\n";
                ERRTOKENS(tokenRange);
            }
            // baseInfo->astStruct->polyArgs[i].virtualType->id = polyInfo->id;
        }
        *realTypeName += ">";
        typeId = info.ast->convertToTypeId(*realTypeName, scopeId);
    } else {
        typeId = info.ast->convertToTypeId(typeName, scopeId);
    }

    if(typeId.isValid()) {
        auto ti = info.ast->getTypeInfo(typeId.baseType());
        Assert(ti);
        if(!ti)
            // TODO: log error here if Assert is removed
            return {};
        if(ti && ti->astStruct && ti->astStruct->polyArgs.size() != 0 && !ti->structImpl) {
            ERRT(tokenRange) << info.ast->typeToString(ti->id)<<" is polymorphic. You must specify poly. types like this: Struct<i32>\n";
            if(printedError)
                *printedError = true;
            return {};
        }
        return typeId;
    }


    if(polyTypes.size() == 0) {
        // ERRT(tokenRange) << <<" is polymorphic. You must specify poly. types like this: Struct<i32>\n";
        return {}; // type isn't polymorphic and does just not exist
    }
    TypeInfo* baseInfo = info.ast->convertToTypeInfo(baseType, scopeId);
    if(!baseInfo) {
        // ERRT(tokenRange) << info.ast->typeToString(ti->id)<<" is polymorphic. You must specify poly. types like this: Struct<i32>\n";
        return {}; // print error? base type for polymorphic type doesn't exist
    }
    if(polyTypes.size() != baseInfo->astStruct->polyArgs.size()) {
        ERR() << "Polymorphic type "<<typeString << " has "<< polyTypes.size() <<" poly. args but the base type "<<info.ast->typeToString(baseInfo->id)<<" needs "<<baseInfo->astStruct->polyArgs.size()<< "\n";
        if(printedError)
            *printedError = true;
        return {}; // type isn't polymorphic and does just not exist
    }



    for(int i=0;i<(int)polyIds.size();i++){
        // TypeInfo* polyInfo = info.ast->convertToTypeInfo(polyTypes[i], scopeId);
        // TypeId id = info.ast->convertToTypeId(polyTypes[i], scopeId);
        // bool printedError = false;
        TypeId id = polyIds[i];
        // CheckType(info, scopeId, polyTypes[i], tokenRange, &printedError);
        // if(i!=0)
            // *realTypeName += ",";
        // *realTypeName += info.ast->typeToString(id);
        // polyIds.push_back(id);
        if(id.isValid()){
            baseInfo->astStruct->polyArgs[i].virtualType->id = id;
        } 
        // else if(!printedError) {
        //     ERRT(tokenRange) << "Type for polymorphic argument was not valid\n";
        //     ERRTOKENS(tokenRange);
        // }
        // baseInfo->astStruct->polyArgs[i].virtualType->id = polyInfo->id;
    }


    TypeInfo* typeInfo = info.ast->createType(*realTypeName, baseInfo->scopeId);
    typeInfo->astStruct = baseInfo->astStruct;
    typeInfo->structImpl = (StructImpl*)engone::Allocate(sizeof(StructImpl));
    new(typeInfo->structImpl)StructImpl();
    typeInfo->structImpl->members.resize(typeInfo->astStruct->members.size());

    bool hm = CheckStructImpl(info, typeInfo->astStruct, baseInfo, typeInfo->structImpl);
    if(!hm) {
        ERR() << __FUNCTION__ <<": structImpl for type "<<typeString << " failed\n";
    } else {
        _TC_LOG(log::out << typeString << " was evaluated to "<<typeInfo->structImpl->size<<" bytes\n";)
    }
    TypeId outId = typeInfo->id;
    outId.setPointerLevel(plevel);
    return outId;
}
int CheckStructs(CheckInfo& info, ASTScope* scope) {
    using namespace engone;
    MEASURE;
    _TC_LOG(FUNC_ENTER)
    //-- complete structs

    ASTScope* nextNamespace = scope->namespaces;
    while(nextNamespace) {
        ASTScope* now = nextNamespace;
        nextNamespace = nextNamespace->next;
        
        CheckStructs(info, now);
    }
   
    // TODO @Optimize: The code below doesn't need to run if the structs are complete.
    //   We can skip directly to going through the closed scopes further down.
    //   How to keep track of all this is another matter.
    ASTStruct* nextStruct=scope->structs;
    while(nextStruct){
        ASTStruct* astStruct = nextStruct;
        nextStruct = nextStruct->next;
        
        //-- Get struct info
        TypeInfo* structInfo = 0;
        if(astStruct->state==ASTStruct::TYPE_EMPTY){
            // structInfo = info.ast->getTypeInfo(scope->scopeId, astStruct->name,false,true);
            structInfo = info.ast->createType(Token(astStruct->name), scope->scopeId);
            if(!structInfo){
                astStruct->state = ASTStruct::TYPE_ERROR;
                ERRT(astStruct->tokenRange) << astStruct->name<<" is already defined\n";
                // TODO: Provide information (file, line, column) of the first definition.
            } else {
                log::out << "Defined struct "<<info.ast->typeToString(structInfo->id)<<"\n";
                astStruct->state = ASTStruct::TYPE_CREATED;
                structInfo->astStruct = astStruct;
                for(int i=0;i<(int)astStruct->polyArgs.size();i++){
                    auto& arg = astStruct->polyArgs[i];
                    arg.virtualType = info.ast->createType(arg.name, astStruct->scopeId);
                }
            }
        } else if(astStruct->state != ASTStruct::TYPE_ERROR){
            structInfo = info.ast->convertToTypeInfo(Token(astStruct->name), scope->scopeId);   
        }
        
        if(astStruct->state == ASTStruct::TYPE_CREATED){
            Assert(structInfo);

            // log::out << "Evaluating "<<*astStruct->name<<"\n";
            bool yes = true;
            if(astStruct->polyArgs.size()==0){
                yes = CheckStructImpl(info, astStruct,structInfo, &astStruct->baseImpl);
            }
            if(yes){
                astStruct->state = ASTStruct::TYPE_COMPLETE;
                info.anotherTurn=true;
                if(astStruct->polyArgs.size()==0) {
                    _TC_LOG(log::out << astStruct->name << " was evaluated to "<<astStruct->baseImpl.size<<" bytes\n";)
                }
            } else {
                astStruct->state = ASTStruct::TYPE_CREATED;
                info.completedStructs = false;
            }
        }
    }
    
    // Structs in scopes below cannot be used by the above
    // so if the above is false then there isn't a reason to run the
    // below since they might require structs from above.
    if(!info.anotherTurn){
        ASTStatement* nextState = scope->statements;
        while(nextState) {
            ASTStatement* astate = nextState;
            nextState = nextState->next;
            
            if(astate->body){
                int result = CheckStructs(info, astate->body);   
                // if(!result)
                //     error = false;
            }
            if(astate->elseBody){
                int result = CheckStructs(info, astate->elseBody);
                // if(!result)
                //     error = false;
            }
        }
        
        ASTFunction* nextFunc = scope->functions;
        while(nextFunc) {
            ASTFunction* afunc = nextFunc;
            nextFunc = nextFunc->next;
            
            if(afunc->body){
                int result = CheckStructs(info, afunc->body);
                // if(!result)
                //     error = false;
            }
        }
    }
    
    return true;
}
int CheckExpression(CheckInfo& info, ASTScope* scope, ASTExpression* expr){
    using namespace engone;
    MEASURE;
    Assert(scope)
    Assert(expr)
    _TC_LOG(FUNC_ENTER)
    
    if(expr->left)
        CheckExpression(info,scope, expr->left);
    if(expr->right)
        CheckExpression(info,scope, expr->right);
    if(expr->typeId == AST_STRING){
        if(!expr->name){
            ERRT(expr->tokenRange) << "string was null at "<<expr->tokenRange.firstToken<<" (compiler bug)\n";
            ERRTOKENS(expr->tokenRange)
            return false;
        }

        auto pair = info.ast->constStrings.find(*expr->name);
        if(pair == info.ast->constStrings.end()){
            AST::ConstString tmp={};
            tmp.length = expr->name->length();
            info.ast->constStrings[*expr->name] = tmp;
        }
        CheckType(info, scope->scopeId, "Slice<char>", expr->tokenRange, nullptr);
    } else if(expr->typeId == AST_SIZEOF) {
        if(!expr->name){
            ERRT(expr->tokenRange) << "string was null at "<<expr->tokenRange.firstToken<<" (compiler bug)\n";
            ERRTOKENS(expr->tokenRange)
            return false;
        }
        auto ti = CheckType(info, scope->scopeId, *expr->name, expr->tokenRange, nullptr);
        int size = 0;
        if(ti.isValid()){
            size = info.ast->getTypeSize(ti);
        }
        expr->typeId = AST_UINT32;
        expr->i64Value = size;
    }
    if(expr->typeId.isString()){
        bool printedError = false;
        auto ti = CheckType(info, scope->scopeId, expr->typeId, expr->tokenRange, &printedError);
        if (ti.isValid()) {
        } else if(!printedError){
            ERRT(expr->tokenRange) << "type "<< info.ast->getTokenFromTypeString(expr->typeId) << " does not exist\n";
        }
        expr->typeId = ti;
    }
    if(expr->castType.isString()){
        bool printedError = false;
        auto ti = CheckType(info, scope->scopeId, expr->castType, expr->tokenRange, &printedError);
        if (ti.isValid()) {
        } else if(!printedError){
            ERRT(expr->tokenRange) << "type "<<info.ast->getTokenFromTypeString(expr->castType) << " does not exist\n";
        }
        expr->castType = ti;
    }
    
    if(expr->next)
        CheckExpression(info, scope, expr->next);
        
    return true;
}

int CheckRest(CheckInfo& info, ASTScope* scope);
int CheckFunction(CheckInfo& info, ASTScope* scope, ASTFunction* func, ASTStruct* parentStruct){
    MEASURE;
    _TC_LOG(FUNC_ENTER)
    int offset = 0; // offset starts before call frame (fp, pc)
    int firstSize = 0;
    // Based on 8-bit alignment. The last argument must be aligned by it.
    for(int i=0;i<(int)func->arguments.size();i++){
        auto& arg = func->arguments[i];
        // TypeInfo* typeInfo = 0;
        if(arg.typeId.isString()){
            bool printedError = false;
            // Token token = info.ast->getTokenFromTypeString(arg.typeId);
            auto ti = CheckType(info, scope->scopeId, arg.typeId, func->tokenRange, &printedError);
            
            if(ti.isValid()){
            }else if(!printedError) {
                ERRT(func->tokenRange) << info.ast->getTokenFromTypeString(arg.typeId) <<" was void (type doesn't exist or you used void which isn't allowed)\n";
            }
            arg.typeId = ti;

        } else {
            // typeInfo = info.ast->getTypeInfo(arg.typeId);
        }
            
        int size = info.ast->getTypeSize(arg.typeId);
        int asize = info.ast->getTypeAlignedSize(arg.typeId);
        Assert(size != 0 && asize != 0);
        // if(size ==0 || asize == 0)
        //     continue;
        if(i==0)
            firstSize = size;
        
        if((offset%asize) != 0){
            offset += asize - offset%asize;
        }
        arg.offset = offset;
        // log::out << " Arg "<<arg.offset << ": "<<arg.name<<" ["<<size<<"]\n";

        offset += size;
    }
    int diff = offset%8;
    if(diff!=0)
        offset += 8-diff; // padding to ensure 8-bit alignment

    // log::out << "total size "<<offset<<"\n";
    // reverse
    for(int i=0;i<(int)func->arguments.size();i++){
        auto& arg = func->arguments[i];
        // TypeInfo* typeInfo = info.ast->getTypeInfo(arg.typeId);
        int size = info.ast->getTypeSize(arg.typeId);
        arg.offset = offset - arg.offset - size;
        // log::out << " Reverse Arg "<<arg.offset << ": "<<arg.name<<"\n";
    }
    func->argSize = offset;

    // return values should also have 8-bit alignment but since the call frame already
    // is aligned there is no need for any special stuff here.
    //(note that the special code would exist where functions are generated and not here)
    offset = 0;
    for(int i=0;i<(int)func->returnTypes.size();i++){
        auto& ret = func->returnTypes[i];
        // TypeInfo* typeInfo = 0;
        if(ret.typeId.isString()){
            bool printedError = false;
            auto ti = CheckType(info, scope->scopeId, ret.typeId, func->tokenRange, &printedError);
            if(ti.isValid()){
            }else if(!printedError) {
                // TODO: fix location?
                ERRT(func->tokenRange) << info.ast->getTokenFromTypeString(ret.typeId)<<" is not a type (function)\n";
            }
            ret.typeId = ti;
            
        } else {
            // typeInfo = info.ast->getTypeInfo(ret.typeId);
        }
        int size = info.ast->getTypeSize(ret.typeId);
        int asize = info.ast->getTypeAlignedSize(ret.typeId);
        Assert(size != 0 && asize != 0);
        
        if ((-offset)%asize != 0){
            offset -= asize - (-offset)%asize;
        }
        offset -= size; // size included in the offset going negative on the stack
        ret.offset = offset;
        // log::out << " Ret "<<ret.offset << ": ["<<size<<"]\n";
    }
    func->returnSize = -offset;

    CheckRest(info, func->body);

    // func->next is checked in CheckRest
    return true;
}
int CheckRest(CheckInfo& info, ASTScope* scope){
    MEASURE;
    _TC_LOG(FUNC_ENTER)
    // arguments
    ASTFunction* nextFunc = scope->functions;
    while(nextFunc){
        ASTFunction* func = nextFunc;
        nextFunc = nextFunc->next;
        bool yes = CheckFunction(info, scope, func, nullptr);

        Identifier* id = info.ast->addFunction(scope->scopeId, func);
        if (!id) {
            ERRT(func->tokenRange) << func->name << " is already defined\n";
        }
        // (void)info.ast->addFunction(scope->scopeId,func);
    }

    ASTStruct* nextStruct = scope->structs;
    while(nextStruct) {
        ASTStruct * now = nextStruct;
        nextStruct = nextStruct->next;

        ASTFunction* nextFunc = now->functions;
        while(nextFunc) {
            ASTFunction* func = nextFunc;
            nextFunc = nextFunc->next;

            CheckFunction(info, scope, func, now);
            // NOTE: Not using addFunction here since the function belongs to the struct
            //   and not any scope.
        }
    }

    // TODO: Type check default values in structs and functions

    ASTStatement* next = scope->statements;
    while(next){
        ASTStatement* now = next;
        next = next->next;

        if(now->lvalue)
            CheckExpression(info, scope ,now->lvalue);
        if(now->rvalue)
            CheckExpression(info, scope, now->rvalue);
            
        if(now->typeId.isString()){
            bool printedError = false;
            auto ti = CheckType(info, scope->scopeId, now->typeId, now->tokenRange, &printedError);
            // NOTE: We don't care whether it's a pointer just that the type exists.
            now->typeId = ti;
            if (!ti.isValid() && !printedError) {
                ERRT(now->tokenRange) << info.ast->getTokenFromTypeString(now->typeId)<<" is not a type (statement)\n";
            } else {
            }
        }

        if(now->body){
            int result = CheckRest(info, now->body);
        }
        if(now->elseBody){
            int result = CheckRest(info, now->elseBody);   
        }
    }
    ASTScope* nextNS = scope->namespaces;
    while(nextNS){
        ASTScope* now = nextNS;
        nextNS = nextNS->next;

        int result = CheckRest(info, now);
    }
    return 0;
}
int TypeCheck(AST* ast, ASTScope* scope){
    using namespace engone;
    MEASURE;
    CheckInfo info = {};
    info.ast = ast;

    _VLOG(log::out << log::BLUE << "Type check:\n";)

    int result = CheckEnums(info, scope);
    
    info.completedStructs=false;
    info.showErrors = false;
    while(!info.completedStructs){
        info.completedStructs = true;
        info.anotherTurn=false;
        
        result = CheckStructs(info, scope);
        
        if(!info.anotherTurn && !info.completedStructs){
            // info.errors += 1;
            if(!info.showErrors){
                info.showErrors = true;
                continue;
            }
            // log::out << log::RED << "Some types could not be evaluated\n";
            return info.errors;
        }
    }
    result = CheckRest(info, scope);
    
    return info.errors;
}