#include "BetBat/TypeChecker.h"

#undef ERRT
#undef ERR
#undef ERRTYPE
#undef ERRTOKENS
#undef TOKENINFO
#undef LOGAT

// Remember to wrap macros in {} when using if or loops
// since macros have multiple statements.
#define ERR() info.errors++;engone::log::out << engone::log::RED <<"Typerror, "
#define ERRT(R) info.errors++;engone::log::out << engone::log::RED <<"TypeError "<<(R.firstToken.line)<<":"<<(R.firstToken.column)<<", "
#define ERRTYPE(R,LT,RT) ERRT(R) << "Type mismatch "<<info.ast->getTypeInfo(LT)->name<<" - "<<info.ast->getTypeInfo(RT)->name<<" "

#define LOGAT(R) R.firstToken.line <<":"<<R.firstToken.column

#define ERRTOKENS(R) log::out <<log::RED<< "LN "<<R.firstToken.line <<": "; R.print();log::out << "\n";

#define TOKENINFO(R) {std::string temp="";R.feed(temp);info.code->addDebugText(std::string("Ln ")+std::to_string(R.firstToken.line)+ ": ");info.code->addDebugText(temp+"\n");}

int CheckEnums(CheckInfo& info, ASTScope* scope){
    using namespace engone;
    Assert(scope);
    MEASURE;
    
    int error = true;
    ASTEnum* nextEnum = scope->enums;
    while(nextEnum){
        ASTEnum* aenum = nextEnum;
        nextEnum = nextEnum->next;
        
        TypeInfo* typeInfo = info.ast->createType(Token(aenum->name), scope->scopeId);
        if(typeInfo){
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
// int CheckType(CheckInfo& info, ASTScope* scope, TypeId& inOutType){
//     using namespace engone;
//     if(inOutType.isString()){
//         Token token = info.ast->getTokenFromTypeString(inOutType);
//         auto ti = info.ast->convertToTypeId(inOutType, scope->scopeId);
//         // auto ti = info.ast->getTypeInfo(scope->scopeId,Token(str));
//         if (ti.isValid()) {
//             inOutType = ti;
//         } else {
//             ERRT(expr->tokenRange) << "type "<< token << " does not exist\n";
//         }
//     }
// }
int CheckStructs(CheckInfo& info, ASTScope* scope) {
    using namespace engone;
    MEASURE;
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
                astStruct->state = ASTStruct::TYPE_CREATED;
                structInfo->astStruct = astStruct;   
            }
        } else if(astStruct->state != ASTStruct::TYPE_ERROR){
            structInfo = info.ast->convertToTypeInfo(Token(astStruct->name), scope->scopeId);   
        }
        
        if(astStruct->state == ASTStruct::TYPE_CREATED){
            Assert(structInfo);

            // log::out << "Evaluating "<<*astStruct->name<<"\n";
            astStruct->state = ASTStruct::TYPE_COMPLETE;
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
            //-- Check members
            bool polymorphic = astStruct->polyNames.size()!=0;
            for (int i = 0;i<(int)astStruct->members.size();i++) {
                auto& member = astStruct->members[i];
                auto& implMem = astStruct->baseImpl.members[i];
                // TypeInfo* typeInfo = 0;
                if(implMem.typeId.isString()){
                    Token tok = info.ast->getTokenFromTypeString(implMem.typeId);
                    // u32 plevel=0;
                    // Token raw = AST::TrimPointer(tok, &plevel);
                    auto id = info.ast->convertToTypeId(tok, scope->scopeId);
                    // TypeInfo* typeInfo = info.ast->getTypeInfo(scope->scopeId, raw);
                    if(!id.isValid()){
                        if(polymorphic)
                            continue;
                        
                        if(info.showErrors) {
                            ERRT(astStruct->tokenRange) << "type "<< tok << " in "<< astStruct->name<<"."<<member.name << " is not a type\n";
                        }
                        astStruct->state = ASTStruct::TYPE_CREATED;
                        break;
                    }
                    implMem.typeId = id;
                }
            }
            for (int i = 0;i<(int)astStruct->members.size();i++) {
                auto& member = astStruct->members[i];
                auto& implMem = astStruct->baseImpl.members[i];
                if(polymorphic){
                    continue;
                }
                i32 size = info.ast->getTypeSize(implMem.typeId);
                i32 asize = info.ast->getTypeAlignedSize(implMem.typeId);
                // log::out << " "<<typeInfo->name << " "<<typeInfo->size()<<"\n";
                if(size==0){
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
                    astStruct->state = ASTStruct::TYPE_CREATED;
                    break;
                }
                if(alignedSize<asize)
                    alignedSize = asize;

                /* OLD WAY TO ALIGN WHICH DOESN'T WORK
                */
                // int misalign = offset % size;
                // if(misalign!=0){
                //     offset+=size-misalign;
                // }

                
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
                    auto& implMema = astStruct->baseImpl.members[i+1];
                    i32 nsize = info.ast->getTypeSize(implMema.typeId);
                    // i32 asize = info.ast->getTypeAlignedSize(implMem.typeId);
                    int misalign = (offset + nsize) % size;
                    if(misalign!=0){
                        offset+=size-misalign;
                    }
                }
            }

            if(astStruct->state != ASTStruct::TYPE_COMPLETE) {
                // log::out << log::RED << "Evaluation failed\n";
                info.completedStructs = false;
            } else if (polymorphic){
                _TC_LOG(log::out << astStruct->name << " was polymorphic and cannot be evaluated yet\n";)
            }else {
                // align the end so the struct can be stacked together
                if(offset != 0){
                    astStruct->baseImpl.alignedSize = alignedSize;
                    int misalign = offset % alignedSize;
                    if(misalign!=0){
                        offset+=alignedSize-misalign;
                    }
                }
                astStruct->baseImpl.size = offset;
                _TC_LOG(log::out << astStruct->name << " was evaluated to "<<offset<<" bytes\n";)
                info.anotherTurn=true;
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
    } else if(expr->typeId == AST_SIZEOF) {
        if(!expr->name){
            ERRT(expr->tokenRange) << "string was null at "<<expr->tokenRange.firstToken<<" (compiler bug)\n";
            ERRTOKENS(expr->tokenRange)
            return false;
        }
        auto ti = info.ast->convertToTypeId(*expr->name, scope->scopeId);
        int size = info.ast->getTypeSize(ti);
        expr->typeId = AST_UINT32;
        expr->i64Value = size;
    }
    if(expr->typeId.isString()){
        Token token = info.ast->getTokenFromTypeString(expr->typeId);
        auto ti = info.ast->convertToTypeId(expr->typeId, scope->scopeId);
        // auto ti = info.ast->getTypeInfo(scope->scopeId,Token(str));
        if (ti.isValid()) {
            expr->typeId = ti;
        } else {
            ERRT(expr->tokenRange) << "type "<< token << " does not exist\n";
        }
    }
    if(expr->castType.isString()){
        Token str = info.ast->getTokenFromTypeString(expr->castType);
        auto ti = info.ast->convertToTypeId(expr->castType, scope->scopeId);
        // auto ti = info.ast->getTypeInfo(scope->scopeId,Token(str));
        if (ti.isValid()) {
            expr->castType = ti;
        } else {
            ERRT(expr->tokenRange) << "type "<<str << " does not exist\n";
        }
    }
    
    if(expr->next)
        CheckExpression(info, scope, expr->next);
        
    return true;
}

int CheckRest(CheckInfo& info, ASTScope* scope);
int CheckFunction(CheckInfo& info, ASTScope* scope, ASTFunction* func, ASTStruct* parentStruct){
    MEASURE;
    int offset = 0; // offset starts before call frame (fp, pc)
    int firstSize = 0;
    // Based on 8-bit alignment. The last argument must be aligned by it.
    for(int i=0;i<(int)func->arguments.size();i++){
        auto& arg = func->arguments[i];
        // TypeInfo* typeInfo = 0;
        if(arg.typeId.isString()){
            Token str = info.ast->getTokenFromTypeString(arg.typeId);
            auto id = info.ast->convertToTypeId(arg.typeId, scope->scopeId);
            
            if(id.isValid()){
                arg.typeId = id;
            }else{
                ERRT(func->tokenRange) << str <<" was void (type doesn't exist or you used void which isn't allowed)\n";
            }
        } else {
            // typeInfo = info.ast->getTypeInfo(arg.typeId);
        }
            
        int size = info.ast->getTypeSize(arg.typeId);
        int asize = info.ast->getTypeAlignedSize(arg.typeId);
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
            Token str = info.ast->getTokenFromTypeString(ret.typeId);
            auto ti = info.ast->convertToTypeId(ret.typeId, scope->scopeId);
            if(ti.isValid()){
                ret.typeId = ti;
            }else{
                // TODO: fix location?
                ERRT(func->tokenRange) << str<<" is not a type (function)\n";
            }
            
        } else {
            // typeInfo = info.ast->getTypeInfo(ret.typeId);
        }
        int size = info.ast->getTypeSize(ret.typeId);
        int asize = info.ast->getTypeAlignedSize(ret.typeId);
        
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
            Token str = info.ast->getTokenFromTypeString(now->typeId);
            // NOTE: We don't care whether it's a pointer just that the type exists.
            TypeInfo* ti = info.ast->convertToTypeInfo(str, scope->scopeId);
            if(!ti){
                ERRT(now->tokenRange) << str<<" is not a type (statement)\n";
            }else {
                if(ti->astStruct && ti->astStruct->polyNames.size()!=0){
                    ERRT(now->tokenRange) << str<<" is polymorphic. You must specify extra types like this: Struct<i32>\n";
                }
                now->typeId = ti->id;
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