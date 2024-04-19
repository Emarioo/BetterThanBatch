#include "BetBat/Generator.h"
#include "BetBat/Compiler.h"

#undef ERRTYPE
#undef ERRTYPE1
#define ERRTYPE1(R, LT, RT, M) ERR_SECTION(ERR_HEAD2(R, ERROR_TYPE_MISMATCH) ERR_MSG("Type mismatch " << info.ast->typeToString(LT) << " - " << info.ast->typeToString(RT) << " " << M) ERR_LINE2(R,"expects "+info.ast->typeToString(RT)))

// #define ERRTYPE(L, R, LT, RT, M) ERR_SECTION(ERR_HEAD2(L, ERROR_TYPE_MISMATCH) ERR_MSG("Type mismatch " << info.ast->typeToString(LT) << " - " << info.ast->typeToString(RT) << " " << M) ERR_LINE2(L,info.ast->typeToString(LT)) ERR_LINE2(R,info.ast->typeToString(RT)))
#define ERRTYPE(L, R, LT, RT, M) ERR_SECTION(ERR_HEAD2(L, ERROR_TYPE_MISMATCH) ERR_MSG("Type mismatch " << ast->typeToString(LT) << " - " << ast->typeToString(RT) << " " << M) ERR_LINE2(L,ast->typeToString(LT)) ERR_LINE2(R,ast->typeToString(RT)))

#undef ERR_SECTION
#define ERR_SECTION(CONTENT) BASE_SECTION("Generator, ", CONTENT)

#undef LOGAT
#define LOGAT(R) R.firstToken.line << ":" << R.firstToken.column

#define MAKE_NODE_SCOPE(X) info.pushNode(dynamic_cast<ASTNode*>(X));NodeScope nodeScope{&info};

/* #region  */
GenContext::LoopScope* GenContext::pushLoop(){
    LoopScope* ptr = TRACK_ALLOC(LoopScope);
    new(ptr) LoopScope();
    if(!ptr)
        return nullptr;
    loopScopes.add(ptr);
    return ptr;
}
GenContext::LoopScope* GenContext::getLoop(int index){
    if(index < 0 || index >= (int)loopScopes.size()) return nullptr;
    return loopScopes[index];
}
bool GenContext::popLoop(){
    if(loopScopes.size()==0)
        return false;
    LoopScope* ptr = loopScopes.last();
    ptr->~LoopScope();
    // engone::Free(ptr, sizeof(LoopScope));
    TRACK_FREE(ptr, LoopScope);
    loopScopes.pop();
    return true;
}
void GenContext::pushNode(ASTNode* node){
    nodeStack.add(node);
}
void GenContext::popNode(){
    nodeStack.pop();
}
void GenContext::addCallToResolve(i32 bcIndex, FuncImpl* funcImpl){
    if(disableCodeGeneration) return;
    // if(bcIndex == 97) {
    //     __debugbreak();
    // }
    // ResolveCall tmp{};
    // tmp.bcIndex = bcIndex;
    // tmp.funcImpl = funcImpl;
    // callsToResolve.add(tmp);

    tinycode->addRelocation(bcIndex, funcImpl);
}
// void GenContext::addCall(LinkConventions linkConvention, CallConventions callConvention){
//     addInstruction({BC_CALL, (u8)linkConvention, (u8)callConvention},true);
// }
// void GenContext::popInstructions(u32 count){
//     Assert(code->length() >= count);

//     for(int i=0;i<count;i++) {
//         u32 index = indexOfNonImmediates.last();
//         Assert(index == code->instructionSegment.used-1); // We can't pop instructions with immediates. We need more complexity here to do that.
//         Instruction inst = code->instructionSegment.data[index];
        
//         // IMPORTANT: THERE IS A HIGH CHANCE OF A BUG HERE WITH VIRTUAL STACK POINTER.
//         switch(inst.opcode) {
//             case BCInstruction::BC_PUSH: {
//                 int size = 8; // fixed size
//                 WHILE_TRUE {
//                     if(!hasErrors()){
//                         Assert(("bug in compiler!", stackAlignment.size()!=0));
//                     }
//                     if(stackAlignment.size()!=0){
//                         auto align = stackAlignment.last();
//                         if(!hasErrors()){
//                             // size of 0 could mean extra alignment for between structs
//                             Assert(("bug in compiler!", align.size == size));
//                         }
//                         // You pushed some size and tried to pop a different size.
//                         // Did you copy paste some code involving addPop/addPush recently?
//                         virtualStackPointer += size;
//                         stackAlignment.pop();
//                         if (align.diff) {
//                             virtualStackPointer += align.diff;
//                             // code->addDebugText("align sp\n");
//                             i16 offset = align.diff;
//                             code->instructionSegment.used-=1;
//                             // addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
//                         }
//                         if(align.size == 0)
//                             continue;
//                     }
//                     break;
//                 }
//                 break;   
//             }
//             // case BCInstruction::BC_POP: {
                
//             //     break;
//             // }
//             default: {
//                 // We can't pop instructions that modify stack or base pointer because we need to revert virtualStackPointer and such.
//                 // That can get quite complex.
//                 Assert(false);
//                 break;   
//             }
//         }
        
//         code->instructionSegment.used-=count;
//         indexOfNonImmediates.used-=count;
//     }

//     code->instructionSegment.used-=count;
//     indexOfNonImmediates.used-=count;
//     if(code->instructionSegment.used>0 && count != 0){
//         for(int i = code->instructionSegment.used-1;i>=0;i--){
//             u32 locIndex = -1;
//             auto loc = code->getLocation(i, &locIndex);
//             if(loc){
//                 lastLine = loc->line;
//                 lastStream = (TokenStream*)loc->stream;
//                 lastLocationIndex = locIndex;
//                 break;
//             }
//         }
//     }
// }
// bool GenContext::addInstruction(Instruction inst, bool bypassAsserts){
//     using namespace engone;

//     // Assert(inst.op0 == CAST_SINT_SINT);

//     // This is here to prevent mistakes.
//     // the stack is kept track of behind the scenes with builder.emit_push, builder.emit_pop.
//     // using builder.emit_ would bypass that which would cause bugs.
//     if(!bypassAsserts){
//         Assert((inst.opcode!=BC_POP && inst.opcode!=BC_PUSH && inst.opcode != BC_CALL && inst.opcode != BC_LI));
//     }

//     if(inst.opcode == BC_MOV_MR || inst.opcode == BC_MOV_RM || inst.opcode == BC_MOV_MR_DISP32 || inst.opcode == BC_MOV_RM_DISP32) {
//         Assert(inst.op2 != 0);
//     }

//     if(code->length() == 693) {
//         int k = 923;
//     }
    
//     if(disableCodeGeneration) return true;

//     #ifdef OPTIMIZED
//     /*
//         Some of this code is kind of neat. It has a cascading effect where
//             li eax, push eax, pop ecx   becomes
//             li eax, mov eax, ecx        but then again
//             li ecx
//     */
//     // IMPORTANT: THE CODE DOESN'T EXPECT IMMEDIATES.
//     //  There should be a variable which can tell us the previous bytes are an immediate and if so
//     //  where the real instruction is.
//     //  I have disabled some optimizations to prevent this bug from happening untill I fix it..
//     if(indexOfNonImmediates.size()>0){
//         int index = indexOfNonImmediates.last();
//         Instruction instLast = code->get(index);
//         if(inst.opcode == BC_POP && instLast.opcode == BC_PUSH
//         && inst.op0 == instLast.op0) {
//             popInstructions(1);
//             _GLOG(log::out << log::GRAY << "(delete redundant push/pop)\n";)
//             return false;
//         }
//         // if(inst.opcode == BC_POP && instLast.opcode == BC_PUSH
//         // && DECODE_REG_SIZE(inst.op0) == DECODE_REG_SIZE(instLast.op0)) {
//         //     u8 op = instLast.op0;
//         //     popInstructions(1);
//         //     _GLOG(log::out << log::GRAY << "(delete redundant push/pop)\n";)
//         //     bool yes = addInstruction({BC_MOV_RR, op, inst.op0});
//         //     if(yes) {
//         //         _GLOG(log::out << log::GRAY << "(replaced with register move)\n";)
//         //     }
//         //     return false;
//         // }
//         // if(inst.opcode == BC_MOV_RR && inst.op0 == instLast.op0) {
//         //     u8 opcode = instLast.opcode;
//         //     u8* outOp = nullptr;
//         //     switch(opcode) {
//         //         // add, mul and other instructions don't work
//         //         // because output registers are also input registers
//         //         case BC_DATAPTR:
//         //         case BC_CODEPTR:
//         //         case BC_LI: {
//         //             outOp = &instLast.op0;
//         //             break;
//         //         }
//         //         case BC_MOV_MR:
//         //         case BC_MOV_MR_DISP32: {
//         //             outOp = &instLast.op1;
//         //             break;
//         //         }
//         //     }
//         //     if(outOp) {
//         //         *outOp = inst.op1;
//         //         _GLOG(log::out << log::GRAY << "(modified instruction)\n";)
//         //         _GLOG(code->printInstruction(index,true);)
//         //         return false;
//         //     }
//         // }
//     }
//     #endif

//     if(!hasErrors()) {
//         // FP (base pointer in x64) is callee-saved
//         Assert(inst.opcode != BC_RET || (code->length()>0 &&
//             code->get(code->length()-1).opcode == BC_POP &&
//             code->get(code->length()-1).op0 == BC_REG_BP));
//     }
//     indexOfNonImmediates.add(code->length());
//     code->add_notabug(inst);
//     // Assert(nodeStack.size()!=0); // can't assert because some instructions are added which doesn't link to any AST node.
//     if(nodeStack.size()==0)
//         return true;
//     if(nodeStack.last()->tokenRange.firstToken.line == lastLine &&
//         nodeStack.last()->tokenRange.firstToken.tokenStream == lastStream){
//         // log::out << "loc "<<lastLocationIndex<<"\n";
//         auto location = code->setLocationInfo(lastLocationIndex,code->length()-1);
//     } else {
//         lastLine = nodeStack.last()->tokenRange.firstToken.line;
//         lastStream = nodeStack.last()->tokenRange.firstToken.tokenStream;
//         auto location = code->setLocationInfo(nodeStack.last()->tokenRange.firstToken,code->length()-1, &lastLocationIndex);
//         // log::out << "new loc "<<lastLocationIndex<<"\n";
//         if(nodeStack.last()->tokenRange.tokenStream())
//             location->desc = nodeStack.last()->tokenRange.firstToken.getLine();
//     }
//     return true;
// }
// void GenContext::addLoadIm(u8 reg, i32 value){
//     addInstruction({BC_LI, reg}, true);
//     if(disableCodeGeneration) return;
//     code->addIm_notabug(value);
// }
// void GenContext::addLoadIm2(u8 reg, i64 value){
//     addInstruction({BC_LI, reg, 2}, true);
//     if(disableCodeGeneration) return;
//     code->addIm_notabug(value&0xFFFFFFFF);
//     code->addIm_notabug(value>>32);
// }
// void GenContext::addImm(i32 value){
//     if(disableCodeGeneration) return;
//     code->addIm_notabug(value);
// }
// void GenContext::addPop(int reg) {
//     using namespace engone;
//     int size = DECODE_REG_SIZE(reg);
//     if (size == 0 ) { // we don't print if we had errors since they probably caused size of 0
//         if(!hasErrors()){
//             Assert(("register had 0 zero",false));
//             log::out << log::RED << "GenContext::addPop : Cannot pop register with 0 size\n";
//         }
//         return;
//     }
//     size = 8; // NOTE: function popInstructions assume 8 in size
//     reg = RegBySize(DECODE_REG_TYPE(reg), size); // force 8 bytes

//     addInstruction({BC_POP, (u8)reg}, true);
//     // if errors != 0 then don't assert and just return since the stack
//     // is probably messed up because of the errors. OR you try
//     // to manage the stack even with errors. Unnecessary work though so don't!? 
//     WHILE_TRUE {
//         if(!hasErrors()){
//             Assert(("bug in compiler!", stackAlignment.size()!=0));
//         }
//         if(stackAlignment.size()!=0){
//             auto align = stackAlignment.last();
//             if(!hasErrors() && align.size!=0){
//                 // size of 0 could mean extra alignment for between structs
//                 Assert(("bug in compiler!", align.size == size));
//             }
//             // You pushed some size and tried to pop a different size.
//             // Did you copy paste some code involving addPop/addPush recently?
//             stackAlignment.pop();
//             if (align.diff != 0) {
//                 virtualStackPointer += align.diff;
//                 // code->addDebugText("align sp\n");
//                 i16 offset = align.diff;
//                 addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
//             }
//             if(align.size == 0)
//                 continue;
//         }
//         virtualStackPointer += size;
//         break;
//     }
//     _GLOG(log::out << "relsp "<<virtualStackPointer<<"\n";)
// }
// void GenContext::addPush(int reg, bool withoutInstruction) {
//     using namespace engone;
//     // Assert(!withoutInstruction);
//     // NOTE: When optimizing, we mess with the virtual stack pointer because we remove unnecessary pop/push instructions.
//     // I Don't know if withoutInstruction would cause bugs. - Emarioo, 2023-12-19
    
//     int size = DECODE_REG_SIZE(reg);
//     if (size == 0 ) { // we don't print if we had errors since they probably caused size of 0
//         if(errors == 0){
//             log::out << log::RED << "GenContext::addPush : Cannot push register with 0 size\n";
//         }
//         return;
//     }
//     size = 8;
//     reg = RegBySize(DECODE_REG_TYPE(reg), 8); // force 8 bytes

//     int diff = (size - (-virtualStackPointer) % size) % size; // how much to increment sp by to align it
//     // TODO: Instructions are generated from top-down and the stackAlignment
//     //   sees pop and push in this way but how about jumps. It doesn't consider this. Is it an issue?
//     if (diff) {
//         Assert(false); // shouldn't happen anymore
//         virtualStackPointer -= diff;
//         // code->addDebugText("align sp\n");
//         i16 offset = -diff;
//         addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
//     }
//     if(!withoutInstruction) {
//         addInstruction({BC_PUSH, (u8)reg}, true);
//     }
//     // BREAK(size == 16);
//     stackAlignment.add({diff, size});
//     virtualStackPointer -= size;
//     _GLOG(log::out << "relsp "<<virtualStackPointer<<"\n";)
// }
// void GenContext::addIncrSp(i16 offset) {
//     using namespace engone;
//     if (offset == 0)
//         return;
//     // Assert(offset>0); // TOOD: doesn't handle decrement of sp
//     if (offset > 0) {
//         int at = offset;
//         while (at > 0 && stackAlignment.size() > 0) {
//             auto align = stackAlignment.last();
//             // log::out << "pop stackalign "<<align.diff<<":"<<align.size<<"\n";
//             stackAlignment.pop();
//             at -= align.size;
//             // Assert(at >= 0);
//             // Assert doesn't work because diff isn't accounted for in offset.
//             // Asserting before at -= diff might not work either.

//             at -= align.diff;
//         }
//     }
//     else if (offset < 0) {
//         stackAlignment.add({0, -offset});
//     }
//     virtualStackPointer += offset;
//     addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
//     _GLOG(log::out << "relsp "<<virtualStackPointer<<"\n";)
// }
// void GenContext::addAlign(int alignment){
//     int diff = (alignment - (-virtualStackPointer) % alignment) % alignment; // how much to increment sp by to align it
//     // TODO: Instructions are generated from top-down and the stackAlignment
//     //   sees pop and push in this way but how about jumps. It doesn't consider this. Is it an issue?
//     if (diff) {
//         addIncrSp(-diff);
//         // virtualStackPointer -= diff;
//         // code->addDebugText("align sp\n");
//         // i16 offset = -diff;
//         // code->add({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
//     }
// }
// int GenContext::saveStackMoment() {
//     return virtualStackPointer;
// }

