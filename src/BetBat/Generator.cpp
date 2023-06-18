#include "BetBat/Generator.h"

#undef ERRT
#undef ERR
#undef ERRTYPE
#undef ERRTOKENS
#undef TOKENINFO
#undef LOGAT

// Remember to wrap macros in {} when using if or loops
// since macros have multiple statements.
#define ERR()      \
    info.errors++; \
    engone::log::out << engone::log::RED << "GenError, "
#define ERRT(R)    \
    info.errors++; \
    engone::log::out << engone::log::RED << "GenError " << (R.firstToken.line) << ":" << (R.firstToken.column) << ", "
#define ERRL(R)    \
    info.errors++; \
    engone::log::out << engone::log::RED << "GenError " << (R.firstToken.line) << ":" << (R.tokenStream->get(R.endIndex - 1).column + R.tokenStream->get(R.endIndex - 1).length) << ", "
#define ERRTYPE(R, LT, RT) ERRT(R) << "Type mismatch " << info.ast->typeToString(LT) << " - " << info.ast->typeToString(RT) << " "

#define LOGAT(R) R.firstToken.line << ":" << R.firstToken.column

#define ERRTOKENS(R)                                            \
    log::out << log::RED << "LN " << R.firstToken.line << ": "; \
    R.print();                                                  \
    log::out << "\n";

#define TOKENINFO(R)                                                                            \
    {                                                                                           \
        std::string temp = "";                                                                  \
        R.feed(temp);                                                                           \
        info.code->addDebugText(std::string("Ln ") + std::to_string(R.firstToken.line) + ": "); \
        info.code->addDebugText(temp + "\n");                                                   \
    }

