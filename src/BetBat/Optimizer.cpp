#include "BetBat/Optimizer.h"

// #include "BetBat/Generator.h"

#ifdef OLOG
#define _OLOG(x) x;
#else
#define _OLOG(x) ;
#endif

bool OptimizeBytecode(Bytecode& code){
    using namespace engone;
    
    log::out <<log::BLUE<< "\n##   Optimizer\n";
    
    int oldMemory = code.getMemoryUsage();
    std::vector<int> removedInsts;
    
    struct OptInfo {
        int createdBy=-1;
        std::vector<int> wasted;
        bool needed=false;
    };
    OptInfo needed[256]{};
    
    /*
    str $b
    loadc $b
    str $a
    add $a $b $a
    del $b
    
    add $a $b $a
    add $a $b $b
    add $a $b $c

    run $a
    del $a
    
    */

    // Instruction& inst = code.get(0);
    // inst.type==BC_ADD;
    // needed[inst.reg0].needed == false
    // needed[inst.reg1].needed == false
    // inst.reg2 = 0
    
    
    // Todo: code is barely tested. there are probably many edge cases not taken into account
    
    // Removes instructions which create values and deletes them without doing
    //  anything with them.
    for(int i=0;i<code.length();i++){
        Instruction& inst = code.get(i);
        if(inst.type==0){
            _OLOG(log::out << log::LIME<<"Delete "<<i<<": "<<code.get(i) << "\n";)
            code.remove(i);
            removedInsts.push_back(i);
        }
        // if(inst.type==BC_ADD){
        //     // OptInfo& r0 = needed[inst.reg0];
        //     // OptInfo& r1 = needed[inst.reg1];
        //     // OptInfo& r2 = needed[inst.reg2];

            

        //     // if(!r0.needed&&!r1.needed){
        //     //     // instruction is guarranteed to be r2 = 0
        //     //     if(!r2.needed){
        //     //         // 0 = 0 + 0 is useless
        //     //         // remove instructions
        //     //     } else{
        //     //         // hmm?
        //     //     }
        //     // }
        // Note: DON'T DO ELSE on inst.type. BC_ADD affects code below, don't skip it with
        // else if
        // }
        if(inst.type==BC_NUM||inst.type==BC_STR){
            if(inst.reg0>=REG_ACC0){
                OptInfo& info = needed[inst.reg0] = {};
                info.needed = false;
                info.createdBy = i;
                info.wasted.clear();
            }
        } else if(inst.type==BC_COPY){
            if(inst.reg1>=REG_ACC0){
                OptInfo& info = needed[inst.reg1] = {};
                info.needed = false;
                info.createdBy = i;
                info.wasted.clear();
            }
        } else if(inst.type==BC_DEL){
            if(inst.reg0>=REG_ACC0){
                OptInfo& info = needed[inst.reg0];
                if(info.createdBy!=-1&&!info.needed){
                    _OLOG(log::out << log::LIME<<"Delete "<<info.createdBy<<": "<<code.get(info.createdBy) << "\n";)
                    code.remove(info.createdBy);
                    removedInsts.push_back(info.createdBy);
                    for(auto index : info.wasted){
                        _OLOG(log::out << log::LIME<<"Delete "<<index<<": "<<code.get(index) << "\n";)
                        code.remove(index);
                        removedInsts.push_back(index);
                    }
                    _OLOG(log::out << log::LIME<<"Delete "<<i<<": "<<code.get(i) << "\n";)
                    code.remove(i);
                    removedInsts.push_back(i);
                }
            }
        } else if(inst.type==BC_LOADC){
            OptInfo& info = needed[inst.reg0];
            info.wasted.push_back(i);
        } else if(inst.type!=BC_LOADV) {
            OptInfo& info = needed[inst.reg0];
            info.needed = true;
            OptInfo& info0 = needed[inst.reg1];
            info0.needed = true;
            OptInfo& info1 = needed[inst.reg2];
            info1.needed = true;
        }
    }
    bool rearrange=false;
    // Remove and rearrange instructions
    if(rearrange&&removedInsts.size()!=0){
        // Todo: relocate jump address numbers
        // go through all instructions, find the an instructions jump address
        
        std::vector<Number*> constAddresses;
        
        // int offset = 0;
        int removeIndex=0;
        for(int i=0;i<code.length();i++){
            Instruction& inst = code.get(i);
            
            if((inst.type==BC_JUMP||inst.type==BC_JUMPNIF)){
                int reg = inst.reg0;
                if(inst.type==BC_JUMPNIF)
                    reg = inst.reg1;
                    
                Instruction& prev = code.get(i-1);
                // Todo: for loop to check multiple previous instructions.
                //  Not necessary if loadc is guarranteed to exist before jump
                
                if(prev.type==BC_LOADC && prev.reg0==reg){
                    int constIndex = (int)prev.reg1 | ((int)prev.reg2<<8);
                    constAddresses.push_back(code.getConstNumber(constIndex));
                    // double num = code.getConstNumber(constIndex)->value -= offset;
                }else{
                    log::out << log::YELLOW<<"Optimizer: can't relocate jump, expected loadc not found\n";
                }
            }
        }
        for(int i=0;i<(int)code.debugLines.used;i++){
            auto line = (Bytecode::DebugLine*)code.debugLines.data + i;
            int offset = 0;
            for(int index : removedInsts){
                if(line->instructionIndex>index)
                    offset++;
                else
                    break;
            }
            line->instructionIndex -= offset;
            // _OLOG(log::out << "OFFSET(line) "<<" "<<(line->instructionIndex+offset)<<" -> "<< (line->instructionIndex)<<"\n";)
        }
        for(Number* num : constAddresses){
            int offset = 0;
            for(int index : removedInsts){
                if(num->value>index)
                    offset++;
                else
                    break;
            }
            num->value -= offset;
            // _OLOG(log::out << "OFFSET(jump) "<<" "<<(num->value+offset)<<" -> "<< (num->value)<<"\n";)
        }
    
        // Todo: can you reduce the memcpy's?
        Instruction* basePtr = (Instruction*)code.codeSegment.data;
        int baseIndex = removedInsts[0];
        int from = removedInsts[0]+1;
        for(int i=1;i<(int)removedInsts.size();i++){
            int to = removedInsts[i];
            
            if(from<to){
                // _OLOG(log::out << "MOV "<<from << " "<<to<<" -> "<<baseIndex<<" "<<(baseIndex+to-from)<<"\n";)
                memmove(basePtr+baseIndex,basePtr+from,sizeof(Instruction)*(to-from));
                baseIndex += (to-from);
            }
            from = removedInsts[i]+1;
            
        }
        int to = code.length();
        if(from<to){
            // _OLOG(log::out << "MOV "<<from << " "<<to<<" -> "<<baseIndex<<" "<<(baseIndex+to-from)<<"\n";)
            memmove(basePtr+baseIndex,basePtr+from,sizeof(Instruction)*(to-from));
        }
        code.codeSegment.used-=removedInsts.size();
    }
    
    // Todo: remove unnecessary constants
    // Todo: combine duplicate constants. (cannot be done in generator since a
    //  jump address may be combined with a normal number but the address may need
    //  to be relocated because of optimizations which would affect the normal number)
    
    #define MIN_SIZE(ARR) code.ARR.resize(code.ARR.used);
    // MIN_SIZE(codeSegment)
    // MIN_SIZE(constNumbers)
    // MIN_SIZE(constStrings)
    // MIN_SIZE(constStringText)
    // MIN_SIZE(debugLines)
    // MIN_SIZE(debugLineText)
    
    int freedMemory = oldMemory-code.getMemoryUsage();
    
    log::out << "Optimized away "<<removedInsts.size()<<" instructions, ";
    if(freedMemory!=0)
        log::out <<"freed "<<freedMemory<<" bytes\n";
    
    return true;
}