#include "BetBat/x64_Converter.h"

#include "BetBat/ObjectWriter.h"

void ConversionInfo::write(u8 byte){
    textSegment.add(byte);
}

void ConvertTox64(Bytecode* bytecode){
    using namespace engone;
    for(int i=0;i<bytecode->length();i++){
        auto inst = bytecode->get(i);
        u8 opcode = inst.opcode;
        log::out << inst;
        if(opcode == BC_LI || opcode==BC_JMP || opcode==BC_JE || opcode==BC_JNE ){
            i++;
            log::out << bytecode->getIm(i);
        }
        log::out << "\n";
        
        auto op0 = inst.op0;
        auto op1 = inst.op1;
        auto op2 = inst.op2;
        switch(opcode){
            case BC_ADDI: {
                
                break;
            }
        }
    }

}