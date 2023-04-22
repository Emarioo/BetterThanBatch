#include "BetBat/Optimizer.h"

// #include "BetBat/Generator.h"

#ifdef OLOG
#define _OLOG(x) x;
#else
#define _OLOG(x) ;
#endif

bool OptimizeBytecode(Bytecode& code){
    using namespace engone;
    
    _SILENT(log::out <<log::BLUE<< "\n##   Optimizer\n";)
    
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
        } else if(inst.type==BC_LOADNC||inst.type==BC_LOADSC){
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
                
                if((prev.type==BC_LOADNC||prev.type==BC_LOADSC) && prev.reg0==reg){
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
    
    struct ConstData {
        int index;
        int count; // occurrences
    };
    std::unordered_map<double, ConstData> numberConstants;
    engone::Memory numberArray{sizeof(Number)};
    std::unordered_map<std::string,ConstData> stringConstants; // is this fine with larger strings?
    engone::Memory stringArray{sizeof(String)};
    engone::Memory stringData{1};
    
    for(int i=0;i<(int)code.codeSegment.used;i++){
        Instruction& inst = *((Instruction*)code.codeSegment.data+i);  
        
        if(inst.type==BC_LOADNC){
            int constIndex = (uint)inst.reg1 | ((uint)inst.reg2<<8);
            
            Number* num = code.getConstNumber(constIndex);
            
            auto pair = numberConstants.find(num->value);
            if(pair != numberConstants.end()){
                inst.reg1 = pair->second.index&0xFF;
                inst.reg2 = pair->second.index>>8;
                pair->second.count++;
            }else{
                if(numberArray.max<numberArray.used+1)
                    numberArray.resize(numberArray.max*2+10);
                int newIndex = numberArray.used;
                *((Number*)numberArray.data + newIndex) = *num;
                numberArray.used++;
                numberConstants[num->value] = {newIndex,1};
                inst.reg1 = newIndex&0xFF;
                inst.reg2 = newIndex>>8;
            }
        } else if(inst.type==BC_LOADSC){
            // Note: Not optimizing strings because most will probably be unique anyway.
            //   Should still optimize though but it's not very important right now.
            // Note: I ended up implementing this too. I might as well have.
            //   Should work well for small strings not sure about the larger ones.
            //   Duplicates should at least be removed even it it may be slow.
            
            int constIndex = (uint)inst.reg1 | ((uint)inst.reg2<<8);
            
            String* str = code.getConstString(constIndex);
            
            auto pair = stringConstants.find(*str);
            if(pair != stringConstants.end()){
                inst.reg1 = pair->second.index&0xFF;
                inst.reg2 = pair->second.index>>8;
                pair->second.count++;
            }else{
                if(stringArray.max<stringArray.used+1)
                    stringArray.resize(stringArray.max*2+10);
                if(stringData.max<stringData.used+str->memory.used)
                    stringData.resize(stringData.max*2+str->memory.used*2 + 10);
                int newIndex = stringArray.used;
                
                char* ptr = (char*)stringData.used;
                String newStr{};
                newStr.memory.data = ptr;
                newStr.memory.used = str->memory.used;
                
                *((String*)stringArray.data + newIndex) = newStr;
                memcpy((char*)stringData.data+stringData.used, str->memory.data,str->memory.used);
                stringData.used+=str->memory.used;
                stringArray.used++;
                stringConstants[*str] = {newIndex,1};
                inst.reg1 = newIndex&0xFF;
                inst.reg2 = newIndex>>8;
            }
        }
    }
    int removedNumbers = code.constNumbers.used-numberArray.used;
    int removedStrings = code.constStrings.used-stringArray.used;
    // Note: count in ConstData represents how many times a number or string was used.
    //  Not how many actual constants there were. removedNumbers/Strings still
    //  represent how many constants were removed

    _SILENT(
    if(removedNumbers!=0)log::out << "Removed "<<(removedNumbers)<<" const. numbers\n";
    _OLOG(for(auto& pair : numberConstants){
        if(pair.second.count>1){
            log::out << " "<<pair.first<<": "<<pair.second.count<<" usages\n";
        }
    })
    if(removedStrings!=0)log::out << "Removed "<<(removedStrings)<<" const. strings\n";
    _OLOG(for(auto& pair : stringConstants){
        if(pair.second.count>1){
            log::out << " "<<pair.first<<": "<<pair.second.count<<" usages\n";
        }
    })
    )

    code.constNumbers.resize(0);
    code.constNumbers = numberArray;
    
    code.constStrings.resize(0);
    code.constStringText.resize(0);
    code.constStrings = stringArray;
    code.constStringText = stringData;
    // finish pointers
    for(int i=0;i<(int)code.constStrings.used;i++){
        String* str = (String*)code.constStrings.data + i;
        str->memory.data = (char*)code.constStringText.data + (uint64)str->memory.data;
    }
    
    #define MIN_SIZE(ARR) code.ARR.resize(code.ARR.used);
    // MIN_SIZE(codeSegment)
    // MIN_SIZE(constNumbers)
    // MIN_SIZE(constStrings)
    // MIN_SIZE(constStringText)
    // MIN_SIZE(debugLines)
    // MIN_SIZE(debugLineText)
    
    int freedMemory = oldMemory-code.getMemoryUsage();
    
    _SILENT(log::out << "Optimized away "<<removedInsts.size()<<" instructions, ";)
    if(freedMemory!=0){
        _SILENT(log::out <<"freed "<<freedMemory<<" bytes";)
    }
    _SILENT(log::out << "\n";)
    return true;
}