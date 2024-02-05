#include "BetBat/TypeChecker.h"
#include "BetBat/Compiler.h"

// Old logging
#undef WARN_HEAD3
#define WARN_HEAD3(R,M) info.compileInfo->compileOptions->compileStats.warnings++; engone::log::out << WARN_DEFAULT_R(R,"Type warning","W0000") << M

#undef ERR_SECTION
#define ERR_SECTION(CONTENT) BASE_SECTION2(CONTENT)

#define _TCLOG_ENTER(...) FUNC_ENTER_IF(global_loggingSection & LOG_TYPECHECKER)
// #define _TCLOG_ENTER(...) _TCLOG(__VA_ARGS__)
// #define _TCLOG_ENTER(X)

TypeId CheckType(CheckInfo& info, ScopeId scopeId, TypeId typeString, lexer::SourceLocation location, bool* printedError);
TypeId CheckType(CheckInfo& info, ScopeId scopeId, StringView typeString, lexer::SourceLocation location, bool* printedError);

SignalIO CheckEnums(CheckInfo& info, ASTScope* scope);

SignalIO CheckStructs(CheckInfo& info, ASTScope* scope);
SignalIO CheckStructImpl(CheckInfo& info, ASTStruct* astStruct, TypeInfo* structInfo, StructImpl* structImpl);

SignalIO CheckFunctions(CheckInfo& info, ASTScope* scope);
SignalIO CheckFunction(CheckInfo& info, ASTFunction* function, ASTStruct* parentStruct, ASTScope* scope);
SignalIO CheckFunctionImpl(CheckInfo& info, ASTFunction* func, FuncImpl* funcImpl, ASTStruct* parentStruct, QuickArray<TypeId>* outTypes);
SignalIO CheckFuncImplScope(CheckInfo& info, ASTFunction* func, FuncImpl* funcImpl);

SignalIO CheckRest(CheckInfo& info, ASTScope* scope);
SignalIO CheckExpression(CheckInfo& info, ScopeId scopeId, ASTExpression* expr, QuickArray<TypeId>* outTypes, bool attempt);
SignalIO CheckFncall(CheckInfo& info, ScopeId scopeId, ASTExpression* expr, QuickArray<TypeId>* outTypes, bool attempt, bool operatorOverloadAttempt, QuickArray<TypeId>* operatorArgs = nullptr);

