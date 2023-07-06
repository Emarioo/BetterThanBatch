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
#define WARN_HEAD(R,M) info.compileInfo->warnings++; engone::log::out << WARN_DEFAULT_R(R,"Type warning","W0000") << M
#define ERR_END MSG_END

#define ERR_LINE(R, M) PrintCode(R, M)
#define ERR_LINET(T, M) PrintCode(T.tokenIndex, T.tokenStream, M)
// #define ERRTYPE(R,LT,RT) ERR_HEAD2(R) << "Type mismatch "<<info.ast->getTypeInfo(LT)->name<<" - "<<info.ast->getTypeInfo(RT)->name<<" "

#define ERRTOKENS(R) engone::log::out <<engone::log::GREEN<<R.firstToken.line <<" |> "; R.print();engone::log::out << "\n";

#define TOKENINFO(R) {std::string temp="";R.feed(temp);info.code->addDebugText(std::string("Ln ")+std::to_string(R.firstToken.line)+ ": ");info.code->addDebugText(temp+"\n");}

#ifdef TC_LOG
// #define _TC_LOG_ENTER(X) X
#define _TC_LOG_ENTER(X)
#else
#define _TC_LOG_ENTER(X)
#endif

int CheckEnums(CheckInfo& info, ASTScope* scope){
    using namespace engone;
    Assert(scope);
    MEASURE;
    _TC_LOG_ENTER(FUNC_ENTER)
    
    int error = true;
    for(auto aenum : scope->enums){
        TypeInfo* typeInfo = info.ast->createType(aenum->name, scope->scopeId);
        if(typeInfo){
            _TC_LOG(log::out << "Defined enum "<<info.ast->typeToString(typeInfo->id)<<"\n";)
            typeInfo->_size = 4; // i32
            typeInfo->astEnum = aenum;
        } else {
            ERR_HEAD(((TokenRange)aenum->name), aenum->name << " is taken.\n";
            )
        }
    }
    
    for(auto it : scope->namespaces){
        int result = CheckEnums(info,it);
        if(!result)
            error = false;
    }
    
    for(auto astate : scope->statements) {
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
    
    for(auto afunc : scope->functions) {
        if(afunc->body){
            int result = CheckEnums(info, afunc->body);
            if(!result)
                error = false;
        }
    }
    return error;
}
TypeId CheckType(CheckInfo& info, ScopeId scopeId, TypeId typeString, const TokenRange& tokenRange, bool* printedError);
TypeId CheckType(CheckInfo& info, ScopeId scopeId, Token typeString, const TokenRange& tokenRange, bool* printedError);
bool CheckStructImpl(CheckInfo& info, ASTStruct* astStruct, TypeInfo* structInfo, StructImpl* structImpl){
    using namespace engone;
    _TC_LOG_ENTER(FUNC_ENTER)
    int offset=0;
    int alignedSize=0; // offset will be aligned to match this at the end

    Assert(astStruct->polyArgs.size() == structImpl->polyArgs.size())
    DynamicArray<TypeId> prevVirtuals{};
    prevVirtuals.resize(astStruct->polyArgs.size());
    for(int i=0;i<(int)astStruct->polyArgs.size();i++){
        TypeId id = structImpl->polyArgs[i];
        Assert(id.isValid());
        prevVirtuals[i] = astStruct->polyArgs[i].virtualType->id;
        astStruct->polyArgs[i].virtualType->id = id;
    }
    defer{
        for(int i=0;i<(int)astStruct->polyArgs.size();i++){
            astStruct->polyArgs[i].virtualType->id = prevVirtuals[i];
        }
    };
   
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
    _VLOG(log::out << "Struct "<<log::LIME << astStruct->name<<polys<<log::SILVER<<" (size: "<<structImpl->size <<(astStruct->isPolymorphic()?", poly. impl.":"")<<")\n";)
    for(int i=0;i<(int)structImpl->members.size();i++){
        auto& name = astStruct->members[i].name;
        auto& mem = structImpl->members[i];
        
        i32 size = info.ast->getTypeSize(mem.typeId);
        _VLOG(log::out << " "<<mem.offset<<": "<<name<<" ("<<size<<" bytes)\n";)
    }
    // _TC_LOG(log::out << info.ast->typeToString << " was evaluated to "<<offset<<" bytes\n";)
    // }
    return success;
}
TypeId CheckType(CheckInfo& info, ScopeId scopeId, TypeId typeString, const TokenRange& tokenRange, bool* printedError){
    using namespace engone;
    Assert(typeString.isString())
    if(!typeString.isString()) {
        _TC_LOG(log::out << "check type typeid wasn't string type\n";)
        return typeString;
    }
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
        ERR() << "Polymorphic type "<<typeString << " has "<< polyTokens.size() <<" poly. args but the base type "<<info.ast->typeToString(baseInfo->id)<<" needs "<<baseInfo->astStruct->polyArgs.size()<< "\n";
        if(printedError)
            *printedError = true;
        return {}; // type isn't polymorphic and does just not exist
    }

    TypeInfo* typeInfo = info.ast->createType(*realTypeName, baseInfo->scopeId);
    typeInfo->astStruct = baseInfo->astStruct;
    typeInfo->structImpl = (StructImpl*)engone::Allocate(sizeof(StructImpl));
    new(typeInfo->structImpl)StructImpl();

    typeInfo->structImpl->polyArgs.resize(polyArgs.size());
    for(int i=0;i<(int)polyArgs.size();i++){
        TypeId id = polyArgs[i];
        typeInfo->structImpl->polyArgs[i] = id;
    }

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
                ERR_HEAD(astStruct->tokenRange, "'"<<astStruct->name<<"' is already defined.\n");
                // TODO: Provide information (file, line, column) of the first definition.
                // We don't care about another turn. We failed but we don't set
                // completedStructs to false since this will always fail.
            } else {
                // _TC_LOG(log::out << "Defined struct "<<info.ast->typeToString(structInfo->id)<<" in scope "<<scope->scopeId<<"\n";)
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
                yes = CheckStructImpl(info, astStruct,structInfo, &astStruct->baseImpl);
                if(!yes){
                    astStruct->state = ASTStruct::TYPE_CREATED;
                    info.completedStructs = false;
                } else {
                    _TC_LOG(log::out << astStruct->name << " was evaluated to "<<astStruct->baseImpl.size<<" bytes\n";)
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
        
        for(auto afunc : scope->functions) {
            if(afunc->body){
                int result = CheckStructs(info, afunc->body);
                // if(!result)
                //     error = false;
            }
        }
    // }
    return true;
}
int CheckExpression(CheckInfo& info, ASTScope* scope, ASTExpression* expr, TypeId* outType, bool checkNext=true);
int CheckFunctionImpl(CheckInfo& info, const ASTFunction* func, FuncImpl* funcImpl, ASTStruct* parentStruct, TypeId* outType);
int CheckFncall(CheckInfo& info, ASTScope* scope, ASTExpression* expr, TypeId* outType) {
    using namespace engone;
    *outType = AST_VOID; // Default to void if zero return values

    //-- Get poly args from the function call
    DynamicArray<TypeId> fnPolyArgs;
    std::vector<Token> polyTokens;
    Token baseName = AST::TrimPolyTypes(expr->name, &polyTokens);
    for(int i=0;i<(int)polyTokens.size();i++){
        bool printedError = false;
        TypeId id = CheckType(info, scope->scopeId, polyTokens[i], expr->tokenRange, &printedError);
        fnPolyArgs.add(id);
        // TODO: What about void?
        if(id.isValid()){

        } else if(!printedError) {
            ERR_HEAD(expr->tokenRange, "Type for polymorphic argument was not valid.\n\n";
                ERR_LINE(expr->tokenRange,"bad");
            )
        }
    }

    /*
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
        Assert(("typeids not evaluated yet",false))
        std::vector<TypeId> chickenDinner;
        if(polyTokens.size()==0) {
            *outType = AST_VOID;
            auto baseMethod = ti->getImpl()->getMethod(baseName, chickenDinner);
            if(baseMethod->astFunc && baseMethod->astFunc->polyArgs.size()!=0){
                ERR_HEAD(expr->tokenRange, "'"<<baseName<<"' is a polymorphic method of '"<<info.ast->typeToString(structType.baseType())<<"'. Your usage of '"<<*expr->name<<"' does not contain polymorphic arguments.\n\n";
                    ERR_LINE(expr->tokenRange,"ex: func<i32>(...)");
                )
                return false;
            }
            if(ti->astStruct->polyArgs.size()!=0){
                if(baseMethod->astFunc){
                    if(baseMethod->funcImpl->returnTypes.size()!=0)
                        *outType = baseMethod->funcImpl->returnTypes[0].typeId;
                    // ERR_HEAD(expr->tokenRange, "'"<<baseName<<"' is not a method of '"<<info.ast->typeToString(structType.baseType())<<"'.\n\n";
                    //     ERR_LINE(expr->tokenRange,"not a method");
                    // )
                    return true;
                }
                // we want base function and then create poly function?
                // fncall name doesn't have base? we need to trim?
                auto baseMethod = ti->astStruct->baseImpl.getMethod(baseName, chickenDinner);
                if(baseMethod->astFunc && baseMethod->astFunc->polyArgs.size()!=0){
                    ERR_HEAD(expr->tokenRange, "'"<<baseName<<"' is a polymorphic method of '"<<info.ast->typeToString(structType.baseType())<<"'. Your usage of '"<<*expr->name<<"' does not contain polymorphic arguments.\n\n";
                        ERR_LINE(expr->tokenRange,"ex: func<i32>(...)");
                    )
                    return false;
                }

                ScopeInfo* funcScope = info.ast->getScope(baseMethod->astFunc->scopeId);

                FuncImpl* funcImpl = (FuncImpl*)engone::Allocate(sizeof(FuncImpl));
                new(funcImpl)FuncImpl();
                funcImpl->name = baseName;
                funcImpl->structImpl = ti->getImpl();

                // It's not a direct poly method, the poly struct makes the method
                // poly
                ti->getImpl()->addPolyMethod(baseName, baseMethod->astFunc, funcImpl);
                
                baseMethod->astFunc->impls.push_back(funcImpl);
                // funcImpl->polyIds.resize(polyIds.size()); // no polyIds to set

                // for(int i=0;i<(int)ti->getImpl()->polyIds.size();i++){
                //     TypeId id = ti->getImpl()->polyIds[i];
                //     if(id.isValid()){
                //         ti->astStruct->polyArgs[i].virtualType->id = id;
                //     } 
                // }
                
                // TODO: What you are calling a struct method?  if (expr->boolvalue) do structmethod
                int result = CheckFunctionImpl(info,baseMethod->astFunc,funcImpl,ti->astStruct, outType);
                
                // for(int i=0;i<(int)ti->getImpl()->polyIds.size();i++){
                //     // TypeId id = polyIds[i];
                //     // if(id.isValid()){
                //         ti->astStruct->polyArgs[i].virtualType->id = {};
                //     // } 
                // }
            } else {
                if(!baseMethod->astFunc){
                ERR_HEAD(expr->tokenRange, "'"<<baseName<<"' is not a method of '"<<info.ast->typeToString(structType.baseType())<<"'.\n\n";
                    ERR_LINE(expr->tokenRange,"not a method");
                )
                return false;
                }
                if(baseMethod->astFunc->polyArgs.size()!=0){
                    ERR_HEAD(expr->tokenRange, "'"<<baseName<<"' is a polymorphic method of '"<<info.ast->typeToString(structType.baseType())<<"'. Your usage of '"<<*expr->name<<"' does not contain polymorphic arguments.\n\n";
                        ERR_LINE(expr->tokenRange,"ex: func<i32>(...)");
                    )
                    return false;
                }
                *outType = AST_VOID;
                if(baseMethod->funcImpl->returnTypes.size()!=0)
                    *outType = baseMethod->funcImpl->returnTypes[0].typeId;
            }
        } else {
            auto method = ti->getImpl()->getMethod(*realTypeName,chickenDinner);
            // ASTStruct::Method method = ti->astStruct->getMethod(*realTypeName);
            if(method->astFunc)      
                return true; // poly method already exists, no worries, everything is fine.
            
            auto baseMethod = ti->astStruct->baseImpl.getMethod(baseName,chickenDinner);
            // ASTStruct::Method baseMethod = ti->astStruct->getMethod(baseName);
            if(!baseMethod->astFunc){
                ERR_HEAD(expr->tokenRange, "'"<<baseName<<"' is not a method of "<<info.ast->typeToString(structType.baseType())<<".\n\n";
                ERR_LINE(expr->tokenRange,"bad");
                )
                return false;
            }
            if(baseMethod->astFunc->polyArgs.size()==0){
                ERR_HEAD(expr->tokenRange, "Method '"<<baseName<<"' isn't polymorphic.\n\n";
                ERR_LINE(expr->tokenRange,"bad");
                )
                return false;
            }
            
            // we want base function and then create poly function?
            // fncall name doesn't have base? we need to trim?
            ScopeInfo* funcScope = info.ast->getScope(baseMethod->astFunc->scopeId);

            FuncImpl* funcImpl = (FuncImpl*)engone::Allocate(sizeof(FuncImpl));
            new(funcImpl)FuncImpl();
            funcImpl->name = *realTypeName;
            funcImpl->structImpl = ti->getImpl();

            ti->getImpl()->addPolyMethod(*realTypeName, baseMethod->astFunc, funcImpl);
            
            baseMethod->astFunc->impls.push_back(funcImpl);
            funcImpl->polyIds.resize(polyIds.size());

            // for(int i=0;i<(int)ti->getImpl()->polyIds.size();i++){
            //     TypeId id = ti->getImpl()->polyIds[i];
            //     if(id.isValid()){
            //         ti->astStruct->polyArgs[i].virtualType->id = id;
            //     } 
            // }
            
            for(int i=0;i<(int)polyIds.size();i++){
                TypeId id = polyIds[i];
                if(id.isValid()){
                    // baseMethod->astFunc->polyArgs[i].virtualType->id = id;
                    funcImpl->polyIds[i] = id;
                }
            }
            // TODO: BUG! methods args do not see poly type names of parent struct.
            //   or rather the poly types of the parent struct might not be set when
            //   checking args? For overload I mean. Not in CheckFunctionImpl. That works.
            // TODO: What you are calling a struct method?  if (expr->boolvalue) do structmethod
            int result = CheckFunctionImpl(info,baseMethod->astFunc,funcImpl,ti->astStruct, outType);
            
            // for(int i=0;i<(int)polyIds.size();i++){
            //     TypeId id = polyIds[i];
            //     if(id.isValid()){
            //         baseMethod->astFunc->polyArgs[i].virtualType->id = {};
            //     } 
            // }
            // for(int i=0;i<(int)ti->getImpl()->polyIds.size();i++){
            //     // TypeId id = polyIds[i];
            //     // if(id.isValid()){
            //         ti->astStruct->polyArgs[i].virtualType->id = {};
            //     // } 
            // }
        }
        return true;
    }
    */
    //-- Get essential arguments from the function call
    DynamicArray<TypeId> argTypes{};
    for(auto argExpr : *expr->args){
        Assert(argExpr);

        TypeId argType={};
        CheckExpression(info,scope,argExpr,&argType, false);
        if(argExpr->namedValue){
            // don't add argType but keep checking expressions
            // remaining args will be named
        }else{
            // WARN_HEAD(argExpr->tokenRange, "Named arguments broke because of function overloading.\n");
            argTypes.add(argType);
        }
    }
    //-- Get identifier, the namespace of overloads for the function.
    Identifier* iden = info.ast->getIdentifier(scope->scopeId, baseName);
    if(!iden){
        ERR_HEAD(expr->tokenRange, "Function "<<baseName <<" does not exist.\n\n";
            ERR_LINET(baseName,"undefined");
        )
        return false;
    }
    if(fnPolyArgs.size()==0){
        // match args with normal impls
        FnOverloads::Overload* overload = iden->funcOverloads.getOverload(argTypes);
        if(overload){
            if(overload->funcImpl->returnTypes.size()>0)
                *outType = overload->funcImpl->returnTypes[0].typeId;
            return true;
        }
        // TODO: Implicit call to polymorphic functions. Currently throwing error instead.
        //  Should be done in generator too.
        ERR_HEAD(expr->tokenRange, "Overloads for function '"<<baseName <<"' does not match these argument(s): ";
            if(argTypes.size()==0){
                log::out << "zero arguments";
            }
            for(int i=0;i<(int)argTypes.size();i++){
                if(i!=0) log::out << ", ";
                log::out <<info.ast->typeToString(argTypes[i]);
            }
            log::out << "\n";
            if(iden->funcOverloads.polyOverloads.size()!=0){
                log::out << log::YELLOW<<"(implicit call to polymorphic function is not implemented)\n";
            }
            if(expr->args->size()!=0 && expr->args->get(0)->namedValue){
                log::out << log::YELLOW<<"(named arguments cannot identify overloads)\n";
            }
            log::out <<"\n";
            ERR_LINE(expr->tokenRange, "bad");
            // TODO: show list of available overloaded function args
        )
        return false;
        // if implicit polymorphism then
        // macth poly impls
        // generate poly impl if failed
        // use new impl if succeeded.
    }
    // code when we call polymorphic function

    // match args and poly args with poly impls
    // if match then return that impl
    // if not then try to generate a implementation

    FnOverloads::Overload* overload = iden->funcOverloads.getOverload(argTypes,fnPolyArgs);
    if(overload){
        if(overload->funcImpl->returnTypes.size()>0)
            *outType = overload->funcImpl->returnTypes[0].typeId;
        return true;
    }
    ASTFunction* polyFunc = nullptr;
    // // Find possible polymorphic type and later create implementation for it
    for(int i=0;i<(int)iden->funcOverloads.polyOverloads.size();i++){
        FnOverloads::PolyOverload& overload = iden->funcOverloads.polyOverloads[i];
        if(overload.astFunc->polyArgs.size() != fnPolyArgs.size())
            continue;// unless implicit polymorphic types
        for(int j=0;j<(int)overload.astFunc->polyArgs.size();j++){
            overload.astFunc->polyArgs[j].virtualType->id = fnPolyArgs[j];
        }
        defer {
            for(int j=0;j<(int)overload.astFunc->polyArgs.size();j++){
                overload.astFunc->polyArgs[j].virtualType->id = {};
            }
        };
        u32 nonDefaults = 0;
        while (nonDefaults<overload.astFunc->arguments.size() &&
          !overload.astFunc->arguments[nonDefaults].defaultValue){
            nonDefaults++;
        }
        // continue if more args than possible
        // continue if less args than minimally required
        if(argTypes.size() > overload.astFunc->arguments.size() || argTypes.size() < nonDefaults)
            continue;
        bool found = true;
        for (u32 j=0;j<nonDefaults;j++){
            // log::out << "Arg:"<<info.ast->typeToString(overload.astFunc->arguments[j].stringType)<<"\n";
            TypeId argType = CheckType(info, overload.astFunc->scopeId,overload.astFunc->arguments[j].stringType,
            // TypeId argType = CheckType(info, scope->scopeId,overload.astFunc->arguments[j].stringType,
                overload.astFunc->arguments[j].name,nullptr);
            log::out << "Arg: "<<info.ast->typeToString(argType)<<"\n";
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
        ERR_HEAD(expr->tokenRange, "Overloads for function '"<<baseName <<"' does not match these argument(s): ";
            if(argTypes.size()==0){
                log::out << "zero arguments";
            }
            for(int i=0;i<(int)argTypes.size();i++){
                if(i!=0) log::out << ", ";
                log::out <<info.ast->typeToString(argTypes[i]);
            }
            log::out << "\n";
            log::out << log::YELLOW<<"(polymorphic functions could not be generated if there are any)\n";
            log::out <<"\n";
            ERR_LINE(expr->tokenRange, "bad");
            // TODO: show list of available overloaded function args
        )
        return false;
    }
    ScopeInfo* funcScope = info.ast->getScope(polyFunc->scopeId);
    FuncImpl* funcImpl = polyFunc->createImpl();
    funcImpl->name = expr->name;

    funcImpl->polyArgs.resize(fnPolyArgs.size());

    for(int i=0;i<(int)fnPolyArgs.size();i++){
        TypeId id = fnPolyArgs[i];
        funcImpl->polyArgs[i] = id;
    }
    // TODO: What you are calling a struct method?  if (expr->boolvalue) do structmethod
    int result = CheckFunctionImpl(info,polyFunc,funcImpl,nullptr, outType);

    FnOverloads::Overload* newOverload = iden->funcOverloads.addPolyImplOverload(polyFunc, funcImpl);
    CheckInfo::CheckImpl checkImpl{};
    checkImpl.astFunc = polyFunc;
    checkImpl.funcImpl = funcImpl;
    // checkImpl.scope = scope;
    info.checkImpls.add(checkImpl);

    // Can overload be null since we generate a new func impl?
    overload = iden->funcOverloads.getOverload(argTypes, fnPolyArgs);
    Assert(overload == newOverload);
    if(!overload){
        ERR_HEAD(expr->tokenRange, "Specified polymorphic arguments does not match with passed arguments for call to '"<<baseName <<"'.\n";
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
            }
            for(int i=0;i<(int)argTypes.size();i++){
                if(i!=0) log::out << ", ";
                log::out <<info.ast->typeToString(argTypes[i]);
            }
            log::out << "\n";
            // TODO: show list of available overloaded function args
        )
        return false;
    } 
    if(overload->funcImpl->returnTypes.size()>0)
        *outType = overload->funcImpl->returnTypes[0].typeId;
    return true;
}
int CheckExpression(CheckInfo& info, ASTScope* scope, ASTExpression* expr, TypeId* outType, bool checkNext){
    using namespace engone;
    MEASURE;
    Assert(scope)
    Assert(expr)
    _TC_LOG_ENTER(FUNC_ENTER)
    
    // IMPORTANT NOTE: CheckExpression HAS to run for left and right expressions
    //  since they may require types to be checked. AST_SIZEOF needs to evalute for example

    if(expr->typeId == AST_REFER){
        if(expr->left) {
            CheckExpression(info,scope, expr->left, outType);
        }
        outType->setPointerLevel(outType->getPointerLevel()+1);
    } else if(expr->typeId == AST_DEREF){
        if(expr->left) {
            CheckExpression(info,scope, expr->left, outType);
        }
        if(outType->getPointerLevel()==0){
            ERR_HEAD(expr->left->tokenRange, "Cannot dereference non-pointer.\n\n";
                ERR_LINE(expr->left->tokenRange,"not a pointer");
            )
        }else{
            outType->setPointerLevel(outType->getPointerLevel()-1);
        }
    } else if(expr->typeId == AST_MEMBER){
        if(expr->left) {
            CheckExpression(info,scope, expr->left, outType);
        }
        // outType holds the type of expr->left
        TypeInfo* ti = info.ast->getTypeInfo(outType->baseType());
        if(ti && ti->astStruct){
            TypeInfo::MemberData memdata = ti->getMember(expr->name);
            if(memdata.index!=-1){
                *outType = memdata.typeId;
            }
        }
    } else if(expr->typeId == AST_INDEX) {
        if(expr->left) {
            CheckExpression(info,scope, expr->left, outType);
        }
        TypeId temp={};
        if(expr->right) {
            CheckExpression(info,scope, expr->right, &temp);
        }
        if(outType->getPointerLevel()==0){
            std::string strtype = info.ast->typeToString(*outType);
            ERR_HEAD(expr->left->tokenRange, "Cannot index non-pointer.\n\n";
                ERR_LINE(expr->left->tokenRange,strtype.c_str());
            )
        }else{
            outType->setPointerLevel(outType->getPointerLevel()-1);
        }
    } else if(expr->typeId == AST_VAR){
        auto iden = info.ast->getIdentifier(scope->scopeId, expr->name);
        if(iden){
            auto var = info.ast->getVariable(iden);
            if(var){
                *outType = var->typeId;
            }
        }
    } else if(expr->typeId == AST_RANGE){
        TypeId temp={};
        if(expr->left) {
            CheckExpression(info,scope, expr->left, &temp);
        }
        if(expr->right) {
            CheckExpression(info,scope, expr->right, &temp);
        }
        *outType = CheckType(info, scope->scopeId, "Range", expr->tokenRange, nullptr);
    } else if(expr->typeId == AST_STRING){
        // if(!expr->name){
        //     ERR_HEAD2(expr->tokenRange) << "string was null at "<<expr->tokenRange.firstToken<<" (compiler bug)\n";
        //     ERRTOKENS(expr->tokenRange)
        //     ERR_END
        //     return false;
        // }

        auto pair = info.ast->constStrings.find(expr->name);
        if(pair == info.ast->constStrings.end()){
            AST::ConstString tmp={};
            tmp.length = expr->name.length;
            info.ast->constStrings[expr->name] = tmp;
        }
        *outType = CheckType(info, scope->scopeId, "Slice<char>", expr->tokenRange, nullptr);
    }  else if(expr->typeId == AST_SIZEOF) {
        // log::out << log::LIME<<"EVAL SIZEOF"<<"\n";
        // if(!expr->name){
        //     ERR_HEAD2(expr->tokenRange) << "string was null at "<<expr->tokenRange.firstToken<<" (compiler bug)\n";
        //     ERRTOKENS(expr->tokenRange)
        //     ERR_END
        //     return false;
        // }
        auto ti = CheckType(info, scope->scopeId, expr->name, expr->tokenRange, nullptr);
        // THIS COMMENTED CODE modifies the expression which isn't allowed.
        // int size = 0;
        // if(ti.isValid()){
        //     size = info.ast->getTypeSize(ti);
        // }
        // expr->typeId = AST_UINT32;
        // expr->i64Value = size;
        // *outType = expr->typeId;
        *outType = AST_UINT32;
    } else if(expr->typeId == AST_FNCALL){
        CheckFncall(info,scope,expr, outType);
    } else {
        if(expr->left) {
            CheckExpression(info,scope, expr->left, outType);
        }
        if(expr->right) {
            TypeId temp={};
            CheckExpression(info,scope, expr->right, expr->left?&temp: outType);
            // TODO: PRIORITISING LEFT FOR OUTPUT TYPE WONT ALWAYS WORK
            //  Some operations like "2 + ptr" will use i32 as out type when
            //  it should be of pointer type
        }
        Assert(!expr->typeId.isString())
        // I don't think typeId can be a type string but assert to be sure.
        // if it's not then no need to worry about checking type which is done below
        // this would need to be done in generator too if isString is true
        // if(expr->typeId.isString()){
        //     bool printedError = false;
        //     auto ti = CheckType(info, scope->scopeId, expr->typeId, expr->tokenRange, &printedError);
        //     if (ti.isValid()) {
        //     } else if(!printedError){
        //         ERR_HEAD(expr->tokenRange, "Type "<< info.ast->getTokenFromTypeString(expr->typeId) << " does not exist.\n";
        //         )
        //     }
        //     // expr->typeId = ti;
        //     *outType = ti;
        // }
        if(expr->castType.isString()){
            bool printedError = false;
            auto ti = CheckType(info, scope->scopeId, expr->castType, expr->tokenRange, &printedError);
            if (ti.isValid()) {
            } else if(!printedError){
                ERR_HEAD(expr->tokenRange, "Type "<<info.ast->getTokenFromTypeString(expr->castType) << " does not exist.\n";
                )
            }
            // expr->castType = ti; // don't if polymorphic scope
            *outType = ti;
        }
        if(expr->typeId.getId() < AST_TRUE_PRIMITIVES){
            *outType = expr->typeId;
        }
    }
    if(expr->args && checkNext){
        for(auto now : *expr->args){
            TypeId typeId={};
            CheckExpression(info, scope, now, &typeId);
        }
    }
    // if(expr->next && checkNext) {
    // }
        
    return true;
}

int CheckRest(CheckInfo& info, ASTScope* scope);
// Evaluates types and offset for the given function implementation
// It does not modify ast func
int CheckFunctionImpl(CheckInfo& info, const ASTFunction* func, FuncImpl* funcImpl, ASTStruct* parentStruct, TypeId* outType){
    using namespace engone;
    MEASURE;
    _TC_LOG_ENTER(FUNC_ENTER)
    int offset = 0; // offset starts before call frame (fp, pc)
    int firstSize = 0;

    Assert(funcImpl->polyArgs.size() == func->polyArgs.size());
    for(int i=0;i<(int)funcImpl->polyArgs.size();i++){
        TypeId id = funcImpl->polyArgs[i];
        Assert(id.isValid())
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


    _TC_LOG(log::out << "FUNC IMPL "<< funcImpl->name<<"\n";)

    // TODO: parentStruct argument may not be necessary since this function only calculates
    //  offsets of arguments and return values.

    funcImpl->argumentTypes.resize(func->arguments.size());
    funcImpl->returnTypes.resize(func->returnTypes.size());

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
                ERR_HEAD(func->tokenRange, "Type '"<<msg<<"' is unknown. Did you misspell or forget to declare the type?\n";
                    ERR_LINE(func->arguments[i].name,msg.c_str());
                )
            } else if(ti.isValid()){

            } else if(!printedError) {
                std::string msg = info.ast->typeToString(arg.stringType);
                ERR_HEAD(func->tokenRange, "Type '"<<msg<<"' is unknown. Did you misspell it or is the compiler messing with you?\n";
                    ERR_LINE(func->arguments[i].name,msg.c_str());
                )
            }
            argImpl.typeId = ti;
        } else {
            argImpl.typeId = arg.stringType;
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
        offset += 8-diff; // padding to ensure 8-bit alignment

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
        auto& retStringType = func->returnTypes[i];
        // TypeInfo* typeInfo = 0;
        if(retStringType.isString()){
            bool printedError = false;
            auto ti = CheckType(info, func->scopeId, retStringType, func->tokenRange, &printedError);
            if(ti.isValid()){
            }else if(!printedError) {
                // TODO: fix location?
                ERR_HEAD2(func->tokenRange) << info.ast->getTokenFromTypeString(retStringType)<<" is not a type (function)\n";
                ERR_END
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

    return true;
}
// Ensures that the function identifier exists.
// Adds the overload for the function.
// Checks if an existing overload would collide with the new overload.
int CheckFunction(CheckInfo& info, ASTFunction* function, ASTStruct* nowStruct, ASTScope* scope){
    using namespace engone;
    _TC_LOG_ENTER(FUNC_ENTER)
    
    // Create virtual types
    for(int i=0;i<(int)function->polyArgs.size();i++){
        auto& arg = function->polyArgs[i];
        arg.virtualType = info.ast->createType(arg.name, function->scopeId);
        // _TC_LOG(log::out << "Virtual type["<<i<<"] "<<arg.name<<"\n";)
        arg.virtualType->id = AST_POLY;
    }
    defer {
        for(int i=0;i<(int)function->polyArgs.size();i++){
            auto& arg = function->polyArgs[i];
            arg.virtualType->id = {};
        }
    };
    // The code below is used to 
    // Acquire identifiable arguments
    DynamicArray<TypeId> argTypes{};
    for(int i=0;i<(int)function->arguments.size();i++){
        // if(function->arguments[i].defaultValue)
        //     break;
        TypeId typeId = CheckType(info, function->scopeId, function->arguments[i].stringType, function->tokenRange, nullptr);
        // Assert(typeId.isValid());
        if(!typeId.isValid()){
            std::string msg = info.ast->typeToString(function->arguments[i].stringType);
            ERR_HEAD(function->arguments[i].name.range(),"Unknown type '"<<msg<<"'.\n\n";
                ERR_LINE(function->arguments[i].name.range(),msg.c_str());
            )
        }
        argTypes.add(typeId);
    }
    // _TC_LOG(log::out << "Method/function has polymorphic properties: "<<function->name<<"\n";)
    if(nowStruct){
        Assert(("Methods don't work",false))
        // nowStruct->isPolymorphic() || function->isPolymorphic();
    }
    Identifier* iden = info.ast->getIdentifier(scope->scopeId, function->name);
    if(!iden){
        iden = info.ast->addIdentifier(scope->scopeId, function->name);
        iden->type = Identifier::FUNC;
    }
    if(iden->type != Identifier::FUNC){
        ERR_HEAD(function->tokenRange, "'"<< function->name << "' is already defined as a non-function.\n";
        )
        return false;
    }
    if(function->polyArgs.size()==0){
        FnOverloads::Overload* outOverload = nullptr;
        for(int i=0;i<(int)iden->funcOverloads.overloads.size();i++){
            auto& overload = iden->funcOverloads.overloads[i];
            // u32 nonDefaults0 = 0;
            // TODO: Store non defaults in Identifier or ASTStruct to save time.
            //   Recalculating non default arguments here every time you get a function is
            //   unnecessary.
            // for(auto& arg : overload.astFunc->arguments){
            //     if(arg.defaultValue)
            //         break;
            //     nonDefaults0++;
            // }
            // if(argTypes.size() > overload.astFunc->arguments.size() || argTypes.size() < nonDefaults0)
            //     continue;
            bool found = true;
            for (u32 j=0;j<overload.funcImpl->argumentTypes.size() && j < argTypes.size();j++){
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
        // return outOverload;
        // FnOverloads::Overload* overload = id->funcOverloads.getOverload(argTypes);
        if(outOverload){
            //  TODO: better error message which shows what and where the already defined variable/function is.
            ERR_HEAD(function->tokenRange, "Argument types are ambiguous with another overload of function '"<< function->name << "'.\n\n";
                ERR_LINE(outOverload->astFunc->tokenRange,"initial overload");
                ERR_LINE(function->tokenRange,"ambiguous overload");
            )
            // print list of overloads?
        } else {
            FuncImpl* funcImpl = function->createImpl();
            funcImpl->name = function->name;
            // function->_impls.add(funcImpl);
            iden->funcOverloads.addOverload(function, funcImpl);
            TypeId retType={};
            bool yes = CheckFunctionImpl(info, function, funcImpl, nowStruct, &retType);
            CheckInfo::CheckImpl checkImpl{};
            checkImpl.astFunc = function;
            checkImpl.funcImpl = funcImpl;
            // checkImpl.scope = scope;
            info.checkImpls.add(checkImpl);
        }
    }else{
        WARN_HEAD(function->tokenRange,"Polymorphic functions have not been implemented properly\n\n";)
        // Base poly overload is added without regard for ambiguity.
        // Checking for ambiguity is very difficult which is why we do this.
        iden->funcOverloads.addPolyOverload(function);
    }
    return true;
}
int CheckFunctions(CheckInfo& info, ASTScope* scope){
    using namespace engone;
    MEASURE;
    _TC_LOG_ENTER(FUNC_ENTER)
    Assert(scope)

    for(auto now : scope->namespaces) {
        CheckFunctions(info, now);
    }
    
    for(auto it : scope->functions){
        CheckFunction(info, it, nullptr, scope);
        int result = CheckFunctions(info, it->body);
    }
    for(auto it : scope->structs){
        for(auto fn : it->functions){
            CheckFunction(info, fn , it, scope);
            int result = CheckFunctions(info, fn->body);
        }
    }
    
    for(auto astate : scope->statements) {
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

int CheckFuncImplScope(CheckInfo& info, ASTFunction* func, FuncImpl* funcImpl){
    using namespace engone;
    if(func->body->nativeCode)
        return true;

    log::out << log::YELLOW << __FILE__<<":"<<__LINE__ << ": Fix methods\n";
    // Where does ASTStruct come from. Arguments? in FuncImpl?

    for(int i=0;i<(int)func->polyArgs.size();i++){
        auto& arg = func->polyArgs[i];
        arg.virtualType->id = funcImpl->polyArgs[i];
    }
    struct Var {
        ScopeId scopeId;
        std::string name;
    };
    // TODO: Add arguments as variables
    std::vector<Var> vars;
    _TC_LOG(log::out << "arg ";)
    for (int i=0;i<(int)func->arguments.size();i++) {
        auto& arg = func->arguments[i];
        auto& argImpl = funcImpl->argumentTypes[i];
        _TC_LOG(log::out << " " << arg.name<<": "<< info.ast->typeToString(argImpl.typeId) <<"\n";)
        auto varinfo = info.ast->addVariable(func->scopeId, std::string(arg.name));
        if(varinfo){
            varinfo->typeId = argImpl.typeId; // typeId comes from CheckExpression which may or may not evaluate
            // the same type as the generator.
            vars.push_back({func->scopeId,std::string(arg.name)});
        }   
    }
    _TC_LOG(log::out << "\n";)

    CheckRest(info, func->body);
    
    for(auto& e : vars){
        info.ast->removeIdentifier(e.scopeId, e.name);
    }
    for(int i=0;i<(int)func->polyArgs.size();i++){
        auto& arg = func->polyArgs[i];
        arg.virtualType->id = {};
    }
    return true;
}

int CheckRest(CheckInfo& info, ASTScope* scope){
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
    // for(auto it : scope->functions){
    //     for
    //     info.checkImpls.add({it, });
    //     CheckRest(info, it, scope);
    // }
    // for(auto now : scope->structs) {
    //     for(auto it : now->functions){
    //         CheckRest(info, it, scope);
    //     }
    // }

    struct A {
        ScopeId scopeId;
        std::string name;
    };
    std::vector<A> vars;
    for (auto now : scope->statements){
        TypeId typeId={};
        if(now->rvalue)
            CheckExpression(info, scope, now->rvalue, &typeId);
        
        for(auto& var : now->varnames){
            if(var.assignType.isString()){
                bool printedError = false;
                auto ti = CheckType(info, scope->scopeId, var.assignType, now->tokenRange, &printedError);
                // NOTE: We don't care whether it's a pointer just that the type exists.
                if (!ti.isValid() && !printedError) {
                    ERR_HEAD(now->tokenRange, "'"<<info.ast->getTokenFromTypeString(var.assignType)<<"' is not a type (statement).\n\n";
                        ERR_LINE(now->tokenRange,"bad");
                    )
                } else {
                    // If typeid is invalid we don't want to replace the invalid one with the type
                    // with the string. The generator won't see the names of the invalid types.
                    // now->typeId = ti;
                    var.assignType = ti;
                    log::out << log::YELLOW << __FILE__<<":"<<__LINE__<<" (COMPILER BUG): Cannot modify varnames of statements with polymorphism. It ruins everything!\n";
                }
            }
        }

        if(now->body && now->type != ASTStatement::FOR){
            int result = CheckRest(info, now->body);
        }
        if(now->elseBody){
            int result = CheckRest(info, now->elseBody);
        }
        if(now->type == ASTStatement::ASSIGN){
            // log::out << "ASSING\n";
            _TC_LOG(log::out << "assign ";)
            for (auto& var : now->varnames) {
                if(!var.assignType.isValid()) {
                    var.assignType = typeId;
                } 
                // else {
                if(var.assignType.isString()) {
                    // perhaps it should always be string? huh?
                    // log::out <<"YOU STRING WHY!?\n";
                }
                //     log::out << "ASSIGN WHAT? "<<info.ast->typeToString(var.assignType)<<", "<<info.ast->typeToString(typeId)<<" from rvalue\n";
                // }
                _TC_LOG(log::out << " " << var.name<<": "<< info.ast->typeToString(var.assignType) <<"\n";)
                auto varinfo = info.ast->addVariable(scope->scopeId, std::string(var.name));
                if(varinfo){
                    if(var.assignType.isValid()){
                        varinfo->typeId = var.assignType; // typeId comes from CheckExpression which may or may not evaluate
                    }
                    // the same type as the generator.
                    vars.push_back({scope->scopeId,std::string(var.name)});
                }
            }
            _TC_LOG(log::out << "\n";)
        } else if(now->type == ASTStatement::FOR){
            Assert(now->varnames.size()==1);
            auto& varname = now->varnames[0];

            ScopeId varScope = now->body->scopeId;

            // TODO: for loop variables
            auto iterinfo = info.ast->getTypeInfo(typeId);
            if(iterinfo&&iterinfo->astStruct){
                if(iterinfo->astStruct->name == "Slice"){
                    auto varinfo_index = info.ast->addVariable(varScope, "nr");
                    if(varinfo_index) {
                        varinfo_index->typeId = AST_INT32;
                    } else {
                        WARN_HEAD(now->tokenRange, "Cannot add 'nr' variable to use in for loop. Is it already defined?\n";)
                    }

                    // _TC_LOG(log::out << " " << var.name<<": "<< info.ast->typeToString(var.assignType) <<"\n";)
                    auto varinfo_item = info.ast->addVariable(varScope, std::string(varname.name));
                    if(varinfo_item){
                        auto memdata = iterinfo->getMember("ptr");
                        auto itemtype = memdata.typeId;
                        if(!now->pointer){
                            itemtype.setPointerLevel(itemtype.getPointerLevel()-1);
                        }
                        varinfo_item->typeId = itemtype;
                        varname.assignType = itemtype;
                    } else {
                        WARN_HEAD(now->tokenRange, "Cannot add "<<varname.name<<" variable to use in for loop. Is it already defined?\n";)
                    }

                    if(now->body){
                        int result = CheckRest(info, now->body);
                    }
                    info.ast->removeIdentifier(varScope, varname.name);
                    info.ast->removeIdentifier(varScope, "nr");
                    continue;
                } else if(iterinfo->astStruct->name == "Range"){
                    if(now->pointer){
                        WARN_HEAD(now->tokenRange, "@ppointer annotation does nothing with Range as for loop. It only works with slices.\n";)
                    }
                    now->rangedForLoop = true;
                    auto varinfo_index = info.ast->addVariable(varScope, "nr");
                    auto memdata = iterinfo->getMember(0); // begin, now or whatever name it has
                    TypeId inttype = memdata.typeId;
                    varname.assignType = inttype;
                    if(varinfo_index) {
                        varinfo_index->typeId = inttype;
                    } else {
                        WARN_HEAD(now->tokenRange, "Cannot add 'nr' variable to use in for loop. Is it already defined?\n";)
                    }

                    // _TC_LOG(log::out << " " << var.name<<": "<< info.ast->typeToString(var.assignType) <<"\n";)
                    auto varinfo_item = info.ast->addVariable(varScope, std::string(varname.name));
                    if(varinfo_item){
                        varinfo_item->typeId = inttype;
                    } else {
                        WARN_HEAD(now->tokenRange, "Cannot add "<<varname.name<<" variable to use in for loop. Is it already defined?\n";)
                    }

                    if(now->body){
                        int result = CheckRest(info, now->body);
                    }
                    info.ast->removeIdentifier(varScope, varname.name);
                    info.ast->removeIdentifier(varScope, "nr");
                    continue;
                }
            }
            std::string strtype = info.ast->typeToString(typeId);
            ERR_HEAD(now->rvalue->tokenRange, "The expression in for loop must be a Slice.\n\n";
                ERR_LINE(now->rvalue->tokenRange,strtype.c_str());
            )
            continue;
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
                    // @NO-MODIFY-AST ha... ha...  modifying the AST isn't possible
                    // with polymorphism since it will check scopes
                    // multiple times with different types. With modififying I mostly
                    // mean removing or adding nodes. Changing names, types is bad too.
                    Assert(("Broken code",false))
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

                    Assert(("Broken using keyword code",false))
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

                Assert(("Fix broken code for using keyword",false))
                
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
        }
    }
    for(auto& e : vars){
        info.ast->removeIdentifier(e.scopeId, e.name);
    }
    for(auto now : scope->namespaces){
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

    // while(info.checkImpls.size()!=0){
    //     auto checkImpl = info.checkImpls[info.checkImpls.size()-1];
    //     info.checkImpls.pop();

    //     CheckRest(info, checkImpl.astFunc, checkImpl.funcImpl);
    // }

    result = CheckRest(info, scope);

    while(info.checkImpls.size()!=0){
        auto checkImpl = info.checkImpls[info.checkImpls.size()-1];
        info.checkImpls.pop();

        CheckFuncImplScope(info, checkImpl.astFunc, checkImpl.funcImpl);
    }
    
    info.compileInfo->errors += info.errors;
    info.compileInfo->typeErrors += info.errors;
    return info.errors;
}