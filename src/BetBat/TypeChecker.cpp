#include "BetBat/TypeChecker.h"
#include "BetBat/Compiler.h"

/* IMPORTANT: QuickArray DO NOT REPLACE quick array with dynamic array!
    QuickArray is faster than DynamicArray, no destructor/constructors.
*/

// Old logging
#undef WARN_HEAD3
#define WARN_HEAD3(R,M) info.compileInfo->options->compileStats.warnings++; engone::log::out << WARN_DEFAULT_R(R,"Type warning","W0000") << M

#undef ERR_SECTION
#define ERR_SECTION(CONTENT) BASE_SECTION2(CONTENT)

#undef TEMP_ARRAY
#define TEMP_ARRAY(TYPE,NAME) QuickArray<TYPE> NAME; NAME.init(&scratch_allocator);
#define TEMP_ARRAY_N(TYPE,NAME, N) QuickArray<TYPE> NAME; NAME.init(&scratch_allocator); NAME.reserve(N);

// #define TEMP_ARRAY(TYPE,NAME) QuickArray<TYPE> NAME;

// #define _TCLOG_ENTER(...) FUNC_ENTER_IF(global_loggingSection & LOG_TYPECHECKER)
// #define _TCLOG_ENTER(...) _TCLOG(__VA_ARGS__)
#define _TCLOG_ENTER(X)