#define GEN_SUCCESS 1
#define GEN_ERROR 0
/* #region  */
GenInfo::LoopScope* GenInfo::pushLoop(){
    LoopScope* ptr = (LoopScope*)engone::Allocate(sizeof(LoopScope));
    new(ptr) LoopScope();
    if(!ptr)
        return nullptr;
    loopScopes.push_back(ptr);
    return ptr;
}
GenInfo::LoopScope* GenInfo::getLoop(int index){
    if(index < 0 || index >= (int)loopScopes.size()) return nullptr;
    return loopScopes[index];
}
bool GenInfo::popLoop(){
    if(loopScopes.empty())
        return false;
    LoopScope* ptr = loopScopes.back();
    ptr->~LoopScope();
    engone::Free(ptr, sizeof(LoopScope));
    loopScopes.pop_back();
    return true;
}
void GenInfo::addPop(int reg) {
    using namespace engone;
    int size = DECODE_REG_SIZE(reg);
    if (size == 0 && errors == 0) {// we don't print if we had errors since they probably caused size of 0
        log::out << log::RED << "GenInfo::addPop : Cannot pop register with 0 size\n";
        return;
    }

    code->add({BC_POP, (u8)reg});
    Assert(("bug in compiler!", !stackAlignment.empty()))
    auto align = stackAlignment.back();
    Assert(("bug in compiler!", align.size == size));
    // You pushed some size and tried to pop a different size.
    // Did you copy paste some code involving addPop/addPush recently?

    stackAlignment.pop_back();
    if (align.diff != 0) {
        virtualStackPointer += align.diff;
        code->addDebugText("align sp\n");
        i16 offset = align.diff;
        code->add({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
    }
    virtualStackPointer += size;
    _GLOG(log::out << "relsp "<<virtualStackPointer<<"\n";)
}
void GenInfo::addPush(int reg) {
    using namespace engone;
    int size = DECODE_REG_SIZE(reg);
    if (size == 0 && errors == 0) { // we don't print if we had errors since they probably caused size of 0
        log::out << log::RED << "GenInfo::addPush : Cannot push register with 0 size\n";
        return;
    }

    int diff = (size - (-virtualStackPointer) % size) % size; // how much to increment sp by to align it
    // TODO: Instructions are generated from top-down and the stackAlignment
    //   sees pop and push in this way but how about jumps. It doesn't consider this. Is it an issue?
    if (diff) {
        virtualStackPointer -= diff;
        code->addDebugText("align sp\n");
        i16 offset = -diff;
        code->add({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
    }
    code->add({BC_PUSH, (u8)reg});
    stackAlignment.push_back({diff, size});
    virtualStackPointer -= size;
    _GLOG(log::out << "relsp "<<virtualStackPointer<<"\n";)
}
void GenInfo::addIncrSp(i16 offset) {
    using namespace engone;
    if (offset == 0)
        return;
    // Assert(offset>0) // TOOD: doesn't handle decrement of sp
    if (offset > 0) {
        int at = offset;
        while (at > 0 && stackAlignment.size() > 0) {
            auto align = stackAlignment.back();
            // log::out << "pop stackalign "<<align.diff<<":"<<align.size<<"\n";
            stackAlignment.pop_back();
            at -= align.size;
            // Assert(at >= 0);
            // Assert doesn't work because diff isn't accounted for in offset.
            // Asserting before at -= diff might not work either.

            at -= align.diff;
            
        }
    }
    else if (offset < 0) {
        stackAlignment.push_back({0, -offset});
    }
    virtualStackPointer += offset;
    code->add({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
    _GLOG(log::out << "relsp "<<virtualStackPointer<<"\n";)
}
void GenInfo::addStackSpace(i16 size) {
    using namespace engone;
    if (size == 0)
        return;

    int asize = 8;
    if(abs(size) < 2) {
        asize = 1;
    } else if(abs(size) < 4) {
        asize = 2;
    }  else if(abs(size) < 8) {
        asize = 4;
    }
    if(size < 0) {
        int diff = (-size - (-virtualStackPointer) % asize) % asize; // how much to increment sp by to align it
        // diff = 0;
        if (diff) {
            virtualStackPointer -= diff;
            code->addDebugText("align sp\n");
            i16 offset = -diff;
            code->add({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
        }
        
        code->add({BC_INCR, BC_REG_SP, (u8)(0xFF & size), (u8)(size >> 8)});
        stackAlignment.push_back({diff, -size});
        virtualStackPointer += size;
    } else if(size > 0) {
        code->add({BC_INCR, BC_REG_SP, (u8)(0xFF & size), (u8)(size >> 8)});

        // code->add({BC_POP, (u8)reg});
        Assert(("bug in compiler!", !stackAlignment.empty()))
        auto align = stackAlignment.back();
        Assert(("bug in compiler!", align.size == size));
        // You pushed some size and tried to pop a different size.
        // Did you copy paste some code involving addPop/addPush recently?

        stackAlignment.pop_back();
        if (align.diff != 0) {
            virtualStackPointer += align.diff;
            code->addDebugText("align sp\n");
            i16 offset = align.diff;
            code->add({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
        }
        virtualStackPointer += size;
    }
}
int GenInfo::saveStackMoment() {
    return virtualStackPointer;
}
void GenInfo::restoreStackMoment(int moment) {
    using namespace engone;
    int offset = moment - virtualStackPointer;
    if (offset == 0)
        return;
    int at = moment - virtualStackPointer;
    while (at > 0 && stackAlignment.size() > 0) {
        auto align = stackAlignment.back();
        // log::out << "pop stackalign "<<align.diff<<":"<<align.size<<"\n";
        stackAlignment.pop_back();
        at -= align.size;
        at -= align.diff;
        Assert(at >= 0)
    }
    virtualStackPointer = moment;
    code->add({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
}
/* #endregion */
// Will perform cast on float and integers with pop, cast, push
bool PerformSafeCast(GenInfo &info, TypeId from, TypeId to) {
    if (from == to)
        return true;
    // TODO: I just threw in currentScopeId. Not sure if it is supposed to be in all cases.
    // auto fti = info.ast->getTypeInfo(from);
    // auto tti = info.ast->getTypeInfo(to);
    auto fti = info.ast->getTypeSize(from);
    auto tti = info.ast->getTypeSize(to);
    // u8 reg0 = RegBySize(1, fti->size());
    // u8 reg1 = RegBySize(1, tti->size());
    u8 reg0 = RegBySize(1, fti);
    u8 reg1 = RegBySize(1, tti);
    if (from == AST_FLOAT32 && AST::IsInteger(to)) {
        info.addPop(reg0);
        info.code->add({BC_CAST, CAST_FLOAT_SINT, reg0, reg1});
        info.addPush(reg1);
        return true;
    }
    if (AST::IsInteger(from) && to == AST_FLOAT32) {
        info.addPop(reg0);
        info.code->add({BC_CAST, CAST_SINT_FLOAT, reg0, reg1});
        info.addPush(reg1);
        return true;
    }
    if ((AST::IsInteger(from) && to == AST_CHAR) ||
        (from == AST_CHAR && AST::IsInteger(to))) {
        // if(fti->size() != tti->size()){
        info.addPop(reg0);
        info.code->add({BC_CAST, CAST_SINT_SINT, reg0, reg1});
        info.addPush(reg1);
        // }
        return true;
    }
    if (AST::IsInteger(from) && AST::IsInteger(to)) {
        info.addPop(reg0);
        if (AST::IsSigned(from) && AST::IsSigned(to))
            info.code->add({BC_CAST, CAST_SINT_SINT, reg0, reg1});
        if (AST::IsSigned(from) && !AST::IsSigned(to))
            info.code->add({BC_CAST, CAST_SINT_UINT, reg0, reg1});
        if (!AST::IsSigned(from) && AST::IsSigned(to))
            info.code->add({BC_CAST, CAST_UINT_SINT, reg0, reg1});
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
    if (from.isPointer() && to.isPointer())
        return true;

    return from == to;

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
int GenerateExpression(GenInfo &info, ASTExpression *expression, TypeId *outTypeId, ScopeId idScope = -1) {
    using namespace engone;
    MEASURE;
    if(idScope==(ScopeId)-1)
        idScope = info.currentScopeId;
    _GLOG(FUNC_ENTER)
    Assert(expression);

    *outTypeId = AST_VOID;

    if (expression->isValue) {
        // data type
        if (AST::IsInteger(expression->typeId)) {
            i32 val = expression->i64Value;

            // TODO: immediate only allows 32 bits. What about larger values?
            // info.code->addDebugText("  expr push int");
            TOKENINFO(expression->tokenRange)

            // TODO: Int types should come from global scope. Is it a correct assumption?
            // TypeInfo *typeInfo = info.ast->getTypeInfo(expression->typeId);
            u32 size = info.ast->getTypeSize(expression->typeId);
            u8 reg = RegBySize(1, size);
            info.code->add({BC_LI, reg});
            info.code->addIm(val);
            info.addPush(reg);
        }
        else if (expression->typeId == AST_BOOL) {
            bool val = expression->boolValue;

            TOKENINFO(expression->tokenRange)
            // info.code->addDebugText("  expr push bool");
            info.code->add({BC_LI, BC_REG_AL});
            info.code->addIm(val);
            info.addPush(BC_REG_AL);
        }
        else if (expression->typeId == AST_CHAR) {
            char val = expression->charValue;

            TOKENINFO(expression->tokenRange)
            // info.code->addDebugText("  expr push char");
            info.code->add({BC_LI, BC_REG_AL});
            info.code->addIm(val);
            info.addPush(BC_REG_AL);
        }
        else if (expression->typeId == AST_FLOAT32) {
            float val = expression->f32Value;

            TOKENINFO(expression->tokenRange)
            // info.code->addDebugText("  expr push float");
            info.code->add({BC_LI, BC_REG_EAX});
            void* p = &val;
            info.code->addIm(*(u32*)p);
            info.addPush(BC_REG_EAX);
        }
        else if (expression->typeId == AST_VAR) {
            // NOTE: HELLO! i commented out the code below because i thought it was strange and out of place.
            //   It might be important but I just don't know why. Yes it was important past me.
            //   AST_VAR and variables have simular syntax.
            if (expression->name) {
                TypeInfo *typeInfo = info.ast->convertToTypeInfo(Token(*expression->name), idScope);
                // A simple check to see if the identifier in the expr node is an enum type.
                // no need to check for pointers or so.
                if (typeInfo && typeInfo->astEnum) {
                    // ERRT(expression->tokenRange) << "cannot access "<<(expression->member?*expression->member:"?")<<" from non-enum "<<*expression->name<<"\n";
                    // return GEN_ERROR;
                    *outTypeId = typeInfo->id;
                    return GEN_SUCCESS;
                }
            }
            // check data type and get it
            auto id = info.ast->getIdentifier(idScope, *expression->name);
            if (id && id->type == Identifier::VAR) {
                auto var = info.ast->getVariable(id);
                // TODO: check data type?
                // fp + offset
                // TODO: what about struct
                _GLOG(log::out << " expr var push " << *expression->name << "\n";)

                TOKENINFO(expression->tokenRange)
                // char buf[100];
                // int len = sprintf(buf,"  expr push %s",expression->name->c_str());
                // info.code->addDebugText(buf,len);

                TypeInfo *typeInfo = 0;
                if(var->typeId.isNormalType())
                    typeInfo = info.ast->getTypeInfo(var->typeId);

                // if(!typeInfo) { This breaks struct_pointer.member
                //     // log error or is that already done in the type checker?
                //     return GEN_ERROR;
                // }
                u32 size = info.ast->getTypeSize(var->typeId);
                if (!typeInfo || !typeInfo->astStruct) {
                    info.code->add({BC_LI, BC_REG_RBX});
                    info.code->addIm(var->frameOffset);
                    info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX});

                    u8 reg = RegBySize(1, size); // get the appropriate register

                    info.code->add({BC_MOV_MR, BC_REG_RBX, reg});
                    // info.code->add({BC_PUSH,BC_REG_RAX});
                    info.addPush(reg);
                } else {
                    auto &members = typeInfo->astStruct->members;
                    for (int i = (int)members.size() - 1; i >= 0; i--) {
                        auto &member = typeInfo->astStruct->members[i];
                        auto memdata = typeInfo->getMember(i);
                        _GLOG(log::out << "  member " << member.name << "\n";)

                        // TypeInfo* ti = info.ast->getTypeInfo(member.typeId);

                        u8 reg = RegBySize(1, info.ast->getTypeSize(memdata.typeId));

                        info.code->add({BC_LI, BC_REG_RBX});
                        info.code->addIm(var->frameOffset + memdata.offset);
                        info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX});
                        info.code->add({BC_MOV_MR, BC_REG_RBX, reg});
                        info.addPush(reg);
                    }
                }

                *outTypeId = var->typeId;
                return GEN_SUCCESS;
            } else {
                ERRT(expression->tokenRange) << expression->tokenRange.firstToken << " is undefined\n";
                // log::out << log::RED<<"var "<<*expression->varName<<" undefined\n";
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }
        }
        else if (expression->typeId == AST_FNCALL) {
            if(expression->boolValue) { // indicates method if true (struct.method)

                ASTExpression* varExpr = expression->left->left;
                if(varExpr->typeId != AST_VAR) {
                    // What happens if typeId is an operation instead of a type convertable to a string with typeToString?
                    ERRT(varExpr->tokenRange) << " expected a variable not "<<varExpr->tokenRange.firstToken <<" ("<< info.ast->typeToString(varExpr->typeId) <<")\n";
                    *outTypeId = AST_VOID;
                    return GEN_ERROR;
                }
                if(!varExpr->name){
                    ERRT(varExpr->tokenRange) << " name was null\n";
                    ERRTOKENS(varExpr->tokenRange)
                    *outTypeId = AST_VOID;
                    return GEN_ERROR;
                }

                auto id = info.ast->getIdentifier(info.currentScopeId, *varExpr->name);
                if(!id || id->type != Identifier::VAR) {
                    ERRT(varExpr->tokenRange) << varExpr->tokenRange.firstToken<< " is not a variable\n";
                    *outTypeId = AST_VOID;
                    return GEN_ERROR;
                }

                auto var = info.ast->getVariable(id);
                // var->typeId
                auto ti = info.ast->getTypeInfo(var->typeId.baseType());
                if(!ti || !ti->astStruct){
                    // TODO: POINTER IS ACTUALLY OKAY!
                    ERRT(varExpr->tokenRange) << varExpr->tokenRange.firstToken<< " must be a struct (was "<<info.ast->typeToString(var->typeId.baseType())<<")\n";
                    *outTypeId = AST_VOID;
                    return GEN_ERROR;
                }
                
                ASTFunction* astFunc = ti->astStruct->getFunction(*expression->name);
                if(!astFunc){
                    // TODO: POINTER IS ACTUALLY OKAY!
                    ERRT(expression->tokenRange) << expression->tokenRange.firstToken<< " is not a method of "<<info.ast->typeToString(var->typeId.baseType())<<"\n";
                    *outTypeId = AST_VOID;
                    return GEN_ERROR;
                }

                ASTExpression *argt = expression->left;
                if (argt)
                    _GLOG(log::out << "push arguments\n");
                int startSP = info.saveStackMoment();

                //-- Align
                int modu = (astFunc->argSize - info.virtualStackPointer) % 8;
                if (modu != 0) {
                    int diff = 8 - modu;
                    // log::out << "   align\n";
                    info.code->addDebugText("   align\n");
                    info.addIncrSp(-diff); // Align
                    // TODO: does something need to be done with stackAlignment list.
                }
                std::vector<ASTExpression *> realArgs;
                realArgs.resize(astFunc->arguments.size());

                int index = -1;
                while (argt) {
                    ASTExpression *arg = argt;
                    argt = argt->next;
                    index++;

                    if (!arg->namedValue) {
                        if ((int)realArgs.size() <= index) {
                            ERRT(arg->tokenRange) << "To many arguments for function " << astFunc->name << " (" << astFunc->arguments.size() << " argument(s) allowed)\n";
                            continue;
                        }
                        else
                            realArgs[index] = arg;
                    } else {
                        int argIndex = -1;
                        for (int i = 0; i < (int)astFunc->arguments.size(); i++) {
                            if (astFunc->arguments[i].name == *arg->namedValue) {
                                argIndex = i;
                                break;
                            }
                        }
                        if (argIndex == -1) {
                            ERRT(arg->tokenRange) << *arg->namedValue << " is not an argument in " << astFunc->name << "\n";
                            continue;
                        } else {
                            if (realArgs[argIndex]) {
                                ERRT(arg->tokenRange) << "argument for " << astFunc->arguments[argIndex].name << " is already specified at " << LOGAT(realArgs[argIndex]->tokenRange) << "\n";
                            } else {
                                realArgs[argIndex] = arg;
                            }
                        }
                    }
                }

                for (int i = 0; i < (int)astFunc->arguments.size(); i++) {
                    auto &arg = astFunc->arguments[i];
                    if (!realArgs[i])
                        realArgs[i] = arg.defaultValue;
                }

                index = -1;
                for (auto arg : realArgs) {
                    // ASTExpression* arg = argt;
                    // argt = argt->next;
                    index++;
                    TypeId dt = {};
                    if (!arg) {
                        ERRT(expression->tokenRange) << "missing argument for " << astFunc->arguments[index].name << " (call to " << astFunc->name << ")\n";
                        continue;
                    }

                    int result = GenerateExpression(info, arg, &dt);
                    if (result != GEN_SUCCESS) {
                        continue;
                    }
                    if (index >= (int)astFunc->arguments.size()) {
                        // ERR() << "To many arguments! func:"<<*expression->funcName<<" max: "<<astFunc->arguments.size()<<"\n";
                        continue;
                    }

                    _GLOG(log::out << " pushed " << astFunc->arguments[index].name << "\n";)

                    if (!PerformSafeCast(info, dt, astFunc->arguments[index].typeId)) {   // implicit conversion
                        // if(astFunc->arguments[index].typeId!=dt){ // strict, no conversion
                        ERRTYPE(arg->tokenRange, dt, astFunc->arguments[index].typeId) << "(fncall)\n";
                        continue;
                    }
                    // values are already pushed to the stack
                }
                {
                    // align to 8 bytes because the call frame requires it.
                    int modu = (astFunc->argSize - info.virtualStackPointer) % 8;
                    if (modu != 0) {
                        int diff = 8 - modu;
                        // log::out << "   align\n";
                        info.code->addDebugText("   align\n");
                        info.addIncrSp(-diff); // Align
                        // TODO: does something need to be done with stackAlignment list.
                    }
                }

                TOKENINFO(expression->tokenRange)
                info.code->add({BC_LI, BC_REG_RAX});
                
                if(astFunc->address == Identifier::INVALID_FUNC_ADDRESS) {
                    info.callsToResolve.push_back({info.code->length(), astFunc});
                    info.code->addIm(999999999);
                } else {
                    // no need to resolve if address has been calculated
                    info.code->addIm(astFunc->address);
                }
                info.code->add({BC_CALL, BC_REG_RAX});

                // pop arguments
                if (expression->left) {
                    _GLOG(log::out << "pop arguments\n");
                    info.restoreStackMoment(startSP);
                }
                // return types?
                if (astFunc->returnTypes.empty()) {
                    *outTypeId = AST_VOID;
                } else {
                    *outTypeId = astFunc->returnTypes[0].typeId;
                    if (astFunc->returnTypes.size() != 0) {
                        _GLOG(log::out << "extract return values\n";)
                    }
                    // NOTE: info.currentScopeId MAY NOT WORK FOR THE TYPES IF YOU PASS FUNCTION POINTERS!
                    // move return values
                    int offset = -astFunc->argSize - GenInfo::ARG_OFFSET;
                    for (int i = 0; i < (int)astFunc->returnTypes.size(); i++) {
                        auto &ret = astFunc->returnTypes[i];
                        TypeId typeId = ret.typeId;

                        TypeInfo *typeInfo = 0;
                        if(typeId.isNormalType())
                            typeInfo = info.ast->getTypeInfo(typeId);
                        if (!typeInfo || !typeInfo->astStruct) {
                            // TODO: can't use index for arguments, structs have different sizes
                            _GLOG(log::out << " extract " << info.ast->typeToString(typeId) << "\n";)
                            info.code->add({BC_LI, BC_REG_RBX});
                            info.code->addIm(offset + ret.offset);
                            u8 reg = RegBySize(1, info.ast->getTypeSize(typeId));
                            // index*8 for arguments, ARG_OFFSET for pc and fp, (i+1)*8 is where the return values are
                            info.code->add({BC_ADDI, BC_REG_SP, BC_REG_RBX, BC_REG_RBX});
                            info.code->add({BC_MOV_MR, BC_REG_RBX, reg});
                            info.addPush(reg);
                            // don't need to change offset since push will increment stack pointer
                        } else {
                            // TODO: Crash here
                            _GLOG(log::out << " extract ";
                            if(typeInfo)
                                log::out << info.ast->typeToString(typeInfo->id);
                            log::out << "\n";)
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
            }
            // check data type and get it
            auto id = info.ast->getIdentifier(idScope, *expression->name);
            if (!id || id->type != Identifier::FUNC) {
                ERRT(expression->tokenRange) << expression->tokenRange.firstToken << " is undefined\n";
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }
            ASTFunction* astFunc = info.ast->getFunction(id);

            ASTExpression *argt = expression->left;
            if (argt)
                _GLOG(log::out << "push arguments\n");
            int startSP = info.saveStackMoment();

            //-- align
            int modu = (astFunc->argSize - info.virtualStackPointer) % 8;
            if (modu != 0) {
                int diff = 8 - modu;
                // log::out << "   align\n";
                info.code->addDebugText("   align\n");
                info.addIncrSp(-diff); // Align
                // TODO: does something need to be done with stackAlignment list.
            }
            std::vector<ASTExpression *> realArgs;
            realArgs.resize(astFunc->arguments.size());

            int index = -1;
            while (argt) {
                ASTExpression *arg = argt;
                argt = argt->next;
                index++;

                if (!arg->namedValue) {
                    if ((int)realArgs.size() <= index) {
                        ERRT(arg->tokenRange) << "To many arguments for function " << astFunc->name << " (" << astFunc->arguments.size() << " argument(s) allowed)\n";
                        continue;
                    }
                    else
                        realArgs[index] = arg;
                } else {
                    int argIndex = -1;
                    for (int i = 0; i < (int)astFunc->arguments.size(); i++) {
                        if (astFunc->arguments[i].name == *arg->namedValue) {
                            argIndex = i;
                            break;
                        }
                    }
                    if (argIndex == -1) {
                        ERRT(arg->tokenRange) << *arg->namedValue << " is not an argument in " << astFunc->name << "\n";
                        continue;
                    } else {
                        if (realArgs[argIndex]) {
                            ERRT(arg->tokenRange) << "argument for " << astFunc->arguments[argIndex].name << " is already specified at " << LOGAT(realArgs[argIndex]->tokenRange) << "\n";
                        } else {
                            realArgs[argIndex] = arg;
                        }
                    }
                }
            }

            for (int i = 0; i < (int)astFunc->arguments.size(); i++) {
                auto &arg = astFunc->arguments[i];
                if (!realArgs[i])
                    realArgs[i] = arg.defaultValue;
            }

            index = -1;
            for (auto arg : realArgs) {
                // ASTExpression* arg = argt;
                // argt = argt->next;
                index++;
                TypeId dt = {};
                if (!arg) {
                    ERRT(expression->tokenRange) << "missing argument for " << astFunc->arguments[index].name << " (call to " << astFunc->name << ")\n";
                    continue;
                }

                int result = GenerateExpression(info, arg, &dt);
                if (result != GEN_SUCCESS) {
                    continue;
                }
                if (index >= (int)astFunc->arguments.size()) {
                    // ERR() << "To many arguments! func:"<<*expression->funcName<<" max: "<<astFunc->arguments.size()<<"\n";
                    continue;
                }

                _GLOG(log::out << " pushed " << astFunc->arguments[index].name << "\n";)

                if (!PerformSafeCast(info, dt, astFunc->arguments[index].typeId)) {   // implicit conversion
                    // if(astFunc->arguments[index].typeId!=dt){ // strict, no conversion
                    ERRTYPE(arg->tokenRange, dt, astFunc->arguments[index].typeId) << "(fncall)\n";
                    continue;
                }
                // values are already pushed to the stack
            }
            {
                // align to 8 bytes because the call frame requires it.
                int modu = (astFunc->argSize - info.virtualStackPointer) % 8;
                if (modu != 0) {
                    int diff = 8 - modu;
                    // log::out << "   align\n";
                    info.code->addDebugText("   align\n");
                    info.addIncrSp(-diff); // Align
                    // TODO: does something need to be done with stackAlignment list.
                }
            }

            TOKENINFO(expression->tokenRange)
            info.code->add({BC_LI, BC_REG_RAX});
            
            if(astFunc->address == Identifier::INVALID_FUNC_ADDRESS) {
                info.callsToResolve.push_back({info.code->length(), astFunc});
                info.code->addIm(999999999);
            } else {
                // no need to resolve if address has been calculated
                info.code->addIm(astFunc->address);
            }
            info.code->add({BC_CALL, BC_REG_RAX});

            // pop arguments
            if (expression->left) {
                _GLOG(log::out << "pop arguments\n");
                info.restoreStackMoment(startSP);
            }
            // return types?
            if (astFunc->returnTypes.empty()) {
                *outTypeId = AST_VOID;
            } else {
                *outTypeId = astFunc->returnTypes[0].typeId;
                if (astFunc->returnTypes.size() != 0) {
                    _GLOG(log::out << "extract return values\n";)
                }
                // NOTE: info.currentScopeId MAY NOT WORK FOR THE TYPES IF YOU PASS FUNCTION POINTERS!
                // move return values
                int offset = -astFunc->argSize - GenInfo::ARG_OFFSET;
                for (int i = 0; i < (int)astFunc->returnTypes.size(); i++) {
                    auto &ret = astFunc->returnTypes[i];
                    TypeId typeId = ret.typeId;

                    TypeInfo *typeInfo = 0;
                    if(typeId.isNormalType())
                        typeInfo = info.ast->getTypeInfo(typeId);
                    if (!typeInfo || !typeInfo->astStruct) {
                        // TODO: can't use index for arguments, structs have different sizes
                        _GLOG(log::out << " extract " << info.ast->typeToString(typeId) << "\n";)
                        info.code->add({BC_LI, BC_REG_RBX});
                        info.code->addIm(offset + ret.offset);
                        u8 reg = RegBySize(1, info.ast->getTypeSize(typeId));
                        // index*8 for arguments, ARG_OFFSET for pc and fp, (i+1)*8 is where the return values are
                        info.code->add({BC_ADDI, BC_REG_SP, BC_REG_RBX, BC_REG_RBX});
                        info.code->add({BC_MOV_MR, BC_REG_RBX, reg});
                        info.addPush(reg);
                        // don't need to change offset since push will increment stack pointer
                    } else {
                        // TODO: Crash here
                        _GLOG(log::out << " extract ";
                        if(typeInfo)
                            log::out <<  info.ast->typeToString(typeInfo->id);
                        log::out << "\n";)
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
        
        } else if(expression->typeId==AST_STRING){
            // string initializer literal
            // TODO: const isn't implented yet but when it is the type should
            //   be const char[] instead of char[].

            if(!expression->name){
                ERRT(expression->tokenRange) << "string "<<expression->tokenRange.firstToken<<" was null (compiler bug)\n";
                ERRTOKENS(expression->tokenRange)
                return GEN_ERROR;
            }

            auto pair = info.ast->constStrings.find(*expression->name);
            if(pair == info.ast->constStrings.end()){
                ERRT(expression->tokenRange) << "string "<<expression->tokenRange.firstToken<<" does not exist in the static segment (compiler bug)\n";
                return GEN_ERROR;
            }

            TypeInfo* typeInfo = info.ast->convertToTypeInfo(Token("Slice<char>"), info.ast->globalScopeId);
            if(!typeInfo){
                ERRT(expression->tokenRange) << expression->tokenRange<<" cannot be converted to Slice<char> because the type doesn't exist. Use #import \"Basic\"\n";
                return GEN_ERROR;
            }
            Assert(typeInfo->astStruct);
            Assert(typeInfo->astStruct->members.size() == 2);
            // TODO: More assert checks?

            // last member in slice is pushed first
            // info.addIncrSp(-4);§

            info.code->add({BC_LI, BC_REG_EAX});
            info.code->addIm(pair->second.length);
            info.addPush(BC_REG_EAX);

            // first member is pushed last
            info.code->add({BC_LI, BC_REG_RBX});
            info.code->addIm(pair->second.address);
            info.code->add({BC_ADDI, BC_REG_DP, BC_REG_RBX, BC_REG_RBX});
            info.addPush(BC_REG_RBX);

            // TODO: Implement polymorphic slices so char[] can  be used.

            *outTypeId = typeInfo->id;

            // return GEN_ERROR;
            return GEN_SUCCESS;
        } else if (expression->typeId == AST_NULL) {
            // TODO: Move into the type checker?
            // info.code->addDebugText("  expr push null");
            TOKENINFO(expression->tokenRange)
            info.code->add({BC_LI, BC_REG_RAX});
            info.code->addIm(0);
            info.addPush(BC_REG_RAX);

            TypeInfo *typeInfo = info.ast->convertToTypeInfo(Token("void"), info.ast->globalScopeId);
            TypeId newId = typeInfo->id;
            newId.setPointerLevel(1);
            *outTypeId = newId;
            return GEN_SUCCESS;
        } else {
            // info.code->add({BC_PUSH,BC_REG_RAX}); // push something so the stack stays synchronized, or maybe not?
            ERRT(expression->tokenRange) << expression->tokenRange.firstToken << " is an unknown data type\n";
            // log::out <<  log::RED<<"GenExpr: data type not implemented\n";
            *outTypeId = AST_VOID;
            return GEN_ERROR;
        }
        *outTypeId = expression->typeId;
    } else {
        // pop $rdx
        // pop $rax
        // addi $rax $rdx $rax
        // push $rax

        TypeId ltype = AST_VOID;
        if (expression->typeId == AST_REFER) {
            std::vector<ASTExpression*> memberExprs;
            ASTExpression* varExpr=nullptr;

            ASTExpression *next = expression->left;
            while(next) {
                ASTExpression* now = next;
                next = next->left; // left exists with AST_MEMBER

                if(now->typeId.getId() == AST_VAR) {
                    varExpr = now;
                    break;
                } else if(now->typeId.getId() == AST_MEMBER) {
                    memberExprs.push_back(now);
                } else {
                    // TODO: identifier may be defined but not a variable, give proper message
                    ERRT(now->tokenRange) << now->tokenRange.firstToken << " is not a variable or member\n";
                    return GEN_ERROR;
                }
            }
            auto id = info.ast->getIdentifier(info.currentScopeId, *varExpr->name);
            if (id && id->type == Identifier::VAR) {
                auto var = info.ast->getVariable(id);
                
                TypeId typeId = var->typeId;
                TypeInfo* typeInfo = info.ast->getTypeInfo(var->typeId);
                ASTExpression* lastExpr = varExpr;
                int offset = 0;
                for(int i=0;i<(int)memberExprs.size();i++){
                    ASTExpression* memExpr = memberExprs[i];
                    if(!typeInfo || !typeInfo->astStruct){
                        ERRT(lastExpr->tokenRange) << lastExpr->tokenRange.firstToken << " is not a struct\n";
                        return GEN_ERROR;
                    }
                    lastExpr = memExpr;
                    auto memData = typeInfo->getMember(*memExpr->name);
                    if(memData.index == -1){
                        ERRT(memExpr->tokenRange) << *memExpr->name << " is not a member of struct "<<info.ast->typeToString(typeInfo->id)<<"\n";
                        return GEN_ERROR;
                    }
                    offset += memData.offset;
                    typeId = memData.typeId;
                    typeInfo = info.ast->getTypeInfo(memData.typeId);
                }
                if(!typeInfo){
                    ERRT(varExpr->tokenRange) << *varExpr->name << "'s type gave null as TypeInfo\n";
                    return GEN_ERROR;
                }

                // TODO: check data type?
                TOKENINFO(expression->tokenRange)
                info.code->add({BC_LI, BC_REG_RAX});
                info.code->addIm(var->frameOffset + offset);
                info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RAX, BC_REG_RAX}); // fp + offset
                info.addPush(BC_REG_RAX);

                // new pointer data type
                typeId.setPointerLevel(typeId.getPointerLevel()+1);

                *outTypeId = typeId;
            } else {
                ERRT(varExpr->tokenRange) << *varExpr->name << " is not a variable\n";
                return GEN_ERROR;
            }
        }
        else if (expression->typeId == AST_DEREF) {
            int err = GenerateExpression(info, expression->left, &ltype);
            if (err == GEN_ERROR)
                return err;

            if (!ltype.isPointer()) {
                ERRT(expression->left->tokenRange) << "cannot dereference " << info.ast->typeToString(ltype) << " from " << expression->left->tokenRange.firstToken << "\n";
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }

            TypeId newId = ltype;
            newId.setPointerLevel(newId.getPointerLevel()-1);

            if (newId == AST_VOID) {
                ERRT(expression->tokenRange) << "cannot dereference " << info.ast->typeToString(ltype)<<"\n";
                return GEN_ERROR;
            }
            Assert(info.ast->getTypeSize(ltype) == 8); // left expression must return pointer
            info.addPop(BC_REG_RBX);

            if(newId.isPointer()){
                int size = info.ast->getTypeSize(newId);

                u8 reg = RegBySize(1, size);

                info.code->add({BC_MOV_MR, BC_REG_RBX, reg});
                info.addPush(reg);
            } else {
                TypeInfo* typeInfo = info.ast->getTypeInfo(newId);
                u32 size = info.ast->getTypeSize(newId);
                if (!typeInfo || !typeInfo->astStruct) {
                    u8 reg = RegBySize(1, size); // get the appropriate register

                    info.code->add({BC_MOV_MR, BC_REG_RBX, reg});
                    info.addPush(reg);
                } else {
                    auto &members = typeInfo->astStruct->members;
                    for (int i = (int)members.size() - 1; i >= 0; i--) {
                        auto &member = typeInfo->astStruct->members[i];
                        auto memdata = typeInfo->getMember(i);
                        _GLOG(log::out << "  member " << member.name << "\n";)

                        u8 reg = RegBySize(1, info.ast->getTypeSize(memdata.typeId));

                        info.code->add({BC_LI, BC_REG_RCX});
                        info.code->addIm(memdata.offset);
                        info.code->add({BC_ADDI, BC_REG_RCX, BC_REG_RBX, BC_REG_RBX});
                        info.code->add({BC_MOV_MR, BC_REG_RBX, reg});
                        info.addPush(reg);
                    }
                }
            }

            *outTypeId = newId;
        }
        else if (expression->typeId == AST_NOT) {
            int err = GenerateExpression(info, expression->left, &ltype);
            if (err == GEN_ERROR)
                return err;
            // TypeInfo *ti = info.ast->getTypeInfo(ltype);
            u32 size = info.ast->getTypeSize(ltype);
            u8 reg = RegBySize(1, size);

            info.addPop(reg);
            info.code->add({BC_NOT, reg, reg});
            info.addPush(reg);

            *outTypeId = ltype;
        }
        else if (expression->typeId == AST_BNOT) {
            int err = GenerateExpression(info, expression->left, &ltype);
            if (err == GEN_ERROR)
                return err;
            // TypeInfo *ti = info.ast->getTypeInfo(ltype);
            u32 size = info.ast->getTypeSize(ltype);
            u8 reg = RegBySize(1, size);

            info.addPop(reg);
            info.code->add({BC_BNOT, reg, reg});
            info.addPush(reg);

            *outTypeId = ltype;
        }
        else if (expression->typeId == AST_CAST) {
            int err = GenerateExpression(info, expression->left, &ltype);
            if (err == GEN_ERROR)
                return err;

            TypeId castType = expression->castType;
            // TypeInfo *ti = info.ast->getTypeInfo(ltype);
            // TypeInfo *tic = info.ast->getTypeInfo(castType);
            u32 ti = info.ast->getTypeSize(ltype);
            u32 tic = info.ast->getTypeSize(castType);
            u8 lreg = RegBySize(1, ti);
            u8 creg = RegBySize(1, tic);
            if (
                // (AST::IsInteger(castType) && AST::IsInteger(ltype))
                (castType.isPointer() && ltype.isPointer()) || (castType.isPointer() && (ltype == (TypeId)AST_INT64 ||
                 ltype == (TypeId)AST_UINT64 || ltype == (TypeId)AST_INT32)) || ((castType == (TypeId)AST_INT64 ||
                  castType == (TypeId)AST_UINT64 || ltype == (TypeId)AST_INT32) && ltype.isPointer())) {
                info.addPop(lreg);
                info.addPush(creg);
                // data is fine as it is, just change the data type
            } else {
                bool yes = PerformSafeCast(info, ltype, castType);
                if (yes) {
                } else {
                    ERRT(expression->tokenRange) << "cannot cast " << info.ast->typeToString(ltype) << " to " << info.ast->typeToString(castType) << "\n";
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
            //     ERRT(expression->tokenRange) << "cannot cast "<<info.ast->typeToString(ltype) << " to "<<info.ast->typeToString(castType)<<"\n";
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
        }
        else if (expression->typeId == AST_FROM_NAMESPACE) {
            info.code->addDebugText("ast-namespaced expr\n");

            auto si = info.ast->getScope(*expression->name, info.currentScopeId);
            TypeId exprId;
            int result = GenerateExpression(info, expression->left, &exprId, si->id);
            if (result != GEN_SUCCESS)
                return result;

            *outTypeId = exprId;
            // info.currentScopeId = savedScope;
        }
        else if (expression->typeId == AST_MEMBER) {
            // TODO: if propfrom is a variable we can directly take the member from it
            //  instead of pushing struct to the stack and then getting it.

            info.code->addDebugText("ast-member expr\n");
            TypeId exprId;
            int result = GenerateExpression(info, expression->left, &exprId);
            if (result != GEN_SUCCESS)
                return result;

            TypeInfo *typeInfo = info.ast->getTypeInfo(exprId);
            if(exprId.isPointer()) {
                TypeId structType = exprId;
                structType.setPointerLevel(structType.getPointerLevel()-1);
                TypeInfo *typeInfo = info.ast->getTypeInfo(structType);

                if(!typeInfo || !typeInfo->astStruct) {
                    ERRT(expression->tokenRange) << info.ast->typeToString(structType) << " is not a struct\n";
                    return GEN_ERROR;
                }

                auto memdata = typeInfo->getMember(*expression->name);
                if (memdata.index == -1) {
                    ERRT(expression->tokenRange) << *expression->name << " is not a member of struct " << info.ast->typeToString(ltype) << "\n";
                    return GEN_ERROR;
                }

                ltype = memdata.typeId;
                // auto& member = typeInfo->astStruct->members[meminfo.index];
                
                
                info.addPop(BC_REG_RBX);
                TypeInfo* memTypeInfo = info.ast->getTypeInfo(ltype);
                int size = info.ast->getTypeSize(ltype);
                if(!memTypeInfo || !memTypeInfo->astStruct) {
                    // enum works here too
                    u8 reg = RegBySize(1, size);
                    info.code->add({BC_LI, BC_REG_RAX});
                    info.code->addIm(memdata.offset);
                    info.code->add({BC_ADDI, BC_REG_RBX, BC_REG_RAX, BC_REG_RBX});
                    info.code->add({BC_MOV_MR, BC_REG_RBX, reg});
                    info.addPush(reg);
                    
                } else if(memTypeInfo->astStruct) {
                    Assert(("deref on struct member",false));
                    // for(int i = 0; i < (int) memTypeInfo->astStruct->members.size(); i++){
                    //     auto& member = typeInfo->astStruct->members[i];

                    // }
                } 
                *outTypeId = ltype;
            } else if (typeInfo->astStruct) {
                auto memdata = typeInfo->getMember(*expression->name);

                if (memdata.index == -1) {
                    *outTypeId = AST_VOID;
                    ERRT(expression->tokenRange) << *expression->name << " is not a member of struct " << info.ast->typeToString(ltype) << "\n";
                    return GEN_ERROR;
                }

                ltype = memdata.typeId;

                int freg = 0;
                info.code->addDebugText("ast-member extract\n");
                for (int i = 0; i < (int)typeInfo->astStruct->members.size(); i++) {
                    // auto &member = typeInfo->astStruct->members[i];
                    auto mdata = typeInfo->getMember(i);
                    // TypeInfo *minfo = info.ast->getTypeInfo(member.typeId);
                    u32 size = info.ast->getTypeSize(mdata.typeId);

                    if (i == memdata.index) {
                        freg = RegBySize(1, size);
                        info.addPop(freg);
                    } else {
                        int reg = RegBySize(4, size);
                        info.addPop(reg);
                    }
                }
                info.addPush(freg);

                *outTypeId = ltype;
            } else if (typeInfo->astEnum) {
                int enumValue;
                bool found = typeInfo->astEnum->getMember(*expression->name, &enumValue);
                if (!found) {
                    ERRT(expression->tokenRange) << expression->tokenRange.firstToken << " is not a member of enum " << typeInfo->astEnum->name << "\n";
                    return GEN_ERROR;
                }

                info.code->add({BC_LI, BC_REG_EAX}); // NOTE: fixed size of 4 bytes for enums?
                info.code->addIm(enumValue);
                info.addPush(BC_REG_EAX);

                *outTypeId = exprId;
            } else {
                *outTypeId = AST_VOID;
                ERRT(expression->tokenRange) << "member access only works on structs, pointers and enums. " << info.ast->typeToString(exprId) << " isn't one (astStruct/Enum was null at least)\n";
                return GEN_ERROR;
            }
        }
        else if (expression->typeId == AST_INITIALIZER) {
            TypeInfo *structInfo = info.ast->getTypeInfo(expression->castType); // TODO: castType should be renamed
            // TypeInfo *structInfo = info.ast->getTypeInfo(info.currentScopeId, Token(*expression->name));
            if (!structInfo || !structInfo->astStruct) {
                auto str = info.ast->typeToString(expression->castType);
                ERRT(expression->tokenRange) << "cannot do initializer on non-struct " << log::YELLOW << str << "\n";
                return GEN_ERROR;
            }

            ASTStruct *astruct = structInfo->astStruct;

            // store expressions in a list so we can iterate in reverse
            // TODO: parser could arrange the expressions in reverse.
            //   it would probably be easier since it constructs the nodes
            //   and has a little more context and control over it.
            std::vector<ASTExpression *> exprs;
            exprs.resize(astruct->members.size(), nullptr);

            ASTExpression *nextExpr = expression->left;
            int index = -1;
            while (nextExpr) {
                ASTExpression *expr = nextExpr;
                nextExpr = nextExpr->next;
                index++;

                if (!expr->namedValue) {
                    if ((int)exprs.size() <= index) {
                        ERRT(expr->tokenRange) << "To many members for struct " << astruct->name << " (" << astruct->members.size() << " member(s) allowed)\n";
                        continue;
                    }
                    else
                        exprs[index] = expr;
                } else {
                    int memIndex = -1;
                    for (int i = 0; i < (int)astruct->members.size(); i++) {
                        if (astruct->members[i].name == *expr->namedValue) {
                            memIndex = i;
                            break;
                        }
                    }
                    if (memIndex == -1) {
                        ERRT(expr->tokenRange) << *expr->namedValue << " is not an member in " << astruct->name << "\n";
                        continue;
                    } else {
                        if (exprs[memIndex]) {
                            ERRT(expr->tokenRange) << "argument for " << astruct->members[memIndex].name << " is already specified at " << LOGAT(exprs[memIndex]->tokenRange) << "\n";
                        } else {
                            exprs[memIndex] = expr;
                        }
                    }
                }
            }

            for (int i = 0; i < (int)astruct->members.size(); i++) {
                auto &mem = astruct->members[i];
                if (!exprs[i])
                    exprs[i] = mem.defaultValue;
            }
            // for (int i = (int)astruct->members.size()-1;i>=0; i--) {
            //     auto &mem = astruct->members[i];
            //     if (!exprs[i])
            //         exprs[i] = mem.defaultValue;
            // }

            index = (int)exprs.size();
            while (index > 0) {
                index--;
                ASTExpression *expr = exprs[index];
                if (!expr) {
                    ERRT(expression->tokenRange) << "missing argument for " << astruct->members[index].name << " (call to " << astruct->name << ")\n";
                    continue;
                }

                TypeId exprId;
                int result = GenerateExpression(info, expr, &exprId);
                if (result != GEN_SUCCESS)
                    return result;

                if (index >= (int)structInfo->astStruct->members.size()) {
                    // ERR() << "To many arguments! func:"<<*expression->funcName<<" max: "<<astFunc->arguments.size()<<"\n";
                    continue;
                }
                TypeId tid = structInfo->getMember(index).typeId;
                if (!PerformSafeCast(info, exprId, tid)) {   // implicit conversion
                    // if(astFunc->arguments[index].typeId!=dt){ // strict, no conversion
                    ERRTYPE(expr->tokenRange, exprId, tid) << "(initializer)\n";
                    continue;
                }
            }
            // if((int)exprs.size()!=(int)structInfo->astStruct->members.size()){
            //     ERRT(expression->tokenRange) << "Found "<<exprs.size()<<" initializer values but "<<*expression->name<<" requires "<<structInfo->astStruct->members.size()<<"\n";
            //     log::out <<log::RED<< "LN "<<expression->tokenRange.firstFToken.line <<": "; expression->tokenRange.print();log::out << "\n";
            //     // return GEN_ERROR;
            // }

            *outTypeId = structInfo->id;
        }
        else if (expression->typeId == AST_SLICE_INITIALIZER) {

            // TypeInfo* typeInfo = info.ast->getTypeInfo(*expression->name);
            // if(!structInfo||!structInfo->astStruct){
            //     ERRT(expression->tokenRange) << "cannot do initializer on non-struct "<<log::YELLOW<<*expression->name<<"\n";
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
            //         // ERR() << "To many arguments! func:"<<*expression->funcName<<" max: "<<astFunc->arguments.size()<<"\n";
            //         continue;
            //     }
            //     if(!PerformSafeCast(info,exprId,structInfo->astStruct->members[index].typeId)){ // implicit conversion
            //     // if(astFunc->arguments[index].typeId!=dt){ // strict, no conversion
            //         ERRT(expr->tokenRange) << "Type mismatch (initializer): "<<*info.ast->getTypeInfo(exprId)<<" - "<<*info.ast->getTypeInfo(structInfo->astStruct->members[index].typeId)<<"\n";
            //         continue;
            //     }
            // }
            // if((int)exprs.size()!=(int)structInfo->astStruct->members.size()){
            //     ERRT(expression->tokenRange) << "Found "<<index<<" arguments but "<<*expression->name<<" requires "<<structInfo->astStruct->members.size()<<"\n";
            //     // return GEN_ERROR;
            // }

            // *outTypeId = structInfo->id;
        }
        else if (expression->typeId == AST_FROM_NAMESPACE) {
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
        } else {
            // basic operations
            TypeId rtype;
            int err = GenerateExpression(info, expression->left, &ltype);
            if (err == GEN_ERROR)
                return err;
            err = GenerateExpression(info, expression->right, &rtype);
            if (err == GEN_ERROR)
                return err;

            // TODO: Allow other integers. Not just INT32
            if (ltype.isPointer() && rtype == AST_INT32) {
                if (expression->typeId == AST_ADD) {
                    TOKENINFO(expression->tokenRange)
                    info.addPop(BC_REG_EAX); // int
                    info.addPop(BC_REG_RDX); // pointer

                    info.code->add({BC_ADDI, BC_REG_EAX, BC_REG_RDX, BC_REG_RAX});
                    *outTypeId = ltype;
                    info.addPush(BC_REG_RAX);
                    return GEN_SUCCESS;
                } else if (expression->typeId == AST_SUB) {
                    TOKENINFO(expression->tokenRange)
                    info.addPop(BC_REG_EAX); // int
                    info.addPop(BC_REG_RDX); // pointer

                    info.code->add({BC_SUBI, BC_REG_RDX, BC_REG_EAX, BC_REG_RAX});
                    *outTypeId = ltype;
                    info.addPush(BC_REG_RAX);
                    return GEN_SUCCESS;
                } else {
                    ERRT(expression->tokenRange) << " operation "<<expression->tokenRange.firstToken << " not available for types "<<
                        info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<"\n";
                    return GEN_ERROR;
                }
            }

            if (ltype != rtype) {
                ERRTYPE(expression->tokenRange, ltype, rtype) << "\n";
                *outTypeId = AST_VOID;
                return GEN_ERROR;
            }
            TOKENINFO(expression->tokenRange)
            // TypeInfo *lInfo = info.ast->getTypeInfo(ltype);
            // TypeInfo *rInfo = info.ast->getTypeInfo(rtype);
            u32 lsize = info.ast->getTypeSize(ltype);
            u32 rsize = info.ast->getTypeSize(rtype);
            u8 reg1 = RegBySize(4, lsize); // get the appropriate registers
            u8 reg2 = RegBySize(1, rsize);
            info.addPop(reg2); // note that right expression should be popped first
            info.addPop(reg1);

#define GEN_OP(X, Y)                   \
    if (expression->typeId == AST_##X) \
        info.code->add({Y, reg1, reg2, reg2});
            if (ltype == AST_FLOAT32) {
                // NOTE: This looked neat but I accidently formatted it using VSCode's C++ garbage style and now it looks like this.
                GEN_OP(ADD, BC_ADDF)
                else GEN_OP(SUB, BC_SUBF) else GEN_OP(MUL, BC_MULF) else GEN_OP(DIV, BC_DIVF) else GEN_OP(MODULUS, BC_MODF) else {
                    // TODO: do OpToStr first and then if it fails print the typeId number
                    // log::out
                    // << log::RED << "GenExpr: operation " << expression->typeId.getId() << " not implemented\n";
                    ERRT(expression->tokenRange) << " operation "<<expression->tokenRange.firstToken << " not available for types "<<
                        info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<"\n";
                }
            } else {
                GEN_OP(ADD, BC_ADDI)
                else GEN_OP(SUB, BC_SUBI) else GEN_OP(MUL, BC_MULI) else GEN_OP(DIV, BC_DIVI) else GEN_OP(DIV, BC_DIVI) else GEN_OP(MODULUS, BC_MODI)

                        else GEN_OP(EQUAL, BC_EQ) else GEN_OP(NOT_EQUAL, BC_NEQ) else GEN_OP(LESS, BC_LT) else GEN_OP(LESS_EQUAL, BC_LTE) else GEN_OP(GREATER, BC_GT) else GEN_OP(GREATER_EQUAL, BC_GTE) else GEN_OP(AND, BC_ANDI) else GEN_OP(OR, BC_ORI)

                            else GEN_OP(BAND, BC_BAND) else GEN_OP(BOR, BC_BOR) else GEN_OP(BXOR, BC_BXOR) else GEN_OP(BNOT, BC_BNOT) else GEN_OP(BLSHIFT, BC_BLSHIFT) else GEN_OP(BRSHIFT, BC_BRSHIFT) else {
                    // TODO: do OpToStr first and then if it fails print the typeId number
                    // log::out
                    ERRT(expression->tokenRange) << " operation "<<expression->tokenRange.firstToken << " not available for types "<<
                        info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<"\n";
                    // << log::RED << "GenExpr: operation " << expression->typeId.getId() << " not implemented\n";
                        }
            }
#undef GEN_OP
            *outTypeId = ltype;

            info.addPush(reg2);
            // info.code->add({BC_PUSH,BC_REG_RAX});
        }
    }
    return GEN_SUCCESS;
}
int GenerateDefaultValue(GenInfo &info, TypeId typeId, TokenRange* tokenRange = 0) {
    using namespace engone;
    MEASURE;
    // Assert(typeInfo)
    TypeInfo* typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = info.ast->getTypeInfo(typeId);
    u32 size = info.ast->getTypeSize(typeId);
    if (typeInfo && typeInfo->astStruct) {
        for (int i = typeInfo->astStruct->members.size() - 1; i >= 0; i--) {
            auto &member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            if(member.defaultValue && typeInfo->astStruct->polyArgs.size()){
                log::out <<  log::YELLOW;
                if(tokenRange)
                    log::out << LOGAT((*tokenRange))<<": ";
                log::out << "Warning! Polymorphism may not work with default values!\n";
            }
            if (member.defaultValue) {
                TypeId typeId = {};
                int result = GenerateExpression(info, member.defaultValue, &typeId);
                // TODO: do type check in EvaluateTypes
                if (memdata.typeId != typeId) {
                    ERRTYPE(member.defaultValue->tokenRange, memdata.typeId, typeId) << "(default member)\n";
                }
            } else {
                int result = GenerateDefaultValue(info, memdata.typeId, tokenRange);
            }
        }
    } else {
        info.code->add({BC_BXOR, BC_REG_RAX, BC_REG_RAX, BC_REG_RAX});
        int sizeLeft = size;
        while (sizeLeft > 0) {
            int reg = 0;
            if (sizeLeft >= 8) {
                reg = RegBySize(1, 8);
                sizeLeft -= 8;
            } else if (sizeLeft >= 4) {
                reg = RegBySize(1, 4);
                sizeLeft -= 4;
            } else if (sizeLeft >= 2) {
                reg = RegBySize(1, 2);
                sizeLeft -= 2;
            } else if (sizeLeft >= 1) {
                reg = RegBySize(1, 1);
                sizeLeft -= 1;
            }

            Assert(reg);
            info.addPush(reg);
        }
    }
    return GEN_SUCCESS;
}
int GenerateBody(GenInfo &info, ASTScope *body);
int GenerateFunction(GenInfo& info, ASTFunction* function, ASTStruct* astStruct = 0){
    using namespace engone;
    MEASURE;
    _GLOG(FUNC_ENTER)

    int lastOffset = info.currentFrameOffset;
    // TODO: This does probably not work with structs in structs. Referencing them could cause collisions.
    if(astStruct){
        info.code->add({BC_JMP});
        int skipIndex = info.code->length();
        info.code->addIm(0);

        info.currentFunction = function;
        function->address = info.code->length();

        int startSP = info.saveStackMoment();

        // expecting 8-bit alignment when generating function
        Assert(info.virtualStackPointer % 8 == 0);

        if (function->arguments.size() != 0) {
            _GLOG(log::out << "set " << function->arguments.size() << " args\n");
            // int offset = 0;
            for (int i = 0; i < (int)function->arguments.size(); i++) {
                // for(int i = astFunc->arguments.size()-1;i>=0;i--){
                auto &arg = function->arguments[i];
                auto var = info.ast->addVariable(info.currentScopeId, arg.name);
                if (!var) {
                    ERRT(function->tokenRange) << arg.name << " already exists\n";
                }
                var->typeId = arg.typeId;
                TypeInfo *typeInfo = info.ast->getTypeInfo(arg.typeId);

                var->frameOffset = GenInfo::ARG_OFFSET + arg.offset;
                _GLOG(log::out << " " << arg.name << ": " << var->frameOffset << "\n";)
            }
            _GLOG(log::out << "\n";)
        }
        // info.currentFrameOffset = 0;
        if (function->returnTypes.size() != 0) {
            _GLOG(log::out << "space for " << function->returnTypes.size() << " return value(s) (struct may cause multiple push)\n");
            
            info.code->addDebugText("ZERO init return values\n");
            info.code->add({BC_LI, BC_REG_RBX});
            info.code->addIm(-function->returnSize);
            info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX});
            u16 size = function->returnSize;
            info.code->add({BC_ZERO_MEM, BC_REG_RBX, (u8)(0xFF & size), (u8)(size << 8)});
            _GLOG(log::out << "\n";)
            info.currentFrameOffset -= function->returnSize; // TODO: size can be uneven like 13. FIX IN EVALUATETYPES
        }

        // TODO: MAKE SURE SP IS ALIGNED TO 8 BYTES, 16 could work to.
        // SHOULD stackAlignment, virtualStackPointer be reset and then restored?
        // ALSO DO IT BEFORE CALLING FUNCTION (FNCALL)
        //
        int result = GenerateBody(info, function->body);
        info.currentFrameOffset = lastOffset;

        info.restoreStackMoment(startSP);

        // log::out << function->name<<" " <<function->returnTypes.size()<<"\n";
        //-- Check for return statement if return values are used
        if (function->returnTypes.size() != 0) {
            bool foundReturn = false;
            ASTStatement *nextState = function->body->statements;
            while (nextState) {
                if (nextState->type == ASTStatement::RETURN) {
                    foundReturn = true;
                }
                nextState = nextState->next;
            }
            if (!foundReturn) {
                ERR() << "missing return statement in " << function->name << "\n";
            }
        }

        info.code->add({BC_RET});
        *((u32 *)info.code->get(skipIndex)) = info.code->length();

        for (auto &arg : function->arguments) {
            info.ast->removeIdentifier(info.currentScopeId, arg.name);
        }
        return GEN_SUCCESS;
    }

    auto fid = info.ast->getIdentifier(info.currentScopeId, function->name);
    if (!fid) {
        // NOTE: function may not have been added in the type checker stage for some reason.
        // THANK YOU, past me for writing this note. I was wondering what I broke and reading the
        // note made instantly realise that I broke something in the type checker.
        ERRT(function->tokenRange) << function->name << " was null (compiler bug)\n";
        if (function->tokenRange.firstToken.str) {
            ERRTOKENS(function->tokenRange)
        }
        return GEN_ERROR;
    }
    if (fid->type != Identifier::FUNC) {
        // I have a feeling some error has been printed if we end up here.
        // Double check that though.
        return GEN_ERROR;
    }
    ASTFunction* astFunc = info.ast->getFunction(fid);
    Assert(astFunc);

    info.code->add({BC_JMP});
    int skipIndex = info.code->length();
    info.code->addIm(0);

    info.currentFunction = function;
    astFunc->address = info.code->length();

    int startSP = info.saveStackMoment();

    
    //-- Align
    // int modu = (- info.virtualStackPointer) % 8;
    // int diff = 8 - modu;
    // if (modu != 0) {
    //     // log::out << "   align\n";
    //     info.code->addDebugText("   align\n");
    //     info.addIncrSp(-diff); // Align
    //     // TODO: does something need to be done with stackAlignment list.
    // }
    // expecting 8-bit alignment when generating function
    Assert(info.virtualStackPointer % 8 == 0);

    if (astFunc->arguments.size() != 0) {
        _GLOG(log::out << "set " << astFunc->arguments.size() << " args\n");
        // int offset = 0;
        for (int i = 0; i < (int)astFunc->arguments.size(); i++) {
            // for(int i = astFunc->arguments.size()-1;i>=0;i--){
            auto &arg = astFunc->arguments[i];
            auto var = info.ast->addVariable(info.currentScopeId, arg.name);
            if (!var) {
                ERRT(astFunc->tokenRange) << arg.name << " already exists\n";
            }
            var->typeId = arg.typeId;
            TypeInfo *typeInfo = info.ast->getTypeInfo(arg.typeId);

            var->frameOffset = GenInfo::ARG_OFFSET + arg.offset;
            _GLOG(log::out << " " << arg.name << ": " << var->frameOffset << "\n";)
        }
        _GLOG(log::out << "\n";)
    }
    // info.currentFrameOffset = 0;
    if (astFunc->returnTypes.size() != 0) {
        _GLOG(log::out << "space for " << astFunc->returnTypes.size() << " return value(s) (struct may cause multiple push)\n");
        
        info.code->addDebugText("ZERO init return values\n");
        info.code->add({BC_LI, BC_REG_RBX});
        info.code->addIm(-astFunc->returnSize);
        info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX});
        u16 size = astFunc->returnSize;
        info.code->add({BC_ZERO_MEM, BC_REG_RBX, (u8)(0xFF & size), (u8)(size << 8)});
        _GLOG(log::out << "\n";)
        info.currentFrameOffset -= astFunc->returnSize; // TODO: size can be uneven like 13. FIX IN EVALUATETYPES
    }

    // TODO: MAKE SURE SP IS ALIGNED TO 8 BYTES, 16 could work to.
    // SHOULD stackAlignment, virtualStackPointer be reset and then restored?
    // ALSO DO IT BEFORE CALLING FUNCTION (FNCALL)
    //
    int result = GenerateBody(info, function->body);
    info.currentFrameOffset = lastOffset;

    info.restoreStackMoment(startSP);

    // log::out << *function->name<<" " <<function->returnTypes.size()<<"\n";
    if (function->returnTypes.size() != 0) {
        bool foundReturn = false;
        ASTStatement *nextState = function->body->statements;
        while (nextState) {
            if (nextState->type == ASTStatement::RETURN) {
                foundReturn = true;
            }
            nextState = nextState->next;
        }
        if (!foundReturn) {
            ERR() << "missing return statement in " << function->name << "\n";
        }
    }

    info.code->add({BC_RET});
    *((u32 *)info.code->get(skipIndex)) = info.code->length();

    for (auto &arg : astFunc->arguments) {
        info.ast->removeIdentifier(info.currentScopeId, arg.name);
    }
    return GEN_SUCCESS;
}
int GenerateBody(GenInfo &info, ASTScope *body) {
    using namespace engone;
    MEASURE;
    _GLOG(FUNC_ENTER)
    Assert(body)

        ScopeId savedScope = info.currentScopeId;
    defer { info.currentScopeId = savedScope; };

    info.currentScopeId = body->scopeId;

    //-- Begin by generating functions
    ASTFunction *nextFunction = body->functions;
    while (nextFunction) {
        ASTFunction *function = nextFunction;
        nextFunction = nextFunction->next;

        int result = GenerateFunction(info, function);
    }
    ASTStruct* nextStruct = body->structs;
    while(nextStruct) {
        ASTStruct* nowStruct = nextStruct;
        nextStruct = nextStruct->next;

        ASTFunction *nextFunction = nowStruct->functions;
        while (nextFunction) {
            ASTFunction *function = nextFunction;
            nextFunction = nextFunction->next;

            int result = GenerateFunction(info, function, nowStruct);
        }   
    }

    int lastOffset = info.currentFrameOffset;

    ASTScope *nextSpace = body->namespaces;
    while (nextSpace) {
        ASTScope *space = nextSpace;
        nextSpace = nextSpace->next;

        int result = GenerateBody(info, space);
    }

    ASTStatement *nextStatement = body->statements;
    while (nextStatement) {
        ASTStatement *statement = nextStatement;
        nextStatement = nextStatement->next;

        if (statement->type == ASTStatement::ASSIGN) {
            _GLOG(SCOPE_LOG("ASSIGN"))
            TypeId rightType = {};
            if (!statement->rvalue) {
                auto id = info.ast->getIdentifier(info.currentScopeId, *statement->name);
                if (id) {
                    ERRT(statement->tokenRange) << "Identifier " << *statement->name << " already declared\n";
                    continue;
                }
                // declare new variable at current stack pointer
                auto var = info.ast->addVariable(info.currentScopeId,*statement->name);

                // typeid comes from current scope since we do "var: Type;"
                var->typeId = statement->typeId;
                TypeInfo *typeInfo = info.ast->getTypeInfo(var->typeId.baseType());
                i32 size = info.ast->getTypeSize(var->typeId);
                i32 asize = info.ast->getTypeAlignedSize(var->typeId);
                if (!typeInfo) {
                    ERRT(statement->tokenRange) << "Undefined type " << info.ast->typeToString(var->typeId) << "\n";
                    continue;
                }
                if (size == 0) {
                    ERRT(statement->tokenRange) << "Cannot use type " << info.ast->typeToString(var->typeId) << " which has 0 in size\n";
                    continue;
                }

                // data type may be zero if it wasn't specified during initial assignment
                // a = 9  <-  implicit / explicit  ->  a : i32 = 9

                // int size = info.ast->getTypeSize(var->typeId);

                int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
                if (diff != asize) {
                    info.currentFrameOffset -= diff; // align
                }
                info.currentFrameOffset -= size;
                var->frameOffset = info.currentFrameOffset;

                // log::out << count <<" "<<(typeInfo->size+7)<<"\n";
                // TODO: default values?

                // if(typeInfo->astStruct){
                TOKENINFO(statement->tokenRange)
                int result = GenerateDefaultValue(info, var->typeId, &statement->tokenRange);
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
            int result = GenerateExpression(info, statement->rvalue, &rightType);
            if (result != GEN_SUCCESS) {
                continue;
                // return result;
            }
            auto id = info.ast->getIdentifier(info.currentScopeId, *statement->name);
            VariableInfo* var = nullptr;
            if (id) {
                if (id->type == Identifier::VAR) {
                    var = info.ast->getVariable(id);
                } else {
                    ERRT(statement->tokenRange) << "identifier " << *statement->name << " was not a variable\n";
                    continue;
                }
            }
            if (result == GEN_ERROR) {
            }
            else if (rightType == AST_VOID) {
                ERRT(statement->tokenRange) << "datatype for assignment cannot be void\n";
            }
            else if (!id || id->type == Identifier::VAR) {
                if(var) {
                    if(!PerformSafeCast(info, rightType, var->typeId)){
                        ERRTYPE(statement->tokenRange, var->typeId, rightType) << "(assign)\n";
                        ERRTOKENS(statement->tokenRange);
                        continue;
                    }
                } else if (statement->typeId.isValid()){
                    if(!PerformSafeCast(info, rightType, statement->typeId)) {
                        ERRTYPE(statement->tokenRange, statement->typeId, rightType) << "(assign)\n";
                        ERRTOKENS(statement->tokenRange);
                        continue;
                    }
                }
            
                // TODO: nonetype is only valid for as implicit initial decleration.
                //    Not allowed afterwards? or maybe it's find we just check variable type
                //    not looking at the statement data type?
                // TODO: Something special needs to happen if variable is declared
                //   and you try to assign using a type (a: i32 = 9 where a is declared isn't handled)
                bool decl = !var;
                if (!var) {
                    var = info.ast->addVariable(info.currentScopeId, *statement->name);
                    if (statement->typeId.getId() != AST_VOID)
                        var->typeId = statement->typeId;
                    else
                        var->typeId = rightType;
                    // data type may be zero if it wasn't specified during initial assignment
                    // a = 9  <-  implicit / explicit  ->  a : i32 = 9

                    /*
                    If you are wondering whether frameOffset will collide with other push and pops
                    it won't. push, pop occurs in expressions which are temporary. The only non temporary push
                    is when we do an assignment.
                    */
                }
                // etype comes from gen expression, does the type exist in our scope?
                // TypeInfo *eInfo = info.ast->getTypeInfo(etype);
                // variables can be accessed from current or any parent scope.
                // the type should therefore also exist in one of those scopes.

                i32 rightSize = info.ast->getTypeSize(rightType);
                // u32 asize = info.ast->getTypeAlignedSize(var->typeId);

                i32 leftSize = info.ast->getTypeSize(var->typeId);
                i32 asize = info.ast->getTypeAlignedSize(var->typeId);

                int alignment = 0;
                if (decl) {
                    // new declaration
                    // already on the stack, no need to pop and push
                    // info.code->add({BC_POP,BC_REG_RAX});
                    // info.code->add({BC_PUSH,BC_REG_RAX});

                    // TODO: Actually pop and BC_MOV is needed because the push doesn't
                    // have the proper offsets of the struct. (or maybe it does?)
                    // look into it.
                    // if (varInfo->size() == 0)
                    if (leftSize == 0) {
                        ERRT(statement->tokenRange) << "Size of type " << "?" << " was 0\n";
                        continue;
                    }

                    int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
                    if (diff != asize) {
                        info.currentFrameOffset -= diff; // align
                    }
                    info.currentFrameOffset -= leftSize;
                    var->frameOffset = info.currentFrameOffset;
                    if(diff!=asize)
                        alignment = diff;

                    _GLOG(log::out << "declare " << *statement->name << " at " << var->frameOffset << "\n";)
                    // NOTE: inconsistent
                    // char buf[100];
                    // int len = sprintf(buf," ^ was assigned %s",statement->name->c_str());
                    // info.code->addDebugText(buf,len);
                }
                // else{
                // local variable exists on stack
                char buf[100];
                int len = sprintf(buf, "  assign %s\n", statement->name->c_str());
                info.code->addDebugText(buf, len);
                TypeInfo *varInfo = 0;
                if(var->typeId.getPointerLevel()==0)
                    varInfo = info.ast->getTypeInfo(var->typeId.baseType());

                if (!varInfo || !varInfo->astStruct) {
                    u8 leftReg = RegBySize(4, leftSize);
                    info.addPop(leftReg);
                    info.code->add({BC_LI, BC_REG_RBX});
                    info.code->addIm(var->frameOffset);
                    info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX}); // rbx = fp + offset
                    info.code->add({BC_MOV_RM, leftReg, BC_REG_RBX});
                } else {
                    // for(int i=varInfo->astStruct->members.size()-1;i>=0;i--){
                    for (int i = 0; i < (int)varInfo->astStruct->members.size(); i++) {
                        auto &member = varInfo->astStruct->members[i];
                        auto memdata = varInfo->getMember(i);
                        // TODO: struct within struct?
                        u32 size = info.ast->getTypeSize(memdata.typeId);
                        u8 reg = RegBySize(4, size);
                        info.addPop(reg);
                        info.code->add({BC_LI, BC_REG_RBX});
                        info.code->addIm(var->frameOffset + memdata.offset);
                        info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX}); // rbx = fp + offset
                        info.code->add({BC_MOV_RM, reg, BC_REG_RBX});
                    }
                }
                if (decl) {
                    info.code->addDebugText("incr sp after decl\n");
                    // This is where we actually move the stack pointer
                    // to make space for the variable.
                    // We used BC_MOV_RM before since a structs
                    // has many values on the stack and we need to pop
                    // them before we can push new values.
                    // BC_MOV must therefore be used since we can't pop.

                    info.addIncrSp(-leftSize - alignment);
                }
                // }
                // if(decl){
                // move forward stack pointer
                //     info.code->add({BC_LI,BC_REG_RCX});
                //     info.code->addIm(8);
                //     info.code->add({BC_ADDI,BC_REG_SP,BC_REG_RCX,BC_REG_SP});
                // }
            }
            _GLOG(log::out << " " << *statement->name << " : " << info.ast->typeToString(var->typeId) << "\n";)
        }
        else if (statement->type == ASTStatement::PROP_ASSIGN) {
            _GLOG(SCOPE_LOG("PROP_ASSIGN"))

            if ((statement->opType == AST_ADD || statement->opType == AST_SUB ||
                 statement->opType == AST_MUL || statement->opType == AST_DIV)) {
                if (statement->lvalue->typeId.getId() != AST_VAR) {
                    ERRL(statement->lvalue->tokenRange) << "+= like operation is limited to variables ";
                    if (statement->lvalue->typeId == AST_MEMBER)
                        log::out << "(member is not implemented)";
                    if (statement->lvalue->typeId == AST_DEREF)
                        log::out << "(dereference is not implemented)";
                    log::out << "\n";
                    ERRTOKENS(statement->tokenRange)
                    continue;
                }

                auto iden = info.ast->getIdentifier(info.currentScopeId,*statement->lvalue->name);
                if (!iden || iden->type != Identifier::VAR) {
                    ERRT(statement->lvalue->tokenRange) << *statement->lvalue->name << " is not a variable\n";
                    continue;
                }
                auto var = info.ast->getVariable(iden);

                info.code->addDebugText("prop-assign (var)\n");
                TypeId rtype = {};
                int result = GenerateExpression(info, statement->rvalue, &rtype);
                if (result != GEN_SUCCESS) {
                    continue;
                }

                if (!IsSafeCast(var->typeId, rtype)) {
                    ERRTYPE(statement->tokenRange, var->typeId, rtype) << "\n";
                    continue;
                }
                // TypeInfo *linfo = info.ast->getTypeInfo(var->typeId);
                // TypeInfo *rinfo = info.ast->getTypeInfo(rtype);
                u32 lsize = info.ast->getTypeSize(var->typeId);
                u32 rsize = info.ast->getTypeSize(rtype);

                u8 lreg = RegBySize(1, lsize); // reg a
                u8 rreg = RegBySize(4, rsize); // reg d

                info.code->add({BC_LI, BC_REG_RBX});
                info.code->addIm(var->frameOffset);
                info.code->add({BC_ADDI, BC_REG_RBX, BC_REG_FP, BC_REG_RBX});

                info.code->add({BC_MOV_MR, BC_REG_RBX, lreg});
                info.addPop(rreg);

                u8 op = 0;
                if (var->typeId == AST_FLOAT32) {
                    switch (statement->opType) {
                    case AST_ADD:
                        op = BC_ADDF;
                        break;
                    case AST_SUB:
                        op = BC_SUBF;
                        break;
                    case AST_MUL:
                        op = BC_MULF;
                        break;
                    case AST_DIV:
                        op = BC_DIVF;
                        break;
                    }
                } else {
                    switch (statement->opType) {
                    case AST_ADD:
                        op = BC_ADDI;
                        break;
                    case AST_SUB:
                        op = BC_SUBI;
                        break;
                    case AST_MUL:
                        op = BC_MULI;
                        break;
                    case AST_DIV:
                        op = BC_DIVI;
                        break;
                    }
                }
                info.code->add({op, lreg, rreg, lreg});
                info.code->add({BC_MOV_RM, lreg, BC_REG_RBX});
            }
            else if (statement->lvalue->typeId == AST_DEREF) {
                TypeId dtype = {};
                int result = GenerateExpression(info, statement->lvalue->left, &dtype);
                if (result != GEN_SUCCESS) {
                    // return result;
                    continue;
                }

                if (!dtype.isPointer()) {
                    ERRT(statement->lvalue->tokenRange) << "expected pointer type for deref assignment not " << info.ast->typeToString(dtype) << "\n";
                    continue;
                    // return GEN_ERROR;
                }

                info.code->addDebugText("prop-assign\n");
                TypeId rtype = {};
                result = GenerateExpression(info, statement->rvalue, &rtype);
                if (result != GEN_SUCCESS) {
                    continue;
                    // return result;
                }

                Assert(("Doesn't work with structs, copy code from expression DEREF",false))

                // TypeInfo *typeInfo = info.ast->getTypeInfo(rtype);
                u32 size = info.ast->getTypeSize(rtype);
                // TypeInfo *typeInfo = info.ast->getTypeInfo(rtype);
                u8 reg = RegBySize(1, size);
                info.addPop(reg);
                info.addPop(BC_REG_RBX);
                info.code->add({BC_MOV_RM, reg, BC_REG_RBX});
            }
            else if (statement->lvalue->typeId == AST_MEMBER) {
                //-- Validate left side
                std::vector<ASTExpression *> members;
                members.push_back(statement->lvalue);
                auto varexpr = statement->lvalue->left; // isn't variable expr yet but should be after while loop
                bool failed=false;
                while (true) {
                    if (varexpr) {
                        // TODO: Handle deref and pointers
                        if (varexpr->typeId == AST_VAR) {
                            break;
                        }
                        else if (varexpr->typeId == AST_MEMBER) {
                            members.push_back(varexpr);
                            varexpr = varexpr->left;
                            continue;
                        }
                    }
                    // TODO: Does varexpr->typeid come from current scope?
                    ERRT(statement->lvalue->tokenRange) << "expected variable when doing prop assign not " << info.ast->typeToString(varexpr->typeId) << "\n";
                    ERRTOKENS(statement->tokenRange)
                    failed=true;
                    break;
                }
                if(failed)
                    continue;

                auto id = info.ast->getIdentifier(info.currentScopeId, *varexpr->name);
                if (!id || id->type != Identifier::VAR) {
                    ERRT(varexpr->tokenRange) << *varexpr->name << " is not a variable\n";
                    continue;
                    // return GEN_ERROR;
                }
                auto var = info.ast->getVariable(id);
                auto typeInfo = info.ast->getTypeInfo(var->typeId);
                if (!var->typeId.isNormalType() || !typeInfo || !typeInfo->astStruct) {
                    ERRT(varexpr->tokenRange) << *varexpr->name << " is not a struct, can't do prop assignment\n";
                    continue;
                    // return GEN_ERROR;
                }
                // u32 size = info.ast->getTypeSize(var->typeId);
                int memberOffset = 0;
                ASTStruct::Member *member = 0;
                TypeId memTypeId = {};
                for (int i = members.size() - 1; i >= 0; i--) {
                    auto expr = members[i];
                    auto memdata = typeInfo->getMember(*expr->name);
                    if (memdata.index == -1) {
                        ERRT(expr->tokenRange) << *expr->name << " is not a member of " << typeInfo->astStruct->name << "\n";
                        continue;
                        // return GEN_ERROR; // TODO: continue instead
                    }

                    memTypeId = memdata.typeId;
                    memberOffset += memdata.offset;
                    break;
                }

                //-- generate right side
                info.code->addDebugText("prog-assign member expr\n");
                TypeId typeId;
                int result = GenerateExpression(info, statement->rvalue, &typeId);
                if (result != GEN_SUCCESS) {
                    continue;
                    // return result;
                }

                if (!PerformSafeCast(info, typeId, memTypeId)) {
                    ERRT(statement->rvalue->tokenRange) << "cannot cast to ?" /*<< memtypename*/ << "\n";
                }

                info.code->addDebugText("prop-assign member push\n");
                //-- generate left side

                // TOOD: WHAT ABOUT STRUCT? doesn't fit in register
                u32 msize = info.ast->getTypeSize(memTypeId);
                u8 reg = RegBySize(1, msize);
                info.addPop(reg);

                info.code->add({BC_LI, BC_REG_RBX});
                info.code->addIm(var->frameOffset + memberOffset);
                info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX});
                info.code->add({BC_MOV_RM, reg, BC_REG_RBX});
            } else {
                ERRT(statement->tokenRange) << "expected deref or prop for prop assignment, was " << OpToStr((OperationType)statement->lvalue->typeId.getId()) << "\n";
                continue;
                // return GEN_ERROR;
            }
        }
        else if (statement->type == ASTStatement::IF) {
            _GLOG(SCOPE_LOG("IF"))

            TypeId dtype = {};
            int result = GenerateExpression(info, statement->rvalue, &dtype);
            if (result == GEN_ERROR) {
                // generate body anyway or not?
                continue;
            }
            // if(dtype!=AST_BOOL8){
            //     ERRT(statement->expression->token) << "Expected a boolean, not '"<<TypeIdToStr(dtype)<<"'\n";
            //     continue;
            // }

            // info.code->add({BC_POP,BC_REG_RAX});
            // TypeInfo *typeInfo = info.ast->getTypeInfo(dtype);
            u32 size = info.ast->getTypeSize(dtype);
            if (!size) {
                // TODO: Print error? or does generate expression do that since it gives us dtype?
                continue;
            }
            u8 reg = RegBySize(1, size);

            info.addPop(reg);
            info.code->add({BC_JNE, reg});
            u32 skipIfBodyIndex = info.code->length();
            info.code->addIm(0);

            result = GenerateBody(info, statement->body);
            if (result == GEN_ERROR)
                continue;

            u32 skipElseBodyIndex = -1;
            if (statement->elseBody) {
                info.code->add({BC_JMP});
                skipElseBodyIndex = info.code->length();
                info.code->addIm(0);
            }

            // fix address for jump instruction
            *(u32 *)info.code->get(skipIfBodyIndex) = info.code->length();

            if (statement->elseBody) {
                result = GenerateBody(info, statement->elseBody);
                if (result == GEN_ERROR)
                    continue;

                *(u32 *)info.code->get(skipElseBodyIndex) = info.code->length();
            }
        } else if (statement->type == ASTStatement::WHILE) {
            _GLOG(SCOPE_LOG("WHILE"))

            GenInfo::LoopScope* loopScope = info.pushLoop();
            defer {
                _GLOG(log::out << "pop loop\n");
                bool yes = info.popLoop();
                if(!yes){
                    log::out << log::RED << "popLoop failed (bug in compiler)\n";
                }
            };

            _GLOG(log::out << "push loop\n");
            loopScope->continueAddress = info.code->length();
            loopScope->stackMoment = info.saveStackMoment();

            TypeId dtype = {};
            int result = GenerateExpression(info, statement->rvalue, &dtype);
            if (result == GEN_ERROR) {
                // generate body anyway or not?
                continue;
            }
            // if(dtype!=AST_BOOL8){
            //     ERRT(statement->expression->token) << "Expected a boolean, not '"<<TypeIdToStr(dtype)<<"'\n";
            //     continue;
            // }

            // info.code->add({BC_POP,BC_REG_RAX});
            // TypeInfo *typeInfo = info.ast->getTypeInfo(dtype);
            u32 size = info.ast->getTypeSize(dtype);
            u8 reg = RegBySize(1, size);

            info.addPop(reg);
            info.code->add({BC_JNE, reg});
            loopScope->resolveBreaks.push_back(info.code->length());
            info.code->addIm(0);

            result = GenerateBody(info, statement->body);
            if (result == GEN_ERROR)
                continue;

            info.code->add({BC_JMP});
            info.code->addIm(loopScope->continueAddress);

            for (auto ind : loopScope->resolveBreaks) {
                *(u32 *)info.code->get(ind) = info.code->length();
            }

            // pop loop happens in defer
            // _GLOG(log::out << "pop loop\n");
            // bool yes = info.popLoop();
            // if(!yes){
            //     log::out << log::RED << "popLoop failed (bug in compiler)\n";
            // }
        } else if(statement->type == ASTStatement::BREAK) {
            log::out << log::YELLOW << "Warning! defer doesn't work with break!\n";

            GenInfo::LoopScope* loop = info.getLoop(info.loopScopes.size()-1);
            if(!loop) {
                ERRT(statement->tokenRange) << "Loop was null\n";
                continue;
            }
            info.restoreStackMoment(loop->stackMoment);
            
            // TODO: Generate defers

            info.code->add({BC_JMP});
            loop->resolveBreaks.push_back(info.code->length());
            info.code->addIm(0);
        } else if(statement->type == ASTStatement::CONTINUE) {
            log::out << log::YELLOW << "Warning! defer doesn't work with continue!\n";
            
            GenInfo::LoopScope* loop = info.getLoop(info.loopScopes.size()-1);
            if(!loop) {
                ERRT(statement->tokenRange) << "Loop was null\n";
                continue;
            }
            info.restoreStackMoment(loop->stackMoment);
            
            // TODO: Generate defers

            info.code->add({BC_JMP});
            info.code->addIm(loop->continueAddress);
        } else if (statement->type == ASTStatement::RETURN) {
            _GLOG(SCOPE_LOG("RETURN"))
            log::out << log::YELLOW << "Warning! defer doesn't work with return!\n";

            if (!info.currentFunction) {
                ERRT(statement->tokenRange) << "return only allowed in function\n";
                continue;
                // return GEN_ERROR;
            }

            //-- Evaluate return values
            ASTExpression *nextExpr = statement->rvalue;
            int argi = -1;
            while (nextExpr) {
                ASTExpression *expr = nextExpr;
                nextExpr = nextExpr->next;
                argi++;

                TypeId dtype = {};
                int result = GenerateExpression(info, expr, &dtype);
                if (result == GEN_ERROR) {
                    continue;
                }
                if (argi >= (int)info.currentFunction->returnTypes.size()) {
                    continue;
                }
                ASTFunction::ReturnType &retType = info.currentFunction->returnTypes[argi];
                if (!PerformSafeCast(info, retType.typeId, dtype)) {
                    // if(info.currentFunction->returnTypes[argi]!=dtype){

                    ERRTYPE(expr->tokenRange, dtype, info.currentFunction->returnTypes[argi].typeId) << "(return values)\n";
                }
                TypeInfo *typeInfo = 0;
                if(dtype.isNormalType())
                    typeInfo = info.ast->getTypeInfo(dtype);
                u32 size = info.ast->getTypeSize(dtype);
                if (!typeInfo || !typeInfo->astStruct) {
                    _GLOG(log::out << "move return value\n";)
                    u8 reg = RegBySize(1, size);
                    info.addPop(reg);
                    info.code->add({BC_LI, BC_REG_RBX});
                    info.code->addIm(retType.offset);
                    info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX});
                    info.code->add({BC_MOV_RM, reg, BC_REG_RBX});
                } else {
                    // offset-=typeInfo->astStruct->size;
                    for (int i = 0; i < (int)typeInfo->astStruct->members.size(); i++) {
                        auto &member = typeInfo->astStruct->members[i];
                        auto memdata = typeInfo->getMember(i);
                        // auto meminfo = info.ast->getTypeInfo(member.typeId);
                        u32 msize = info.ast->getTypeSize(memdata.typeId);
                        _GLOG(log::out << "move return value member " << member.name << "\n";)
                        u8 reg = RegBySize(1, size);
                        info.addPop(reg);
                        info.code->add({BC_LI, BC_REG_RBX});
                        // retType.offset is negative and member.offset is positive which is correct
                        info.code->addIm(retType.offset + memdata.offset);
                        info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX});
                        info.code->add({BC_MOV_RM, reg, BC_REG_RBX});
                    }
                }
                _GLOG(log::out << "\n";)
            }
            argi++; // incremented to get argument count
            if (argi != (int)info.currentFunction->returnTypes.size()) {
                ERRT(statement->tokenRange) << "Found " << argi << " return value(s) but should have " << info.currentFunction->returnTypes.size() << " for '" << info.currentFunction->name << "'\n";
            }

            // fix stack pointer before returning
            // info.code->add({BC_LI,BC_REG_RCX});
            // info.code->addIm(info.currentFrameOffset-lastOffset);
            // info.code->add({BC_ADDI,BC_REG_SP,BC_REG_RCX,BC_REG_SP});
            info.addIncrSp(info.currentFrameOffset - lastOffset);
            info.currentFrameOffset = lastOffset;
            info.code->add({BC_RET});
            return GEN_SUCCESS;
        }
        else if (statement->type == ASTStatement::CALL) {
            _GLOG(SCOPE_LOG("CALL"))

            TypeId dtype = {};
            int result = GenerateExpression(info, statement->rvalue, &dtype);
            if (result == GEN_ERROR) {
                continue;
            }
            if (dtype.getId() != AST_VOID) {
                // TODO: handle struct
                // TypeInfo *typeInfo = info.ast->getTypeInfo(dtype);
                u32 size = info.ast->getTypeSize(dtype);
                u8 reg = RegBySize(1, size);
                info.addPop(reg);
            }
        }
        else if (statement->type == ASTStatement::USING) {
            _GLOG(SCOPE_LOG("USING"))

            auto origin = info.ast->getIdentifier(info.currentScopeId, *statement->name);
            if(!origin){
                ERRT(statement->tokenRange) << *statement->name << " is not a variable (using)\n";
                ERRTOKENS(statement->tokenRange);
                return GEN_ERROR;
            }
            auto aliasId = info.ast->addIdentifier(info.currentScopeId, *statement->alias);
            if(!aliasId){
                ERRT(statement->tokenRange) << *statement->alias << " is already a variable or alias (using)\n";
                ERRTOKENS(statement->tokenRange);
                return GEN_ERROR;
            }

            aliasId->type = origin->type;
            aliasId->varIndex = origin->varIndex;
        } else if (statement->type == ASTStatement::DEFER) {
            _GLOG(SCOPE_LOG("DEFER"))

            int result = GenerateBody(info, statement->body);
            // Is it okay to do nothing with result?
        } else if (statement->type == ASTStatement::BODY) {
            _GLOG(SCOPE_LOG("BODY (statement)"))

            int result = GenerateBody(info, statement->body);
            // Is it okay to do nothing with result?
        }
    }
    if (lastOffset != info.currentFrameOffset) {
        _GLOG(log::out << "fix sp when exiting body\n";)
        info.code->addDebugText("fix sp when exiting body\n");
        // info.code->add({BC_LI,BC_REG_RCX});
        // info.code->addIm(lastOffset-info.currentFrameOffset);
        // info.addIncrSp(info.currentFrameOffset);
        info.addIncrSp(lastOffset - info.currentFrameOffset);
        // info.code->add({BC_ADDI,BC_REG_SP,BC_REG_RCX,BC_REG_SP});
        info.currentFrameOffset = lastOffset;
    }
    return GEN_SUCCESS;
}
int GenerateData(GenInfo& info, AST* ast) {
    using namespace engone;

    // Also virtualStackPointer should be aligned before generating functions
    // Also there are some asserts going off.
    
    for(auto& pair : ast->constStrings) {
        int offset = info.code->appendData(pair.first.data(), pair.first.length());
        if(offset == -1){
            continue;
        }
        pair.second.address = offset;
    }
    return GEN_SUCCESS;
}

Bytecode *Generate(AST *ast, int *err) {
    using namespace engone;
    MEASURE;
#ifdef ALLOC_LOG
    static bool sneaky=false;
    if(!sneaky){
        sneaky=true;
        TrackType(sizeof(GenInfo::LoopScope), "LoopScope");
    }
#endif

    // _VLOG(log::out <<log::BLUE<<  "##   Generator   ##\n";)

    GenInfo info{};
    info.code = Bytecode::Create();
    info.ast = ast;
    info.currentScopeId = ast->globalScopeId;

    std::vector<ASTFunction *> predefinedFuncs;
    #define STR_TO_TYPE(X) ast->convertToTypeId(Token(X), ast->globalScopeId)
    {
        auto astfun = ast->createFunction("alloc");
        auto id = info.ast->addFunction(ast->globalScopeId,astfun);
        astfun->address = BC_EXT_ALLOC;

        // don't add to ast because GenerateBody will want to create the functions
        
        predefinedFuncs.push_back(astfun);
        astfun->returnTypes.push_back(STR_TO_TYPE("void*"));
        astfun->arguments.push_back({});
        astfun->arguments.back().typeId = STR_TO_TYPE("u64");
        astfun->arguments.back().name = "size";
    }
    {
        auto astfun = ast->createFunction("realloc");
        auto id = info.ast->addFunction(ast->globalScopeId, astfun);
        astfun->address = BC_EXT_REALLOC;

        predefinedFuncs.push_back(astfun);
        astfun->returnTypes.push_back(STR_TO_TYPE("void*"));
        astfun->arguments.push_back({});
        astfun->arguments.back().typeId = STR_TO_TYPE("void*");
        astfun->arguments.back().name = "ptr";
        astfun->arguments.push_back({});
        astfun->arguments.back().typeId = STR_TO_TYPE("u64");;
        astfun->arguments.back().name = "oldsize";
        astfun->arguments.push_back({});
        astfun->arguments.back().typeId = STR_TO_TYPE("u64");;
        astfun->arguments.back().name = "size";
    }
    {
        auto astfun = ast->createFunction("free");
        auto id = info.ast->addFunction(ast->globalScopeId,astfun);
        astfun->address = BC_EXT_FREE;
        predefinedFuncs.push_back(astfun);
        astfun->arguments.push_back({});
        astfun->arguments.back().typeId = STR_TO_TYPE("void*");
        astfun->arguments.back().name = "ptr";
        astfun->arguments.push_back({});
        astfun->arguments.back().typeId = STR_TO_TYPE("u64");
        astfun->arguments.back().name = "size";
    }
    {
        auto astfun = ast->createFunction("printi");
        auto id = info.ast->addFunction(ast->globalScopeId,astfun);
        astfun->address = BC_EXT_PRINTI;

        predefinedFuncs.push_back(astfun);
        astfun->arguments.push_back({});
        astfun->arguments.back().typeId = STR_TO_TYPE("i64");;
        astfun->arguments.back().name = "num";
    }
    {
        auto astfun = ast->createFunction("printc");
        auto id = info.ast->addFunction(ast->globalScopeId, astfun);
        astfun->address = BC_EXT_PRINTC;

        predefinedFuncs.push_back(astfun);
        astfun->arguments.push_back({});
        astfun->arguments.back().typeId = STR_TO_TYPE("char");;
        astfun->arguments.back().name = "chr";
    }

    int result = GenerateData(info,info.ast);

    result = GenerateBody(info, info.ast->mainBody);
    // TODO: What to do about result? nothing?

    for(auto& e : info.callsToResolve){
        auto inst = info.code->get(e.bcIndex);
        Assert(e.astFunc->address != Identifier::INVALID_FUNC_ADDRESS);
        *((i32*)inst) = e.astFunc->address;
    }

    for (auto fun : predefinedFuncs) {
        ast->destroy(fun);
    }

    // compiler logs the error count, dont need it here too
    // if(info.errors)
    //     log::out << log::RED<<"Generator failed with "<<info.errors<<" error(s)\n";
    if (err)
        (*err) += info.errors;
    return info.code;
}