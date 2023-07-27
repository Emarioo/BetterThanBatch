#include "BetBat/TypeChecker.h"
#include "BetBat/Compiler.h"

/*
    Old logging
    Should be deprecated but it's a lot of work
*/
// Remember to wrap macros in {} when using if or loops since macros may have multiple statements.
// #define ERR() info.errors++; engone::log::out << engone::log::RED <<"TypeError: "

#undef ERR_HEAD2
#define ERR_HEAD2(R) info.errors++; engone::log::out << ERR_DEFAULT_R(R,"Type error","E0000")

#undef ERR_HEAD3
#define ERR_HEAD3(R,M) info.errors++; engone::log::out << ERR_DEFAULT_R(R,"Type error","E0000") << M
#undef WARN_HEAD3
#define WARN_HEAD3(R,M) info.compileInfo->warnings++; engone::log::out << WARN_DEFAULT_R(R,"Type warning","W0000") << M

/*
    Latest logging
*/

#undef ERR_SECTION
#define ERR_SECTION(CONTENT) { BASE_SECTION("Type. error, E0000"); CONTENT; }


#ifdef TC_LOG
#define _TC_LOG_ENTER(X) X
// #define _TC_LOG_ENTER(X)
#else
#define _TC_LOG_ENTER(X)
#endif

SignalDefault CheckExpression(CheckInfo& info, ScopeId scopeId, ASTExpression* expr, DynamicArray<TypeId>* outTypes);