// void GenContext::restoreStackMoment(int moment, bool withoutModification, bool withoutInstruction) {
//     using namespace engone;
//     int offset = moment - virtualStackPointer;
//     if (offset == 0)
//         return;
//     if(!withoutModification || withoutInstruction) {
//         int at = moment - virtualStackPointer;
//         while (at > 0 && stackAlignment.size() > 0) {
//             auto align = stackAlignment.last();
//             // log::out << "pop stackalign "<<align.diff<<":"<<align.size<<"\n";
//             stackAlignment.pop();
//             at -= align.size;
//             at -= align.diff;
//             if(!hasErrors()) {
//                 Assert(at >= 0);
//             }
//         }
//         virtualStackPointer = moment;
//     } 
//     // else {
//     //     _GLOG(log::out << "relsp "<<moment<<"\n";)
//     // }
//     if(!withoutInstruction){
//         addInstruction({BC_INCR, BC_REG_SP, (u8)(0xFF & offset), (u8)(offset >> 8)});
//     }
//     if(withoutModification) {
//         _GLOG(log::out << "relsp (temp) "<<moment<<"\n";)
//     } else {
//         _GLOG(log::out << "relsp "<<moment<<"\n";)
//     }
// }
/* #endregion */
// Will perform cast on float and integers with pop, cast, push
// uses register A
// TODO: handle struct cast?
bool GenContext::performSafeCast(TypeId from, TypeId to) {
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
    auto from_size = ast->getTypeSize(from);
    auto to_size = ast->getTypeSize(to);
    BCRegister reg = BC_REG_T0;
    if (AST::IsDecimal(from) && AST::IsInteger(to)) {
        builder.emit_pop(reg);
        // if(AST::IsSigned(to))
        builder.emit_cast(reg, CAST_FLOAT_SINT, from_size, to_size);
            // builder.emit_({BC_CAST, CAST_FLOAT_UINT, reg0, reg1});
        builder.emit_push(reg);
        return true;
    }
    if (AST::IsInteger(from) && AST::IsDecimal(to)) {
        builder.emit_pop(reg);
        if(AST::IsSigned(from))
            builder.emit_cast(reg, CAST_SINT_FLOAT, from_size, to_size);
        else
            builder.emit_cast(reg, CAST_UINT_FLOAT, from_size, to_size);
        builder.emit_push(reg);
        return true;
    }
    if (AST::IsDecimal(from) && AST::IsDecimal(to)) {
        builder.emit_pop(reg);
        builder.emit_cast(reg, CAST_FLOAT_FLOAT, from_size, to_size);
        builder.emit_push(reg);
        return true;
    }
    if ((AST::IsInteger(from) && to == AST_CHAR) ||
        (from == AST_CHAR && AST::IsInteger(to))) {
        // if(fti->size() != tti->size()){
        builder.emit_pop(reg);
        builder.emit_cast(reg, CAST_SINT_SINT, from_size, to_size);
        builder.emit_push(reg);
        // }
        return true;
    }
    if ((AST::IsInteger(from) && to == AST_BOOL) ||
        (from == AST_BOOL && AST::IsInteger(to))) {
        // if(fti->size() != tti->size()){
        builder.emit_pop(reg);
        builder.emit_cast(reg, CAST_SINT_SINT, from_size, to_size);
        builder.emit_push(reg);
        // }
        return true;
    }
    if (AST::IsInteger(from) && AST::IsInteger(to)) {
        builder.emit_pop(reg);
        if (AST::IsSigned(from) && AST::IsSigned(to))
            builder.emit_cast(reg, CAST_SINT_SINT, from_size, to_size);
        if (AST::IsSigned(from) && !AST::IsSigned(to))
            builder.emit_cast(reg, CAST_SINT_UINT, from_size, to_size);
        if (!AST::IsSigned(from) && AST::IsSigned(to))
            builder.emit_cast(reg, CAST_UINT_SINT, from_size, to_size);
        if (!AST::IsSigned(from) && !AST::IsSigned(to))
            builder.emit_cast(reg, CAST_SINT_UINT, from_size, to_size);
        builder.emit_push(reg);
        return true;
    }
    TypeInfo* from_typeInfo = nullptr;
    if(from.isNormalType())
        from_typeInfo = ast->getTypeInfo(from);
    // int to_size = info.ast->getTypeSize(to);
    if(from_typeInfo && from_typeInfo->astEnum && AST::IsInteger(to)) {
        // TODO: Print an error that says big enums can't be casted to small integers.
        if(to_size >= from_typeInfo->_size) {
            // builder.emit_pop(reg);
            // builder.emit_push(reg);
            return true;
        }
    }
    // Asserts when we have missed a conversion
    if(!hasForeignErrors()){
        std::string l = ast->typeToString(from);
        std::string r = ast->typeToString(to);
        Assert(!ast->castable(from,to));
    }
    return false;
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
InstructionType ASTOpToBytecode(TypeId astOp, bool floatVersion){
    
// #define CASE(X, Y) case X: return Y;
#define CASE(X, Y) else if(op == X) return Y;
    OperationType op = (OperationType)astOp.getId();
    if(false) ;
    CASE(AST_ADD, BC_ADD)
    CASE(AST_SUB, BC_SUB)
    CASE(AST_MUL, BC_MUL)
    CASE(AST_DIV, BC_DIV) 
    CASE(AST_MODULUS, BC_MOD)
    CASE(AST_EQUAL, BC_EQ)
    CASE(AST_NOT_EQUAL, BC_NEQ) 
    CASE(AST_LESS, BC_LT) 
    CASE(AST_LESS_EQUAL, BC_LTE) 
    CASE(AST_GREATER, BC_GT) 
    CASE(AST_GREATER_EQUAL, BC_GTE) 
    CASE(AST_AND, BC_LAND)
    CASE(AST_OR, BC_LOR)
    CASE(AST_AND, BC_LNOT)
    CASE(AST_BAND, BC_BAND) 
    CASE(AST_BOR, BC_BOR) 
    CASE(AST_BXOR, BC_BXOR) 
    CASE(AST_BNOT, BC_BNOT) 
    CASE(AST_BLSHIFT, BC_BLSHIFT) 
    CASE(AST_BRSHIFT, BC_BRSHIFT) 
#undef CASE
    return (InstructionType)0;
}
SignalIO GenContext::generatePushFromValues(BCRegister baseReg, int baseOffset, TypeId typeId, int* movingOffset){
    using namespace engone;
    
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
        BCRegister reg = BC_REG_T0;
        if(*movingOffset == 0){
            // If you are here to optimize some instructions then you are out of luck.
            // I checked where GeneratePush is used whether ADDI can LI can be removed and
            // replaced with a MOV_MR_DISP32 but those instructions come from GenerateReference.
            // What you need is a system to optimise away instructions while adding them (like pop after push)
            // or an optimizer which runs after the generator.
            // You need something more sophisticated to optimize further basically.
            builder.emit_mov_rm(reg, baseReg, size);
        }else{
            builder.emit_mov_rm_disp(reg, baseReg, size, *movingOffset);
        }
        builder.emit_push(reg);
        *movingOffset += size;
    } else {
        for(int i = (int) typeInfo->astStruct->members.size() - 1; i>=0; i--){
            auto& member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            
            _GLOG(log::out << "push " << member.name << "\n";)
            generatePushFromValues(baseReg, baseOffset, memdata.typeId, movingOffset);
            *movingOffset += size;
        }
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generateArtificialPush(TypeId typeId) {
    using namespace engone;
    if(typeId == AST_VOID) {
        return SIGNAL_FAILURE;
    }
    // if(baseReg!=0) {
    //     Assert(DECODE_REG_SIZE(baseReg) == 8 && DECODE_REG_TYPE(baseReg) != BC_AX);
    // }
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    u32 size = ast->getTypeSize(typeId);
    if(!typeInfo || !typeInfo->astStruct) {
        // enum works here too
        BCRegister reg = BC_REG_T0;
        // if(offset == 0){
        //     // If you are here to optimize some instructions then you are out of luck.
        //     // I checked where GeneratePush is used whether ADDI can LI can be removed and
        //     // replaced with a MOV_MR_DISP32 but those instructions come from GenerateReference.
        //     // What you need is a system to optimise away instructions while adding them (like pop after push)
        //     // or an optimizer which runs after the generator.
        //     // You need something more sophisticated to optimize further basically.
        //     builder.emit_mov_rm(baseReg, reg, (u8)size});
        // }else{
        //     // IMPORTANT: Understand the issue before changing code here. We always use
        //     // disp32 because the converter asserts when using frame pointer.
        //     // "Use addModRM_disp32 instead". We always use disp32 to avoid the assert.
        //     // Well, the assert occurs anyway.
        //     builder.emit_({BC_MOV_MR_DISP32, baseReg, reg, (u8)size});
        //     info.addImm(offset);
        //     }
        // }
        builder.emit_push(reg, true);
    } else {
        for(int i = (int) typeInfo->astStruct->members.size() - 1; i>=0; i--){
            auto& member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            
            _GLOG(log::out << "push " << member.name << "\n";)
            generateArtificialPush(memdata.typeId);
        }
    }
    return SIGNAL_SUCCESS;
}
// push of structs
SignalIO GenContext::generatePush(BCRegister baseReg, int offset, TypeId typeId){
    using namespace engone;
    if(typeId == AST_VOID) {
        return SIGNAL_FAILURE;
    }
    
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    u32 size = ast->getTypeSize(typeId);
    if(size == 0) {
        Assert(hasForeignErrors());
        return SIGNAL_FAILURE;
    }

    if(!typeInfo || !typeInfo->astStruct) {
        BCRegister reg = BC_REG_T0;
        builder.emit_mov_rm_disp(reg, baseReg, size, offset);
        builder.emit_push(reg);
    } else {
        for(int i = (int) typeInfo->astStruct->members.size() - 1; i>=0; i--){
            auto& member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            
            _GLOG(log::out << "push " << member.name << "\n";)
            generatePush(baseReg, offset + memdata.offset, memdata.typeId);
        }
    }
    return SIGNAL_SUCCESS;
}
// pop of structs
SignalIO GenContext::generatePop(BCRegister baseReg, int offset, TypeId typeId){
    using namespace engone;
    // Assert(baseReg!=BC_REG_RCX);
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    u8 size = ast->getTypeSize(typeId);
    if(size == 0) {
        Assert(hasForeignErrors());
        return SIGNAL_FAILURE;
    }
    if (!typeInfo || !typeInfo->astStruct) {
        _GLOG(log::out << "move return value\n";)
        BCRegister reg = BC_REG_T0;
        builder.emit_pop(reg);
        if(baseReg==BC_REG_INVALID) {
            // throw away value
        } else {
            if(offset == 0){
                // The x64 architecture has a special meaning when using fp.
                // The converter asserts about using addModRM_disp32 instead. Instead of changing
                // it to allow this instruction we always use disp32 to avoid the complaint.
                // This instruction fires the assert: BC_MOV_RM rax, fp 
                builder.emit_mov_mr(baseReg, reg, size);
            }else{
                builder.emit_mov_mr_disp(baseReg, reg, size, offset);
            }
        }
    } else {
        for (int i = 0; i < (int)typeInfo->astStruct->members.size(); i++) {
            auto &member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            _GLOG(log::out << "move return value member " << member.name << "\n";)
            generatePop(baseReg, offset + memdata.offset, memdata.typeId);
        }
    }    
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generatePush_get_param (int offset, TypeId typeId) {
    using namespace engone;
    if(typeId == AST_VOID) {
        return SIGNAL_FAILURE;
    }
    
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    u32 size = ast->getTypeSize(typeId);
    if(size == 0) {
        Assert(hasForeignErrors());
        return SIGNAL_FAILURE;
    }

    if(!typeInfo || !typeInfo->astStruct) {
        BCRegister reg = BC_REG_T0;
        builder.emit_get_param(reg, offset, size, AST::IsDecimal(typeId));
        builder.emit_push(reg);
    } else {
        for(int i = (int) typeInfo->astStruct->members.size() - 1; i>=0; i--){
            auto& member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            
            _GLOG(log::out << "push " << member.name << "\n";)
            generatePush_get_param(offset + memdata.offset, memdata.typeId);
        }
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generatePop_set_arg    (int offset, TypeId typeId) {
    using namespace engone;
    // Assert(baseReg!=BC_REG_RCX);
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    u8 size = ast->getTypeSize(typeId);
    if(size == 0) {
        Assert(hasForeignErrors());
        return SIGNAL_FAILURE;
    }
    if (!typeInfo || !typeInfo->astStruct) {
        _GLOG(log::out << "move return value\n";)
        BCRegister reg = BC_REG_T0;
        builder.emit_pop(reg);
        builder.emit_set_arg(reg, offset, size, AST::IsDecimal(typeId));
    } else {
        for (int i = 0; i < (int)typeInfo->astStruct->members.size(); i++) {
            auto &member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            _GLOG(log::out << "move return value member " << member.name << "\n";)
            generatePop_set_arg(offset + memdata.offset, memdata.typeId);
        }
    }    
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generatePush_get_val   (int offset, TypeId typeId) {
    using namespace engone;
    if(typeId == AST_VOID) {
        return SIGNAL_FAILURE;
    }
    
    TypeInfo *typeInfo = nullptr;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    u32 size = ast->getTypeSize(typeId);
    if(size == 0) {
        Assert(hasForeignErrors());
        return SIGNAL_FAILURE;
    }

    if(!typeInfo || !typeInfo->astStruct) {
        BCRegister reg = BC_REG_T0;
        builder.emit_get_val(reg, offset, size, AST::IsDecimal(typeId));
        builder.emit_push(reg);
    } else {
        for(int i = (int) typeInfo->astStruct->members.size() - 1; i>=0; i--){
            auto& member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            
            _GLOG(log::out << "push " << member.name << "\n";)
            generatePush_get_val(offset + memdata.offset, memdata.typeId);
        }
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generatePop_set_ret    (int offset, TypeId typeId) {
    using namespace engone;
    // Assert(baseReg!=BC_REG_RCX);
    TypeInfo *typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    u8 size = ast->getTypeSize(typeId);
    if(size == 0) {
        Assert(hasForeignErrors());
        return SIGNAL_FAILURE;
    }
    if (!typeInfo || !typeInfo->astStruct) {
        _GLOG(log::out << "move return value\n";)
        BCRegister reg = BC_REG_T0;
        builder.emit_pop(reg);
        builder.emit_set_ret(reg, offset, size, AST::IsDecimal(typeId));
    } else {
        for (int i = 0; i < (int)typeInfo->astStruct->members.size(); i++) {
            auto &member = typeInfo->astStruct->members[i];
            auto memdata = typeInfo->getMember(i);
            _GLOG(log::out << "move return value member " << member.name << "\n";)
            generatePop_set_ret(offset + memdata.offset, memdata.typeId);
        }
    }    
    return SIGNAL_SUCCESS;
}
void GenContext::genMemzero(BCRegister ptr_reg, BCRegister size_reg, int size) {
    // TODO: Move this logic into BytecodeBuilder? (emit_memzero)
    if(size <= 8) {
        Assert(size == 1 || size == 2 || size == 4 || size == 8);
        builder.emit_bxor(size_reg, size_reg);
        builder.emit_mov_mr(ptr_reg, size_reg, size);
    } else {
        u8 batch = 1;
        if(size % 8 == 0)
            batch = 8;
        else if(size % 4 == 0)
            batch = 4;
        else if(size % 2 == 0)
            batch = 2;
        // size = size / batch; // don't do this, see how memzero is converted
        builder.emit_li32(size_reg, size);
        builder.emit_memzero(ptr_reg, size_reg, batch);
        // builder.emit_li32(size_reg, size);
        // builder.emit_({BC_MEMZERO, ptr_reg, size_reg, batch});
    }
    // builder.emit_li32(size_reg, size);
    // builder.emit_({BC_MEMZERO, ptr_reg, size_reg, 1});
}
// baseReg as 0 will push default values to stack
// non-zero as baseReg will mov default values to the pointer in baseReg
SignalIO GenContext::generateDefaultValue(BCRegister baseReg, int offset, TypeId typeId, lexer::SourceLocation* location, bool zeroInitialize) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue2);
    // if(baseReg!=0) {
    //     Assert(DECODE_REG_SIZE(baseReg) == 8);
    // }
    // MAKE_NODE_SCOPE(_expression);
    // Assert(typeInfo)
    TypeInfo* typeInfo = 0;
    if(typeId.isNormalType())
        typeInfo = ast->getTypeInfo(typeId);
    u8 size = ast->getTypeSize(typeId);
    if(baseReg != 0 && zeroInitialize) {
        #ifndef DISABLE_ZERO_INITIALIZATION
        
        builder.emit_li32(BC_REG_T1, offset);
        builder.emit_add(BC_REG_T1, baseReg, false, 8);
        genMemzero(BC_REG_T1, BC_REG_T0, size);
        
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
                SignalIO result = generateExpression(member.defaultValue, &tempTypes);
                
                if(tempTypes.size() == 1){
                    if (!performSafeCast(tempTypes[0], memdata.typeId)) {
                        // info.comp
                        ERRTYPE(member.location, member.defaultValue->location, tempTypes[0], memdata.typeId, "(default member)\n");
                    }
                    if(baseReg!=0){
                        SignalIO result = generatePop(baseReg, offset + memdata.offset, memdata.typeId);
                    }
                } else {
                    StringBuilder values = {};
                    FORN(tempTypes) {
                        auto& it = tempTypes[nr];
                        if(nr != 0)
                            values += ", ";
                        values += ast->typeToString(it);
                    }
                    ERR_SECTION(
                        ERR_HEAD2(member.defaultValue->location)
                        ERR_MSG("Default value of member produces more than one value but only one is allowed.")
                        ERR_LINE2(member.defaultValue->location, values.data())
                    )
                }
            } else {
                if(!memdata.typeId.isValid() || memdata.typeId == AST_VOID) {
                    Assert(hasForeignErrors());
                } else {
                    SignalIO result = generateDefaultValue(baseReg, offset + memdata.offset, memdata.typeId, location, false);
                }
            }
        }
    } else {
        Assert(size <= 8);
        #ifndef DISABLE_ZERO_INITIALIZATION
        // only structs have default values, otherwise zero is the default
        if(baseReg == 0){
            builder.emit_bxor(BC_REG_A, BC_REG_A);
            builder.emit_push(BC_REG_A);
        } else {
            // we generate memzero above which zero initializes
            // builder.emit_bxor(BC_REG_A, BC_REG_A);
            // BCRegister reg = BC_REG_A;
            // builder.emit_mov_mr_disp(baseReg, reg, size, offset);
        }
        #else
        // Not setting zero here is certainly a bad idea
        if(baseReg == 0){
            builder.emit_push(BC_REG_A);
        }
        #endif
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::framePush(TypeId typeId, i32* outFrameOffset, bool genDefault, bool staticData){
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
        return SIGNAL_FAILURE;
    
    if(staticData) {
        info.code->ensureAlignmentInData(asize);
        int offset = info.code->appendData(nullptr,size);
        
        info.builder.emit_dataptr(BC_REG_D, offset);

        *outFrameOffset = offset;
        if(genDefault){
            SignalIO result = info.generateDefaultValue(BC_REG_D, 0, typeId);
            if(result!=SIGNAL_SUCCESS)
                return SIGNAL_FAILURE;
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
            builder.emit_alloc_local(BC_REG_INVALID, diff);
            // info.builder.emit_incr(BC_REG_SP, -diff);
        }
        info.currentFrameOffset -= size;
        *outFrameOffset = info.currentFrameOffset;
        
        BCRegister reg = genDefault ? BC_REG_F : BC_REG_INVALID; // TODO: Is F register in use?
        builder.emit_alloc_local(reg, size);
        // builder.emit_incr(BC_REG_SP, -size);

        if(genDefault){
            SignalIO result = info.generateDefaultValue(reg, info.currentFrameOffset, typeId);
            if(result!=SIGNAL_SUCCESS)
                return SIGNAL_FAILURE;
        }
    }
    return SIGNAL_SUCCESS;
}
/*
##################################
    Generation functions below
##################################
*/
// SignalIO GenerateExpression(GenContext &info, ASTExpression *expression, TypeId *outTypeId, ScopeId idScope){
//     DynamicArray<TypeId> types{};
//     SignalIO result = GenerateExpression(info,expression,&types,idScope);
//     *outTypeId = AST_VOID;
//     if(types.size()>1){
//         ERR_SECTION(
//             ERR_HEAD2(expression->location)
//             ERR_MSG("Returns multiple values but the way you use the expression only supports one (or none).")
//             ERR_LINE2(expression->location,"returns "<<types.size() << " values")
//         )
//     } else {
//         if(types.size()==1)
//             *outTypeId = types[0];
//     }
//     return result;
// }
// outTypeId will represent the type (integer, struct...) but the value pushed on the stack will always
// be a pointer. EVEN when the outType isn't a pointer. It is an implicit extra level of indirection commonly
// used for assignment.
SignalIO GenContext::generateReference(ASTExpression* _expression, TypeId* outTypeId, ScopeId idScope, bool* wasNonReference){
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue2);
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
                        ERR_HEAD2(now->location)
                        ERR_MSG("'"<< info.compiler->lexer.tostring(now->location) << "' is undefined.")
                        ERR_LINE2(now->location,"bad");
                    )
                }
                return SIGNAL_FAILURE;
            }
        
            auto varinfo = info.ast->getVariableByIdentifier(id);
            _GLOG(log::out << " expr var push " << now->name << "\n";)
            // TOKENINFO(now->location)
            // char buf[100];
            // int len = sprintf(buf,"  expr push %s",now->name->c_str());
            // info.code->addDebugText(buf,len);

            TypeInfo *typeInfo = 0;
            if(varinfo->versions_typeId[info.currentPolyVersion].isNormalType())
                typeInfo = info.ast->getTypeInfo(varinfo->versions_typeId[info.currentPolyVersion]);
            TypeId typeId = varinfo->versions_typeId[info.currentPolyVersion];
            
            switch(varinfo->type) {
                case VariableInfo::GLOBAL: {
                    builder.emit_dataptr(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion]);
                    break; 
                }
                case VariableInfo::LOCAL: {
                    builder.emit_get_local(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion]);
                    // builder.emit_add(BC_REG_B, BC_REG_BP, false, 8);
                    break; 
                }
                case VariableInfo::MEMBER: {
                    // NOTE: Is member variable/argument always at this offset with all calling conventions?
                    Assert(info.currentFunction->callConvention == BETCALL);
                    builder.emit_get_param(BC_REG_B, 0, 8, false);
                    // builder.emit_mov_rm_disp(BC_REG_B, BC_REG_BP, 8, GenContext::FRAME_SIZE);
                    
                    builder.emit_li32(BC_REG_A, varinfo->versions_dataOffset[info.currentPolyVersion]);
                    builder.emit_add(BC_REG_B, BC_REG_A, false, 8);
                    break;
                }
                default: {
                    Assert(false);
                }
            }
            builder.emit_push(BC_REG_B);

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
            SignalIO result = generateExpression(now, &tempTypes, idScope);
            if(result!=SIGNAL_SUCCESS || tempTypes.size()!=1){
                return SIGNAL_FAILURE;
            }
            if(!tempTypes.last().isPointer()){
                ERR_SECTION(
                    ERR_HEAD2(now->location)
                    ERR_MSG("'"<<compiler->lexer.tostring(now->location)<<
                    "' must be a reference to some memory. "
                    "A variable, member or expression resulting in a dereferenced pointer would be.")
                    ERR_LINE2(now->location, "must be a reference")
                )
                return SIGNAL_FAILURE;
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
                    ERR_HEAD2(now->location)
                    ERR_MSG("'"<<info.ast->typeToString(endType)<<"' has to many levels of pointing. Can only access members of a single or non-pointer.")
                    ERR_LINE2(now->location, "to pointy")
                )
                return SIGNAL_FAILURE;
            }
            TypeInfo* typeInfo = nullptr;
            typeInfo = info.ast->getTypeInfo(endType.baseType());
            if(!typeInfo || !typeInfo->astStruct){ // one level of pointer is okay.
                ERR_SECTION(
                    ERR_HEAD2(now->location)
                    ERR_MSG("'"<<info.ast->typeToString(endType)<<"' is not a struct. Cannot access member.")
                    ERR_LINE2(now->left->location, info.ast->typeToString(endType).c_str())
                )
                return SIGNAL_FAILURE;
            }

            auto memberData = typeInfo->getMember(now->name);
            if(memberData.index==-1){
                Assert(info.hasForeignErrors());
                // error should have been caught by type checker.
                // if it was then we just return error here.
                // don't want message printed twice.
                // ERR_SECTION(
                // ERR_HEAD2(now->location, "'"<<now->name << "' is not a member of struct '" << info.ast->typeToString(endType) << "'. "
                //         "These are the members: ";
                //     for(int i=0;i<(int)typeInfo->astStruct->members.size();i++){
                //         if(i!=0)
                //             log::out << ", ";
                //         log::out << log::LIME << typeInfo->astStruct->members[i].name<<log::NO_COLOR<<": "<<info.ast->typeToString(typeInfo->getMember(i).typeId);
                //     }
                //     log::out <<"\n";
                //     log::out <<"\n";
                //     ERR_LINE2(now->location, "not a member");
                // )
                return SIGNAL_FAILURE;
            }
            // TODO: You can do more optimisations here as long as you don't
            //  need to dereference or use MOV_MR. You can have the RBX popped
            //  and just use ADDI on it to get the correct members offset and keep
            //  doing this for all member accesses. Then you can push when done.
            
            bool popped = false;
            BCRegister reg = BC_REG_B;
            if(endType.getPointerLevel()>0){
                if(!popped)
                    builder.emit_pop(reg);
                popped = true;
                builder.emit_mov_rm(BC_REG_C, reg, 8);
                reg = BC_REG_C;
            }
            if(memberData.offset!=0){
                if(!popped)
                    builder.emit_pop(reg);
                popped = true;
                
                builder.emit_li32(BC_REG_A, memberData.offset);
                builder.emit_add(reg, BC_REG_A, false, 8);
            }
            if(popped)
                builder.emit_push(reg);

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
                    ERR_HEAD2(now->left->location)
                    ERR_MSG("type '"<<info.ast->typeToString(endType)<<"' is not a pointer.")
                    ERR_LINE2(now->left->location,"must be a pointer")
                )
                return SIGNAL_FAILURE;
            }
            if(endType.getPointerLevel()>0){
                
                builder.emit_pop(BC_REG_B);
                builder.emit_mov_rm(BC_REG_C, BC_REG_B, 8);
                builder.emit_push(BC_REG_C);
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
            SignalIO result = generateExpression(now->right, &tempTypes);
            if (result != SIGNAL_SUCCESS && tempTypes.size() != 1)
                return SIGNAL_FAILURE;
            
            if(!AST::IsInteger(tempTypes.last())){
                std::string strtype = info.ast->typeToString(tempTypes.last());
                ERR_SECTION(
                    ERR_HEAD2(now->right->location)
                    ERR_MSG("Index operator (array[23]) requires integer type in the inner expression. '"<<strtype<<"' is not an integer. (Operator overloading doesn't work here)")
                    ERR_LINE2(now->right->location,strtype.c_str());
                )
                return SIGNAL_FAILURE;
            }

            if(pointerType){
                pointerType=false;
                endType.setPointerLevel(endType.getPointerLevel()-1);
                
                u32 typesize = info.ast->getTypeSize(endType);
                u32 rsize = info.ast->getTypeSize(tempTypes.last());
                BCRegister reg = BC_REG_C;
                builder.emit_pop(reg); // integer
                builder.emit_pop(BC_REG_B); // reference

                if(typesize>1){
                    builder.emit_li32(BC_REG_A, typesize);
                    // builder.emit_li32(BC_REG_A, typesize);
                    builder.emit_mul(BC_REG_A, reg, false, 8, false);
                    // builder.emit_({BC_MULI, ARITHMETIC_SINT, reg, BC_REG_A});
                }
                builder.emit_add(BC_REG_B, BC_REG_A, false, 8);
                // builder.emit_({BC_ADDI, BC_REG_B, BC_REG_A, BC_REG_B});
                builder.emit_push(BC_REG_B);
                // builder.emit_push(BC_REG_B);
                continue;
            }

            if(!endType.isPointer()){
                if(!info.hasForeignErrors()){
                    std::string strtype = info.ast->typeToString(endType);
                    ERR_SECTION(
                        ERR_HEAD2(now->left->location)
                        ERR_MSG("Index operator (array[23]) requires pointer type in the outer expression. '"<<strtype<<"' is not a pointer. (Operator overloading doesn't work here)")
                        ERR_LINE2(now->left->location,strtype.c_str());
                    )
                }
                return SIGNAL_FAILURE;
            }

            endType.setPointerLevel(endType.getPointerLevel()-1);

            u32 typesize = info.ast->getTypeSize(endType);
            u32 rsize = info.ast->getTypeSize(tempTypes.last());
            BCRegister reg = BC_REG_D;
            builder.emit_pop(reg); // integer
            builder.emit_pop(BC_REG_B); // reference
            // dereference pointer to pointer
            builder.emit_mov_rm(BC_REG_C, BC_REG_B, 8);
            
            if(typesize>1){
                builder.emit_li32(BC_REG_A, typesize);
                builder.emit_mul(BC_REG_A, reg, false, 8, false);
            }
            builder.emit_add(BC_REG_C, BC_REG_A, false, 8);
            // builder.emit_({BC_ADDI, BC_REG_RCX, BC_REG_A, BC_REG_RCX});

            builder.emit_push(BC_REG_C);
        }
    }
    if(pointerType && !wasNonReference){
        ERR_SECTION(
            ERR_HEAD2(_expression->location)
            ERR_MSG("'"<<compiler->lexer.tostring(_expression->location)<<
            "' must be a reference to some memory. "
            "A variable, member or expression resulting in a dereferenced pointer would work.")
            ERR_LINE2(_expression->location, "must be a reference")
        )
        return SIGNAL_FAILURE;
    }
    if(wasNonReference)
        *wasNonReference = pointerType;
    *outTypeId = endType;
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generateFnCall(ASTExpression* expression, DynamicArray<TypeId>* outTypeIds, bool isOperator){
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
        return SIGNAL_FAILURE;
    }

    // overload comes from type checker
    if(isOperator) {
        _GLOG(log::out << "Operator overload: ";funcImpl->print(info.ast,nullptr);log::out << "\n";)
    } else {
        _GLOG(log::out << "Overload: ";funcImpl->print(info.ast,nullptr);log::out << "\n";)
    }
    TINY_ARRAY(ASTExpression*,all_arguments,5);
    // std::vector<ASTExpression*> fullArgs;
    all_arguments.resize(astFunc->arguments.size());
    
    if(isOperator) {
        all_arguments[0] = expression->left;
        all_arguments[1] = expression->right;
    } else {
        for (int i = 0; i < (int)expression->args.size();i++) {
            ASTExpression* arg = expression->args.get(i);
            Assert(arg);
            if(!arg->namedValue.len){
                if(expression->hasImplicitThis()) {
                    all_arguments[i+1] = arg;
                } else {
                    all_arguments[i] = arg;
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
                    if (all_arguments[argIndex] != nullptr) {
                        ERR_SECTION(
                            ERR_HEAD2(arg->location)
                            ERR_MSG( "A named argument cannot specify an occupied parameter.")
                            ERR_LINE2(astFunc->location,"this overload")
                            ERR_LINE2(all_arguments[argIndex]->location,"occupied")
                            ERR_LINE2(arg->location,"bad coder")
                        )
                    } else {
                        all_arguments[argIndex] = arg;
                    }
                } else{
                    ERR_SECTION(
                        ERR_HEAD2(arg->location)
                        ERR_MSG_COLORED("Named argument is not a parameter of '"; funcImpl->print(info.ast,astFunc); engone::log::out << "'.")
                        ERR_LINE2(arg->location,"not valid")
                    )
                    // continue like nothing happened
                }
            }
        }
        for (int i = 0; i < (int)astFunc->arguments.size(); i++) {
            auto &arg = astFunc->arguments[i];
            if (!all_arguments[i])
                all_arguments[i] = arg.defaultValue;
        }
    }

    // NOTE: alloc_local allocates memory for local variable
    //   It should have been done when we did alloc_local
    int allocated_stack_space = 0;

    // I don't think there is anything to fix in there because conventions
    // are really about assembly instructions which type checker has nothing to do with.
    // log::out << log::YELLOW << "Fix call conventions in type checker\n";
    // int startSP = builder.saveStackMoment();
    switch(astFunc->callConvention){
        case BETCALL: {
            int stackSpace = funcImpl->argSize;
            if(stackSpace)
                builder.emit_alloc_local(BC_REG_INVALID, stackSpace);
            allocated_stack_space = stackSpace;
        } break;
        case STDCALL: {
            for(int i=0;i<astFunc->arguments.size();i++){
                int size = info.ast->getTypeSize(funcImpl->argumentTypes[i].typeId);
                if(size>8){
                    // TODO: This should be moved to the type checker.
                    ERR_SECTION(
                        ERR_HEAD2(expression->location)
                        ERR_MSG("Argument types cannot be larger than 8 bytes when using stdcall.")
                        ERR_LINE2(expression->location, "bad")
                    )
                    return SIGNAL_FAILURE;
                }
            }

            int stackSpace = astFunc->arguments.size() * 8;
            if(stackSpace<32)
                stackSpace = 32;
            if(stackSpace % 16 != 0) // 16-byte alignment
                stackSpace += 16 - (stackSpace%16);
            builder.emit_alloc_local(BC_REG_INVALID, stackSpace);
            allocated_stack_space = stackSpace;
        } break;
        case UNIXCALL: {
            for(int i=0;i<astFunc->arguments.size();i++){
                int size = info.ast->getTypeSize(funcImpl->argumentTypes[i].typeId);
                if(size>8){
                    // TODO: This should be moved to the type checker.
                    ERR_SECTION(
                        ERR_HEAD2(expression->location)
                        ERR_MSG("Argument types cannot be larger than 8 bytes when using unixcall.")
                        ERR_LINE2(expression->location, "argument type is to large")
                    )
                    return SIGNAL_FAILURE;
                }
            }
            Assert(astFunc->arguments.size() <= 4); // INCOMPLETE, we need to allocate proper stack space
            int stackSpace = 0;
            builder.emit_alloc_local(BC_REG_INVALID, stackSpace);
            allocated_stack_space = stackSpace;
        } break;
        case CDECL_CONVENTION: {
            Assert(false); // INCOMPLETE
        } break;
        case INTRINSIC: {
            // do nothing
        } break;
        default: Assert(false);
    }

    // int virtualArgumentSpace = builder.getStackPointer();
    for(int i=0;i<(int)all_arguments.size();i++){
        auto arg = all_arguments[i];
        if(expression->hasImplicitThis() && i == 0) {
            // Make sure we actually have this stored before at the base pointer of
            // the current function.
            Assert(info.currentFunction && info.currentFunction->parentStruct);
            Assert(info.currentFunction->callConvention == BETCALL);
            // NOTE: Is member variable/argument always at this offset with all calling conventions?
            // builder.emit_mov_rm_disp(BC_REG_B, BC_REG_BP, 8, GenContext::FRAME_SIZE);
            builder.emit_get_param(BC_REG_B, 0, 8, false);
            builder.emit_push(BC_REG_B);
            continue;
        } else {
            Assert(arg);
        }
        
        TypeId argType = {};
        SignalIO result = SIGNAL_FAILURE;
        if(expression->hasImplicitThis() && i == 0) {
            Assert(false); // shouldn't run?
            // method call and first argument which is 'this'
            bool nonReference = false;
            result = generateReference(arg, &argType, -1, &nonReference);
            if(result==SIGNAL_SUCCESS){
                if(!nonReference) {
                    if(!argType.isPointer()){
                        argType.setPointerLevel(1);
                    } else {
                        Assert(argType.getPointerLevel()==1);
                        builder.emit_pop(BC_REG_B);
                        builder.emit_mov_rm(BC_REG_A, BC_REG_B, 8);
                        builder.emit_push(BC_REG_A);
                    }
                }
            }
        } else {
            DynamicArray<TypeId> tempTypes{};
            result = generateExpression(arg, &tempTypes);
            if(tempTypes.size()>0) {
                TypeId argType = tempTypes[0];
                // log::out << "PUSH ARG "<<info.ast->typeToString(argType)<<"\n";
                bool wasSafelyCasted = performSafeCast(argType, funcImpl->argumentTypes[i].typeId);
                if(!wasSafelyCasted && !info.hasErrors()){
                    ERR_SECTION(
                        ERR_HEAD2(arg->location)
                        ERR_MSG("Cannot cast argument of type " << info.ast->typeToString(argType) << " to " << info.ast->typeToString(funcImpl->argumentTypes[i].typeId) << ". This is the function: '"<<astFunc->name<<"'. (function may be an overloaded operator)")
                        ERR_LINE2(arg->location,"bad argument")
                    )
                }
            } else {
                Assert(info.hasForeignErrors());
            }
        }
    }
    auto callConvention = astFunc->callConvention;
    // if(info.compileInfo->options->target == TARGET_BYTECODE &&
    //     (IS_IMPORT(astFunc->linkConvention))) {
    //     callConvention = BETCALL;
    // }

    // operator overloads should always use BETCALL
    Assert(!isOperator || callConvention == BETCALL);

    // TODO: Refactor stdcall, betbat, and unixcall because the calling convention is abstracted in the bytecode.
    switch (callConvention) {
        case INTRINSIC: {
            // TODO: You could do some special optimisations when using intrinsics.
            //  If the arguments are strictly variables or constants then you can use a mov instruction 
            //  instead messing with push and pop.
            auto& name = funcImpl->astFunction->name;
            if(name == "memcpy"){
                builder.emit_pop(BC_REG_C);
                builder.emit_pop(BC_REG_B);
                builder.emit_pop(BC_REG_A);
                builder.emit_memcpy(BC_REG_A, BC_REG_B, BC_REG_C);
            } else if(name == "strlen"){
                builder.emit_pop(BC_REG_B);
                builder.emit_strlen(BC_REG_A, BC_REG_B);
                builder.emit_push(BC_REG_A); // len
                outTypeIds->add(AST_UINT32);
            } else if(name == "memzero"){
                builder.emit_pop(BC_REG_B); // ptr
                builder.emit_pop(BC_REG_A);
                builder.emit_memzero(BC_REG_A, BC_REG_B, 0);
            } else if(name == "rdtsc"){
                builder.emit_rdtsc(BC_REG_A);
                builder.emit_push(BC_REG_A); // timestamp counter
                outTypeIds->add(AST_UINT64);
            // } else if(funcImpl->name == "rdtscp"){
            //     builder.emit_({BC_RDTSC, BC_REG_A, BC_REG_ECX, BC_REG_RDX});
            //     builder.emit_push(BC_REG_A); // timestamp counter
            //     builder.emit_push(BC_REG_ECX); // processor thing?
                
            //     outTypeIds->add(AST_UINT64);
            //     outTypeIds->add(AST_UINT32);
            } else if(name == "atomic_compare_swap"){
                builder.emit_pop(BC_REG_C); // new
                builder.emit_pop(BC_REG_B); // old
                builder.emit_pop(BC_REG_A); // ptr
                builder.emit_atomic_cmp_swap(BC_REG_A, BC_REG_B, BC_REG_C);
                builder.emit_push(BC_REG_A);
                outTypeIds->add(AST_INT32);
            } else if(name == "atomic_add"){
                builder.emit_pop(BC_REG_B);
                builder.emit_pop(BC_REG_A);
                builder.emit_atomic_add(BC_REG_A, BC_REG_B, CONTROL_32B);
                builder.emit_push(BC_REG_A);
                outTypeIds->add(AST_INT32);
            } else if(name == "sqrt"){
                builder.emit_pop(BC_REG_A);
                builder.emit_sqrt(BC_REG_A);
                builder.emit_push(BC_REG_A);
                outTypeIds->add(AST_FLOAT32);
            } else if(name == "round"){
                builder.emit_pop(BC_REG_A);
                builder.emit_round(BC_REG_A);
                builder.emit_push(BC_REG_A);
                outTypeIds->add(AST_FLOAT32);
            } 
            // else if(funcImpl->name == "sin"){
            //     builder.emit_pop(BC_REG_A);
            //     builder.emit_({BC_SIN, BC_REG_A});
            //     builder.emit_push(BC_REG_A);
            //     outTypeIds->add(AST_FLOAT32);
            // } else if(funcImpl->name == "cos"){
            //     builder.emit_pop(BC_REG_A);
            //     builder.emit_({BC_COS, BC_REG_A});
            //     builder.emit_push(BC_REG_A);
            //     outTypeIds->add(AST_FLOAT32);
            // } else if(funcImpl->name == "tan"){
            //     builder.emit_pop(BC_REG_A);
            //     builder.emit_({BC_TAN, BC_REG_A});
            //     builder.emit_push(BC_REG_A);
            //     outTypeIds->add(AST_FLOAT32);
            // }
            else {
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG("'"<<name<<"' is not an intrinsic function.")
                    ERR_LINE2(expression->location,"not an intrinsic")
                )
            }
            return SIGNAL_SUCCESS;
        } break; 
        case BETCALL: {
            // I have not implemented linkConvention because it's rare
            // that you need it for BETCALL.
            // if(info.compileInfo->options->target != TARGET_BYTECODE) {
            //     Assert(astFunc->linkConvention == LinkConventions::NONE || astFunc->linkConvention == NATIVE);
            // }
            // TODO: It would be more efficient to do GeneratePop right after the argument expressions are generated instead
            //  of pushing them to the stack and then popping them here. You would save some pushing and popping
            //  The only difficulty is handling all the calling conventions. stdcall requires some more thought
            //  it's arguments should be put in registers and those are probably used when generating expressions. 
            if(all_arguments.size() != 0){
                // int baseOffset = virtualArgumentSpace - builder.getStackPointer();
                // builder.emit_li32(BC_REG_B, virtualSP - info.virtualStackPointer);
                // builder.emit_mov_rr(BC_REG_B, BC_REG_SP);
                // builder.emit_mov_rr(BC_REG_SP, BC_REG_B});
                for(int i=all_arguments.size()-1;i>=0;i--){
                    auto arg = all_arguments[i];
                    
                    // log::out << "POP ARG "<<info.ast->typeToString(funcImpl->argumentTypes[i].typeId)<<"\n";
                    generatePop_set_arg(funcImpl->argumentTypes[i].offset, funcImpl->argumentTypes[i].typeId);
                    // generatePop(BC_REG_ARGS, baseOffset + funcImpl->argumentTypes[i].offset, funcImpl->argumentTypes[i].typeId);
                    
                    // TODO: Use this instead?
                    // int baseOffset = virtualArgumentSpace - info.virtualStackPointer;
                    // GeneratePop(info,BC_REG_SP, baseOffset + funcImpl->argumentTypes[i].offset, funcImpl->argumentTypes[i].typeId);
                }
            }
            
            i32 reloc = 0;
            if(compiler->options->target == TARGET_BYTECODE &&
                (astFunc->linkConvention == IMPORT || astFunc->linkConvention == DLLIMPORT)) {
                // info.addCall(NATIVE, astFunc->callConvention);
                builder.emit_call(NATIVE, astFunc->callConvention, &reloc);
            } else {
                // info.addCall(astFunc->linkConvention, astFunc->callConvention);
                builder.emit_call(astFunc->linkConvention, astFunc->callConvention, &reloc);
            }
            info.addCallToResolve(reloc,funcImpl);

            if (funcImpl->returnTypes.size()==0) {
                _GLOG(log::out << "pop arguments\n");
                if(allocated_stack_space != 0)
                    builder.emit_free_local(allocated_stack_space);
                outTypeIds->add(AST_VOID);
            } else {
                _GLOG(log::out << "extract return values\n";)
                // NOTE: info.currentScopeId MAY NOT WORK FOR THE TYPES IF YOU PASS FUNCTION POINTERS!
                
                if(allocated_stack_space != 0)
                    builder.emit_free_local(allocated_stack_space);

                for (int i = 0;i<(int)funcImpl->returnTypes.size(); i++) {
                    auto &ret = funcImpl->returnTypes[i];
                    TypeId typeId = ret.typeId;

                    generatePush_get_val(ret.offset - funcImpl->returnSize, typeId);
                    // generatePush(BC_REG_B, -GenContext::FRAME_SIZE + ret.offset - funcImpl->returnSize, typeId);
                    outTypeIds->add(ret.typeId);
                }
            }
        } break; 
        case STDCALL: {
            // if(all_arguments.size() > 4) {
                // Assert(virtualArgumentSpace - builder.getStackPointer() == all_arguments.size()*8);
                // int argOffset = all_arguments.size()*8;
                // builder.emit_mov_rr(BC_REG_B, BC_REG_SP);
                // for(int i=all_arguments.size()-1;i>=4;i--){
                for(int i=all_arguments.size()-1;i>=0;i--){
                    auto arg = all_arguments[i];

                    generatePop_set_arg(funcImpl->argumentTypes[i].offset, funcImpl->argumentTypes[i].typeId);
                    
                    // log::out << "POP ARG "<<info.ast->typeToString(funcImpl->argumentTypes[i].typeId)<<"\n";
                    // NOTE: funcImpl->argumentTypes[i].offset SHOULD NOT be used 8*i is correct
                    // u32 size = info.ast->getTypeSize(funcImpl->argumentTypes[i].typeId);
                    // generatePop(BC_REG_B, argOffset + i*8, funcImpl->argumentTypes[i].typeId);
                }
            // }
            // Hmmm... we don't have x64 registers.
            // The call instruction must store the calling convention and
            // somehow recognize which registers we use.
            // const BCRegister normal_regs[4]{
            //     BC_REG_RCX,
            //     BC_REG_RDX,
            //     BC_REG_R8,
            //     BC_REG_R9,
            // };
            // const BCRegister float_regs[4] {
            //     BC_REG_XMM0, // nocheckin, 32-bit or 64-bit floats?
            //     BC_REG_XMM1,
            //     BC_REG_XMM2,
            //     BC_REG_XMM3,
            // };
            // auto& argTypes = funcImpl->argumentTypes;
            // for(int i=fullArgs.size()-1;i>=0;i--) {
            //     auto argType = argTypes[i].typeId;
            //     if(AST::IsDecimal(argType)) {
            //         Assert(false); // broken, 32-bit or 64-bit floats?
            //         builder.emit_pop(float_regs[i]);
            //     } else {
            //         builder.emit_pop(normal_regs[i]);
            //     }
            // }

            // native function can be handled normally
            int reloc = 0;
            if(astFunc->linkConvention == LinkConventions::IMPORT || astFunc->linkConvention == LinkConventions::DLLIMPORT
                || astFunc->linkConvention == LinkConventions::VARIMPORT){
                builder.emit_call(astFunc->linkConvention, astFunc->callConvention, &reloc, code->externalRelocations.size());
                std::string alias = astFunc->linked_alias.size() == 0 ? astFunc->name : astFunc->linked_alias;
                std::string lib_path = "";
                for(auto& lib : info.imp->libraries) {
                    if(astFunc->linked_library == lib.named_as) {
                        lib_path = lib.path;
                        break;
                    }
                }
                if(astFunc->linkConvention == DLLIMPORT){
                    addExternalRelocation("__imp_"+alias, lib_path, reloc);
                } else if(astFunc->linkConvention == VARIMPORT){
                    Assert(false); // importing variables as function calls makes no since?
                    addExternalRelocation(alias, lib_path, reloc);
                } else {
                    addExternalRelocation(alias, lib_path, reloc);
                }
            } else {
                builder.emit_call(astFunc->linkConvention, astFunc->callConvention, &reloc);
                // linkin == native, none or export should be fine
                info.addCallToResolve(reloc, funcImpl);
            }

            Assert(funcImpl->returnTypes.size() < 2); // stdcall can only have on return value

            if (funcImpl->returnTypes.size()==0) {
                
                if(allocated_stack_space != 0)
                    builder.emit_free_local(allocated_stack_space);
                // builder.restoreStackMoment(startSP);
                outTypeIds->add(AST_VOID);
            } else {
                // return type must fit in RAX
                Assert(info.ast->getTypeSize(funcImpl->returnTypes[0].typeId) <= 8);
                
                if(allocated_stack_space != 0)
                    builder.emit_free_local(allocated_stack_space);
                // builder.restoreStackMoment(startSP);
                
                // TODO: Return value is passed in a special register if it's a float. We need to pass a bool is_float. Or perhaps a special instruction for floats.
                auto &ret = funcImpl->returnTypes[0];
                generatePush_get_val(0 - 8, ret.typeId);

                // int size = info.ast->getTypeSize(ret.typeId);
                // if(AST::IsDecimal(ret.typeId)) {
                //     builder.emit_push(BC_REG_XMM0); // nocheckin, 32-bit or 64-bit float?
                // } else {
                //     BCRegister reg = BC_REG_A;
                //     builder.emit_push(reg);
                // }
                outTypeIds->add(ret.typeId);
            }
        } break;
        case UNIXCALL: {
            // if(fullArgs.size() > 4) {
            //     Assert(virtualArgumentSpace - info.virtualStackPointer == fullArgs.size()*8);
            //     int argOffset = fullArgs.size()*8;
            //     builder.emit_mov_rr(BC_REG_SP, BC_REG_B});
            //     for(int i=fullArgs.size()-1;i>=4;i--){
            //         auto arg = fullArgs[i];
                    
            //         // log::out << "POP ARG "<<info.ast->typeToString(funcImpl->argumentTypes[i].typeId)<<"\n";
            //         // NOTE: funcImpl->argumentTypes[i].offset SHOULD NOT be used 8*i is correct
            //         u32 size = info.ast->getTypeSize(funcImpl->argumentTypes[i].typeId);
            //         GeneratePop(info,BC_REG_B, argOffset + i*8, funcImpl->argumentTypes[i].typeId);
            //     }
            // }
            for(int i=all_arguments.size()-1;i>=0;i--){
                auto arg = all_arguments[i];

                generatePop_set_arg(funcImpl->argumentTypes[i].offset, funcImpl->argumentTypes[i].typeId);
            }
            // TODO IMPORTANT: Do we need to do something about the "red zone" (128 bytes below stack pointer).
            //   What does the red zone imply? No pushing, popping?
            // auto& argTypes = funcImpl->argumentTypes;
            // const BCRegister normal_regs[6]{
            //     BC_REG_RDI,
            //     BC_REG_RSI,
            //     BC_REG_RDX,
            //     BC_REG_RCX,
            //     BC_REG_R8,
            //     BC_REG_R9,
            // };
            // const BCRegister float_regs[8] {
            //     BC_REG_XMM0, // nocheckin, 32 or 64 bit floats?
            //     BC_REG_XMM1,
            //     BC_REG_XMM2,
            //     BC_REG_XMM3,
            //     // BC_REG_XMM4,
            //     // BC_REG_XMM5,
            //     // BC_REG_XMM6,
            //     // BC_REG_XMM7,
            // };
            // int used_normals = 0;
            // int used_floats = 0;
            // // Because we need to pop arguments in reverse, we must first know how many
            // // integer/pointer and float type arguments we have.
            // for(int i=fullArgs.size()-1;i>=0;i--) {
            //     if(AST::IsDecimal(argTypes[i].typeId)) {
            //         used_floats++;
            //     } else {
            //         used_normals++;
            //     }
            // }
            // // Then, from the back, we can pop and place arguments in the right register.
            // for(int i=fullArgs.size()-1;i>=0;i--) {
            //     if(AST::IsDecimal(argTypes[i].typeId)) {
            //         Assert(false); // nocheckin, 32 or 64 bit floats?
            //         // builder.emit_pop(float_regs[--used_floats]);
            //     } else {
            //         builder.emit_pop(normal_regs[--used_normals]);
            //     }
            // }
            
            int reloc = 0;
            if(astFunc->linkConvention == LinkConventions::IMPORT || astFunc->linkConvention == LinkConventions::DLLIMPORT
                || astFunc->linkConvention == LinkConventions::VARIMPORT){
                builder.emit_call(astFunc->linkConvention, astFunc->callConvention, &reloc, code->externalRelocations.size());
                std::string alias = astFunc->linked_alias.size() == 0 ? astFunc->name : astFunc->linked_alias;
                std::string lib_path = "";
                for(auto& lib : info.imp->libraries) {
                    if(astFunc->linked_library == lib.named_as) {
                        lib_path = lib.path;
                        break;
                    }
                }
                if(astFunc->linkConvention == DLLIMPORT){
                    // addExternalRelocation("__imp_"+alias, lib_path, reloc);
                    addExternalRelocation(alias, lib_path, reloc);
                } else if(astFunc->linkConvention == VARIMPORT){
                    Assert(false); // importing variables as function calls makes no since?
                    addExternalRelocation(alias, lib_path, reloc);
                } else {
                    addExternalRelocation(alias, lib_path, reloc);
                }

                // if(astFunc->linkConvention == DLLIMPORT){
                //     info.addExternalRelocation(funcImpl->astFunction->name, reloc);
                //     // info.addExternalRelocation("__imp_"+funcImpl->name, info.code->length());
                // } else if(astFunc->linkConvention == VARIMPORT){
                //     info.addExternalRelocation(funcImpl->astFunction->name, reloc);
                // } else {
                //     info.addExternalRelocation(funcImpl->astFunction->name, reloc);
                // }
            } else {
                builder.emit_call(astFunc->linkConvention, astFunc->callConvention, &reloc);
                // linkin == native, none or export should be fine
                info.addCallToResolve(reloc, funcImpl);
            }

            Assert(funcImpl->returnTypes.size() < 2); // stdcall can only have on return value

            if (funcImpl->returnTypes.size()==0) {
                if(allocated_stack_space != 0)
                    builder.emit_free_local(allocated_stack_space);
                // builder.restoreStackMoment(startSP);
                outTypeIds->add(AST_VOID);
            } else {
                // return type must fit in RAX
                Assert(info.ast->getTypeSize(funcImpl->returnTypes[0].typeId) <= 8);
                
                // builder.restoreStackMoment(startSP);
                if(allocated_stack_space != 0)
                    builder.emit_free_local(allocated_stack_space);
                
                // TODO: Return value is passed in a special register if it's a float. We need to pass a bool is_float. Or perhaps a special instruction for floats.
                auto &ret = funcImpl->returnTypes[0];
                generatePush_get_val(0 - 8, ret.typeId);
                
                // auto &ret = funcImpl->returnTypes[0];
                // int size = info.ast->getTypeSize(ret.typeId);
                // if(AST::IsDecimal(ret.typeId)) {
                //     builder.emit_push(BC_REG_XMM0); // nocheckin, 32 or 64 bit floats?
                // } else {
                //     BCRegister reg = BC_REG_A;
                //     builder.emit_push(reg);
                // }
                outTypeIds->add(ret.typeId);
            }
        } break;
        default: Assert(false);
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generateExpression(ASTExpression *expression, TypeId *outTypeIds, ScopeId idScope) {
    *outTypeIds = AST_VOID;
    DynamicArray<TypeId> tmp_types{};
    auto signal = generateExpression(expression, &tmp_types, idScope);
    if(tmp_types.size()) *outTypeIds = tmp_types.last();
    return signal;
}
SignalIO GenContext::generateExpression(ASTExpression *expression, DynamicArray<TypeId> *outTypeIds, ScopeId idScope) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue2);
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
// ERR_HEAD2(expression->location,"Type "<<info.ast->getTokenFromTypeString(expression->castType) << " does not exist.\n";)
    //     }
    // }
    // castType = info.ast->ensureNonVirtualId(castType);

    // if(expression->computeWhenPossible) {
    //     if(expression->constantValue){
    //         u32 startInstruction = info.code->length();

    //         int moment = builder.saveStackMoment();
    //         int startVirual = info.virtualStackPointer;
            
    //         expression->computeWhenPossible = false; // temporarily disable to preven infinite loop
    //         SignalIO result = generateExpression(expression, outTypeIds, idScope);
    //         expression->computeWhenPossible = true; // must enable since other polymorphic implementations want to compute too

    //         int endVirtual = info.virtualStackPointer;
    //         Assert(endVirtual < startVirual);

    //         builder.restoreStackMoment(moment, false, true);

    //         u32 endInstruction = info.code->length();

    //         // TODO: Would it be better to put interpreter in GenContext. It would be possible to
    //         //   reuse allocations the interpreter made.
    //         VirtualMachine interpreter{};
    //         interpreter.expectValuesOnStack = true;
    //         interpreter.silent = true;
    //         interpreter.executePart(info.code, startInstruction, endInstruction);
    //         log::out.flush();
    //         // interpreter.printRegisters();
    //         // this won't work with the stack
    //         info.popInstructions(endInstruction-startInstruction);

    //         if(!info.disableCodeGeneration) {
    //             info.code->ensureAlignmentInData(8);

    //             void* pushedValues = (void*)interpreter.sp;
    //             int pushedSize = -(endVirtual - startVirual);
                
    //             int offset = info.code->appendData(pushedValues, pushedSize);
    //             builder.emit_dataptr(BC_REG_B, );
    //             info.addImm(offset);
    //             for(int i=0;i<(int)outTypeIds->size();i++){
    //                 SignalIO result = GeneratePushFromValues(info, BC_REG_B, 0, outTypeIds->get(i));
    //             }
    //         }
            
    //         interpreter.cleanup();
    //         return SIGNAL_SUCCESS;
    //     } else {
    //         ERR_SECTION(
    //             ERR_HEAD2(expression->location)
    //             ERR_MSG("Cannot compute non-constant expression at compile time.")
    //             ERR_LINE2(expression->location, "run only works on constant expressions")
    //         )
    //         // TODO: Find which part of the expression isn't constant
    //     }
    // }

    if (expression->isValue) {
        // data type
        if (AST::IsInteger(expression->typeId)) {
            i64 val = expression->i64Value;
            // Assert(!(val&0xFFFFFFFF00000000));

            // TODO: immediate only allows 32 bits. What about larger values?
            // info.code->addDebugText("  expr push int");
            // TOKENINFO(expression->location)

            // TODO: Int types should come from global scope. Is it a correct assumption?
            // TypeInfo *typeInfo = info.ast->getTypeInfo(expression->typeId);
            u32 size = info.ast->getTypeSize(expression->typeId);
            BCRegister reg = BC_REG_A;
            if(size == 8) {
                builder.emit_li64(reg, val);
            } else {
                builder.emit_li32(reg, val);
            }
            builder.emit_push(reg);
        }
        else if (expression->typeId == AST_BOOL) {
            bool val = expression->boolValue;

            // TOKENINFO(expression->location)
            // info.code->addDebugText("  expr push bool");
            builder.emit_li32(BC_REG_A, val);
            builder.emit_push(BC_REG_A);
        }
        else if (expression->typeId == AST_CHAR) {
            char val = expression->charValue;

            // TOKENINFO(expression->location)
            // info.code->addDebugText("  expr push char");
            builder.emit_li32(BC_REG_A, val);
            builder.emit_push(BC_REG_A);
        }
        else if (expression->typeId == AST_FLOAT32) {
            float val = expression->f32Value;

            // TOKENINFO(expression->location)
            // info.code->addDebugText("  expr push float");
            builder.emit_li32(BC_REG_A, *(u32*)&val);
            builder.emit_push(BC_REG_A);
            
            // outTypeIds->add( expression->typeId);
            // return SIGNAL_SUCCESS;
        }
        else if (expression->typeId == AST_FLOAT64) {
            double val = expression->f64Value;

            // TOKENINFO(expression->location)
            // info.code->addDebugText("  expr push float");
            builder.emit_li64(BC_REG_A, *(u64*)&val);
            builder.emit_push(BC_REG_A);
            
            // outTypeIds->add( expression->typeId);
            // return SIGNAL_SUCCESS;
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
            //     return SIGNAL_SUCCESS;
            // }

            if(expression->enum_ast) {
                // SAMECODE as AST_MEMBER further down
                auto& mem = expression->enum_ast->members[expression->enum_member];
                int size = info.ast->getTypeSize(expression->enum_ast->actualType);
                Assert(size <= 8);
                
                if(size > 4) {
                    Assert(sizeof(mem.enumValue) == size); // bug in parser/AST
                    builder.emit_li64(BC_REG_A, mem.enumValue);
                    builder.emit_push(BC_REG_A);
                } else {
                    builder.emit_li32(BC_REG_A, mem.enumValue);
                    builder.emit_push(BC_REG_A);
                }

                outTypeIds->add(expression->enum_ast->actualType);
                return SIGNAL_SUCCESS;
            }

            // check data type and get it
            // auto id = info.ast->findIdentifier(idScope, , expression->name);
            auto id = expression->identifier;
            if (id) {
                if (id->type == Identifier::VARIABLE) {
                    auto varinfo = info.ast->getVariableByIdentifier(id);
                    if(!varinfo->versions_typeId[info.currentPolyVersion].isValid()){
                        return SIGNAL_FAILURE;
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
                            builder.emit_dataptr(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion]);
                            generatePush(BC_REG_B, 0, varinfo->versions_typeId[info.currentPolyVersion]);
                        } break; 
                        case VariableInfo::LOCAL: {
                            // generatePush(BC_REG_LP, varinfo->versions_dataOffset[info.currentPolyVersion],
                            //     varinfo->versions_typeId[info.currentPolyVersion]);
                            generatePush(BC_REG_LOCALS, varinfo->versions_dataOffset[info.currentPolyVersion],
                                varinfo->versions_typeId[info.currentPolyVersion]);
                        } break;
                        case VariableInfo::ARGUMENT: {
                            generatePush_get_param(varinfo->versions_dataOffset[info.currentPolyVersion],
                                varinfo->versions_typeId[info.currentPolyVersion]);
                        } break;
                        case VariableInfo::MEMBER: {
                            // NOTE: Is member variable/argument always at this offset with all calling conventions?
                            auto type = varinfo->versions_typeId[info.currentPolyVersion];
                            builder.emit_get_param(BC_REG_B, 0, 8, AST::IsDecimal(type));
                        
                            generatePush(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion],
                                varinfo->versions_typeId[info.currentPolyVersion]);
                        } break;
                        default: {
                            Assert(false);
                        }
                    }

                    outTypeIds->add(varinfo->versions_typeId[info.currentPolyVersion]);
                    return SIGNAL_SUCCESS;
                } else if (id->type == Identifier::FUNCTION) {
                    _GLOG(log::out << " expr func push " << expression->name << "\n";)

                    if(id->funcOverloads.overloads.size()==1){
                        Assert(false); // nocheckin
                        // builder.emit_codeptr(BC_REG_A, );
                        // info.addCallToResolve(info.code->length(), id->funcOverloads.overloads[0].funcImpl);
                        // info.addImm(id->funcOverloads.overloads[0].funcImpl->address);

                        // info.code->exportSymbol(expression->name, id->funcOverloads.overloads[0].funcImpl->address);
                    } else {
                        ERR_SECTION(
                            ERR_HEAD2(expression->location)
                            ERR_MSG("You can only refer to functions with one overload. '"<<expression->name<<"' has "<<id->funcOverloads.overloads.size()<<".")
                            ERR_LINE2(expression->location,"cannot refer to function")
                        )
                    }

                    builder.emit_push(BC_REG_A);

                    outTypeIds->add(AST_FUNC_REFERENCE);
                    return SIGNAL_SUCCESS;
                } else {
                    INCOMPLETE
                }
            } else {
                Assert(info.hasForeignErrors());
                // {
                //     ERR_SECTION(
                //         ERR_HEAD2(expression->location)
                //         ERR_MSG("'"<<expression->tokenRange.firstToken<<"' is not declared.")
                //         ERR_LINE2(expression->location,"undeclared")
                //     )
                // }
                return SIGNAL_FAILURE;
            }
        } else if (expression->typeId == AST_FNCALL) {
            return generateFnCall(expression, outTypeIds, false);
        } else if(expression->typeId==AST_STRING){
            // Assert(expression->constStrIndex!=-1);
            int& constIndex = expression->versions_constStrIndex[info.currentPolyVersion];
            auto& constString = info.ast->getConstString(constIndex);

            TypeInfo* typeInfo = nullptr;
            if (expression->flags & ASTNode::NULL_TERMINATED) {
                typeInfo = info.ast->convertToTypeInfo("char*", info.ast->globalScopeId, true);
                Assert(typeInfo);
                builder.emit_dataptr(BC_REG_B, constString.address);
                builder.emit_push(BC_REG_B);
                TypeId ti = AST_CHAR;
                ti.setPointerLevel(1);
                outTypeIds->add(ti);
            } else {
                typeInfo = info.ast->convertToTypeInfo("Slice<char>", info.ast->globalScopeId, true);
                if(!typeInfo){
                    ERR_SECTION(
                        ERR_HEAD2(expression->location)
                        ERR_MSG("'"<<compiler->lexer.tostring(expression->location)<<"' cannot be converted to Slice<char> because Slice doesn't exist. Did you forget #import \"Basic\"")
                    )
                    return SIGNAL_FAILURE;
                }
                Assert(typeInfo->astStruct);
                Assert(typeInfo->astStruct->members.size() == 2);
                Assert(typeInfo->structImpl->members[0].offset == 0);
                // Assert(typeInfo->structImpl->members[1].offset == 8);// len: u64
                // Assert(typeInfo->structImpl->members[1].offset == 12); // len: u32
                // last member in slice is pushed first
                if(typeInfo->structImpl->members[1].offset == 8){
                    builder.emit_li32(BC_REG_A,constString.length);
                    builder.emit_push(BC_REG_A);
                } else {
                    builder.emit_li32(BC_REG_A,constString.length);
                    builder.emit_push(BC_REG_A);
                }

                // first member is pushed last
                builder.emit_dataptr(BC_REG_B, constString.address);
                builder.emit_push(BC_REG_B);
                outTypeIds->add(typeInfo->id);
            }
            return SIGNAL_SUCCESS;
        } else if(expression->typeId == AST_SIZEOF){

            // TypeId typeId = info.ast->convertToTypeId(expression->name, idScope);
            // Assert(typeId.isValid()); // Did type checker fix this? Maybe not on errors?

            TypeId typeId = expression->versions_outTypeSizeof[info.currentPolyVersion];
            u32 size = info.ast->getTypeSize(typeId);

            builder.emit_li32(BC_REG_A, size);
            builder.emit_push(BC_REG_A);

            outTypeIds->add(AST_UINT32);
            return SIGNAL_SUCCESS;
        } else if(expression->typeId == AST_TYPEID){

            // TypeId typeId = info.ast->convertToTypeId(expression->name, idScope);
            // Assert(typeId.isValid()); // Did type checker fix this? Maybe not on errors?

            TypeId result_typeId = expression->versions_outTypeTypeid[info.currentPolyVersion];

            const char* tname = "lang_TypeId";
            TypeInfo* typeInfo = info.ast->convertToTypeInfo(tname, info.ast->globalScopeId, true);
            if(!typeInfo){
                Assert(info.hasForeignErrors());
                return SIGNAL_FAILURE;
            }
            Assert(typeInfo->getSize() == 4);

            // NOTE: This is scuffed, could be a bug in the future
            // lang::TypeId tmp={};
            // tmp.index0 = result_typeId._infoIndex0;
            // tmp.index1 = result_typeId._infoIndex1;
            // tmp.ptr_level = result_typeId.pointer_level;

            // builder.emit_li32(BC_REG_A, *(u32*)&tmp);
            // builder.emit_push(BC_REG_A);
            
            // NOTE: Struct members are pushed in the opposite order
            builder.emit_li32(BC_REG_A, result_typeId.pointer_level);
            builder.emit_push(BC_REG_A);

            builder.emit_li32(BC_REG_A, result_typeId._infoIndex1);
            builder.emit_push(BC_REG_A);

            builder.emit_li32(BC_REG_A, result_typeId._infoIndex0);
            builder.emit_push(BC_REG_A);

            outTypeIds->add(typeInfo->id);
            return SIGNAL_SUCCESS;
        }  else if(expression->typeId == AST_ASM){
            // TODO: How does polymorphism complicated this?
            //   You don't want duplicate inline assembly.
            if(!info.disableCodeGeneration) {
                u32 start = info.code->rawInlineAssembly.used;
                Assert(false); // nocheckin, rewrite removed token range
                // +2 and -1 to avoid adding "asm { }"
                #ifdef gone
                TokenRange range{};
                range.firstToken = expression->tokenRange.tokenStream()->get(expression->tokenRange.startIndex()+2);
                range.endIndex = expression->tokenRange.endIndex-1;
                
                int size = range.queryFeedSize();
                info.code->rawInlineAssembly._reserve(info.code->rawInlineAssembly.used + size);
                int feed_size = range.feed(info.code->rawInlineAssembly.data() + info.code->rawInlineAssembly.size(), size);
                // Assert(feed_size == size);
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
                builder.emit_({BC_ASM, ASM_ENCODE_INDEX(asmInstanceIndex)}); // ASM_ENCODE_INDEX results in bytes separated by commas
                // builder.emit_({BC_ASM, (u8)(asmInstanceIndex&0xFF), (u8)((asmInstanceIndex>>8)&0xFF), (u8)((asmInstanceIndex>>16)&0xFF)});
                #endif
            }
            outTypeIds->add(AST_VOID);
            return SIGNAL_SUCCESS;
        } 
        else if(expression->typeId == AST_NAMEOF){
            // TypeId typeId = info.ast->convertToTypeId(expression->name, idScope);
            // Assert(typeId.isValid());

            // std::string name = info.ast->typeToString(typeId);

            // Assert(expression->constStrIndex!=-1);
            int constIndex = expression->versions_constStrIndex[info.currentPolyVersion];
            auto& constString = info.ast->getConstString(constIndex);

            TypeInfo* typeInfo = info.ast->convertToTypeInfo("Slice<char>", info.ast->globalScopeId, true);
            if(!typeInfo){
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG("'"<<compiler->lexer.tostring(expression->location)<<"' cannot be converted to Slice<char> because Slice doesn't exist. Did you forget #import \"Basic\"")
                )
                return SIGNAL_FAILURE;
            }
            Assert(typeInfo->astStruct);
            Assert(typeInfo->astStruct->members.size() == 2);
            Assert(typeInfo->structImpl->members[0].offset == 0);
            // Assert(typeInfo->structImpl->members[1].offset == 8);// len: u64
            // Assert(typeInfo->structImpl->members[1].offset == 12); // len: u32

            // last member in slice is pushed first
            if(typeInfo->structImpl->members[1].offset == 8){
                builder.emit_li32(BC_REG_A, constString.length);
                builder.emit_push(BC_REG_A);
            } else {
                builder.emit_li32(BC_REG_A, constString.length);
                builder.emit_push(BC_REG_A);
            }

            // first member is pushed last
            builder.emit_dataptr(BC_REG_B, constString.address);
            builder.emit_push(BC_REG_B);

            outTypeIds->add(typeInfo->id);
            return SIGNAL_SUCCESS;
        } else if (expression->typeId == AST_NULL) {
            // TODO: Move into the type checker?
            // info.code->addDebugText("  expr push null");
            builder.emit_li32(BC_REG_A, 0);
            builder.emit_push(BC_REG_A);

            TypeInfo *typeInfo = info.ast->convertToTypeInfo("void", info.ast->globalScopeId, true);
            TypeId newId = typeInfo->id;
            newId.setPointerLevel(1);
            outTypeIds->add( newId);
            return SIGNAL_SUCCESS;
        } else {
            auto typeName = info.ast->typeToString(expression->typeId);
            // builder.emit_({BC_PUSH,BC_REG_A}); // push something so the stack stays synchronized, or maybe not?
            ERR_SECTION(
                ERR_HEAD2(expression->location)
                ERR_MSG("'" <<typeName << "' is an unknown data type.")
                ERR_LINE2(expression->location, typeName)
            )
            // log::out <<  log::RED<<"GenExpr: data type not implemented\n";
            outTypeIds->add( AST_VOID);
            return SIGNAL_FAILURE;
        }
        outTypeIds->add(expression->typeId);
    } else {
        FuncImpl* operatorImpl = nullptr;
        if(expression->versions_overload._array.size()>0)
            operatorImpl = expression->versions_overload[info.currentPolyVersion].funcImpl;
        TypeId ltype = AST_VOID;
        DynamicArray<TypeId> tmp_types{};
        if(operatorImpl){
            return generateFnCall(expression, outTypeIds, true);
        } else if (expression->typeId == AST_REFER) {

            SignalIO result = generateReference(expression->left,&ltype,idScope);
            if(result!=SIGNAL_SUCCESS)
                return SIGNAL_FAILURE;
            if(ltype.getPointerLevel()==3){
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG("Cannot take a reference of a triple pointer (compiler has a limit of 0-3 depth of pointing). Cast to u64 if you need triple pointers.")
                )
                return SIGNAL_FAILURE;
            }
            ltype.setPointerLevel(ltype.getPointerLevel()+1);
            outTypeIds->add( ltype); 
        } else if (expression->typeId == AST_DEREF) {
            DynamicArray<TypeId> ltypes{};
            
            SignalIO result = generateExpression(expression->left, &ltypes);
            if (result != SIGNAL_SUCCESS)
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
                    ERR_HEAD2(expression->left->location)
                    ERR_MSG("Cannot dereference more than one value.")
                    ERR_LINE2(expression->left->location, values.data())
                )
                return SIGNAL_FAILURE;
            }
            ltype = ltypes[0];

            if (!ltype.isPointer()) {
                ERR_SECTION(
                    ERR_HEAD2(expression->left->location)
                    ERR_MSG("Cannot dereference " << info.ast->typeToString(ltype) << ".")
                    ERR_LINE2(expression->left->location, "bad")
                )
                outTypeIds->add( AST_VOID);
                return SIGNAL_FAILURE;
            }

            TypeId outId = ltype;
            outId.setPointerLevel(outId.getPointerLevel()-1);

            if (outId == AST_VOID) {
                ERR_SECTION(
                    ERR_HEAD2(expression->left->location)
                    ERR_MSG("Cannot dereference " << info.ast->typeToString(ltype) << ".")
                    ERR_LINE2(expression->left->location, "bad")
                )
                return SIGNAL_FAILURE;
            }
            Assert(info.ast->getTypeSize(ltype) == 8); // left expression must return pointer
            builder.emit_pop(BC_REG_B);

            if(outId.isPointer()){
                u8 size = info.ast->getTypeSize(outId);

                BCRegister reg = BC_REG_A;

                builder.emit_mov_rm(reg, BC_REG_B, size);
                builder.emit_push(reg);
            } else {
                generatePush(BC_REG_B, 0, outId);
            }
            outTypeIds->add(outId);
        }
        else if (expression->typeId == AST_NOT) {
            SignalIO result = generateExpression(expression->left, &tmp_types);
            if(tmp_types.size())
                ltype = tmp_types.last();
            if (result != SIGNAL_SUCCESS)
                return result;
            // TypeInfo *ti = info.ast->getTypeInfo(ltype);
            u32 size = info.ast->getTypeSize(ltype);
            // using two registers instead of one because
            // it is easier to implement in x64 generator 
            // or not actually.
            BCRegister reg = BC_REG_A;
            // u8 reg2 = RegBySize(BC_BX, size);

            builder.emit_pop(reg);
            builder.emit_lnot(reg, reg);
            builder.emit_push(reg);
            // builder.emit_({BC_NOT, reg, reg2});
            // builder.emit_push(reg2);

            outTypeIds->add(AST_BOOL);
        }
        else if (expression->typeId == AST_BNOT) {
            SignalIO result = generateExpression(expression->left, &tmp_types);
            if(tmp_types.size())
                ltype = tmp_types.last();
            if (result != SIGNAL_SUCCESS)
                return result;
            // TypeInfo *ti = info.ast->getTypeInfo(ltype);
            u32 size = info.ast->getTypeSize(ltype);
            BCRegister reg = BC_REG_A;

            builder.emit_pop(reg);
            builder.emit_bnot(reg, reg);
            builder.emit_push(reg);

            outTypeIds->add(ltype);
        }
        else if (expression->typeId == AST_CAST) {
            SignalIO result = generateExpression(expression->left, &tmp_types);
            if(tmp_types.size())
                ltype = tmp_types.last();
            if (result != SIGNAL_SUCCESS)
                return result;

            // TypeInfo *ti = info.ast->getTypeInfo(ltype);
            // TypeInfo *tic = info.ast->getTypeInfo(castType);
            u32 typeSize = info.ast->getTypeSize(ltype);
            TypeId castType = expression->versions_castType[info.currentPolyVersion];
            u32 castSize = info.ast->getTypeSize(castType);
            BCRegister lreg = BC_REG_A;
            BCRegister creg = BC_REG_A;
            if(expression->isUnsafeCast()) {
                if(expression->left->typeId == AST_ASM) {
                    // TODO: Deprecate this and use asm<i32> {} instead of cast_unsafe<i32> asm {}. asm -> i32 {} is an alternative syntax
                    //  asm<i32> makes more since because the casting is a little special since
                    //  we don't know what type the inline assembly produces. Maybe it does 2 pushes
                    //  or none. With unsafe cast we assume a type which isn't ideal.
                    //  Unfortunately, it's difficult to know what type is pushed in the inline assembly.
                    //  It might ruin the stack and frame pointers.
                    
                    // The unsafe cast implies that the asm block did this. hopefully it did.
                    SignalIO result = generateArtificialPush(castType);
                } else {
                    builder.emit_pop(lreg);
                    builder.emit_push(creg);
                }
                outTypeIds->add(castType);
            } else {
                // if ((castType.isPointer() && ltype.isPointer()) || (castType.isPointer() && (ltype == (TypeId)AST_INT64 ||
                //     ltype == (TypeId)AST_UINT64 || ltype == (TypeId)AST_INT32)) || ((castType == (TypeId)AST_INT64 ||
                //     castType == (TypeId)AST_UINT64 || ltype == (TypeId)AST_INT32) && ltype.isPointer())) {
                //     builder.emit_pop(lreg);
                //     builder.emit_push(creg);
                //     outTypeIds->add(castType);
                //     // data is fine as it is, just change the data type
                // } else { 
                bool yes = performSafeCast(ltype, castType);
                if (yes) {
                    outTypeIds->add(castType);
                } else {
                    Assert(info.hasForeignErrors());
                    
                    outTypeIds->add(ltype); // ltype since cast failed
                    return SIGNAL_FAILURE;
                }
                // }
            }
        }
        else if (expression->typeId == AST_FROM_NAMESPACE) {
            // info.code->addDebugText("ast-namespaced expr\n");

            auto si = info.ast->findScope(expression->name, info.currentScopeId, true);
            SignalIO result = generateExpression(expression->left, &tmp_types, si->id);
            if(tmp_types.size())
                ltype = tmp_types.last();
            if (result != SIGNAL_SUCCESS)
                return result;

            outTypeIds->add(ltype);
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
                        // ERR_HEAD2(expression->location, expression->tokenRange.firstToken << " is not a member of enum " << typeInfo->astEnum->name << "\n";
                        // )
                        return SIGNAL_FAILURE;
                    }

                    builder.emit_li32(BC_REG_A, enumValue);
                    builder.emit_push(BC_REG_A);

                    outTypeIds->add(typeInfo->id);
                    return SIGNAL_SUCCESS;
                }
            }
            // TODO: We don't allow Apple{"Green",9}.name because initializer is not
            //  a reference. We should allow it though.

            SignalIO result = generateReference(expression,&exprId);
            if(result != SIGNAL_SUCCESS) {
                return SIGNAL_FAILURE;
            }

            builder.emit_pop(BC_REG_B);
            result = generatePush(BC_REG_B,0, exprId);
            
            outTypeIds->add(exprId);
        }
        else if (expression->typeId == AST_INITIALIZER) {
            TypeId castType = expression->versions_castType[info.currentPolyVersion];
            TypeInfo* base_typeInfo = info.ast->getTypeInfo(castType.baseType()); // TODO: castType should be renamed
            // TypeInfo *structInfo = info.ast->getTypeInfo(info.currentScopeId, Token(*expression->name));
            if (!base_typeInfo) {
                auto str = info.ast->typeToString(castType);
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG_COLORED("Cannot do initializer on type '" << log::YELLOW << str << log::NO_COLOR << "'.")
                    ERR_LINE2(expression->location, "bad")
                )
                return SIGNAL_FAILURE;
            }
            bool is_struct = base_typeInfo->astStruct && castType.isNormalType();
            if(!is_struct && expression->args.size() > 1) {
                auto str = info.ast->typeToString(castType);
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG_COLORED("The type '" << log::YELLOW << str << log::NO_COLOR << "' is not a struct and cannot have more than one value/expression in the initializer.")
                    ERR_LINE2(expression->args[1]->location, "not allowed")
                )
                return SIGNAL_FAILURE;
            }    
            
            ASTStruct *ast_struct = base_typeInfo->astStruct;
            
            if(is_struct) {
                // store expressions in a list so we can iterate in reverse
                // TODO: parser could arrange the expressions in reverse.
                //   it would probably be easier since it constructs the nodes
                //   and has a little more context and control over it.
                // std::vector<ASTExpression *> exprs;
                TINY_ARRAY(ASTExpression*,exprs,6);
                exprs.resize(ast_struct->members.size());

                // Assert(expression->args);
                // for (int index = 0; index < (int)expression->args->size(); index++) {
                //     ASTExpression *expr = expression->args->get(index);
                    
                for (int index = 0; index < (int)expression->args.size(); index++) {
                    ASTExpression *expr = expression->args.get(index);

                    if (!expr->namedValue.len) {
                        if ((int)exprs.size() <= index) {
                            ERR_SECTION(
                                ERR_HEAD2(expr->location)
                                ERR_MSG("To many members for struct " << ast_struct->name << " (" << ast_struct->members.size() << " member(s) allowed).")
                            )
                            continue;
                        }
                        else
                            exprs[index] = expr;
                    } else {
                        int memIndex = -1;
                        for (int i = 0; i < (int)ast_struct->members.size(); i++) {
                            if (ast_struct->members[i].name == expr->namedValue) {
                                memIndex = i;
                                break;
                            }
                        }
                        if (memIndex == -1) {
                            ERR_SECTION(
                                ERR_HEAD2(expr->location)
                                ERR_MSG("'"<<expr->namedValue << "' is not an member in " << ast_struct->name << ".")
                            )
                            continue;
                        } else {
                            if (exprs[memIndex]) {
                                ERR_SECTION(
                                    ERR_HEAD2(expr->location)
                                    ERR_MSG("Argument for " << ast_struct->members[memIndex].name << " is already specified.")
                                    ERR_LINE2(exprs[memIndex]->location, "here")
                                )
                            } else {
                                exprs[memIndex] = expr;
                            }
                        }
                    }
                }

                for (int i = 0; i < (int)ast_struct->members.size(); i++) {
                    auto &mem = ast_struct->members[i];
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
                        exprId = base_typeInfo->getMember(index).typeId;
                        SignalIO result = generateDefaultValue(BC_REG_INVALID, 0, exprId, nullptr);
                        if (result != SIGNAL_SUCCESS)
                            return result;
                        // ERR_SECTION(
                    // ERR_HEAD2(expression->location, "Missing argument for " << astruct->members[index].name << " (call to " << astruct->name << ").\n";
                        // )
                        // continue;
                    } else {
                        SignalIO result = generateExpression(expr, &tmp_types);
                        if(tmp_types.size()) exprId = tmp_types.last();
                        if (result != SIGNAL_SUCCESS)
                            return result;
                    }

                    if (index >= (int)base_typeInfo->astStruct->members.size()) {
                        // ERR() << "To many arguments! func:"<<*expression->funcName<<" max: "<<astFunc->arguments.size()<<"\n";
                        continue;
                    }
                    TypeId tid = base_typeInfo->getMember(index).typeId;
                    if (!performSafeCast(exprId, tid)) {   // implicit conversion
                        // if(astFunc->arguments[index].typeId!=dt){ // strict, no conversion
                        ERRTYPE1(expr->location, exprId, tid, "(initializer)");
                        
                        continue;
                    }
                }
            } else {
                if(expression->args.size()>0) {
                    ASTExpression* expr = expression->args[0];
                    TypeId expr_type{};
                    SignalIO result = generateExpression(expr, &tmp_types);
                     if(tmp_types.size()) expr_type = tmp_types.last();
                    if (result != SIGNAL_SUCCESS)
                        return result;
                        
                    // if(expr_type!=castType){ // strict, no conversion
                    if (!performSafeCast(expr_type, castType)) { // implicit conversion
                        ERRTYPE1(expr->location, expr_type, castType, "(initializer)");
                        return SIGNAL_FAILURE;
                    }
                } else {
                    SignalIO result = generateDefaultValue(BC_REG_INVALID, 0, castType, nullptr);
                    if (result != SIGNAL_SUCCESS)
                        return result;
                }
            }
            // if((int)exprs.size()!=(int)structInfo->astStruct->members.size()){
            //     // return SIGNAL_FAILURE;
            // }
            if(outTypeIds)
                outTypeIds->add(castType);
        }
        else if (expression->typeId == AST_SLICE_INITIALIZER) {

            // TypeInfo* typeInfo = info.ast->getTypeInfo(*expression->name);
            // if(!structInfo||!structInfo->astStruct){
            //     ERR_HEAD2(expression->location) << "cannot do initializer on non-struct "<<log::YELLOW<<*expression->name<<"\n";
            //     return SIGNAL_FAILURE;
            // }

            // int index = (int)exprs.size();
            // // int index=-1;
            // while(index>0){
            //     index--;
            //     ASTExpression* expr = exprs[index];
            //     TypeId exprId;
            //     int result = GenerateExpression(info,expr,&exprId);
            //     if(result!=SIGNAL_SUCCESS) return result;

            //     if(index>=(int)structInfo->astStruct->members.size()){
            //         // ERR() << "To many arguments! func:"<<*expression->funcName<<" max: "<<astFunc->arguments.size()<<"\n";
            //         continue;
            //     }
            //     if(!PerformSafeCast(info,exprId,structInfo->astStruct->members[index].typeId)){ // implicit conversion
            //     // if(astFunc->arguments[index].typeId!=dt){ // strict, no conversion
            //         ERR_HEAD2(expr->location) << "Type mismatch (initializer): "<<*info.ast->getTypeInfo(exprId)<<" - "<<*info.ast->getTypeInfo(structInfo->astStruct->members[index].typeId)<<"\n";
            //         continue;
            //     }
            // }
            // if((int)exprs.size()!=(int)structInfo->astStruct->members.size()){
            //     ERR_HEAD2(expression->location) << "Found "<<index<<" arguments but "<<*expression->name<<" requires "<<structInfo->astStruct->members.size()<<"\n";
            //     // return SIGNAL_FAILURE;
            // }

            // outTypeIds->add( structInfo->id;
        }
        else if (expression->typeId == AST_FROM_NAMESPACE) {
            // TODO: chould not work on enums? Enum.One is used instead
            // TypeInfo* typeInfo = info.ast->getTypeInfo(*expression->name);
            // if(!typeInfo||!typeInfo->astEnum){
            //     ERR_HEAD2(expression->location) << "cannot access "<<(expression->member?*expression->member:"?")<<" from non-enum "<<*expression->name<<"\n";
            //     return SIGNAL_FAILURE;
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
            //     builder.emit_({BC_LI,BC_REG_A});
            //     info.addImm(typeInfo->astEnum->members[index].enumValue);
            //     builder.emit_push(BC_REG_A);

            //     outTypeIds->add( AST_INT32;
            // }
        }
        else if(expression->typeId==AST_RANGE){
            TypeInfo* typeInfo = info.ast->convertToTypeInfo("Range", info.ast->globalScopeId, true);
            if(!typeInfo){
                ERR_SECTION(
                    ERR_HEAD2(expression->location)
                    ERR_MSG("'"<<compiler->lexer.tostring(expression->location)<<"' cannot be converted to struct Range since it doesn't exist. Did you forget #import \"Basic\".")
                    ERR_LINE2(expression->location,"Where is Range?")
                )
                return SIGNAL_FAILURE;
            }
            Assert(typeInfo->astStruct);
            Assert(typeInfo->astStruct->members.size() == 2);
            Assert(typeInfo->structImpl->members[0].typeId == typeInfo->structImpl->members[1].typeId);

            TypeId inttype = typeInfo->structImpl->members[0].typeId;

            TypeId ltype={};
            SignalIO result = generateExpression(expression->left,&ltype,idScope);
            if(result!=SIGNAL_SUCCESS)
                return result;
            TypeId rtype={};
            result = generateExpression(expression->right,&rtype,idScope);
            if(result!=SIGNAL_SUCCESS)
                return result;
            
            BCRegister lreg = BC_REG_C;
            BCRegister rreg = BC_REG_D;
            builder.emit_pop(rreg);
            builder.emit_pop(lreg);

            builder.emit_push(rreg);
            if(!performSafeCast(rtype,inttype)){
                std::string msg = info.ast->typeToString(rtype);
                ERR_SECTION(
                    ERR_HEAD2(expression->right->location)
                    ERR_MSG("Cannot convert to "<<info.ast->typeToString(inttype)<<".")
                    ERR_LINE2(expression->right->location,msg.c_str())
                )
            }

            builder.emit_push(lreg);
            if(!performSafeCast(ltype,inttype)){
                std::string msg = info.ast->typeToString(ltype);
                ERR_SECTION(
                    ERR_HEAD2(expression->right->location)
                    ERR_MSG("Cannot convert to "<<info.ast->typeToString(inttype)<<".")
                    ERR_LINE2(expression->right->location,msg.c_str());
                )
            }

            outTypeIds->add(typeInfo->id);
            return SIGNAL_SUCCESS;
        } 
        else if(expression->typeId == AST_INDEX){
            FuncImpl* operatorImpl = nullptr;
            if(expression->versions_overload._array.size()>0)
                operatorImpl = expression->versions_overload[info.currentPolyVersion].funcImpl;
            TypeId ltype = AST_VOID;
            if(operatorImpl){
                return generateFnCall(expression, outTypeIds, true);
            }
            // TOKENINFO(expression->location);
            SignalIO err = generateExpression(expression->left, &tmp_types);
            if(tmp_types.size()) ltype = tmp_types.last();
            if (err != SIGNAL_SUCCESS)
                return SIGNAL_FAILURE;

            TypeId rtype;
            err = generateExpression(expression->right, &tmp_types);
            if(tmp_types.size()) rtype = tmp_types.last();
            if (err != SIGNAL_SUCCESS)
                return SIGNAL_FAILURE;

            if(!ltype.isPointer()){
                if(!info.hasForeignErrors()){
                    std::string strtype = info.ast->typeToString(ltype);
                    ERR_SECTION(
                        ERR_HEAD2(expression->right->location)
                        ERR_MSG("Index operator ( array[23] ) requires pointer type in the outer expression. '"<<strtype<<"' is not a pointer.")
                        ERR_LINE2(expression->right->location,strtype.c_str())
                    )
                }
                return SIGNAL_FAILURE;
            }
            if(!AST::IsInteger(rtype)){
                std::string strtype = info.ast->typeToString(rtype);
                ERR_SECTION(
                    ERR_HEAD2(expression->right->location)
                    ERR_MSG("Index operator ( array[23] ) requires integer type in the inner expression. '"<<strtype<<"' is not an integer.")
                    ERR_LINE2(expression->right->location,strtype.c_str())
                )
                return SIGNAL_FAILURE;
            }
            ltype.setPointerLevel(ltype.getPointerLevel()-1);

            u32 lsize = info.ast->getTypeSize(ltype);
            u32 rsize = info.ast->getTypeSize(rtype);
            BCRegister reg = BC_REG_D;
            builder.emit_pop(reg); // integer
            builder.emit_pop(BC_REG_B); // reference
            if(lsize>1){
                builder.emit_li32(BC_REG_A, lsize);
                builder.emit_mul(BC_REG_A, reg, false, 8, false);
            }
            builder.emit_add(BC_REG_B, BC_REG_A, false, 8);

            SignalIO result = generatePush(BC_REG_B, 0, ltype);

            outTypeIds->add(ltype);
        }
        else if(expression->typeId == AST_INCREMENT || expression->typeId == AST_DECREMENT){
            SignalIO result = generateReference(expression->left, &ltype, idScope);
            if(result != SIGNAL_SUCCESS){
                return SIGNAL_FAILURE;
            }
            
            if(!AST::IsInteger(ltype) && !ltype.isPointer()){
                std::string strtype = info.ast->typeToString(ltype);
                ERR_SECTION(
                    ERR_HEAD2(expression->left->location)
                    ERR_MSG("Increment/decrement only works on integer types unless overloaded.")
                    ERR_LINE2(expression->left->location, strtype.c_str())
                )
                return SIGNAL_FAILURE;
            }

            u8 size = info.ast->getTypeSize(ltype);
            BCRegister reg = BC_REG_A;

            builder.emit_pop(BC_REG_B); // reference

            builder.emit_mov_rm(reg, BC_REG_B, size);
            // post
            if(expression->isPostAction())
                builder.emit_push(reg);
            if(expression->typeId == AST_INCREMENT)
                builder.emit_incr(reg, 1);
            else
                builder.emit_incr(reg, -1);
            builder.emit_mov_mr(BC_REG_B, reg, size);
            // pre
            if(!expression->isPostAction())
                builder.emit_push(reg);
                
            outTypeIds->add(ltype);
        }
        else if(expression->typeId == AST_UNARY_SUB){
            SignalIO result = generateExpression(expression->left, &tmp_types, idScope);
            if(tmp_types.size()) ltype = tmp_types.last();
            if(result != SIGNAL_SUCCESS){
                return SIGNAL_FAILURE;
            }
            
            if(AST::IsInteger(ltype)) {
                u8 size = info.ast->getTypeSize(ltype);
                BCRegister regFinal = BC_REG_A;
                BCRegister regValue = BC_REG_D;

                builder.emit_bxor(regFinal, regFinal);
                builder.emit_pop(regValue);
                builder.emit_sub(regFinal, regValue, false, 8); // assuming 64-bit integer
                builder.emit_push(regFinal);
                outTypeIds->add(ltype);
            } else if (AST::IsDecimal(ltype)) {
                u8 size = info.ast->getTypeSize(ltype);
                BCRegister regMask = BC_REG_A;
                BCRegister regValue = BC_REG_D;
                if(size == 8)
                    builder.emit_li64(regMask, 0x8000000000000000);
                else
                    builder.emit_li32(regMask, 0x80000000);
                builder.emit_pop(regValue);
                builder.emit_bxor(regValue, regMask);
                builder.emit_push(regValue);
                outTypeIds->add(ltype);
            } else {
                std::string strtype = info.ast->typeToString(ltype);
                ERR_SECTION(
                    ERR_HEAD2(expression->left->location)
                    ERR_MSG("Unary subtraction only works with integers and decimals.")
                    ERR_LINE2(expression->left->location, strtype.c_str())
                )
                return SIGNAL_FAILURE;
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
                // WARN_HEAD3(expression->location,"Expression is generated first and then reference. Right to left instead of left to right. "
                // "This is important if you use assignments/increments/decrements on the same memory/reference/variable.\n\n";
                //     WARN_LINE(expression->right->location,"generated first");
                //     WARN_LINE(expression->left->location,"then this");
                // )
                TypeId rtype{};
                SignalIO result = generateExpression(expression->right, &tmp_types, idScope);
                if(tmp_types.size()) rtype = tmp_types.last();
                if(result!=SIGNAL_SUCCESS)
                    return SIGNAL_FAILURE;

                TypeId assignType={};
                result = generateReference(expression->left,&assignType,idScope);
                if(result!=SIGNAL_SUCCESS)
                    return SIGNAL_FAILURE;

                builder.emit_pop(BC_REG_B);

                if (!performSafeCast(rtype, assignType)) { // CASTING RIGHT VALUE TO TYPE ON THE LEFT
                    // std::string leftstr = info.ast->typeToString(assignType);
                    // std::string rightstr = info.ast->typeToString(rtype);
                    ERRTYPE1(expression->location, assignType, rtype, ""
                        // ERR_LINE2(expression->left->location, leftstr.c_str());
                        // ERR_LINE2(expression->right->location,rightstr.c_str());
                    )
                    return SIGNAL_FAILURE;
                }
                // if(assignType != rtype){
                //     if(info.errors != 0 || info.compileInfo->typeErrors !=0){
                //         return SIGNAL_FAILURE;
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
                result = generatePop(BC_REG_B, 0, assignType);
                result = generatePush(BC_REG_B, 0, assignType);
                
                outTypeIds->add(rtype);
            } else {
                    
                // basic operations
                // BREAK(expression->nodeId == 1365)
                TypeId ltype{}, rtype{};
                SignalIO err = generateExpression(expression->left, &tmp_types);
                if(tmp_types.size()) ltype = tmp_types.last();
                if (err != SIGNAL_SUCCESS)
                    return err;
                tmp_types.resize(0);
                err = generateExpression(expression->right, &tmp_types);
                if(tmp_types.size()) rtype = tmp_types.last();
                if (err != SIGNAL_SUCCESS)
                    return err;
                
                TypeInfo* left_info = info.ast->getTypeInfo(ltype.baseType());
                TypeInfo* right_info = info.ast->getTypeInfo(rtype.baseType());
                Assert(left_info && right_info);
                
                if(ltype.getId()>=AST_TRUE_PRIMITIVES || rtype.getId()>=AST_TRUE_PRIMITIVES){
                    Assert(left_info->astStruct || right_info->astStruct); // can we assume that this is a struct?
                    std::string lmsg = info.ast->typeToString(ltype);
                    std::string rmsg = info.ast->typeToString(rtype);
                    ERR_SECTION(
                        ERR_HEAD2(expression->location)
                        ERR_MSG("Cannot do operation on struct.")
                        ERR_LINE2(expression->left->location,lmsg.c_str());
                        ERR_LINE2(expression->right->location,rmsg.c_str());
                    )
                    return SIGNAL_FAILURE;
                }
                
                TypeId outType = ltype;
                OperationType operationType = (OperationType)expression->typeId.getId();
                if(expression->typeId==AST_ASSIGN){
                    operationType = expression->assignOpType;
                }
                
                bool is_arithmetic = operationType == AST_ADD || operationType == AST_SUB || operationType == AST_MUL || operationType == AST_DIV || operationType == AST_MODULUS;
                bool is_comparison = operationType == AST_LESS || operationType == AST_LESS_EQUAL || operationType == AST_GREATER || operationType == AST_GREATER_EQUAL;
                bool is_equality = operationType == AST_EQUAL || operationType == AST_NOT_EQUAL;
                bool is_signed = AST::IsSigned(ltype) || AST::IsSigned(rtype);
                bool is_float = AST::IsDecimal(ltype) || AST::IsDecimal(rtype);
                
                BCRegister left_reg = BC_REG_C; // also out register
                BCRegister right_reg = BC_REG_D;
                builder.emit_pop(right_reg);
                builder.emit_pop(left_reg);
                
                u8 outSize = 0;
                
                /*###########################
                 Validation operations on types
                ################################*/
                if((ltype.isPointer() && AST::IsInteger(rtype)) || (AST::IsInteger(ltype) && rtype.isPointer()) || (ltype.isPointer() && rtype.isPointer())){
                    if(operationType == AST_ADD && ltype.isPointer() && rtype.isPointer()) {
                        ERR_SECTION(
                            ERR_HEAD2(expression->location)
                            ERR_MSG("You cannot add two pointers together. (is this a good or bad restriction?)")
                            ERR_LINE2(expression->left->location, info.ast->typeToString(ltype));
                            ERR_LINE2(expression->right->location, info.ast->typeToString(rtype));
                        )
                        return SIGNAL_FAILURE;
                    }
                    if((operationType == AST_EQUAL || operationType == AST_NOT_EQUAL)) {
                        if((AST::IsInteger(rtype) && info.ast->getTypeSize(rtype) != 8) || (AST::IsInteger(rtype) && info.ast->getTypeSize(rtype) != 8)) {
                            ERR_SECTION(
                                ERR_HEAD2(expression->location)
                                ERR_MSG("When using equality operators with a pointer and integer they must be of the same size. The integer must be u64 or i64.")
                                ERR_LINE2(expression->left->location, info.ast->typeToString(ltype));
                                ERR_LINE2(expression->right->location, info.ast->typeToString(rtype));
                            )
                            return SIGNAL_FAILURE;
                        }
                    }
                    if (operationType != AST_ADD && operationType != AST_SUB  && operationType != AST_EQUAL  && operationType != AST_NOT_EQUAL) {
                        ERR_SECTION(
                            ERR_HEAD2(expression->location)
                            ERR_MSG(OP_NAME(operationType) << " does not work with pointers. Only addition, subtraction and equality works.")
                            ERR_LINE2(expression->left->location, info.ast->typeToString(ltype));
                            ERR_LINE2(expression->right->location, info.ast->typeToString(rtype));
                        )
                        return SIGNAL_FAILURE;
                    }
                    if(rtype.isPointer()) {
                        outType = rtype; // Make sure '1 + ptr' results in a pointer type and not an int while 'ptr + 1' would be a pointer.
                        outSize = ast->getTypeSize(outType);
                    }
                } else if ((AST::IsInteger(ltype) || ltype == AST_CHAR || left_info->astEnum) && (AST::IsInteger(rtype) || rtype == AST_CHAR || right_info->astEnum)){
                    // TODO: WE DON'T CHECK SIGNEDNESS WITH ENUMS.
                    u8 lsize = info.ast->getTypeSize(ltype);
                    u8 rsize = info.ast->getTypeSize(rtype);
                    if(operationType == AST_DIV) {
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD2(expression->location)
                                ERR_MSG("You cannot divide signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE2(expression->left->location,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE2(expression->right->location,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                            return SIGNAL_FAILURE;
                        }
                    } else if(operationType == AST_MUL) {
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD2(expression->location)
                                ERR_MSG("You cannot multiply signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE2(expression->left->location,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE2(expression->right->location,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                            return SIGNAL_FAILURE;
                        }
                    }  else if(operationType == AST_MODULUS) {
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD2(expression->location)
                                ERR_MSG("You cannot take remainder (modulus) of signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE2(expression->left->location,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE2(expression->right->location,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                            return SIGNAL_FAILURE;
                        }
                    } else if(is_comparison) {
                        if(AST::IsSigned(ltype) != AST::IsSigned(rtype)) {
                            ERR_SECTION(
                                ERR_HEAD2(expression->location)
                                ERR_MSG("You cannot compare signed and unsigned integers. Both integers must be either signed or unsigned.")
                                ERR_LINE2(expression->left->location,(AST::IsSigned(ltype)?"signed":"unsigned"))
                                ERR_LINE2(expression->right->location,(AST::IsSigned(rtype)?"signed":"unsigned"))
                            )
                            return SIGNAL_FAILURE;
                        }
                    }
                    u8 outSize = lsize > rsize ? lsize : rsize;
                    
                    if(lsize != outSize || (!AST::IsSigned(ltype) && AST::IsSigned(rtype))) {
                        if(!AST::IsSigned(ltype) && !AST::IsSigned(rtype))
                            builder.emit_cast(left_reg, CAST_UINT_UINT, lsize, outSize);
                        if(!AST::IsSigned(ltype) && AST::IsSigned(rtype))
                            builder.emit_cast(left_reg, CAST_UINT_SINT, lsize, outSize);
                        if(AST::IsSigned(ltype) && !AST::IsSigned(rtype))
                            builder.emit_cast(left_reg, CAST_SINT_UINT, lsize, outSize);
                        if(AST::IsSigned(ltype) && AST::IsSigned(rtype))
                            builder.emit_cast(left_reg, CAST_SINT_SINT, lsize, outSize);
                    }
                    if(rsize != outSize || (!AST::IsSigned(rtype) && AST::IsSigned(ltype))) {
                        if(!AST::IsSigned(rtype) && !AST::IsSigned(ltype))
                            builder.emit_cast(right_reg, CAST_UINT_UINT, rsize, outSize);
                        if(!AST::IsSigned(rtype) && AST::IsSigned(ltype))
                            builder.emit_cast(right_reg, CAST_UINT_SINT, rsize, outSize);
                        if(AST::IsSigned(rtype) && !AST::IsSigned(ltype))
                            builder.emit_cast(right_reg, CAST_SINT_UINT, rsize, outSize);
                        if(AST::IsSigned(rtype) && AST::IsSigned(ltype))
                            builder.emit_cast(right_reg, CAST_SINT_SINT, rsize, outSize);
                    }
                    if(lsize < rsize) {
                        outType = rtype;
                    } else {
                        outType = ltype;
                    }
                    if(AST::IsSigned(ltype) || AST::IsSigned(rtype)) {
                        if(!AST::IsSigned(outType))
                            outType._infoIndex0 += 4;
                        Assert(AST::IsSigned(outType));
                    }
                } else if ((AST::IsDecimal(ltype) || AST::IsInteger(ltype)) && (AST::IsDecimal(rtype) || AST::IsInteger(rtype))){
                    if(!is_arithmetic && !is_comparison) {
                        // We may allow bitwise operation like 'num & 0x8000_0000' to check if a decimal is decimal or not.
                        // Or 'num & 0x7F_FFFF, num & > 23' to extract the exponent and mantissa.
                        ERR_SECTION(
                            ERR_HEAD2(expression->location)
                            ERR_MSG("Floats are limited to arithmetic and comparison operations.")
                            ERR_LINE2(expression->location, "here")
                        )
                        return SIGNAL_FAILURE;
                    }
                    
                    /*
                    integers should be converted to floats
                    floats should be converted to the biggest float
                    */

                    InstructionType bytecodeOp = ASTOpToBytecode(operationType,true);
                    u8 lsize = info.ast->getTypeSize(ltype);
                    u8 rsize = info.ast->getTypeSize(rtype);

                    u8 finalSize = 0;
                    if(AST::IsDecimal(ltype)) {
                        finalSize = lsize;
                        outType = ltype;
                    }
                    if(AST::IsDecimal(rtype)) {
                        if(rsize > finalSize) {
                            finalSize = rsize;
                            outType = rtype;
                        }
                    }

                    if(AST::IsInteger(rtype)){
                        if(AST::IsSigned(rtype)){
                            builder.emit_cast(right_reg, CAST_SINT_FLOAT, rsize, finalSize);
                        } else {
                            builder.emit_cast(right_reg, CAST_UINT_FLOAT, rsize, finalSize);
                        }
                    } else if(rsize != finalSize) {
                        builder.emit_cast(right_reg, CAST_FLOAT_FLOAT, rsize, finalSize);
                    }
                    if(AST::IsInteger(ltype)){
                        if(AST::IsSigned(ltype)){
                            builder.emit_cast(left_reg, CAST_SINT_FLOAT, lsize, finalSize);
                        } else {
                            builder.emit_cast(left_reg, CAST_UINT_FLOAT, lsize, finalSize);
                        }
                    } else if(lsize != finalSize) {
                        builder.emit_cast(left_reg, CAST_FLOAT_FLOAT, lsize, finalSize);
                    }
                }
                
                if(is_comparison || is_equality) {
                    outType = AST_BOOL;
                }
                
                outSize = ast->getTypeSize(outType);

                /*#########################
                        Code generation
                ###########################*/
                
                switch(operationType) {
                case AST_ADD:           builder.emit_add(    left_reg, right_reg, is_float, outSize); break;
                case AST_SUB:           builder.emit_sub(    left_reg, right_reg, is_float, outSize); break;
                case AST_MUL:           builder.emit_mul(    left_reg, right_reg, is_float, outSize, is_signed); break;
                case AST_DIV:           builder.emit_div(    left_reg, right_reg, is_float, outSize, is_signed); break;
                case AST_MODULUS:       builder.emit_mod(    left_reg, right_reg, is_float, outSize, is_signed); break;
                case AST_EQUAL:         builder.emit_eq(     left_reg, right_reg, is_float); break;
                case AST_NOT_EQUAL:     builder.emit_neq(    left_reg, right_reg, is_float); break;
                case AST_LESS:          builder.emit_lt(     left_reg, right_reg, is_float, is_signed); break;
                case AST_LESS_EQUAL:    builder.emit_lte(    left_reg, right_reg, is_float, is_signed); break;
                case AST_GREATER:       builder.emit_gt(     left_reg, right_reg, is_float, is_signed); break;
                case AST_GREATER_EQUAL: builder.emit_gte(    left_reg, right_reg, is_float, is_signed); break;
                case AST_AND:           builder.emit_land(   left_reg, right_reg); break;
                case AST_OR:            builder.emit_lor(    left_reg, right_reg); break;
                case AST_BAND:          builder.emit_band(   left_reg, right_reg); break;
                case AST_BOR:           builder.emit_bor(    left_reg, right_reg); break;
                case AST_BXOR:          builder.emit_bxor(   left_reg, right_reg); break;
                case AST_BLSHIFT:       builder.emit_blshift(left_reg, right_reg); break;
                case AST_BRSHIFT:       builder.emit_blshift(left_reg, right_reg); break;
                default: Assert(("Operation not implemented",false));
                }
                
                if(expression->typeId==AST_ASSIGN){
                    TypeId assignType={};
                    SignalIO result = generateReference(expression->left,&assignType,idScope);
                    if(result!=SIGNAL_SUCCESS)
                        return SIGNAL_FAILURE;
                    Assert(assignType == ltype);
                    Assert(BC_REG_B != left_reg);
                    builder.emit_pop(BC_REG_B);
                    int assignedSize = info.ast->getTypeSize(ltype);
                    builder.emit_mov_mr(BC_REG_B, left_reg, assignedSize);
                }
                
                builder.emit_push(left_reg);
                
                if(outTypeIds)
                    outTypeIds->add(outType);
            }
        }
    }
    for(auto& typ : *outTypeIds){
        TypeInfo* ti = info.ast->getTypeInfo(typ.baseType());
        if(ti){
            Assert(("Leaking virtual type!",ti->id == typ.baseType()));
        }
    }
    // To avoid ensuring non virtual type everywhere all the entry point where virtual type can enter
    // the expression system is checked. The rest of the system won't have to check it.
    // An entry point has been missed if the assert above fire.
    return SIGNAL_SUCCESS;
}

