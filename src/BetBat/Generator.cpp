#include "BetBat/Generator.h"

#define ERRT(T) engone::log::out<< "CompileError at "<<T.line<<":"<<T.column<<": ";
#define ERRTL(T) engone::log::out<<"CompileError at "<<T.line<<":"<<T.column+T.length<<": ";
#define ERRTOK engone::log::out<<"CompileError at "<<token.line<<":"<<token.column<<": ";
#define ERRTOKL engone::log::out<<"CompileError at "<<token.line<<":"<<token.column + token.length<<": ";
#define ERRAT(L,C) engone::log::out << "CompileError at "<<L<<":"<<C<<": ";
#define CHECK_END if(info.end()){\
        engone::log::out << "CompileError: Unexpected end of file\n";\
        break;\
    }
#define CHECK_ENDR if(info.end()){\
        engone::log::out << "CompileError: Unexpected end of file\n";\
        return false;\
    }


const char* InstToString(int type){
    #define INSTCASE(x) case x: return #x;
    switch(type){
        INSTCASE(BC_ADD)
        INSTCASE(BC_SUB)
        INSTCASE(BC_MUL)
        INSTCASE(BC_DIV)
        
        INSTCASE(BC_LOAD_CONST)

        INSTCASE(BC_MAKE_NUMBER)

        INSTCASE(BC_JUMP)
    }
    return "BC_?";
}

void Instruction::print(){
    using namespace engone;
    if((type&BC_MASK) == BC_R3)
        log::out<<InstToString(type)<<" $"<<reg0<<" $"<<reg1<<" $"<<reg2;
    if((type&BC_MASK) == BC_R2)
        log::out << InstToString(type)<<" $" <<reg0<<" $"<< reg12;
    if((type&BC_MASK) == BC_R1)
        log::out << InstToString(type)<<" $"<< (reg0 | (reg12<<8));
}
engone::Logger& operator<<(engone::Logger& logger, Instruction& instruction){
    instruction.print();
    return logger;
}
bool Bytecode::add(Instruction instruction){
    if(codeSegment.max == codeSegment.used){
        if(!codeSegment.resize(codeSegment.max*2 + 100))
            return false;   
    }
    *((Instruction*)codeSegment.data + codeSegment.used++) = instruction;
    return true;
}
bool Bytecode::add(uint8 type, uint8 reg0, uint8 reg1, uint8 reg2){
    Instruction inst{};
    inst.type = type;
    inst.reg0 = reg0;
    inst.reg1 = reg1;
    inst.reg2 = reg2;
    return add(inst);
}
bool Bytecode::add(uint8 type, uint8 reg0, uint16 reg12){
    Instruction inst{};
    inst.type = type;
    inst.reg0 = reg0;
    inst.reg12 = reg12;
    return add(inst);
}
bool Bytecode::add(uint8 type, uint reg012){
    Instruction inst{};
    inst.type = type;
    inst.reg0 = reg012&0xFF;
    inst.reg12 = reg012>>8;
    return add(inst);
}
uint Bytecode::addConstNumber(double number){
    if(constNumbers.max == constNumbers.used){
        if(!constNumbers.resize(constNumbers.max*2 + 100))
            return false;   
    }
    int index = constNumbers.used;
    *((Number*)constNumbers.data + index) = {number};
    constNumbers.used++;
    return index;
}
Number* Bytecode::getConstNumber(uint index){
    return (Number*)constNumbers.data + index;
}
uint Bytecode::length(){
    return codeSegment.used;   
}
Instruction& Bytecode::get(uint index){
    Assert(index<codeSegment.used);
    return *((Instruction*)codeSegment.data + index);
}

