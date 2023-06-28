#include "BetBat/TypeChecker.h"
#include "BetBat/Compiler.h"

#undef WARN_HEAD
#undef ERR_HEAD
#undef ERR_HEAD2
#undef ERR_LINE
#undef ERR
#undef ERR_END
// #undef ERRTYPE
#undef ERRTOKENS
#undef TOKENINFO

// Remember to wrap macros in {} when using if or loops
// since macros have multiple statements.
#define ERR() info.errors++; engone::log::out << engone::log::RED <<"TypeError: "
// #define ERR_HEAD2(R) info.errors++; engone::log::out << ERR_LOCATION_RANGE(R)
#define ERR_HEAD2(R) info.errors++; engone::log::out << ERR_DEFAULT_R(R,"Type error","E0000")
#define ERR_HEAD(R,M) info.errors++; engone::log::out << ERR_DEFAULT_R(R,"Type error","E0000") << M
#define WARN_HEAD(R) info.compileInfo->warnings++; engone::log::out << WARN_DEFAULT_R(R,"Type warning","W0000")
#define ERR_END MSG_END

#define ERR_LINE(R, M) PrintCode(&R, M)
// #define ERRTYPE(R,LT,RT) ERR_HEAD2(R) << "Type mismatch "<<info.ast->getTypeInfo(LT)->name<<" - "<<info.ast->getTypeInfo(RT)->name<<" "