SignalIO GenContext::generateFunction(ASTFunction* function, ASTStruct* astStruct){
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue2);

    _GLOG(FUNC_ENTER_IF(function->linkConvention == LinkConventions::NONE)) // no need to log
    
    MAKE_NODE_SCOPE(function);

    if(!hasForeignErrors()) {
        Assert(!function->body == !function->needsBody());
    }
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
                ERR_HEAD2(function->location)
                ERR_MSG("'"<<function->name << "' was null (compiler bug).")
                ERR_LINE2(function->location, "bad")
            )
            // if (function->tokenRange.firstToken.str) {
            //     ERRTOKENS(function->location)
            // }
            // 
            return SIGNAL_FAILURE;
        }
        if (identifier->type != Identifier::FUNCTION) {
            // I have a feeling some error has been printed if we end up here.
            // Double check that though.
            return SIGNAL_FAILURE;
        }
        // ASTFunction* tempFunc = info.ast->getFunction(fid);
        // // Assert(tempFunc==function);
        // if(tempFunc!=function){
        //     // error already printed?
        //     return SIGNAL_FAILURE;
        // }
    }
    if(function->linkConvention != LinkConventions::NONE){
        if(function->polyArgs.size()!=0 || (astStruct && astStruct->polyArgs.size()!=0)){
            ERR_SECTION(
                ERR_HEAD2(function->location)
                ERR_MSG("Imported/native functions cannot be polymorphic.")
                ERR_LINE2(function->location, "remove polymorphism")
            )
            return SIGNAL_FAILURE;
        }
        if(identifier->funcOverloads.overloads.size() != 1){
            // TODO: This error prints multiple times for each duplicate definition of native function.
            //   With this, you know the location of the colliding functions but the error message is printed multiple times.
            //  It is better if the message is shown once and then show all locations at once.
            ERR_SECTION(
                ERR_HEAD2(function->location)
                ERR_MSG("Imported/native functions can only have one overload.")
                ERR_LINE2(function->location, "bad")
            )
            return SIGNAL_FAILURE;
        }
        // if(!info.hasForeignErrors()){
        //     Assert(function->_impls.size()==1);
        // }
        if(function->linkConvention == NATIVE ||
            info.compiler->options->target == TARGET_BYTECODE
        ){
            // #ifdef gone
            // Assert(info.compileInfo->nativeRegistry);
            auto nativeRegistry = NativeRegistry::GetGlobal();
            auto nativeFunction = nativeRegistry->findFunction(function->name);
            // auto nativeFunction = info.compileInfo->nativeRegistry->findFunction(function->name);
            if(nativeFunction){
                if(function->_impls.size()>0){
                    // impls may be zero if type checker found multiple definitions for native functions.
                    // we don't want to crash here when that happens and we don't need to throw an error
                    // because type checker already did.
                    // Assert(false);
                    function->_impls.last()->tinycode_id = nativeFunction->jumpAddress;
                }
            } else {
                if(function->linkConvention != NATIVE){
                    if(function->linked_library.size() != 0) {
                        bool found = false;
                        for(int i=0;i<imp->libraries.size();i++) {
                            auto& lib = imp->libraries[i];
                            if(lib.named_as == function->linked_library) {
                                found = true;
                                break;
                            }
                        }
                        if(found) {

                        } else {
                            ERR_SECTION(
                                ERR_HEAD2(function->location)
                                ERR_MSG_COLORED("The imported function refers to the library '"<<log::LIME<<function->linked_library<<log::NO_COLOR<<"' but it does not exist in the file. (loaded/linked libraries are scoped per file).")
                                ERR_LINE2(function->location, "unknown '"+function->linked_library+"' in @import");
                                ERR_EXAMPLE(1,"#load \"path_to_library.dll\" as your_lib\n"
                                            "fn @import(your_lib) func_in_library();")
                            )
                        }
                    } else {
                        ERR_SECTION(
                            ERR_HEAD2(function->location)
                            ERR_MSG("Imported functions must come from a library which is specified in the annotation.")
                            ERR_LINE2(function->location, "missing library");
                            ERR_EXAMPLE(1,"#load \"path_to_library.dll\" as your_lib\n"
                                            "fn @import(your_lib) func_in_library();")
                        )
                    }
                } else {
                    ERR_SECTION(
                        ERR_HEAD2(function->location)
                        ERR_MSG("'"<<function->name<<"' is not a native function. None of the "<<nativeRegistry->nativeFunctions.size()<<" native functions matched.")
                        ERR_LINE2(function->location, "bad")
                    )
                }
                return SIGNAL_FAILURE;
            }
            // #endif
            _GLOG(log::out << "Native function "<<function->name<<"\n";)
        } else {
            // exports not handled
            Assert(IS_IMPORT(function->linkConvention));
            // if(function->_impls.size()>0){
            //     function->_impls.last()->address = FuncImpl::ADDRESS_EXTERNAL;
            // }
            _GLOG(log::out << "External function "<<function->name<<"\n";)
        }
        return SIGNAL_SUCCESS;
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
        Assert(("func has already been generated!",funcImpl->tinycode_id == 0));
        // This happens with functions inside of polymorphic function.
        // if(function->callConvention != BETCALL) {
        //     ERR_SECTION(
        //         ERR_HEAD2(function->location)
        //         ERR_MSG("The convention '" << function->callConvention << "' has not been implemented for user functions. The normal convention (betcall) is the only one that works.")
        //         ERR_LINE2(function->location,"use betcall");
        //     )
        //     continue;
        // }
        // if(function->callConvention == UNIXCALL) {
        //     ERR_SECTION(
        //         ERR_HEAD2(function->location)
        //         ERR_MSG("Unix calling convention is not implemented for user defined functions. You can however declare functions with unix convention and call them if they come from an external library.")
        //         ERR_LINE2(function->location,"unixcall not implemented")
        //     )
        //     continue;
        // }
        // Assert(("Unix calling convention not implemented for user defined functions.",false));

        // Assert(!info.compileInfo->options->useDebugInformation);
        // IMPORTANT: How to deal with overloading and polymorphism?
        // add #0 at the end of the function name?
        // DebugInformation::Function* dfun = &di->functions.last();
        // dfun->name = "main";
        // dfun->fileIndex = di->files.size();
        // di->files.add(info.compileInfo->options->initialSourceFile.text);
        DebugInformation* di = info.code->debugInformation;
        Assert(di);
        
        info.debugFunctionIndex = di->functions.size();
        DebugInformation::Function* dfun = di->addFunction(funcImpl->astFunction->name);
        if(function->body->statements.size()>0) {
            // nocheckin, no tokenrange
            auto imp = compiler->lexer.getImport_unsafe(function->body->statements[0]->location);
            dfun->fileIndex = di->addOrGetFile(imp->path);
        } else {
            dfun->fileIndex = di->addOrGetFile(info.compiler->options->source_file);
        }
        dfun->funcImpl = funcImpl;
        dfun->funcAst = function;
        // dfun->tokenStream = function->body->tokenRange.tokenStream(); // nocheckin, replace with import
        info.currentScopeDepth = -1; // incremented to 0 in GenerateBody
        // dfun->scopeId = function->body->scopeId;

        _GLOG(log::out << "Function " << function->name << "\n";)

        info.currentPolyVersion = funcImpl->polyVersion;

        // TODO: Export function symbol if annotated with @export
        //  Perhaps force functions named main to be annotated with @export in parser
        //  instead of handling the special case for main here?
        

        auto prevFunc = info.currentFunction;
        auto prevFuncImpl = info.currentFuncImpl;
        auto prevScopeId = info.currentScopeId;
        info.currentFunction = function;
        info.currentFuncImpl = funcImpl;
        info.currentScopeId = function->scopeId;
        defer { info.currentFunction = prevFunc;
            info.currentFuncImpl = prevFuncImpl;
            info.currentScopeId = prevScopeId; };

        // int functionStackMoment = builder.saveStackMoment();
        // -8 since the start of the frame is 0 and after the program counter is -8
        // info.virtualStackPointer = GenContext::VIRTUAL_STACK_START;
        
        if(funcImpl->astFunction->name == "main") {
            switch(info.compiler->options->target) { // this is really cheeky
            case TARGET_WINDOWS_x64:
                function->callConvention = STDCALL;
                break;
            case TARGET_UNIX_x64:
                function->callConvention = UNIXCALL;
                break;
            case TARGET_BYTECODE:
                function->callConvention = BETCALL;
                break;
            default: Assert(false);
            }
        }

        auto tiny = code->createTiny(function->callConvention);
        out_codes->add(tiny);
        tinycode = tiny;
        builder.init(code, tiny);
        tiny->name = function->name; // what about poly types?
        funcImpl->tinycode_id = tiny->index + 1;
        if(tiny->name == "main") {
            code->index_of_main = tiny->index;
        }

        if(funcImpl->astFunction->name == "main") {
            bool yes = info.code->addExportedFunction(funcImpl->astFunction->name, tiny->index);
            if(!yes) {
                ERR_SECTION(
                    ERR_HEAD2(function->location)
                    ERR_MSG("You cannot have two functions named main since it is the entry point. Either main is overloaded or polymorphic which isn't allowed.\n")
                    ERR_LINE2(function->location,"second main (or third...)")
                    // TODO: Show all functions named main
                )
            }
        } else {
            // bool yes = info.code->addExportedSymbol(funcImpl->name, funcImpl->address);
            // if(!yes) {
            //     ERR_SECTION(
            //         ERR_HEAD2(function->location)
            //         ERR_MSG("Exporting two functions with the same name is not possible.\n")
            //         ERR_LINE2(function->location,"second function, same name")
            //         // TODO: Show all functions named main
            //     )
            // }
        }

        // reset frame offset at beginning of function
        currentFrameOffset = 0;

        // dfun->funcStart = info.code->length(); // nocheckin
        // dfun->bc_start = info.code->length();
        auto tokinfo = compiler->lexer.getTokenSource_unsafe(function->location);
        dfun->entry_line = tokinfo->line;

        #define DFUN_ADD_VAR(NAME, OFFSET, TYPE) dfun->localVariables.add({});\
                            dfun->localVariables.last().name = NAME;\
                            dfun->localVariables.last().frameOffset = OFFSET;\
                            dfun->localVariables.last().typeId = TYPE;

        int allocated_stack_space = 0;

        // Why to we calculate var offsets here? Can't we do it in 
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
                            ERR_HEAD2(arg.location)
                            ERR_MSG(arg.name << " is already defined.")
                            ERR_LINE2(arg.location,"cannot use again")
                        )
                    }
                    // var->versions_typeId[info.currentPolyVersion] = argImpl.typeId;
                    // TypeInfo *typeInfo = info.ast->getTypeInfo(argImpl.typeId.baseType());
                    // var->globalData = false;
                    varinfo->versions_dataOffset[info.currentPolyVersion] = argImpl.offset;
                    // _GLOG(log::out << " " <<"["<<varinfo->versions_dataOffset[info.currentPolyVersion]<<"] "<< arg.name << ": " << info.ast->typeToString(argImpl.typeId) << "\n";)
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
            // builder.emit_push(BC_REG_BP);
            // builder.emit_mov_rr(BC_REG_BP, BC_REG_SP);
            if (funcImpl->returnTypes.size() != 0) {
                // _GLOG(log::out << "space for " << funcImpl->returnTypes.size() << " return value(s) (struct may cause multiple push)\n");
                // info.code->addDebugText("ZERO init return values\n");
                
                // We don't need to zero initialize return value.
                // You cannot return without specifying what to return.
                // If we have a feature where return values can be set like
                // local variables then we may need to rethink things.
            // #ifndef DISABLE_ZERO_INITIALIZATION
                // builder.emit_alloc_local(BC_REG_B, funcImpl->returnSize);
                // genMemzero(BC_REG_B, BC_REG_C, funcImpl->returnSize);
            // #else
                builder.emit_alloc_local(BC_REG_INVALID, funcImpl->returnSize);
            // #endif

                allocated_stack_space += funcImpl->returnSize;
                info.currentFrameOffset -= funcImpl->returnSize; // TODO: size can be uneven like 13. FIX IN EVALUATETYPES

                _GLOG(log::out << "Return values:\n";)
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
        } break;
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
                            ERR_HEAD2(arg.location)
                            ERR_MSG(arg.name << " is already defined.")
                            ERR_LINE2(arg.location,"cannot use again")
                        )
                    }
                    // var->versions_typeId[info.currentPolyVersion] = argImpl.typeId;
                    u8 size = info.ast->getTypeSize(varinfo->versions_typeId[info.currentPolyVersion]);
                    if(size>8) {
                        ERR_SECTION(
                            ERR_HEAD2(arg.location)
                            ERR_MSG(ToString(function->callConvention) << " does not allow arguments larger than 8 bytes. Pass by pointer if arguments are larger.")
                            ERR_LINE2(arg.location, "bad")
                        )
                    }
                    // TypeInfo *typeInfo = info.ast->getTypeInfo(argImpl.typeId.baseType());
                    // stdcall should put first 4 args in registers but the function will put
                    // the arguments onto the stack automatically so in the end 8*i will work fine.
                    varinfo->versions_dataOffset[info.currentPolyVersion] = 8 * i;
                    // varinfo->versions_dataOffset[info.currentPolyVersion] = GenContext::FRAME_SIZE + 8 * i;
                    _GLOG(log::out << " " <<"["<<varinfo->versions_dataOffset[info.currentPolyVersion]<<"] "<< arg.name << ": " << info.ast->typeToString(argImpl.typeId) << "\n";)
                    // DFUN_ADD_VAR(arg.name, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion])
                }
                _GLOG(log::out << "\n";)
            }
            // TODO: Member variables for stdcall.
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
            // builder.emit_push(BC_REG_BP);
            // builder.emit_mov_rr(BC_REG_BP, BC_REG_SP);

            if (funcImpl->returnTypes.size() > 1) {
                ERR_SECTION(
                    ERR_HEAD2(function->location)
                    ERR_MSG(ToString(function->callConvention) << " only allows one return value. BETCALL (the default calling convention in this language) supports multiple return values.")
                    ERR_LINE2(function->location, "bad")
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

            // TODO: WE NEED AN INSTRUCTION TO MOVE RCX,RDX... TO THE ARGUMENT STACK AREA
            // const BCRegister normal_regs[4]{
            //     BC_REG_RCX,
            //     BC_REG_RDX,
            //     BC_REG_R8,
            //     BC_REG_R9,
            // };
            // const BCRegister float_regs[4] {
            //     BC_REG_XMM0,
            //     BC_REG_XMM1,
            //     BC_REG_XMM2,
            //     BC_REG_XMM3,
            // };
            // auto& argTypes = funcImpl->argumentTypes;
            // for(int i=argTypes.size()-1;i>=0;i--) {
            //     auto argType = argTypes[i].typeId;

            //     u8 size = info.ast->getTypeSize(argType);
            //     if(AST::IsDecimal(argType)) {
            //         Assert(false);
            //         builder.emit_mov_mr_disp(BC_REG_BP, float_regs[i], size, GenContext::FRAME_SIZE + i * 8);
            //     } else {
            //         builder.emit_mov_mr_disp(BC_REG_BP, normal_regs[i], size, GenContext::FRAME_SIZE + i * 8);
            //     }
            // }

            // TODO: MAKE SURE SP IS ALIGNED TO 8 BYTES
            // SHOULD stackAlignment, virtualStackPointer be reset and then restored?
            // ALSO DO IT BEFORE CALLING FUNCTION (FNCALL)
            //
            // TODO: GenerateBody may be called multiple times for the same body with
            //  polymorphic functions. This is a problem since GenerateBody generates functions.
            //  Generating them multiple times for each function is bad.
            // NOTE: virtualTypes from this function may be accessed from a function within it's body.
            //  This could be bad.
        } break;
        case UNIXCALL: {
            Assert(false);
            #ifdef gone
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
                            ERR_HEAD2(arg.location)
                            ERR_MSG(arg.name << " is already defined.")
                            ERR_LINE2(arg.location,"cannot use again")
                        )
                    }
                    // var->versions_typeId[info.currentPolyVersion] = argImpl.typeId;
                    u8 size = info.ast->getTypeSize(varinfo->versions_typeId[info.currentPolyVersion]);
                    if(size>8) {
                        ERR_SECTION(
                            ERR_HEAD2(arg.location)
                            ERR_MSG(ToString(function->callConvention) << " does not allow arguments larger than 8 bytes. Pass by pointer if arguments are larger.")
                            ERR_LINE2(arg.location, "bad")
                        )
                    }
                    // TypeInfo *typeInfo = info.ast->getTypeInfo(argImpl.typeId.baseType());
                    // stdcall should put first 4 args in registers but the function will put
                    // the arguments onto the stack automatically so in the end 8*i will work fine.
                    varinfo->versions_dataOffset[info.currentPolyVersion] = 8 * i;
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
            // builder.emit_push(BC_REG_BP);
            // builder.emit_mov_rr(BC_REG_BP, BC_REG_SP);

            if (funcImpl->returnTypes.size() > 1) {
                ERR_SECTION(
                    ERR_HEAD2(function->location)
                    ERR_MSG(ToString(function->callConvention) << " only allows one return value. BETCALL (the default calling convention in this language) supports multiple return values.")
                    ERR_LINE2(function->location, "bad")
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
            const BCRegister normal_regs[6]{
                BC_REG_RDI,
                BC_REG_RSI,
                BC_REG_RDX,
                BC_REG_RCX,
                BC_REG_R8,
                BC_REG_R9,
            };
            const BCRegister float_regs[8] {
                BC_REG_XMM0,
                BC_REG_XMM1,
                BC_REG_XMM2,
                BC_REG_XMM3,
                // BC_REG_XMM4,
                // BC_REG_XMM5,
                // BC_REG_XMM6,
                // BC_REG_XMM7,
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
                    Assert(false); // what size should the floats be?
                    // I am not sure if doubles work so we should test and not assume.
                    // This assert will prevent incorrect assumptions from causing trouble.
                    builder.emit_mov_mr_disp(BC_REG_BP,float_regs[--used_floats], size, GenContext::FRAME_SIZE + i * 8);
                } else {
                    builder.emit_mov_mr_disp(BC_REG_BP,normal_regs[--used_normals], size, GenContext::FRAME_SIZE + i * 8);
                }
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
            #endif
        } break;
        default: {
            Assert(false);
        }
        }
        
        // dfun->codeStart = info.code->length();

        if(funcImpl->astFunction->name == "main") {
            generatePreload();
        }
        
        SignalIO result = generateBody(function->body);

        // dfun->codeEnd = info.code->length();

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
                bool foundReturn = function->body->statements.size()>0 && 
                    function->body->statements.get(function->body->statements.size()-1) ->type == ASTStatement::RETURN;
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
                            ERR_HEAD2(function->location)
                            ERR_MSG("Missing return statement in '" << function->name << "'.")
                            ERR_LINE2(function->location,"put a return in the body")
                        )
                    }
                }
            }
            if(builder.get_last_opcode() != BC_RET) {
                // add return with no return values if it doesn't exist
                // this is only fine if the function doesn't return values
                // builder.emit_pop(BC_REG_BP);
                if(allocated_stack_space != 0)
                    builder.emit_free_local(allocated_stack_space);
                // builder.restoreStackMoment(functionStackMoment);
                builder.emit_ret();
            } else {
                // nocheckin, usually we would restore virtual stack pointer but maybe we don't need to with the rewrite
                // builder.restoreStackMoment(VIRTUAL_STACK_START, false, true);
                // Assert(builder.getStackPointer() == functionStackMoment);
            }
        } break;
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
            Assert(funcImpl->returnTypes.size() <= 1);
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
                            ERR_HEAD2(function->location)
                            ERR_MSG("Missing return statement in '" << function->name << "'.")
                            ERR_LINE2(function->location,"put a return in the body")
                        )
                    }
                }
            }
            if(builder.get_last_opcode() != BC_RET) {
                // add return with no return values if it doesn't exist
                // this is only fine if the function doesn't return values
                // builder.emit_bxor(BC_REG_A, BC_REG_A);
                // builder.emit_pop(BC_REG_BP);
                builder.emit_ret();
            } else {
                // builder.restoreStackMoment(GenContext::VIRTUAL_STACK_START, false, true);
            }
        } break;
        case UNIXCALL: {
            Assert(false);
            #ifdef gone
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
                            ERR_HEAD2(function->location)
                            ERR_MSG("Missing return statement in '" << function->name << "'.")
                            ERR_LINE2(function->location,"put a return in the body")
                        )
                    }
                }
            }
            if(builder.get_last_opcode() != BC_RET) {
                // add return with no return values if it doesn't exist
                // this is only fine if the function doesn't return values
                // builder.emit_bxor(BC_REG_A, BC_REG_A);
                // builder.emit_pop(BC_REG_BP);
                builder.emit_ret();
            } else {
                builder.restoreStackMoment(GenContext::VIRTUAL_STACK_START, false, true);
            }
            Assert(builder.getStackPointer() == 0);
            #endif
        } break;
        default: {
            Assert(false);
        }
        }
        
        // TODO: Assert that we haven't allocated more stack space than we freed!
        // Assert(builder.getStackPointer() == 0);
        // Assert(info.currentFrameOffset == 0);

        // dfun->funcEnd = info.code->length();
        // dfun->bc_end = info.code->length();
        // needs to be done after frame pop
        // builder.restoreStackMoment(functionStackMoment, false, true);
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generateFunctions(ASTScope* body){
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue2);
    _GLOG(FUNC_ENTER)
    Assert(body || info.hasForeignErrors());
    if(!body) return SIGNAL_FAILURE;
    // MAKE_NODE_SCOPE(body); // we don't generate bytecode here so no need for this

    ScopeId savedScope = info.currentScopeId;
    defer { info.currentScopeId = savedScope; };

    info.currentScopeId = body->scopeId;

    for(auto it : body->namespaces) {
        generateFunctions(it);
    }
    for(auto it : body->functions) {
        if(it->body) {
            // External/native does not have body.
            // Not calling function reducess log messages
            generateFunctions(it->body);
        }
        generateFunction(it);
    }
    for(auto it : body->structs) {
        for (auto function : it->functions) {
            Assert(function->body);
            generateFunctions(function->body);
            generateFunction(function, it);
        }
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generateBody(ASTScope *body) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue2);
    PROFILE_SCOPE
    _GLOG(FUNC_ENTER)
    Assert(body||info.hasForeignErrors());
    if(!body) return SIGNAL_FAILURE;

    MAKE_NODE_SCOPE(body); // no need, the scope itself doesn't generate code

    Bytecode::Dump debugDump{};
    if(body->flags & ASTNode::DUMP_ASM) {
        debugDump.dumpAsm = true;
    }
    if(body->flags & ASTNode::DUMP_BC) {
        debugDump.dumpBytecode = true;
    }
    // debugDump.bc_startIndex = info.code->length();
    
    bool codeWasDisabled = info.disableCodeGeneration;
    bool errorsWasIgnored = info.ignoreErrors;
    ScopeId savedScope = info.currentScopeId;
    ScopeInfo* body_scope = info.ast->getScope(body->scopeId);
    // body_scope->bc_start = info.code->length();
    info.currentScopeDepth++;

    info.currentScopeId = body->scopeId;

    int lastOffset = info.currentFrameOffset;
    // int savedMoment = builder.saveStackMoment();

    defer {
        info.disableCodeGeneration = codeWasDisabled;

        if (lastOffset != info.currentFrameOffset) {
            _GLOG(log::out << "fix sp when exiting body\n";)
            builder.emit_free_local(lastOffset - info.currentFrameOffset);
            // builder.restoreStackMoment(savedMoment); // -8 to not include BC_REG_BP
            // info.code->addDebugText("fix sp when exiting body\n");
            
            info.currentFrameOffset = lastOffset;
        } else {
            // builder.restoreStackMoment(savedMoment);
        }

        info.currentScopeDepth--;
        // body_scope->bc_end = info.code->length();
        info.currentScopeId = savedScope; 
        info.ignoreErrors = errorsWasIgnored;
        if(debugDump.dumpAsm || debugDump.dumpBytecode) {
            // compiler->lexer.getImport_unsafe(body->loc);
            Assert(false); // nocheckin, no token range
            // debugDump.description = body->tokenRange.tokenStream()->streamName + ":"+std::to_string(body->tokenRange.firstToken.line);
            // debugDump.description = TrimDir(body->tokenRange.tokenStream()->streamName) + ":"+std::to_string(body->tokenRange.firstToken.line);
            // debugDump.bc_endIndex = info.code->length();
            Assert(debugDump.bc_startIndex <= debugDump.bc_endIndex);
            info.code->debugDumps.add(debugDump);
        }
    };

    // for(auto it : body->namespaces) {
    //     SignalIO result = generateBody(it);
    // }

    for (auto statement : body->statements) {
        MAKE_NODE_SCOPE(statement);

        auto& fun = info.code->debugInformation->functions[info.debugFunctionIndex];
        // fun.addLine(statement->tokenRange.firstToken.line, info.code->length(), statement->tokenRange.firstToken.tokenIndex); // nocheckin, token is gone
        
        info.disableCodeGeneration = codeWasDisabled;
        info.ignoreErrors = errorsWasIgnored;
        
        int prev_stackAlignment_size = 0;
        int prev_virtualStackPointer = 0;
        int prev_currentFrameOffset = 0;
        if(statement->isNoCode()) {
            info.disableCodeGeneration = true;
            info.ignoreErrors = true;
            
            Assert(false); // nocheckin, broken
            // prev_stackAlignment_size = info.stackAlignment.size();
            // prev_virtualStackPointer = info.virtualStackPointer;
            // prev_currentFrameOffset = info.currentFrameOffset;
        } else {
            std::string line = compiler->lexer.getline(statement->location);
            auto tokinfo = compiler->lexer.getTokenSource_unsafe(statement->location);
            builder.push_line(tokinfo->line, line);
        }

        defer {
            if(statement->isNoCode()) {
                Assert(false); // nocheckin, broken
                // Assert(prev_stackAlignment_size <= info.stackAlignment.size()); // We lost information, the no code remove stack elements which we can't get back. We would need to save the elements not just the size of stack alignment
                // info.stackAlignment.resize(prev_stackAlignment_size);
                // info.virtualStackPointer = prev_virtualStackPointer;
                // info.currentFrameOffset = prev_currentFrameOffset;
            }
        };

        if (statement->type == ASTStatement::DECLARATION) {
            _GLOG(SCOPE_LOG("ASSIGN"))
            
            auto& typesFromExpr = statement->versions_expressionTypes[info.currentPolyVersion];
            if(statement->firstExpression && typesFromExpr.size() < statement->varnames.size()) {
                if(!info.hasForeignErrors()){
                    // char msg[100];
                    ERR_SECTION(
                        ERR_HEAD2(statement->location)
                        ERR_MSG("To many variables.")
                        // sprintf(msg,"%d variables",(int)statement->varnames.size());
                        ERR_LINE2(statement->location, statement->varnames.size() << " variables");
                        // sprintf(msg,"%d return values",(int)typesFromExpr.size());
                        ERR_LINE2(statement->firstExpression->location, typesFromExpr.size() << " return values");
                    )
                }
                continue;
            }

            // This is not an error we want. You should be able to do this.
            // if(statement->globalDeclaration && statement->firstExpression && info.currentScopeId != info.ast->globalScopeId) {
            //     ERR_SECTION(
            //         ERR_HEAD2(statement->location)
            //         ERR_MSG("Assigning a value to a global variable inside a local scope is not allowed. Either move the variable to the global scope or separate the declaration and assignment with expression.")
            //         ERR_LINE2(statement->location,"bad")
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
                //     ERRTYPE(statement->location, declaredType, typeFromExpr, "(assign)\n";
                //         ERR_LINE2(statement->location,"bad");
                //     )
                //     continue;
                // }

                // i32 rightSize = info.ast->getTypeSize(typeFromExpr);
                // i32 leftSize = info.ast->getTypeSize(var->typeId);
                // i32 asize = info.ast->getTypeAlignedSize(var->typeId);

                int alignment = 0;
                if (varname.arrayLength>0){
                    Assert(("global arrays not implemented",!statement->globalDeclaration));
                    // TODO: Fix arrays with static data
                    if(statement->firstExpression) {
                        ERR_SECTION(
                            ERR_HEAD2(statement->firstExpression->location)
                            ERR_MSG("An expression is not allowed when declaring an array on the stack. The array is zero-initialized by default.")
                            ERR_LINE2(statement->firstExpression->location, "bad")
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
                            ERR_HEAD2(statement->location)
                            ERR_MSG((int)(pow(2,16)/2-1) << " is the maximum size of arrays on the stack. "<<(arraySize)<<" was used which exceeds that. The limit comes from the instruction BC_INCR which uses a signed 16-bit integer.")
                            ERR_LINE2(statement->location, elementSize << " * " << std::to_string(varname.arrayLength) << " = " << std::to_string(arraySize))
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
                    BCRegister reg_data = BC_REG_C;
                    builder.emit_alloc_local(reg_data, arraySize);
                    // builder.emit_incr(BC_REG_SP, -arraySize);
                    if(i == (int)statement->varnames.size()-1){
                        frameOffsetOfLastVarname = arrayFrameOffset;
                    }

                    #ifndef DISABLE_ZERO_INITIALIZATION
                    // builder.emit_li32(BC_REG_RDX,arraySize);
                    // builder.emit_({BC_MEMZERO, BC_REG_SP, BC_REG_RDX});
                    // builder.emit_mov_rr(BC_REG_D, BC_REG_SP);
                    // builder.emit_mov_rr(BC_REG_D, BC_REG_ARGS);
                    genMemzero(reg_data, BC_REG_B, arraySize);
                    #endif
                    TypeInfo* elementInfo = info.ast->getTypeInfo(elementType);
                    if(elementInfo->astStruct) {
                        // TODO: Annotation to disable this
                        // TODO: Create a loop with cmp, je, jmp instructions instead of
                        //  "unrolling" the loop like this. We generate a lot of instructions from this.
                        for(int j = 0;j<varname.arrayLength;j++) {
                            SignalIO result = generateDefaultValue(reg_data, elementSize * j, elementType);
                            // SignalIO result = generateDefaultValue(BC_REG_BP, arrayFrameOffset + elementSize * j, elementType);
                            if(result!=SIGNAL_SUCCESS)
                                return SIGNAL_FAILURE;
                        }
                    } else {
                        
                    }
                    // data type may be zero if it wasn't specified during initial assignment
                    // a = 9  <-  implicit / explicit  ->  a : i32 = 9
                    // int diff = asize - (-info.currentFrameOffset) % asize; // how much to fix alignment
                    // if (diff != asize) {
                    //     info.currentFrameOffset -= diff; // align
                    // }
                    // info.currentFrameOffset -= size;
                    // var->frameOffset = info.currentFrameOffset;

                    
                    SignalIO result = framePush(varinfo->versions_typeId[info.currentPolyVersion],&varinfo->versions_dataOffset[info.currentPolyVersion],false, varinfo->isGlobal());

                    // TODO: Don't hardcode this slice stuff, maybe I have to.
                    // push length
                    builder.emit_li32(BC_REG_D,varname.arrayLength);
                    builder.emit_push(BC_REG_D);

                    // push ptr
                    // builder.emit_li32(BC_REG_B, arrayFrameOffset);
                    // builder.emit_add(BC_REG_B, BC_REG_BP, false, 8);
                    builder.emit_get_local(BC_REG_B, arrayFrameOffset);
                    builder.emit_push(BC_REG_B);

                    generatePop(BC_REG_LOCALS, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);
                    // generatePop(BC_REG_BP, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);

                    auto& fun = info.code->debugInformation->functions[info.debugFunctionIndex];
                    fun.addVar(varname.name,
                        varinfo->versions_dataOffset[info.currentPolyVersion],
                        varinfo->versions_typeId[info.currentPolyVersion],
                        info.currentScopeDepth,
                        varname.identifier->scopeId);
                } else {
                    if(!varinfo->isGlobal()) {
                        // address of global variables is managed in type checker
                        SignalIO result = framePush(varinfo->versions_typeId[info.currentPolyVersion], &varinfo->versions_dataOffset[info.currentPolyVersion],
                            !statement->firstExpression, false);

                        auto& fun = info.code->debugInformation->functions[info.debugFunctionIndex];
                        fun.addVar(varname.name,
                            varinfo->versions_dataOffset[info.currentPolyVersion],
                            varinfo->versions_typeId[info.currentPolyVersion],
                            info.currentScopeDepth,
                            varname.identifier->scopeId);
                    }
                }
                _GLOG(log::out << "declare " << (varinfo->isGlobal()?"global ":"")<< varname.name << " at " << varinfo->versions_dataOffset[info.currentPolyVersion] << "\n";)
                    // NOTE: inconsistent
                    // char buf[100];
                    // int len = sprintf(buf," ^ was assigned %s",statement->name->c_str());
                    // info.code->addDebugText(buf,len);
                // }
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
                    SignalIO result = generateExpression(value, &rightTypes);
                    if (result != SIGNAL_SUCCESS) {
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

                    if(!performSafeCast(rightTypes[0], elementType)){
                        Assert(!info.hasForeignErrors());
                        continue;
                    }
                    switch(varinfo->type) {
                        case VariableInfo::GLOBAL: {
                            Assert(false); // broken with arrays
                            // builder.emit_dataptr(BC_REG_B, );
                            // info.addImm(varinfo->versions_dataOffset[info.currentPolyVersion]);
                            // GeneratePop(info, BC_REG_B, 0, varinfo->versions_typeId[info.currentPolyVersion]);
                            break; 
                        }
                        case VariableInfo::LOCAL: {
                            // builder.emit_li32(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion]);
                            // builder.emit_({BC_ADDI, BC_REG_BP, BC_REG_B, BC_REG_B});
                            generatePop(BC_REG_LOCALS, frameOffsetOfLastVarname + j * elementSize, elementType);
                            // generatePop(BC_REG_BP, frameOffsetOfLastVarname + j * elementSize, elementType);
                            break;
                        }
                        case VariableInfo::MEMBER: {
                            Assert(false); // broken with arrays, this should probably not be allowed
                            // Assert(info.currentFunction && info.currentFunction->parentStruct);
                            // // TODO: Verify that  you
                            // // NOTE: Is member variable/argument always at this offset with all calling conventions?
                            // builder.emit_({BC_MOV_MR_DISP32, BC_REG_BP, BC_REG_B, 8});
                            // info.addImm(GenContext::FRAME_SIZE);
                            
                            // // builder.emit_li32(BC_REG_A, varinfo->versions_dataOffset[info.currentPolyVersion]);
                            // // builder.emit_({BC_ADDI, BC_REG_B, BC_REG_A, BC_REG_B});
                            // GeneratePop(info, BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);
                            break;
                        }
                        default: {
                            Assert(false);
                        }
                    }
                }
            }
            if(statement->firstExpression){
                SignalIO result = generateExpression(statement->firstExpression, &rightTypes);
                if (result != SIGNAL_SUCCESS) {
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
                        generatePop(BC_REG_INVALID, 0, typeFromExpr);
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

                    if(!performSafeCast(typeFromExpr, varinfo->versions_typeId[info.currentPolyVersion])){
                        if(!info.hasForeignErrors()){
                            ERRTYPE1(statement->location, typeFromExpr, varinfo->versions_typeId[info.currentPolyVersion], "(assign)."
                                // ERR_LINE2(statement->location,"bad");
                            )
                        }
                        continue;
                    }
                    // Assert(!var->globalData || info.currentScopeId == info.ast->globalScopeId);
                    switch(varinfo->type) {
                        case VariableInfo::GLOBAL: {
                            builder.emit_dataptr(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion]);
                            generatePop(BC_REG_B, 0, varinfo->versions_typeId[info.currentPolyVersion]);
                        } break; 
                        case VariableInfo::LOCAL: {
                            // builder.emit_li32(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion]);
                            // builder.emit_({BC_ADDI, BC_REG_BP, BC_REG_B, BC_REG_B});
                            generatePop(BC_REG_LOCALS, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);
                            // generatePop(BC_REG_BP, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);
                        } break;
                        case VariableInfo::MEMBER: {
                            Assert(info.currentFunction && info.currentFunction->parentStruct);
                            // TODO: Verify that  you
                            // NOTE: Is member variable/argument always at this offset with all calling conventions?
                            auto type = varinfo->versions_typeId[info.currentPolyVersion];
                            builder.emit_get_param(BC_REG_B, 0, 8, AST::IsDecimal(type));
                            // builder.emit_mov_rm_disp(BC_REG_B, BC_REG_BP, 8, GenContext::FRAME_SIZE);
                            
                            // builder.emit_li32(BC_REG_A,varinfo->versions_dataOffset[info.currentPolyVersion]);
                            // builder.emit_({BC_ADDI, BC_REG_B, BC_REG_A, BC_REG_B});
                            generatePop(BC_REG_B, varinfo->versions_dataOffset[info.currentPolyVersion], varinfo->versions_typeId[info.currentPolyVersion]);
                        } break;
                        case VariableInfo::ARGUMENT: {
                            Assert(false);
                        } break;
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
            DynamicArray<TypeId> tmp_types{};
            SignalIO result = generateExpression(statement->firstExpression, &tmp_types);
            if(tmp_types.size()) dtype = tmp_types.last();
            if (result != SIGNAL_SUCCESS) {
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
                    ERR_HEAD2(statement->firstExpression->location)
                    ERR_MSG("The type '"<<info.ast->typeToString(dtype)<<"' in this expression was bigger than 8 bytes and can't be used in an if statement.")
                    ERR_LINE2(statement->firstExpression->location, size << " bytes is to much")
                )
                generatePop(BC_REG_INVALID, 0, dtype);
                continue;
            }

            BCRegister reg = BC_REG_A;

            builder.emit_pop(reg);
            int skipIfBodyIndex = builder.emit_jz(reg);

            result = generateBody(statement->firstBody);
            if (result != SIGNAL_SUCCESS)
                continue;

            int skipElseBodyIndex = 0;
            if (statement->secondBody) {
                skipElseBodyIndex = builder.emit_jmp();
            }

            // fix address for jump instruction
            builder.fix_jump_imm32_here(skipIfBodyIndex);

            if (statement->secondBody) {
                result = generateBody(statement->secondBody);
                if (result != SIGNAL_SUCCESS)
                    continue;

                builder.fix_jump_imm32_here(skipIfBodyIndex);
            }
        } else if (statement->type == ASTStatement::SWITCH) {
            _GLOG(SCOPE_LOG("SWITCH"))

            GenContext::LoopScope* loopScope = info.pushLoop();
            defer {
                _GLOG(log::out << "pop loop\n");
                bool yes = info.popLoop();
                if(!yes){
                    log::out << log::RED << "popLoop failed (bug in compiler)\n";
                }
            };

            _GLOG(log::out << "push loop\n");
            // loopScope->stackMoment = builder.saveStackMoment();
            loopScope->stackMoment = currentFrameOffset;

            TypeId exprType{};
            if(statement->versions_expressionTypes[info.currentPolyVersion].size()!=0)
                exprType = statement->versions_expressionTypes[info.currentPolyVersion][0];
            else
                { Assert(false); }
                // continue; // error message printed already?

            i32 switchValueOffset = 0; // frame offset
            framePush(exprType, &switchValueOffset,false, false);
            
            TypeId dtype = {};
            DynamicArray<TypeId> tmp_types{};
            SignalIO result = generateExpression(statement->firstExpression, &tmp_types);
            if(tmp_types.size()) dtype = tmp_types.last();
            if (result != SIGNAL_SUCCESS) {
                continue;
            }
            Assert(exprType == dtype);
            auto* typeInfo = info.ast->getTypeInfo(dtype);
            Assert(typeInfo);
            
            if(AST::IsInteger(dtype) || typeInfo->astEnum) {
                
            } else {
                ERR_SECTION(
                    ERR_HEAD2(statement->location)
                    ERR_MSG("You can only do switch on integers and enums.")
                    ERR_LINE2(statement->location, "bad")
                )
                continue;
            }
            u32 switchExprSize = info.ast->getTypeSize(dtype);
            if (!switchExprSize) {
                // TODO: Print error? or does generate expression do that since it gives us dtype?
                continue;
            }
            BCRegister switchValueReg = BC_REG_D;
            
            // TODO: Optimize instruction generation.
            
            Assert(switchExprSize <= 8);
            builder.emit_pop(switchValueReg);
            builder.emit_mov_mr_disp(BC_REG_LOCALS, switchValueReg, switchExprSize, switchValueOffset);
            // builder.emit_mov_mr_disp(BC_REG_BP, switchValueReg, switchExprSize, switchValueOffset);
            
            struct RelocData {
                int caseJumpAddress = 0;
                int caseBreakAddress = 0;
            };
            DynamicArray<RelocData> caseData{};
            caseData.resize(statement->switchCases.size());
            
            for(int nr=0;nr<(int)statement->switchCases.size();nr++) {
                auto it = &statement->switchCases[nr];
                caseData[nr] = {};
                
                TypeId dtype = {};
                u32 size = 0;
                BCRegister caseValueReg = BC_REG_INVALID;
                bool wasMember = false;
                if(typeInfo->astEnum && it->caseExpr->typeId == AST_ID) {
                    
                    int index = -1;
                    bool yes = typeInfo->astEnum->getMember(it->caseExpr->name, &index);
                    if(yes) {
                        wasMember = true;
                        dtype = typeInfo->id;
                        
                        size = info.ast->getTypeSize(dtype);
                        Assert(size == 4); // 64-bit enum not implemented (neither is 8, 16 ), you need to load 64-bit immediate instead of 32-bit
                        caseValueReg = BC_REG_A;
                        
                        builder.emit_li32(caseValueReg, typeInfo->astEnum->members[index].enumValue);
                    }
                }
                if(!wasMember) {
                    DynamicArray<TypeId> tmp_types{};
                    SignalIO result = generateExpression(it->caseExpr, &tmp_types);
                    if(tmp_types.size()) dtype = tmp_types.last();
                    if (result != SIGNAL_SUCCESS) {
                        continue;
                    }
                    size = info.ast->getTypeSize(dtype);
                    caseValueReg = BC_REG_A;
                    
                    builder.emit_pop(caseValueReg);
                }
                // TODO: Don't generate this instruction if switchValueReg is untouched.
                builder.emit_mov_rm_disp(switchValueReg, BC_REG_LOCALS, (u8)size, switchValueOffset);
                // builder.emit_mov_rm_disp(switchValueReg, BC_REG_BP, (u8)size, switchValueOffset);
                
                builder.emit_eq(caseValueReg, switchValueReg, false);
                caseData[nr].caseJumpAddress = builder.emit_jnz(caseValueReg);
            }
            if(statement->firstBody) {
                result = generateBody(statement->firstBody);
                // if (result != SIGNAL_SUCCESS)
                //     continue;
            }
            int noCaseJumpAddress = builder.emit_jmp();
            
            /* TODO: Cases without a body does not fall but perhaps they should.
            I can think of a scenario where implicit fall would be confusing.
            If you have a bunch of cases, none use @fall, and you comment out a body
            or remove it you may not realize that empty bodies will fall to next
            statements. You become irritated and put a nop statment. Alternative is 
            @fall after each case which isn't ideal either but it's consistent and it's
            fast to type. NO IMPLICIT FALLS FOR NOW. -Emarioo, 2024-01-10
            code.
                case 1:     <- implicit fall would be nifty
                case 2:
                    say_hi()
                case 4:
                    no()
            */
            // i32 address_prevFallJump = -1;
            
            auto& list = statement->switchCases;
            for(int nr=0;nr<(int)statement->switchCases.size();nr++) {
                auto it = &statement->switchCases[nr];
                builder.fix_jump_imm32_here(caseData[nr].caseJumpAddress);
               
                // if(address_prevFallJump != -1) {
                //     info.code->getIm(address_prevFallJump) = info.code->length();
                // }
                
                // TODO: break statements in body should jump to here
                result = generateBody(it->caseBody);
                if (result != SIGNAL_SUCCESS)
                    continue;
                
                if(!it->fall) {
                    // implicit break
                    caseData[nr].caseBreakAddress = builder.emit_jmp();
                } else {
                    caseData[nr].caseBreakAddress = 0;
                    // skip the jump so that we execute instructions on the next body
                    // NOTE: The last case doesn't need to jump out of the switch because
                    //  we just added the last instructions so we already are at the end.
                    //  Perhaps we won't be in the future?
                    
                    // builder.emit_({BC_JMP});
                    // address_prevFallJump = info.code->length();
                    // info.addImm(0);
                }
            }
            builder.fix_jump_imm32_here(noCaseJumpAddress);
                
            for(int nr=0;nr<(int)statement->switchCases.size();nr++) {
                if(caseData[nr].caseBreakAddress != 0)
                    builder.fix_jump_imm32_here(caseData[nr].caseBreakAddress);
            }
            
            for (auto ind : loopScope->resolveBreaks) {
                builder.fix_jump_imm32_here(ind);
            }
            
        } else if (statement->type == ASTStatement::WHILE) {
            _GLOG(SCOPE_LOG("WHILE"))

            GenContext::LoopScope* loopScope = info.pushLoop();
            defer {
                _GLOG(log::out << "pop loop\n");
                bool yes = info.popLoop();
                if(!yes){
                    log::out << log::RED << "popLoop failed (bug in compiler)\n";
                }
            };

            _GLOG(log::out << "push loop\n");
            loopScope->continueAddress = builder.get_pc();
            loopScope->stackMoment = currentFrameOffset;
            // loopScope->stackMoment = builder.saveStackMoment();

            SignalIO result{};
            if(statement->firstExpression) {
                TypeId dtype = {};
                DynamicArray<TypeId> tmp_types{};
                result = generateExpression(statement->firstExpression, &tmp_types);
                if(tmp_types.size()) dtype = tmp_types.last();
                if (result != SIGNAL_SUCCESS) {
                    // generate body anyway or not?
                    continue;
                }
                u32 size = info.ast->getTypeSize(dtype);
                BCRegister reg = BC_REG_A;

                builder.emit_pop(reg);
                int imm_offset = builder.emit_jz(reg);
                loopScope->resolveBreaks.add(imm_offset);
            } else {
                // infinite loop
            }

            result = generateBody(statement->firstBody);
            if (result != SIGNAL_SUCCESS)
                continue;

            builder.emit_jmp(loopScope->continueAddress);

            for (auto ind : loopScope->resolveBreaks) {
                builder.fix_jump_imm32_here(ind);
            }
            
            // TODO: Should this error exist? We need to check for returns too.
            // if(!statement->firstExpression && loopScope->resolveBreaks.size() == 0) {
            //     ERR_SECTION(
            //         ERR_HEAD2(statement->location)
            //         ERR_MSG("A true infinite loop without any break statements is not allowed. If this was intended, write 'while true' or put 'if false break' in the body of the loop.")
            //         ERR_LINE2(statement->location,"true infinite loop")
            //     )
            // }

            // pop loop happens in defer
            // _GLOG(log::out << "pop loop\n");
            // bool yes = info.popLoop();
            // if(!yes){
            //     log::out << log::RED << "popLoop failed (bug in compiler)\n";
            // }
        } else if (statement->type == ASTStatement::FOR) {
            _GLOG(SCOPE_LOG("FOR"))

            GenContext::LoopScope* loopScope = info.pushLoop();
            defer {
                _GLOG(log::out << "pop loop\n");
                bool yes = info.popLoop();
                if(!yes){
                    log::out << log::RED << "popLoop failed (bug in compiler)\n";
                }
            };

            int stackBeforeLoop = currentFrameOffset;
            // int stackBeforeLoop = builder.saveStackMoment(); // nocheckin, i have remove saveStackMoment, is that okay or does it break things?
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
                        ERR_HEAD2(statement->location)
                        ERR_MSG("Identifier '"<<(varnameNr.identifier ? (varnameIt.identifier ? StringBuilder{} << varnameIt.name : "") : (varnameIt.identifier ? (StringBuilder{} << varnameNr.name <<" and " << varnameIt.name) : StringBuilder{} << varnameNr.name))<<"' in for loop was null. Bug in compiler?")
                        ERR_LINE2(statement->location, "")
                    )
                }
                continue;
            }
            auto varinfo_index = info.ast->getVariableByIdentifier(varnameNr.identifier);
            auto varinfo_item = info.ast->getVariableByIdentifier(varnameIt.identifier);

            // NOTE: We add debug information last because varinfo does not have the frame offsets yet.

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
                    framePush(typeId, &varinfo_index->versions_dataOffset[info.currentPolyVersion],false, false);
                    varinfo_item->versions_dataOffset[info.currentPolyVersion] = varinfo_index->versions_dataOffset[info.currentPolyVersion];

                    TypeId dtype = {};
                    // Type should be checked in type checker and further down
                    // when extracting ptr and len. No need to check here.
                    SignalIO result = generateExpression(statement->firstExpression, &dtype);
                    if (result != SIGNAL_SUCCESS) {
                        continue;
                    }
                    if(statement->isReverse()){
                        builder.emit_pop(BC_REG_D); // throw range.now
                        builder.emit_pop(BC_REG_A);
                    }else{
                        builder.emit_pop(BC_REG_A);
                        builder.emit_pop(BC_REG_D); // throw range.end
                        builder.emit_incr(BC_REG_A, -1);
                        // decrement by one since for loop increments
                        // before going into the loop
                    }
                    builder.emit_push(BC_REG_A);
                    generatePop(BC_REG_LOCALS, varinfo_index->versions_dataOffset[info.currentPolyVersion], typeId);
                    // generatePop(BC_REG_BP, varinfo_index->versions_dataOffset[info.currentPolyVersion], typeId);
                }
                // i32 itemsize = info.ast->getTypeSize(varinfo_item->typeId);

                _GLOG(log::out << "push loop\n");
                loopScope->stackMoment = currentFrameOffset;
                // loopScope->stackMoment = builder.saveStackMoment();
                loopScope->continueAddress = builder.get_pc();

                // log::out << "frame: "<<varinfo_index->frameOffset<<"\n";
                // TODO: don't generate ptr and length everytime.
                // put them in a temporary variable or something.
                TypeId dtype = {};
                DynamicArray<TypeId> tmp_types;
                // info.code->addDebugText("Generate and push range\n");
                SignalIO result = generateExpression(statement->firstExpression, &tmp_types);
                if(tmp_types.size()) dtype = tmp_types.last();
                if (result != SIGNAL_SUCCESS) {
                    continue;
                }
                BCRegister index_reg = BC_REG_C;
                BCRegister length_reg = BC_REG_D;

                if(statement->isReverse()){
                    builder.emit_pop(length_reg); // range.now we care about
                    builder.emit_pop(BC_REG_A);
                } else {
                    builder.emit_pop(BC_REG_A); // throw range.now
                    builder.emit_pop(length_reg); // range.end we care about
                }

                builder.emit_mov_rm_disp(index_reg, BC_REG_LOCALS, 4, varinfo_index->versions_dataOffset[info.currentPolyVersion]);
                // builder.emit_mov_rm_disp(index_reg, BC_REG_BP, 4, varinfo_index->versions_dataOffset[info.currentPolyVersion]);

                if(statement->isReverse()){
                    builder.emit_incr(index_reg, -1);
                }else{
                    builder.emit_incr(index_reg, 1);
                }
                
                builder.emit_mov_mr_disp(BC_REG_LOCALS, index_reg, 4, varinfo_index->versions_dataOffset[info.currentPolyVersion]);
                // builder.emit_mov_mr_disp(BC_REG_BP, index_reg, 4, varinfo_index->versions_dataOffset[info.currentPolyVersion]);

                if(statement->isReverse()){
                    // info.code->addDebugText("For condition (reversed)\n");
                    builder.emit_gte(index_reg,length_reg, false, true);
                } else {
                    // info.code->addDebugText("For condition\n");
                    builder.emit_lt(index_reg,length_reg, false, true);
                }
                loopScope->resolveBreaks.add(builder.emit_jz(index_reg));

                result = generateBody(statement->firstBody);
                if (result != SIGNAL_SUCCESS)
                    continue;

                builder.emit_jmp(loopScope->continueAddress);

                for (auto ind : loopScope->resolveBreaks) {
                    builder.fix_jump_imm32_here(ind);
                }
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

                    SignalIO result = framePush(varinfo_index->versions_typeId[info.currentPolyVersion], &varinfo_index->versions_dataOffset[info.currentPolyVersion], false, false);

                    if(statement->isReverse()){
                        TypeId dtype = {};
                        // Type should be checked in type checker and further down
                        // when extracting ptr and len. No need to check here.
                        SignalIO result = generateExpression(statement->firstExpression, &dtype);
                        if (result != SIGNAL_SUCCESS) {
                            continue;
                        }
                        builder.emit_pop(BC_REG_D); // throw ptr
                        builder.emit_pop(BC_REG_A); // keep len
                        builder.emit_cast(BC_REG_A, CAST_UINT_SINT, 4,4);
                    }else{
                        builder.emit_li32(BC_REG_A,-1);
                    }
                    builder.emit_push(BC_REG_A);

                    Assert(varinfo_index->versions_typeId[info.currentPolyVersion] == AST_INT64);

                    generatePop(BC_REG_LOCALS,varinfo_index->versions_dataOffset[info.currentPolyVersion],varinfo_index->versions_typeId[info.currentPolyVersion]);
                    // generatePop(BC_REG_BP,varinfo_index->versions_dataOffset[info.currentPolyVersion],varinfo_index->versions_typeId[info.currentPolyVersion]);
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

                    SignalIO result = framePush(varinfo_item->versions_typeId[info.currentPolyVersion], &varinfo_item->versions_dataOffset[info.currentPolyVersion], true, false);
                }

                _GLOG(log::out << "push loop\n");
                loopScope->stackMoment = currentFrameOffset;
                // loopScope->stackMoment = builder.saveStackMoment();
                loopScope->continueAddress = builder.get_pc();

                // TODO: don't generate ptr and length everytime.
                // put them in a temporary variable or something.
                TypeId dtype = {};
                DynamicArray<TypeId> tmp_types{};
                // info.code->addDebugText("Generate and push slice\n");
                SignalIO result = generateExpression(statement->firstExpression, &tmp_types);
                if(tmp_types.size()) dtype = tmp_types.last();
                if (result != SIGNAL_SUCCESS) {
                    continue;
                }
                TypeInfo* ti = info.ast->getTypeInfo(dtype);
                Assert(ti && ti->astStruct && ti->astStruct->name == "Slice");
                auto memdata_ptr = ti->getMember("ptr");
                auto memdata_len = ti->getMember("len");

                // NOTE: careful when using registers since you might use 
                //  one for multiple things. 
                BCRegister ptr_reg = BC_REG_F;
                BCRegister length_reg = BC_REG_B;
                BCRegister index_reg = BC_REG_C;

                // info.code->addDebugText("extract ptr and length\n");
                builder.emit_pop(ptr_reg);
                builder.emit_pop(length_reg);

                // info.code->addDebugText("Index increment/decrement\n");
                
                u8 ptr_size = ast->getTypeSize(memdata_ptr.typeId);
                u8 index_size = ast->getTypeSize(memdata_len.typeId);

                builder.emit_mov_rm_disp(index_reg, BC_REG_LOCALS, index_size, varinfo_index->versions_dataOffset[info.currentPolyVersion]);
                // builder.emit_mov_rm_disp(index_reg, BC_REG_BP, index_size, varinfo_index->versions_dataOffset[info.currentPolyVersion]);
                if(statement->isReverse()){
                    builder.emit_incr(index_reg, -1);
                }else{
                    builder.emit_incr(index_reg, 1);
                }
                builder.emit_mov_mr_disp(BC_REG_LOCALS, index_reg, index_size, varinfo_index->versions_dataOffset[info.currentPolyVersion]);
                // builder.emit_mov_mr_disp(BC_REG_BP, index_reg, index_size, varinfo_index->versions_dataOffset[info.currentPolyVersion]);

                // u8 cond_reg = BC_REG_EBX;
                // u8 cond_reg = BC_REG_R9;
                if(statement->isReverse()){
                    // info.code->addDebugText("For condition (reversed)\n");
                    builder.emit_li32(length_reg, 0); // length reg is not used with reversed
                    builder.emit_gte(index_reg,length_reg, false, true);
                } else {
                    // info.code->addDebugText("For condition\n");
                    builder.emit_lt(index_reg,length_reg, false, true);
                }
                // resolve end, not break, resolveBreaks is bad naming
                // builder.emit_({BC_JZ, BC_REG_A});
                loopScope->resolveBreaks.add(builder.emit_jz(length_reg));

                // BE VERY CAREFUL SO YOU DON'T USE BUSY REGISTERS AND OVERWRITE
                // VALUES. UNEXPECTED VALUES WILL HAPPEN AND IT WILL BE PAINFUL.
                
                // NOTE: index_reg is modified here since it isn't needed anymore
                if(statement->isPointer()){
                    if(itemsize>1){
                        builder.emit_li32(BC_REG_A,itemsize);
                        builder.emit_mul(BC_REG_A, index_reg, false, 4, true);
                    } else {
                        builder.emit_mov_rr(BC_REG_A, index_reg);
                    }
                    builder.emit_add(ptr_reg, BC_REG_A, false, 8);

                    builder.emit_mov_mr_disp(BC_REG_LOCALS, ptr_reg, 8, varinfo_item->versions_dataOffset[info.currentPolyVersion]);
                    // builder.emit_mov_mr_disp(BC_REG_BP, ptr_reg, 8, varinfo_item->versions_dataOffset[info.currentPolyVersion]);
                } else {
                    if(itemsize>1){
                        builder.emit_li32(BC_REG_A,itemsize);
                        builder.emit_mul(BC_REG_A, index_reg, false, 4, true);
                    } else {
                        builder.emit_mov_rr(BC_REG_A, index_reg);
                    }
                    builder.emit_add(ptr_reg, BC_REG_A, false, 8);

                    builder.emit_get_local(BC_REG_E, varinfo_item->versions_dataOffset[info.currentPolyVersion]);
                    // builder.emit_add(BC_REG_E, BC_REG_BP, false, 8);
                    
                    // builder.emit_({BC_BXOR, BC_REG_A, BC_REG_A}); // BC_MEMCPY USES AL
                    
                    builder.emit_li32(BC_REG_B,itemsize);
                    // builder.emit_mov_rr(BC_REG_A, BC_REG_B}); // BC_MEMCPY USES AL
                    builder.emit_memcpy(BC_REG_E, ptr_reg, BC_REG_B);
                    

                    // info.code->add({BC_MOV_RR, ptr_reg, BC_REG_A});
                    // info.code->add({BC_SUBI, BC_REG_A, BC_REG_RDI, BC_REG_A});
                }

                result = generateBody(statement->firstBody);
                if (result != SIGNAL_SUCCESS)
                    continue;

                builder.emit_jmp(loopScope->continueAddress);

                for (auto ind : loopScope->resolveBreaks) {
                    builder.fix_jump_imm32_here(ind);
                }
                
                // delete nr, frameoffset needs to be changed to if so
                // builder.emit_pop(BC_REG_A); 

                // info.ast->removeIdentifier(scopeForVariables, "nr");
                // info.ast->removeIdentifier(scopeForVariables, itemvar);
            }
            
            auto& fun = info.code->debugInformation->functions[info.debugFunctionIndex];
            fun.addVar(varnameIt.name,
                varinfo_item->versions_dataOffset[info.currentPolyVersion],
                varinfo_item->versions_typeId[info.currentPolyVersion],
                info.currentScopeDepth + 1, // +1 because variables exist within stmt->firstBody, not the current scope
                varnameIt.identifier->scopeId);
            fun.addVar(varnameNr.name,
                varinfo_index->versions_dataOffset[info.currentPolyVersion],
                varinfo_index->versions_typeId[info.currentPolyVersion],
                info.currentScopeDepth + 1,
                varnameNr.identifier->scopeId);
                
            // fun.localVariables.add({});
            // fun.localVariables.last().name = varnameIt.name;
            // fun.localVariables.last().frameOffset = varinfo_item->versions_dataOffset[info.currentPolyVersion];
            // fun.localVariables.last().typeId = varinfo_item->versions_typeId[info.currentPolyVersion];
            
            // fun.localVariables.add({});
            // fun.localVariables.last().name = varnameNr.name;
            // fun.localVariables.last().frameOffset = varinfo_index->versions_dataOffset[info.currentPolyVersion];
            // fun.localVariables.last().typeId = varinfo_index->versions_typeId[info.currentPolyVersion];

            // builder.restoreStackMoment(stackBeforeLoop);
            info.currentFrameOffset = frameBeforeLoop;

            // pop loop happens in defer
        } else if(statement->type == ASTStatement::BREAK) {
            GenContext::LoopScope* loop = info.getLoop(info.loopScopes.size()-1);
            if(!loop) {
                ERR_SECTION(
                    ERR_HEAD2(statement->location)
                    ERR_MSG("Break is only allowed in loops.")
                    ERR_LINE2(statement->location,"not in a loop")
                )
                continue;
            }
            // builder.restoreStackMoment(loop->stackMoment, true);
            if(loop->stackMoment != currentFrameOffset)
                builder.emit_free_local(loop->stackMoment - currentFrameOffset); // freeing local variables without modifying currentFrameOffset
            
            loop->resolveBreaks.add(builder.emit_jmp());
        } else if(statement->type == ASTStatement::CONTINUE) {
            GenContext::LoopScope* loop = info.getLoop(info.loopScopes.size()-1);
            if(!loop) {
                ERR_SECTION(
                    ERR_HEAD2(statement->location)
                    ERR_MSG("Continue is only allowed in loops.")
                    ERR_LINE2(statement->location,"not in a loop")
                )
                continue;
            }
            // builder.restoreStackMoment(loop->stackMoment, true);
            if(loop->stackMoment != currentFrameOffset)
                builder.emit_free_local(loop->stackMoment - currentFrameOffset);

            builder.emit_jmp(loop->continueAddress);
        } else if (statement->type == ASTStatement::RETURN) {
            _GLOG(SCOPE_LOG("RETURN"))

            if (!info.currentFunction) {
                ERR_SECTION(
                    ERR_HEAD2(statement->location)
                    ERR_MSG("Return only allowed in function.")
                    ERR_LINE2(statement->location, "bad")
                )
                continue;
                // return SIGNAL_FAILURE;
            }
            if ((int)statement->arrayValues.size() != (int)info.currentFuncImpl->returnTypes.size()) {
                // std::string msg = 
                ERR_SECTION(
                    ERR_HEAD2(statement->location)
                    ERR_MSG("Found " << statement->arrayValues.size() << " return value(s) but should have " << info.currentFuncImpl->returnTypes.size() << " for '" << info.currentFunction->name << "'.")
                    ERR_LINE2(info.currentFunction->location, "X return values")
                    ERR_LINE2(statement->location, "Y values")
                )
            }

            if(info.currentFunction->callConvention == STDCALL || info.currentFunction->callConvention == UNIXCALL) {
                Assert(statement->arrayValues.size() <= 1);
                // stdcall and unixcall can only have one return value
                // error message should be handled in type checker
            }

            //-- Evaluate return values
            // switch(info.currentFunction->callConvention){
            // case BETCALL: {
                for (int argi = 0; argi < (int)statement->arrayValues.size(); argi++) {
                    ASTExpression *expr = statement->arrayValues.get(argi);
                    // nextExpr = nextExpr->next;
                    // argi++;

                    TypeId dtype = {};
                    SignalIO result = generateExpression(expr, &dtype);
                    if (result != SIGNAL_SUCCESS) {
                        continue;
                    }
                    if (argi < (int)info.currentFuncImpl->returnTypes.size()) {
                        // auto a = info.ast->typeToString(dtype);
                        // auto b = info.ast->typeToString(info.currentFuncImpl->returnTypes[argi].typeId);
                        auto& retType = info.currentFuncImpl->returnTypes[argi];
                        if (!performSafeCast(dtype, retType.typeId)) {
                            // if(info.currentFunction->returnTypes[argi]!=dtype){
                            ERRTYPE1(expr->location, dtype, info.currentFuncImpl->returnTypes[argi].typeId, "(return values)");
                            
                            generatePop(BC_REG_INVALID, 0, dtype);
                        } else {
                            generatePop_set_ret(retType.offset - info.currentFuncImpl->returnSize, retType.typeId);
                            // generatePop(BC_REG_BP, retType.offset - info.currentFuncImpl->returnSize, retType.typeId);
                        }
                    } else {
                        // error here which has been printed somewhere
                        // but we should throw away values on stack so that
                        // we don't become desyncrhonized.
                        generatePop(BC_REG_INVALID, 0, dtype);
                    }
                }
            // } break;
            // case STDCALL: {
            //     for (int argi = 0; argi < (int)statement->arrayValues.size(); argi++) {
            //         ASTExpression *expr = statement->arrayValues.get(argi);
            //         // nextExpr = nextExpr->next;
            //         // argi++;

            //         TypeId dtype = {};
            //         SignalIO result = generateExpression(expr, &dtype);
            //         if (result != SIGNAL_SUCCESS) {
            //             continue;
            //         }
            //         if (argi < (int)info.currentFuncImpl->returnTypes.size()) {
            //             // auto a = info.ast->typeToString(dtype);
            //             // auto b = info.ast->typeToString(info.currentFuncImpl->returnTypes[argi].typeId);
            //             auto& retType = info.currentFuncImpl->returnTypes[argi];
            //             if (!performSafeCast(dtype, retType.typeId)) {
            //                 // if(info.currentFunction->returnTypes[argi]!=dtype){

            //                 ERRTYPE1(expr->location, dtype, info.currentFuncImpl->returnTypes[argi].typeId, "(return values)");

            //                 generatePop(BC_REG_INVALID, 0, dtype); // throw away value to prevent cascading bugs
            //             }
            //             u8 size = info.ast->getTypeSize(retType.typeId);
            //             if(size<=8){
            //                 if(AST::IsDecimal(retType.typeId)) {
            //                     Assert(false); // what size on the float?
            //                     builder.emit_pop(BC_REG_XMM0);
            //                 } else {
            //                     builder.emit_pop(BC_REG_A);
            //                 }
            //             } else {
            //                 generatePop(BC_REG_INVALID, 0, retType.typeId); // throw away value to prevent cascading bugs
            //             }
            //         } else {
            //             generatePop(BC_REG_INVALID, 0, dtype); // throw away value to prevent cascading bugs
            //         }
            //     }
            //     // if(statement->arrayValues.size()==0){
            //     //     builder.emit_bxor(BC_REG_A,BC_REG_A);
            //     // }
            // } break;
            // case UNIXCALL: {
            //     Assert(false);
            //     #ifdef gone
            //     for (int argi = 0; argi < (int)statement->arrayValues.size(); argi++) {
            //         ASTExpression *expr = statement->arrayValues.get(argi);
            //         // nextExpr = nextExpr->next;
            //         // argi++;

            //         TypeId dtype = {};
            //         SignalIO result = generateExpression(expr, &dtype);
            //         if (result != SIGNAL_SUCCESS) {
            //             continue;
            //         }
            //         if (argi < (int)info.currentFuncImpl->returnTypes.size()) {
            //             // auto a = info.ast->typeToString(dtype);
            //             // auto b = info.ast->typeToString(info.currentFuncImpl->returnTypes[argi].typeId);
            //             auto& retType = info.currentFuncImpl->returnTypes[argi];
            //             if (!performSafeCast(dtype, retType.typeId)) {
            //                 // if(info.currentFunction->returnTypes[argi]!=dtype){

            //                 ERRTYPE1(expr->location, dtype, info.currentFuncImpl->returnTypes[argi].typeId, "(return values)");

            //                 generatePop(BC_REG_INVALID, 0, dtype); // throw away value to prevent cascading bugs
            //             }
            //             u8 size = info.ast->getTypeSize(retType.typeId);
            //             if(size<=8){
            //                 if(AST::IsDecimal(retType.typeId)) {
            //                     Assert(false),
            //                     builder.emit_pop(BC_REG_XMM0);
            //                 } else {
            //                     builder.emit_pop(BC_REG_A);
            //                 }
            //             } else {
            //                 generatePop(BC_REG_INVALID, 0, retType.typeId); // throw away value to prevent cascading bugs
            //             }
            //         } else {
            //             generatePop(BC_REG_INVALID, 0, dtype); // throw away value to prevent cascading bugs
            //         }
            //     }
            //     #endif
            //     // if(statement->arrayValues.size()==0){
            //     //     builder.emit_bxor(BC_REG_A,BC_REG_A);
            //     // }
            //     break;
            // }
            // default: {
            //     INCOMPLETE
            // }
            // }
            // builder.emit_pop(BC_REG_BP);
            if(currentFrameOffset != 0)
                builder.emit_free_local(-currentFrameOffset);
            // builder.restoreStackMoment(VIRTUAL_STACK_START, true);
            builder.emit_ret();
            info.currentFrameOffset = lastOffset; // nochecking TODO: Should we reset frame like this? If so, should we not break this loop and skip the rest of the statements too?
        }
        else if (statement->type == ASTStatement::EXPRESSION) {
            _GLOG(SCOPE_LOG("EXPRESSION"))

            DynamicArray<TypeId> exprTypes{};
            SignalIO result = generateExpression(statement->firstExpression, &exprTypes);
            if (result != SIGNAL_SUCCESS) {
                continue;
            }
            if(exprTypes.size() > 0 && exprTypes[0] != AST_VOID){
                for(int i = 0; i < (int) exprTypes.size();i++) {
                    TypeId dtype = exprTypes[i];
                    generatePop(BC_REG_INVALID, 0, dtype);
                }
            }
        }
        else if (statement->type == ASTStatement::USING) {
            _GLOG(SCOPE_LOG("USING"))
            Assert(false); // TODO: Fix this code and error messages
            Assert(statement->varnames.size()==1);

            // Token* name = &statement->varnames[0].name;

            // auto origin = info.ast->findIdentifier(info.currentScopeId, *name);
            // if(!origin){
            //     ERR_HEAD2(statement->location) << *name << " is not a variable (using)\n";
                
            //     return SIGNAL_FAILURE;
            // }
            // Fix something with content order? Is something fixed in type checker? what?
            // auto aliasId = info.ast->addIdentifier(info.currentScopeId, *statement->alias);
            // if(!aliasId){
            //     ERR_HEAD2(statement->location) << *statement->alias << " is already a variable or alias (using)\n";
                
            //     return SIGNAL_FAILURE;
            // }

            // aliasId->type = origin->type;
            // aliasId->varIndex = origin->varIndex;
        } else if (statement->type == ASTStatement::DEFER) {
            _GLOG(SCOPE_LOG("DEFER"))
            Assert(false); // defers should have been replaced in the parser no?
            SignalIO result = generateBody(statement->firstBody);
            // Is it okay to do nothing with result?
        } else if (statement->type == ASTStatement::BODY) {
            _GLOG(SCOPE_LOG("BODY (statement)"))

            SignalIO result = generateBody(statement->firstBody);
            // Is it okay to do nothing with result?
        } else if (statement->type == ASTStatement::TEST) {
            int moment = currentFrameOffset;
            // int moment = builder.saveStackMoment();
            DynamicArray<TypeId> exprTypes{};
            SignalIO result = generateExpression(statement->testValue, &exprTypes);
            if(exprTypes.size() != 1) {
                ERR_SECTION(
                    ERR_HEAD2(statement->testValue->location)
                    ERR_MSG("The expression in test statements must consist of 1 type.")
                    ERR_LINE2(statement->testValue->location,exprTypes.size()<<" types")
                )
                continue;
            }
            int size = info.ast->getTypeSize(exprTypes.last());
            if(size > 8 ) {
                ERR_SECTION(
                    ERR_HEAD2(statement->testValue->location)
                    ERR_MSG("Type cannot be larger than 8 bytes. Test statement doesn't handle larger structs.")
                    ERR_LINE2(statement->testValue->location,"bad")
                )
                continue;
            }
            exprTypes.resize(0);
            result = generateExpression(statement->firstExpression, &exprTypes);
            if(exprTypes.size() != 1) {
                ERR_SECTION(
                    ERR_HEAD2(statement->firstExpression->location)
                    ERR_MSG("The expression in test statements must consist of 1 type.")
                    ERR_LINE2(statement->firstExpression->location,exprTypes.size()<<" types")
                )
                continue;
            }
            size = info.ast->getTypeSize(exprTypes.last());
            if(size > 8) {
                ERR_SECTION(
                    ERR_HEAD2(statement->firstExpression->location)
                    ERR_MSG("Type cannot be larger than 8 bytes. Test statement doesn't handle larger structs.")
                    ERR_LINE2(statement->testValue->location,"bad")
                )
                continue;
            }
            // TODO: Should the types match? u16 - i32 might be fine but f32 - u16 shouldn't be okay.
            builder.emit_pop(BC_REG_A);
            builder.emit_pop(BC_REG_D);
            
            // TEST_VALUE calls stdcall functions which needs 16-byte alignment
            Assert(currentFrameOffset % 16 == 0);
            // TODO: we may need to alloc_local to align stack, then test, then free_local
            // builder.emit_stack_alignment(16); 
            // Assert(false);
            int loc = compiler->options->addTestLocation(statement->location, &compiler->lexer);
            builder.emit_test(BC_REG_D, BC_REG_A, 8, loc);
            
            // builder.restoreStackMoment(moment);
            
        } else {
            Assert(("You forgot to implement statement type!",false));
        }
    }
    return SIGNAL_SUCCESS;
}
SignalIO GenContext::generatePreload() {
    BCRegister src_reg = BC_REG_F;
    BCRegister dst_reg = BC_REG_E;
    int polyVersion = 0;

    if(!info.varInfos[VAR_INFOS])
        return SIGNAL_SUCCESS;

    // Take pointer of type information arrays
    // and move into the global slices.
    builder.emit_dataptr(src_reg, info.dataOffset_types);
    builder.emit_dataptr(dst_reg, info.varInfos[VAR_INFOS]->versions_dataOffset[polyVersion]);
    builder.emit_mov_mr(dst_reg, src_reg, 8);
    
    builder.emit_dataptr(src_reg, info.dataOffset_members);
    builder.emit_dataptr(dst_reg, info.varInfos[VAR_MEMBERS]->versions_dataOffset[polyVersion]);
    builder.emit_mov_mr(dst_reg, src_reg, 8);

    builder.emit_dataptr(src_reg, info.dataOffset_strings);
    builder.emit_dataptr(dst_reg, info.varInfos[VAR_STRINGS]->versions_dataOffset[polyVersion]);
    builder.emit_mov_mr(dst_reg, src_reg, 8);
    return SIGNAL_SUCCESS;
}
// Generate data from the type checker
SignalIO GenContext::generateData() {
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
                    count_stringdata += mem.name.length() + 1; // +1 for null termination
                }
            }
        }
        info.code->ensureAlignmentInData(8); // just to be safe
        int off_typedata   = info.code->appendData(nullptr, count_types * sizeof(lang::TypeInfo));
        info.code->ensureAlignmentInData(8); // just to be safe
        int off_memberdata = code->appendData(nullptr, count_members * sizeof(lang::TypeMember));
        int off_stringdata = code->appendData(nullptr, count_stringdata);
        
        lang::TypeInfo* typedata = (lang::TypeInfo*)(info.code->dataSegment.data() + off_typedata);
        lang::TypeMember* memberdata = (lang::TypeMember*)(info.code->dataSegment.data() + off_memberdata);
        char* stringdata = (char*)(info.code->dataSegment.data() + off_stringdata);

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

                    memcpy(stringdata + string_offset, ast_mem.name.c_str(), ast_mem.name.length());
                    stringdata[string_offset + ast_mem.name.length()] = '\0';
                    string_offset += ast_mem.name.length() + 1;

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

        lang::Slice* slice_types = (lang::Slice*)(code->dataSegment.data() + info.varInfos[VAR_INFOS]->versions_dataOffset[polyVersion]);
        lang::Slice* slice_members = (lang::Slice*)(code->dataSegment.data() + info.varInfos[VAR_MEMBERS]->versions_dataOffset[polyVersion]);
        lang::Slice* slice_strings = (lang::Slice*)(code->dataSegment.data() + info.varInfos[VAR_STRINGS]->versions_dataOffset[polyVersion]);
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

    return SIGNAL_SUCCESS;
}
#ifdef gone
Bytecode* Generate(AST *ast, CompileInfo* compileInfo) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue);
#ifdef LOG_ALLOC
    static bool sneaky=false;
    if(!sneaky){
        sneaky=true;
        TrackType(sizeof(GenContext::LoopScope), "LoopScope");
    }
