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

#define TOKENINFO(R)
    //                                                                         \
    // {                                                                                           \
    //     std::string temp = "";                                                                  \
    //     R.feed(temp);                                                                           \
    //     info.code->addDebugText(std::string("Ln ") + std::to_string(R.firstToken.line) + ": "); \
    //     info.code->addDebugText(temp + "\n");                                                   \
    // }

#define MAKE_NODE_SCOPE(X) info.pushNode(dynamic_cast<ASTNode*>(X));NodeScope nodeScope{&info};

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
void GenInfo::pushNode(ASTNode* node){
    nodeStack.add(node);
}
void GenInfo::popNode(){
    nodeStack.pop();
}
void GenInfo::addCallToResolve(int bcIndex, FuncImpl* funcImpl){
    ResolveCall tmp{};
    tmp.bcIndex = bcIndex;
    tmp.funcImpl = funcImpl;
    callsToResolve.add(tmp);
}
void GenInfo::addCall(LinkConventions linkConvention, CallConventions callConvention){
    addInstruction({BC_CALL, (u8)linkConvention, (u8)callConvention},true);
}
void GenInfo::addInstruction(Instruction inst, bool bypassAsserts){
    using namespace engone;

    // This is here to prevent mistakes.
    // the stack is kept track of behind the scenes with info.addPush, info.addPop.
    // using info.addInstruction would bypass that which would cause bugs.
    if(!bypassAsserts){
        Assert((inst.opcode!=BC_POP && inst.opcode!=BC_PUSH && inst.opcode != BC_CALL && inst.opcode != BC_LI));
    }

    if(inst.opcode == BC_POP && code->length()>0
         && code->get(code->length()-1).opcode == BC_PUSH
         && inst.op0 == code->get(code->length()-1).op0) {
        code->codeSegment.used--;
        
        if(code->codeSegment.used>0){
            for(int i = code->codeSegment.used-1;i>=0;i--){
                u32 locIndex = -1;
                auto loc = code->getLocation(i, &locIndex);
                if(loc){
                    lastLine = loc->line;
                    lastStream = (TokenStream*)loc->stream;
                    lastLocationIndex = locIndex;
                    break;
                }
            }
        }
        return;
    }

    // FP (base pointer in x64) is callee-saved
    Assert(inst.opcode != BC_RET || (code->length()>0 &&
        code->get(code->length()-1).opcode == BC_POP &&
        code->get(code->length()-1).op0 == BC_REG_FP));

    Assert(nodeStack.size()!=0);
    code->add(inst);
    if(nodeStack.last()->tokenRange.firstToken.line == lastLine &&
        nodeStack.last()->tokenRange.firstToken.tokenStream == lastStream){
        // log::out << "loc "<<lastLocationIndex<<"\n";
        auto location = code->setLocationInfo(lastLocationIndex,code->length()-1);
    } else {
        lastLine = nodeStack.last()->tokenRange.firstToken.line;
        lastStream = nodeStack.last()->tokenRange.firstToken.tokenStream;
        auto location = code->setLocationInfo(nodeStack.last()->tokenRange.firstToken,code->length()-1, &lastLocationIndex);
        // log::out << "new loc "<<lastLocationIndex<<"\n";
        if(nodeStack.last()->tokenRange.tokenStream())
            location->desc = nodeStack.last()->tokenRange.firstToken.getLine();
    }
}
void GenInfo::addLoadIm(u8 reg, i32 value){
    addInstruction({BC_LI,reg}, true);
    code->addIm(value);
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
    size = 8;

    addInstruction({BC_POP, (u8)reg}, true);
    // if errors != 0 then don't assert and just return since the stack
    // is probably messed up because of the errors. OR you try
    // to manage the stack even with errors. Unnecessary work though so don't!? 
    WHILE_TRUE {
        if(errors==0){
            Assert(("bug in compiler!", !stackAlignment.empty()));
        }
        auto align = stackAlignment.back();
        if(errors==0 && align.size!=0){
            // size of 0 could mean extra alignment for between structs
            Assert(("bug in compiler!", align.size == size));
        }
        // You pushed some size and tried to pop a different size.
        // Did you copy paste some code involving addPop/addPush recently?

        stackAlignment.pop_back();
        if (align.diff != 0) {
            virtualStackPointer += align.diff;
            // code->addDebugText("align sp\n");
            i16 offset = align.diff;
            addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
        }
        if(align.size == 0)
            continue;
        virtualStackPointer += size;
        break;
    }
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
    size = 8; // force 8 bytes

    int diff = (size - (-virtualStackPointer) % size) % size; // how much to increment sp by to align it
    // TODO: Instructions are generated from top-down and the stackAlignment
    //   sees pop and push in this way but how about jumps. It doesn't consider this. Is it an issue?
    if (diff) {
        virtualStackPointer -= diff;
        // code->addDebugText("align sp\n");
        i16 offset = -diff;
        addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
    }
    addInstruction({BC_PUSH, (u8)reg}, true);
    stackAlignment.push_back({diff, size});
    virtualStackPointer -= size;
    _GLOG(log::out << "relsp "<<virtualStackPointer<<"\n";)
}
void GenInfo::addIncrSp(i16 offset) {
    using namespace engone;
    if (offset == 0)
        return;
    // Assert(offset>0); // TOOD: doesn't handle decrement of sp
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
    addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
    _GLOG(log::out << "relsp "<<virtualStackPointer<<"\n";)
}
void GenInfo::addAlign(int alignment){
    int diff = (alignment - (-virtualStackPointer) % alignment) % alignment; // how much to increment sp by to align it
    // TODO: Instructions are generated from top-down and the stackAlignment
    //   sees pop and push in this way but how about jumps. It doesn't consider this. Is it an issue?
    if (diff) {
        addIncrSp(-diff);
        // virtualStackPointer -= diff;
        // code->addDebugText("align sp\n");
        // i16 offset = -diff;
        // code->add({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
    }
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
            // code->addDebugText("align sp\n");
            i16 offset = -diff;
            addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
        }
        
        addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & size), (u8)(size >> 8)});
        stackAlignment.push_back({diff, -size});
        virtualStackPointer += size;
    } else if(size > 0) {
        addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & size), (u8)(size >> 8)});

        // code->add({BC_POP, (u8)reg});
        Assert(("bug in compiler!", !stackAlignment.empty()));
        auto align = stackAlignment.back();
        Assert(("bug in compiler!", align.size == size));
        // You pushed some size and tried to pop a different size.
        // Did you copy paste some code involving addPop/addPush recently?

        stackAlignment.pop_back();
        if (align.diff != 0) {
            virtualStackPointer += align.diff;
            // code->addDebugText("align sp\n");
            i16 offset = align.diff;
            addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
        }
        virtualStackPointer += size;
    }
    
    _GLOG(log::out << "relsp "<<virtualStackPointer<<"\n";)
}
int GenInfo::saveStackMoment() {
    return virtualStackPointer;
}
void GenInfo::restoreStackMoment(int moment, bool withoutModification) {
    using namespace engone;
    int offset = moment - virtualStackPointer;
    if (offset == 0)
        return;
    if(!withoutModification) {
        int at = moment - virtualStackPointer;
        while (at > 0 && stackAlignment.size() > 0) {
            auto align = stackAlignment.back();
            // log::out << "pop stackalign "<<align.diff<<":"<<align.size<<"\n";
            stackAlignment.pop_back();
            at -= align.size;
            at -= align.diff;
            Assert(at >= 0);
        }
        virtualStackPointer = moment;
    } 
    // else {
    //     _GLOG(log::out << "relsp "<<moment<<"\n";)
    // }
    addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
    if(withoutModification)
        _GLOG(log::out << "relsp (temp) "<<moment<<"\n";)
    else
        _GLOG(log::out << "relsp "<<moment<<"\n";)
}
/* #endregion */
// Will perform cast on float and integers with pop, cast, push
// uses register A
// TODO: handle struct cast?
bool PerformSafeCast(GenInfo &info, TypeId from, TypeId to) {
    if (from == to)
        return true;
    TypeId vp = AST_VOID;
    vp.setPointerLevel(1);
    if(from.isPointer() && (to == vp || to==AST_UINT64 || to==AST_INT64))
        return true;
    if((from == vp||from==AST_UINT64||from==AST_INT64) && to.isPointer())
        return true;
    // TODO: I just threw in currentScopeId. Not sure if it is supposed to be in all cases.
    // auto fti = info.ast->getTypeInfo(from);
    // auto tti = info.ast->getTypeInfo(to);
    auto fti = info.ast->getTypeSize(from);
    auto tti = info.ast->getTypeSize(to);
    // u8 reg0 = RegBySize(BC_AX, fti->size());
    // u8 reg1 = RegBySize(BC_AX, tti->size());
    // IMPORTANT: DO NOT!!!!!! USE ANY OTHER REGISTER THAN A!!!!!!
    u8 reg0 = RegBySize(BC_AX, fti);
    u8 reg1 = RegBySize(BC_AX, tti);
    if (from == AST_FLOAT32 && AST::IsInteger(to)) {
        info.addPop(reg0);
        info.addInstruction({BC_CAST, CAST_FLOAT_SINT, reg0, reg1});
        info.addPush(reg1);
        return true;
    }
    if (AST::IsInteger(from) && to == AST_FLOAT32) {
        info.addPop(reg0);
        info.addInstruction({BC_CAST, CAST_SINT_FLOAT, reg0, reg1});
        info.addPush(reg1);
        return true;
    }
    if ((AST::IsInteger(from) && to == AST_CHAR) ||
        (from == AST_CHAR && AST::IsInteger(to))) {
        // if(fti->size() != tti->size()){
        info.addPop(reg0);
        info.addInstruction({BC_CAST, CAST_SINT_SINT, reg0, reg1});
        info.addPush(reg1);
        // }
        return true;
    }
    if ((AST::IsInteger(from) && to == AST_BOOL) ||
        (from == AST_BOOL && AST::IsInteger(to))) {
        // if(fti->size() != tti->size()){
        info.addPop(reg0);
        info.addInstruction({BC_CAST, CAST_SINT_SINT, reg0, reg1});
        info.addPush(reg1);
        // }
        return true;
    }
    if (AST::IsInteger(from) && AST::IsInteger(to)) {
        info.addPop(reg0);
        if (AST::IsSigned(from) && AST::IsSigned(to))
            info.addInstruction({BC_CAST, CAST_SINT_SINT, reg0, reg1});
        if (AST::IsSigned(from) && !AST::IsSigned(to))
            info.addInstruction({BC_CAST, CAST_SINT_UINT, reg0, reg1});
        if (!AST::IsSigned(from) && AST::IsSigned(to))
            info.addInstruction({BC_CAST, CAST_UINT_SINT, reg0, reg1});
        info.addPush(reg1);
        return true;
    }
    // Asserts when we have missed a conversion
    Assert(!info.ast->castable(from,to));
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
// bool IsSafeCast(TypeId from, TypeId to) {
//     // if(AST::IsInteger(from)&&AST::IsInteger(to))
//     // return true;
//     if (from.isPointer() && to.isPointer())
//         return true;

//     return from == to;

//     // bool fromSigned = AST::IsSigned(from);
//     // bool toSigned = AST::IsSigned(to);

//     // if(fromSigned&&!toSigned)
//     //     return false;

//     // if(fromSigned==toSigned){
//     //     return to>=from;
//     // }
//     // if(!fromSigned&&toSigned){
//     //     return from-AST_UINT8 < to-AST_INT8;
//     // }
//     // Assert(("IsSafeCast not handled case",0));
//     // return false;
// }
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
        u8 reg = RegBySize(BC_AX, size);
        if(offset == 0){
            // If you are here to optimize some instructions then you are out of luck.
            // I checked where GeneratePush is used whether ADDI can LI can be removed and
            // replaced with a MOV_MR_DISP32 but those instructions come from GenerateReference.
            // What you need is a system to optimise away instructions while adding them (like pop after push)
            // or an optimizer which runs after the generator.
            // You need something more sophisticated to optimize further basically.
            info.addInstruction({BC_MOV_MR, baseReg, reg, (u8)size});
        }else{
            info.addInstruction({BC_MOV_MR_DISP32, baseReg, reg, (u8)size});
            info.code->addIm(offset);
        }
        info.addPush(reg);
    } else {
        for(int i = (int) typeInfo->astStruct->members.size() - 1; i>=0; i--){
            auto& member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);

            // TODO: This code doesn't seem to be necessary. It should be though.
            //  Figure out why it isn't. Something with BC_MOV bypassing the need
            //  for this?
            if(i+1<(int)typeInfo->astStruct->members.size()){
                // structs in structs will be aligned by their individual members
                // instead of the alignment of the structs as a whole.
                // This will make sure the structs are aligned.
                auto prevMem = typeInfo->getMember(i+1);
                u32 alignSize = info.ast->getTypeAlignedSize(prevMem.typeId);
                // log::out << "Try align "<<alignSize<<"\n";
                // info.addAlign(alignSize);
            }
            
            _GLOG(log::out << "push " << member.name << "\n";)
            // info.code->addDebugText("push " + std::string(member.name)+"\n");
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
    u8 size = info.ast->getTypeSize(typeId);
    if (!typeInfo || !typeInfo->astStruct) {
        _GLOG(log::out << "move return value\n";)
        u8 reg = RegBySize(BC_AX, size);
        info.addPop(reg);
        if(baseReg!=0){ // baseReg == 0 says: "dont' care about value, just pop it"
            if(offset == 0){
                info.addInstruction({BC_MOV_RM, reg, baseReg, size});
            }else{
                info.addInstruction({BC_MOV_RM_DISP32, reg, baseReg, size});
                info.code->addIm(offset);
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
int GenerateDefaultValue(GenInfo& info, u8 baseReg, int offset, TypeId typeId, TokenRange* tokenRange = 0);
// int FramePush(GenInfo& info, TypeId typeId, i32* outFrameOffset, bool genDefault){
//     // IMPORTANT: This code has been copied to array assignment. Change that code
//     //  when changing code here.
//     i32 size = info.ast->getTypeSize(typeId);
//     i32 asize = info.ast->getTypeAlignedSize(typeId);
//     size = 8;
//     asize = 8;
//     if(asize==0)
//         return GEN_ERROR;
//     int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
//     if (diff != asize) {
//         info.currentFrameOffset -= diff; // align
//         info.addIncrSp(-diff);
//     }
//     info.currentFrameOffset -= size;
//     *outFrameOffset = info.currentFrameOffset;
    
//     info.addIncrSp(-size);

//     if(genDefault){
//         int result = GenerateDefaultValue(info, BC_REG_FP, info.currentFrameOffset, typeId);
//         if(result!=GEN_SUCCESS)
//             return GEN_ERROR;
//     }
//     return GEN_SUCCESS;
// }
int FramePush(GenInfo& info, TypeId typeId, i32* outFrameOffset, bool genDefault){
    // IMPORTANT: This code has been copied to array assignment. Change that code
    //  when changing code here.
    i32 size = info.ast->getTypeSize(typeId);
    i32 asize = info.ast->getTypeAlignedSize(typeId);
    
    int sizeDiff = size % 8;
    if(sizeDiff != 0){
        size += 8 - sizeDiff;
    }
    asize = 8;
    if(asize==0)
        return GEN_ERROR;
    int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
    if (diff != asize) {
        info.currentFrameOffset -= diff; // align
        info.addIncrSp(-diff);
        Assert(false);
    }
    info.currentFrameOffset -= size;
    *outFrameOffset = info.currentFrameOffset;
    
    info.addIncrSp(-size);

    if(genDefault){
        int result = GenerateDefaultValue(info, BC_REG_FP, info.currentFrameOffset, typeId);
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
int GenerateExpression(GenInfo &info, ASTExpression *expression, DynamicArray<TypeId> *outTypeIds, ScopeId idScope = -1);
int GenerateExpression(GenInfo &info, ASTExpression *expression, TypeId *outTypeId, ScopeId idScope = -1){
    DynamicArray<TypeId> types{};
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
    MAKE_NODE_SCOPE(_expression);
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

        if(now->typeId == AST_ID) {
            // end point
            auto id = info.ast->findIdentifier(idScope, now->name);
            if (!id || id->type != Identifier::VAR) {
                if(info.compileInfo->typeErrors==0){
                    ERR_HEAD(now->tokenRange, "'"<<now->tokenRange.firstToken << " is undefined.\n\n";
                        ERR_LINE(now->tokenRange,"bad");
                    )
                }
                return GEN_ERROR;
            }
        
            auto var = info.ast->identifierToVariable(id);
            _GLOG(log::out << " expr var push " << now->name << "\n";)
            // TOKENINFO(now->tokenRange)
            // char buf[100];
            // int len = sprintf(buf,"  expr push %s",now->name->c_str());
            // info.code->addDebugText(buf,len);

            TypeInfo *typeInfo = 0;
            if(var->typeId.isNormalType())
                typeInfo = info.ast->getTypeInfo(var->typeId);
            TypeId typeId = var->typeId;

            info.addLoadIm(BC_REG_RBX, var->frameOffset);
            
            info.addInstruction({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX});
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
                Assert(info.compileInfo->typeErrors!=0);
                // error should have been caught by type checker.
                // if it was then we just return error here.
                // don't want message printed twice.
                // ERR_HEAD(now->tokenRange, "'"<<now->name << "' is not a member of struct '" << info.ast->typeToString(endType) << "'. "
                //         "These are the members: ";
                //     for(int i=0;i<(int)typeInfo->astStruct->members.size();i++){
                //         if(i!=0)
                //             log::out << ", ";
                //         log::out << log::LIME << typeInfo->astStruct->members[i].name<<log::SILVER<<": "<<info.ast->typeToString(typeInfo->getMember(i).typeId);
                //     }
                //     log::out <<"\n";
                //     log::out <<"\n";
                //     ERR_LINE(now->tokenRange, "not a member");
                // )
                return GEN_ERROR;
            }
            // TODO: You can do more optimisations here as long as you don't
            //  need to dereference or use MOV_MR. You can have the RBX popped
            //  and just use ADDI on it to get the correct members offset and keep
            //  doing this for all member accesses. Then you can push when done.
            
            bool popped = false;
            u8 reg = BC_REG_RBX;
            if(endType.getPointerLevel()>0){
                if(!popped)
                    info.addPop(reg);
                popped = true;
                info.addInstruction({BC_MOV_MR, reg, BC_REG_RCX, 8});
                reg = BC_REG_RCX;
            }
            if(memberData.offset!=0){
                if(!popped)
                    info.addPop(reg);
                popped = true;
                info.addLoadIm(BC_REG_EAX, memberData.offset);
                info.addInstruction({BC_ADDI,reg, BC_REG_EAX, reg});
            }
            if(popped)
                info.addPush(reg);

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
                info.addInstruction({BC_MOV_MR,BC_REG_RBX, BC_REG_RCX, 8});
                info.addPush(BC_REG_RCX);
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
                u8 reg = RegBySize(BC_DX,rsize);
                info.addPop(reg); // integer
                info.addPop(BC_REG_RBX); // reference

                info.addLoadIm(BC_REG_EAX, typesize);
                info.addInstruction({BC_MULI, reg, BC_REG_EAX, reg});
                info.addInstruction({BC_ADDI, BC_REG_RBX, reg, BC_REG_RBX});

                info.addPush(BC_REG_RBX);
                continue;
            }

            if(!endType.isPointer()){
                if(info.compileInfo->typeErrors == 0){
                    std::string strtype = info.ast->typeToString(endType);
                    ERR_HEAD(now->right->tokenRange, "Index operator ( array[23] ) requires pointer type in the outer expression. '"<<strtype<<"' is not a pointer.\n\n";
                        ERR_LINE(now->right->tokenRange,strtype.c_str());
                    )
                }
                return GEN_ERROR;
            }

            endType.setPointerLevel(endType.getPointerLevel()-1);

            u32 typesize = info.ast->getTypeSize(endType);
            u32 rsize = info.ast->getTypeSize(rtype);
            u8 reg = RegBySize(BC_DX,rsize);
            info.addPop(reg); // integer
            info.addPop(BC_REG_RBX); // reference
            // dereference pointer to pointer
            info.addInstruction({BC_MOV_MR, BC_REG_RBX, BC_REG_RCX, 8});
            
            info.addLoadIm(BC_REG_EAX, typesize);
            info.addInstruction({BC_MULI, reg, BC_REG_EAX, reg});
            info.addInstruction({BC_ADDI, BC_REG_RCX, reg, BC_REG_RCX});

            info.addPush(BC_REG_RCX);
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
int GenerateExpression(GenInfo &info, ASTExpression *expression, DynamicArray<TypeId> *outTypeIds, ScopeId idScope) {
    using namespace engone;
    MEASURE;
    if(idScope==(ScopeId)-1)
        idScope = info.currentScopeId;
    _GLOG(FUNC_ENTER)
    
    MAKE_NODE_SCOPE(expression);
    Assert(expression);

    // IMPORTANT: This DOES NOT work duudeeee. you must use poly versions
    // TypeId castType = expression->castType;
    // if(castType.isString()){
    //     castType = info.ast->convertToTypeId(castType,idScope);
    //     if(!castType.isValid()){
    //         ERR_HEAD(expression->tokenRange,"Type "<<info.ast->getTokenFromTypeString(expression->castType) << " does not exist.\n";)
    //     }
    // }
    // castType = info.ast->ensureNonVirtualId(castType);

    if (expression->isValue) {
        // data type
        if (AST::IsInteger(expression->typeId)) {
            i64 val = expression->i64Value;

            // TODO: immediate only allows 32 bits. What about larger values?
            // info.code->addDebugText("  expr push int");
            // TOKENINFO(expression->tokenRange)

            // TODO: Int types should come from global scope. Is it a correct assumption?
            // TypeInfo *typeInfo = info.ast->getTypeInfo(expression->typeId);
            u32 size = info.ast->getTypeSize(expression->typeId);
            u8 reg = RegBySize(BC_AX, size);
            info.addLoadIm(reg, val);
            info.addPush(reg);
        }
        else if (expression->typeId == AST_BOOL) {
            bool val = expression->boolValue;

            // TOKENINFO(expression->tokenRange)
            // info.code->addDebugText("  expr push bool");
            info.addLoadIm(BC_REG_AL, val);
            info.addPush(BC_REG_AL);
        }
        else if (expression->typeId == AST_CHAR) {
            char val = expression->charValue;

            // TOKENINFO(expression->tokenRange)
            // info.code->addDebugText("  expr push char");
            info.addLoadIm(BC_REG_AL, val);
            info.addPush(BC_REG_AL);
        }
        else if (expression->typeId == AST_FLOAT32) {
            float val = expression->f32Value;

            // TOKENINFO(expression->tokenRange)
            // info.code->addDebugText("  expr push float");
            info.addLoadIm(BC_REG_EAX, *(u32*)&val);
            info.addPush(BC_REG_EAX);
            
            // outTypeIds->add( expression->typeId);
            // return GEN_SUCCESS;
        }
        else if (expression->typeId == AST_ID) {
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
                outTypeIds->add(typeInfo->id);
                return GEN_SUCCESS;
            }
            // }
            // check data type and get it
            auto id = info.ast->findIdentifier(idScope, expression->name);
            if (id && id->type == Identifier::VAR) {
                auto var = info.ast->identifierToVariable(id);
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

                outTypeIds->add(var->typeId);
                return GEN_SUCCESS;
            } else {
                if(info.compileInfo->typeErrors==0){
                    ERR_HEAD(expression->tokenRange, "'"<<expression->tokenRange.firstToken<<"' is not declared.\n\n";
                        ERR_LINE(expression->tokenRange,"undeclared");
                    )
                }
                return GEN_ERROR;
            }
        }
        else if (expression->typeId == AST_FNCALL) {
            ASTFunction* astFunc = nullptr;
            FuncImpl* funcImpl = nullptr;
            {
                FnOverloads::Overload overload = expression->versions_overload[info.currentPolyVersion];
                astFunc = overload.astFunc;
                funcImpl = overload.funcImpl;
            }
            if(info.compileInfo->typeErrors==0) {
                // not ok, type checker should have generated the right overload.
                Assert(astFunc && funcImpl);
            }
            if(!astFunc || !funcImpl) {
                return GEN_ERROR;
            }

            // overload comes from type checker
            _GLOG(log::out << "Overload: ";funcImpl->print(info.ast,nullptr);log::out << "\n";)

            std::vector<ASTExpression*> fullArgs;
            fullArgs.resize(astFunc->arguments.size(),nullptr);
            for (int i = 0; i < (int)expression->args->size();i++) {
                ASTExpression* arg = expression->args->get(i);
                Assert(arg);
                if(!arg->namedValue.str){
                    fullArgs[i] = arg;
                }else{
                    int argIndex = -1;
                    for(int j=0;j<(int)astFunc->arguments.size();j++){
                        if(astFunc->arguments[j].name==arg->namedValue){
                            argIndex=j;
                            break;
                        }
                    }
                    if(argIndex!=-1) {
                        if (fullArgs[argIndex] != nullptr) {
                            ERR_HEAD(arg->namedValue.range(), "A named argument cannot specify an occupied parameter.\n\n";
                                ERR_LINE(astFunc->tokenRange,"this overload");
                                ERR_LINE(fullArgs[argIndex]->tokenRange,"occupied");
                                ERR_LINE(arg->namedValue,"bad coder");
                            )
                        } else {
                            fullArgs[argIndex] = arg;
                        }
                    } else{
                        ERR_HEAD(arg->tokenRange, "Named argument is not a parameter of '";funcImpl->print(info.ast,astFunc);log::out<<"'\n\n";
                            ERR_LINE(arg->namedValue,"not valid");
                        )
                        // continue like nothing happened
                    }
                }
            }
            for (int i = 0; i < (int)astFunc->arguments.size(); i++) {
                auto &arg = astFunc->arguments[i];
                if (!fullArgs[i])
                    fullArgs[i] = arg.defaultValue;
            }
            // I don't think there is anything to fix in there because conventions
            // are really about assembly instructions which type checker has nothing to do with.
            // log::out << log::YELLOW << "Fix call conventions in type checker\n";
            int startSP = info.saveStackMoment();
            switch(astFunc->callConvention){
                case BETCALL: {
                    Assert(0 == (-info.virtualStackPointer) % 8); // should be aligned

                    info.addIncrSp(-funcImpl->argSize);
                }
                break; case STDCALL: {
                    for(int i=0;i<astFunc->arguments.size();i++){
                        int size = info.ast->getTypeSize(funcImpl->argumentTypes[i].typeId);
                        if(size>8){
                            // TODO: This should be moved to the type checker.
                            ERR_HEAD(expression->tokenRange, "Argument types cannot be larger than 8 bytes.";
                                ERR_LINE(expression->tokenRange, "bad");
                            )
                            return GEN_ERROR;
                        }
                    }

                    int alignment = 16;
                    int stackSpace = astFunc->arguments.size() * 8;
                    if(stackSpace<32)
                        stackSpace = 32;
                    stackSpace += MISALIGNMENT(-info.virtualStackPointer + stackSpace,alignment);
                    info.addIncrSp(-stackSpace);
                    // int misalign = MISALIGNMENT(-info.virtualStackPointer + stackSpace,alignment);
                    // info.addIncrSp(-misalign);
                }
                break; case CDECL_CONVENTION: {
                    Assert(false); // @Incomplete
                }
                break; case INTRINSIC: {
                    // do nothing
                }
            }

            int virtualSP = info.virtualStackPointer;
            for(int i=0;i<(int)fullArgs.size();i++){
                auto arg = fullArgs[i];
                
                TypeId argType = {};
                int result = 0;
                if(expression->boolValue && i == 0) {
                    // method call and first argument which is 'this'
                    result = GenerateReference(info, arg, &argType);
                    if(result==GEN_SUCCESS){
                        if(!argType.isPointer()){
                            argType.setPointerLevel(1);
                        } else {
                            Assert(argType.getPointerLevel()==1);
                            info.addPop(BC_REG_RBX);
                            info.addInstruction({BC_MOV_MR, BC_REG_RBX, BC_REG_RBX, 8});
                            info.addPush(BC_REG_RBX);
                        }
                    }
                } else {
                    result = GenerateExpression(info, arg, &argType);
                    // log::out << "PUSH ARG "<<info.ast->typeToString(argType)<<"\n";
                    bool wasSafelyCasted = PerformSafeCast(info,argType, funcImpl->argumentTypes[i].typeId);
                    Assert(wasSafelyCasted);
                }
            }

            switch (astFunc->callConvention) {
                case INTRINSIC: {
                    // TODO: You could do some special optimisations when using intrinsics.
                    //  If the arguments are strictly variables or constants then you can use a mov instruction 
                    //  instead messing with push and pop.
                    if(funcImpl->name == "memcpy"){
                        info.addPop(BC_REG_RBX);
                        info.addPop(BC_REG_RSI);
                        info.addPop(BC_REG_RDI);
                        info.addInstruction({BC_MEMCPY, BC_REG_RDI, BC_REG_RSI, BC_REG_RBX});
                    } else if(funcImpl->name == "memzero"){
                        info.addPop(BC_REG_RBX);
                        info.addPop(BC_REG_RDI);
                        info.addInstruction({BC_MEMZERO, BC_REG_RDI, BC_REG_RBX});
                    } 
                    // else if(funcImpl->name == "sin"){
                    //     info.addPop(BC_REG_EAX);
                    //     info.addInstruction({BC_SIN, BC_REG_EAX});
                    //     info.addPush(BC_REG_EAX);
                    //     outTypeIds->add(AST_FLOAT32);
                    // } else if(funcImpl->name == "cos"){
                    //     info.addPop(BC_REG_EAX);
                    //     info.addInstruction({BC_COS, BC_REG_EAX});
                    //     info.addPush(BC_REG_EAX);
                    //     outTypeIds->add(AST_FLOAT32);
                    // } else if(funcImpl->name == "tan"){
                    //     info.addPop(BC_REG_EAX);
                    //     info.addInstruction({BC_TAN, BC_REG_EAX});
                    //     info.addPush(BC_REG_EAX);
                    //     outTypeIds->add(AST_FLOAT32);
                    // }
                     else {
                        ERR_HEAD(expression->tokenRange, "'"<<funcImpl->name<<"' is not an intrinsic function\n";
                            ERR_LINE(expression->tokenRange,"not an intrinsic");
                        )
                    }
                    return GEN_SUCCESS;
                }
                break; case BETCALL: {
                    // I have not implemented linkConvention because it's rare
                    // that you need it for BETCALL.
                    Assert(astFunc->linkConvention == LinkConventions::NONE || astFunc->linkConvention == NATIVE);
                    if(fullArgs.size() != 0){
                        int baseOffset = virtualSP - info.virtualStackPointer;
                        // info.addLoadIm(BC_REG_RBX, virtualSP - info.virtualStackPointer);
                        info.addInstruction({BC_MOV_RR, BC_REG_SP, BC_REG_RBX});
                        for(int i=fullArgs.size()-1;i>=0;i--){
                            auto arg = fullArgs[i];
                            
                            // log::out << "POP ARG "<<info.ast->typeToString(funcImpl->argumentTypes[i].typeId)<<"\n";
                            GeneratePop(info,BC_REG_RBX, baseOffset + funcImpl->argumentTypes[i].offset, funcImpl->argumentTypes[i].typeId);
                        }
                    }

                    info.addCall(astFunc->linkConvention, astFunc->callConvention);
                    info.addCallToResolve(info.code->length(),funcImpl);
                    info.code->addIm(999999999);

                    if (funcImpl->returnTypes.size()==0) {
                        _GLOG(log::out << "pop arguments\n");
                        info.restoreStackMoment(startSP);
                        outTypeIds->add(AST_VOID);
                    } else {
                        _GLOG(log::out << "extract return values\n";)
                        // NOTE: info.currentScopeId MAY NOT WORK FOR THE TYPES IF YOU PASS FUNCTION POINTERS!
                        
                        // Hello! This stuff is very complicated and you can make many mistakes!
                        // I removed the BC_MOV_RR because I thought "Huh, this doesn't need to be here. I guess it was left behind when I changed some stuff."
                        // Don't do that you dummy!

                        // These two lines combined tell us where to find the return values on the stack.
                        // sp at this moment in time is at the end of current call frame
                        // sp + frame size would be the start of the call frame we had 
                        // this is were the return values can be found
                        int offset = -GenInfo::FRAME_SIZE;
                        info.addInstruction({BC_MOV_RR,BC_REG_SP,BC_REG_RBX});
                        // sp is our correct value right here. DO NOT rearrange this instruction because
                        // sp is very sensitive. You should rarely use stack pointer because of this but here we have to.
                        
                        // we restore AFTER we got the stack pointer because it resets alignments and arg offset
                        // which we don't want to include when adding our offsets. Once again, it's complicated.
                        info.restoreStackMoment(startSP); // PLEASE FOR GOODNESS SAKE DO NOT REFACTOR THIS RESTORE ELSE WHERE!
                        
                        for (int i = 0;i<(int)funcImpl->returnTypes.size(); i++) {
                            auto &ret = funcImpl->returnTypes[i];
                            TypeId typeId = ret.typeId;

                            GeneratePush(info, BC_REG_RBX, offset + ret.offset, typeId);
                            outTypeIds->add(ret.typeId);
                        }
                    }
                }
                break; case STDCALL: {
                    if(fullArgs.size() > 4) {
                        Assert(virtualSP - info.virtualStackPointer == fullArgs.size()*8);
                        int argOffset = fullArgs.size()*8;
                        info.addInstruction({BC_MOV_RR, BC_REG_SP, BC_REG_RBX});
                        for(int i=fullArgs.size()-1;i>=4;i--){
                            auto arg = fullArgs[i];
                            
                            // log::out << "POP ARG "<<info.ast->typeToString(funcImpl->argumentTypes[i].typeId)<<"\n";
                            // NOTE: funcImpl->argumentTypes[i].offset SHOULD NOT be used 8*i is correct
                            u32 size = info.ast->getTypeSize(funcImpl->argumentTypes[i].typeId);
                            GeneratePop(info,BC_REG_RBX, argOffset + i*8, funcImpl->argumentTypes[i].typeId);
                        }
                    }
                    auto& argTypes = funcImpl->argumentTypes;
                    if(fullArgs.size() > 3){
                        if(argTypes[3].typeId == AST_FLOAT32)
                            info.addPop(BC_REG_XMM3);
                        else
                            info.addPop(BC_REG_R9);
                    }
                    if(fullArgs.size() > 2){
                        if(argTypes[2].typeId == AST_FLOAT32)
                            info.addPop(BC_REG_XMM2);
                        else
                            info.addPop(BC_REG_R8);
                    }
                    if(fullArgs.size() > 1){
                        if(argTypes[1].typeId == AST_FLOAT32)
                            info.addPop(BC_REG_XMM1);
                        else
                            info.addPop(BC_REG_RDX);
                    }
                    if(fullArgs.size() > 0){
                        if(argTypes[0].typeId == AST_FLOAT32)
                            info.addPop(BC_REG_XMM0);
                        else
                            info.addPop(BC_REG_RCX);
                    }
                    // native function can be handled normally
                    info.addCall(astFunc->linkConvention, astFunc->callConvention);
                    if(astFunc->linkConvention == LinkConventions::IMPORT || astFunc->linkConvention == LinkConventions::DLLIMPORT){
                        if(astFunc->linkConvention == DLLIMPORT){
                            info.code->addExternalRelocation("__imp_"+funcImpl->name, info.code->length());
                        } else
                            info.code->addExternalRelocation(funcImpl->name, info.code->length());
                        info.code->addIm(info.code->externalRelocations.size()-1);
                    } else {
                        // linkin == native, none or export should be fine
                        info.addCallToResolve(info.code->length(), funcImpl);
                        info.code->addIm(999999);
                    }

                    Assert(funcImpl->returnTypes.size() < 2); // stdcall can only have on return value

                    if (funcImpl->returnTypes.size()==0) {
                        info.restoreStackMoment(startSP);
                        outTypeIds->add(AST_VOID);
                    } else {
                        // return type must fit in RAX
                        Assert(info.ast->getTypeSize(funcImpl->returnTypes[0].typeId) <= 8);
                        
                        info.restoreStackMoment(startSP);
                        
                        auto &ret = funcImpl->returnTypes[0];
                        if(ret.typeId == AST_FLOAT32) {
                            info.addPush(BC_REG_XMM0);
                        } else {
                            info.addPush(BC_REG_RAX);
                        }
                        outTypeIds->add(ret.typeId);
                    }
                }
            }
            return GEN_SUCCESS;
        } else if(expression->typeId==AST_STRING){
            // Assert(expression->constStrIndex!=-1);
            int& constIndex = expression->versions_constStrIndex[info.currentPolyVersion];
            auto& constString = info.ast->getConstString(constIndex);

            TypeInfo* typeInfo = info.ast->convertToTypeInfo(Token("Slice<char>"), info.ast->globalScopeId);
            if(!typeInfo){
                ERR_HEAD(expression->tokenRange, expression->tokenRange<<" cannot be converted to Slice<char> because Slice doesn't exist. Did you forget #import \"Basic\"\n";
                )
                return GEN_ERROR;
            }
            Assert(typeInfo->astStruct);
            Assert(typeInfo->astStruct->members.size() == 2);
            Assert(typeInfo->structImpl->members[0].offset == 0);
            // Assert(typeInfo->structImpl->members[1].offset == 8);// len: u64
            // Assert(typeInfo->structImpl->members[1].offset == 12); // len: u32

            // last member in slice is pushed first
            if(typeInfo->structImpl->members[1].offset == 8){
                info.addLoadIm(BC_REG_RAX,constString.length);
                info.addPush(BC_REG_RAX);
            } else {
                info.addLoadIm(BC_REG_RAX,constString.length);
                info.addPush(BC_REG_EAX);
            }

            // first member is pushed last
            info.addInstruction({BC_DATAPTR, BC_REG_RBX});
            info.code->addIm(constString.address);
            info.addPush(BC_REG_RBX);

            outTypeIds->add(typeInfo->id);
            return GEN_SUCCESS;
        } else if(expression->typeId == AST_SIZEOF){
            // TOKENINFO(expression->tokenRange)

            // TypeId typeId = info.ast->convertToTypeId(expression->name, idScope);
            // Assert(typeId.isValid()); // Did type checker fix this? Maybe not on errors?

            TypeId typeId = expression->versions_outTypeSizeof[info.currentPolyVersion];
            u32 size = info.ast->getTypeSize(typeId);

            info.addLoadIm(BC_REG_EAX, size);
            info.addPush(BC_REG_EAX);

            outTypeIds->add(AST_UINT32);
            return GEN_SUCCESS;
        } else if(expression->typeId == AST_NAMEOF){
            // TypeId typeId = info.ast->convertToTypeId(expression->name, idScope);
            // Assert(typeId.isValid());

            // std::string name = info.ast->typeToString(typeId);

            // Assert(expression->constStrIndex!=-1);
            int constIndex = expression->versions_constStrIndex[info.currentPolyVersion];
            auto& constString = info.ast->getConstString(constIndex);

            TypeInfo* typeInfo = info.ast->convertToTypeInfo(Token("Slice<char>"), info.ast->globalScopeId);
            if(!typeInfo){
                ERR_HEAD(expression->tokenRange, expression->tokenRange<<" cannot be converted to Slice<char> because Slice doesn't exist. Did you forget #import \"Basic\"\n";
                )
                return GEN_ERROR;
            }
            Assert(typeInfo->astStruct);
            Assert(typeInfo->astStruct->members.size() == 2);
            Assert(typeInfo->structImpl->members[0].offset == 0);
            // Assert(typeInfo->structImpl->members[1].offset == 8);// len: u64
            // Assert(typeInfo->structImpl->members[1].offset == 12); // len: u32

            // last member in slice is pushed first
            if(typeInfo->structImpl->members[1].offset == 8){
                info.addLoadIm(BC_REG_RAX, constString.length);
                info.addPush(BC_REG_RAX);
            } else {
                info.addLoadIm(BC_REG_EAX, constString.length);
                info.addPush(BC_REG_EAX);
            }

            // first member is pushed last
            info.addInstruction({BC_DATAPTR, BC_REG_RBX});
            info.code->addIm(constString.address);
            info.addPush(BC_REG_RBX);

            outTypeIds->add(typeInfo->id);
            return GEN_SUCCESS;
        } else if (expression->typeId == AST_NULL) {
            // TODO: Move into the type checker?
            // info.code->addDebugText("  expr push null");
            TOKENINFO(expression->tokenRange)
            info.addLoadIm(BC_REG_RAX, 0);
            info.addPush(BC_REG_RAX);

            TypeInfo *typeInfo = info.ast->convertToTypeInfo(Token("void"), info.ast->globalScopeId);
            TypeId newId = typeInfo->id;
            newId.setPointerLevel(1);
            outTypeIds->add( newId);
            return GEN_SUCCESS;
        } else {
            std::string typeName = info.ast->typeToString(expression->typeId);
            // info.addInstruction({BC_PUSH,BC_REG_RAX}); // push something so the stack stays synchronized, or maybe not?
            ERR_HEAD(expression->tokenRange, "'" <<typeName << "' is an unknown data type.\n\n";
                ERR_LINE(expression->tokenRange,typeName.c_str());
            )
            // log::out <<  log::RED<<"GenExpr: data type not implemented\n";
            outTypeIds->add( AST_VOID);
            return GEN_ERROR;
        }
        outTypeIds->add(expression->typeId);
    } else {
        FuncImpl* operatorImpl = nullptr;
        if(expression->versions_overload._array.size()>0)
            operatorImpl = expression->versions_overload[info.currentPolyVersion].funcImpl;
        TypeId ltype = AST_VOID;
        if(operatorImpl){
            ASTFunction* astFunc = nullptr;
            FuncImpl* funcImpl = nullptr;
            {
                FnOverloads::Overload overload = expression->versions_overload[info.currentPolyVersion];
                astFunc = overload.astFunc;
                funcImpl = overload.funcImpl;
            }
            Assert(astFunc && funcImpl);

            // overload comes from type checker
            _GLOG(log::out << "Operator overload: ";funcImpl->print(info.ast,nullptr);log::out << "\n";)

            std::vector<ASTExpression*> fullArgs;
            fullArgs.push_back(expression->left);
            fullArgs.push_back(expression->right);

            // I don't think there is anything to fix in there because conventions
            // are really about assembly instructions which type checker has nothing to do with.
            // log::out << log::YELLOW << "Fix call conventions in type checker\n";
            int startSP = info.saveStackMoment();
            switch(astFunc->callConvention){
                case BETCALL: {
                    Assert(0 == (-info.virtualStackPointer) % 8); // should be aligned

                    info.addIncrSp(-funcImpl->argSize);
                }
                break; case STDCALL: {
                    for(int i=0;i<astFunc->arguments.size();i++){
                        int size = info.ast->getTypeSize(funcImpl->argumentTypes[i].typeId);
                        if(size>8){
                            // TODO: This should be moved to the type checker.
                            ERR_HEAD(expression->tokenRange, "Argument types cannot be larger than 8 bytes.";
                                ERR_LINE(expression->tokenRange, "bad");
                            )
                            return GEN_ERROR;
                        }
                    }

                    int alignment = 16;
                    int stackSpace = astFunc->arguments.size() * 8;
                    if(stackSpace<32)
                        stackSpace = 32;
                    stackSpace += MISALIGNMENT(-info.virtualStackPointer + stackSpace,alignment);
                    info.addIncrSp(-stackSpace);
                    // int misalign = MISALIGNMENT(-info.virtualStackPointer + stackSpace,alignment);
                    // info.addIncrSp(-misalign);
                }
                break; case CDECL_CONVENTION: {
                    Assert(false); // @Incomplete
                }
            }

            int virtualSP = info.virtualStackPointer;
            for(int i=0;i<(int)fullArgs.size();i++){
                auto arg = fullArgs[i];
                
                TypeId argType = {};
                int result = 0;
                result = GenerateExpression(info, arg, &argType);
                // log::out << "PUSH ARG "<<info.ast->typeToString(argType)<<"\n";
                bool wasSafelyCasted = PerformSafeCast(info,argType, funcImpl->argumentTypes[i].typeId);
                Assert(wasSafelyCasted);
            }

            switch (astFunc->callConvention) {
                break; case BETCALL: {
                    // I have not implemented linkConvention because it's rare
                    // that you need it for BETCALL.
                    if(fullArgs.size() != 0){
                        int baseOffset = virtualSP - info.virtualStackPointer;
                        // info.addLoadIm(BC_REG_RBX, virtualSP - info.virtualStackPointer);
                        info.addInstruction({BC_MOV_RR, BC_REG_SP, BC_REG_RBX});
                        for(int i=fullArgs.size()-1;i>=0;i--){
                            auto arg = fullArgs[i];
                            
                            // log::out << "POP ARG "<<info.ast->typeToString(funcImpl->argumentTypes[i].typeId)<<"\n";
                            GeneratePop(info,BC_REG_RBX, baseOffset + funcImpl->argumentTypes[i].offset, funcImpl->argumentTypes[i].typeId);
                        }
                    }

                    info.addCall(astFunc->linkConvention, astFunc->callConvention);
                    info.addCallToResolve(info.code->length(),funcImpl);
                    info.code->addIm(999999999);

                    if (funcImpl->returnTypes.size()==0) {
                        _GLOG(log::out << "pop arguments\n");
                        info.restoreStackMoment(startSP);
                        outTypeIds->add(AST_VOID);
                    } else {
                        _GLOG(log::out << "extract return values\n";)
                        // NOTE: info.currentScopeId MAY NOT WORK FOR THE TYPES IF YOU PASS FUNCTION POINTERS!
                        
                        // Hello! This stuff is very complicated and you can make many mistakes!
                        // I removed the BC_MOV_RR because I thought "Huh, this doesn't need to be here. I guess it was left behind when I changed some stuff."
                        // Don't do that you dummy!

                        // These two lines combined tell us where to find the return values on the stack.
                        // sp at this moment in time is at the end of current call frame
                        // sp + frame size would be the start of the call frame we had 
                        // this is were the return values can be found
                        int offset = -GenInfo::FRAME_SIZE;
                        info.addInstruction({BC_MOV_RR,BC_REG_SP,BC_REG_RBX});
                        // sp is our correct value right here. DO NOT rearrange this instruction because
                        // sp is very sensitive. You should rarely use stack pointer because of this but here we have to.
                        
                        // we restore AFTER we got the stack pointer because it resets alignments and arg offset
                        // which we don't want to include when adding our offsets. Once again, it's complicated.
                        info.restoreStackMoment(startSP); // PLEASE FOR GOODNESS SAKE DO NOT REFACTOR THIS RESTORE ELSE WHERE!
                        
                        for (int i = 0;i<(int)funcImpl->returnTypes.size(); i++) {
                            auto &ret = funcImpl->returnTypes[i];
                            TypeId typeId = ret.typeId;

                            GeneratePush(info, BC_REG_RBX, offset + ret.offset, typeId);
                            outTypeIds->add(ret.typeId);
                        }
                    }
                }
                // break; case STDCALL: {
                //     if(fullArgs.size() > 4) {
                //         Assert(virtualSP - info.virtualStackPointer == fullArgs.size()*8);
                //         int argOffset = fullArgs.size()*8;
                //         info.addInstruction({BC_MOV_RR, BC_REG_SP, BC_REG_RBX});
                //         for(int i=fullArgs.size()-1;i>=4;i--){
                //             auto arg = fullArgs[i];
                            
                //             // log::out << "POP ARG "<<info.ast->typeToString(funcImpl->argumentTypes[i].typeId)<<"\n";
                //             // NOTE: funcImpl->argumentTypes[i].offset SHOULD NOT be used 8*i is correct
                //             u32 size = info.ast->getTypeSize(funcImpl->argumentTypes[i].typeId);
                //             GeneratePop(info,BC_REG_RBX, argOffset + i*8, funcImpl->argumentTypes[i].typeId);
                //         }
                //     }
                //     if(fullArgs.size() > 3){
                //         info.addPop(BC_REG_R9);
                //     }
                //     if(fullArgs.size() > 2){
                //         info.addPop(BC_REG_R8);
                //     }
                //     if(fullArgs.size() > 1){
                //         info.addPop(BC_REG_RDX);
                //     }
                //     if(fullArgs.size() > 0){
                //         info.addPop(BC_REG_RCX);
                //     }
                //     // native function can be handled normally
                //     info.addCall(astFunc->linkConvention, astFunc->callConvention);
                //     if(astFunc->linkConvention == LinkConventions::IMPORT || astFunc->linkConvention == LinkConventions::DLLIMPORT){
                //         if(astFunc->linkConvention == DLLIMPORT){
                //             info.code->addExternalRelocation("__imp_"+funcImpl->name, info.code->length());
                //         } else
                //             info.code->addExternalRelocation(funcImpl->name, info.code->length());
                //         info.code->addIm(info.code->externalRelocations.size()-1);
                //     } else {
                //         // linkin == native, none or export should be fine
                //         info.addCallToResolve(info.code->length(), funcImpl);
                //         info.code->addIm(999999);
                //     }

                //     Assert(funcImpl->returnTypes.size() < 2); // stdcall can only have on return value

                //     if (funcImpl->returnTypes.size()==0) {
                //         info.restoreStackMoment(startSP);
                //         outTypeIds->add(AST_VOID);
                //     } else {
                //         // return type must fit in RAX
                //         Assert(info.ast->getTypeSize(funcImpl->returnTypes[0].typeId) <= 8);
                        
                //         info.restoreStackMoment(startSP);
                        
                //         auto &ret = funcImpl->returnTypes[0];
                //         info.addPush(BC_REG_RAX);
                //         outTypeIds->add(ret.typeId);
                //     }
                // }
            }
            return GEN_SUCCESS;
        } else if (expression->typeId == AST_REFER) {

            int result = GenerateReference(info,expression->left,&ltype,idScope);
            if(result!=GEN_SUCCESS)
                return GEN_ERROR;
            if(ltype.getPointerLevel()==3){
                ERR_HEAD(expression->tokenRange,"Cannot take a reference of a triple pointer (compiler has a limit of 0-3 depth of pointing). Cast to u64 if you need triple pointers.";)
                return GEN_ERROR;
            }
            ltype.setPointerLevel(ltype.getPointerLevel()+1);
            outTypeIds->add( ltype); 
        } else if (expression->typeId == AST_DEREF) {
            int err = GenerateExpression(info, expression->left, &ltype);
            if (err == GEN_ERROR)
                return err;

            if (!ltype.isPointer()) {
                ERR_HEAD(expression->left->tokenRange, "Cannot dereference " << info.ast->typeToString(ltype) << ".\n\n";
                    ERR_LINE(expression->left->tokenRange, "bad");
                )
                outTypeIds->add( AST_VOID);
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
                u8 size = info.ast->getTypeSize(outId);

                u8 reg = RegBySize(BC_AX, size);

                info.addInstruction({BC_MOV_MR, BC_REG_RBX, reg, size});
                info.addPush(reg);
            } else {
                GeneratePush(info, BC_REG_RBX, 0, outId);
            }
            outTypeIds->add( outId);
        }
        else if (expression->typeId == AST_NOT) {
            int err = GenerateExpression(info, expression->left, &ltype);
            if (err == GEN_ERROR)
                return err;
            // TypeInfo *ti = info.ast->getTypeInfo(ltype);
            u32 size = info.ast->getTypeSize(ltype);
            // using two registers instead of one because
            // it is easier to implement in x64 generator 
            // or not actually.
            u8 reg = RegBySize(BC_AX, size);
            // u8 reg2 = RegBySize(BC_BX, size);

            info.addPop(reg);
            info.addInstruction({BC_NOT, reg, reg});
            info.addPush(reg);
            // info.addInstruction({BC_NOT, reg, reg2});
            // info.addPush(reg2);

            outTypeIds->add( ltype);
        }
        else if (expression->typeId == AST_BNOT) {
            int err = GenerateExpression(info, expression->left, &ltype);
            if (err == GEN_ERROR)
                return err;
            // TypeInfo *ti = info.ast->getTypeInfo(ltype);
            u32 size = info.ast->getTypeSize(ltype);
            u8 reg = RegBySize(BC_AX, size);

            info.addPop(reg);
            info.addInstruction({BC_BNOT, reg, reg});
            info.addPush(reg);

            outTypeIds->add( ltype);
        }
        else if (expression->typeId == AST_CAST) {
            int err = GenerateExpression(info, expression->left, &ltype);
            if (err == GEN_ERROR)
                return err;

            // TypeInfo *ti = info.ast->getTypeInfo(ltype);
            // TypeInfo *tic = info.ast->getTypeInfo(castType);
            u32 ti = info.ast->getTypeSize(ltype);
            TypeId castType = expression->versions_castType[info.currentPolyVersion];
            u32 tic = info.ast->getTypeSize(castType);
            u8 lreg = RegBySize(BC_AX, ti);
            u8 creg = RegBySize(BC_AX, tic);
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
                    outTypeIds->add( ltype); // ltype since cast failed
                    return GEN_ERROR;
                }
            }
            // } else if(AST::IsInteger(castType)&&ltype==AST_FLOAT32){

            //     info.addPop(lreg);
            //     info.addInstruction({BC_CASTI64,lreg,creg});
            //     info.addPush(creg);
            // } else if(castType == AST_FLOAT32 && AST::IsInteger(ltype)){
            //     info.addPop(lreg);
            //     info.addInstruction({BC_CASTF32,lreg,creg});
            //     info.addPush(creg);
            // } else {
            //     ERR_HEAD2(expression->tokenRange) << "cannot cast "<<info.ast->typeToString(ltype) << " to "<<info.ast->typeToString(castType)<<"\n";
            //     outTypeIds->add( ltype; // ltype since cast failed
            //     return GEN_ERROR;
            // }

            //  float f = 7.29;
            // u32 raw = *(u32*)&f;
            // log::out << " "<<(void*)raw<<"\n";

            // u32 sign = raw>>31;
            // u32 expo = ((raw&0x80000000)>>(23));
            // u32 mant = ((raw&&0x80000000)>>(23));
            // log::out << sign << " "<<expo << " "<<mant<<"\n";

            outTypeIds->add( castType); // NOTE: we use castType and not ltype
        }
        else if (expression->typeId == AST_FROM_NAMESPACE) {
            // info.code->addDebugText("ast-namespaced expr\n");

            auto si = info.ast->getScope(expression->name, info.currentScopeId);
            TypeId exprId;
            int result = GenerateExpression(info, expression->left, &exprId, si->id);
            if (result != GEN_SUCCESS)
                return result;

            outTypeIds->add( exprId);
            // info.currentScopeId = savedScope;
        }
        else if (expression->typeId == AST_MEMBER) {
            // TODO: if propfrom is a variable we can directly take the member from it
            //  instead of pushing struct to the stack and then getting it.

            // info.code->addDebugText("ast-member expr\n");
            TypeId exprId;

            if(expression->left->typeId == AST_ID){
                TypeInfo *typeInfo = info.ast->convertToTypeInfo(expression->left->name, idScope);
                // A simple check to see if the identifier in the expr node is an enum type.
                // no need to check for pointers or so.
                if (typeInfo && typeInfo->astEnum) {
                    i32 enumValue;
                    bool found = typeInfo->astEnum->getMember(expression->name, &enumValue);
                    if (!found) {
                        Assert(info.compileInfo->typeErrors!=0);
                        // ERR_HEAD(expression->tokenRange, expression->tokenRange.firstToken << " is not a member of enum " << typeInfo->astEnum->name << "\n";
                        // )
                        return GEN_ERROR;
                    }

                    info.addLoadIm(BC_REG_EAX, enumValue);
                    info.addPush(BC_REG_EAX);

                    outTypeIds->add(typeInfo->id);
                    return GEN_SUCCESS;
                }
            }

            int result = GenerateReference(info,expression,&exprId);
            if(result != GEN_SUCCESS) {
                return GEN_ERROR;
            }

            info.addPop(BC_REG_RBX);
            result = GeneratePush(info,BC_REG_RBX,0, exprId);
            
            outTypeIds->add(exprId);
        }
        else if (expression->typeId == AST_INITIALIZER) {
            TypeId castType = expression->versions_castType[info.currentPolyVersion];
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

            Assert(expression->args);
            for (int index = 0; index < (int)expression->args->size(); index++) {
                ASTExpression *expr = expression->args->get(index);

                if (!expr->namedValue.str) {
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
                        if (astruct->members[i].name == expr->namedValue) {
                            memIndex = i;
                            break;
                        }
                    }
                    if (memIndex == -1) {
                        ERR_HEAD2(expr->tokenRange) << expr->namedValue << " is not an member in " << astruct->name << "\n";
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
                    int result = GenerateDefaultValue(info, 0, 0, exprId, nullptr);
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

            outTypeIds->add( structInfo->id);
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

            // outTypeIds->add( structInfo->id;
        }
        else if (expression->typeId == AST_FROM_NAMESPACE) {
            // TODO: chould not work on enums? Enum.One is used instead
            // TypeInfo* typeInfo = info.ast->getTypeInfo(*expression->name);
            // if(!typeInfo||!typeInfo->astEnum){
            //     ERR_HEAD2(expression->tokenRange) << "cannot access "<<(expression->member?*expression->member:"?")<<" from non-enum "<<*expression->name<<"\n";
            //     return GEN_ERROR;
            // }
            // Assert(expression->member);
            // int index=-1;
            // for(int i=0;i<(int)typeInfo->astEnum->members.size();i++){
            //     if(typeInfo->astEnum->members[i].name == *expression->member) {
            //         index = i;
            //         break;
            //     }
            // }
            // if(index!=-1){
            //     info.addInstruction({BC_LI,BC_REG_EAX});
            //     info.code->addIm(typeInfo->astEnum->members[index].enumValue);
            //     info.addPush(BC_REG_EAX);

            //     outTypeIds->add( AST_INT32;
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
            Assert(typeInfo->structImpl->members[0].typeId == typeInfo->structImpl->members[1].typeId);

            TypeId inttype = typeInfo->structImpl->members[0].typeId;

            TypeId ltype={};
            int result = GenerateExpression(info, expression->left,&ltype,idScope);
            if(result!=GEN_SUCCESS)
                return GEN_ERROR;
            TypeId rtype={};
            result = GenerateExpression(info, expression->right,&rtype,idScope);
            if(result!=GEN_SUCCESS)
                return GEN_ERROR;
            
            int lreg = RegBySize(BC_CX, info.ast->getTypeSize(ltype));
            int rreg = RegBySize(BC_DX, info.ast->getTypeSize(rtype));
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

            outTypeIds->add(typeInfo->id);
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
            u8 reg = RegBySize(BC_DX,rsize);
            info.addPop(reg); // integer
            info.addPop(BC_REG_RBX); // reference
            info.addLoadIm(BC_REG_EAX, lsize);
            info.addInstruction({BC_MULI, reg, BC_REG_EAX, reg});
            info.addInstruction({BC_ADDI, BC_REG_RBX, reg, BC_REG_RBX});

            int result = GeneratePush(info, BC_REG_RBX, 0, ltype);

            outTypeIds->add(ltype);
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

            u8 size = info.ast->getTypeSize(ltype);
            u8 reg = RegBySize(BC_AX, size);

            info.addPop(BC_REG_RBX); // reference

            info.addInstruction({BC_MOV_MR, BC_REG_RBX, reg, size});
            // post
            if(expression->boolValue)
                info.addPush(reg);
            if(expression->typeId == AST_INCREMENT)
                info.addInstruction({BC_INCR, reg, 1, 0});
            else
                info.addInstruction({BC_INCR, reg, (u8)-1, (u8)-1});
            info.addInstruction({BC_MOV_RM, reg, BC_REG_RBX, size});
            // pre
            if(!expression->boolValue)
                info.addPush(reg);
                
            outTypeIds->add(ltype);
        }
        else {
            // const char* str = OpToStr((OperationType)expr->typeId.getId(), true);
            // if(str) {
            //     int yes = CheckFncall(info,scopeId,expr, outTypes, true);
                
            //     if(yes)
            //         return true;
            // }
            if(expression->typeId == AST_ASSIGN && expression->assignOpType == (OperationType)0){
                // THIS IS PURELY ASSIGN NOT +=, *=
                // WARN_HEAD(expression->tokenRange,"Expression is generated first and then reference. Right to left instead of left to right. "
                // "This is important if you use assignments/increments/decrements on the same memory/reference/variable.\n\n";
                //     WARN_LINE(expression->right->tokenRange,"generated first");
                //     WARN_LINE(expression->left->tokenRange,"then this");
                // )
                TypeId rtype;
                int result = GenerateExpression(info, expression->right, &rtype, idScope);
                if(result!=GEN_SUCCESS)
                    return GEN_ERROR;

                TypeId assignType={};
                result = GenerateReference(info,expression->left,&assignType,idScope);
                if(result!=GEN_SUCCESS)
                    return GEN_ERROR;

                info.addPop(BC_REG_RBX);

                if (!PerformSafeCast(info, rtype, assignType)) { // CASTING RIGHT VALUE TO TYPE ON THE LEFT
                    std::string leftstr = info.ast->typeToString(assignType);
                    std::string rightstr = info.ast->typeToString(rtype);
                    ERRTYPE(expression->tokenRange, assignType, rtype, "\n\n";
                        ERR_LINE(expression->left->tokenRange, leftstr.c_str());
                        ERR_LINE(expression->right->tokenRange,rightstr.c_str());
                    )
                    return GEN_ERROR;
                }
                // if(assignType != rtype){
                //     if(info.errors != 0 || info.compileInfo->typeErrors !=0){
                //         return GEN_ERROR;
                //     }
                //     log::out << info.ast->typeToString(assignType)<<" = "<<info.ast->typeToString(rtype)<<"\n";
                //     // TODO: Try to cast, you may need to rearragne some things and also add some poly_versions type stuff.
                //     //  You'll figure it out.
                //     Assert(assignType == rtype);
                // }

                 // note that right expression should be popped first

                // TODO: This code can be improved.
                //  Don't push if value is ignored.
                //  Don't pop, just copy if you need the pushed values.
                //  perhaps implement a function called GenerateCopy? nah, come up with a better name, rename GeneratePop/Push too.
                result = GeneratePop(info, BC_REG_RBX, 0, assignType);
                result = GeneratePush(info, BC_REG_RBX, 0, assignType);

                // int rsize = info.ast->getTypeSize(rtype);
                // u8 reg = RegBySize(BC_AX, rsize);
                // info.addPop(reg);
                // // info.addPop(BC_REG_RBX);
                // info.addInstruction({BC_MOV_RM, reg, BC_REG_RBX}); size?
                // info.addPush(reg);
                
                outTypeIds->add(rtype);
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
                    op = expression->assignOpType;
                    info.addPop(BC_REG_RBX);
                }
                u8 FIRST_REG = BC_CX; 
                u8 SECOND_REG = BC_AX;
                u8 OUT_REG = BC_CX;
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

                    u8 lsize = info.ast->getTypeSize(ltype);
                    u8 rsize = info.ast->getTypeSize(rtype);
                    u8 reg1 = RegBySize(FIRST_REG, lsize); // get the appropriate registers
                    u8 reg2 = RegBySize(SECOND_REG, rsize);
                    info.addPop(reg2); // note that right expression should be popped first
                    info.addPop(reg1);

                    u8 bytecodeOp = ASTOpToBytecode(op,false);
                    if(bytecodeOp==0){
                        ERR_HEAD2(expression->tokenRange) << " operation "<<expression->tokenRange.firstToken << " not available for types "<<
                            info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<"\n";
                        ERR_END
                    }

                    if(ltype.isPointer()){
                        info.addInstruction({bytecodeOp, reg1, reg2, reg1});
                        info.addPush(reg1);
                        if(expression->typeId==AST_ASSIGN){
                            info.addInstruction({BC_MOV_RM, reg1, BC_REG_RBX, lsize});
                        }
                        outTypeIds->add(ltype);
                    }else{
                        info.addInstruction({bytecodeOp, reg1, reg2, reg2});
                        info.addPush(reg2);
                        if(expression->typeId==AST_ASSIGN){
                            info.addInstruction({BC_MOV_RM, reg2, BC_REG_RBX, rsize});
                        }
                        outTypeIds->add(rtype);
                    }
                } else if ((AST::IsInteger(ltype) || ltype == AST_CHAR) && (AST::IsInteger(rtype) || rtype == AST_CHAR)){
                    // u8 reg = RegBySize(BC_DX, info.ast->getTypeSize(rtype));
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
                    u8 bytecodeOp = ASTOpToBytecode(op,false);
                    u8 lsize = info.ast->getTypeSize(ltype);
                    u8 rsize = info.ast->getTypeSize(rtype);
                    // We do a little switchero on the registers to better
                    // adapt to x64 instructions
                    if(bytecodeOp == BC_DIVI) {
                        FIRST_REG = BC_AX;
                        SECOND_REG = BC_CX;
                        OUT_REG = BC_AX;
                    } else if(bytecodeOp == BC_MODI) {
                        FIRST_REG = BC_AX;
                        SECOND_REG = BC_CX;
                        OUT_REG = BC_DX;
                    } else if(bytecodeOp == BC_BLSHIFT || bytecodeOp == BC_BRSHIFT) {
                        FIRST_REG = BC_AX;
                        SECOND_REG = BC_CX;
                        OUT_REG = BC_AX;
                    }
                    u8 reg1 = RegBySize(FIRST_REG, lsize); // get the appropriate registers
                    u8 reg2 = RegBySize(SECOND_REG, rsize);
                    u8 regOut = RegBySize(OUT_REG, lsize);
                    info.addPop(reg2); // note that right expression should be popped first
                    info.addPop(reg1);

                    if(bytecodeOp==0){
                        ERR_HEAD2(expression->tokenRange) << " operation "<<expression->tokenRange.firstToken << " not available for types "<<
                            info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<"\n";
                        ERR_END
                    }

                    // if(lsize > rsize){
                        info.addInstruction({bytecodeOp, reg1, reg2, regOut});
                        info.addPush(regOut);
                        if(expression->typeId==AST_ASSIGN){
                            info.addInstruction({BC_MOV_RM, regOut, BC_REG_RBX, lsize});
                        }
                        outTypeIds->add(ltype);
                    // }else{
                    //     info.addInstruction({bytecodeOp, reg1, reg2, reg2});
                    //     info.addPush(reg2);
                    //     if(expression->typeId==AST_ASSIGN){
                    //         info.addInstruction({BC_MOV_RM, reg2, BC_REG_RBX}); size?
                    //     }
                    //     outTypeIds->add(rtype);
                    // }
                } 
                // else if ((ltype == AST_CHAR && AST::IsInteger(rtype))||
                //     (AST::IsInteger(ltype)&&rtype == AST_CHAR)){

                //     TOKENINFO(expression->tokenRange)
                //     u8 lsize = info.ast->getTypeSize(ltype);
                //     u8 rsize = info.ast->getTypeSize(rtype);
                //     u8 lreg = RegBySize(FIRST_REG, lsize); // get the appropriate registers
                //     u8 rreg = RegBySize(SECOND_REG, rsize);
                //     info.addPop(rreg); // note that right expression should be popped first
                //     info.addPop(lreg);

                //     u8 bytecodeOp = ASTOpToBytecode(op,false);
                //     if(bytecodeOp==0){
                //         ERR_HEAD2(expression->tokenRange) << " operation "<<expression->tokenRange.firstToken << " not available for types "<<
                //             info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<"\n";
                //         ERR_END
                //     }

                //     if(ltype == AST_CHAR){
                //         info.addInstruction({bytecodeOp, lreg, rreg, lreg});
                //         info.addPush(lreg);
                //         if(expression->typeId==AST_ASSIGN){
                //             info.addInstruction({BC_MOV_RM, lreg, BC_REG_RBX, lsize});
                //         }
                //         outTypeIds->add(ltype);
                //     }else{
                //         info.addInstruction({bytecodeOp, lreg, rreg, rreg});
                //         info.addPush(rreg);
                //         if(expression->typeId==AST_ASSIGN){
                //             info.addInstruction({BC_MOV_RM, rreg, BC_REG_RBX, rsize});
                //         }
                //         outTypeIds->add(rtype);
                //     }
                // } 
                else {
                    int bad = false;
                    if(ltype.getId()>=AST_TRUE_PRIMITIVES){
                        bad=true;
                        std::string msg = info.ast->typeToString(ltype);
                        ERR_HEAD(expression->left->tokenRange, "Cannot do operation on struct.\n\n";
                            ERR_LINE(expression->left->tokenRange,msg.c_str());
                        )
                    }
                    if(rtype.getId()>=AST_TRUE_PRIMITIVES){
                        bad=true;
                        std::string msg = info.ast->typeToString(rtype);
                        ERR_HEAD(expression->right->tokenRange, "Cannot do operation on struct.\n\n";
                            ERR_LINE(expression->right->tokenRange,msg.c_str());
                        )
                    }
                    if(bad){
                        return GEN_ERROR;
                    }

                    // u32 rsize = info.ast->getTypeSize(rtype);

                    // u8 reg = RegBySize(BC_DX, rsize);
                    // info.addPop(reg);
                    if (!PerformSafeCast(info, rtype, ltype)) { // CASTING RIGHT VALUE TO TYPE ON THE LEFT
                        std::string leftstr = info.ast->typeToString(ltype);
                        std::string rightstr = info.ast->typeToString(rtype);
                        ERRTYPE(expression->tokenRange, ltype, rtype, "\n\n";
                            ERR_LINE(expression->left->tokenRange, leftstr.c_str());
                            ERR_LINE(expression->right->tokenRange,rightstr.c_str());
                        )
                        return GEN_ERROR;
                    }
                    rtype = ltype;
                    
                    u8 lsize = info.ast->getTypeSize(ltype);
                    u8 rsize = info.ast->getTypeSize(rtype);
                    // info.addPush(reg);

                    TOKENINFO(expression->tokenRange)
                    u8 reg1 = RegBySize(FIRST_REG, lsize); // get the appropriate registers
                    u8 reg2 = RegBySize(SECOND_REG, rsize);
                    info.addPop(reg2); // note that right expression should be popped first
                    info.addPop(reg1);

                    u8 bytecodeOp = ASTOpToBytecode(op,ltype==AST_FLOAT32);
                    if(bytecodeOp==0){
                        ERR_HEAD2(expression->tokenRange) << " operation "<<expression->tokenRange.firstToken << " not available for types "<<
                            info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<"\n";
                        ERR_END
                    }

                    if(ltype.isPointer()){
                        info.addInstruction({bytecodeOp, reg1, reg2, reg1});
                        outTypeIds->add(ltype);
                    }else{
                        info.addInstruction({bytecodeOp, reg1, reg2, reg2});
                        outTypeIds->add(rtype);
                    }
                    if(expression->typeId==AST_ASSIGN){
                        info.addInstruction({BC_MOV_RM, reg2, BC_REG_RBX, rsize});
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
            Assert(("Leaking virtual type!",ti->id == typ));
        }
    }
    // To avoid ensuring non virtual type everywhere all the entry point where virtual type can enter
    // the expression system is checked. The rest of the system won't have to check it.
    // An entry point has been missed if the assert above fire.
    return GEN_SUCCESS;
}
int GenerateDefaultValue(GenInfo &info, u8 baseReg, int offset, TypeId typeId, TokenRange* tokenRange) {
    using namespace engone;
    MEASURE;
    
    // MAKE_NODE_SCOPE(_expression);
    // Assert(typeInfo)
    TypeInfo* typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = info.ast->getTypeInfo(typeId);
    u8 size = info.ast->getTypeSize(typeId);
    if (typeInfo && typeInfo->astStruct) {
        for (int i = typeInfo->astStruct->members.size() - 1; i >= 0; i--) {
            auto &member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            // log::out << "GEN "<<typeInfo->astStruct->name<<"."<<member.name<<"\n";
            // log::out << " alignedSize "<<info.ast->getTypeAlignedSize(memdata.typeId)<<"\n";
            if(member.defaultValue && typeInfo->astStruct->polyArgs.size()){
                WARN_HEAD((*tokenRange), "Polymorphism may not work with default values!";)
            }
            if(i+1<(int)typeInfo->astStruct->members.size()){
                // structs in structs will be aligned by their individual members
                // instead of the alignment of the structs as a whole.
                // This will make sure the structs are aligned.
                auto prevMem = typeInfo->getMember(i+1);
                u32 alignSize = info.ast->getTypeAlignedSize(prevMem.typeId);
                // log::out << "Try align "<<alignSize<<"\n";
                // info.addAlign(alignSize);
            }

            if (member.defaultValue) {
                TypeId typeId = {};
                int result = GenerateExpression(info, member.defaultValue, &typeId);
                if (memdata.typeId != typeId) {
                    ERRTYPE(member.defaultValue->tokenRange, memdata.typeId, typeId, "(default member)\n");
                    ERR_END
                }
                if(baseReg!=0){
                    int result = GeneratePop(info, baseReg, offset, typeId);
                }
            } else {
                int result = GenerateDefaultValue(info, baseReg, offset, memdata.typeId, tokenRange);
            }
        }
    } else {
        Assert(size <= 8);
        if(baseReg == 0){
            info.addPush(BC_REG_RAX);
        } else {
            u8 reg = RegBySize(BC_AX,size);
            info.addInstruction({BC_BXOR, reg, reg, reg});
            // info.addLoadIm(BC_REG_RCX,offset);
            // info.addInstruction({BC_ADDI,BC_REG_RCX, baseReg, BC_REG_RCX});
            info.addInstruction({BC_MOV_RM_DISP32,reg,baseReg,size});
            info.code->addIm(offset);
        }
    }
    return GEN_SUCCESS;
}
int GenerateBody(GenInfo &info, ASTScope *body);
int GenerateFunction(GenInfo& info, ASTFunction* function, ASTStruct* astStruct = nullptr){
    using namespace engone;
    MEASURE;

    _GLOG(FUNC_ENTER_IF(function->linkConvention == LinkConventions::NONE)) // no need to log
    
    MAKE_NODE_SCOPE(function);

    Assert(!function->body == !function->needsBody());
    // Assert(function->body || function->external || function->native ||info.compileInfo->errors!=0);
    // if(!function->body && !function->external && !function->native) return PARSE_ERROR;
    
    // NOTE: This is the only difference between how methods and functions
    //  are generated.
    Identifier* identifier = nullptr;
    if(!astStruct){
        identifier = info.ast->findIdentifier(info.currentScopeId, function->name);
        if (!identifier) {
            // NOTE: function may not have been added in the type checker stage for some reason.
            // THANK YOU, past me for writing this note. I was wondering what I broke and reading the
            // note made instantly realise that I broke something in the type checker.
            ERR_HEAD(function->tokenRange, "'"<<function->name << "' was null (compiler bug)\n";
                ERR_LINE(function->tokenRange, "bad");
            )
            // if (function->tokenRange.firstToken.str) {
            //     ERRTOKENS(function->tokenRange)
            // }
            // ERR_END
            return GEN_ERROR;
        }
        if (identifier->type != Identifier::FUNC) {
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
    if(function->linkConvention != LinkConventions::NONE){
        if(function->polyArgs.size()!=0 || (astStruct && astStruct->polyArgs.size()!=0)){
            ERR_HEAD(function->tokenRange, "External/native functions cannot be polymorphic.\n\n";
                ERR_LINE(function->tokenRange, "remove polymorphism");
            )
            return GEN_ERROR;
        }
        if(identifier->funcOverloads.overloads.size() != 1){
            // TODO: This error prints multiple times for each duplicate definition of native function.
            //   With this, you know the location of the colliding functions but the error message is printed multiple times.
            //  It is better if the message is shown once and then show all locations at once.
            ERR_HEAD(function->tokenRange, "External/native functions can only have one overload.\n\n";
                ERR_LINE(function->tokenRange, "bad");
            )
            return GEN_ERROR;
        }
        // if(info.compileInfo->typeErrors==0){
        //     Assert(function->_impls.size()==1);
        // }
        if(function->linkConvention == NATIVE){
        // info.compileInfo->targetPlatform == INTERPRETER
            Assert(info.compileInfo->nativeRegistry);
            auto nativeFunction = info.compileInfo->nativeRegistry->findFunction(function->name);
            if(nativeFunction){
                if(function->_impls.size()>0){
                    // impls may be zero if type checker found multiple definitions for native functions.
                    // we don't want to crash here when that happens and we don't need to throw an error
                    // because type checker already did.
                    function->_impls.last()->address = nativeFunction->jumpAddress;
                }
            } else {
                ERR_HEAD(function->tokenRange, "'"<<function->name<<"' is not an native function. None of the "<<info.compileInfo->nativeRegistry->nativeFunctions.size()<<" native functions matched.\n\n";
                    ERR_LINE(function->name, "bad");
                )
                return GEN_ERROR;
            }
            _GLOG(log::out << "Native function "<<function->name<<"\n";)
        } else {
            // exports not handled
            Assert(function->linkConvention == IMPORT || function->linkConvention == DLLIMPORT);
            if(function->_impls.size()>0){
                function->_impls.last()->address = FuncImpl::ADDRESS_EXTERNAL;
            }
            _GLOG(log::out << "External function "<<function->name<<"\n";)
        }
        return GEN_SUCCESS;
    }

    // When export is implemented I need to come back here and fix it.
    // The assert will notify me when that happens.
    Assert(function->linkConvention == LinkConventions::NONE);

    for(auto& funcImpl : function->getImpls()) {
        Assert(("func has already been generated!",funcImpl->address == 0));
        // This happens with functions inside of polymorphic function.
        if(function->callConvention != BETCALL) {
            ERR_HEAD(function->tokenRange, "The convention '" << function->callConvention << "' has not been implemented for user functions. The normal convention (betcall) is the only one that works.";
                ERR_LINE(function->tokenRange,"use betcall");
            )
            continue;
        }

        _GLOG(log::out << "Function " << funcImpl->name << "\n";)
        
        funcImpl->address = info.code->length();
        info.currentPolyVersion = funcImpl->polyVersion;

        auto prevFunc = info.currentFunction;
        auto prevFuncImpl = info.currentFuncImpl;
        auto prevScopeId = info.currentScopeId;
        info.currentFunction = function;
        info.currentFuncImpl = funcImpl;
        info.currentScopeId = function->scopeId;
        defer { info.currentFunction = prevFunc;
            info.currentFuncImpl = prevFuncImpl;
            info.currentScopeId = prevScopeId; };

        info.functionStackMoment = info.saveStackMoment();

        int lastOffset = info.currentFrameOffset;
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
                // TypeInfo *typeInfo = info.ast->getTypeInfo(argImpl.typeId.baseType());

                var->frameOffset = GenInfo::FRAME_SIZE + argImpl.offset;
                _GLOG(log::out << " " <<"["<<var->frameOffset<<"] "<< arg.name << ": " << info.ast->typeToString(argImpl.typeId) << "\n";)
            }
            _GLOG(log::out << "\n";)
        }
        // info.currentFrameOffset = 0;
        
        // HELLO READ THIS!
        // Before changing this code to make the caller make space for return values instead of
        // the calle, you have to realise that the return values must have the same offset everytime
        // you call the function. Meaning, return values must be aligned to 8 bytes even if they are
        // 4 byte integers. If not then the function will put the return values in the wrong place.

        info.currentFrameOffset = 0; // reset frame offset at beginning of function

        // x64's call instruction does ignores the base pointer (frame pointer).
        // The interpreter does can do either.
        #ifndef SAVE_FP_IN_CALL_FRAME
        info.addPush(BC_REG_FP);
        info.addInstruction({BC_MOV_RR, BC_REG_SP, BC_REG_FP});
        #endif
        if (funcImpl->returnTypes.size() != 0) {
            _GLOG(log::out << "space for " << funcImpl->returnTypes.size() << " return value(s) (struct may cause multiple push)\n");
            
            // info.code->addDebugText("ZERO init return values\n");
            info.addLoadIm(BC_REG_RCX, funcImpl->returnSize);
            info.addInstruction({BC_MOV_RR, BC_REG_FP, BC_REG_RBX});
            info.addInstruction({BC_SUBI, BC_REG_RBX, BC_REG_RCX, BC_REG_RBX});
            info.addInstruction({BC_MEMZERO, BC_REG_RBX, BC_REG_RCX});
            // info.addIncrSp(-funcImpl->returnSize);
            info.addStackSpace(-funcImpl->returnSize);
            _GLOG(log::out << "\n";)
            info.currentFrameOffset -= funcImpl->returnSize; // TODO: size can be uneven like 13. FIX IN EVALUATETYPES
        }
        if(astStruct){
            for(int i=0;i<(int)astStruct->polyArgs.size();i++){
                auto& arg = astStruct->polyArgs[i];
                arg.virtualType->id = funcImpl->structImpl->polyArgs[i];
            }
        }
        for(int i=0;i<(int)function->polyArgs.size();i++){
            auto& arg = function->polyArgs[i];
            arg.virtualType->id = funcImpl->polyArgs[i];
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
                    ERR_HEAD(function->tokenRange,"Missing return statement in '" << function->name << "'.\n\n";
                        ERR_LINE(function->tokenRange,"put a return in the body");
                    )
                }
            }
        }
        if(info.code->length()<1 || info.code->get(info.code->length()-1).opcode!=BC_RET) {
            // add return if it doesn't exist
            
            info.addPop(BC_REG_FP);
            info.addInstruction({BC_RET});
        }
        // needs to be done after frame pop
        info.restoreStackMoment(info.functionStackMoment);
        for (auto &arg : function->arguments) {
            info.ast->removeIdentifier(info.currentScopeId, arg.name);
        }
    }
    return GEN_SUCCESS;
}
int GenerateFunctions(GenInfo& info, ASTScope* body){
    using namespace engone;
    MEASURE;
    _GLOG(FUNC_ENTER)
    Assert(body || info.compileInfo->errors!=0);
    if(!body) return PARSE_ERROR;
    // MAKE_NODE_SCOPE(body); // we don't generate bytecode here so no need for this

    ScopeId savedScope = info.currentScopeId;
    defer { info.currentScopeId = savedScope; };

    info.currentScopeId = body->scopeId;

    for(auto it : body->namespaces) {
        GenerateFunctions(info, it);
    }
    for(auto it : body->functions) {
        if(it->body) {
            // External/native does not have body.
            // Not calling function reducess log messages
            GenerateFunctions(info, it->body);
        }
        GenerateFunction(info, it);
    }
    for(auto it : body->structs) {
        for (auto function : it->functions) {
            Assert(function->body);
            GenerateFunctions(info, function->body);
            GenerateFunction(info, function, it);
        }
    }
    return true;
}
int GenerateBody(GenInfo &info, ASTScope *body) {
    using namespace engone;
    MEASURE;
    _GLOG(FUNC_ENTER)
    Assert(body||info.compileInfo->errors!=0);
    if(!body) return PARSE_ERROR;

    MAKE_NODE_SCOPE(body); // no need, the scope itself doesn't generate code

    ScopeId savedScope = info.currentScopeId;
    defer { info.currentScopeId = savedScope; };

    info.currentScopeId = body->scopeId;

    int lastOffset = info.currentFrameOffset;

    for(auto it : body->namespaces) {
        int result = GenerateBody(info, it);
    }

    DynamicArray<std::string> varsToRemove;

    for (auto statement : body->statements) {
        MAKE_NODE_SCOPE(statement);
        if (statement->type == ASTStatement::ASSIGN) {
            _GLOG(SCOPE_LOG("ASSIGN"))
            
            auto& typesFromExpr = statement->versions_expresssionTypes[info.currentPolyVersion];
            if(statement->firstExpression && typesFromExpr.size() < statement->varnames.size()) {
                if(info.compileInfo->typeErrors==0){
                    char msg[100];
                    sprintf(msg,"%d variables",(int)statement->varnames.size());
                    ERR_HEAD(statement->tokenRange, "To many variables.";
                        ERR_LINE(statement->tokenRange, msg);
                        sprintf(msg,"%d return values",(int)typesFromExpr.size());
                        ERR_LINE(statement->firstExpression->tokenRange, msg);
                    )
                }
                continue;
            }

            // for(int i = 0;i<(int)typesFromExpr.size();i++){
            //     TypeId typeFromExpr = typesFromExpr[i];
            //     if((int)statement->varnames.size() <= i){
            //         // _GLOG(log::out << log::LIME<<"just pop "<<info.ast->typeToString(rightType)<<"\n";)
            //         continue;
            //     }

            for(int i = 0;i<(int)statement->varnames.size();i++){
                auto& varname = statement->varnames[i];
                // _GLOG(log::out << log::LIME <<"assign pop "<<info.ast->typeToString(rightType)<<"\n";)
                
                TypeId declaredType = varname.versions_assignType[info.currentPolyVersion];
                Assert(declaredType.isValid()); // Type checker should have fixed this. Implicit ones too.
                bool wasDeclaration = false;
                Identifier* varIdentifier = info.ast->findIdentifier(info.currentScopeId, varname.name);
                VariableInfo* varinfo = nullptr;
                if(!varIdentifier){
                    varinfo = info.ast->addVariable(info.currentScopeId, varname.name);
                    Assert(varinfo);
                    varsToRemove.add(varname.name);
                    varinfo->typeId = declaredType;
                    wasDeclaration = true;
                } else if(varIdentifier->type==Identifier::VAR) {
                    // variable is already defined
                    varinfo = info.ast->identifierToVariable(varIdentifier);
                    Assert(varinfo);
                    if(varname.declaration()){
                        if(varIdentifier->scopeId == info.currentScopeId) {
                            // info.ast->removeIdentifier(varIdentifier->scopeId, varname.name); // add variable already does this
                            varinfo = info.ast->addVariable(info.currentScopeId, varname.name, true);
                        } else {
                            varinfo = info.ast->addVariable(info.currentScopeId, varname.name, true);
                        }
                        varsToRemove.add(varname.name);
                        Assert(varinfo);
                        varinfo->typeId = declaredType;
                        wasDeclaration=true;
                    } else {
                        // TODO: Check casting
                        if(!info.ast->castable(declaredType, varinfo->typeId)){
                            if(info.compileInfo->typeErrors==0){
                                std::string leftstr = info.ast->typeToString(varinfo->typeId);
                                std::string rightstr = info.ast->typeToString(varname.versions_assignType[info.currentPolyVersion]);
                                ERR_HEAD(statement->tokenRange, "Type mismatch '"<<leftstr<<"' <- '"<<rightstr<< "' in assignment.\n\n";
                                    ERR_LINE(varname.name, leftstr.c_str());
                                    ERR_LINE(statement->firstExpression->tokenRange,rightstr.c_str());
                                )
                            }
                            // ERR_HEAD(varname.name.range(), "Type mismatch when assigning.\n\n";
                            //     ERR_LINE(varname.name.range(),"bad");
                            // )
                            continue;
                        }
                    }
                } else {
                    ERR_HEAD(varname.name.range(), "'"<<varname.name<<"' is defined as a non-variable and cannot be used.\n\n";
                        ERR_LINE(varname.name, "bad");
                    )
                    continue;
                }
                // is this casting handled already? type checker perhaps?
                // if(!info.ast->castable(typeFromExpr, declaredType)) {
                //     ERRTYPE(statement->tokenRange, declaredType, typeFromExpr, "(assign)\n";
                //         ERR_LINE(statement->tokenRange,"bad");
                //     )
                //     continue;
                // }

                // i32 rightSize = info.ast->getTypeSize(typeFromExpr);
                // i32 leftSize = info.ast->getTypeSize(var->typeId);
                // i32 asize = info.ast->getTypeAlignedSize(var->typeId);

                int alignment = 0;
                if (wasDeclaration) {
                    // if (leftSize == 0) {
                    //     ERR_HEAD(statement->tokenRange, "Size of type " << "?" << " was 0\n";
                    //     )
                    //     continue;
                    // }
                    if (varname.arrayLength>0){
                        if(statement->firstExpression) {
                            ERR_HEAD(statement->firstExpression->tokenRange, "An expression is not allowed when declaring an array on the stack. The array is zero-initialized by default.\n\n";
                                ERR_LINE(statement->firstExpression->tokenRange, "bad");
                            )
                            continue;
                        }
                        // Assert(("Arrays disabled due to refactoring of assignments",false));
                        // I have not refactored arrays. Do that. Probably not a lot of working. Mostly
                        // Checking that it works as it should and handle any errors. I don't think arrays
                        // were properly implemented before.

                        // make sure type is a slice?
                        // it will always be at the moment of writing since arrayLength is only set
                        // when slice is used but this may not be true in the future.
                        int arrayFrameOffset = 0;
                        {
                            TypeInfo *typeInfo = info.ast->getTypeInfo(varinfo->typeId.baseType());
                            TypeId innerType = typeInfo->structImpl->polyArgs[0];
                            if(!innerType.isValid())
                                continue; // error message should have been printed in type checker
                            i32 size2 = info.ast->getTypeSize(innerType);
                            // i32 asize2 = info.ast->getTypeAlignedSize(innerType);
                            int arraySize = size2 * varname.arrayLength;
                            
                            // Assert(size2 * varname.arrayLength <= pow(2,16)/2);
                            if(arraySize > pow(2,16)/2) {
                                std::string msg = std::to_string(size2) + " * "+ std::to_string(varname.arrayLength) +" = "+std::to_string(arraySize);
                                ERR_HEAD(statement->tokenRange, (int)(pow(2,16)/2) << " is the maximum size of arrays on the stack. "<<(arraySize)<<" was used which exceeds that. The limit comes from the instruction BC_INCR which uses a signed 16-bit integer.\n\n";
                                    ERR_LINE(statement->tokenRange, msg.c_str());
                                )
                                continue;
                            }

                            int diff = arraySize % 8;
                            if(diff != 0){
                                arraySize += 8 - diff;
                            }
                            Assert(info.currentFrameOffset%8 == 0);
                            
                            info.currentFrameOffset -= arraySize;
                            arrayFrameOffset = info.currentFrameOffset;
                            info.addIncrSp(-arraySize);

                            
                            info.addLoadIm(BC_REG_RDX,arraySize);
                            info.addInstruction({BC_MEMZERO, BC_REG_SP, BC_REG_RDX});
                        }
                        // data type may be zero if it wasn't specified during initial assignment
                        // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                        // int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
                        // if (diff != asize) {
                        //     info.currentFrameOffset -= diff; // align
                        // }
                        // info.currentFrameOffset -= size;
                        // var->frameOffset = info.currentFrameOffset;

                        
                        int result = FramePush(info,varinfo->typeId,&varinfo->frameOffset,false);

                        // TODO: Don't hardcode this slice stuff, maybe I have to.
                        // push length
                        info.addLoadIm(BC_REG_RDX,varname.arrayLength);
                        info.addPush(BC_REG_RDX);

                        // push ptr
                        info.addLoadIm(BC_REG_RBX,arrayFrameOffset);
                        info.addInstruction({BC_ADDI,BC_REG_RBX, BC_REG_FP, BC_REG_RBX});
                        info.addPush(BC_REG_RBX);

                        GeneratePop(info, BC_REG_FP, varinfo->frameOffset, varinfo->typeId);
                    } else {
                        int result = FramePush(info, varinfo->typeId, &varinfo->frameOffset, !statement->firstExpression);
                    }
                    _GLOG(log::out << "declare " << varname.name << " at " << varinfo->frameOffset << "\n";)
                    // NOTE: inconsistent
                    // char buf[100];
                    // int len = sprintf(buf," ^ was assigned %s",statement->name->c_str());
                    // info.code->addDebugText(buf,len);
                }
                if(varinfo){
                    _GLOG(log::out << " " << varname.name << " : " << info.ast->typeToString(varinfo->typeId) << "\n";)
                }
            }
            if(statement->firstExpression){
                DynamicArray<TypeId> rightTypes{};
                int result = GenerateExpression(info, statement->firstExpression, &rightTypes);
                if (result != GEN_SUCCESS) {
                    // assign fails and variable will not be declared potentially causing
                    // undeclared errors in later code. This is probably better than
                    // declaring the variable and using void type. Then you get type mismatch
                    // errors.
                    continue;
                }
                // Type checker or generator has a bug if they check/generate different types
                Assert(typesFromExpr.size()==rightTypes.size());
                for(int i=0;i<typesFromExpr.size();i++){
                    Assert(typesFromExpr[i] == rightTypes[i]);
                }

                for(int i = (int)typesFromExpr.size()-1;i>=0;i--){
                    TypeId typeFromExpr = typesFromExpr[i];
                    if((int)statement->varnames.size() <= i){
                        _GLOG(log::out << log::LIME<<"just pop "<<info.ast->typeToString(typeFromExpr)<<"\n";)
                        GeneratePop(info, 0, 0, typeFromExpr);
                        continue;
                    }
                    _GLOG(log::out << log::LIME <<"assign pop "<<info.ast->typeToString(typeFromExpr)<<"\n";)
                    
                    TypeId stateTypeId = statement->varnames[i].versions_assignType[info.currentPolyVersion];
                    Token* name = &statement->varnames[i].name;
                    auto id = info.ast->findIdentifier(info.currentScopeId, *name);
                    if(!id){
                        Assert(info.errors!=0); // there should have been errors
                        continue;
                    }
                    VariableInfo* var = info.ast->identifierToVariable(id);
                    if(!var){
                        Assert(info.errors!=0); // there should have been errors
                        continue;
                    }

                    if(!PerformSafeCast(info, typeFromExpr, var->typeId)){
                        if(info.compileInfo->typeErrors==0){
                            ERRTYPE(statement->tokenRange, var->typeId, typeFromExpr, "(assign).\n\n";
                                ERR_LINE(statement->tokenRange,"bad");
                            )
                        }
                        continue;
                    }

                    GeneratePop(info, BC_REG_FP, var->frameOffset, var->typeId);
                }
            }
        }
        else if (statement->type == ASTStatement::IF) {
            _GLOG(SCOPE_LOG("IF"))

            TypeId dtype = {};
            int result = GenerateExpression(info, statement->firstExpression, &dtype);
            if (result == GEN_ERROR) {
                // generate body anyway or not?
                continue;
            }
            // if(dtype!=AST_BOOL8){
            //     ERR_HEAD2(statement->expression->token) << "Expected a boolean, not '"<<TypeIdToStr(dtype)<<"'\n";
            //     continue;
            // }

            // info.addInstruction({BC_POP,BC_REG_RAX});
            // TypeInfo *typeInfo = info.ast->getTypeInfo(dtype);
            u32 size = info.ast->getTypeSize(dtype);
            if (!size) {
                // TODO: Print error? or does generate expression do that since it gives us dtype?
                continue;
            }
            u8 reg = RegBySize(BC_AX, size);

            info.addPop(reg);
            info.addInstruction({BC_JNE, reg});
            u32 skipIfBodyIndex = info.code->length();
            info.code->addIm(0);

            result = GenerateBody(info, statement->firstBody);
            if (result == GEN_ERROR)
                continue;

            u32 skipElseBodyIndex = -1;
            if (statement->secondBody) {
                info.addInstruction({BC_JMP});
                skipElseBodyIndex = info.code->length();
                info.code->addIm(0);
            }

            // fix address for jump instruction
            info.code->getIm(skipIfBodyIndex) = info.code->length();

            if (statement->secondBody) {
                result = GenerateBody(info, statement->secondBody);
                if (result == GEN_ERROR)
                    continue;

                info.code->getIm(skipElseBodyIndex) = info.code->length();
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
            int result = GenerateExpression(info, statement->firstExpression, &dtype);
            if (result == GEN_ERROR) {
                // generate body anyway or not?
                continue;
            }
            // if(dtype!=AST_BOOL8){
            //     ERR_HEAD2(statement->expression->token) << "Expected a boolean, not '"<<TypeIdToStr(dtype)<<"'\n";
            //     continue;
            // }
            // info.addInstruction({BC_POP,BC_REG_RAX});
            // TypeInfo *typeInfo = info.ast->getTypeInfo(dtype);
            u32 size = info.ast->getTypeSize(dtype);
            u8 reg = RegBySize(BC_AX, size);

            info.addPop(reg);
            info.addInstruction({BC_JNE, reg});
            loopScope->resolveBreaks.push_back(info.code->length());
            info.code->addIm(0);

            result = GenerateBody(info, statement->firstBody);
            if (result == GEN_ERROR)
                continue;

            info.addInstruction({BC_JMP});
            info.code->addIm(loopScope->continueAddress);

            for (auto ind : loopScope->resolveBreaks) {
                info.code->getIm(ind) = info.code->length();
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

            int stackBeforeLoop = info.saveStackMoment();
            int frameBeforeLoop = info.currentFrameOffset;

            // TODO: Save stack moment here?

            // body scope is used since the for loop's variables
            // shouldn't collide with the variables in the current scope.
            // not sure how well this works, we shall see.
            ScopeId scopeForVariables = statement->firstBody->scopeId;

            if(statement->rangedForLoop){
                Token& itemvar = statement->varnames[0].name;
                TypeId itemtype = statement->varnames[0].versions_assignType[info.currentPolyVersion];
                if(!itemtype.isValid()){
                    // error has probably been handled somewhere. no need to print again.
                    continue;
                }
                auto* varinfo_index = info.ast->addVariable(scopeForVariables,"nr");
                if(!varinfo_index) {
                    ERR_HEAD(statement->tokenRange, "nr variable already exists\n.";)
                    continue;
                }
                // auto iden = info.ast->findIdentifier(scopeForVariables,"nr");
                // log::out << "IDEN VAR INDEX, "<<iden->varIndex<<"\n";

                auto varinfo_item = info.ast->addVariable(scopeForVariables,itemvar);
                 if(!varinfo_item) {
                    ERR_HEAD(statement->tokenRange, itemvar << " variable already exists\n.";)
                    continue;
                }
                varinfo_item->typeId = itemtype;
                varinfo_index->typeId = itemtype;

                {
                    // i32 size = info.ast->getTypeSize(varinfo_index->typeId);
                    // i32 asize = info.ast->getTypeAlignedSize(varinfo_index->typeId);
                    // // data type may be zero if it wasn't specified during initial assignment
                    // // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                    // int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
                    // if (diff != asize) {
                    //     info.currentFrameOffset -= diff; // align
                    // }
                    // info.currentFrameOffset -= size;

                    // varinfo_index->frameOffset = 0;
                    // TypeId typeId = statement->varnames[0].versions_assignType[info.currentPolyVersion];
                    TypeId typeId = AST_INT32; // you may want to use the type in varname, the reason i don't is because
                    // i seem to have used EAX here so it's best to keep doing that until i decide to fix this for real.
                    FramePush(info, typeId, &varinfo_index->frameOffset,false);
                    varinfo_item->frameOffset = varinfo_index->frameOffset;

                    TypeId dtype = {};
                    // Type should be checked in type checker and further down
                    // when extracting ptr and len. No need to check here.
                    int result = GenerateExpression(info, statement->firstExpression, &dtype);
                    if (result != GEN_SUCCESS) {
                        continue;
                    }
                    if(statement->reverse){
                        info.addPop(BC_REG_EDX); // throw range.now
                        info.addPop(BC_REG_EAX);
                    }else{
                        info.addPop(BC_REG_EAX);
                        info.addPop(BC_REG_EDX); // throw range.end
                        info.addInstruction({BC_INCR,BC_REG_EAX,0xFF,0xFF});
                        // decrement by one since for loop increments
                        // before going into the loop
                    }
                    info.addPush(BC_REG_EAX);
                    GeneratePop(info, BC_REG_FP, varinfo_index->frameOffset, typeId);
                }
                // i32 itemsize = info.ast->getTypeSize(varinfo_item->typeId);

                _GLOG(log::out << "push loop\n");
                loopScope->stackMoment = info.saveStackMoment();
                loopScope->continueAddress = info.code->length();

                // log::out << "frame: "<<varinfo_index->frameOffset<<"\n";
                // TODO: don't generate ptr and length everytime.
                // put them in a temporary variable or something.
                TypeId dtype = {};
                // info.code->addDebugText("Generate and push range\n");
                int result = GenerateExpression(info, statement->firstExpression, &dtype);
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

                info.addInstruction({BC_MOV_MR_DISP32, BC_REG_FP, index_reg, 4});
                info.code->addIm(varinfo_index->frameOffset);

                if(statement->reverse){
                    info.addInstruction({BC_INCR,index_reg, 0xFF, 0xFF}); // hexidecimal represent -1
                }else{
                    info.addInstruction({BC_INCR,index_reg,1});
                }
                
                info.addInstruction({BC_MOV_RM_DISP32, index_reg, BC_REG_FP, 4});
                info.code->addIm(varinfo_index->frameOffset);

                if(statement->reverse){
                    // info.code->addDebugText("For condition (reversed)\n");
                    info.addInstruction({BC_GTE,index_reg,length_reg,BC_REG_EAX});
                } else {
                    // info.code->addDebugText("For condition\n");
                    info.addInstruction({BC_LT,index_reg,length_reg,BC_REG_EAX});
                }
                info.addInstruction({BC_JNE, BC_REG_EAX});
                // resolve end, not break, resolveBreaks is bad naming
                loopScope->resolveBreaks.push_back(info.code->length());
                info.code->addIm(0);

                result = GenerateBody(info, statement->firstBody);
                if (result != GEN_SUCCESS)
                    continue;

                info.addInstruction({BC_JMP});
                info.code->addIm(loopScope->continueAddress);

                for (auto ind : loopScope->resolveBreaks) {
                    info.code->getIm(ind) = info.code->length();
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
                    // i32 size = info.ast->getTypeSize(varinfo_index->typeId);
                    // i32 asize = info.ast->getTypeAlignedSize(varinfo_index->typeId);
                    // data type may be zero if it wasn't specified during initial assignment
                    // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                    // int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
                    // if (diff != asize) {
                    //     info.currentFrameOffset -= diff; // align
                    // }
                    // info.currentFrameOffset -= size;
                    // varinfo_index->frameOffset = info.currentFrameOffset;

                    int result = FramePush(info, varinfo_index->typeId, &varinfo_index->frameOffset, false);

                    if(statement->reverse){
                        TypeId dtype = {};
                        // Type should be checked in type checker and further down
                        // when extracting ptr and len. No need to check here.
                        int result = GenerateExpression(info, statement->firstExpression, &dtype);
                        if (result != GEN_SUCCESS) {
                            continue;
                        }
                        info.addPop(BC_REG_RAX);
                        info.addPop(BC_REG_RAX);
                        info.addInstruction({BC_CAST,CAST_UINT_SINT, BC_REG_RAX,BC_REG_EAX});
                    }else{
                        info.addLoadIm(BC_REG_EAX,-1);
                    }
                    info.addPush(BC_REG_EAX);

                    Assert(varinfo_index->typeId == AST_INT32);

                    GeneratePop(info, BC_REG_FP,varinfo_index->frameOffset,varinfo_index->typeId);
                }
                Token& itemvar = statement->varnames[0].name;
                TypeId itemtype = statement->varnames[0].versions_assignType[info.currentPolyVersion];
                if(!itemtype.isValid()){
                    // error has probably been handled somewhere. no need to print again.
                    // if it wasn't and you just found out that things went wrong here
                    // then sorry.
                    continue;
                }
                i32 itemsize = info.ast->getTypeSize(itemtype);
                auto varinfo_item = info.ast->addVariable(scopeForVariables,itemvar);
                varinfo_item->typeId = itemtype;
                if(statement->pointer){
                    varinfo_item->typeId.setPointerLevel(varinfo_item->typeId.getPointerLevel()+1);
                }
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
                // info.code->addDebugText("Generate and push slice\n");
                int result = GenerateExpression(info, statement->firstExpression, &dtype);
                if (result != GEN_SUCCESS) {
                    continue;
                }
                TypeInfo* ti = info.ast->getTypeInfo(dtype);
                Assert(ti && ti->astStruct && ti->astStruct->name == "Slice");
                auto memdata_ptr = ti->getMember("ptr");
                auto memdata_len = ti->getMember("len");

                // NOTE: careful when using registers since you might use 
                //  one for multiple things. 
                u8 ptr_reg = RegBySize(BC_BX,info.ast->getTypeSize(memdata_ptr.typeId));
                u8 length_reg = RegBySize(BC_DX,info.ast->getTypeSize(memdata_len.typeId));
                u8 index_reg = BC_REG_ECX;

                // info.code->addDebugText("extract ptr and length\n");
                info.addPop(ptr_reg);
                info.addPop(length_reg);

                // info.code->addDebugText("Index increment/decrement\n");

                info.addInstruction({BC_MOV_MR_DISP32, BC_REG_FP, index_reg, 4});
                info.code->addIm(varinfo_index->frameOffset);
                if(statement->reverse){
                    info.addInstruction({BC_INCR,index_reg, 0xFF, 0xFF}); // hexidecimal represent -1
                }else{
                    info.addInstruction({BC_INCR,index_reg,1});
                }
                info.addInstruction({BC_MOV_RM_DISP32, index_reg, BC_REG_FP, 4});
                info.code->addIm(varinfo_index->frameOffset);

                if(statement->reverse){
                    // info.code->addDebugText("For condition (reversed)\n");
                    info.addLoadIm(length_reg,-1); // length reg is not used with reversed
                    info.addInstruction({BC_GT,index_reg,length_reg,BC_REG_EAX});
                } else {
                    // info.code->addDebugText("For condition\n");
                    info.addInstruction({BC_LT,index_reg,length_reg,BC_REG_EAX});
                }
                info.addInstruction({BC_JNE, BC_REG_EAX});
                // resolve end, not break, resolveBreaks is bad naming
                loopScope->resolveBreaks.push_back(info.code->length());
                info.code->addIm(0);

                // BE VERY CAREFUL SO YOU DON'T USE BUSY REGISTERS AND OVERWRITE
                // VALUES. UNEXPECTED VALUES WILL HAPPEN AND IT WILL BE PAINFUL.
                u8 dst_reg = BC_REG_RCX;
                u8 src_reg = BC_REG_RBX;
                u8 size_reg = BC_REG_RDX;
                info.addLoadIm(size_reg,itemsize);
                
                // NOTE: index_reg is modified here since it isn't needed anymore
                info.addInstruction({BC_MULI, index_reg, size_reg, index_reg});
                info.addInstruction({BC_ADDI, ptr_reg, index_reg, src_reg});
                if(statement->pointer){
                    info.addInstruction({BC_MOV_RM_DISP32, src_reg, BC_REG_FP, 8});
                    info.code->addIm(varinfo_item->frameOffset);

                } else {
                    info.addLoadIm(BC_REG_RAX,varinfo_item->frameOffset);
                    info.addInstruction({BC_MOV_RR, BC_REG_FP, dst_reg});
                    info.addInstruction({BC_ADDI,dst_reg, BC_REG_RAX, dst_reg});

                    info.addInstruction({BC_MEMCPY,dst_reg, src_reg, size_reg});
                }

                result = GenerateBody(info, statement->firstBody);
                if (result != GEN_SUCCESS)
                    continue;

                info.addInstruction({BC_JMP});
                info.code->addIm(loopScope->continueAddress);

                for (auto ind : loopScope->resolveBreaks) {
                    info.code->getIm(ind) = info.code->length();
                }
                
                // delete nr, frameoffset needs to be changed to if so
                // info.addPop(BC_REG_EAX); 

                info.ast->removeIdentifier(scopeForVariables, "nr");
                info.ast->removeIdentifier(scopeForVariables, itemvar);
            }

            info.restoreStackMoment(stackBeforeLoop);
            info.currentFrameOffset = frameBeforeLoop;

            // pop loop happens in defer
        } else if(statement->type == ASTStatement::BREAK) {
            GenInfo::LoopScope* loop = info.getLoop(info.loopScopes.size()-1);
            if(!loop) {
                ERR_HEAD(statement->tokenRange, "Break is only allowed in loops.\n\n";
                    ERR_LINE(statement->tokenRange,"not in a loop");
                )
                continue;
            }
            info.restoreStackMoment(loop->stackMoment, true);
            
            info.addInstruction({BC_JMP});
            loop->resolveBreaks.push_back(info.code->length());
            info.code->addIm(0);
        } else if(statement->type == ASTStatement::CONTINUE) {
            GenInfo::LoopScope* loop = info.getLoop(info.loopScopes.size()-1);
            if(!loop) {
                ERR_HEAD(statement->tokenRange, "Continue is only allowed in loops.\n\n";
                    ERR_LINE(statement->tokenRange,"not in a loop");
                )
                continue;
            }
            info.restoreStackMoment(loop->stackMoment, true);

            info.addInstruction({BC_JMP});
            info.code->addIm(loop->continueAddress);
        } else if (statement->type == ASTStatement::RETURN) {
            _GLOG(SCOPE_LOG("RETURN"))

            if (!info.currentFunction) {
                ERR_HEAD(statement->tokenRange, "Return only allowed in function.\n\n";
                    ERR_LINE(statement->tokenRange, "bad");
                )
                continue;
                // return GEN_ERROR;
            }
            if ((int)statement->returnValues.size() != (int)info.currentFuncImpl->returnTypes.size()) {
                // std::string msg = 
                ERR_HEAD(statement->tokenRange, "Found " << statement->returnValues.size() << " return value(s) but should have " << info.currentFuncImpl->returnTypes.size() << " for '" << info.currentFunction->name << "'.\n\n";
                    ERR_LINE(info.currentFunction->tokenRange, "X return values");
                    ERR_LINE(statement->tokenRange, "Y values");
                )
            }

            //-- Evaluate return values
            // ASTExpression *nextExpr = statement->firstExpression;
            // int argi = -1;
            for (int argi = 0; argi < (int)statement->returnValues.size(); argi++) {
                ASTExpression *expr = statement->returnValues.get(argi);
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
            }

            // fix stack pointer before returning
            // info.addIncrSp(-info.currentFrameOffset);
            info.currentFrameOffset = lastOffset;
            info.restoreStackMoment(info.functionStackMoment - 8, true); // -8 to not include BC_REG_FP
            info.addInstruction({BC_POP, BC_REG_FP},true);
            info.addInstruction({BC_RET});
        }
        else if (statement->type == ASTStatement::EXPRESSION) {
            _GLOG(SCOPE_LOG("EXPRESSION"))

            TypeId dtype = {};
            int result = GenerateExpression(info, statement->firstExpression, &dtype);
            if (result == GEN_ERROR) {
                continue;
            }
            if (dtype != AST_VOID) {
                // TODO: handle struct
                // TypeInfo *typeInfo = info.ast->getTypeInfo(dtype);
                // u32 size = info.ast->getTypeSize(dtype);
                // u8 reg = RegBySize(BC_AX, size);
                // info.addPop(reg);

                GeneratePop(info, 0, 0, dtype);
            }
        }
        else if (statement->type == ASTStatement::USING) {
            _GLOG(SCOPE_LOG("USING"))

            Assert(statement->varnames.size()==1);

            Token* name = &statement->varnames[0].name;

            auto origin = info.ast->findIdentifier(info.currentScopeId, *name);
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

            int result = GenerateBody(info, statement->firstBody);
            // Is it okay to do nothing with result?
        } else if (statement->type == ASTStatement::BODY) {
            _GLOG(SCOPE_LOG("BODY (statement)"))

            int result = GenerateBody(info, statement->firstBody);
            // Is it okay to do nothing with result?
        }
    }
    for(auto& name : varsToRemove){
        info.ast->removeIdentifier(body->scopeId,name);
    }
    if (lastOffset != info.currentFrameOffset) {
        _GLOG(log::out << "fix sp when exiting body\n";)
        // info.code->addDebugText("fix sp when exiting body\n");
        // info.addIncrSp(info.currentFrameOffset);
        info.addIncrSp(lastOffset - info.currentFrameOffset);
        // info.addInstruction({BC_ADDI,BC_REG_SP,BC_REG_RCX,BC_REG_SP});
        info.currentFrameOffset = lastOffset;
    }
    return GEN_SUCCESS;
}
int GenerateData(GenInfo& info) {
    using namespace engone;

    // Also virtualStackPointer should be aligned before generating functions
    // Also there are some asserts going off.

    // IMPORTANT: TODO: Some data like 64-bit integers needs alignment.
    //   Strings don's so it's fine for now but don't forget about fixing this.
    for(auto& pair : info.ast->_constStringMap) {
        int offset = info.code->appendData(pair.first.data(), pair.first.length());
        if(offset == -1){
            continue;
        }
        auto& constString = info.ast->getConstString(pair.second);
        constString.address = offset;
    }
    return GEN_SUCCESS;
}

Bytecode *Generate(AST *ast, CompileInfo* compileInfo) {
    using namespace engone;
    MEASURE;
#ifdef LOG_ALLOC
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

    int result = GenerateData(info);

    // TODO: Skip function generation if there are no functions.
    //   We would need to go through every scope to know that though.
    //   Maybe the type checker can inform the generator?
    // info.code->addDebugText("FUNCTION SEGMENT\n");
    info.code->setLocationInfo("FUNCTION SEGMENT");
    _GLOG(log::out << "Jump to skip functions\n";)
    info.code->add({BC_JMP});
    int skipIndex = info.code->length();
    info.code->addIm(0);
    result = GenerateFunctions(info, info.ast->mainBody);
    if(skipIndex == info.code->length() -1) {
        // skip jump instruction if no functions where generated
        info.code->codeSegment.used = 0;
    } else {
        info.code->getIm(skipIndex) = info.code->length();
    }

    info.code->setLocationInfo("GLOBAL CODE SEGMENT");
    // info.code->addDebugText("GLOBAL CODE SEGMENT\n");

    info.currentPolyVersion = 0;
    result = GenerateBody(info, info.ast->mainBody);
    // TODO: What to do about result? nothing?

    std::unordered_map<FuncImpl*, int> resolveFailures;
    for(auto& e : info.callsToResolve){
        auto& inst = info.code->get(e.bcIndex);
        // Assert(e.funcImpl->address != Identifier::ADDRESS_INVALID);
        if(e.funcImpl->address != FuncImpl::ADDRESS_INVALID){
            *((i32*)&inst) = e.funcImpl->address;
        } else {
            ERR() << "Invalid function address for instruction["<<e.bcIndex << "]\n";
            auto pair = resolveFailures.find(e.funcImpl);
            if(pair == resolveFailures.end())
                resolveFailures[e.funcImpl] = 1;
            else
                pair->second++;
        }
    }
    if(resolveFailures.size()!=0 && info.compileInfo->errors==0 && info.errors==0){
        log::out << log::RED << "Invalid function resolutions:\n";
        for(auto& pair : resolveFailures){
            info.errors += pair.second;
            log::out << log::RED << " "<<pair.first->name <<": "<<pair.second<<" bad address(es)\n";
        }
    }
    info.callsToResolve.cleanup();

    // compiler logs the error count, dont need it here too
    // if(info.errors)
    //     log::out << log::RED<<"Generator failed with "<<info.errors<<" error(s)\n";
    info.compileInfo->errors += info.errors;
    return info.code;
}