SignalDefault CheckEnums(CheckInfo& info, ASTScope* scope){
    using namespace engone;
    Assert(scope);
    MEASURE;
    _TC_LOG_ENTER(FUNC_ENTER)

    SignalDefault error = SignalDefault::SUCCESS;
    for(auto aenum : scope->enums){
        TypeInfo* typeInfo = info.ast->createType(aenum->name, scope->scopeId);
        if(typeInfo){
            _TC_LOG(log::out << "Defined enum "<<info.ast->typeToString(typeInfo->id)<<"\n";)
            typeInfo->_size = 4; // i32
            typeInfo->astEnum = aenum;
        } else {
            ERR_HEAD3(((TokenRange)aenum->name), aenum->name << " is taken.\n";
            )
        }
    }
    
    for(auto it : scope->namespaces){
        SignalDefault result = CheckEnums(info,it);
        if(result != SignalDefault::SUCCESS)
            error = SignalDefault::FAILURE;
    }
    
    for(auto astate : scope->statements) {
        if(astate->firstBody){
            SignalDefault result = CheckEnums(info, astate->firstBody);   
            if(result != SignalDefault::SUCCESS)
                error = SignalDefault::FAILURE;
        }
        if(astate->secondBody){
            SignalDefault result = CheckEnums(info, astate->secondBody);
            if(result != SignalDefault::SUCCESS)
                error = SignalDefault::FAILURE;
        }
    }
    
    for(auto afunc : scope->functions) {
        if(afunc->body){
            SignalDefault result = CheckEnums(info, afunc->body);
            if(result != SignalDefault::SUCCESS)
                error = SignalDefault::FAILURE;
        }
    }
    return error;
}
TypeId CheckType(CheckInfo& info, ScopeId scopeId, TypeId typeString, const TokenRange& tokenRange, bool* printedError);
TypeId CheckType(CheckInfo& info, ScopeId scopeId, Token typeString, const TokenRange& tokenRange, bool* printedError);
SignalDefault CheckStructImpl(CheckInfo& info, ASTStruct* astStruct, TypeInfo* structInfo, StructImpl* structImpl){
    using namespace engone;
    _TC_LOG_ENTER(FUNC_ENTER)
    int offset=0;
    int alignedSize=0; // offset will be aligned to match this at the end

    Assert(astStruct->polyArgs.size() == structImpl->polyArgs.size());
    DynamicArray<TypeId> prevVirtuals{};
    prevVirtuals.resize(astStruct->polyArgs.size());
    for(int i=0;i<(int)astStruct->polyArgs.size();i++){
        TypeId id = structImpl->polyArgs[i];
        if(info.errors == 0){
            // Assert(id.isValid()); // poly arg type could have failed, an error would have been logged
        }
        prevVirtuals[i] = astStruct->polyArgs[i].virtualType->id;
        astStruct->polyArgs[i].virtualType->id = id;
    }
    defer{
        for(int i=0;i<(int)astStruct->polyArgs.size();i++){
            astStruct->polyArgs[i].virtualType->id = prevVirtuals[i];
        }
    };
   
    structImpl->members.resize(astStruct->members.size());

    bool success = true;
    _TC_LOG(log::out << "Check struct impl "<<info.ast->typeToString(structInfo->id)<<"\n";)
    //-- Check members
    for (int i = 0;i<(int)astStruct->members.size();i++) {
        auto& member = astStruct->members[i];
        // auto& baseMem = astStruct->baseImpl.members[i];
        auto& implMem = structImpl->members[i];

        Assert(member.stringType.isString());
        bool printedError = false;
        TypeId tid = CheckType(info, astStruct->scopeId, member.stringType, astStruct->tokenRange, &printedError);
        if(!tid.isValid() && !printedError){
            if(info.showErrors) {
                ERR_HEAD3(astStruct->tokenRange, "Type '"<< info.ast->getTokenFromTypeString(member.stringType) << "' in "<< astStruct->name<<"."<<member.name << " is not a type.\n\n";
                    ERR_LINE(member.name,"bad type");
                )
            }
            success = false;
            continue;
        }
        implMem.typeId = tid;
        if(member.defaultValue){
            // TODO: Don't check default expression every time. Do check once and store type in AST.
            DynamicArray<TypeId> temp{};
            CheckExpression(info,structInfo->scopeId, member.defaultValue,&temp);
            if(temp.size()==0)
                temp.add(AST_VOID);
            if(!info.ast->castable(implMem.typeId, temp.last())){
                std::string deftype = info.ast->typeToString(temp.last());
                std::string memtype = info.ast->typeToString(implMem.typeId);
                ERR_HEAD3(member.defaultValue->tokenRange, "Type of default value does not match member.\n\n";
                    ERR_LINE(member.defaultValue->tokenRange,deftype.c_str());
                    ERR_LINE(member.name,memtype.c_str());
                )
                continue; // continue when failing
            }
        }
        _TC_LOG(log::out << " checked member["<<i<<"] "<<info.ast->typeToString(tid)<<"\n";)
    }
    if(!success){
        return SignalDefault::FAILURE;
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
                    ERR_HEAD2(astStruct->tokenRange) << "member "<<member.name << "'s type uses the parent struct. (recursive struct does not work)\n";
                }
                // TODO: phrase the error message in a better way
                // TOOD: print some column and line info.
                // TODO: message is printed twice
                // log::out << log::RED <<"Member "<< member.name <<" in struct "<<*astStruct->name<<" cannot be it's own type\n";
            } else {
                // astStruct->state = ASTStruct::TYPE_ERROR;
                if(info.showErrors) {
                    ERR_HEAD2(astStruct->tokenRange) << "type "<< info.ast->typeToString(implMem.typeId) << " in "<< astStruct->name<<"."<<member.name << " is not a type\n";
                }
            }
            success = false;
            continue;
        }
        if(alignedSize<asize)
            alignedSize = asize > 8 ? 8 : asize;

        if(i+1<(int)astStruct->members.size()){
            auto& implMema = structImpl->members[i+1];
            i32 nextSize = info.ast->getTypeSize(implMema.typeId); // TODO: Use getTypeAlignedSize instead?
            nextSize = nextSize>8?8:nextSize;
            // i32 asize = info.ast->getTypeAlignedSize(implMem.typeId);
            int misalign = (offset + size) % nextSize;
            if(misalign!=0){
                offset+=nextSize-misalign;
            }
        } else {
            int misalign = (offset + size) % alignedSize;
            if(misalign!=0){
                offset+=alignedSize-misalign;
            }
        }
        
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
    }

    if(offset != 0){
        structImpl->alignedSize = alignedSize;
        int misalign = offset % alignedSize;
        if(misalign!=0){
            offset+=alignedSize-misalign;
        }
    }
    structImpl->size = offset;
    std::string polys = "";
    if(structImpl->polyArgs.size()!=0){
        polys+="<";
        for(int i=0;i<(int)structImpl->polyArgs.size();i++){
            if(i!=0) polys+=",";
            polys += info.ast->typeToString(structImpl->polyArgs[i]);
        }
        polys+=">";
    }
    _VLOG(log::out << "Struct "<<log::LIME << astStruct->name<<polys<<log::SILVER<<" (size: "<<structImpl->size <<(astStruct->isPolymorphic()?", poly. impl.":"")<<", scope: "<<info.ast->getScope(astStruct->scopeId)->parent<<")\n";)
    for(int i=0;i<(int)structImpl->members.size();i++){
        auto& name = astStruct->members[i].name;
        auto& mem = structImpl->members[i];
        
        i32 size = info.ast->getTypeSize(mem.typeId);
        _VLOG(log::out << " "<<mem.offset<<": "<<name<<" ("<<size<<" bytes)\n";)
    }
    // _TC_LOG(log::out << info.ast->typeToString << " was evaluated to "<<offset<<" bytes\n";)
    // }
    if(success)
        return SignalDefault::SUCCESS;
    return SignalDefault::FAILURE;
}
TypeId CheckType(CheckInfo& info, ScopeId scopeId, TypeId typeString, const TokenRange& tokenRange, bool* printedError){
    using namespace engone;
    Assert(typeString.isString());
    // if(!typeString.isString()) {
    //     _TC_LOG(log::out << "check type typeid wasn't string type\n";)
    //     return typeString;
    // }
    Token token = info.ast->getTokenFromTypeString(typeString);
    return CheckType(info, scopeId, token, tokenRange, printedError);
}
TypeId CheckType(CheckInfo& info, ScopeId scopeId, Token typeString, const TokenRange& tokenRange, bool* printedError){
    using namespace engone;
    MEASURE;
    _TC_LOG_ENTER(FUNC_ENTER)
    _TC_LOG(log::out << "check "<<typeString<<"\n";)

    TypeId typeId = {};

    u32 plevel=0;
    std::vector<Token> polyTokens;
    // TODO: namespace?
    Token typeName = AST::TrimPointer(typeString, &plevel);
    Token baseType = AST::TrimPolyTypes(typeName, &polyTokens);
    // Token typeName = AST::TrimNamespace(noPointers, &ns);
    std::vector<TypeId> polyArgs;
    std::string* realTypeName = nullptr;

    if(polyTokens.size() != 0) {
        // We trim poly types and then put them back together to get the "official" names for the types
        // Maybe you used some aliasing or namespaces.
        realTypeName = info.ast->createString();
        *realTypeName += baseType;
        *realTypeName += "<";
        // TODO: Virtual poly arguments does not work with multithreading. Or you need mutexes at least.
        for(int i=0;i<(int)polyTokens.size();i++){
            // TypeInfo* polyInfo = info.ast->convertToTypeInfo(polyTokens[i], scopeId);
            // TypeId id = info.ast->convertToTypeId(polyTokens[i], scopeId);
            bool printedError = false;
            TypeId id = CheckType(info, scopeId, polyTokens[i], tokenRange, &printedError);
            if(i!=0)
                *realTypeName += ",";
            *realTypeName += info.ast->typeToString(id);
            polyArgs.push_back(id);
            if(id.isValid()){

            } else if(!printedError) {
                // ERR_HEAD3(tokenRange, "Type '"<<info.ast->typeToString(id)<<"' for polymorphic argument was not valid.\n\n";
                ERR_HEAD3(tokenRange, "Type '"<<polyTokens[i]<<"' for polymorphic argument was not valid.\n\n";
                    ERR_LINE(tokenRange,"here somewhere");
                )
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
            ERR_HEAD3(tokenRange, info.ast->typeToString(ti->id)<<" is polymorphic. You must specify poly. types like this: Struct<i32>. (if in a function: compiler does not have knowledge of where)\n\n";
            ERR_LINE(tokenRange,"");
            )
            if(printedError)
                *printedError = true;
            return {};
        }
        typeId.setPointerLevel(plevel);
        return typeId;
    }

    if(polyTokens.size() == 0) {
        // ERR_HEAD2(tokenRange) << <<" is polymorphic. You must specify poly. types like this: Struct<i32>\n";
        return {}; // type isn't polymorphic and does just not exist
    }
    TypeInfo* baseInfo = info.ast->convertToTypeInfo(baseType, scopeId);
    if(!baseInfo) {
        // ERR_HEAD2(tokenRange) << info.ast->typeToString(ti->id)<<" is polymorphic. You must specify poly. types like this: Struct<i32>\n";
        return {}; // print error? base type for polymorphic type doesn't exist
    }
    if(polyTokens.size() != baseInfo->astStruct->polyArgs.size()) {
        ERR_SECTION(
            ERR_HEAD(tokenRange)
            ERR_MSG("Polymorphic type "<<typeString << " has "<< (u32)polyTokens.size() <<" poly. args but the base type "<<info.ast->typeToString(baseInfo->id)<<" needs "<<(u32)baseInfo->astStruct->polyArgs.size()<<".")
        )
        // ERR() << "Polymorphic type "<<typeString << " has "<< polyTokens.size() <<" poly. args but the base type "<<info.ast->typeToString(baseInfo->id)<<" needs "<<baseInfo->astStruct->polyArgs.size()<< "\n";
        if(printedError)
            *printedError = true;
        return {}; // type isn't polymorphic and does just not exist
    }

    TypeInfo* typeInfo = info.ast->createType(*realTypeName, baseInfo->scopeId);
    typeInfo->astStruct = baseInfo->astStruct;
    typeInfo->structImpl = baseInfo->astStruct->createImpl();

    typeInfo->structImpl->polyArgs.resize(polyArgs.size());
    for(int i=0;i<(int)polyArgs.size();i++){
        TypeId id = polyArgs[i];
        typeInfo->structImpl->polyArgs[i] = id;
    }

    SignalDefault hm = CheckStructImpl(info, typeInfo->astStruct, baseInfo, typeInfo->structImpl);
    if(hm != SignalDefault::SUCCESS) {
        ERR_SECTION(
            ERR_HEAD(tokenRange)
            ERR_MSG(__FUNCTION__ <<": structImpl for type "<<typeString << " failed.")
        )
    } else {
        _TC_LOG(log::out << typeString << " was evaluated to "<<typeInfo->structImpl->size<<" bytes\n";)
    }
    TypeId outId = typeInfo->id;
    outId.setPointerLevel(plevel);
    return outId;
}
SignalDefault CheckStructs(CheckInfo& info, ASTScope* scope) {
    using namespace engone;
    MEASURE;
    _TC_LOG_ENTER(FUNC_ENTER)
    //-- complete structs

    for(auto it : scope->namespaces) {
        CheckStructs(info, it);
    }
   
    // TODO: @Optimize The code below doesn't need to run if the structs are complete.
    //   We can skip directly to going through the closed scopes further down.
    //   How to keep track of all this is another matter.
    for(auto astStruct : scope->structs){
        //-- Get struct info
        TypeInfo* structInfo = 0;
        if(astStruct->state==ASTStruct::TYPE_EMPTY){
            // structInfo = info.ast->getTypeInfo(scope->scopeId, astStruct->name,false,true);
            structInfo = info.ast->createType(astStruct->name, scope->scopeId);
            if(!structInfo){
                astStruct->state = ASTStruct::TYPE_ERROR;
                ERR_HEAD3(astStruct->tokenRange, "'"<<astStruct->name<<"' is already defined.\n");
                // TODO: Provide information (file, line, column) of the first definition.
                // We don't care about another turn. We failed but we don't set
                // completedStructs to false since this will always fail.
            } else {
                _TC_LOG(log::out << "Created struct type "<<info.ast->typeToString(structInfo->id)<<" in scope "<<scope->scopeId<<"\n";)
                astStruct->state = ASTStruct::TYPE_CREATED;
                structInfo->astStruct = astStruct;
                for(int i=0;i<(int)astStruct->polyArgs.size();i++){
                    auto& arg = astStruct->polyArgs[i];
                    arg.virtualType = info.ast->createType(arg.name, astStruct->scopeId);
                }
            }
        }
        if(astStruct->state == ASTStruct::TYPE_CREATED){
            if(!structInfo) // TYPE_EMPTY may have set structInfo. No need to do it again.
                structInfo = info.ast->convertToTypeInfo(astStruct->name, scope->scopeId);   
            Assert(structInfo); // compiler bug

            // log::out << "Evaluating "<<*astStruct->name<<"\n";
            bool yes = true;
            if(astStruct->polyArgs.size()==0){
                // Assert(!structInfo->structImpl);
                if(!structInfo->structImpl){
                    structInfo->structImpl = astStruct->createImpl();
                    astStruct->nonPolyStruct = structInfo->structImpl;
                }
                yes = CheckStructImpl(info, astStruct, structInfo, structInfo->structImpl) == SignalDefault::SUCCESS;
                if(!yes){
                    astStruct->state = ASTStruct::TYPE_CREATED;
                    info.completedStructs = false;
                } else {
                    // _TC_LOG(log::out << astStruct->name << " was evaluated to "<<astStruct->baseImpl.size<<" bytes\n";)
                }
            }
            if(yes){
                astStruct->state = ASTStruct::TYPE_COMPLETE;
                info.anotherTurn=true;
            }
        }
    }
    
    // Structs in scopes below cannot be used by the above
    // so if the above is false then there isn't a reason to run the
    // below since they might require structs from above.
    // Is this assumption okay because it makes things not work sometimes.
    // No it's a wrong assumption. It could be a good idea to skip the below
    // if the above fails but we can't assume that based on info.anotherTurn.
    // if(!info.anotherTurn){
        for(auto astate : scope->statements) {
            
            if(astate->firstBody){
                SignalDefault result = CheckStructs(info, astate->firstBody);   
                // if(!result)
                //     error = false;
            }
            if(astate->secondBody){
                SignalDefault result = CheckStructs(info, astate->secondBody);
                // if(!result)
                //     error = false;
            }
        }
        
        for(auto afunc : scope->functions) {
            if(afunc->body){
                SignalDefault result = CheckStructs(info, afunc->body);
                // if(!result)
                //     error = false;
            }
        }
    // }
    return SignalDefault::SUCCESS;
}
SignalDefault CheckFunctionImpl(CheckInfo& info, const ASTFunction* func, FuncImpl* funcImpl, ASTStruct* parentStruct, DynamicArray<TypeId>* outTypes);
SignalAttempt CheckFncall(CheckInfo& info, ScopeId scopeId, ASTExpression* expr, DynamicArray<TypeId>* outTypes, bool operatorOverloadAttempt) {
    using namespace engone;

    #define FNCALL_SUCCESS \
        for(auto& ret : overload->funcImpl->returnTypes) \
            outTypes->add(ret.typeId); \
        if(overload->funcImpl->returnTypes.size()==0)    \
            outTypes->add(AST_VOID);\
        expr->versions_overload[info.currentPolyVersion] = *overload;
    #define FNCALL_FAIL \
        outTypes->add(AST_VOID);
    // fail should perhaps not output void as return type.

    //-- Get poly args from the function call
    DynamicArray<TypeId> fnPolyArgs;
    std::vector<Token> polyTokens;
    Token baseName{};
    
    if(operatorOverloadAttempt) {
        baseName = OpToStr((OperationType)expr->typeId.getId());
    } else {
        baseName = AST::TrimPolyTypes(expr->name, &polyTokens);
        for(int i=0;i<(int)polyTokens.size();i++){
            bool printedError = false;
            TypeId id = CheckType(info, scopeId, polyTokens[i], expr->tokenRange, &printedError);
            fnPolyArgs.add(id);
            // TODO: What about void?
            if(id.isValid()){

            } else if(!printedError) {
                ERR_HEAD3(expr->tokenRange, "Type for polymorphic argument was not valid.\n\n";
                    ERR_LINE(expr->tokenRange,"bad");
                )
            }
        }
    }

    //-- Get essential arguments from the function call

    // DynamicArray<TypeId>& argTypes = expr->versions_argTypes[info.currentPolyVersion];
    DynamicArray<TypeId> argTypes{};
    if(operatorOverloadAttempt){
        DynamicArray<TypeId> tempTypes{};
        SignalDefault resultLeft = CheckExpression(info,scopeId,expr->left,&tempTypes);
        if(tempTypes.size()==0){
            argTypes.add(AST_VOID);
        } else {
            argTypes.add(tempTypes.last());
        }
        tempTypes.resize(0);
        SignalDefault resultRight = CheckExpression(info,scopeId,expr->right,&tempTypes);
        if(tempTypes.size()==0){
            argTypes.add(AST_VOID);
        } else {
            argTypes.add(tempTypes.last());
        }
        if(resultLeft != SignalDefault::SUCCESS || resultRight != SignalDefault::SUCCESS)
            return SignalAttempt::FAILURE;
            // return SignalAttempt::BAD_ATTEMPT;
    } else {
        bool thisFailed=false;
        for(int i = 0; i<(int)expr->args->size();i++){
            auto argExpr = expr->args->get(i);
            Assert(argExpr);

            DynamicArray<TypeId> tempTypes{};
            CheckExpression(info,scopeId,argExpr,&tempTypes);
            if(tempTypes.size()==0){
                argTypes.add(AST_VOID);
            } else {
                if(expr->boolValue && i==0){
                    if(tempTypes.last().getPointerLevel()>1){
                        thisFailed=true;
                    } else {
                        tempTypes.last().setPointerLevel(1);
                    }
                }
                argTypes.add(tempTypes.last());
            }
        }
        // cannot continue if we don't know which struct the method comes from
        if(thisFailed) {
            FNCALL_FAIL
            return SignalAttempt::FAILURE;
        }
    }

    //-- Get identifier, the namespace of overloads for the function/method.
    FnOverloads* fnOverloads = nullptr;
    StructImpl* parentStructImpl = nullptr;
    ASTStruct* parentAstStruct = nullptr;
    if(expr->boolValue){
        Assert(expr->args->size()>0);
        ASTExpression* thisArg = expr->args->get(0);
        TypeId structType = argTypes[0];
        // CheckExpression(info,scope, thisArg, &structType);
        if(structType.getPointerLevel()>1){
            ERR_HEAD3(thisArg->tokenRange, "'"<<info.ast->typeToString(structType)<<"' to much of a pointer.\n";
                ERR_LINE(thisArg->tokenRange,"must be a reference to a struct");
            )
            FNCALL_FAIL
            return SignalAttempt::FAILURE;
        }
        TypeInfo* ti = info.ast->getTypeInfo(structType.baseType());
        
        if(!ti || !ti->astStruct){
            ERR_HEAD3(expr->tokenRange, "'"<<info.ast->typeToString(structType)<<"' is not a struct. Method cannot be called.\n";
                ERR_LINE(thisArg->tokenRange,"not a struct");
            )
            FNCALL_FAIL
            return SignalAttempt::FAILURE;
        }
        if(!ti->getImpl()){
            ERR_HEAD3(expr->tokenRange, "'"<<info.ast->typeToString(structType)<<"' is a polymorphic struct with not poly args specified.\n";
                ERR_LINE(thisArg->tokenRange,"base polymorphic struct");
            )
            FNCALL_FAIL
            return SignalAttempt::FAILURE;
        }
        parentStructImpl = ti->structImpl;
        parentAstStruct = ti->astStruct;
        fnOverloads = &ti->astStruct->getMethod(baseName);
    } else {
        Identifier* iden = info.ast->findIdentifier(scopeId, baseName);
        if(!iden || iden->type != Identifier::FUNC){
            if(operatorOverloadAttempt)
                return SignalAttempt::BAD_ATTEMPT;

            ERR_HEAD3(expr->tokenRange, "Function '"<<baseName <<"' does not exist. Did you forget an import?\n\n";
                ERR_LINE(baseName,"undefined");
            )
            FNCALL_FAIL
            return SignalAttempt::FAILURE;
        }
        fnOverloads = &iden->funcOverloads;
    }
    
    if(fnPolyArgs.size()==0 && (!parentAstStruct || parentAstStruct->polyArgs.size()==0)){
        // match args with normal impls
        FnOverloads::Overload* overload = fnOverloads->getOverload(info.ast, argTypes, expr, fnOverloads->overloads.size()==1 && !operatorOverloadAttempt);
        if(!overload && !operatorOverloadAttempt)
            overload = fnOverloads->getOverload(info.ast, argTypes, expr, true);
        if(overload){
            if(overload->astFunc->body && overload->funcImpl->usages == 0){
                CheckInfo::CheckImpl checkImpl{};
                checkImpl.astFunc = overload->astFunc;
                checkImpl.funcImpl = overload->funcImpl;
                info.checkImpls.add(checkImpl);
            }
            overload->funcImpl->usages++;

            FNCALL_SUCCESS
            return SignalAttempt::SUCCESS;
        }
        if(!operatorOverloadAttempt){
            if(fnOverloads->overloads.size()==0){
                ERR_HEAD3(expr->tokenRange, "'"<<baseName<<"' is not a function/method.\n\n";
                    ERR_LINE(expr->tokenRange, "bad");
                )
            } else {
                // TODO: Implicit call to polymorphic functions. Currently throwing error instead.
                //  Should be done in generator too.
                ERR_HEAD3(expr->tokenRange, "Overloads for function '"<<baseName <<"' does not match these argument(s): ";
                    if(argTypes.size()==0){
                        log::out << "zero arguments";
                    } else {
                        expr->printArgTypes(info.ast, argTypes);
                    }
                    log::out << "\n";
                    if(fnOverloads->polyOverloads.size()!=0){
                        log::out << log::YELLOW<<"(implicit call to polymorphic function is not implemented)\n";
                    }
                    if(expr->args->size()!=0 && expr->args->get(0)->namedValue.str){
                        log::out << log::YELLOW<<"(named arguments cannot identify overloads)\n";
                    }
                    log::out <<"\n";
                    ERR_LINE(expr->tokenRange, "bad");
                    // TODO: show list of available overloaded function args
                ) 
            }
            FNCALL_FAIL
        }
        return SignalAttempt::FAILURE;
        // if implicit polymorphism then
        // macth poly impls
        // generate poly impl if failed
        // use new impl if succeeded.
    }
    // code when we call polymorphic function

    // match args and poly args with poly impls
    // if match then return that impl
    // if not then try to generate a implementation

    FnOverloads::Overload* overload = fnOverloads->getOverload(info.ast, argTypes,fnPolyArgs, expr);
    if(overload){
        overload->funcImpl->usages++;
        FNCALL_SUCCESS
        return SignalAttempt::SUCCESS;
    }
    ASTFunction* polyFunc = nullptr;
    // // Find possible polymorphic type and later create implementation for it
    for(int i=0;i<(int)fnOverloads->polyOverloads.size();i++){
        FnOverloads::PolyOverload& overload = fnOverloads->polyOverloads[i];
        if(overload.astFunc->polyArgs.size() != fnPolyArgs.size())
            continue;// unless implicit polymorphic types
        if(parentAstStruct){
            for(int j=0;j<(int)parentAstStruct->polyArgs.size();j++){
                parentAstStruct->polyArgs[j].virtualType->id = parentStructImpl->polyArgs[j];
            }
        }
        for(int j=0;j<(int)overload.astFunc->polyArgs.size();j++){
            overload.astFunc->polyArgs[j].virtualType->id = fnPolyArgs[j];
        }
        defer {
            for(int j=0;j<(int)overload.astFunc->polyArgs.size();j++){
                overload.astFunc->polyArgs[j].virtualType->id = {};
            }
            if(parentAstStruct){
                for(int j=0;j<(int)parentAstStruct->polyArgs.size();j++){
                    parentAstStruct->polyArgs[j].virtualType->id = {};
                }
            }
        };
        // continue if more args than possible
        // continue if less args than minimally required
        if(expr->nonNamedArgs > overload.astFunc->arguments.size() || 
            expr->nonNamedArgs < overload.astFunc->nonDefaults ||
            argTypes.size() > overload.astFunc->arguments.size()
            )
            continue;
        bool found = true;
        for (u32 j=0;j<expr->nonNamedArgs;j++){
            // log::out << "Arg:"<<info.ast->typeToString(overload.astFunc->arguments[j].stringType)<<"\n";
            TypeId argType = CheckType(info, overload.astFunc->scopeId,overload.astFunc->arguments[j].stringType,
                overload.astFunc->arguments[j].name,nullptr);
            // TypeId argType = CheckType(info, scope->scopeId,overload.astFunc->arguments[j].stringType,
            // log::out << "Arg: "<<info.ast->typeToString(argType)<<" = "<<info.ast->typeToString(argTypes[j])<<"\n";
            if(argType != argTypes[j]){
                found = false;
                break;
            }
        }
        if(found){
            // NOTE: You can break here because there should only be one matching overload.
            // But we keep going in case we find more matches which would indicate
            // a bug in the compiler. An optimised build would not do this.
            if(polyFunc) {
                // log::out << log::RED << __func__ <<" (COMPILER BUG): More than once match!\n";
                Assert(("More than one match!",false));
                // return outFunc;
                break;
            }
            polyFunc = overload.astFunc;
            //break;
        }
    }
    if(!polyFunc){
        ERR_HEAD3(expr->tokenRange, "Overloads for function '"<<baseName <<"' does not match these argument(s): ";
            if(argTypes.size()==0){
                log::out << "zero arguments";
            } else {
                expr->printArgTypes(info.ast, argTypes);
            }
            
            log::out << "\n";
            log::out << log::YELLOW<<"(polymorphic functions could not be generated if there are any)\n";
            log::out <<"\n";
            ERR_LINE(expr->tokenRange, "bad");
            // TODO: show list of available overloaded function args
        )
        FNCALL_FAIL
        return SignalAttempt::FAILURE;
    }
    ScopeInfo* funcScope = info.ast->getScope(polyFunc->scopeId);
    FuncImpl* funcImpl = polyFunc->createImpl();
    funcImpl->name = expr->name;
    funcImpl->structImpl = parentStructImpl;

    funcImpl->polyArgs.resize(fnPolyArgs.size());

    for(int i=0;i<(int)fnPolyArgs.size();i++){
        TypeId id = fnPolyArgs[i];
        funcImpl->polyArgs[i] = id;
    }
    // TODO: What you are calling a struct method?  if (expr->boolvalue) do structmethod
    SignalDefault result = CheckFunctionImpl(info,polyFunc,funcImpl,parentAstStruct, outTypes);

    FnOverloads::Overload* newOverload = fnOverloads->addPolyImplOverload(polyFunc, funcImpl);
    
    CheckInfo::CheckImpl checkImpl{};
    checkImpl.astFunc = polyFunc;
    checkImpl.funcImpl = funcImpl;
    funcImpl->usages++;
    info.checkImpls.add(checkImpl);

    // Can overload be null since we generate a new func impl?
    overload = fnOverloads->getOverload(info.ast, argTypes, fnPolyArgs, expr);
    Assert(overload == newOverload);
    if(!overload){
        ERR_HEAD3(expr->tokenRange, "Specified polymorphic arguments does not match with passed arguments for call to '"<<baseName <<"'.\n";
            log::out << log::CYAN<<"Generates args: "<<log::SILVER;
            if(argTypes.size()==0){
                log::out << "zero arguments";
            }
            for(int i=0;i<(int)funcImpl->argumentTypes.size();i++){
                if(i!=0) log::out << ", ";
                log::out <<info.ast->typeToString(funcImpl->argumentTypes[i].typeId);
            }
            log::out << "\n";
            log::out << log::CYAN<<"Passed args: "<<log::LIME;
            if(argTypes.size()==0){
                log::out << "zero arguments";
            } else {
                expr->printArgTypes(info.ast, argTypes);
            }
            log::out << "\n";
            // TODO: show list of available overloaded function args
        )
        FNCALL_FAIL
        return SignalAttempt::FAILURE;
    } 
    FNCALL_SUCCESS
    return SignalAttempt::SUCCESS;
}
SignalDefault CheckExpression(CheckInfo& info, ScopeId scopeId, ASTExpression* expr, DynamicArray<TypeId>* outTypes){
    using namespace engone;
    MEASURE;
    Assert(expr);
    _TC_LOG_ENTER(FUNC_ENTER)
    
    defer {
        if(outTypes->size()==0){
            outTypes->add(AST_VOID);
        }
    };

    // IMPORTANT NOTE: CheckExpression HAS to run for left and right expressions
    //  since they may require types to be checked. AST_SIZEOF needs to evalute for example

    if(expr->typeId == AST_REFER){
        if(expr->left) {
            CheckExpression(info,scopeId, expr->left, outTypes);
        }
        outTypes->last().setPointerLevel(outTypes->last().getPointerLevel()+1);
    } else if(expr->typeId == AST_DEREF){
        if(expr->left) {
            CheckExpression(info,scopeId, expr->left, outTypes);
        }
        if(outTypes->last().getPointerLevel()==0){
            ERR_HEAD3(expr->left->tokenRange, "Cannot dereference non-pointer.\n\n";
                ERR_LINE(expr->left->tokenRange,"not a pointer");
            )
        }else{
            outTypes->last().setPointerLevel(outTypes->last().getPointerLevel()-1);
        }
    } else if(expr->typeId == AST_MEMBER){
        Assert(expr->left);
        if(expr->left->typeId == AST_ID){
            // TODO: Generator passes idScope. What is it and is it required in type checker?
            // A simple check to see if the identifier in the expr node is an enum type.
            // no need to check for pointers or so.
            TypeInfo *typeInfo = info.ast->convertToTypeInfo(expr->left->name, scopeId);
            if (typeInfo && typeInfo->astEnum) {
                i32 enumValue;
                bool found = typeInfo->astEnum->getMember(expr->name, &enumValue);
                if (!found) {
                    ERR_HEAD3(expr->tokenRange, expr->name << " is not a member of enum " << typeInfo->astEnum->name << "\n";
                    )
                    return SignalDefault::FAILURE;
                }

                outTypes->add(typeInfo->id);
                return SignalDefault::SUCCESS;
            }
        }
        DynamicArray<TypeId> typeArray{};
        // if(expr->left) {
            CheckExpression(info,scopeId, expr->left, &typeArray);
        // }
        outTypes->add(AST_VOID);
        // outType holds the type of expr->left
        TypeInfo* ti = info.ast->getTypeInfo(typeArray.last().baseType());
        Assert(typeArray.last().getPointerLevel()<2);
        if(ti && ti->astStruct){
            TypeInfo::MemberData memdata = ti->getMember(expr->name);
            if(memdata.index!=-1){
                outTypes->last() = memdata.typeId;
            } else {
                std::string msgtype = "not member of "+info.ast->typeToString(typeArray.last());
                ERR_HEAD3(expr->tokenRange, "'"<<expr->name<<"' is not a member in struct '"<<info.ast->typeToString(typeArray.last())<<"'.";
                    log::out << "These are the members: ";
                    for(int i=0;i<(int)ti->astStruct->members.size();i++){
                        if(i!=0)
                            log::out << ", ";
                        log::out << log::LIME << ti->astStruct->members[i].name<<log::SILVER<<": "<<info.ast->typeToString(ti->getMember(i).typeId);
                    }
                    log::out <<"\n";
                    log::out <<"\n";
                    ERR_LINE(expr->name,msgtype.c_str());
                )
                return SignalDefault::FAILURE;
            }
        } else {
            std::string msgtype = info.ast->typeToString(typeArray.last());
            ERR_HEAD3(expr->tokenRange, "Member access only works on structs, pointers to structs, and enums. '" << info.ast->typeToString(typeArray.last()) << "' is neither (astStruct/astEnum were null).\n\n";
                ERR_LINE(expr->tokenRange,msgtype.c_str());
            )
            return SignalDefault::FAILURE;
        }
    } else if(expr->typeId == AST_INDEX) {
        DynamicArray<TypeId> typeArray{};
        if(expr->left) {
            CheckExpression(info,scopeId, expr->left, &typeArray);
            // if(typeArray.size()==0){
            //     typeArray.add(AST_VOID);
            // }
        }
        
        outTypes->add(AST_VOID);
        DynamicArray<TypeId> temp{};
        if(expr->right) {
            CheckExpression(info,scopeId, expr->right, &temp);
        }
        if(typeArray.last().getPointerLevel()==0){
            std::string strtype = info.ast->typeToString(typeArray.last());
            ERR_HEAD3(expr->left->tokenRange, "Cannot index non-pointer.\n\n";
                ERR_LINE(expr->left->tokenRange,strtype.c_str());
            )
        }else{
            outTypes->last() = typeArray.last();
            outTypes->last().setPointerLevel(outTypes->last().getPointerLevel()-1);
        }
    } else if(expr->typeId == AST_ID){
        // NOTE: When changing this code, don't forget to do the same with AST_SIZEOF. It also has code for AST_ID.
        if(IsName(expr->name)){
            // TODO: What about enum?
            auto iden = info.ast->findIdentifier(scopeId, expr->name);
            if(iden){
                if(iden->type == Identifier::VAR){
                    auto varinfo = info.ast->identifierToVariable(iden);
                    if(varinfo){
                        outTypes->add(varinfo->typeId);
                    }
                } else if(iden->type == Identifier::FUNC) {
                    if(iden->funcOverloads.overloads.size() == 1) {
                        auto overload = &iden->funcOverloads.overloads[0];
                        if(overload->astFunc->body && overload->funcImpl->usages == 0){
                            CheckInfo::CheckImpl checkImpl{};
                            checkImpl.astFunc = overload->astFunc;
                            checkImpl.funcImpl = overload->funcImpl;
                            info.checkImpls.add(checkImpl);
                        }
                        overload->funcImpl->usages++;
                        outTypes->add(AST_FUNC_REFERENCE);
                    } else {
                        // overload makes func reference ambiguous
                        outTypes->add(AST_VOID);
                    }
                } else {
                    INCOMPLETE
                }
            } else {
                ERR_HEAD3(expr->tokenRange, "'"<<expr->name<<"' is not declared.\n\n";
                    ERR_LINE(expr->tokenRange,"undeclared");
                )
                return SignalDefault::FAILURE;
            }
        } else {
            // TODO: What to do with type? Return some type object?
        }
    }  else if(expr->typeId == AST_SIZEOF) {
        Assert(expr->left);
        TypeId finalType = {};
        if(expr->left->typeId == AST_ID){
            // AST_ID could result in a type or a variable
            Identifier* iden = nullptr;
            Token& name = expr->left->name;
            // TODO: Handle function pointer type
            // This code may need to update when code for AST_ID does
            if(IsName(name) && (iden = info.ast->findIdentifier(scopeId, name)) && iden->type==Identifier::VAR){
                auto var = info.ast->identifierToVariable(iden);
                Assert(var);
                if(var){
                    finalType = var->typeId;
                }
            } else {
                finalType = CheckType(info, scopeId, name, expr->tokenRange, nullptr);
            }
        } else {
            DynamicArray<TypeId> temps{};
            SignalDefault result = CheckExpression(info, scopeId, expr->left, &temps);
            finalType = temps.size()==0?AST_VOID:temps.last();
        }
        expr->versions_outTypeSizeof[info.currentPolyVersion] = finalType;
        outTypes->add(AST_UINT32);
    } else if(expr->typeId == AST_RANGE){
        DynamicArray<TypeId> temp={};
        if(expr->left) {
            CheckExpression(info,scopeId, expr->left, &temp);
        }
        if(expr->right) {
            CheckExpression(info,scopeId, expr->right, &temp);
        }
        TypeId theType = CheckType(info, scopeId, "Range", expr->tokenRange, nullptr);
        outTypes->add(theType);
    } else if(expr->typeId == AST_STRING){
        u32 index=0;
        auto constString = info.ast->getConstString(expr->name,&index);
        // Assert(constString);
        expr->versions_constStrIndex[info.currentPolyVersion] = index;

        TypeId theType = CheckType(info, scopeId, "Slice<char>", expr->tokenRange, nullptr);
        outTypes->add(theType);
    } else if(expr->typeId == AST_NULL){
        // u32 index=0;
        // auto constString = info.ast->getConstString(expr->name,&index);
        // // Assert(constString);
        // expr->versions_constStrIndex[info.currentPolyVersion] = index;

        // TypeId theType = CheckType(info, scopeId, "Slice<char>", expr->tokenRange, nullptr);
        TypeId theType = AST_VOID;
        theType.setPointerLevel(1);
        outTypes->add(theType);
    }  else if(expr->typeId == AST_NAMEOF) {
        // Assert(expr->left);
        // TypeId finalType = {};
        // if(expr->left->typeId == AST_ID){
        //     // AST_ID could result in a type or a variable
        //     Identifier* iden = nullptr;
        //     Token& name = expr->left->name;
        //     // TODO: Handle function pointer type
        //     // This code may need to update when code for AST_ID does
        //     if(IsName(name) && (iden = info.ast->findIdentifier(scopeId, name)) && iden->type==Identifier::VAR){
        //         auto var = info.ast->identifierToVariable(iden);
        //         Assert(var);
        //         if(var){
        //             finalType = var->typeId;
        //         }
        //     } else {
        //         finalType = CheckType(info, scopeId, name, expr->tokenRange, nullptr);
        //     }
        // } else {
        //     DynamicArray<TypeId> temps{};
        //     int result = CheckExpression(info, scopeId, expr->left, &temps);
        //     finalType = temps.size()==0?AST_VOID:temps.last();
        // }
        // expr->versions_outTypeSizeof[info.currentPolyVersion] = finalType;
        // outTypes->add(AST_UINT32);
        
        // TODO: nameof doesn't use an expression, it uses expr->name, so you cannot
        //   evaluate expressions like sizeof. You may want to change it so nameof works
        //   like sizeof.
        std::string name = "";
        auto ti = CheckType(info, scopeId, expr->name, expr->tokenRange, nullptr);
        if(ti.isValid()){
            name = info.ast->typeToString(ti);
        }else{
            // AST_ID could result in a type or a variable
            Identifier* iden = nullptr;
            // Token& name = expr->name;
            // TODO: Handle function pointer type
            // This code may need to update when code for AST_ID does
            if(IsName(expr->name) && (iden = info.ast->findIdentifier(scopeId, expr->name)) && iden->type==Identifier::VAR){
                auto var = info.ast->identifierToVariable(iden);
                Assert(var);
                if(var){
                    name = info.ast->typeToString(var->typeId);
                    // finalType = var->typeId;
                }
            } else {
                name = expr->name;
                // finalType = CheckType(info, scopeId, name, expr->tokenRange, nullptr);
            }
        }
        u32 index=0;
        auto constString = info.ast->getConstString(name,&index);
        // Assert(constString);
        expr->versions_constStrIndex[info.currentPolyVersion] = index;
        // expr->constStrIndex = index;

        TypeId theType =CheckType(info, scopeId, "Slice<char>", expr->tokenRange, nullptr);;
        outTypes->add(theType);
    } else if(expr->typeId == AST_FNCALL){
        CheckFncall(info,scopeId,expr, outTypes, false);
    } else if(expr->typeId == AST_INITIALIZER) {
        Assert(expr->args);
        for(auto now : *expr->args){
            DynamicArray<TypeId> temp={};
            CheckExpression(info, scopeId, now, &temp);
        }
        auto ti = CheckType(info, scopeId, expr->castType, expr->tokenRange, nullptr);
        outTypes->add(ti);
        expr->versions_castType[info.currentPolyVersion] = ti;
    } else if(expr->typeId == AST_CAST){
        DynamicArray<TypeId> temp{};
        Assert(expr->left);
        CheckExpression(info,scopeId, expr->left, &temp);
        Assert(temp.size()==1);

        Assert(expr->castType.isString());
        bool printedError = false;
        auto ti = CheckType(info, scopeId, expr->castType, expr->tokenRange, &printedError);
        if (ti.isValid()) {
        } else if(!printedError){
            ERR_HEAD3(expr->tokenRange, "Type "<<info.ast->getTokenFromTypeString(expr->castType) << " does not exist.\n";
            )
        }
        outTypes->add(ti);
        expr->versions_castType[info.currentPolyVersion] = ti;
        
        if(!info.ast->castable(temp.last(),ti)){
            std::string strleft = info.ast->typeToString(temp.last());
            std::string strcast = info.ast->typeToString(ti);
            ERR_SECTION(
                ERR_HEAD(expr->tokenRange)
                ERR_MSG("'"<<strleft << "' cannot be casted to '"<<strcast<<"'. Perhaps you can cast to a type that can be casted to the type you want?.")
                ERR_LINE(expr->left->tokenRange,strleft)
                ERR_LINE(expr->tokenRange,strcast)
                ERR_EXAMPLE_TINY("cast<void*> cast<u64> number")
            )
        }
    } else {
        // TODO: You should not be allowed to overload all operators.
        //  Fix some sort of way to limit which ones you can.
        const char* str = OpToStr((OperationType)expr->typeId.getId(), true);
        if(str && expr->left && expr->right) {
            expr->nonNamedArgs = 2; // unless operator overloading
            SignalAttempt result = CheckFncall(info,scopeId,expr, outTypes, true);
            
            if(result == SignalAttempt::SUCCESS)
                return SignalDefault::SUCCESS;

            if(result != SignalAttempt::BAD_ATTEMPT)
                return CastSignal(result);
        }

        // TODO: This code is buggy and doesn't behave as it should for all types.
        //   Not sure what to do about it.
        if(expr->left) {
            CheckExpression(info,scopeId, expr->left, outTypes);
        }
        if(expr->right) {
            DynamicArray<TypeId> temp{};
            // CheckExpression(info,scopeId, expr->right, expr->left?&temp: outType);
            CheckExpression(info,scopeId, expr->right, expr->left?&temp: outTypes);
            // TODO: PRIORITISING LEFT FOR OUTPUT TYPE WONT ALWAYS WORK
            //  Some operations like "2 + ptr" will use i32 as out type when
            //  it should be of pointer type
        }
        Assert(!expr->typeId.isString());
        // I don't think typeId can be a type string but assert to be sure.
        // if it's not then no need to worry about checking type which is done below
        // this would need to be done in generator too if isString is true
        // if(expr->typeId.isString()){
        //     bool printedError = false;
        //     auto ti = CheckType(info, scopeId, expr->typeId, expr->tokenRange, &printedError);
        //     if (ti.isValid()) {
        //     } else if(!printedError){
        //         ERR_HEAD3(expr->tokenRange, "Type "<< info.ast->getTokenFromTypeString(expr->typeId) << " does not exist.\n";
        //         )
        //     }
        //     // expr->typeId = ti;
        //     *outType = ti;
        // }
        Assert(!expr->castType.isValid());
        // castType should be used with AST_CAST or AST_INITIALIZER, which is handled above
        // we don't deal with it here. I don't even think this works with the poly version system
        // if(expr->castType.isString()){
        //     bool printedError = false;
        //     auto ti = CheckType(info, scopeId, expr->castType, expr->tokenRange, &printedError);
        //     if (ti.isValid()) {
        //     } else if(!printedError){
        //         ERR_HEAD3(expr->tokenRange, "Type "<<info.ast->getTokenFromTypeString(expr->castType) << " does not exist.\n";
        //         )
        //     }
        //     // expr->castType = ti; // don't if polymorphic scope
        //     outTypes->add(ti);
        // }
        if(expr->typeId.getId() < AST_TRUE_PRIMITIVES){
            outTypes->add(expr->typeId);
        }
    }
    return SignalDefault::SUCCESS;
}

SignalDefault CheckRest(CheckInfo& info, ASTScope* scope);
// Evaluates types and offset for the given function implementation
// It does not modify ast func
SignalDefault CheckFunctionImpl(CheckInfo& info, const ASTFunction* func, FuncImpl* funcImpl, ASTStruct* parentStruct, DynamicArray<TypeId>* outTypes){
    using namespace engone;
    MEASURE;
    _TC_LOG_ENTER(FUNC_ENTER)
    int offset = 0; // offset starts before call frame (fp, pc)
    int firstSize = 0;

    Assert(funcImpl->polyArgs.size() == func->polyArgs.size());
    for(int i=0;i<(int)funcImpl->polyArgs.size();i++){
        TypeId id = funcImpl->polyArgs[i];
        Assert(id.isValid());
        func->polyArgs[i].virtualType->id = id;
    }
    if(funcImpl->structImpl){
        Assert(parentStruct);
        // Assert funcImpl->structImpl must come from parentStruct
        for(int i=0;i<(int)funcImpl->structImpl->polyArgs.size();i++){
            TypeId id = funcImpl->structImpl->polyArgs[i];
            Assert(id.isValid());
            parentStruct->polyArgs[i].virtualType->id = id;
        }
    }
    defer {
        for(int i=0;i<(int)funcImpl->polyArgs.size();i++){
            func->polyArgs[i].virtualType->id = {};
        }
        if(funcImpl->structImpl){
            Assert(parentStruct);
            for(int i=0;i<(int)funcImpl->structImpl->polyArgs.size();i++){
                TypeId id = funcImpl->structImpl->polyArgs[i];
                parentStruct->polyArgs[i].virtualType->id = {};
            }
        }
    };

    _TC_LOG(log::out << "FUNC IMPL "<< funcImpl->name<<"\n";)

    // TODO: parentStruct argument may not be necessary since this function only calculates
    //  offsets of arguments and return values.

    // TODO: Handle calling conventions

    funcImpl->argumentTypes.resize(func->arguments.size());
    funcImpl->returnTypes.resize(func->returnValues.size());

    // Based on 8-bit alignment. The last argument must be aligned by it.
    for(int i=0;i<(int)func->arguments.size();i++){
        auto& arg = func->arguments[i];
        auto& argImpl = funcImpl->argumentTypes[i];
        if(arg.stringType.isString()){
            bool printedError = false;
            // Token token = info.ast->getTokenFromTypeString(arg.typeId);
            auto ti = CheckType(info, func->scopeId, arg.stringType, func->tokenRange, &printedError);
            
            if(ti == AST_VOID){
                std::string msg = info.ast->typeToString(arg.stringType);
                ERR_HEAD3(func->tokenRange, "Type '"<<msg<<"' is unknown. Did you misspell or forget to declare the type?\n";
                    ERR_LINE(func->arguments[i].name,msg.c_str());
                )
            } else if(ti.isValid()){

            } else if(!printedError) {
                std::string msg = info.ast->typeToString(arg.stringType);
                ERR_HEAD3(func->tokenRange, "Type '"<<msg<<"' is unknown. Did you misspell it or is the compiler messing with you?\n";
                    ERR_LINE(func->arguments[i].name,msg.c_str());
                )
            }
            argImpl.typeId = ti;
        } else {
            argImpl.typeId = arg.stringType;
        }

        if(arg.defaultValue){
            // TODO: Don't check default expression every time. Do check once and store type in AST.
            DynamicArray<TypeId> temp{};
            CheckExpression(info,func->scopeId, arg.defaultValue,&temp);
            if(temp.size()==0)
                temp.add(AST_VOID);
            if(!info.ast->castable(temp.last(),argImpl.typeId)){
            // if(temp.last() != argImpl.typeId){
                std::string deftype = info.ast->typeToString(temp.last());
                std::string argtype = info.ast->typeToString(argImpl.typeId);
                ERR_HEAD3(arg.defaultValue->tokenRange, "Type of default value does not match type of argument.\n\n";
                    ERR_LINE(arg.defaultValue->tokenRange,deftype.c_str());
                    ERR_LINE(arg.name,argtype.c_str());
                )
                continue; // continue when failing
            }
        }
            
        int size = info.ast->getTypeSize(argImpl.typeId);
        int asize = info.ast->getTypeAlignedSize(argImpl.typeId);
        // Assert(size != 0 && asize != 0);
        // Actually, don't continue here. argImpl.offset shouldn't be uninitialized.
        // if(size ==0 || asize == 0) // Probably due to an error which was logged. We don't want to assert and crash the compiler.
        //     continue;
        if(i==0)
            firstSize = size;
        if(asize!=0){
            if((offset%asize) != 0){
                offset += asize - offset%asize;
            }
        }
        argImpl.offset = offset;
        // log::out << " Arg "<<arg.offset << ": "<<arg.name<<" ["<<size<<"]\n";
        offset += size;
    }
    int diff = offset%8;
    if(diff!=0)
        offset += 8-diff; // padding to ensure 8-byte alignment

    // log::out << "total size "<<offset<<"\n";
    // reverse
    for(int i=0;i<(int)func->arguments.size();i++){
        // auto& arg = func->arguments[i];
        auto& argImpl = funcImpl->argumentTypes[i];
        // TypeInfo* typeInfo = info.ast->getTypeInfo(arg.typeId);
        int size = info.ast->getTypeSize(argImpl.typeId);
        argImpl.offset = offset - argImpl.offset - size;
        // log::out << " Reverse Arg "<<arg.offset << ": "<<arg.name<<"\n";
    }
    funcImpl->argSize = offset;

    // return values should also have 8-bit alignment but since the call frame already
    // is aligned there is no need for any special stuff here.
    //(note that the special code would exist where functions are generated and not here)
    offset = 0;
    for(int i=0;i<(int)funcImpl->returnTypes.size();i++){
        auto& retImpl = funcImpl->returnTypes[i];
        auto& retStringType = func->returnValues[i].stringType;
        // TypeInfo* typeInfo = 0;
        if(retStringType.isString()){
            bool printedError = false;
            auto ti = CheckType(info, func->scopeId, retStringType, func->tokenRange, &printedError);
            if(ti.isValid()){
            }else if(!printedError) {
                #ifdef DEBUG
                #define EXTRA_MSG log::out << " ("<<__func__<<")"; 
                #else
                #define EXTRA_MSG
                #endif
                ERR_HEAD3(func->returnValues[i].valueToken, "'"<<info.ast->getTokenFromTypeString(retStringType)<<"' is not a type.";
                    EXTRA_MSG
                    log::out << "\n\n";

                    ERR_LINE(func->returnValues[i].valueToken,"bad");
                )
                #undef EXTRA_MSG
            }
            retImpl.typeId = ti;
        } else {
            retImpl.typeId = retStringType;
            // typeInfo = info.ast->getTypeInfo(ret.typeId);
        }
        int size = info.ast->getTypeSize(retImpl.typeId);
        int asize = info.ast->getTypeAlignedSize(retImpl.typeId);
        // Assert(size != 0 && asize != 0);
        if(size == 0 || asize == 0){ // We don't want to crash the compiler with assert.
            continue;
        }
        
        if ((-offset)%asize != 0){
            offset -= asize - (-offset)%asize;
        }
        offset -= size; // size included in the offset going negative on the stack
        retImpl.offset = offset;
        // log::out << " Ret "<<ret.offset << ": ["<<size<<"]\n";
    }
    diff = (-offset)%8;
    if(diff!=0)
        offset -= 8-diff; // padding to ensure 8-byte alignment

    for(int i=0;i<(int)funcImpl->returnTypes.size();i++){
        auto& ret = funcImpl->returnTypes[i];
        // TypeInfo* typeInfo = info.ast->getTypeInfo(arg.typeId);
        int size = info.ast->getTypeSize(ret.typeId);
        ret.offset = offset - ret.offset - size;
        // log::out << " Reverse Arg "<<arg.offset << ": "<<arg.name<<"\n";
    }
    funcImpl->returnSize = -offset;

    if(funcImpl->returnTypes.size()==0){
        outTypes->add(AST_VOID);
    } else {
        outTypes->add(funcImpl->returnTypes[0].typeId);
    }
    return SignalDefault::SUCCESS;
}
// Ensures that the function identifier exists.
// Adds the overload for the function.
// Checks if an existing overload would collide with the new overload.
SignalDefault CheckFunction(CheckInfo& info, ASTFunction* function, ASTStruct* parentStruct, ASTScope* scope){
    using namespace engone;
    _TC_LOG_ENTER(FUNC_ENTER)
    
    // log::out << "CheckFunction "<<function->name<<"\n";
    if(parentStruct){
        for(int i=0;i<(int)parentStruct->polyArgs.size();i++){
            for(int j=0;j<(int)function->polyArgs.size();j++){
                if(parentStruct->polyArgs[i].name == function->polyArgs[j].name){
                    ERR_HEAD3(function->polyArgs[j].name.range(), "Name for polymorphic argument is already taken by the parent struct.\n\n";
                        ERR_LINE(parentStruct->polyArgs[i].name,"taken");
                        ERR_LINE(function->polyArgs[j].name,"unavailable");
                    )
                }
            }
        }
    }

    // Create virtual types
    for(int i=0;i<(int)function->polyArgs.size();i++){
        auto& arg = function->polyArgs[i];
        arg.virtualType = info.ast->createType(arg.name, function->scopeId);
        // _TC_LOG(log::out << "Virtual type["<<i<<"] "<<arg.name<<"\n";)
        // arg.virtualType->id = AST_POLY;
    }
    // defer {
    //     for(int i=0;i<(int)function->polyArgs.size();i++){
    //         auto& arg = function->polyArgs[i];
    //         arg.virtualType->id = {};
    //     }
    // };
    // _TC_LOG(log::out << "Method/function has polymorphic properties: "<<function->name<<"\n";)
    FnOverloads* fnOverloads = nullptr;
    Identifier* iden = nullptr;
    if(parentStruct){
        fnOverloads = &parentStruct->getMethod(function->name);
    } else {
        iden = info.ast->findIdentifier(scope->scopeId, function->name);
        if(!iden){
            iden = info.ast->addIdentifier(scope->scopeId, function->name);
            iden->type = Identifier::FUNC;
        }
        if(iden->type != Identifier::FUNC){
            ERR_HEAD3(function->tokenRange, "'"<< function->name << "' is already defined as a non-function.\n";
            )
            return SignalDefault::FAILURE;
        }
        fnOverloads = &iden->funcOverloads;
    }
    if(function->polyArgs.size()==0 && (!parentStruct || parentStruct->polyArgs.size() == 0)){
        // The code below is used to 
        // Acquire identifiable arguments
        DynamicArray<TypeId> argTypes{};
        for(int i=0;i<(int)function->arguments.size();i++){
            // info.ast->printTypesFromScope(function->scopeId);

            TypeId typeId = CheckType(info, function->scopeId, function->arguments[i].stringType, function->tokenRange, nullptr);
            // Assert(typeId.isValid());
            if(!typeId.isValid()){
                std::string msg = info.ast->typeToString(function->arguments[i].stringType);
                ERR_SECTION(
                    ERR_HEAD(function->arguments[i].name.range())
                    ERR_MSG("Unknown type '"<<msg<<"' for parameter '"<<function->arguments[i].name<<"'" << ((function->parentStruct && i==0) ? " (this parameter is auto-generated for methods)" : "")<<".")
                    ERR_LINE(function->arguments[i].name.range(),msg.c_str())
                )
                // ERR_HEAD3(function->arguments[i].name.range(),
                //     "Unknown type '"<<msg<<"' for parameter '"<<function->arguments[i].name<<"'";
                //     if(function->parentStruct && i==0){
                //         log::out << " (this parameter is auto-generated for methods)";
                //     }
                //     log::out << ".\n\n";
                //     ERR_LINE(function->arguments[i].name.range(),msg.c_str());
                // )
            }
            argTypes.add(typeId);
        }
        FnOverloads::Overload* outOverload = nullptr;
        for(int i=0;i<(int)fnOverloads->overloads.size();i++){
            auto& overload = fnOverloads->overloads[i];
            if(overload.astFunc->nonDefaults > function->arguments.size() ||
                function->nonDefaults > overload.astFunc->arguments.size())
                continue;
            bool found = true;
            for (u32 j=0;j<function->nonDefaults || j<overload.astFunc->nonDefaults;j++){
                if(overload.funcImpl->argumentTypes[j].typeId != argTypes[j]){
                    found = false;
                    break;
                }
            }
            if(found){
                // return &overload;
                // NOTE: You can return here because there should only be one matching overload.
                // But we keep going in case we find more matches which would indicate
                // a bug in the compiler. An optimised build would not do this.
                if(outOverload) {
                    // log::out << log::RED << __func__ <<" (COMPILER BUG): More than once match!\n";
                    Assert(("More than one match!",false));
                    // return &overload;
                    break;
                }
                outOverload = &overload;
            }
        }
        if(outOverload){
            if(outOverload->astFunc->linkConvention != NATIVE &&
                    function->linkConvention != NATIVE) {
                //  TODO: better error message which shows what and where the already defined variable/function is.
                ERR_HEAD3(function->tokenRange, "Argument types are ambiguous with another overload of function '"<< function->name << "'.\n\n";
                    ERR_LINE(outOverload->astFunc->tokenRange,"existing overload");
                    ERR_LINE(function->tokenRange,"new ambiguous overload");
                )
                // print list of overloads?
            } else {
                // native functions can be defined twice
            }
        } else {
            if(iden && iden->funcOverloads.overloads.size()>0 && function->linkConvention == NATIVE) {
                ERR_SECTION(
                    ERR_HEAD(function->tokenRange)
                    ERR_MSG("There already is an overload of the native function '"<<function->name<<"'.")
                    ERR_LINE(iden->funcOverloads.overloads[0].astFunc->tokenRange, "previous")
                    ERR_LINE(function->tokenRange, "new")
                )
            } else {
                FuncImpl* funcImpl = function->createImpl();
                funcImpl->name = function->name;
                funcImpl->usages = 0;
                fnOverloads->addOverload(function, funcImpl);
                if(parentStruct)
                    funcImpl->structImpl = parentStruct->nonPolyStruct;
                DynamicArray<TypeId> retTypes{}; // @unused
                SignalDefault yes = CheckFunctionImpl(info, function, funcImpl, parentStruct, &retTypes);
                // implementation isn't checked/generated
                // if(function->body){
                //     CheckInfo::CheckImpl checkImpl{};
                //     checkImpl.astFunc = function;
                //     checkImpl.funcImpl = funcImpl;
                //     // checkImpl.scope = scope;
                //     info.checkImpls.add(checkImpl);
                // }
                _TC_LOG(log::out << "ADD OVERLOAD ";funcImpl->print(info.ast, function);log::out<<"\n";)
            }
        }
    } else {
        if(fnOverloads->polyOverloads.size()!=0){
            WARN_HEAD3(function->tokenRange,"Ambiguity for polymorphic overloads is not checked!\n\n";)
        }
        // Base poly overload is added without regard for ambiguity. It's hard to check ambiguity so to it later.
        fnOverloads->addPolyOverload(function);
    }
    return SignalDefault::SUCCESS;
}
SignalDefault CheckFunctions(CheckInfo& info, ASTScope* scope){
    using namespace engone;
    MEASURE;
    _TC_LOG_ENTER(FUNC_ENTER)
    Assert(scope||info.compileInfo->errors!=0);
    if(!scope) return SignalDefault::FAILURE;

    // for(int index = 0; index < scope->contentOrder.size(); index++){
    //     auto& spot = scope->contentOrder[index];
    //     switch(spot.spotType) {
    //         case ASTScope::STATEMENT: {
    //             auto it = scope->statements[spot.index];
    //             if(it->hasNodes()){
    //                 if(it->firstBody){
    //                     SignalDefault result = CheckFunctions(info, it->firstBody);   
    //                 }
    //                 if(it->secondBody){
    //                     SignalDefault result = CheckFunctions(info, it->secondBody);
    //                 }
    //             }
    //         }
    //         break; case ASTScope::NAMESPACE: {
    //             auto it = scope->namespaces[spot.index];
    //             CheckFunctions(info, it);
    //         }
    //         break; case ASTScope::FUNCTION: {
    //             auto it = scope->functions[spot.index];
    //             CheckFunction(info, it, nullptr, scope);
    //             if(it->body){
    //                 SignalDefault result = CheckFunctions(info, it->body);
    //             }
    //         }
    //         break; case ASTScope::STRUCT: {
    //             auto it = scope->structs[spot.index];
    //             for(auto fn : it->functions){
    //                 CheckFunction(info, fn , it, scope);
    //                 if(fn->body){ // external/native function do not have bodies
    //                     SignalDefault result = CheckFunctions(info, fn->body);
    //                 }
    //             }
    //         }
    //         break; case ASTScope::ENUM: {
    //         }
    //         break; default: {
    //             INCOMPLETE
    //         }
    //     }
    // }

    for(auto now : scope->namespaces) {
        CheckFunctions(info, now);
    }
    
    for(auto fn : scope->functions){
        CheckFunction(info, fn, nullptr, scope);
        if(fn->body){ // external/native function do not have bodies
            SignalDefault result = CheckFunctions(info, fn->body);
        }
    }
    for(auto it : scope->structs){
        for(auto fn : it->functions){
            CheckFunction(info, fn , it, scope);
            if(fn->body){ // external/native function do not have bodies
                SignalDefault result = CheckFunctions(info, fn->body);
            }
        }
    }
    
    for(auto astate : scope->statements) {
        if(astate->hasNodes()){
            if(astate->firstBody){
                SignalDefault result = CheckFunctions(info, astate->firstBody);   
            }
            if(astate->secondBody){
                SignalDefault result = CheckFunctions(info, astate->secondBody);
            }
        }
    }
    
    return SignalDefault::SUCCESS;
}

SignalDefault CheckFuncImplScope(CheckInfo& info, ASTFunction* func, FuncImpl* funcImpl){
    using namespace engone;
    _TC_LOG(FUNC_ENTER) // if(func->body->nativeCode)
    //     return true;

    funcImpl->polyVersion = func->polyVersionCount++; // New poly version
    info.currentPolyVersion = funcImpl->polyVersion;
    // log::out << log::YELLOW << __FILE__<<":"<<__LINE__ << ": Fix methods\n";
    // Where does ASTStruct come from. Arguments? FuncImpl?

    if(func->parentStruct){
        for(int i=0;i<(int)func->parentStruct->polyArgs.size();i++){
            auto& arg = func->parentStruct->polyArgs[i];
            arg.virtualType->id = funcImpl->structImpl->polyArgs[i];
        }
    }
    for(int i=0;i<(int)func->polyArgs.size();i++){
        auto& arg = func->polyArgs[i];
        arg.virtualType->id = funcImpl->polyArgs[i];
    }
    defer {
        if(func->parentStruct){
            for(int i=0;i<(int)func->parentStruct->polyArgs.size();i++){
                auto& arg = func->parentStruct->polyArgs[i];
                arg.virtualType->id = {};
            }
        }
        for(int i=0;i<(int)func->polyArgs.size();i++){
            auto& arg = func->polyArgs[i];
            arg.virtualType->id = {};
        }
    };

    // TODO: Add arguments as variables
    DynamicArray<std::string> vars;
    _TC_LOG(log::out << "arg ";)
    for (int i=0;i<(int)func->arguments.size();i++) {
        auto& arg = func->arguments[i];
        auto& argImpl = funcImpl->argumentTypes[i];
        _TC_LOG(log::out << " " << arg.name<<": "<< info.ast->typeToString(argImpl.typeId) <<"\n";)
        auto varinfo = info.ast->addVariable(func->scopeId, std::string(arg.name));
        if(varinfo){
            varinfo->typeId = argImpl.typeId; // typeId comes from CheckExpression which may or may not evaluate
            // the same type as the generator.
            vars.add(std::string(arg.name));
        }   
    }
    _TC_LOG(log::out << "\n";)

    CheckRest(info, func->body);
    
    for(auto& name : vars){
        info.ast->removeIdentifier(func->scopeId, name);
    }
    return SignalDefault::SUCCESS;
}

SignalDefault CheckRest(CheckInfo& info, ASTScope* scope){
    using namespace engone;
    MEASURE;
    _TC_LOG_ENTER(FUNC_ENTER)

    // Hello me in the future!
    //  I have disrespectfully left a complex and troublesome problem to you.
    //  CheckRest needs to be run for each function implementation to find new
    //  polymorphic functions to check and also run CheckRest on. This is highly
    //  recursive and very much a spider net. Good luck.

    // Hello past me!
    //  I seem to have a good implementation of function overloading and was
    //  wondering why sizeof wasn't calculated properly. I figured something
    //  was wrong with CheckRest and indeed there is! I don't appreciate that
    //  you left this for me but I'm in a good mood so I'll let it slide.

    // TODO: Type check default values in structs and functions
    // bool nonStruct = true;
    // Function bodies/impl to check are added to a list
    // in CheckFunction and CheckFnCall if polymorphic
 

    DynamicArray<std::string> vars;
    for (auto now : scope->statements){
        DynamicArray<TypeId> typeArray{};
        
        //-- Check assign types in all varnames. The result is put in version_assignType for
        //   the generator and rest of the code to use.
        for(auto& varname : now->varnames) {
            if(varname.assignString.isString()){
                bool printedError = false;
                auto ti = CheckType(info, scope->scopeId, varname.assignString, now->tokenRange, &printedError);
                // NOTE: We don't care whether it's a pointer just that the type exists.
                if (!ti.isValid() && !printedError) {
                    ERR_HEAD3(now->tokenRange, "'"<<info.ast->getTokenFromTypeString(varname.assignString)<<"' is not a type (statement).\n\n";
                        ERR_LINE(now->tokenRange,"bad");
                    )
                } else {
                    // if(varname.arrayLength != 0){
                    //     auto ti = CheckType(info, scope->scopeId, varname.assignString, now->tokenRange, &printedError);
                    // } else {
                        // If typeid is invalid we don't want to replace the invalid one with the type
                        // with the string. The generator won't see the names of the invalid types.
                        // now->typeId = ti;
                        varname.versions_assignType[info.currentPolyVersion] = ti;
                    // }
                }
            }
        }

        if(now->type == ASTStatement::CONTINUE || now->type == ASTStatement::BREAK){
            // nothing
        } else if(now->type == ASTStatement::BODY || now->type == ASTStatement::DEFER){
            SignalDefault result = CheckRest(info, now->firstBody);
        } else if(now->type == ASTStatement::EXPRESSION){
            CheckExpression(info, scope->scopeId, now->firstExpression, &typeArray);
            // if(typeArray.size()==0)
            //     typeArray.add(AST_VOID);
        } else if(now->type == ASTStatement::RETURN){
            for(auto ret : now->returnValues){
                typeArray.resize(0);
                SignalDefault result = CheckExpression(info, scope->scopeId, ret, &typeArray);
            }
        } else if(now->type == ASTStatement::IF){
            SignalDefault result = CheckExpression(info, scope->scopeId,now->firstExpression,&typeArray);
            result = CheckRest(info, now->firstBody);
            if(now->secondBody){
                result = CheckRest(info, now->secondBody);
            }
        } else if(now->type == ASTStatement::ASSIGN){
            auto& typeArray = now->versions_expresssionTypes[info.currentPolyVersion];
            if(now->firstExpression){
                // may not exist, meaning just a declaration, no assignment
                SignalDefault result = CheckExpression(info, scope->scopeId,now->firstExpression, &typeArray);
                if(now->versions_expresssionTypes[info.currentPolyVersion].size()==0)
                    typeArray.add(AST_VOID);
            }
            bool hadError = false;
            if(now->firstExpression && typeArray.size() < now->varnames.size()){
                if(info.compileInfo->typeErrors==0) {
                    ERR_SECTION(
                        ERR_HEAD(now->tokenRange)
                        ERR_MSG("Too many variables were declared.")
                        ERR_LINE(now->tokenRange, now->varnames.size() + " variables")
                        ERR_LINE(now->tokenRange, typeArray.size() + " return values")
                    )
                    hadError = true;
                }
                // don't return here, we can still evaluate some things
            }
            _TC_LOG(log::out << "assign ";)
            for (int vi=0;vi<(int)now->varnames.size();vi++) {
                auto& varname = now->varnames[vi];
                // possible implicit type
                // if(varname.arrayLength!=0){
                //     if(varname.assignString.isValid()) { // if assigned type didn't exist the

                //     }
                // } else 
                if(!varname.assignString.isValid()) { // if assigned type didn't exist then 
                    if(vi < typeArray.size()){ // out of bounds, error is printed above
                        varname.versions_assignType[info.currentPolyVersion] = typeArray[vi];
                    } else {
                        if(!hadError){
                            ERR_HEAD3(varname.name.operator TokenRange(), "Variable '"<<log::LIME<<varname.name<<log::SILVER<<"' does not have a type. You either define it explicitly (var: i32) or let the type be inferred from the expression. Neither case happens.\n\n";
                                ERR_LINE(varname.name, "type-less");
                            )
                        }
                    }
                }
                // TODO: Do you need to do something about global data here?
                _TC_LOG(log::out << " " << varname.name<<": "<< info.ast->typeToString(varname.versions_assignType[info.currentPolyVersion]) <<"\n";)
                Identifier* varIdentifier = info.ast->findIdentifier(scope->scopeId, varname.name);
                if(!varIdentifier){
                    auto varinfo = info.ast->addVariable(scope->scopeId, varname.name);
                    Assert(varinfo);
                    varinfo->typeId = varname.versions_assignType[info.currentPolyVersion];
                    vars.add(varname.name);
                } else if(varIdentifier->type==Identifier::VAR) {
                    // variable is already defined
                    auto varinfo = info.ast->identifierToVariable(varIdentifier);
                    Assert(varinfo);
                    if(varname.declaration()){
                        if(varIdentifier->scopeId == scope->scopeId) {
                            // info.ast->removeIdentifier(varIdentifier->scopeId, varname.name); // add variable already does this
                            varinfo = info.ast->addVariable(scope->scopeId, varname.name, true);
                        } else {
                            varinfo = info.ast->addVariable(scope->scopeId, varname.name, true);
                        }
                        Assert(varinfo);
                        varinfo->typeId = varname.versions_assignType[info.currentPolyVersion];
                        vars.add(varname.name);
                    } else {
                        if(!info.ast->castable(varname.versions_assignType[info.currentPolyVersion], varinfo->typeId)){
                            std::string leftstr = info.ast->typeToString(varinfo->typeId);
                            std::string rightstr = info.ast->typeToString(varname.versions_assignType[info.currentPolyVersion]);
                            ERR_HEAD3(now->tokenRange, "Type mismatch '"<<leftstr<<"' <- '"<<rightstr<< "' in assignment.\n\n";
                                ERR_LINE(varname.name, leftstr.c_str());
                                ERR_LINE(now->firstExpression->tokenRange,rightstr.c_str());
                            )
                        }
                    }
                } else {
                    ERR_HEAD3(varname.name.range(), "'"<<varname.name<<"' is defined as a non-variable and cannot be used.\n\n";
                        ERR_LINE(varname.name, "bad");
                    )
                }
            }
            _TC_LOG(log::out << "\n";)
        } else if(now->type == ASTStatement::WHILE){
            SignalDefault result = CheckExpression(info, scope->scopeId, now->firstExpression, &typeArray);
            result = CheckRest(info, now->firstBody);
        } else if(now->type == ASTStatement::FOR){
            DynamicArray<TypeId> temp{}; // For has varnames which use typeArray. Therfore, we need another array.
            SignalDefault result = CheckExpression(info, scope->scopeId, now->firstExpression, &temp);

            Assert(now->varnames.size()==1);
            auto& varname = now->varnames[0];

            ScopeId varScope = now->firstBody->scopeId;

            // TODO: for loop variables
            auto iterinfo = info.ast->getTypeInfo(temp.last());
            if(iterinfo&&iterinfo->astStruct){
                if(iterinfo->astStruct->name == "Slice"){
                    auto varinfo_index = info.ast->addVariable(varScope, "nr");
                    if(varinfo_index) {
                        varinfo_index->typeId = AST_INT32;
                    } else {
                        WARN_HEAD3(now->tokenRange, "Cannot add 'nr' variable to use in for loop. Is it already defined?\n";)
                    }

                    // _TC_LOG(log::out << " " << varname.name<<": "<< info.ast->typeToString(varname.assignType) <<"\n";)
                    auto varinfo_item = info.ast->addVariable(varScope, varname.name);
                    if(varinfo_item){
                        auto memdata = iterinfo->getMember("ptr");
                        auto itemtype = memdata.typeId;
                        itemtype.setPointerLevel(itemtype.getPointerLevel()-1);
                        varname.versions_assignType[info.currentPolyVersion] = itemtype;
                        
                        auto vartype = memdata.typeId;
                        if(!now->pointer){
                            vartype.setPointerLevel(vartype.getPointerLevel()-1);
                        }
                        varinfo_item->typeId = vartype;
                    } else {
                        WARN_HEAD3(now->tokenRange, "Cannot add "<<varname.name<<" variable to use in for loop. Is it already defined?\n";)
                    }

                    SignalDefault result = CheckRest(info, now->firstBody);
                    info.ast->removeIdentifier(varScope, varname.name);
                    info.ast->removeIdentifier(varScope, "nr");
                    continue;
                } else if(iterinfo->astStruct->members.size() == 2) {
                    auto mem0 = iterinfo->getMember(0);
                    auto mem1 = iterinfo->getMember(1);
                    if(mem0.typeId == mem1.typeId && AST::IsInteger(mem0.typeId)){
                        if(now->pointer){
                            WARN_HEAD3(now->tokenRange, "@pointer annotation does nothing with Range as for loop. It only works with slices.\n";)
                        }
                        now->rangedForLoop = true;
                        auto varinfo_index = info.ast->addVariable(varScope, "nr");

                        TypeId inttype = mem0.typeId;
                        varname.versions_assignType[info.currentPolyVersion] = inttype;
                        if(varinfo_index) {
                            varinfo_index->typeId = inttype;
                        } else {
                            WARN_HEAD3(now->tokenRange, "Cannot add 'nr' variable to use in for loop. Is it already defined?\n";)
                        }

                        // _TC_LOG(log::out << " " << varname.name<<": "<< info.ast->typeToString(varname.assignType) <<"\n";)
                        auto varinfo_item = info.ast->addVariable(varScope, varname.name);
                        if(varinfo_item){
                            varinfo_item->typeId = inttype;
                        } else {
                            WARN_HEAD3(now->tokenRange, "Cannot add "<<varname.name<<" variable to use in for loop. Is it already defined?\n";)
                        }

                        SignalDefault result = CheckRest(info, now->firstBody);
                        info.ast->removeIdentifier(varScope, varname.name);
                        info.ast->removeIdentifier(varScope, "nr");
                        continue;
                    }
                }
            }
            std::string strtype = info.ast->typeToString(typeArray.last());
            ERR_HEAD3(now->firstExpression->tokenRange, "The expression in for loop must be a Slice or Range.\n\n";
                ERR_LINE(now->firstExpression->tokenRange,strtype.c_str());
            )
            continue;
        } else 
        // Doing using after body so that continue can be used inside it without
        // messing things up. Using shouldn't have a body though so it doesn't matter.
        if(now->type == ASTStatement::USING){
            // TODO: type check now->name now->alias
            //  if not type then check namespace
            //  otherwise it's variables
            Assert(("Broken code",false));

            if(now->varnames.size()==1 && now->alias){
                Token& name = now->varnames[0].name;
                TypeId originType = CheckType(info, scope->scopeId, name, now->tokenRange, nullptr);
                TypeId aliasType = info.ast->convertToTypeId(*now->alias, scope->scopeId);
                if(originType.isValid() && !aliasType.isValid()){
                    TypeInfo* aliasInfo = info.ast->createType(*now->alias, scope->scopeId);
                    aliasInfo->id = originType;

                    // using statement isn't necessary anymore
                    // @NO-MODIFY-AST ha... ha...  modifying the AST isn't possible
                    // with polymorphism since it will check scopes
                    // multiple times with different types. With modififying I mostly
                    // mean removing or adding nodes. Changing names, types is bad too.
                    // now->next = nullptr;
                    // info.ast->destroy(now);
                    // if(prev){
                    //     prev->next = next;
                    // } else {
                    //     scope->statements = next;
                    // }
                    continue;
                }
                ScopeInfo* originScope = info.ast->getScopeFromParents(name,scope->scopeId);
                ScopeInfo* aliasScope = info.ast->getScopeFromParents(*now->alias,scope->scopeId);
                if(originScope && !aliasScope) {
                    ScopeInfo* scopeInfo = info.ast->getScope(scope->scopeId);
                    // TODO: Alias can't contain multiple namespaces. How to implement
                    //  that?
                    scopeInfo->nameScopeMap[*now->alias] = originScope->id;

                    Assert(("Broken using keyword code",false));
                    // CANNOT MODIFY TREE. SEE @NO-MODIFY-AST
                    // now->next = nullptr;
                    // info.ast->destroy(now);
                    // if(prev){
                    //     prev->next = next;
                    // } else {
                    //     scope->statements = next;
                    // }
                    continue;
                }
                // using might refer to variables

                // if(!originType.isValid()){
                //     ERR_HEAD2(now->tokenRange) << *now->name<<" is not a type (using)\n";
                // }
                // if(aliasType.isValid()){
                //     ERR_HEAD2(now->tokenRange) << *now->alias<<" is already a type (using)\n";
                // }
                // continue;
                // CheckType may create a new type if polymorphic. We don't want that.
                // TODO: It is okay to create a new type with a name which already
                // exists if it's in a different scope.
                // TODO: How does polymorphism work here?
                // Will it work if just left as is or should it be disallowed.
            } else if (now->varnames.size()==1) {
                Token& name = now->varnames[0].name;
                // ERR_HEAD2(now->tokenRange) << "inheriting namespace with using doesn't work\n";
                ScopeInfo* originInfo = info.ast->getScopeFromParents(name,scope->scopeId);
                if(originInfo){
                    ScopeInfo* scopeInfo = info.ast->getScope(scope->scopeId);
                    scopeInfo->usingScopes.push_back(originInfo);
                }

                // TODO: Inherit enum values.

                // using variable; Should not work.

                Assert(("Fix broken code for using keyword",false));
                
                // CANNOT MODIFY TREE. SEE @NO-MODIFY-AST
                // now->next = nullptr;
                // info.ast->destroy(now);
                // if(prev){
                //     prev->next = next;
                // } else {
                //     scope->statements = next;
                // }
                continue;
            }
        } else {
            Assert(("Statement type not handled!",false));
        }
    }
    for(auto& name : vars){
        info.ast->removeIdentifier(scope->scopeId, name);
    }
    for(auto now : scope->namespaces){
        SignalDefault result = CheckRest(info, now);
    }
    return SignalDefault::SUCCESS;
}
int TypeCheck(AST* ast, ASTScope* scope, CompileInfo* compileInfo){
    using namespace engone;
    MEASURE;
    CheckInfo info = {};
    info.ast = ast;
    info.compileInfo = compileInfo;

    _VLOG(log::out << log::BLUE << "Type check:\n";)

    SignalDefault result = CheckEnums(info, scope);

    /*
        How to make global variables and using work with functions and structs
        functions and structs are out of order while global and using isn't.

        functions and structs therefore needs some kind of ordering which has been fixed with contentOrder.
        With it, we know the order of functions, global, using and structs.

    */
    /*
        Preprocessor cannot replace using (for some situations sure but not for all so you might as well go all in on using).
        This: using Array<T> as A<T>
        is not possible with macros because they don't allow special characters (for good reasons)
    */
    /* using needs to run alongside struct checks

        using Node as N // DynamicArray hasn't been created yet
        struct Node<T> {
            sup: N*

        }

        a: Node
    */

    
    // Jonathan Blow has a video about dependenies when
    // checking types (https://www.youtube.com/watch?v=4q0cgjXhhTo)
    // The video is 8 years old so perhaps the system has changed to
    // something better. In any case, he uses an array of pointers to nodes
    // that will be checked. If the node couldn't be checked, then you move on to the next
    // node until all nodes have been checked. If you went a full round without changing anything
    // then you have a circular dependency and should print an error.
    // It might be a good idea to use a iterative approach with an array instead of
    // going deep into each struct like it is now. An array is easier to manage and
    // may improve cache hits. Before this is to be done, there must be a significant amount
    // of types and structs to test this on. See the performance difference.
    // This might not be a good idea, it might be fine as it is. I mean, it does work.
    // If turns out to be slow when you are dealing with many structs then it's time to
    // fix this.
    info.completedStructs=false;
    info.showErrors = false;
    while(!info.completedStructs){
        info.completedStructs = true;
        info.anotherTurn=false;
        
        result = CheckStructs(info, scope);
        
        if(!info.anotherTurn && !info.completedStructs){
            if(!info.showErrors){
                info.showErrors = true;
                continue;
            }
            // log::out << log::RED << "Some types could not be evaluated\n";
            break;
        }
    }
    result = CheckFunctions(info, scope);

    result = CheckRest(info, scope);

    while(info.checkImpls.size()!=0){
        auto checkImpl = info.checkImpls[info.checkImpls.size()-1];
        info.checkImpls.pop();
        Assert(checkImpl.astFunc->body); // impl should not have been added if there was no body
        CheckFuncImplScope(info, checkImpl.astFunc, checkImpl.funcImpl);
    }
    
    info.compileInfo->errors += info.errors;
    info.compileInfo->typeErrors += info.errors;
    return info.errors;
}