#include "BetBat/Generator.h"

#undef ERRT
#undef ERR
#undef ERRTYPE

// Remember to wrap macros in {} when using if or loops
// since macros have multiple statements.
#define ERR() info.errors++;engone::log::out << engone::log::RED <<"GenError, "
#define ERRT(R) info.errors++;engone::log::out << engone::log::RED <<"GenError "<<(R.firstToken.line)<<":"<<(R.firstToken.column)<<", "
#define ERRL(R) info.errors++;engone::log::out << engone::log::RED <<"GenError "<<(R.firstToken.line)<<":"<<(R.tokenStream->get(R.endIndex-1).column + R.tokenStream->get(R.endIndex-1).length)<<", "
#define ERRTYPE(R,LT,RT) ERRT(R) << "Type mismatch "<<info.ast->getTypeInfo(LT)->getFullType(info.ast)<<" - "<<info.ast->getTypeInfo(RT)->getFullType(info.ast)<<" "

#define LOGAT(R) R.firstToken.line <<":"<<R.firstToken.column

#define ERRTOKENS(R) log::out <<log::RED<< "LN "<<R.firstToken.line <<": "; R.print();log::out << "\n";

#define TOKENINFO(R) {std::string temp="";R.feed(temp);info.code->addDebugText(std::string("Ln ")+std::to_string(R.firstToken.line)+ ": ");info.code->addDebugText(temp+"\n");}

