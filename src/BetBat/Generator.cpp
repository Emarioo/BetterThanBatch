#include "BetBat/Generator.h"
#include "BetBat/Compiler.h"

#undef ERR_HEAD2
#undef ERR_HEAD
#undef WARN_HEAD
#undef WARN_LINE
#undef ERR_END
#undef ERR
#undef ERRTYPE
#undef ERRTOKENS
#undef TOKENINFO
#undef LOGAT
#undef ERR_LINE

#define ERR()      \
    info.errors++; \
    engone::log::out << engone::log::RED << "GenError, "
#define ERR_HEAD2(R) info.errors++; engone::log::out << ERR_DEFAULT_R(R,"Gen. error","E0000")
#define ERR_HEAD(R, M) info.errors++; engone::log::out << ERR_DEFAULT_R(R,"Gen. error","E0000") << M
#define ERR_LINE(R, M) PrintCode(R, M)
#define ERRTYPE(R, LT, RT, M) ERR_HEAD(R, "Type mismatch " << info.ast->typeToString(LT) << " - " << info.ast->typeToString(RT) << " " << M)
// #define ERRTYPE2(R, LT, RT) ERR_HEAD2(R) << "Type mismatch " << info.ast->typeToString(LT) << " - " << info.ast->typeToString(RT) << " "
#define ERR_END MSG_END