#endif

    // _VLOG(log::out <<log::BLUE<<  "##   Generator   ##\n";)

    GenContext info{};
    info.code = Bytecode::Create();
    info.ast = ast;
    info.compileInfo = compileInfo;
    info.currentScopeId = ast->globalScopeId;
    // info.code->nativeRegistry = info.compileInfo->nativeRegistry;

    // SignalIO result = GenerateData(info); // nocheckin

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
    // result = GenerateFunctions(info, info.ast->mainBody); // nocheckin
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
            dfun->fileIndex = di->addOrGetFile(info.compileInfo->options->sourceFile.text);
        }
        // TODO: You could create a funcImpl for main here BUT we don't need to because this main
        //  encapsulates the global code which doesn't have a function with arguments.
        dfun->funcImpl = nullptr;
        dfun->funcAst = nullptr;
        dfun->funcStart = info.code->length();
        dfun->bc_start = info.code->length();
        // IMPORTANT TODO: A global main body does not have one tokenstream...
        dfun->tokenStream = info.ast->mainBody->tokenRange.tokenStream();
        info.currentScopeDepth = -1;
        // dfun->scopeId = info.ast->globalScopeId;

        info.currentPolyVersion = 0;
        info.virtualStackPointer = GenContext::VIRTUAL_STACK_START;
        info.currentFrameOffset = 0;
        builder.emit_push(BC_REG_BP);
        builder.emit_mov_rr(BC_REG_BP, BC_REG_SP);
        
        dfun->codeStart = info.code->length();
        dfun->entry_line = info.ast->mainBody->tokenRange.firstToken.line;

        // GeneratePreload(info); // nocheckin

        // TODO: What to do about result? nothing?
        // result = GenerateBody(info, info.ast->mainBody); // nocheckin
        
        dfun->codeEnd = info.code->length();

        builder.emit_pop(BC_REG_BP);

        builder.emit_({BC_RET});
        dfun->funcEnd = info.code->length();
        dfun->bc_end = info.code->length();
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

    info.compileInfo->options->compileStats.errors += info.errors;
    return info.code;
}
#endif