int IsInteger(Token& token){
    const char numchars[]="0123456789";
    int len = sizeof(numchars);
    for(int i=0;i<token.length;i++){
        for(int j=0;j<sizeof(numchars);j++){
            if(token.str[i]!=numchars[j]){
                return false;
            }
        }
    }
    return true;
}
// Can also be an integer
int IsDecimal(Token& token){
    const char numchars[]="0123456789.";
    int len = sizeof(numchars);
    int hasNums=false;
    for(int i=0;i<token.length;i++){
        if(token.str[i]!='.')
            hasNums=true;
        bool found=false;
        for(int j=0;j<sizeof(numchars);j++){
            if(token.str[i]==numchars[j]){
                found = true;
                break;
            }
        }
        if(!found)
            return false;
    }
    return hasNums;
}
double ConvertInteger(Token& token){
    // Todo: string is not null terminated
    //  temporariy changing character after token
    //  is not safe since it could be a memory violation
    char tmp = *(token.str+token.length);
    *(token.str+token.length) = 0;
    double num = atoi(token.str);
    *(token.str+token.length) = tmp;
    return num;
}
double ConvertDecimal(Token& token){
    // Todo: string is not null terminated
    //  temporariy changing character after token
    //  is not safe since it could be a memory violation
    char tmp = *(token.str+token.length);
    *(token.str+token.length) = 0;
    double num = atof(token.str);
    *(token.str+token.length) = tmp;
    return num;
}
Token GenerationInfo::next(){
    return tokens.get(index++);
}
Token GenerationInfo::prev(){
    return tokens.get(index-1);
}
Token GenerationInfo::now(){
    return tokens.get(index);
}
bool GenerationInfo::end(){
    Assert(index<=tokens.length());
    return index==tokens.length();
}
void GenerationInfo::finish(){
    index = tokens.length();
}
int GenerationInfo::at(){
    return index-1;
}
void GenerationInfo::nextLine(){
    while(!end()){
        Token pre = prev();
        if(pre.flags&TOKEN_SUFFIX_LINE_FEED){
            break;
        }
        Token token = next();
        // if(token.flags&TOKEN_SUFFIX_LINE_FEED){
        //     break;
        // }
    }
}
void EvaluateExpression(GenerationInfo& info){
    using namespace engone;
    while(!info.end()){
        Token token = info.next();
        if(IsDecimal(token)){
            // We have constant number
            // Add it to data segment

            if(info.baseIndex==-1){
                info.baseIndex = info.at();
            }

            // double num = ConvertNumber(token);
        }
        if(token == "+"){
            if(info.baseIndex==-1){
                log::out << "GenError: cannot do add, invalid baseIndex\n";
                info.finish();
                break;
            }
            Token tmp = info.tokens.get(info.baseIndex);
            double num = ConvertDecimal(tmp);
            int dataIndex = info.code.addConstNumber(num);
            info.code.add(BC_MAKE_NUMBER,1);
            info.code.add(BC_LOAD_CONST,1,dataIndex);
            info.code.add(BC_ADD,1,2,3);
        }
    }
}
Bytecode CompileScript(Tokens tokens){
    using namespace engone;
    log::out << "\n####  Compile Script  ####\n";
    
    GenerationInfo info{};
    info.tokens = tokens;
    
    while (!info.end()){
        EvaluateExpression(info);
    }

    info.code.add(BC_MAKE_NUMBER,0);
    info.code.add(BC_MAKE_NUMBER,1);
    info.code.add(BC_MAKE_NUMBER,2);
    
    int a = info.code.addConstNumber(5);
    int b = info.code.addConstNumber(9);
    
    info.code.add(BC_LOAD_CONST,0,a);
    info.code.add(BC_LOAD_CONST,1,b);
    
    info.code.add(BC_ADD,0,1,2);
    info.code.add(BC_ADD,2,1,2);
    
    // Todo: Cleanup memory in GenerationInfo

    return info.code;
}
#define COMPILE_ARG_VAR 1
#define COMPILE_ARG_ADDR 2
bool CompileInstructionArg(GenerationInfo& info, int& num, int flags){
    using namespace engone;
    Token prev = info.prev(); // make sure prev exists
    if(prev.flags&TOKEN_SUFFIX_LINE_FEED){
        ERRTL(prev) log::out << "Expected an integer ($registernumber)\n";
        return false;
    }
    if(0==prev.flags&TOKEN_SUFFIX_SPACE){
        ERRTL(prev) log::out << "Expected a space after "; prev.print(); log::out<<"\n";
        return false;
    }
    CHECK_ENDR;
    Token token = info.next();
    if((flags&COMPILE_ARG_VAR)){
        auto find = info.nameNumberMap.find(token);
        if(find==info.nameNumberMap.end()){
            ERRTOK token.print(); log::out<<" is undefined\n";
            return false;
        }
        num = find->second;
    }else if((flags&COMPILE_ARG_ADDR)){
        if(IsInteger(token)){
            num = ConvertInteger(token);
        }else{
            // Todo: Verify valid token name
            num = 0;
            info.instructionsToResolve.push_back({info.code.length(),token});
        }
    }else{
        if(token=="$"){
            if(token.flags&TOKEN_SUFFIX_LINE_FEED){
                ERRTOKL log::out <<"Expected a register ($integer or $ab)\n";
                return false;
            } else if(token.flags&TOKEN_SUFFIX_SPACE){
                ERRTOKL log::out << "Expected a register without space ($integer or $ab)\n";
                return false;
            } else {
                CHECK_ENDR
                token = info.next();
                
                if(IsInteger(token)){
                    num = ConvertInteger(token);
                }else{
                    int value = 0;
                    if(token.length==1){
                        if(*token.str>='a'&&*token.str<='z')
                            num = *token.str-'a';
                        else{
                            ERRTOK log::out <<"Expected $integer or $ab\n";
                            return false;
                        }
                    }else if(token.length==2){
                        char s0 = *token.str;
                        char s1 = *(token.str+1);
                        if(s0>='a'&&s0<='z' && s1>='a'&&s1<='z')
                            num = s1-'a' + ('z'-'a')*(s0-'a');
                        else{
                            ERRTOK log::out << "Expected $integer or $ab\n";
                            return false;
                        }
                    }else{
                        ERRTOK log::out <<"Expected $integer or $ab\n";
                        return false;
                    }
                }
            }
        }else{
            ERRTOK log::out<<"Expected $\n";
            return false;
        }
    }
    return true;
}
bool LineEndError(GenerationInfo& info){
    using namespace engone;
    Token prev = info.prev();
    if(0==(prev.flags&TOKEN_SUFFIX_LINE_FEED) && !info.end()){
        ERRTL(prev) log::out << "Expected line feed found "<<info.now()<<"\n";
        info.nextLine();
        return true;
    }
    return false;
}
Bytecode CompileInstructions(Tokens tokens){
    using namespace engone;
    log::out<<"\n#### Compile Instructions ####\n";
    
    std::unordered_map<std::string,int> instructionMap;
    #define MAP(K,V) instructionMap[K] = V;
    
    MAP("add",BC_ADD)
    MAP("sub",BC_SUB)
    MAP("div",BC_DIV)
    MAP("mul",BC_MUL)

    MAP("load",BC_LOAD_CONST)

    MAP("num",BC_MAKE_NUMBER)
    MAP("jump",BC_JUMP)

    GenerationInfo info{};
    info.tokens = tokens;

    // Todo: It is easy to handle multiple syntax errors in assembly.
    //  Each line in assembly is independent of other lines.
    //  Because of this you can skip the to the new line and continue
    //  generating instructions and catch more syntax errors.

    while(!info.end()){
        Token token = info.next();

        if(token.flags&TOKEN_SUFFIX_LINE_FEED){
            ERRTOK log::out << "Expected something after ";token.print();log::out<<"\n";
            info.nextLine();
            continue;
        }

        // first we expect an adress or instruction name
        Token name = token;
    
        CHECK_END
        token = info.next();

        if(token == ":"){
            // Jump address or constant
            if(token.flags & TOKEN_SUFFIX_LINE_FEED){
                // Jump adress
                int address = info.code.length();
                info.addressMap[name] = address;
                log::out<<"Address ";name.print(); log::out<<" = "<<address<<"\n";
            }else{
                CHECK_END
                token = info.next();
                if(IsDecimal(token)){
                    double number = ConvertDecimal(token);
                    int index = info.code.addConstNumber(number);
                    // std::string str = token;
                    info.nameNumberMap[name] = index;
                    log::out<<"Constant ";name.print(); log::out<<"="<<number<<" to "<<index<<"\n";
                }else{
                    ERRTOK log::out<<"String constant not implemented\n";
                    info.nextLine();
                    continue;
                }
                if(0==token.flags&TOKEN_SUFFIX_LINE_FEED){
                    ERRTOKL log::out<<"Expected line feed (\\n)\n";
                    info.nextLine();
                    continue;
                }
            }
        } else if(0==name.flags & TOKEN_SUFFIX_SPACE){
            ERRTL(name) log::out<<"Expected a space\n";
            info.nextLine();
            continue;
        } else {
            // Instruction confirmed
            auto find = instructionMap.find(name);
            if(find==instructionMap.end()){
                ERRTOK log::out<<"Invalid instruction "; name.print(); log::out<<"\n";
                info.nextLine();
                continue;
            }
            int instType = find->second;
            info.index--;
            if((instType & BC_MASK) == BC_R3){
                int regs[3]{0};
                bool error=false;
                for(int i=0;i<3;i++){
                    if(!CompileInstructionArg(info,regs[i],false)) {
                        error=true;
                        break;
                    }
                }
                if(error){
                    info.nextLine();
                    continue;
                }

                info.code.add(instType,regs[0],regs[1],regs[2]);
                info.code.get(info.code.length()-1).print();log::out<<("\n");

                if(LineEndError(info))
                    continue;
            } else if((instType & BC_MASK) == BC_R2){
                int reg=0;
                int var=0;

                if(!CompileInstructionArg(info,reg,0)){
                    info.nextLine();
                    continue;
                }

                // if(instType==BC_LOAD_CONST){
                if(!CompileInstructionArg(info,var,COMPILE_ARG_VAR)){
                    info.nextLine();
                    continue;
                }
                // }

                
                info.code.add(instType,reg,var);
                info.code.get(info.code.length()-1).print();log::out<<("\n");
                
                if(LineEndError(info))
                    continue;
            } else if((instType & BC_MASK) == BC_R1){
                int reg=0;

                if(instType == BC_JUMP){
                    if(!CompileInstructionArg(info,reg,COMPILE_ARG_ADDR)){
                        info.nextLine();
                        continue;
                    }
                }else{
                    if(!CompileInstructionArg(info,reg,0)){
                        info.nextLine();
                        continue;
                    }
                }

                info.code.add(instType,reg);
                info.code.get(info.code.length()-1).print();log::out<<("\n");

                if(LineEndError(info))
                    continue;
            }
        }
    }

    for(auto& addr : info.instructionsToResolve){
        // Todo: check if instruction is of a type that uses addresses?
        auto find = info.addressMap.find(addr.addrName);
        int address = addr.instIndex + 1;
        if(find==info.addressMap.end()){
            log::out << "CompileError at "<<addr.addrName.line<<":"<<addr.addrName.column<<" : Address "<<addr.addrName<<" could not be resolved\n";
        }else{
            address = find->second;
        }
        
        info.code.get(addr.instIndex).reg0 = address&0xFF;
        info.code.get(addr.instIndex).reg12 = address>>8;
    }

    return info.code;
}