SignalIO CheckEnums(CheckInfo& info, ASTScope* scope){
    using namespace engone;
    Assert(scope);
    ZoneScopedC(tracy::Color::Purple4);
    _TCLOG_ENTER(FUNC_ENTER)

    // OPTIMIZE TODO: Recursively checking enums may be slower than iteratively doing so with a
    //   list of scopes to check. How much performance do we gain from that?
    // OPTIMIZE TODO: Parser should set a flag for whether a scope contains enums, structs and functions.
    //   We can do that per namespace, function or import scope. Don't have to do that for each if, while, for scope.

    SignalIO error = SIGNAL_SUCCESS;
    for(auto aenum : scope->enums){
        if(aenum->colonType.isString()) {
            auto typeString = info.ast->getStringFromTypeString(aenum->colonType);
            bool printedError = false;
            TypeId colonType = CheckType(info, scope->scopeId, typeString, aenum->location, &printedError);
            if(!colonType.isValid()) {
                if(!printedError) {
                    ERR_SECTION(
                        ERR_HEAD2(aenum->location)
                        ERR_MSG("The type for the enum after : was not valid.")
                        ERR_LINE2(aenum->location, typeString<< " was invalid")
                    )
                }
            } else {
                aenum->colonType = colonType;
            }
        }
        TypeInfo* typeInfo = info.ast->createType(aenum->name, scope->scopeId);
        if(typeInfo){
            aenum->actualType = typeInfo->id;
            _TCLOG(log::out << "Defined enum "<<info.ast->typeToString(typeInfo->id)<<"\n";)
            
            if(aenum->colonType.isValid()) {
                typeInfo->_size = info.ast->getTypeSize(aenum->colonType);
            } else {
                typeInfo->_size = 4;
            }
            typeInfo->astEnum = aenum;
        } else {
            ERR_SECTION(
                ERR_HEAD2(aenum->location)
                ERR_MSG("'"<<aenum->name << "' is already a type. TODO: Show where the previous definition was.")
            )
        }
    }
    
    for(auto it : scope->namespaces){
        SignalIO result = CheckEnums(info,it);
        if(result != SIGNAL_SUCCESS)
            error = SIGNAL_FAILURE;
    }
    
    for(auto astate : scope->statements) {
        if(astate->firstBody){
            SignalIO result = CheckEnums(info, astate->firstBody);   
            if(result != SIGNAL_SUCCESS)
                error = SIGNAL_FAILURE;
        }
        if(astate->secondBody){
            SignalIO result = CheckEnums(info, astate->secondBody);
            if(result != SIGNAL_SUCCESS)
                error = SIGNAL_FAILURE;
        }
    }
    
    for(auto afunc : scope->functions) {
        if(afunc->body){
            SignalIO result = CheckEnums(info, afunc->body);
            if(result != SIGNAL_SUCCESS)
                error = SIGNAL_FAILURE;
        }
    }
    return error;
}
SignalIO CheckStructImpl(CheckInfo& info, ASTStruct* astStruct, TypeInfo* structInfo, StructImpl* structImpl){
    using namespace engone;
    _TCLOG_ENTER(FUNC_ENTER)
    int offset=0;
    int alignedSize=0; // offset will be aligned to match this at the end

    Assert(astStruct->polyArgs.size() == structImpl->polyArgs.size());
    // DynamicArray<TypeId> prevVirtuals{};
    
    // info.prevVirtuals.resize(astStruct->polyArgs.size());
    // for(int i=0;i<(int)astStruct->polyArgs.size();i++){
    //     TypeId id = structImpl->polyArgs[i];
    //     if(info.errors == 0){
    //         // Assert(id.isValid()); // poly arg type could have failed, an error would have been logged
    //     }
    //     // Assert(astStruct->polyArgs[i].virtualType->isVirtual());
    //     info.prevVirtuals[i] = astStruct->polyArgs[i].virtualType->id;
    //     astStruct->polyArgs[i].virtualType->id = id;
    // }
    astStruct->pushPolyState(structImpl);
    defer{
        astStruct->popPolyState();
        // for(int i=0;i<(int)astStruct->polyArgs.size();i++){
        //     astStruct->polyArgs[i].virtualType->id = info.prevVirtuals[i];
        // }
    };
   
    structImpl->members.resize(astStruct->members.size());

    TINY_ARRAY(TypeId, tempTypes, 4)
    bool success = true;
    _TCLOG(log::out << "Check struct impl "<<info.ast->typeToString(structInfo->id)<<"\n";)
    //-- Check members
    for (int i = 0;i<(int)astStruct->members.size();i++) {
        auto& member = astStruct->members[i];
        auto& implMem = structImpl->members[i];

        Assert(member.stringType.isString());
        bool printedError = false;
        TypeId tid = CheckType(info, astStruct->scopeId, member.stringType, astStruct->location, &printedError);
        if(!tid.isValid() && !printedError){
            if(info.showErrors) {
                ERR_SECTION(
                    ERR_HEAD2(astStruct->location)
                    ERR_MSG("Type '"<< info.ast->getStringFromTypeString(member.stringType) << "' in "<< astStruct->name<<"."<<member.name << " is not a type.")
                    ERR_LINE2(member.location,"bad type")
                )
            }
            success = false;
            continue;
        }
        implMem.typeId = tid;
        if(member.defaultValue){
            // TODO: Don't check default expression every time. Do check once and store type in AST.
            tempTypes.resize(0);
            CheckExpression(info,structInfo->scopeId, member.defaultValue,&tempTypes, false);
            if(tempTypes.size()==0)
                tempTypes.add(AST_VOID);
            if(!info.ast->castable(implMem.typeId, tempTypes.last())){
                std::string deftype = info.ast->typeToString(tempTypes.last());
                std::string memtype = info.ast->typeToString(implMem.typeId);
                ERR_SECTION(
                    ERR_HEAD2(member.defaultValue->location) // nocheckin, message should mark the whole expression, not just some random token/operation inside it
                    ERR_MSG("Type of default value does not match member.")
                    ERR_LINE2(member.defaultValue->location,deftype.c_str())
                    ERR_LINE2(member.location,memtype.c_str())
                )
                continue; // continue when failing
            }
        }
        _TCLOG(log::out << " checked member["<<i<<"] "<<info.ast->typeToString(tid)<<"\n";)
    }
    if(!success){
        return SIGNAL_FAILURE;
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
                    ERR_SECTION(
                        ERR_HEAD2(member.location)
                        ERR_MSG("Member "<<member.name << "'s type uses the parent struct. (recursive struct does not work).")
                    )
                }
                // TODO: phrase the error message in a better way
                // TOOD: print some column and line info.
                // TODO: message is printed twice
                // log::out << log::RED <<"Member "<< member.name <<" in struct "<<*astStruct->name<<" cannot be it's own type\n";
            } else {
                // astStruct->state = ASTStruct::TYPE_ERROR;
                if(info.showErrors) {
                    ERR_SECTION(
                        ERR_HEAD2(member.location)
                        ERR_MSG("Type '"<< info.ast->typeToString(implMem.typeId) << "' in '"<< astStruct->name<<"."<<member.name << "' is not a type or an incomplete one. Do structs depend on each other recursively?")
                    )
                }
            }
            success = false;
            continue;
        }
        if(alignedSize<asize)
            alignedSize = asize > 8 ? 8 : asize;

        if(offset % asize != 0) {
            offset += asize - (offset % asize);
        }

        // if(i+1<(int)astStruct->members.size()){
        //     auto& implMema = structImpl->members[i+1];
        //     i32 nextSize = info.ast->getTypeSize(implMema.typeId); // TODO: Use getTypeAlignedSize instead?
        //     nextSize = nextSize>8?8:nextSize;
        //     // i32 asize = info.ast->getTypeAlignedSize(implMem.typeId);
        //     int misalign = (offset + size) % nextSize;
        //     if(misalign!=0){
        //         offset+=nextSize-misalign;
        //     }
        // } else {
        //     int misalign = (offset + size) % alignedSize;
        //     if(misalign!=0){
        //         offset+=alignedSize-misalign;
        //     }
        // }
        
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
    if(success) {
        if(offset != 0){
            structImpl->alignedSize = alignedSize;
            int misalign = offset % alignedSize;
            if(misalign!=0){
                offset+=alignedSize-misalign;
            }
        }
        structImpl->size = offset;
    }
    _VLOG(
        std::string polys = "";
        if(structImpl->polyArgs.size()!=0){
            polys+="<";
            for(int i=0;i<(int)structImpl->polyArgs.size();i++){
                if(i!=0) polys+=",";
                polys += info.ast->typeToString(structImpl->polyArgs[i]);
            }
            polys+=">";
        }
        log::out << "Struct "<<log::LIME << astStruct->name<<polys<<log::NO_COLOR<<" (size: "<<structImpl->size <<(astStruct->isPolymorphic()?", poly. impl.":"")<<", scope: "<<info.ast->getScope(astStruct->scopeId)->parent<<")\n";
        for(int i=0;i<(int)structImpl->members.size();i++){
            auto& name = astStruct->members[i].name;
            auto& mem = structImpl->members[i];
            
            i32 size = info.ast->getTypeSize(mem.typeId);
            _VLOG(log::out << " "<<mem.offset<<": "<<name<<" ("<<size<<" bytes)\n";)
        }
    )
    // _TCLOG(log::out << info.ast->typeToString << " was evaluated to "<<offset<<" bytes\n";)
    // }
    if(success)
        return SIGNAL_SUCCESS;
    return SIGNAL_FAILURE;
}
TypeId CheckType(CheckInfo& info, ScopeId scopeId, TypeId typeString, lexer::SourceLocation err_location, bool* printedError){
    using namespace engone;
    Assert(typeString.isString());
    // if(!typeString.isString()) {
    //     _TCLOG(log::out << "check type typeid wasn't string type\n";)
    //     return typeString;
    // }
    auto token = info.ast->getStringFromTypeString(typeString);
    return CheckType(info, scopeId, token, err_location, printedError);
}
TypeId CheckType(CheckInfo& info, ScopeId scopeId, StringView typeString, lexer::SourceLocation err_location, bool* printedError){
    using namespace engone;
    ZoneScopedC(tracy::Color::Purple4);
    _TCLOG_ENTER(FUNC_ENTER)
    _TCLOG(log::out << "check "<<typeString<<"\n";)

    // TODO: Log what type we thought it was

    TypeId typeId = {};

    u32 plevel=0;
    QuickArray<StringView> polyTokens;
    // TODO: namespace?
    StringView typeName{};
    StringView baseType{};
    AST::DecomposePointer(typeString, &typeName, &plevel);
    AST::DecomposePolyTypes(typeName, &baseType, &polyTokens);
    // Token typeName = AST::TrimNamespace(noPointers, &ns);
    TINY_ARRAY(TypeId, polyArgs, 4);
    std::string realTypeName{};

    if(polyTokens.size() != 0) {
        // We trim poly types and then put them back together to get the "official" names for the types
        // Maybe you used some aliasing or namespaces.
        realTypeName += baseType;
        realTypeName += "<";
        // TODO: Virtual poly arguments does not work with multithreading. Or you need mutexes at least.
        for(int i=0;i<(int)polyTokens.size();i++){
            // TypeInfo* polyInfo = info.ast->convertToTypeInfo(polyTokens[i], scopeId);
            // TypeId id = info.ast->convertToTypeId(polyTokens[i], scopeId);
            bool printedError = false;
            TypeId id = CheckType(info, scopeId, polyTokens[i], err_location, &printedError);
            if(i!=0)
                realTypeName += ",";
            realTypeName += info.ast->typeToString(id);
            polyArgs.add(id);
            if(id.isValid()){

            } else if(!printedError) {
            //     // ERR_SECTION(
            // ERR_HEAD2(err_location, "Type '"<<info.ast->typeToString(id)<<"' for polymorphic argument was not valid.\n\n";
                ERR_SECTION(
                    ERR_HEAD2(err_location)
                    ERR_MSG("Type '"<<polyTokens[i]<<"' for polymorphic argument was not valid.")
                    ERR_LINE2(err_location,"here somewhere")
                )
            }
            // baseInfo->astStruct->polyArgs[i].virtualType->id = polyInfo->id;
        }
        realTypeName += ">";
        typeId = info.ast->convertToTypeId(realTypeName, scopeId, true);
    } else {
        typeId = info.ast->convertToTypeId(typeName, scopeId, true);
    }

    if(typeId.isValid()) {
        auto ti = info.ast->getTypeInfo(typeId.baseType());
        Assert(ti);
        if(!ti)
            // TODO: log error here if Assert is removed
            return {};
        if(ti && ti->astStruct && ti->astStruct->polyArgs.size() != 0 && !ti->structImpl) {
            ERR_SECTION(
                ERR_HEAD2(err_location)
                ERR_MSG(info.ast->typeToString(ti->id)<<" is polymorphic. You must specify poly. types like this: Struct<i32>. (if in a function: compiler does not have knowledge of where).")
                ERR_LINE2(err_location,"")
            )
            if(printedError)
                *printedError = true;
            return {};
        }
        typeId.setPointerLevel(plevel);
        return typeId;
    }

    if(polyTokens.size() == 0) {
        // ERR_SECTION(
        // ERR_HEAD2(err_location) << <<" is polymorphic. You must specify poly. types like this: Struct<i32>\n";
        return {}; // type isn't polymorphic and does just not exist
    }
    TypeInfo* baseInfo = info.ast->convertToTypeInfo(baseType, scopeId, true);
    if(!baseInfo) {
        // ERR_SECTION(
        // ERR_HEAD2(err_location) << info.ast->typeToString(ti->id)<<" is polymorphic. You must specify poly. types like this: Struct<i32>\n";
        return {}; // print error? base type for polymorphic type doesn't exist
    }
    if(polyTokens.size() != baseInfo->astStruct->polyArgs.size()) {
        ERR_SECTION(
            ERR_HEAD2(err_location)
            ERR_MSG("Polymorphic type "<<typeString << " has "<< (u32)polyTokens.size() <<" poly. args but the base type "<<info.ast->typeToString(baseInfo->id)<<" needs "<<(u32)baseInfo->astStruct->polyArgs.size()<<".")
        )
        // ERR() << "Polymorphic type "<<typeString << " has "<< polyTokens.size() <<" poly. args but the base type "<<info.ast->typeToString(baseInfo->id)<<" needs "<<baseInfo->astStruct->polyArgs.size()<< "\n";
        if(printedError)
            *printedError = true;
        return {}; // type isn't polymorphic and does just not exist
    }

    TypeInfo* typeInfo = info.ast->createType(realTypeName, baseInfo->scopeId);
    typeInfo->astStruct = baseInfo->astStruct;
    typeInfo->structImpl = info.ast->createStructImpl();
    // typeInfo->structImpl = baseInfo->astStruct->createImpl();

    typeInfo->structImpl->polyArgs.resize(polyArgs.size());
    for(int i=0;i<(int)polyArgs.size();i++){
        TypeId id = polyArgs[i];
        typeInfo->structImpl->polyArgs[i] = id;
    }

    SignalIO hm = CheckStructImpl(info, typeInfo->astStruct, baseInfo, typeInfo->structImpl);
    if(hm != SIGNAL_SUCCESS) {
        ERR_SECTION(
            ERR_HEAD2(err_location)
            ERR_MSG(__FUNCTION__ <<": structImpl for type "<<typeString << " failed.")
        )
    } else {
        _TCLOG(log::out << typeString << " was evaluated to "<<typeInfo->structImpl->size<<" bytes\n";)
    }
    TypeId outId = typeInfo->id;
    outId.setPointerLevel(plevel);
    return outId;
}
SignalIO CheckStructs(CheckInfo& info, ASTScope* scope) {
    using namespace engone;
    ZoneScopedC(tracy::Color::X11Purple);
    _TCLOG_ENTER(FUNC_ENTER)
    //-- complete structs

    for(auto it : scope->namespaces) {
        CheckStructs(info, it);
    }
   
    // TODO: @Optimize The code below doesn't need to run if the structs are complete.
    //   We can skip directly to going through the closed scopes further down.
    //   How to keep track of all this is another matter.
    // @OPTIMIZE: When a struct fails, you can store the index you failed at continue from there instead of restarting from the beginning.
    for(auto astStruct : scope->structs){
        //-- Get struct info
        TypeInfo* structInfo = nullptr;
        if(astStruct->state==ASTStruct::TYPE_EMPTY){
            // structInfo = info.ast->getTypeInfo(scope->scopeId, astStruct->name,false,true);
            structInfo = info.ast->createType(astStruct->name, scope->scopeId);
            if(!structInfo){
                astStruct->state = ASTStruct::TYPE_ERROR;
                ERR_SECTION(
                    ERR_HEAD2(astStruct->location)
                    ERR_MSG("'"<<astStruct->name<<"' is already defined.")
                )
                // TODO: Provide information (file, line, column) of the first definition.
                // We don't care about another turn. We failed but we don't set
                // completedStructs to false since this will always fail.
            } else {
                _TCLOG(log::out << "Created struct type "<<info.ast->typeToString(structInfo->id)<<" in scope "<<scope->scopeId<<"\n";)
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
                structInfo = info.ast->convertToTypeInfo(astStruct->name, scope->scopeId, true);   
            Assert(structInfo); // compiler bug

            // log::out << "Evaluating "<<*astStruct->name<<"\n";
            bool yes = true;
            if(astStruct->polyArgs.size()==0){
                // Assert(!structInfo->structImpl);
                if(!structInfo->structImpl){
                    structInfo->structImpl = info.ast->createStructImpl();
                    // structInfo->structImpl = astStruct->createImpl();
                    astStruct->nonPolyStruct = structInfo->structImpl;
                }
                yes = CheckStructImpl(info, astStruct, structInfo, structInfo->structImpl) == SIGNAL_SUCCESS;
                if(!yes){
                    astStruct->state = ASTStruct::TYPE_CREATED;
                    info.completedStructs = false;
                } else {
                    // _TCLOG(log::out << astStruct->name << " was evaluated to "<<astStruct->baseImpl.size<<" bytes\n";)
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
                SignalIO result = CheckStructs(info, astate->firstBody);   
                // if(!result)
                //     error = false;
            }
            if(astate->secondBody){
                SignalIO result = CheckStructs(info, astate->secondBody);
                // if(!result)
                //     error = false;
            }
        }
        
        for(auto afunc : scope->functions) {
            if(afunc->body){
                SignalIO result = CheckStructs(info, afunc->body);
                // if(!result)
                //     error = false;
            }
        }
    // }
    return SIGNAL_SUCCESS;
}
SignalIO CheckFncall(CheckInfo& info, ScopeId scopeId, ASTExpression* expr, QuickArray<TypeId>* outTypes, bool attempt, bool operatorOverloadAttempt, QuickArray<TypeId>* operatorArgs) {
    using namespace engone;
    Assert(!outTypes || outTypes->size()==0);
    #define FNCALL_SUCCESS \
        if(outTypes) {\
            for(auto& ret : overload->funcImpl->returnTypes)\
                outTypes->add(ret.typeId); \
            if(overload->funcImpl->returnTypes.size()==0)\
                outTypes->add(AST_VOID);\
        }\
        expr->versions_overload[info.currentPolyVersion] = *overload;
    #define FNCALL_FAIL \
        if(outTypes) outTypes->add(AST_VOID);
    // fail should perhaps not output void as return type.

    //-- Get poly args from the function call
    // DynamicArray<TypeId> fnPolyArgs;
    TINY_ARRAY(TypeId, fnPolyArgs, 4);
    // TINY_ARRAY(Token,polyTokens,4);
    QuickArray<StringView> polyTypes{};

    // Token baseName{};
    StringView baseName{};
    
    if(operatorOverloadAttempt) {
        baseName = OP_NAME((OperationType)expr->typeId.getId());
    } else {
        // baseName = AST::TrimPolyTypes(expr->name, &polyTokens);
        AST::DecomposePolyTypes(expr->name, &baseName, &polyTypes);
        for(int i=0;i<(int)polyTypes.size();i++){
            bool printedError = false;
            TypeId id = CheckType(info, scopeId, polyTypes[i], expr->location, &printedError);
            fnPolyArgs.add(id);
            // TODO: What about void?
            if(id.isValid()){

            } else if(!printedError) {
                ERR_SECTION(
                    ERR_HEAD2(expr->location) // nocheckin, mark all tokens in the expression,location is just one token
                    ERR_MSG("Type for polymorphic argument was not valid.")
                    ERR_LINE2(expr->location,"bad")
                )
            }
        }
    }

    //-- Get essential arguments from the function call

    // DynamicArray<TypeId>& argTypes = expr->versions_argTypes[info.currentPolyVersion];
    // DynamicArray<TypeId> argTypes{};
    TINY_ARRAY(TypeId, argTypes, 5);
    TINY_ARRAY(TypeId, tempTypes, 2);

    // TinyArray<TypeId> tempTypes{};
    // TINY_ARRAY(TypeId, tempTypes, 2);
    // SAVEPOINT
    if(operatorOverloadAttempt){
        Assert(operatorArgs);
        if(operatorArgs->size() != 2) {
            return SIGNAL_FAILURE;
        }
        argTypes.resize(operatorArgs->size());
        memcpy(argTypes.data(), operatorArgs->data(), operatorArgs->size() * sizeof(TypeId)); // TODO: Use TintArray::copyFrom() instead (function not implemented yet)
        
        // DynamicArray<TypeId> tempTypes{};
        // tempTypes.resize(0);
        // NOTE: We pour out types into tempTypes instead of argTypes because
        //   we want to separate how many types we get from each expression.
        //   If an expression gives us more than one type then we throw away the other ones.
        //   We might throw an error instead haven't decided yet.
        // SignalIO resultLeft = CheckExpression(info,scopeId,expr->left,&tempTypes, true);
        // if(tempTypes.size()==0){
        //     argTypes.add(AST_VOID);
        // } else {
        //     argTypes.add(tempTypes[0]);
        // }
        // tempTypes.resize(0);
        // SignalIO resultRight = CheckExpression(info,scopeId,expr->right,&tempTypes, true);
        // if(tempTypes.size()==0){
        //     argTypes.add(AST_VOID);
        // } else {
        //     argTypes.add(tempTypes[0]);
        // }
        // tempTypes.resize(0);
        // if(resultLeft != SIGNAL_SUCCESS || resultRight != SIGNAL_SUCCESS)
        //     return SIGNAL_FAILURE; // should be a failure since the expressions actually failed
        // Assert(argTypes.size() == 2);

    } else {
        bool thisFailed=false;
        // for(int i = 0; i<(int)expr->args->size();i++){
        //     auto argExpr = expr->args->get(i);
            
        for(int i = 0; i<(int)expr->args.size();i++){
            auto argExpr = expr->args.get(i);
            Assert(argExpr);

            tempTypes.resize(0);
            CheckExpression(info,scopeId,argExpr,&tempTypes, false);
            Assert(tempTypes.size()==1); // should be void at least
            if(expr->hasImplicitThis() && i==0){
                /* You can do this
                    l: List;
                    p := &l;
                    list.add(1) // here, the type of "this" is not a pointer so we implicitly reference the variable
                    p.add(2) // here, the type is a pointer so everything is fine
                */
                if(tempTypes[0].getPointerLevel()>1){
                    thisFailed=true; // We do not allow calling function from type 'List**'
                } else {
                    tempTypes[0].setPointerLevel(1); // type is either 'List' or 'List*' both are fine
                }
            }
            // log::out << "Yes "<< info.ast->typeToString(tempTypes[0])<<"\n";
            argTypes.add(tempTypes[0]);
        }
        // cannot continue if we don't know which struct the method comes from
        if(thisFailed) {
            FNCALL_FAIL
            return SIGNAL_FAILURE;
        }
    }

    //-- Get identifier, the namespace of overloads for the function/method.
    FnOverloads* fnOverloads = nullptr;
    StructImpl* parentStructImpl = nullptr;
    ASTStruct* parentAstStruct = nullptr;
    if(expr->hasImplicitThis()){
        // Assert(expr->args->size()>0);
        // ASTExpression* thisArg = expr->args->get(0);
        Assert(expr->args.size()>0);
        ASTExpression* thisArg = expr->args.get(0);
        TypeId structType = argTypes[0];
        // CheckExpression(info,scope, thisArg, &structType);
        if(structType.getPointerLevel()>1){
            ERR_SECTION(
                ERR_HEAD2(thisArg->location) // nocheckin, badly marked token
                ERR_MSG("'"<<info.ast->typeToString(structType)<<"' to much of a pointer.")
                ERR_LINE2(thisArg->location,"must be a reference to a struct")
            )
            FNCALL_FAIL
            return SIGNAL_FAILURE;
        }
        TypeInfo* ti = info.ast->getTypeInfo(structType.baseType());
        
        if(!ti || !ti->astStruct){
            ERR_SECTION(
                ERR_HEAD2(expr->location)// nocheckin, badly marked token
                ERR_MSG("'"<<info.ast->typeToString(structType)<<"' is not a struct. Method cannot be called.")
                ERR_LINE2(thisArg->location,"not a struct")
            )
            FNCALL_FAIL
            return SIGNAL_FAILURE;
        }
        if(!ti->getImpl()){
            ERR_SECTION(
                ERR_HEAD2(expr->location) // nocheckin, badly marked token
                ERR_MSG("'"<<info.ast->typeToString(structType)<<"' is a polymorphic struct with not poly args specified.")
                ERR_LINE2(thisArg->location,"base polymorphic struct")
            )
            FNCALL_FAIL
            return SIGNAL_FAILURE;
        }
        parentStructImpl = ti->structImpl;
        parentAstStruct = ti->astStruct;
        fnOverloads = ti->astStruct->getMethod(baseName);

        if(!fnOverloads) {
            if(operatorOverloadAttempt || attempt)
                return SIGNAL_NO_MATCH;

            ERR_SECTION(
                ERR_HEAD2(expr->location) // nocheckin, badly marked token
                ERR_MSG("Method '"<<baseName <<"' does not exist. Did you mispell the name?")
                ERR_LINE2(expr->location,"undefined")
            )
            FNCALL_FAIL
            return SIGNAL_NO_MATCH;
        }
    } else {
        if(info.currentAstFunc && info.currentAstFunc->parentStruct) {
            // TODO: Check if function name exists in parent struct
            //   If so, an implicit this argument should be taken into account.
            fnOverloads = info.currentAstFunc->parentStruct->getMethod(baseName);
            if(fnOverloads){
                parentStructImpl = info.currentFuncImpl->structImpl;
                parentAstStruct = info.currentAstFunc->parentStruct;
                expr->setImplicitThis(true);
            }
        }
        if(!fnOverloads) {
            Identifier* iden = info.ast->findIdentifier(scopeId, info.getCurrentOrder(), baseName);
            if(!iden || iden->type != Identifier::FUNCTION){
                if(operatorOverloadAttempt || attempt)
                    return SIGNAL_NO_MATCH;

                ERR_SECTION(
                    ERR_HEAD2(expr->location)
                    ERR_MSG("Function '"<<baseName <<"' does not exist. Did you forget an import?")
                    ERR_LINE2(expr->location,"undefined")
                )
                FNCALL_FAIL
                return SIGNAL_FAILURE;
            }
            fnOverloads = &iden->funcOverloads;
        }
    }
    bool implicitPoly = false; // implicit poly does not affect parentStruct
    // if(fnPolyArgs.size()==0){
    if(fnPolyArgs.size()==0 && (!parentAstStruct || parentAstStruct->polyArgs.size()==0)){
        // match args with normal impls
        // FnOverloads::Overload* overload = fnOverloads->getOverload(info.ast, argTypes, expr, fnOverloads->overloads.size()==1 && !operatorOverloadAttempt);
        FnOverloads::Overload* overload = fnOverloads->getOverload(info.ast, argTypes, expr, fnOverloads->overloads.size()==1);
        if(!overload)
            overload = fnOverloads->getOverload(info.ast, argTypes, expr, true);
        if(operatorOverloadAttempt && !overload)
            return SIGNAL_NO_MATCH;
        if(overload){
            if(overload->astFunc->body && overload->funcImpl->usages == 0){
                CheckInfo::CheckImpl checkImpl{};
                checkImpl.astFunc = overload->astFunc;
                checkImpl.funcImpl = overload->funcImpl;
                info.checkImpls.add(checkImpl);
            }
            overload->funcImpl->usages++;

            for(int i=argTypes.size(); i<overload->astFunc->arguments.size();i++) {
                auto& argImpl = overload->funcImpl->argumentTypes[i];
                auto& arg = overload->astFunc->arguments[i];;
                tempTypes.resize(0);
                SignalIO result = CheckExpression(info, scopeId, arg.defaultValue,&tempTypes,false);
                //  DynamicArray<TypeId> temp{};
                // info.temp_defaultArgs.resize(0);
                // CheckExpression(info,func->scopeId, arg.defaultValue, &info.temp_defaultArgs, false);
                if(tempTypes.size()==0)
                    tempTypes.add(AST_VOID);
                if(!info.ast->castable(tempTypes.last(),argImpl.typeId)){
                // if(temp.last() != argImpl.typeId){
                    std::string deftype = info.ast->typeToString(info.temp_defaultArgs.last());
                    std::string argtype = info.ast->typeToString(argImpl.typeId);
                    ERR_SECTION(
                        ERR_HEAD2(arg.defaultValue->location)
                        ERR_MSG("Type of default value does not match type of argument.")
                        ERR_LINE2(arg.defaultValue->location,deftype.c_str())
                        ERR_LINE2(arg.location,argtype.c_str())
                    )
                    continue; // continue when failing
                }
            }

            FNCALL_SUCCESS
            return SIGNAL_SUCCESS;
        }
        // TODO: Implicit call to polymorphic functions. Currently throwing error instead.
        //  Should be done in generator too.
        if(fnOverloads->polyOverloads.size() == 0) {
            // Arguments for fname does not match an overload. These were the arguments:
            // These are the valid overloads: 
            ERR_SECTION(
                ERR_HEAD2(expr->location, ERROR_OVERLOAD_MISMATCH)
                // custom code for error message
                log::out << "Arguments for '"<<baseName <<"' does not match an overload.\n";
                ERR_LINE2(expr->location, "bad");
                log::out << "These were the arguments: ";
                if(argTypes.size()==0){
                    log::out << "zero arguments";
                } else {
                    log::out << "(";
                    for(int j=0;j<argTypes.size();j++){
                        auto& argType = argTypes[j];
                        if(j!=0)
                            log::out << ", ";
                        log::out << log::LIME << info.ast->typeToString(argType) << log::NO_COLOR;
                    }
                    log::out << ")";
                }
                log::out << "\n";
                log::out << "These are the valid overloads: ";

                // if(fnOverloads->overloads.size()!=1)
                //     log::out << "\n";

                // TODO: You may not want to print 10 overloads here. Limiting it to 3-5 might
                //  be a good idea. You could have an option in user profiles.
                for (int i = 0; i < fnOverloads->overloads.size();i++) {
                    auto& overload = fnOverloads->overloads[i];
                    log::out << "(";
                    for(int j=0;j<overload.funcImpl->argumentTypes.size();j++){
                        auto& argType = overload.funcImpl->argumentTypes[j];
                        if(j!=0)
                            log::out << ", ";
                        log::out << log::LIME << info.ast->typeToString(argType.typeId) << log::NO_COLOR;
                    }
                    log::out << "), ";
                    // log::out << ")\n";
                }
                log::out << "\n";

                if(fnOverloads->polyOverloads.size()!=0){
                    log::out << log::YELLOW<<"(implicit call to polymorphic function is not implemented)\n";
                }
                if(expr->args.size()!=0 && expr->args.get(0)->namedValue.ptr){
                    log::out << log::YELLOW<<"(named arguments cannot identify overloads)\n";
                }
                // if(expr->args->size()!=0 && expr->args->get(0)->namedValue.str){
                //     log::out << log::YELLOW<<"(named arguments cannot identify overloads)\n";
                // }
                log::out <<"\n";
                // TODO: show list of available overloaded function args
            ) 
            FNCALL_FAIL
            return SIGNAL_FAILURE;
        }
        implicitPoly = true;
        // if implicit polymorphism then
        // macth poly impls
        // generate poly impl if failed
        // use new impl if succeeded.
    }
    // code when we call polymorphic function

    // match args and poly args with poly impls
    // if match then return that impl
    // if not then try to generate a implementation
    
    // Assert(!expr->hasImplicitThis());
    // if(expr->hasImplicitThis()) {

    //     ERR_SECTION(
    //         ERR_HEAD2(expr->location, ERROR_OVERLOAD_MISMATCH)
    //         // custom code for error message
    //         log::out << "Arguments for '"<<baseName <<"' does not match an overload.\n";
    //         ERR_LINE2(expr->location, "bad");
    //         log::out << "These were the arguments: ";
    //         if(argTypes.size()==0){
    //             log::out << "zero arguments";
    //         } else {
    //             log::out << "(";
    //             for(int j=0;j<argTypes.size();j++){
    //                 auto& argType = argTypes[j];
    //                 if(j!=0)
    //                     log::out << ", ";
    //                 log::out << log::LIME << info.ast->typeToString(argType) << log::NO_COLOR;
    //             }
    //             log::out << ")";
    //         }
    //         log::out << "\n";
    //         log::out << "These are the valid overloads: ";

    //         // if(fnOverloads->overloads.size()!=1)
    //         //     log::out << "\n";

    //         // TODO: You may not want to print 10 overloads here. Limiting it to 3-5 might
    //         //  be a good idea. You could have an option in user profiles.
    //         for (int i = 0; i < fnOverloads->overloads.size();i++) {
    //             auto& overload = fnOverloads->overloads[i];
    //             log::out << "(";
    //             for(int j=0;j<overload.funcImpl->argumentTypes.size();j++){
    //                 auto& argType = overload.funcImpl->argumentTypes[j];
    //                 if(j!=0)
    //                     log::out << ", ";
    //                 log::out << log::LIME << info.ast->typeToString(argType.typeId) << log::NO_COLOR;
    //             }
    //             log::out << "), ";
    //             // log::out << ")\n";
    //         }
    //         log::out << "\n";

    //         if(fnOverloads->polyOverloads.size()!=0){
    //             log::out << log::YELLOW<<"(implicit call to polymorphic function is not implemented)\n";
    //         }
    //         if(expr->args.size()!=0 && expr->args.get(0)->namedValue.str){
    //             log::out << log::YELLOW<<"(named arguments cannot identify overloads)\n";
    //         }
    //         // if(expr->args->size()!=0 && expr->args->get(0)->namedValue.str){
    //         //     log::out << log::YELLOW<<"(named arguments cannot identify overloads)\n";
    //         // }
    //         log::out <<"\n";
    //         // TODO: show list of available overloaded function args
    //     ) 
    //     FNCALL_FAIL
    //     return SIGNAL_FAILURE;   
    // }
    
    // TODO: Allow implicit cast. Same as we do with normal functions.
    // TODO: Optimize by checking what in the overloads didn't match. If all parent structs are a bad match then
    //  we don't have we don't need to getOverload the second time with canCast=true
    FnOverloads::Overload* overload = fnOverloads->getOverload(info.ast, argTypes, fnPolyArgs, parentStructImpl, expr, implicitPoly);
    if(overload){
        overload->funcImpl->usages++;
        FNCALL_SUCCESS
        return SIGNAL_SUCCESS;
    }
    // bool useCanCast = false;
    overload = fnOverloads->getOverload(info.ast, argTypes, fnPolyArgs, parentStructImpl, expr, implicitPoly, true);
    if(overload){
        overload->funcImpl->usages++;
        FNCALL_SUCCESS
        return SIGNAL_SUCCESS;
    }

    // Hello!
    // The code below does not handle implicitThis.

    ASTFunction* polyFunc = nullptr;
    if(implicitPoly) {
        // IMPORTANT: Parent structs may not be handled properly.
        // Assert(!parentStructImpl);

        int lessArguments = 0;
        if(expr->hasImplicitThis())
            lessArguments = 1;
        // if(parentAstStruct) {
        //     // ignoring methods for the time being
        //     INCOMPLETE
        // }
        // What needs to be done compared to other code paths is that fnPolyArgs is empty and needs to be decided.
        // This can get complicated fn size<T>(a: Slice<T*>)
        DynamicArray<TypeId> choosenTypes{};
        DynamicArray<TypeId> realChoosenTypes{};
        FnOverloads::PolyOverload* polyOverload = nullptr;
        for(int i=0;i<(int)fnOverloads->polyOverloads.size();i++){
            FnOverloads::PolyOverload& overload = fnOverloads->polyOverloads[i];

            // if(parentStructImpl) {
            //     if(overload.funcImpl->structImpl->polyArgs.size() != polyArgs.size() /* && !implicitPoly */)
            //         continue;
            //     // I don't think implicitPoly should affects this
            //     for(int j=0;j<(int)parentStruct->polyArgs.size();j++){
            //         if(parentStruct->polyArgs[j] != overload.funcImpl->structImpl->polyArgs[j]){
            //             doesPolyArgsMatch = false;
            //             break;
            //         }
            //     }
            //     if(!doesPolyArgsMatch)
            //             continue;
            // }

            // continue if more args than possible
            // continue if less args than minimally required
            if(expr->nonNamedArgs > overload.astFunc->arguments.size() - lessArguments || 
                expr->nonNamedArgs < overload.astFunc->nonDefaults - lessArguments ||
                argTypes.size() > overload.astFunc->arguments.size() - lessArguments
                )
                continue;
            
            // What is the purpose of voiding virtual types?
            // The loop of nonNamedArgs doesn't transform virtuals.
            // If the type is a virtual then it handles it differently so
            // in theory we won't ever access the value virtualType->id (just virtualType->originalId)
            // It might do something important though.
            // - Emarioo, 2024-01-03
            // for(int j=0;j<(int)overload.astFunc->polyArgs.size();j++){
            //     overload.astFunc->polyArgs[j].virtualType->id = {};
            // }
            // if(parentAstStruct){
            //     for(int j=0;j<(int)parentAstStruct->polyArgs.size();j++){
            //         parentAstStruct->polyArgs[j].virtualType->id = {};
            //     }
            // }

            // IMPORTANT TODO: NAMESPACES ARE IGNORED AT THE MOMENT. HANDLE THEM!
            // TODO: Fix this code it does not work properly. Some behaviour is missing.
            bool found = true;
            for (u32 j=0;j<expr->nonNamedArgs;j++){
                TypeId typeToMatch = argTypes[j];
                // Assert(typeToMatch.isValid());
                // We begin by matching the first argument.
                // We can't use CheckType because it will create types and we need some more fine tuned checking.
                // We begin checking the base type (we ignore polymoprhism and pointer level)
                // If the arguments match then we have a primitive type. We don't need to decide poly arguments here and everything is fine.
                // If it didn't match then we check if the type is polymorphic. If so then we must decide now.

                ScopeId argScope = overload.astFunc->scopeId;

                TypeId stringType = overload.astFunc->arguments[j + lessArguments].stringType;
                auto typeString = info.ast->getStringFromTypeString(stringType);

                u32 plevel=0;
                polyTypes.resize(0);
                StringView typeName, baseToken;
                AST::DecomposePointer(typeString, &typeName, &plevel);
                AST::DecomposePolyTypes(typeName, &baseToken, &polyTypes);
                // namespace?

                // TypeId baseType = info.ast->convertToTypeId(baseToken, argScope, false);
                TypeInfo* typeInfo = info.ast->convertToTypeInfo(baseToken, argScope, false);
                TypeId baseType = typeInfo->id;
                // TypeInfo* typeInfo = info.ast->getTypeInfo(baseType);
                // if(typeInfo->id != baseType) {
                bool isVirtual = false;
                for(int k = 0;k<overload.astFunc->polyArgs.size();k++){
                    if(overload.astFunc->polyArgs[k].virtualType->originalId == typeInfo->originalId) {
                        isVirtual = true;
                        break;
                    }
                }
                if(isVirtual) {
                    if(polyTypes.size() != 0){
                        // This code should allow implicit calculation of incomplete polymorphic types.
                        // This sould be allowed: fn add<T>(a: T<i32>).
                        // But it's not so we get this: fn add<T>(a: T), no polymorphic types
                        INCOMPLETE
                    }
                    // polymorphic type! we must decide!
                    // check if type exists in choosen types
                    // if so then reuse, if not then use up a time
                    int typeIndex = -1;
                    for(int k=0;k<choosenTypes.size();k++){
                        // NOTE: The choosen poly type must match in full.
                        if(choosenTypes[k].baseType() == typeToMatch.baseType() &&
                            choosenTypes[k].getPointerLevel() + plevel == typeToMatch.getPointerLevel()
                        ) {
                            typeIndex = k;
                            break;
                        }
                    }
                    if(typeIndex !=-1){
                        // one of the choosen types fulfill the type to match
                        // no need to do anything
                        continue;
                    } else {
                        if(choosenTypes.size() >= overload.astFunc->polyArgs.size()) {
                            // no types remaining
                            found = false;
                            break;
                        }
                        TypeId newChoosen = typeToMatch;
                        if(typeToMatch.getPointerLevel() < plevel) {
                            found = false;
                            break;
                        }
                        newChoosen.setPointerLevel(typeToMatch.getPointerLevel() - plevel);
                        choosenTypes.add(newChoosen);

                        // size<T,K>(T* <K>*)
                        // size(u32**)
                    }
                } else {
                    // baseType isn't one of the polymorphic arguments
                    // we can't choose a polymorphic argument so things match
                    if(baseType.baseType() == typeToMatch.baseType()){
                        if(typeToMatch.getPointerLevel() != plevel || polyTypes.size() != 0) {
                            found = false; // pointers don't match or our baseType has some polyTokens which typeToMatch doesn't have.
                            break;
                        }
                        continue; // sweet, a simple match no polymorphic arguments to worry about
                    }
                    if(polyTypes.size() == 0){
                        found = false; // alright, types can't possibly ever match no matter what polymorphic arguments we choose.
                        break;
                    }
                    // crap, no match, more code

                    INCOMPLETE
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
                polyOverload = &overload;
                realChoosenTypes.stealFrom(choosenTypes);
                //break;
            }
        }
        if(polyFunc) {
            // We found a function
            fnPolyArgs.resize(polyFunc->polyArgs.size());
            for(int i = 0; i < polyFunc->polyArgs.size();i++) {
                if(i < realChoosenTypes.size()) {
                    fnPolyArgs[i] = realChoosenTypes[i];
                } else {
                    fnPolyArgs[i] = AST_VOID;
                }
            }
            //-- Double check so that the types we choose actually works.
            // Just in case the code for implicit types has bugs.
            // IMPORTANT: We also run CheckType in case types needs to be created.
            // if(parentAstStruct){
            //     for(int j=0;j<(int)parentAstStruct->polyArgs.size();j++){
            //         parentAstStruct->polyArgs[j].virtualType->id = parentStructImpl->polyArgs[j];
            //     }
            // }
            // for(int j=0;j<(int)polyOverload->astFunc->polyArgs.size();j++){
            //     polyOverload->astFunc->polyArgs[j].virtualType->id = fnPolyArgs[j];
            // }
            polyOverload->astFunc->pushPolyState(&fnPolyArgs, parentStructImpl);
            defer {
                polyOverload->astFunc->popPolyState();
                // for(int j=0;j<(int)polyOverload->astFunc->polyArgs.size();j++){
                //     polyOverload->astFunc->polyArgs[j].virtualType->id = {};
                // }
                // if(parentAstStruct){
                //     for(int j=0;j<(int)parentAstStruct->polyArgs.size();j++){
                //         parentAstStruct->polyArgs[j].virtualType->id = {};
                //     }
                // }
            };
            bool found = true;
            for (u32 j=0;j<expr->nonNamedArgs;j++){
                // log::out << "Arg:"<<info.ast->typeToString(overload.astFunc->arguments[j].stringType)<<"\n";
                TypeId argType = CheckType(info, polyOverload->astFunc->scopeId,polyOverload->astFunc->arguments[j].stringType,
                    polyOverload->astFunc->arguments[j].location,nullptr);
                // TypeId argType = CheckType(info, scope->scopeId,overload.astFunc->arguments[j].stringType,
                // log::out << "Arg: "<<info.ast->typeToString(argType)<<" = "<<info.ast->typeToString(argTypes[j])<<"\n";
                if(argType != argTypes[j]){
                    found = false;
                    break;
                }
            }
            Assert(found); // If function we thought would match doesn't then the code is incorrect.
        }
    } else {
        // IMPORTANT: Poly parent structs may not be handled properly!
        // Assert(!parentStructImpl);

        int lessArguments = 0;
        if(expr->hasImplicitThis())
            lessArguments = 1;
        // // Find possible polymorphic type and later create implementation for it
        for(int i=0;i<(int)fnOverloads->polyOverloads.size();i++){
            FnOverloads::PolyOverload& overload = fnOverloads->polyOverloads[i];
            if(overload.astFunc->polyArgs.size() != fnPolyArgs.size())
                continue;
            // continue if more args than possible
            // continue if less args than minimally required
            if(expr->nonNamedArgs > overload.astFunc->arguments.size() - lessArguments || 
                expr->nonNamedArgs < overload.astFunc->nonDefaults - lessArguments ||
                argTypes.size() > overload.astFunc->arguments.size() - lessArguments
                )
                continue;
            // if(parentAstStruct){
            //     for(int j=0;j<(int)parentAstStruct->polyArgs.size();j++){
            //         parentAstStruct->polyArgs[j].virtualType->id = parentStructImpl->polyArgs[j];
            //     }
            // }
            // for(int j=0;j<(int)overload.astFunc->polyArgs.size();j++){
            //     overload.astFunc->polyArgs[j].virtualType->id = fnPolyArgs[j];
            // }
            overload.astFunc->pushPolyState(&fnPolyArgs, parentStructImpl);
            defer {
                overload.astFunc->popPolyState();
                // for(int j=0;j<(int)overload.astFunc->polyArgs.size();j++){
                //     overload.astFunc->polyArgs[j].virtualType->id = {};
                // }
                // if(parentAstStruct){
                //     for(int j=0;j<(int)parentAstStruct->polyArgs.size();j++){
                //         parentAstStruct->polyArgs[j].virtualType->id = {};
                //     }
                // }
            };
            bool found = true;
            for (u32 j=0;j<expr->nonNamedArgs;j++){
                // log::out << "Arg:"<<info.ast->typeToString(overload.astFunc->arguments[j].stringType)<<"\n";
                TypeId argType = CheckType(info, overload.astFunc->scopeId,overload.astFunc->arguments[j+lessArguments].stringType,
                    overload.astFunc->arguments[j+lessArguments].location,nullptr);
                // TypeId argType = CheckType(info, scope->scopeId,overload.astFunc->arguments[j].stringType,
                // log::out << "Arg: "<<info.ast->typeToString(argType)<<" = "<<info.ast->typeToString(argTypes[j])<<"\n";
                if(!info.ast->castable(argTypes[j], argType)){
                    found = false;
                    break;
                }
                // if(argType != argTypes[j]){
                //     found = false;
                //     break;
                // }
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
    }
    if(!polyFunc){
        ERR_SECTION(
            ERR_HEAD2(expr->location, ERROR_OVERLOAD_MISMATCH)
            ERR_MSG_LOG(
                "Overloads for function '"<< log::LIME<< baseName << log::NO_COLOR <<"' does not match these argument(s): ";
                if(argTypes.size()==0){
                    log::out << "zero arguments";
                } else {
                    log::out << "(";
                    expr->printArgTypes(info.ast, argTypes);
                    log::out << ")";
                }
                
                log::out << "\n";
                log::out << log::YELLOW<<"(polymorphic functions could not be generated if there are any)\nTODO: Show list of available functions.\n";
                log::out << "\n"
            )
            ERR_LINE2(expr->location, "bad");
            // TODO: show list of available overloaded function args
        )
        FNCALL_FAIL
        return SIGNAL_FAILURE;
    }
    ScopeInfo* funcScope = info.ast->getScope(polyFunc->scopeId);
    FuncImpl* funcImpl = info.ast->createFuncImpl(polyFunc);
    // FuncImpl* funcImpl = polyFunc->createImpl();
    // funcImpl->name = expr->name;
    funcImpl->structImpl = parentStructImpl;

    funcImpl->polyArgs.resize(fnPolyArgs.size());

    for(int i=0;i<(int)fnPolyArgs.size();i++){
        TypeId id = fnPolyArgs[i];
        funcImpl->polyArgs[i] = id;
    }
    // TODO: What you are calling a struct method?  if (expr->boolvalue) do structmethod
    SignalIO result = CheckFunctionImpl(info,polyFunc,funcImpl,parentAstStruct, nullptr);
    // outTypes->used = 0; // FNCALL_SUCCESS will fix the types later, we don't want to add them twice

    FnOverloads::Overload* newOverload = fnOverloads->addPolyImplOverload(polyFunc, funcImpl);
    
    CheckInfo::CheckImpl checkImpl{};
    checkImpl.astFunc = polyFunc;
    checkImpl.funcImpl = funcImpl;
    funcImpl->usages++;
    info.checkImpls.add(checkImpl);

    // Can overload be null since we generate a new func impl?
    overload = fnOverloads->getOverload(info.ast, argTypes, fnPolyArgs, parentStructImpl, expr);
    if(!overload)
        overload = fnOverloads->getOverload(info.ast, argTypes, fnPolyArgs, parentStructImpl, expr, false, true);
    Assert(overload == newOverload);
    if(!overload){
        ERR_SECTION(
            ERR_HEAD2(expr->location, ERROR_OVERLOAD_MISMATCH)
            ERR_MSG_LOG("Specified polymorphic arguments does not match with passed arguments for call to '"<<baseName <<"'.\n";
                log::out << log::CYAN<<"Generates args: "<<log::NO_COLOR;
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
                log::out << "\n"
            )
            // TODO: show list of available overloaded function args
        )
        FNCALL_FAIL
        return SIGNAL_FAILURE;
    } 
    FNCALL_SUCCESS
    return SIGNAL_SUCCESS;
}
SignalIO CheckExpression(CheckInfo& info, ScopeId scopeId, ASTExpression* expr, QuickArray<TypeId>* outTypes, bool attempt){
    using namespace engone;
    ZoneScopedC(tracy::Color::Purple);
    Assert(expr);
    _TCLOG_ENTER(FUNC_ENTER)
    
    defer {
        if(outTypes && outTypes->size()==0){
            outTypes->add(AST_VOID);
        }
    };

    // IMPORTANT NOTE: CheckExpression HAS to run for left and right expressions
    //  since they may require types to be checked. AST_SIZEOF needs to evalute for example

    TINY_ARRAY(TypeId, typeArray, 1);

    // switch 
    if(expr->isValue) {
        if(expr->typeId == AST_ID){
            // NOTE: When changing this code, don't forget to do the same with AST_SIZEOF. It also has code for AST_ID.
            //   Some time later...
            //   Oh, nice comment! Good job me. - Emarioo, 2024-02-03
            
            // ScopeInfo* sc = info.ast->getScope(scopeId);
            // sc->print(info.ast);
            // TODO: What about enum?
            auto iden = info.ast->findIdentifier(scopeId, info.getCurrentOrder(), expr->name);
            if(iden){
                expr->identifier = iden;
                if(iden->type == Identifier::VARIABLE){
                    auto varinfo = info.ast->getVariableByIdentifier(iden);
                    if(varinfo){
                        if(outTypes) outTypes->add(varinfo->versions_typeId[info.currentPolyVersion]);
                    }
                } else if(iden->type == Identifier::FUNCTION) {
                    if(iden->funcOverloads.overloads.size() == 1) {
                        auto overload = &iden->funcOverloads.overloads[0];
                        CallConventions expected_convention = CallConventions::UNIXCALL;
                        if(info.compileInfo->compileOptions->target == TARGET_WINDOWS_x64)
                            expected_convention = CallConventions::STDCALL;
                        if(info.compileInfo->compileOptions->target == TARGET_UNIX_x64)
                            expected_convention = CallConventions::UNIXCALL;
                        if (overload->astFunc->callConvention != expected_convention) {
                            ERR_SECTION(
                                ERR_HEAD2(expr->location)
                                ERR_MSG("You can only take a reference from functions that use "<<ToString(expected_convention)<<" (calling convention). The system for function pointers is a temporary solution and can't support anything else. But it exists at least; making threads possible.")
                                ERR_LINE2(expr->location, "function must use stdcall")
                            )
                            if(outTypes) outTypes->add(AST_VOID);
                        } else {
                            if(overload->astFunc->body && overload->funcImpl->usages == 0){
                                CheckInfo::CheckImpl checkImpl{};
                                checkImpl.astFunc = overload->astFunc;
                                checkImpl.funcImpl = overload->funcImpl;
                                info.checkImpls.add(checkImpl);
                            }
                            overload->funcImpl->usages++;
                            if(outTypes) outTypes->add(AST_FUNC_REFERENCE);
                        }
                    } else {
                        ERR_SECTION(
                            ERR_HEAD2(expr->location)
                            ERR_MSG("Function is overloaded. Taking a reference would therefore be ambiguous. You must rename the function so that it only has one overload. There will be a better way to solve this in the future.")
                            ERR_LINE2(expr->location, "ambiguous")
                        )
                        if(outTypes) outTypes->add(AST_VOID);
                    }
                } else {
                    INCOMPLETE
                }
            } else {
                // Perhaps it's an enum member?
                int memberIndex = -1;
                ASTEnum* astEnum = nullptr;
                bool found = info.ast->findEnumMember(scopeId, expr->name, info.getCurrentOrder(), &astEnum, &memberIndex);
                if(found){
                    expr->enum_ast = astEnum;
                    expr->enum_member = memberIndex;
                    if(outTypes) outTypes->add(astEnum->actualType);
                    return SIGNAL_SUCCESS;
                } else if(astEnum) {
                    ERR_SECTION(
                        ERR_HEAD2(expr->location, ERROR_UNDECLARED)
                        ERR_MSG("'"<<expr->name<<"' is not declared but a member of the enum '"<<astEnum->name<<"' could have matched if it wasn't @enclosed.")
                        ERR_LINE2(astEnum->members[memberIndex].location,"could have matched")
                        ERR_LINE2(expr->location,"undeclared")
                    )
                } else {
                    ERR_SECTION(
                        ERR_HEAD2(expr->location, ERROR_UNDECLARED)
                        ERR_MSG("'"<<expr->name<<"' is not a declared variable.")
                        ERR_LINE2(expr->location,"undeclared")
                    )
                }
                return SIGNAL_FAILURE;
            }
        } else if(expr->typeId == AST_FNCALL){
            CheckFncall(info,scopeId,expr, outTypes, attempt, false);
        } else if(expr->typeId == AST_STRING){
            u32 index=0;
            auto constString = info.ast->getConstString(expr->name,&index);
            // Assert(constString);
            expr->versions_constStrIndex[info.currentPolyVersion] = index;

            if(expr->flags & ASTNode::NULL_TERMINATED) {
                TypeId theType = CheckType(info, scopeId, "char*", expr->location, nullptr);
                Assert(theType.isValid()); if(outTypes) outTypes->add(theType);
            } else {
                TypeId theType = CheckType(info, scopeId, "Slice<char>", expr->location, nullptr);
                Assert(theType.isValid());
                if(outTypes) outTypes->add(theType);
            } 
        } else if(expr->typeId == AST_SIZEOF || expr->typeId == AST_NAMEOF || expr->typeId == AST_TYPEID) {
            Assert(expr->left);
            TypeId finalType = {};
            if(expr->left->typeId == AST_ID){
                // AST_ID could result in a type or a variable
                Identifier* iden = nullptr;
                auto& name = expr->left->name;
                // TODO: Handle function pointer type
                // This code may need to update when code for AST_ID does

                // if(IsName(name) && (iden = ...   nocheckin, I REMOVED 'IsName', IN THE 2.1 REWRITE WAS THAT OKAY?
                if((iden = info.ast->findIdentifier(scopeId, info.getCurrentOrder(), name)) && iden->type==Identifier::VARIABLE){
                    auto var = info.ast->getVariableByIdentifier(iden);
                    Assert(var);
                    if(var){
                        finalType = var->versions_typeId[info.currentPolyVersion];
                    }
                } else {
                    // auto sc = info.ast->getScope(scopeId);
                    // sc->print(info.ast);
                    finalType = CheckType(info, scopeId, name, expr->location, nullptr);
                }
            }
            if(!finalType.isValid()) {
                // DynamicArray<TypeId> temps{};
                typeArray.resize(0);
                SignalIO result = CheckExpression(info, scopeId, expr->left, &typeArray, attempt);
                finalType = typeArray.size()==0 ? AST_VOID : typeArray.last();
            }
            if(finalType.isValid()) {
                if(expr->typeId == AST_SIZEOF) {
                    expr->versions_outTypeSizeof[info.currentPolyVersion] = finalType;
                    if(outTypes)  outTypes->add(AST_UINT32);
                } else if(expr->typeId == AST_NAMEOF) {
                    std::string name = info.ast->typeToString(finalType);
                    u32 index=0;
                    auto constString = info.ast->getConstString(name,&index);
                    // Assert(constString);
                    expr->versions_constStrIndex[info.currentPolyVersion] = index;
                    // expr->constStrIndex = index;

                    TypeId theType = CheckType(info, scopeId, "Slice<char>", expr->location, nullptr);
                    if(outTypes)  outTypes->add(theType);
                } else if(expr->typeId == AST_TYPEID) {
                    
                    expr->versions_outTypeTypeid[info.currentPolyVersion] = finalType;
                    
                    const char* tname = "lang_TypeId";
                    TypeId theType = CheckType(info, scopeId, tname, expr->location, nullptr);
                    if(!theType.isValid()) {
                        ERR_SECTION(
                            ERR_HEAD2(expr->location)
                            ERR_MSG("'"<<tname << "' was not a valid type. Did you forget to #import \"Lang\".")
                            ERR_LINE2(expr->location, "bad")
                        )
                    }
                    if(outTypes)  outTypes->add(theType);
                }
            } else {
                if(outTypes) outTypes->add(AST_VOID);
            }
        } else if(expr->typeId == AST_ASM){
            // Polymorphism may cause duplicated inline assembly.
            Assert(info.currentPolyVersion==0);
            // TODO: Check variables?
            if(outTypes)
                outTypes->add(AST_VOID);
        } else if(expr->typeId == AST_NULL){
            // u32 index=0;
            // auto constString = info.ast->getConstString(expr->name,&index);
            // // Assert(constString);
            // expr->versions_constStrIndex[info.currentPolyVersion] = index;

            // TypeId theType = CheckType(info, scopeId, "Slice<char>", expr->location, nullptr);
            TypeId theType = AST_VOID;
            theType.setPointerLevel(1);
            if(outTypes)
                outTypes->add(theType);
        } else {
            Assert(expr->typeId.getId() < AST_PRIMITIVE_COUNT);
            if(outTypes) outTypes->add(expr->typeId);
        }
        // if(outTypes) outTypes->add(expr->typeId);
    } else {
        TypeId leftType{};
        TypeId rightType{};
        if(expr->typeId != AST_REFER && expr->typeId != AST_DEREF && expr->typeId != AST_MEMBER&& expr->typeId != AST_CAST && expr->typeId != AST_INITIALIZER) {
            TINY_ARRAY(TypeId, operatorArgs, 2);
            if(expr->left) {
                typeArray.resize(0);
                CheckExpression(info,scopeId, expr->left, &typeArray, attempt);
                Assert(typeArray.size()<2); // error message
                if(typeArray.size()>0) {
                    leftType = typeArray.last();
                    operatorArgs.add(typeArray.last());
                }
            }
            if(expr->right) {
                typeArray.resize(0);
                CheckExpression(info,scopeId, expr->right, &typeArray, attempt);
                Assert(typeArray.size()<2); // error message
                if(typeArray.size()>0) {
                    rightType = typeArray.last();
                    operatorArgs.add(typeArray.last());
                }
            }
            // TODO: You should not be allowed to overload all operators.
            //  Fix some sort of way to limit which ones you can.
            const char* str = OP_NAME(expr->typeId.getId());
            if(str && operatorArgs.size() == 2) {
                
                expr->nonNamedArgs = 2; // unless operator overloading <- what do i mean by this - Emarioo 2023-12-19
                SignalIO result = CheckFncall(info,scopeId,expr, outTypes, attempt, true, &operatorArgs);
                
                if(result == SIGNAL_SUCCESS)
                    return SIGNAL_SUCCESS;

                if(result != SIGNAL_NO_MATCH)
                    return result;
            }
        }
        switch(expr->typeId.getId()) {
        case AST_REFER: {
            if(expr->left) {
                CheckExpression(info,scopeId, expr->left, outTypes, attempt);
                if(outTypes) {
                    if(outTypes->size()>0)
                        outTypes->last().setPointerLevel(outTypes->last().getPointerLevel()+1);
                }
            }
            break;
        }
        case AST_DEREF: {
            if(expr->left) {
                CheckExpression(info,scopeId, expr->left, outTypes, attempt);
                if(outTypes) {
                    if(outTypes->last().getPointerLevel()==0){
                        ERR_SECTION(
                            ERR_HEAD2(expr->left->location)
                            ERR_MSG("Cannot dereference non-pointer.")
                            ERR_LINE2(expr->left->location,info.ast->typeToString(outTypes->last()));
                        )
                    }else{
                        outTypes->last().setPointerLevel(outTypes->last().getPointerLevel()-1);
                    }
                }
            }
            break;
        }
        case AST_MEMBER: {
            Assert(expr->left);
            if(expr->left->typeId == AST_ID){
                // TODO: Generator passes idScope. What is it and is it required in type checker?
                // A simple check to see if the identifier in the expr node is an enum type.
                // no need to check for pointers or so.
                TypeInfo *typeInfo = info.ast->convertToTypeInfo(expr->left->name, scopeId, true);
                if (typeInfo && typeInfo->astEnum) {
                    i32 enumValue;
                    bool found = typeInfo->astEnum->getMember(expr->name, &enumValue);
                    if (!found) {
                        ERR_SECTION(
                            ERR_HEAD2(expr->location)
                            ERR_MSG("'"<<expr->name << "' is not a member of enum " << typeInfo->astEnum->name <<".")
                        )
                        return SIGNAL_FAILURE;
                    }

                    if(outTypes) outTypes->add(typeInfo->id);
                    return SIGNAL_SUCCESS;
                }
            }
            // DynamicArray<TypeId> typeArray{};
            // TINY_ARRAY(TypeId, typeArray, 1);
            if(expr->left) {
                // Operator overload checking does not run on AST_MEMBER since it can't be overloaded
                // leftType has therefore note been set and expression not checked.
                // We must check it here.
                CheckExpression(info,scopeId, expr->left, &typeArray, attempt);
                if(typeArray.size()>0)
                    leftType = typeArray.last();
            }

            // if(typeArray.size()==0) 
            //     typeArray.add(AST_VOID);
            // outType holds the type of expr->left
            TypeInfo* ti = info.ast->getTypeInfo(leftType.baseType());
            Assert(leftType.getPointerLevel()<2);
            if(ti && ti->astStruct){
                TypeInfo::MemberData memdata = ti->getMember(expr->name);
                if(memdata.index!=-1){
                    if(outTypes)
                        outTypes->add(memdata.typeId);
                } else {
                    std::string msgtype = "not member of "+info.ast->typeToString(leftType);
                    ERR_SECTION(
                        ERR_HEAD2(expr->location)
                        ERR_MSG_LOG("'"<<expr->name<<"' is not a member in struct '"<<info.ast->typeToString(leftType)<<"'.";
                            log::out << "These are the members: ";
                            for(int i=0;i<(int)ti->astStruct->members.size();i++){
                                if(i!=0)
                                    log::out << ", ";
                                log::out << log::LIME << ti->astStruct->members[i].name<<log::NO_COLOR<<": "<<info.ast->typeToString(ti->getMember(i).typeId);
                            }
                            log::out <<"\n";
                            log::out << "\n"
                        )
                        ERR_LINE2(expr->location,msgtype.c_str());
                    )
                    if(outTypes)
                        outTypes->add(AST_VOID);
                    return SIGNAL_FAILURE;
                }
            } else {
                ERR_SECTION(
                    ERR_HEAD2(expr->location)
                    ERR_MSG("Member access only works on variable with a struct type and enums. The type '" << info.ast->typeToString(typeArray.last()) << "' is neither (astStruct/astEnum were null).")
                    ERR_LINE2(expr->left->location,"cannot take a member from this")
                    ERR_LINE2(expr->location,"member to access")
                )
                if(outTypes)
                    outTypes->add(AST_VOID);
                return SIGNAL_FAILURE;
            }
            break;
        }
        case AST_INDEX: {
            // TODO: THE COMMENTED CODE CAN BE REMOVED. IT'S OLD. SOMETHING WITH OPERATOR OVERLOADING
            // BUT WE DO THAT ABOVE.
            // TINY_ARRAY(TypeId, operatorArgs, 2);
            
            // TypeId leftType = {}, rightType = {};
            // if(expr->left) {
            //     typeArray.resize(0);
            //     CheckExpression(info,scopeId, expr->left, &typeArray, attempt);
            //     Assert(typeArray.size()<2); // error message
            //     if(typeArray.size()>0) {
            //         leftType = typeArray.last();
            //         operatorArgs.add(typeArray.last());
            //     }
            // }
            // if(expr->right) {
            //     typeArray.resize(0);
            //     CheckExpression(info,scopeId, expr->right, &typeArray, attempt);
            //     Assert(typeArray.size()<2); // error message
            //     if(typeArray.size()>0) {
            //         rightType = typeArray.last();
            //         operatorArgs.add(typeArray.last());
            //     }
            // }
            // if(expr->left) {
            //     CheckExpression(info,scopeId, expr->left, &typeArray, attempt);
            //     if(typeArray.)
            //     // if(typeArray.size()==0){
            //     //     typeArray.add(AST_VOID);
            //     // }
            // }
            // if(expr->right) {
            //     CheckExpression(info,scopeId, expr->right, nullptr, attempt);
            // }
            // if(expr->left && expr->right) {
            //     expr->nonNamedArgs = 2; // unless operator overloading
            //     SignalIO result = CheckFncall(info,scopeId,expr, outTypes, attempt, true, &operatorArgs);
                
            //     if(result == SIGNAL_SUCCESS)
            //         return SIGNAL_SUCCESS;

            //     if(result != SIGNAL_NO_MATCH)
            //         return result;
            // }
            // DynamicArray<TypeId> typeArray{};
            
            // if(outTypes) outTypes->add(AST_VOID);
            // DynamicArray<TypeId> temp{};
            if(leftType.getPointerLevel()==0){
                if(!attempt) {
                    ERR_SECTION(
                        ERR_HEAD2(expr->left->location)
                        ERR_MSG("Cannot index non-pointer.")
                        ERR_LINE2(expr->left->location,info.ast->typeToString(leftType))
                    )
                    return SIGNAL_FAILURE;
                }
                return SIGNAL_NO_MATCH;
            }else{
                if(outTypes){
                    outTypes->add(leftType);
                    outTypes->last().setPointerLevel(outTypes->last().getPointerLevel()-1);
                }
            }
            break;
        }
        case AST_RANGE: {
            // DynamicArray<TypeId> temp={};
            // if(expr->left) {
            //     CheckExpression(info,scopeId, expr->left, nullptr, attempt);
            // }
            // if(expr->right) {
            //     CheckExpression(info,scopeId, expr->right, nullptr, attempt);
            // }
            TypeId theType = CheckType(info, scopeId, "Range", expr->location, nullptr);
            if(outTypes) outTypes->add(theType);
            break;
        }
        case AST_INITIALIZER: {
            // Assert(expr->args);
            // for(auto now : *expr->args){
            for(auto arg_expr : expr->args){
                // DynamicArray<TypeId> temp={};
                CheckExpression(info, scopeId, arg_expr, nullptr, attempt);
            }
            auto ti = CheckType(info, scopeId, expr->castType, expr->location, nullptr);
            if(!ti.isValid()) {
                ERR_SECTION(
                    ERR_HEAD2(expr->location)
                    ERR_MSG("'"<<info.ast->typeToString(expr->castType)<<"' is not a type.")
                    ERR_LINE2(expr->location,"bad")
                )
                return SIGNAL_FAILURE;
            }
            if(outTypes)
                outTypes->add(ti);
            expr->versions_castType[info.currentPolyVersion] = ti;
            break;
        }
        case AST_CAST: {
            // DynamicArray<TypeId> temp{};
            Assert(expr->left);
            CheckExpression(info,scopeId, expr->left, &typeArray, attempt);
            Assert(typeArray.size()==1);

            Assert(expr->castType.isString());
            bool printedError = false;
            auto ti = CheckType(info, scopeId, expr->castType, expr->location, &printedError);
            if (ti.isValid()) {

            } else if(!printedError){
                ERR_SECTION(
                    ERR_HEAD2(expr->location)
                    ERR_MSG("Type "<<info.ast->getStringFromTypeString(expr->castType) << " does not exist.")
                    ERR_LINE2(expr->location,"bad")
                )
            }
            if(outTypes)
                outTypes->add(ti);
            expr->versions_castType[info.currentPolyVersion] = ti;
            
            if(expr->isUnsafeCast()) {
                if(expr->left->typeId == AST_ASM) {
                    // We don't know what type ASM generates so we always allow it.

                    // TODO: Deprecate this and use asm<i32> {} instead of cast_unsafe<i32> asm {}. asm -> i32 {} is an alternative syntax
                    //  asm<i32> makes more since because the casting is a little special since
                    //  we don't know what type the inline assembly produces. Maybe it does 2 pushes
                    //  or none. With unsafe cast we assume a type which isn't ideal.
                    //  Unfortunately, it's difficult to know what type is pushed in the inline assembly.
                    //  It might ruin the stack and frame pointers.
                    
                    // The unsafe cast implies that the asm block did this. hopefully it did.
                } else {
                    int ls = info.ast->getTypeSize(ti);
                    int rs = info.ast->getTypeSize(typeArray.last());
                    if(ls != rs) {
                        std::string strleft = info.ast->typeToString(typeArray.last()) + " ("+std::to_string(ls)+")";
                        std::string strcast = info.ast->typeToString(ti)+ " ("+std::to_string(rs)+")";
                        ERR_SECTION(
                            ERR_HEAD2(expr->location, ERROR_CASTING_TYPES)
                            ERR_MSG("cast_unsafe requires that both types have the same size. "<<ls << " != "<<rs<<"'.")
                            ERR_LINE2(expr->left->location,strleft)
                            ERR_LINE2(expr->location,strcast)
                            ERR_EXAMPLE_TINY("cast<void*> cast<u64> number")
                        )
                    }
                }
            } else if (expr->left->typeId == AST_ASM) {
                ERR_SECTION(
                    ERR_HEAD2(expr->location, )
                    ERR_MSG("You must use cast_unsafe to cast an inline assembly block. BE VERY careful as you are likely to make mistakes.")
                    ERR_LINE2(expr->location,"use cast_unsafe instead?")
                    ERR_EXAMPLE_TINY("cast_unsafe<i32> asm { mov eax, 23 }")
                )
            } else if(!info.ast->castable(typeArray.last(),ti)){
                std::string strleft = info.ast->typeToString(typeArray.last());
                std::string strcast = info.ast->typeToString(ti);
                ERR_SECTION(
                    ERR_HEAD2(expr->location, ERROR_CASTING_TYPES)
                    ERR_MSG("'"<<strleft << "' cannot be casted to '"<<strcast<<"'. Perhaps you can cast to a type that can be casted to the type you want?")
                    ERR_LINE2(expr->left->location,strleft)
                    ERR_LINE2(expr->location,strcast)
                    ERR_EXAMPLE_TINY("cast<void*> cast<u64> number")
                )
            }
            break;
        }
        case AST_ASSIGN: {
            if(outTypes) {
                outTypes->add(leftType);
            }
            break;
        }
        case AST_ADD:
        case AST_SUB:
        case AST_MUL:
        case AST_DIV:
        case AST_MODULUS:
        case AST_UNARY_SUB: {
            if(outTypes) {
                if(!rightType.isValid() || rightType == AST_VOID) {
                    outTypes->add(leftType); // unary operator
                } else if(leftType.isPointer() && AST::IsInteger(rightType)){
                    outTypes->add(leftType);
                } else if (AST::IsInteger(rightType) && rightType.isPointer()) {
                    outTypes->add(rightType);
                } else if ((AST::IsInteger(leftType) || leftType == AST_CHAR) && (AST::IsInteger(rightType) || rightType == AST_CHAR)){
                    u8 lsize = info.ast->getTypeSize(leftType);
                    u8 rsize = info.ast->getTypeSize(rightType);
                    outTypes->add(lsize > rsize ? leftType : rightType);
                } else if ((AST::IsDecimal(leftType) || AST::IsInteger(leftType)) && (AST::IsDecimal(rightType) || AST::IsInteger(rightType))){
                    if(AST::IsDecimal(leftType))
                        outTypes->add(leftType);
                    else
                        outTypes->add(rightType);
                } else {
                    outTypes->add(leftType);
                }
            }
            break;
        }
        case AST_EQUAL:
        case AST_NOT_EQUAL:
        case AST_LESS:
        case AST_GREATER:
        case AST_LESS_EQUAL:
        case AST_GREATER_EQUAL:
        case AST_AND:
        case AST_OR:
        case AST_NOT: {
            if(outTypes) {
                outTypes->add(TypeId(AST_BOOL));
            }
            break;
        }
        case AST_BAND:
        case AST_BOR:
        case AST_BXOR: {
            if(leftType != rightType) {
                ERR_SECTION(
                    ERR_HEAD2(expr->location)
                    ERR_MSG("Left and right type must be the same for binary AND, OR, XOR operations. The types were not the same.")
                    ERR_LINE2(expr->left->location, info.ast->typeToString(leftType))
                    ERR_LINE2(expr->right->location, info.ast->typeToString(rightType))
                )
                if(outTypes) {
                    // TODO: output poison type
                    // outTypes->add(leftType);
                }
            } else {
                if(outTypes) {
                    outTypes->add(leftType);
                }
            }
            break;
        }
        case AST_BNOT:
        case AST_BLSHIFT:
        case AST_BRSHIFT: {
            if(outTypes) {
                outTypes->add(leftType);
            }
            break;
        }
        case AST_INCREMENT:
        case AST_DECREMENT: {
            if(outTypes) {
                outTypes->add(leftType);
            }
            break;
        }
        default: {
            Assert(false);
        }
        }
    }
    return SIGNAL_SUCCESS;
}

// Evaluates types and offset for the given function implementation
// It does not modify ast func
SignalIO CheckFunctionImpl(CheckInfo& info, ASTFunction* func, FuncImpl* funcImpl, ASTStruct* parentStruct, QuickArray<TypeId>* outTypes){
    using namespace engone;
    ZoneScopedC(tracy::Color::Purple4);
    _TCLOG_ENTER(FUNC_ENTER)

    Assert(funcImpl->polyArgs.size() == func->polyArgs.size());
    // for(int i=0;i<(int)funcImpl->polyArgs.size();i++){
    //     TypeId id = funcImpl->polyArgs[i];
    //     Assert(id.isValid());
    //     func->polyArgs[i].virtualType->id = id;
    // }
    // if(funcImpl->structImpl){
    //     Assert(parentStruct);
    //     // Assert funcImpl->structImpl must come from parentStruct
    //     for(int i=0;i<(int)funcImpl->structImpl->polyArgs.size();i++){
    //         TypeId id = funcImpl->structImpl->polyArgs[i];
    //         Assert(id.isValid());
    //         parentStruct->polyArgs[i].virtualType->id = id;
    //     }
    // }
    Assert(func->parentStruct == parentStruct);
    func->pushPolyState(funcImpl);
    defer {
        func->popPolyState();
        // for(int i=0;i<(int)funcImpl->polyArgs.size();i++){
        //     func->polyArgs[i].virtualType->id = {};
        // }
        // if(funcImpl->structImpl){
        //     Assert(parentStruct);
        //     for(int i=0;i<(int)funcImpl->structImpl->polyArgs.size();i++){
        //         TypeId id = funcImpl->structImpl->polyArgs[i];
        //         parentStruct->polyArgs[i].virtualType->id = {};
        //     }
        // }
    };

    _TCLOG(log::out << "Check func impl "<< funcImpl->astFunction->name<<"\n";)

    // TODO: parentStruct argument may not be necessary since this function only calculates
    //  offsets of arguments and return values.

    // TODO: Handle calling conventions

    funcImpl->argumentTypes.resize(func->arguments.size());
    funcImpl->returnTypes.resize(func->returnValues.size());

    int offset = 0; // offset starts before call frame (fp, pc)

    // Based on 8-bit alignment. The last argument must be aligned by it.
    for(int i=0;i<(int)func->arguments.size();i++){
        auto& arg = func->arguments[i];
        auto& argImpl = funcImpl->argumentTypes[i];
        if(arg.stringType.isString()){
            bool printedError = false;
            // Token token = info.ast->getStringFromTypeString(arg.typeId);
            auto ti = CheckType(info, func->scopeId, arg.stringType, func->location, &printedError);
            
            if(ti == AST_VOID){
                if(!info.hasErrors()) { 
                    std::string msg = info.ast->typeToString(arg.stringType);
                    ERR_SECTION(
                        ERR_HEAD2(func->location)
                        ERR_MSG("Type '"<<msg<<"' is unknown. Did you misspell or forget to declare the type?")
                        ERR_LINE2(func->arguments[i].location,msg.c_str())
                    )
                }
            } else if(ti.isValid()){

            } else if(!printedError) {
                if(!info.hasErrors()) { 
                    std::string msg = info.ast->typeToString(arg.stringType);
                    ERR_SECTION(
                        ERR_HEAD2(func->location)
                        ERR_MSG("Type '"<<msg<<"' is unknown. Did you misspell it or is the compiler messing with you?")
                        ERR_LINE2(func->arguments[i].location,msg.c_str())
                    )
                }
            }
            argImpl.typeId = ti;
        } else {
            argImpl.typeId = arg.stringType;
        }

        // NOTE: We check the default value at the call site every time. Checking the expression requires the content
        //   order number and stack to be setup which it isn't here. We handle that as an edge cause but we also need to
        //   know polymorphic types and I guess we are checking the implementation so it should be set up but perhaps
        //   it's just better to check at call site. Easier at least.
        // if(arg.defaultValue){
        //      DynamicArray<TypeId> temp{};
        //     info.temp_defaultArgs.resize(0);
        //     CheckExpression(info,func->scopeId, arg.defaultValue, &info.temp_defaultArgs, false);
        //     if(info.temp_defaultArgs.size()==0)
        //         info.temp_defaultArgs.add(AST_VOID);
        //     if(!info.ast->castable(info.temp_defaultArgs.last(),argImpl.typeId)){
        //     // if(temp.last() != argImpl.typeId){
        //         std::string deftype = info.ast->typeToString(info.temp_defaultArgs.last());
        //         std::string argtype = info.ast->typeToString(argImpl.typeId);
        //         ERR_SECTION(
        //             ERR_HEAD2(arg.defaultValue->location)
        //             ERR_MSG("Type of default value does not match type of argument.")
        //             ERR_LINE2(arg.defaultValue->location,deftype.c_str())
        //             ERR_LINE2(arg.name,argtype.c_str())
        //         )
        //         continue; // continue when failing
        //     }
        // }
            
        int size = info.ast->getTypeSize(argImpl.typeId);
        int asize = info.ast->getTypeAlignedSize(argImpl.typeId);
        // Assert(size != 0 && asize != 0);
        // Actually, don't continue here. argImpl.offset shouldn't be uninitialized.
        if(size ==0 || asize == 0) // Probably due to an error which was logged. We don't want to assert and crash the compiler.
            continue;
        // if(asize!=0){
        if((offset%asize) != 0){
            offset += asize - offset%asize;
        }
        // }
        argImpl.offset = offset;
        // log::out << " Arg "<<arg.offset << ": "<<arg.name<<" ["<<size<<"]\n";
        offset += size;
    }
    int diff = offset%8;
    if(diff!=0)
        offset += 8-diff; // padding to ensure 8-byte alignment

    // log::out << "total size "<<offset<<"\n";
    // reverse
    // for(int i=0;i<(int)func->arguments.size();i++){
    //     // auto& arg = func->arguments[i];
    //     auto& argImpl = funcImpl->argumentTypes[i];
    //     // TypeInfo* typeInfo = info.ast->getTypeInfo(arg.typeId);
    //     int size = info.ast->getTypeSize(argImpl.typeId);
    //     argImpl.offset = offset - argImpl.offset - size;
    //     // log::out << " Reverse Arg "<<arg.offset << ": "<<arg.name<<"\n";
    // }
    funcImpl->argSize = offset;

    // return values should also have 8-bit alignment but since the call frame already
    // is aligned there is no need for any special stuff here.
    //(note that the special code would exist where functions are generated and not here)
    offset = 0;
    if(outTypes){
        Assert(outTypes->size()==0);
    }
    
    for(int i=0;i<(int)funcImpl->returnTypes.size();i++){
        auto& retImpl = funcImpl->returnTypes[i];
        auto& retStringType = func->returnValues[i].stringType;
        // TypeInfo* typeInfo = 0;
        if(retStringType.isString()){
            bool printedError = false;
            auto ti = CheckType(info, func->scopeId, retStringType, func->location, &printedError);
            if(ti.isValid()){
            }else if(!printedError) {
                ERR_SECTION(
                    ERR_HEAD2(func->returnValues[i].location)
                    ERR_MSG_LOG(
                        "'"<<info.ast->getStringFromTypeString(retStringType)<<"' is not a type.";
                        // #ifdef DEBUG
                        log::out << " ("<<__func__<<")"; 
                        // #endif
                        log::out << "\n";
                    )
                    ERR_LINE2(func->returnValues[i].location,"bad");
                )
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
        
        if ((offset)%asize != 0){
            offset += asize - (offset)%asize;
        }
        retImpl.offset = offset;
        offset += size;
        // log::out << " Ret "<<ret.offset << ": ["<<size<<"]\n";
        if(outTypes)
            outTypes->add(retImpl.typeId);
    }
    if((offset)%8!=0)
        offset += 8-(offset)%8; // padding to ensure 8-byte alignment

    // for(int i=0;i<(int)funcImpl->returnTypes.size();i++){
    //     auto& ret = funcImpl->returnTypes[i];
    //     // TypeInfo* typeInfo = info.ast->getTypeInfo(arg.typeId);
    //     int size = info.ast->getTypeSize(ret.typeId);
    //     ret.offset = ret.offset - offset;
    //     // log::out << " Reverse Arg "<<arg.offset << ": "<<arg.name<<"\n";
    // }
    funcImpl->returnSize = offset;

    if(outTypes && funcImpl->returnTypes.size()==0){
        outTypes->add(AST_VOID);
    }
    return SIGNAL_SUCCESS;
}
// Ensures that the function identifier exists.
// Adds the overload for the function.
// Checks if an existing overload would collide with the new overload.
SignalIO CheckFunction(CheckInfo& info, ASTFunction* function, ASTStruct* parentStruct, ASTScope* scope){
    using namespace engone;
    _TCLOG_ENTER(FUNC_ENTER)
    
    // log::out << "CheckFunction "<<function->name<<"\n";
    if(parentStruct){
        for(int i=0;i<(int)parentStruct->polyArgs.size();i++){
            for(int j=0;j<(int)function->polyArgs.size();j++){
                if(parentStruct->polyArgs[i].name == function->polyArgs[j].name){
                    ERR_SECTION(
                        ERR_HEAD2(function->polyArgs[j].location)
                        ERR_MSG("Name for polymorphic argument is already taken by the parent struct.")
                        ERR_LINE2(parentStruct->polyArgs[i].location,"taken")
                        ERR_LINE2(function->polyArgs[j].location,"unavailable")
                    )
                }
            }
        }
    }

    // Create virtual types
    for(int i=0;i<(int)function->polyArgs.size();i++){
        auto& arg = function->polyArgs[i];
        arg.virtualType = info.ast->createType(arg.name, function->scopeId);
        // _TCLOG(log::out << "Virtual type["<<i<<"] "<<arg.name<<"\n";)
    }
    // defer {
    //     for(int i=0;i<(int)function->polyArgs.size();i++){
    //         auto& arg = function->polyArgs[i];
    //         arg.virtualType->id = {};
    //     }
    // };
    // _TCLOG(log::out << "Method/function has polymorphic properties: "<<function->name<<"\n";)
    FnOverloads* fnOverloads = nullptr;
    Identifier* iden = nullptr;
    if(parentStruct){
        fnOverloads = parentStruct->getMethod(function->name, true);
    } else {
        iden = info.ast->findIdentifier(scope->scopeId, CONTENT_ORDER_MAX, function->name);
        if(!iden){
            iden = info.ast->addIdentifier(scope->scopeId, function->name, CONTENT_ORDER_ZERO, nullptr);
            iden->type = Identifier::FUNCTION;
        }
        if(iden->type != Identifier::FUNCTION){
            ERR_SECTION(
                ERR_HEAD2(function->location)
                ERR_MSG("'"<< function->name << "' is already defined as a non-function.")
            )
            return SIGNAL_FAILURE;
        }
        fnOverloads = &iden->funcOverloads;
        function->identifier = iden;
    }
    for(int i=0;i<(int)function->arguments.size();i++){
        auto& arg = function->arguments[i];
        auto var = info.ast->addVariable(function->scopeId, arg.name, CONTENT_ORDER_ZERO, &arg.identifier, nullptr);
    }
    if(parentStruct) {
        function->memberIdentifiers.resize(parentStruct->members.size());
        for(int i=0;i<(int)parentStruct->members.size();i++){
            auto& mem = parentStruct->members[i];
            auto varinfo = info.ast->addVariable(function->scopeId, mem.name, CONTENT_ORDER_ZERO, &function->memberIdentifiers[i], nullptr);
            if(varinfo){
                varinfo->type = VariableInfo::MEMBER;
            }
        }
    }
    // if(function->name == "std_print") {
    //     int a = 9;
    // }
    if(function->polyArgs.size()==0 && (!parentStruct || parentStruct->polyArgs.size() == 0)){
        // The code below is used to 
        // Acquire identifiable arguments
        DynamicArray<TypeId> argTypes{};
        for(int i=0;i<(int)function->arguments.size();i++){
            // info.ast->printTypesFromScope(function->scopeId);

            TypeId typeId = CheckType(info, function->scopeId, function->arguments[i].stringType, function->location, nullptr);
            // Assert(typeId.isValid());
            if(!typeId.isValid()){
                std::string msg = info.ast->typeToString(function->arguments[i].stringType);
                ERR_SECTION(
                    ERR_HEAD2(function->arguments[i].location)
                    ERR_MSG("Unknown type '"<<msg<<"' for parameter '"<<function->arguments[i].name<<"'" << ((function->parentStruct && i==0) ? " (this parameter is auto-generated for methods)" : "")<<".")
                    ERR_LINE2(function->arguments[i].location,msg.c_str())
                )
                // ERR_SECTION(
            // ERR_HEAD2(function->arguments[i].name.range(),
                //     "Unknown type '"<<msg<<"' for parameter '"<<function->arguments[i].name<<"'";
                //     if(function->parentStruct && i==0){
                //         log::out << " (this parameter is auto-generated for methods)";
                //     }
                //     log::out << ".\n\n";
                //     ERR_LINE2(function->arguments[i].name.range(),msg.c_str());
                // )
            }
            argTypes.add(typeId);
        }
        FnOverloads::Overload* ambiguousOverload = nullptr;
        for(int i=0;i<(int)fnOverloads->overloads.size();i++){
            auto& overload = fnOverloads->overloads[i];
            bool found = true;
            // if(parentStruct) {
            //     if(overload.astFunc->nonDefaults+1 > function->arguments.size() ||
            //         function->nonDefaults+1 > overload.astFunc->arguments.size())
            //         continue;
            //     for (u32 j=0;j<function->nonDefaults || j<overload.astFunc->nonDefaults;j++){
            //         if(overload.funcImpl->argumentTypes[j+1].typeId != argTypes[j+1]){
            //             found = false;
            //             break;
            //         }
            //     }
            // } else {

                if(overload.astFunc->nonDefaults > function->arguments.size() ||
                    function->nonDefaults > overload.astFunc->arguments.size())
                    continue;
                for (u32 j=0;j<function->nonDefaults || j<overload.astFunc->nonDefaults;j++){
                    if(overload.funcImpl->argumentTypes[j].typeId != argTypes[j]){
                        found = false;
                        break;
                    }
                }
            // }
            if(found){
                // return &overload;
                // NOTE: You can return here because there should only be one matching overload.
                // But we keep going in case we find more matches which would indicate
                // a bug in the compiler. An optimised build would not do this.
                if(ambiguousOverload) {
                    // log::out << log::RED << __func__ <<" (COMPILER BUG): More than once match!\n";
                    Assert(("More than one match!",false));
                    // return &overload;
                    break;
                }
                ambiguousOverload = &overload;
            }
        }
        // TODO: Code for checking collision between overload needs to be fixed.
        //  Normal functions cannot be ambiguous, native functions can be defined twice
        //  as long as they have the exact same arguments. Polymorphic functions are
        //  difficult but ambiguity should be detected to some degree.
        if(ambiguousOverload){
            if(ambiguousOverload->astFunc->linkConvention != NATIVE &&
                    function->linkConvention != NATIVE) {
                //  TODO: better error message which shows what and where the already defined variable/function is.
                ERR_SECTION(
                    ERR_HEAD2(function->location)
                    ERR_MSG("Argument types are ambiguous with another overload of function '"<< function->name << "'.")
                    ERR_LINE2(ambiguousOverload->astFunc->location,"existing overload");
                    ERR_LINE2(function->location,"new ambiguous overload");
                )
                // print list of overloads?
            } else {
                // native functions can be defined twice
            }
        } else {
            if(iden && iden->funcOverloads.overloads.size()>0 && function->linkConvention == NATIVE) {
                ERR_SECTION(
                    ERR_HEAD2(function->location)
                    ERR_MSG("There already is an overload of the native function '"<<function->name<<"'.")
                    ERR_LINE2(iden->funcOverloads.overloads[0].astFunc->location, "previous")
                    ERR_LINE2(function->location, "new")
                )
            } else {
                FuncImpl* funcImpl = info.ast->createFuncImpl(function);
                // FuncImpl* funcImpl = function->createImpl();
                // funcImpl->name = function->name;
                funcImpl->usages = 0;
                fnOverloads->addOverload(function, funcImpl);
                int overload_i = fnOverloads->overloads.size()-1;
                if(parentStruct)
                    funcImpl->structImpl = parentStruct->nonPolyStruct;
                // DynamicArray<TypeId> retTypes{}; // @unused
                SignalIO yes = CheckFunctionImpl(info, function, funcImpl, parentStruct, nullptr);

                if(function->name == "main") {
                    funcImpl->usages = 1;
                    CheckInfo::CheckImpl checkImpl{};
                    checkImpl.astFunc = fnOverloads->overloads[overload_i].astFunc;
                    checkImpl.funcImpl = fnOverloads->overloads[overload_i].funcImpl;
                    info.checkImpls.add(checkImpl);
                }
                // implementation isn't checked/generated
                // if(function->body){
                //     CheckInfo::CheckImpl checkImpl{};
                //     checkImpl.astFunc = function;
                //     checkImpl.funcImpl = funcImpl;
                //     // checkImpl.scope = scope;
                //     info.checkImpls.add(checkImpl);
                // }
                _TCLOG(log::out << "ADD OVERLOAD ";funcImpl->print(info.ast, function);log::out<<"\n";)
            }
        }
    } else {
        if(fnOverloads->polyOverloads.size()!=0){
            log::out << log::YELLOW << "WARNING: Ambiguity for polymorphic overloads is not checked! (implementation incomplete) '"<<log::LIME << function->name<<log::NO_COLOR<<"'\n";
            // WARN_HEAD3(function->location,"Ambiguity for polymorphic overloads is not checked! '"<<log::LIME << function->name<<log::NO_COLOR<<"'\n\n";)
        }
        // Base poly overload is added without regard for ambiguity. It's hard to check ambiguity so to it later.
        fnOverloads->addPolyOverload(function);
    }
    return SIGNAL_SUCCESS;
}
SignalIO CheckFunctions(CheckInfo& info, ASTScope* scope){
    using namespace engone;
    ZoneScopedC(tracy::Color::Purple4);
    _TCLOG_ENTER(FUNC_ENTER)
    Assert(scope||info.errors!=0);
    if(!scope) return SIGNAL_FAILURE;

    // for(int index = 0; index < scope->contentOrder.size(); index++){
    //     auto& spot = scope->contentOrder[index];
    //     switch(spot.spotType) {
    //         case ASTScope::STATEMENT: {
    //             auto it = scope->statements[spot.index];
    //             if(it->hasNodes()){
    //                 if(it->firstBody){
    //                     SignalIO result = CheckFunctions(info, it->firstBody);   
    //                 }
    //                 if(it->secondBody){
    //                     SignalIO result = CheckFunctions(info, it->secondBody);
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
    //                 SignalIO result = CheckFunctions(info, it->body);
    //             }
    //         }
    //         break; case ASTScope::STRUCT: {
    //             auto it = scope->structs[spot.index];
    //             for(auto fn : it->functions){
    //                 CheckFunction(info, fn , it, scope);
    //                 if(fn->body){ // external/native function do not have bodies
    //                     SignalIO result = CheckFunctions(info, fn->body);
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
            SignalIO result = CheckFunctions(info, fn->body);
        }
    }
    for(auto it : scope->structs){
        for(auto fn : it->functions){
            SignalIO result = CheckFunction(info, fn , it, scope);
            if(result != SIGNAL_SUCCESS) {
                log::out << log::RED <<  "Bad\n";
            }
            if(fn->body){ // external/native function do not have bodies
                SignalIO result = CheckFunctions(info, fn->body);
            }
        }
    }
    
    for(auto astate : scope->statements) {
        // if(astate->hasNodes()){
            if(astate->firstBody){
                SignalIO result = CheckFunctions(info, astate->firstBody);   
            }
            if(astate->secondBody){
                SignalIO result = CheckFunctions(info, astate->secondBody);
            }
        // }
    }
    
    return SIGNAL_SUCCESS;
}

SignalIO CheckFuncImplScope(CheckInfo& info, ASTFunction* func, FuncImpl* funcImpl){
    using namespace engone;
    _TCLOG_ENTER(FUNC_ENTER) // if(func->body->nativeCode)
    //     return true;

    funcImpl->polyVersion = func->polyVersionCount++; // New poly version
    info.currentPolyVersion = funcImpl->polyVersion;
    // log::out << log::YELLOW << __FILE__<<":"<<__LINE__ << ": Fix methods\n";
    // Where does ASTStruct come from. Arguments? FuncImpl?

    info.currentFuncImpl = funcImpl;
    info.currentAstFunc = func;

    _TCLOG(log::out << "Impl scope: "<<funcImpl->astFunction->name<<"\n";)

    // if(func->parentStruct){
    //     for(int i=0;i<(int)func->parentStruct->polyArgs.size();i++){
    //         auto& arg = func->parentStruct->polyArgs[i];
    //         arg.virtualType->id = funcImpl->structImpl->polyArgs[i];
    //     }
    // }
    // for(int i=0;i<(int)func->polyArgs.size();i++){
    //     auto& arg = func->polyArgs[i];
    //     arg.virtualType->id = funcImpl->polyArgs[i];
    // }
    // BREAK(func->name == "add")
    func->pushPolyState(funcImpl);
    defer {
        func->popPolyState();
        // if(func->parentStruct){
        //     for(int i=0;i<(int)func->parentStruct->polyArgs.size();i++){
        //         auto& arg = func->parentStruct->polyArgs[i];
        //         arg.virtualType->id = {};
        //     }
        // }
        // for(int i=0;i<(int)func->polyArgs.size();i++){
        //     auto& arg = func->polyArgs[i];
        //     arg.virtualType->id = {};
        // }
    };

    // Assert(false);
    // TODO: Add arguments as variables
    // This shouldn't be necessary since variables are added once in the new
    // system and will remain to the end.
    // DynamicArray<std::string> vars;
    _TCLOG(log::out << "arg:\n";)
    for (int i=0;i<(int)func->arguments.size();i++) {
        auto& arg = func->arguments[i];
        auto& argImpl = funcImpl->argumentTypes[i];
        _TCLOG(log::out << " " << arg.name<<": "<< info.ast->typeToString(argImpl.typeId) <<"\n";)
        // auto varinfo = info.ast->addVariable(func->scopeId, std::string(arg.name), CONTENT_ORDER_ZERO, &arg.identifier);
        auto varinfo = info.ast->getVariableByIdentifier(arg.identifier);
        Assert(varinfo); // nocheckin
        if(varinfo){
            varinfo->versions_typeId[info.currentPolyVersion] = argImpl.typeId; // typeId comes from CheckExpression which may or may not evaluate
            // the same type as the generator.
            // vars.add(std::string(arg.name));
        }   
    }
    // _TCLOG(log::out << "ret:\n";)
    // for (int i=0;i<(int)func->returnValues.size();i++) {
    //     auto& ret = func->returnValues[i];
    //     auto& retImpl = funcImpl->returnTypes[i];
    //     _TCLOG(log::out << " [" <<i <<"] : "<< info.ast->typeToString(retImpl.typeId) <<"\n";)
    //     // auto varinfo = info.ast->addVariable(func->scopeId, std::string(arg.name), CONTENT_ORDER_ZERO, &arg.identifier);
    //     // auto varinfo = info.ast->identifierToVariable(arg.identifier);
    //     // if(varinfo){
    //     //     varinfo->versions_typeId[info.currentPolyVersion] = argImpl.typeId; // typeId comes from CheckExpression which may or may not evaluate
    //         // the same type as the generator.
    //         // vars.add(std::string(arg.name));
    //     // }   
    // }
    _TCLOG(log::out << "Struct members:\n";)
    if(func->parentStruct) {
        for (int i=0;i<(int)func->memberIdentifiers.size();i++) {
            auto iden = func->memberIdentifiers[i];
            if(!iden) continue; // happens if an argument has the same name as the member. If so the arg is prioritised and the member ignored.
            auto& memImpl = funcImpl->structImpl->members[i];
            _TCLOG(log::out << " " << iden->name<<": "<< info.ast->typeToString(memImpl.typeId) <<"\n";)
            // auto varinfo = info.ast->addVariable(func->scopeId, std::string(arg.name), CONTENT_ORDER_ZERO, &arg.identifier);
            auto varinfo = info.ast->getVariableByIdentifier(iden);
            Assert(varinfo); // nocheckin
            if(varinfo){
                varinfo->versions_typeId[info.currentPolyVersion] = memImpl.typeId; // typeId comes from CheckExpression which may or may not evaluate
                // the same type as the generator.
                // vars.add(std::string(arg.name));
            }   
        }
    }
    _TCLOG(log::out << "\n";)

    // log::out << log::CYAN << "BODY " << log::NO_COLOR << func->body->location<<"\n";
    CheckRest(info, func->body);
    
    // for(auto& name : vars){
    //     info.ast->removeIdentifier(func->scopeId, name);
    // }
    return SIGNAL_SUCCESS;
}
// SignalIO CheckGlobals(CheckInfo& info, ASTScope* scope) {

//     return SIGNAL_SUCCESS;
// }
SignalIO CheckRest(CheckInfo& info, ASTScope* scope){
    using namespace engone;
    ZoneScopedC(tracy::Color::Purple4);
    _TCLOG_ENTER(FUNC_ENTER)

    // Hello me in the future!
    //  I have disrespectfully left a complex and troublesome problem to you.
    //  CheckRest needs to be run for each function implementation to find new
    //  polymorphic functions to check and also run CheckRest on. This is highly
    //  recursive and very much a spider net. Good luck.

    // Hello past me!
    //  I seem to have a good implementation of function overloading and was
    //  wondering why sizeof wasn't calculated properly. I figured something
    //  was wrong with CheckRest and indeed there was! I don't appreciate that
    //  you left this for me but I'm in a good mood so I'll let it slide.

    // TODO: Type check default values in structs and functions
    // bool nonStruct = true;
    // Function bodies/impl to check are added to a list
    // in CheckFunction and CheckFnCall if polymorphic
 
    bool errorsWasIgnored = info.ignoreErrors;
    info.currentContentOrder.add(CONTENT_ORDER_ZERO);
    defer {
        info.currentContentOrder.pop();
        info.ignoreErrors = errorsWasIgnored;
    };

    // DynamicArray<std::string> vars;
    // for (auto now : scope->statements){
    
    // DynamicArray<TypeId> typeArray{};
    TINY_ARRAY(TypeId, typeArray, 4)

    bool perform_global_check = true;

    // TODO: Optimize by only doing one loop if the scope has no globals.
    //  The parser could set a flag if the scope has globals. Most scopes won't have globals so this will save us a lot of performance.
    for(int contentOrder=0;contentOrder<scope->content.size();contentOrder++){
        defer {
            if(perform_global_check) {
                if(contentOrder == scope->content.size() - 1) {
                    // We have done global check, now do the normal.
                    perform_global_check = false;
                    contentOrder = -1; // -1 because for loop increments by 1
                }
            }
        };
        if(scope->content[contentOrder].spotType!=ASTScope::STATEMENT)
            continue;
        info.currentContentOrder.last() = contentOrder;
        auto now = scope->statements[scope->content[contentOrder].index];

        if(perform_global_check) {
            if(now->type != ASTStatement::ASSIGN || !now->globalAssignment)
                continue;
        }
        typeArray.resize(0);
        info.ignoreErrors = errorsWasIgnored;
        if(now->isNoCode()) {
            info.ignoreErrors = true;
        }


        // #define print_t { auto ti = CheckType(info,scope->scopeId, "T", now->location, nullptr); log::out << "T = " << ti << "\n"; }
        // print_t

        //-- Check assign types in all varnames. The result is put in version_assignType for
        //   the generator and rest of the code to use.
        for(auto& varname : now->varnames) {
            // if(varname.name == "ptr_elem")
            //     __debugbreak();
            if(varname.assignString.isString()){
                // log::out << "Check assign "<<varname.name<<"\n";
                // info.ast->getScope(scope->scopeId)->print(info.ast);
                bool printedError = false;
                auto ti = CheckType(info, scope->scopeId, varname.assignString, now->location, &printedError);
                // NOTE: We don't care whether it's a pointer just that the type exists.
                if (!ti.isValid() && !printedError) {
                    ERR_SECTION(
                        ERR_HEAD2(now->location)
                        ERR_MSG("'"<<info.ast->getStringFromTypeString(varname.assignString)<<"' is not a type (statement).")
                        ERR_LINE2(now->location,"bad")
                    )
                } else {
                    // if(varname.arrayLength != 0){
                    //     auto ti = CheckType(info, scope->scopeId, varname.assignString, now->location, &printedError);
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
            SignalIO result = CheckRest(info, now->firstBody);
        } else if(now->type == ASTStatement::EXPRESSION){
            CheckExpression(info, scope->scopeId, now->firstExpression, &typeArray, false);
            // if(typeArray.size()==0)
            //     typeArray.add(AST_VOID);
        } else if(now->type == ASTStatement::RETURN){
            for(auto ret : now->arrayValues){
                typeArray.resize(0);
                SignalIO result = CheckExpression(info, scope->scopeId, ret, &typeArray, false);
            }
        } else if(now->type == ASTStatement::IF){
            SignalIO result1 = CheckExpression(info, scope->scopeId,now->firstExpression,&typeArray, false);
            SignalIO result = CheckRest(info, now->firstBody);
            if(now->secondBody){
                result = CheckRest(info, now->secondBody);
            }
        } else if(now->type == ASTStatement::ASSIGN){
            if(!perform_global_check && now->globalAssignment)
                continue;; // We already checked globals

            // log::out << now->varnames[0].name << " checked\n";
                
            auto& poly_typeArray = now->versions_expressionTypes[info.currentPolyVersion];
            poly_typeArray.resize(0);
            typeArray.resize(0);
            if(now->firstExpression){
                // may not exist, meaning just a declaration, no assignment
                // if(Equal(now->firstExpression->name, "_reserve")) {
                    // __debugbreak();
                    // log::out << "okay\n";
                // }
                SignalIO result = CheckExpression(info, scope->scopeId,now->firstExpression, &typeArray, false);
                for(int i=0;i<typeArray.size();i++){
                    if(typeArray[i].isValid() && typeArray[i] != AST_VOID)
                        continue;
                    ERR_SECTION(
                        ERR_HEAD2(now->firstExpression->location, ERROR_INVALID_TYPE)
                        ERR_MSG("Expression must produce valid types for the assignment.")
                        ERR_LINE2(now->location, "this assignment")
                        if(typeArray[i].isValid()) {
                        ERR_LINE2(now->firstExpression->location, info.ast->typeToString(typeArray[i]))
                        } else {
                        ERR_LINE2(now->firstExpression->location, "no type?")
                        }
                    )
                }
            }
            for(auto& t : typeArray)
                poly_typeArray.add(t);

            bool hadError = false;
            if(now->firstExpression && poly_typeArray.size() < now->varnames.size()){
                if(info.errors==0) {
                    ERR_SECTION(
                        ERR_HEAD2(now->location, ERROR_TOO_MANY_VARIABLES)
                        ERR_MSG("Too many variables were declared.")
                        ERR_LINE2(now->location, now->varnames.size() + " variables")
                        ERR_LINE2(now->firstExpression->location, poly_typeArray.size() + " return values")
                    )
                    hadError = true;
                }
                // don't return here, we can still evaluate some things
            }

            _TCLOG(log::out << "assign ";)
            for (int vi=0;vi<(int)now->varnames.size();vi++) {
                auto& varname = now->varnames[vi];

                const auto err_non_variable = [&]() {
                    // TODO: Print what type the identifier is. Don't just say non-variable
                    ERR_SECTION(
                        ERR_HEAD2(varname.location)
                        ERR_MSG("'"<<varname.name<<"' is defined as a non-variable and cannot be used.")
                        ERR_LINE2(varname.location, "bad")
                    )
                };

                
                // We begin by matching the varname in ASTStatement with an identifier and VariableInfo

                VariableInfo* varinfo = nullptr;
                if(!varname.declaration) {
                    if(!varname.identifier) {
                        // info.ast->getScope(scope->scopeId)->print(info.ast);
                        Identifier* possible_identifier = info.ast->findIdentifier(scope->scopeId, contentOrder, varname.name);
                        if(!possible_identifier) {
                            ERR_SECTION(
                                ERR_HEAD2(varname.location)
                                ERR_MSG("'"<<varname.name<<"' is not a defined identifier. You have to declare the variable first.")
                                ERR_LINE2(varname.location, "undeclared")
                                ERR_EXAMPLE(1,"var: i32 = 12;")
                                ERR_EXAMPLE(2,"var := 23;  // inferred type")
                            )
                            continue;
                        } else if(possible_identifier->type != Identifier::VARIABLE) {
                            err_non_variable();
                            continue;
                        } else {
                            varname.identifier = possible_identifier;
                        }
                    }
                    varinfo = info.ast->getVariableByIdentifier(varname.identifier);
                } else {
                    // Infer type
                    if(!varname.assignString.isValid()) {
                        if(vi < poly_typeArray.size()){
                            varname.versions_assignType[info.currentPolyVersion] = poly_typeArray[vi];
                        } else {
                            Assert(hadError);
                            // Do we need this err section? do we not always check the out of bounds above
                            //  // out of bounds
                            // if(!hadError){ // error may have been printed above
                            //     ERR_SECTION(
                            //         ERR_HEAD2(varname.name)
                            //         ERR_MSG_LOG("Variable '"<<log::LIME<<varname.name<<log::NO_COLOR<<"' does not have a type. You either define it explicitly (var: i32) or let the type be inferred from the expression. Neither case happens.\n")
                            //         ERR_LINE2(varname.name, "typeless");
                            //     )
                            // }
                        }
                    }
                    // Logic if identifier exists or not and whether the identifier is of a variable type or not AND which scope the identifier was found in.
                    // Based on all that, we may use the one we found or create a new variable identifier.
                    Identifier* possible_identifier = nullptr;
                    if(!varname.identifier) {
                        possible_identifier = info.ast->findIdentifier(scope->scopeId, contentOrder, varname.name);
                        if(!possible_identifier) {
                            // identifier does not exist, we should create it.
                            if(now->globalAssignment) {
                                varinfo = info.ast->addVariable(scope->scopeId, varname.name, CONTENT_ORDER_ZERO, &varname.identifier, nullptr);
                            } else {
                                varinfo = info.ast->addVariable(scope->scopeId, varname.name, contentOrder, &varname.identifier, nullptr);
                            }
                            Assert(varinfo);
                            varinfo->type = now->globalAssignment ? VariableInfo::GLOBAL : VariableInfo::LOCAL;
                        } else if(possible_identifier->type == Identifier::VARIABLE) {
                            // Identifier does exist but we can still declare it if is in a parent scope.

                            // An identifier would have to allow multiple variables with different content orders.
                            if(possible_identifier->scopeId == scope->scopeId) {
                                // variable may already exist if we are in a polymorphic scope.
                                // if we are in the same scope with the same content order then we have
                                // already touched this statement. we can set the versionTypeID just fine.
                                // The other scenario has to do with declaring a new variable but the 
                                // name already exists. If you allow this then it's called shadowing.
                                // This should be allowed but the current system can't support it so we
                                // don't allow it for now.

                                // NOTE: I had some issues with polymorphic structs and local variables in methods.
                                //  As a quick fix I added this. Whether this does what I think it does or if we have
                                //  to add something else for edge cases to be taken care of is another matter.
                                //  - Emarioo, 2023-12-29 (By the way, merry christmas and happy new year!)
                                if(possible_identifier->order != contentOrder) {
                                    ERR_SECTION(
                                        ERR_HEAD2(varname.location)
                                        ERR_MSG("Cannot redeclare variables. Identifier shadowing not implemented yet\n")
                                        ERR_LINE2(varname.location,"");
                                    )
                                    continue;
                                } else {
                                    varname.identifier = possible_identifier;
                                    // We should only be here if the scope is polymorphic.
                                    varinfo = info.ast->getVariableByIdentifier(varname.identifier);
                                    // The 'now' statement was checked to be global and thus made varinfo global. varinfo->type will always be the same as now->globalAssignment
                                    // The assert will fire if we have a bug in the code.
                                    Assert(varinfo->type == (now->globalAssignment ? VariableInfo::GLOBAL : VariableInfo::LOCAL));
                                }
                            } else {
                                varname.identifier = possible_identifier;
                                // variable does not exist in scope, we can therefore create it
                                varinfo = info.ast->addVariable(scope->scopeId, varname.name, contentOrder, &varname.identifier, nullptr);
                                Assert(varinfo);
                                // ACTUALLY! I thought global assignment in local scopes didn't work but I believe it does.
                                //  The code here just handles where the variable is visible. That should be the local scope which is correct.
                                //  The VariableInfo::GLOBAL type is what puts the data into the global data section. Silly me.
                                //  - Emarioo, 2024-1-03
                                // if(now->globalAssignment && scope->scopeId != 0) {
                                //     ERR_SECTION(
                                //         ERR_HEAD2(now->location)
                                //         ERR_MSG("Global assignment in local scope not implemented. Move global variable to global scope or remove the global keyword. In the future, ")
                                //         ERR_LINE2(now->location, "can't be global")
                                //     )
                                //     // TODO: Global assignment needs extra code.
                                //     //  We should not add variable into 'scope->scopeId' like we do above
                                //     //  We also need to check the identifier against the global scope not 'scope->scopeId'
                                //     //  Basically, global assignment could use it's own code path.
                                // }
                                varinfo->type = now->globalAssignment ? VariableInfo::GLOBAL : VariableInfo::LOCAL;
                            }
                        } else {
                            err_non_variable();
                            continue;
                        }
                    } else {
                        // We should only be here if we are in a polymorphic scope/check.
                        Assert(varname.identifier->type == Identifier::VARIABLE);
                        varinfo = info.ast->getVariableByIdentifier(varname.identifier);
                        Assert(varinfo);
                        Assert(varinfo->type == (now->globalAssignment ? VariableInfo::GLOBAL : VariableInfo::LOCAL));
                    }
                    Assert(varinfo);

                    // O don't think this will ever fire. But just in case.
                    // HAHA, IT DID FIRE! Globals were checked twice! -Emarioo, 2024-01-06
                    Assert(!varinfo->versions_typeId[info.currentPolyVersion].isValid());

                    // FINALLY we can set the declaration type
                    varinfo->versions_typeId[info.currentPolyVersion] = varname.versions_assignType[info.currentPolyVersion];
                }

                if(varname.declaration) {
                    _TCLOG(log::out << " " << varname.name<<": "<< info.ast->typeToString(varname.versions_assignType[info.currentPolyVersion]) << " scope: "<<scope->scopeId << " order: "<<contentOrder<< "\n";)
                } else {
                    _TCLOG(log::out << " " << varname.name<<": "<< info.ast->typeToString(varinfo->versions_typeId[info.currentPolyVersion]) << " scope: "<<scope->scopeId << " order: "<<contentOrder<< "\n";)
                }

                // We continue by checking the expression with the variable/identifier info type

                #ifdef gone
                // NOTE: This is old code but I changed it a big before deprecating it so it doesn't work

                // TODO: Do you need to do something about global data here?
                Identifier* possible_identifier = nullptr;
                VariableInfo* varinfo = nullptr;
                if(!varname.identifier) {
                    possible_identifier = info.ast->findIdentifier(scope->scopeId, contentOrder, varname.name);
                    if(!possible_identifier) {
                        // identifier does not exist, we should create it.
                        varinfo = info.ast->addVariable(scope->scopeId, varname.name, contentOrder, &varname.identifier);
                        Assert(varinfo);
                        varinfo->type = now->globalAssignment ? VariableInfo::GLOBAL : VariableInfo::LOCAL;

                        // Can we move these down? Are they similar to code below?
                        varinfo->versions_typeId[info.currentPolyVersion] = varname.versions_assignType[info.currentPolyVersion];
                        varname.declaration = true;
                    } else {

                    }
                }
                if(!varname.identifier){
                    // vars.add(varname.name);
                } else if(varname.identifier->type==Identifier::VARIABLE) {
                    // variable/identifier does exist

                    if(varname.assignString.isValid()){
                        // A valid assignString means that variable should be declared

                        // An identifier would have to allow multiple variables with different content orders.
                        if(varname.identifier->scopeId == scope->scopeId) {
                            // variable may already exist if we are in a polymorphic scope.
                            // if we are in the same scope with the same content order then we have
                            // already touched this statement. we can set the versionTypeID just fine.
                            // The other scenario has to do with declaring a new variable but the 
                            // name already exists. If you allow this then it's called shadowing.
                            // This should be allowed but the current system can't support it so we
                            // don't allow it for now.

                            // NOTE: I had some issues with polymorphic structs and local variables in methods.
                            //  As a quick fix I added this. Whether this does what I think it does or if we have
                            //  to add something else for edge cases to be taken care of is another matter.
                            //  - Emarioo, 2023-12-29 (By the way, merry christmas and happy new year!)
                            if(varname.identifier->order != contentOrder) {
                                ERR_SECTION(
                                    ERR_HEAD2(varname.name)
                                    ERR_MSG("Cannot redeclare variables. Identifier shadowing not implemented yet\n")
                                    ERR_LINE2(varname.name,"");
                                )
                                continue;
                            } else {
                                varinfo = info.ast->getVariable(varname.identifier);
                            }
                            // Assert(varname.identifier->order == contentOrder);
                            // varinfo = info.ast->identifierToVariable(varname.identifier);
                            // Assert(varinfo);
                            // // Double checking these becuase they should have been set last time.
                            // Assert(varname.declaration);
                            // Assert(varinfo->type == (now->globalAssignment ? VariableInfo::GLOBAL : VariableInfo::LOCAL));
                        } else {
                            // variable does not exist in scope, we can therefore create it
                            varinfo = info.ast->addVariable(scope->scopeId, varname.name, contentOrder, &varname.identifier);
                            Assert(varinfo);
                            varname.declaration = true;
                            varinfo->type = now->globalAssignment ? VariableInfo::GLOBAL : VariableInfo::LOCAL;
                        }
                        varinfo->versions_typeId[info.currentPolyVersion] = varname.versions_assignType[info.currentPolyVersion];
                    } else {
                        if(varname.name == "ptr_elem")
                            __debugbreak();
                        varinfo = info.ast->identifierToVariable(varname.identifier);
                        // TypeId typeId = info.ast->ensureNonVirtualId(varinfo->versions_typeId[info.currentPolyVersion])
                        Assert(varinfo);
                        if(!info.ast->castable(varname.versions_assignType[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion])){
                            std::string leftstr = info.ast->typeToString(varinfo->versions_typeId[info.currentPolyVersion]);
                            std::string rightstr = info.ast->typeToString(varname.versions_assignType[info.currentPolyVersion]);
                            ERR_SECTION(
                                ERR_HEAD2(now->location, ERROR_TYPE_MISMATCH)
                                ERR_MSG("Type mismatch '"<<leftstr<<"' <- '"<<rightstr<< "' in assignment.")
                                ERR_LINE2(varname.name, leftstr.c_str())
                                ERR_LINE2(now->firstExpression->location,rightstr.c_str())
                            )
                        }
                    }
                } else {
                    ERR_SECTION(
                        ERR_HEAD2(varname.name)
                        ERR_MSG("'"<<varname.name<<"' is defined as a non-variable and cannot be used.")
                        ERR_LINE2(varname.name, "bad")
                    )
                }
                #endif
                // Technically, we don't need vi < poly_typeArray.size
                // We check for it above. HOWEVER, we don't stop and that's because we try to
                // find more errors. That is why we need vi < ...
                if(vi < (int)poly_typeArray.size()){
                    if(!info.ast->castable(poly_typeArray[vi], varinfo->versions_typeId[info.currentPolyVersion])) {
                        ERR_SECTION(
                            ERR_HEAD2(now->firstExpression->location, ERROR_TYPE_MISMATCH)
                            ERR_MSG("Type mismatch " << info.ast->typeToString(poly_typeArray[vi]) << " - " << info.ast->typeToString(varinfo->versions_typeId[info.currentPolyVersion]) << ".")
                            ERR_LINE2(varname.location, info.ast->typeToString(varinfo->versions_typeId[info.currentPolyVersion]))
                            ERR_LINE2(now->firstExpression->location, info.ast->typeToString(poly_typeArray[vi]))
                        )
                    }
                }
                if(varname.declaration && varinfo->isGlobal()) {
                    u32 size = info.ast->getTypeSize(varinfo->versions_typeId[info.currentPolyVersion]);
                    u32 offset = info.ast->aquireGlobalSpace(size);
                    varinfo->versions_dataOffset[info.currentPolyVersion] = offset;
                }
                if(now->arrayValues.size()!=0) {
                    TypeInfo* arrTypeInfo = info.ast->getTypeInfo(now->varnames.last().versions_assignType[info.currentPolyVersion].baseType());
                    if(!arrTypeInfo->astStruct || arrTypeInfo->astStruct->name != "Slice") {
                        ERR_SECTION(
                            ERR_HEAD2(now->varnames.last().location)
                            ERR_MSG("Variables that use array initializers must use the 'Slice<T>' type")
                            ERR_LINE2(now->varnames.last().location, "must use Slice<T> not "<<info.ast->typeToString(arrTypeInfo->id))
                            // TODO: Print the location of the type instead of the variable name
                        )
                    } else {
                        TypeId elementType = arrTypeInfo->structImpl->polyArgs[0];
                        for(int j=0;j<now->arrayValues.size();j++){
                            ASTExpression* value = now->arrayValues[j];
                            typeArray.resize(0);
                            SignalIO result = CheckExpression(info, scope->scopeId, value, &typeArray, false);
                            if(result != SIGNAL_SUCCESS){
                                continue;
                            }
                            if(typeArray.size()!=1) {
                                if(typeArray.size()==0) {
                                    ERR_SECTION(
                                        ERR_HEAD2(value->location)
                                        ERR_MSG("Expressions in array initializers cannot consist of of no values. Did you call a function that returns void?")
                                        ERR_LINE2(value->location, "bad")
                                    )
                                    continue;
                                } else {
                                    ERR_SECTION(
                                        ERR_HEAD2(value->location)
                                        ERR_MSG("Expressions in array initializers cannot consist of multiple values. Did you call a function that returns more than one value?")
                                        ERR_LINE2(value->location, "bad")
                                        // TODO: Print the types in rightTypes
                                    )
                                    continue;
                                }
                            }
                            if(!info.ast->castable(typeArray[0], elementType)) {
                                ERR_SECTION(
                                    ERR_HEAD2(value->location)
                                    ERR_MSG("Cannot cast '"<<info.ast->typeToString(typeArray[0])<<"' to '"<<info.ast->typeToString(elementType)<<"'.")
                                    ERR_LINE2(value->location, "cannot cast")
                                )
                            }
                        }
                    }
                }
            }
            _TCLOG(log::out << "\n";)
        } else if(now->type == ASTStatement::WHILE){
            if(now->firstExpression) {
                // no expresion represents infinite loop
                SignalIO result1 = CheckExpression(info, scope->scopeId, now->firstExpression, &typeArray, false);
            }
            SignalIO result = CheckRest(info, now->firstBody);
        } else if(now->type == ASTStatement::FOR){
            // DynamicArray<TypeId> temp{};
            SignalIO result1 = CheckExpression(info, scope->scopeId, now->firstExpression, &typeArray, false);
            SignalIO result=SIGNAL_FAILURE;

            Assert(now->varnames.size()==2);
            auto& varnameIt = now->varnames[0];
            auto& varnameNr = now->varnames[1];

            ScopeId varScope = now->firstBody->scopeId;
            
            bool reused_index = false;
            bool reused_item = false;
            auto varinfo_index = info.ast->addVariable(varScope, varnameNr.name, CONTENT_ORDER_ZERO, &varnameNr.identifier, &reused_index);
            auto varinfo_item = info.ast->addVariable(varScope, varnameIt.name, CONTENT_ORDER_ZERO, &varnameIt.identifier, &reused_item);
            if(!varinfo_index || !varinfo_item) {
                ERR_SECTION(
                    ERR_HEAD2(now->location)
                    ERR_MSG("Cannot add variable '"<<varnameNr.name<<"' or '"<<varnameIt.name<<"' to use in for loop. Is it already defined?")
                    ERR_LINE2(now->location,"")
                )
                continue;
            }
            auto iterinfo = info.ast->getTypeInfo(typeArray.last());
            if(iterinfo&&iterinfo->astStruct){
                if(iterinfo->astStruct->name == "Slice"){
                    Identifier* nrId = nullptr;
                    // Where to put nrId identifier in AST?
                    // if(varinfo_index) {
                    varinfo_index->versions_typeId[info.currentPolyVersion] = AST_INT64;
                    varnameNr.versions_assignType[info.currentPolyVersion] = AST_INT64;
                    // } else {
                    //     ERR_SECTION(
                    //         ERR_HEAD2(now->location)
                    //         ERR_MSG("Cannot add '"<<varnameNr.name<<"' variable to use in for loop. Is it already defined?")
                    //         ERR_LINE2(now->location,"")
                    //     )
                    // }

                    // _TCLOG(log::out << " " << varname.name<<": "<< info.ast->typeToString(varname.assignType) <<"\n";)
                    // if(varinfo_item){
                        auto memdata = iterinfo->getMember("ptr");
                        auto itemtype = memdata.typeId;
                        itemtype.setPointerLevel(itemtype.getPointerLevel()-1);
                        varnameIt.versions_assignType[info.currentPolyVersion] = itemtype;
                        
                        auto vartype = memdata.typeId;
                        if(!now->isPointer()){
                            vartype.setPointerLevel(vartype.getPointerLevel()-1);
                        }
                        varinfo_item->versions_typeId[info.currentPolyVersion] = vartype;
                    // } else {
                    //     ERR_SECTION(
                    //         ERR_HEAD2(now->location)
                    //         ERR_MSG("Cannot add '"<<varnameIt.name<<"' variable to use in for loop. Is it already defined?")
                    //         ERR_LINE2(now->location,"")
                    //     )
                    // }

                    SignalIO result = CheckRest(info, now->firstBody);
                    // info.ast->removeIdentifier(varScope, varname.name);
                    // info.ast->removeIdentifier(varScope, "nr");
                    continue;
                } else if(iterinfo->astStruct->members.size() == 2) {
                    auto mem0 = iterinfo->getMember(0);
                    auto mem1 = iterinfo->getMember(1);
                    if(mem0.typeId == mem1.typeId && AST::IsInteger(mem0.typeId)){
                        if(now->isPointer()){
                            ERR_SECTION(
                                ERR_HEAD2(now->location)
                                ERR_MSG("@pointer annotation isn't allowed when Range is used in a for loop. @pointer only works with Slice.")
                                ERR_LINE2(now->location,"")
                            )
                        }
                        TypeId inttype = mem0.typeId;
                        now->rangedForLoop = true;
                        // auto varinfo_index = info.ast->addVariable(varScope, varnameNr.name, contentOrder, &varnameNr.identifier);
                        // if(varinfo_index) {
                        varnameNr.versions_assignType[info.currentPolyVersion] = inttype;
                        varinfo_index->versions_typeId[info.currentPolyVersion] = inttype;
                        // } else {
                        //     ERR_SECTION(
                        //         ERR_HEAD2(now->location)
                        //         ERR_MSG("Cannot add '"<<varnameNr.name<<"' variable to use in for loop. Is it already defined?")
                        //         ERR_LINE2(now->location,"")
                        //     )
                        // }

                        // _TCLOG(log::out << " " << varname.name<<": "<< info.ast->typeToString(varname.assignType) <<"\n";)
                        // auto varinfo_item = info.ast->addVariable(varScope, varnameIt.name, contentOrder, &varnameIt.identifier);
                        // if(varinfo_item){
                        varnameIt.versions_assignType[info.currentPolyVersion] = inttype;
                        varinfo_item->versions_typeId[info.currentPolyVersion] = inttype;
                        // } else {
                        //     ERR_SECTION(
                        //         ERR_HEAD2(now->location)
                        //         ERR_MSG("Cannot add '"<<varnameIt.name<<"' variable to use in for loop. Is it already defined?")
                        //         ERR_LINE2(now->location, "")
                        //     )
                        // }

                        SignalIO result = CheckRest(info, now->firstBody);
                        // info.ast->removeIdentifier(varScope, varname.name);
                        // info.ast->removeIdentifier(varScope, "nr");
                        continue;
                    }
                }
            }
            std::string strtype = info.ast->typeToString(typeArray.last());
            ERR_SECTION(
                ERR_HEAD2(now->firstExpression->location)
                ERR_MSG("The expression in for loop must be a Slice or Range.")
                ERR_LINE2(now->firstExpression->location,strtype.c_str())
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
                auto& name = now->varnames[0].name;
                TypeId originType = CheckType(info, scope->scopeId, name, now->location, nullptr);
                TypeId aliasType = info.ast->convertToTypeId(*now->alias, scope->scopeId, true);
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
                ScopeInfo* originScope = info.ast->findScope(name,scope->scopeId, true);
                ScopeInfo* aliasScope = info.ast->findScope(*now->alias,scope->scopeId, true);
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
                //     ERR_SECTION(
// ERR_HEAD2(now->location) << *now->name<<" is not a type (using)\n";
                // }
                // if(aliasType.isValid()){
                //     ERR_SECTION(
// ERR_HEAD2(now->location) << *now->alias<<" is already a type (using)\n";
                // }
                // continue;
                // CheckType may create a new type if polymorphic. We don't want that.
                // TODO: It is okay to create a new type with a name which already
                // exists if it's in a different scope.
                // TODO: How does polymorphism work here?
                // Will it work if just left as is or should it be disallowed.
            } else if (now->varnames.size()==1) {
                auto& name = now->varnames[0].name;
                // ERR_SECTION(
// ERR_HEAD2(now->location) << "inheriting namespace with using doesn't work\n";
                ScopeInfo* originInfo = info.ast->findScope(name,scope->scopeId,true);
                if(originInfo){
                    ScopeInfo* scopeInfo = info.ast->getScope(scope->scopeId);
                    scopeInfo->sharedScopes.add(originInfo);
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
        } else if(now->type == ASTStatement::TEST) {
            CheckExpression(info, scope->scopeId, now->testValue, &typeArray, false);
            typeArray.resize(0);
            CheckExpression(info, scope->scopeId, now->firstExpression, &typeArray, false);
            typeArray.resize(0);
        } else if(now->type == ASTStatement::SWITCH) {
            // check switch expression
            CheckExpression(info, scope->scopeId, now->firstExpression, &typeArray, false);
            
            ASTEnum* astEnum = nullptr;
            Assert(typeArray.size() == 1);
            if(typeArray[0].isValid()) {
                auto typeInfo = info.ast->getTypeInfo(typeArray[0]);
                Assert(typeInfo);
                if(typeInfo->astEnum) {
                    astEnum = typeInfo->astEnum;
                    // TODO: What happens if type is virtual or aliased? Will this code work?
                }
            }
            now->versions_expressionTypes[info.currentPolyVersion] = {};
            now->versions_expressionTypes[info.currentPolyVersion].add(typeArray.last());
            typeArray.resize(0);
            
            int usedMemberCount = 0;
            QuickArray<ASTExpression*> usedMembers{};
            if(astEnum) {
                usedMembers.resize(astEnum->members.size());
                FOR(usedMembers) {
                    it = nullptr;   
                }
            }
            
            // TODO: Enum switch statements should not be limited to just enum members.
            //  Constant expressions that evaluate to the same value as one of the members should
            //  count towards having all the members present in the switch. Unless enum members
            //  aren't forced to exist in the switch.
            
            bool allCasesUseEnumType = true;
            
            FOR(now->switchCases){
                bool wasMember = false;
                if(astEnum && it.caseExpr->typeId == AST_ID) {
                    int index = -1;
                    bool yes = astEnum->getMember(it.caseExpr->name, &index);
                    if(yes) {
                        wasMember = true;
                        if(usedMembers[index]) {
                            ERR_SECTION(
                                ERR_HEAD2(it.caseExpr->location, ERROR_DUPLICATE_CASE)
                                ERR_MSG("Two cases handle the same enum member.")
                                ERR_LINE2(usedMembers[index]->location, "previous")
                                ERR_LINE2(it.caseExpr->location, "bad")
                            )
                        } else {
                            usedMembers[index] = it.caseExpr;
                            usedMemberCount++;
                        }
                    } else {
                        allCasesUseEnumType = false;   
                    }
                } else {
                    allCasesUseEnumType = false;   
                }
                if(!wasMember){
                    CheckExpression(info, scope->scopeId, it.caseExpr, &typeArray, false);
                    typeArray.resize(0);
                }
                
                CheckRest(info, it.caseBody);
            }
            
            if(now->firstBody) {
                // default case
                CheckRest(info, now->firstBody);
            } else {
                // not default case
                if(allCasesUseEnumType && usedMemberCount != (int)usedMembers.size()) {
                    // NOTE: I find that errors about having missed members is annoying most of the time.
                    //  BUT, there are certainly times were you forget to handle new enums you add which is when
                    //  this error is nice. I have added this as a TODO because we need to add some annotations
                    //  so that the user can decide how they want the error to work an on what members.

                    // ERR_SECTION(
                    //     ERR_HEAD2(now->location, ERROR_MISSING_ENUM_MEMBERS_IN_SWITCH)
                    //     ERR_MSG("You have forgotten these enum members in the switch statement:")
                        
                    //     ERR_LINE2(now->location, "this switch")
                    // )
                }
            }
        } else {
            Assert(("Statement type not handled!",false));
        }
    }
    // for(auto& name : vars){
    //     info.ast->removeIdentifier(scope->scopeId, name);
    // }
    for(auto now : scope->namespaces){
        SignalIO result = CheckRest(info, now);
    }
    return SIGNAL_SUCCESS;
}

int TypeCheck(AST* ast, ASTScope* scope, CompileInfo* compileInfo){
    using namespace engone;
    // MEASURE;
    ZoneScopedC(tracy::Color::Purple4);
    CheckInfo info = {};
    info.ast = ast;
    info.compileInfo = compileInfo;

    _VLOG(log::out << log::BLUE << "Type check:\n";)
    Assert(false);
    // Check enums first since they don't depend on anything.
    SignalIO result = CheckEnums(info, scope); // nocheckin

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
    // An improvement here might be to create empty struct types first.
    // Any usage of pointers to structs won't cause failures since we know the size of pointers.
    // If a type isn't a pointer then we might fail if the struct hasn't been created yet.
    // You could implement some dependency or priority queue.
    while(!info.completedStructs){
        info.completedStructs = true;
        info.anotherTurn=false;
        
        result = CheckStructs(info, scope); // nocheckin
        
        if(!info.anotherTurn && !info.completedStructs){
            if(!info.showErrors){
                info.showErrors = true;
                continue;
            }
            // log::out << log::RED << "Some types could not be evaluated\n";
            break;
        }
    }
    
    // We check function "header" first and then later, we check the body.
    // This is because bodies may reference other functions. If we check header and body at once,
    // a referenced function may not exist yet (this is certain if the body of two functions reference each other).
    // If so we would have to stop checking that body and continue later until that reference have been cleared up.
    // So instead of that mess, we check headers first to ensure that all references will work.
    result = CheckFunctions(info, scope); // nocheckin

    // Check rest will go through scopes and create polymorphic implementations if necessary.
    // This includes structs and functions.
    result = CheckRest(info, scope); // nocheckin


    while(info.checkImpls.size()!=0){
        auto checkImpl = info.checkImpls[info.checkImpls.size()-1];
        info.checkImpls.pop();
        Assert(checkImpl.astFunc->body); // impl should not have been added if there was no body
        
        CheckFuncImplScope(info, checkImpl.astFunc, checkImpl.funcImpl); // nocheckin
    }
    
    info.compileInfo->compileOptions->compileStats.errors += info.errors;
    return info.errors;
}

void TypeCheckEnums(AST* ast, ASTScope* scope, Compiler* compiler) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Purple4);
    CheckInfo info = {};
    info.ast = ast;
    info.compiler = compiler;

    _VLOG(log::out << log::BLUE << "Type check enums:\n";)
    // Check enums first since they don't depend on anything.
    SignalIO result = CheckEnums(info, scope); // nocheckin
    
}
SignalIO TypeCheckStructs(AST* ast, ASTScope* scope, Compiler* compiler, bool show_errors) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Purple4);
    CheckInfo info = {};
    info.ast = ast;
    info.compiler = compiler;
    _VLOG(log::out << log::BLUE << "Type check structs:\n";)

    info.completedStructs=false;
    // An improvement here might be to create empty struct types first.
    // Any usage of pointers to structs won't cause failures since we know the size of pointers.
    // If a type isn't a pointer then we might fail if the struct hasn't been created yet.
    // You could implement some dependency or priority queue.
        
    while(true) {
        info.completedStructs = true;
        info.anotherTurn = false;
        info.showErrors = show_errors;
        SignalIO result = CheckStructs(info, scope); // nocheckin
        
        if(info.completedStructs)
            return SIGNAL_SUCCESS;

        if(info.anotherTurn)
            continue;

        if(!show_errors)
            return SIGNAL_NO_MATCH;
        
        // log::out << log::RED << "Some types could not be evaluated\n";
        return SIGNAL_COMPLETE_FAILURE;
    }
    return SIGNAL_COMPLETE_FAILURE;
}
void TypeCheckFunctions(AST* ast, ASTScope* scope, Compiler* compiler) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Purple4);
    CheckInfo info = {};
    info.ast = ast;
    info.compiler = compiler;
    _VLOG(log::out << log::BLUE << "Type check functions:\n";)

    
    // We check function "header" first and then later, we check the body.
    // This is because bodies may reference other functions. If we check header and body at once,
    // a referenced function may not exist yet (this is certain if the body of two functions reference each other).
    // If so we would have to stop checking that body and continue later until that reference have been cleared up.
    // So instead of that mess, we check headers first to ensure that all references will work.
    SignalIO result = CheckFunctions(info, scope); // nocheckin
}

void TypeCheckBodies(AST* ast, ASTScope* scope, Compiler* compiler) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Purple4);
    CheckInfo info = {};
    info.ast = ast;
    info.compiler = compiler;
    _VLOG(log::out << log::BLUE << "Type check functions:\n";)

    // Check rest will go through scopes and create polymorphic implementations if necessary.
    // This includes structs and functions.
    SignalIO result = CheckRest(info, scope); // nocheckin


    while(info.checkImpls.size()!=0){
        auto checkImpl = info.checkImpls[info.checkImpls.size()-1];
        info.checkImpls.pop();
        Assert(checkImpl.astFunc->body); // impl should not have been added if there was no body
        
        CheckFuncImplScope(info, checkImpl.astFunc, checkImpl.funcImpl); // nocheckin
    }
    
    info.compileInfo->compileOptions->compileStats.errors += info.errors;
    // return info.errors;
}