bool GenerateScope(ASTScope* scope, Compiler* compiler, CompilerImport* imp, DynamicArray<TinyBytecode*>* out_codes, bool is_initial_import) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Blue);

    // _VLOG(log::out <<log::BLUE<<  "##   Generator   ##\n";)

    GenContext context{};
    // 
    context.ast = compiler->ast;
    context.code = compiler->bytecode;
    context.compiler = compiler;
    context.reporter = &compiler->reporter;
    context.imp = imp;
    context.out_codes = out_codes;

    context.generateFunctions(scope);
    
    if(is_initial_import) {
        auto iden = compiler->ast->findIdentifier(compiler->ast->globalScopeId,0,"main");
        if(!iden) {
            // If no main function exists then the code in global scope of
            // initial import will be the main function.

            // TODO: Code here
            CallConventions main_conv;
            switch(compiler->options->target) {
                case TARGET_WINDOWS_x64: main_conv = STDCALL; break;
                case TARGET_UNIX_x64: main_conv = UNIXCALL; break;
                case TARGET_BYTECODE: main_conv = BETCALL; break;
                default: Assert(false);
            }
            TinyBytecode* tb_main = context.code->createTiny(main_conv);
            tb_main->name = "main";
            context.code->index_of_main = tb_main->index;

            // TODO: Code below should be the same as the one in generateFunction.
            //   If we change the code in generateFunction but forget to here then
            //   we will be in trouble. So, how do we combine the code?

            context.tinycode = tb_main;
            context.builder.init(context.code, context.tinycode);
            context.currentScopeId = context.ast->globalScopeId;
            context.currentFrameOffset = 0;

            out_codes->add(tb_main);

            auto di = context.code->debugInformation;
            context.debugFunctionIndex = di->functions.size();
            DebugInformation::Function* dfun = di->addFunction(context.tinycode->name);
            context.code->addExportedFunction("main", context.tinycode->index);
            
            context.generateBody(scope);
            
            if(context.builder.get_last_opcode() != BC_RET) {
                if(context.currentFrameOffset != 0)
                    context.builder.emit_free_local(-context.currentFrameOffset);
                // context.builder.restoreStackMoment(GenContext::VIRTUAL_STACK_START, true);
                context.builder.emit_ret();
            } else {
                // context.builder.restoreStackMoment(GenContext::VIRTUAL_STACK_START, false, true);
            }
        }
    }
    
    // TODO: Relocations?
    
    context.compiler->options->compileStats.errors += context.errors;

    // return tb_main;
    return true;
}