#define ERRTOKENS(R) engone::log::out <<engone::log::GREEN<<R.firstToken.line <<" |> "; R.print();engone::log::out << "\n";

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
            ERR_HEAD2(aenum->tokenRange) << aenum->name << " is taken\n";
            ERR_END
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
   
    structImpl->members.resize(astStruct->baseImpl.members.size());

    bool success = true;
    _TC_LOG(log::out << "Check struct impl "<<info.ast->typeToString(structInfo->id)<<"\n";)
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
                    ERR_HEAD2(astStruct->tokenRange) << "type "<< info.ast->getTokenFromTypeString(implMem.typeId) << " in "<< astStruct->name<<"."<<member.name << " is not a type\n";
                    ERR_END
                }
                success = false;
                break;
            }
            implMem.typeId = tid;
            _TC_LOG(log::out << " checked member["<<i<<"] "<<info.ast->typeToString(tid)<<"\n";)
        } else {
            implMem.typeId = baseMem.typeId;
        }
    }
    if(!success){
        return success;
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
                    ERR_END
                }
                // TODO: phrase the error message in a better way
                // TOOD: print some column and line info.
                // TODO: message is printed twice
                // log::out << log::RED <<"Member "<< member.name <<" in struct "<<*astStruct->name<<" cannot be it's own type\n";
            } else {
                // astStruct->state = ASTStruct::TYPE_ERROR;
                if(info.showErrors) {
                    ERR_HEAD2(astStruct->tokenRange) << "type "<< info.ast->typeToString(implMem.typeId) << " in "<< astStruct->name<<"."<<member.name << " is not a type\n";
                    ERR_END
                }
            }
            success = false;
            break;
        }
        if(alignedSize<asize)
            alignedSize = asize;

        /* You may see this:
            offset 4: a
            offset 8: b
            evalutate to 16 bytes
        You may think that a should have offset 0 but it shouldn't.
        GeneratingExpressions pushes the values to the stack growing downwards.
        First b is pushed (16-8). Then a is pushed (8-4) right next to b.
        If a had 0 as offset you would need to pop the pushed a and align it to
        0 instead of 4.
        */
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
        // if(i+1<(int)astStruct->members.size()){
        //     auto& implMema = structImpl->members[i+1];
        //     i32 nsize = info.ast->getTypeSize(implMema.typeId);
        //     // i32 asize = info.ast->getTypeAlignedSize(implMem.typeId);
        //     int misalign = (offset + nsize) % size;
        //     if(misalign!=0){
        //         offset+=size-misalign;
        //     }
        // }
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
            structImpl->members.back().offset += misalign;
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

    // Token ns={};
    u32 plevel=0;
    std::vector<Token> polyTypes;
    Token typeName = AST::TrimPointer(typeString, &plevel);
    Token baseType = AST::TrimPolyTypes(typeName, &polyTypes);
    // Token typeName = AST::TrimNamespace(noPointers, &ns);
    std::vector<TypeId> polyIds;
    std::string* realTypeName = nullptr;

    if(polyTypes.size() != 0) {
        // We trim poly types and then put them back together to get the "official" names for the types
        // Maybe you used some aliasing or namespaces.
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
                ERR_HEAD(tokenRange, "Type '"<<info.ast->typeToString(id)<<"' for polymorphic argument was not valid.\n\n";
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
            ERR_HEAD(tokenRange, info.ast->typeToString(ti->id)<<" is polymorphic. You must specify poly. types like this: Struct<i32>. (if in a function: compiler does not have knowledge of where)\n\n";
            ERR_LINE(tokenRange,"");
            )
            if(printedError)
                *printedError = true;
            return {};
        }
        typeId.setPointerLevel(plevel);
        return typeId;
    }

    if(polyTypes.size() == 0) {
        // ERR_HEAD2(tokenRange) << <<" is polymorphic. You must specify poly. types like this: Struct<i32>\n";
        return {}; // type isn't polymorphic and does just not exist
    }
    TypeInfo* baseInfo = info.ast->convertToTypeInfo(baseType, scopeId);
    if(!baseInfo) {
        // ERR_HEAD2(tokenRange) << info.ast->typeToString(ti->id)<<" is polymorphic. You must specify poly. types like this: Struct<i32>\n";
        return {}; // print error? base type for polymorphic type doesn't exist
    }
    if(polyTypes.size() != baseInfo->astStruct->polyArgs.size()) {
        ERR() << "Polymorphic type "<<typeString << " has "<< polyTypes.size() <<" poly. args but the base type "<<info.ast->typeToString(baseInfo->id)<<" needs "<<baseInfo->astStruct->polyArgs.size()<< "\n";
        if(printedError)
            *printedError = true;
        return {}; // type isn't polymorphic and does just not exist
    }

    TypeInfo* typeInfo = info.ast->createType(*realTypeName, baseInfo->scopeId);
    typeInfo->astStruct = baseInfo->astStruct;
    typeInfo->structImpl = (StructImpl*)engone::Allocate(sizeof(StructImpl));
    new(typeInfo->structImpl)StructImpl();

    typeInfo->structImpl->polyIds.resize(polyIds.size());
    for(int i=0;i<(int)polyIds.size();i++){
        TypeId id = polyIds[i];
        if(id.isValid()){
            baseInfo->astStruct->polyArgs[i].virtualType->id = id;
        }
        typeInfo->structImpl->polyIds[i] = id;
    }

    bool hm = CheckStructImpl(info, typeInfo->astStruct, baseInfo, typeInfo->structImpl);
    if(!hm) {
        ERR() << __FUNCTION__ <<": structImpl for type "<<typeString << " failed\n";
    } else {
        _TC_LOG(log::out << typeString << " was evaluated to "<<typeInfo->structImpl->size<<" bytes\n";)
    }
    for(int i=0;i<(int)polyIds.size();i++){
        baseInfo->astStruct->polyArgs[i].virtualType->id = {}; // disable types
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
                ERR_HEAD(astStruct->tokenRange, "'"<<astStruct->name<<"' is already defined.\n");
                // TODO: Provide information (file, line, column) of the first definition.
            } else {
                _TC_LOG(log::out << "Defined struct "<<info.ast->typeToString(structInfo->id)<<"\n";)
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
int CheckExpression(CheckInfo& info, ASTScope* scope, ASTExpression* expr, TypeId* outType);
int CheckFunctionImpl(CheckInfo& info, ASTFunction* func, FuncImpl* funcImpl, ASTStruct* parentStruct, TypeId* outType);
int CheckFncall(CheckInfo& info, ASTScope* scope, ASTExpression* expr, TypeId* outType) {
    using namespace engone;
    if(!expr->name){
        ERR_HEAD2(expr->tokenRange) << "string was null at "<<expr->tokenRange.firstToken<<" (compiler bug)\n";
        ERRTOKENS(expr->tokenRange)
        ERR_END
        return false;
    }
    // TODO: Can expr->name contain namespace or is that handled with AST_FROM_NAMESPACE?
    //  It can later if we set name to *realTypenName or maybe not? should it?
    //  not if it was accessed? what?
    TypeId typeId = {};
    std::vector<Token> polyTypes;
    Token baseName = AST::TrimPolyTypes(*expr->name, &polyTypes);
    // Token typeName = AST::TrimNamespace(noPointers, &ns);
    std::vector<TypeId> polyIds;
    std::string* realTypeName = nullptr; // rename to realFuncName?
    if(polyTypes.size()!=0){
        // We trim poly types and then put them back together to get the "official" names for them.
        // Maybe you used some aliasing or namespaces.
        realTypeName = info.ast->createString();
        *realTypeName += baseName;
        *realTypeName += "<";
        // TODO: Virtual poly arguments does not work with multithreading. Or you need mutexes at least.
        for(int i=0;i<(int)polyTypes.size();i++){
            // TypeInfo* polyInfo = info.ast->convertToTypeInfo(polyTypes[i], scopeId);
            // TypeId id = info.ast->convertToTypeId(polyTypes[i], scopeId);
            bool printedError = false;
            TypeId id = CheckType(info, scope->scopeId, polyTypes[i], expr->tokenRange, &printedError);
            if(i!=0)
                *realTypeName += ",";
            *realTypeName += info.ast->typeToString(id);
            polyIds.push_back(id);
            if(id.isValid()){
            //     baseInfo->astStruct->polyArgs[i].virtualType->id = id;
            } else if(!printedError) {
                ERR_HEAD2(expr->tokenRange) << "Type for polymorphic argument was not valid\n";
                ERRTOKENS(expr->tokenRange);
                ERR_END
            }
            // baseInfo->astStruct->polyArgs[i].virtualType->id = polyInfo->id;
        }
        *realTypeName += ">";
        *expr->name = *realTypeName; // We modify the expression to use the official poly types
    }
    if (expr->boolValue){
        TypeId structType = {};
        CheckExpression(info,scope, expr->left, &structType);
        if(structType.getPointerLevel()>1){
            ERR_HEAD(expr->left->tokenRange, "'"<<info.ast->typeToString(structType)<<"' to much of a pointer.\n";
                ERR_LINE(expr->left->tokenRange,"must be a reference to a struct");
            )
            return false;
        }
        TypeInfo* ti = info.ast->getTypeInfo(structType.baseType());
        
        if(!ti || !ti->astStruct){
            ERR_HEAD(expr->tokenRange, "'"<<info.ast->typeToString(structType)<<"' is not a struct. Methods cannot be called.\n";
                ERR_LINE(expr->left->tokenRange,"not a struct");
            )
            return false;
        }
        if(!ti->getImpl()){
            ERR_HEAD(expr->tokenRange, "'"<<info.ast->typeToString(structType)<<"' is a polymorphic struct with not poly args specified.\n";
                ERR_LINE(expr->left->tokenRange,"base polymorphic struct");
            )
            return false;
        }
        if(polyTypes.size()==0) {
            *outType = AST_VOID;
            StructImpl::Method baseMethod = ti->getImpl()->getMethod(baseName);
            if(baseMethod.astFunc && baseMethod.astFunc->polyArgs.size()!=0){
                ERR_HEAD(expr->tokenRange, "'"<<baseName<<"' is a polymorphic method of '"<<info.ast->typeToString(structType.baseType())<<"'. Your usage of '"<<*expr->name<<"' does not contain polymorphic arguments.\n\n";
                    ERR_LINE(expr->tokenRange,"ex: func<i32>(...)");
                )
                return false;
            }
            if(ti->astStruct->polyArgs.size()!=0){
                if(baseMethod.astFunc){
                    if(baseMethod.funcImpl->returnTypes.size()!=0)
                        *outType = baseMethod.funcImpl->returnTypes[0].typeId;
                    // ERR_HEAD(expr->tokenRange, "'"<<baseName<<"' is not a method of '"<<info.ast->typeToString(structType.baseType())<<"'.\n\n";
                    //     ERR_LINE(expr->tokenRange,"not a method");
                    // )
                    return true;
                }
                // we want base function and then create poly function?
                // fncall name doesn't have base? we need to trim?
                StructImpl::Method baseMethod = ti->astStruct->baseImpl.getMethod(baseName);
                if(baseMethod.astFunc && baseMethod.astFunc->polyArgs.size()!=0){
                    ERR_HEAD(expr->tokenRange, "'"<<baseName<<"' is a polymorphic method of '"<<info.ast->typeToString(structType.baseType())<<"'. Your usage of '"<<*expr->name<<"' does not contain polymorphic arguments.\n\n";
                        ERR_LINE(expr->tokenRange,"ex: func<i32>(...)");
                    )
                    return false;
                }

                ScopeInfo* funcScope = info.ast->getScope(baseMethod.astFunc->scopeId);

                FuncImpl* funcImpl = (FuncImpl*)engone::Allocate(sizeof(FuncImpl));
                new(funcImpl)FuncImpl();
                funcImpl->name = baseName;
                funcImpl->structImpl = ti->getImpl();

                // It's not a direct poly method, the poly struct makes the method
                // poly
                ti->getImpl()->addPolyMethod(baseName, baseMethod.astFunc, funcImpl);
                
                baseMethod.astFunc->polyImpls.push_back(funcImpl);
                // funcImpl->polyIds.reserve(polyIds.size()); // no polyIds to set

                for(int i=0;i<(int)ti->getImpl()->polyIds.size();i++){
                    TypeId id = ti->getImpl()->polyIds[i];
                    if(id.isValid()){
                        ti->astStruct->polyArgs[i].virtualType->id = id;
                    } 
                }
                
                // TODO: What you are calling a struct method?  if (expr->boolvalue) do structmethod
                int result = CheckFunctionImpl(info,baseMethod.astFunc,funcImpl,ti->astStruct, outType);
                
                for(int i=0;i<(int)ti->getImpl()->polyIds.size();i++){
                    // TypeId id = polyIds[i];
                    // if(id.isValid()){
                        ti->astStruct->polyArgs[i].virtualType->id = {};
                    // } 
                }
            } else {
                if(!baseMethod.astFunc){
                ERR_HEAD(expr->tokenRange, "'"<<baseName<<"' is not a method of '"<<info.ast->typeToString(structType.baseType())<<"'.\n\n";
                    ERR_LINE(expr->tokenRange,"not a method");
                )
                return false;
                }
                if(baseMethod.astFunc->polyArgs.size()!=0){
                    ERR_HEAD(expr->tokenRange, "'"<<baseName<<"' is a polymorphic method of '"<<info.ast->typeToString(structType.baseType())<<"'. Your usage of '"<<*expr->name<<"' does not contain polymorphic arguments.\n\n";
                        ERR_LINE(expr->tokenRange,"ex: func<i32>(...)");
                    )
                    return false;
                }
                *outType = AST_VOID;
                if(baseMethod.funcImpl->returnTypes.size()!=0)
                    *outType = baseMethod.funcImpl->returnTypes[0].typeId;
            }
        } else {
            StructImpl::Method method = ti->getImpl()->getMethod(*realTypeName);
            // ASTStruct::Method method = ti->astStruct->getMethod(*realTypeName);
            if(method.astFunc)      
                return true; // poly method already exists, no worries, everything is fine.
            
            StructImpl::Method baseMethod = ti->astStruct->baseImpl.getMethod(baseName);
            // ASTStruct::Method baseMethod = ti->astStruct->getMethod(baseName);
            if(!baseMethod.astFunc){
                ERR_HEAD(expr->tokenRange, "'"<<baseName<<"' is not a method of "<<info.ast->typeToString(structType.baseType())<<".\n\n";
                ERR_LINE(expr->tokenRange,"bad");
                )
                return false;
            }
            if(baseMethod.astFunc->polyArgs.size()==0){
                ERR_HEAD(expr->tokenRange, "Method '"<<baseName<<"' isn't polymorphic.\n\n";
                ERR_LINE(expr->tokenRange,"bad");
                )
                return false;
            }
            
            // we want base function and then create poly function?
            // fncall name doesn't have base? we need to trim?
            ScopeInfo* funcScope = info.ast->getScope(baseMethod.astFunc->scopeId);

            FuncImpl* funcImpl = (FuncImpl*)engone::Allocate(sizeof(FuncImpl));
            new(funcImpl)FuncImpl();
            funcImpl->name = *realTypeName;
            funcImpl->structImpl = ti->getImpl();

            ti->getImpl()->addPolyMethod(*realTypeName, baseMethod.astFunc, funcImpl);
            
            baseMethod.astFunc->polyImpls.push_back(funcImpl);
            funcImpl->polyIds.reserve(polyIds.size());

            for(int i=0;i<(int)ti->getImpl()->polyIds.size();i++){
                TypeId id = ti->getImpl()->polyIds[i];
                if(id.isValid()){
                    ti->astStruct->polyArgs[i].virtualType->id = id;
                } 
            }
            
            for(int i=0;i<(int)polyIds.size();i++){
                TypeId id = polyIds[i];
                if(id.isValid()){
                    baseMethod.astFunc->polyArgs[i].virtualType->id = id;
                    funcImpl->polyIds[i] = id;
                } 
            }
            
            // TODO: What you are calling a struct method?  if (expr->boolvalue) do structmethod
            int result = CheckFunctionImpl(info,baseMethod.astFunc,funcImpl,ti->astStruct, outType);
            
            for(int i=0;i<(int)polyIds.size();i++){
                TypeId id = polyIds[i];
                if(id.isValid()){
                    baseMethod.astFunc->polyArgs[i].virtualType->id = {};
                } 
            }
            for(int i=0;i<(int)ti->getImpl()->polyIds.size();i++){
                // TypeId id = polyIds[i];
                // if(id.isValid()){
                    ti->astStruct->polyArgs[i].virtualType->id = {};
                // } 
            }
        }
        return true;
    }
    TypeId someType={};
    if(expr->left)
        CheckExpression(info,scope,expr->left,&someType);

    // Token fname 
    Identifier* id = nullptr;
    if(polyTypes.size() == 0) {
        // poly function doesn't exist
        id = info.ast->getIdentifier(scope->scopeId, baseName);
        if(id && id->astFunc && id->astFunc->polyArgs.size() != 0){
            ERR_HEAD2(expr->tokenRange) << "Function "<<baseName <<" requires polymorphic arguments\n";
            ERRTOKENS(expr->tokenRange);
            ERR_END
        }
        if(id && id->astFunc) {
            auto func = info.ast->getFunction(id);
            *outType = AST_VOID;
            if(func->baseImpl.returnTypes.size()!=0)
                *outType = func->baseImpl.returnTypes[0].typeId;
        }
        // Non-existant function (or non-function identifier) is handled in generator. Whether it should do that 
        // is arguable.
    } else {
        id = info.ast->getIdentifier(scope->scopeId, *realTypeName);
        if (!id) {
            // poly function doesn't exist
            id = info.ast->getIdentifier(scope->scopeId, baseName);
            if(!id){
                ERR_HEAD2(expr->tokenRange) << "Function "<<baseName <<" doesn't exist\n";
                ERRTOKENS(expr->tokenRange);
                ERR_END
            } else {
                ASTFunction* astFunc = info.ast->getFunction(id);
                if(astFunc->polyArgs.size()==0){
                    ERR_HEAD2(expr->tokenRange) << "Function "<<baseName<<" isn't polymorphic\n";
                    ERRTOKENS(expr->tokenRange);
                    ERR_END
                } else {
                    // we want base function and then create poly function?
                    // fncall name doesn't have base? we need to trim?
                    ScopeInfo* funcScope = info.ast->getScope(astFunc->scopeId);
                    Identifier* polyFuncId = info.ast->addFunction(funcScope->parent, *realTypeName, astFunc);
                    
                    polyFuncId->funcImpl = (FuncImpl*)engone::Allocate(sizeof(FuncImpl));
                    new(polyFuncId->funcImpl)FuncImpl();
                    polyFuncId->funcImpl->name = *realTypeName;
                    astFunc->polyImpls.push_back(polyFuncId->funcImpl);
                    polyFuncId->funcImpl->polyIds.reserve(polyIds.size());

                    for(int i=0;i<(int)polyIds.size();i++){
                        TypeId id = polyIds[i];
                        if(id.isValid()){
                            astFunc->polyArgs[i].virtualType->id = id;
                            polyFuncId->funcImpl->polyIds[i] = id;
                        } 
                    }
                    
                    // TODO: What you are calling a struct method?  if (expr->boolvalue) do structmethod
                    int result = CheckFunctionImpl(info,astFunc,polyFuncId->funcImpl,nullptr, outType);

                    for(int i=0;i<(int)polyIds.size();i++){
                        TypeId id = polyIds[i];
                        if(id.isValid()){
                            astFunc->polyArgs[i].virtualType->id = {};
                        } 
                    }
                }
            }
        }
    }
    return true;
}
int CheckExpression(CheckInfo& info, ASTScope* scope, ASTExpression* expr, TypeId* outType){
    using namespace engone;
    MEASURE;
    Assert(scope)
    Assert(expr)
    _TC_LOG(FUNC_ENTER)
    
    if(expr->left && expr->typeId != AST_FNCALL) {
        CheckExpression(info,scope, expr->left, outType);
    }
    if(expr->right) {
        TypeId temp={};
        CheckExpression(info,scope, expr->right, expr->left?&temp: outType);
        // TODO: PRIORITISING LEFT FOR OUTPUT TYPE WONT ALWAYS WORK
        //  Some operations like "2 + ptr" will use i32 as out type when
        //  it should be of pointer type
    }
    if(expr->typeId == AST_REFER){
        outType->setPointerLevel(outType->getPointerLevel()+1);
    } else if(expr->typeId == AST_DEREF){
        if(outType->getPointerLevel()==0){
            ERR_HEAD(expr->left->tokenRange, "Cannot dereference non-pointer.\n\n";
                ERR_LINE(expr->left->tokenRange,"not a pointer");
            )
        }else{
            outType->setPointerLevel(outType->getPointerLevel()-1);
        }
    } else if(expr->typeId == AST_MEMBER){
        // outType holds the type of expr->left
        TypeInfo* ti = info.ast->getTypeInfo(outType->baseType());
        if(ti && ti->astStruct){
            TypeInfo::MemberData memdata = ti->getMember(*expr->name);
            if(memdata.index!=-1){
                *outType = memdata.typeId;
            }
        }
    } else if(expr->typeId == AST_INDEX) {
        if(outType->getPointerLevel()==0){
            std::string strtype = info.ast->typeToString(*outType);
            ERR_HEAD(expr->left->tokenRange, "Cannot index non-pointer.\n\n";
                ERR_LINE(expr->left->tokenRange,strtype.c_str());
            )
        }else{
            outType->setPointerLevel(outType->getPointerLevel()-1);
        }
    } else if(expr->typeId == AST_VAR){
        auto iden = info.ast->getIdentifier(scope->scopeId, *expr->name);
        if(iden){
            auto var = info.ast->getVariable(iden);
            if(var){
                *outType = var->typeId;
            }
        }
    }
    if(expr->typeId == AST_STRING){
        if(!expr->name){
            ERR_HEAD2(expr->tokenRange) << "string was null at "<<expr->tokenRange.firstToken<<" (compiler bug)\n";
            ERRTOKENS(expr->tokenRange)
            ERR_END
            return false;
        }

        auto pair = info.ast->constStrings.find(*expr->name);
        if(pair == info.ast->constStrings.end()){
            AST::ConstString tmp={};
            tmp.length = expr->name->length();
            info.ast->constStrings[*expr->name] = tmp;
        }
        *outType = CheckType(info, scope->scopeId, "Slice<char>", expr->tokenRange, nullptr);
    } else if(expr->typeId == AST_SIZEOF) {
        if(!expr->name){
            ERR_HEAD2(expr->tokenRange) << "string was null at "<<expr->tokenRange.firstToken<<" (compiler bug)\n";
            ERRTOKENS(expr->tokenRange)
            ERR_END
            return false;
        }
        auto ti = CheckType(info, scope->scopeId, *expr->name, expr->tokenRange, nullptr);
        int size = 0;
        if(ti.isValid()){
            size = info.ast->getTypeSize(ti);
        }
        expr->typeId = AST_UINT32;
        expr->i64Value = size;
        *outType = expr->typeId;
    } else if(expr->typeId == AST_FNCALL){
        CheckFncall(info,scope,expr, outType);
    }
    if(expr->typeId.isString()){
        bool printedError = false;
        auto ti = CheckType(info, scope->scopeId, expr->typeId, expr->tokenRange, &printedError);
        if (ti.isValid()) {
        } else if(!printedError){
            ERR_HEAD2(expr->tokenRange) << "type "<< info.ast->getTokenFromTypeString(expr->typeId) << " does not exist\n";
            ERR_END
        }
        expr->typeId = ti;
        *outType = expr->typeId;
    }
    if(expr->castType.isString()){
        bool printedError = false;
        auto ti = CheckType(info, scope->scopeId, expr->castType, expr->tokenRange, &printedError);
        if (ti.isValid()) {
        } else if(!printedError){
            ERR_HEAD2(expr->tokenRange) << "type "<<info.ast->getTokenFromTypeString(expr->castType) << " does not exist\n";
            ERR_END
        }
        expr->castType = ti;
        *outType = expr->castType;
    }
    if(expr->typeId.getId() < AST_TRUE_PRIMITIVES){
        *outType = expr->typeId;
    }
    
    if(expr->next) {
        TypeId typeId={};
        CheckExpression(info, scope, expr->next, &typeId);
    }
        
    return true;
}

int CheckRest(CheckInfo& info, ASTScope* scope);
int CheckFunctionImpl(CheckInfo& info, ASTFunction* func, FuncImpl* funcImpl, ASTStruct* parentStruct, TypeId* outType){
    using namespace engone;
    MEASURE;
    _TC_LOG(FUNC_ENTER)
    int offset = 0; // offset starts before call frame (fp, pc)
    int firstSize = 0;

    _TC_LOG(log::out << "FUNC IMPL "<< funcImpl->name<<"\n";)

    // TODO: parentStruct argument may not be necessary since this function only calculates
    //  offsets of arguments and return values.

    funcImpl->arguments.resize(func->baseImpl.arguments.size());
    funcImpl->returnTypes.resize(func->baseImpl.returnTypes.size());

    // Based on 8-bit alignment. The last argument must be aligned by it.
    for(int i=0;i<(int)func->arguments.size();i++){
        auto& arg = func->arguments[i];
        auto& argBase = func->baseImpl.arguments[i];
        auto& argImpl = funcImpl->arguments[i];
        // TypeInfo* typeInfo = 0;
        if(argBase.typeId.isString()){
            bool printedError = false;
            // Token token = info.ast->getTokenFromTypeString(arg.typeId);
            auto ti = CheckType(info, func->scopeId, argBase.typeId, func->tokenRange, &printedError);
            
            if(ti.isValid()){
            }else if(!printedError) {
                ERR_HEAD2(func->tokenRange) << info.ast->getTokenFromTypeString(argBase.typeId) <<" was void (type doesn't exist or you used void which isn't allowed)\n";
                ERR_END
            }
            argImpl.typeId = ti;
        } else {
            argImpl.typeId = argBase.typeId;
        }
            
        int size = info.ast->getTypeSize(argImpl.typeId);
        int asize = info.ast->getTypeAlignedSize(argImpl.typeId);
        // Assert(size != 0 && asize != 0);
        if(size ==0 || asize == 0) // Probably due to an error which was logged. We don't want to assert and crash the compiler.
            continue;
        if(i==0)
            firstSize = size;
        
        if((offset%asize) != 0){
            offset += asize - offset%asize;
        }
        argImpl.offset = offset;
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
        auto& argImpl = funcImpl->arguments[i];
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
        auto& retBase = func->baseImpl.returnTypes[i];
        // TypeInfo* typeInfo = 0;
        if(retBase.typeId.isString()){
            bool printedError = false;
            auto ti = CheckType(info, func->scopeId, retBase.typeId, func->tokenRange, &printedError);
            if(ti.isValid()){
            }else if(!printedError) {
                // TODO: fix location?
                ERR_HEAD2(func->tokenRange) << info.ast->getTokenFromTypeString(retBase.typeId)<<" is not a type (function)\n";
                ERR_END
            }
            retImpl.typeId = ti;
        } else {
            retImpl.typeId = retBase.typeId;
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
    for(int i=0;i<(int)funcImpl->returnTypes.size();i++){
        auto& ret = funcImpl->returnTypes[i];
        // TypeInfo* typeInfo = info.ast->getTypeInfo(arg.typeId);
        int size = info.ast->getTypeSize(ret.typeId);
        ret.offset = offset - ret.offset - size;
        // log::out << " Reverse Arg "<<arg.offset << ": "<<arg.name<<"\n";
    }
    funcImpl->returnSize = -offset;

    if(funcImpl->returnTypes.size()==0){
        *outType = AST_VOID;
    } else {
        *outType = funcImpl->returnTypes[0].typeId;
    }

    return true;
}
int CheckFunctions(CheckInfo& info, ASTScope* scope){
    using namespace engone;
    MEASURE;
    _TC_LOG(FUNC_ENTER)
    if(!scope)
        return false;

    ASTScope* nextNamespace = scope->namespaces;
    while(nextNamespace) {
        ASTScope* now = nextNamespace;
        nextNamespace = nextNamespace->next;
        
        CheckFunctions(info, now);
    }

    // This code goes through the functions and methods of structs in this scope
    bool checkScopeFunctions = scope->functions;
    ASTStruct* nextStruct = scope->structs;
    while(nextStruct || checkScopeFunctions) {
        ASTFunction* nextFunction=nullptr;
        ASTStruct* nowStruct = nullptr;
        if(checkScopeFunctions){
            checkScopeFunctions = false;
            nextFunction = scope->functions;
        } else {
            nowStruct = nextStruct;
            nextFunction = nowStruct->functions;
            nextStruct = nextStruct->next;
        }
        while(nextFunction){
            ASTFunction* function = nextFunction;
            nextFunction = nextFunction->next;
            
            if(function->polyArgs.size()==0 && (!nowStruct || nowStruct->polyArgs.size()==0)){
                // the struct above may have stuff
                // check impl for function or method
                TypeId ttttt={};
                bool yes = CheckFunctionImpl(info, function, &function->baseImpl, nowStruct, &ttttt);
            } else {
                for(int i=0;i<(int)function->polyArgs.size();i++){
                    auto& arg = function->polyArgs[i];
                    arg.virtualType = info.ast->createType(arg.name, function->scopeId);
                    _TC_LOG(log::out << "Virtual type["<<i<<"] "<<arg.name<<"\n";)
                }
                _TC_LOG(log::out << "Method/function has polymorphic properties: "<<function->name<<"\n";)
            }
            _TC_LOG(log::out << "Defined "<<(nowStruct?"method ":"function ")<<function->name<<"\n";)
            Identifier* id = info.ast->addFunction(scope->scopeId, function->name, function);
            if(function->polyArgs.size()==0){
                id->funcImpl = &function->baseImpl;
            } else {
                // will always be nullptr
                // the polymorphic version will not be (it uses a different identifier)
                id->funcImpl = nullptr;
            }
            if (!id) {
                ERR_HEAD2(function->tokenRange) << function->name << " is already defined\n";
                ERR_END
            }

            int result = CheckFunctions(info, function->body);
        }
    }
    
    ASTStatement* nextState = scope->statements;
    while(nextState) {
        ASTStatement* astate = nextState;
        nextState = nextState->next;
        
        if(astate->body){
            int result = CheckFunctions(info, astate->body);   
            // if(!result)
            //     error = false;
        }
        if(astate->elseBody){
            int result = CheckFunctions(info, astate->elseBody);
            // if(!result)
            //     error = false;
        }
    }
    
    return true;
}
int CheckRest(CheckInfo& info, ASTScope* scope){
    using namespace engone;
    MEASURE;
    _TC_LOG(FUNC_ENTER)

    // Hello me in the future!
    // I have disrespectfully left a complex and troublesome problem to you.
    // CheckRest needs to be run for each function implementation to find new
    // polymorphic functions to check and also run CheckRest on. This is highly
    // recursive and very much a spider net. Good luck.

    // TODO: Type check default values in structs and functions
    bool nonStruct = true;
    ASTStruct* nextStruct = scope->structs;
    while(nextStruct) {
        ASTStruct * now = nextStruct;
        nextStruct = nextStruct->next;

        // TODO: setup variables
        ASTFunction* nextFunc = nullptr;
        if(nonStruct){
            nextFunc = scope->functions;
            nonStruct = false;
        }else{
            nextFunc = now->functions;
        }

        while(nextFunc) {
            ASTFunction* func = nextFunc;
            nextFunc = nextFunc->next;
            if(func->body->nativeCode)
                continue;

            for(int i=0;i<(int)func->polyImpls.size() || func->polyImpls.size()==0;i++){
                FuncImpl* funcImpl = nullptr;
                if(func->polyImpls.size()==0)
                    funcImpl = &func->baseImpl;
                else
                    funcImpl = func->polyImpls[i];
                
                struct Var {
                    ScopeId scopeId;
                    std::string name;
                };
                // TODO: Add arguments as variables
                std::vector<Var> vars;
                _TC_LOG(log::out << "arg ";)
                for (int i=0;i<(int)func->arguments.size();i++) {
                    auto& arg = func->arguments[i];
                    auto& argImpl = funcImpl->arguments[i];
                    _TC_LOG(log::out << " " << arg.name<<": "<< info.ast->typeToString(argImpl.typeId) <<"\n";)
                    auto varinfo = info.ast->addVariable(scope->scopeId, std::string(arg.name));
                    if(varinfo){
                        varinfo->typeId = argImpl.typeId; // typeId comes from CheckExpression which may or may not evaluate
                        // the same type as the generator.
                        vars.push_back({scope->scopeId,std::string(arg.name)});
                    }
                }
                _TC_LOG(log::out << "\n";)

                CheckRest(info, func->body);
                
                for(auto& e : vars){
                    info.ast->removeIdentifier(e.scopeId, e.name);
                }
                // TODO: Remove variables
                
                if(func->polyImpls.size()==0)
                    break;
            }
        }
    }
    struct A {
        ScopeId scopeId;
        std::string name;
    };
    std::vector<A> vars;
    ASTStatement* next = scope->statements;
    ASTStatement* nextPrev = nullptr;
    while(next){
        ASTStatement* now = next;
        ASTStatement* prev = nextPrev;
        next = next->next;
        nextPrev = now;

        // if(now->lvalue)
        //     CheckExpression(info, scope ,now->lvalue);
        TypeId typeId={};
        if(now->rvalue)
            CheckExpression(info, scope, now->rvalue, &typeId);
        
        for(auto& var : now->varnames){
            if(var.assignType.isString()){
                bool printedError = false;
                auto ti = CheckType(info, scope->scopeId, var.assignType, now->tokenRange, &printedError);
                // NOTE: We don't care whether it's a pointer just that the type exists.
                if (!ti.isValid() && !printedError) {
                    ERR_HEAD2(now->tokenRange) << info.ast->getTokenFromTypeString(var.assignType)<<" is not a type (statement)\n";
                    ERR_END
                } else {
                    // If typeid is invalid we don't want to replace the invalid one with the type
                    // with the string. The generator won't see the names of the invalid types.
                    // now->typeId = ti;
                    var.assignType = ti;
                }
            }
        }

        if(now->body){
            int result = CheckRest(info, now->body);
        }
        if(now->elseBody){
            int result = CheckRest(info, now->elseBody);
        }
        if(now->type == ASTStatement::ASSIGN){
            _TC_LOG(log::out << "assign ";)
            for (auto& var : now->varnames) {
                _TC_LOG(log::out << " " << var.name<<": "<< info.ast->typeToString(var.assignType) <<"\n";)
                auto varinfo = info.ast->addVariable(scope->scopeId, std::string(var.name));
                if(varinfo){
                    varinfo->typeId = typeId; // typeId comes from CheckExpression which may or may not evaluate
                    // the same type as the generator.
                    vars.push_back({scope->scopeId,std::string(var.name)});
                }
            }
            _TC_LOG(log::out << "\n";)
        } else 
        // Doing using after body so that continue can be used inside it without
        // messing things up. Using shouldn't have a body though so it doesn't matter.
        if(now->type == ASTStatement::USING){
            // TODO: type check now->name now->alias
            //  if not type then check namespace
            //  otherwise it's variables
            
            if(now->varnames.size()==1 && now->alias){
                Token& name = now->varnames[0].name;
                TypeId originType = CheckType(info, scope->scopeId, name, now->tokenRange, nullptr);
                TypeId aliasType = info.ast->convertToTypeId(*now->alias, scope->scopeId);
                if(originType.isValid() && !aliasType.isValid()){
                    TypeInfo* aliasInfo = info.ast->createType(*now->alias, scope->scopeId);
                    aliasInfo->id = originType;

                    // using statement isn't necessary anymore
                    now->next = nullptr;
                    info.ast->destroy(now);
                    if(prev){
                        prev->next = next;
                    } else {
                        scope->statements = next;
                    }
                    continue;
                }
                ScopeInfo* originScope = info.ast->getScopeFromParents(name,scope->scopeId);
                ScopeInfo* aliasScope = info.ast->getScopeFromParents(*now->alias,scope->scopeId);
                if(originScope && !aliasScope) {
                    ScopeInfo* scopeInfo = info.ast->getScope(scope->scopeId);
                    // TODO: Alias can't contain multiple namespaces. How to implement
                    //  that?
                    scopeInfo->nameScopeMap[*now->alias] = originScope->id;

                    now->next = nullptr;
                    info.ast->destroy(now);
                    if(prev){
                        prev->next = next;
                    } else {
                        scope->statements = next;
                    }
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
                now->next = nullptr;
                info.ast->destroy(now);
                if(prev){
                    prev->next = next;
                } else {
                    scope->statements = next;
                }
                continue;
            }
        }
    }
    for(auto& e : vars){
        info.ast->removeIdentifier(e.scopeId, e.name);
    }
    ASTScope* nextNS = scope->namespaces;
    while(nextNS){
        ASTScope* now = nextNS;
        nextNS = nextNS->next;

        int result = CheckRest(info, now);
    }
    return 0;
}
int TypeCheck(AST* ast, ASTScope* scope, CompileInfo* compileInfo){
    using namespace engone;
    MEASURE;
    CheckInfo info = {};
    info.ast = ast;
    info.compileInfo = compileInfo;

    _VLOG(log::out << log::BLUE << "Type check:\n";)

    int result = CheckEnums(info, scope);
    
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
            return info.errors;
        }
    }
    result = CheckFunctions(info, scope);

    result = CheckRest(info, scope);
    
    info.compileInfo->errors += info.errors;
    return info.errors;
}