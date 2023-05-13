#include "BetBat/Generator.h"

#undef ERRT
#undef ERR

#define ERRT(T) info.errors++;engone::log::out << engone::log::RED <<"GenError "<<(T.line)<<":"<<(T.column)<<", "
#define ERR() info.errors++;engone::log::out << engone::log::RED <<"GenError, "

#define GEN_SUCCESS 0
#define GEN_ERROR 1
GenInfo::Variable* GenInfo::addVariable(const std::string& name){
    using namespace engone;
    // log::out << "count "<<identifiers.size()<<"\n";
    auto pair = identifiers.find(name);
    if(pair!=identifiers.end())
        return 0;
    
    // std::string nam = "a";
    
    // fprintf(stderr,"hoax1\n");
    identifiers[name] = {};
    // log::out << "inserted "<<identifiers.size()<<"\n";
    
    // fprintf(stderr,"hoax2\n");
    
    auto pair2 = identifiers.find(name);
    if(pair2==identifiers.end()){
        // fprintf(stderr,"eh what\n");
        return 0;
    }
    
    auto id = &pair2->second;
    id->type = Identifier::VAR;
    id->index = variables.size();
    variables.push_back({});
    return &variables[id->index];
}
GenInfo::Function* GenInfo::addFunction(const std::string& name){
    using namespace engone;
    auto pair = identifiers.find(name);
    if(pair!=identifiers.end())
        return 0;
    
    identifiers[name] = {};
    // log::out << "inserted "<<identifiers.size()<<"\n";
    
    auto pair2 = identifiers.find(name);
    if(pair2==identifiers.end()){
        // fprintf(stderr,"eh func what\n");
        return 0;
    }
    
    auto id = &pair2->second;
    id->type = Identifier::FUNC;
    id->index = functions.size();
    functions.push_back({});
    return &functions[id->index];
}
GenInfo::Identifier* GenInfo::getIdentifier(const std::string& name){
    auto pair = identifiers.find(name);
    if(pair!=identifiers.end())
        return &pair->second;
    return 0;
}
GenInfo::Variable* GenInfo::getVariable(int index){
    // TODO: bound check
    return &variables[index];
}
GenInfo::Function* GenInfo::getFunction(int index){
    // TODO: bound check
    return &functions[index];
}
void GenInfo::removeIdentifier(const std::string& name){
    auto pair = identifiers.find(name);
    if(pair!=identifiers.end()){
        identifiers.erase(pair);
    }
}
// Will perform cast on float and integers with pop, cast, push
bool PerformSafeCast(GenInfo& info, TypeId from, TypeId to) {
    if(from==AST_FLOAT32&&AST::IsInteger(to)){
        info.code->add({BC_POP,BC_REG_RAX});
        info.code->add({BC_CASTI64,BC_REG_RAX,BC_REG_RAX});
        info.code->add({BC_PUSH,BC_REG_RAX});
        return true;
    }
    if(AST::IsInteger(from)&&to==AST_FLOAT32){
        info.code->add({BC_POP,BC_REG_RAX});
        info.code->add({BC_CASTF32,BC_REG_RAX,BC_REG_RAX});
        info.code->add({BC_PUSH,BC_REG_RAX});
        return true;
    }
    if(!AST::IsInteger(from)||!AST::IsInteger(to))
        return from==to;
    
    return true; // allow cast between unsigned/signed numbers of all sizes. It's the most convenient.
    
    // bool fromSigned = AST::IsSigned(from);
    // bool toSigned = AST::IsSigned(to);
    
    // if(fromSigned&&!toSigned)
    //     return false;
    
    // if(fromSigned==toSigned){
    //     return to>=from;   
    // }
    // if(!fromSigned&&toSigned){
    //     return from-AST_UINT8 < to-AST_INT8;
    // }
    // Assert(("IsSafeCast not handled case",0))
    // return false;
}
bool IsSafeCast(TypeId from, TypeId to) {
    if(!AST::IsInteger(from)||!AST::IsInteger(to))
        return from==to;
    
    return true; // allow cast between unsigned/signed numbers of all sizes. It's the most convenient.
    
    // bool fromSigned = AST::IsSigned(from);
    // bool toSigned = AST::IsSigned(to);
    
    // if(fromSigned&&!toSigned)
    //     return false;
    
    // if(fromSigned==toSigned){
    //     return to>=from;   
    // }
    // if(!fromSigned&&toSigned){
    //     return from-AST_UINT8 < to-AST_INT8;
    // }
    // Assert(("IsSafeCast not handled case",0))
    // return false;
}
/*
##################################
    Generation functions below
##################################
*/
int GenerateExpression(GenInfo& info, ASTExpression* expression, TypeId* outTypeId){
    using namespace engone;
    _GLOG(FUNC_ENTER)
    Assert(expression)
    if(expression->isValue){
        // data type
        if(expression->typeId>=AST_UINT8 && expression->typeId<=AST_INT64){
            i32 val = expression->i64Value;
            
            // TODO: immediate only allows 32 bits. but what about larger values?
            info.code->addDebugText("  expr push int");
            info.code->add({BC_LI,BC_REG_RAX});
            info.code->addIm(val);
            info.code->add({BC_PUSH,BC_REG_RAX});
        } else if(expression->typeId==AST_BOOL8){
            bool val = expression->b8Value;
            
            info.code->addDebugText("  expr push bool");
            info.code->add({BC_LI,BC_REG_RAX});
            info.code->addIm(val);
            info.code->add({BC_PUSH,BC_REG_RAX});
        } else if(expression->typeId==AST_FLOAT32){
            float val = expression->f32Value;
            
            info.code->addDebugText("  expr push float");
            info.code->add({BC_LI,BC_REG_RAX});
            info.code->addIm(*(u32*)&val);
            info.code->add({BC_PUSH,BC_REG_RAX});
        } else if(expression->typeId==AST_VAR){
            // check data type and get it
            auto id = info.getIdentifier(*expression->name);
            if(id && id->type == GenInfo::Identifier::VAR){
                auto var = info.getVariable(id->index);
                // TODO: check data type?
                // fp + offset
                // TODO: what about struct
                _GLOG(log::out<<" expr var push "<<*expression->name<<"\n";)
                    
                char buf[100];
                int len = sprintf(buf,"  expr push %s",expression->name->c_str());
                info.code->addDebugText(buf,len);
                TypeInfo* typeInfo = info.ast->getTypeInfo(var->typeId);
                if(!typeInfo->astStruct){
                    info.code->add({BC_LI,BC_REG_RAX});
                    info.code->addIm(var->frameOffset);
                    info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RAX, BC_REG_RAX});
                    info.code->add({BC_MOV_MR,BC_REG_RAX,BC_REG_RAX});
                    info.code->add({BC_PUSH,BC_REG_RAX});
                } else {
                    auto& members = typeInfo->astStruct->members;
                    for(int i=(int)members.size()-1;i>=0;i--){
                        auto& member = typeInfo->astStruct->members[i];
                        log::out<<"  member "<< member.name<<"\n";
                        info.code->add({BC_LI,BC_REG_RAX});
                        info.code->addIm(var->frameOffset+i*8); // TODO: don't always do 8
                        info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RAX, BC_REG_RAX});
                        info.code->add({BC_MOV_MR,BC_REG_RAX,BC_REG_RAX});
                        info.code->add({BC_PUSH,BC_REG_RAX});
                    }
                }
                
                *outTypeId = var->typeId;
                return GEN_SUCCESS;
            }else{
                ERRT(expression->tokenRange.firstToken) << expression->tokenRange.firstToken<<" is undefined\n";
                // log::out << log::RED<<"var "<<*expression->varName<<" undefined\n";   
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }
        }  else if(expression->typeId==AST_FNCALL){
            // check data type and get it
            auto id = info.getIdentifier(*expression->name);
            if(id && id->type == GenInfo::Identifier::FUNC){
                auto func = info.getFunction(id->index);
                
                ASTExpression* argt = expression->left;
                if(argt)
                    _GLOG(log::out<<"push arguments\n");
                int index = -1;
                while (argt){
                    ASTExpression* arg = argt;
                    argt = argt->next;
                    index++;
                    TypeId dt=0;
                    int result = GenerateExpression(info,arg,&dt);
                    if(result!=GEN_SUCCESS){
                        continue;   
                    }
                    if(index>=(int)func->astFunc->arguments.size()){
                        // ERR() << "To many arguments! func:"<<*expression->funcName<<" max: "<<func->astFunc->arguments.size()<<"\n";
                        continue;
                    }

                    _GLOG(log::out<<" pushed "<<func->astFunc->arguments[index].name<<"\n";)
                    
                    if(!IsSafeCast(dt,func->astFunc->arguments[index].typeId)){ // implicit conversion
                    // if(func->astFunc->arguments[index].typeId!=dt){ // strict, no conversion
                        ERRT(arg->tokenRange.firstToken) << "Type mismatch (fncall): "<<*info.ast->getTypeId(dt)<<" - "<<*info.ast->getTypeId(func->astFunc->arguments[index].typeId)<<"\n";
                        continue;
                    }
                    // values are already pushed to the stack
                }
                
                index++; // incremented to get argument count
                if(index!=(int)func->astFunc->arguments.size()){
                    ERRT(expression->tokenRange.firstToken) << "Found "<<index<<" arguments but "<<*expression->name<<" requires "<<func->astFunc->arguments.size()<<"\n";   
                }
                if(func->address==0){
                    ERR() << "func address was invalid\n";
                }
                
                char buf[100];
                int len = sprintf(buf,"  call %s",expression->name->c_str());
                info.code->addDebugText(buf,len);
                info.code->add({BC_LI,BC_REG_RAX});
                info.code->addIm(func->address); // may be invalid if function exists after call
                info.code->add({BC_CALL,BC_REG_RAX});
                
                // return types?
                if(func->astFunc->returnTypes.empty())
                    *outTypeId = AST_VOID;
                else {
                    *outTypeId = func->astFunc->returnTypes[0];
                    // pop arguments
                    if(expression->left){
                        _GLOG(log::out<<"pop arguments\n");
                        info.code->add({BC_LI,BC_REG_RAX});
                        info.code->addIm(index*8);
                        info.code->add({BC_ADDI,BC_REG_SP,BC_REG_RAX,BC_REG_SP});
                    }
                    if(func->astFunc->returnTypes.size()!=0)
                        _GLOG(log::out << "extract return values\n";)
                    // move return values
                    int offset = 0;
                    for(int i=0;i<(int)func->astFunc->returnTypes.size();i++){
                        TypeId typeId = func->astFunc->returnTypes[i];
                        TypeInfo* typeInfo = info.ast->getTypeInfo(typeId);
                        if(!typeInfo->astStruct){
                            // TODO: can't use index for arguments, structs have different sizes
                            log::out<<" extract "<< *info.ast->getTypeId(typeId)<<"\n";
                            info.code->add({BC_LI,BC_REG_RAX});
                            info.code->addIm(-index*8 - GenInfo::ARG_OFFSET - 8 + offset);
                            // index*8 for arguments, ARG_OFFSET for pc and fp, (i+1)*8 is where the return values are
                            info.code->add({BC_ADDI, BC_REG_SP, BC_REG_RAX, BC_REG_RAX});
                            info.code->add({BC_MOV_MR, BC_REG_RAX, BC_REG_RAX});
                            info.code->add({BC_PUSH, BC_REG_RAX});
                            // offset -= 8;
                            // don't need to change offset since push will increment stack pointer
                        }else{
                            log::out<<" extract "<< *info.ast->getTypeId(typeId)<<"\n";
                            for(int j=typeInfo->astStruct->members.size()-1;j>=0;j--){
                            // for(int j=0;j<(int)typeInfo->astStruct->members.size();j++){
                                auto& member = typeInfo->astStruct->members[j];
                                log::out<<"  member "<< member.name<<"\n";
                                info.code->add({BC_LI,BC_REG_RAX});
                                info.code->addIm(-index*8 - GenInfo::ARG_OFFSET - 8 + offset);
                                // index*8 for arguments, ARG_OFFSET for pc and fp, (i+1)*8 is where the return values are
                                info.code->add({BC_ADDI, BC_REG_SP, BC_REG_RAX, BC_REG_RAX});
                                info.code->add({BC_MOV_MR, BC_REG_RAX, BC_REG_RAX});
                                info.code->add({BC_PUSH, BC_REG_RAX});
                                // offset -= 8;
                                // don't need to change offset since push will increment stack pointer
                            }
                        }
                    }
                }
                return GEN_SUCCESS;
            } else{
                ERRT(expression->tokenRange.firstToken) << expression->tokenRange.firstToken<<" is undefined\n";
                // log::out << log::RED<<"var "<<*expression->varName<<" undefined\n";   
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }
        } else if(expression->typeId==AST_NULL){
            info.code->addDebugText("  expr push null");
            info.code->add({BC_LI,BC_REG_RAX});
            info.code->addIm(0);
            info.code->add({BC_PUSH,BC_REG_RAX});
            
            TypeInfo* typeInfo = info.ast->getTypeInfo("void*");
            *outTypeId = typeInfo->id;
            return GEN_SUCCESS;
        } else {
            // info.code->add({BC_PUSH,BC_REG_RAX}); // push something so the stack stays synchronized, or maybe not?
            ERRT(expression->tokenRange.firstToken) << expression->tokenRange.firstToken<<" is an unknown data type\n";
            // log::out <<  log::RED<<"GenExpr: data type not implemented\n";
            *outTypeId = AST_VOID;
            return GEN_ERROR;
        }
        *outTypeId = expression->typeId;
    }else{
        // pop $rdx
        // pop $rax
        // addi $rax $rdx $rax
        // push $rax
        
        TypeId ltype = AST_VOID;
        if(expression->typeId==AST_REFER){
            ASTExpression* expr = expression->left;
            if(expr->typeId!=AST_VAR){
                ERRT(expr->tokenRange.firstToken) << expr->tokenRange.firstToken<<", can only reference a variable\n";
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }
            auto id = info.getIdentifier(*expr->name);
            if(id && id->type == GenInfo::Identifier::VAR){
                auto var = info.getVariable(id->index);
                // TODO: check data type?
                info.code->add({BC_LI,BC_REG_RAX});
                info.code->addIm(var->frameOffset);
                info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RAX, BC_REG_RAX}); // fp + offset
                info.code->add({BC_PUSH,BC_REG_RAX});
                
                // new pointer data type
                auto dtname = info.ast->getTypeId(var->typeId);
                std::string temp = *dtname;
                temp += "*";
                auto id = info.ast->getTypeId(temp);
                
                *outTypeId = id;
            } else {
                // TODO: identifier may be defined but not a variable, give proper message
                ERRT(expr->tokenRange.firstToken) << expr->tokenRange.firstToken<<" is undefined\n";
                // log::out << log::RED<<"var "<<*expression->varName<<" undefined\n";   
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }
        }else if(expression->typeId==AST_DEREF){
            int err = GenerateExpression(info,expression->left,&ltype);
            if(err==GEN_ERROR) return err;

            if(!AST::IsPointer(ltype)){
                ERRT(expression->left->tokenRange.firstToken) << expression->left->tokenRange.firstToken<<", can only derefence a pointer variable\n";
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }
            info.code->add({BC_POP,BC_REG_RBX});
            info.code->add({BC_MOV_MR,BC_REG_RBX,BC_REG_RAX});
            info.code->add({BC_PUSH,BC_REG_RAX});

            auto dtname = info.ast->getTypeId(ltype);
            std::string temp = *dtname;
            temp.pop_back(); // pop *
            auto id = info.ast->getTypeId(temp);
            
            *outTypeId = id;
            if(id==AST_VOID){
                ERRT(expression->tokenRange.firstToken) << "cannot dereference *void\n";
            }
        }else if(expression->typeId==AST_NOT){
            int err = GenerateExpression(info,expression->left,&ltype);
            if(err==GEN_ERROR) return err;
            
            info.code->add({BC_POP,BC_REG_RAX});
            info.code->add({BC_NOTB,BC_REG_RAX,BC_REG_RAX});
            info.code->add({BC_PUSH,BC_REG_RAX});
            
            *outTypeId = ltype;
        }else if(expression->typeId==AST_CAST){
            int err = GenerateExpression(info,expression->left,&ltype);
            if(err==GEN_ERROR) return err;
            
            TypeId castType = expression->castType;
            
            if((AST::IsInteger(castType) && AST::IsInteger(ltype))
                ||(AST::IsPointer(castType)&& AST::IsPointer(ltype))
                ||(AST::IsPointer(castType)&& (ltype == (TypeId)AST_INT64||ltype==(TypeId)AST_UINT64||ltype==(TypeId)AST_INT32))
                ||((castType == (TypeId)AST_INT64||castType==(TypeId)AST_UINT64||ltype==(TypeId)AST_INT32) && AST::IsPointer(ltype))
            ){
                // data is fine as it is, just change the data type
            } else if(AST::IsInteger(castType)&&ltype==AST_FLOAT32){
                info.code->add({BC_POP,BC_REG_RAX});
                info.code->add({BC_CASTI64,BC_REG_RAX,BC_REG_RAX});
                info.code->add({BC_PUSH,BC_REG_RAX});
            }else if(castType == AST_FLOAT32 && AST::IsInteger(ltype)){
                info.code->add({BC_POP,BC_REG_RAX});
                info.code->add({BC_CASTF32,BC_REG_RAX,BC_REG_RAX});
                info.code->add({BC_PUSH,BC_REG_RAX});
            } else {
                ERRT(expression->tokenRange.firstToken) << "cannot cast "<<*info.ast->getTypeId(ltype) << " to "<<*info.ast->getTypeId(castType)<<"\n";
                *outTypeId = ltype; // ltype since cast failed
                return GEN_ERROR;
            }
            
            //  float f = 7.29;
            // u32 raw = *(u32*)&f;
            // log::out << " "<<(void*)raw<<"\n";
            
            // u32 sign = raw>>31;
            // u32 expo = ((raw&0x80000000)>>(23));
            // u32 mant = ((raw&&0x80000000)>>(23));
            // log::out << sign << " "<<expo << " "<<mant<<"\n";
            
            *outTypeId = castType; // NOTE: we use castType and not ltype
        }else if(expression->typeId==AST_PROP){
            
            // TODO: if propfrom is a variable we can directly take the member from it
            //  instead of pushing struct to the stack and then getting it.
            TypeId exprId;
            int result = GenerateExpression(info,expression->left,&exprId);
            if(result!=GEN_SUCCESS) return result;
            
            TypeInfo* typeInfo = info.ast->getTypeInfo(exprId);
            if(!typeInfo->astStruct){
                *outTypeId = AST_VOID;
                ERRT(expression->tokenRange.firstToken) << "property access only works on structs. "<<*info.ast->getTypeId(exprId)<<" isn't one\n";
                return GEN_ERROR;
            }
            auto& members = typeInfo->astStruct->members;
            int index=-1;
            for(int i=0;i<(int)members.size();i++){
                if(members[i].name == *expression->name){
                    index = i;
                    break;
                }
            }
            if(index==-1){
                *outTypeId = AST_VOID;
                ERRT(expression->tokenRange.firstToken) << *expression->name<<" is not a member in "<<*info.ast->getTypeId(ltype)<<"\n";
                return GEN_ERROR;
            }
            
            ltype = members[index].typeId;
            
            for(int i=0;i<(int)members.size();i++){
                if(i==index)
                    info.code->add({BC_POP,BC_REG_RAX});
                else
                    info.code->add({BC_POP,BC_REG_RDX});
            }
            info.code->add({BC_PUSH,BC_REG_RAX});
            
            *outTypeId = ltype;
        }else if(expression->typeId==AST_INITIALIZER){
            
            TypeInfo* structInfo = info.ast->getTypeInfo(*expression->name);
            if(!structInfo||!structInfo->astStruct){
                ERRT(expression->tokenRange.firstToken) << "cannot do initializer on non-struct "<<log::GOLD<<*expression->name<<"\n";
                return GEN_ERROR;   
            }
            
            // store expressions in a list so we can iterate in reverse
            // TODO: parser could arrange the expressions in reverse.
            //   it would probably be easier since it constructs the nodes
            //   and has a little more context and control over it.
            std::vector<ASTExpression*> exprs;
            ASTExpression* nextExpr=expression->left;
            while(nextExpr){
                exprs.push_back(nextExpr);
                nextExpr = nextExpr->next;
            }

            int index = (int)exprs.size();
            // int index=-1;
            while(index>0){
                index--;
                ASTExpression* expr = exprs[index];
                TypeId exprId;
                int result = GenerateExpression(info,expr,&exprId);
                if(result!=GEN_SUCCESS) return result;
                
                if(index>=(int)structInfo->astStruct->members.size()){
                    // ERR() << "To many arguments! func:"<<*expression->funcName<<" max: "<<func->astFunc->arguments.size()<<"\n";
                    continue;
                }
                if(!PerformSafeCast(info,exprId,structInfo->astStruct->members[index].typeId)){ // implicit conversion
                // if(func->astFunc->arguments[index].typeId!=dt){ // strict, no conversion
                    ERRT(expr->tokenRange.firstToken) << "Type mismatch (initializer): "<<*info.ast->getTypeId(exprId)<<" - "<<*info.ast->getTypeId(structInfo->astStruct->members[index].typeId)<<"\n";
                    continue;
                }
            }
            if((int)exprs.size()!=(int)structInfo->astStruct->members.size()){
                ERRT(expression->tokenRange.firstToken) << "Found "<<index<<" arguments but "<<*expression->name<<" requires "<<structInfo->astStruct->members.size()<<"\n";   
                // return GEN_ERROR;
            }
            
            *outTypeId = structInfo->id;
        }else if(expression->typeId==AST_FROM_NAMESPACE){
            TypeInfo* typeInfo = info.ast->getTypeInfo(*expression->name);
            if(!typeInfo||!typeInfo->astEnum){
                ERRT(expression->tokenRange.firstToken) << "cannot access "<<(expression->member?*expression->member:"?")<<" from non-enum "<<*expression->name<<"\n";
                return GEN_ERROR;
            }
            Assert(expression->member)
            int index=-1;
            for(int i=0;i<(int)typeInfo->astEnum->members.size();i++){
                if(typeInfo->astEnum->members[i].name == *expression->member) {
                    index = i;
                    break;
                }
            }
            if(index!=-1){
                info.code->add({BC_LI,BC_REG_RAX});
                info.code->addIm(typeInfo->astEnum->members[index].id);
                info.code->add({BC_PUSH,BC_REG_RAX});
                *outTypeId = AST_INT32;   
            }
        }else{
            // basic operations
            TypeId rtype;
            int err = GenerateExpression(info,expression->left,&ltype);
            if(err==GEN_ERROR) return err;
            err = GenerateExpression(info,expression->right,&rtype);
            if(err==GEN_ERROR) return err;
            
            info.code->add({BC_POP,BC_REG_RDX});
            info.code->add({BC_POP,BC_REG_RAX});

            if(AST::IsPointer(ltype)&&rtype==AST_INT32){
                if(expression->typeId==AST_ADD){
                    info.code->add({BC_ADDI,BC_REG_RAX,BC_REG_RDX,BC_REG_RAX});
                    *outTypeId = ltype;
                    info.code->add({BC_PUSH,BC_REG_RAX});
                    return GEN_SUCCESS;
                }else{
                    log::out << log::RED<<"GenExpr: operation not implemented\n";
                    *outTypeId = AST_VOID;
                    return GEN_ERROR;
                }
            }

            if(ltype!=rtype){
                ERRT(expression->tokenRange.firstToken) << expression->tokenRange.firstToken<<" mismatch of data types ("<<*info.ast->getTypeId(ltype)<<" - "<<*info.ast->getTypeId(rtype)<<")\n";
                // log::out << log::RED<<"TypeId mismatch\n";
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }

            #define GEN_OP(X,Y) if(expression->typeId==AST_##X) info.code->add({Y,BC_REG_RAX,BC_REG_RDX,BC_REG_RAX});
            if(ltype==AST_FLOAT32){
                GEN_OP(ADD,BC_ADDF)
                else GEN_OP(SUB,BC_SUBF)
                else GEN_OP(MUL,BC_MULF)
                else GEN_OP(DIV,BC_DIVF)
                else
                    // TODO: do OpToStr first and then if it fails print the typeId number
                    log::out << log::RED<<"GenExpr: operation "<<expression->typeId<<" not implemented\n";    
            }else{
                GEN_OP(ADD,BC_ADDI)
                else GEN_OP(SUB,BC_SUBI)
                else GEN_OP(MUL,BC_MULI)
                else GEN_OP(DIV,BC_DIVI)

                else GEN_OP(EQUAL,BC_EQ)
                else GEN_OP(NOT_EQUAL,BC_NEQ)
                else GEN_OP(LESS,BC_LT)
                else GEN_OP(LESS_EQUAL,BC_LTE)
                else GEN_OP(GREATER,BC_GT)
                else GEN_OP(GREATER_EQUAL,BC_GTE)
                else GEN_OP(AND,BC_ANDI)
                else GEN_OP(OR,BC_ORI)
                else
                    // TODO: do OpToStr first and then if it fails print the typeId number
                    log::out << log::RED<<"GenExpr: operation "<<expression->typeId<<" not implemented\n";
            }
            #undef GEN_OP
            *outTypeId = ltype;
            
            info.code->add({BC_PUSH,BC_REG_RAX});
        }
    }
    return GEN_SUCCESS;
}
int GenerateBody(GenInfo& info, ASTBody* body){
    using namespace engone;
    _GLOG(FUNC_ENTER)
    Assert(body)

    //-- Begin by generating functions
    ASTFunction* nextFunction = body->functions;
    while(nextFunction){
        ASTFunction* function = nextFunction;
        nextFunction = nextFunction->next;
        
        _GLOG(SCOPE_LOG("FUNCTION"))
        

        int lastOffset=info.currentFrameOffset;
        
        auto func = info.addFunction(*function->name);
        func->astFunc = function;
        // TODO: What if variable name is the same as function variable?
        
        info.code->add({BC_JMP});
        int skipIndex=info.code->length();
        info.code->addIm(0);
        
        info.currentFunction = function;
        func->address = info.code->length();
        
        if(func->astFunc->arguments.size()!=0){
            _GLOG(log::out << "set "<<func->astFunc->arguments.size()<<" args\n");
            int offset = 0;
            for(int i = func->astFunc->arguments.size()-1;i>=0;i--){
                auto& arg = func->astFunc->arguments[i];
                auto var = info.addVariable(arg.name);
                var->typeId = arg.typeId;
                TypeInfo* typeInfo = info.ast->getTypeInfo(arg.typeId);
                if(typeInfo->astStruct) {
                    var->frameOffset = GenInfo::ARG_OFFSET + offset + typeInfo->astStruct->size - 8;
                    // _GLOG(log::out << " "<<arg.name<<" "<<var->frameOffset<<"\n";)
                    offset+=typeInfo->astStruct->size;
                }else{
                    var->frameOffset = GenInfo::ARG_OFFSET + offset;
                    offset+=8;
                }
                _GLOG(log::out << " "<<arg.name<<" "<<var->frameOffset<<"\n";)
            }
            _GLOG(log::out<<"\n";)
        }
        if(func->astFunc->returnTypes.size()!=0){
            _GLOG(log::out << "space for "<<func->astFunc->returnTypes.size()<<" return value(s) (struct may cause multiple push)\n");
            for(int i=0;i<(int)func->astFunc->returnTypes.size();i++){
                auto& typeId = func->astFunc->returnTypes[i];
                TypeInfo* typeInfo = info.ast->getTypeInfo(typeId);
                
                char buf[100];
                int len = sprintf(buf,"  ret value %s",info.ast->getTypeId(typeId)->c_str());
                info.code->addDebugText(buf,len);
                info.code->add({BC_BXOR,BC_REG_RAX,BC_REG_RAX,BC_REG_RAX});
                if(!typeInfo->astStruct){
                    info.code->add({BC_PUSH,BC_REG_RAX});
                    info.currentFrameOffset-=8;
                }else{
                    len += sprintf(buf+len,"\n");
                    for(int i=typeInfo->astStruct->members.size()-1;i>=0;i--){
                        auto& member = typeInfo->astStruct->members[i];
                        len += sprintf(buf+len,"    member %s",member.name.c_str());
                        info.code->addDebugText(buf,len);
                        info.code->add({BC_PUSH,BC_REG_RAX});
                        info.currentFrameOffset-=8;
                        len=0;
                    }
                }
            }
            _GLOG(log::out<<"\n";)
        }
        
        int result = GenerateBody(info,function->body);
        info.currentFrameOffset = lastOffset;

        // TODO: default return doesn't fix the stack pointer or set
        //  default return values. Should be based on return types.
        //  create a return ASTStatement and somehow call the code in GenerateBody
        //  that deals with return instead of duplicating that code here?
        //  another option is to throw an error which is probably the best.

        // TODO: find return statement, must have one if returning a value
        // for statement : function->body->statement
        //     if statement.type == RETURN
        //          found = true

        info.code->add({BC_RET});
        *((u32*)info.code->get(skipIndex)) = info.code->length();

        for(auto& arg : func->astFunc->arguments){
            info.removeIdentifier(arg.name);
        }
    }
    int lastOffset=info.currentFrameOffset;
    
    ASTStatement* nextStatement = body->statements;
    while(nextStatement){
        ASTStatement* statement = nextStatement;
        nextStatement = nextStatement->next;

        if(statement->type==ASTStatement::ASSIGN){
            _GLOG(SCOPE_LOG("ASSIGN"))
            TypeId dtype=0;
            if(!statement->rvalue){
                auto id = info.getIdentifier(*statement->name);
                if(id){
                    ERRT(statement->tokenRange.firstToken) << "identifier "<<*statement->name<<" already declared\n";
                    continue;
                }
                GenInfo::Variable* var = info.addVariable(*statement->name);
                var->typeId = statement->typeId;
                TypeInfo* typeInfo = info.ast->getTypeInfo(var->typeId);
                // data type may be zero if it wasn't specified during initial assignment
                // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                info.currentFrameOffset-=8; // skip last variable
                
                Assert(typeInfo->size!=0)
                // int misaligned = info.currentFrameOffset%typeInfo->size;
                // if(misaligned!=0){
                //     info.currentFrameOffset+=typeInfo->size-misaligned;
                // }
                var->frameOffset = info.currentFrameOffset;
                
                int count = (typeInfo->size+7)/8; // +7 so that types not divisible by 8 is rounded up
                if(count>1){
                    info.currentFrameOffset-=(count-1)*8;
                }
                // log::out << count <<" "<<(typeInfo->size+7)<<"\n";
                // TODO: default values?
                
                // info.code->add({BC_LI,BC_REG_RAX});
                // info.code->addIm();
                // info.code->add({BC_ADDI,BC_REG_SP,BC_REG_RAX,BC_REG_SP});
                // careful with sp and push, push will increment sp
                
                char buf[100];
                int len = sprintf(buf,"  declare %s",statement->name->c_str());
                info.code->addDebugText(buf,len);
                info.code->add({BC_BXOR,BC_REG_RAX,BC_REG_RAX,BC_REG_RAX});
                for(int i=0;i<count;i++){
                    info.code->add({BC_PUSH, BC_REG_RAX});
                }
                
                continue;
            }
            int result = GenerateExpression(info, statement->rvalue,&dtype);
            if(result!=GEN_SUCCESS) return result;
            auto id = info.getIdentifier(*statement->name);
            GenInfo::Variable* var = 0;
            if(id){
                if(id->type==GenInfo::Identifier::VAR){
                    var = info.getVariable(id->index);
                }else{
                    ERRT(statement->tokenRange.firstToken) << "identifier "<<*statement->name<<" was not a variable\n";
                    continue;
                }
            }
            if(result==GEN_ERROR) {

            } else if(dtype==AST_VOID){
                ERRT(statement->tokenRange.firstToken) << "datatype for assignment cannot be void\n";
            }else if(!id||id->type==GenInfo::Identifier::VAR){
                if(dtype==statement->typeId || statement->typeId==AST_VOID){
                    // TODO: nonetype is only valid for as implicit initial decleration.
                    //    Not allowed afterwards? or maybe it's find we just check variable type
                    //    not looking at the statement data type?
                    bool decl = !var;
                    if(!var){
                        var = info.addVariable(*statement->name);
                        var->typeId = dtype;
                        // data type may be zero if it wasn't specified during initial assignment
                        // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                        
                        /*
                        If you are wondering whether frameOffset will collide with other push and pops
                        it won't. push, pop occurs in expressions which are temporary. The only non temporary push
                        is when we do an assignment.
                        */
                    }
                    TypeInfo* typeInfo = info.ast->getTypeInfo(var->typeId);
                    if(decl){
                        // new declaration
                        // info.code->add({BC_POP,BC_REG_RAX});
                        // info.code->add({BC_PUSH,BC_REG_RAX});
                        int count = (typeInfo->size+7)/8; // +7 so that types not divisible by 8 is rounded up
                        // if(count>1){
                            info.currentFrameOffset-=count*8;
                        // }
                        // info.currentFrameOffset-=8;
                        var->frameOffset = info.currentFrameOffset;
                        _GLOG(log::out<<"declare "<<*statement->name<<" at "<<var->frameOffset;)
                        // NOTE: inconsistent
                        // char buf[100];
                        // int len = sprintf(buf," ^ was assigned %s",statement->name->c_str());
                        // info.code->addDebugText(buf,len);
                    }else{
                        // local variable exists on stack
                        char buf[100];
                        int len = sprintf(buf,"  assign %s",statement->name->c_str());
                        info.code->addDebugText(buf,len);
                        if(!typeInfo->astStruct){
                            info.code->add({BC_POP,BC_REG_RDX});
                            info.code->add({BC_LI,BC_REG_RBX});
                            info.code->addIm(var->frameOffset);
                            info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RBX,BC_REG_RBX}); // rbx = fp + offset
                            info.code->add({BC_MOV_RM,BC_REG_RDX,BC_REG_RBX});
                        }else{
                            int offset=typeInfo->astStruct->size;
                            for(int i=typeInfo->astStruct->members.size()-1;i>=0;i--){
                                auto& member = typeInfo->astStruct->members[i];
                                info.code->add({BC_POP,BC_REG_RDX});
                                info.code->add({BC_LI,BC_REG_RBX});
                                info.code->addIm(var->frameOffset-i*8);
                                info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RBX,BC_REG_RBX}); // rbx = fp + offset
                                info.code->add({BC_MOV_RM,BC_REG_RDX,BC_REG_RBX});
                            }
                        }
                    }
                    // if(decl){
                    // move forward stack pointer
                    //     info.code->add({BC_LI,BC_REG_RCX});
                    //     info.code->addIm(8);
                    //     info.code->add({BC_ADDI,BC_REG_SP,BC_REG_RCX,BC_REG_SP});
                    // }
                }else{
                    ERRT(statement->tokenRange.firstToken) << "Type mismatch for variable "<<*statement->name<<". Should be "<<*info.ast->getTypeId(statement->typeId)<<" but was "<<*info.ast->getTypeId(dtype)<<"\n";
                    continue;
                    // TODO: Show were in the code
                }
            }
            _GLOG(log::out << " "<<*statement->name << " : "<<*info.ast->getTypeId(dtype)<<"\n";)
        } else if(statement->type==ASTStatement::PROP_ASSIGN){
            _GLOG(SCOPE_LOG("PROP_ASSIGN"))
            
            if(statement->lvalue->typeId==AST_DEREF){
                TypeId dtype=0;
                int result = GenerateExpression(info, statement->lvalue->left,&dtype);
                if(result!=GEN_SUCCESS) return result;
                
                if(!AST::IsPointer(dtype)){
                    ERRT(statement->lvalue->tokenRange.firstToken) << "expected pointer type for deref assignment not "<<*info.ast->getTypeId(dtype)<<"\n";
                    return GEN_ERROR;
                }
                
                TypeId rtype=0;
                result = GenerateExpression(info, statement->rvalue,&rtype);
                if(result!=GEN_SUCCESS) return result;
                
                info.code->add({BC_POP,BC_REG_RAX});
                info.code->add({BC_POP,BC_REG_RBX});
                info.code->add({BC_MOV_RM,BC_REG_RAX,BC_REG_RBX});
                
            } else if(statement->lvalue->typeId==AST_PROP){
                //-- validate left side
                auto varexpr = statement->lvalue->left;
                if(varexpr->typeId != AST_VAR){
                    ERRT(statement->lvalue->tokenRange.firstToken) << "expected variable when doing prop assign not "<<*info.ast->getTypeId(varexpr->typeId)<<"\n";
                    return GEN_ERROR;
                }
                auto id = info.getIdentifier(*varexpr->name);
                if(!id||id->type!=GenInfo::Identifier::VAR){
                    ERRT(varexpr->tokenRange.firstToken) << varexpr->name<<" is not a variable\n";
                    return GEN_ERROR;
                }
                auto var = info.getVariable(id->index);
                auto typeInfo = info.ast->getTypeInfo(var->typeId);
                if(!typeInfo->astStruct){
                    ERRT(varexpr->tokenRange.firstToken) << varexpr->name<<" is not a struct, can't do prop assignment\n";
                    return GEN_ERROR;
                }
                int index=-1;
                for(int i=0;i<(int)typeInfo->astStruct->members.size();i++){
                    if(typeInfo->astStruct->members[i].name == *statement->lvalue->name){
                        index=i;
                        break;
                    }
                }
                if(index==-1){
                    ERRT(statement->tokenRange.firstToken) << *statement->lvalue->name<<" is not a member of "<<*typeInfo->astStruct->name<<"\n";
                    return GEN_ERROR;   
                }
                auto& member = typeInfo->astStruct->members[index];
                
                //-- generate right side
                TypeId typeId;
                int result = GenerateExpression(info,statement->rvalue,&typeId);
                if(result!=GEN_SUCCESS) return result;
                
                //-- generate left side
                info.code->add({BC_POP,BC_REG_RAX});
                
                info.code->add({BC_LI,BC_REG_RBX});
                info.code->addIm(var->frameOffset + member.offset);
                info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RBX,BC_REG_RBX});
                info.code->add({BC_MOV_RM,BC_REG_RAX,BC_REG_RBX});
                
            } else {
                ERRT(statement->tokenRange.firstToken) << "expected deref or prop for prop assignment, was "<<OpToStr(statement->lvalue->typeId)<<"\n";
                return GEN_ERROR;
            }
        } else if (statement->type == ASTStatement::IF){
            _GLOG(SCOPE_LOG("IF"))
            TypeId dtype=0;
            int result = GenerateExpression(info,statement->rvalue,&dtype);
            if(result==GEN_ERROR){
                // generate body anyway or not?
                continue;
            }
            // if(dtype!=AST_BOOL8){
            //     ERRT(statement->expression->token) << "Expected a boolean, not '"<<TypeIdToStr(dtype)<<"'\n";
            //     continue;
            // }

            info.code->add({BC_POP,BC_REG_RAX});
            info.code->add({BC_JNE,BC_REG_RAX});
            u32 skipIfBodyIndex = info.code->length();
            info.code->addIm(0);

            result = GenerateBody(info,statement->body);
            if(result==GEN_ERROR)
                continue;

            u32 skipElseBodyIndex = -1;
            if(statement->elseBody){
                info.code->add({BC_JMP});
                skipElseBodyIndex = info.code->length();
                info.code->addIm(0);
            }

            // fix address for jump instruction
            *(u32*)info.code->get(skipIfBodyIndex) = info.code->length();

            if(statement->elseBody){
                result = GenerateBody(info,statement->elseBody);
                if(result==GEN_ERROR)
                    continue;

                *(u32*)info.code->get(skipElseBodyIndex) = info.code->length();
            }
        } else if(statement->type == ASTStatement::WHILE){
            _GLOG(SCOPE_LOG("WHILE"))

            u32 loopAddress = info.code->length();

            TypeId dtype=0;
            int result = GenerateExpression(info,statement->rvalue,&dtype);
            if(result==GEN_ERROR){
                // generate body anyway or not?
                continue;
            }
            // if(dtype!=AST_BOOL8){
            //     ERRT(statement->expression->token) << "Expected a boolean, not '"<<TypeIdToStr(dtype)<<"'\n";
            //     continue;
            // }

            info.code->add({BC_POP,BC_REG_RAX});
            info.code->add({BC_JNE,BC_REG_RAX});
            u32 endIndex = info.code->length();
            info.code->addIm(0);

            result = GenerateBody(info,statement->body);
            if(result==GEN_ERROR)
                continue;

            info.code->add({BC_JMP});
            info.code->addIm(loopAddress);

            // fix address for jump instruction
            *(u32*)info.code->get(endIndex) = info.code->length();
        } else if(statement->type == ASTStatement::RETURN){
            _GLOG(SCOPE_LOG("RETURN"))
            
            if(!info.currentFunction){
                ERRT(statement->tokenRange.firstToken) << "return only allowed in function\n";
                return GEN_ERROR;
            }
            
            //-- evaluate return values
            ASTExpression* expr = statement->rvalue;
            int argi=-1;
            int offset=0;
            while(expr){
                ASTExpression* temp = expr;
                expr = expr->next;
                argi++;
                
                TypeId dtype=0;
                int result = GenerateExpression(info,temp,&dtype);
                if(result==GEN_ERROR){
                    continue;
                }
                if(argi>=(int)info.currentFunction->returnTypes.size()){
                    continue;
                }
                
                if(!IsSafeCast(info.currentFunction->returnTypes[argi],dtype)){
                // if(info.currentFunction->returnTypes[argi]!=dtype){
                    ERRT(temp->tokenRange.firstToken) << "data type mismatch on return values "<<*info.ast->getTypeId(dtype)<<" - "
                        <<*info.ast->getTypeId(info.currentFunction->returnTypes[argi])<<"\n";
                }
                TypeInfo* typeInfo = info.ast->getTypeInfo(dtype);
                if(!typeInfo->astStruct){
                    offset-=8;
                    _GLOG(log::out<<"move return value\n";)
                    info.code->add({BC_POP,BC_REG_RAX});
                    info.code->add({BC_LI,BC_REG_RBX});
                    info.code->addIm(offset);
                    info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RBX,BC_REG_RBX});
                    info.code->add({BC_MOV_RM,BC_REG_RAX,BC_REG_RBX});
                }else{
                    offset-=typeInfo->astStruct->size;
                    for(int i=0;i<(int)typeInfo->astStruct->members.size();i++){
                        auto& member = typeInfo->astStruct->members[i];
                        _GLOG(log::out<<"move return value member "<<member.name<<"\n";)
                        info.code->add({BC_POP,BC_REG_RAX});
                        info.code->add({BC_LI,BC_REG_RBX});
                        info.code->addIm(offset+i*8);
                        info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RBX,BC_REG_RBX});
                        info.code->add({BC_MOV_RM,BC_REG_RAX,BC_REG_RBX});
                    }
                }
                _GLOG(log::out<<"\n";)
            }
            argi++; // incremented to get argument count
            if(argi!=(int)info.currentFunction->returnTypes.size()){
                ERRT(statement->tokenRange.firstToken) << "Found "<<argi<<" return values but should have "<<info.currentFunction->returnTypes.size()<<" for '"<<*info.currentFunction->name<<"'\n";   
            }
            
            // fix stack pointer before returning
            info.code->add({BC_LI,BC_REG_RCX});
            info.code->addIm(info.currentFrameOffset-lastOffset);
            info.code->add({BC_ADDI,BC_REG_SP,BC_REG_RCX,BC_REG_SP});
            info.currentFrameOffset = lastOffset;
            info.code->add({BC_RET});
            return GEN_SUCCESS;
        } else if(statement->type == ASTStatement::CALL){
            _GLOG(SCOPE_LOG("CALL"))
            
            TypeId dtype=0;
            int result = GenerateExpression(info,statement->rvalue,&dtype);
            if(result==GEN_ERROR){
                continue;
            }
            // TODO: handle struct
            info.code->add({BC_POP,BC_REG_RAX}); // don't care about return value
        }
    }
    if(lastOffset!=info.currentFrameOffset){
        info.code->add({BC_LI,BC_REG_RCX});
        info.code->addIm(info.currentFrameOffset-lastOffset);
        info.code->add({BC_ADDI,BC_REG_SP,BC_REG_RCX,BC_REG_SP});
        info.currentFrameOffset = lastOffset;
    }
    
    return GEN_SUCCESS;
}
BytecodeX* Generate(AST* ast, int* err){
    using namespace engone;
    _VLOG(log::out <<log::BLUE<<  "##   Generator   ##\n";)
    
    GenInfo info{};
    info.code = BytecodeX::Create();
    info.ast = ast;
    {
    auto fun = info.addFunction("alloc");
    fun->astFunc = ast->createFunction("alloc");
    fun->astFunc->returnTypes.push_back(ast->getTypeId("void*"));
    fun->astFunc->arguments.push_back({});
    fun->astFunc->arguments.back().typeId = ast->getTypeId("u64");
    fun->astFunc->arguments.back().name = "size";
    fun->address = BC_EXT_ALLOC;
    }{
    auto fun = info.addFunction("realloc");
    fun->astFunc = ast->createFunction("realloc");
    fun->astFunc->returnTypes.push_back(ast->getTypeId("void*"));
    fun->astFunc->arguments.push_back({});
    fun->astFunc->arguments.back().typeId = ast->getTypeId("void*");
    fun->astFunc->arguments.back().name = "ptr";
    fun->astFunc->arguments.push_back({});
    fun->astFunc->arguments.back().typeId = ast->getTypeId("u64");
    fun->astFunc->arguments.back().name = "oldsize";
    fun->astFunc->arguments.push_back({});
    fun->astFunc->arguments.back().typeId = ast->getTypeId("u64");
    fun->astFunc->arguments.back().name = "size";
    fun->address = BC_EXT_REALLOC;
    }{
    auto fun = info.addFunction("free");
    fun->astFunc = ast->createFunction("free");
    fun->astFunc->arguments.push_back({});
    fun->astFunc->arguments.back().typeId = ast->getTypeId("void*");
    fun->astFunc->arguments.back().name = "ptr";
    fun->astFunc->arguments.push_back({});
    fun->astFunc->arguments.back().typeId = ast->getTypeId("u64");
    fun->astFunc->arguments.back().name = "size";
    fun->address = BC_EXT_FREE;
    }{
    auto fun = info.addFunction("printi");
    fun->astFunc = ast->createFunction("printi");
    fun->astFunc->arguments.push_back({});
    fun->astFunc->arguments.back().typeId = ast->getTypeId("i64");
    fun->astFunc->arguments.back().name = "num";
    fun->address = BC_EXT_PRINTI;
    }
    int result = GenerateBody(info,info.ast->body);
    // TODO: what to do about result?
    
    if(info.errors)
        log::out << log::RED<<"Generator failed with "<<info.errors<<" errors\n";
    if(err)
        *err = info.errors;
    return info.code;
}