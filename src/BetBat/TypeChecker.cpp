#include "BetBat/TypeChecker.h"

#undef ERRT
#undef ERR
#undef ERRTYPE
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
    
    int error = true;
    ASTEnum* nextEnum = scope->enums;
    while(nextEnum){
        ASTEnum* aenum = nextEnum;
        nextEnum = nextEnum->next;
        
        TypeInfo* typeInfo = info.ast->getTypeInfo(scope->scopeId, aenum->name,false,true);
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
int CheckStructs(CheckInfo& info, ASTScope* scope) {
    using namespace engone;
    
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
            astStruct->state = ASTStruct::TYPE_CREATED;
            structInfo = info.ast->getTypeInfo(scope->scopeId, astStruct->name,false,true);
            if(!structInfo){
                ERRT(astStruct->tokenRange) << astStruct->name<<" is taken\n";
            } else {
                structInfo->astStruct = astStruct;   
            }
        } else {
            structInfo = info.ast->getTypeInfo(scope->scopeId, astStruct->name);   
        }
        
        if(astStruct->state == ASTStruct::TYPE_CREATED){
            if(!structInfo)
                structInfo = info.ast->getTypeInfo(scope->scopeId, astStruct->name);
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
            for(auto& member : astStruct->members){
                TypeInfo* typeInfo = 0;
                if(member.typeId & AST::STRING_BIT){
                    typeInfo = info.ast->getTypeInfo(scope->scopeId, info.ast->getTypeString(member.typeId));
                    member.typeId = typeInfo->id;
                } else {
                    typeInfo = info.ast->getTypeInfo(member.typeId);
                }
                
                // log::out << " "<<typeInfo->name << " "<<typeInfo->size()<<"\n";
                if(!typeInfo || typeInfo->size()==0){
                    if(member.typeId == structInfo->id){
                        if(info.showErrors) {
                            ERRT(astStruct->tokenRange) << "member "<<member.name << "'s type uses the parent struct. (recursive struct does not work)\n";
                        }
                        // TODO: phrase the error message in a better way
                        // TOOD: print some column and line info.
                        // TODO: message is printed twice
                        // log::out << log::RED <<"Member "<< member.name <<" in struct "<<*astStruct->name<<" cannot be it's own type\n";
                    } else {
                        if(info.showErrors) {
                            ERRT(astStruct->tokenRange) << "type "<<typeInfo->name << " in "<< astStruct->name<<"."<<member.name << " is not a type\n";
                        }
                    }
                    astStruct->state = ASTStruct::TYPE_CREATED;
                    break;
                }
                if(alignedSize<typeInfo->alignedSize())
                    alignedSize = typeInfo->alignedSize();
                int misalign = offset % typeInfo->size();
                if(misalign!=0){
                    offset+=typeInfo->size()-misalign;
                }
                // log::out << " "<<offset<<": "<<member.name<<" ("<<typeInfo->size()<<" bytes)\n";
                member.offset = offset;
                offset+=typeInfo->size();
            }

            if(astStruct->state != ASTStruct::TYPE_COMPLETE) {
                // log::out << log::RED << "Evaluation failed\n";
                info.completedStructs = false;
            } else {
                // align the end so the struct can be stacked together
                astStruct->alignedSize = alignedSize;
                int misalign = offset % alignedSize;
                if(misalign!=0){
                    offset+=alignedSize-misalign;
                }
                astStruct->size = offset;
                log::out << astStruct->name << " was evaluated to "<<offset<<" bytes\n";
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
    Assert(scope)
    Assert(expr)
    
    if(expr->left)
        CheckExpression(info,scope, expr->left);
    if(expr->right)
        CheckExpression(info,scope, expr->right);
    
    if(expr->typeId & AST::STRING_BIT){
        const std::string& str = info.ast->getTypeString(expr->typeId);
        auto ti = info.ast->getTypeInfo(scope->scopeId,str,true);
        if(ti){
            expr->typeId = ti->id;
        }else{
            ERRT(expr->tokenRange) << "type "<<str << " does not exist\n";
        }
    }
    if(expr->castType & AST::STRING_BIT){
        const std::string& str = info.ast->getTypeString(expr->castType);
        auto ti = info.ast->getTypeInfo(scope->scopeId,str,true);
        if(ti){
            expr->castType = ti->id;
        }else{
            ERRT(expr->tokenRange) << "type "<<str << " does not exist\n";
        }
    }
        
    return true;
}
int CheckRest(CheckInfo& info, ASTScope* scope){
    // arguments
    ASTFunction* nextFunc = scope->functions;
    while(nextFunc){
        ASTFunction* func = nextFunc;
        nextFunc = nextFunc->next;

        // log::out << "Eval func "<<*func->name<<"\n";
        int offset = 0; // offset starts before call frame (fp, pc)
        int firstSize = 0;
        // for(int i=func->arguments.size()-1;i>=0;i--){
        // Based on 8-bit alignment. The last argument must be aligned by it.
        for(int i=0;i<(int)func->arguments.size();i++){
            auto& arg = func->arguments[i];
            TypeInfo* typeInfo = 0;
            if(arg.typeId & AST::STRING_BIT){
                const std::string& str = info.ast->getTypeString(arg.typeId);
                typeInfo = info.ast->getTypeInfo(scope->scopeId, str);
                if(typeInfo){
                    arg.typeId = typeInfo->id;
                }else{
                    ERRT(func->tokenRange) << str <<" is not a type\n";
                }
            } else {
                typeInfo = info.ast->getTypeInfo(arg.typeId);
            }
                
            int size = typeInfo->size();
            int asize = typeInfo->alignedSize();
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
            TypeInfo* typeInfo = info.ast->getTypeInfo(arg.typeId);
            int size = typeInfo->size();
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
            TypeInfo* typeInfo = 0;
            if(ret.typeId & AST::STRING_BIT){
                const std::string& str = info.ast->getTypeString(ret.typeId);
                typeInfo = info.ast->getTypeInfo(scope->scopeId, str);
                if(typeInfo){
                    ret.typeId = typeInfo->id;
                }else{
                    // TODO: fix location?
                    ERRT(func->tokenRange) << str<<" is not a type\n";
                }
                
            } else {
                typeInfo = info.ast->getTypeInfo(ret.typeId);
            }
            int size = typeInfo->size();
            int asize = typeInfo->alignedSize();
            
            if ((-offset)%asize != 0){
                offset -= asize - (-offset)%asize;
            }
            offset -= size; // size included in the offset going negative on the stack
            ret.offset = offset;
            // log::out << " Ret "<<ret.offset << ": ["<<size<<"]\n";
        }
        func->returnSize = -offset;
        // size of return types doesn't have to match any alignment
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
            
        if(now->typeId & AST::STRING_BIT){
            const std::string& str = info.ast->getTypeString(now->typeId);
            TypeInfo* ti = info.ast->getTypeInfo(scope->scopeId, str, true);
            if(!ti){
                ERRT(now->tokenRange) << str<<" is not a type\n";
            }else {
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
    CheckInfo info = {};
    info.ast = ast;

    _VLOG(log::out << log::BLUE << "Typechecking:\n";)

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
            return false;
        }
    }
    result = CheckRest(info, scope);
    
    return info.errors;
}