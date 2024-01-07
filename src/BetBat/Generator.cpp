#include "BetBat/Generator.h"
#include "BetBat/Compiler.h"

#undef ERRTYPE
#undef ERRTYPE1
#define ERRTYPE(L, R, LT, RT, M) ERR_SECTION(ERR_HEAD(L, ERROR_TYPE_MISMATCH) ERR_MSG("Type mismatch " << info.ast->typeToString(LT) << " - " << info.ast->typeToString(RT) << " " << M) ERR_LINE(L,info.ast->typeToString(LT)) ERR_LINE(R,info.ast->typeToString(RT)))
#define ERRTYPE1(R, LT, RT, M) ERR_SECTION(ERR_HEAD(R, ERROR_TYPE_MISMATCH) ERR_MSG("Type mismatch " << info.ast->typeToString(LT) << " - " << info.ast->typeToString(RT) << " " << M) ERR_LINE(R,"expects "+info.ast->typeToString(RT)))

#undef WARN_HEAD3
#define WARN_HEAD3(R, M) info.compileInfo->warnings++;engone::log::out << WARN_DEFAULT_R(R,"Gen. warning","W0000") << M

#undef ERR_SECTION
#define ERR_SECTION(CONTENT) BASE_SECTION("Generator, ", CONTENT)

#undef LOGAT
#define LOGAT(R) R.firstToken.line << ":" << R.firstToken.column


#define MAKE_NODE_SCOPE(X) info.pushNode(dynamic_cast<ASTNode*>(X));NodeScope nodeScope{&info};

/* #region  */
GenInfo::LoopScope* GenInfo::pushLoop(){
    LoopScope* ptr = TRACK_ALLOC(LoopScope);
    new(ptr) LoopScope();
    if(!ptr)
        return nullptr;
    loopScopes.add(ptr);
    return ptr;
}
GenInfo::LoopScope* GenInfo::getLoop(int index){
    if(index < 0 || index >= (int)loopScopes.size()) return nullptr;
    return loopScopes[index];
}
bool GenInfo::popLoop(){
    if(loopScopes.size()==0)
        return false;
    LoopScope* ptr = loopScopes.last();
    ptr->~LoopScope();
    // engone::Free(ptr, sizeof(LoopScope));
    TRACK_FREE(ptr, LoopScope);
    loopScopes.pop();
    return true;
}
void GenInfo::pushNode(ASTNode* node){
    nodeStack.add(node);
}
void GenInfo::popNode(){
    nodeStack.pop();
}
void GenInfo::addCallToResolve(int bcIndex, FuncImpl* funcImpl){
    if(disableCodeGeneration) return;
    // if(bcIndex == 97) {
    //     __debugbreak();
    // }
    ResolveCall tmp{};
    tmp.bcIndex = bcIndex;
    tmp.funcImpl = funcImpl;
    callsToResolve.add(tmp);
}
void GenInfo::addCall(LinkConventions linkConvention, CallConventions callConvention){
    addInstruction({BC_CALL, (u8)linkConvention, (u8)callConvention},true);
}
void GenInfo::popInstructions(u32 count){
    Assert(code->length() >= count);

    for(int i=0;i<count;i++) {
        u32 index = indexOfNonImmediates.last();
        Assert(index == code->instructionSegment.used-1); // We can't pop instructions with immediates. We need more complexity here to do that.
        Instruction inst = code->instructionSegment.data[index];
        
        // IMPORTANT: THERE IS A HIGH CHANCE OF A BUG HERE WITH VIRTUAL STACK POINTER.
        switch(inst.opcode) {
            case BCInstruction::BC_PUSH: {
                int size = 8; // fixed size
                WHILE_TRUE {
                    if(!hasErrors()){
                        Assert(("bug in compiler!", stackAlignment.size()!=0));
                    }
                    if(stackAlignment.size()!=0){
                        auto align = stackAlignment.last();
                        if(!hasErrors() && align.size!=0){
                            // size of 0 could mean extra alignment for between structs
                            Assert(("bug in compiler!", align.size == size));
                        }
                        // You pushed some size and tried to pop a different size.
                        // Did you copy paste some code involving addPop/addPush recently?
                        stackAlignment.pop();
                        if (align.diff != 0) {
                            virtualStackPointer += align.diff;
                            // code->addDebugText("align sp\n");
                            i16 offset = align.diff;
                            // addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
                        }
                        if(align.size == 0)
                            continue;
                    }
                    virtualStackPointer += size;
                    break;
                }
                break;   
            }
            // case BCInstruction::BC_POP: {
                
            //     break;
            // }
            default: {
                // We can't pop instructions that modify stack or base pointer because we need to revert virtualStackPointer and such.
                // That can get quite complex.
                Assert(false);
                break;   
            }
        }
        
        code->instructionSegment.used-=count;
        indexOfNonImmediates.used-=count;
    }

    code->instructionSegment.used-=count;
    indexOfNonImmediates.used-=count;
    if(code->instructionSegment.used>0 && count != 0){
        for(int i = code->instructionSegment.used-1;i>=0;i--){
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
}
bool GenInfo::addInstruction(Instruction inst, bool bypassAsserts){
    using namespace engone;

    // Assert(inst.op0 == CAST_SINT_SINT);

    // This is here to prevent mistakes.
    // the stack is kept track of behind the scenes with info.addPush, info.addPop.
    // using info.addInstruction would bypass that which would cause bugs.
    if(!bypassAsserts){
        Assert((inst.opcode!=BC_POP && inst.opcode!=BC_PUSH && inst.opcode != BC_CALL && inst.opcode != BC_LI));
    }

    if(inst.opcode == BC_MOV_MR || inst.opcode == BC_MOV_RM || inst.opcode == BC_MOV_MR_DISP32 || inst.opcode == BC_MOV_RM_DISP32) {
        Assert(inst.op2 != 0);
    }

    if(code->length() == 693) {
        int k = 923;
    }
    
    if(disableCodeGeneration) return true;

    #ifdef OPTIMIZED
    /*
        Some of this code is kind of neat. It has a cascading effect where
            li eax, push eax, pop ecx   becomes
            li eax, mov eax, ecx        but then again
            li ecx
    */
    // IMPORTANT: THE CODE DOESN'T EXPECT IMMEDIATES.
    //  There should be a variable which can tell us the previous bytes are an immediate and if so
    //  where the real instruction is.
    //  I have disabled some optimizations to prevent this bug from happening untill I fix it..
    if(indexOfNonImmediates.size()>0){
        int index = indexOfNonImmediates.last();
        Instruction instLast = code->get(index);
        if(inst.opcode == BC_POP && instLast.opcode == BC_PUSH
        && inst.op0 == instLast.op0) {
            popInstructions(1);
            _GLOG(log::out << log::GRAY << "(delete redundant push/pop)\n";)
            return false;
        }
        if(inst.opcode == BC_POP && instLast.opcode == BC_PUSH
        && DECODE_REG_SIZE(inst.op0) == DECODE_REG_SIZE(instLast.op0)) {
            u8 op = instLast.op0;
            popInstructions(1);
            _GLOG(log::out << log::GRAY << "(delete redundant push/pop)\n";)
            bool yes = addInstruction({BC_MOV_RR, op, inst.op0});
            if(yes) {
                _GLOG(log::out << log::GRAY << "(replaced with register move)\n";)
            }
            return false;
        }
        // if(inst.opcode == BC_MOV_RR && inst.op0 == instLast.op0) {
        //     u8 opcode = instLast.opcode;
        //     u8* outOp = nullptr;
        //     switch(opcode) {
        //         // add, mul and other instructions don't work
        //         // because output registers are also input registers
        //         case BC_DATAPTR:
        //         case BC_CODEPTR:
        //         case BC_LI: {
        //             outOp = &instLast.op0;
        //             break;
        //         }
        //         case BC_MOV_MR:
        //         case BC_MOV_MR_DISP32: {
        //             outOp = &instLast.op1;
        //             break;
        //         }
        //     }
        //     if(outOp) {
        //         *outOp = inst.op1;
        //         _GLOG(log::out << log::GRAY << "(modified instruction)\n";)
        //         _GLOG(code->printInstruction(index,true);)
        //         return false;
        //     }
        // }
    }
    #endif

    if(!hasErrors()) {
        // FP (base pointer in x64) is callee-saved
        Assert(inst.opcode != BC_RET || (code->length()>0 &&
            code->get(code->length()-1).opcode == BC_POP &&
            code->get(code->length()-1).op0 == BC_REG_FP));
    }
    indexOfNonImmediates.add(code->length());
    code->add_notabug(inst);
    // Assert(nodeStack.size()!=0); // can't assert because some instructions are added which doesn't link to any AST node.
    if(nodeStack.size()==0)
        return true;
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
    return true;
}
void GenInfo::addLoadIm(u8 reg, i32 value){
    addInstruction({BC_LI, reg}, true);
    if(disableCodeGeneration) return;
    code->addIm_notabug(value);
}
void GenInfo::addLoadIm2(u8 reg, i64 value){
    addInstruction({BC_LI, reg, 2}, true);
    if(disableCodeGeneration) return;
    code->addIm_notabug(value&0xFFFFFFFF);
    code->addIm_notabug(value>>32);
}
void GenInfo::addImm(i32 value){
    if(disableCodeGeneration) return;
    code->addIm_notabug(value);
}
void GenInfo::addPop(int reg) {
    using namespace engone;
    int size = DECODE_REG_SIZE(reg);
    if (size == 0 ) { // we don't print if we had errors since they probably caused size of 0
        if(!hasErrors()){
            Assert(("register had 0 zero",false));
            log::out << log::RED << "GenInfo::addPop : Cannot pop register with 0 size\n";
        }
        return;
    }
    size = 8; // NOTE: function popInstructions assume 8 in size
    reg = RegBySize(DECODE_REG_TYPE(reg), size); // force 8 bytes

    addInstruction({BC_POP, (u8)reg}, true);
    // if errors != 0 then don't assert and just return since the stack
    // is probably messed up because of the errors. OR you try
    // to manage the stack even with errors. Unnecessary work though so don't!? 
    WHILE_TRUE {
        if(!hasErrors()){
            Assert(("bug in compiler!", stackAlignment.size()!=0));
        }
        if(stackAlignment.size()!=0){
            auto align = stackAlignment.last();
            if(!hasErrors() && align.size!=0){
                // size of 0 could mean extra alignment for between structs
                Assert(("bug in compiler!", align.size == size));
            }
            // You pushed some size and tried to pop a different size.
            // Did you copy paste some code involving addPop/addPush recently?
            stackAlignment.pop();
            if (align.diff != 0) {
                virtualStackPointer += align.diff;
                // code->addDebugText("align sp\n");
                i16 offset = align.diff;
                addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
            }
            if(align.size == 0)
                continue;
        }
        virtualStackPointer += size;
        break;
    }
    _GLOG(log::out << "relsp "<<virtualStackPointer<<"\n";)
}
void GenInfo::addPush(int reg, bool withoutInstruction) {
    using namespace engone;
    // Assert(!withoutInstruction);
    // NOTE: When optimizing, we mess with the virtual stack pointer because we remove unnecessary pop/push instructions.
    // I Don't know if withoutInstruction would cause bugs. - Emarioo, 2023-12-19
    
    int size = DECODE_REG_SIZE(reg);
    if (size == 0 ) { // we don't print if we had errors since they probably caused size of 0
        if(errors == 0){
            log::out << log::RED << "GenInfo::addPush : Cannot push register with 0 size\n";
        }
        return;
    }
    size = 8;
    reg = RegBySize(DECODE_REG_TYPE(reg), 8); // force 8 bytes

    int diff = (size - (-virtualStackPointer) % size) % size; // how much to increment sp by to align it
    // TODO: Instructions are generated from top-down and the stackAlignment
    //   sees pop and push in this way but how about jumps. It doesn't consider this. Is it an issue?
    if (diff) {
        Assert(false); // shouldn't happen anymore
        virtualStackPointer -= diff;
        // code->addDebugText("align sp\n");
        i16 offset = -diff;
        addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
    }
    if(!withoutInstruction) {
        addInstruction({BC_PUSH, (u8)reg}, true);
    }
    stackAlignment.add({diff, size});
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
            auto align = stackAlignment.last();
            // log::out << "pop stackalign "<<align.diff<<":"<<align.size<<"\n";
            stackAlignment.pop();
            at -= align.size;
            // Assert(at >= 0);
            // Assert doesn't work because diff isn't accounted for in offset.
            // Asserting before at -= diff might not work either.

            at -= align.diff;
        }
    }
    else if (offset < 0) {
        stackAlignment.add({0, -offset});
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
        stackAlignment.add({diff, -size});
        virtualStackPointer += size;
    } else if(size > 0) {
        addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & size), (u8)(size >> 8)});

        // code->add({BC_POP, (u8)reg});
        Assert(("bug in compiler!", stackAlignment.size()!=0));
        auto align = stackAlignment.last();
        Assert(("bug in compiler!", align.size == size));
        // You pushed some size and tried to pop a different size.
        // Did you copy paste some code involving addPop/addPush recently?

        stackAlignment.pop();
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