SignalIO TyperContext::checkEnums(ASTScope* scope){
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
            TypeId colonType = checkType(scope->scopeId, typeString, aenum->location, &printedError);
            if(!colonType.isValid()) {
                // if(!printedError) {
                    ERR_SECTION(
                        ERR_HEAD2(aenum->location)
                        ERR_MSG("The type for the enum after : was not valid.")
                        ERR_LINE2(aenum->location, typeString<< " was invalid")
                    )
                // }
            } else {
                aenum->colonType = colonType;
            }
        }
        TypeInfo* typeInfo = info.ast->createType(aenum->name, scope->scopeId);
        // log::out << "Enum " << aenum->name<<"\n";
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
        SignalIO result = checkEnums(it);
        if(result != SIGNAL_SUCCESS)
            error = SIGNAL_FAILURE;
    }
    
    for(auto astate : scope->statements) {
        if(astate->firstBody){
            SignalIO result = checkEnums(astate->firstBody);   
            if(result != SIGNAL_SUCCESS)
                error = SIGNAL_FAILURE;
        }
        if(astate->secondBody){
            SignalIO result = checkEnums(astate->secondBody);
            if(result != SIGNAL_SUCCESS)
                error = SIGNAL_FAILURE;
        }
    }
    for(auto afunc : scope->functions) {
        if(afunc->body){
            SignalIO result = checkEnums(afunc->body);
            if(result != SIGNAL_SUCCESS)
                error = SIGNAL_FAILURE;
        }
    }
    for(auto astStruct : scope->structs){
        for(auto fun : astStruct->functions){
            if(fun->body) {
                SignalIO result = checkEnums(fun->body);
                if(result != SIGNAL_SUCCESS)
                    error = SIGNAL_FAILURE;
            }
        }
    }
    return error;
}
SignalIO TyperContext::checkStructImpl(ASTStruct* astStruct, TypeInfo* structInfo, StructImpl* structImpl){
    using namespace engone;
    _TCLOG_ENTER(FUNC_ENTER)
    int offset=0;
    int alignedSize=1; // offset will be aligned to match this at the end

    Assert(astStruct->polyArgs.size() == structImpl->polyArgs.size());
    
    astStruct->pushPolyState(structImpl);
    defer{
        astStruct->popPolyState();
    };
   
    structImpl->members.resize(astStruct->members.size());

    SCOPED_ALLOCATOR_MOMENT(scratch_allocator)

    TEMP_ARRAY_N(TypeId, tempTypes, 5)
    
    bool success = true;
    _TCLOG(log::out << "Check struct impl '"<<info.ast->typeToString(structInfo->id)<<"'\n";)
    //-- Check members
    for (int i = 0;i<(int)astStruct->members.size();i++) {
        auto& member = astStruct->members[i];
        auto& implMem = structImpl->members[i];

        Assert(member.stringType.isString());
        bool printedError = false;
        TypeId tid = checkType(astStruct->scopeId, member.stringType, astStruct->location, &printedError);
        if(!tid.isValid() && !printedError){
            if(info.showErrors) {
                ERR_SECTION(
                    ERR_HEAD2(member.location)
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
            inferred_type = implMem.typeId;
            tempTypes.resize(0);
            checkExpression(structInfo->scopeId, member.defaultValue,&tempTypes, false);
            inferred_type = {};

            if(tempTypes.size()==0)
                tempTypes.add(AST_VOID);
            if(!info.ast->castable(implMem.typeId, tempTypes.last(), false)){
                std::string deftype = info.ast->typeToString(tempTypes.last());
                std::string memtype = info.ast->typeToString(implMem.typeId);
                ERR_SECTION(
                    ERR_HEAD2(member.defaultValue->location) // TODO: message should mark the whole expression, not just some random token/operation inside it
                    ERR_MSG("Type of default value does not match member.")
                    ERR_LINE2(member.defaultValue->location,deftype.c_str())
                    ERR_LINE2(member.location,memtype.c_str())
                )
                continue; // continue when failing
            }
        }
        _TCLOG(log::out << " checked member["<<i<<"] '"<<info.ast->typeToString(tid)<<"'\n";)
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
        }
        if(size==0 || asize == 0){
            if(info.showErrors) {
                ERR_SECTION(
                    ERR_HEAD2(member.location)
                    ERR_MSG("Type '"<< info.ast->typeToString(implMem.typeId) << "' in '"<< astStruct->name<<"."<<member.name << "' is not a type, an incomplete one, or is zero in size. If it's an incomplete one then structs may recursively depend on each other.")
                )
            }
            success = false;
            continue;
        }
        if(astStruct->no_padding) {
            asize = 1; // disable alignment
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
        if(member.array_length)
            offset += size * member.array_length;
        else
            offset += size;

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
TypeId TyperContext::checkType(ScopeId scopeId, TypeId typeString, lexer::SourceLocation err_location, bool* printedError){
    using namespace engone;
    Assert(typeString.isString());
    // if(!typeString.isString()) {
    //     _TCLOG(log::out << "check type typeid wasn't string type\n";)
    //     return typeString;
    // }
    auto view = info.ast->getStringFromTypeString(typeString);
    return checkType(scopeId, view, err_location, printedError);
}
TypeId TyperContext::checkType(ScopeId scopeId, StringView typeString, lexer::SourceLocation err_location, bool* printedError){
    using namespace engone;

    /*
        Evaluate poly types
    */

    if(typeString == "int") {
        ERR_SECTION(
            ERR_HEAD2(err_location)
            ERR_MSG("Type '"<<typeString<<"' does not exist. Did you mean i32.")
            ERR_LINE2(err_location,"here somewhere")
        )
        return {};
    } 

    ZoneScopedC(tracy::Color::Purple4);
    _TCLOG_ENTER(FUNC_ENTER)
    // _TCLOG(log::out << "check "<<typeString<<"\n";)

    SCOPED_ALLOCATOR_MOMENT(scratch_allocator)

    // TODO: Log what type we thought it was
    TypeId typeId = {};
    u32 plevel=0;
    TEMP_ARRAY(StringView, polyTokens)
    StringView baseType{};
    std::string realTypeName{};
    TEMP_ARRAY(TypeId, polyArgs)

    std::string tmp = typeString;
    if (tmp.substr(0,3) == "fn(" || tmp.substr(0,3) == "fn@") {
        // This code is better
        // TODO: Polymorphic functions don't work, not tested at least.
        
        TEMP_ARRAY(TypeId, args);
        TEMP_ARRAY(TypeId, rets);
        CallConvention convention = BETCALL;
        
        int head = 2;
        int str_start = 0;
        int depth = 0;
        bool process_args = true;
        bool process_anot = false;
        while(head < tmp.size()) {
            char chr = tmp[head];
            char next_chr = 0;
            if (head+1 < tmp.size())
                next_chr = tmp[head+1];
            head++;
            
            if(depth == 0) {
                if(chr == '-' && next_chr == '>') {
                    head++;
                    process_args = false;
                    continue;
                }
            }
            if(chr == '@' && depth == 0) {
                process_anot = true;
                continue;
            }
            
            if(chr == '(') {
                depth++;
                if(depth == 1) {
                    if(process_anot) {
                        process_anot = false;
                        int str_end = head-1;
                        std::string inner_type = std::string(tmp.data() + str_start, str_end - str_start);
                        // log::out << "anot "<<inner_type<<"\n";
                        if(inner_type == "stdcall")
                            convention = STDCALL;
                        else if(inner_type == "unixcall")
                            convention = UNIXCALL;
                        else if(inner_type == "betcall")
                            convention = BETCALL;
                        else if(inner_type == "oscall") {
                            if(info.compiler->options->target == TARGET_WINDOWS_x64) {
                                convention = STDCALL;
                            } else if(info.compiler->options->target == TARGET_WINDOWS_x64) {
                                convention = UNIXCALL;
                            }
                        } else {
                            // do we just ignore?
                        }
                        str_start = 0;
                    }
                    continue;
                }
            }
            if(chr == ')') {
                depth--;
            }
            if(chr == '<') { // we use the same depth variable for parenthesis and arrows, we should detect if the user is mixing them and print errors
                depth++;
            }
            if(chr == '>') {
                depth--;
            }
            
            if(( depth == 1 && chr == ',') || ( depth == 0 && chr == ')') || (head == tmp.size() && !process_args)) {
                if(str_start == 0) {
                    continue;
                }
                int str_end = head-1;
                if(head == tmp.size() && !process_args)
                    str_end = head;
                std::string inner_type = std::string(tmp.data() + str_start, str_end - str_start);
                bool printedError = false;
                auto type = checkType(scopeId, inner_type, err_location, &printedError); // transformVirtual is false because we don't handle it correctly
                
                // type may not be converted if polymorphic version wasn't created?
                if(!type.isValid()) {
                    if (!printedError) {
                        ERR_SECTION(
                            ERR_HEAD2(err_location)
                            ERR_MSG("Type '"<<inner_type<<"' for polymorphic argument was not valid.")
                            ERR_LINE2(err_location,"here somewhere")
                        )
                    }
                    // return {};
                    // Even though type is invalid, we don't have to return here.
                    // We can continue evaluating types in case we find more invalid ones.
                    // Also, it could be bad to change the number of arguments since it could have
                    // cascading errors.
                }

                if(process_args) {
                    args.add(type);
                } else {
                    rets.add(type);
                }
                str_start = 0;
                continue;
            }
            if(str_start == 0)
                str_start = head-1;
        }
        
        auto type = info.ast->findOrAddFunctionSignature(args, rets, convention);
        
        if(!type)
            return AST_VOID;
        return type->id;
    } else {
        // TODO: namespace?
        StringView typeName{};
        AST::DecomposePointer(typeString, &typeName, &plevel);
        AST::DecomposePolyTypes(typeName, &baseType, &polyTokens);
        // Token typeName = AST::TrimNamespace(noPointers, &ns);

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
                TypeId id = checkType(scopeId, polyTokens[i], err_location, &printedError);
                if(i!=0)
                    realTypeName += ",";
                realTypeName += info.ast->typeToString(id);
                polyArgs.add(id);
                if(id.isValid()){

                } else if(!printedError) {
                //     // ERR_SECTION(
                // ERR_HEAD2(err_location, "Type '"<<info.ast->typeToString(id)<<"' for polymorphic argument was not valid.\n\n";
                    if(polyTokens[i] == "int") {
                        ERR_SECTION(
                            ERR_HEAD2(err_location)
                            ERR_MSG("Type '"<<polyTokens[i]<<"' does not exist. Use i32.")
                            ERR_LINE2(err_location,"here somewhere")
                        )
                    } else {
                        ERR_SECTION(
                            ERR_HEAD2(err_location)
                            ERR_MSG("Type '"<<polyTokens[i]<<"' for polymorphic argument was not valid.")
                            ERR_LINE2(err_location,"here somewhere")
                        )
                    }
                }
                // baseInfo->astStruct->polyArgs[i].virtualType->id = polyInfo->id;
            }
            realTypeName += ">";
            typeId = info.ast->convertToTypeId(realTypeName, scopeId, true);
        } else {
            typeId = info.ast->convertToTypeId(typeName, scopeId, true);
        }
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
        if(ti->getSize() == 0) {
            if(ti->astStruct) {
                // Struct needs to be evaluated again
                if(!ti->structImpl->is_evaluating) {
                    // TODO: Not thread safe, mutex on is_evaluating
                    ti->structImpl->is_evaluating = true;
                    SignalIO hm = checkStructImpl(ti->astStruct, ti, ti->structImpl);
                    ti->structImpl->is_evaluating = false;
                }
            }
        }
        typeId.setPointerLevel(typeId.getPointerLevel() + plevel);
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
    if(!baseInfo->astStruct) {
        ERR_SECTION(
            ERR_HEAD2(err_location)
            ERR_MSG("Polymorphic type '"<<typeString<<"' is not a struct and cannot be polymorphic.")
            ERR_LINE2(err_location, "here")
        )
        return {};
    }
    if(polyTokens.size() != baseInfo->astStruct->polyArgs.size()) {
        ERR_SECTION(
            ERR_HEAD2(err_location)
            ERR_MSG("Polymorphic type "<<typeString << " has "<< (u32)polyTokens.size() <<" poly. args but the base type "<<info.ast->typeToString(baseInfo->id)<<" needs "<<(u32)baseInfo->astStruct->polyArgs.size()<<".")
            ERR_LINE2(err_location,"here somewhere")
        )
        // ERR() << "Polymorphic type "<<typeString << " has "<< polyTokens.size() <<" poly. args but the base type "<<info.ast->typeToString(baseInfo->id)<<" needs "<<baseInfo->astStruct->polyArgs.size()<< "\n";
        if(printedError)
            *printedError = true;
        return {}; // type isn't polymorphic and does just not exist
    }

    TypeInfo* typeInfo = info.ast->createType(realTypeName, baseInfo->scopeId);
    typeInfo->astStruct = baseInfo->astStruct;
    typeInfo->structImpl = info.ast->createStructImpl(typeInfo->id);
    // typeInfo->structImpl = baseInfo->astStruct->createImpl();

    typeInfo->structImpl->polyArgs.resize(polyArgs.size());
    for(int i=0;i<(int)polyArgs.size();i++){
        TypeId id = polyArgs[i];
        typeInfo->structImpl->polyArgs[i] = id;
    }

    SignalIO hm = checkStructImpl(typeInfo->astStruct, baseInfo, typeInfo->structImpl);
    // log::out << "create structimpl " << realTypeName<<", size: "<<typeInfo->structImpl->size<<"\n";

    if(hm != SIGNAL_SUCCESS) {
        ERR_SECTION(
            ERR_HEAD2(err_location)
            ERR_MSG(__FUNCTION__ <<": structImpl for type "<<typeString << " failed.")
        )
    } else {
        _TCLOG(log::out <<"'"<< typeString << "' was evaluated to "<<typeInfo->structImpl->size<<" bytes\n";)
    }
    TypeId outId = typeInfo->id;
    outId.setPointerLevel(plevel);
    return outId;
}
SignalIO TyperContext::checkStructs(ASTScope* scope) {
    using namespace engone;
    ZoneScopedC(tracy::Color::X11Purple);
    _TCLOG_ENTER(FUNC_ENTER)
    //-- complete structs

    for(auto it : scope->namespaces) {
        checkStructs(it);
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
                astStruct->base_typeId = structInfo->id;

                _TCLOG(log::out << log::LIME << "struct '"<<info.ast->typeToString(structInfo->id)<<"'"<< log::NO_COLOR <<" in scope "<<scope->scopeId<<"\n";)
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
                    structInfo->structImpl = info.ast->createStructImpl(structInfo->id);
                    astStruct->nonPolyStruct = structInfo->structImpl;
                }
                yes = checkStructImpl(astStruct, structInfo, structInfo->structImpl) == SIGNAL_SUCCESS;
                if(!yes){
                    astStruct->state = ASTStruct::TYPE_CREATED;
                    info.incompleteStruct = true;
                } else {
                    // _TCLOG(log::out << astStruct->name << " was evaluated to "<<astStruct->baseImpl.size<<" bytes\n";)
                }
            }
            if(yes){
                astStruct->state = ASTStruct::TYPE_COMPLETE;
                info.completedStruct = true;
            }
        }
    }

    // checks struct in methods after checking the upper structs in case
    // lower structs refer to upper structs.
    for(auto astStruct : scope->structs){
        for(auto fun : astStruct->functions){
            if(fun->body) {
                SignalIO result = checkStructs(fun->body);
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
                SignalIO result = checkStructs(astate->firstBody);   
                // if(!result)
                //     error = false;
            }
            if(astate->secondBody){
                SignalIO result = checkStructs(astate->secondBody);
                // if(!result)
                //     error = false;
            }
        }
        
        for(auto afunc : scope->functions) {
            if(afunc->body){
                SignalIO result = checkStructs(afunc->body);
                // if(!result)
                //     error = false;
            }
        }
    // }
    return SIGNAL_SUCCESS;
}
SignalIO TyperContext::checkFncall(ScopeId scopeId, ASTExpression* expr, QuickArray<TypeId>* outTypes, bool attempt, bool operatorOverloadAttempt, QuickArray<TypeId>* operatorArgs) {
    using namespace engone;

    TRACE_FUNC()

    CALLBACK_ON_ASSERT(
        ERR_LINE2(expr->location, "crash why?")
    )

    Assert(!outTypes || outTypes->size()==0);
    #define FNCALL_SUCCESS \
        if(outTypes) {\
            for(auto& ret : overload->funcImpl->signature.returnTypes)\
                outTypes->add(ret.typeId); \
            if(overload->funcImpl->signature.returnTypes.size()==0)\
                outTypes->add(AST_VOID);\
        }\
        expr->versions_overload[info.currentPolyVersion] = *overload;
    #define FNCALL_FAIL \
        if(outTypes) outTypes->add(AST_VOID); \
        FIX_NO_SPECIAL_ACTIONS

    #define FIX_NO_SPECIAL_ACTIONS for (auto& ent : possible_overload_groups) { ent.iden = nullptr; ent.set_implicit_this = false; }

    SCOPED_ALLOCATOR_MOMENT(scratch_allocator)

    // fail should perhaps not output void as return type.

    //-- Get poly args from the function call
    TEMP_ARRAY(TypeId, fnPolyArgs);
    TEMP_ARRAY(StringView, polyTypes);

    // Token baseName{};
    StringView baseName{};
    
    if(operatorOverloadAttempt) {
        baseName = OP_NAME((OperationType)expr->typeId.getId());
    } else {
        // baseName = AST::TrimPolyTypes(expr->name, &polyTokens);
        AST::DecomposePolyTypes(expr->name, &baseName, &polyTypes);
        for(int i=0;i<(int)polyTypes.size();i++){
            bool printedError = false;
            TypeId id = checkType(scopeId, polyTypes[i], expr->location, &printedError);
            fnPolyArgs.add(id);
            // TODO: What about void?
            if(id.isValid()){

            } else if(!printedError) {
                ERR_SECTION(
                    ERR_HEAD2(expr->location) // TODO: mark all tokens in the expression,location is just one token
                    ERR_MSG("Type for polymorphic argument was not valid.")
                    ERR_LINE2(expr->location,"bad")
                )
            }
        }
    }

    //-- Get essential arguments from the function call

    // DynamicArray<TypeId>& argTypes = expr->versions_argTypes[info.currentPolyVersion];
    // DynamicArray<TypeId> argTypes{};
    TEMP_ARRAY_N(TypeId, argTypes, 3);
    TEMP_ARRAY_N(TypeId, tempTypes, 5);
    // QuickArray<TypeId> tempTypes{};

    TEMP_ARRAY_N(bool, inferred_args, 5); // Inferred arguments are assumed to be initializers for structs. Variable should maybe be renamed to initializer arguments.
    struct Entry {
        OverloadGroup* fn_overloads = nullptr;
        StructImpl* parentStructImpl = nullptr;
        ASTStruct* parentAstStruct = nullptr;

        // special actions
        bool set_implicit_this = false;
        Identifier* iden = nullptr;
    };
    TEMP_ARRAY(Entry, possible_overload_groups)
    defer {
        // these asserts helps preventing mistakes
        // when finding function names we need to do certain things like setting
        // identifier or implicit this in the expression
        // we don't want to forget that so we specify the special actions we have to do if
        // the overload matches in the entry and then reset them we did so.
        // that way, we will assert if we forget to handle the special action
        for(auto& ent : possible_overload_groups) {
            Assert(!ent.set_implicit_this);
            Assert(!ent.iden);
        }
    };

    if(operatorOverloadAttempt){
        Assert(operatorArgs);
        if(operatorArgs->size() != 2) {
            return SIGNAL_FAILURE;
        }
        argTypes.resize(operatorArgs->size());
        memcpy(argTypes.data(), operatorArgs->data(), operatorArgs->size() * sizeof(TypeId)); // TODO: Use TintArray::copyFrom() instead (function not implemented yet)
        inferred_args.resize(2);
        
        // NOTE: We pour out types into tempTypes instead of argTypes because
        //   we want to separate how many types we get from each expression.
        //   If an expression gives us more than one type then we throw away the other ones.
        //   We might throw an error instead haven't decided yet.
        // SignalIO resultLeft = checkExpression(scopeId,expr->left,&tempTypes, true);
        // if(tempTypes.size()==0){
        //     argTypes.add(AST_VOID);
        // } else {
        //     argTypes.add(tempTypes[0]);
        // }
        // tempTypes.resize(0);
        // SignalIO resultRight = checkExpression(scopeId,expr->right,&tempTypes, true);
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
        // BREAK(expr->args.size() == 4 && expr->args[0]->name == "memory_tracker")
        for(int i = 0; i<(int)expr->args.size();i++){
            auto argExpr = expr->args.get(i);
            Assert(argExpr);

            tempTypes.resize(0);
            SignalIO signal = SIGNAL_SUCCESS;
            if(argExpr->is_inferred_initializer()) {
                // do not check inferred initializer because the type comes from
                // function argument. Since we don't know the function to call,
                // we can't check the expression yet.
                // We still add something though.
                argTypes.add({});
                inferred_args.add(true);
                continue;
            } else
                signal = checkExpression(scopeId,argExpr,&tempTypes, false);
            // log::out << "signal " << signal<<"\n";
            Assert(tempTypes.size()==1); // should be void at least
            if(expr->isMemberCall() && i==0){
                /* You can do this
                    l: List;
                    p := &l;
                    list.add(1) // here, the type of "this" is not a pointer so we implicitly reference the variable
                    p.add(2) // here, the type is a pointer so everything is fine
                */
                if(tempTypes[0].getPointerLevel()>1){
                    ERR_SECTION(
                        ERR_HEAD2(expr->args[i]->location)
                        ERR_MSG("Methods only work on types of single pointer type. Double pointer is not okay. Non-pointers are okay with variables or expressions that are evaluated to \"references\" (basically pointers).")
                        ERR_LINE2(expr->args[i]->location, "here")
                    )
                    thisFailed=true; // We do not allow calling function from type 'List**'
                } else {
                    tempTypes[0].setPointerLevel(1); // type is either 'List' or 'List*' both are fine
                }
            }
            // log::out << "Yes "<< info.ast->typeToString(tempTypes[0])<<"\n";
            argTypes.add(tempTypes[0]);
            inferred_args.add(false);
        }
        // cannot continue if we don't know which struct the method comes from
        if(thisFailed) {
            FNCALL_FAIL
            return SIGNAL_FAILURE;
        }
    }

    TypeId this_type{};

    //-- Get identifier, the namespace of overloads for the function/method.
    
    if(expr->isMemberCall()){
        // Assert(expr->args->size()>0);
        // ASTExpression* thisArg = expr->args->get(0);
        Assert(expr->args.size()>0);
        ASTExpression* thisArg = expr->args.get(0);
        TypeId structType = argTypes[0];
        this_type = structType;
        // checkExpression(scope, thisArg, &structType);
        // if(structType.getPointerLevel()>1){
        //     ERR_SECTION(
        //         ERR_HEAD2(thisArg->location) // TODO: badly marked token
        //         ERR_MSG("'"<<info.ast->typeToString(structType)<<"' to much of a pointer.")
        //         ERR_LINE2(thisArg->location,"must be a reference to a struct")
        //     )
        //     FNCALL_FAIL
        //     return SIGNAL_FAILURE;
        // }
        TypeInfo* ti = info.ast->getTypeInfo(structType.baseType());
        
        if(!ti || !ti->astStruct){
            ERR_SECTION(
                ERR_HEAD2(expr->location)// TODO: badly marked token
                ERR_MSG("'"<<info.ast->typeToString(structType)<<"' is not a struct. Method cannot be called.")
                ERR_LINE2(thisArg->location,"not a struct")
            )
            FNCALL_FAIL
            return SIGNAL_FAILURE;
        }
        if(!ti->getImpl()){
            ERR_SECTION(
                ERR_HEAD2(expr->location) // TODO: badly marked token
                ERR_MSG("'"<<info.ast->typeToString(structType)<<"' is a polymorphic struct with not poly args specified.")
                ERR_LINE2(thisArg->location,"base polymorphic struct")
            )
            FNCALL_FAIL
            return SIGNAL_FAILURE;
        }
        auto fn_overloads = ti->astStruct->getMethod(baseName);
        if (fn_overloads)
            possible_overload_groups.add({ fn_overloads, ti->structImpl, ti->astStruct });

        auto mem = ti->getMember(baseName);
        if(mem.index != -1) {
            auto type = mem.typeId;
            
            auto type_info = info.ast->getTypeInfo(type);
            if(!type_info->funcType) {
                if (possible_overload_groups.size())
                    goto member_did_not_match;

                if(operatorOverloadAttempt || attempt) {
                    FIX_NO_SPECIAL_ACTIONS
                    return SIGNAL_NO_MATCH;
                }

                ERR_SECTION(
                    ERR_HEAD2(expr->location)
                    ERR_MSG("The identifier '"<<baseName <<"' is a variable but not a function pointer. You can only \"call\" variables if they are a function pointer.")
                    ERR_LINE2(expr->location, info.ast->typeToString(type))
                )
                FNCALL_FAIL
                return SIGNAL_FAILURE;
            }
            
            if(expr->nonNamedArgs != argTypes.size()) {
                if (possible_overload_groups.size())
                    goto member_did_not_match;

                if(operatorOverloadAttempt || attempt) {
                    FIX_NO_SPECIAL_ACTIONS
                    return SIGNAL_NO_MATCH;
                }

                ERR_SECTION(
                    ERR_HEAD2(expr->location)
                    ERR_MSG("Named arguments is not possible when calling a function pointer that does not have parameter names.")
                    ERR_LINE2(expr->location, info.ast->typeToString(type))
                )
                FNCALL_FAIL
                return SIGNAL_FAILURE;
            }
            
            auto f = type_info->funcType;
            
            bool matched = true;

            // first type is 'this' but this code should check
            // members that are function pointers and doesn't have 'this'
            argTypes.removeAt(0);

            if(f->argumentTypes.size() != argTypes.size()) {
                matched = false;   
            } else {
                for(int i=0;i<f->argumentTypes.size();i++) {
                    if (inferred_args[i]) {
                        continue;
                    }
                    if(!info.ast->castable(argTypes[i], f->argumentTypes[i].typeId, false)) {
                        matched = false;
                        break;
                    }
                }
            }
            
            if(!matched) {
                if (possible_overload_groups.size())
                    goto member_did_not_match;

                if(operatorOverloadAttempt || attempt) {
                    FIX_NO_SPECIAL_ACTIONS
                    return SIGNAL_NO_MATCH;
                }

                ERR_SECTION(
                    ERR_HEAD2(expr->location)
                    ERR_MSG("Args don't match with function pointer.")
                    ERR_LINE2(expr->location, info.ast->typeToString(type))
                )
                FNCALL_FAIL
                return SIGNAL_FAILURE;
            }

            for(int i=0;i<expr->args.size();i++) {
                auto argExpr = expr->args[i];
                if (inferred_args[i]) {
                    Assert(argExpr->namedValue.size() == 0); // Fix named args later, i need to know if this will work first.
                    inferred_type = f->argumentTypes[i].typeId;
                    auto signal = checkExpression(scopeId,argExpr,&tempTypes, false);
                    inferred_type = {};
                }
             }
            
            if(outTypes) {
                if(f->returnTypes.size()==0)
                    outTypes->add(AST_VOID);
                else
                    for(auto& ret : f->returnTypes)
                        outTypes->add(ret.typeId);
            }
            expr->versions_func_type.set(info.currentPolyVersion, f);
            return SIGNAL_SUCCESS;
        } else {
            if (possible_overload_groups.size())
                goto member_did_not_match;

            if(operatorOverloadAttempt || attempt) {
                FIX_NO_SPECIAL_ACTIONS
                return SIGNAL_NO_MATCH;
            }

            ERR_SECTION(
                ERR_HEAD2(expr->location) // TODO: badly marked token
                ERR_MSG("Method '"<<baseName <<"' does not exist. Did you mispell the name?")
                ERR_LINE2(expr->location,"undefined")
            )
            FNCALL_FAIL
            return SIGNAL_NO_MATCH;
        }

    member_did_not_match:
        ;
    } else {
        // special functions
        if(expr->name == "destruct" || expr->name == "construct") {

             if(expr->args.size() != 1){
                ERR_SECTION(
                    ERR_HEAD2(expr->location)
                    ERR_MSG("'"<<expr->name<<"' takes one argument, not "<<expr->args.size()<<".")
                    ERR_LINE2(expr->location, "here")
                )
                return SIGNAL_FAILURE;
            }

            TypeId arg_type = argTypes[0];

            bool is_destruct = expr->name == "destruct";

            if(expr->name == "construct" || expr->name == "destruct") {
                if(arg_type.getPointerLevel() == 0) {
                    if(!is_destruct) {
                        ERR_SECTION(
                            ERR_HEAD2(expr->location)
                            ERR_MSG("'"<<expr->name<<"' expects a pointer to a \"data object\" where initialization should occur.")
                            ERR_LINE2(expr->location, "here")
                        )
                    } else {
                        ERR_SECTION(
                            ERR_HEAD2(expr->location)
                            ERR_MSG("'"<<expr->name<<"' expects a pointer to a \"data object\" where destruction should occur.")
                            ERR_LINE2(expr->location, "here")
                        )
                    }
                    return SIGNAL_FAILURE;
                }

                arg_type.setPointerLevel(arg_type.getPointerLevel()-1);

                TypeInfo* typeinfo = ast->getTypeInfo(arg_type.baseType());
                Assert(typeinfo); // shouldn't be possible

                if(arg_type.getPointerLevel() == 0 && typeinfo->astStruct) {

                    OverloadGroup::Overload* overload = nullptr;
                    OverloadGroup* overloads = nullptr;
                    if(!is_destruct)
                        overloads = typeinfo->astStruct->getMethod("init");
                    else
                        overloads = typeinfo->astStruct->getMethod("cleanup");

                    if (overloads) {
                        for(auto& o : overloads->overloads) {
                            // TODO: Should we allow return types? If it's an integer or something we
                            //   just throw it away but if it's a struct with an allocation we kind of
                            //   have to deal with it properly.
                            
                            if(o.funcImpl->signature.returnTypes.size() != 0)
                                continue;

                            // First argument 'this' should not be default
                            bool all_defaults = true;
                            for(int i=1;i<o.funcImpl->signature.argumentTypes.size();i++) {
                                auto& arg = o.astFunc->arguments[i];
                                if(!arg.defaultValue) {
                                    all_defaults = false;
                                    break;
                                }
                            }
                            if(all_defaults) {
                                overload = &o;
                                break;
                            }
                        }
                        if(!overload) {
                            // Check polymorphic functions
                            for(auto& o : overloads->polyImplOverloads) {
                                // TODO: Should we allow return types? If it's an integer or something we
                                //   just throw it away but if it's a struct with an allocation we kind of
                                //   have to deal with it properly.
                                
                                if(o.funcImpl->signature.returnTypes.size() != 0)
                                    continue;

                                // First argument 'this' should not be default
                                bool all_defaults = true;
                                for(int i=1;i<o.funcImpl->signature.argumentTypes.size();i++) {
                                    auto& arg = o.astFunc->arguments[i];
                                    if(!arg.defaultValue) {
                                        all_defaults = false;
                                        break;
                                    }
                                }
                                if(all_defaults) {
                                    overload = &o;
                                    break;
                                }
                            }
                        }
                        if(!overload) {
                            for(auto& o : overloads->polyOverloads) {
                                // TODO: Should we allow return types? If it's an integer or something we
                                //   just throw it away but if it's a struct with an allocation we kind of
                                //   have to deal with it properly.
                                
                                if(o.astFunc->returnValues.size() != 0)
                                    continue;
                                if(o.astFunc->polyArgs.size() != 0)
                                    continue;

                                // First argument is 'this' and should not have a default value
                                bool all_defaults = true;
                                for(int i=1;i<o.astFunc->arguments.size();i++) {
                                    auto& arg = o.astFunc->arguments[i];
                                    if(!arg.defaultValue) {
                                        all_defaults = false;
                                        break;
                                    }
                                }
                                if(all_defaults) {
                                    auto polyFunc = o.astFunc;
                                    auto parentStructImpl = typeinfo->structImpl;
                                    auto parentAstStruct = typeinfo->astStruct;

                                    ScopeInfo* funcScope = info.ast->getScope(polyFunc->scopeId);
                                    FuncImpl* funcImpl = info.ast->createFuncImpl(polyFunc);
                                    funcImpl->structImpl = parentStructImpl;

                                    // We don't match with polymorphic function, we do match polymorphic struct
                                    // funcImpl->signature.polyArgs.resize(fnPolyArgs.size());
                                    // for(int i=0;i<(int)fnPolyArgs.size();i++){
                                    //     TypeId id = fnPolyArgs[i];
                                    //     funcImpl->signature.polyArgs[i] = id;
                                    // }

                                    // IMPORTANT TODO: checkFunctionImpl with multi-threading will break stuff! We need to check 'is_being_checked'.
                                    // TODO: What you are calling a struct method?  if (expr->boolvalue) do structmethod
                                    SignalIO result = checkFunctionImpl(polyFunc,funcImpl,parentAstStruct, nullptr, parentStructImpl);

                                    overload = ast->addPolyImplOverload(overloads, polyFunc, funcImpl);
                                    
                                    info.compiler->addTask_type_body(polyFunc, funcImpl);

                                    funcImpl->usages++;

                                    break;
                                }
                            }
                            
                        }
                    }
                    if(!overload) {
                        
                    } else {
                        ast->declareUsageOfOverload(overload);
                        expr->versions_overload[currentPolyVersion] = *overload;
                    }
                } else {
                    
                }
            }

            if(outTypes)
                outTypes->add(AST_VOID);
            return SIGNAL_SUCCESS;
        }

        if(info.currentAstFunc && info.currentAstFunc->parentStruct) {
            // TODO: Check if function name exists in parent struct
            //   If so, an implicit this argument should be taken into account.
            auto fn_overloads = info.currentAstFunc->parentStruct->getMethod(baseName);
            if(fn_overloads){
                possible_overload_groups.add({});
                possible_overload_groups.last().fn_overloads = fn_overloads;
                possible_overload_groups.last().parentStructImpl = info.currentFuncImpl->structImpl;
                possible_overload_groups.last().parentAstStruct = info.currentAstFunc->parentStruct;
                possible_overload_groups.last().set_implicit_this = true;
                // expr->setImplicitThis(true);
            }
        }

        // We add method as potential overload but we also need to find identifier since method might not match.   
        // Identifier* iden = info.ast->findIdentifier(scopeId, info.getCurrentOrder(), baseName, nullptr);

        TEMP_ARRAY(Identifier*, identifiers);
        info.ast->findIdentifiers(scopeId, info.getCurrentOrder(), baseName, identifiers, nullptr);

        // if(iden) {
        for (auto iden : identifiers) {
            // if(!iden) {
            //     if(operatorOverloadAttempt || attempt)
            //         return SIGNAL_NO_MATCH;

            //     ERR_SECTION(
            //         ERR_HEAD2(expr->location)
            //         ERR_MSG("Function '"<<baseName <<"' does not exist. Did you forget an import?")
            //         ERR_LINE2(expr->location,"undefined")
            //     )
            //     FNCALL_FAIL
            //     return SIGNAL_FAILURE;
            // }
            expr->identifier = iden;
            if(iden->type == Identifier::FUNCTION) {
                possible_overload_groups.add({});
                possible_overload_groups.last().fn_overloads = &iden->cast_fn()->funcOverloads;
                possible_overload_groups.last().iden = iden;
            } else if(iden->is_var()){
                auto var = expr->identifier->cast_var();
                // auto var = info.ast->getVariableByIdentifier(iden);
                auto type = var->versions_typeId[info.currentPolyVersion];
                
                TypeInfo* type_info = nullptr;
                if (type.isNormalType())
                    type_info = info.ast->getTypeInfo(type);
                if(!type_info || !type_info->funcType) {
                    if(operatorOverloadAttempt || attempt) {
                        FIX_NO_SPECIAL_ACTIONS
                        return SIGNAL_NO_MATCH;
                    }

                    ERR_SECTION(
                        ERR_HEAD2(expr->location)
                        ERR_MSG("The identifier '"<<baseName <<"' is a variable but not a function pointer. You can only \"call\" variables if they are a function pointer.")
                        ERR_LINE2(expr->location, info.ast->typeToString(type))
                    )
                    FNCALL_FAIL
                    return SIGNAL_FAILURE;
                }
                
                if(expr->nonNamedArgs != argTypes.size()) {
                    if(operatorOverloadAttempt || attempt) {
                        FIX_NO_SPECIAL_ACTIONS
                        return SIGNAL_NO_MATCH;
                    }

                    ERR_SECTION(
                        ERR_HEAD2(expr->location)
                        ERR_MSG("Named arguments is not possible when calling a function pointer that does not have parameter names.")
                        ERR_LINE2(expr->location, info.ast->typeToString(type))
                    )
                    FNCALL_FAIL
                    return SIGNAL_FAILURE;
                }
                
                auto f = type_info->funcType;
                
                bool matched = true;
                if(f->argumentTypes.size() != argTypes.size()) {
                    matched = false;   
                } else {
                    for(int i=0;i<f->argumentTypes.size();i++) {
                        if(inferred_args[i])
                            continue;
                        if(!info.ast->castable(argTypes[i], f->argumentTypes[i].typeId, false)) {
                            matched = false;
                            break;
                        }
                    }
                }
                
                if(!matched) {
                    if(operatorOverloadAttempt || attempt) {
                        FIX_NO_SPECIAL_ACTIONS
                        return SIGNAL_NO_MATCH;
                    }

                    ERR_SECTION(
                        ERR_HEAD2(expr->location)
                        ERR_MSG("Args don't match with function pointer.")
                        ERR_LINE2(expr->location, info.ast->typeToString(type))
                    )
                    FNCALL_FAIL
                    return SIGNAL_FAILURE;
                }

                for(int i=0;i<expr->args.size();i++) {
                    auto argExpr = expr->args[i];
                    if (inferred_args[i]) {
                        Assert(argExpr->namedValue.size() == 0); // Fix named args later, i need to know if this will work first.
                        // nocheckin TODO: argTypes index is displaced if methods or implicit this.
                        argTypes[i] = f->argumentTypes[i].typeId;
                        inferred_type = argTypes[i];
                        auto signal = checkExpression(scopeId,argExpr,&tempTypes, false);
                        inferred_type = {};
                    }
                }
                
                if(outTypes) {
                    if(f->returnTypes.size()==0)
                        outTypes->add(AST_VOID);
                    else
                        for(auto& ret : f->returnTypes)
                            outTypes->add(ret.typeId);
                }
                expr->versions_func_type.set(currentPolyVersion, f);
                return SIGNAL_SUCCESS;
            }
        }
    }

    #define FIX_SPECIAL_ACTIONS \
        if(ent.set_implicit_this) {                     \
            expr->setImplicitThis(true);                \
            ent.set_implicit_this = false;              \
        }                                               \
        if(ent.iden) {                                  \
            expr->identifier = ent.iden;                \
            ent.iden = nullptr;                         \
        }                                               \
        for (auto& ent2 : possible_overload_groups) {   \
            if (&ent == &ent2)                          \
                continue;                               \
            ent2.iden = nullptr;                        \
            ent2.set_implicit_this = false;             \
        } 
        // log::out << "WA "<<expr->name<<"\n";          \
        // for (auto& ent2 : possible_overload_groups) {   \
        //     log::out << " ent "<< (void*)ent2.iden << "\n";                      \
        // } \
        // log::out.flush();

    // IMPORTANT TODO: There is a bug with polymorphism and overloading. A function call in
    //   a polymoprhic function may match with a method with certain arguments
    //   but the same polymorphic function may match with a normal function
    //   with different types as arguments. If so, then we would need to
    //   set implicit this based on poly version.

    bool implicitPoly = false; // implicit poly does not affect parentStruct
    for (auto& ent : possible_overload_groups) {
        // if(fnPolyArgs.size()==0){
        auto parentAstStruct = ent.parentAstStruct;
        auto fnOverloads = ent.fn_overloads;
        if(fnPolyArgs.size()==0 && (!parentAstStruct || parentAstStruct->polyArgs.size()==0)){
            // match args with normal impls
            
            OverloadGroup::Overload* overload = ast->getOverload(fnOverloads, scopeId, argTypes, ent.set_implicit_this, expr, fnOverloads->overloads.size()==1, &inferred_args);
            if(!overload)
                overload = ast->getOverload(fnOverloads, scopeId, argTypes, ent.set_implicit_this, expr, true, &inferred_args);


            if(operatorOverloadAttempt && !overload) {
                // FIX_NO_SPECIAL_ACTIONS
                // return SIGNAL_NO_MATCH;
                continue; // current overload group doesn't match, perhaps another one will or perhaps a polymorphic one exists. either way don't return here.
            }
            if(overload){
                info.ast->declareUsageOfOverload(overload);

                // BREAK(overload->astFunc->name == "create_snapshot")

                // Check inferred expressions, initializers
                if(!operatorOverloadAttempt) {
                    Assert(argTypes.size() == expr->args.size());

                    for(int i=0; i<argTypes.size();i++) {
                        auto exprArg = expr->args[i];
                        if(!inferred_args[i])
                            continue;
                        // nocheckin TODO: Won't work with methods, implicit argument.
                        argTypes[i] = overload->funcImpl->signature.argumentTypes[i].typeId;
                        inferred_type = argTypes[i];
                        SignalIO result = checkExpression(scopeId, exprArg, &tempTypes, false);
                        inferred_type = {};
                    }
                }
                // Check default values of function
                for(int i=argTypes.size() + (ent.set_implicit_this?1:0); i<overload->astFunc->arguments.size();i++) {

                // for(int i=argTypes.size(); i<overload->astFunc->arguments.size();i++) {
                    auto& argImpl = overload->funcImpl->signature.argumentTypes[i];
                    auto& arg = overload->astFunc->arguments[i];
                    if(!arg.defaultValue)
                        continue;
                    tempTypes.resize(0);
                    inferred_type = argImpl.typeId;
                    SignalIO result = checkExpression(scopeId, arg.defaultValue,&tempTypes,false);
                    inferred_type = {};
                    if(tempTypes.size()==0)
                        tempTypes.add(AST_VOID);

                    bool is_castable = info.ast->castable(tempTypes.last(),argImpl.typeId, false);
                    // if(!is_castable){
                    //     // Not castable with normal conversion
                    //     // Check hard conversions
                    //     is_castable = info.ast->findCastOperator(scopeId, tempTypes.last(), argImpl.typeId);
                    // }

                    if(!is_castable){
                    // if(temp.last() != argImpl.typeId){
                        // std::string deftype = info.ast->typeToString(info.temp_defaultArgs.last());
                        std::string deftype = info.ast->typeToString(tempTypes.last());
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

                FIX_SPECIAL_ACTIONS

                FNCALL_SUCCESS

                // log::out << "WA2 "<<expr->name<<"\n";
                // for (auto& ent2 : possible_overload_groups) {
                //     log::out << " ent "<< (void*)ent2.iden << "\n";
                // }
                // log::out.flush();

                return SIGNAL_SUCCESS;
            }
        }
    }

    // match args and poly args with poly impls
    // if match then return that impl
    // if not then try to generate a implementation
    
    // log::out << "Poly overloads ("<<possible_overload_groups.size()<<"):\n";
    // ERR_LINE2(expr->location,"here");

    for(auto& ent : possible_overload_groups) {
        auto fnOverloads = ent.fn_overloads;

        if(fnOverloads->polyOverloads.size() == 0)
            continue; // overloads are not polymorphic, don't check them

        auto parentStructImpl = ent.parentStructImpl;
        auto parentAstStruct = ent.parentAstStruct;
        bool implicitPoly = (fnPolyArgs.size()==0 && (!parentAstStruct || parentAstStruct->polyArgs.size()==0));
        // TODO: Optimize by checking what in the overloads didn't match. If all parent structs are a bad match then
        //  we don't have we don't need to getOverload the second time with canCast=true
        OverloadGroup::Overload* overload = ast->getOverload(fnOverloads, argTypes, fnPolyArgs, parentStructImpl, ent.set_implicit_this, expr, implicitPoly, &inferred_args);
        if(overload){
            overload->funcImpl->usages++;
            
            FIX_SPECIAL_ACTIONS

            FNCALL_SUCCESS
            return SIGNAL_SUCCESS;
        }
        // bool useCanCast = false;
        overload = ast->getOverload(fnOverloads, argTypes, fnPolyArgs, parentStructImpl, ent.set_implicit_this, expr, implicitPoly, true, &inferred_args);
        if(overload){
            overload->funcImpl->usages++;

            FIX_SPECIAL_ACTIONS

            FNCALL_SUCCESS
            return SIGNAL_SUCCESS;
        }

        ASTFunction* polyFunc = nullptr;
        if(implicitPoly) {
            // log::out << "Poly overloads ("<<ent.iden->name<<"):\n";
            // ERR_LINE2(expr->location,"here");

            // IMPORTANT: Parent structs may not be handled properly.
            // Assert(!parentStructImpl);

            int lessArguments = 0;
            // if(ent.set_implicit_this || expr->isMemberCall()) 
            if(ent.set_implicit_this)
                lessArguments = 1;
            
            // What needs to be done compared to other code paths is that fnPolyArgs is empty and needs to be decided.
            // This can get complicated fn size<T>(a: Slice<T*>)
            DynamicArray<TypeId> choosenTypes{};
            DynamicArray<TypeId> realChoosenTypes{};
            OverloadGroup::PolyOverload* polyOverload = nullptr;
            for(int i=0;i<(int)fnOverloads->polyOverloads.size();i++){
                OverloadGroup::PolyOverload& overload = fnOverloads->polyOverloads[i];

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

                choosenTypes.resize(0); // clear types from previous check that didn't work out

                // IMPORTANT TODO: NAMESPACES ARE IGNORED AT THE MOMENT. HANDLE THEM!
                // TODO: Fix this code it does not work properly. Some behaviour is missing.
                bool found = true;
                for (u32 j=0;j<expr->nonNamedArgs;j++){
                    if(inferred_args[j]) {
                        ERR_SECTION(
                            ERR_HEAD2(expr->args[i]->location)
                            ERR_MSG("Initializers is not allowed with polymorphic functions.")
                            ERR_LINE2(expr->args[i]->location, "here")
                        )
                        continue;
                    }
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
                    // TODO: We need to decompose function pointers too.
                    AST::DecomposePointer(typeString, &typeName, &plevel);
                    AST::DecomposePolyTypes(typeName, &baseToken, &polyTypes);
                    // namespace?

                    TypeInfo* typeInfo = info.ast->convertToTypeInfo(baseToken, argScope, false);
                    TypeId baseType = typeInfo->id;
                    // TypeInfo* typeInfo = info.ast->getTypeInfo(baseType);
                    // if(typeInfo->id != baseType) {
                    bool is_base_virtual = false;
                    bool is_poly_virtual = false;
                    for(int k = 0;k<overload.astFunc->polyArgs.size();k++){
                        if(overload.astFunc->polyArgs[k].virtualType->originalId == typeInfo->originalId) {
                            is_base_virtual = true;
                            break;
                        }
                        for (int pi=0;pi<polyTypes.size();pi++) {
                            auto ptype = polyTypes[pi];
                            
                            // WARNING, we assume no pointer
                            TypeInfo* typeInfo = info.ast->convertToTypeInfo(ptype, argScope, false);
                            // TypeId baseType = typeInfo->id;

                            if(overload.astFunc->polyArgs[k].virtualType->originalId == typeInfo->originalId) {
                                is_poly_virtual = true;
                                break;
                            }
                        }
                    }
                    // TODO: This code only matches certain types. Complicated ones like T<K,T<K,V>> is not handled (T<K,V> is handled though)

                    if(!is_base_virtual && !is_poly_virtual) {
                        // no virtual/polymorphic type
                        TypeId real_type = info.ast->convertToTypeId(typeString, scopeId, true);
                        // TODO: We cast if we're not dealing with polymorphic types. We should however try to match overload without casting first, and if it fails, try match with casting.
                        //   Perhaps we should cast when some types are polmorphic too?
                        if(ast->castable(typeToMatch, real_type)){
                            continue;
                        }
                        found = false;
                        break;
                    } else if (is_base_virtual && polyTypes.size() == 0) {
                        // Basic type matching with T, not anything complicated like T<K,V>

                        int typeIndex = -1;
                        // get base of typetomatch
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
                        // Assert(is_base_virtual && is_poly_virtual);
                        // log::out << "Check "<<expr->name<< "\n";
                        // log::out << " match " << ast->typeToString(typeToMatch)<<"\n";
                        // log::out << " arg " << typeName<<"\n";

                        // Matching polymorphic parameter and argument is a little complex.
                        // A paramater may be of type T<K,V> while argument is Map<String,i32>
                        // The polymorphic argument T,K,V should then be matched with Map, String and i32

                        // We need to match it with the argument type of the caller
                        // first we check if base type matches (T)
                        // Then the individual ones

                        // NOTE: I am fairly certain there are bugs here and scenarios that aren't handled.

                        TypeInfo* match_typeInfo = info.ast->getTypeInfo(typeToMatch.baseType());
                        if(!match_typeInfo->astStruct) {
                            found = false;
                            break;
                        }
                        TypeId match_baseType = match_typeInfo->astStruct->base_typeId;

                        if(is_base_virtual) {
                            // polymorphic type! we must decide!
                            // check if type exists in choosen types
                            // if so then reuse, if not then use up a time
                            int typeIndex = -1;
                            // get base of typetomatch
                            for(int k=0;k<choosenTypes.size();k++){
                                // NOTE: The choosen poly type must match in full.
                                if(choosenTypes[k].baseType() == match_baseType.baseType() &&
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
                                if(typeToMatch.getPointerLevel() < plevel) {
                                    found = false;
                                    break;
                                }
                                TypeId newChoosen = match_baseType;
                                newChoosen.setPointerLevel(typeToMatch.getPointerLevel() - plevel);
                                choosenTypes.add(newChoosen);

                                // size<T,K>(T* <K>*)
                                // size(u32**)
                            }
                        } else {
                            TypeId real_type = info.ast->convertToTypeId(baseToken, scopeId, true);
                            
                            if(real_type != match_baseType || plevel != typeToMatch.getPointerLevel()){
                                found = false;
                                break;
                            }
                        }

                        if(is_poly_virtual){
                            // This code should allow implicit calculation of incomplete polymorphic types.
                            // This sould be allowed: fn add<T>(a: T<i32>).
                            // But it's not so we get this: fn add<T>(a: T), no polymorphic types
                            // Also this: fn add<T>(a: Array<T>).
                            // Also this: fn add<T>(a: T<T>).

                            if(!match_typeInfo->structImpl || match_typeInfo->structImpl->polyArgs.size() != polyTypes.size()) {
                                found = false;
                                break;
                            }

                            for(int pti = 0; pti < polyTypes.size(); pti++){
                                auto typeName = polyTypes[pti];
                                TypeId arg_type = info.ast->convertToTypeId(typeName, argScope, false);
                                TypeInfo* typeInfo = info.ast->getTypeInfo(arg_type.baseType());

                                auto polyarg = match_typeInfo->structImpl->polyArgs[pti];

                                bool is_poly = false;
                                for(int k = 0;k<overload.astFunc->polyArgs.size();k++){
                                    if(overload.astFunc->polyArgs[k].virtualType->originalId == typeInfo->originalId) {
                                        is_poly = true;
                                        break;
                                    }
                                }
                                if (!is_poly) {
                                    if (arg_type != polyarg) {
                                        found = false;
                                        break;
                                    }
                                    continue; // type match, check next poly argument
                                }

                                // auto polytype = typeInfo->id;

                                int typeIndex = -1;
                                for(int k=0;k<choosenTypes.size();k++){
                                    // NOTE: The choosen poly type must match in full.
                                    if(choosenTypes[k].baseType() == polyarg.baseType() &&
                                        choosenTypes[k].getPointerLevel() == polyarg.getPointerLevel()
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
                                    TypeId newChoosen = polyarg;
                                    if(polyarg.getPointerLevel() < plevel) {
                                        found = false;
                                        break;
                                    }
                                    newChoosen.setPointerLevel(polyarg.getPointerLevel() - plevel);
                                    choosenTypes.add(newChoosen);
                                }
                            }
                        } else {
                            for(int pti = 0; pti < polyTypes.size(); pti++){
                                auto typeName = polyTypes[pti];
                                TypeId arg_type = info.ast->convertToTypeId(typeName, argScope, false);
                                TypeInfo* typeInfo = info.ast->getTypeInfo(arg_type.baseType());

                                auto polyarg = match_typeInfo->structImpl->polyArgs[pti];

                                if(arg_type == polyarg)
                                    continue;
                                
                                found = false;
                                break;
                            }
                        }
                    }
                }
                // Now we have gone through all arguments, found indicates whether all types matched
                // and whether we are good.
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
                    realChoosenTypes.steal_from(choosenTypes);
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
                
                polyOverload->astFunc->pushPolyState(&fnPolyArgs, parentStructImpl);
                defer {
                    polyOverload->astFunc->popPolyState();
                };
                bool found = true;
                int extra = ent.set_implicit_this ? 1 : 0;
                for (u32 j=0;j<expr->nonNamedArgs;j++){
                    // IMPORTANT: We also run CheckType in case types needs to be created.
                    // log::out << "Arg:"<<info.ast->typeToString(overload.astFunc->arguments[j].stringType)<<"\n";
                    TypeId argType = checkType(polyOverload->astFunc->scopeId,polyOverload->astFunc->arguments[j + extra].stringType,
                        polyOverload->astFunc->arguments[j + extra].location,nullptr);
                    // TypeId argType = checkType(scope->scopeId,overload.astFunc->arguments[j].stringType,
                    // log::out << "Arg: "<<info.ast->typeToString(argType)<<" = "<<info.ast->typeToString(argTypes[j])<<"\n";
                    if(!ast->castable(argTypes[j], argType)){
                    // if(argType != argTypes[j]){
                        found = false;
                        break;
                    }
                }
                if(!found) {
                    if (operatorOverloadAttempt) {
                        ERR_SECTION(
                            ERR_HEAD2(expr->location)
                            ERR_MSG("COMPILER BUG, when matching operator overloads. Polymorphic overload was generated which didn't match the actual arguments. It's also possible that we shouldn't have generated one to begin with.")
                            if (expr->left && argTypes.size() > 0) {
                                ERR_LINE2(expr->left->location, ast->typeToString(argTypes[0]))
                            }
                            if (expr->right && argTypes.size() > 1) {
                                ERR_LINE2(expr->right->location,ast->typeToString(argTypes[1]))
                            }
                        )
                    } else {
                        ERR_SECTION(
                            ERR_HEAD2(expr->location)
                            ERR_MSG("COMPILER BUG, when matching function overloads. The matching generated a polymorphic overload that was meant to match the arguments but which doesn't.")
                            
                            for (int i=0;argTypes.size();i++) {
                                if (expr->args.size() > i)
                                ERR_LINE2(expr->args[i]->location, ast->typeToString(argTypes[i]))
                            }
                        )
                    }
                    Assert(found); // If function we thought would match doesn't then the code is incorrect.
                }
            }
        } else {
            // IMPORTANT: Poly parent structs may not be handled properly!
            // Assert(!parentStructImpl);

            int lessArguments = 0;
            if(ent.set_implicit_this)
                lessArguments = 1;
            // // Find possible polymorphic type and later create implementation for it
            for(int i=0;i<(int)fnOverloads->polyOverloads.size();i++){
                OverloadGroup::PolyOverload& overload = fnOverloads->polyOverloads[i];
                if(overload.astFunc->polyArgs.size() != fnPolyArgs.size())
                    continue;
                // continue if more args than possible
                // continue if less args than minimally required
                if(expr->nonNamedArgs > overload.astFunc->arguments.size() - lessArguments || 
                    expr->nonNamedArgs < overload.astFunc->nonDefaults - lessArguments ||
                    argTypes.size() > overload.astFunc->arguments.size() - lessArguments
                    )
                    continue;

                overload.astFunc->pushPolyState(&fnPolyArgs, parentStructImpl);
                defer {
                    overload.astFunc->popPolyState();
                };
                bool found = true;
                for (u32 j=0;j<expr->nonNamedArgs;j++){
                    if(expr->isMemberCall() && j == 0)
                        continue; // skip first argument because they will be the same.

                    // log::out << "Arg:"<<info.ast->typeToString(overload.astFunc->arguments[j].stringType)<<"\n";
                    lexer::SourceLocation loc = overload.astFunc->arguments[j+lessArguments].location;
                    if(loc.tok.type == lexer::TOKEN_NONE) {
                        // bad token
                        loc = expr->location;
                    }
                    TypeId argType = checkType(overload.astFunc->scopeId,overload.astFunc->arguments[j+lessArguments].stringType,
                        loc,nullptr);
                    // TypeId argType = checkType(scope->scopeId,overload.astFunc->arguments[j].stringType,
                    // log::out << "Arg: "<<info.ast->typeToString(argType)<<" = "<<info.ast->typeToString(argTypes[j])<<"\n";
                    if(!info.ast->castable(argTypes[j], argType, false)){
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
        // TODO: We should not quit early as we are doing here.
        if(!polyFunc){
            continue;
            // if(operatorOverloadAttempt || attempt) {
            //     FIX_NO_SPECIAL_ACTIONS
            //     return SIGNAL_NO_MATCH;
            // }

            // ERR_SECTION(
            //     ERR_HEAD2(expr->location, ERROR_OVERLOAD_MISMATCH)
            //     ERR_MSG_LOG(
            //         "Overloads for function '"<< log::LIME<< baseName << log::NO_COLOR <<"' does not match these argument(s): ";
            //         if(argTypes.size()==0){
            //             log::out << "zero arguments";
            //         } else {
            //             log::out << "(";
            //             expr->printArgTypes(info.ast, argTypes);
            //             log::out << ")";
            //         }
                    
            //         log::out << "\n";
            //         log::out << log::YELLOW<<"(polymorphic functions could not be generated if there are any)\nTODO: Show list of available functions.\n";
            //         log::out << "\n"
            //     )
            //     ERR_LINE2(expr->location, "bad");
            //     // TODO: show list of available overloaded function args
            // )
            // FNCALL_FAIL
            // return SIGNAL_FAILURE;
            // goto error_on_fncall;
        }
        ScopeInfo* funcScope = info.ast->getScope(polyFunc->scopeId);
        FuncImpl* funcImpl = info.ast->createFuncImpl(polyFunc);
        funcImpl->structImpl = parentStructImpl;

        funcImpl->signature.polyArgs.resize(fnPolyArgs.size());

        for(int i=0;i<(int)fnPolyArgs.size();i++){
            TypeId id = fnPolyArgs[i];
            funcImpl->signature.polyArgs[i] = id;
        }

        // TODO: THIS IS TEMPORARY CODE!
        WHILE_TRUE_N(1000) {
            // In processImports we use 'lock_imports' mutex when checking is_being_checked.
            // This would mean that all threads must use the same mutex 'lock_imports' when checking 'is_being_checked', HOWEVER, we are not in a different phase and no thread will check is_being_checked with a different mutex except for the code right here. SOOO, we can use whichever mutex we'd like.
            compiler->lock_imports.lock(); // TODO: use a different mutex named more appropriately
            bool is_being_checked = polyFunc->is_being_checked || (
                (!currentAstFunc || currentAstFunc->parentStruct != parentAstStruct) &&  // if the function we check with checkFunctionImpl is a method and the parent struct of the method is the same struct as the current parent struct THEN we don't mind that the parent struct is being checked because this thread we are currently doing that.
                parentAstStruct && parentAstStruct->is_being_checked);
            if(!is_being_checked) {
                polyFunc->is_being_checked = true;
                if(parentAstStruct)
                    parentAstStruct->is_being_checked = true;
                compiler->lock_imports.unlock();
                break;
            }
            compiler->lock_imports.unlock();

            engone::Sleep(0.001); // TODO: Don't sleep, try generating another function instead. Move responsibility to compiler instead of generator, currently the generator goes through all functions and generates stuff, perhaps the compiler loop should instead. We can add "generate function" tasks.
        }

        // TODO: What you are calling a struct method?  if (expr->boolvalue) do structmethod
        SignalIO result = checkFunctionImpl(polyFunc,funcImpl,parentAstStruct, nullptr, parentStructImpl);
        // outTypes->used = 0; // FNCALL_SUCCESS will fix the types later, we don't want to add them twice

        compiler->lock_imports.lock();
        polyFunc->is_being_checked = false;
        if(parentAstStruct)
            parentAstStruct->is_being_checked = false;
        compiler->lock_imports.unlock();


        OverloadGroup::Overload* newOverload = ast->addPolyImplOverload(fnOverloads, polyFunc, funcImpl);
        
        // TypeChecker::CheckImpl checkImpl{};
        // checkImpl.astFunc = polyFunc;
        // checkImpl.funcImpl = funcImpl;
        // info.typeChecker->checkImpls.add(checkImpl);

        info.compiler->addTask_type_body(polyFunc, funcImpl);

        funcImpl->usages++;

        // Can overload be null since we generate a new func impl?
        overload = ast->getOverload(fnOverloads, argTypes, fnPolyArgs, parentStructImpl, ent.set_implicit_this, expr);
        if(!overload)
            overload = ast->getOverload(fnOverloads, argTypes, fnPolyArgs, parentStructImpl, ent.set_implicit_this, expr, false, true);
        Assert(overload == newOverload);
        if(!overload){
            ERR_SECTION(
                ERR_HEAD2(expr->location, ERROR_OVERLOAD_MISMATCH)
                ERR_MSG_LOG("Specified polymorphic arguments does not match with passed arguments for call to '"<<baseName <<"'.\n";
                    log::out << log::CYAN<<"Generates args: "<<log::NO_COLOR;
                    if(argTypes.size()==0){
                        log::out << "zero arguments";
                    }
                    for(int i=0;i<(int)funcImpl->signature.argumentTypes.size();i++){
                        if(i!=0) log::out << ", ";
                        log::out <<info.ast->typeToString(funcImpl->signature.argumentTypes[i].typeId);
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

        FIX_SPECIAL_ACTIONS

        FNCALL_SUCCESS
        return SIGNAL_SUCCESS;
    }

    // error_on_fncall:

    if(operatorOverloadAttempt || attempt) {
        FIX_NO_SPECIAL_ACTIONS
        return SIGNAL_NO_MATCH;
    }

    if(possible_overload_groups.size() == 0) {
        ERR_SECTION(
            ERR_HEAD2(expr->location, ERROR_OVERLOAD_MISMATCH)
            ERR_MSG_LOG("There is no function called '" << log::LIME << baseName << log::NO_COLOR <<"' in the scope.\n\n");
            ERR_LINE2(expr->location, "bad");
        )

        FNCALL_FAIL
        return SIGNAL_FAILURE;
    }

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
        // TODO: What if there are no possible overloads?
        int amount_of_overloads = 0;
        for(auto& ent : possible_overload_groups) {
            auto fnOverloads = ent.fn_overloads;
            for (int i = 0; i < fnOverloads->overloads.size();i++) {
                auto& overload = fnOverloads->overloads[i];
                amount_of_overloads++;
                log::out << "(";
                for(int j=0;j<overload.funcImpl->signature.argumentTypes.size();j++){
                    auto& argType = overload.funcImpl->signature.argumentTypes[j];
                    if(j!=0)
                        log::out << ", ";
                    log::out << log::LIME << info.ast->typeToString(argType.typeId) << log::NO_COLOR;
                }
                log::out << "), ";
                // log::out << ")\n";
            }
            for (int i = 0; i < fnOverloads->polyOverloads.size();i++) {
                auto& overload = fnOverloads->polyOverloads[i];
                amount_of_overloads++;
                log::out << "(";
                for(int j=0;j<overload.astFunc->arguments.size();j++){
                    auto& arg = overload.astFunc->arguments[j];
                    if(j!=0)
                        log::out << ", ";
                    log::out << log::LIME << info.ast->typeToString(arg.stringType) << log::NO_COLOR;
                }
                log::out << "), ";
            }
           
            if(fnOverloads->overloads.size() || fnOverloads->polyOverloads.size())
                log::out << "\n";

            // if(fnOverloads->polyOverloads.size()!=0){
            //     log::out << log::YELLOW<<"(implicit call to polymorphic function is not implemented)\n";
            // }
            // if(expr->args.size()!=0 && expr->args.get(0)->namedValue.ptr){
            //     log::out << log::YELLOW<<"(named arguments cannot identify overloads)\n";
            // }
        }
        if(amount_of_overloads == 0) {
            log::out << "nothing";
        }
        log::out <<"\n";
    )

    FNCALL_FAIL
    return SIGNAL_FAILURE;
}
SignalIO TyperContext::checkExpression(ScopeId scopeId, ASTExpression* expr, QuickArray<TypeId>* outTypes, bool attempt, int* array_length){
    using namespace engone;
    ZoneScopedC(tracy::Color::Purple);
    Assert(expr);
    _TCLOG_ENTER(FUNC_ENTER)
    
    defer {
        if(outTypes && outTypes->size()==0){
            outTypes->add(AST_VOID);
        }
    };

    TRACE_FUNC()

    SINGLE_CALLBACK_ON_ASSERT(
        ERR_SECTION(
            ERR_HEAD2(expr->location)
            ERR_MSG("Compiler bug!")
            ERR_LINE2(expr->location,"cause")
        )
    )

    SCOPED_ALLOCATOR_MOMENT(scratch_allocator)

    // IMPORTANT NOTE: CheckExpression HAS to run for left and right expressions
    //  since they may require types to be checked. AST_SIZEOF needs to evalute for example

    TEMP_ARRAY_N(TypeId, tempTypes, 5);
    TEMP_ARRAY_N(TypeId, operatorArgs, 2);

    // switch
    if(expr->isValue) {
        if(expr->typeId == AST_ID){
            // NOTE: When changing this code, don't forget to do the same with AST_SIZEOF. It also has code for AST_ID.
            //   Some time later...
            //   Oh, nice comment! Good job me. - Emarioo, 2024-02-03
            
            // ScopeInfo* sc = info.ast->getScope(scopeId);
            // sc->print(info.ast);
            // TODO: What about enum?
            bool crossed_function_boundary = false;
            // BREAK(expr->name == "argc")
            auto _iden = info.ast->findIdentifier(scopeId, info.getCurrentOrder(), expr->name, &crossed_function_boundary);
            if(_iden){
                expr->identifier = _iden;
                if(_iden->is_var()){
                    auto iden = _iden->cast_var();

                    if (iden->type != Identifier::GLOBAL_VARIABLE && crossed_function_boundary) {
                        /* NOTE:
                            crossed_function_boundary tells us that the variable we found is
                            within a parent scope outside of the current function. We should not
                            be able to access such variables, hence the error. See example below.

                            fn outer() {
                                n := 8
                                fn inner() {
                                    n // should not be accessible
                                }
                            }
                        */
                        ERR_SECTION(
                            ERR_HEAD2(expr->location, ERROR_UNDECLARED)
                            ERR_MSG("'"<<expr->name<<"' is not a declared variable.")
                            ERR_LINE2(expr->location,"undeclared")
                        )

                        if(outTypes) outTypes->add(AST_VOID);
                        return SIGNAL_FAILURE;
                    }
                    
                    if(iden->type == Identifier::MEMBER_VARIABLE) {
                        auto& mem = currentAstFunc->parentStruct->members[iden->memberIndex];
                        if (mem.array_length) {
                            TypeId type = iden->versions_typeId[info.currentPolyVersion];

                            Assert(type.getPointerLevel() < 3);
                            type.setPointerLevel(type.getPointerLevel() + 1);
                            if(array_length)
                                *array_length = mem.array_length;

                            // std::string real_type = "Slice<"+ast->typeToString(mem.stringType)+">";
                            // bool printed = false;
                            // TypeId type = checkType(scopeId, real_type, expr->location, &printed);
                            
                            outTypes->add(type);
                            return SIGNAL_SUCCESS;
                        }
                    }
                    
                    if(outTypes) {
                        // NOTE: NASTY FUTURE BUG! Accessing a global at the
                        //   import scope should use 0 as poly version
                        //   BUT when writing code you may copy some code
                        //   and forget this and assume that currentPolyVersion
                        //   is correct which it isn't, currentPolyVersion
                        //   specifies the version for the current polymorphic
                        //   scope that is evaluated, the global variable doesn't
                        //   have a polymorphic scope and should therefore not
                        //   use a poly version. This would get crazier if we
                        //   allow polymorphic namespace...
                        //   Ohh goodness me, imagine the madness we would have to deal with.
                        if(iden->is_import_global) {
                            outTypes->add(iden->versions_typeId[0]);
                        } else
                            outTypes->add(iden->versions_typeId[info.currentPolyVersion]);
                    }
                } else if(_iden->is_fn()) {
                    auto iden = _iden->cast_fn();
                    if(iden->funcOverloads.overloads.size() == 1) {
                        auto overload = &iden->funcOverloads.overloads[0];
                        
                        if (overload->astFunc->callConvention == INTRINSIC) {
                            ERR_SECTION(
                                ERR_HEAD2(expr->location)
                                ERR_MSG("You cannot take a pointer to an intrinsic function.")
                                ERR_LINE2(expr->location, "here")
                                ERR_LINE2(overload->astFunc->location, "this function")
                            )
                            if(outTypes) outTypes->add(AST_VOID);
                            return SIGNAL_FAILURE;
                        }
                        if (overload->astFunc->linkConvention != LinkConvention::NONE) {
                            ERR_SECTION(
                                ERR_HEAD2(expr->location)
                                ERR_MSG("You cannot take a pointer to an external/imported function (yet).")
                                ERR_LINE2(expr->location, "here")
                                ERR_LINE2(overload->astFunc->location, "this function")
                            )
                            if(outTypes) outTypes->add(AST_VOID);
                            return SIGNAL_FAILURE;
                        }
                        
                        info.ast->declareUsageOfOverload(overload);
                        
                        // TODO: Don't create new arrays, restructure funcImpl so that you can just
                        //   pass arrays when finding function type
                        // DynamicArray<TypeId> args{};
                        // DynamicArray<TypeId> rets{};
                        // auto f = overload->funcImpl;
                        // args.reserve(f->signature.argumentTypes.size());
                        // rets.reserve(f->signature.returnTypes.size());
                        // for(auto& t : f->signature.argumentTypes)
                        //     args.add(t.typeId);
                        // for(auto& t : f->signature.returnTypes)
                        //     rets.add(t.typeId);
                        auto type = info.ast->findOrAddFunctionSignature(&overload->funcImpl->signature);
                        if(!type) {
                            Assert(("can this crash?",false));
                        } else {
                            if(outTypes)
                                outTypes->add(type->id);
                        }
                    } else {
                        ERR_SECTION(
                            ERR_HEAD2(expr->location)
                            ERR_MSG("Function is overloaded. Taking a reference would therefore be ambiguous. You must rename the function so that it only has one overload.")
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
                bool found = info.ast->findEnumMember(scopeId, expr->name, &astEnum, &memberIndex);
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
            return checkFncall(scopeId,expr, outTypes, attempt, false);
        } else if(expr->typeId == AST_STRING){
            u32 index=0;
            
            auto constString = info.ast->getConstString(expr->name,&index);
            // Assert(constString);
            // log::out << " "<<expr->name << " "<<index<<"\n";
            expr->versions_constStrIndex.set(currentPolyVersion, index);

            if(expr->flags & ASTNode::NULL_TERMINATED) {
                TypeId theType = checkType(scopeId, "char*", expr->location, nullptr);
                Assert(theType.isValid()); if(outTypes) outTypes->add(theType);
            } else {
                TypeId theType = checkType(scopeId, "Slice<char>", expr->location, nullptr);
                Assert(theType.isValid());
                if(outTypes) outTypes->add(theType);
            } 
        } else if(expr->typeId == AST_SIZEOF || expr->typeId == AST_NAMEOF || expr->typeId == AST_TYPEID) {
            TypeId finalType = {};
            if(expr->left) {
                if(expr->left->typeId == AST_ID){
                    // AST_ID could result in a type or a variable
                    auto& name = expr->left->name;
                    // TODO: Handle function pointer type
                    // This code may need to update when code for AST_ID does

                    bool crossed_boundary = false;
                    Identifier* iden = info.ast->findIdentifier(scopeId, info.getCurrentOrder(), name, &crossed_boundary);

                    if(iden && !iden->is_fn() && (iden->type == Identifier::GLOBAL_VARIABLE || !crossed_boundary)){
                        auto var = iden->cast_var();
                        finalType = var->versions_typeId[info.currentPolyVersion];
                    } else {
                        // auto sc = info.ast->getScope(scopeId);
                        // sc->print(info.ast);
                        finalType = checkType(scopeId, name, expr->location, nullptr);
                    }
                }
                if(!finalType.isValid()) {
                    // DynamicArray<TypeId> temps{};
                    tempTypes.resize(0);
                    SignalIO result = checkExpression(scopeId, expr->left, &tempTypes, attempt);
                    finalType = tempTypes.size()==0 ? AST_VOID : tempTypes.last();
                }
            } else {
                auto& name = expr->name;
                bool crossed_boundary = false;
                Identifier* iden = info.ast->findIdentifier(scopeId, info.getCurrentOrder(), name, &crossed_boundary);

                if(iden && !iden->is_fn() && (iden->type == Identifier::GLOBAL_VARIABLE || !crossed_boundary)){
                    auto var = iden->cast_var();
                    finalType = var->versions_typeId[info.currentPolyVersion];
                } else {
                    // auto sc = info.ast->getScope(scopeId);
                    // sc->print(info.ast);
                    finalType = checkType(scopeId, name, expr->location, nullptr);
                }
                if(!finalType.isValid()) {
                    if(!hasForeignErrors()) {
                        Assert(expr->name.size()); // error, if we didn't have any
                    } else {
                        // return SIGNAL_FAILURE; // OI, WE DO NOTHING HERE, OK!
                    }
                    finalType = checkType(scopeId, expr->name, expr->location, nullptr);
                }
            }
            
            if(finalType.isValid()) {
                if(expr->typeId == AST_SIZEOF) {
                    expr->versions_outTypeSizeof.set(currentPolyVersion, finalType);
                    if(outTypes)  outTypes->add(AST_INT32);
                } else if(expr->typeId == AST_NAMEOF) {
                    std::string name = info.ast->typeToString(finalType);
                    u32 index=0;
                    auto constString = info.ast->getConstString(name,&index);
                    // Assert(constString);
                    expr->versions_constStrIndex.set(currentPolyVersion, index);
                    // expr->constStrIndex = index;

                    TypeId theType = checkType(scopeId, "Slice<char>", expr->location, nullptr);
                    if(outTypes)  outTypes->add(theType);
                } else if(expr->typeId == AST_TYPEID) {
                    
                    expr->versions_outTypeTypeid.set(currentPolyVersion, finalType);
                    
                    const char* tname = "lang_TypeId";
                    TypeId theType = checkType(scopeId, tname, expr->location, nullptr);
                    if(!theType.isValid()) {
                        // TODO: Don't hardcode "Lang" import. It may change in the future.
                        ERR_SECTION(
                            ERR_HEAD2(expr->location)
                            ERR_MSG("'"<<tname << "' was not a valid type. Did you forget to #import \"Lang\".")
                            ERR_LINE2(expr->location, "bad")
                        )
                    }
                    if(outTypes)  outTypes->add(theType);
                }
            } else {
                Assert(hasForeignErrors());
                if(outTypes) outTypes->add(AST_VOID);
            }
        } else if(expr->typeId == AST_ASM){
            for(auto a : expr->args) {
                QuickArray<TypeId> types{};
                auto signal = checkExpression(scopeId, a, &types, false, nullptr);
            }
            
            if(!expr->asmTypeString.isString()) {
                // asm has not type
                expr->versions_asmType.set(currentPolyVersion, AST_VOID);
                if(outTypes)
                    outTypes->add(AST_VOID);   
            } else {
                bool printedError = false;
                auto ti = checkType(scopeId, expr->asmTypeString, expr->location, &printedError);
                if (ti.isValid()) {
                    if(outTypes)
                        outTypes->add(ti);
                    expr->versions_asmType.set(currentPolyVersion, ti);
                } else {
                    if(!printedError){
                        ERR_SECTION(
                            ERR_HEAD2(expr->location)
                            ERR_MSG("Type "<<info.ast->getStringFromTypeString(expr->castType) << " does not exist.")
                            ERR_LINE2(expr->location,"bad")
                        )
                    }
                    if(outTypes)
                        outTypes->add(AST_VOID);
                }
            }
        } else if(expr->typeId == AST_NULL){
            // u32 index=0;
            // auto constString = info.ast->getConstString(expr->name,&index);
            // // Assert(constString);
            // expr->versions_constStrIndex[info.currentPolyVersion] = index;

            // TypeId theType = checkType(scopeId, "Slice<char>", expr->location, nullptr);
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
        
        // const char* str = OP_NAME(expr->typeId.getId());
        // bool is_operator_func = false;
        // log::out << "Check " << str << "\n";
        // if(str) {

        if(expr->typeId != AST_REFER && expr->typeId != AST_DEREF && expr->typeId != AST_MEMBER&& expr->typeId != AST_CAST && expr->typeId != AST_INITIALIZER) {
            // BREAK(expr->nodeId == 2606)
            
            if(expr->left) {
                tempTypes.resize(0);
                checkExpression(scopeId, expr->left, &tempTypes, attempt);
                if(tempTypes.size() > 1) {
                    // Error message for right expr is the same as below, DON'T
                    // forget to modify both when changing stuff!
                    ERR_SECTION(
                        ERR_HEAD2(expr->right->location)
                        ERR_MSG("Left expression produced more than one value which isn't allowed with the operation '"<<info.ast->typeToString(expr->typeId)<<"'.")
                        ERR_LINE2(expr->right->location,tempTypes.size() << " values");
                    )
                    return SIGNAL_FAILURE;
                }
                if(tempTypes.size()>0) {
                    leftType = tempTypes.last();
                    operatorArgs.add(tempTypes.last());
                }
            }
            if(expr->right) {
                tempTypes.resize(0);
                bool is_assignment = (expr->typeId == AST_ASSIGN && expr->assignOpType == (OperationType)0);
                if(is_assignment) {
                    inferred_type = leftType;
                }
                auto signal = checkExpression(scopeId, expr->right, &tempTypes, attempt);
                inferred_type = {};
                
                if(tempTypes.size() > 0 && tempTypes[0] == AST_VOID) {
                    if(hasAnyErrors()) {
                        return SIGNAL_FAILURE;
                    } else {
                        log::out << log::RED << "WHY WAS TYPE VOID\n";
                        Assert(false);
                    }
                }

                if(is_assignment) {
                    // TODO: Skipping values with assignment is okay.
                    //   But user may want to specify that you can't skip
                    //   values from certain functions (like error codes).
                    //   How do we do that here?
                    if(tempTypes.size()>0) {
                        rightType = tempTypes[0];
                        operatorArgs.add(tempTypes[0]);
                    } else {
                        ERR_SECTION(
                            ERR_HEAD2(expr->right->location)
                            ERR_MSG("Right expression produced zero values but the expression must produce at least one value for assignments.")
                            ERR_LINE2(expr->right->location, "zero values");
                        )
                        return SIGNAL_FAILURE;
                    }
                } else {
                    if(tempTypes.size() > 1) {
                        // Error message for left expr is the same as above, DON'T
                        // forget to modify both when changing stuff!
                        ERR_SECTION(
                            ERR_HEAD2(expr->right->location)
                            ERR_MSG("Right expression produced more than one value which isn't allowed with the operation '"<<info.ast->typeToString(expr->typeId)<<"'.")
                            ERR_LINE2(expr->right->location,tempTypes.size() << " values");
                        )
                        return SIGNAL_FAILURE;
                    }
                    if(tempTypes.size()>0) {
                        rightType = tempTypes[0];
                        operatorArgs.add(tempTypes[0]);
                    }
                }
            }
            // TODO: Optimize operator overload check. checkExpression executes in 250 ms where checking operator overloading is responsible for 100 ms. If we could optimize then we may run checkExpression in 150 + 20 ms instead. The key is a fast determination of whether expression is operator overloaded.

            // TODO: You should not be allowed to overload all operators.
            //  Fix some sort of way to limit which ones you can.
            const char* str = OP_NAME(expr->typeId.getId());
            // // log::out << "Check " << str << "\n";
            if(str && operatorArgs.size() == 2) {
                ZoneScopedNC("check operator",tracy::Color::WebPurple);
                int prev = expr->nonNamedArgs;
                expr->nonNamedArgs = 2; // unless operator overloading <- what do i mean by this - Emarioo 2023-12-19
                SignalIO result = checkFncall(scopeId,expr, outTypes, attempt, true, &operatorArgs);
                
                // log::out << "Check, " << expr->nodeId<<"\n";
                if(result == SIGNAL_SUCCESS) {
                    // log::out << "Yes, " << expr->nodeId<<"\n";
                    return SIGNAL_SUCCESS;
                }

                if(result != SIGNAL_NO_MATCH) {
                    // log::out << "Error, " << expr->nodeId<<"\n";
                    return result; // error bad
                }
                // no match, try normal operator
                expr->nonNamedArgs = prev;
            }
        }

        ZoneScopedNC("check operation",tracy::Color::WebPurple);

        switch(expr->typeId.getId()) {
        case AST_REFER: {
            if(expr->left) {
                checkExpression(scopeId, expr->left, outTypes, attempt);
                if(outTypes) {
                    if(outTypes->size()>0)
                        outTypes->last().setPointerLevel(outTypes->last().getPointerLevel()+1);
                }
            }
        } break;
        case AST_DEREF: {
            if(expr->left) {
                checkExpression(scopeId, expr->left, outTypes, attempt);
                if(outTypes) {
                    if(outTypes->last().getPointerLevel()==0){
                        ERR_SECTION(
                            ERR_HEAD2(expr->left->location)
                            ERR_MSG("Cannot dereference non-pointer.")
                            ERR_LINE2(expr->left->location,info.ast->typeToString(outTypes->last()));
                        )
                        return SIGNAL_FAILURE;
                    }else{
                        outTypes->last().setPointerLevel(outTypes->last().getPointerLevel()-1);
                    }
                }
            }
        } break;
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
            int expr_array_length = 0;
            if(expr->left) {
                // Operator overload checking does not run on AST_MEMBER since it can't be overloaded
                // leftType has therefore note been set and expression not checked.
                // We must check it here.
                checkExpression(scopeId, expr->left, &tempTypes, attempt, &expr_array_length);
                if(tempTypes.size()>0)  
                    leftType = tempTypes.last();
            }

            // if(tempTypes.size()==0) 
            //     tempTypes.add(AST_VOID);
            // outType holds the type of expr->left
            TypeInfo* ti = info.ast->getTypeInfo(leftType.baseType());
            if(leftType.getPointerLevel() >= 2) {
                ERR_SECTION(
                    ERR_HEAD2(expr->left->location)
                    ERR_MSG("Cannot access member of a double pointer type. Single pointers will be implicitly dereferenced.")
                    ERR_LINE2(expr->left->location,info.ast->typeToString(leftType))
                )
                // Assert(leftType.getPointerLevel()<2);
                return SIGNAL_FAILURE;
            }
            if(expr_array_length) {
                if(expr->name == "len") {
                    if(outTypes)
                        outTypes->add(AST_INT32);
                } else if(expr->name == "ptr") {
                    if(outTypes)
                        outTypes->add(leftType);
                } else {
                    ERR_SECTION(
                        ERR_HEAD2(expr->location)
                        ERR_MSG("'"<<info.ast->typeToString(leftType)<<"' is an array within a struct and does not have members. If the elements of the array are structs then index into the array first.")
                        ERR_LINE2(expr->left->location, info.ast->typeToString(leftType).c_str())
                    )
                    return SIGNAL_FAILURE;
                }
            } else if(ti && ti->astStruct){
                TypeInfo::MemberData memdata = ti->getMember(expr->name);
                if(memdata.index!=-1){
                    auto& mem = ti->astStruct->members[memdata.index];
                    if(mem.array_length > 0) {
                        // TODO: Possible bug here
                        if(array_length) {
                            TypeId id = memdata.typeId;
                            id.setPointerLevel(id.getPointerLevel()+1);
                            if(outTypes)
                                outTypes->add(id);
                            *array_length = mem.array_length;
                        } else {
                            TypeId id = memdata.typeId;
                            id.setPointerLevel(id.getPointerLevel()+1);
                            if(outTypes)
                                outTypes->add(id);

                            // NOTE: Previously, We returned slice type but now we return pointer instead. We do this because indexing pointer doesn't require operator overload and things work better.
                            // std::string slice_name = "Slice<" + info.ast->typeToString(memdata.typeId) + ">";
                            // TypeId id = checkType(scopeId, slice_name, expr->location, nullptr);
                            // // if(!slice_info) {
                            // //     ERR_SECTION(
                            // //         ERR_HEAD2(expr->location)
                            // //         ERR_MSG("Member '"<<expr->name<<"' was an array within a struct which evaluates to the type '"<<slice_name<<"' BUT it was not a valid type.")
                            // //         ERR_LINE2(expr->location,"bad type?");
                            // //     )
                            // //     if(outTypes)
                            // //         outTypes->add(AST_VOID);
                            // //     return SIGNAL_FAILURE;
                            // // }
                            // if(outTypes)
                            //     outTypes->add(id);
                        }
                    } else {
                        if(outTypes)
                            outTypes->add(memdata.typeId);
                    }
                } else {
                    std::string msgtype = "not member of "+info.ast->typeToString(leftType);
                    ERR_SECTION(
                        ERR_HEAD2(expr->location)
                        ERR_MSG("'"<<expr->name<<"' is not a member in struct '"<<info.ast->typeToString(leftType)<<"'.")
                        ERR_LINE2(expr->location,msgtype.c_str());
                        log::out << "These are the members: ";
                        for(int i=0;i<(int)ti->astStruct->members.size();i++){
                            if(i!=0)
                                log::out << ", ";
                            log::out << log::LIME << ti->astStruct->members[i].name<<log::NO_COLOR;
                            // TODO: Option to print types along with member names.
                            // log::out << log::LIME << ti->astStruct->members[i].name<<log::NO_COLOR<<": "<<info.ast->typeToString(ti->getMember(i).typeId);
                        }
                        log::out <<"\n";
                        log::out << "\n"
                    )
                    if(outTypes)
                        outTypes->add(AST_VOID);
                    return SIGNAL_FAILURE;
                }
            } else {
                ERR_SECTION(
                    ERR_HEAD2(expr->location)
                    ERR_MSG("Member access only works on variable with a struct type and enums. The type '" << info.ast->typeToString(tempTypes.last()) << "' is neither (astStruct/astEnum were null).")
                    ERR_LINE2(expr->left->location,"cannot take a member from this")
                    ERR_LINE2(expr->location,"member to access")
                )
                if(outTypes)
                    outTypes->add(AST_VOID);
                return SIGNAL_FAILURE;
            }
        } break;
        case AST_INDEX: {
            // TODO: THE COMMENTED CODE CAN BE REMOVED. IT'S OLD. SOMETHING WITH OPERATOR OVERLOADING
            // BUT WE DO THAT ABOVE.
            
            // TypeId leftType = {}, rightType = {};
            // if(expr->left) {
            //     tempTypes.resize(0);
            //     checkExpression(scopeId, expr->left, &tempTypes, attempt);
            //     Assert(tempTypes.size()<2); // error message
            //     if(tempTypes.size()>0) {
            //         leftType = tempTypes.last();
            //         operatorArgs.add(tempTypes.last());
            //     }
            // }
            // if(expr->right) {
            //     tempTypes.resize(0);
            //     checkExpression(scopeId, expr->right, &tempTypes, attempt);
            //     Assert(tempTypes.size()<2); // error message
            //     if(tempTypes.size()>0) {
            //         rightType = tempTypes.last();
            //         operatorArgs.add(tempTypes.last());
            //     }
            // }
            // if(expr->left) {
            //     checkExpression(scopeId, expr->left, &tempTypes, attempt);
            //     if(tempTypes.)
            //     // if(tempTypes.size()==0){
            //     //     tempTypes.add(AST_VOID);
            //     // }
            // }
            // if(expr->right) {
            //     checkExpression(scopeId, expr->right, nullptr, attempt);
            // }
            // if(expr->left && expr->right) {
            //     expr->nonNamedArgs = 2; // unless operator overloading
            //     SignalIO result = CheckFncall(info,scopeId,expr, outTypes, attempt, true, &operatorArgs);
                
            //     if(result == SIGNAL_SUCCESS)
            //         return SIGNAL_SUCCESS;

            //     if(result != SIGNAL_NO_MATCH)
            //         return result;
            // }
            // DynamicArray<TypeId> tempTypes{};
            
            // if(outTypes) outTypes->add(AST_VOID);
            // DynamicArray<TypeId> temp{};
            TypeInfo* linfo = nullptr;
            if(leftType.isNormalType()) {
                linfo = info.ast->getTypeInfo(leftType);
            }
            if(linfo && linfo->astStruct && linfo->astStruct->name == "Slice") {
                auto& mem = linfo->structImpl->members[0];
                Assert(linfo->astStruct->members[0].name == "ptr"); // generator checks this too
                if(outTypes){
                    outTypes->add(mem.typeId);
                    outTypes->last().setPointerLevel(outTypes->last().getPointerLevel()-1);
                }
            } else  if(leftType.getPointerLevel()==0){
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
        } break;
        case AST_RANGE: {
            // DynamicArray<TypeId> temp={};
            // if(expr->left) {
            //     checkExpression(scopeId, expr->left, nullptr, attempt);
            // }
            // if(expr->right) {
            //     checkExpression(scopeId, expr->right, nullptr, attempt);
            // }
            TypeId theType = checkType(scopeId, "Range", expr->location, nullptr);
            if(outTypes) outTypes->add(theType);
        } break;
        case AST_INITIALIZER: {
            TypeId ti{};
            if (!expr->castType.isValid()) {
                // infer type
                if (!inferred_type.isValid()) {
                    ERR_SECTION(
                        ERR_HEAD2(expr->location)
                        ERR_MSG_COLORED("Struct name was not specified for initializer. This requires that the type is inferred but the inferred type was '" << log::LIME << ast->typeToString(inferred_type) << log::NO_COLOR << "'. Types only be inferred from assignments and function arguments.")
                        ERR_LINE2(expr->location, "here")
                        ERR_EXAMPLE(1, "a: vec2 = {1,2}")
                        ERR_EXAMPLE(1, "fn hi(a: vec2) {}\nhi({1,2})")
                    )
                    return SIGNAL_FAILURE;
                } else {
                    ti = inferred_type;
                    inferred_type = {};
                }
            }
            if(!ti.isValid())
                ti = checkType(scopeId, expr->castType, expr->location, nullptr);
            if(!ti.isValid()) {
                ERR_SECTION(
                    ERR_HEAD2(expr->location)
                    ERR_MSG("'"<<info.ast->typeToString(expr->castType)<<"' is not a type.")
                    ERR_LINE2(expr->location,"bad")
                )
                return SIGNAL_FAILURE;
            }
            TypeInfo* typeinfo = nullptr;
            if(ti.isNormalType())
                typeinfo = ast->getTypeInfo(ti);
            for(int i=0;i<expr->args.size();i++) {
                auto arg_expr = expr->args[i];
                if(typeinfo && typeinfo->structImpl) {
                    // getMember returns invalid type if 'i' is out of bounds
                    inferred_type = typeinfo->getMember(i).typeId;
                }
                checkExpression(scopeId, arg_expr, nullptr, attempt);
                inferred_type = {};
            }
            if(outTypes)
                outTypes->add(ti);
            expr->versions_castType.set(currentPolyVersion, ti);
        } break;
        case AST_CAST: {
            // DynamicArray<TypeId> temp{};
            Assert(expr->left);
            checkExpression(scopeId, expr->left, &tempTypes, attempt);
            Assert(tempTypes.size()==1);

            Assert(expr->castType.isString());
            bool printedError = false;
            auto ti = checkType(scopeId, expr->castType, expr->location, &printedError);
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
            expr->versions_castType.set(currentPolyVersion, ti);
            
            if(expr->isUnsafeCast()) {
                int ls = info.ast->getTypeSize(ti);
                int rs = info.ast->getTypeSize(tempTypes.last());
                if(ls != rs) {
                    std::string strleft = info.ast->typeToString(tempTypes.last()) + " ("+std::to_string(ls)+")";
                    std::string strcast = info.ast->typeToString(ti)+ " ("+std::to_string(rs)+")";
                    ERR_SECTION(
                        ERR_HEAD2(expr->location, ERROR_CASTING_TYPES)
                        ERR_MSG("cast_unsafe requires that both types have the same size. "<<ls << " != "<<rs<<"'.")
                        ERR_LINE2(expr->left->location,strleft)
                        ERR_LINE2(expr->location,strcast)
                        ERR_EXAMPLE_TINY("cast<void*> cast<u64> number")
                    )
                }
            } else if(!(tempTypes.last().getPointerLevel() > 0 && ti.getPointerLevel() > 0) && !info.ast->castable(tempTypes.last(),ti, true)){
                std::string strleft = info.ast->typeToString(tempTypes.last());
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
        case AST_MODULO:
        case AST_UNARY_SUB: {
            if(outTypes) {
                if(!rightType.isValid() || rightType == AST_VOID) {
                    outTypes->add(leftType); // unary operator
                } else if(leftType.isPointer() && AST::IsInteger(rightType)){
                    outTypes->add(leftType);
                } else if (AST::IsInteger(rightType) && rightType.isPointer()) {
                    outTypes->add(rightType);
                } else if (AST::IsInteger(leftType) && AST::IsInteger(rightType)) {
                    u8 lsize = info.ast->getTypeSize(leftType);
                    u8 rsize = info.ast->getTypeSize(rightType);
                    auto outtype = lsize > rsize ? leftType : rightType;
                    if(AST::IsSigned(leftType) || AST::IsSigned(rightType)) {
                        if(!AST::IsSigned(outtype))
                            outtype._infoIndex0 += 4;
                        Assert(AST::IsSigned(outtype) && AST::IsInteger(outtype));
                    }
                    outTypes->add(outtype);
                } else if ((AST::IsInteger(leftType) || leftType == AST_CHAR) && (AST::IsInteger(rightType) || rightType == AST_CHAR)){
                    // '0' + 5 should return a char and not an integer
                    // std_print('0' + 5) this would print integer instead of char otherwise
                    outTypes->add(AST_CHAR);
                } else if ((AST::IsDecimal(leftType) || AST::IsInteger(leftType)) && (AST::IsDecimal(rightType) || AST::IsInteger(rightType))){
                    if(AST::IsDecimal(leftType))
                        outTypes->add(leftType);
                    else
                        outTypes->add(rightType);
                } else if(leftType == rightType) {
                    outTypes->add(leftType);
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
            // TODO: Verify valid types on left and right.
            //   A structure type is not allowed (unless overloaded operator which is handled earlier)
            //   The generator catches does bugs but it would be nice to do so here.

            if(outTypes) {
                outTypes->add(TypeId(AST_BOOL));
            }
        } break;
        case AST_BAND:
        case AST_BOR:
        case AST_BXOR: {
            // TODO: Signed unsigned doesn't matter, the size and integer does
            auto linfo = info.ast->getTypeInfo(leftType);
            auto rinfo = info.ast->getTypeInfo(rightType);
            int lsize = info.ast->getTypeSize(leftType);
            int rsize = info.ast->getTypeSize(rightType);
            if((AST::IsInteger(leftType) || leftType == AST_CHAR) && (AST::IsInteger(rightType) || rightType == AST_CHAR) && lsize == rsize) {
                if(outTypes) {
                    outTypes->add(leftType);
                }
            } else if (linfo && rinfo && linfo->astEnum && linfo->astEnum == rinfo->astEnum) {
                if(outTypes) {
                    outTypes->add(leftType);
                }
            } else {
                ERR_SECTION(
                    ERR_HEAD2(expr->location)
                    ERR_MSG("Left and right type must be an integer of the same size for the binary operations AND, OR, and XOR. This was not the case.")
                    ERR_LINE2(expr->left->location, info.ast->typeToString(leftType))
                    ERR_LINE2(expr->right->location, info.ast->typeToString(rightType))
                )
                if(outTypes) {
                    // TODO: output poison type
                    // outTypes->add(leftType);
                }
                return SIGNAL_FAILURE;
            }
        } break;
        case AST_BNOT:
        case AST_BLSHIFT:
        case AST_BRSHIFT: {
            if(outTypes) {
                outTypes->add(leftType);
            }
        } break;
        case AST_INCREMENT:
        case AST_DECREMENT: {
            if(outTypes) {
                outTypes->add(leftType);
            }
        } break;
        default: {
            Assert(false);
        }
        }
    }
    return SIGNAL_SUCCESS;
}

// Evaluates types and offset for the given function implementation
// It does not modify ast func
SignalIO TyperContext::checkFunctionImpl(ASTFunction* func, FuncImpl* funcImpl, ASTStruct* parentStruct, QuickArray<TypeId>* outTypes, StructImpl* parentStructImpl){
    using namespace engone;
    ZoneScopedC(tracy::Color::Purple4);
    _TCLOG_ENTER(FUNC_ENTER)

    funcImpl->polyVersion = func->polyVersionCount++;

    // log::out << "IMPL " << func->name<<"\n";

    Assert(funcImpl->signature.polyArgs.size() == func->polyArgs.size());
    
    Assert(func->parentStruct == parentStruct);
    func->pushPolyState(funcImpl);
    defer {
        func->popPolyState();
    };

    _TCLOG(log::out << "Check func impl "<< funcImpl->astFunction->name<<"\n";)

    // TODO: parentStruct argument may not be necessary since this function only calculates
    //  offsets of arguments and return values.

    // TODO: Handle calling conventions

    funcImpl->signature.argumentTypes.resize(func->arguments.size());
    funcImpl->signature.returnTypes.resize(func->returnValues.size());
    funcImpl->signature.convention = func->callConvention;

    int offset = 0; // offset starts before call frame (fp, pc)

    for(int i=0;i<(int)func->arguments.size();i++){

        auto& arg = func->arguments[i];
        auto& argImpl = funcImpl->signature.argumentTypes[i];
        if(arg.stringType.isString()){
            bool printedError = false;
            // Token token = info.ast->getStringFromTypeString(arg.typeId);
            TypeId ti{};
            if(parentStruct && i==0 && parentStructImpl && parentStructImpl->typeId.isValid()) {
                ti = parentStructImpl->typeId;
                ti.setPointerLevel(1);
            } else
                ti = checkType(func->scopeId, arg.stringType, func->location, &printedError);
            
            if(ti == AST_VOID){
                if(!info.hasAnyErrors()) { 
                    std::string msg = info.ast->typeToString(arg.stringType);
                    ERR_SECTION(
                        ERR_HEAD2(func->location)
                        ERR_MSG("Type '"<<msg<<"' is unknown. Did you misspell or forget to declare the type?")
                        ERR_LINE2(func->arguments[i].location,msg.c_str())
                    )
                }
            } else if(ti.isValid()){

            } else if(!printedError) {
                if(!info.hasAnyErrors()) { 
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
        //     checkExpression(func->scopeId, arg.defaultValue, &info.temp_defaultArgs, false);
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
        if(func->callConvention == STDCALL || func->callConvention == UNIXCALL)
            asize = 8;
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
    //     auto& argImpl = funcImpl->signature.argumentTypes[i];
    //     // TypeInfo* typeInfo = info.ast->getTypeInfo(arg.typeId);
    //     int size = info.ast->getTypeSize(argImpl.typeId);
    //     argImpl.offset = offset - argImpl.offset - size;
    //     // log::out << " Reverse Arg "<<arg.offset << ": "<<arg.name<<"\n";
    // }
    funcImpl->signature.argSize = offset;

    offset = 0;
    if(outTypes){
        Assert(outTypes->size()==0);
    }
    
    for(int i=(int)funcImpl->signature.returnTypes.size()-1;i>=0;i--){
        auto& retImpl = funcImpl->signature.returnTypes[i];
        auto& retStringType = func->returnValues[i].stringType;
        if(retStringType.isString()){
            bool printedError = false;
            auto ti = checkType(func->scopeId, retStringType, func->location, &printedError);
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
        if(func->callConvention == STDCALL || func->callConvention == UNIXCALL)
            asize = 8;
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

    // for(int i=0;i<(int)funcImpl->signature.returnTypes.size();i++){
    //     auto& ret = funcImpl->signature.returnTypes[i];
    //     // TypeInfo* typeInfo = info.ast->getTypeInfo(arg.typeId);
    //     int size = info.ast->getTypeSize(ret.typeId);
    //     ret.offset = ret.offset - offset;
    //     // log::out << " Reverse Arg "<<arg.offset << ": "<<arg.name<<"\n";
    // }
    funcImpl->signature.returnSize = offset;

    if(outTypes && funcImpl->signature.returnTypes.size()==0){
        outTypes->add(AST_VOID);
    }
    return SIGNAL_SUCCESS;
}
// Ensures that the function identifier exists.
// Adds the overload for the function.
// Checks if an existing overload would collide with the new overload.
SignalIO TyperContext::checkFunction(ASTFunction* function, ASTStruct* parentStruct, ASTScope* scope){
    using namespace engone;
    _TCLOG_ENTER(FUNC_ENTER)
    
    SCOPED_ALLOCATOR_MOMENT(scratch_allocator)

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
    // _TCLOG(log::out << "Method/function has polymorphic properties: "<<function->name<<"\n";)
    OverloadGroup* fnOverloads = nullptr;
    IdentifierFunction* iden = nullptr;
    if(parentStruct){
        fnOverloads = parentStruct->getMethod(function->name, true);
    } else {
        iden = (IdentifierFunction*)info.ast->findIdentifier(scope->scopeId, CONTENT_ORDER_MAX, function->name, nullptr);
        if(!iden){
            // cast operator won't work
            iden = info.ast->addFunction(scope->scopeId, function->name, CONTENT_ORDER_ZERO);
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
    if(function->isOperator() && function->name == "cast") {
        std::string path;
        int line,column;
        info.compiler->lexer.get_source_information(function->location, &path, &line, &column);
        
        // TODO: Fix overloads
        // log::out << log::YELLOW << path <<":"<<line<<":"<<column<< " (warning): Type conversions (operator cast) is experimental and incomplete.\n";
    }
    if (function->callConvention == UNIXCALL && function->arguments.size() > 6) {
        ERR_SECTION(
            ERR_HEAD2(function->location)
            ERR_MSG_LOG("The compiler does not support unixcall (Sys V ABI) calling convention with more than "<<log::LIME << " 6 "<<log::NO_COLOR << " arguments. (it needs testing)\n\n")
            ERR_LINE2(function->location, "too many arguments for unixcall")
        )
        return SIGNAL_FAILURE;
    }
    if(parentStruct) {
        function->memberIdentifiers.resize(parentStruct->members.size());
        for(int i=0;i<(int)parentStruct->members.size();i++){
            auto& mem = parentStruct->members[i];
            bool reused = false;
            auto varinfo = info.ast->addVariable(Identifier::MEMBER_VARIABLE, function->scopeId, mem.name, CONTENT_ORDER_ZERO, &reused);
            function->memberIdentifiers[i] = varinfo;
            varinfo->memberIndex = i;
        }
    }
    for(int i=0;i<(int)function->arguments.size();i++){
        auto& arg = function->arguments[i];
        bool reused = false;
        auto var = info.ast->addVariable(Identifier::ARGUMENT_VARIABLE, function->scopeId, arg.name, CONTENT_ORDER_ZERO, &reused);
        if(!var) {
            // TODO: Specify exactly which member or argument has the same name.
            //   Also show where it is.
            ERR_SECTION(
                ERR_HEAD2(arg.location)
                ERR_MSG("The argument cannot be named '"<<arg.name<<"' because it already exists. Either a previous argument or a member of the parent struct uses the same name.")
                ERR_LINE2(arg.location,"rename to something else")
            )
            continue;
        }
        arg.identifier = var;
        var->argument_index = i;
    }
    if(function->polyArgs.size()==0 && (!parentStruct || parentStruct->polyArgs.size() == 0)){
        // The code below is used to 
        // Acquire identifiable arguments
        TEMP_ARRAY(TypeId, argTypes);
        for(int i=0;i<(int)function->arguments.size();i++){
            // info.ast->printTypesFromScope(function->scopeId);

            TypeId typeId = checkType(function->scopeId, function->arguments[i].stringType, function->location, nullptr);
            // Assert(typeId.isValid());
            if(!typeId.isValid()){
                // TODO: Don't print this? CheckType already did?
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
        OverloadGroup::Overload* ambiguousOverload = nullptr;
        for(int i=0;i<(int)fnOverloads->overloads.size();i++){
            auto& overload = fnOverloads->overloads[i];
            bool found = true;
            // if(parentStruct) {
            //     if(overload.astFunc->nonDefaults+1 > function->arguments.size() ||
            //         function->nonDefaults+1 > overload.astFunc->arguments.size())
            //         continue;
            //     for (u32 j=0;j<function->nonDefaults || j<overload.astFunc->nonDefaults;j++){
            //         if(overload.funcImpl->signature.argumentTypes[j+1].typeId != argTypes[j+1]){
            //             found = false;
            //             break;
            //         }
            //     }
            // } else {

                if(overload.astFunc->nonDefaults > function->arguments.size() ||
                    function->nonDefaults > overload.astFunc->arguments.size())
                    continue;
                for (u32 j=0;j<function->nonDefaults || j<overload.astFunc->nonDefaults;j++){
                    if(overload.funcImpl->signature.argumentTypes[j].typeId != argTypes[j]){
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
            auto al = ambiguousOverload->astFunc->linkConvention;
            auto bl = function->linkConvention;
            auto ac = ambiguousOverload->astFunc->callConvention;
            auto bc = function->callConvention;
            
            if ((ac == INTRINSIC && bc == INTRINSIC) || (al == NATIVE && bl == NATIVE)) {
                // We should check signature just so that you don't do
                //     strlen(n: i32) or strlen(b: f32)
                // doesn't have to happen here though.
                // native functions can be defined twice
            } else {
                //  TODO: better error message which shows what and where the already defined variable/function is.
                ERR_SECTION(
                    ERR_HEAD2(function->location)
                    ERR_MSG("Argument types are ambiguous with another overload of function '"<< function->name << "'.")
                    ERR_LINE2(ambiguousOverload->astFunc->location,"existing overload");
                    ERR_LINE2(function->location,"new ambiguous overload");
                )
                // print list of overloads?
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
                ast->addOverload(fnOverloads, function, funcImpl);
                int overload_i = fnOverloads->overloads.size()-1;
                if(parentStruct)
                    funcImpl->structImpl = parentStruct->nonPolyStruct;
                // DynamicArray<TypeId> retTypes{}; // @unused
                SignalIO yes = checkFunctionImpl( function, funcImpl, parentStruct, nullptr);

                // TODO: Implement a list of functions to forcefully generate
                if(function->name == compiler->entry_point) {
                    funcImpl->usages = 1;
                    // We add main automatically in Compiler::processImports
                    // no need to addTask_type_body here
                    // TypeChecker::CheckImpl checkImpl{};
                    // checkImpl.astFunc = fnOverloads->overloads[overload_i].astFunc;
                    // checkImpl.funcImpl = fnOverloads->overloads[overload_i].funcImpl;
                    // info.typeChecker->checkImpls.add(checkImpl);
                } else if(function->export_alias.size() != 0) {
                    auto overload = fnOverloads->overloads.last();
                    ast->declareUsageOfOverload(&overload);
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
        if (function->export_alias.size() != 0) {
            ERR_SECTION(
                ERR_HEAD2(function->location)
                ERR_MSG("You cannot export polymorphic functions.")
                ERR_LINE2(function->location, "here")
            )
        }
        // TODO: Is the ambiguity solved? I did some changes to overload matching and it seems to work fine.

        // if(fnOverloads->polyOverloads.size()!=0){
        //     std::string path;
        //     int line, column;
        //     info.compiler->lexer.get_source_information(function->location, &path, &line, &column);
        //     MSG_CODE_LOCATION;
        //     log::out << log::YELLOW << path <<":"<<line<<":"<<column<< " (warning): Ambiguity for polymorphic overloads is not checked! (implementation incomplete) '"<<log::LIME << function->name<<log::NO_COLOR<<"'\n";
        // }

        // Base poly overload is added without regard for ambiguity. It's hard to check ambiguity so to it later.
        ast->addPolyOverload(fnOverloads, function);
    }
    return SIGNAL_SUCCESS;
}
SignalIO TyperContext::checkFunctions(ASTScope* scope){
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
    //                     SignalIO result = checkFunctions(it->firstBody);   
    //                 }
    //                 if(it->secondBody){
    //                     SignalIO result = checkFunctions(it->secondBody);
    //                 }
    //             }
    //         }
    //         break; case ASTScope::NAMESPACE: {
    //             auto it = scope->namespaces[spot.index];
    //             checkFunctions(it);
    //         }
    //         break; case ASTScope::FUNCTION: {
    //             auto it = scope->functions[spot.index];
    //             checkFunction(it, nullptr, scope);
    //             if(it->body){
    //                 SignalIO result = checkFunctions(it->body);
    //             }
    //         }
    //         break; case ASTScope::STRUCT: {
    //             auto it = scope->structs[spot.index];
    //             for(auto fn : it->functions){
    //                 checkFunction(fn , it, scope);
    //                 if(fn->body){ // external/native function do not have bodies
    //                     SignalIO result = checkFunctions(fn->body);
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
        checkFunctions(now);
    }
    
    for(auto fn : scope->functions){
        checkFunction(fn, nullptr, scope);
        if(fn->body){ // external/native function do not have bodies
            SignalIO result = checkFunctions(fn->body);
        }
    }
    for(auto it : scope->structs){
        for(auto fn : it->functions){
            SignalIO result = checkFunction(fn , it, scope);
            if(result != SIGNAL_SUCCESS) {
                log::out << log::RED <<  "Bad\n";
            }
            if(fn->body){ // external/native function do not have bodies
                SignalIO result = checkFunctions(fn->body);
            }
        }
    }
    
    for(auto astate : scope->statements) {
        // if(astate->hasNodes()){
            if(astate->firstBody){
                SignalIO result = checkFunctions(astate->firstBody);   
            }
            if(astate->secondBody){
                SignalIO result = checkFunctions(astate->secondBody);
            }
        // }
    }
    
    return SIGNAL_SUCCESS;
}

SignalIO TyperContext::checkFuncImplScope(ASTFunction* func, FuncImpl* funcImpl){
    using namespace engone;
    _TCLOG_ENTER(FUNC_ENTER) // if(func->body->nativeCode)
    //     return true;

    
    info.currentPolyVersion = funcImpl->polyVersion;
    // log::out << log::YELLOW << __FILE__<<":"<<__LINE__ << ": Fix methods\n";
    // Where does ASTStruct come from. Arguments? FuncImpl?
    // log::out << " " <<funcImpl->polyVersion << " " << func->polyVersionCount<<"\n";
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
        auto& argImpl = funcImpl->signature.argumentTypes[i];
        _TCLOG(log::out << " " << arg.name<<": "<< info.ast->typeToString(argImpl.typeId) <<"\n";)
        arg.identifier->versions_typeId.set(currentPolyVersion, argImpl.typeId); // typeId comes from CheckExpression which may or may not evaluate the same type as the generator.
    }
    // _TCLOG(log::out << "ret:\n";)
    // for (int i=0;i<(int)func->returnValues.size();i++) {
    //     auto& ret = func->returnValues[i];
    //     auto& retImpl = funcImpl->signature.returnTypes[i];
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
            iden->versions_typeId.set(currentPolyVersion, memImpl.typeId); // typeId comes from CheckExpression which may or may not evaluate the same type as the generator.
        }
    }
    _TCLOG(log::out << "\n";)

    // log::out << log::CYAN << "BODY " << log::NO_COLOR << func->body->location<<"\n";
    checkRest(func->body);
    
    return SIGNAL_SUCCESS;
}
// SignalIO CheckGlobals(ASTScope* scope) {

//     return SIGNAL_SUCCESS;
// }
SignalIO TyperContext::checkDeclaration(ASTStatement* now, ContentOrder contentOrder, ASTScope* scope) {
    using namespace engone;
    // log::out << now->varnames[0].name << " checked\n";

    if(now->linked_library.size() != 0) {
        // imported thing
        if(now->firstExpression) {
            ERR_SECTION(
                ERR_HEAD2(now->location)
                ERR_MSG("Imported variables cannot have expressions.")
                ERR_LINE2(now->location, "here")
            )
            return SIGNAL_FAILURE;
        }
        if(now->varnames.size() != 1) {
            ERR_SECTION(
                ERR_HEAD2(now->location)
                ERR_MSG("Imported variables cannot only have one variable name and one type.")
                ERR_LINE2(now->location, "here")
            )
            return SIGNAL_FAILURE;
        }
    }

    SCOPED_ALLOCATOR_MOMENT(scratch_allocator)

    TEMP_ARRAY_N(TypeId, tempTypes, 5)

    // auto poly_typeArray = now->versions_expressionTypes[info.currentPolyVersion];
    QuickArray<TypeId> poly_typeArray{};
    now->versions_expressionTypes.steal_element_into(info.currentPolyVersion, poly_typeArray);
    defer {
        now->versions_expressionTypes.set(info.currentPolyVersion, poly_typeArray);
    };
    poly_typeArray.resize(0);
    // tempTypes.resize(0);
    if(now->firstExpression){
        // may not exist, meaning just a declaration, no assignment
        // if(Equal(now->firstExpression->name, "reserve")) {
            // __debugbreak();
            // log::out << "okay\n";
        // }
        if (now->varnames.size() > 0) {
            auto& last_varname = now->varnames.last();
            if(last_varname.assignString.isValid()) {
                inferred_type = now->varnames.last().versions_assignType[info.currentPolyVersion];
            } else {
                // can't set inferred type
                // TODO: Throw error if expression is initializer without a type
            }
        }
        SignalIO result = checkExpression(scope->scopeId,now->firstExpression, &poly_typeArray, false);
        inferred_type = {};
        
        for(int i=0;i<poly_typeArray.size();i++){
            if(poly_typeArray[i].isValid() && poly_typeArray[i] != AST_VOID)
                continue;

            if(result == SIGNAL_SUCCESS) { // expressions said we were successful but that's not actually true
                ERR_SECTION(
                    ERR_HEAD2(now->firstExpression->location, ERROR_INVALID_TYPE)
                    ERR_MSG("Expression must produce valid types for the assignment.")
                    ERR_LINE2(now->location, "this assignment")
                    if(poly_typeArray[i].isValid()) {
                    ERR_LINE2(now->firstExpression->location, info.ast->typeToString(poly_typeArray[i]))
                    } else {
                    ERR_LINE2(now->firstExpression->location, "no type?")
                    }
                )
            }
        }
    }

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

        IdentifierVariable* varinfo = nullptr;

        if(!varname.declaration) {
            // ASSIGN TO EXISTING VARIABLES (find them first)
            if(varname.identifier) {
                // a polymorphic version fixed identifier for us
            } else {
                // info.ast->getScope(scope->scopeId)->print(info.ast);
                Identifier* possible_identifier = info.ast->findIdentifier(scope->scopeId, contentOrder, varname.name, nullptr);
                if(!possible_identifier) {
                    ERR_SECTION(
                        ERR_HEAD2(varname.location)
                        ERR_MSG("'"<<varname.name<<"' is not a defined identifier. You have to declare the variable first.")
                        ERR_LINE2(varname.location, "undeclared")
                        ERR_EXAMPLE(1,"var: i32 = 12;")
                        ERR_EXAMPLE(1,"var := 23;  // inferred type")
                        ERR_EXAMPLE(1,"var: i32, other = return_one_and_two();\n// declared var, other should be declared")
                    )
                    return SIGNAL_FAILURE;
                    // continue;
                } else if(!possible_identifier->is_var()) {
                    err_non_variable();
                    return SIGNAL_FAILURE;
                    // continue;
                } else {
                    varname.identifier = possible_identifier->cast_var();
                }
            }
            varinfo = varname.identifier;
        } else {
            // DECLARE NEW VARIABLE(S)

            // The code below should have caused less errors but it causes more errors because of undeclared identifiers
            // if(!varname.versions_assignType[info.currentPolyVersion].isValid() && info.hasAnyErrors()) {
            //     // Assert(info.hasAnyErrors());
            //     continue;
            // }

            // We begin by matching the varname in ASTStatement with an identifier and VariableInfo

            // Infer type
            if(!varname.assignString.isValid()) {
                if(vi < poly_typeArray.size()){
                    varname.versions_assignType.set(currentPolyVersion, poly_typeArray[vi]);
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
                possible_identifier = info.ast->findIdentifier(scope->scopeId, contentOrder, varname.name, nullptr);
                if(!possible_identifier) {
                    // identifier does not exist, we should create it.
                    if(now->globalDeclaration) {
                        varinfo = info.ast->addVariable(Identifier::GLOBAL_VARIABLE, scope->scopeId, varname.name, CONTENT_ORDER_ZERO, nullptr);
                    } else {
                        varinfo = info.ast->addVariable(Identifier::LOCAL_VARIABLE, scope->scopeId, varname.name, contentOrder, nullptr);
                    }
                    varname.identifier = varinfo;
                    Assert(varinfo);
                } else if(possible_identifier->is_var()) {
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
                                ERR_MSG("Cannot redeclare variables. Shadowing identifiers may or may not be a feature in the future.")
                                ERR_LINE2(varname.location,"");
                                // TODO: Print the location of the previous varable
                            )
                            return SIGNAL_FAILURE;

                            // continue;
                        } else {
                            varname.identifier = possible_identifier->cast_var();
                            // We should only be here if the scope is polymorphic.
                            varinfo = varname.identifier;
                            // varinfo = info.ast->getVariableByIdentifier(varname.identifier);
                            // The 'now' statement was checked to be global and thus made varinfo global. varinfo->type will always be the same as now->globalDeclaration
                            // The assert will fire if we have a bug in the code.
                            Assert(varinfo->type == (now->globalDeclaration ? Identifier::GLOBAL_VARIABLE : Identifier::LOCAL_VARIABLE));
                        }
                    } else {
                        // varname.identifier = possible_identifier->cast_var();
                        // variable does not exist in scope, we can therefore create it
                        varinfo = info.ast->addVariable(now->globalDeclaration ? Identifier::GLOBAL_VARIABLE : Identifier::LOCAL_VARIABLE, scope->scopeId, varname.name, contentOrder, nullptr);
                        varname.identifier = varinfo;
                        Assert(varinfo);
                        // ACTUALLY! I thought global assignment in local scopes didn't work but I believe it does.
                        //  The code here just handles where the variable is visible. That should be the local scope which is correct.
                        //  The Identifier::GLOBAL_VARIABLE type is what puts the data into the global data section. Silly me.
                        //  - Emarioo, 2024-1-03
                        // if(now->globalDeclaration && scope->scopeId != 0) {
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
                    }
                } else {
                    err_non_variable();
                    return SIGNAL_FAILURE;
                    // continue;

                }
            } else {
                // We should only be here if we are in a polymorphic scope/check.
                Assert(varname.identifier->is_var());
                varinfo = varname.identifier;
                // varinfo = info.ast->getVariableByIdentifier(varname.identifier);
                // Assert(varinfo);
                Assert(varinfo->type == (now->globalDeclaration ? Identifier::GLOBAL_VARIABLE : Identifier::LOCAL_VARIABLE));
            }
            Assert(varinfo);

            if(now->globalDeclaration)
                varinfo->is_import_global = inside_import_scope;

            // I don't think the Assert will ever fire. But just in case.
            // HAHA, IT DID FIRE! Globals were checked twice! - Emarioo, 2024-01-06
            // Yes, it happens no and then with new bugs. We now throw an error
            //   instead of relying on an assert. - Emarioo, 2024-05-03
            if(varinfo->versions_typeId[info.currentPolyVersion].isValid()) {
                // I believe the final program will work fine, even if we check things twice.
                // It's just that we might allocate some extra unecessary global data.
                bool preverr = info.ignoreErrors;
                info.ignoreErrors = true;
                defer { info.ignoreErrors = preverr; };

                ERR_SECTION(
                    ERR_HEAD2(now->location)
                    ERR_MSG("Compiler bug! Declaration already had a valid type which indicates that we checked it twice. The compiler should not check things twice. Program will still compile but please notify developers about this.")
                    ERR_LINE2(now->location,"here")
                )
            }

            // FINALLY we can set the declaration type
            varinfo->versions_typeId.set(currentPolyVersion, varname.versions_assignType[info.currentPolyVersion]);
        }

        // if(varname.declaration) {
            _TCLOG(log::out << " " << varname.name<<": "<< info.ast->typeToString(varname.versions_assignType[info.currentPolyVersion]) << " scope: "<<scope->scopeId << " order: "<<contentOrder<< "\n";)
        // } else {
        //     _TCLOG(log::out << " " << varname.name<<": "<< info.ast->typeToString(varinfo->versions_typeId[info.currentPolyVersion]) << " scope: "<<scope->scopeId << " order: "<<contentOrder<< "\n";)
        // }

        // We continue by checking the expression with the variable/identifier info type

        // Technically, we don't need vi < poly_typeArray.size
        // We check for it above. HOWEVER, we don't stop and that's because we try to
        // find more errors. That is why we need vi < ...
        if(vi < (int)poly_typeArray.size()){
            if(!info.ast->castable(poly_typeArray[vi], varinfo->versions_typeId[info.currentPolyVersion], false)) {
                auto badtype = info.ast->typeToString(varname.assignString);
                ERR_SECTION(
                    ERR_HEAD2(now->firstExpression->location, ERROR_TYPE_MISMATCH)
                    ERR_MSG("Type mismatch " << info.ast->typeToString(poly_typeArray[vi]) << " - " << badtype << ".")
                    // varinfo->versions_typeId[info.currentPolyVersion]
                    ERR_LINE2(varname.location, badtype)
                    ERR_LINE2(now->firstExpression->location, info.ast->typeToString(poly_typeArray[vi]))
                )
            }
        }
        if(varinfo->isGlobal()) {
            if(!now->isImported()) {
                u32 size = info.ast->getTypeSize(varinfo->versions_typeId[info.currentPolyVersion]);
                u32 offset = info.ast->aquireGlobalSpace(size);
                varinfo->versions_dataOffset.set(currentPolyVersion, offset);

                if (now->firstExpression) {
                    info.ast->globals_to_evaluate.add({now, scope->scopeId});
                    // log::out << "global " << log::CYAN << varname.name << "\n";
                } else {
                    auto type = varinfo->versions_typeId[info.currentPolyVersion];
                    if(type.isNormalType()) {
                        auto typeinfo = info.ast->getTypeInfo(type);
                        if(typeinfo && typeinfo->astStruct) {
                            // TODO: Optimize by having a flag on each struct that
                            //   indicates whether any member has default values.
                            //   This gets difficult to keep track of with nested
                            //   structs. As long as we only need to know about default values here,
                            //   we don't really need to optimize, probably.
                            bool has_default_values = false;
                            
                            TEMP_ARRAY(TypeInfo*, stack);
                            stack.add(typeinfo);
                            int head = 0;
                            while(head < stack.size()) {
                                auto now = stack[head];
                                head++;
                                
                                Assert(now->astStruct);
                                for(int mi = 0; mi < now->astStruct->members.size(); mi++) {
                                    auto& mem = now->astStruct->members[mi];
                                    if(mem.defaultValue) {
                                        stack.resize(0);
                                        has_default_values = true;
                                        break;
                                    }
                                    auto memtype = now->structImpl->members[mi].typeId;
                                    if(!memtype.isNormalType())
                                        continue;
                                    auto new_info = ast->getTypeInfo(memtype);
                                    if(new_info->astStruct) {
                                        bool found = false;
                                        for(auto& t : stack) {
                                            if(new_info == t) {
                                                found = true;
                                                break;
                                            }
                                        }
                                        if(!found)
                                            stack.add(new_info);
                                    }
                                }
                            }

                            if(has_default_values)
                                info.ast->globals_to_evaluate.add({now, scope->scopeId});
                        }
                    }
                }
            } else {
                // varinfo->declaration = now;
            }
            // should we not always set this?
            varinfo->declaration = now;
        }
        // Array initializer list
        if(now->arrayValues.size()!=0) {
            Assert(vi == 0); // we are not handling array initializers with multiple varnames.
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
                    tempTypes.resize(0);
                    SignalIO result = checkExpression(scope->scopeId, value, &tempTypes, false);
                    if(result != SIGNAL_SUCCESS){
                        // continue;
                        return SIGNAL_FAILURE;
                    }
                    if(tempTypes.size()!=1) {
                        if(tempTypes.size()==0) {
                            ERR_SECTION(
                                ERR_HEAD2(value->location)
                                ERR_MSG("Expressions in array initializers cannot consist of of no values. Did you call a function that returns void?")
                                ERR_LINE2(value->location, "bad")
                            )
                            return SIGNAL_FAILURE;
                            // continue;
                        } else {
                            ERR_SECTION(
                                ERR_HEAD2(value->location)
                                ERR_MSG("Expressions in array initializers cannot consist of multiple values. Did you call a function that returns more than one value?")
                                ERR_LINE2(value->location, "bad")
                                // TODO: Print the types in rightTypes
                            )
                            return SIGNAL_FAILURE;
                            // continue;
                        }
                    }
                    if(!info.ast->castable(tempTypes[0], elementType, false)) {
                        ERR_SECTION(
                            ERR_HEAD2(value->location)
                            ERR_MSG("Cannot cast '"<<info.ast->typeToString(tempTypes[0])<<"' to '"<<info.ast->typeToString(elementType)<<"'.")
                            ERR_LINE2(value->location, "cannot cast")
                        )
                    }
                }
            }
        }
    }
    return SIGNAL_SUCCESS;
}
SignalIO TyperContext::checkRest(ASTScope* scope){
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
 

    // Some code was copied to TypeCheckFunctions.. Don't forget to modify TypeCheckFunctions and here when making changes.
    bool errorsWasIgnored = info.ignoreErrors;
    info.currentContentOrder.add(CONTENT_ORDER_ZERO);
    defer {
        info.currentContentOrder.pop();
        info.ignoreErrors = errorsWasIgnored;
    };

    TEMP_ARRAY_N(TypeId, tempTypes, 5)

    bool perform_global_check = true;
    perform_global_check = !info.do_not_check_global_globals;

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
            if(now->type != ASTStatement::DECLARATION || !now->globalDeclaration)
                continue;
        }
        tempTypes.resize(0);
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
                auto ti = checkType(scope->scopeId, varname.assignString, now->location, &printedError);
                // NOTE: We don't care whether it's a pointer just that the type exists.
                if (!ti.isValid() && !printedError) {
                    ERR_SECTION(
                        ERR_HEAD2(now->location)
                        ERR_MSG("'"<<info.ast->getStringFromTypeString(varname.assignString)<<"' is not a type (statement).")
                        ERR_LINE2(now->location,"bad")
                    )
                } else {
                    // if(varname.arrayLength != 0){
                    //     auto ti = checkType(scope->scopeId, varname.assignString, now->location, &printedError);
                    // } else {
                        // If typeid is invalid we don't want to replace the invalid one with the type
                        // with the string. The generator won't see the names of the invalid types.
                        // now->typeId = ti;
                        varname.versions_assignType.set(currentPolyVersion, ti);
                    // }
                }
            }
        }

        if(now->type == ASTStatement::CONTINUE || now->type == ASTStatement::BREAK){
            // nothing
        } else if(now->type == ASTStatement::BODY || now->type == ASTStatement::DEFER){
            SignalIO result = checkRest(now->firstBody);
        } else if(now->type == ASTStatement::EXPRESSION){
            checkExpression(scope->scopeId, now->firstExpression, &tempTypes, false);
            // if(tempTypes.size()==0)
            //     tempTypes.add(AST_VOID);
        } else if(now->type == ASTStatement::RETURN){
            for(auto ret : now->arrayValues){
                tempTypes.resize(0);
                SignalIO result = checkExpression(scope->scopeId, ret, &tempTypes, false);
            }
        } else if(now->type == ASTStatement::IF){
            SignalIO result1 = checkExpression(scope->scopeId,now->firstExpression,&tempTypes, false);
            SignalIO result = checkRest(now->firstBody);
            if(now->secondBody){
                result = checkRest(now->secondBody);
            }
        } else if(now->type == ASTStatement::DECLARATION){
            if(!perform_global_check && now->globalDeclaration)
                continue; // We already checked globals

            // log::out << " " << now->varnames[0].name<<"\n";

            auto result = checkDeclaration(now, contentOrder, scope);
            if(result != SIGNAL_SUCCESS)
                continue;
            
            _TCLOG(log::out << "\n";)
        } else if(now->type == ASTStatement::WHILE){
            if(now->firstExpression) {
                // no expresion represents infinite loop
                SignalIO result1 = checkExpression(scope->scopeId, now->firstExpression, &tempTypes, false);
            }
            SignalIO result = checkRest(now->firstBody);
        } else if(now->type == ASTStatement::FOR){
            // DynamicArray<TypeId> temp{};
            SignalIO result1 = checkExpression(scope->scopeId, now->firstExpression, &tempTypes, false);
            SignalIO result=SIGNAL_FAILURE;

            Assert(now->varnames.size()==2);
            // auto& varname0 = now->varnames[0]; // it with sliced, nr with ranged
            // auto& varname1 = now->varnames[1]; // nr with sliced

            ScopeId varScope = now->firstBody->scopeId;
            
            auto bad_var = [&](IdentifierVariable* varinfo, const std::string& name) {
                if(!varinfo) {
                    ERR_SECTION(
                        ERR_HEAD2(now->location)
                        ERR_MSG("Cannot add variable '"<<name<<"' to use in for loop. Is it already defined?")
                        ERR_LINE2(now->location,"")
                    )
                }
            };
            
            bool reused_index = false;
            bool reused_item = false;
            
            auto iterinfo = info.ast->getTypeInfo(tempTypes.last());

            if(iterinfo&&iterinfo->astStruct){
                if(iterinfo->astStruct->name == "Slice"){
                    if(now->varnames[0].name.size() == 0)
                        now->varnames[0].name = "it";
                    if(now->varnames[1].name.size() == 0)
                        now->varnames[1].name = "nr";
                    auto& varnameIt = now->varnames[0];
                    auto& varnameNr = now->varnames[1];
    
                    auto varinfo_item = info.ast->addVariable(Identifier::LOCAL_VARIABLE, varScope, varnameIt.name, CONTENT_ORDER_ZERO, &reused_item);
                    varnameIt.identifier = varinfo_item;
                    
                    bad_var(varinfo_item, varnameIt.name);
                    
                    auto varinfo_index = info.ast->addVariable(Identifier::LOCAL_VARIABLE, varScope, varnameNr.name, CONTENT_ORDER_ZERO, &reused_index);
                    varnameNr.identifier = varinfo_index;
            
                    bad_var(varinfo_index, varnameNr.name);
                    
                    // Identifier* nrId = nullptr;
                    varinfo_index->versions_typeId.set(currentPolyVersion, AST_INT64);
                    varnameNr.versions_assignType.set(info.currentPolyVersion, AST_INT64);
                    
                    auto memdata = iterinfo->getMember("ptr");
                    auto itemtype = memdata.typeId;
                    itemtype.setPointerLevel(itemtype.getPointerLevel()-1);
                    varnameIt.versions_assignType.set(currentPolyVersion, itemtype);
                    
                    auto vartype = memdata.typeId;
                    if(!now->isPointer()){
                        vartype.setPointerLevel(vartype.getPointerLevel()-1);
                    }
                    varinfo_item->versions_typeId.set(currentPolyVersion, vartype);

                    SignalIO result = checkRest(now->firstBody);
                    continue;
                } else if(iterinfo->astStruct->name == "Range") {
                    if(now->varnames[0].name.size() == 0)
                        now->varnames[0].name = "nr";
                    auto& varnameNr = now->varnames[0];
                    auto varinfo_index = info.ast->addVariable(Identifier::LOCAL_VARIABLE, varScope, varnameNr.name, CONTENT_ORDER_ZERO, &reused_index);
                    varnameNr.identifier = varinfo_index;

                    bad_var(varinfo_index, varnameNr.name);
                    
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
                        varnameNr.versions_assignType.set(currentPolyVersion, inttype);
                        varinfo_index->versions_typeId.set(currentPolyVersion, inttype);

                        SignalIO result = checkRest(now->firstBody);
                        continue;
                    }
                }
            }
            std::string strtype = info.ast->typeToString(tempTypes.last());
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
            #ifdef gone
            if(now->varnames.size()==1 && now->alias.size()){
                auto& name = now->varnames[0].name;
                TypeId originType = checkType(scope->scopeId, name, now->location, nullptr);
                TypeId aliasType = info.ast->convertToTypeId(now->alias, scope->scopeId, true);
                if(originType.isValid() && !aliasType.isValid()){
                    TypeInfo* aliasInfo = info.ast->createType(now->alias, scope->scopeId);
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
                ScopeInfo* aliasScope = info.ast->findScope(now->alias,scope->scopeId, true);
                if(originScope && !aliasScope) {
                    ScopeInfo* scopeInfo = info.ast->getScope(scope->scopeId);
                    // TODO: Alias can't contain multiple namespaces. How to implement
                    //  that?
                    scopeInfo->nameScopeMap[now->alias] = originScope->id;

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
                    scopeInfo->sharedScopes.add(originInfo); // 'using' keyword
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
            #endif
        } else if(now->type == ASTStatement::TEST) {
            checkExpression(scope->scopeId, now->testValue, &tempTypes, false);
            tempTypes.resize(0);
            checkExpression(scope->scopeId, now->firstExpression, &tempTypes, false);
            tempTypes.resize(0);
        } else if(now->type == ASTStatement::TRY) {
            checkRest(now->firstBody);
            
            for(int i=0;i<now->switchCases.size();i++) {
                auto catch_expr = now->switchCases[i].caseExpr;
                auto catch_body = now->switchCases[i].caseBody;

                auto result = checkExpression(scope->scopeId, catch_expr, &tempTypes, false);
                if(result != SIGNAL_SUCCESS)
                    return result;

                if(tempTypes.size() != 1) {
                    ERR_SECTION(
                        ERR_HEAD2(catch_expr->location)
                        ERR_MSG("Expression should evaluate to one type.")
                        ERR_LINE2(catch_expr->location,"here")
                    )
                    return SIGNAL_FAILURE;
                }
                TypeId catch_type = tempTypes.last();
                TypeInfo* typeinfo = ast->getTypeInfo(catch_type);
                if(!AST::IsInteger(catch_type) && !typeinfo->astEnum) {
                    ERR_SECTION(
                        ERR_HEAD2(catch_expr->location)
                        ERR_MSG_COLORED("Type in catch was '"<<log::LIME<<ast->typeToString(catch_type)<<log::NO_COLOR<<"' but integer was expected. Although integers are accepted, using members from the '"<<log::LIME<<"ExceptionType"<<log::NO_COLOR<<"' enum is recommended.")
                        ERR_LINE2(catch_expr->location,"here")
                    )
                }

                // TODO: steal_element_into is thread safe (mutex behind the scenes)
                //    and we need it here when modifying types of AST from threads.
                //   BUT it would be nice if we could fix it up a little.
                QuickArray<TypeId> tmp{};
                now->versions_expressionTypes.steal_element_into(currentPolyVersion, tmp);
                tmp.add(tempTypes.last());
                now->versions_expressionTypes.steal_element_from(currentPolyVersion, tmp);

                tempTypes.resize(0);

                checkRest(catch_body);
            }
            if(now->secondBody) // finally body is optional
                checkRest(now->secondBody);

        } else if(now->type == ASTStatement::SWITCH) {
            // check switch expression
            checkExpression(scope->scopeId, now->firstExpression, &tempTypes, false);
            
            ASTEnum* astEnum = nullptr;
            Assert(tempTypes.size() == 1);
            if(tempTypes[0].isValid()) {
                auto typeInfo = info.ast->getTypeInfo(tempTypes[0]);
                Assert(typeInfo);
                if(typeInfo->astEnum) {
                    astEnum = typeInfo->astEnum;
                    // TODO: What happens if type is virtual or aliased? Will this code work?
                }
            }
            // TODO: This is scuffed...
            QuickArray<TypeId> tmp{};
            now->versions_expressionTypes.steal_element_into(currentPolyVersion, tmp);
            tmp.add(tempTypes.last());
            now->versions_expressionTypes.steal_element_from(currentPolyVersion, tmp);

            // now->versions_expressionTypes[info.currentPolyVersion].add(tempTypes.last());
            tempTypes.resize(0);
            
            int usedMemberCount = 0;
            TEMP_ARRAY(ASTExpression*, usedMembers);
            if(astEnum) {
                usedMembers.resize(astEnum->members.size());
            }
            
            // TODO: Enum switch statements should not be limited to just enum members.
            //  Constant expressions that evaluate to the same value as one of the members should
            //  count towards having all the members present in the switch. Unless enum members
            //  aren't forced to exist in the switch.
            
            bool allCasesUseEnumType = astEnum != nullptr;
            
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
                    checkExpression(scope->scopeId, it.caseExpr, &tempTypes, false);
                    tempTypes.resize(0);
                }
                
                checkRest(it.caseBody);
            }
            
            if(now->firstBody) {
                // default case
                checkRest(now->firstBody);
            } else {
                // not default case
                if(usedMemberCount != (int)usedMembers.size() && !astEnum->hasRule(ASTEnum::LENIENT_SWITCH)) {
                    ERR_SECTION(
                        ERR_HEAD2(now->location, ERROR_MISSING_ENUM_MEMBERS_IN_SWITCH)
                        ERR_MSG_LOG("The switch-statement is matching an enum value and requires all members to be present unless you specify a default case. You can add the annotation "<<log::GOLD<<"@lenient_switch"<<log::NO_COLOR<<" to the enum declaration to neglect this situation. These are the missing enum members: ")
                        int n = 0;
                        for (int i=0;i<astEnum->members.size();i++) {
                            auto& mem = astEnum->members[i];
                            if(!usedMembers[i]) {
                                if(n!=0)
                                    log::out << ", ";
                                log::out << log::GREEN << mem.name << log::NO_COLOR;
                                n++;
                            }
                        }
                        log::out << ".\n";
                        if(!allCasesUseEnumType) {
                            log::out << "Note that some cases use a non-enum member\n";
                        }
                        log::out << "\n";
                        
                        ERR_LINE2(now->location, "this switch")
                    )
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
        SignalIO result = checkRest(now);
    }
    return SIGNAL_SUCCESS;
}

void TypeCheckEnums(AST* ast, ASTScope* scope, Compiler* compiler) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Purple4);
    TyperContext info = {};
    info.init_context(compiler);

    _VLOG(log::out << log::BLUE << "Type check enums:\n";)
    // Check enums first since they don't depend on anything.
    SignalIO result = info.checkEnums(scope);
    info.compiler->options->compileStats.errors += info.errors;
}
SignalIO TypeCheckStructs(AST* ast, ASTScope* scope, Compiler* compiler, bool ignore_errors, bool* changed) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Purple4);
    TyperContext info = {};
    info.init_context(compiler);
    _VLOG(log::out << log::BLUE << "Type check structs:\n";)
    
    *changed = false;
    // An improvement here might be to create empty struct types first.
    // Any usage of pointers to structs won't cause failures since we know the size of pointers.
    // If a type isn't a pointer then we might fail if the struct hasn't been created yet.
    // You could implement some dependency or priority queue.
        
    // Content order for default values in struct members
    // Will this cause problems somehow?
    info.currentContentOrder.add(CONTENT_ORDER_MAX);
    
    info.incompleteStruct = false;
    info.completedStruct = false;
    info.showErrors = !ignore_errors;
    info.ignoreErrors = !info.showErrors;
    info.checkStructs(scope);

    // if(!info.showErrors)
    if(info.showErrors)
        info.compiler->options->compileStats.errors += info.errors;

    if(info.completedStruct) {
        *changed = true;
    }

    if(info.incompleteStruct) {
        return SIGNAL_FAILURE;
    }
    return SIGNAL_SUCCESS;
}
void TypeCheckFunctions(AST* ast, ASTScope* scope, Compiler* compiler, bool is_initial_import) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Purple4);
    TyperContext info = {};
    info.init_context(compiler);
    _VLOG(log::out << log::BLUE << "Type check functions:\n";)

    
    // We check function "header" first and then later, we check the body.
    // This is because bodies may reference other functions. If we check header and body at once,
    // a referenced function may not exist yet (this is certain if the body of two functions reference each other).
    // If so we would have to stop checking that body and continue later until that reference have been cleared up.
    // So instead of that mess, we check headers first to ensure that all references will work.
    SignalIO result = info.checkFunctions(scope);

    info.currentContentOrder.add(CONTENT_ORDER_ZERO);
    defer {
        info.currentContentOrder.pop();
    };

    // Check global declarations
    for(int contentOrder=0;contentOrder<scope->content.size();contentOrder++){
        
        if(scope->content[contentOrder].spotType!=ASTScope::STATEMENT)
            continue;

        info.currentContentOrder.last() = contentOrder;
        auto now = scope->statements[scope->content[contentOrder].index];

        if(now->type != ASTStatement::DECLARATION || !now->globalDeclaration)
            continue;

        for(auto& varname : now->varnames) {
            if(varname.assignString.isString()){
                bool printedError = false;
                auto ti = info.checkType(scope->scopeId, varname.assignString, now->location, &printedError);
                // NOTE: We don't care whether it's a pointer just that the type exists.
                if (!ti.isValid() && !printedError) {
                    ERR_SECTION(
                        ERR_HEAD2(now->location)
                        ERR_MSG("'"<<info.ast->getStringFromTypeString(varname.assignString)<<"' is not a type (statement).")
                        ERR_LINE2(now->location,"here somewhere")
                    )
                } else {
                    // if(varname.arrayLength != 0){
                    //     auto ti = checkType(scope->scopeId, varname.assignString, now->location, &printedError);
                    // } else {
                        // If typeid is invalid we don't want to replace the invalid one with the type
                        // with the string. The generator won't see the names of the invalid types.
                        // now->typeId = ti;
                        varname.versions_assignType[info.currentPolyVersion] = ti;
                    // }
                }
            }
        }
        info.inside_import_scope = true;
        auto result = info.checkDeclaration(now, contentOrder, scope);
        if(result != SIGNAL_SUCCESS)
            continue;
    }

    info.compiler->options->compileStats.errors += info.errors;
}

void TypeCheckBody(Compiler* compiler, ASTFunction* ast_func, FuncImpl* func_impl, ASTScope* import_scope) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Purple4);
    TyperContext info = {};
    info.init_context(compiler);
    
    info.do_not_check_global_globals = true;
    
    _VLOG(log::out << log::BLUE << "Type check functions:\n";)

    // log::out << "Check " << ast_func->name<<"\n";

    // Check rest will go through scopes and create polymorphic implementations if necessary.
    // This includes structs and functions.
    if(import_scope) {
        auto result = info.checkRest(import_scope);
    }

    info.do_not_check_global_globals = false;

    if(ast_func && ast_func->body) {
        // 1. ast_func may be nullptr if no main function was specified, the global scope is the main function if so.
        // 2. Native, imported or intrinsic functions does not have bodies and we cannot and should not check them.
        // log::out << "check "<<ast_func->name<<"\n";
        auto result = info.checkFuncImplScope(ast_func, func_impl);
    }

    info.compiler->options->compileStats.errors += info.errors;
    // return info.errors;
}
void TyperContext::init_context(Compiler* compiler) {
    this->compiler = compiler;
    ast = compiler->ast;
    reporter = &compiler->reporter;
    typeChecker = &compiler->typeChecker;
    scratch_allocator.init(0x10000);
}