#define GEN_SUCCESS 1
#define GEN_ERROR 0
/* #region  */
void GenInfo::addPop(int reg){
    using namespace engone;
    int size = DECODE_REG_SIZE(reg);
    if(size==0 && errors==0) // we don't print if we had errors since they probably caused size of 0
        log::out <<log::RED<< "GenInfo::addPush : Cannot pop register with 0 size\n";

    code->add({BC_POP,(u8)reg});
    auto align = stackAlignment.back();
    stackAlignment.pop_back();
    if(align.diff!=0){
        relativeStackPointer += align.diff;
        code->addDebugText("align sp\n");
        i16 offset = align.diff;
        code->add({BC_INCR,BC_REG_SP,(u8)(0xFF&offset), (u8)(offset>>8)});
    }
    relativeStackPointer += size;
}
void GenInfo::addPush(int reg){
    using namespace engone;
    int size = DECODE_REG_SIZE(reg);
    if(size==0 && errors==0) // we don't print if we had errors since they probably caused size of 0
        log::out <<log::RED<< "GenInfo::addPush : Cannot push register with 0 size\n";

    int diff = (size-(-relativeStackPointer) % size)%size; // how much to increment sp by to align it
    // TODO: Instructions are generated from top-down and the stackAlignment
    //   sees pop and push in this way but how about jumps. It doesn't consider this. Is it an issue?
    if(diff){
        relativeStackPointer -= diff;
        code->addDebugText("align sp\n");
        i16 offset = -diff;
        code->add({BC_INCR,BC_REG_SP,(u8)(0xFF&offset), (u8)(offset>>8)});
    }
    code->add({BC_PUSH,(u8)reg});
    stackAlignment.push_back({diff,size});
    relativeStackPointer-=size;
}
void GenInfo::addIncrSp(i16 offset){
    using namespace engone;
    if(offset==0) return;
    // Assert(offset>0) // TOOD: doesn't handle decrement of sp
    if(offset>0){
        int at=offset;
        while(at>0&&stackAlignment.size()>0){
            auto align = stackAlignment.back();
            // log::out << "pop stackalign "<<align.diff<<":"<<align.size<<"\n";
            stackAlignment.pop_back();
            at-=align.size;
            at-=align.diff;
            Assert(at>=0)
        }
    }else if(offset<0){
        stackAlignment.push_back({0,-offset});
    }
    relativeStackPointer += offset;
    code->add({ BC_INCR, BC_REG_SP, (u8)(0xFF&offset), (u8)(offset>>8)});
}
int GenInfo::saveStackMoment(){
    return relativeStackPointer;
}
void GenInfo::restoreStackMoment(int moment){
    using namespace engone;
    int offset = moment-relativeStackPointer;
    if(offset == 0) return;
    int at=moment-relativeStackPointer;
    while(at>0&&stackAlignment.size()>0){
        auto align = stackAlignment.back();
        // log::out << "pop stackalign "<<align.diff<<":"<<align.size<<"\n";
        stackAlignment.pop_back();
        at-=align.size;
        at-=align.diff;
        Assert(at>=0)
    }
    relativeStackPointer = moment;
    code->add({ BC_INCR, BC_REG_SP, (u8)(0xFF&offset), (u8)(offset>>8)});
}
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
GenInfo::Namespace* GenInfo::addNamespace(const std::string& name){
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
    id->type = Identifier::NAMESPACE;
    id->index = namespaces.size();
    namespaces.push_back({});
    return &namespaces[id->index];
}
GenInfo::Identifier* GenInfo::addIdentifier(const std::string& name){
    using namespace engone;
    auto pair = identifiers.find(name);
    if(pair!=identifiers.end())
        return 0;
    
    identifiers[name] = {};
    
    auto pair2 = identifiers.find(name);
    if(pair2==identifiers.end()){
        return 0;
    }
    
    return &pair2->second;
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
GenInfo::Namespace* GenInfo::getNamespace(int index){
    // TODO: bound check
    return &namespaces[index];
}
void GenInfo::removeIdentifier(const std::string& name){
    auto pair = identifiers.find(name);
    if(pair!=identifiers.end()){
        identifiers.erase(pair);
    }
}
/* #endregion */
// Will perform cast on float and integers with pop, cast, push
bool PerformSafeCast(GenInfo& info, TypeId from, TypeId to) {
    if(from==to) return true;
    // TODO: I just threw in currentScopeId. Not sure if it is supposed to be in all cases.
    auto fti = info.ast->getTypeInfo(from);
    auto tti = info.ast->getTypeInfo(to);
    u8 reg0 = RegBySize(1,fti->size());
    u8 reg1 = RegBySize(1,tti->size());
    if(from==AST_FLOAT32&&AST::IsInteger(to)){
        info.addPop(reg0);
        info.code->add({BC_CAST,CAST_FLOAT_SINT,reg0,reg1});
        info.addPush(reg1);
        return true;
    }
    if(AST::IsInteger(from)&&to==AST_FLOAT32){
        info.addPop(reg0);
        info.code->add({BC_CAST,CAST_SINT_FLOAT,reg0,reg1});
        info.addPush(reg1);
        return true;
    }
    if((AST::IsInteger(from) && to == AST_CHAR) ||
        (from == AST_CHAR && AST::IsInteger(to))){
        // if(fti->size() != tti->size()){
            info.addPop(reg0);
            info.code->add({BC_CAST,CAST_SINT_SINT,reg0,reg1});
            info.addPush(reg1);
        // }
        return true;
    }
    if(AST::IsInteger(from)&&AST::IsInteger(to)){
        info.addPop(reg0);
        if(AST::IsSigned(from)&&AST::IsSigned(to))
            info.code->add({BC_CAST,CAST_SINT_SINT,reg0,reg1});
        if(AST::IsSigned(from)&&!AST::IsSigned(to))
            info.code->add({BC_CAST,CAST_SINT_UINT,reg0,reg1});
        if(!AST::IsSigned(from)&&AST::IsSigned(to))
            info.code->add({BC_CAST,CAST_UINT_SINT,reg0,reg1});
        info.addPush(reg1);
        return true;
    }
    
    return false;
    
    // allow cast between unsigned/signed numbers of all sizes. It's the most convenient.
    
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
// pointers are safe to convert
// you need to pop the values with correct registers
// popping small register and pushing bigger register can be dangerous
// since a part of the register contains unexpected data you will use that
// when pushing bigger registers. You would need to XOR 64 bit register first.
bool IsSafeCast(TypeId from, TypeId to) {
    // if(AST::IsInteger(from)&&AST::IsInteger(to))
        // return true;
    if(AST::IsPointer(from)&&AST::IsPointer(to))
        return true;
    
    return from==to;
    
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
            
            // TODO: immediate only allows 32 bits. What about larger values?
            // info.code->addDebugText("  expr push int");
            TOKENINFO(expression->tokenRange)
            
            // TODO: Int types should come from global scope. Is it a correct assumption?
            TypeInfo* typeInfo = info.ast->getTypeInfo(expression->typeId);
            u8 reg = RegBySize(1,typeInfo->size());
            info.code->add({BC_LI,reg});
            info.code->addIm(val);
            info.addPush(reg);
        } else if(expression->typeId==AST_BOOL){
            bool val = expression->boolValue;
            
            TOKENINFO(expression->tokenRange)
            // info.code->addDebugText("  expr push bool");
            info.code->add({BC_LI,BC_REG_AL});
            info.code->addIm(val);
            info.addPush(BC_REG_AL);
        } else if(expression->typeId==AST_CHAR){
            char val = expression->charValue;
            
            
            TOKENINFO(expression->tokenRange)
            // info.code->addDebugText("  expr push char");
            info.code->add({BC_LI,BC_REG_AL});
            info.code->addIm(val);
            info.addPush(BC_REG_AL);
        } else if(expression->typeId==AST_FLOAT32){
            float val = expression->f32Value;
            
            TOKENINFO(expression->tokenRange)
            // info.code->addDebugText("  expr push float");
            info.code->add({BC_LI,BC_REG_EAX});
            info.code->addIm(*(u32*)&val);
            info.addPush(BC_REG_EAX);
        } else if(expression->typeId==AST_VAR) {
            // NOTE: HELLO! i commented out the code below because i thought it was strange and out of place.
            //   It might be important but I just don't know why. Yes it was important past me.
            //   AST_VAR and variables have simular syntax.
            if(expression->name){
                TypeInfo* typeInfo = info.ast->getTypeInfo(info.currentScopeId, *expression->name);
                if(typeInfo&&typeInfo->astEnum){
                    // ERRT(expression->tokenRange) << "cannot access "<<(expression->member?*expression->member:"?")<<" from non-enum "<<*expression->name<<"\n";
                    // return GEN_ERROR;
                    *outTypeId = typeInfo->id;
                    return GEN_SUCCESS;
                }
            }
            // check data type and get it
            auto id = info.getIdentifier(*expression->name);
            if(id && id->type == GenInfo::Identifier::VAR){
                auto var = info.getVariable(id->index);
                // TODO: check data type?
                // fp + offset
                // TODO: what about struct
                _GLOG(log::out<<" expr var push "<<*expression->name<<"\n";)
                    
                TOKENINFO(expression->tokenRange)
                // char buf[100];
                // int len = sprintf(buf,"  expr push %s",expression->name->c_str());
                // info.code->addDebugText(buf,len);
                TypeInfo* typeInfo = info.ast->getTypeInfo(var->typeId);
                if(!typeInfo->astStruct){
                    info.code->add({BC_LI,BC_REG_RBX});
                    info.code->addIm(var->frameOffset);
                    info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RBX, BC_REG_RBX});

                    u8 reg = RegBySize(1,typeInfo->size()); // get the appropriate register

                    info.code->add({BC_MOV_MR,BC_REG_RBX,reg});
                    // info.code->add({BC_PUSH,BC_REG_RAX});
                    info.addPush(reg);
                } else {
                    auto& members = typeInfo->astStruct->members;
                    for(int i=(int)members.size()-1;i>=0;i--){
                        auto& member = typeInfo->astStruct->members[i];
                        _GLOG(log::out<<"  member "<< member.name<<"\n";)
                        
                        TypeInfo* ti = info.ast->getTypeInfo(member.typeId);
                        u8 reg = RegBySize(1,ti->size());

                        info.code->add({BC_LI,BC_REG_RBX});
                        info.code->addIm(var->frameOffset+member.offset);
                        info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RBX, BC_REG_RBX});
                        info.code->add({BC_MOV_MR,BC_REG_RBX,reg});
                        info.addPush(reg);

                    }
                }
                
                *outTypeId = var->typeId;
                return GEN_SUCCESS;
            }else{
                ERRT(expression->tokenRange) << expression->tokenRange.firstToken<<" is undefined\n";
                // log::out << log::RED<<"var "<<*expression->varName<<" undefined\n";   
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }
        } else if(expression->typeId==AST_FNCALL){
            // check data type and get it
            auto id = info.getIdentifier(*expression->name);
            if(id && id->type == GenInfo::Identifier::FUNC){
                auto func = info.getFunction(id->index);
                
                ASTExpression* argt = expression->left;
                if(argt)
                    _GLOG(log::out<<"push arguments\n");
                int startSP = info.saveStackMoment();

                //-- align
                int modu = (func->astFunc->argSize - info.relativeStackPointer )% 8;
                if(modu!=0){
                    int diff = 8 - modu;
                    // log::out << "   align\n";
                    info.code->addDebugText("   align\n");
                    info.addIncrSp(-diff); // Align
                    // TODO: does something need to be done with stackAlignment list.
                }
                std::vector<ASTExpression*> realArgs;
                realArgs.resize(func->astFunc->arguments.size());

                int index = -1;
                while (argt){
                    ASTExpression* arg = argt;
                    argt = argt->next;
                    index++;

                    if(!arg->namedValue){
                        if((int)realArgs.size()<=index){
                            ERRT(arg->tokenRange) << "To many arguments for function "<<func->astFunc->name<<" ("<<func->astFunc->arguments.size()<<" argument(s) allowed)\n";
                            continue;
                        }else
                            realArgs[index] = arg;
                    } else {
                        int argIndex=-1;
                        for(int i=0;i<(int)func->astFunc->arguments.size();i++){
                            if(func->astFunc->arguments[i].name == *arg->namedValue){
                                argIndex = i;
                                break;
                            }
                        }
                        if(argIndex==-1){
                            ERRT(arg->tokenRange) << *arg->namedValue<<" is not an argument in "<<func->astFunc->name<<"\n";
                            continue;
                        } else {
                            if(realArgs[argIndex]){
                                ERRT(arg->tokenRange) << "argument for "<<func->astFunc->arguments[argIndex].name <<" is already specified at "<<LOGAT(realArgs[argIndex]->tokenRange)<<"\n";
                            }else{
                                realArgs[argIndex] = arg;
                            }
                        }
                    }
                }

                for(int i=0;i<(int)func->astFunc->arguments.size();i++){
                    auto& arg = func->astFunc->arguments[i];
                    if(!realArgs[i])
                        realArgs[i] = arg.defaultValue;
                }

                index = -1;
                for (auto arg : realArgs){
                    // ASTExpression* arg = argt;
                    // argt = argt->next;
                    index++;
                    TypeId dt=0;
                    if(!arg){
                        ERRT(expression->tokenRange) << "missing argument for "<<func->astFunc->arguments[index].name<< " (call to "<<func->astFunc->name<<")\n";
                        continue;
                    }

                    int result = GenerateExpression(info,arg,&dt);
                    if(result!=GEN_SUCCESS){
                        continue;   
                    }
                    if(index>=(int)func->astFunc->arguments.size()){
                        // ERR() << "To many arguments! func:"<<*expression->funcName<<" max: "<<func->astFunc->arguments.size()<<"\n";
                        continue;
                    }

                    _GLOG(log::out<<" pushed "<<func->astFunc->arguments[index].name<<"\n";)
                    
                    if(!PerformSafeCast(info,dt,func->astFunc->arguments[index].typeId)){ // implicit conversion
                    // if(func->astFunc->arguments[index].typeId!=dt){ // strict, no conversion
                        ERRTYPE(arg->tokenRange,dt,func->astFunc->arguments[index].typeId) << "(fncall)\n";
                        continue;
                    }
                    // values are already pushed to the stack
                }
                {
                    // align to 8 bytes because the call frame requires it.
                    int modu = (func->astFunc->argSize - info.relativeStackPointer )% 8;
                    if(modu!=0){
                        int diff = 8 - modu;
                        // log::out << "   align\n";
                        info.code->addDebugText("   align\n");
                        info.addIncrSp(-diff); // Align
                        // TODO: does something need to be done with stackAlignment list.
                    }
                }
                // TODO: FIX ALIGN HERE
                // log::out << "ALIGNMENT (rel. sp): "<<info.relativeStackPointer<<"\n";
                // int argBytes = startSP - info.relativeStackPointer; // NOTE: that relative stack pointer is negative and argBytes will be positive
                // index++; // incremented to get argument count
                // if(index!=(int)func->astFunc->arguments.size()){
                //     ERRT(expression->tokenRange) << "Found "<<index<<" arguments but "<<*expression->name<<" requires "<<func->astFunc->arguments.size()<<"\n";   
                // }
                if(func->address==0){
                    ERR() << "func address was invalid\n";
                }
                
                // char buf[100];
                // int len = sprintf(buf,"  call %s",expression->name->c_str());
                // info.code->addDebugText(buf,len);
                TOKENINFO(expression->tokenRange)
                info.code->add({BC_LI,BC_REG_RAX});
                info.code->addIm(func->address); // may be invalid if function exists after call
                info.code->add({BC_CALL,BC_REG_RAX});
                
                // pop arguments
                if(expression->left){
                    _GLOG(log::out<<"pop arguments\n");
                    // info.code->add({BC_LI,BC_REG_RAX});
                    // info.code->addIm(index*8);
                    // info.code->add({BC_ADDI,BC_REG_SP,BC_REG_RAX,BC_REG_SP});
                    
                    // info.addIncrSp(argBytes);
                    info.restoreStackMoment(startSP);
                }
                // return types?
                if(func->astFunc->returnTypes.empty()) {
                    *outTypeId = AST_VOID;
                } else {
                    *outTypeId = func->astFunc->returnTypes[0].typeId;
                    if(func->astFunc->returnTypes.size()!=0){
                        _GLOG(log::out << "extract return values\n";)
                    }
                    // NOTE: info.currentScopeId MAY NOT WORK FOR THE TYPES IF YOU PASS FUNCTION POINTERS!
                    // move return values
                    int offset = -func->astFunc->argSize-GenInfo::ARG_OFFSET;
                    for(int i=0;i<(int)func->astFunc->returnTypes.size();i++){
                        // int offset = 9;
                        auto& ret = func->astFunc->returnTypes[i];
                        TypeId typeId = ret.typeId;
                        TypeInfo* typeInfo = info.ast->getTypeInfo(typeId);
                        if(!typeInfo->astStruct){
                            // TODO: can't use index for arguments, structs have different sizes
                            _GLOG(log::out<<" extract "<< info.ast->getTypeInfo(typeId)->name<<"\n";)
                            info.code->add({BC_LI,BC_REG_RBX});
                            info.code->addIm(offset + ret.offset);
                            u8 reg = RegBySize(1,typeInfo->size());
                            // index*8 for arguments, ARG_OFFSET for pc and fp, (i+1)*8 is where the return values are
                            info.code->add({BC_ADDI, BC_REG_SP, BC_REG_RBX, BC_REG_RBX});
                            info.code->add({BC_MOV_MR, BC_REG_RBX, reg});
                            info.addPush(reg);
                            // offset -= 8;
                            // don't need to change offset since push will increment stack pointer
                        }else{
                            _GLOG(log::out<<" extract "<< info.ast->getTypeInfo(typeId)->name<<"\n";)
                            BROKEN;
                            // for(int j=typeInfo->astStruct->members.size()-1;j>=0;j--){
                            // // for(int j=0;j<(int)typeInfo->astStruct->members.size();j++){
                            //     auto& member = typeInfo->astStruct->members[j];
                            //     _GLOG(log::out<<"  member "<< member.name<<"\n";)
                            //     info.code->add({BC_LI,BC_REG_RBX});
                            //     info.code->addIm(-index*8 - GenInfo::ARG_OFFSET - 8 + offset);
                            //     // index*8 for arguments, ARG_OFFSET for pc and fp, (i+1)*8 is where the return values are
                            //     info.code->add({BC_ADDI, BC_REG_SP, BC_REG_RBX, BC_REG_RBX});
                            //     info.code->add({BC_MOV_MR, BC_REG_RBX, BC_REG_RAX});
                            //     info.addPush(BC_REG_RAX);
                            //     // offset -= 8;
                            //     // don't need to change offset since push will increment stack pointer
                            // }
                        }
                    }
                }
                return GEN_SUCCESS;
            } else{
                ERRT(expression->tokenRange) << expression->tokenRange.firstToken<<" is undefined\n";
                // log::out << log::RED<<"var "<<*expression->varName<<" undefined\n";   
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }
        // } else if(expression->typeId==AST_STRING){
        //     // string initializer literal
        //     ERR() << "string not implemented\n";
            
        //     //  a = "adad" + "aaaadadad";
        //     // pushing the whole string to the stack will be slow
        //     // it needs to be put into data segment and you would receive
        //     // a pointer to it

        //     return GEN_ERROR;
        //     return GEN_SUCCESS;
        } else if(expression->typeId==AST_NULL){
            // info.code->addDebugText("  expr push null");
            TOKENINFO(expression->tokenRange)
            info.code->add({BC_LI,BC_REG_RAX});
            info.code->addIm(0);
            info.addPush(BC_REG_RAX);
            
            TypeInfo* typeInfo = info.ast->getTypeInfo(info.ast->globalScopeId, "void*");
            *outTypeId = typeInfo->id;
            return GEN_SUCCESS;
        } else {
            // info.code->add({BC_PUSH,BC_REG_RAX}); // push something so the stack stays synchronized, or maybe not?
            ERRT(expression->tokenRange) << expression->tokenRange.firstToken<<" is an unknown data type\n";
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
                ERRT(expr->tokenRange) << expr->tokenRange.firstToken<<", can only reference a variable\n";
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }
            auto id = info.getIdentifier(*expr->name);
            if(id && id->type == GenInfo::Identifier::VAR){
                auto var = info.getVariable(id->index);
                // TODO: check data type?
                TOKENINFO(expression->tokenRange)
                info.code->add({BC_LI,BC_REG_RAX});
                info.code->addIm(var->frameOffset);
                info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RAX, BC_REG_RAX}); // fp + offset
                info.addPush(BC_REG_RAX);
                
                // new pointer data type
                // auto dtname = info.ast->getTypeInfo(var->typeId)->name;
                auto varTypeInfo = info.ast->getTypeInfo(var->typeId);
                std::string temp = varTypeInfo->name;
                temp += "*";
                // pointer type should exist in same scope as the normal type
                auto id = info.ast->getTypeInfo(varTypeInfo->scopeId, temp)->id;
                
                *outTypeId = id;
            } else {
                // TODO: identifier may be defined but not a variable, give proper message
                ERRT(expr->tokenRange) << expr->tokenRange.firstToken<<" is undefined\n";
                // log::out << log::RED<<"var "<<*expression->varName<<" undefined\n";   
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }
        }else if(expression->typeId==AST_DEREF){
            int err = GenerateExpression(info,expression->left,&ltype);
            if(err==GEN_ERROR) return err;

            if(!AST::IsPointer(ltype)){
                ERRT(expression->left->tokenRange) << "cannot dereference "<<info.ast->getTypeInfo(ltype)->name<<" from "<<expression->left->tokenRange.firstToken<<"\n";
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }

            auto defType = info.ast->getTypeInfo(ltype);
            std::string temp = defType->name;
            temp.pop_back(); // pop *
            auto typeInfo = info.ast->getTypeInfo(defType->scopeId,temp); // non-pointer should exist in same scope as pointer
            
            // TODO: deref on struct
            _GLOG(log::out << "DEREF?\n";)
            // info.code->addDebugText("deref");
            TOKENINFO(expression->tokenRange)
            info.addPop(BC_REG_RBX);
            u8 reg = RegBySize(1,typeInfo->size());
            
            info.code->add({BC_MOV_MR,BC_REG_RBX,reg});
            // else {
            //     ERRT(expression->tokenRange) << "cannot dereference "<<temp << " as it has a size of "<<typeInfo->size()<<" (only 1, 2, 4 and 8 are allowed)\n";
            // }

            info.addPush(reg);
            
            *outTypeId = typeInfo->id;
            if(typeInfo->id==AST_VOID){
                ERRT(expression->tokenRange) << "cannot dereference *void\n";
            }
        }else if(expression->typeId==AST_NOT){
            int err = GenerateExpression(info,expression->left,&ltype);
            if(err==GEN_ERROR) return err;
            TypeInfo* ti = info.ast->getTypeInfo(ltype);
            u8 reg = RegBySize(1,ti->size());

            info.addPop(reg);
            info.code->add({BC_NOT,reg,reg});
            info.addPush(reg);
            
            *outTypeId = ltype;
        }else if(expression->typeId==AST_BNOT){
            int err = GenerateExpression(info,expression->left,&ltype);
            if(err==GEN_ERROR) return err;
            TypeInfo* ti = info.ast->getTypeInfo(ltype);
            u8 reg = RegBySize(1,ti->size());

            info.addPop(reg);
            info.code->add({BC_BNOT,reg,reg});
            info.addPush(reg);
            
            *outTypeId = ltype;
        }else if(expression->typeId==AST_CAST){
            int err = GenerateExpression(info,expression->left,&ltype);
            if(err==GEN_ERROR) return err;
            
            TypeId castType = expression->castType;
            TypeInfo* ti = info.ast->getTypeInfo(ltype);
            TypeInfo* tic = info.ast->getTypeInfo(castType);
            u8 lreg = RegBySize(1,ti->size());
            u8 creg = RegBySize(1,tic->size());
            if(
                // (AST::IsInteger(castType) && AST::IsInteger(ltype))
                (AST::IsPointer(castType)&& AST::IsPointer(ltype))
                ||(AST::IsPointer(castType)&& (ltype == (TypeId)AST_INT64||ltype==(TypeId)AST_UINT64||ltype==(TypeId)AST_INT32))
                ||((castType == (TypeId)AST_INT64||castType==(TypeId)AST_UINT64||ltype==(TypeId)AST_INT32) && AST::IsPointer(ltype))
            ) {
                info.addPop(lreg);
                info.addPush(creg);
                // data is fine as it is, just change the data type
            } else {
                bool yes = PerformSafeCast(info,ltype,castType);
                if(yes){
                
                } else {
                    ERRT(expression->tokenRange) << "cannot cast "<<info.ast->getTypeInfo(ltype)->name << " to "<<info.ast->getTypeInfo(castType)->name<<"\n";
                    *outTypeId = ltype; // ltype since cast failed
                    return GEN_ERROR;
                }
            }
            // } else if(AST::IsInteger(castType)&&ltype==AST_FLOAT32){
                
            //     info.addPop(lreg);
            //     info.code->add({BC_CASTI64,lreg,creg});
            //     info.addPush(creg);
            // } else if(castType == AST_FLOAT32 && AST::IsInteger(ltype)){
            //     info.addPop(lreg);
            //     info.code->add({BC_CASTF32,lreg,creg});
            //     info.addPush(creg);
            // } else {
            //     ERRT(expression->tokenRange) << "cannot cast "<<info.ast->getTypeInfo(ltype)->name << " to "<<info.ast->getTypeInfo(castType)->name<<"\n";
            //     *outTypeId = ltype; // ltype since cast failed
            //     return GEN_ERROR;
            // }
            
            //  float f = 7.29;
            // u32 raw = *(u32*)&f;
            // log::out << " "<<(void*)raw<<"\n";
            
            // u32 sign = raw>>31;
            // u32 expo = ((raw&0x80000000)>>(23));
            // u32 mant = ((raw&&0x80000000)>>(23));
            // log::out << sign << " "<<expo << " "<<mant<<"\n";
            
            *outTypeId = castType; // NOTE: we use castType and not ltype
        } else if(expression->typeId==AST_FROM_NAMESPACE){
            info.code->addDebugText("ast-namespaced expr\n");

            auto iden = info.getIdentifier(*expression->name);
            if(!iden||iden->type!=GenInfo::Identifier::NAMESPACE){
                ERRT(expression->tokenRange) << *expression->name <<" is not a namespace\n";
                return GEN_ERROR;
            }
            auto astNS = info.getNamespace(iden->index)->astNamespace;
            ScopeId savedScope = info.currentScopeId;
            info.currentScopeId = astNS->scopeId;
            TypeId exprId;
            int result = GenerateExpression(info,expression->left,&exprId);
            if(result!=GEN_SUCCESS) return result;
            
            *outTypeId = exprId;
            info.currentScopeId = savedScope;
        } else if(expression->typeId==AST_MEMBER){
            // TODO: if propfrom is a variable we can directly take the member from it
            //  instead of pushing struct to the stack and then getting it.
            
            info.code->addDebugText("ast-member expr\n");
            TypeId exprId;
            int result = GenerateExpression(info,expression->left,&exprId);
            if(result!=GEN_SUCCESS) return result;
            
            TypeInfo* typeInfo = info.ast->getTypeInfo(exprId);
            if(!typeInfo){
                log::out.setMasterColor(log::RED);
                log::out << "AST (partly):\n";
                expression->print(info.ast,1);
                log::out.setMasterColor(log::NO_COLOR);
                ERRT(expression->tokenRange) << "Type info was null (bug in compiler?)\n";
                return GEN_ERROR;
            }
            if(typeInfo->astStruct){
                auto meminfo = typeInfo->getMember(*expression->name);
                
                if(meminfo.index==-1){
                    *outTypeId = AST_VOID;
                    ERRT(expression->tokenRange) << *expression->name<<" is not a member of struct "<<info.ast->getTypeInfo(ltype)->name<<"\n";
                    return GEN_ERROR;
                }
                
                ltype = meminfo.typeId;
                
                int freg = 0;
                info.code->addDebugText("ast-member extract\n");
                for(int i=0;i<(int)typeInfo->astStruct->members.size();i++){
                    auto& member = typeInfo->astStruct->members[i];
                    TypeInfo* minfo = info.ast->getTypeInfo(member.typeId);
                    if(i==meminfo.index){
                        freg = RegBySize(1,minfo->size());                    
                        info.addPop(freg);
                    }else{
                        int reg = RegBySize(4,minfo->size());
                        info.addPop(reg);
                    }
                }
                info.addPush(freg);
                
                *outTypeId = ltype;
            } else if (typeInfo->astEnum){
                int enumValue;
                bool found = typeInfo->astEnum->getMember(*expression->name, &enumValue);
                if(!found){
                    ERRT(expression->tokenRange) << expression->tokenRange.firstToken<<" is not a member of enum "<<typeInfo->astEnum->name<<"\n";
                    return GEN_ERROR;
                }

                info.code->add({BC_LI,BC_REG_EAX}); // NOTE: fixed size of 4 bytes for enums?
                info.code->addIm(enumValue);
                info.addPush(BC_REG_EAX);

                *outTypeId = exprId;
                // *outTypeId = AST_INT32;
            } else {
                *outTypeId = AST_VOID;
                ERRT(expression->tokenRange) << "member access only works on structs and enums. "<<info.ast->getTypeInfo(exprId)->name<<" isn't one (astStruct/Enum was null at least)\n";
                return GEN_ERROR;
            }

        }else if(expression->typeId==AST_INITIALIZER){
            
            TypeInfo* structInfo = info.ast->getTypeInfo(info.currentScopeId, *expression->name);
            if(!structInfo||!structInfo->astStruct){
                ERRT(expression->tokenRange) << "cannot do initializer on non-struct "<<log::GOLD<<*expression->name<<"\n";
                return GEN_ERROR;   
            }

            ASTStruct* astruct = structInfo->astStruct;
            
            // store expressions in a list so we can iterate in reverse
            // TODO: parser could arrange the expressions in reverse.
            //   it would probably be easier since it constructs the nodes
            //   and has a little more context and control over it.
            std::vector<ASTExpression*> exprs;
            exprs.resize(astruct->members.size(),nullptr);

            ASTExpression* nextExpr=expression->left;
            int index = -1;
            while (nextExpr){
                ASTExpression* expr = nextExpr;
                nextExpr = nextExpr->next;
                index++;

                if(!expr->namedValue){
                    if((int)exprs.size()<=index){
                        ERRT(expr->tokenRange) << "To many members for struct "<<astruct->name<<" ("<<astruct->members.size()<<" member(s) allowed)\n";
                        continue;
                    }else
                        exprs[index] = expr;
                } else {
                    int memIndex=-1;
                    for(int i=0;i<(int)astruct->members.size();i++){
                        if(astruct->members[i].name == *expr->namedValue){
                            memIndex = i;
                            break;
                        }
                    }
                    if(memIndex==-1){
                        ERRT(expr->tokenRange) << *expr->namedValue<<" is not an member in "<<astruct->name<<"\n";
                        continue;
                    } else {
                        if(exprs[memIndex]){
                            ERRT(expr->tokenRange) << "argument for "<<astruct->members[memIndex].name <<" is already specified at "<<LOGAT(exprs[memIndex]->tokenRange)<<"\n";
                        }else{
                            exprs[memIndex] = expr;
                        }
                    }
                }
            }

            for(int i=0;i<(int)astruct->members.size();i++){
                auto& mem = astruct->members[i];
                if(!exprs[i])
                    exprs[i] = mem.defaultValue;
            }

            index = (int)exprs.size();
            while(index>0){
                index--;
                ASTExpression* expr = exprs[index];
                if(!expr){
                    ERRT(expression->tokenRange) << "missing argument for "<<astruct->members[index].name<< " (call to "<<astruct->name<<")\n";
                    continue;
                }

                TypeId exprId;
                int result = GenerateExpression(info,expr,&exprId);
                if(result!=GEN_SUCCESS) return result;
                
                if(index>=(int)structInfo->astStruct->members.size()){
                    // ERR() << "To many arguments! func:"<<*expression->funcName<<" max: "<<func->astFunc->arguments.size()<<"\n";
                    continue;
                }
                if(!PerformSafeCast(info,exprId,structInfo->astStruct->members[index].typeId)){ // implicit conversion
                // if(func->astFunc->arguments[index].typeId!=dt){ // strict, no conversion
                    ERRTYPE(expr->tokenRange,exprId,structInfo->astStruct->members[index].typeId) << "(initializer)\n";
                    continue;
                }
            }
            // if((int)exprs.size()!=(int)structInfo->astStruct->members.size()){
            //     ERRT(expression->tokenRange) << "Found "<<exprs.size()<<" initializer values but "<<*expression->name<<" requires "<<structInfo->astStruct->members.size()<<"\n";   
            //     log::out <<log::RED<< "LN "<<expression->tokenRange.firstToken.line <<": "; expression->tokenRange.print();log::out << "\n";
            //     // return GEN_ERROR;
            // }
            
            *outTypeId = structInfo->id;
        } else if(expression->typeId==AST_SLICE_INITIALIZER){
            
            // TypeInfo* typeInfo = info.ast->getTypeInfo(*expression->name);
            // if(!structInfo||!structInfo->astStruct){
            //     ERRT(expression->tokenRange) << "cannot do initializer on non-struct "<<log::GOLD<<*expression->name<<"\n";
            //     return GEN_ERROR;   
            // }
            
            // int index = (int)exprs.size();
            // // int index=-1;
            // while(index>0){
            //     index--;
            //     ASTExpression* expr = exprs[index];
            //     TypeId exprId;
            //     int result = GenerateExpression(info,expr,&exprId);
            //     if(result!=GEN_SUCCESS) return result;
                
            //     if(index>=(int)structInfo->astStruct->members.size()){
            //         // ERR() << "To many arguments! func:"<<*expression->funcName<<" max: "<<func->astFunc->arguments.size()<<"\n";
            //         continue;
            //     }
            //     if(!PerformSafeCast(info,exprId,structInfo->astStruct->members[index].typeId)){ // implicit conversion
            //     // if(func->astFunc->arguments[index].typeId!=dt){ // strict, no conversion
            //         ERRT(expr->tokenRange) << "Type mismatch (initializer): "<<*info.ast->getTypeInfo(exprId)<<" - "<<*info.ast->getTypeInfo(structInfo->astStruct->members[index].typeId)<<"\n";
            //         continue;
            //     }
            // }
            // if((int)exprs.size()!=(int)structInfo->astStruct->members.size()){
            //     ERRT(expression->tokenRange) << "Found "<<index<<" arguments but "<<*expression->name<<" requires "<<structInfo->astStruct->members.size()<<"\n";   
            //     // return GEN_ERROR;
            // }
            
            // *outTypeId = structInfo->id;
        } else if(expression->typeId==AST_FROM_NAMESPACE){
            // TODO: chould not work on enums? Enum.One is used instead
            // TypeInfo* typeInfo = info.ast->getTypeInfo(*expression->name);
            // if(!typeInfo||!typeInfo->astEnum){
            //     ERRT(expression->tokenRange) << "cannot access "<<(expression->member?*expression->member:"?")<<" from non-enum "<<*expression->name<<"\n";
            //     return GEN_ERROR;
            // }
            // Assert(expression->member)
            // int index=-1;
            // for(int i=0;i<(int)typeInfo->astEnum->members.size();i++){
            //     if(typeInfo->astEnum->members[i].name == *expression->member) {
            //         index = i;
            //         break;
            //     }
            // }
            // if(index!=-1){
            //     info.code->add({BC_LI,BC_REG_EAX});
            //     info.code->addIm(typeInfo->astEnum->members[index].enumValue);
            //     info.addPush(BC_REG_EAX);

            //     *outTypeId = AST_INT32;   
            // }
        }else{
            // basic operations
            TypeId rtype;
            int err = GenerateExpression(info,expression->left,&ltype);
            if(err==GEN_ERROR) return err;
            err = GenerateExpression(info,expression->right,&rtype);
            if(err==GEN_ERROR) return err;

            if(AST::IsPointer(ltype)&&rtype==AST_INT32){
                if(expression->typeId==AST_ADD){
                    TOKENINFO(expression->tokenRange)
                    info.addPop(BC_REG_EAX); // int
                    info.addPop(BC_REG_RDX); // pointer

                    info.code->add({BC_ADDI,BC_REG_EAX,BC_REG_RDX,BC_REG_RAX});
                    *outTypeId = ltype;
                    info.addPush(BC_REG_RAX);
                    return GEN_SUCCESS;
                }else{
                    log::out << log::RED<<"GenExpr: operation not implemented\n";
                    *outTypeId = AST_VOID;
                    return GEN_ERROR;
                }
            }

            if(ltype!=rtype){
                ERRTYPE(expression->tokenRange,ltype,rtype) << "\n";
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }
            TOKENINFO(expression->tokenRange)
            TypeInfo* lInfo = info.ast->getTypeInfo(ltype);
            TypeInfo* rInfo = info.ast->getTypeInfo(rtype);
            u8 reg1 = RegBySize(4,lInfo->size()); // get the appropriate registers
            u8 reg2 = RegBySize(1,rInfo->size());
            info.addPop(reg2); // note that right expression should be popped first
            info.addPop(reg1);
            
            #define GEN_OP(X,Y) if(expression->typeId==AST_##X) info.code->add({Y,reg1,reg2,reg2});
            if(ltype==AST_FLOAT32){
                GEN_OP(ADD,BC_ADDF)
                else GEN_OP(SUB,BC_SUBF)
                else GEN_OP(MUL,BC_MULF)
                else GEN_OP(DIV,BC_DIVF)
                else GEN_OP(MODULUS,BC_MODF)
                else
                    // TODO: do OpToStr first and then if it fails print the typeId number
                    log::out << log::RED<<"GenExpr: operation "<<expression->typeId<<" not implemented\n";    
            }else{
                GEN_OP(ADD,BC_ADDI)
                else GEN_OP(SUB,BC_SUBI)
                else GEN_OP(MUL,BC_MULI)
                else GEN_OP(DIV,BC_DIVI)
                else GEN_OP(DIV,BC_DIVI)
                else GEN_OP(MODULUS,BC_MODI)

                else GEN_OP(EQUAL,BC_EQ)
                else GEN_OP(NOT_EQUAL,BC_NEQ)
                else GEN_OP(LESS,BC_LT)
                else GEN_OP(LESS_EQUAL,BC_LTE)
                else GEN_OP(GREATER,BC_GT)
                else GEN_OP(GREATER_EQUAL,BC_GTE)
                else GEN_OP(AND,BC_ANDI)
                else GEN_OP(OR,BC_ORI)

                else GEN_OP(BAND,BC_BAND)
                else GEN_OP(BOR,BC_BOR)
                else GEN_OP(BXOR,BC_BXOR)
                else GEN_OP(BNOT,BC_BNOT)
                else GEN_OP(BLSHIFT,BC_BLSHIFT)
                else GEN_OP(BRSHIFT,BC_BRSHIFT)
                else
                    // TODO: do OpToStr first and then if it fails print the typeId number
                    log::out << log::RED<<"GenExpr: operation "<<expression->typeId<<" not implemented\n";
            }
            #undef GEN_OP
            *outTypeId = ltype;
            
            info.addPush(reg2);
            // info.code->add({BC_PUSH,BC_REG_RAX});
        }
    }
    return GEN_SUCCESS;
}
int GenerateDefaultValue(GenInfo& info, TypeInfo* typeInfo){
    Assert(typeInfo)
    if(typeInfo->astStruct){
        for(int i=typeInfo->astStruct->members.size()-1;i>=0;i--){
            auto& member = typeInfo->astStruct->members[i];
            if(member.defaultValue){
                TypeId typeId = 0;
                int result = GenerateExpression(info,member.defaultValue,&typeId);
                // TODO: do type check in EvaluateTypes
                if(member.typeId != typeId){
                    ERRTYPE(member.defaultValue->tokenRange,member.typeId,typeId) << "(default member)\n";
                }
            } else {
                int result = GenerateDefaultValue(info,info.ast->getTypeInfo(member.typeId));
            }
        }
    } else {
        info.code->add({BC_BXOR,BC_REG_RAX,BC_REG_RAX,BC_REG_RAX});
        int sizeLeft = typeInfo->size();
        while(sizeLeft>0){
            int reg = 0;
            if(sizeLeft>=8){
                reg = RegBySize(1,8);
                sizeLeft-=8;
            }else if(sizeLeft>=4){
                reg = RegBySize(1,4);
                sizeLeft-=4;
            }else if(sizeLeft>=2){
                reg = RegBySize(1,2);
                sizeLeft-=2;
            }else if(sizeLeft>=1){
                reg = RegBySize(1,1);
                sizeLeft-=1;
            }
            
            Assert(reg)
            info.addPush(reg);
        }
    }
    return GEN_SUCCESS;
}
int GenerateBody(GenInfo& info, ASTScope* body){
    using namespace engone;
    _GLOG(FUNC_ENTER)
    Assert(body)

    ScopeId savedScope = info.currentScopeId;
    defer {info.currentScopeId = savedScope; };

    info.currentScopeId = body->scopeId;

    //-- Begin by generating functions
    ASTFunction* nextFunction = body->functions;
    while(nextFunction){
        ASTFunction* function = nextFunction;
        nextFunction = nextFunction->next;
        
        _GLOG(SCOPE_LOG("FUNCTION"))

        int lastOffset=info.currentFrameOffset;
        
        auto func = info.addFunction(function->name);
        if(!func){
            ERRT(function->tokenRange) << function->name<<" is already defined\n";
            if(function->tokenRange.firstToken.str){
                ERRTOKENS(function->tokenRange)
            }
            continue;
        }
        func->astFunc = function;
        // TODO: What if variable name is the same as function variable?
        
        info.code->add({BC_JMP});
        int skipIndex=info.code->length();
        info.code->addIm(0);
        
        info.currentFunction = function;
        func->address = info.code->length();

        int startSP = info.saveStackMoment();

        // expecting 8-bit alignment when generating function
        Assert(info.relativeStackPointer%8==0)

        // TODO: FIX ALIGNMENT HERE
        
        if(func->astFunc->arguments.size()!=0){
            _GLOG(log::out << "set "<<func->astFunc->arguments.size()<<" args\n");
            // int offset = 0;
            for(int i = 0;i<(int)func->astFunc->arguments.size();i++){
            // for(int i = func->astFunc->arguments.size()-1;i>=0;i--){
                auto& arg = func->astFunc->arguments[i];
                auto var = info.addVariable(arg.name);
                if(!var){
                    ERRT(func->astFunc->tokenRange) << arg.name << " already exists\n";
                }
                var->typeId = arg.typeId;
                TypeInfo* typeInfo = info.ast->getTypeInfo(arg.typeId);

                var->frameOffset = GenInfo::ARG_OFFSET + arg.offset;
                _GLOG(log::out << " "<<arg.name<<": "<<var->frameOffset<<"\n";)
            }
            _GLOG(log::out<<"\n";)
        }
        // info.currentFrameOffset = 0;
        if(func->astFunc->returnTypes.size()!=0){
            _GLOG(log::out << "space for "<<func->astFunc->returnTypes.size()<<" return value(s) (struct may cause multiple push)\n");
            // TODO: handle default values for structs?
            //   currently doing zero initialization
            // info.code->add({BC_BXOR,BC_REG_RAX,BC_REG_RAX,BC_REG_RAX});
            // for(int i=0;i<(int)func->astFunc->returnTypes.size();i++){
            //     auto& typeId = func->astFunc->returnTypes[i].typeId;
            //     TypeInfo* typeInfo = info.ast->getTypeInfo(typeId);
                
            //     char buf[100];
            //     int len = sprintf(buf,"  ret value %s",info.ast->getTypeInfo(typeId)->name.c_str());
            //     info.code->addDebugText(buf,len);

            //     // space for each return value
            //     int reg = RegBySize(1,typeInfo->size());
            //     if(!typeInfo->astStruct){
            //         info.code->add({BC_LI,BC_REG_RBX});
            //         info.code->addIm();
            //         info.code->add({BC_ADDI,BC_REG_RBX,BC_REG_FP,BC_REG_RBX});
            //         info.code->add({BC_MOV_RM, {});
            //         info.addPush(reg);
            //         // info.currentFrameOffset-=8;
            //     }else{
            //         len += sprintf(buf+len,"\n");
            //         for(int i=typeInfo->astStruct->members.size()-1;i>=0;i--){
            //             auto& member = typeInfo->astStruct->members[i];
            //             len += sprintf(buf+len,"    member %s",member.name.c_str());
            //             info.code->addDebugText(buf,len);
            //             info.addPush(reg);
            //             // info.currentFrameOffset-=8;
            //             len=0;
            //         }
            //     }
            // }
            info.code->addDebugText("ZERO init return values\n");
            info.code->add({BC_LI,BC_REG_RBX});
            info.code->addIm(-func->astFunc->returnSize);
            info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RBX,BC_REG_RBX});
            u16 size = func->astFunc->returnSize;
            info.code->add({BC_ZERO_MEM,BC_REG_RBX,(u8)(0xFF&size),(u8)(size<<8)});
            _GLOG(log::out<<"\n";)
            info.currentFrameOffset -= func->astFunc->returnSize; // TODO: size can be uneven like 13. FIX IN EVALUATETYPES
        }
        
        // TODO: MAKE SURE SP IS ALIGNED TO 8 BYTES, 16 could work to.
        // SHOULD stackAlignment, relativeStackPointer be reset and then restored?
        // ALSO DO IT BEFORE CALLING FUNCTION (FNCALL)
        // 
        int result = GenerateBody(info,function->body);
        info.currentFrameOffset = lastOffset;

        info.restoreStackMoment(startSP);

        // log::out << *function->name<<" " <<function->returnTypes.size()<<"\n";
        if(function->returnTypes.size()!=0){
            bool foundReturn = false;
            ASTStatement* nextState = function->body->statements;
            while(nextState){
                if(nextState->type==ASTStatement::RETURN){
                    foundReturn = true;
                }
                nextState = nextState->next;
            }
            if(!foundReturn){
                ERR()<<"missing return statement in "<<function->name<<"\n";
            }
        }

        info.code->add({BC_RET});
        *((u32*)info.code->get(skipIndex)) = info.code->length();

        for(auto& arg : func->astFunc->arguments){
            info.removeIdentifier(arg.name);
        }
    }
    int lastOffset=info.currentFrameOffset;
    
    ASTScope* nextSpace = body->namespaces;
    while(nextSpace){
        ASTScope* space = nextSpace;
        nextSpace = nextSpace->next;
        // TODO: There are some serious issues with identifiers and namespaces.
        //   If a variable or functions uses the same name things could be overwritten
        //   and identifiers are kind of global so namespace nesting of the same name
        //   doesn't work. If these flaws are okay then you can make things more robust
        //   here because I haven't taken care of some cases. If the flaws aren't okay
        //   then you would need to rethink how namespaces are accessed. Identifier system
        //   can not be used for namespaces.
        auto id = info.getIdentifier(*space->name);
        if(!id){
            auto ns = info.addNamespace(*space->name);
            ns->astNamespace = space;
            continue;
        }
        if(id->type!=GenInfo::Identifier::NAMESPACE){
            ERRT(space->tokenRange) << "Identifier "<<*space->name<<" is declared as a "<<(id->type==GenInfo::Identifier::VAR?"variable":"function")<<"\n";
            continue;
        }
        ERRT(space->tokenRange) << "multiple namespaces not implemented!\n";
        // TODO: Do you allow multiple space by using a vector in Namespace struct?
        //   How can you access types from multiple scopes at the same time? (scopeId)
    }

    ASTStatement* nextStatement = body->statements;
    while(nextStatement){
        ASTStatement* statement = nextStatement;
        nextStatement = nextStatement->next;

        if(statement->type==ASTStatement::ASSIGN){
            _GLOG(SCOPE_LOG("ASSIGN"))
            TypeId etype=0;
            if(!statement->rvalue){
                auto id = info.getIdentifier(*statement->name);
                if(id){
                    ERRT(statement->tokenRange) << "Identifier "<<*statement->name<<" already declared\n";
                    continue;
                }
                // declare new variable at current stack pointer
                GenInfo::Variable* var = info.addVariable(*statement->name);

                // typeid comes from current scope since we do "var: Type;"
                var->typeId = statement->typeId;
                TypeInfo* typeInfo = info.ast->getTypeInfo(var->typeId);
                if(!typeInfo||typeInfo->size()==0){
                    ERRT(statement->tokenRange) << "Undefined type ";
                    if(typeInfo)
                        log::out << typeInfo->name;
                    log::out << " (the size was 0 which isn't allowed)\n";
                    continue; 
                }

                // data type may be zero if it wasn't specified during initial assignment
                // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                
                int size = typeInfo->size();

                int diff = size-(-info.currentFrameOffset)%size; // how much to fix alignment
                if(diff!=size){
                    info.currentFrameOffset-=diff; // align
                }
                info.currentFrameOffset-=size;
                var->frameOffset = info.currentFrameOffset;
                
                // log::out << count <<" "<<(typeInfo->size+7)<<"\n";
                // TODO: default values?
                
                // if(typeInfo->astStruct){
                TOKENINFO(statement->tokenRange)
                int result = GenerateDefaultValue(info,typeInfo);
                // } else {
                
                // careful with sp and push, push will increment sp
                
                // char buf[100];
                // int len = sprintf(buf,"  declare %s",statement->name->c_str());
                // info.code->addDebugText(buf,len);
                // info.code->add({BC_BXOR,BC_REG_RAX,BC_REG_RAX,BC_REG_RAX});
                // int sizeLeft = size;
                // while(sizeLeft>0){
                //     int reg = 0;
                //     if(sizeLeft>=8){
                //         reg = RegBySize(1,8);
                //         sizeLeft-=8;
                //     }else if(sizeLeft>=4){
                //         reg = RegBySize(1,4);
                //         sizeLeft-=4;
                //     }else if(sizeLeft>=2){
                //         reg = RegBySize(1,2);
                //         sizeLeft-=2;
                //     }else if(sizeLeft>=1){
                //         reg = RegBySize(1,1);
                //         sizeLeft-=1;
                //     }
                    
                //     Assert(reg)
                //     info.addPush(reg);
                // }
                continue;
            }
            int result = GenerateExpression(info, statement->rvalue,&etype);
            if(result!=GEN_SUCCESS) {
                continue;
                // return result; 
            }
            auto id = info.getIdentifier(*statement->name);
            GenInfo::Variable* var = 0;
            if(id){
                if(id->type==GenInfo::Identifier::VAR){
                    var = info.getVariable(id->index);
                }else{
                    ERRT(statement->tokenRange) << "identifier "<<*statement->name<<" was not a variable\n";
                    continue;
                }
            }
            if(result==GEN_ERROR) {

            } else if(etype==AST_VOID){
                ERRT(statement->tokenRange) << "datatype for assignment cannot be void\n";
            } else if(!id||id->type==GenInfo::Identifier::VAR) {
                if(!(!var && statement->typeId==AST_VOID) &&
                    (!var && !PerformSafeCast(info,etype,statement->typeId)) &&
                    (var && !PerformSafeCast(info,etype,var->typeId))
                    ) {
                    if(var){
                        ERRTYPE(statement->tokenRange,var->typeId,etype) << "(assign)\n";
                    } else {
                        ERRTYPE(statement->tokenRange,statement->typeId,etype) << "(assign)\n";
                    }
                    ERRTOKENS(statement->tokenRange);
                    continue;
                } else {
                    // if(statement->typeId!=AST_VOID)
                    //     etype = statement->typeId;
                // if(dtype==statement->typeId || statement->typeId==AST_VOID){
                    // TODO: nonetype is only valid for as implicit initial decleration.
                    //    Not allowed afterwards? or maybe it's find we just check variable type
                    //    not looking at the statement data type?
                    // TODO: Something special needs to happen if variable is declared
                    //   and you try to assign using a type (a: i32 = 9 where a is declared isn't handled)
                    bool decl = !var;
                    if(!var){
                        var = info.addVariable(*statement->name);
                        if(statement->typeId!=AST_VOID)
                            var->typeId = statement->typeId;
                        else
                            var->typeId = etype;
                        // data type may be zero if it wasn't specified during initial assignment
                        // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                        
                        /*
                        If you are wondering whether frameOffset will collide with other push and pops
                        it won't. push, pop occurs in expressions which are temporary. The only non temporary push
                        is when we do an assignment.
                        */
                    }
                    // etype comes from gen expression, does the type exist in our scope?
                    TypeInfo* eInfo = info.ast->getTypeInfo(etype);
                    // variables can be accessed from current or any parent scope.
                    // the type should therefore also exist in one of those scopes.
                    TypeInfo* varInfo = info.ast->getTypeInfo(var->typeId);
                    int alignment = 0;
                    if(decl){
                        // new declaration
                        // already on the stack, no need to pop and push
                        // info.code->add({BC_POP,BC_REG_RAX});
                        // info.code->add({BC_PUSH,BC_REG_RAX});

                        // TODO: Actually pop and BC_MOV is needed because the push doesn't
                        // have the proper offsets of the struct. (or maybe it does?)
                        // look into it.

                        if(varInfo->size()==0){
                            ERRT(statement->tokenRange) << "Size of type "<<varInfo->name << " was 0\n";
                            continue;
                        }
                        
                        int size = varInfo->size();
                        int asize = varInfo->alignedSize();
                        int diff = (size-(-info.currentFrameOffset)%asize)%asize; // how much to fix alignment
                        if(diff){
                            info.currentFrameOffset-=diff; // align
                        }
                        info.currentFrameOffset-=size;
                        var->frameOffset = info.currentFrameOffset;
                        alignment = diff;

                        _GLOG(log::out<<"declare "<<*statement->name<<" at "<<var->frameOffset<<"\n";)
                        // NOTE: inconsistent
                        // char buf[100];
                        // int len = sprintf(buf," ^ was assigned %s",statement->name->c_str());
                        // info.code->addDebugText(buf,len);
                    }
                    // else{
                        // local variable exists on stack
                        char buf[100];
                        int len = sprintf(buf,"  assign %s\n",statement->name->c_str());
                        info.code->addDebugText(buf,len);
                        if(!varInfo->astStruct){
                            u8 ereg = RegBySize(4,eInfo->size());
                            u8 vreg = RegBySize(4,varInfo->size());
                            info.addPop(ereg);
                            info.code->add({BC_LI,BC_REG_RBX});
                            info.code->addIm(var->frameOffset);
                            info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RBX,BC_REG_RBX}); // rbx = fp + offset
                            info.code->add({BC_MOV_RM,vreg,BC_REG_RBX});
                        }else{
                            // for(int i=varInfo->astStruct->members.size()-1;i>=0;i--){
                            for(int i=0;i<(int)varInfo->astStruct->members.size();i++){
                                auto& member = varInfo->astStruct->members[i];
                                // TODO: struct within struct?
                                TypeInfo* memInfo = info.ast->getTypeInfo(member.typeId);
                                u8 reg = RegBySize(4,memInfo->size());
                                info.addPop(reg);
                                info.code->add({BC_LI,BC_REG_RBX});
                                info.code->addIm(var->frameOffset + member.offset);
                                info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RBX,BC_REG_RBX}); // rbx = fp + offset
                                info.code->add({BC_MOV_RM,reg,BC_REG_RBX});
                            }
                        }
                        if(decl){
                            info.code->addDebugText("incr sp after decl\n");

                            info.addIncrSp(-varInfo->size() - alignment);
                        }
                    // }
                    // if(decl){
                    // move forward stack pointer
                    //     info.code->add({BC_LI,BC_REG_RCX});
                    //     info.code->addIm(8);
                    //     info.code->add({BC_ADDI,BC_REG_SP,BC_REG_RCX,BC_REG_SP});
                    // }
                }
                // else{
                //     if(var){
                //         ERRTYPE(statement->tokenRange,var->typeId,etype) << "(assign)\n";
                //     } else {
                //         ERRTYPE(statement->tokenRange,statement->typeId,etype) << "(assign)\n";
                //     }
                //     ERRTOKENS(statement->tokenRange);
                //     continue;
                //     // TODO: Show were in the code
                // }
            }
            _GLOG(log::out << " "<<*statement->name << " : "<<info.ast->getTypeInfo(var->typeId)->name<<"\n";)
        } else if(statement->type==ASTStatement::PROP_ASSIGN){
            _GLOG(SCOPE_LOG("PROP_ASSIGN"))
            
            if((statement->opType == AST_ADD ||statement->opType==AST_SUB||
                   statement->opType==AST_MUL||statement->opType==AST_DIV)){
                if(statement->lvalue->typeId != AST_VAR){
                        ERRL(statement->lvalue->tokenRange) << "+= like operation is limited to variables ";
                        if(statement->lvalue->typeId == AST_MEMBER)
                            log::out << "(member is not implemented)";
                        if(statement->lvalue->typeId == AST_DEREF)
                            log::out << "(dereference is not implemented)";
                        log::out << "\n";
                        ERRTOKENS(statement->tokenRange)
                        continue;
                }
                
                auto iden = info.getIdentifier(*statement->lvalue->name);
                if(!iden || iden->type != GenInfo::Identifier::VAR){
                    ERRT(statement->lvalue->tokenRange) << *statement->lvalue->name << " is not a variable\n";
                    continue;
                }
                auto var = info.getVariable(iden->index);
                
                info.code->addDebugText("prop-assign (var)\n");
                TypeId rtype=0;
                int result = GenerateExpression(info, statement->rvalue,&rtype);
                if(result!=GEN_SUCCESS) {
                    continue;
                }
                
                if(!IsSafeCast(var->typeId, rtype)){
                    ERRTYPE(statement->tokenRange,var->typeId,rtype) << "\n";
                    continue;
                }
                TypeInfo* linfo = info.ast->getTypeInfo(var->typeId);
                TypeInfo* rinfo = info.ast->getTypeInfo(rtype);
                
                u8 lreg = RegBySize(1,linfo->size()); // reg a
                u8 rreg = RegBySize(4,rinfo->size()); // reg d
                
                info.code->add({BC_LI,BC_REG_RBX});
                info.code->addIm(var->frameOffset);
                info.code->add({BC_ADDI,BC_REG_RBX,BC_REG_FP,BC_REG_RBX});
                
                info.code->add({BC_MOV_MR, BC_REG_RBX, lreg});
                info.addPop(rreg);
                
                u8 op = 0;
                if(var->typeId == AST_FLOAT32) {
                    switch(statement->opType){
                        case AST_ADD: op = BC_ADDF; break;
                        case AST_SUB: op = BC_SUBF; break;
                        case AST_MUL: op = BC_MULF; break;
                        case AST_DIV: op = BC_DIVF; break;
                    }
                } else {
                    switch(statement->opType){
                        case AST_ADD: op = BC_ADDI; break;
                        case AST_SUB: op = BC_SUBI; break;
                        case AST_MUL: op = BC_MULI; break;
                        case AST_DIV: op = BC_DIVI; break;
                    }
                }
                info.code->add({op,lreg,rreg,lreg});
                info.code->add({BC_MOV_RM,lreg,BC_REG_RBX});
            } else if(statement->lvalue->typeId==AST_DEREF){
                TypeId dtype=0;
                int result = GenerateExpression(info, statement->lvalue->left,&dtype);
                if(result!=GEN_SUCCESS) {
                    // return result;
                    continue;
                }
                
                if(!AST::IsPointer(dtype)){
                    ERRT(statement->lvalue->tokenRange) << "expected pointer type for deref assignment not "<<info.ast->getTypeInfo(dtype)->name<<"\n";
                    continue;
                    // return GEN_ERROR;
                }
                
                info.code->addDebugText("prop-assign\n");
                TypeId rtype=0;
                result = GenerateExpression(info, statement->rvalue,&rtype);
                if(result!=GEN_SUCCESS) {
                    continue;
                    // return result;
                }
                
                TypeInfo* typeInfo = info.ast->getTypeInfo(rtype);
                u8 reg = RegBySize(1,typeInfo->size());
                info.addPop(reg);
                info.addPop(BC_REG_RBX);
                info.code->add({BC_MOV_RM,reg,BC_REG_RBX});
                
            } else if(statement->lvalue->typeId==AST_MEMBER){
                //-- validate left side
                std::vector<ASTExpression*> members;
                members.push_back(statement->lvalue);
                auto varexpr = statement->lvalue->left; // isn't variable expr yet but should be after while loop
                while(true){
                    if(varexpr){
                        // TODO: handle deref and pointers
                        if(varexpr->typeId == AST_VAR) {
                            break;
                        } else if(varexpr->typeId == AST_MEMBER) {
                            members.push_back(varexpr);
                            varexpr = varexpr->left;
                            continue;
                        }
                    }
                    // TODO: does varexpr->typeid come from current scope?
                    auto varti = info.ast->getTypeInfo(varexpr->typeId);
                    ERRT(statement->lvalue->tokenRange) << "expected variable when doing prop assign not "<<(varti ? varti->name : "")<<"\n";
                    ERRTOKENS(statement->tokenRange)
                    return GEN_ERROR; // TODO: don't return, continue instead
                }
                    
                auto id = info.getIdentifier(*varexpr->name);
                if(!id||id->type!=GenInfo::Identifier::VAR){
                    ERRT(varexpr->tokenRange) << *varexpr->name<<" is not a variable\n";
                    continue;
                    // return GEN_ERROR;
                }
                auto var = info.getVariable(id->index);
                auto typeInfo = info.ast->getTypeInfo(var->typeId);
                if(!typeInfo || !typeInfo->astStruct){
                    ERRT(varexpr->tokenRange) << *varexpr->name<<" is not a struct, can't do prop assignment\n";
                    continue;
                    // return GEN_ERROR;
                }
                int memberOffset = 0;
                ASTStruct::Member* member = 0;
                for (int i=members.size()-1;i>=0;i--) {
                    auto expr = members[i];
                    auto meminfo = typeInfo->getMember(*expr->name);
                    if(meminfo.index==-1){
                        ERRT(expr->tokenRange) << *expr->name<<" is not a member of "<<typeInfo->astStruct->name<<"\n";
                        return GEN_ERROR; // TODO: continue instead
                    }
                    
                    member = &typeInfo->astStruct->members[meminfo.index];
                    typeInfo = info.ast->getTypeInfo(meminfo.typeId);
                    
                    memberOffset += member->offset;
                }
                
                //-- generate right side
                info.code->addDebugText("prog-assign member expr\n");
                TypeId typeId;
                int result = GenerateExpression(info,statement->rvalue,&typeId);
                if(result!=GEN_SUCCESS) {
                    continue;
                    // return result;
                }
                
                    
                const std::string& memtypename = typeInfo->name;
                _GLOG(log::out << "prop type "<<memtypename<<"\n";)
                if(!PerformSafeCast(info,typeId,member->typeId)){
                    ERRT(statement->rvalue->tokenRange) << "cannot cast to "<<memtypename<<"\n";
                }

                info.code->addDebugText("prop-assign member push\n");
                //-- generate left side
                
                // TOOD: WHAT ABOUT STRUCT? doesn't fit in register
                u8 reg = RegBySize(1,typeInfo->size());
                info.addPop(reg);
                
                info.code->add({BC_LI,BC_REG_RBX});
                info.code->addIm(var->frameOffset + memberOffset);
                info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RBX,BC_REG_RBX});
                info.code->add({BC_MOV_RM,reg,BC_REG_RBX});
                
            } else {
                ERRT(statement->tokenRange) << "expected deref or prop for prop assignment, was "<<OpToStr(statement->lvalue->typeId)<<"\n";
                continue;
                // return GEN_ERROR;
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

            // info.code->add({BC_POP,BC_REG_RAX});
            TypeInfo* typeInfo = info.ast->getTypeInfo(dtype);
            if(!typeInfo){
                
                // TODO: Print error? or does generate expression do that since it gives us dtype?
                continue;   
            }
            u8 reg = RegBySize(1,typeInfo->size());
            
            info.addPop(reg);
            info.code->add({BC_JNE,reg});
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

            // info.code->add({BC_POP,BC_REG_RAX});
            TypeInfo* typeInfo = info.ast->getTypeInfo(dtype);
            u8 reg = RegBySize(1,typeInfo->size());
            
            info.addPop(reg);
            info.code->add({BC_JNE,reg});
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
                ERRT(statement->tokenRange) << "return only allowed in function\n";
                continue;
                // return GEN_ERROR;
            }
            
            //-- evaluate return values
            ASTExpression* expr = statement->rvalue;
            int argi=-1;
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
                ASTFunction::ReturnType& retType = info.currentFunction->returnTypes[argi];
                if(!PerformSafeCast(info,retType.typeId,dtype)){
                // if(info.currentFunction->returnTypes[argi]!=dtype){
                    
                    ERRTYPE(expr->tokenRange,dtype,info.currentFunction->returnTypes[argi].typeId) << "(return values)\n";
                }
                TypeInfo* typeInfo = info.ast->getTypeInfo(dtype);
                if(!typeInfo->astStruct){
                    _GLOG(log::out<<"move return value\n";)
                    u8 reg = RegBySize(1,typeInfo->size());
                    info.addPop(reg);
                    info.code->add({BC_LI,BC_REG_RBX});
                    info.code->addIm(retType.offset);
                    info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RBX,BC_REG_RBX});
                    info.code->add({BC_MOV_RM,reg,BC_REG_RBX});
                }else{
                    // offset-=typeInfo->astStruct->size;
                    for(int i=0;i<(int)typeInfo->astStruct->members.size();i++){
                        auto& member = typeInfo->astStruct->members[i];
                        auto meminfo = info.ast->getTypeInfo(member.typeId);
                        _GLOG(log::out<<"move return value member "<<member.name<<"\n";)
                        u8 reg = RegBySize(1,meminfo->size());
                        info.addPop(reg);
                        info.code->add({BC_LI,BC_REG_RBX});
                        // retType.offset is negative and member.offset is positive which is correct
                        info.code->addIm(retType.offset + member.offset);
                        info.code->add({BC_ADDI,BC_REG_FP,BC_REG_RBX,BC_REG_RBX});
                        info.code->add({BC_MOV_RM,reg,BC_REG_RBX});
                    }
                }
                _GLOG(log::out<<"\n";)
            }
            argi++; // incremented to get argument count
            if(argi!=(int)info.currentFunction->returnTypes.size()){
                ERRT(statement->tokenRange) << "Found "<<argi<<" return values but should have "<<info.currentFunction->returnTypes.size()<<" for '"<<info.currentFunction->name<<"'\n";   
            }
            
            // fix stack pointer before returning
            // info.code->add({BC_LI,BC_REG_RCX});
            // info.code->addIm(info.currentFrameOffset-lastOffset);
            // info.code->add({BC_ADDI,BC_REG_SP,BC_REG_RCX,BC_REG_SP});
            info.addIncrSp(info.currentFrameOffset-lastOffset);
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
            if(dtype!=AST_VOID){
                // TODO: handle struct
                TypeInfo* typeInfo = info.ast->getTypeInfo(dtype);
                u8 reg = RegBySize(1,typeInfo->size());
                info.addPop(reg);
            }
        } else if(statement->type == ASTStatement::USING){
            _GLOG(SCOPE_LOG("USING"))

            auto origin = info.getIdentifier(*statement->name);
            auto aliasId = info.addIdentifier(*statement->alias);

            aliasId->type = origin->type;
            aliasId->index = origin->index;
        } else if(statement->type == ASTStatement::BODY){
            _GLOG(SCOPE_LOG("BODY (statement)"))

            int result = GenerateBody(info, statement->body);
            // Is it okay to do nothing with result?
        }
    }
    if(lastOffset!=info.currentFrameOffset){
        _GLOG(log::out << "fix sp when exiting body\n";)
        info.code->addDebugText("fix sp when exiting body\n");
        // info.code->add({BC_LI,BC_REG_RCX});
        // info.code->addIm(lastOffset-info.currentFrameOffset);
        // info.addIncrSp(info.currentFrameOffset);
        info.addIncrSp(lastOffset-info.currentFrameOffset);
        // info.code->add({BC_ADDI,BC_REG_SP,BC_REG_RCX,BC_REG_SP});
        info.currentFrameOffset = lastOffset;
    }
    
    return GEN_SUCCESS;
}
Bytecode* Generate(AST* ast, int* err){
    using namespace engone;
    // _VLOG(log::out <<log::BLUE<<  "##   Generator   ##\n";)
    
    GenInfo info{};
    info.code = Bytecode::Create();
    info.ast = ast;
    info.currentScopeId = ast->globalScopeId;
    
    std::vector<ASTFunction*> predefinedFuncs;
    
    {
    auto fun = info.addFunction("alloc");
    fun->address = BC_EXT_ALLOC;
    
    auto astfun = (fun->astFunc = ast->createFunction("alloc"));
    // don't add to ast because GenerateBody will want to create the functions
    predefinedFuncs.push_back(astfun);
    astfun->returnTypes.push_back(ast->getTypeInfo(ast->globalScopeId,"void*")->id);
    astfun->arguments.push_back({});
    astfun->arguments.back().typeId = ast->getTypeInfo(ast->globalScopeId,"u64")->id;
    astfun->arguments.back().name = "size";
    }{
    auto fun = info.addFunction("realloc");
    fun->address = BC_EXT_REALLOC;
    
    auto astfun = (fun->astFunc = ast->createFunction("realloc"));
    predefinedFuncs.push_back(astfun);
    astfun->returnTypes.push_back(ast->getTypeInfo(ast->globalScopeId,"void*")->id);
    astfun->arguments.push_back({});
    astfun->arguments.back().typeId = ast->getTypeInfo(ast->globalScopeId,"void*")->id;
    astfun->arguments.back().name = "ptr";
    astfun->arguments.push_back({});
    astfun->arguments.back().typeId = ast->getTypeInfo(ast->globalScopeId,"u64")->id;
    astfun->arguments.back().name = "oldsize";
    astfun->arguments.push_back({});
    astfun->arguments.back().typeId = ast->getTypeInfo(ast->globalScopeId,"u64")->id;
    astfun->arguments.back().name = "size";
    }{
    auto fun = info.addFunction("free");
    fun->address = BC_EXT_FREE;
    auto astfun = (fun->astFunc = ast->createFunction("free"));
    predefinedFuncs.push_back(astfun);
    astfun->arguments.push_back({});
    astfun->arguments.back().typeId = ast->getTypeInfo(ast->globalScopeId,"void*")->id;
    astfun->arguments.back().name = "ptr";
    astfun->arguments.push_back({});
    astfun->arguments.back().typeId = ast->getTypeInfo(ast->globalScopeId,"u64")->id;
    astfun->arguments.back().name = "size";
    }{
    auto fun = info.addFunction("printi");
    fun->address = BC_EXT_PRINTI;
    
    auto astfun = (fun->astFunc = ast->createFunction("printi"));
    predefinedFuncs.push_back(astfun);
    astfun->arguments.push_back({});
    astfun->arguments.back().typeId = ast->getTypeInfo(ast->globalScopeId,"i64")->id;
    astfun->arguments.back().name = "num";
    }{
    auto fun = info.addFunction("printc");
    fun->address = BC_EXT_PRINTC;
    
    auto astfun = (fun->astFunc = ast->createFunction("printc"));
    predefinedFuncs.push_back(astfun);
    astfun->arguments.push_back({});
    astfun->arguments.back().typeId = ast->getTypeInfo(ast->globalScopeId,"char")->id;
    astfun->arguments.back().name = "chr";
    }

    //-- Append static data
    // for(int i=0;i<(int)info.ast->constStrings.size();i++){
    //     auto& str = info.ast->constStrings[i];
    //     int offset = info.code->appendData(str.data(),str.length());
    //     info.constStringMapping.push_back(offset);
    // }

    int result = GenerateBody(info,info.ast->mainBody);
    // TODO: What to do about result? nothing?
    
    for(auto fun : predefinedFuncs){
        ast->destroy(fun);
    }
    
    // compiler logs the error count, dont need it here too
    // if(info.errors)
    //     log::out << log::RED<<"Generator failed with "<<info.errors<<" error(s)\n";
    if(err)
        (*err) += info.errors;
    return info.code;
}