void GenInfo::restoreStackMoment(int moment, bool withoutModification, bool withoutInstruction) {
    using namespace engone;
    int offset = moment - virtualStackPointer;
    if (offset == 0)
        return;
    if(!withoutModification || withoutInstruction) {
        int at = moment - virtualStackPointer;
        while (at > 0 && stackAlignment.size() > 0) {
            auto align = stackAlignment.last();
            // log::out << "pop stackalign "<<align.diff<<":"<<align.size<<"\n";
            stackAlignment.pop();
            at -= align.size;
            at -= align.diff;
            if(!hasErrors()) {
                Assert(at >= 0);
            }
        }
        virtualStackPointer = moment;
    } 
    // else {
    //     _GLOG(log::out << "relsp "<<moment<<"\n";)
    // }
    if(!withoutInstruction){
        addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
    }
    if(withoutModification) {
        _GLOG(log::out << "relsp (temp) "<<moment<<"\n";)
    } else {
        _GLOG(log::out << "relsp "<<moment<<"\n";)
    }
}
/* #endregion */
// Will perform cast on float and integers with pop, cast, push
// uses register A
// TODO: handle struct cast?
bool PerformSafeCast(GenInfo &info, TypeId from, TypeId to) {
    if(!from.isValid() || !to.isValid())
        return false;
    if (from == to)
        return true;
    if(from.isPointer()) {
        if (to == AST_BOOL)
            return true;
        if ((to.baseType() == AST_UINT64 || to.baseType() == AST_INT64) && 
            from.getPointerLevel() - to.getPointerLevel() == 1)
            return true;
        // if(to.baseType() == AST_VOID && from.getPointerLevel() == to.getPointerLevel())
        //     return true;
        if(to.baseType() == AST_VOID && to.getPointerLevel() > 0)
            return true;
    }
    if(to.isPointer()) {
        if ((from.baseType() == AST_UINT64 || from.baseType() == AST_INT64) && 
            to.getPointerLevel() - from.getPointerLevel() == 1)
            return true;
        // if(from.baseType() == AST_VOID && from.getPointerLevel() == to.getPointerLevel())
        //     return true;
        if(from.baseType() == AST_VOID && from.getPointerLevel() > 0)
            return true;
    }

    // TODO: I just threw in currentScopeId. Not sure if it is supposed to be in all cases.
    // auto fti = info.ast->getTypeInfo(from);
    // auto tti = info.ast->getTypeInfo(to);
    auto from_size = info.ast->getTypeSize(from);
    auto to_size = info.ast->getTypeSize(to);
    // u8 reg0 = RegBySize(BC_AX, fti->size());
    // u8 reg1 = RegBySize(BC_AX, tti->size());
    // IMPORTANT: DO NOT!!!!!! USE ANY OTHER REGISTER THAN A!!!!!!
    // OTHER REGISTERS MAY BE IN USE I THINK?
    u8 reg0 = RegBySize(BC_AX, from_size);
    u8 reg1 = RegBySize(BC_AX, to_size);
    if (AST::IsDecimal(from) && AST::IsInteger(to)) {
        info.addPop(reg0);
        // if(AST::IsSigned(to))
        info.addInstruction({BC_CAST, CAST_FLOAT_SINT, reg0, reg1});
            // info.addInstruction({BC_CAST, CAST_FLOAT_UINT, reg0, reg1});
        info.addPush(reg1);
        return true;
    }
    if (AST::IsInteger(from) && AST::IsDecimal(to)) {
        info.addPop(reg0);
        if(AST::IsSigned(from))
            info.addInstruction({BC_CAST, CAST_SINT_FLOAT, reg0, reg1});
        else
            info.addInstruction({BC_CAST, CAST_UINT_FLOAT, reg0, reg1});
        info.addPush(reg1);
        return true;
    }
    if (AST::IsDecimal(from) && AST::IsDecimal(to)) {
        info.addPop(reg0);
        info.addInstruction({BC_CAST, CAST_FLOAT_FLOAT, reg0, reg1});
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
        if (!AST::IsSigned(from) && !AST::IsSigned(to))
            info.addInstruction({BC_CAST, CAST_SINT_UINT, reg0, reg1});
        info.addPush(reg1);
        return true;
    }
    auto from_typeInfo = info.ast->getTypeInfo(from);
    // int to_size = info.ast->getTypeSize(to);
    if(from_typeInfo && from_typeInfo->astEnum && AST::IsInteger(to)) {
        // TODO: Print an error that says big enums can't be casted to small integers.
        if(to_size >= from_typeInfo->_size) {
            info.addPop(reg0);
            info.addPush(reg1);
            return true;
        }
    }
    // Asserts when we have missed a conversion
    if(!info.hasForeignErrors()){
        std::string l = info.ast->typeToString(from);
        std::string r = info.ast->typeToString(to);
        Assert(!info.ast->castable(from,to));
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
BCInstruction ASTOpToBytecode(TypeId astOp, bool floatVersion){
    
// #define CASE(X, Y) case X: return Y;
#define CASE(X, Y) else if(op == X) return Y;
    OperationType op = (OperationType)astOp.getId();
    if (floatVersion) {
        // switch((op){
            if(false) ;
            CASE(AST_ADD, BC_FADD)
            CASE(AST_SUB, BC_FSUB)
            CASE(AST_MUL, BC_FMUL)
            CASE(AST_DIV, BC_FDIV)
            CASE(AST_MODULUS, BC_FMOD)

            CASE(AST_LESS, BC_FLT)
            CASE(AST_LESS_EQUAL, BC_FLTE)
            CASE(AST_GREATER, BC_FGT)
            CASE(AST_GREATER_EQUAL, BC_FGTE)
            CASE(AST_EQUAL, BC_FEQ)
            CASE(AST_NOT_EQUAL, BC_FNEQ)
        // }
        // TODO: do OpToStr first and then if it fails print the typeId number
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
    return (BCInstruction)0;
}
SignalDefault GeneratePushFromValues(GenInfo& info, u8 baseReg, int baseOffset, TypeId typeId, int* movingOffset = nullptr){
    using namespace engone;
    Assert(baseReg!=BC_REG_RCX);
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = info.ast->getTypeInfo(typeId);
    // u32 size = info.ast->getTypeSize(typeId);
    u32 size = 8;
    int _movingOffset = baseOffset;
    if(!movingOffset)
        movingOffset = &_movingOffset;
    
    if(!typeInfo || !typeInfo->astStruct) {
        // enum works here too
        u8 reg = RegBySize(BC_AX, size);
        if(*movingOffset == 0){
            // If you are here to optimize some instructions then you are out of luck.
            // I checked where GeneratePush is used whether ADDI can LI can be removed and
            // replaced with a MOV_MR_DISP32 but those instructions come from GenerateReference.
            // What you need is a system to optimise away instructions while adding them (like pop after push)
            // or an optimizer which runs after the generator.
            // You need something more sophisticated to optimize further basically.
            info.addInstruction({BC_MOV_MR, baseReg, reg, (u8)size});
        }else{
            info.addInstruction({BC_MOV_MR_DISP32, baseReg, reg, (u8)size});
            info.addImm(*movingOffset);
        }
        info.addPush(reg);
        *movingOffset += size;
    } else {
        for(int i = (int) typeInfo->astStruct->members.size() - 1; i>=0; i--){
            auto& member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            
            _GLOG(log::out << "push " << member.name << "\n";)
            GeneratePushFromValues(info, baseReg, baseOffset, memdata.typeId, movingOffset);
            *movingOffset += size;
        }
    }
    return SignalDefault::SUCCESS;
}
SignalDefault GenerateArtificialPush(GenInfo& info, TypeId typeId) {
    using namespace engone;
    if(typeId == AST_VOID) {
        return SignalDefault::FAILURE;
    }
    // if(baseReg!=0) {
    //     Assert(DECODE_REG_SIZE(baseReg) == 8 && DECODE_REG_TYPE(baseReg) != BC_AX);
    // }
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = info.ast->getTypeInfo(typeId);
    u32 size = info.ast->getTypeSize(typeId);
    if(!typeInfo || !typeInfo->astStruct) {
        // enum works here too
        u8 reg = RegBySize(BC_AX, size);
        // if(offset == 0){
        //     // If you are here to optimize some instructions then you are out of luck.
        //     // I checked where GeneratePush is used whether ADDI can LI can be removed and
        //     // replaced with a MOV_MR_DISP32 but those instructions come from GenerateReference.
        //     // What you need is a system to optimise away instructions while adding them (like pop after push)
        //     // or an optimizer which runs after the generator.
        //     // You need something more sophisticated to optimize further basically.
        //     info.addInstruction({BC_MOV_MR, baseReg, reg, (u8)size});
        // }else{
        //     // IMPORTANT: Understand the issue before changing code here. We always use
        //     // disp32 because the converter asserts when using frame pointer.
        //     // "Use addModRM_disp32 instead". We always use disp32 to avoid the assert.
        //     // Well, the assert occurs anyway.
        //     info.addInstruction({BC_MOV_MR_DISP32, baseReg, reg, (u8)size});
        //     info.addImm(offset);
        //     }
        // }
        info.addPush(reg, true);
    } else {
        for(int i = (int) typeInfo->astStruct->members.size() - 1; i>=0; i--){
            auto& member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            
            _GLOG(log::out << "push " << member.name << "\n";)
            GenerateArtificialPush(info, memdata.typeId);
        }
    }
    return SignalDefault::SUCCESS;
}
// push of structs
SignalDefault GeneratePush(GenInfo& info, u8 baseReg, int offset, TypeId typeId){
    using namespace engone;
    if(typeId == AST_VOID) {
        return SignalDefault::FAILURE;
    }
    if(baseReg!=0) {
        Assert(DECODE_REG_SIZE(baseReg) == 8 && DECODE_REG_TYPE(baseReg) != BC_AX);
    }
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
            // IMPORTANT: Understand the issue before changing code here. We always use
            // disp32 because the converter asserts when using frame pointer.
            // "Use addModRM_disp32 instead". We always use disp32 to avoid the assert.
            // Well, the assert occurs anyway.
            info.addInstruction({BC_MOV_MR_DISP32, baseReg, reg, (u8)size});
            info.addImm(offset);
        }
        info.addPush(reg);
    } else {
        for(int i = (int) typeInfo->astStruct->members.size() - 1; i>=0; i--){
            auto& member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            
            _GLOG(log::out << "push " << member.name << "\n";)
            GeneratePush(info, baseReg, offset + memdata.offset, memdata.typeId);
        }
    }
    return SignalDefault::SUCCESS;
}
SignalDefault GeneratePreload(GenInfo& info);
SignalDefault GenerateExpression(GenInfo &info, ASTExpression *expression, DynamicArray<TypeId> *outTypeIds, ScopeId idScope = -1);
SignalDefault GenerateExpression(GenInfo &info, ASTExpression *expression, TypeId *outTypeId, ScopeId idScope = -1);
// pop of structs
SignalDefault GeneratePop(GenInfo& info, u8 baseReg, int offset, TypeId typeId){
    using namespace engone;
    // Assert(baseReg!=BC_REG_RCX);
    if(baseReg!=0) {
        Assert(DECODE_REG_SIZE(baseReg) == 8 && DECODE_REG_TYPE(baseReg) != BC_AX);
    }
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = info.ast->getTypeInfo(typeId);
    u8 size = info.ast->getTypeSize(typeId);
    if(size == 0) {
        Assert(info.hasForeignErrors());
        return SignalDefault::FAILURE;
    }
    if (!typeInfo || !typeInfo->astStruct) {
        _GLOG(log::out << "move return value\n";)
        u8 reg = RegBySize(BC_AX, size);
        info.addPop(reg);
        if(baseReg==0) {
            // 0 as register will just pop and ignore the value.
            // We want this when an error occurs and we need to pop disallowed types.
        } else {
            if(offset == 0){
                // The x64 architecture has a special meaning when using fp.
                // The converter asserts about using addModRM_disp32 instead. Instead of changing
                // it to allow this instruction we always use disp32 to avoid the complaint.
                // This instruction fires the assert: BC_MOV_RM rax, fp 
                info.addInstruction({BC_MOV_RM, reg, baseReg, size});
            }else{
                info.addInstruction({BC_MOV_RM_DISP32, reg, baseReg, size});
                info.addImm(offset);
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
    return SignalDefault::SUCCESS;
}
void GenMemzero(GenInfo& info, u8 ptr_reg, u8 size_reg, int size) {
    if(size <= 8) {
        Assert(size == 1 || size == 2 || size == 4 || size == 8);
        info.addInstruction({BC_BXOR, size_reg, size_reg, size_reg});
        info.addInstruction({BC_MOV_RM,size_reg,ptr_reg,(u8)size});
    } else {
        u8 batch = 1;
        if(size % 8 == 0)
            batch = 8;
        else if(size % 4 == 0)
            batch = 4;
        else if(size % 2 == 0)
            batch = 2;
        // size = size / batch; // don't do this, see how memzero is converted
        info.addLoadIm(size_reg, size);
        info.addInstruction({BC_MEMZERO, ptr_reg, size_reg, batch});
    }
    // info.addLoadIm(size_reg, size);
    // info.addInstruction({BC_MEMZERO, ptr_reg, size_reg, 1});
}
// baseReg as 0 will push default values to stack
// non-zero as baseReg will mov default values to the pointer in baseReg
SignalDefault GenerateDefaultValue(GenInfo &info, u8 baseReg, int offset, TypeId typeId, TokenRange* tokenRange = nullptr, bool zeroInitialize=true) {
    using namespace engone;
    MEASURE;
    if(baseReg!=0) {
        Assert(DECODE_REG_SIZE(baseReg) == 8);
    }
    // MAKE_NODE_SCOPE(_expression);
    // Assert(typeInfo)
    TypeInfo* typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = info.ast->getTypeInfo(typeId);
    u8 size = info.ast->getTypeSize(typeId);
    if(baseReg != 0) {
        Assert(baseReg != BC_REG_RBX); // is used with BC_MEMZERO, you would have to push baseReg and then pop after BC_MEMZERO to maintain it.
        // That is inefficient though so it's better to use a different register all together.
        #ifndef DISABLE_ZERO_INITIALIZATION
        if(baseReg != BC_REG_RDI){
            info.addLoadIm(BC_REG_RDI, offset);
            info.addInstruction({BC_ADDI, baseReg, BC_REG_RDI, BC_REG_RDI});
        } else {
            info.addLoadIm(BC_REG_RBX, offset);
            info.addInstruction({BC_ADDI, baseReg, BC_REG_RBX, BC_REG_RDI});
        }
        GenMemzero(info, BC_REG_RDI, BC_REG_RBX, size);
        // info.addLoadIm(BC_REG_RBX, size);
        // info.addInstruction({BC_MEMZERO, BC_REG_RDI, BC_REG_RBX});
        #endif
    }
    if (typeInfo && typeInfo->astStruct) {
        for (int i = typeInfo->astStruct->members.size() - 1; i >= 0; i--) {
            auto &member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            // log::out << "GEN "<<typeInfo->astStruct->name<<"."<<member.name<<"\n";
            // log::out << " alignedSize "<<info.ast->getTypeAlignedSize(memdata.typeId)<<"\n";
            // if(i+1<(int)typeInfo->astStruct->members.size()){
            //     // structs in structs will be aligned by their individual members
            //     // instead of the alignment of the structs as a whole.
            //     // This will make sure the structs are aligned.
            //     auto prevMem = typeInfo->getMember(i+1);
            //     u32 alignSize = info.ast->getTypeAlignedSize(prevMem.typeId);
            //     // log::out << "Try align "<<alignSize<<"\n";
            //     // info.addAlign(alignSize);
            // }

            if (member.defaultValue) {
                // TypeId tempTypeId = {};
                DynamicArray<TypeId> tempTypes{};
                SignalDefault result = GenerateExpression(info, member.defaultValue, &tempTypes);
                
                if(tempTypes.size() == 1){
                    if (!PerformSafeCast(info, tempTypes[0], memdata.typeId)) {
                        ERRTYPE(member.name, member.defaultValue->tokenRange, tempTypes[0], memdata.typeId, "(default member)\n");
                    }
                    if(baseReg!=0){
                        SignalDefault result = GeneratePop(info, baseReg, offset + memdata.offset, memdata.typeId);
                    }
                } else {
                    StringBuilder values = {};
                    FORN(tempTypes) {
                        auto& it = tempTypes[nr];
                        if(nr != 0)
                            values += ", ";
                        values += info.ast->typeToString(it);
                    }
                    ERR_SECTION(
                        ERR_HEAD(member.defaultValue->tokenRange)
                        ERR_MSG("Default value of member produces more than one value but only one is allowed.")
                        ERR_LINE(member.defaultValue->tokenRange, values.data())
                    )
                }
            } else {
                if(!memdata.typeId.isValid() || memdata.typeId == AST_VOID) {
                    Assert(info.hasForeignErrors());
                } else {
                    SignalDefault result = GenerateDefaultValue(info, baseReg, offset + memdata.offset, memdata.typeId, tokenRange, false);
                }
            }
        }
    } else {
        Assert(size <= 8);
        #ifndef DISABLE_ZERO_INITIALIZATION
        // only structs have default values, otherwise zero is the default
        info.addInstruction({BC_BXOR, BC_REG_RAX, BC_REG_RAX, BC_REG_RAX});
        if(baseReg == 0){
            info.addPush(BC_REG_RAX);
        } else {
            u8 reg = RegBySize(BC_AX,size);
            info.addInstruction({BC_MOV_RM_DISP32,reg,baseReg,size});
            info.addImm(offset);
        }
        #else
        // Not setting zero here is certainly a bad idea
        if(baseReg == 0){
            info.addPush(BC_REG_RAX);
        }
        #endif
    }
    return SignalDefault::SUCCESS;
}
SignalDefault FramePush(GenInfo& info, TypeId typeId, i32* outFrameOffset, bool genDefault, bool staticData){
    Assert(outFrameOffset);
    Assert(!staticData); // This is unsafe if functions use the static data before FramePush makes space for it.
    // This usually result static offsets always pointing at the start of the data segment. Then when FramePush
    // runs those offsets would be valid but it's already to late.
    // I am leaveing the comment here in case you decide to use FramePush to handle static data in the future WHICH YOU REALLY SHOULDN'T!

    // IMPORTANT: This code has been copied to array assignment. Change that code
    //  when changing code here.
    i32 size = info.ast->getTypeSize(typeId);
    i32 asize = info.ast->getTypeAlignedSize(typeId);
    if(asize==0)
        return SignalDefault::FAILURE;
    
    if(staticData) {
        info.code->ensureAlignmentInData(asize);
        int offset = info.code->appendData(nullptr,size);
        
        info.addInstruction({BC_DATAPTR, BC_REG_RDI});
        info.addImm(offset);

        *outFrameOffset = offset;
        if(genDefault){
            SignalDefault result = GenerateDefaultValue(info, BC_REG_RDI, 0, typeId);
            if(result!=SignalDefault::SUCCESS)
                return SignalDefault::FAILURE;
        }
    } else {
        int sizeDiff = size % 8;
        if(sizeDiff != 0){
            size += 8 - sizeDiff;
        }
        asize = 8;
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
            SignalDefault result = GenerateDefaultValue(info, BC_REG_FP, info.currentFrameOffset, typeId);
            if(result!=SignalDefault::SUCCESS)
                return SignalDefault::FAILURE;
        }
    }
    return SignalDefault::SUCCESS;
}
/*
##################################
    Generation functions below
##################################
*/
SignalDefault GenerateExpression(GenInfo &info, ASTExpression *expression, TypeId *outTypeId, ScopeId idScope){
    DynamicArray<TypeId> types{};
    SignalDefault result = GenerateExpression(info,expression,&types,idScope);
    *outTypeId = AST_VOID;
    if(types.size()>1){
        ERR_SECTION(
            ERR_HEAD(expression->tokenRange)
            ERR_MSG("Returns multiple values but the way you use the expression only supports one (or none).")
            ERR_LINE(expression->tokenRange,"returns "<<types.size() << " values")
        )
    } else {
        if(types.size()==1)
            *outTypeId = types[0];
    }
    return result;
}
// outTypeId will represent the type (integer, struct...) but the value pushed on the stack will always
// be a pointer. EVEN when the outType isn't a pointer. It is an implicit extra level of indirection commonly
// used for assignment.
SignalDefault GenerateReference(GenInfo& info, ASTExpression* _expression, TypeId* outTypeId, ScopeId idScope = -1, bool* wasNonReference = nullptr){
    using namespace engone;
    MEASURE;
    _GLOG(FUNC_ENTER)
    Assert(_expression);
    MAKE_NODE_SCOPE(_expression);
    if(idScope==(ScopeId)-1)
        idScope = info.currentScopeId;
    *outTypeId = AST_VOID;

    DynamicArray<ASTExpression*> exprs;
    DynamicArray<TypeId> tempTypes;
    TypeId endType = {};
    bool pointerType=false;
    ASTExpression* next=_expression;
    while(next){
        ASTExpression* now = next;
        next = next->left;

        if(now->typeId == AST_ID) {
            // end point
            // auto id = info.ast->findIdentifier(idScope, now->name);
            auto id = now->identifier;
            if (!id || id->type != Identifier::VARIABLE) {
                if(!info.hasForeignErrors()){
                    ERR_SECTION(
                        ERR_HEAD(now->tokenRange)
                        ERR_MSG("'"<<now->tokenRange.firstToken << "' is undefined.")
                        ERR_LINE(now->tokenRange,"bad");
                    )
                }
                return SignalDefault::FAILURE;
            }
        
            auto varinfo = info.ast->getVariableByIdentifier(id);
            _GLOG(log::out << " expr var push " << now->name << "\n";)
            // TOKENINFO(now->tokenRange)
            // char buf[100];
            // int len = sprintf(buf,"  expr push %s",now->name->c_str());
            // info.code->addDebugText(buf,len);

            TypeInfo *typeInfo = 0;
            if(varinfo->versions_typeId[info.currentPolyVersion].isNormalType())
                typeInfo = info.ast->getTypeInfo(varinfo->versions_typeId[info.currentPolyVersion]);
            TypeId typeId = varinfo->versions_typeId[info.currentPolyVersion];
            
            switch(varinfo->type) {
                case VariableInfo::GLOBAL: {
                    info.addInstruction({BC_DATAPTR, BC_REG_RBX});
                    info.addImm(varinfo->versions_dataOffset[info.currentPolyVersion]);
                    break; 
                }
                case VariableInfo::LOCAL: {
                    info.addLoadIm(BC_REG_RBX, varinfo->versions_dataOffset[info.currentPolyVersion]);
                    info.addInstruction({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX});
                    break; 
                }
                case VariableInfo::MEMBER: {
                    // NOTE: Is member variable/argument always at this offset with all calling conventions?
                    Assert(info.currentFunction->callConvention == BETCALL);
                    info.addInstruction({BC_MOV_MR_DISP32, BC_REG_FP, BC_REG_RBX, 8});
                    info.addImm(GenInfo::FRAME_SIZE);
                    
                    info.addLoadIm(BC_REG_RAX, varinfo->versions_dataOffset[info.currentPolyVersion]);
                    info.addInstruction({BC_ADDI, BC_REG_RBX, BC_REG_RAX, BC_REG_RBX});
                    break;
                }
                default: {
                    Assert(false);
                }
            }
            info.addPush(BC_REG_RBX);

            endType = typeId;
            break;
        } else if(now->typeId == AST_MEMBER || now->typeId == AST_DEREF || now->typeId == AST_INDEX){
            // TODO: Refer is handled in GenerateExpression since refer will
            //   be combined with other operations. Refer on it's own isn't
            //   very useful and therefore not common so there is not
            //   benefit of handling refer here. Probably.
            exprs.add(now);
        } else {
            // end point
            tempTypes.resize(0);
            SignalDefault result = GenerateExpression(info, now, &tempTypes, idScope);
            if(result!=SignalDefault::SUCCESS || tempTypes.size()!=1){
                return SignalDefault::FAILURE;
            }
            if(!tempTypes.last().isPointer()){
                ERR_SECTION(
                    ERR_HEAD(now->tokenRange)
                    ERR_MSG("'"<<now->tokenRange<<
                    "' must be a reference to some memory. "
                    "A variable, member or expression resulting in a dereferenced pointer would be.")
                    ERR_LINE(now->tokenRange, "must be a reference")
                )
                return SignalDefault::FAILURE;
            }
            endType = tempTypes.last();
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
                ERR_SECTION(
                    ERR_HEAD(now->tokenRange)
                    ERR_MSG("'"<<info.ast->typeToString(endType)<<"' has to many levels of pointing. Can only access members of a single or non-pointer.")
                    ERR_LINE(now->tokenRange, "to pointy")
                )
                return SignalDefault::FAILURE;
            }
            TypeInfo* typeInfo = nullptr;
            typeInfo = info.ast->getTypeInfo(endType.baseType());
            if(!typeInfo || !typeInfo->astStruct){ // one level of pointer is okay.
                ERR_SECTION(
                    ERR_HEAD(now->tokenRange)
                    ERR_MSG("'"<<info.ast->typeToString(endType)<<"' is not a struct. Cannot access member.")
                    ERR_LINE(now->left->tokenRange, info.ast->typeToString(endType).c_str())
                )
                return SignalDefault::FAILURE;
            }

            auto memberData = typeInfo->getMember(now->name);
            if(memberData.index==-1){
                Assert(info.hasForeignErrors());
                // error should have been caught by type checker.
                // if it was then we just return error here.
                // don't want message printed twice.
                // ERR_SECTION(
                // ERR_HEAD(now->tokenRange, "'"<<now->name << "' is not a member of struct '" << info.ast->typeToString(endType) << "'. "
                //         "These are the members: ";
                //     for(int i=0;i<(int)typeInfo->astStruct->members.size();i++){
                //         if(i!=0)
                //             log::out << ", ";
                //         log::out << log::LIME << typeInfo->astStruct->members[i].name<<log::NO_COLOR<<": "<<info.ast->typeToString(typeInfo->getMember(i).typeId);
                //     }
                //     log::out <<"\n";
                //     log::out <<"\n";
                //     ERR_LINE(now->tokenRange, "not a member");
                // )
                return SignalDefault::FAILURE;
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
                ERR_SECTION(
                    ERR_HEAD(now->left->tokenRange)
                    ERR_MSG("type '"<<info.ast->typeToString(endType)<<"' is not a pointer.")
                    ERR_LINE(now->left->tokenRange,"must be a pointer")
                )
                return SignalDefault::FAILURE;
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
            FuncImpl* operatorImpl = nullptr;
            if(now->versions_overload._array.size()>0)
                operatorImpl = now->versions_overload[info.currentPolyVersion].funcImpl;
            Assert(("operator overloading when referencing not implemented",!operatorImpl));
            
            tempTypes.resize(0);
            SignalDefault result = GenerateExpression(info, now->right, &tempTypes);
            if (result != SignalDefault::SUCCESS && tempTypes.size() != 1)
                return SignalDefault::FAILURE;
            
            if(!AST::IsInteger(tempTypes.last())){
                std::string strtype = info.ast->typeToString(tempTypes.last());
                ERR_SECTION(
                    ERR_HEAD(now->right->tokenRange)
                    ERR_MSG("Index operator (array[23]) requires integer type in the inner expression. '"<<strtype<<"' is not an integer. (Operator overloading doesn't work here)")
                    ERR_LINE(now->right->tokenRange,strtype.c_str());
                )
                return SignalDefault::FAILURE;
            }

            if(pointerType){
                pointerType=false;
                endType.setPointerLevel(endType.getPointerLevel()-1);
                
                u32 typesize = info.ast->getTypeSize(endType);
                u32 rsize = info.ast->getTypeSize(tempTypes.last());
                u8 reg = RegBySize(BC_CX,rsize);
                info.addPop(reg); // integer
                info.addPop(BC_REG_RBX); // reference

                if(typesize>1){
                    info.addLoadIm(BC_REG_EAX, typesize);
                    info.addInstruction({BC_MULI, ARITHMETIC_SINT, reg, BC_REG_EAX});
                }
                info.addInstruction({BC_ADDI, BC_REG_RBX, BC_REG_EAX, BC_REG_RBX});

                info.addPush(BC_REG_RBX);
                continue;
            }

            if(!endType.isPointer()){
                if(!info.hasForeignErrors()){
                    std::string strtype = info.ast->typeToString(endType);
                    ERR_SECTION(
                        ERR_HEAD(now->left->tokenRange)
                        ERR_MSG("Index operator (array[23]) requires pointer type in the outer expression. '"<<strtype<<"' is not a pointer. (Operator overloading doesn't work here)")
                        ERR_LINE(now->left->tokenRange,strtype.c_str());
                    )
                }
                return SignalDefault::FAILURE;
            }

            endType.setPointerLevel(endType.getPointerLevel()-1);

            u32 typesize = info.ast->getTypeSize(endType);
            u32 rsize = info.ast->getTypeSize(tempTypes.last());
            u8 reg = RegBySize(BC_DX,rsize);
            info.addPop(reg); // integer
            info.addPop(BC_REG_RBX); // reference
            // dereference pointer to pointer
            info.addInstruction({BC_MOV_MR, BC_REG_RBX, BC_REG_RCX, 8});
            
            if(typesize>1){
                info.addLoadIm(BC_REG_EAX, typesize);
                info.addInstruction({BC_MULI, ARITHMETIC_SINT, reg, BC_REG_EAX});
            }
            info.addInstruction({BC_ADDI, BC_REG_RCX, BC_REG_EAX, BC_REG_RCX});

            info.addPush(BC_REG_RCX);
        }
    }
    if(pointerType && !wasNonReference){
        ERR_SECTION(
            ERR_HEAD(_expression->tokenRange)
            ERR_MSG("'"<<_expression->tokenRange<<
            "' must be a reference to some memory. "
            "A variable, member or expression resulting in a dereferenced pointer would work.")
            ERR_LINE(_expression->tokenRange, "must be a reference")
        )
        return SignalDefault::FAILURE;
    }
    if(wasNonReference)
        *wasNonReference = pointerType;
    *outTypeId = endType;
    return SignalDefault::SUCCESS;
}
SignalDefault GenerateFnCall(GenInfo& info, ASTExpression* expression, DynamicArray<TypeId>* outTypeIds, bool isOperator){
    using namespace engone;
    ASTFunction* astFunc = nullptr;
    FuncImpl* funcImpl = nullptr;
    {
        FnOverloads::Overload overload = expression->versions_overload[info.currentPolyVersion];
        astFunc = overload.astFunc;
        funcImpl = overload.funcImpl;
    }
    if(!info.hasForeignErrors()) {
        // not ok, type checker should have generated the right overload.
        Assert(astFunc && funcImpl);
    }
    if(!astFunc || !funcImpl) {
        return SignalDefault::FAILURE;
    }

    // overload comes from type checker
    if(isOperator) {
        _GLOG(log::out << "Operator overload: ";funcImpl->print(info.ast,nullptr);log::out << "\n";)
    } else {
        _GLOG(log::out << "Overload: ";funcImpl->print(info.ast,nullptr);log::out << "\n";)
    }
    TINY_ARRAY(ASTExpression*,fullArgs,5);
    // std::vector<ASTExpression*> fullArgs;
    fullArgs.resize(astFunc->arguments.size());
    
    if(isOperator) {
        fullArgs[0] = expression->left;
        fullArgs[1] = expression->right;
    } else {
        for (int i = 0; i < (int)expression->args.size();i++) {
            ASTExpression* arg = expression->args.get(i);
            Assert(arg);
            if(!arg->namedValue.str){
                if(expression->hasImplicitThis()) {
                    fullArgs[i+1] = arg;
                } else {
                    fullArgs[i] = arg;
                }
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
                        ERR_SECTION(
                            ERR_HEAD(arg->namedValue.range())
                            ERR_MSG( "A named argument cannot specify an occupied parameter.")
                            ERR_LINE(astFunc->tokenRange,"this overload")
                            ERR_LINE(fullArgs[argIndex]->tokenRange,"occupied")
                            ERR_LINE(arg->namedValue,"bad coder")
                        )
                    } else {
                        fullArgs[argIndex] = arg;
                    }
                } else{
                    ERR_SECTION(
                        ERR_HEAD(arg->tokenRange)
                        ERR_MSG_LOG("Named argument is not a parameter of '"; funcImpl->print(info.ast,astFunc); engone::log::out << "'\n")
                        ERR_LINE(arg->namedValue,"not valid")
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
    }

    // I don't think there is anything to fix in there because conventions
    // are really about assembly instructions which type checker has nothing to do with.
    // log::out << log::YELLOW << "Fix call conventions in type checker\n";
    int startSP = info.saveStackMoment();
    switch(astFunc->callConvention){
        case BETCALL: {
            // Assert(0 == (-info.virtualStackPointer) % 8); // should be aligned

            // info.addIncrSp(-funcImpl->argSize);
            
            int alignment = 16;
            int stackSpace = funcImpl->argSize;
            stackSpace += MISALIGNMENT(-info.virtualStackPointer + stackSpace,alignment);
            info.addIncrSp(-stackSpace);
            break;
        }
        case STDCALL: {
            for(int i=0;i<astFunc->arguments.size();i++){
                int size = info.ast->getTypeSize(funcImpl->argumentTypes[i].typeId);
                if(size>8){
                    // TODO: This should be moved to the type checker.
                    ERR_SECTION(
                        ERR_HEAD(expression->tokenRange)
                        ERR_MSG("Argument types cannot be larger than 8 bytes when using stdcall.")
                        ERR_LINE(expression->tokenRange, "bad")
                    )
                    return SignalDefault::FAILURE;
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
            break;
        }
        case UNIXCALL: {
            for(int i=0;i<astFunc->arguments.size();i++){
                int size = info.ast->getTypeSize(funcImpl->argumentTypes[i].typeId);
                if(size>8){
                    // TODO: This should be moved to the type checker.
                    ERR_SECTION(
                        ERR_HEAD(expression->tokenRange)
                        ERR_MSG("Argument types cannot be larger than 8 bytes when using unixcall.")
                        ERR_LINE(expression->tokenRange, "argument type is to large")
                    )
                    return SignalDefault::FAILURE;
                }
            }

            // TODO: I DON'T THINK UNIX CALL NEEDS 32-BYTE STACK SPACE
            // MAY NOT NEED TO BE ALIGNED TO 16 BYTES EITHER
            int alignment = 16;
            int stackSpace = 0;
            stackSpace += MISALIGNMENT(-info.virtualStackPointer + stackSpace,alignment);
            info.addIncrSp(-stackSpace);
            // int misalign = MISALIGNMENT(-info.virtualStackPointer + stackSpace,alignment);
            // info.addIncrSp(-misalign);
            break;
        }
        case CDECL_CONVENTION: {
            Assert(false); // @Incomplete
        }
        case INTRINSIC: {
            // do nothing
            break;
        }
        default: {
            Assert(false);
        }
    }

    int virtualArgumentSpace = info.virtualStackPointer;
    for(int i=0;i<(int)fullArgs.size();i++){
        auto arg = fullArgs[i];
        if(expression->hasImplicitThis() && i == 0) {
            // Make sure we actually have this stored before at the base pointer of
            // the current function.
            Assert(info.currentFunction && info.currentFunction->parentStruct);
            Assert(info.currentFunction->callConvention == BETCALL);
            // NOTE: Is member variable/argument always at this offset with all calling conventions?
            info.addInstruction({BC_MOV_MR_DISP32, BC_REG_FP, BC_REG_RBX, 8});
            info.addImm(GenInfo::FRAME_SIZE);
            info.addPush(BC_REG_RBX);
            continue;
        } else {
            Assert(arg);
        }
        
        TypeId argType = {};
        SignalDefault result = SignalDefault::FAILURE;
        if(expression->isMemberCall() && i == 0) {
            // method call and first argument which is 'this'
            bool nonReference = false;
            result = GenerateReference(info, arg, &argType, -1, &nonReference);
            if(result==SignalDefault::SUCCESS){
                if(!nonReference) {
                    if(!argType.isPointer()){
                        argType.setPointerLevel(1);
                    } else {
                        Assert(argType.getPointerLevel()==1);
                        info.addPop(BC_REG_RBX);
                        info.addInstruction({BC_MOV_MR, BC_REG_RBX, BC_REG_RAX, 8});
                        info.addPush(BC_REG_RAX);
                        // info.addInstruction({BC_MOV_MR, BC_REG_RBX, BC_REG_RBX, 8});
                        // info.addPush(BC_REG_RBX);
                    }
                }
            }
        } else {
            DynamicArray<TypeId> tempTypes{};
            result = GenerateExpression(info, arg, &tempTypes);
            if(tempTypes.size()>0) {
                TypeId argType = tempTypes[0];
                // log::out << "PUSH ARG "<<info.ast->typeToString(argType)<<"\n";
                bool wasSafelyCasted = PerformSafeCast(info,argType, funcImpl->argumentTypes[i].typeId);
                if(!wasSafelyCasted && !info.hasErrors()){
                    ERR_SECTION(
                        ERR_HEAD(arg->tokenRange)
                        ERR_MSG("Cannot cast argument of type " << info.ast->typeToString(argType) << " to " << info.ast->typeToString(funcImpl->argumentTypes[i].typeId) << ". This is the function: '"<<astFunc->name<<"'. (function may be an overloaded operator)")
                        ERR_LINE(arg->tokenRange,"bad argument")
                    )
                }
            } else {
                Assert(info.hasForeignErrors());
            }
                // Assert(wasSafelyCasted);
            // } else {
            //     StringBuilder values = {};
            //     FORN(tempTypes) {
            //         if(nr != 0)
            //             values += ", ";
            //         values += info.ast->typeToString(*it);
            //     }
            //     ERR_SECTION(
            //         ERR_HEAD(member.defaultValue->tokenRange)
            //         ERR_MSG("Default value of member produces more than one value but only one is allowed.")
            //         ERR_LINE(member.defaultValue->tokenRange, values.data())
            //     )
            // }
        }
    }
    auto callConvention = astFunc->callConvention;
    if(info.compileInfo->compileOptions->target == TARGET_BYTECODE &&
        (IS_IMPORT(astFunc->linkConvention))) {
        callConvention = BETCALL;
    }

    // operator overloads should always use BETCALL
    Assert(!isOperator || callConvention == BETCALL);

    switch (callConvention) {
        case INTRINSIC: {
            // TODO: You could do some special optimisations when using intrinsics.
            //  If the arguments are strictly variables or constants then you can use a mov instruction 
            //  instead messing with push and pop.
            if(funcImpl->name == "memcpy"){
                info.addPop(BC_REG_RBX);
                info.addPop(BC_REG_RSI);
                info.addPop(BC_REG_RDI);
                info.addInstruction({BC_MEMCPY, BC_REG_RDI, BC_REG_RSI, BC_REG_RBX});
            } else if(funcImpl->name == "strlen"){
                info.addPop(BC_REG_RSI);
                info.addInstruction({BC_STRLEN, BC_REG_RSI, BC_REG_RCX, BC_REG_RAX});
                info.addPush(BC_REG_ECX); // len
                outTypeIds->add(AST_UINT32);
            } else if(funcImpl->name == "memzero"){
                info.addPop(BC_REG_RBX); // ptr
                info.addPop(BC_REG_RDI);
                info.addInstruction({BC_MEMZERO, BC_REG_RDI, BC_REG_RBX});
            } else if(funcImpl->name == "rdtsc"){
                // no input
                info.addInstruction({BC_RDTSC, BC_REG_RAX, BC_REG_RDX});
                info.addPush(BC_REG_RAX); // timestamp counter
                
                outTypeIds->add(AST_UINT64);
            // } else if(funcImpl->name == "rdtscp"){
            //     info.addInstruction({BC_RDTSC, BC_REG_RAX, BC_REG_ECX, BC_REG_RDX});
            //     info.addPush(BC_REG_RAX); // timestamp counter
            //     info.addPush(BC_REG_ECX); // processor thing?
                
            //     outTypeIds->add(AST_UINT64);
            //     outTypeIds->add(AST_UINT32);
            } else if(funcImpl->name == "compare_swap"){
                info.addPop(BC_REG_EDX); // new
                info.addPop(BC_REG_EAX); // old
                info.addPop(BC_REG_RBX); // ptr
                info.addInstruction({BC_CMP_SWAP, BC_REG_RBX, BC_REG_EAX, BC_REG_EDX});
                info.addPush(BC_REG_AL);
                
                outTypeIds->add(AST_BOOL);
            } else if(funcImpl->name == "atomic_add"){
                info.addPop(BC_REG_EAX);
                info.addPop(BC_REG_RBX);
                info.addInstruction({BC_ATOMIC_ADD, BC_REG_RBX, BC_REG_EAX});
            } else if(funcImpl->name == "sqrt"){
                info.addPop(BC_REG_XMM0f);
                info.addInstruction({BC_SQRT, BC_REG_XMM0f});
                info.addPush(BC_REG_XMM0f);
                outTypeIds->add(AST_FLOAT32);
            } else if(funcImpl->name == "round"){
                info.addPop(BC_REG_XMM0f);
                info.addInstruction({BC_ROUND, BC_REG_XMM0f});
                info.addPush(BC_REG_XMM0f);
                outTypeIds->add(AST_FLOAT32);
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
                ERR_SECTION(
                    ERR_HEAD(expression->tokenRange)
                    ERR_MSG("'"<<funcImpl->name<<"' is not an intrinsic function.")
                    ERR_LINE(expression->tokenRange,"not an intrinsic")
                )
            }
            return SignalDefault::SUCCESS;
            break; 
        }
        case BETCALL: {
            // I have not implemented linkConvention because it's rare
            // that you need it for BETCALL.
            if(info.compileInfo->compileOptions->target != TARGET_BYTECODE) {
                Assert(astFunc->linkConvention == LinkConventions::NONE || astFunc->linkConvention == NATIVE);
            }
            // TODO: It would be more efficient to do GeneratePop right after the argument expressions are generated instead
            //  of pushing them to the stack and then popping them here. You would save some pushing and popping
            //  The only difficulty is handling all the calling conventions. stdcall requires some more thought
            //  it's arguments should be put in registers and those are probably used when generating expressions. 
            if(fullArgs.size() != 0){
                int baseOffset = virtualArgumentSpace - info.virtualStackPointer;
                // info.addLoadIm(BC_REG_RBX, virtualSP - info.virtualStackPointer);
                info.addInstruction({BC_MOV_RR, BC_REG_SP, BC_REG_RBX});
                for(int i=fullArgs.size()-1;i>=0;i--){
                    auto arg = fullArgs[i];
                    
                    // log::out << "POP ARG "<<info.ast->typeToString(funcImpl->argumentTypes[i].typeId)<<"\n";
                    GeneratePop(info,BC_REG_RBX, baseOffset + funcImpl->argumentTypes[i].offset, funcImpl->argumentTypes[i].typeId);
                    
                    // TODO: Use this instead?
                    // int baseOffset = virtualArgumentSpace - info.virtualStackPointer;
                    // GeneratePop(info,BC_REG_SP, baseOffset + funcImpl->argumentTypes[i].offset, funcImpl->argumentTypes[i].typeId);
                }
            }
            
            if(info.compileInfo->compileOptions->target == TARGET_BYTECODE &&
                (astFunc->linkConvention == IMPORT || astFunc->linkConvention == DLLIMPORT)) {
                info.addCall(NATIVE, astFunc->callConvention);
            } else {
                info.addCall(astFunc->linkConvention, astFunc->callConvention);
            }
            info.addCallToResolve(info.code->length(),funcImpl);
            info.addImm(999999999);

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
                info.addInstruction({BC_MOV_RR,BC_REG_SP,BC_REG_RBX});
                // sp is our correct value right here. DO NOT rearrange this instruction because
                // sp is very sensitive. You should rarely use stack pointer because of this but here we have to.
                
                // we restore AFTER we got the stack pointer because it resets alignments and arg offset
                // which we don't want to include when adding our offsets. Once again, it's complicated.
                info.restoreStackMoment(startSP); // PLEASE FOR GOODNESS SAKE DO NOT REFACTOR THIS RESTORE ELSE WHERE!
                
                for (int i = 0;i<(int)funcImpl->returnTypes.size(); i++) {
                    auto &ret = funcImpl->returnTypes[i];
                    TypeId typeId = ret.typeId;

                    GeneratePush(info, BC_REG_RBX, -GenInfo::FRAME_SIZE + ret.offset - funcImpl->returnSize, typeId);
                    outTypeIds->add(ret.typeId);
                }
            }
            break; 
        }
        case STDCALL: {
            if(fullArgs.size() > 4) {
                Assert(virtualArgumentSpace - info.virtualStackPointer == fullArgs.size()*8);
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
            const int normal_regs[4]{
                BC_REG_RCX,
                BC_REG_RDX,
                BC_REG_R8,
                BC_REG_R9,
            };
            const int float_regs[4] {
                BC_REG_XMM0d,
                BC_REG_XMM1d,
                BC_REG_XMM2d,
                BC_REG_XMM3d,
            };
            auto& argTypes = funcImpl->argumentTypes;
            for(int i=fullArgs.size()-1;i>=0;i--) {
                auto argType = argTypes[i].typeId;
                if(AST::IsDecimal(argType)) {
                    info.addPop(float_regs[i]);
                } else {
                    info.addPop(normal_regs[i]);
                }
            }

            // native function can be handled normally
            info.addCall(astFunc->linkConvention, astFunc->callConvention);
            if(astFunc->linkConvention == LinkConventions::IMPORT || astFunc->linkConvention == LinkConventions::DLLIMPORT
                || astFunc->linkConvention == LinkConventions::VARIMPORT){
                if(astFunc->linkConvention == DLLIMPORT){
                    info.addExternalRelocation("__imp_"+funcImpl->name, info.code->length());
                } else if(astFunc->linkConvention == VARIMPORT){
                    info.addExternalRelocation(funcImpl->name, info.code->length());
                } else {
                    info.addExternalRelocation(funcImpl->name, info.code->length());
                }
                info.addImm(info.code->externalRelocations.size()-1);
            } else {
                // linkin == native, none or export should be fine
                info.addCallToResolve(info.code->length(), funcImpl);
                info.addImm(999999);
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
                int size = info.ast->getTypeSize(ret.typeId);
                if(AST::IsDecimal(ret.typeId)) {
                    info.addPush(ENCODE_REG_BITS(BC_XMM0, SIZE_TO_BITS(size)));
                } else {
                    u8 reg = RegBySize(BC_AX, size);
                    info.addPush(reg);
                }
                outTypeIds->add(ret.typeId);
            }
            break;
        }
        case UNIXCALL: {
            // if(fullArgs.size() > 4) {
            //     Assert(virtualArgumentSpace - info.virtualStackPointer == fullArgs.size()*8);
            //     int argOffset = fullArgs.size()*8;
            //     info.addInstruction({BC_MOV_RR, BC_REG_SP, BC_REG_RBX});
            //     for(int i=fullArgs.size()-1;i>=4;i--){
            //         auto arg = fullArgs[i];
                    
            //         // log::out << "POP ARG "<<info.ast->typeToString(funcImpl->argumentTypes[i].typeId)<<"\n";
            //         // NOTE: funcImpl->argumentTypes[i].offset SHOULD NOT be used 8*i is correct
            //         u32 size = info.ast->getTypeSize(funcImpl->argumentTypes[i].typeId);
            //         GeneratePop(info,BC_REG_RBX, argOffset + i*8, funcImpl->argumentTypes[i].typeId);
            //     }
            // }
            // TODO IMPORTANT: Do we need to do something about the "red zone" (128 bytes below stack pointer).
            //   What does the red zone imply? No pushing, popping?
            auto& argTypes = funcImpl->argumentTypes;
            const int normal_regs[6]{
                BC_REG_RDI,
                BC_REG_RSI,
                BC_REG_RDX,
                BC_REG_RCX,
                BC_REG_R8,
                BC_REG_R9,
            };
            const int float_regs[8] {
                BC_REG_XMM0d,
                BC_REG_XMM1d,
                BC_REG_XMM2d,
                BC_REG_XMM3d,
                BC_REG_XMM4d,
                BC_REG_XMM5d,
                BC_REG_XMM6d,
                BC_REG_XMM7d,
            };
            int used_normals = 0;
            int used_floats = 0;
            // Because we need to pop arguments in reverse, we must first know how many
            // integer/pointer and float type arguments we have.
            for(int i=fullArgs.size()-1;i>=0;i--) {
                if(AST::IsDecimal(argTypes[i].typeId)) {
                    used_floats++;
                } else {
                    used_normals++;
                }
            }
            // Then, from the back, we can pop and place arguments in the right register.
            for(int i=fullArgs.size()-1;i>=0;i--) {
                if(AST::IsDecimal(argTypes[i].typeId)) {
                    info.addPop(float_regs[--used_floats]);
                } else {
                    info.addPop(normal_regs[--used_normals]);
                }
            }
            info.addCall(astFunc->linkConvention, astFunc->callConvention);
            if(astFunc->linkConvention == LinkConventions::IMPORT || astFunc->linkConvention == LinkConventions::DLLIMPORT
                || astFunc->linkConvention == LinkConventions::VARIMPORT){
                if(astFunc->linkConvention == DLLIMPORT){
                    info.addExternalRelocation(funcImpl->name, info.code->length());
                    // info.addExternalRelocation("__imp_"+funcImpl->name, info.code->length());
                } else if(astFunc->linkConvention == VARIMPORT){
                    info.addExternalRelocation(funcImpl->name, info.code->length());
                } else {
                    info.addExternalRelocation(funcImpl->name, info.code->length());
                }
                info.addImm(info.code->externalRelocations.size()-1);
            } else {
                // linkin == native, none or export should be fine
                info.addCallToResolve(info.code->length(), funcImpl);
                info.addImm(999999);
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
                int size = info.ast->getTypeSize(ret.typeId);
                if(AST::IsDecimal(ret.typeId)) {
                    info.addPush(ENCODE_REG_BITS(BC_XMM0, SIZE_TO_BITS(size)));
                } else {
                    u8 reg = RegBySize(BC_AX, size);
                    info.addPush(reg);
                }
                outTypeIds->add(ret.typeId);
            }
            break;
        }
        default: {
            Assert(false);
        }
    }
    return SignalDefault::SUCCESS;
}
SignalDefault GenerateExpression(GenInfo &info, ASTExpression *expression, DynamicArray<TypeId> *outTypeIds, ScopeId idScope) {
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
    //         ERR_SECTION(
// ERR_HEAD(expression->tokenRange,"Type "<<info.ast->getTokenFromTypeString(expression->castType) << " does not exist.\n";)
    //     }
    // }
    // castType = info.ast->ensureNonVirtualId(castType);

    if(expression->computeWhenPossible) {
        if(expression->constantValue){
            u32 startInstruction = info.code->length();

            int moment = info.saveStackMoment();
            int startVirual = info.virtualStackPointer;
            
            expression->computeWhenPossible = false; // temporarily disable to preven infinite loop
            SignalDefault result = GenerateExpression(info, expression, outTypeIds, idScope);
            expression->computeWhenPossible = true; // must enable since other polymorphic implementations want to compute too

            int endVirtual = info.virtualStackPointer;
            Assert(endVirtual < startVirual);

            info.restoreStackMoment(moment, false, true);

            u32 endInstruction = info.code->length();

            // TODO: Would it be better to put interpreter in GenInfo. It would be possible to
            //   reuse allocations the interpreter made.
            Interpreter interpreter{};
            interpreter.expectValuesOnStack = true;
            interpreter.silent = true;
            interpreter.executePart(info.code, startInstruction, endInstruction);
            log::out.flush();
            // interpreter.printRegisters();
            // this won't work with the stack
            info.popInstructions(endInstruction-startInstruction);

            if(!info.disableCodeGeneration) {
                info.code->ensureAlignmentInData(8);

                void* pushedValues = (void*)interpreter.sp;
                int pushedSize = -(endVirtual - startVirual);
                
                int offset = info.code->appendData(pushedValues, pushedSize);
                info.addInstruction({BC_DATAPTR, BC_REG_RBX});
                info.addImm(offset);
                for(int i=0;i<(int)outTypeIds->size();i++){
                    SignalDefault result = GeneratePushFromValues(info, BC_REG_RBX, 0, outTypeIds->get(i));
                }
            }
            
            interpreter.cleanup();
            return SignalDefault::SUCCESS;
        } else {
            ERR_SECTION(
                ERR_HEAD(expression->tokenRange)
                ERR_MSG("Cannot compute non-constant expression at compile time.")
                ERR_LINE(expression->tokenRange, "run only works on constant expressions")
            )
            // TODO: Find which part of the expression isn't constant
        }
    }

    if (expression->isValue) {
        // data type
        if (AST::IsInteger(expression->typeId)) {
            i64 val = expression->i64Value;
            // Assert(!(val&0xFFFFFFFF00000000));

            // TODO: immediate only allows 32 bits. What about larger values?
            // info.code->addDebugText("  expr push int");
            // TOKENINFO(expression->tokenRange)

            // TODO: Int types should come from global scope. Is it a correct assumption?
            // TypeInfo *typeInfo = info.ast->getTypeInfo(expression->typeId);
            u32 size = info.ast->getTypeSize(expression->typeId);
            u8 reg = RegBySize(BC_AX, size);
            if(size == 8) {
                info.addLoadIm2(reg, val);
            } else {
                info.addLoadIm(reg, val);
            }
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
            // return SignalDefault::SUCCESS;
        }
        else if (expression->typeId == AST_FLOAT64) {
            double val = expression->f64Value;

            // TOKENINFO(expression->tokenRange)
            // info.code->addDebugText("  expr push float");
            info.addLoadIm2(BC_REG_RAX, *(u64*)&val);
            info.addPush(BC_REG_RAX);
            
            // outTypeIds->add( expression->typeId);
            // return SignalDefault::SUCCESS;
        }
        else if (expression->typeId == AST_ID) {
            // NOTE: HELLO! i commented out the code below because i thought it was strange and out of place.
            //   It might be important but I just don't know why.
            // NOTE: Yes it was important past me. AST_ID and variables have simular syntax.
            // NOTE: Hello again! I am commenting the enum code again because I don't think it's necessary with the new changes.
            
            // TypeInfo *typeInfo = info.ast->convertToTypeInfo(expression->name, idScope, true);
            // // A simple check to see if the identifier in the expr node is an enum type.
            // // no need to check for pointers or so.
            // if (typeInfo && typeInfo->astEnum) {
            //     Assert(false);
            //     outTypeIds->add(typeInfo->id);
            //     return SignalDefault::SUCCESS;
            // }

            if(expression->enum_ast) {
                // SAMECODE as AST_MEMBER further down
                auto& mem = expression->enum_ast->members[expression->enum_member];
                int size = info.ast->getTypeSize(expression->enum_ast->actualType);
                Assert(size <= 8);
                
                if(size > 4) {
                    Assert(sizeof(mem.enumValue) == size); // bug in parser/AST
                    info.addLoadIm2(BC_REG_EAX, mem.enumValue);
                    info.addPush(BC_REG_RAX);
                } else {
                    info.addLoadIm(BC_REG_EAX, mem.enumValue);
                    info.addPush(BC_REG_EAX);
                }

                outTypeIds->add(expression->enum_ast->actualType);
                return SignalDefault::SUCCESS;
            }

            // check data type and get it
            // auto id = info.ast->findIdentifier(idScope, , expression->name);
            auto id = expression->identifier;
            if (id) {
                if (id->type == Identifier::VARIABLE) {
                    auto varinfo = info.ast->getVariableByIdentifier(id);
                    if(!varinfo->versions_typeId[info.currentPolyVersion].isValid()){
                        return SignalDefault::FAILURE;
                    }
                    // auto var = info.ast->getVariableByIdentifier(id);
                    // TODO: check data type?
                    // fp + offset
                    // TODO: what about struct
                    _GLOG(log::out << " expr var push " << expression->name << "\n";)

                    // char buf[100];
                    // int len = sprintf(buf,"  expr push %s",expression->name->c_str());
                    // info.code->addDebugText(buf,len);
                    // log::out << "AST_VAR: "<<id->name<<", "<<id->varIndex<<", "<<var->frameOffset<<"\n";
                    
                    switch(varinfo->type) {
                        case VariableInfo::GLOBAL: {
                            info.addInstruction({BC_DATAPTR, BC_REG_RBX});
                            info.addImm(varinfo->versions_dataOffset[info.currentPolyVersion]);
                            GeneratePush(info, BC_REG_RBX, 0, varinfo->versions_typeId[info.currentPolyVersion]);
                            break; 
                        }
                        case VariableInfo::LOCAL: {
                            GeneratePush(info, BC_REG_FP, varinfo->versions_dataOffset[info.currentPolyVersion],
                                varinfo->versions_typeId[info.currentPolyVersion]);
                            break;
                        }
                        case VariableInfo::MEMBER: {
                            // NOTE: Is member variable/argument always at this offset with all calling conventions?
                            info.addInstruction({BC_MOV_MR_DISP32, BC_REG_FP, BC_REG_RBX, 8});
                            info.addImm(GenInfo::FRAME_SIZE);
                            
                            // info.addLoadIm(BC_REG_RAX, varinfo->versions_dataOffset[info.currentPolyVersion]);
                            // info.addInstruction({BC_ADDI, BC_REG_RBX, BC_REG_RAX, BC_REG_RBX});

                            GeneratePush(info, BC_REG_RBX, varinfo->versions_dataOffset[info.currentPolyVersion],
                                varinfo->versions_typeId[info.currentPolyVersion]);
                            break;
                        }
                        default: {
                            Assert(false);
                        }
                    }

                    outTypeIds->add(varinfo->versions_typeId[info.currentPolyVersion]);
                    return SignalDefault::SUCCESS;
                } else if (id->type == Identifier::FUNCTION) {
                    _GLOG(log::out << " expr func push " << expression->name << "\n";)

                    if(id->funcOverloads.overloads.size()==1){
                        info.addInstruction({BC_CODEPTR, BC_REG_RAX});
                        info.addCallToResolve(info.code->length(), id->funcOverloads.overloads[0].funcImpl);
                        info.addImm(id->funcOverloads.overloads[0].funcImpl->address);

                        // info.code->exportSymbol(expression->name, id->funcOverloads.overloads[0].funcImpl->address);
                    } else {
                        ERR_SECTION(
                            ERR_HEAD(expression->tokenRange)
                            ERR_MSG("You can only refer to functions with one overload. '"<<expression->name<<"' has "<<id->funcOverloads.overloads.size()<<".")
                            ERR_LINE(expression->tokenRange,"cannot refer to function")
                        )
                    }

                    info.addPush(BC_REG_RAX);

                    outTypeIds->add(AST_FUNC_REFERENCE);
                    return SignalDefault::SUCCESS;
                } else {
                    INCOMPLETE
                }
            } else {
                Assert(info.hasForeignErrors());
                // {
                //     ERR_SECTION(
                //         ERR_HEAD(expression->tokenRange)
                //         ERR_MSG("'"<<expression->tokenRange.firstToken<<"' is not declared.")
                //         ERR_LINE(expression->tokenRange,"undeclared")
                //     )
                // }
                return SignalDefault::FAILURE;
            }
        } else if (expression->typeId == AST_FNCALL) {
            return GenerateFnCall(info, expression, outTypeIds, false);
        } else if(expression->typeId==AST_STRING){
            // Assert(expression->constStrIndex!=-1);
            int& constIndex = expression->versions_constStrIndex[info.currentPolyVersion];
            auto& constString = info.ast->getConstString(constIndex);

            TypeInfo* typeInfo = nullptr;
            if (expression->flags & ASTNode::NULL_TERMINATED) {
                typeInfo = info.ast->convertToTypeInfo(Token("char*"), info.ast->globalScopeId, true);
                Assert(typeInfo);
                info.addInstruction({BC_DATAPTR, BC_REG_RBX});
                info.addImm(constString.address);
                info.addPush(BC_REG_RBX);
                TypeId ti = AST_CHAR;
                ti.setPointerLevel(1);
                outTypeIds->add(ti);
            } else {
                typeInfo = info.ast->convertToTypeInfo(Token("Slice<char>"), info.ast->globalScopeId, true);
                if(!typeInfo){
                    ERR_SECTION(
                        ERR_HEAD(expression->tokenRange)
                        ERR_MSG("'"<<expression->tokenRange<<"' cannot be converted to Slice<char> because Slice doesn't exist. Did you forget #import \"Basic\"")
                    )
                    return SignalDefault::FAILURE;
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
                info.addImm(constString.address);
                info.addPush(BC_REG_RBX);
                outTypeIds->add(typeInfo->id);
            }
            return SignalDefault::SUCCESS;
        } else if(expression->typeId == AST_SIZEOF){

            // TypeId typeId = info.ast->convertToTypeId(expression->name, idScope);
            // Assert(typeId.isValid()); // Did type checker fix this? Maybe not on errors?

            TypeId typeId = expression->versions_outTypeSizeof[info.currentPolyVersion];
            u32 size = info.ast->getTypeSize(typeId);

            info.addLoadIm(BC_REG_EAX, size);
            info.addPush(BC_REG_EAX);

            outTypeIds->add(AST_UINT32);
            return SignalDefault::SUCCESS;
        } else if(expression->typeId == AST_TYPEID){

            // TypeId typeId = info.ast->convertToTypeId(expression->name, idScope);
            // Assert(typeId.isValid()); // Did type checker fix this? Maybe not on errors?

            TypeId result_typeId = expression->versions_outTypeTypeid[info.currentPolyVersion];

            const char* tname = "lang_TypeId";
            TypeInfo* typeInfo = info.ast->convertToTypeInfo(Token(tname), info.ast->globalScopeId, true);
            if(!typeInfo){
                Assert(info.hasForeignErrors());
                return SignalDefault::FAILURE;
            }
            Assert(typeInfo->getSize() == 4);

            // NOTE: This is scuffed, could be a bug in the future
            // lang::TypeId tmp={};
            // tmp.index0 = result_typeId._infoIndex0;
            // tmp.index1 = result_typeId._infoIndex1;
            // tmp.ptr_level = result_typeId.pointer_level;

            // info.addLoadIm(BC_REG_EAX, *(u32*)&tmp);
            // info.addPush(BC_REG_EAX);
            
            // NOTE: Struct members are pushed in the opposite order
            info.addLoadIm(BC_REG_AL, result_typeId.pointer_level);
            info.addPush(BC_REG_AL);

            info.addLoadIm(BC_REG_AL, result_typeId._infoIndex1);
            info.addPush(BC_REG_AL);

            info.addLoadIm(BC_REG_AX, result_typeId._infoIndex0);
            info.addPush(BC_REG_AX);

            outTypeIds->add(typeInfo->id);
            return SignalDefault::SUCCESS;
        }  else if(expression->typeId == AST_ASM){
            // TODO: How does polymorphism complicated this?
            //   You don't want duplicate inline assembly.
            if(!info.disableCodeGeneration) {
                u32 start = info.code->rawInlineAssembly.used;

                // +2 and -1 to avoid adding "asm { }"
                TokenRange range{};
                range.firstToken = expression->tokenRange.tokenStream()->get(expression->tokenRange.startIndex()+2);
                range.endIndex = expression->tokenRange.endIndex-1;
                
                int size = range.queryFeedSize();
                info.code->rawInlineAssembly._reserve(info.code->rawInlineAssembly.used + size);
                int feed_size = range.feed(info.code->rawInlineAssembly.data() + info.code->rawInlineAssembly.size(), size);
                Assert(feed_size == size);
                info.code->rawInlineAssembly.used+=size;
                
                // This doesn't output the tokens correctly. feed is built to accurately output the tokens
                // to their original form (except contiguous spacing)
                // u32 startT = expression->tokenRange.startIndex()+2;
                // u32 endT = expression->tokenRange.endIndex-1;
                // TokenStream* stream = expression->tokenRange.tokenStream();
                // /// We can assume that all tokens come from the same stream for now.
                // for (int i=startT; i < (int)endT; i++) {
                //     Token& tok = stream->get(i);
                //     // OPTIMIZE: TODO: You can compute the character length of the inline assembly in the parser and
                //     //  resize in advance instead of resizing per token.
                //     info.code->rawInlineAssembly._reserve(info.code->rawInlineAssembly.used + tok.length + 2); // +2 for space or line feed
                //     memcpy(info.code->rawInlineAssembly._ptr + info.code->rawInlineAssembly.used,
                //         tok.str, tok.length);
                //     info.code->rawInlineAssembly.used += tok.length;
                //     if(tok.flags&TOKEN_SUFFIX_LINE_FEED) {
                //         info.code->rawInlineAssembly._ptr[info.code->rawInlineAssembly.used] = '\n';
                //         info.code->rawInlineAssembly.used += 1;
                //     } else if(tok.flags&TOKEN_SUFFIX_SPACE) {
                //         info.code->rawInlineAssembly._ptr[info.code->rawInlineAssembly.used] = ' ';
                //         info.code->rawInlineAssembly.used += 1;
                //     }
                //     Assert(0==(tok.flags&TOKEN_MASK_QUOTED));
                // }

                u32 end = info.code->rawInlineAssembly.used;

                int asmInstanceIndex = info.code->asmInstances.size();
                info.code->asmInstances.add({start, end});
                auto& inst = info.code->asmInstances.last();
                inst.lineStart = range.firstToken.line;
                inst.lineEnd = range.tokenStream()->get(range.endIndex-1).line;
                inst.file = range.tokenStream()->streamName;

                // NOTE: ASM_ENCODE_INDEX result in 3 expression separated by comma
                info.addInstruction({BC_ASM, ASM_ENCODE_INDEX(asmInstanceIndex)}); // ASM_ENCODE_INDEX results in bytes separated by commas
                // info.addInstruction({BC_ASM, (u8)(asmInstanceIndex&0xFF), (u8)((asmInstanceIndex>>8)&0xFF), (u8)((asmInstanceIndex>>16)&0xFF)});
            }
            outTypeIds->add(AST_VOID);
            return SignalDefault::SUCCESS;
        } 
        else if(expression->typeId == AST_NAMEOF){
            // TypeId typeId = info.ast->convertToTypeId(expression->name, idScope);
            // Assert(typeId.isValid());

            // std::string name = info.ast->typeToString(typeId);

            // Assert(expression->constStrIndex!=-1);
            int constIndex = expression->versions_constStrIndex[info.currentPolyVersion];
            auto& constString = info.ast->getConstString(constIndex);

            TypeInfo* typeInfo = info.ast->convertToTypeInfo(Token("Slice<char>"), info.ast->globalScopeId, true);
            if(!typeInfo){
                ERR_SECTION(
                    ERR_HEAD(expression->tokenRange)
                    ERR_MSG("'"<<expression->tokenRange<<"' cannot be converted to Slice<char> because Slice doesn't exist. Did you forget #import \"Basic\"")
                )
                return SignalDefault::FAILURE;
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
            info.addImm(constString.address);
            info.addPush(BC_REG_RBX);

            outTypeIds->add(typeInfo->id);
            return SignalDefault::SUCCESS;
        } else if (expression->typeId == AST_NULL) {
            // TODO: Move into the type checker?
            // info.code->addDebugText("  expr push null");
            info.addLoadIm(BC_REG_RAX, 0);
            info.addPush(BC_REG_RAX);

            TypeInfo *typeInfo = info.ast->convertToTypeInfo(Token("void"), info.ast->globalScopeId, true);
            TypeId newId = typeInfo->id;
            newId.setPointerLevel(1);
            outTypeIds->add( newId);
            return SignalDefault::SUCCESS;
        } else {
            auto typeName = info.ast->typeToString(expression->typeId);
            // info.addInstruction({BC_PUSH,BC_REG_RAX}); // push something so the stack stays synchronized, or maybe not?
            ERR_SECTION(
                ERR_HEAD(expression->tokenRange)
                ERR_MSG("'" <<typeName << "' is an unknown data type.")
                ERR_LINE(expression->tokenRange, typeName)
            )
            // log::out <<  log::RED<<"GenExpr: data type not implemented\n";
            outTypeIds->add( AST_VOID);
            return SignalDefault::FAILURE;
        }
        outTypeIds->add(expression->typeId);
    } else {
        FuncImpl* operatorImpl = nullptr;
        if(expression->versions_overload._array.size()>0)
            operatorImpl = expression->versions_overload[info.currentPolyVersion].funcImpl;
        TypeId ltype = AST_VOID;
        if(operatorImpl){
            return GenerateFnCall(info, expression, outTypeIds, true);
        } else if (expression->typeId == AST_REFER) {

            SignalDefault result = GenerateReference(info,expression->left,&ltype,idScope);
            if(result!=SignalDefault::SUCCESS)
                return SignalDefault::FAILURE;
            if(ltype.getPointerLevel()==3){
                ERR_SECTION(
                    ERR_HEAD(expression->tokenRange)
                    ERR_MSG("Cannot take a reference of a triple pointer (compiler has a limit of 0-3 depth of pointing). Cast to u64 if you need triple pointers.")
                )
                return SignalDefault::FAILURE;
            }
            ltype.setPointerLevel(ltype.getPointerLevel()+1);
            outTypeIds->add( ltype); 
        } else if (expression->typeId == AST_DEREF) {
            DynamicArray<TypeId> ltypes{};
            
            SignalDefault result = GenerateExpression(info, expression->left, &ltypes);
            if (result != SignalDefault::SUCCESS)
                return result;
            
            if(ltypes.size()!=1) {
                StringBuilder values = {};
                FORN(ltypes) {
                    auto& it = ltypes[nr];
                    if(nr != 0)
                        values += ", ";
                    values += info.ast->typeToString(it);
                }
                ERR_SECTION(
                    ERR_HEAD(expression->left->tokenRange)
                    ERR_MSG("Cannot dereference more than one value.")
                    ERR_LINE(expression->left->tokenRange, values.data())
                )
                return SignalDefault::FAILURE;
            }
            ltype = ltypes[0];

            if (!ltype.isPointer()) {
                ERR_SECTION(
                    ERR_HEAD(expression->left->tokenRange)
                    ERR_MSG("Cannot dereference " << info.ast->typeToString(ltype) << ".")
                    ERR_LINE(expression->left->tokenRange, "bad")
                )
                outTypeIds->add( AST_VOID);
                return SignalDefault::FAILURE;
            }

            TypeId outId = ltype;
            outId.setPointerLevel(outId.getPointerLevel()-1);

            if (outId == AST_VOID) {
                ERR_SECTION(
                    ERR_HEAD(expression->left->tokenRange)
                    ERR_MSG("Cannot dereference " << info.ast->typeToString(ltype) << ".")
                    ERR_LINE(expression->left->tokenRange, "bad")
                )
                return SignalDefault::FAILURE;
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
            SignalDefault result = GenerateExpression(info, expression->left, &ltype);
            if (result != SignalDefault::SUCCESS)
                return result;
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

            outTypeIds->add(AST_BOOL);
        }
        else if (expression->typeId == AST_BNOT) {
            SignalDefault result = GenerateExpression(info, expression->left, &ltype);
            if (result == SignalDefault::FAILURE)
                return result;
            // TypeInfo *ti = info.ast->getTypeInfo(ltype);
            u32 size = info.ast->getTypeSize(ltype);
            u8 reg = RegBySize(BC_AX, size);

            info.addPop(reg);
            info.addInstruction({BC_BNOT, reg, reg});
            info.addPush(reg);

            outTypeIds->add( ltype);
        }
        else if (expression->typeId == AST_CAST) {
            SignalDefault result = GenerateExpression(info, expression->left, &ltype);
            if (result != SignalDefault::SUCCESS)
                return result;

            // TypeInfo *ti = info.ast->getTypeInfo(ltype);
            // TypeInfo *tic = info.ast->getTypeInfo(castType);
            u32 typeSize = info.ast->getTypeSize(ltype);
            TypeId castType = expression->versions_castType[info.currentPolyVersion];
            u32 castSize = info.ast->getTypeSize(castType);
            u8 lreg = RegBySize(BC_AX, typeSize);
            u8 creg = RegBySize(BC_AX, castSize);
            if(expression->isUnsafeCast()) {
                if(expression->left->typeId == AST_ASM) {
                    // TODO: Deprecate this and use asm<i32> {} instead of cast_unsafe<i32> asm {}. asm -> i32 {} is an alternative syntax
                    //  asm<i32> makes more since because the casting is a little special since
                    //  we don't know what type the inline assembly produces. Maybe it does 2 pushes
                    //  or none. With unsafe cast we assume a type which isn't ideal.
                    //  Unfortunately, it's difficult to know what type is pushed in the inline assembly.
                    //  It might ruin the stack and frame pointers.
                    
                    // The unsafe cast implies that the asm block did this. hopefully it did.
                    SignalDefault result = GenerateArtificialPush(info, castType);
                } else {
                    info.addPop(lreg);
                    info.addPush(creg);
                }
                outTypeIds->add(castType);
            } else {
                // if ((castType.isPointer() && ltype.isPointer()) || (castType.isPointer() && (ltype == (TypeId)AST_INT64 ||
                //     ltype == (TypeId)AST_UINT64 || ltype == (TypeId)AST_INT32)) || ((castType == (TypeId)AST_INT64 ||
                //     castType == (TypeId)AST_UINT64 || ltype == (TypeId)AST_INT32) && ltype.isPointer())) {
                //     info.addPop(lreg);
                //     info.addPush(creg);
                //     outTypeIds->add(castType);
                //     // data is fine as it is, just change the data type
                // } else { 
                bool yes = PerformSafeCast(info, ltype, castType);
                if (yes) {
                    outTypeIds->add(castType);
                } else {
                    Assert(info.hasForeignErrors());
                    
                    outTypeIds->add(ltype); // ltype since cast failed
                    return SignalDefault::FAILURE;
                }
                // }
            }
        }
        else if (expression->typeId == AST_FROM_NAMESPACE) {
            // info.code->addDebugText("ast-namespaced expr\n");

            auto si = info.ast->getScope(expression->name, info.currentScopeId);
            TypeId exprId;
            SignalDefault result = GenerateExpression(info, expression->left, &exprId, si->id);
            if (result != SignalDefault::SUCCESS)
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
                TypeInfo *typeInfo = info.ast->convertToTypeInfo(expression->left->name, idScope, true);
                // A simple check to see if the identifier in the expr node is an enum type.
                // no need to check for pointers or so.
                if (typeInfo && typeInfo->astEnum) {
                    // SAMECODE as when checking AST_ID further up
                    i32 enumValue;
                    bool found = typeInfo->astEnum->getMember(expression->name, &enumValue);
                    if (!found) {
                        Assert(info.hasForeignErrors());
                        // ERR_SECTION(
                        // ERR_HEAD(expression->tokenRange, expression->tokenRange.firstToken << " is not a member of enum " << typeInfo->astEnum->name << "\n";
                        // )
                        return SignalDefault::FAILURE;
                    }

                    info.addLoadIm(BC_REG_EAX, enumValue);
                    info.addPush(BC_REG_EAX);

                    outTypeIds->add(typeInfo->id);
                    return SignalDefault::SUCCESS;
                }
            }
            // TODO: We don't allow Apple{"Green",9}.name because initializer is not
            //  a reference. We should allow it though.

            SignalDefault result = GenerateReference(info,expression,&exprId);
            if(result != SignalDefault::SUCCESS) {
                return SignalDefault::FAILURE;
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
                ERR_SECTION(
                    ERR_HEAD(expression->tokenRange)
                    ERR_MSG_LOG("cannot do initializer on non-struct " << log::YELLOW << str << "\n")
                )
                return SignalDefault::FAILURE;
            }

            ASTStruct *astruct = structInfo->astStruct;

            // store expressions in a list so we can iterate in reverse
            // TODO: parser could arrange the expressions in reverse.
            //   it would probably be easier since it constructs the nodes
            //   and has a little more context and control over it.
            // std::vector<ASTExpression *> exprs;
            TINY_ARRAY(ASTExpression*,exprs,6);
            exprs.resize(astruct->members.size());

            // Assert(expression->args);
            // for (int index = 0; index < (int)expression->args->size(); index++) {
            //     ASTExpression *expr = expression->args->get(index);
                
            for (int index = 0; index < (int)expression->args.size(); index++) {
                ASTExpression *expr = expression->args.get(index);

                if (!expr->namedValue.str) {
                    if ((int)exprs.size() <= index) {
                        ERR_SECTION(
                            ERR_HEAD(expr->tokenRange)
                            ERR_MSG("To many members for struct " << astruct->name << " (" << astruct->members.size() << " member(s) allowed).")
                        )
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
                        ERR_SECTION(
                            ERR_HEAD(expr->tokenRange)
                            ERR_MSG("'"<<expr->namedValue << "' is not an member in " << astruct->name << ".")
                        )
                        continue;
                    } else {
                        if (exprs[memIndex]) {
                            ERR_SECTION(
                                ERR_HEAD(expr->tokenRange)
                                ERR_MSG("Argument for " << astruct->members[memIndex].name << " is already specified at " << LOGAT(exprs[memIndex]->tokenRange) << ".")
                            )
                            
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
                    SignalDefault result = GenerateDefaultValue(info, 0, 0, exprId, nullptr);
                    if (result != SignalDefault::SUCCESS)
                        return result;
                    // ERR_SECTION(
                // ERR_HEAD(expression->tokenRange, "Missing argument for " << astruct->members[index].name << " (call to " << astruct->name << ").\n";
                    // )
                    // continue;
                } else {
                    SignalDefault result = GenerateExpression(info, expr, &exprId);
                    if (result != SignalDefault::SUCCESS)
                        return result;
                }

                if (index >= (int)structInfo->astStruct->members.size()) {
                    // ERR() << "To many arguments! func:"<<*expression->funcName<<" max: "<<astFunc->arguments.size()<<"\n";
                    continue;
                }
                TypeId tid = structInfo->getMember(index).typeId;
                if (!PerformSafeCast(info, exprId, tid)) {   // implicit conversion
                    // if(astFunc->arguments[index].typeId!=dt){ // strict, no conversion
                    ERRTYPE1(expr->tokenRange, exprId, tid, "(initializer)");
                    
                    continue;
                }
            }
            // if((int)exprs.size()!=(int)structInfo->astStruct->members.size()){
            //     // return SignalDefault::FAILURE;
            // }

            outTypeIds->add( structInfo->id);
        }
        else if (expression->typeId == AST_SLICE_INITIALIZER) {

            // TypeInfo* typeInfo = info.ast->getTypeInfo(*expression->name);
            // if(!structInfo||!structInfo->astStruct){
            //     ERR_HEAD2(expression->tokenRange) << "cannot do initializer on non-struct "<<log::YELLOW<<*expression->name<<"\n";
            //     return SignalDefault::FAILURE;
            // }

            // int index = (int)exprs.size();
            // // int index=-1;
            // while(index>0){
            //     index--;
            //     ASTExpression* expr = exprs[index];
            //     TypeId exprId;
            //     int result = GenerateExpression(info,expr,&exprId);
            //     if(result!=SignalDefault::SUCCESS) return result;

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
            //     // return SignalDefault::FAILURE;
            // }

            // outTypeIds->add( structInfo->id;
        }
        else if (expression->typeId == AST_FROM_NAMESPACE) {
            // TODO: chould not work on enums? Enum.One is used instead
            // TypeInfo* typeInfo = info.ast->getTypeInfo(*expression->name);
            // if(!typeInfo||!typeInfo->astEnum){
            //     ERR_HEAD2(expression->tokenRange) << "cannot access "<<(expression->member?*expression->member:"?")<<" from non-enum "<<*expression->name<<"\n";
            //     return SignalDefault::FAILURE;
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
            //     info.addImm(typeInfo->astEnum->members[index].enumValue);
            //     info.addPush(BC_REG_EAX);

            //     outTypeIds->add( AST_INT32;
            // }
        }
        else if(expression->typeId==AST_RANGE){
            TypeInfo* typeInfo = info.ast->convertToTypeInfo(Token("Range"), info.ast->globalScopeId, true);
            if(!typeInfo){
                ERR_SECTION(
                    ERR_HEAD(expression->tokenRange)
                    ERR_MSG("'"<<expression->tokenRange<<"' cannot be converted to struct Range since it doesn't exist. Did you forget #import \"Basic\".")
                    ERR_LINE(expression->tokenRange,"Where is Range?")
                )
                return SignalDefault::FAILURE;
            }
            Assert(typeInfo->astStruct);
            Assert(typeInfo->astStruct->members.size() == 2);
            Assert(typeInfo->structImpl->members[0].typeId == typeInfo->structImpl->members[1].typeId);

            TypeId inttype = typeInfo->structImpl->members[0].typeId;

            TypeId ltype={};
            SignalDefault result = GenerateExpression(info, expression->left,&ltype,idScope);
            if(result!=SignalDefault::SUCCESS)
                return result;
            TypeId rtype={};
            result = GenerateExpression(info, expression->right,&rtype,idScope);
            if(result!=SignalDefault::SUCCESS)
                return result;
            
            int lreg = RegBySize(BC_CX, info.ast->getTypeSize(ltype));
            int rreg = RegBySize(BC_DX, info.ast->getTypeSize(rtype));
            info.addPop(rreg);
            info.addPop(lreg);

            info.addPush(rreg);
            if(!PerformSafeCast(info,rtype,inttype)){
                std::string msg = info.ast->typeToString(rtype);
                ERR_SECTION(
                    ERR_HEAD(expression->right->tokenRange)
                    ERR_MSG("Cannot convert to "<<info.ast->typeToString(inttype)<<".")
                    ERR_LINE(expression->right->tokenRange,msg.c_str())
                )
            }

            info.addPush(lreg);
            if(!PerformSafeCast(info,ltype,inttype)){
                std::string msg = info.ast->typeToString(ltype);
                ERR_SECTION(
                    ERR_HEAD(expression->right->tokenRange)
                    ERR_MSG("Cannot convert to "<<info.ast->typeToString(inttype)<<".")
                    ERR_LINE(expression->right->tokenRange,msg.c_str());
                )
            }

            outTypeIds->add(typeInfo->id);
            return SignalDefault::SUCCESS;
        } 
        else if(expression->typeId == AST_INDEX){
            FuncImpl* operatorImpl = nullptr;
            if(expression->versions_overload._array.size()>0)
                operatorImpl = expression->versions_overload[info.currentPolyVersion].funcImpl;
            TypeId ltype = AST_VOID;
            if(operatorImpl){
                return GenerateFnCall(info, expression, outTypeIds, true);
            }
            // TOKENINFO(expression->tokenRange);
            SignalDefault err = GenerateExpression(info, expression->left, &ltype);
            if (err != SignalDefault::SUCCESS)
                return SignalDefault::FAILURE;

            TypeId rtype;
            err = GenerateExpression(info, expression->right, &rtype);
            if (err != SignalDefault::SUCCESS)
                return SignalDefault::FAILURE;

            if(!ltype.isPointer()){
                if(!info.hasForeignErrors()){
                    std::string strtype = info.ast->typeToString(ltype);
                    ERR_SECTION(
                        ERR_HEAD(expression->right->tokenRange)
                        ERR_MSG("Index operator ( array[23] ) requires pointer type in the outer expression. '"<<strtype<<"' is not a pointer.")
                        ERR_LINE(expression->right->tokenRange,strtype.c_str())
                    )
                }
                return SignalDefault::FAILURE;
            }
            if(!AST::IsInteger(rtype)){
                std::string strtype = info.ast->typeToString(rtype);
                ERR_SECTION(
                    ERR_HEAD(expression->right->tokenRange)
                    ERR_MSG("Index operator ( array[23] ) requires integer type in the inner expression. '"<<strtype<<"' is not an integer.")
                    ERR_LINE(expression->right->tokenRange,strtype.c_str())
                )
                return SignalDefault::FAILURE;
            }
            ltype.setPointerLevel(ltype.getPointerLevel()-1);

            u32 lsize = info.ast->getTypeSize(ltype);
            u32 rsize = info.ast->getTypeSize(rtype);
            u8 reg = RegBySize(BC_DX,rsize);
            info.addPop(reg); // integer
            info.addPop(BC_REG_RBX); // reference
            if(lsize>1){
                info.addLoadIm(BC_REG_EAX, lsize);
                info.addInstruction({BC_MULI, ARITHMETIC_SINT, reg, BC_REG_EAX});
            }
            info.addInstruction({BC_ADDI, BC_REG_RBX, BC_REG_EAX, BC_REG_RBX});

            SignalDefault result = GeneratePush(info, BC_REG_RBX, 0, ltype);

            outTypeIds->add(ltype);
        }
        else if(expression->typeId == AST_INCREMENT || expression->typeId == AST_DECREMENT){
            SignalDefault result = GenerateReference(info, expression->left, &ltype, idScope);
            if(result != SignalDefault::SUCCESS){
                return SignalDefault::FAILURE;
            }
            
            if(!AST::IsInteger(ltype) && !ltype.isPointer()){
                std::string strtype = info.ast->typeToString(ltype);
                ERR_SECTION(
                    ERR_HEAD(expression->left->tokenRange)
                    ERR_MSG("Increment/decrement only works on integer types unless overloaded.")
                    ERR_LINE(expression->left->tokenRange, strtype.c_str())
                )
                return SignalDefault::FAILURE;
            }

            u8 size = info.ast->getTypeSize(ltype);
            u8 reg = RegBySize(BC_AX, size);

            info.addPop(BC_REG_RBX); // reference

            info.addInstruction({BC_MOV_MR, BC_REG_RBX, reg, size});
            // post
            if(expression->isPostAction())
                info.addPush(reg);
            if(expression->typeId == AST_INCREMENT)
                info.addInstruction({BC_INCR, reg, 1, 0});
            else
                info.addInstruction({BC_INCR, reg, (u8)-1, (u8)-1});
            info.addInstruction({BC_MOV_RM, reg, BC_REG_RBX, size});
            // pre
            if(!expression->isPostAction())
                info.addPush(reg);
                
            outTypeIds->add(ltype);
        }
        else if(expression->typeId == AST_UNARY_SUB){
            SignalDefault result = GenerateExpression(info, expression->left, &ltype, idScope);
            if(result != SignalDefault::SUCCESS){
                return SignalDefault::FAILURE;
            }
            
            if(AST::IsInteger(ltype)) {
                u8 size = info.ast->getTypeSize(ltype);
                u8 regFinal = RegBySize(BC_AX, size);
                u8 regValue = RegBySize(BC_DX, size);

                info.addInstruction({BC_BXOR, regFinal, regFinal, regFinal});
                info.addPop(regValue);
                info.addInstruction({BC_SUBI, regFinal, regValue, regFinal});
                info.addPush(regFinal);
                outTypeIds->add(ltype);
            } else if (AST::IsDecimal(ltype)) {
                u8 size = info.ast->getTypeSize(ltype);
                u8 regMask = RegBySize(BC_AX, size);
                u8 regValue = RegBySize(BC_DX, size);
                if(size == 8)
                    info.addLoadIm2(regMask, 0x8000000000000000);
                else
                    info.addLoadIm(regMask, 0x80000000);
                info.addPop(regValue);
                info.addInstruction({BC_BXOR, regValue, regMask, regValue});
                info.addPush(regValue);
                outTypeIds->add(ltype);
            } else {
                std::string strtype = info.ast->typeToString(ltype);
                ERR_SECTION(
                    ERR_HEAD(expression->left->tokenRange)
                    ERR_MSG("Unary subtraction only works with integers and decimals.")
                    ERR_LINE(expression->left->tokenRange, strtype.c_str())
                )
                return SignalDefault::FAILURE;
            }
        }
        else {
            // I commented this out at some point for some reason. Not sure why?
            // Did I disable operator overloading? Should I uncomment?
            // const char* str = OpToStr((OperationType)expr->typeId.getId(), true);
            // if(str) {
            //     int yes = CheckFncall(info,scopeId,expr, outTypes, true);
                
            //     if(yes)
            //         return true;
            // }
            if(expression->typeId == AST_ASSIGN && expression->assignOpType == (OperationType)0){
                // THIS IS PURELY ASSIGN NOT +=, *=
                // WARN_HEAD3(expression->tokenRange,"Expression is generated first and then reference. Right to left instead of left to right. "
                // "This is important if you use assignments/increments/decrements on the same memory/reference/variable.\n\n";
                //     WARN_LINE(expression->right->tokenRange,"generated first");
                //     WARN_LINE(expression->left->tokenRange,"then this");
                // )
                TypeId rtype;
                SignalDefault result = GenerateExpression(info, expression->right, &rtype, idScope);
                if(result!=SignalDefault::SUCCESS)
                    return SignalDefault::FAILURE;

                TypeId assignType={};
                result = GenerateReference(info,expression->left,&assignType,idScope);
                if(result!=SignalDefault::SUCCESS)
                    return SignalDefault::FAILURE;

                info.addPop(BC_REG_RBX);

                if (!PerformSafeCast(info, rtype, assignType)) { // CASTING RIGHT VALUE TO TYPE ON THE LEFT
                    // std::string leftstr = info.ast->typeToString(assignType);
                    // std::string rightstr = info.ast->typeToString(rtype);
                    ERRTYPE1(expression->tokenRange, assignType, rtype, ""
                        // ERR_LINE(expression->left->tokenRange, leftstr.c_str());
                        // ERR_LINE(expression->right->tokenRange,rightstr.c_str());
                    )
                    return SignalDefault::FAILURE;
                }
                // if(assignType != rtype){
                //     if(info.errors != 0 || info.compileInfo->typeErrors !=0){
                //         return SignalDefault::FAILURE;
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
                
                outTypeIds->add(rtype);
            } else {
                // basic operations
                // if() {
                //     int a = 923;
                // }
                // log::out << "id: "<<expression->nodeId<<"\n";
                // BREAK(expression->nodeId == 1365)
                TypeId rtype;
                SignalDefault err = GenerateExpression(info, expression->left, &ltype);
                if (err != SignalDefault::SUCCESS)
                    return err;
                err = GenerateExpression(info, expression->right, &rtype);
                if (err != SignalDefault::SUCCESS)
                    return err;

                TypeId operationType = expression->typeId;
                int assignedSize = info.ast->getTypeSize(ltype);
                if(expression->typeId==AST_ASSIGN){
                    TypeId assignType={};
                    SignalDefault result = GenerateReference(info,expression->left,&assignType,idScope);
                    if(result!=SignalDefault::SUCCESS) return SignalDefault::FAILURE;
                    Assert(assignType == ltype);
                    operationType = expression->assignOpType;
                    info.addPop(BC_REG_RBX);
                }

                TypeInfo* left_info = info.ast->getTypeInfo(ltype.baseType());
                TypeInfo* right_info = info.ast->getTypeInfo(rtype.baseType());
                Assert(left_info && right_info);
                // if(operationType == AST_GREATER_EQUAL){
                //     int a = 0;
                // }
                u8 FIRST_REG = BC_CX; 
                u8 SECOND_REG = BC_AX;
                u8 OUT_REG = BC_CX;
                // if(ltype.isPointer() || rtype.isPointer()){
                if(((ltype.isPointer() && AST::IsInteger(rtype)) || (AST::IsInteger(ltype) && rtype.isPointer()) || (ltype.isPointer() && rtype.isPointer()))){
                    // TODO: Add operation should not be possible with two pointers.
                    if(operationType == AST_ADD && ltype.isPointer() && rtype.isPointer()) {
                        ERR_SECTION(
                            ERR_HEAD(expression->tokenRange)
                            ERR_MSG("You cannot add two pointers together. (is this a good or bad restriction?)")
                            ERR_LINE(expression->left->tokenRange, info.ast->typeToString(ltype));
                            ERR_LINE(expression->right->tokenRange, info.ast->typeToString(rtype));
                        )
                        return SignalDefault::FAILURE;
                    }
                    if((operationType == AST_EQUAL || operationType == AST_NOT_EQUAL)) {
                        if((AST::IsInteger(rtype) && info.ast->getTypeSize(rtype) != 8) || (AST::IsInteger(rtype) && info.ast->getTypeSize(rtype) != 8)) {
                            ERR_SECTION(
                                ERR_HEAD(expression->tokenRange)
                                ERR_MSG("When using equal operator with a pointer and integer they must be of the same size. The integer must be u64 or i64.")
                                ERR_LINE(expression->left->tokenRange, info.ast->typeToString(ltype));
                                ERR_LINE(expression->right->tokenRange, info.ast->typeToString(rtype));
                            )
                            return SignalDefault::FAILURE;
                        }
                    }
                    if (operationType != AST_ADD && operationType != AST_SUB  && operationType != AST_EQUAL  && operationType != AST_NOT_EQUAL) {
                        ERR_SECTION(
                            ERR_HEAD(expression->tokenRange)
                            ERR_MSG(OP_NAME((OperationType)operationType.getId()) << " does not work with pointers. Only addition and subtraction works.")
                            ERR_LINE(expression->left->tokenRange, info.ast->typeToString(ltype));
                            ERR_LINE(expression->right->tokenRange, info.ast->typeToString(rtype));
                        )
                        return SignalDefault::FAILURE;
                    }

                    u8 lsize = info.ast->getTypeSize(ltype);
                    u8 rsize = info.ast->getTypeSize(rtype);
                    u8 reg1 = RegBySize(FIRST_REG, lsize); // get the appropriate registers
                    u8 reg2 = RegBySize(SECOND_REG, rsize);
                    info.addPop(reg2); // note that right expression should be popped first
                    info.addPop(reg1);

                    BCInstruction bytecodeOp = ASTOpToBytecode(operationType,false);
                    if(bytecodeOp==0){
                        ERR_SECTION(
                            ERR_HEAD(expression->tokenRange)
                            ERR_MSG("Operation "<<expression->tokenRange.firstToken << " not available for types "<<
                            info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<"")
                        )
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
                } else if ((AST::IsInteger(ltype) || ltype == AST_CHAR || left_info->astEnum) && (AST::IsInteger(rtype) || rtype == AST_CHAR || right_info->astEnum)){
                    // TODO: WE DON'T CHECK SIGNEDNESS WITH ENUMS.

                    BCInstruction bytecodeOp = ASTOpToBytecode(operationType,false);
                    u8 lsize = info.ast->getTypeSize(ltype);
                    u8 rsize = info.ast->getTypeSize(rtype);
                    // We do a little switchero on the registers to better
                    // adapt to x64 instructions. This is stupid if you generate instructions for
                    // multiple architectures but we won't and probably won't. Plus we will probably
                    // do a revamp on our own bytecode and registers.
                    // IMPORTANT: Some instructions garble registers. Do not reuse mod and stuff
                    
                    bool compareOp = bytecodeOp == BC_LT || bytecodeOp == BC_LTE || bytecodeOp == BC_GT || bytecodeOp == BC_GTE;
                    u8 cmpType = CMP_SINT_SINT;
                    u8 arithmeticType = AST::IsSigned(ltype) ? ARITHMETIC_SINT : ARITHMETIC_UINT;
                    if(bytecodeOp == BC_DIVI) {
                        FIRST_REG = BC_AX;
                        SECOND_REG = BC_DX;
                        OUT_REG = FIRST_REG;
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD(expression->tokenRange)
                                ERR_MSG("You cannot divide signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE(expression->left->tokenRange,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE(expression->right->tokenRange,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                        }
                    } else if(bytecodeOp == BC_MULI) {
                        FIRST_REG = BC_DX;
                        SECOND_REG = BC_AX;
                        OUT_REG = SECOND_REG;
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD(expression->tokenRange)
                                ERR_MSG("You cannot multiply signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE(expression->left->tokenRange,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE(expression->right->tokenRange,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                        }
                    }  else if(bytecodeOp == BC_MODI) {
                        FIRST_REG = BC_AX;
                        SECOND_REG = BC_DX;
                        OUT_REG = SECOND_REG;
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD(expression->tokenRange)
                                ERR_MSG("You cannot take remainder (modulus) of signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE(expression->left->tokenRange,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE(expression->right->tokenRange,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                        }
                    } else if(bytecodeOp == BC_BLSHIFT || bytecodeOp == BC_BRSHIFT) {
                        FIRST_REG = BC_AX;
                        SECOND_REG = BC_CX;
                        OUT_REG = FIRST_REG;
                    } else if(compareOp) {
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD(expression->tokenRange)
                                ERR_MSG("You cannot compare signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE(expression->left->tokenRange,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE(expression->right->tokenRange,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                        }
                        // cmpType = (u8)(((u8)AST::IsSigned(ltype)<<1) | (u8)AST::IsSigned(rtype));
                        cmpType = CMP_DECODE(ltype,rtype,AST::IsSigned);
                        FIRST_REG = BC_AX;
                        SECOND_REG = BC_CX;
                    }
                    u8 outSize = lsize > rsize ? lsize : rsize;
                    
                    
                    u8 reg1 = RegBySize(FIRST_REG, lsize); // get the appropriate registers
                    u8 reg2 = RegBySize(SECOND_REG, rsize);
                    u8 regOut = RegBySize(OUT_REG, outSize); // TODO: Use the biggest size?
                    info.addPop(reg2); // note that right expression should be popped first
                    info.addPop(reg1);
                    if(lsize != outSize) {
                        u8 reg1_big = RegBySize(FIRST_REG, outSize); // get the appropriate registers
                        if(arithmeticType == ARITHMETIC_SINT)
                            info.addInstruction({BC_CAST, CAST_SINT_SINT, reg1, reg1_big});
                        if(arithmeticType == ARITHMETIC_UINT)
                            info.addInstruction({BC_CAST, CAST_SINT_UINT, reg1, reg1_big});
                        reg1 = reg1_big;
                    } else if(rsize != outSize) {
                        u8 reg2_big = RegBySize(SECOND_REG, outSize); // get the appropriate registers
                        if(arithmeticType == ARITHMETIC_SINT)
                            info.addInstruction({BC_CAST, CAST_SINT_SINT, reg2, reg2_big});
                        if(arithmeticType == ARITHMETIC_UINT)
                            info.addInstruction({BC_CAST, CAST_SINT_UINT, reg2, reg2_big});
                        reg2 = reg2_big;
                    }

                    if(bytecodeOp==0){
                        ERR_SECTION(
                            ERR_HEAD(expression->tokenRange)
                            ERR_MSG("Operation "<<expression->tokenRange.firstToken << " not available for types "<<
                            info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<".")
                        )
                    }
                    if(compareOp) {
                        info.addInstruction({bytecodeOp, cmpType, reg1, reg2});
                        outTypeIds->add(AST_BOOL);
                        info.addPush(reg2);
                    } else {
                        if(bytecodeOp == BC_DIVI || bytecodeOp == BC_MODI || bytecodeOp == BC_MULI) {
                            info.addInstruction({bytecodeOp, arithmeticType, reg1, reg2});
                        } else {
                            info.addInstruction({bytecodeOp, reg1, reg2, regOut});
                        }
                        if(lsize < rsize)
                            outTypeIds->add(rtype);
                        else
                            outTypeIds->add(ltype);
                        info.addPush(regOut);
                    }
                    if(expression->typeId==AST_ASSIGN){
                        Assert(assignedSize <= 8); // a mov instruction isn't enough for memory larger than 8-bytes.
                        Assert(assignedSize < 256); // assignedSize is casted to u8, we have loss of information
                        info.addInstruction({BC_MOV_RM, regOut, BC_REG_RBX, (u8)assignedSize});
                        // we can't use outsize, it may have been elevated to a bigger size than
                        // what the reference/variable we are assigning to causing us to another variable's memory.
                        // info.addInstruction({BC_MOV_RM, regOut, BC_REG_RBX, outSize});
                    }
                } else if ((AST::IsDecimal(ltype) || AST::IsInteger(ltype)) && (AST::IsDecimal(rtype) || AST::IsInteger(rtype))){
                    // Note that we will at least have one decimal.
                    Assert(AST::IsDecimal(ltype) || AST::IsDecimal(rtype));

                    /*
                    integers should be converted to floats
                    floats should have same size
                    smaller float should be converted to the bigger float
                    */
                    

                    BCInstruction bytecodeOp = ASTOpToBytecode(operationType,true);
                    u8 lsize = info.ast->getTypeSize(ltype);
                    u8 rsize = info.ast->getTypeSize(rtype);

                    u8 reg1i = RegBySize(BC_AX, lsize); // get the appropriate input registers
                    u8 reg2i = RegBySize(BC_AX, rsize);
                    u8 reg1f = RegBySize(BC_XMM0, lsize);
                    u8 reg2f = RegBySize(BC_XMM1, rsize);
                    
                    u8 finalSize = 0;
                    if(AST::IsDecimal(ltype)) {
                        finalSize = lsize;
                    }
                    if(AST::IsDecimal(rtype)) {
                        if(rsize > finalSize)
                            finalSize = rsize;
                    }

                    u8 reg1 = ENCODE_REG_BITS(BC_XMM0, SIZE_TO_BITS(finalSize));
                    u8 reg2 = ENCODE_REG_BITS(BC_XMM1, SIZE_TO_BITS(finalSize));
                    u8 regOut = ENCODE_REG_BITS(BC_XMM0, SIZE_TO_BITS(finalSize));

                    if(AST::IsInteger(rtype)){
                        info.addPop(reg2i); // note that right expression should be popped first
                        // log::out << __FILE__ << ":"<<__LINE__<<"\n";
                        // Assert(false);
                        if(AST::IsSigned(rtype)){
                            info.addInstruction({BC_CAST, CAST_SINT_FLOAT, reg2i, reg2});
                        } else {
                            info.addInstruction({BC_CAST, CAST_UINT_FLOAT, reg2i, reg2});
                        }
                        rsize = finalSize;
                    } else {
                        if(rsize < finalSize) {
                            info.addPop(reg2f);
                            info.addInstruction({BC_CAST, CAST_FLOAT_FLOAT, reg2f, reg2});
                        } else {
                            info.addPop(reg2);
                        }
                    }
                    if(AST::IsInteger(ltype)){
                        info.addPop(reg1i);
                        if(AST::IsSigned(rtype)){
                            info.addInstruction({BC_CAST, CAST_SINT_FLOAT, reg1i, reg1});
                        } else {
                            info.addInstruction({BC_CAST, CAST_UINT_FLOAT, reg1i, reg1});
                        }
                        lsize = finalSize;
                    } else {
                        if(lsize < finalSize) {
                            info.addPop(reg1f);
                            info.addInstruction({BC_CAST, CAST_FLOAT_FLOAT, reg1f, reg1});
                        } else {
                            info.addPop(reg1);
                        }
                    }
                    
                    bool comparison = false;
                    if(bytecodeOp == BC_FLT ||bytecodeOp == BC_FLTE || bytecodeOp == BC_FGT || bytecodeOp == BC_FGTE || bytecodeOp == BC_FEQ || bytecodeOp == BC_FNEQ) {
                        regOut = BC_REG_AL;
                        comparison = true;
                    }
                    if(bytecodeOp == BC_FMOD) {
                        // we must use xmm2 instead, it makes it easier to deal with in x64 converter
                        regOut = ENCODE_REG_BITS(BC_XMM2, SIZE_TO_BITS(finalSize));
                    }

                    if(bytecodeOp==0){
                        ERR_SECTION(
                            ERR_HEAD(expression->tokenRange)
                            ERR_MSG("Operation "<< OP_NAME((OperationType)operationType.getId()) << " not available for types "<<info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype))
                            // ERR_LINE(expression->left->tokenRange, info.ast->typeToString(ltype))
                            // ERR_LINE(expression->right->tokenRange, info.ast->typeToString(rtype))
                            ERR_LINE(expression->tokenRange, "types don't work with the operation")
                        )
                    }

                    info.addInstruction({bytecodeOp, reg1, reg2, regOut});
                    info.addPush(regOut);
                    if(expression->typeId==AST_ASSIGN){
                        info.addInstruction({BC_MOV_RM, regOut, BC_REG_RBX, DECODE_REG_SIZE(regOut)});
                    }
                    if(comparison)
                        outTypeIds->add(AST_BOOL);
                    else if(finalSize == 8)
                        outTypeIds->add(AST_FLOAT64);
                    else
                        outTypeIds->add(AST_FLOAT32);
                } else if (operationType == AST_AND || operationType==AST_OR || operationType == AST_NOT){
                    
                    // if (!PerformSafeCast(info, rtype, ltype)) { // CASTING RIGHT VALUE TO TYPE ON THE LEFT
                    //     std::string leftstr = info.ast->typeToString(ltype);
                    //     std::string rightstr = info.ast->typeToString(rtype);
                    //     ERRTYPE(expression->tokenRange, ltype, rtype, "\n\n";
                    //         ERR_LINE(expression->left->tokenRange, leftstr.c_str());
                    //         ERR_LINE(expression->right->tokenRange,rightstr.c_str());
                    //     )
                    //     return SignalDefault::FAILURE;
                    // }
                    // rtype = ltype;
                    
                    u8 lsize = info.ast->getTypeSize(ltype);
                    u8 rsize = info.ast->getTypeSize(rtype);
                    // info.addPush(reg);

                    u8 reg1 = RegBySize(FIRST_REG, lsize); // get the appropriate registers
                    u8 reg2 = RegBySize(SECOND_REG, rsize);
                    info.addPop(reg2); // note that right expression should be popped first
                    info.addPop(reg1);

                    BCInstruction bytecodeOp = ASTOpToBytecode(operationType,false);
                    if(bytecodeOp==0){
                        ERR_SECTION(
                            ERR_HEAD(expression->tokenRange)
                            ERR_MSG("Operation "<<expression->tokenRange.firstToken << " not available for types "<<
                            info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<".")
                        )
                    }

                    info.addInstruction({bytecodeOp, reg1, reg2, reg1});
                    u8 rego = RegBySize(FIRST_REG, 1);
                    info.addPush(rego);
                    outTypeIds->add(AST_BOOL);
                    // if(ltype.isPointer()){
                    // }else{
                    //     info.addInstruction({bytecodeOp, reg1, reg2, reg2});
                    //     outTypeIds->add(rtype);
                    // }
                    // if(expression->typeId==AST_ASSIGN){
                    //     info.addInstruction({BC_MOV_RM, reg2, BC_REG_RBX, rsize});
                    // }
                    // if(ltype.isPointer()){
                    //     info.addPush(reg1);
                    // }else{
                    //     info.addPush(reg2);
                    // }
                } else {
                    int bad = false;
                    if(ltype.getId()>=AST_TRUE_PRIMITIVES){
                        bad=true;
                        std::string msg = info.ast->typeToString(ltype);
                        ERR_SECTION(
                            ERR_HEAD(expression->tokenRange)
                            ERR_MSG("Cannot do operation on struct.")
                            ERR_LINE(expression->left->tokenRange,msg.c_str());
                            ERR_LINE(expression->right->tokenRange,msg.c_str());
                        )
                    }
                    // if(rtype.getId()>=AST_TRUE_PRIMITIVES){
                    //     bad=true;
                    //     std::string msg = info.ast->typeToString(rtype);
                    //     ERR_SECTION(
                    //         ERR_HEAD(expression->right->tokenRange)
                    //         ERR_MSG("Cannot do operation on struct.")
                    //         ERR_LINE(expression->right->tokenRange,msg.c_str());
                    //     )
                    // }
                    if(bad){
                        return SignalDefault::FAILURE;
                    } 
                    // compare is special with signed, unsigned
                    ERR_SECTION(
                        ERR_HEAD(expression->tokenRange)
                        ERR_MSG("Operation "<<expression->tokenRange.firstToken << " not available for types "<<
                            info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype) <<". Some operations haven't been implemented for some types yet.")
                        ERR_LINE(expression->tokenRange,"bad");
                    )
                    return SignalDefault::FAILURE;

                    // u32 rsize = info.ast->getTypeSize(rtype);

                    // u8 reg = RegBySize(BC_DX, rsize);
                    // info.addPop(reg);
                    if (!PerformSafeCast(info, rtype, ltype)) { // CASTING RIGHT VALUE TO TYPE ON THE LEFT
                        std::string leftstr = info.ast->typeToString(ltype);
                        std::string rightstr = info.ast->typeToString(rtype);
                        ERRTYPE1(expression->tokenRange, ltype, rtype, ""
                            // ERR_LINE(expression->left->tokenRange, leftstr.c_str());
                            // ERR_LINE(expression->right->tokenRange,rightstr.c_str());
                        )
                        return SignalDefault::FAILURE;
                    }
                    rtype = ltype;
                    
                    u8 lsize = info.ast->getTypeSize(ltype);
                    u8 rsize = info.ast->getTypeSize(rtype);
                    // info.addPush(reg);

                    u8 reg1 = RegBySize(FIRST_REG, lsize); // get the appropriate registers
                    u8 reg2 = RegBySize(SECOND_REG, rsize);
                    info.addPop(reg2); // note that right expression should be popped first
                    info.addPop(reg1);

                    BCInstruction bytecodeOp = ASTOpToBytecode(operationType,false);
                    if(bytecodeOp==0){
                        ERR_SECTION(
                            ERR_HEAD(expression->tokenRange)
                            ERR_MSG("Operation "<<expression->tokenRange.firstToken << " not available for types "<<
                            info.ast->typeToString(ltype) << " - "<<info.ast->typeToString(rtype)<<".")
                        )
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
    return SignalDefault::SUCCESS;
}

SignalDefault GenerateBody(GenInfo &info, ASTScope *body);
SignalDefault GenerateFunction(GenInfo& info, ASTFunction* function, ASTStruct* astStruct = nullptr){
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
        // TODO: Store function identifier in AST
        identifier = info.ast->findIdentifier(info.currentScopeId, CONTENT_ORDER_MAX, function->name);
        if (!identifier) {
            // NOTE: function may not have been added in the type checker stage for some reason.
            // THANK YOU, past me for writing this note. I was wondering what I broke and reading the
            // note made instantly realise that I broke something in the type checker.
            ERR_SECTION(
                ERR_HEAD(function->tokenRange)
                ERR_MSG("'"<<function->name << "' was null (compiler bug).")
                ERR_LINE(function->tokenRange, "bad")
            )
            // if (function->tokenRange.firstToken.str) {
            //     ERRTOKENS(function->tokenRange)
            // }
            // 
            return SignalDefault::FAILURE;
        }
        if (identifier->type != Identifier::FUNCTION) {
            // I have a feeling some error has been printed if we end up here.
            // Double check that though.
            return SignalDefault::FAILURE;
        }
        // ASTFunction* tempFunc = info.ast->getFunction(fid);
        // // Assert(tempFunc==function);
        // if(tempFunc!=function){
        //     // error already printed?
        //     return SignalDefault::FAILURE;
        // }
    }
    if(function->linkConvention != LinkConventions::NONE){
        if(function->polyArgs.size()!=0 || (astStruct && astStruct->polyArgs.size()!=0)){
            ERR_SECTION(
                ERR_HEAD(function->tokenRange)
                ERR_MSG("Imported/native functions cannot be polymorphic.")
                ERR_LINE(function->tokenRange, "remove polymorphism")
            )
            return SignalDefault::FAILURE;
        }
        if(identifier->funcOverloads.overloads.size() != 1){
            // TODO: This error prints multiple times for each duplicate definition of native function.
            //   With this, you know the location of the colliding functions but the error message is printed multiple times.
            //  It is better if the message is shown once and then show all locations at once.
            ERR_SECTION(
                ERR_HEAD(function->tokenRange)
                ERR_MSG("Imported/native functions can only have one overload.")
                ERR_LINE(function->tokenRange, "bad")
            )
            return SignalDefault::FAILURE;
        }
        // if(!info.hasForeignErrors()){
        //     Assert(function->_impls.size()==1);
        // }
        if(function->linkConvention == NATIVE ||
            info.compileInfo->compileOptions->target == TARGET_BYTECODE
        ){
            // Assert(info.compileInfo->nativeRegistry);
            auto nativeRegistry = NativeRegistry::GetGlobal();
            auto nativeFunction = nativeRegistry->findFunction(function->name);
            // auto nativeFunction = info.compileInfo->nativeRegistry->findFunction(function->name);
            if(nativeFunction){
                if(function->_impls.size()>0){
                    // impls may be zero if type checker found multiple definitions for native functions.
                    // we don't want to crash here when that happens and we don't need to throw an error
                    // because type checker already did.
                    function->_impls.last()->address = nativeFunction->jumpAddress;
                }
            } else {
                if(function->linkConvention != NATIVE){
                    ERR_SECTION(
                        ERR_HEAD(function->tokenRange)
                        ERR_MSG("'"<<function->name<<"' is not an import that the interpreter supports (not native). None of the "<<nativeRegistry->nativeFunctions.size()<<" native functions matched.")
                        ERR_LINE(function->name, "bad");
                    )
                } else {
                    ERR_SECTION(
                        ERR_HEAD(function->tokenRange)
                        ERR_MSG("'"<<function->name<<"' is not a native function. None of the "<<nativeRegistry->nativeFunctions.size()<<" native functions matched.")
                        ERR_LINE(function->name, "bad")
                    )
                }
                return SignalDefault::FAILURE;
            }
            _GLOG(log::out << "Native function "<<function->name<<"\n";)
        } else {
            // exports not handled
            Assert(IS_IMPORT(function->linkConvention));
            // if(function->_impls.size()>0){
            //     function->_impls.last()->address = FuncImpl::ADDRESS_EXTERNAL;
            // }
            _GLOG(log::out << "External function "<<function->name<<"\n";)
        }
        return SignalDefault::SUCCESS;
    }

    // When export is implemented I need to come back here and fix it.
    // The assert will notify me when that happens.
    Assert(function->linkConvention == LinkConventions::NONE);

    for(auto& funcImpl : function->getImpls()) {
        // IMPORTANT: If you uncomment this then you have to make sure
        //  that the type checker checked this function body. It won't if
        //  the implementation isn't used.
        if(!funcImpl->isUsed())
            // Skips implementation if it isn't used
            continue;
        Assert(("func has already been generated!",funcImpl->address == FuncImpl::ADDRESS_INVALID));
        // This happens with functions inside of polymorphic function.
        // if(function->callConvention != BETCALL) {
        //     ERR_SECTION(
        //         ERR_HEAD(function->tokenRange)
        //         ERR_MSG("The convention '" << function->callConvention << "' has not been implemented for user functions. The normal convention (betcall) is the only one that works.")
        //         ERR_LINE(function->tokenRange,"use betcall");
        //     )
        //     continue;
        // }
        // if(function->callConvention == UNIXCALL) {
        //     ERR_SECTION(
        //         ERR_HEAD(function->tokenRange)
        //         ERR_MSG("Unix calling convention is not implemented for user defined functions. You can however declare functions with unix convention and call them if they come from an external library.")
        //         ERR_LINE(function->tokenRange,"unixcall not implemented")
        //     )
        //     continue;
        // }
        // Assert(("Unix calling convention not implemented for user defined functions.",false));

        // Assert(!info.compileInfo->compileOptions->useDebugInformation);
        // IMPORTANT: How to deal with overloading and polymorphism?
        // add #0 at the end of the function name?
        // DebugInformation::Function* dfun = &di->functions.last();
        // dfun->name = "main";
        // dfun->fileIndex = di->files.size();
        // di->files.add(info.compileInfo->compileOptions->initialSourceFile.text);
        DebugInformation* di = info.code->debugInformation;
        Assert(di);
        
        info.debugFunctionIndex = di->functions.size();
        DebugInformation::Function* dfun = di->addFunction(funcImpl->name);
        if(function->body->statements.size()>0) {
            dfun->fileIndex = di->addOrGetFile(function->body->statements[0]->tokenRange.tokenStream()->streamName);
        } else {
            dfun->fileIndex = di->addOrGetFile(info.compileInfo->compileOptions->sourceFile.text);
        }
        dfun->funcImpl = funcImpl;
        dfun->funcAst = function;

        _GLOG(log::out << "Function " << funcImpl->name << "\n";)

        funcImpl->address = info.code->length();
        info.currentPolyVersion = funcImpl->polyVersion;

        // TODO: Export function symbol if annotated with @export
        //  Perhaps force functions named main to be annotated with @export in parser
        //  instead of handling the special case for main here?
        if(funcImpl->name == "main") {
            switch(info.compileInfo->compileOptions->target) { // this is really cheeky
            case TARGET_WINDOWS_x64:
                function->callConvention = STDCALL;
                break;
            case TARGET_UNIX_x64:
                function->callConvention = UNIXCALL;
                break;
            default:
                Assert(false);
                break;
            }
            bool yes = info.code->addExportedSymbol(funcImpl->name, funcImpl->address);
            if(!yes) {
                ERR_SECTION(
                    ERR_HEAD(function->tokenRange)
                    ERR_MSG("You cannot have two functions named main since it is the entry point. Either main is overloaded or polymorphic which isn't allowed.\n")
                    ERR_LINE(function->tokenRange,"second main (or third...)")
                    // TODO: Show all functions named main
                )
            }
        } else {
            // bool yes = info.code->addExportedSymbol(funcImpl->name, funcImpl->address);
            // if(!yes) {
            //     ERR_SECTION(
            //         ERR_HEAD(function->tokenRange)
            //         ERR_MSG("Exporting two functions with the same name is not possible.\n")
            //         ERR_LINE(function->tokenRange,"second function, same name")
            //         // TODO: Show all functions named main
            //     )
            // }
        }

        auto prevFunc = info.currentFunction;
        auto prevFuncImpl = info.currentFuncImpl;
        auto prevScopeId = info.currentScopeId;
        info.currentFunction = function;
        info.currentFuncImpl = funcImpl;
        info.currentScopeId = function->scopeId;
        defer { info.currentFunction = prevFunc;
            info.currentFuncImpl = prevFuncImpl;
            info.currentScopeId = prevScopeId; };

        // info.functionStackMoment = info.saveStackMoment();
        // -8 since the start of the frame is 0 and after the program counter is -8
        info.virtualStackPointer = GenInfo::VIRTUAL_STACK_START;
        // reset frame offset at beginning of function
        info.currentFrameOffset = 0;

        dfun->funcStart = info.code->length();
        dfun->entry_line = function->name.line;

        #define DFUN_ADD_VAR(NAME, OFFSET, TYPE) dfun->localVariables.add({});\
                            dfun->localVariables.last().name = NAME;\
                            dfun->localVariables.last().frameOffset = OFFSET;\
                            dfun->localVariables.last().typeId = TYPE;

        switch(function->callConvention) {
        case BETCALL: {
            if (function->arguments.size() != 0) {
                _GLOG(log::out << "set " << function->arguments.size() << " args\n");
                // int offset = 0;
                for (int i = 0; i < (int)function->arguments.size(); i++) {
                    // for(int i = function->arguments.size()-1;i>=0;i--){
                    auto &arg = function->arguments[i];
                    auto &argImpl = funcImpl->argumentTypes[i];
                    // auto var = info.ast->addVariable(info.currentScopeId, arg.name);
                    auto varinfo = info.ast->getVariableByIdentifier(arg.identifier);
                    if (!varinfo) {
                        ERR_SECTION(
                            ERR_HEAD(arg.name.range())
                            ERR_MSG(arg.name << " is already defined.")
                            ERR_LINE(arg.name.range(),"cannot use again")
                        )
                    }
                    // var->versions_typeId[info.currentPolyVersion] = argImpl.typeId;
                    // TypeInfo *typeInfo = info.ast->getTypeInfo(argImpl.typeId.baseType());
                    // var->globalData = false;
                    varinfo->versions_dataOffset[info.currentPolyVersion] = GenInfo::FRAME_SIZE + argImpl.offset;
                    _GLOG(log::out << " " <<"["<<varinfo->versions_dataOffset[info.currentPolyVersion]<<"] "<< arg.name << ": " << info.ast->typeToString(argImpl.typeId) << "\n";)
                    // DFUN_ADD_VAR(arg.name, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion])
                }
                _GLOG(log::out << "\n";)
            }
            if(function->parentStruct){
                // This doesn't need to be done here. Data offset is known in type checker since it 
                // depends on the struct type. The argument data offset is predictable. But
                // since the data offset for normal arguments are done here we might as well
                // to members here to for consistency.
                // TypeInfo* ti = info.ast->getTypeInfo(varinfo->versions_typeId[info.currentPolyVersion]);
                // ti->ast
                Assert(function->memberIdentifiers.size() == funcImpl->structImpl->members.size());
                for(int i=0;i<(int)function->memberIdentifiers.size();i++){
                    auto identifier  = function->memberIdentifiers[i];
                    if(!identifier) continue; // see type checker as to why this may happen
                    auto& memImpl = funcImpl->structImpl->members[i];

                    auto varinfo = info.ast->getVariableByIdentifier(identifier);
                    varinfo->versions_dataOffset[info.currentPolyVersion] = memImpl.offset;
                }
            }
            
            // HELLO READ THIS!
            // Before changing this code to make the caller make space for return values instead of
            // the calle, you have to realise that the return values must have the same offset everytime
            // you call the function. Meaning, return values must be aligned to 8 bytes even if they are
            // 4 byte integers. If not then the function will put the return values in the wrong place.

            // x64's call instruction does ignores the base pointer (frame pointer).
            // The interpreter does can do either.
            info.addPush(BC_REG_FP);
            info.addInstruction({BC_MOV_RR, BC_REG_SP, BC_REG_FP});
            if (funcImpl->returnTypes.size() != 0) {
                // _GLOG(log::out << "space for " << funcImpl->returnTypes.size() << " return value(s) (struct may cause multiple push)\n");
                
                // info.code->addDebugText("ZERO init return values\n");
                #ifndef DISABLE_ZERO_INITIALIZATION
                info.addLoadIm(BC_REG_RCX, funcImpl->returnSize);
                info.addInstruction({BC_MOV_RR, BC_REG_FP, BC_REG_RBX});
                info.addInstruction({BC_SUBI, BC_REG_RBX, BC_REG_RCX, BC_REG_RBX});
                GenMemzero(info, BC_REG_RBX, BC_REG_RCX, funcImpl->returnSize);
                // info.addInstruction({BC_MEMZERO, BC_REG_RBX, BC_REG_RCX});
                #endif
                // info.addIncrSp(-funcImpl->returnSize);
                info.addStackSpace(-funcImpl->returnSize);
                _GLOG(log::out << "Return values:\n";)
                info.currentFrameOffset -= funcImpl->returnSize; // TODO: size can be uneven like 13. FIX IN EVALUATETYPES
                for(int i=0;i<(int)funcImpl->returnTypes.size();i++){
                    auto& ret = funcImpl->returnTypes[i];
                    _GLOG(log::out << " " <<"["<<ret.offset<<"] " << ": " << info.ast->typeToString(ret.typeId) << "\n";)
                }
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
            break;
        }
        case STDCALL: {
            if (function->arguments.size() != 0) {
                _GLOG(log::out << "set " << function->arguments.size() << " args\n");
                // int offset = 0;
                for (int i = 0; i < (int)function->arguments.size(); i++) {
                    // for(int i = function->arguments.size()-1;i>=0;i--){
                    auto &arg = function->arguments[i];
                    auto &argImpl = funcImpl->argumentTypes[i];
                    Assert(arg.identifier); // bug in compiler?
                    auto varinfo = info.ast->getVariableByIdentifier(arg.identifier);
                    // auto varinfo = info.ast->addVariable(info.currentScopeId, arg.name);
                    if (!varinfo) {
                        ERR_SECTION(
                            ERR_HEAD(arg.name.range())
                            ERR_MSG(arg.name << " is already defined.")
                            ERR_LINE(arg.name.range(),"cannot use again")
                        )
                    }
                    // var->versions_typeId[info.currentPolyVersion] = argImpl.typeId;
                    u8 size = info.ast->getTypeSize(varinfo->versions_typeId[info.currentPolyVersion]);
                    if(size>8) {
                        ERR_SECTION(
                            ERR_HEAD(arg.name)
                            ERR_MSG(ToString(function->callConvention) << " does not allow arguments larger than 8 bytes. Pass by pointer if arguments are larger.")
                            ERR_LINE(arg.name, "bad")
                        )
                    }
                    // TypeInfo *typeInfo = info.ast->getTypeInfo(argImpl.typeId.baseType());
                    // stdcall should put first 4 args in registers but the function will put
                    // the arguments onto the stack automatically so in the end 8*i will work fine.
                    varinfo->versions_dataOffset[info.currentPolyVersion] = GenInfo::FRAME_SIZE + 8 * i;
                    _GLOG(log::out << " " <<"["<<varinfo->versions_dataOffset[info.currentPolyVersion]<<"] "<< arg.name << ": " << info.ast->typeToString(argImpl.typeId) << "\n";)
                    // DFUN_ADD_VAR(arg.name, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion])
                }
                _GLOG(log::out << "\n";)
            }
            // todo: Member variables for stdcall.
            Assert(!function->parentStruct);
            
            // HELLO READ THIS!
            // Before changing this code to make the caller make space for return values instead of
            // the calle, you have to realise that the return values must have the same offset everytime
            // you call the function. Meaning, return values must be aligned to 8 bytes even if they are
            // 4 byte integers. If not then the function will put the return values in the wrong place.
            // 4-6 days later...
            // Since stack management (push and pop) always work on 8 bytes, this won't be a problem anymore
            // Caller can make space for return values. The return values should be made poppable.

            // x64's call instruction does ignores the base pointer (frame pointer).
            // The interpreter does can do either.
            info.addPush(BC_REG_FP);
            info.addInstruction({BC_MOV_RR, BC_REG_SP, BC_REG_FP});

            if (funcImpl->returnTypes.size() > 1) {
                ERR_SECTION(
                    ERR_HEAD(function->tokenRange)
                    ERR_MSG(ToString(function->callConvention) << " only allows one return value. BETCALL (the default calling convention in this language) supports multiple return values.")
                    ERR_LINE(function->tokenRange, "bad")
                )
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

            const int normal_regs[4]{
                BC_REG_RCX,
                BC_REG_RDX,
                BC_REG_R8,
                BC_REG_R9,
            };
            const int float_regs[4] {
                BC_REG_XMM0d,
                BC_REG_XMM1d,
                BC_REG_XMM2d,
                BC_REG_XMM3d,
            };
            auto& argTypes = funcImpl->argumentTypes;
            for(int i=argTypes.size()-1;i>=0;i--) {
                auto argType = argTypes[i].typeId;

                u8 size = info.ast->getTypeSize(argType);
                if(AST::IsDecimal(argType)) {
                    info.addInstruction({BC_MOV_RM_DISP32, ENCODE_REG_BITS(float_regs[i], SIZE_TO_BITS(size)), BC_REG_FP, size});
                } else {
                    info.addInstruction({BC_MOV_RM_DISP32, ENCODE_REG_BITS(normal_regs[i], SIZE_TO_BITS(size)), BC_REG_FP, size});
                }
                info.addImm(GenInfo::FRAME_SIZE + i * 8);
            }

            // TODO: MAKE SURE SP IS ALIGNED TO 8 BYTES
            // SHOULD stackAlignment, virtualStackPointer be reset and then restored?
            // ALSO DO IT BEFORE CALLING FUNCTION (FNCALL)
            //
            // TODO: GenerateBody may be called multiple times for the same body with
            //  polymorphic functions. This is a problem since GenerateBody generates functions.
            //  Generating them multiple times for each function is bad.
            // NOTE: virtualTypes from this function may be accessed from a function within it's body.
            //  This could be bad.
            break;
        }
        case UNIXCALL: {
            if (function->arguments.size() != 0) {
                _GLOG(log::out << "set " << function->arguments.size() << " args\n");
                // int offset = 0;
                for (int i = 0; i < (int)function->arguments.size(); i++) {
                    // for(int i = function->arguments.size()-1;i>=0;i--){
                    auto &arg = function->arguments[i];
                    auto &argImpl = funcImpl->argumentTypes[i];
                    Assert(arg.identifier); // bug in compiler?
                    auto varinfo = info.ast->getVariableByIdentifier(arg.identifier);
                    // auto varinfo = info.ast->addVariable(info.currentScopeId, arg.name);
                    if (!varinfo) {
                        ERR_SECTION(
                            ERR_HEAD(arg.name.range())
                            ERR_MSG(arg.name << " is already defined.")
                            ERR_LINE(arg.name.range(),"cannot use again")
                        )
                    }
                    // var->versions_typeId[info.currentPolyVersion] = argImpl.typeId;
                    u8 size = info.ast->getTypeSize(varinfo->versions_typeId[info.currentPolyVersion]);
                    if(size>8) {
                        ERR_SECTION(
                            ERR_HEAD(arg.name)
                            ERR_MSG(ToString(function->callConvention) << " does not allow arguments larger than 8 bytes. Pass by pointer if arguments are larger.")
                            ERR_LINE(arg.name, "bad")
                        )
                    }
                    // TypeInfo *typeInfo = info.ast->getTypeInfo(argImpl.typeId.baseType());
                    // stdcall should put first 4 args in registers but the function will put
                    // the arguments onto the stack automatically so in the end 8*i will work fine.
                    varinfo->versions_dataOffset[info.currentPolyVersion] = GenInfo::FRAME_SIZE + 8 * i;
                    _GLOG(log::out << " " <<"["<<varinfo->versions_dataOffset[info.currentPolyVersion]<<"] "<< arg.name << ": " << info.ast->typeToString(argImpl.typeId) << "\n";)
                    // DFUN_ADD_VAR(arg.name, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion])
                }
                _GLOG(log::out << "\n";)
            }
            // Fix member variables for stdcall.
            Assert(!function->parentStruct);
            
            // HELLO READ THIS!
            // Before changing this code to make the caller make space for return values instead of
            // the calle, you have to realise that the return values must have the same offset everytime
            // you call the function. Meaning, return values must be aligned to 8 bytes even if they are
            // 4 byte integers. If not then the function will put the return values in the wrong place.
            // 4-6 days later...
            // Since stack management (push and pop) always work on 8 bytes, this won't be a problem anymore
            // Caller can make space for return values. The return values should be made poppable.

            // x64's call instruction does ignores the base pointer (frame pointer).
            // The interpreter does can do either.
            info.addPush(BC_REG_FP);
            info.addInstruction({BC_MOV_RR, BC_REG_SP, BC_REG_FP});

            if (funcImpl->returnTypes.size() > 1) {
                ERR_SECTION(
                    ERR_HEAD(function->tokenRange)
                    ERR_MSG(ToString(function->callConvention) << " only allows one return value. BETCALL (the default calling convention in this language) supports multiple return values.")
                    ERR_LINE(function->tokenRange, "bad")
                )
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

            auto& argTypes = funcImpl->argumentTypes;
            const int normal_regs[6]{
                BC_REG_RDI,
                BC_REG_RSI,
                BC_REG_RDX,
                BC_REG_RCX,
                BC_REG_R8,
                BC_REG_R9,
            };
            const int float_regs[8] {
                BC_REG_XMM0f,
                BC_REG_XMM1f,
                BC_REG_XMM2f,
                BC_REG_XMM3f,
                BC_REG_XMM4f,
                BC_REG_XMM5f,
                BC_REG_XMM6f,
                BC_REG_XMM7f,
            };
            int used_normals = 0;
            int used_floats = 0;
            // Because we need to pop arguments in reverse, we must first know how many
            // integer/pointer and float type arguments we have.
            for(int i=argTypes.size()-1;i>=0;i--) {
                if(AST::IsDecimal(argTypes[i].typeId)) {
                    used_floats++;
                } else {
                    used_normals++;
                }
            }
            // NOTE: We don't need to reverse, the order we put the args on the stack doesn't matter.
            //   They will be placed in the right location no matter what.
            //   Code was copied from GenerateFncall which is why the loop is reversed.
            for(int i=argTypes.size()-1;i>=0;i--) {
                u8 size = info.ast->getTypeSize(argTypes[i].typeId);
                if(AST::IsDecimal(argTypes[i].typeId)) {
                    // I am not sure if doubles work so we should test and not assume.
                    // This assert will prevent incorrect assumptions from causing trouble.
                     info.addInstruction({BC_MOV_RM_DISP32, (u8)ENCODE_REG_BITS(float_regs[--used_floats], SIZE_TO_BITS(size)), BC_REG_FP, size});
                } else {
                    info.addInstruction({BC_MOV_RM_DISP32, (u8)ENCODE_REG_BITS(normal_regs[--used_normals], SIZE_TO_BITS(size)), BC_REG_FP, size});
                }
                info.addImm(GenInfo::FRAME_SIZE + i * 8);
            }

            // TODO: MAKE SURE SP IS ALIGNED TO 8 BYTES
            // SHOULD stackAlignment, virtualStackPointer be reset and then restored?
            // ALSO DO IT BEFORE CALLING FUNCTION (FNCALL)
            //
            // TODO: GenerateBody may be called multiple times for the same body with
            //  polymorphic functions. This is a problem since GenerateBody generates functions.
            //  Generating them multiple times for each function is bad.
            // NOTE: virtualTypes from this function may be accessed from a function within it's body.
            //  This could be bad.
            break;
        }
        default: {
            Assert(false);
        }
        }
        
        dfun->codeStart = info.code->length();

        if(funcImpl->name == "main") {
            GeneratePreload(info);
        }
        
        SignalDefault result = GenerateBody(info, function->body);

        dfun->codeEnd = info.code->length();

        switch(function->callConvention) {
        case BETCALL: {
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
                        ERR_SECTION(
                            ERR_HEAD(function->tokenRange)
                            ERR_MSG("Missing return statement in '" << function->name << "'.")
                            ERR_LINE(function->tokenRange,"put a return in the body")
                        )
                    }
                }
            }
            if(info.code->length()<1 || info.code->get(info.code->length()-1).opcode!=BC_RET) {
                // add return with no return values if it doesn't exist
                // this is only fine if the function doesn't return values
                info.addPop(BC_REG_FP);
                info.addInstruction({BC_RET});
            }
            break;
        }
        case STDCALL: {
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
            Assert(funcImpl->returnTypes.size() < 2);
            // log::out << *function->name<<" " <<function->returnTypes.size()<<"\n";
            if (funcImpl->returnTypes.size() != 0) {
                // check last statement for a return and "exit" early
                bool foundReturn = function->body->statements.size()>0
                    && function->body->statements.get(function->body->statements.size()-1)
                    ->type == ASTStatement::RETURN;
                // TODO: A return statement might be okay in an inner scope and not necessarily the
                //  top scope. If else for example.
                if(!foundReturn){
                    for(auto it : function->body->statements){
                        if (it->type == ASTStatement::RETURN) {
                            foundReturn = true;
                            break;
                        }
                    }
                    if (!foundReturn) {
                        ERR_SECTION(
                            ERR_HEAD(function->tokenRange)
                            ERR_MSG("Missing return statement in '" << function->name << "'.")
                            ERR_LINE(function->tokenRange,"put a return in the body")
                        )
                    }
                }
            }
            if(info.code->length()<1 || info.code->get(info.code->length()-1).opcode!=BC_RET) {
                // add return with no return values if it doesn't exist
                // this is only fine if the function doesn't return values
                info.addInstruction({BC_BXOR, BC_REG_RAX, BC_REG_RAX, BC_REG_RAX});
                info.addPop(BC_REG_FP);
                info.addInstruction({BC_RET});
            }
            break;
        }
        case UNIXCALL: {
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
            Assert(funcImpl->returnTypes.size() < 2);
            if (funcImpl->returnTypes.size() != 0) {
                // check last statement for a return and "exit" early
                bool foundReturn = function->body->statements.size()>0
                    && function->body->statements.get(function->body->statements.size()-1)
                    ->type == ASTStatement::RETURN;
                // TODO: A return statement might be okay in an inner scope and not necessarily the
                //  top scope. If else for example.
                if(!foundReturn){
                    for(auto it : function->body->statements){
                        if (it->type == ASTStatement::RETURN) {
                            foundReturn = true;
                            break;
                        }
                    }
                    if (!foundReturn) {
                        ERR_SECTION(
                            ERR_HEAD(function->tokenRange)
                            ERR_MSG("Missing return statement in '" << function->name << "'.")
                            ERR_LINE(function->tokenRange,"put a return in the body")
                        )
                    }
                }
            }
            if(info.code->length()<1 || info.code->get(info.code->length()-1).opcode!=BC_RET) {
                // add return with no return values if it doesn't exist
                // this is only fine if the function doesn't return values
                info.addInstruction({BC_BXOR, BC_REG_RAX, BC_REG_RAX, BC_REG_RAX});
                info.addPop(BC_REG_FP);
                info.addInstruction({BC_RET});
            }
            break;
        }
        default: {
            Assert(false);
        }
        }
        
        dfun->funcEnd = info.code->length();

        // Assert(info.virtualStackPointer == GenInfo::VIRTUAL_STACK_START);
        // Assert(info.currentFrameOffset == 0);
        // needs to be done after frame pop
        // info.restoreStackMoment(info.functionStackMoment, false, true);
    }
    return SignalDefault::SUCCESS;
}
SignalDefault GenerateFunctions(GenInfo& info, ASTScope* body){
    using namespace engone;
    MEASURE;
    _GLOG(FUNC_ENTER)
    Assert(body || info.hasForeignErrors());
    if(!body) return SignalDefault::FAILURE;
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
    return SignalDefault::SUCCESS;
}
SignalDefault GenerateBody(GenInfo &info, ASTScope *body) {
    using namespace engone;
    MEASURE
    PROFILE_SCOPE
    _GLOG(FUNC_ENTER)
    Assert(body||info.hasForeignErrors());
    if(!body) return SignalDefault::FAILURE;

    MAKE_NODE_SCOPE(body); // no need, the scope itself doesn't generate code

    Bytecode::Dump debugDump{};
    if(body->flags & ASTNode::DUMP_ASM) {
        debugDump.dumpAsm = true;
    }
    if(body->flags & ASTNode::DUMP_BC) {
        debugDump.dumpBytecode = true;
    }
    debugDump.bc_startIndex = info.code->length();
    
    bool codeWasDisabled = info.disableCodeGeneration;
    bool errorsWasIgnored = info.ignoreErrors;
    ScopeId savedScope = info.currentScopeId;
    defer {
        info.currentScopeId = savedScope; 
        info.disableCodeGeneration = codeWasDisabled;
        info.ignoreErrors = errorsWasIgnored;
        if(debugDump.dumpAsm || debugDump.dumpBytecode) {
            debugDump.description = body->tokenRange.tokenStream()->streamName + ":"+std::to_string(body->tokenRange.firstToken.line);
            // debugDump.description = TrimDir(body->tokenRange.tokenStream()->streamName) + ":"+std::to_string(body->tokenRange.firstToken.line);
            debugDump.bc_endIndex = info.code->length();
            Assert(debugDump.bc_startIndex <= debugDump.bc_endIndex);
            info.code->debugDumps.add(debugDump);
        }
    };

    info.currentScopeId = body->scopeId;

    int lastOffset = info.currentFrameOffset;
    int savedMoment = info.saveStackMoment();

    for(auto it : body->namespaces) {
        SignalDefault result = GenerateBody(info, it);
    }

    for (auto statement : body->statements) {
        MAKE_NODE_SCOPE(statement);

        auto& fun = info.code->debugInformation->functions[info.debugFunctionIndex];
        fun.lines.add({});
        fun.lines.last().funcOffset = info.code->length();
        fun.lines.last().lineNumber = statement->tokenRange.firstToken.line;
        
        info.disableCodeGeneration = codeWasDisabled;
        info.ignoreErrors = errorsWasIgnored;
        if(statement->isNoCode()) {
            info.disableCodeGeneration = true;
            info.ignoreErrors = true;
        }
        if (statement->type == ASTStatement::ASSIGN) {
            _GLOG(SCOPE_LOG("ASSIGN"))
            
            auto& typesFromExpr = statement->versions_expressionTypes[info.currentPolyVersion];
            if(statement->firstExpression && typesFromExpr.size() < statement->varnames.size()) {
                if(!info.hasForeignErrors()){
                    // char msg[100];
                    ERR_SECTION(
                        ERR_HEAD(statement->tokenRange)
                        ERR_MSG("To many variables.")
                        // sprintf(msg,"%d variables",(int)statement->varnames.size());
                        ERR_LINE(statement->tokenRange, statement->varnames.size() << " variables");
                        // sprintf(msg,"%d return values",(int)typesFromExpr.size());
                        ERR_LINE(statement->firstExpression->tokenRange, typesFromExpr.size() << " return values");
                    )
                }
                continue;
            }

            // This is not an error we want. You should be able to do this.
            // if(statement->globalAssignment && statement->firstExpression && info.currentScopeId != info.ast->globalScopeId) {
            //     ERR_SECTION(
            //         ERR_HEAD(statement->tokenRange)
            //         ERR_MSG("Assigning a value to a global variable inside a local scope is not allowed. Either move the variable to the global scope or separate the declaration and assignment with expression.")
            //         ERR_LINE(statement->tokenRange,"bad")
            //         ERR_EXAMPLE(1,"global someName;")
            //         ERR_EXAMPLE(2,"someName = 3;")
            //     )
            //     continue;
            // }

            // for(int i = 0;i<(int)typesFromExpr.size();i++){
            //     TypeId typeFromExpr = typesFromExpr[i];
            //     if((int)statement->varnames.size() <= i){
            //         // _GLOG(log::out << log::LIME<<"just pop "<<info.ast->typeToString(rightType)<<"\n";)
            //         continue;
            //     }

            int frameOffsetOfLastVarname = 0;

            for(int i = 0;i<(int)statement->varnames.size();i++){
                auto& varname = statement->varnames[i];
                // _GLOG(log::out << log::LIME <<"assign pop "<<info.ast->typeToString(rightType)<<"\n";)
                
                // NOTE: Global variables refer to memory in data segment.
                //  Each declaration of a variable which is global will get it's own memory.
                //  This means that polymorphic implementations will have multiple global variables
                //  for the same name, referring to different data. This is fine, the variables don't
                //  collide since the variables exist within a scope. Global variables accessed within
                //  a polymorphic function is also fine since the variable's type will stay the same.

                Identifier* varIdentifier = varname.identifier;
                if(!varIdentifier){
                    Assert(info.hasForeignErrors());
                    // Assert(info.errors!=0); // there should have been errors
                    continue;
                }
                VariableInfo* varinfo = info.ast->getVariableByIdentifier(varIdentifier);
                if(!varinfo){
                    Assert(info.hasForeignErrors());
                    // Assert(info.errors!=0); // there should have been errors
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
                if (varname.declaration) {
                    if (varname.arrayLength>0){
                        Assert(("global arrays not implemented",!statement->globalAssignment));
                        // TODO: Fix arrays with static data
                        if(statement->firstExpression) {
                            ERR_SECTION(
                                ERR_HEAD(statement->firstExpression->tokenRange)
                                ERR_MSG("An expression is not allowed when declaring an array on the stack. The array is zero-initialized by default.")
                                ERR_LINE(statement->firstExpression->tokenRange, "bad")
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
                            TypeInfo *typeInfo = info.ast->getTypeInfo(varinfo->versions_typeId[info.currentPolyVersion].baseType());
                            TypeId elementType = typeInfo->structImpl->polyArgs[0];
                            if(!elementType.isValid())
                                continue; // error message should have been printed in type checker
                            i32 elementSize = info.ast->getTypeSize(elementType);
                            // i32 asize2 = info.ast->getTypeAlignedSize(elementType);
                            int arraySize = elementSize * varname.arrayLength;
                            
                            // Assert(size2 * varname.arrayLength <= pow(2,16)/2);
                            if(arraySize > pow(2,16)/2) {
                                // std::string msg = std::to_string(size2) + " * "+ std::to_string(varname.arrayLength) +" = "+std::to_string(arraySize);
                                ERR_SECTION(
                                    ERR_HEAD(statement->tokenRange)
                                    ERR_MSG((int)(pow(2,16)/2-1) << " is the maximum size of arrays on the stack. "<<(arraySize)<<" was used which exceeds that. The limit comes from the instruction BC_INCR which uses a signed 16-bit integer.")
                                    ERR_LINE(statement->tokenRange, elementSize << " * " << std::to_string(varname.arrayLength) << " = " << std::to_string(arraySize))
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
                            if(i == (int)statement->varnames.size()-1){
                                frameOffsetOfLastVarname = arrayFrameOffset;
                            }

                            #ifndef DISABLE_ZERO_INITIALIZATION
                            // info.addLoadIm(BC_REG_RDX,arraySize);
                            // info.addInstruction({BC_MEMZERO, BC_REG_SP, BC_REG_RDX});
                            info.addInstruction({BC_MOV_RR, BC_REG_SP, BC_REG_RDI});
                            GenMemzero(info, BC_REG_RDI, BC_REG_RBX, arraySize);
                            #endif
                            TypeInfo* elementInfo = info.ast->getTypeInfo(elementType);
                            if(elementInfo->astStruct) {
                                // TODO: Annotation to disable this
                                // TODO: Create a loop with cmp, je, jmp instructions instead of
                                //  "unrolling" the loop like this. We generate a lot of instructions from this.
                                for(int j = 0;j<varname.arrayLength;j++) {
                                    SignalDefault result = GenerateDefaultValue(info, BC_REG_FP, arrayFrameOffset + elementSize * j, elementType);
                                    if(result!=SignalDefault::SUCCESS)
                                        return SignalDefault::FAILURE;
                                }
                            } else {
                                
                            }
                        }
                        // data type may be zero if it wasn't specified during initial assignment
                        // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                        // int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
                        // if (diff != asize) {
                        //     info.currentFrameOffset -= diff; // align
                        // }
                        // info.currentFrameOffset -= size;
                        // var->frameOffset = info.currentFrameOffset;

                        
                        SignalDefault result = FramePush(info,varinfo->versions_typeId[info.currentPolyVersion],&varinfo->versions_dataOffset[info.currentPolyVersion],false, varinfo->isGlobal());

                        // TODO: Don't hardcode this slice stuff, maybe I have to.
                        // push length
                        info.addLoadIm(BC_REG_RDX,varname.arrayLength);
                        info.addPush(BC_REG_RDX);

                        // push ptr
                        info.addLoadIm(BC_REG_RBX,arrayFrameOffset);
                        info.addInstruction({BC_ADDI,BC_REG_RBX, BC_REG_FP, BC_REG_RBX});
                        info.addPush(BC_REG_RBX);

                        GeneratePop(info, BC_REG_FP, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);

                        auto& fun = info.code->debugInformation->functions[info.debugFunctionIndex];
                        fun.localVariables.add({});
                        fun.localVariables.last().name = varname.name;
                        fun.localVariables.last().frameOffset = varinfo->versions_dataOffset[info.currentPolyVersion];
                        fun.localVariables.last().typeId = varinfo->versions_typeId[info.currentPolyVersion];
                    } else {
                        if(!varinfo->isGlobal()) {
                            // address of global variables is managed in type checker
                            SignalDefault result = FramePush(info, varinfo->versions_typeId[info.currentPolyVersion], &varinfo->versions_dataOffset[info.currentPolyVersion],
                                !statement->firstExpression, false);

                            auto& fun = info.code->debugInformation->functions[info.debugFunctionIndex];
                            fun.localVariables.add({});
                            fun.localVariables.last().name = varname.name;
                            fun.localVariables.last().frameOffset = varinfo->versions_dataOffset[info.currentPolyVersion];
                            fun.localVariables.last().typeId = varinfo->versions_typeId[info.currentPolyVersion];
                        }
                    }
                    _GLOG(log::out << "declare " << (varinfo->isGlobal()?"global ":"")<< varname.name << " at " << varinfo->versions_dataOffset[info.currentPolyVersion] << "\n";)
                    // NOTE: inconsistent
                    // char buf[100];
                    // int len = sprintf(buf," ^ was assigned %s",statement->name->c_str());
                    // info.code->addDebugText(buf,len);
                }
                if(varinfo){
                    _GLOG(log::out << " " << varname.name << " : " << info.ast->typeToString(varinfo->versions_typeId[info.currentPolyVersion]) << "\n";)
                }
            }
            // TINY_ARRAY(TypeId, rightTypes, 3)
            DynamicArray<TypeId> rightTypes{};
            if (statement->arrayValues.size()){
                auto& varname = statement->varnames.last();
                TypeInfo* sometypeInfo = info.ast->getTypeInfo(varname.versions_assignType[info.currentPolyVersion].baseType());
                TypeId elementType = sometypeInfo->structImpl->polyArgs[0];
                int elementSize = info.ast->getTypeSize(elementType);
                for(int j=0;j<(int)statement->arrayValues.size();j++){
                    ASTExpression* value = statement->arrayValues[j];

                    rightTypes.resize(0);
                    SignalDefault result = GenerateExpression(info, value, &rightTypes);
                    if (result != SignalDefault::SUCCESS) {
                        continue;
                    }

                    if(rightTypes.size()!=1) {
                        Assert(info.hasForeignErrors());
                        continue; // error handled in type checker
                    }

                    // TypeId stateTypeId = varname.versions_assignType[info.currentPolyVersion];
                    Identifier* id = varname.identifier;
                    VariableInfo* varinfo = info.ast->getVariableByIdentifier(id);
                    if(!varinfo){
                        Assert(info.errors!=0); // there should have been errors
                        continue;
                    }

                    if(!PerformSafeCast(info, rightTypes[0], elementType)){
                        Assert(!info.hasForeignErrors());
                        continue;
                    }
                    switch(varinfo->type) {
                        case VariableInfo::GLOBAL: {
                            Assert(false); // broken with arrays
                            // info.addInstruction({BC_DATAPTR, BC_REG_RBX});
                            // info.addImm(varinfo->versions_dataOffset[info.currentPolyVersion]);
                            // GeneratePop(info, BC_REG_RBX, 0, varinfo->versions_typeId[info.currentPolyVersion]);
                            break; 
                        }
                        case VariableInfo::LOCAL: {
                            // info.addLoadIm(BC_REG_RBX, varinfo->versions_dataOffset[info.currentPolyVersion]);
                            // info.addInstruction({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX});
                            GeneratePop(info, BC_REG_FP, frameOffsetOfLastVarname + j * elementSize, elementType);
                            break;
                        }
                        case VariableInfo::MEMBER: {
                            Assert(false); // broken with arrays, this should probably not be allowed
                            // Assert(info.currentFunction && info.currentFunction->parentStruct);
                            // // TODO: Verify that  you
                            // // NOTE: Is member variable/argument always at this offset with all calling conventions?
                            // info.addInstruction({BC_MOV_MR_DISP32, BC_REG_FP, BC_REG_RBX, 8});
                            // info.addImm(GenInfo::FRAME_SIZE);
                            
                            // // info.addLoadIm(BC_REG_RAX, varinfo->versions_dataOffset[info.currentPolyVersion]);
                            // // info.addInstruction({BC_ADDI, BC_REG_RBX, BC_REG_RAX, BC_REG_RBX});
                            // GeneratePop(info, BC_REG_RBX, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);
                            break;
                        }
                        default: {
                            Assert(false);
                        }
                    }
                }
            }
            if(statement->firstExpression){
                SignalDefault result = GenerateExpression(info, statement->firstExpression, &rightTypes);
                if (result != SignalDefault::SUCCESS) {
                    // assign fails and variable will not be declared potentially causing
                    // undeclared errors in later code. This is probably better than
                    // declaring the variable and using void type. Then you get type mismatch
                    // errors.
                    continue;
                }
                // Type checker or generator has a bug if they check/generate different types
                Assert(typesFromExpr.size()==rightTypes.size());
                bool cont = false;
                for(int i=0;i<(int)typesFromExpr.size();i++){
                    std::string a0 = info.ast->typeToString(typesFromExpr[i]);
                    std::string a1 = info.ast->typeToString(rightTypes[i]);
                    // Assert(typesFromExpr[i] == rightTypes[i]);
                    if(typesFromExpr[i] != rightTypes[i]) {
                        Assert(info.hasForeignErrors());
                        cont=true;
                        continue;
                    }
                }
                if(cont) continue;
                for(int i = (int)typesFromExpr.size()-1;i>=0;i--){
                    TypeId typeFromExpr = typesFromExpr[i];
                    if((int)statement->varnames.size() <= i){
                        _GLOG(log::out << log::LIME<<"just pop "<<info.ast->typeToString(typeFromExpr)<<"\n";)
                        GeneratePop(info, 0, 0, typeFromExpr);
                        continue;
                    }
                    auto& varname = statement->varnames[i];
                    _GLOG(log::out << log::LIME <<"assign pop "<<info.ast->typeToString(typeFromExpr)<<"\n";)
                    
                    TypeId stateTypeId = varname.versions_assignType[info.currentPolyVersion];
                    Identifier* id = varname.identifier;

                    // Token* name = &statement->varnames[i].name;
                    // auto id = info.ast->findIdentifier(info.currentScopeId, *name);
                    if(!id){
                        // there should have been errors
                        Assert(info.hasForeignErrors());
                        continue;
                    }
                    VariableInfo* varinfo = info.ast->getVariableByIdentifier(id);
                    if(!varinfo){
                        Assert(info.errors!=0); // there should have been errors
                        continue;
                    }
                    // if(varinfo->versions_typeId[info.currentPolyVersion] == AST_VOID) {
                        
                    // }

                    if(!PerformSafeCast(info, typeFromExpr, varinfo->versions_typeId[info.currentPolyVersion])){
                        if(!info.hasForeignErrors()){
                            ERRTYPE1(statement->tokenRange, typeFromExpr, varinfo->versions_typeId[info.currentPolyVersion], "(assign)."
                                // ERR_LINE(statement->tokenRange,"bad");
                            )
                        }
                        continue;
                    }
                    // Assert(!var->globalData || info.currentScopeId == info.ast->globalScopeId);
                    switch(varinfo->type) {
                        case VariableInfo::GLOBAL: {
                            info.addInstruction({BC_DATAPTR, BC_REG_RBX});
                            info.addImm(varinfo->versions_dataOffset[info.currentPolyVersion]);
                            GeneratePop(info, BC_REG_RBX, 0, varinfo->versions_typeId[info.currentPolyVersion]);
                            break; 
                        }
                        case VariableInfo::LOCAL: {
                            // info.addLoadIm(BC_REG_RBX, varinfo->versions_dataOffset[info.currentPolyVersion]);
                            // info.addInstruction({BC_ADDI, BC_REG_FP, BC_REG_RBX, BC_REG_RBX});
                            GeneratePop(info, BC_REG_FP, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);
                            break;
                        }
                        case VariableInfo::MEMBER: {
                            Assert(info.currentFunction && info.currentFunction->parentStruct);
                            // TODO: Verify that  you
                            // NOTE: Is member variable/argument always at this offset with all calling conventions?
                            info.addInstruction({BC_MOV_MR_DISP32, BC_REG_FP, BC_REG_RBX, 8});
                            info.addImm(GenInfo::FRAME_SIZE);
                            
                            // info.addLoadIm(BC_REG_RAX, varinfo->versions_dataOffset[info.currentPolyVersion]);
                            // info.addInstruction({BC_ADDI, BC_REG_RBX, BC_REG_RAX, BC_REG_RBX});
                            GeneratePop(info, BC_REG_RBX, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);
                            break;
                        }
                        default: {
                            Assert(false);
                        }
                    }
                }
            }
        }
        else if (statement->type == ASTStatement::IF) {
            _GLOG(SCOPE_LOG("IF"))

            TypeId dtype = {};
            SignalDefault result = GenerateExpression(info, statement->firstExpression, &dtype);
            if (result != SignalDefault::SUCCESS) {
                // generate body anyway or not?
                continue;
            }
            // if(dtype!=AST_BOOL8){
            //     ERR_HEAD2(statement->expression->token) << "Expected a boolean, not '"<<TypeIdToStr(dtype)<<"'\n";
            //     continue;
            // }
            Assert(dtype.isValid()); // We expect a good type, otherwise result should have been a failure
            u32 size = info.ast->getTypeSize(dtype);
            Assert(size != 0);

            if(size > 8) {
                ERR_SECTION(
                    ERR_HEAD(statement->firstExpression->tokenRange)
                    ERR_MSG("The type '"<<info.ast->typeToString(dtype)<<"' in this expression was bigger than 8 bytes and can't be used in an if statement.")
                    ERR_LINE(statement->firstExpression->tokenRange, size << " bytes is to much")
                )
                GeneratePop(info, 0, 0, dtype);
                continue;
            }

            u8 reg = RegBySize(BC_AX, size);

            info.addPop(reg);
            info.addInstruction({BC_JNE, reg});
            u32 skipIfBodyIndex = info.code->length();
            info.addImm(0);

            result = GenerateBody(info, statement->firstBody);
            if (result != SignalDefault::SUCCESS)
                continue;

            u32 skipElseBodyIndex = -1;
            if (statement->secondBody) {
                info.addInstruction({BC_JMP});
                skipElseBodyIndex = info.code->length();
                info.addImm(0);
            }

            // fix address for jump instruction
            info.code->getIm(skipIfBodyIndex) = info.code->length();

            if (statement->secondBody) {
                result = GenerateBody(info, statement->secondBody);
                if (result != SignalDefault::SUCCESS)
                    continue;

                info.code->getIm(skipElseBodyIndex) = info.code->length();
            }
        } else if (statement->type == ASTStatement::SWITCH) {
            _GLOG(SCOPE_LOG("SWITCH"))

            GenInfo::LoopScope* loopScope = info.pushLoop();
            defer {
                _GLOG(log::out << "pop loop\n");
                bool yes = info.popLoop();
                if(!yes){
                    log::out << log::RED << "popLoop failed (bug in compiler)\n";
                }
            };

            _GLOG(log::out << "push loop\n");
            loopScope->stackMoment = info.saveStackMoment();

            TypeId exprType{};
            if(statement->versions_expressionTypes[info.currentPolyVersion].size()!=0)
                exprType = statement->versions_expressionTypes[info.currentPolyVersion][0];
            else
                { Assert(false); }
                // continue; // error message printed already?

            i32 switchValueOffset = 0; // frame offset
            FramePush(info, exprType, &switchValueOffset,false, false);
            
            TypeId dtype = {};
            SignalDefault result = GenerateExpression(info, statement->firstExpression, &dtype);
            if (result != SignalDefault::SUCCESS) {
                continue;
            }
            Assert(exprType == dtype);
            auto* typeInfo = info.ast->getTypeInfo(dtype);
            Assert(typeInfo);
            
            if(AST::IsInteger(dtype) || typeInfo->astEnum) {
                
            } else {
                ERR_SECTION(
                    ERR_HEAD(statement->tokenRange)
                    ERR_MSG("You can only do switch on integers and enums.")
                    ERR_LINE(statement->tokenRange, "bad")
                )
                continue;
            }
            u32 switchExprSize = info.ast->getTypeSize(dtype);
            if (!switchExprSize) {
                // TODO: Print error? or does generate expression do that since it gives us dtype?
                continue;
            }
            u8 switchValueReg = RegBySize(BC_DX, switchExprSize);
            
            // TODO: Optimize instruction generation.
            
            info.addPop(switchValueReg);
            info.addInstruction({BC_MOV_RM_DISP32, switchValueReg, BC_REG_FP, (u8)switchExprSize});
            info.addImm(switchValueOffset);
            
            struct RelocData {
                u32 caseJumpAddress = 0;
                u32 caseBreakAddress = 0;
            };
            DynamicArray<RelocData> caseData{};
            caseData.resize(statement->switchCases.size());
            
            for(int nr=0;nr<(int)statement->switchCases.size();nr++) {
                auto it = &statement->switchCases[nr];
                caseData[nr] = {};
                
                TypeId dtype = {};
                u32 size = 0;
                u8 caseValueReg = 0;
                bool wasMember = false;
                if(typeInfo->astEnum && it->caseExpr->typeId == AST_ID) {
                    
                    int index = -1;
                    bool yes = typeInfo->astEnum->getMember(it->caseExpr->name, &index);
                    if(yes) {
                        wasMember = true;
                        dtype = typeInfo->id;
                        
                        size = info.ast->getTypeSize(dtype);
                        Assert(size == 4); // 64-bit enum not implemented (neither is 8, 16 ), you need to load 64-bit immediate instead of 32-bit
                        caseValueReg = RegBySize(BC_AX, size);
                        
                        info.addLoadIm(caseValueReg, typeInfo->astEnum->members[index].enumValue);
                    }
                }
                if(!wasMember) {
                    SignalDefault result = GenerateExpression(info, it->caseExpr, &dtype);
                    if (result != SignalDefault::SUCCESS) {
                        continue;
                    }
                    size = info.ast->getTypeSize(dtype);
                    caseValueReg = RegBySize(BC_AX, size);
                    
                    info.addPop(caseValueReg);
                }
                // TODO: Don't generate this instruction if switchValueReg is untouched.
                info.addInstruction({BC_MOV_MR_DISP32, BC_REG_FP, switchValueReg, (u8)size});
                info.addImm(switchValueOffset);
                
                info.addInstruction({BC_EQ, switchValueReg, caseValueReg, caseValueReg});
                info.addInstruction({BC_JE, caseValueReg});
                caseData[nr].caseJumpAddress = info.code->length();
                info.addImm(0);
            }
            if(statement->firstBody) {
                result = GenerateBody(info, statement->firstBody);
                // if (result != SignalDefault::SUCCESS)
                //     continue;
            }
            info.addInstruction({BC_JMP});
            u32 noCaseJumpAddress = info.code->length();
            info.addImm(0);
            auto& list = statement->switchCases;
            for(int nr=0;nr<(int)statement->switchCases.size();nr++) {
                auto it = &statement->switchCases[nr];
                info.code->getIm(caseData[nr].caseJumpAddress) = info.code->length();
                
                
                // TODO: break statements in body should jump to here
                result = GenerateBody(info, it->caseBody);
                if (result != SignalDefault::SUCCESS)
                    continue;
                
                
                // implicit break
                info.addInstruction({BC_JMP});
                caseData[nr].caseBreakAddress = info.code->length();
                info.addImm(0);
            }
            
            info.code->getIm(noCaseJumpAddress) = info.code->length();
                
            for(int nr=0;nr<(int)statement->switchCases.size();nr++) {
                info.code->getIm(caseData[nr].caseBreakAddress) = info.code->length();
            }
            
            for (auto ind : loopScope->resolveBreaks) {
                info.code->getIm(ind) = info.code->length();
            }
            info.restoreStackMoment(loopScope->stackMoment);
            
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

            SignalDefault result{};
            if(statement->firstExpression) {
                TypeId dtype = {};
                result = GenerateExpression(info, statement->firstExpression, &dtype);
                if (result != SignalDefault::SUCCESS) {
                    // generate body anyway or not?
                    continue;
                }
                u32 size = info.ast->getTypeSize(dtype);
                u8 reg = RegBySize(BC_AX, size);

                info.addPop(reg);
                info.addInstruction({BC_JNE, reg});
                loopScope->resolveBreaks.add(info.code->length());
                info.addImm(0);
            } else {
                // infinite loop
            }

            result = GenerateBody(info, statement->firstBody);
            if (result != SignalDefault::SUCCESS)
                continue;

            info.addInstruction({BC_JMP});
            info.addImm(loopScope->continueAddress);

            for (auto ind : loopScope->resolveBreaks) {
                info.code->getIm(ind) = info.code->length();
            }

            if(!statement->firstExpression && loopScope->resolveBreaks.size() == 0) {
                // TODO: Should this error exist?
                ERR_SECTION(
                    ERR_HEAD(statement->tokenRange)
                    ERR_MSG("A true infinite loop without any break statements is not allowed. If this was intended, write 'while true' or put 'if false break' in the body of the loop.")
                    ERR_LINE(statement->tokenRange,"true infinite loop")
                )
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


            int stackBeforeLoop = info.saveStackMoment();
            int frameBeforeLoop = info.currentFrameOffset;

            // TODO: Save stack moment here?

            // body scope is used since the for loop's variables
            // shouldn't collide with the variables in the current scope.
            // not sure how well this works, we shall see.
            ScopeId scopeForVariables = statement->firstBody->scopeId;

            Assert(statement->varnames.size()==2);
            auto& varnameIt = statement->varnames[0];
            auto& varnameNr = statement->varnames[1];
            if(!varnameIt.versions_assignType[info.currentPolyVersion].isValid() || !varnameNr.versions_assignType[info.currentPolyVersion].isValid()){
                // error has probably been handled somewhere. no need to print again.
                continue;
            }
            if(!varnameNr.identifier || !varnameIt.identifier) {
                if(!info.hasForeignErrors()){
                    ERR_SECTION(
                        ERR_HEAD(statement->tokenRange)
                        ERR_MSG("Identifier '"<<(varnameNr.identifier ? (varnameIt.identifier ? StringBuilder{} << varnameIt.name : "") : (varnameIt.identifier ? (StringBuilder{} << varnameNr.name <<" and " << varnameIt.name) : StringBuilder{} << varnameNr.name))<<"' in for loop was null. Bug in compiler?")
                        ERR_LINE(statement->tokenRange, "")
                    )
                }
                continue;
            }
            auto varinfo_index = info.ast->getVariableByIdentifier(varnameNr.identifier);
            auto varinfo_item = info.ast->getVariableByIdentifier(varnameIt.identifier);

            if(statement->rangedForLoop){
                // TypeId itemtype = varname.versions_assignType[info.currentPolyVersion];
                // auto varinfo_index = info.ast->addVariable(scopeForVariables,"nr", nullptr);
                // auto iden = info.ast->findIdentifier(scopeForVariables,"nr");
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
                    FramePush(info, typeId, &varinfo_index->versions_dataOffset[info.currentPolyVersion],false, false);
                    varinfo_item->versions_dataOffset[info.currentPolyVersion] = varinfo_index->versions_dataOffset[info.currentPolyVersion];

                    TypeId dtype = {};
                    // Type should be checked in type checker and further down
                    // when extracting ptr and len. No need to check here.
                    SignalDefault result = GenerateExpression(info, statement->firstExpression, &dtype);
                    if (result != SignalDefault::SUCCESS) {
                        continue;
                    }
                    if(statement->isReverse()){
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
                    GeneratePop(info, BC_REG_FP, varinfo_index->versions_dataOffset[info.currentPolyVersion], typeId);
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
                SignalDefault result = GenerateExpression(info, statement->firstExpression, &dtype);
                if (result != SignalDefault::SUCCESS) {
                    continue;
                }
                u8 index_reg = BC_REG_ECX;
                u8 length_reg = BC_REG_EDX;

                if(statement->isReverse()){
                    info.addPop(length_reg); // range.now we care about
                    info.addPop(BC_REG_EAX);

                } else {
                    info.addPop(BC_REG_EAX); // throw range.now
                    info.addPop(length_reg); // range.end we care about
                }

                info.addInstruction({BC_MOV_MR_DISP32, BC_REG_FP, index_reg, 4});
                info.addImm(varinfo_index->versions_dataOffset[info.currentPolyVersion]);

                if(statement->isReverse()){
                    info.addInstruction({BC_INCR,index_reg, 0xFF, 0xFF}); // hexidecimal represent -1
                }else{
                    info.addInstruction({BC_INCR,index_reg,1});
                }
                
                info.addInstruction({BC_MOV_RM_DISP32, index_reg, BC_REG_FP, 4});
                info.addImm(varinfo_index->versions_dataOffset[info.currentPolyVersion]);

                if(statement->isReverse()){
                    // info.code->addDebugText("For condition (reversed)\n");
                    // info.addInstruction({BC_GTE,index_reg,length_reg,BC_REG_EAX});
                    info.addInstruction({BC_GTE,CMP_SINT_SINT,index_reg,length_reg});
                } else {
                    // info.code->addDebugText("For condition\n");
                    // info.addInstruction({BC_LT,index_reg,length_reg,BC_REG_EAX});
                    info.addInstruction({BC_LT,CMP_SINT_SINT,index_reg,length_reg});
                }
                // info.addInstruction({BC_JNE, BC_REG_EAX});
                info.addInstruction({BC_JNE, length_reg});
                // resolve end, not break, resolveBreaks is bad naming
                loopScope->resolveBreaks.add(info.code->length());
                info.addImm(0);

                result = GenerateBody(info, statement->firstBody);
                if (result != SignalDefault::SUCCESS)
                    continue;

                info.addInstruction({BC_JMP});
                info.addImm(loopScope->continueAddress);

                for (auto ind : loopScope->resolveBreaks) {
                    info.code->getIm(ind) = info.code->length();
                }
                
                // info.ast->removeIdentifier(scopeForVariables, "nr");
                // info.ast->removeIdentifier(scopeForVariables, itemvar);
            }else{
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

                    SignalDefault result = FramePush(info, varinfo_index->versions_typeId[info.currentPolyVersion], &varinfo_index->versions_dataOffset[info.currentPolyVersion], false, false);

                    if(statement->isReverse()){
                        TypeId dtype = {};
                        // Type should be checked in type checker and further down
                        // when extracting ptr and len. No need to check here.
                        SignalDefault result = GenerateExpression(info, statement->firstExpression, &dtype);
                        if (result != SignalDefault::SUCCESS) {
                            continue;
                        }
                        info.addPop(BC_REG_RDX); // throw ptr
                        info.addPop(BC_REG_RAX); // keep len
                        info.addInstruction({BC_CAST,CAST_UINT_SINT, BC_REG_RAX,BC_REG_EAX});
                    }else{
                        info.addLoadIm(BC_REG_RAX,-1);
                    }
                    info.addPush(BC_REG_RAX);

                    Assert(varinfo_index->versions_typeId[info.currentPolyVersion] == AST_INT64);

                    GeneratePop(info, BC_REG_FP,varinfo_index->versions_dataOffset[info.currentPolyVersion],varinfo_index->versions_typeId[info.currentPolyVersion]);
                }
                // Token& itemvar = varname.name;
                // TypeId itemtype = varname.versions_assignType[info.currentPolyVersion];
                // if(!itemtype.isValid()){
                //     // error has probably been handled somewhere. no need to print again.
                //     // if it wasn't and you just found out that things went wrong here
                //     // then sorry.
                //     continue;
                // }
                i32 itemsize = info.ast->getTypeSize(varnameIt.versions_assignType[info.currentPolyVersion]);
                // auto varinfo_item = info.ast->addVariable(scopeForVariables,itemvar);
                // auto varinfo_item = info.ast->getVariableByIdentifier(varname.identifier);
                //  info.ast->addVariable(scopeForVariables,itemvar);
                // varinfo_item->typeId = itemtype;
                // if(statement->pointer){
                //     varinfo_item->versions_typeId[info.currentPolyVersion].setPointerLevel(varinfo_item->typeId.getPointerLevel()+1);
                // }
                {
                    // TODO: Automate this code. Pushing and popping variables from the frame is used often and should be functions.
                    i32 asize = info.ast->getTypeAlignedSize(varinfo_item->versions_typeId[info.currentPolyVersion]);
                    if(asize==0)
                        continue;

                    SignalDefault result = FramePush(info, varinfo_item->versions_typeId[info.currentPolyVersion], &varinfo_item->versions_dataOffset[info.currentPolyVersion], true, false);
                }

                _GLOG(log::out << "push loop\n");
                loopScope->stackMoment = info.saveStackMoment();
                loopScope->continueAddress = info.code->length();

                // TODO: don't generate ptr and length everytime.
                // put them in a temporary variable or something.
                TypeId dtype = {};
                // info.code->addDebugText("Generate and push slice\n");
                SignalDefault result = GenerateExpression(info, statement->firstExpression, &dtype);
                if (result != SignalDefault::SUCCESS) {
                    continue;
                }
                TypeInfo* ti = info.ast->getTypeInfo(dtype);
                Assert(ti && ti->astStruct && ti->astStruct->name == "Slice");
                auto memdata_ptr = ti->getMember("ptr");
                auto memdata_len = ti->getMember("len");

                // NOTE: careful when using registers since you might use 
                //  one for multiple things. 
                u8 ptr_reg = RegBySize(BC_SI,info.ast->getTypeSize(memdata_ptr.typeId));
                u8 length_reg = RegBySize(BC_BX,info.ast->getTypeSize(memdata_len.typeId));
                u8 index_reg = BC_REG_RCX;

                // info.code->addDebugText("extract ptr and length\n");
                info.addPop(ptr_reg);
                info.addPop(length_reg);

                // info.code->addDebugText("Index increment/decrement\n");

                info.addInstruction({BC_MOV_MR_DISP32, BC_REG_FP, index_reg, DECODE_REG_SIZE(index_reg)});
                info.addImm(varinfo_index->versions_dataOffset[info.currentPolyVersion]);
                if(statement->isReverse()){
                    info.addInstruction({BC_INCR,index_reg, 0xFF, 0xFF}); // hexidecimal represent -1
                }else{
                    info.addInstruction({BC_INCR,index_reg,1});
                }
                info.addInstruction({BC_MOV_RM_DISP32, index_reg, BC_REG_FP, DECODE_REG_SIZE(index_reg)});
                info.addImm(varinfo_index->versions_dataOffset[info.currentPolyVersion]);

                // u8 cond_reg = BC_REG_EBX;
                // u8 cond_reg = BC_REG_R9;
                if(statement->isReverse()){
                    // info.code->addDebugText("For condition (reversed)\n");
                    info.addLoadIm(length_reg,-1); // length reg is not used with reversed
                    // info.addInstruction({BC_GT,index_reg,length_reg,BC_REG_RAX});
                    info.addInstruction({BC_GT,CMP_SINT_SINT,index_reg,length_reg});
                } else {
                    // info.code->addDebugText("For condition\n");
                    // info.addInstruction({BC_LT,index_reg,length_reg,BC_REG_RAX});
                    info.addInstruction({BC_LT,CMP_SINT_SINT,index_reg,length_reg});
                }
                // resolve end, not break, resolveBreaks is bad naming
                // info.addInstruction({BC_JNE, BC_REG_RAX});
                info.addInstruction({BC_JNE, length_reg});
                loopScope->resolveBreaks.add(info.code->length());
                info.addImm(0);

                // BE VERY CAREFUL SO YOU DON'T USE BUSY REGISTERS AND OVERWRITE
                // VALUES. UNEXPECTED VALUES WILL HAPPEN AND IT WILL BE PAINFUL.
                
                // NOTE: index_reg is modified here since it isn't needed anymore
                if(statement->isPointer()){
                    if(itemsize>1){
                        info.addLoadIm(BC_REG_RAX,itemsize);
                        info.addInstruction({BC_MULI, ARITHMETIC_SINT, index_reg, BC_REG_RAX});
                    } else {
                        info.addInstruction({BC_MOV_RR, index_reg, BC_REG_RAX});
                    }
                    info.addInstruction({BC_ADDI, ptr_reg, BC_REG_RAX, ptr_reg});

                    info.addInstruction({BC_MOV_RM_DISP32, ptr_reg, BC_REG_FP, 8});
                    info.addImm(varinfo_item->versions_dataOffset[info.currentPolyVersion]);
                } else {
                    if(itemsize>1){
                        info.addLoadIm(BC_REG_RAX,itemsize);
                        info.addInstruction({BC_MULI, ARITHMETIC_SINT, index_reg, BC_REG_RAX});
                    } else {
                        info.addInstruction({BC_MOV_RR, index_reg, BC_REG_RAX});
                    }
                    info.addInstruction({BC_ADDI, ptr_reg, BC_REG_RAX, ptr_reg});

                    info.addLoadIm(BC_REG_RDI,varinfo_item->versions_dataOffset[info.currentPolyVersion]);
                    info.addInstruction({BC_ADDI,BC_REG_RDI, BC_REG_FP, BC_REG_RDI});
                    
                    // info.addInstruction({BC_BXOR, BC_REG_RAX, BC_REG_RAX}); // BC_MEMCPY USES AL
                    
                    info.addLoadIm(BC_REG_RBX,itemsize);
                    // info.addInstruction({BC_MOV_RR, BC_REG_RAX, BC_REG_RBX}); // BC_MEMCPY USES AL
                    info.addInstruction({BC_MEMCPY,BC_REG_RDI, ptr_reg, BC_REG_RBX});
                    

                    // info.code->add({BC_MOV_RR, ptr_reg, BC_REG_RAX});
                    // info.code->add({BC_SUBI, BC_REG_RAX, BC_REG_RDI, BC_REG_RAX});
                }

                result = GenerateBody(info, statement->firstBody);
                if (result != SignalDefault::SUCCESS)
                    continue;

                info.addInstruction({BC_JMP});
                info.addImm(loopScope->continueAddress);

                for (auto ind : loopScope->resolveBreaks) {
                    info.code->getIm(ind) = info.code->length();
                }
                
                // delete nr, frameoffset needs to be changed to if so
                // info.addPop(BC_REG_EAX); 

                // info.ast->removeIdentifier(scopeForVariables, "nr");
                // info.ast->removeIdentifier(scopeForVariables, itemvar);
            }

            info.restoreStackMoment(stackBeforeLoop);
            info.currentFrameOffset = frameBeforeLoop;

            // pop loop happens in defer
        } else if(statement->type == ASTStatement::BREAK) {
            GenInfo::LoopScope* loop = info.getLoop(info.loopScopes.size()-1);
            if(!loop) {
                ERR_SECTION(
                    ERR_HEAD(statement->tokenRange)
                    ERR_MSG("Break is only allowed in loops.")
                    ERR_LINE(statement->tokenRange,"not in a loop")
                )
                continue;
            }
            info.restoreStackMoment(loop->stackMoment, true);
            
            info.addInstruction({BC_JMP});
            loop->resolveBreaks.add(info.code->length());
            info.addImm(0);
        } else if(statement->type == ASTStatement::CONTINUE) {
            GenInfo::LoopScope* loop = info.getLoop(info.loopScopes.size()-1);
            if(!loop) {
                ERR_SECTION(
                    ERR_HEAD(statement->tokenRange)
                    ERR_MSG("Continue is only allowed in loops.")
                    ERR_LINE(statement->tokenRange,"not in a loop")
                )
                continue;
            }
            info.restoreStackMoment(loop->stackMoment, true);

            info.addInstruction({BC_JMP});
            info.addImm(loop->continueAddress);
        } else if (statement->type == ASTStatement::RETURN) {
            _GLOG(SCOPE_LOG("RETURN"))

            if (!info.currentFunction) {
                ERR_SECTION(
                    ERR_HEAD(statement->tokenRange)
                    ERR_MSG("Return only allowed in function.")
                    ERR_LINE(statement->tokenRange, "bad")
                )
                continue;
                // return SignalDefault::FAILURE;
            }
            if ((int)statement->arrayValues.size() != (int)info.currentFuncImpl->returnTypes.size()) {
                // std::string msg = 
                ERR_SECTION(
                    ERR_HEAD(statement->tokenRange)
                    ERR_MSG("Found " << statement->arrayValues.size() << " return value(s) but should have " << info.currentFuncImpl->returnTypes.size() << " for '" << info.currentFunction->name << "'.")
                    ERR_LINE(info.currentFunction->tokenRange, "X return values")
                    ERR_LINE(statement->tokenRange, "Y values")
                )
            }

            //-- Evaluate return values
            switch(info.currentFunction->callConvention){
            case BETCALL: {
                for (int argi = 0; argi < (int)statement->arrayValues.size(); argi++) {
                    ASTExpression *expr = statement->arrayValues.get(argi);
                    // nextExpr = nextExpr->next;
                    // argi++;

                    TypeId dtype = {};
                    SignalDefault result = GenerateExpression(info, expr, &dtype);
                    if (result != SignalDefault::SUCCESS) {
                        continue;
                    }
                    if (argi < (int)info.currentFuncImpl->returnTypes.size()) {
                        // auto a = info.ast->typeToString(dtype);
                        // auto b = info.ast->typeToString(info.currentFuncImpl->returnTypes[argi].typeId);
                        auto& retType = info.currentFuncImpl->returnTypes[argi];
                        if (!PerformSafeCast(info, dtype, retType.typeId)) {
                            // if(info.currentFunction->returnTypes[argi]!=dtype){
                            ERRTYPE1(expr->tokenRange, dtype, info.currentFuncImpl->returnTypes[argi].typeId, "(return values)");
                            
                            GeneratePop(info, 0, 0, dtype);
                        } else {
                            GeneratePop(info, BC_REG_FP, retType.offset - info.currentFuncImpl->returnSize, retType.typeId);
                        }
                    } else {
                        // error here which has been printed somewhere
                        // but we should throw away values on stack so that
                        // we don't become desyncrhonized.
                        GeneratePop(info, 0, 0, dtype);
                    }
                }
                break;
            }
            case STDCALL: {
                for (int argi = 0; argi < (int)statement->arrayValues.size(); argi++) {
                    ASTExpression *expr = statement->arrayValues.get(argi);
                    // nextExpr = nextExpr->next;
                    // argi++;

                    TypeId dtype = {};
                    SignalDefault result = GenerateExpression(info, expr, &dtype);
                    if (result != SignalDefault::SUCCESS) {
                        continue;
                    }
                    if (argi < (int)info.currentFuncImpl->returnTypes.size()) {
                        // auto a = info.ast->typeToString(dtype);
                        // auto b = info.ast->typeToString(info.currentFuncImpl->returnTypes[argi].typeId);
                        auto& retType = info.currentFuncImpl->returnTypes[argi];
                        if (!PerformSafeCast(info, dtype, retType.typeId)) {
                            // if(info.currentFunction->returnTypes[argi]!=dtype){

                            ERRTYPE1(expr->tokenRange, dtype, info.currentFuncImpl->returnTypes[argi].typeId, "(return values)");

                            GeneratePop(info, 0, 0, dtype); // throw away value to prevent cascading bugs
                        }
                        u8 size = info.ast->getTypeSize(retType.typeId);
                        if(size<=8){
                            if(AST::IsDecimal(retType.typeId)) {
                                info.addPop(BC_REG_XMM0d);
                            } else {
                                info.addPop(BC_REG_RAX);
                            }
                        } else {
                            GeneratePop(info, 0, 0, retType.typeId); // throw away value to prevent cascading bugs
                        }
                    } else {
                        GeneratePop(info, 0, 0, dtype); // throw away value to prevent cascading bugs
                    }
                }
                if(statement->arrayValues.size()==0){
                    info.addInstruction({BC_BXOR, BC_REG_RAX,BC_REG_RAX,BC_REG_RAX});
                }
                break;
            }
            case UNIXCALL: {
                for (int argi = 0; argi < (int)statement->arrayValues.size(); argi++) {
                    ASTExpression *expr = statement->arrayValues.get(argi);
                    // nextExpr = nextExpr->next;
                    // argi++;

                    TypeId dtype = {};
                    SignalDefault result = GenerateExpression(info, expr, &dtype);
                    if (result != SignalDefault::SUCCESS) {
                        continue;
                    }
                    if (argi < (int)info.currentFuncImpl->returnTypes.size()) {
                        // auto a = info.ast->typeToString(dtype);
                        // auto b = info.ast->typeToString(info.currentFuncImpl->returnTypes[argi].typeId);
                        auto& retType = info.currentFuncImpl->returnTypes[argi];
                        if (!PerformSafeCast(info, dtype, retType.typeId)) {
                            // if(info.currentFunction->returnTypes[argi]!=dtype){

                            ERRTYPE1(expr->tokenRange, dtype, info.currentFuncImpl->returnTypes[argi].typeId, "(return values)");

                            GeneratePop(info, 0, 0, dtype); // throw away value to prevent cascading bugs
                        }
                        u8 size = info.ast->getTypeSize(retType.typeId);
                        if(size<=8){
                            if(AST::IsDecimal(retType.typeId)) {
                                info.addPop(BC_REG_XMM0d);
                            } else {
                                info.addPop(BC_REG_RAX);
                            }
                        } else {
                            GeneratePop(info, 0, 0, retType.typeId); // throw away value to prevent cascading bugs
                        }
                    } else {
                        GeneratePop(info, 0, 0, dtype); // throw away value to prevent cascading bugs
                    }
                }
                if(statement->arrayValues.size()==0){
                    info.addInstruction({BC_BXOR, BC_REG_RAX,BC_REG_RAX,BC_REG_RAX});
                }
                break;
            }
            default: {
                INCOMPLETE
            }
            }

            // fix stack pointer before returning
            // info.addIncrSp(-info.currentFrameOffset);
            // info.restoreStackMoment(-8 - 8, true); // -8 to not include BC_REG_FP
            info.restoreStackMoment(GenInfo::VIRTUAL_STACK_START - 8, true); // -8 to not include BC_REG_FP
            info.addInstruction({BC_POP, BC_REG_FP},true);
            info.addInstruction({BC_RET});
            info.currentFrameOffset = lastOffset;
        }
        else if (statement->type == ASTStatement::EXPRESSION) {
            _GLOG(SCOPE_LOG("EXPRESSION"))

            DynamicArray<TypeId> exprTypes{};
            SignalDefault result = GenerateExpression(info, statement->firstExpression, &exprTypes);
            if (result != SignalDefault::SUCCESS) {
                continue;
            }
            if(exprTypes.size() > 0 && exprTypes[0] != AST_VOID){
                for(int i = 0; i < (int) exprTypes.size();i++) {
                    TypeId dtype = exprTypes[i];
                    GeneratePop(info, 0, 0, dtype);
                }
            }
        }
        else if (statement->type == ASTStatement::USING) {
            _GLOG(SCOPE_LOG("USING"))
            Assert(false); // TODO: Fix this code and error messages
            Assert(statement->varnames.size()==1);

            Token* name = &statement->varnames[0].name;

            // auto origin = info.ast->findIdentifier(info.currentScopeId, *name);
            // if(!origin){
            //     ERR_HEAD2(statement->tokenRange) << *name << " is not a variable (using)\n";
                
            //     return SignalDefault::FAILURE;
            // }
            // Fix something with content order? Is something fixed in type checker? what?
            // auto aliasId = info.ast->addIdentifier(info.currentScopeId, *statement->alias);
            // if(!aliasId){
            //     ERR_HEAD2(statement->tokenRange) << *statement->alias << " is already a variable or alias (using)\n";
                
            //     return SignalDefault::FAILURE;
            // }

            // aliasId->type = origin->type;
            // aliasId->varIndex = origin->varIndex;
        } else if (statement->type == ASTStatement::DEFER) {
            _GLOG(SCOPE_LOG("DEFER"))

            SignalDefault result = GenerateBody(info, statement->firstBody);
            // Is it okay to do nothing with result?
        } else if (statement->type == ASTStatement::BODY) {
            _GLOG(SCOPE_LOG("BODY (statement)"))

            SignalDefault result = GenerateBody(info, statement->firstBody);
            // Is it okay to do nothing with result?
        } else if (statement->type == ASTStatement::TEST) {
            int moment = info.saveStackMoment();
            DynamicArray<TypeId> exprTypes{};
            SignalDefault result = GenerateExpression(info, statement->testValue, &exprTypes);
            if(exprTypes.size() != 1) {
                ERR_SECTION(
                    ERR_HEAD(statement->testValue->tokenRange)
                    ERR_MSG("The expression in test statements must consist of 1 type.")
                    ERR_LINE(statement->testValue->tokenRange,exprTypes.size()<<" types")
                )
                continue;
            }
            int size = info.ast->getTypeSize(exprTypes.last());
            if(size > 8 ) {
                ERR_SECTION(
                    ERR_HEAD(statement->testValue->tokenRange)
                    ERR_MSG("Type cannot be larger than 8 bytes. Test statement doesn't handle larger structs.")
                    ERR_LINE(statement->testValue->tokenRange,"bad")
                )
                continue;
            }
            exprTypes.resize(0);
            result = GenerateExpression(info, statement->firstExpression, &exprTypes);
            if(exprTypes.size() != 1) {
                ERR_SECTION(
                    ERR_HEAD(statement->firstExpression->tokenRange)
                    ERR_MSG("The expression in test statements must consist of 1 type.")
                    ERR_LINE(statement->firstExpression->tokenRange,exprTypes.size()<<" types")
                )
                continue;
            }
            size = info.ast->getTypeSize(exprTypes.last());
            if(size > 8) {
                ERR_SECTION(
                    ERR_HEAD(statement->firstExpression->tokenRange)
                    ERR_MSG("Type cannot be larger than 8 bytes. Test statement doesn't handle larger structs.")
                    ERR_LINE(statement->testValue->tokenRange,"bad")
                )
                continue;
            }
            // TODO: Should the types match? u16 - i32 might be fine but f32 - u16 shouldn't be okay.
            info.addPop(BC_REG_RAX);
            info.addPop(BC_REG_RDX);
            
            info.addAlign(16); // TEST_VALUE calls stdcall functions which needs 16-byte alignment
            info.addInstruction({BC_TEST_VALUE, 8, BC_REG_RDX, BC_REG_RAX});
            int loc = info.compileInfo->compileOptions->addTestLocation(statement->tokenRange);
            info.addImm(loc);
            
            info.restoreStackMoment(moment);
        } else {
            Assert(("You forgot to implement statement type!",false));
        }
    }
    // Assert(varsToRemove.size()==0);
    // for(auto& name : varsToRemove){
    //     info.ast->removeIdentifier(body->scopeId,name);
    // }
    if (lastOffset != info.currentFrameOffset) {
        _GLOG(log::out << "fix sp when exiting body\n";)
        info.restoreStackMoment(savedMoment); // -8 to not include BC_REG_FP
        // info.code->addDebugText("fix sp when exiting body\n");
        // info.addIncrSp(info.currentFrameOffset);
        // info.addIncrSp(lastOffset - info.currentFrameOffset);
        // info.addInstruction({BC_ADDI,BC_REG_SP,BC_REG_RCX,BC_REG_SP});
        info.currentFrameOffset = lastOffset;
    } else {
        info.restoreStackMoment(savedMoment, false, true);
    }
    return SignalDefault::SUCCESS;
}
SignalDefault GeneratePreload(GenInfo& info) {
    u8 src_reg = BC_REG_RSI;
    u8 dst_reg = BC_REG_RDI;
    int polyVersion = 0;

    if(!info.varInfos[VAR_INFOS])
        return SignalDefault::SUCCESS;

    // Take pointer of type information arrays
    // and move into the global slices.
    info.addInstruction({BC_DATAPTR, src_reg});
    info.addImm(info.dataOffset_types);
    info.addInstruction({BC_DATAPTR, dst_reg});
    info.addImm(info.varInfos[VAR_INFOS]->versions_dataOffset[polyVersion]);
    info.addInstruction({BC_MOV_RM, src_reg, dst_reg, 8});
    
    info.addInstruction({BC_DATAPTR, src_reg});
    info.addImm(info.dataOffset_members);
    info.addInstruction({BC_DATAPTR, dst_reg});
    info.addImm(info.varInfos[VAR_MEMBERS]->versions_dataOffset[polyVersion]);
    info.addInstruction({BC_MOV_RM, src_reg, dst_reg, 8});

    info.addInstruction({BC_DATAPTR, src_reg});
    info.addImm(info.dataOffset_strings);
    info.addInstruction({BC_DATAPTR, dst_reg});
    info.addImm(info.varInfos[VAR_STRINGS]->versions_dataOffset[polyVersion]);
    info.addInstruction({BC_MOV_RM, src_reg, dst_reg, 8});
    return SignalDefault::SUCCESS;
}
// Generate data from the type checker
SignalDefault GenerateData(GenInfo& info) {
    using namespace engone;

    // type checker requested some space for global variables
    // (at the time of writing this at least)
    if(info.ast->preallocatedGlobalSpace())
        info.code->appendData(nullptr, info.ast->preallocatedGlobalSpace());

    // IMPORTANT: TODO: Some data like 64-bit integers needs alignment.
    //   Strings don's so it's fine for now but don't forget about fixing this.
    for(auto& pair : info.ast->_constStringMap) {
        Assert(pair.first.size()!=0);
        int offset = 0;
        if(pair.first.back()=='\0')
            offset = info.code->appendData(pair.first.data(), pair.first.length());
        else
            offset = info.code->appendData(pair.first.data(), pair.first.length() + 1); // +1 to include null termination, this is to prevent mistakes when using C++ functions which expect it.
        if(offset == -1){
            continue;
        }
        auto& constString = info.ast->getConstString(pair.second);
        constString.address = offset;
    }

    Identifier* identifiers[VAR_COUNT]{
        info.ast->findIdentifier(info.ast->globalScopeId, CONTENT_ORDER_MAX, "lang_typeInfos", true),
        info.ast->findIdentifier(info.ast->globalScopeId, CONTENT_ORDER_MAX, "lang_members", true),
        info.ast->findIdentifier(info.ast->globalScopeId, CONTENT_ORDER_MAX, "lang_strings", true),
    };

    if(identifiers[0] && identifiers[1] && identifiers[2]) {
        FORAN(identifiers) {
            Assert(identifiers[nr]->type == Identifier::VARIABLE);
            info.varInfos[nr] = info.ast->getVariableByIdentifier(identifiers[nr]);
            Assert(info.varInfos[nr] && info.varInfos[nr]->isGlobal());
            Assert(info.varInfos[nr]->versions_dataOffset._array.size() == 1);
        }

        // TODO: Optimize, don't append the types that aren't used in the code.
        //  Implement a feature like FuncImpl.usages
        // TODO: Optimize in general? Many cache misses. Probably need to change the 
        //  Type and AST structure though.

        int count_types = info.ast->_typeInfos.size();
        int count_members = 0;
        int count_stringdata = 0;
        for(int i=0;i<info.ast->_typeInfos.size();i++){
            auto ti = info.ast->_typeInfos[i];
            if(!ti)  continue;
            count_stringdata += ti->name.length() + 1; // +1 for null termination
            if(ti->structImpl) {
                count_members += ti->astStruct->members.size();
                for(int mi=0;mi<ti->astStruct->members.size();mi++){
                    auto& mem = ti->astStruct->members[mi];
                    count_stringdata += mem.name.length + 1; // +1 for null termination
                }
            }
        }
        info.code->ensureAlignmentInData(8); // just to be safe
        int off_typedata   = info.code->appendData(nullptr, count_types * sizeof(lang::TypeInfo));
        info.code->ensureAlignmentInData(8); // just to be safe
        int off_memberdata = info.code->appendData(nullptr, count_members * sizeof(lang::TypeMember));
        int off_stringdata = info.code->appendData(nullptr, count_stringdata);
        
        lang::TypeInfo* typedata = (lang::TypeInfo*)(info.code->dataSegment.data + off_typedata);
        lang::TypeMember* memberdata = (lang::TypeMember*)(info.code->dataSegment.data + off_memberdata);
        char* stringdata = (char*)(info.code->dataSegment.data + off_stringdata);

        // TODO: Zero initialize memory? Or use '_'?

        // log::out << "TypeInfo size " <<sizeof(lang::TypeInfo);

        u32 string_offset = 0;
        int member_count = 0;
        for(int i=0;i<info.ast->_typeInfos.size();i++){
            auto ti = info.ast->_typeInfos[i];
            if(!ti) { 
                typedata[i] = {}; // zero initialize
                // typedata[i].type = lang::Primitive::NONE;
                continue;
            }

            typedata[i].size = ti->getSize();
            if(ti->astEnum)                      typedata[i].type = lang::Primitive::ENUM;
            else if(ti->astStruct)               typedata[i].type = lang::Primitive::STRUCT;
            else if(AST::IsDecimal(ti->id))      typedata[i].type = lang::Primitive::DECIMAL;
            else if(AST::IsInteger(ti->id)) {    
                if(AST::IsSigned(ti->id))        typedata[i].type = lang::Primitive::SIGNED_INT;
                else                             typedata[i].type = lang::Primitive::UNSIGNED_INT;
            } else if(ti->id == AST_CHAR)        typedata[i].type = lang::Primitive::CHAR;
            else if(ti->id == AST_BOOL)          typedata[i].type = lang::Primitive::BOOL;
            else typedata[i].type = lang::Primitive::NONE; // compiler specific type like ast_typeid or ast_string, ast_fncall
            
            typedata[i].name.beg = string_offset;
            // The order of beg, end matters. string_offset is at beginning now
            strncpy(stringdata + string_offset, ti->name.c_str(), ti->name.length() + 1);
            string_offset += ti->name.length() + 1;
            // Now string_offset is at end. DON'T MOVE AROUND THE LINES!
            typedata[i].name.end = string_offset - 1; // -1 won't include null termination
            typedata[i].members.beg = 0; // set later
            typedata[i].members.end = 0;

            if(ti->structImpl) {
                typedata[i].members.beg = member_count;

                for(int mi=0;mi<ti->structImpl->members.size();mi++){
                    auto& mem = ti->structImpl->members[mi];
                    auto& ast_mem = ti->astStruct->members[mi];

                    memberdata[member_count].name.beg = string_offset;

                    memcpy(stringdata + string_offset, ast_mem.name.str, ast_mem.name.length);
                    stringdata[string_offset + ast_mem.name.length] = '\0';
                    string_offset += ast_mem.name.length + 1;

                    memberdata[member_count].name.end = string_offset - 1; // -1 won't include null termination

                    // TODO: Enum member

                    memberdata[member_count].type.index0 = mem.typeId._infoIndex0;
                    memberdata[member_count].type.index1 = mem.typeId._infoIndex1;
                    memberdata[member_count].type.ptr_level = mem.typeId.getPointerLevel();
                    memberdata[member_count].offset = mem.offset;

                    member_count++;
                }
                typedata[i].members.end = member_count;
            }
        }
        Assert(string_offset == count_stringdata && count_members == member_count);

        // TODO: Assert that the variables are of the right type

        int polyVersion = 0;

        lang::Slice* slice_types = (lang::Slice*)(info.code->dataSegment.data + info.varInfos[VAR_INFOS]->versions_dataOffset[polyVersion]);
        lang::Slice* slice_members = (lang::Slice*)(info.code->dataSegment.data + info.varInfos[VAR_MEMBERS]->versions_dataOffset[polyVersion]);
        lang::Slice* slice_strings = (lang::Slice*)(info.code->dataSegment.data + info.varInfos[VAR_STRINGS]->versions_dataOffset[polyVersion]);
        slice_types->len = count_types;
        slice_members->len = count_members;
        slice_strings->len = count_stringdata;
        info.dataOffset_types = off_typedata;
        info.dataOffset_members = off_memberdata;
        info.dataOffset_strings = off_stringdata;
        // log::out << "types "<<count_types<<", members "<<count_members << ", strings "<<count_stringdata <<"\n";

        // This is scrap
        // info.code->ptrDataRelocations.add({
        //     (u32)varInfos[VAR_INFOS]->versions_dataOffset[polyVersion],
        //     (u32)off_typedata});
        // info.code->ptrDataRelocations.add({
        //     (u32)varInfos[VAR_MEMBERS]->versions_dataOffset[polyVersion],
        //     (u32)off_memberdata});
        // info.code->ptrDataRelocations.add({
        //     (u32)varInfos[VAR_STRINGS]->versions_dataOffset[polyVersion],
        //     (u32)off_stringdata});
    }

    return SignalDefault::SUCCESS;
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
    // info.code->nativeRegistry = info.compileInfo->nativeRegistry;

    SignalDefault result = GenerateData(info);

    // TODO: No need to create debug information if the compile options
    //  doesn't specify it.
    info.code->debugInformation = DebugInformation::Create(ast);

    // TODO: Skip function generation if there are no functions.
    //   We would need to go through every scope to know that though.
    //   Maybe the type checker can inform the generator?
    // info.code->addDebugText("FUNCTION SEGMENT\n");
    info.code->setLocationInfo("FUNCTION SEGMENT");
    // _GLOG(log::out << "Jump to skip functions\n";)
    // info.code->add_notabug({BC_JMP});
    // int skipIndex = info.code->length();
    // info.addImm(0);
    // IMPORTANT: Functions are generated before the code because
    //  compile time execution will need these functions to exist.
    //  There is still a dependency problem if you use compile time execution
    //  in a function which uses a function which hasn't been generated yet.
    result = GenerateFunctions(info, info.ast->mainBody);
    // if(skipIndex == info.code->length() -1) {
    //     // skip jump instruction if no functions where generated
    //     // info.popInstructions(2); // pop JMP and immediate
    //     info.code->instructionSegment.used = 0;
    //     _GLOG(log::out << "Delete jump instruction\n";)
    // } else {
    //     info.code->getIm(skipIndex) = info.code->length();
    // }

    info.code->setLocationInfo("GLOBAL CODE SEGMENT");
    // info.code->addDebugText("GLOBAL CODE SEGMENT\n");

    // It is dangerous to take a pointer to an element of an array that may reallocate the elements
    // but the array should only reallocate when generating new functions which we have already done.
    // this function is the last function we are adding. Taking the pointer is therefore not dangerous.
    
    bool hasMain = false;
    for(int i=0;i<info.code->exportedSymbols.size();i++) {
        auto& sym = info.code->exportedSymbols[i];
        if(sym.name == "main") {
            hasMain = true;
            break;
        }
    }

    if(!hasMain) {
        auto di = info.code->debugInformation;
        info.debugFunctionIndex = di->functions.size();
        DebugInformation::Function* dfun = di->addFunction("main");
        info.code->addExportedSymbol("main", info.code->length());
        
        if(info.ast->mainBody->statements.size()>0) {
            dfun->fileIndex = di->addOrGetFile(info.ast->mainBody->statements[0]->tokenRange.tokenStream()->streamName);
        } else {
            dfun->fileIndex = di->addOrGetFile(info.compileInfo->compileOptions->sourceFile.text);
        }
        // TODO: You could create a funcImpl for main here BUT we don't need to because this main
        //  encapsulates the global code which doesn't have a function with arguments.
        dfun->funcImpl = nullptr;
        dfun->funcAst = nullptr;
        dfun->funcStart = info.code->length();

        info.currentPolyVersion = 0;
        info.virtualStackPointer = GenInfo::VIRTUAL_STACK_START;
        info.currentFrameOffset = 0;
        info.addPush(BC_REG_FP);
        info.addInstruction({BC_MOV_RR, BC_REG_SP, BC_REG_FP});
        
        dfun->codeStart = info.code->length();
        dfun->entry_line = info.ast->mainBody->tokenRange.firstToken.line;

        GeneratePreload(info);

        // TODO: What to do about result? nothing?
        result = GenerateBody(info, info.ast->mainBody);
        
        dfun->codeEnd = info.code->length();

        info.addPop(BC_REG_FP);

        info.addInstruction({BC_RET});
        dfun->funcEnd = info.code->length();
    }

    std::unordered_map<FuncImpl*, int> resolveFailures;
    for(auto& e : info.callsToResolve){
        auto& inst = info.code->get(e.bcIndex);
        // Assert(e.funcImpl->address != Identifier::ADDRESS_INVALID);
        if(e.funcImpl->address != FuncImpl::ADDRESS_INVALID){
            *((i32*)&inst) = e.funcImpl->address;
        } else {
            ERR_SECTION(
                log::out << log::RED << "Invalid function address for instruction["<<e.bcIndex << "]\n"
            )
            auto pair = resolveFailures.find(e.funcImpl);
            if(pair == resolveFailures.end())
                resolveFailures[e.funcImpl] = 1;
            else
                pair->second++;
        }
    }
    // if(resolveFailures.size()!=0 && !info.hasErrors()){
    if(resolveFailures.size()!=0){
        log::out << log::RED << "Invalid function resolutions:\n";
        for(auto& pair : resolveFailures){
            info.errors += pair.second;
            log::out << log::RED << " "<<pair.first->name <<": "<<pair.second<<" bad address(es)\n";
        }
    }
    info.callsToResolve.cleanup();

    info.compileInfo->compileOptions->compileStats.errors += info.errors;
    return info.code;
}