#define WARN_HEAD(R, M) info.compileInfo->warnings++;engone::log::out << WARN_DEFAULT_R(R,"Gen. warning","W0000") << M
#define WARN_LINE(R, M) PrintCode(R, M)


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
    if (size == 0 ) { // we don't print if we had errors since they probably caused size of 0
        if( errors == 0){
            log::out << log::RED << "GenInfo::addPop : Cannot pop register with 0 size\n";
        }
        return;
    }

    code->add({BC_POP, (u8)reg});
    // if errors != 0 then don't assert and just return since the stack
    // is probably messed up because of the errors. OR you try
    // to manage the stack even with errors. Unnecessary work though so don't!? 
    //
    if(errors==0){
        Assert(("bug in compiler!", !stackAlignment.empty()))
    }
    auto align = stackAlignment.back();
    if(errors==0){
        Assert(("bug in compiler!", align.size == size));
    }
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
    if (size == 0 ) { // we don't print if we had errors since they probably caused size of 0
        if(errors == 0){
            log::out << log::RED << "GenInfo::addPush : Cannot push register with 0 size\n";
        }
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
void GenInfo::addStackSpace(i32 _size) {
    using namespace engone;
    
    Assert(_size <= (1 << 15));

    i16 size = _size;
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
// uses A register
bool PerformSafeCast(GenInfo &info, TypeId from, TypeId to) {
    if (from == to)
        return true;
    TypeId vp = AST_VOID;
    vp.setPointerLevel(1);
    if(from.isPointer() && to == vp)
        return true;
    if(from == vp && to.isPointer())
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
    if ((AST::IsInteger(from) && to == AST_BOOL) ||
        (from == AST_BOOL && AST::IsInteger(to))) {
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
u8 ASTOpToBytecode(TypeId astOp, bool floatVersion){
    
// #define CASE(X, Y) case X: return Y;
#define CASE(X, Y) else if(op == X) return Y;
    OperationType op = (OperationType)astOp.getId();
    if (floatVersion) {
        // switch((op){
            if(false) ;
            CASE(AST_ADD, BC_ADDF)
            CASE(AST_SUB, BC_SUBF)
            CASE(AST_MUL, BC_MULF)
            CASE(AST_DIV, BC_DIVF)
            CASE(AST_MODULUS, BC_MODF)
        // }
        // TODO: do OpToStr first and then if it fails print the typeId number
        // log::out
        // << log::RED << "GenExpr: operation " << expression->typeId.getId() << " not implemented\n";
        // ERR_HEAD2(expression->tokenRange) << " operation "<<expression->tokenRange.firstToken << " not available for types "<<
        //     info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<"\n";
        // ERR_END
    } else {
        // switch(op){
            if(false) ;
            CASE(AST_ADD, BC_ADDI)
            CASE(AST_SUB, BC_SUBI)
            CASE(AST_MUL, BC_MULI)
            CASE(AST_DIV, BC_DIVI) 
            CASE(AST_MODULUS, BC_MODI)
            CASE(AST_EQUAL, BC_EQ)
            CASE(AST_NOT_EQUAL, BC_NEQ) 
            CASE(AST_LESS, BC_LT) 
            CASE(AST_LESS_EQUAL, BC_LTE) 
            CASE(AST_GREATER, BC_GT) 
            CASE(AST_GREATER_EQUAL, BC_GTE) 
            CASE(AST_AND, BC_ANDI) 
            CASE(AST_OR, BC_ORI)
            CASE(AST_BAND, BC_BAND) 
            CASE(AST_BOR, BC_BOR) 
            CASE(AST_BXOR, BC_BXOR) 
            CASE(AST_BNOT, BC_BNOT) 
            CASE(AST_BLSHIFT, BC_BLSHIFT) 
            CASE(AST_BRSHIFT, BC_BRSHIFT) 
        // }
    }
#undef CASE
    return 0;
}
// push of structs
int GeneratePush(GenInfo& info, u8 baseReg, int offset, TypeId typeId){
    using namespace engone;
    Assert(baseReg!=BC_REG_RCX);
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = info.ast->getTypeInfo(typeId);
    u32 size = info.ast->getTypeSize(typeId);
    if(!typeInfo || !typeInfo->astStruct) {
        // enum works here too
        u8 reg = RegBySize(1, size);
        if(offset == 0){
            info.code->add({BC_MOV_MR, baseReg, reg});
        }else{
            info.code->add({BC_LI, BC_REG_RCX});
            info.code->addIm(offset);
            info.code->add({BC_ADDI, baseReg, BC_REG_RCX, BC_REG_RCX});
            info.code->add({BC_MOV_MR, BC_REG_RCX, reg});
        }
        info.addPush(reg);
    } else {
        for(int i = (int) typeInfo->astStruct->members.size() - 1; i>=0; i--){
            auto& member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            
            _GLOG(log::out << "push " << member.name << "\n";)
            info.code->addDebugText("push " + std::string(member.name)+"\n");
            GeneratePush(info, baseReg, offset + memdata.offset, memdata.typeId);
        }
    }
    return GEN_SUCCESS;
}
// pop of structs
int GeneratePop(GenInfo& info, u8 baseReg, int offset, TypeId typeId){
    using namespace engone;
    Assert(baseReg!=BC_REG_RCX);
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = info.ast->getTypeInfo(typeId);
    u32 size = info.ast->getTypeSize(typeId);
    if (!typeInfo || !typeInfo->astStruct) {
        _GLOG(log::out << "move return value\n";)
        u8 reg = RegBySize(1, size);
        info.addPop(reg);
        if(baseReg!=0){ // baseReg == 0 says: "dont' care about value, just pop it"
            if(offset == 0){
                info.code->add({BC_MOV_RM, reg, baseReg});
            }else{
                info.code->add({BC_LI, BC_REG_RCX});
                info.code->addIm(offset);
                info.code->add({BC_ADDI, baseReg, BC_REG_RCX, BC_REG_RCX});
                info.code->add({BC_MOV_RM, reg, BC_REG_RCX});
            }
        }
    } else {
        for (int i = 0; i < (int)typeInfo->astStruct->members.size(); i++) {
            auto &member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            _GLOG(log::out << "move return value member " << member.name << "\n";)
            GeneratePop(info, baseReg, offset + memdata.offset, memdata.typeId);
        }
    }    
    return GEN_SUCCESS;
}
int GenerateDefaultValue(GenInfo& info, TypeId typeId, TokenRange* tokenRange = 0);
int FramePush(GenInfo& info, TypeId typeId, i32* outFrameOffset, bool genDefault){
    // TODO: Automate this code. Pushing and popping variables from the frame is used often and should be functions.
    i32 size = info.ast->getTypeSize(typeId);
    i32 asize = info.ast->getTypeAlignedSize(typeId);
    if(asize==0)
        return GEN_ERROR;
    int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
    if (diff != asize) {
        info.currentFrameOffset -= diff; // align
    }
    info.currentFrameOffset -= size;
    *outFrameOffset = info.currentFrameOffset;
    
    if(genDefault){
        int result = GenerateDefaultValue(info, typeId);
        if(result!=GEN_SUCCESS)
            return GEN_ERROR;
    }
    return GEN_SUCCESS;
}
// int FramePop(){
//     Assert(("FramePop not implemented",false));
// }

/*
##################################
    Generation functions below
##################################
*/
int GenerateExpression(GenInfo &info, ASTExpression *expression, std::vector<TypeId> *outTypeIds, ScopeId idScope = -1);
int GenerateExpression(GenInfo &info, ASTExpression *expression, TypeId *outTypeId, ScopeId idScope = -1){
    std::vector<TypeId> types;
    int result = GenerateExpression(info,expression,&types,idScope);
    *outTypeId = AST_VOID;
    if(types.size()==1)
        *outTypeId = types[0];
    return result;
}
// outTypeId will represent the type (integer, struct...) but the value pushed on the stack will always
// be a pointer. EVEN when the outType isn't a pointer. It is an implicit extra level of indirection commonly
// used for assignment.
int GenerateReference(GenInfo& info, ASTExpression* _expression, TypeId* outTypeId, ScopeId idScope = -1){
    using namespace engone;
    MEASURE;
    _GLOG(FUNC_ENTER)
    Assert(_expression);
    if(idScope==(ScopeId)-1)
        idScope = info.currentScopeId;
    *outTypeId = AST_VOID;

    std::vector<ASTExpression*> exprs;
    TypeId endType = {};
    bool pointerType=false;
    ASTExpression* next=_expression;
    while(next){
        ASTExpression* now = next;
        next = next->left;

        if(now->typeId == AST_VAR) {
            // end point
            auto id = info.ast->getIdentifier(idScope, now->name);
            if (!id || id->type != Identifier::VAR) {
                ERR_HEAD2(now->tokenRange) << now->tokenRange.firstToken << " is undefined\n";
                ERR_END
                return GEN_ERROR;
            }
        
            auto var = info.ast->getVariable(id);
            _GLOG(log::out << " expr var push " << now->name << "\n";)
            TOKENINFO(now->tokenRange)
            // char buf[100];
            // int len = sprintf(buf,"  expr push %s",now->name->c_str());
            // info.code->addDebugText(buf,len);

            TypeInfo *typeInfo = 0;
            if(var->typeId.isNormalType())
                typeInfo = info.ast->getTypeInfo(var->typeId);
            TypeId typeId = var->typeId;

            info.code->add({BC_LI, BC_REG_RBX});
            info.code->addIm(var->frameOffset);
            info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX});
            info.addPush(BC_REG_RBX);

            endType = typeId;
            break;
        } else if(now->typeId == AST_MEMBER || now->typeId == AST_DEREF || now->typeId == AST_INDEX){
            // TODO: Refer is handled in GenerateExpression since refer will
            //   be combined with other operations. Refer on it's own isn't
            //   very useful and therefore not common so there is not
            //   benefit of handling refer here. Probably.
            exprs.push_back(now);
        } else {
            // end point
            TypeId ltype = {};
            int result = GenerateExpression(info, now, &ltype, idScope);
            if(result!=GEN_SUCCESS){
                return GEN_ERROR;
            }
            if(!ltype.isPointer()){
                ERR_HEAD(now->tokenRange,
                    "'"<<now->tokenRange<<
                    "' must be a reference to some memory. "
                    "A variable, member or expression resulting in a dereferenced pointer would be.\n\n";
                    ERR_LINE(now->tokenRange, "must be a reference");
                    )
                return GEN_ERROR;
            }
            endType = ltype;
            pointerType=true;
            break;
        }
    }

    for(int i=exprs.size()-1;i>=0;i--){
        ASTExpression* now = exprs[i];
        
        if(now->typeId == AST_MEMBER){
            if(pointerType){
                pointerType=false;
                endType.setPointerLevel(endType.getPointerLevel()-1);
            }
            if(endType.getPointerLevel()>1){ // one level of pointer is okay.
                ERR_HEAD(now->tokenRange, "'"<<info.ast->typeToString(endType)<<"' has to many levels of pointing. Can only access members of a single or non-pointer.\n\n";
                    ERR_LINE(now->tokenRange, "to pointy");
                )
                return GEN_ERROR;
            }
            TypeInfo* typeInfo = nullptr;
            typeInfo = info.ast->getTypeInfo(endType.baseType());
            if(!typeInfo || !typeInfo->astStruct){ // one level of pointer is okay.
                std::string msg = info.ast->typeToString(endType);
                ERR_HEAD(now->tokenRange, "'"<<info.ast->typeToString(endType)<<"' is not a struct. Cannot access member.\n\n";
                    ERR_LINE(now->left->tokenRange,msg.c_str());
                )
                return GEN_ERROR;
            }

            auto memberData = typeInfo->getMember(now->name);
            if(memberData.index==-1){
                ERR_HEAD(now->tokenRange, "'"<<now->name << "' is not a member of struct '" << info.ast->typeToString(endType) << "'. "
                        "These are the members: ";
                    for(int i=0;i<(int)typeInfo->astStruct->members.size();i++){
                        if(i!=0)
                            log::out << ", ";
                        log::out << log::LIME << typeInfo->astStruct->members[i].name<<log::SILVER<<": "<<info.ast->typeToString(typeInfo->getMember(i).typeId);
                    }
                    log::out <<"\n";
                    log::out <<"\n";
                    ERR_LINE(now->tokenRange, "not a member");
                )
                return GEN_ERROR;
            }
            
            info.addPop(BC_REG_RBX);
            if(endType.getPointerLevel()>0){
                info.code->add({BC_MOV_MR, BC_REG_RBX, BC_REG_RBX});
            }
            if(memberData.offset!=0){ // optimisation
                info.code->add({BC_LI,BC_REG_EAX});
                info.code->addIm(memberData.offset);
                info.code->add({BC_ADDI,BC_REG_RBX, BC_REG_EAX, BC_REG_RBX});
            }
            info.addPush(BC_REG_RBX);

            endType = memberData.typeId;
        } else if(now->typeId == AST_DEREF) {
            if(pointerType){
                // PROTECTIVE BARRIER took a hit
                pointerType=false;
                endType.setPointerLevel(endType.getPointerLevel()-1);
                continue;
            }
            if(endType.getPointerLevel()<1){
                ERR_HEAD(now->left->tokenRange, "type '"<<info.ast->typeToString(endType)<<"' is not a pointer.\n\n";
                    ERR_LINE(now->left->tokenRange,"must be a pointer");
                )
                return GEN_ERROR;
            }
            if(endType.getPointerLevel()>0){
                info.addPop(BC_REG_RBX);
                info.code->add({BC_MOV_MR,BC_REG_RBX, BC_REG_RBX});
                info.addPush(BC_REG_RBX);
            } else {
                // do nothing
            }
            endType.setPointerLevel(endType.getPointerLevel()-1);
        } else if(now->typeId == AST_INDEX) {
            TypeId rtype;
            int err = GenerateExpression(info, now->right, &rtype);
            if (err != GEN_SUCCESS)
                return GEN_ERROR;
            
            if(!AST::IsInteger(rtype)){
                std::string strtype = info.ast->typeToString(rtype);
                ERR_HEAD(now->right->tokenRange, "Index operator ( array[23] ) requires integer type in the inner expression. '"<<strtype<<"' is not an integer.\n\n";
                    ERR_LINE(now->right->tokenRange,strtype.c_str());
                )
                return GEN_ERROR;
            }

            if(pointerType){
                pointerType=false;
                endType.setPointerLevel(endType.getPointerLevel()-1);
                
                u32 typesize = info.ast->getTypeSize(endType);
                u32 rsize = info.ast->getTypeSize(rtype);
                u8 reg = RegBySize(4,rsize);
                info.addPop(reg); // integer
                info.addPop(BC_REG_RBX); // reference

                info.code->add({BC_LI,BC_REG_EAX});
                info.code->addIm(typesize);
                info.code->add({BC_MULI, reg, BC_REG_EAX, reg});
                info.code->add({BC_ADDI, BC_REG_RBX, reg, BC_REG_RBX});

                info.addPush(BC_REG_RBX);
                continue;
            }

            if(!endType.isPointer()){
                std::string strtype = info.ast->typeToString(endType);
                ERR_HEAD(now->right->tokenRange, "Index operator ( array[23] ) requires pointer type in the outer expression. '"<<strtype<<"' is not a pointer.\n\n";
                    ERR_LINE(now->right->tokenRange,strtype.c_str());
                )
                return GEN_ERROR;
            }

            endType.setPointerLevel(endType.getPointerLevel()-1);

            u32 typesize = info.ast->getTypeSize(endType);
            u32 rsize = info.ast->getTypeSize(rtype);
            u8 reg = RegBySize(4,rsize);
            info.addPop(reg); // integer
            info.addPop(BC_REG_RBX); // reference
            // dereference pointer to pointer
            info.code->add({BC_MOV_MR, BC_REG_RBX, BC_REG_RBX});

            info.code->add({BC_LI,BC_REG_EAX});
            info.code->addIm(typesize);
            info.code->add({BC_MULI, reg, BC_REG_EAX, reg});
            info.code->add({BC_ADDI, BC_REG_RBX, reg, BC_REG_RBX});

            info.addPush(BC_REG_RBX);
        }
    }
    if(pointerType){
        ERR_HEAD(_expression->tokenRange,
            "'"<<_expression->tokenRange<<
            "' must be a reference to some memory. "
            "A variable, member or expression resulting in a dereferenced pointer would work.\n\n";
            ERR_LINE(_expression->tokenRange, "must be a reference");
        )
        return GEN_ERROR;
    }
    *outTypeId = endType;
    return GEN_SUCCESS;
}
int GenCheckFuncImpl(GenInfo& info, ASTExpression* expression, ASTFunction*& astFunc, FuncImpl*& funcImpl, ScopeId idScope){
    // ASTFunction* astFunc = nullptr;
    // FuncImpl* funcImpl = nullptr;
    Assert(("GenCheckFuncImpl is broken",false));
    if(idScope==(ScopeId)-1)
        idScope = info.currentScopeId;

    if(expression->boolValue) { // indicates method if true (struct.method)
        Assert(("Methods are broken",false))
        Assert(expression->left)

        TypeId refid={};
        int result = GenerateReference(info,expression->left,&refid,idScope);
        if(result!=GEN_SUCCESS)
            return GEN_ERROR;
        info.addPop(BC_REG_RBX); // pop and throw away since we just want to know the type
        // could optimise by keeping value and don't to the first GenerateExpression for the arguments
        // later.

        // auto var = info.ast->getVariable(id);
        if(refid.getPointerLevel()>1){
            ERR_HEAD(expression->left->tokenRange, expression->left->tokenRange.firstToken<< " to much of a pointer\n");
            // First argument is to much of a pointer. Was this error caught in type checker?
            return GEN_ERROR;
        }
        refid = refid.baseType();

        auto ti = info.ast->getTypeInfo(refid);
        if(!ti || !ti->astStruct || !ti->getImpl()){
            ERR_HEAD(expression->left->tokenRange, expression->left->tokenRange.firstToken<< " must be a struct (was "<<info.ast->typeToString(refid)<<").\n");
            return GEN_ERROR;
        }
        Assert(("typeids code not done!",false));
        std::vector<TypeId> dinner;
        
        auto method = ti->getImpl()->getMethod(expression->name, dinner);
        // ASTStruct::Method method = ti->astStruct->getMethod(*expression->name);
        astFunc = method->astFunc;
        // ti->astStruct->getMethod(*expression->name);
        if(!astFunc){
            // TODO: POINTER IS ACTUALLY OKAY!
            ERR_HEAD2(expression->tokenRange) << expression->tokenRange.firstToken<< " is not a method of "<<info.ast->typeToString(refid)<<"\n";
            ERR_END
            return GEN_ERROR;
        }
        funcImpl = method->funcImpl;
        if(!funcImpl) { // we got method of the base name, if polymorphic there is no base
            ERR_HEAD2(expression->tokenRange) << expression->tokenRange.firstToken << " is polymorphic. You must specify poly arguments\n";
            ERR_END
            return GEN_ERROR;
        }
    } else {
        // check data type and get it
        auto id = info.ast->getIdentifier(idScope, expression->name);
        if (!id || id->type != Identifier::FUNC) {
            ERR_HEAD(expression->tokenRange, expression->tokenRange.firstToken << " is undefined\n";
            )
            return GEN_ERROR;
        }
        // astFunc = info.ast->getFunction(id, );
        // funcImpl = id->funcImpl;
        // Assert(funcImpl)
        // if(astFunc->polyArgs.size() == 0)
        //     funcImpl = &astFunc->baseImpl;
        if(!funcImpl) {
            // As of writing this funcImpl as null indicates that type checker didn't use baseImpl
            // when adding function identifier. This means that the function is polymorphic.
            // Assert(astFunc->polyArgs.size() != 0) // assert in case the assumption is wrong.
            // - Me 2 minutes later: I forgot about the predefined functions (printi,...)
            ERR_HEAD2(expression->tokenRange) << expression->tokenRange.firstToken << " is polymorphic\n";
            ERR_END
            return GEN_ERROR;
        }
    }
    Assert(funcImpl && astFunc)
    Assert(funcImpl->argumentTypes.size() == astFunc->arguments.size())
    Assert(funcImpl->returnTypes.size() == astFunc->returnTypes.size())
    return GEN_SUCCESS;
}
int GenerateExpression(GenInfo &info, ASTExpression *expression, std::vector<TypeId> *outTypeIds, ScopeId idScope) {
    using namespace engone;
    MEASURE;
    if(idScope==(ScopeId)-1)
        idScope = info.currentScopeId;
    _GLOG(FUNC_ENTER)
    Assert(expression);

    TypeId castType = expression->castType;
    if(castType.isString()){
        castType = info.ast->convertToTypeId(castType,idScope);
        if(!castType.isValid()){
            ERR_HEAD(expression->tokenRange,"Type "<<info.ast->getTokenFromTypeString(expression->castType) << " does not exist.\n";)
        }
    }
    castType = info.ast->ensureNonVirtualId(castType);

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
            // if (expression->name) {
            TypeInfo *typeInfo = info.ast->convertToTypeInfo(expression->name, idScope);
            // A simple check to see if the identifier in the expr node is an enum type.
            // no need to check for pointers or so.
            if (typeInfo && typeInfo->astEnum) {
                // ERR_HEAD2(expression->tokenRange) << "cannot access "<<(expression->member?*expression->member:"?")<<" from non-enum "<<*expression->name<<"\n";
                // ERR_END
                // return GEN_ERROR;
                outTypeIds->push_back(typeInfo->id);
                return GEN_SUCCESS;
            }
            // }
            // check data type and get it
            auto id = info.ast->getIdentifier(idScope, expression->name);
            if (id && id->type == Identifier::VAR) {
                auto var = info.ast->getVariable(id);
                // TODO: check data type?
                // fp + offset
                // TODO: what about struct
                _GLOG(log::out << " expr var push " << expression->name << "\n";)

                TOKENINFO(expression->tokenRange)
                // char buf[100];
                // int len = sprintf(buf,"  expr push %s",expression->name->c_str());
                // info.code->addDebugText(buf,len);
                // log::out << "AST_VAR: "<<id->name<<", "<<id->varIndex<<", "<<var->frameOffset<<"\n";
                GeneratePush(info, BC_REG_FP, var->frameOffset, var->typeId);

                outTypeIds->push_back(var->typeId);
                return GEN_SUCCESS;
            } else {
                ERR_HEAD(expression->tokenRange, "'"<<expression->tokenRange.firstToken<<"' is not declared.\n\n";
                    ERR_LINE(expression->tokenRange,"undeclared");
                    )
                // ERR_HEAD2(expression->tokenRange) << expression->tokenRange.firstToken << " is undefined\n";
                // ERR_END
                // log::out << log::RED<<"var "<<*expression->varName<<" undefined\n";
                return GEN_ERROR;
            }
        }
        else if (expression->typeId == AST_FNCALL) {
            // ASTFunction* astFunc = nullptr;
            // FuncImpl* funcImpl = nullptr;

            // int result = GenCheckFuncImpl(info, expression, astFunc, funcImpl, idScope);
            // if(result!=GEN_SUCCESS)
            //     return GEN_ERROR;

            // ASTExpression *argt = expression->left;
            // if (argt)
            //     _GLOG(log::out << "push arguments\n");
            int startSP = info.saveStackMoment();

            //-- align
            int modu = (info.virtualStackPointer) % 8;
            if (modu != 0) {
                int diff = 8 - modu;
                // log::out << "   align\n";
                info.code->addDebugText("   align for args\n");
                info.addIncrSp(-diff); // Align
                // TODO: does something need to be done with stackAlignment list.
            }

            // std::vector<ASTExpression *> realArgs;
            // realArgs.resize(astFunc->arguments.size());

            // int index = -1;
            // for (int index = 0; index < expression->args->size(); index++) {
            //     ASTExpression *arg = expression->args->at(index);
            //     // argt = argt->next;
            //     // index++;

            //     // if (!arg->namedValue) {
            //         if ((int)realArgs.size() <= index) {
            //             ERR_HEAD2(arg->tokenRange) << "To many arguments for function " << astFunc->name << " (" << astFunc->arguments.size() << " argument(s) allowed)\n";
            //             ERR_END
            //             continue;
            //         }
            //         else
            //             realArgs[index] = arg;
            //     // } else {
            //     //     int argIndex = -1;
            //     //     for (int i = 0; i < (int)astFunc->arguments.size(); i++) {
            //     //         if (astFunc->arguments[i].name == *arg->namedValue) {
            //     //             argIndex = i;
            //     //             break;
            //     //         }
            //     //     }
            //     //     if (argIndex == -1) {
            //     //         ERR_HEAD2(arg->tokenRange) << *arg->namedValue << " is not an argument in " << astFunc->name << "\n";
            //     //         ERR_END
            //     //         continue;
            //     //     } else {
            //     //         if (realArgs[argIndex]) {
            //     //             ERR_HEAD2(arg->tokenRange) << "argument for " << astFunc->arguments[argIndex].name << " is already specified at " << LOGAT(realArgs[argIndex]->tokenRange) << "\n";
            //     //             ERR_END
            //     //         } else {
            //     //             realArgs[argIndex] = arg;
            //     //         }
            //     //     }
            //     // }
            // }

            // for (int i = 0; i < (int)astFunc->arguments.size(); i++) {
            //     auto &arg = astFunc->arguments[i];
            //     if (!realArgs[i])
            //         realArgs[i] = arg.defaultValue;
            // }
            DynamicArray<TypeId> argTypes{};
            int index = 0;
            for (index = 0; index < (int)expression->args->size();index++) {
                ASTExpression* arg = expression->args->get(index);
                if(arg->namedValue){
                    break;
                }

                TypeId dt = {};
                int result = 0;
                if(expression->boolValue && index == 0) {
                    // method call and first argument which is 'this'
                    result = GenerateReference(info, arg, &dt);
                    if(result==GEN_SUCCESS){
                        if(!dt.isPointer()){
                            dt.setPointerLevel(1);
                        } else {
                            info.addPop(BC_REG_RBX);
                            info.code->add({BC_MOV_MR, BC_REG_RBX, BC_REG_RBX});
                            info.addPush(BC_REG_RBX);
                        }
                    }
                } else {
                    result = GenerateExpression(info, arg, &dt);
                }
                argTypes.add(dt);
                // if (result != GEN_SUCCESS) {
                //     continue;
                // }
                // if (index >= (int)astFunc->arguments.size()) {
                //     // ERR() << "To many arguments! func:"<<*expression->funcName<<" max: "<<astFunc->arguments.size()<<"\n";
                //     continue;
                // }

                // _GLOG(log::out << " pushed " << astFunc->arguments[index].name << "\n";)
                // Assert((int)funcImpl->arguments.size()>index);
                // if (!PerformSafeCast(info, dt, funcImpl->arguments[index].typeId)) {   // implicit conversion
                //     // if(astFunc->arguments[index].typeId!=dt){ // strict, no conversion
                //     ERRTYPE(arg->tokenRange, dt, funcImpl->arguments[index].typeId, "(fncall).\n\n";
                //         ERR_LINE(arg->tokenRange, "aa");
                //     )
                //     continue;
                // }
                // values are already pushed to the stack
            }
            std::vector<Token> polyTypes; // not used?
            Token baseName = AST::TrimPolyTypes(expression->name, &polyTypes);

            auto iden = info.ast->getIdentifier(idScope, baseName);
            if(!iden){
                ERR_HEAD(expression->tokenRange, "Function '"<<baseName<<"' does not exist.\n";
                    ERR_LINE(expression->name,"undefined");
                )
                return GEN_ERROR;
            }
            ASTFunction* astFunc = nullptr;
            FuncImpl* funcImpl = nullptr;
            {
                auto overload = iden->funcOverloads.getOverload(argTypes);
                if(!overload){
                    if(info.compileInfo->typeErrors==0){
                        ERR_HEAD(expression->tokenRange, "Overload for function '"<<baseName <<"' does not exist for the argument(s): ";
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
                    }
                    return GEN_ERROR;
                    // If you think an error related to this has been
                    // printed already then you are wrong.
                }
                astFunc = overload->astFunc;
                funcImpl = overload->funcImpl;
            }

            std::vector<ASTExpression*> restArgs;
            restArgs.resize(astFunc->arguments.size(),nullptr);
            for (int i = index; i < (int)expression->args->size();i++) {
                ASTExpression* arg = expression->args->get(i);
                Assert(arg->namedValue);
                restArgs[i] = arg;
            }
            for (int i = index; i < (int)astFunc->arguments.size(); i++) {
                auto &arg = astFunc->arguments[i];
                if (!restArgs[i])
                    restArgs[i] = arg.defaultValue;
            }

            for(int i=index;i<(int)astFunc->arguments.size();i++){
                auto &arg = astFunc->arguments[i];
                Assert(arg.defaultValue);
                // if(!arg.defaultValue){
                //     ERR_HEAD(expression->tokenRange, "Specify more arguments.";)
                //     continue;
                // }
                TypeId dt={};
                int result = GenerateExpression(info, arg.defaultValue, &dt);
                // if (result != GEN_SUCCESS) {
                //     continue;
                // }
            }

            {
                // align to 8 bytes because the call frame requires it.
                int modu = (funcImpl->argSize - info.virtualStackPointer) % 8;
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

            // Always resole later because external calls/ native code needs to be hooked
            info.callsToResolve.push_back({info.code->length(), funcImpl});
            info.code->addIm(999999999);
            // if(funcImpl->address == Identifier::INVALID_FUNC_ADDRESS) {
            //     info.callsToResolve.push_back({info.code->length(), funcImpl});
            //     info.code->addIm(999999999);
            // } else {
            //     // no need to resolve if address has been calculated
            //     info.code->addIm(funcImpl->address);
            // }
            info.code->add({BC_CALL, BC_REG_RAX});

            // info.recentFuncImpl = funcImpl;

            // pop arguments
            // if (expression->args) {
                _GLOG(log::out << "pop arguments\n");
                info.restoreStackMoment(startSP);
            // }
            // return types?
            if (funcImpl->returnTypes.size()==0) {
                outTypeIds->push_back(AST_VOID);
            } else {
                _GLOG(log::out << "extract return values\n";)
                info.code->addDebugText("Push return values\n");
                // NOTE: info.currentScopeId MAY NOT WORK FOR THE TYPES IF YOU PASS FUNCTION POINTERS!
                // move return values
                // int offset = - -funcImpl->argSize - GenInfo::ARG_OFFSET;
                int offset = -funcImpl->argSize - GenInfo::ARG_OFFSET;
                
                info.code->add({BC_MOV_RR, BC_REG_SP, BC_REG_RBX});
                for (int i = funcImpl->returnTypes.size()-1; i >=0; i--) {
                    auto &ret = funcImpl->returnTypes[i];
                    TypeId typeId = ret.typeId;

                    GeneratePush(info, BC_REG_RBX, offset + ret.offset, typeId);
                    outTypeIds->push_back(ret.typeId);
                }
            }
            return GEN_SUCCESS;
        } else if(expression->typeId==AST_STRING){
            // if(!expression->name){
            //     ERR_HEAD2(expression->tokenRange) << "string "<<expression->tokenRange.firstToken<<" was null (compiler bug)\n";
            //     ERRTOKENS(expression->tokenRange)
            //     ERR_END
            //     return GEN_ERROR;
            // }

            auto pair = info.ast->constStrings.find(expression->name);
            if(pair == info.ast->constStrings.end()){
                ERR_HEAD2(expression->tokenRange) << "string "<<expression->tokenRange.firstToken<<" does not exist in the static segment (compiler bug)\n";
                ERR_END
                return GEN_ERROR;
            }

            TypeInfo* typeInfo = info.ast->convertToTypeInfo(Token("Slice<char>"), info.ast->globalScopeId);
            if(!typeInfo){
                ERR_HEAD2(expression->tokenRange) << expression->tokenRange<<" cannot be converted to Slice<char> because Slice doesn't exist. Did you forget #import \"Basic\"\n";
                ERR_END
                return GEN_ERROR;
            }
            Assert(typeInfo->astStruct);
            Assert(typeInfo->astStruct->members.size() == 2);
            Assert(typeInfo->structImpl->members[0].offset == 0);
            // Assert(typeInfo->structImpl->members[1].offset == 8);// len: u64
            // Assert(typeInfo->structImpl->members[1].offset == 12); // len: u32

            // last member in slice is pushed first
            if(typeInfo->structImpl->members[1].offset == 8){
                info.code->add({BC_LI, BC_REG_RAX});
                info.code->addIm(pair->second.length);
                info.addPush(BC_REG_RAX);
            } else {
                info.code->add({BC_LI, BC_REG_EAX});
                info.code->addIm(pair->second.length);
                info.addPush(BC_REG_EAX);
            }

            // first member is pushed last
            info.code->add({BC_LI, BC_REG_RBX});
            info.code->addIm(pair->second.address);
            info.code->add({BC_ADDI, BC_REG_DP, BC_REG_RBX, BC_REG_RBX});
            info.addPush(BC_REG_RBX);

            outTypeIds->push_back(typeInfo->id);
            return GEN_SUCCESS;
        } else if(expression->typeId == AST_SIZEOF){
            // TODO: Move into the type checker?
            // info.code->addDebugText("  expr push null");
            TOKENINFO(expression->tokenRange)
            TypeId typeId = info.ast->convertToTypeId(expression->name, idScope);
            Assert(typeId.isValid()); // Did type checker fix this? Maybe not on errors?

            u32 size = info.ast->getTypeSize(typeId);

            info.code->add({BC_LI, BC_REG_EAX});
            info.code->addIm(size);
            info.addPush(BC_REG_EAX);

            outTypeIds->push_back(AST_UINT32);
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
            outTypeIds->push_back( newId);
            return GEN_SUCCESS;
        } else {
            std::string typeName = info.ast->typeToString(expression->typeId);
            // info.code->add({BC_PUSH,BC_REG_RAX}); // push something so the stack stays synchronized, or maybe not?
            ERR_HEAD(expression->tokenRange, "'" <<typeName << "' is an unknown data type.\n\n";
                ERR_LINE(expression->tokenRange,typeName.c_str());
            )
            // log::out <<  log::RED<<"GenExpr: data type not implemented\n";
            outTypeIds->push_back( AST_VOID);
            return GEN_ERROR;
        }
        outTypeIds->push_back( expression->typeId);
    } else {
        TypeId ltype = AST_VOID;
        if (expression->typeId == AST_REFER) {

            int result = GenerateReference(info,expression->left,&ltype,idScope);
            if(result!=GEN_SUCCESS)
                return GEN_ERROR;
            if(ltype.getPointerLevel()==3){
                ERR_HEAD(expression->tokenRange,"Cannot take a reference of a triple pointer (compiler has a limit of 0-3 depth of pointing). Cast to u64 if you need triple pointers.";)
                return GEN_ERROR;
            }
            ltype.setPointerLevel(ltype.getPointerLevel()+1);
            outTypeIds->push_back( ltype); 
        }
        else if (expression->typeId == AST_DEREF) {
            int err = GenerateExpression(info, expression->left, &ltype);
            if (err == GEN_ERROR)
                return err;

            if (!ltype.isPointer()) {
                ERR_HEAD(expression->left->tokenRange, "Cannot dereference " << info.ast->typeToString(ltype) << ".\n\n";
                    ERR_LINE(expression->left->tokenRange, "bad");
                )
                outTypeIds->push_back( AST_VOID);
                return GEN_ERROR;
            }

            TypeId outId = ltype;
            outId.setPointerLevel(outId.getPointerLevel()-1);

            if (outId == AST_VOID) {
                ERR_HEAD(expression->left->tokenRange, "Cannot dereference " << info.ast->typeToString(ltype) << ".\n\n";
                    ERR_LINE(expression->left->tokenRange, "bad");
                )
                return GEN_ERROR;
            }
            Assert(info.ast->getTypeSize(ltype) == 8); // left expression must return pointer
            info.addPop(BC_REG_RBX);

            if(outId.isPointer()){
                int size = info.ast->getTypeSize(outId);

                u8 reg = RegBySize(1, size);

                info.code->add({BC_MOV_MR, BC_REG_RBX, reg});
                info.addPush(reg);
            } else {
                GeneratePush(info, BC_REG_RBX, 0, outId);
                // TypeInfo* typeInfo = info.ast->getTypeInfo(outId);
                // u32 size = info.ast->getTypeSize(outId);
                // if (!typeInfo || !typeInfo->astStruct) {
                //     u8 reg = RegBySize(1, size); // get the appropriate register

                //     info.code->add({BC_MOV_MR, BC_REG_RBX, reg});
                //     info.addPush(reg);
                // } else {
                //     auto &members = typeInfo->astStruct->members;
                //     for (int i = (int)members.size() - 1; i >= 0; i--) {
                //         auto &member = typeInfo->astStruct->members[i];
                //         auto memdata = typeInfo->getMember(i);
                //         _GLOG(log::out << "  member " << member.name << "\n";)

                //         u8 reg = RegBySize(1, info.ast->getTypeSize(memdata.typeId));

                //         info.code->add({BC_LI, BC_REG_RCX});
                //         info.code->addIm(memdata.offset);
                //         info.code->add({BC_ADDI, BC_REG_RCX, BC_REG_RBX, BC_REG_RBX});
                //         info.code->add({BC_MOV_MR, BC_REG_RBX, reg});
                //         info.addPush(reg);
                //     }
                // }
            }
            outTypeIds->push_back( outId);
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

            outTypeIds->push_back( ltype);
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

            outTypeIds->push_back( ltype);
        }
        else if (expression->typeId == AST_CAST) {
            int err = GenerateExpression(info, expression->left, &ltype);
            if (err == GEN_ERROR)
                return err;

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
                    ERR_HEAD2(expression->tokenRange) << "cannot cast " << info.ast->typeToString(ltype) << " to " << info.ast->typeToString(castType) << "\n";
                    ERR_END
                    outTypeIds->push_back( ltype); // ltype since cast failed
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
            //     ERR_HEAD2(expression->tokenRange) << "cannot cast "<<info.ast->typeToString(ltype) << " to "<<info.ast->typeToString(castType)<<"\n";
            //     outTypeIds->push_back( ltype; // ltype since cast failed
            //     return GEN_ERROR;
            // }

            //  float f = 7.29;
            // u32 raw = *(u32*)&f;
            // log::out << " "<<(void*)raw<<"\n";

            // u32 sign = raw>>31;
            // u32 expo = ((raw&0x80000000)>>(23));
            // u32 mant = ((raw&&0x80000000)>>(23));
            // log::out << sign << " "<<expo << " "<<mant<<"\n";

            outTypeIds->push_back( castType); // NOTE: we use castType and not ltype
        }
        else if (expression->typeId == AST_FROM_NAMESPACE) {
            info.code->addDebugText("ast-namespaced expr\n");

            auto si = info.ast->getScope(expression->name, info.currentScopeId);
            TypeId exprId;
            int result = GenerateExpression(info, expression->left, &exprId, si->id);
            if (result != GEN_SUCCESS)
                return result;

            outTypeIds->push_back( exprId);
            // info.currentScopeId = savedScope;
        }
        else if (expression->typeId == AST_MEMBER) {
            // TODO: if propfrom is a variable we can directly take the member from it
            //  instead of pushing struct to the stack and then getting it.

            info.code->addDebugText("ast-member expr\n");
            TypeId exprId;

            if(expression->left->typeId == AST_VAR){
                // if (expression->left->name) {
                TypeInfo *typeInfo = info.ast->convertToTypeInfo(expression->left->name, idScope);
                // A simple check to see if the identifier in the expr node is an enum type.
                // no need to check for pointers or so.
                if (typeInfo && typeInfo->astEnum) {
                    i32 enumValue;
                    bool found = typeInfo->astEnum->getMember(expression->name, &enumValue);
                    if (!found) {
                        ERR_HEAD(expression->tokenRange, expression->tokenRange.firstToken << " is not a member of enum " << typeInfo->astEnum->name << "\n";
                        )
                        return GEN_ERROR;
                    }

                    info.code->add({BC_LI, BC_REG_EAX}); // NOTE: fixed size of 4 bytes for enums?
                    info.code->addIm(enumValue);
                    info.addPush(BC_REG_EAX);

                    // outTypeIds->push_back(exprId);

                    outTypeIds->push_back(typeInfo->id);
                    return GEN_SUCCESS;
                }
                // }
            }

            // Todo: use generate reference instead? It only deals with pointers
            //  while expression pops and pushed whole structs on the stack.
            int result = GenerateReference(info,expression,&exprId);
            if(result != GEN_SUCCESS) {
                return GEN_ERROR;
            }

            info.addPop(BC_REG_RBX);
            result = GeneratePush(info,BC_REG_RBX,0, exprId);
            
            outTypeIds->push_back(exprId);

            // int result = GenerateExpression(info, expression->left, &exprId);
            // if (result != GEN_SUCCESS)
            //     return result;

            // // if(ex)
            // TypeInfo *typeInfo = info.ast->getTypeInfo(exprId);
            // // if(!typeInfo)
            // //     return GEN_ERROR;
            // if(exprId.isPointer()) {
            //     TypeId structType = exprId;
            //     structType.setPointerLevel(structType.getPointerLevel()-1);
            //     TypeInfo *typeInfo = info.ast->getTypeInfo(structType);

            //     if(!typeInfo || !typeInfo->astStruct) {
            //         ERR_HEAD2(expression->tokenRange) << info.ast->typeToString(structType) << " is not a struct\n";
            //         ERR_END
            //         return GEN_ERROR;
            //     }

            //     auto memdata = typeInfo->getMember(*expression->name);
            //     if (memdata.index == -1) {
            //         ERR_HEAD2(expression->tokenRange) << *expression->name << " is not a member of struct " << info.ast->typeToString(ltype) << "\n";
            //         ERR_END
            //         return GEN_ERROR;
            //     }

            //     ltype = memdata.typeId;
            //     // auto& member = typeInfo->astStruct->members[meminfo.index];
                
            //     info.addPop(BC_REG_RBX);
            //     GeneratePush(info,BC_REG_RBX,memdata.offset, ltype);

            //     // TypeInfo* memTypeInfo = info.ast->getTypeInfo(ltype);
            //     // int size = info.ast->getTypeSize(ltype);
            //     // if(!memTypeInfo || !memTypeInfo->astStruct) {
            //     //     // enum works here too
            //     //     u8 reg = RegBySize(1, size);
            //     //     info.code->add({BC_LI, BC_REG_RAX});
            //     //     info.code->addIm(memdata.offset);
            //     //     info.code->add({BC_ADDI, BC_REG_RBX, BC_REG_RAX, BC_REG_RBX});
            //     //     info.code->add({BC_MOV_MR, BC_REG_RBX, reg});
            //     //     info.addPush(reg);
                    
            //     // } else if(memTypeInfo->astStruct) {
            //     //     Assert(("deref on struct member",false));
            //     //     // for(int i = 0; i < (int) memTypeInfo->astStruct->members.size(); i++){
            //     //     //     auto& member = typeInfo->astStruct->members[i];

            //     //     // }
            //     // } 
            //     outTypeIds->push_back( ltype);
            // } else if (typeInfo && typeInfo->astStruct) {
            //     auto memdata = typeInfo->getMember(*expression->name);

            //     if (memdata.index == -1) {
            //         // TODO: print a list of members?
            //         ERR_HEAD(expression->tokenRange, "'"<<*expression->name << "' is not a member of struct '" << info.ast->typeToString(exprId) << "'. "
            //                 "These are the members: ";
            //             for(int i=0;i<(int)typeInfo->astStruct->members.size();i++){
            //                 if(i!=0)
            //                     log::out << ", ";
            //                 log::out << log::LIME << typeInfo->astStruct->members[i].name<<log::SILVER<<": "<<info.ast->typeToString(typeInfo->getMember(i).typeId);
            //             }
            //             log::out <<"\n";
            //             log::out <<"\n";
            //             ERR_LINE(expression->tokenRange, "not a member");
            //         )
            //         return GEN_ERROR;
            //     }

            //     ltype = memdata.typeId;

            //     // info.code->add({BC_MOV_RR, BC_REG_SP, BC_REG_BX});
                
            //     // TODO: Does not work if member is a larger struct.
            //     //  A struct cannot be into registers.
            //     int freg = 0;
            //     info.code->addDebugText("ast-member extract\n");
            //     for (int i = 0; i < (int)typeInfo->astStruct->members.size(); i++) {
            //         // auto &member = typeInfo->astStruct->members[i];
            //         auto mdata = typeInfo->getMember(i);
            //         // TypeInfo *minfo = info.ast->getTypeInfo(member.typeId);
            //         u32 size = info.ast->getTypeSize(mdata.typeId);

            //         if (i == memdata.index) {
            //             freg = RegBySize(1, size);
            //             info.addPop(freg);
            //         } else {
            //             int reg = RegBySize(4, size);
            //             info.addPop(reg);
            //         }
            //     }
            //     info.addPush(freg);

            //     outTypeIds->push_back( ltype);
            // } else if (typeInfo && typeInfo->astEnum) {
            //     int enumValue;
            //     bool found = typeInfo->astEnum->getMember(*expression->name, &enumValue);
            //     if (!found) {
            //         ERR_HEAD2(expression->tokenRange) << expression->tokenRange.firstToken << " is not a member of enum " << typeInfo->astEnum->name << "\n";
            //         ERR_END
            //         return GEN_ERROR;
            //     }

            //     info.code->add({BC_LI, BC_REG_EAX}); // NOTE: fixed size of 4 bytes for enums?
            //     info.code->addIm(enumValue);
            //     info.addPush(BC_REG_EAX);

            //     outTypeIds->push_back( exprId);
            // } else {
            //     ERR_HEAD2(expression->tokenRange) << "member access only works on structs, pointers and enums. " << info.ast->typeToString(exprId) << " isn't one (astStruct/Enum was null at least)\n";
            //     ERR_END
            //     return GEN_ERROR;
            // }
        }
        else if (expression->typeId == AST_INITIALIZER) {
            TypeInfo *structInfo = info.ast->getTypeInfo(castType); // TODO: castType should be renamed
            // TypeInfo *structInfo = info.ast->getTypeInfo(info.currentScopeId, Token(*expression->name));
            if (!structInfo || !structInfo->astStruct) {
                auto str = info.ast->typeToString(castType);
                ERR_HEAD2(expression->tokenRange) << "cannot do initializer on non-struct " << log::YELLOW << str << "\n";
                ERR_END
                return GEN_ERROR;
            }

            ASTStruct *astruct = structInfo->astStruct;

            // store expressions in a list so we can iterate in reverse
            // TODO: parser could arrange the expressions in reverse.
            //   it would probably be easier since it constructs the nodes
            //   and has a little more context and control over it.
            std::vector<ASTExpression *> exprs;
            exprs.resize(astruct->members.size(), nullptr);

            Assert(expression->args)
            for (int index = 0; index < (int)expression->args->size(); index++) {
                ASTExpression *expr = expression->args->get(index);

                if (!expr->namedValue) {
                    if ((int)exprs.size() <= index) {
                        ERR_HEAD2(expr->tokenRange) << "To many members for struct " << astruct->name << " (" << astruct->members.size() << " member(s) allowed)\n";
                        ERR_END
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
                        ERR_HEAD2(expr->tokenRange) << *expr->namedValue << " is not an member in " << astruct->name << "\n";
                        ERR_END
                        continue;
                    } else {
                        if (exprs[memIndex]) {
                            ERR_HEAD2(expr->tokenRange) << "argument for " << astruct->members[memIndex].name << " is already specified at " << LOGAT(exprs[memIndex]->tokenRange) << "\n";
                            ERR_END
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

            int index = (int)exprs.size();
            while (index > 0) {
                index--;
                ASTExpression *expr = exprs[index];
                TypeId exprId={};
                if (!expr) {
                    exprId = structInfo->getMember(index).typeId;
                    int result = GenerateDefaultValue(info, exprId, nullptr);
                    if (result != GEN_SUCCESS)
                        return result;
                    // ERR_HEAD(expression->tokenRange, "Missing argument for " << astruct->members[index].name << " (call to " << astruct->name << ").\n";
                    // )
                    // continue;
                } else {
                    int result = GenerateExpression(info, expr, &exprId);
                    if (result != GEN_SUCCESS)
                        return result;
                }

                if (index >= (int)structInfo->astStruct->members.size()) {
                    // ERR() << "To many arguments! func:"<<*expression->funcName<<" max: "<<astFunc->arguments.size()<<"\n";
                    continue;
                }
                TypeId tid = structInfo->getMember(index).typeId;
                if (!PerformSafeCast(info, exprId, tid)) {   // implicit conversion
                    // if(astFunc->arguments[index].typeId!=dt){ // strict, no conversion
                    ERRTYPE(expr->tokenRange, exprId, tid, "(initializer)\n");
                    ERR_END
                    continue;
                }
            }
            // if((int)exprs.size()!=(int)structInfo->astStruct->members.size()){
            //     ERR_HEAD2(expression->tokenRange) << "Found "<<exprs.size()<<" initializer values but "<<*expression->name<<" requires "<<structInfo->astStruct->members.size()<<"\n";
            //     log::out <<log::RED<< "LN "<<expression->tokenRange.firstFToken.line <<": "; expression->tokenRange.print();log::out << "\n";
            //     // return GEN_ERROR;
            // }

            outTypeIds->push_back( structInfo->id);
        }
        else if (expression->typeId == AST_SLICE_INITIALIZER) {

            // TypeInfo* typeInfo = info.ast->getTypeInfo(*expression->name);
            // if(!structInfo||!structInfo->astStruct){
            //     ERR_HEAD2(expression->tokenRange) << "cannot do initializer on non-struct "<<log::YELLOW<<*expression->name<<"\n";
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
            //         ERR_HEAD2(expr->tokenRange) << "Type mismatch (initializer): "<<*info.ast->getTypeInfo(exprId)<<" - "<<*info.ast->getTypeInfo(structInfo->astStruct->members[index].typeId)<<"\n";
            //         continue;
            //     }
            // }
            // if((int)exprs.size()!=(int)structInfo->astStruct->members.size()){
            //     ERR_HEAD2(expression->tokenRange) << "Found "<<index<<" arguments but "<<*expression->name<<" requires "<<structInfo->astStruct->members.size()<<"\n";
            //     // return GEN_ERROR;
            // }

            // outTypeIds->push_back( structInfo->id;
        }
        else if (expression->typeId == AST_FROM_NAMESPACE) {
            // TODO: chould not work on enums? Enum.One is used instead
            // TypeInfo* typeInfo = info.ast->getTypeInfo(*expression->name);
            // if(!typeInfo||!typeInfo->astEnum){
            //     ERR_HEAD2(expression->tokenRange) << "cannot access "<<(expression->member?*expression->member:"?")<<" from non-enum "<<*expression->name<<"\n";
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

            //     outTypeIds->push_back( AST_INT32;
            // }
        }
        else if(expression->typeId==AST_RANGE){
            TypeInfo* typeInfo = info.ast->convertToTypeInfo(Token("Range"), info.ast->globalScopeId);
            if(!typeInfo){
                ERR_HEAD(expression->tokenRange, expression->tokenRange<<" cannot be converted to struct Range since it doesn't exist. Did you forget #import \"Basic\".\n\n";
                    ERR_LINE(expression->tokenRange,"Where is Range?");
                )
                return GEN_ERROR;
            }
            Assert(typeInfo->astStruct);
            Assert(typeInfo->astStruct->members.size() == 2);
            Assert(typeInfo->astStruct->baseImpl.members[0].typeId == typeInfo->astStruct->baseImpl.members[1].typeId);

            TypeId inttype = typeInfo->astStruct->baseImpl.members[0].typeId;

            TypeId ltype={};
            int result = GenerateExpression(info, expression->left,&ltype,idScope);
            if(result!=GEN_SUCCESS)
                return GEN_ERROR;
            TypeId rtype={};
            result = GenerateExpression(info, expression->right,&rtype,idScope);
            if(result!=GEN_SUCCESS)
                return GEN_ERROR;
            
            int lreg = RegBySize(3, info.ast->getTypeSize(ltype));
            int rreg = RegBySize(4, info.ast->getTypeSize(rtype));
            info.addPop(rreg);
            info.addPop(lreg);

            info.addPush(rreg);
            if(!PerformSafeCast(info,rtype,inttype)){
                std::string msg = info.ast->typeToString(rtype);
                ERR_HEAD(expression->right->tokenRange,"Cannot convert to "<<info.ast->typeToString(inttype)<<".\n\n";
                    ERR_LINE(expression->right->tokenRange,msg.c_str());
                )
            }

            info.addPush(lreg);
            if(!PerformSafeCast(info,ltype,inttype)){
                std::string msg = info.ast->typeToString(ltype);
                ERR_HEAD(expression->right->tokenRange,"Cannot convert to "<<info.ast->typeToString(inttype)<<".\n\n";
                    ERR_LINE(expression->right->tokenRange,msg.c_str());
                )
            }

            outTypeIds->push_back(typeInfo->id);
            return GEN_SUCCESS;
        } 
        else if(expression->typeId == AST_INDEX){
            // TOKENINFO(expression->tokenRange);
            int err = GenerateExpression(info, expression->left, &ltype);
            if (err != GEN_SUCCESS)
                return GEN_ERROR;

            TypeId rtype;
            err = GenerateExpression(info, expression->right, &rtype);
            if (err != GEN_SUCCESS)
                return GEN_ERROR;

            if(!ltype.isPointer()){
                std::string strtype = info.ast->typeToString(ltype);
                ERR_HEAD(expression->right->tokenRange, "Index operator ( array[23] ) requires pointer type in the outer expression. '"<<strtype<<"' is not a pointer.\n\n";
                    ERR_LINE(expression->right->tokenRange,strtype.c_str());
                )
                return GEN_ERROR;
            }
            if(!AST::IsInteger(rtype)){
                std::string strtype = info.ast->typeToString(rtype);
                ERR_HEAD(expression->right->tokenRange, "Index operator ( array[23] ) requires integer type in the inner expression. '"<<strtype<<"' is not an integer.\n\n";
                    ERR_LINE(expression->right->tokenRange,strtype.c_str());
                )
                return GEN_ERROR;
            }
            ltype.setPointerLevel(ltype.getPointerLevel()-1);

            u32 lsize = info.ast->getTypeSize(ltype);
            u32 rsize = info.ast->getTypeSize(rtype);
            u8 reg = RegBySize(4,rsize);
            info.addPop(reg); // integer
            info.addPop(BC_REG_RBX); // reference
            info.code->add({BC_LI,BC_REG_EAX});
            info.code->addIm(lsize);
            info.code->add({BC_MULI, reg, BC_REG_EAX, reg});
            info.code->add({BC_ADDI, BC_REG_RBX, reg, BC_REG_RBX});

            int result = GeneratePush(info, BC_REG_RBX, 0, ltype);

            outTypeIds->push_back(ltype);
        }
        else if(expression->typeId == AST_INCREMENT || expression->typeId == AST_DECREMENT){
            int result = GenerateReference(info, expression->left, &ltype, idScope);
            if(result != GEN_SUCCESS){
                return GEN_ERROR;
            }
            
            if(!AST::IsInteger(ltype)){
                std::string strtype = info.ast->typeToString(ltype);
                ERR_HEAD(expression->left->tokenRange, "Increment/decrement only works on integer types unless overloaded.\n\n";
                    ERR_LINE(expression->left->tokenRange, strtype.c_str());
                )
                return GEN_ERROR;
            }

            u32 size = info.ast->getTypeSize(ltype);
            u8 reg = RegBySize(1, size);

            info.addPop(BC_REG_RBX); // reference

            info.code->add({BC_MOV_MR, BC_REG_RBX, reg});
            // post
            if(expression->boolValue)
                info.addPush(reg);
            if(expression->typeId == AST_INCREMENT)
                info.code->add({BC_INCR, reg, 1, 0});
            else
                info.code->add({BC_INCR, reg, (u8)-1, (u8)-1});
            info.code->add({BC_MOV_RM, reg, BC_REG_RBX});
            // pre
            if(!expression->boolValue)
                info.addPush(reg);
                
            outTypeIds->push_back(ltype);
        }
        else {
            if(expression->typeId == AST_ASSIGN && castType.getId() == 0){
                // THIS IS PURELY ASSIGN NOT +=, *=
                WARN_HEAD(expression->tokenRange,"Expression is generated first and then reference. Right to left instead of left to right. "
                "This is important if you use assignments/increments/decrements on the same memory/reference/variable.\n\n";
                    WARN_LINE(expression->right->tokenRange,"generated first");
                    WARN_LINE(expression->left->tokenRange,"then this");
                )
                TypeId rtype;
                int result = GenerateExpression(info, expression->right, &rtype, idScope);
                if(result!=GEN_SUCCESS)
                    return GEN_ERROR;

                TypeId assignType={};
                result = GenerateReference(info,expression->left,&assignType,idScope);
                if(result!=GEN_SUCCESS)
                    return GEN_ERROR;

                Assert(assignType == rtype);

                info.addPop(BC_REG_RBX);
                 // note that right expression should be popped first
                result = GeneratePop(info, BC_REG_RBX, 0, rtype);
                result = GeneratePush(info, BC_REG_RBX, 0, rtype);

                // int rsize = info.ast->getTypeSize(rtype);
                // u8 reg = RegBySize(1, rsize);
                // info.addPop(reg);
                // // info.addPop(BC_REG_RBX);
                // info.code->add({BC_MOV_RM, reg, BC_REG_RBX});
                // info.addPush(reg);
                
                outTypeIds->push_back(rtype);
            } else {
                // basic operations
                TypeId rtype;
                int err = GenerateExpression(info, expression->left, &ltype);
                if (err == GEN_ERROR)
                    return err;
                err = GenerateExpression(info, expression->right, &rtype);
                if (err == GEN_ERROR)
                    return err;

                TypeId op = expression->typeId;
                if(expression->typeId==AST_ASSIGN){
                    TypeId assignType={};
                    int result = GenerateReference(info,expression->left,&assignType,idScope);
                    if(result!=GEN_SUCCESS) return GEN_ERROR;
                    Assert(assignType == ltype);
                    op = castType;
                    info.addPop(BC_REG_RBX);
                }
                if(((ltype.isPointer() && AST::IsInteger(rtype)) || (AST::IsInteger(ltype) && rtype.isPointer()))){
                    if (op != AST_ADD && op != AST_SUB) {
                        std::string leftstr = info.ast->typeToString(ltype);
                        std::string rightstr = info.ast->typeToString(rtype);
                        ERR_HEAD(expression->tokenRange, OpToStr((OperationType)op.getId()) << " does not work with pointers. Only addition and subtraction works\n\n";
                            ERR_LINE(expression->left->tokenRange, leftstr.c_str());
                            ERR_LINE(expression->right->tokenRange,rightstr.c_str());
                        )
                        return GEN_ERROR;
                    }

                    u32 lsize = info.ast->getTypeSize(ltype);
                    u32 rsize = info.ast->getTypeSize(rtype);
                    u8 reg1 = RegBySize(4, lsize); // get the appropriate registers
                    u8 reg2 = RegBySize(1, rsize);
                    info.addPop(reg2); // note that right expression should be popped first
                    info.addPop(reg1);

                    u8 bytecodeOp = ASTOpToBytecode(op,false);
                    if(bytecodeOp==0){
                        ERR_HEAD2(expression->tokenRange) << " operation "<<expression->tokenRange.firstToken << " not available for types "<<
                            info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<"\n";
                        ERR_END
                    }

                    if(ltype.isPointer()){
                        info.code->add({bytecodeOp, reg1, reg2, reg1});
                        info.addPush(reg1);
                        if(expression->typeId==AST_ASSIGN){
                            info.code->add({BC_MOV_RM, reg1, BC_REG_RBX});
                        }
                        outTypeIds->push_back(ltype);
                    }else{
                        info.code->add({bytecodeOp, reg1, reg2, reg2});
                        info.addPush(reg2);
                        if(expression->typeId==AST_ASSIGN){
                            info.code->add({BC_MOV_RM, reg2, BC_REG_RBX});
                        }
                        outTypeIds->push_back(rtype);
                    }
                } else if (AST::IsInteger(ltype) && AST::IsInteger(rtype)){
                    // u8 reg = RegBySize(4, info.ast->getTypeSize(rtype));
                    // info.addPop(reg);
                    // if (!PerformSafeCast(info, ltype, rtype)) {
                    //     std::string leftstr = info.ast->typeToString(ltype);
                    //     std::string rightstr = info.ast->typeToString(rtype);
                    //     ERRTYPE(expression->tokenRange, ltype, rtype, "\n\n";
                    //         ERR_LINE(expression->left->tokenRange, leftstr.c_str());
                    //         ERR_LINE(expression->right->tokenRange,rightstr.c_str());
                    //     )
                    //     return GEN_ERROR;
                    // }
                    // ltype = rtype;
                    // info.addPush(reg);

                    TOKENINFO(expression->tokenRange)
                    u32 lsize = info.ast->getTypeSize(ltype);
                    u32 rsize = info.ast->getTypeSize(rtype);
                    u8 reg1 = RegBySize(4, lsize); // get the appropriate registers
                    u8 reg2 = RegBySize(1, rsize);
                    info.addPop(reg2); // note that right expression should be popped first
                    info.addPop(reg1);

                    u8 bytecodeOp = ASTOpToBytecode(op,false);
                    if(bytecodeOp==0){
                        ERR_HEAD2(expression->tokenRange) << " operation "<<expression->tokenRange.firstToken << " not available for types "<<
                            info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<"\n";
                        ERR_END
                    }

                    if(lsize > rsize){
                        info.code->add({bytecodeOp, reg1, reg2, reg1});
                        info.addPush(reg1);
                        if(expression->typeId==AST_ASSIGN){
                            info.code->add({BC_MOV_RM, reg1, BC_REG_RBX});
                        }
                        outTypeIds->push_back(ltype);
                    }else{
                        info.code->add({bytecodeOp, reg1, reg2, reg2});
                        info.addPush(reg2);
                        if(expression->typeId==AST_ASSIGN){
                            info.code->add({BC_MOV_RM, reg2, BC_REG_RBX});
                        }
                        outTypeIds->push_back(rtype);
                    }
                } else if ((ltype == AST_CHAR && AST::IsInteger(rtype))||
                    (AST::IsInteger(ltype)&&rtype == AST_CHAR)){

                    TOKENINFO(expression->tokenRange)
                    u32 lsize = info.ast->getTypeSize(ltype);
                    u32 rsize = info.ast->getTypeSize(rtype);
                    u8 lreg = RegBySize(4, lsize); // get the appropriate registers
                    u8 rreg = RegBySize(1, rsize);
                    info.addPop(rreg); // note that right expression should be popped first
                    info.addPop(lreg);

                    u8 bytecodeOp = ASTOpToBytecode(op,false);
                    if(bytecodeOp==0){
                        ERR_HEAD2(expression->tokenRange) << " operation "<<expression->tokenRange.firstToken << " not available for types "<<
                            info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<"\n";
                        ERR_END
                    }

                    if(ltype == AST_CHAR){
                        info.code->add({bytecodeOp, lreg, rreg, lreg});
                        info.addPush(lreg);
                        if(expression->typeId==AST_ASSIGN){
                            info.code->add({BC_MOV_RM, lreg, BC_REG_RBX});
                        }
                        outTypeIds->push_back(ltype);
                    }else{
                        info.code->add({bytecodeOp, lreg, rreg, rreg});
                        info.addPush(rreg);
                        if(expression->typeId==AST_ASSIGN){
                            info.code->add({BC_MOV_RM, rreg, BC_REG_RBX});
                        }
                        outTypeIds->push_back(rtype);
                    }
                } else {
                    int bad = false;
                    if(ltype.getId()>=AST_TRUE_PRIMITIVES){
                        bad=true;
                        std::string msg = info.ast->typeToString(ltype);
                        ERR_HEAD(expression->left->tokenRange, "Cannot do operation on struct. Overloading not implemented.\n\n";
                            ERR_LINE(expression->left->tokenRange,msg.c_str());
                        )
                    }
                    if(rtype.getId()>=AST_TRUE_PRIMITIVES){
                        bad=true;
                        std::string msg = info.ast->typeToString(rtype);
                        ERR_HEAD(expression->right->tokenRange, "Cannot do operation on struct. Overloading not implemented.\n\n";
                            ERR_LINE(expression->right->tokenRange,msg.c_str());
                        )
                    }
                    if(bad){
                        return GEN_ERROR;
                    }

                    u32 lsize = info.ast->getTypeSize(ltype);
                    u32 rsize = info.ast->getTypeSize(rtype);

                    u8 reg = RegBySize(4, rsize);
                    info.addPop(reg);
                    if (!PerformSafeCast(info, ltype, rtype)) {
                        std::string leftstr = info.ast->typeToString(ltype);
                        std::string rightstr = info.ast->typeToString(rtype);
                        ERRTYPE(expression->tokenRange, ltype, rtype, "\n\n";
                            ERR_LINE(expression->left->tokenRange, leftstr.c_str());
                            ERR_LINE(expression->right->tokenRange,rightstr.c_str());
                        )
                        return GEN_ERROR;
                    }
                    ltype = rtype;
                    info.addPush(reg);

                    TOKENINFO(expression->tokenRange)
                    u8 reg1 = RegBySize(4, lsize); // get the appropriate registers
                    u8 reg2 = RegBySize(1, rsize);
                    info.addPop(reg2); // note that right expression should be popped first
                    info.addPop(reg1);

                    u8 bytecodeOp = ASTOpToBytecode(op,ltype==AST_FLOAT32);
                    if(bytecodeOp==0){
                        ERR_HEAD2(expression->tokenRange) << " operation "<<expression->tokenRange.firstToken << " not available for types "<<
                            info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<"\n";
                        ERR_END
                    }

                    if(ltype.isPointer()){
                        info.code->add({bytecodeOp, reg1, reg2, reg1});
                        outTypeIds->push_back(ltype);
                    }else{
                        info.code->add({bytecodeOp, reg1, reg2, reg2});
                        outTypeIds->push_back(rtype);
                    }
                    if(expression->typeId==AST_ASSIGN){
                        info.code->add({BC_MOV_RM, reg2, BC_REG_RBX});
                    }
                    if(ltype.isPointer()){
                        info.addPush(reg1);
                    }else{
                        info.addPush(reg2);
                    }
                }

            }
        }
    }
    for(auto& typ : *outTypeIds){
        TypeInfo* ti = info.ast->getTypeInfo(typ);
        if(ti){
            Assert(("Leaking virtual type!",ti->id == typ))
        }
    }
    // To avoid ensuring non virtual type everywhere all the entry point where virtual type can enter
    // the expression system is checked. The rest of the system won't have to check it.
    // An entry point has been missed if the assert above fire.
    return GEN_SUCCESS;
}
int GenerateDefaultValue(GenInfo &info, TypeId typeId, TokenRange* tokenRange) {
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
                WARN_HEAD((*tokenRange), "Polymorphism may not work with default values!";)
            }
            if (member.defaultValue) {
                TypeId typeId = {};
                int result = GenerateExpression(info, member.defaultValue, &typeId);
                // TODO: do type check in EvaluateTypes
                if (memdata.typeId != typeId) {
                    ERRTYPE(member.defaultValue->tokenRange, memdata.typeId, typeId, "(default member)\n");
                    ERR_END
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
            Assert(sizeLeft==0); // should only run once
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
    // if(astStruct){
    //     ERR_HEAD2(function->tokenRange) << "Methods have been disabled for now. They will come back after some changes in the compiler.\n";
    //     ERR_END
    //     return GEN_ERROR;
    // }
    // TODO: This does probably not work with structs in structs. Referencing them could cause collisions.
    
    // NOTE: This is the only difference between how methods and functions
    //  are generated.
    if(!astStruct){
        auto fid = info.ast->getIdentifier(info.currentScopeId, function->name);
        if (!fid) {
            // NOTE: function may not have been added in the type checker stage for some reason.
            // THANK YOU, past me for writing this note. I was wondering what I broke and reading the
            // note made instantly realise that I broke something in the type checker.
            ERR_HEAD2(function->tokenRange) << function->name << " was null (compiler bug)\n";
            if (function->tokenRange.firstToken.str) {
                ERRTOKENS(function->tokenRange)
            }
            ERR_END
            return GEN_ERROR;
        }
        if (fid->type != Identifier::FUNC) {
            // I have a feeling some error has been printed if we end up here.
            // Double check that though.
            return GEN_ERROR;
        }
        // ASTFunction* tempFunc = info.ast->getFunction(fid);
        // // Assert(tempFunc==function);
        // if(tempFunc!=function){
        //     // error already printed?
        //     return GEN_ERROR;
        // }
    }
    if(function->body->nativeCode){
        if(function->polyArgs.size()!=0 || (astStruct && astStruct->polyArgs.size()!=0)){
            ERR_HEAD(function->tokenRange, "Function with native code cannot be polymorphic.\n\n";
                ERR_LINE(function->body->tokenRange, "native code");
            )
            return GEN_ERROR;
        }
        // Should not be hardcoded like this
        #define CASE(X,Y) else if (function->name == #X) function->_impls[0]->address = Y;
        if(false) ;
        CASE(FileOpen,BC_EXT_FILEOPEN)
        CASE(FileRead,BC_EXT_FILEREAD)
        CASE(FileWrite,BC_EXT_FILEWRITE)
        CASE(FileClose,BC_EXT_FILECLOSE)
        CASE(malloc,BC_EXT_MALLOC)
        CASE(realloc,BC_EXT_REALLOC)
        CASE(free,BC_EXT_FREE)
        CASE(printi,BC_EXT_PRINTI)
        CASE(printc,BC_EXT_PRINTC)
        CASE(prints,BC_EXT_PRINTS)
        CASE(printd,BC_EXT_PRINTD)
        else {
            ERR_HEAD(function->tokenRange, "'"<<function->name<<"' is not a native function.\n\n";
                ERR_LINE(function->body->tokenRange, "bad");
            )
            return GEN_ERROR;
        }
        #undef CASE

        _GLOG(log::out << "Native function "<<function->name<<"\n";)
        return GEN_SUCCESS;
    }


    _GLOG(log::out << "Function skip\n";)
    info.code->add({BC_JMP});
    int skipIndex = info.code->length();
    info.code->addIm(0);
    int index = 0;
    // std::vector<FuncImpl*> funcImpls;
    // // optimize by not pushing to an array by iterating right away
    // if(astStruct && astStruct->polyArgs.size()==0){
    //     if(function->polyArgs.size()==0){
    //         funcImpls.push_back(&function->baseImpl);
    //     }else{
    //         for(auto& impl : function->polyImpls){
    //             funcImpls.push_back(impl);
    //         }
    //     }
    // } else {
    //     if(function->polyArgs.size()==0){
    //         funcImpls.push_back(&function->baseImpl);
    //     } else {
    //         for(auto& impl : function->polyImpls){
    //             funcImpls.push_back(impl);
    //         }
    //     }
    // }
    // FuncImpl* funcImpl = &function->baseImpl;
    // while((index < (int)function->polyImpls.size()) || (function->polyArgs.size() == 0 && index==0)) {
    //     if(function->polyArgs.size()!=0) {
    //         funcImpl = function->polyImpls[index];
    //     }
    //     index++;
    for(auto& funcImpl : function->getImpls()) {
        Assert(("func has already been generated!",funcImpl->address == 0));
        _GLOG(log::out << "Function " << funcImpl->name << "\n";)
        // This happens with functions inside of polymorphic function.
        
        funcImpl->address = info.code->length();


        auto prevFunc = info.currentFunction;
        auto prevFuncImpl = info.currentFuncImpl;
        auto prevScopeId = info.currentScopeId;
        info.currentFunction = function;
        info.currentFuncImpl = funcImpl;
        info.currentScopeId = function->scopeId;
        defer { info.currentFunction = prevFunc;
            info.currentFuncImpl = prevFuncImpl;
            info.currentScopeId = prevScopeId; };

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

        if (function->arguments.size() != 0) {
            _GLOG(log::out << "set " << function->arguments.size() << " args\n");
            // int offset = 0;
            for (int i = 0; i < (int)function->arguments.size(); i++) {
                // for(int i = function->arguments.size()-1;i>=0;i--){
                auto &arg = function->arguments[i];
                auto &argImpl = funcImpl->argumentTypes[i];
                auto var = info.ast->addVariable(info.currentScopeId, arg.name);
                if (!var) {
                    ERR_HEAD(arg.name.range(), arg.name << " is already defined.\n";
                        ERR_LINE(arg.name.range(),"cannot use again");
                    )
                }
                var->typeId = argImpl.typeId;
                TypeInfo *typeInfo = info.ast->getTypeInfo(argImpl.typeId);

                var->frameOffset = GenInfo::ARG_OFFSET + argImpl.offset;
                _GLOG(log::out << " " <<"["<<var->frameOffset<<"] "<< arg.name << ": " << info.ast->typeToString(argImpl.typeId) << "\n";)
            }
            _GLOG(log::out << "\n";)
        }
        // info.currentFrameOffset = 0;
        if (funcImpl->returnTypes.size() != 0) {
            _GLOG(log::out << "space for " << funcImpl->returnTypes.size() << " return value(s) (struct may cause multiple push)\n");
            
            info.code->addDebugText("ZERO init return values\n");
            info.code->add({BC_LI, BC_REG_RCX});
            info.code->addIm(-funcImpl->returnSize);
            info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RCX, BC_REG_RBX});
            info.code->add({BC_ZERO_MEM, BC_REG_RBX, BC_REG_RCX});
            info.addStackSpace(-funcImpl->returnSize);
            _GLOG(log::out << "\n";)
            info.currentFrameOffset -= funcImpl->returnSize; // TODO: size can be uneven like 13. FIX IN EVALUATETYPES
        }
        if(astStruct){
            for(int i=0;i<(int)astStruct->polyArgs.size();i++){
                auto& arg = astStruct->polyArgs[i];
                arg.virtualType->id = funcImpl->structImpl->polyIds[i];
            }
        }
        for(int i=0;i<(int)function->polyArgs.size();i++){
            auto& arg = function->polyArgs[i];
            arg.virtualType->id = funcImpl->polyIds[i];
        }

        // TODO: MAKE SURE SP IS ALIGNED TO 8 BYTES, 16 could work to.
        // SHOULD stackAlignment, virtualStackPointer be reset and then restored?
        // ALSO DO IT BEFORE CALLING FUNCTION (FNCALL)
        //
        // TODO: GenerateBody may be called multiple times for the same body with
        //  polymorphic functions. This is a problem since GenerateBody generates functions.
        //  Generating them multiple times for each function is bad.
        // NOTE: virtualTypes from this function may be accessed from a function within it's body.
        //  This could be bad.
        int result = GenerateBody(info, function->body);
        info.currentFrameOffset = lastOffset;

        for(int i=0;i<(int)function->polyArgs.size();i++){
            auto& arg = function->polyArgs[i];
            arg.virtualType->id = {}; // disable types
        }
        if(astStruct){
            for(int i=0;i<(int)astStruct->polyArgs.size();i++){
                auto& arg = astStruct->polyArgs[i];
                arg.virtualType->id = {}; // disable types
            }
        }
        info.code->addDebugText("restore stack\n");
        info.restoreStackMoment(startSP); // TODO: MAJOR BUG! RESTORATION OBIOUSLY DOESN'T WORK HERE SINCE
        // IF THE BODY RETURNED, THE INSTRCUTION WILL BE SKIPPED

        // log::out << *function->name<<" " <<function->returnTypes.size()<<"\n";
        if (funcImpl->returnTypes.size() != 0) {
            // check last statement for a return and "exit" early
            bool foundReturn = function->body->statements.size()>0
                && function->body->statements.get(function->body->statements.size()-1)
                ->type == ASTStatement::RETURN;
            // TODO: A return statement might be okay in an inner scope and not necessarily the
            //  top scope.
            if(!foundReturn){
                for(auto it : function->body->statements){
                    if (it->type == ASTStatement::RETURN) {
                        foundReturn = true;
                        break;
                    }
                }
                if (!foundReturn) {
                    ERR() << "missing return statement in " << function->name << "\n";
                }
            }
        }
        if(info.code->length()<1 || info.code->get(info.code->length()-1)->opcode!=BC_RET) {
            // add return if it doesn't exist
            info.code->add({BC_RET});
        }
        for (auto &arg : function->arguments) {
            info.ast->removeIdentifier(info.currentScopeId, arg.name);
        }
    }
    *((u32 *)info.code->get(skipIndex)) = info.code->length();
    return GEN_SUCCESS;
}
int GenerateFunctions(GenInfo& info, ASTScope* body){
    using namespace engone;
    MEASURE;
    _GLOG(FUNC_ENTER)
    Assert(body)

    ScopeId savedScope = info.currentScopeId;
    defer { info.currentScopeId = savedScope; };

    info.currentScopeId = body->scopeId;

    for(auto it : body->namespaces) {
        int result = GenerateFunctions(info, it);
    }
    for(auto it : body->functions) {
        int result = GenerateFunctions(info, it->body);

        result = GenerateFunction(info, it);
    }
    for(auto it : body->structs) {
        for (auto function : it->functions) {
            int result = GenerateFunctions(info, function->body);

            result = GenerateFunction(info, function, it);
        }
    }
    return true;
}
int GenerateBody(GenInfo &info, ASTScope *body) {
    using namespace engone;
    MEASURE;
    _GLOG(FUNC_ENTER)
    Assert(body);

    ScopeId savedScope = info.currentScopeId;
    defer { info.currentScopeId = savedScope; };

    info.currentScopeId = body->scopeId;

    //-- Begin by generating functions
    // ASTFunction *nextFunction = body->functions;
    // while (nextFunction) {
    //     ASTFunction *function = nextFunction;
    //     nextFunction = nextFunction->next;

    //     int result = GenerateFunction(info, function);
    // }
    // ASTStruct* nextStruct = body->structs;
    // while(nextStruct) {
    //     ASTStruct* nowStruct = nextStruct;
    //     nextStruct = nextStruct->next;

    //     ASTFunction *nextFunction = nowStruct->functions;
    //     while (nextFunction) {
    //         ASTFunction *function = nextFunction;
    //         nextFunction = nextFunction->next;

    //         int result = GenerateFunction(info, function, nowStruct);
    //     }   
    // }

    int lastOffset = info.currentFrameOffset;

    for(auto it : body->namespaces) {
        int result = GenerateBody(info, it);
    }

    for (auto statement : body->statements) {
        if (statement->type == ASTStatement::ASSIGN) {
            // TypeId stateTypeId = info.ast->ensureNonVirtualId(statement->typeId);
            _GLOG(SCOPE_LOG("ASSIGN"))
            // TypeId rightType = {};
            if (!statement->rvalue) {
                // solely declaration
                for(auto& varname : statement->varnames){
                    TypeId stateTypeId = info.ast->ensureNonVirtualId(varname.assignType);
                    auto id = info.ast->getIdentifier(info.currentScopeId, varname.name);
                    if (id) {
                        ERR_HEAD(statement->tokenRange, "Identifier " << varname.name << " already declared.\n\n";
                            ERR_LINE(statement->tokenRange,"taken");
                        )
                        continue;
                    }
                    // declare new variable at current stack pointer
                    auto var = info.ast->addVariable(info.currentScopeId,varname.name);

                    // typeid comes from current scope since we do "var: Type;"
                    var->typeId = stateTypeId;
                    TypeInfo *typeInfo = info.ast->getTypeInfo(var->typeId.baseType());
                    i32 size = info.ast->getTypeSize(var->typeId);
                    i32 asize = info.ast->getTypeAlignedSize(var->typeId);
                    if (!typeInfo) {
                        if(info.compileInfo->typeErrors==0){
                            ERR_HEAD(statement->tokenRange, "Undefined type '" << info.ast->typeToString(var->typeId) << "'.\n\n";
                                ERR_LINE(statement->tokenRange,"bad");
                            )
                        }
                        continue;
                    }
                    if (size == 0) {
                        ERR_HEAD(statement->tokenRange, "Cannot use type '" << info.ast->typeToString(var->typeId) << "' which has 0 in size.\n\n";
                            ERR_LINE(statement->tokenRange,"bad");
                        )
                        continue;
                    }

                    if (varname.arrayLength>0){
                        // make sure type is a slice?
                        // it will always be at the moment of writing since arrayLength is only set
                        // when slice is used but this may not be true in the future.
                        int arrayOffset = 0;
                        {
                            TypeId innerType = typeInfo->structImpl->polyIds[0];
                            if(!innerType.isValid())
                                continue; // error message should have been printed in type checker
                            i32 size2 = info.ast->getTypeSize(innerType);
                            i32 asize2 = info.ast->getTypeAlignedSize(innerType);
                            
                            // Assert(size2 * varname.arrayLength <= pow(2,16)/2);
                            if(size2 * varname.arrayLength > pow(2,16)/2) {
                                std::string msg = std::to_string(size2) + " * "+ std::to_string(varname.arrayLength) +" = "+std::to_string(size2 * varname.arrayLength);
                                ERR_HEAD(statement->tokenRange, (int)(pow(2,16)/2) << " is the maximum size of arrays on the stack. "<<size2 * varname.arrayLength<<" was used which exceeds that. The limit comes from the instruction BC_INCR which uses a signed 16-bit integer.\n\n";
                                    ERR_LINE(statement->tokenRange, msg.c_str());
                                )
                                continue;
                            }

                            // int result = FramePush(info,var-)

                            int diff = asize2 - (-info.currentFrameOffset) % asize2; // how much to fix alignment
                            if (diff != asize2) {
                                info.currentFrameOffset -= diff; // align
                            }
                            
                            info.currentFrameOffset -= size2 * varname.arrayLength;
                            arrayOffset = info.currentFrameOffset;
                            
                            info.addStackSpace(-size2*varname.arrayLength);
                            
                            // info.code->add({BC_LI,BC_REG_RDX});
                            // info.code->addIm(size2*varname.arrayLength);
                            // info.code->add({BC_ZERO_MEM, BC_REG_SP, BC_REG_RDX});
                        }
                        // data type may be zero if it wasn't specified during initial assignment
                        // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                        // int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
                        // if (diff != asize) {
                        //     info.currentFrameOffset -= diff; // align
                        // }
                        // info.currentFrameOffset -= size;
                        // var->frameOffset = info.currentFrameOffset;

                        int result = FramePush(info,var->typeId,&var->frameOffset,false);

                        // TODO: Don't hardcode this slice stuff
                        // push length
                        info.code->add({BC_LI,BC_REG_RDX});
                        info.code->addIm(varname.arrayLength);
                        info.addPush(BC_REG_RDX);

                        // push ptr
                        info.code->add({BC_LI,BC_REG_RBX});
                        info.code->addIm(arrayOffset);
                        info.code->add({BC_ADDI,BC_REG_RBX, BC_REG_FP, BC_REG_RBX});
                        info.addPush(BC_REG_RBX);
                        continue;
                    }

                    // data type may be zero if it wasn't specified during initial assignment
                    // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                    // int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
                    // if (diff != asize) {
                    //     info.currentFrameOffset -= diff; // align
                    // }
                    // info.currentFrameOffset -= size;
                    // var->frameOffset = info.currentFrameOffset;

                    int result = FramePush(info,var->typeId, &var->frameOffset, true);

                    // TOKENINFO(statement->tokenRange)
                    // int result = GenerateDefaultValue(info, var->typeId, &statement->tokenRange);
                }
                continue;
            }

            if(statement->rvalue->typeId == AST_FNCALL){
                ASTFunction* astFunc = nullptr;
                FuncImpl* funcImpl = nullptr;
                Assert(("GenCheckFuncImpl is broken due to overloading feature",false))
                // TODO: This is also done in GenerateExpression which isn't necessary.
                // But we need to know astFunc and funcImpl here.
                // astFunc and funcImpl need to be passed to Gen.Expr. somehow to avoid
                // duplicate computation.
                int result = GenCheckFuncImpl(info, statement->rvalue, astFunc, funcImpl, -1);
                if(result!=GEN_SUCCESS)
                    continue;
                Assert(astFunc && funcImpl);

                if(funcImpl->returnTypes.size() < statement->varnames.size()) {
                    char msg[100];
                    sprintf(msg,"%d variables",(int)statement->varnames.size());
                    ERR_HEAD(statement->tokenRange, "To many variables.";
                        ERR_LINE(statement->tokenRange, msg);
                        sprintf(msg,"%d return values",(int)funcImpl->returnTypes.size());
                        ERR_LINE(statement->rvalue->tokenRange, msg);
                    )
                    continue;
                }

                for(int i = 0;i<(int)funcImpl->returnTypes.size();i++){
                    TypeId rightType = funcImpl->returnTypes[i].typeId;
                    if((int)statement->varnames.size() <= i){
                        // _GLOG(log::out << log::LIME<<"just pop "<<info.ast->typeToString(rightType)<<"\n";)
                        // GeneratePop(info, 0, 0, rightType);
                        continue;
                    }
                    // _GLOG(log::out << log::LIME <<"assign pop "<<info.ast->typeToString(rightType)<<"\n";)
                    
                    TypeId stateTypeId = statement->varnames[i].assignType;
                    Token* name = &statement->varnames[i].name;
                    auto id = info.ast->getIdentifier(info.currentScopeId, *name);
                    VariableInfo* var = nullptr;
                    if (id) {
                        if (id->type == Identifier::VAR) {
                            var = info.ast->getVariable(id);
                        } else {
                            ERR_HEAD2(statement->tokenRange) << "identifier " << *name << " was not a variable\n";
                            ERR_END
                            continue;
                        }
                    }
                    bool decl = false;
                    if (stateTypeId.isValid()){
                        Assert(!stateTypeId.isValid()); // haven't fixed this part yet
                        if(!PerformSafeCast(info, rightType, stateTypeId)) {
                            ERRTYPE(statement->tokenRange, stateTypeId, rightType, "(assign)\n");
                            ERRTOKENS(statement->tokenRange);
                            ERR_END
                            continue;
                        }
                        // create variable
                        decl = true;
                        if(!var) {  
                            var = info.ast->addVariable(info.currentScopeId, *name);
                        } else {
                            // clear previous variable
                            *var = {};
                        }
                        var->typeId = stateTypeId;
                    } else {
                        // use existing
                        // create if not existent
                        if(var) {
                            if(rightType != var->typeId){
                                ERRTYPE(statement->tokenRange, var->typeId, rightType, "(assign)\n");
                                ERRTOKENS(statement->tokenRange);
                                ERR_END
                                continue;
                            }
                            // if(!PerformSafeCast(info, rightType, var->typeId)){
                            //     ERRTYPE(statement->tokenRange, var->typeId, rightType, "(assign)\n");
                            //     ERRTOKENS(statement->tokenRange);
                            //     ERR_END
                            //     continue;
                            // }
                        } else {
                            decl = true;
                            var = info.ast->addVariable(info.currentScopeId, *name);
                            var->typeId = rightType;
                        }
                    }
                    /*
                    If you are wondering whether frameOffset will collide with other push and pops
                    it won't. push, pop occurs in expressions which are temporary. The only non temporary push
                    is when we do an assignment.
                    */

                    i32 rightSize = info.ast->getTypeSize(rightType);

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
                        if (leftSize == 0) {
                            ERR_HEAD2(statement->tokenRange) << "Size of type " << "?" << " was 0\n";
                            ERR_END
                            continue;
                        }

                        int result = FramePush(info, var->typeId, &var->frameOffset, true);
                        // int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
                        // if (diff != asize) {
                        //     info.currentFrameOffset -= diff; // align
                        // }
                        // info.currentFrameOffset -= leftSize;
                        // var->frameOffset = info.currentFrameOffset;
                        // if(diff!=asize)
                        //     alignment = diff;

                        _GLOG(log::out << "declare " << *name << " at " << var->frameOffset << "\n";)
                        // NOTE: inconsistent
                        // char buf[100];
                        // int len = sprintf(buf," ^ was assigned %s",statement->name->c_str());
                        // info.code->addDebugText(buf,len);
                    }
                    if(var){
                        _GLOG(log::out << " " << *name << " : " << info.ast->typeToString(var->typeId) << "\n";)
                    }
                }
                std::vector<TypeId> rightTypes;
                result = GenerateExpression(info, statement->rvalue, &rightTypes);
                if (result != GEN_SUCCESS) {
                    // assign fails and variable will not be declared potentially causing
                    // undeclared errors in later code. This is probably better than
                    // declaring the variable and using void type. Then you get type mismatch
                    // errors.
                    continue;
                }
                Assert(!funcImpl || funcImpl->returnTypes.size() == rightTypes.size());

                for(int i = 0;i<(int)rightTypes.size();i++){
                    TypeId rightType = rightTypes[i];
                    if((int)statement->varnames.size() <= i){
                        _GLOG(log::out << log::LIME<<"just pop "<<info.ast->typeToString(rightType)<<"\n";)
                        GeneratePop(info, 0, 0, rightType);
                        continue;
                    }
                    _GLOG(log::out << log::LIME <<"assign pop "<<info.ast->typeToString(rightType)<<"\n";)
                    
                    TypeId stateTypeId = statement->varnames[i].assignType;
                    Token* name = &statement->varnames[i].name;
                    auto id = info.ast->getIdentifier(info.currentScopeId, *name);
                    Assert(id);
                    VariableInfo* var = info.ast->getVariable(id);
                    Assert(var);

                    if(!PerformSafeCast(info, rightType, var->typeId)){
                        ERRTYPE(statement->tokenRange, var->typeId, rightType, "(assign)\n");
                        ERRTOKENS(statement->tokenRange);
                        ERR_END
                        continue;
                    }

                    GeneratePop(info, BC_REG_FP, var->frameOffset, var->typeId);
                }
            } else {
                std::vector<TypeId> rightTypes;
                int result = GenerateExpression(info, statement->rvalue, &rightTypes);

                Assert(statement->varnames.size()==1);
                TypeId rightType = AST_VOID;
                if(rightTypes.size()==1)
                    rightType = rightTypes[0];

                _GLOG(log::out << log::LIME <<"assign pop "<<info.ast->typeToString(rightType)<<"\n";)
                
                TypeId stateTypeId = statement->varnames[0].assignType;
                Token* name = &statement->varnames[0].name;
                auto id = info.ast->getIdentifier(info.currentScopeId, *name);
                VariableInfo* var = nullptr;
                if (id) {
                    if (id->type == Identifier::VAR) {
                        var = info.ast->getVariable(id);
                    } else {
                        ERR_HEAD2(statement->tokenRange) << "identifier " << *name << " was not a variable\n";
                        ERR_END
                        continue;
                    }
                }
                bool decl = false;
                if (stateTypeId.isValid()){
                    if(!PerformSafeCast(info, rightType, stateTypeId)) {
                        ERRTYPE(statement->tokenRange, stateTypeId, rightType, "(assign)\n");
                        ERRTOKENS(statement->tokenRange);
                        ERR_END
                        continue;
                    }
                    // create variable
                    decl = true;
                    if(!var) {  
                        var = info.ast->addVariable(info.currentScopeId, *name);
                    } else {
                        // clear previous variable
                        *var = {};
                    }
                    var->typeId = stateTypeId;
                } else {
                    // use existing
                    // create if not existent
                    if(var) {
                        if(!PerformSafeCast(info, rightType, var->typeId)){
                            ERRTYPE(statement->tokenRange, var->typeId, rightType, "(assign).\n\n";
                                ERR_LINE(statement->tokenRange, "bad");
                            )
                            // ERRTOKENS(statement->tokenRange);
                            // ERR_END
                            continue;
                        }
                    } else {
                        decl = true;
                        var = info.ast->addVariable(info.currentScopeId, *name);
                        var->typeId = rightType;
                    }
                }
                /*
                If you are wondering whether frameOffset will collide with other push and pops
                it won't. push, pop occurs in expressions which are temporary. The only non temporary push
                is when we do an assignment.
                */

                i32 rightSize = info.ast->getTypeSize(rightType);

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
                    if (leftSize == 0) {
                        ERR_HEAD2(statement->tokenRange) << "Size of type " << "?" << " was 0\n";
                        ERR_END
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

                    _GLOG(log::out << "declare " << *name << " at " << var->frameOffset << "\n";)
                    // NOTE: inconsistent
                    // char buf[100];
                    // int len = sprintf(buf," ^ was assigned %s",statement->name->c_str());
                    // info.code->addDebugText(buf,len);
                }
                // local variable exists on stack
                // char buf[100];
                // int len = sprintf(buf, "  assign %s\n", name->c_str());
                // info.code->addDebugText(buf, len);

                GeneratePop(info, BC_REG_FP, var->frameOffset, var->typeId);
                
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
                if(var){
                    _GLOG(log::out << " " << *name << " : " << info.ast->typeToString(var->typeId) << "\n";)
                }
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
            //     ERR_HEAD2(statement->expression->token) << "Expected a boolean, not '"<<TypeIdToStr(dtype)<<"'\n";
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
            //     ERR_HEAD2(statement->expression->token) << "Expected a boolean, not '"<<TypeIdToStr(dtype)<<"'\n";
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
        } else if (statement->type == ASTStatement::FOR) {
            _GLOG(SCOPE_LOG("FOR"))

            GenInfo::LoopScope* loopScope = info.pushLoop();
            defer {
                _GLOG(log::out << "pop loop\n");
                bool yes = info.popLoop();
                if(!yes){
                    log::out << log::RED << "popLoop failed (bug in compiler)\n";
                }
            };

            Assert(statement->varnames.size()==1);

            // body scope is used since the for loop's variables
            // shouldn't collide with the variables in the current scope.
            // not sure how well this works, we shall see.
            ScopeId scopeForVariables = statement->body->scopeId;

            if(statement->rangedForLoop){
                Token& itemvar = statement->varnames[0].name;
                TypeId itemtype = statement->varnames[0].assignType;
                if(!itemtype.isValid()){
                    // error has probably been handled somewhere. no need to print again.
                    continue;
                }
                auto* varinfo_index = info.ast->addVariable(scopeForVariables,"nr");
                if(!varinfo_index) {
                    ERR_HEAD(statement->tokenRange, "nr variable already exists\n.";)
                    continue;
                }
                // auto iden = info.ast->getIdentifier(scopeForVariables,"nr");
                // log::out << "IDEN VAR INDEX, "<<iden->varIndex<<"\n";

                auto varinfo_item = info.ast->addVariable(scopeForVariables,itemvar);
                 if(!varinfo_item) {
                    ERR_HEAD(statement->tokenRange, "nr variable already exists\n.";)
                    continue;
                }
                varinfo_item->typeId = itemtype;
                varinfo_index->typeId = itemtype;

                {
                    i32 size = info.ast->getTypeSize(varinfo_index->typeId);
                    i32 asize = info.ast->getTypeAlignedSize(varinfo_index->typeId);
                    // data type may be zero if it wasn't specified during initial assignment
                    // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                    int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
                    if (diff != asize) {
                        info.currentFrameOffset -= diff; // align
                    }
                    info.currentFrameOffset -= size;
                    varinfo_item->frameOffset = info.currentFrameOffset;
                    varinfo_index->frameOffset = info.currentFrameOffset;

                    TypeId dtype = {};
                    // Type should be checked in type checker and further down
                    // when extracting ptr and len. No need to check here.
                    int result = GenerateExpression(info, statement->rvalue, &dtype);
                    if (result != GEN_SUCCESS) {
                        continue;
                    }
                    if(statement->reverse){
                        info.addPop(BC_REG_EDX); // throw range.now
                        info.addPop(BC_REG_EAX);
                    }else{
                        info.addPop(BC_REG_EAX);
                        info.addPop(BC_REG_EDX); // throw range.end
                        info.code->add({BC_INCR,BC_REG_EAX,0xFF,0xFF});
                        // decrement by one since for loop increments
                        // before going into the loop
                    }
                    info.addPush(BC_REG_EAX);
                }
                // i32 itemsize = info.ast->getTypeSize(varinfo_item->typeId);

                _GLOG(log::out << "push loop\n");
                loopScope->stackMoment = info.saveStackMoment();
                loopScope->continueAddress = info.code->length();

                // log::out << "frame: "<<varinfo_index->frameOffset<<"\n";
                // TODO: don't generate ptr and length everytime.
                // put them in a temporary variable or something.
                TypeId dtype = {};
                info.code->addDebugText("Generate and push range\n");
                int result = GenerateExpression(info, statement->rvalue, &dtype);
                if (result != GEN_SUCCESS) {
                    continue;
                }
                u8 index_reg = BC_REG_ECX;
                u8 length_reg = BC_REG_EDX;

                if(statement->reverse){
                    info.addPop(length_reg); // range.now we care about
                    info.addPop(BC_REG_EAX);

                } else {
                    info.addPop(BC_REG_EAX); // throw range.now
                    info.addPop(length_reg); // range.end we care about
                }

                info.code->add({BC_LI, BC_REG_RAX});
                info.code->addIm(varinfo_index->frameOffset);
                info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RAX, BC_REG_RAX});

                info.code->add({BC_MOV_MR, BC_REG_RAX, index_reg});
                if(statement->reverse){
                    info.code->add({BC_INCR,index_reg, 0xFF, 0xFF}); // hexidecimal represent -1
                }else{
                    info.code->add({BC_INCR,index_reg,1});
                }
                info.code->add({BC_MOV_RM,index_reg, BC_REG_RAX});

                if(statement->reverse){
                    info.code->addDebugText("For condition (reversed)\n");
                    info.code->add({BC_GTE,index_reg,length_reg,BC_REG_EAX});
                } else {
                    info.code->addDebugText("For condition\n");
                    info.code->add({BC_LT,index_reg,length_reg,BC_REG_EAX});
                }
                info.code->add({BC_JNE, BC_REG_EAX});
                // resolve end, not break, resolveBreaks is bad naming
                loopScope->resolveBreaks.push_back(info.code->length());
                info.code->addIm(0);

                result = GenerateBody(info, statement->body);
                if (result != GEN_SUCCESS)
                    continue;

                info.code->add({BC_JMP});
                info.code->addIm(loopScope->continueAddress);

                for (auto ind : loopScope->resolveBreaks) {
                    *(u32 *)info.code->get(ind) = info.code->length();
                }
                
                info.ast->removeIdentifier(scopeForVariables, "nr");
                info.ast->removeIdentifier(scopeForVariables, itemvar);
            }else{
                auto* varinfo_index = info.ast->addVariable(scopeForVariables,"nr");
                if(!varinfo_index) {
                    ERR_HEAD(statement->tokenRange, "nr variable already exists\n.";)
                    continue;
                }
                varinfo_index->typeId = AST_INT32;

                {
                    i32 size = info.ast->getTypeSize(varinfo_index->typeId);
                    i32 asize = info.ast->getTypeAlignedSize(varinfo_index->typeId);
                    // data type may be zero if it wasn't specified during initial assignment
                    // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                    // int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
                    // if (diff != asize) {
                    //     info.currentFrameOffset -= diff; // align
                    // }
                    // info.currentFrameOffset -= size;
                    varinfo_index->frameOffset = info.currentFrameOffset;

                    int result = FramePush(info, varinfo_index->typeId, &varinfo_index->frameOffset, false);

                    if(statement->reverse){
                        TypeId dtype = {};
                        // Type should be checked in type checker and further down
                        // when extracting ptr and len. No need to check here.
                        int result = GenerateExpression(info, statement->rvalue, &dtype);
                        if (result != GEN_SUCCESS) {
                            continue;
                        }
                        info.addPop(BC_REG_RAX);
                        info.addPop(BC_REG_RAX);
                        info.code->add({BC_CAST,CAST_UINT_SINT, BC_REG_RAX,BC_REG_EAX});
                    }else{
                        info.code->add({BC_LI,BC_REG_EAX});
                        info.code->addIm(-1);
                    }
                    info.addPush(BC_REG_EAX);
                }
                Token& itemvar = statement->varnames[0].name;
                TypeId itemtype = statement->varnames[0].assignType;
                if(!itemtype.isValid()){
                    // error has probably been handled somewhere. no need to print again.
                    // if it wasn't and you just found out that things went wrong here
                    // then sorry.
                    continue;
                }
                auto varinfo_item = info.ast->addVariable(scopeForVariables,itemvar);
                varinfo_item->typeId = itemtype;
                i32 itemsize = info.ast->getTypeSize(varinfo_item->typeId);
                {
                    // TODO: Automate this code. Pushing and popping variables from the frame is used often and should be functions.
                    i32 asize = info.ast->getTypeAlignedSize(varinfo_item->typeId);
                    if(asize==0)
                        continue;

                    int result = FramePush(info, varinfo_item->typeId, &varinfo_item->frameOffset, true);
                }

                _GLOG(log::out << "push loop\n");
                loopScope->stackMoment = info.saveStackMoment();
                loopScope->continueAddress = info.code->length();

                // TODO: don't generate ptr and length everytime.
                // put them in a temporary variable or something.
                TypeId dtype = {};
                info.code->addDebugText("Generate and push slice\n");
                int result = GenerateExpression(info, statement->rvalue, &dtype);
                if (result != GEN_SUCCESS) {
                    continue;
                }
                TypeInfo* ti = info.ast->getTypeInfo(dtype);
                Assert(ti && ti->astStruct && ti->astStruct->name == "Slice");
                auto memdata_ptr = ti->getMember("ptr");
                auto memdata_len = ti->getMember("len");

                // NOTE: careful when using registers since you might use 
                //  one for multiple things. 
                u8 ptr_reg = RegBySize(2,info.ast->getTypeSize(memdata_ptr.typeId));
                u8 length_reg = RegBySize(4,info.ast->getTypeSize(memdata_len.typeId));
                u8 index_reg = BC_REG_ECX;

                info.code->addDebugText("extract ptr and length\n");
                info.addPop(ptr_reg);
                info.addPop(length_reg);

                info.code->addDebugText("Index increment/decrement\n");
                info.code->add({BC_LI, BC_REG_RAX});
                info.code->addIm(varinfo_index->frameOffset);
                info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RAX, BC_REG_RAX});

                info.code->add({BC_MOV_MR, BC_REG_RAX, index_reg});
                if(statement->reverse){
                    info.code->add({BC_INCR,index_reg, 0xFF, 0xFF}); // hexidecimal represent -1
                }else{
                    info.code->add({BC_INCR,index_reg,1});
                }
                info.code->add({BC_MOV_RM,index_reg, BC_REG_RAX});

                if(statement->reverse){
                    info.code->addDebugText("For condition (reversed)\n");
                    info.code->add({BC_LI,length_reg}); // length reg is not used with reversed
                    info.code->addIm(-1);
                    info.code->add({BC_GT,index_reg,length_reg,BC_REG_EAX});
                } else {
                    info.code->addDebugText("For condition\n");
                    info.code->add({BC_LT,index_reg,length_reg,BC_REG_EAX});
                }
                info.code->add({BC_JNE, BC_REG_EAX});
                // resolve end, not break, resolveBreaks is bad naming
                loopScope->resolveBreaks.push_back(info.code->length());
                info.code->addIm(0);

                // BE VERY CAREFUL SO YOU DON'T USE BUSY REGISTERS AND OVERWRITE
                // VALUES. UNEXPECTED VALUES WILL HAPPEN AND IT WILL BE PAINFUL.
                u8 dst_reg = BC_REG_RCX;
                u8 src_reg = BC_REG_RBX;
                u8 size_reg = BC_REG_RDX;
                info.code->add({BC_LI,size_reg});
                info.code->addIm(itemsize);
                
                info.code->add({BC_MULI, index_reg, size_reg, BC_REG_RAX});
                info.code->add({BC_ADDI, ptr_reg, BC_REG_RAX, src_reg});
                if(statement->pointer){
                    info.code->add({BC_LI,BC_REG_RAX});
                    info.code->addIm(varinfo_item->frameOffset);
                    info.code->add({BC_ADDI,BC_REG_FP, BC_REG_RAX, BC_REG_RAX});
                    
                    info.code->add({BC_MOV_RM, src_reg, BC_REG_RAX});
                } else {
                    info.code->add({BC_LI,BC_REG_RAX});
                    info.code->addIm(varinfo_item->frameOffset);
                    info.code->add({BC_ADDI,BC_REG_FP, BC_REG_RAX, dst_reg});

                    info.code->add({BC_MEMCPY,dst_reg, src_reg, size_reg});
                }

                result = GenerateBody(info, statement->body);
                if (result != GEN_SUCCESS)
                    continue;

                info.code->add({BC_JMP});
                info.code->addIm(loopScope->continueAddress);

                for (auto ind : loopScope->resolveBreaks) {
                    *(u32 *)info.code->get(ind) = info.code->length();
                }
                
                // delete nr, frameoffset needs to be changed to if so
                // info.addPop(BC_REG_EAX); 

                info.ast->removeIdentifier(scopeForVariables, "nr");
                info.ast->removeIdentifier(scopeForVariables, itemvar);
            }

            // pop loop happens in defer
        } else if(statement->type == ASTStatement::BREAK) {
            GenInfo::LoopScope* loop = info.getLoop(info.loopScopes.size()-1);
            if(!loop) {
                ERR_HEAD2(statement->tokenRange) << "Loop was null\n";
                ERR_END
                continue;
            }
            info.restoreStackMoment(loop->stackMoment);
            
            // TODO: Generate defers

            info.code->add({BC_JMP});
            loop->resolveBreaks.push_back(info.code->length());
            info.code->addIm(0);
        } else if(statement->type == ASTStatement::CONTINUE) {
            GenInfo::LoopScope* loop = info.getLoop(info.loopScopes.size()-1);
            if(!loop) {
                ERR_HEAD2(statement->tokenRange) << "Loop was null\n";
                ERR_END
                continue;
            }
            info.restoreStackMoment(loop->stackMoment);
            
            // TODO: Generate defers

            info.code->add({BC_JMP});
            info.code->addIm(loop->continueAddress);
        } else if (statement->type == ASTStatement::RETURN) {
            _GLOG(SCOPE_LOG("RETURN"))

            if (!info.currentFunction) {
                ERR_HEAD2(statement->tokenRange) << "return only allowed in function\n";
                ERR_END
                continue;
                // return GEN_ERROR;
            }
            if ((int)statement->returnValues->size() != (int)info.currentFuncImpl->returnTypes.size()) {
                ERR_HEAD2(statement->tokenRange) << "Found " << statement->returnValues->size() << " return value(s) but should have " << info.currentFuncImpl->returnTypes.size() << " for '" << info.currentFunction->name << "'\n";
                ERR_END
            }

            //-- Evaluate return values
            // ASTExpression *nextExpr = statement->rvalue;
            // int argi = -1;
            for (int argi = 0; argi < (int)statement->returnValues->size(); argi++) {
                ASTExpression *expr = statement->returnValues->at(argi);
                // nextExpr = nextExpr->next;
                // argi++;

                TypeId dtype = {};
                int result = GenerateExpression(info, expr, &dtype);
                if (result == GEN_ERROR) {
                    continue;
                }
                if (argi >= (int)info.currentFuncImpl->returnTypes.size()) {
                    continue;
                }
                // auto a = info.ast->typeToString(dtype);
                // auto b = info.ast->typeToString(info.currentFuncImpl->returnTypes[argi].typeId);
                auto& retType = info.currentFuncImpl->returnTypes[argi];
                if (!PerformSafeCast(info, dtype, retType.typeId)) {
                    // if(info.currentFunction->returnTypes[argi]!=dtype){

                    ERRTYPE(expr->tokenRange, dtype, info.currentFuncImpl->returnTypes[argi].typeId, "(return values)\n");
                    ERR_END
                }
                GeneratePop(info, BC_REG_FP, retType.offset, retType.typeId);
                // TypeInfo *typeInfo = 0;
                // if(retType.typeId.isNormalType())
                //     typeInfo = info.ast->getTypeInfo(retType.typeId);
                // u32 size = info.ast->getTypeSize(retType.typeId);
                // if (!typeInfo || !typeInfo->astStruct) {
                //     _GLOG(log::out << "move return value\n";)
                //     u8 reg = RegBySize(1, size);
                //     info.addPop(reg);
                //     info.code->add({BC_LI, BC_REG_RBX});
                //     info.code->addIm(retType.offset);
                //     info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX});
                //     info.code->add({BC_MOV_RM, reg, BC_REG_RBX});
                // } else {
                //     for (int i = 0; i < (int)typeInfo->astStruct->members.size(); i++) {
                //         auto &member = typeInfo->astStruct->members[i];
                //         auto memdata = typeInfo->getMember(i);
                //         u32 msize = info.ast->getTypeSize(memdata.typeId);
                //         _GLOG(log::out << "move return value member " << member.name << "\n";)
                //         u8 reg = RegBySize(1, msize);
                //         info.addPop(reg);
                //         info.code->add({BC_LI, BC_REG_RBX});
                //         // retType.offset is negative and member.offset is positive which is correct
                //         info.code->addIm(retType.offset + memdata.offset);
                //         info.code->add({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX});
                //         info.code->add({BC_MOV_RM, reg, BC_REG_RBX});
                //     }
                // }
                // _GLOG(log::out << "\n";)
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
        else if (statement->type == ASTStatement::EXPRESSION) {
            _GLOG(SCOPE_LOG("EXPRESSION"))

            TypeId dtype = {};
            int result = GenerateExpression(info, statement->rvalue, &dtype);
            if (result == GEN_ERROR) {
                continue;
            }
            if (dtype.getId() != AST_VOID) {
                // TODO: handle struct
                // TypeInfo *typeInfo = info.ast->getTypeInfo(dtype);
                // u32 size = info.ast->getTypeSize(dtype);
                // u8 reg = RegBySize(1, size);
                // info.addPop(reg);

                GeneratePop(info, 0, 0, dtype);
            }
        }
        else if (statement->type == ASTStatement::USING) {
            _GLOG(SCOPE_LOG("USING"))

            Assert(statement->varnames.size()==1);

            Token* name = &statement->varnames[0].name;

            auto origin = info.ast->getIdentifier(info.currentScopeId, *name);
            if(!origin){
                ERR_HEAD2(statement->tokenRange) << *name << " is not a variable (using)\n";
                ERRTOKENS(statement->tokenRange);
                ERR_END
                return GEN_ERROR;
            }
            auto aliasId = info.ast->addIdentifier(info.currentScopeId, *statement->alias);
            if(!aliasId){
                ERR_HEAD2(statement->tokenRange) << *statement->alias << " is already a variable or alias (using)\n";
                ERRTOKENS(statement->tokenRange);
                ERR_END
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

Bytecode *Generate(AST *ast, CompileInfo* compileInfo) {
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
    info.compileInfo = compileInfo;
    info.currentScopeId = ast->globalScopeId;

    int result = GenerateData(info,info.ast);

    result = GenerateFunctions(info, info.ast->mainBody);
    result = GenerateBody(info, info.ast->mainBody);
    // TODO: What to do about result? nothing?

    std::unordered_map<FuncImpl*, int> resolveFailures;
    for(auto& e : info.callsToResolve){
        auto inst = info.code->get(e.bcIndex);
        // Assert(e.funcImpl->address != Identifier::INVALID_FUNC_ADDRESS);
        if(e.funcImpl->address != FuncImpl::INVALID_FUNC_ADDRESS){
            *((i32*)inst) = e.funcImpl->address;
        } else {
            // ERR() << "Invalid function address for instruction["<<e.bcIndex << "]\n";
            auto pair = resolveFailures.find(e.funcImpl);
            if(pair == resolveFailures.end())
                resolveFailures[e.funcImpl] = 1;
            else
                pair->second++;
        }
    }
    if(resolveFailures.size()!=0){
        log::out << log::RED << "Invalid function resolutions:\n";
        for(auto& pair : resolveFailures){
            info.errors += pair.second;
            log::out << log::RED << " "<<pair.first->name <<": "<<pair.second<<" bad address(es)\n";
        }
    }
    info.callsToResolve.clear();

    // for (auto fun : predefinedFuncs) {
    //     ast->destroy(fun);
    // }

    // compiler logs the error count, dont need it here too
    // if(info.errors)
    //     log::out << log::RED<<"Generator failed with "<<info.errors<<" error(s)\n";
    info.compileInfo->errors += info.errors;
    return info.code;
}