#include "BetBat/Generator.h"

#define ERRAT(L,C) engone::log::out << "CompileError "<<(L)<<":"<<(C)<<", "
#define ERRT(T) ERRAT(T.line,T.column)
#define ERRTL(T) ERRAT(T.line,T.column+T.length)
#define ERRTOK ERRAT(token.line,token.column)
#define ERRTOKL ERRAT(token.line,token.column+token.length)

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
        INSTCASE(BC_LESS)
        INSTCASE(BC_GREATER)
        INSTCASE(BC_EQUAL)
        
        INSTCASE(BC_JUMPIF)
        
        INSTCASE(BC_COPY)
        INSTCASE(BC_MOV)
        INSTCASE(BC_RUN)

        INSTCASE(BC_NUM)
        INSTCASE(BC_STR)
        INSTCASE(BC_DEL)
        INSTCASE(BC_JUMP)
        INSTCASE(BC_LOAD)
        INSTCASE(BC_RETURN)
        INSTCASE(BC_ENTERSCOPE)
        INSTCASE(BC_EXITSCOPE)
    }
    return "BC_?";
}

void Instruction::print(){
    using namespace engone;
    if((type&BC_MASK) == BC_R3)
        log::out<<InstToString(type)<<" $"<<reg0<<" $"<<reg1<<" $"<<reg2;
    if((type&BC_MASK) == BC_R2)
        log::out << InstToString(type)<<" $" <<reg0<<" $"<< reg1;
    if((type&BC_MASK) == BC_R1){
        if(type==BC_LOAD){
            log::out << InstToString(type)<<" ["<< singleReg()<<"]";
        }else
            log::out << InstToString(type)<<" $"<< singleReg();
        
    }
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
    inst.reg1 = reg12;
    // inst.reg12 = reg12;
    return add(inst);
}
bool Bytecode::add(uint8 type, uint reg012){
    Instruction inst{};
    inst.type = type;
    inst.reg0 = reg012;
    // &0xFF;
    // inst.reg12 = reg012>>8;
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
    if(index>=constNumbers.used) return 0;
    return (Number*)constNumbers.data + index;
}
uint Bytecode::addConstString(Token& token){
    if(constStrings.max == constStrings.used){
        if(!constStrings.resize(constStrings.max*2 + 100))
            return -1;
    }
    int index = constStrings.used;
    String* str = ((String*)constStrings.data + index);
    new(str)String();
    
    if(!str->memory.resize(token.length)){
        return -1;   
    }
    memcpy(str->memory.data,token.str,token.length);
    str->memory.used = token.length;
    
    constStrings.used++;
    return index;
}
String* Bytecode::getConstString(uint index){
    if(index>=constStrings.used) return 0;
    return (String*)constStrings.data + index;
}
uint Bytecode::length(){
    return codeSegment.used;   
}
Instruction& Bytecode::get(uint index){
    Assert(index<codeSegment.used);
    return *((Instruction*)codeSegment.data + index);
}
bool Bytecode::addUnresolved(Token& token, uint address){
    if(unresolveds.max == unresolveds.used){
        if(!unresolveds.resize(unresolveds.max*2 + 10))
            return false;   
    }
    Unresolved* ptr = ((Unresolved*)unresolveds.data+unresolveds.used);
    unresolveds.used++;
    new(ptr)Unresolved();
    ptr->name = token;
    ptr->address = address;
    return true;
}
Bytecode::Unresolved* Bytecode::getUnresolved(uint address){
    for(int i=0;i<unresolveds.used;i++) {
        Unresolved* ptr = ((Unresolved*)unresolveds.data+i);
        if(ptr->address == address)
            return ptr;
    }
    return 0;
}
Bytecode::Unresolved* Bytecode::getUnresolved(const std::string& str){
     for(int i=0;i<unresolveds.used;i++) {
        Unresolved* ptr = ((Unresolved*)unresolveds.data+i);
        if(ptr->name == str)
            return ptr;
    }
    return 0;
}
bool Bytecode::linkUnresolved(const std::string& name, APICall funcPtr){
    Unresolved* ptr = getUnresolved(name);
    if(!ptr)
        return false;
    ptr->func = funcPtr;
    return true;
}
int IsInteger(Token& token){
    for(int i=0;i<token.length;i++){
        char chr = token.str[i];
        if(chr<'0'||chr>'9'){
            return false;
        }
    }
    return true;
}
bool IsName(Token& token){
    for(int i=0;i<token.length;i++){
        char chr = token.str[i];
        if(i==0)
            if(!((chr>='A'&&chr<='Z')||
            (chr>='a'&&chr<='z')||chr=='_'))
                return false;
        else
            if(!((chr>='A'&&chr<='Z')||
            (chr>='a'&&chr<='z')||
            (chr>='0'&&chr<='9')||chr=='_'))
                return false;
    }
    return true;
}
// Can also be an integer
int IsDecimal(Token& token){
    int hasDot=false;
    for(int i=0;i<token.length;i++){
        char chr = token.str[i];
        if(hasDot && chr=='.')
            return false; // cannot have 2 dots
        if(chr=='.')
            hasDot = true;
        else if(chr<'0'||chr>'9')
            return false;
    }
    return true;
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
            info.code.add(BC_NUM,1);
            info.code.add(BC_LOAD,1,dataIndex);
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

    // info.code.add(BC_MAKE_NUMBER,0);
    // info.code.add(BC_MAKE_NUMBER,1);
    // info.code.add(BC_MAKE_NUMBER,2);
    
    // int a = info.code.addConstNumber(5);
    // int b = info.code.addConstNumber(9);
    
    // info.code.add(BC_LOAD_CONST,0,a);
    // info.code.add(BC_LOAD_CONST,1,b);
    
    // info.code.add(BC_ADD,0,1,2);
    // info.code.add(BC_ADD,2,1,2);
    
    // Todo: Cleanup memory in GenerationInfo

    return info.code;
}
#define ARG_REG 1
#define ARG_CONST 2
#define ARG_NUMBER 4
#define ARG_STRING 8
// regIndex: index of the register in the instruction
bool CompileInstructionArg(GenerationInfo& info, int instType, int& num, int flags, uint8 regIndex){
    using namespace engone;
    Assert(("CompileArg... flags must be ARG_REG or so ...",flags!=0));
    
    Token prev = info.prev(); // make sure prev exists
    if(prev.flags&TOKEN_SUFFIX_LINE_FEED){
        ERRTL(prev) << "Expected arguments\n";
        return false;
    }
    if(0==(prev.flags&TOKEN_SUFFIX_SPACE)){
        ERRTL(prev) << "Expected a space after "<< prev <<"\n";
        return false;
    }
    CHECK_ENDR;
    Token token = info.next();
    if(flags&ARG_REG){
        if(token=="$"){
            if(token.flags&TOKEN_SUFFIX_LINE_FEED){
                ERRTOKL <<"Expected a register ($integer or $ab)\n";
                return false;
            } else if(token.flags&TOKEN_SUFFIX_SPACE){
                ERRTOKL << "Expected a register without space ($integer or $ab)\n";
                return false;
            }
            CHECK_ENDR
            token = info.next();
            
            if(IsInteger(token)){
                num = ConvertInteger(token);
                return true;
            }
            if(token.length==1){
                char s = *token.str;
                if(*token.str>='a'&&*token.str<='z')
                    num = s-'a' + 10;
                else if(*token.str>='A'&&*token.str<='Z')
                    num = s-'A' + 10;
                else{
                    ERRTOK <<"Expected $integer or $ab\n";
                    return false;
                }
            }else if(token.length==2){
                char s0 = *token.str;
                char s1 = *(token.str+1);
                if(token == DEFAULT_REG_RETURN_ADDR_S)
                    num = DEFAULT_REG_RETURN_ADDR;
                else if(token == DEFAULT_REG_RETURN_VALUE_S)
                    num = DEFAULT_REG_RETURN_VALUE;
                else if(token == DEFAULT_REG_ARGUMENT_S)
                    num = DEFAULT_REG_ARGUMENT;
                else if(s0>='a'&&s0<='z' && s1>='a'&&s1<='z')
                    num = s1-'a' + ('z'-'a')*(s0-'a') + 10;
                else if(s0>='A'&&s0<='Z' && s1>='A'&&s1<='Z')
                    num = s1-'A' + ('Z'-'A')*(s0-'A') + 10;
                else{
                    ERRTOK << "Expected $integer or $ab\n";
                    return false;
                }
            }else{
                ERRTOK  <<"Expected $integer or $ab\n";
                return false;
            }
            return true;
        }
    }
    if(flags&ARG_CONST){
        if((token.flags&TOKEN_QUOTED)==0&&IsName(token)){
            if(instType!=BC_LOAD){
                // resolve other instruction like jump by using load const
                num = DEFAULT_REG_TEMP0 + regIndex;
                info.code.add(BC_LOAD,num);
                int index=0;
                info.code.add(*(Instruction*)&index);
                info.instructionsToResolve.push_back({info.code.length()-1,token});
                log::out << info.code.get(info.code.length()-1) << "\n";
            }else{
                // resolve bc_load_const directly
                // num = DEFAULT_REG_TEMP0 + regIndex;
                info.instructionsToResolve.push_back({info.code.length()+1,token});
            }
            return true;
        }
    }
    if(flags&ARG_NUMBER){
        if(IsDecimal(token)){
            double number = ConvertDecimal(token);
            uint index = info.code.addConstNumber(number);
            num = DEFAULT_REG_TEMP0 + regIndex;
            info.code.add(BC_NUM,num);
            log::out << info.code.get(info.code.length()-1) << "\n";
            info.code.add(BC_LOAD,num);
            log::out << info.code.get(info.code.length()-1) << "\n";
            info.code.add(*(Instruction*)&index); // address/index
            return true;
        }
    }
    if(flags&ARG_STRING){
        if(token.flags&TOKEN_QUOTED){
            uint index = info.code.addConstString(token);
            num = DEFAULT_REG_TEMP0 + regIndex;
            info.code.add(BC_STR,num);
            log::out << info.code.get(info.code.length()-1) << "\n";
            info.code.add(BC_LOAD,num);
            log::out << info.code.get(info.code.length()-1) << "\n";
            info.code.add(*(Instruction*)&index); // address/index
            return true;
        }
    }
    ERRTOK <<"Expected ";
    if(flags&ARG_REG){
        log::out <<"register (eg. $23, $d)";
    }
    if(flags&ARG_CONST){
        if(flags&ARG_REG)
            if((flags&ARG_NUMBER)||(flags&ARG_STRING))
                log::out << ", ";
            else
                log::out << " or ";
        log::out <<"constant";
    }
    if(flags&ARG_NUMBER){
        if((flags&ARG_REG)||(flags&ARG_CONST))
            if((flags&ARG_NUMBER)||(flags&ARG_STRING))
                log::out << ", ";
            else
                log::out << " or ";
        log::out <<"decimal";
    }
    if(flags&ARG_STRING){
        if((flags&ARG_REG)||(flags&ARG_CONST)||(flags&ARG_NUMBER))
            log::out << " or ";
        log::out <<"quotes";
    }
    log::out << "\n";
    return false;
}
bool LineEndError(GenerationInfo& info){
    using namespace engone;
    Token prev = info.prev();
    if(0==(prev.flags&TOKEN_SUFFIX_LINE_FEED) && !info.end()){
        ERRTL(prev) << "Expected line feed found "<<info.now()<<"\n";
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
    MAP("mul",BC_MUL)
    MAP("div",BC_DIV)
    MAP("less",BC_LESS)
    MAP("greater",BC_GREATER)
    MAP("equal",BC_EQUAL)
    
    MAP("jumpif",BC_JUMPIF)

    MAP("mov",BC_MOV)
    MAP("copy",BC_COPY)
    MAP("run",BC_RUN)

    MAP("num",BC_NUM)
    MAP("str",BC_STR)
    MAP("del",BC_DEL)
    MAP("jump",BC_JUMP)
    MAP("load",BC_LOAD)
    MAP("return",BC_RETURN)
    MAP("enter",BC_ENTERSCOPE)
    MAP("exit",BC_EXITSCOPE)

    GenerationInfo info{};
    info.tokens = tokens;

    while(!info.end()){
        Token token = info.next();
        
        if(token.flags&TOKEN_SUFFIX_LINE_FEED){
            ERRTOK << "Expected something after " << token << "\n";
            info.nextLine();
            continue;
        }

        // first we expect an adress or instruction name
        Token name = token;
    
        CHECK_END
        token = info.next();

        if(token == ":"){
            // Jump address or constant
            if((token.flags & TOKEN_SUFFIX_LINE_FEED)||info.end()){
                // Jump adress
                uint address = info.code.length();
                int index = info.code.addConstNumber(address);
                info.nameOfNumberMap[name] = index;
                log::out<<"Address " << name <<" = "<<address<<"\n";
            }else{
                CHECK_END
                token = info.next();
                if(0==(token.flags&TOKEN_SUFFIX_LINE_FEED)){
                    ERRTOK <<"Expected line feed after "<<token<<"\n";
                    info.nextLine();
                    continue;
                }
                if(IsDecimal(token)){
                    double number = ConvertDecimal(token);
                    int index = info.code.addConstNumber(number);
                    info.nameOfNumberMap[name] = index;
                    log::out<<"Constant " << name<<" = "<<number<<" to "<<index<<"\n";
                }else if(token.flags&TOKEN_QUOTED){
                    int index = info.code.addConstString(token);
                    info.nameOfStringMap[name] = index;
                    log::out<<"Constant " << name<<" = "<<token<<" to "<<index<<"\n";
                }else {
                    ERRTOK << " " <<token<<" cannot be a constant\n";
                    info.nextLine();
                    continue;   
                }
            }
        } else if(0==(name.flags & TOKEN_SUFFIX_SPACE)){
            ERRTL(name) <<"Expected a space\n";
            info.nextLine();
            continue;
        } else {
            // Instruction confirmed
            auto find = instructionMap.find(name);
            if(find==instructionMap.end()){
                ERRTOK <<"Invalid instruction " << name<<"\n";
                info.nextLine();
                continue;
            }
            int instType = find->second;
            info.index--;
            
            int regCount = ((instType & BC_MASK) >> 6);
            
            Instruction inst{};
            inst.type = instType;
            
            bool error=false;
            int tmp=0;
            for(int i=0;i<regCount;i++){
                // if(i==2 && instType==BC_JUMPIF){
                //     if(!CompileInstructionArg(info,tmp,ARG_CONST_NUMBER|ARG_REG)){
                //         info.nextLine();
                //         continue;
                //     }
                // } else 
                if(!CompileInstructionArg(info,instType,tmp,ARG_REG|ARG_CONST|ARG_NUMBER|ARG_STRING,i)) {
                    error=true;
                    break;
                }
                inst.regs[i] = tmp;
            }
            if(error){
                info.nextLine();
                continue;
            }
            
            if(instType == BC_LOAD){
                if(!CompileInstructionArg(info,instType,tmp,ARG_CONST,1)) {
                    info.nextLine();
                    continue;
                }
            }
            
            info.code.add(inst);
            log::out << info.code.get(info.code.length()-1)<<("\n");
            if(instType==BC_LOAD){
                int zeroInst=0;
                info.code.add(*(Instruction*)&zeroInst);
            }
            if(LineEndError(info))
                continue;
                    
            // if((instType & BC_MASK) == BC_R3){
            //     int regs[3]{0};
            //     bool error=false;
            //     for(int i=0;i<3;i++){
            //         if(i==2 && instType==BC_JUMPIF){
            //             if(!CompileInstructionArg(info,regs[i],ARG_CONST_NUMBER|ARG_REG)){
            //                 info.nextLine();
            //                 continue;
            //             }
            //         } else if(!CompileInstructionArg(info,regs[i],ARG_REG)) {
            //             error=true;
            //             break;
            //         }
            //     }
            //     if(error){
            //         info.nextLine();
            //         continue;
            //     }

            //     info.code.add(instType,regs[0],regs[1],regs[2]);
            //     log::out << info.code.get(info.code.length()-1)<<("\n");

            //     if(LineEndError(info))
            //         continue;
            // } else if((instType & BC_MASK) == BC_R2){
            //     int reg=0;
            //     int var=0;

            //     if(!CompileInstructionArg(info,reg,ARG_REG)){
            //         info.nextLine();
            //         continue;
            //     }
                
            //     if(instType==BC_RUN){
            //         if(!CompileInstructionArg(info,var,ARG_REG|ARG_CONST_NUMBER)){
            //             info.nextLine();
            //             continue;
            //         }
            //     }else{
            //         if(!CompileInstructionArg(info,var,ARG_REG)){
            //             info.nextLine();
            //             continue;
            //         }
            //     }
                
            //     info.code.add(instType,reg,var);
            //     info.code.get(info.code.length()-1).print();log::out<<("\n");
                
            //     if(LineEndError(info))
            //         continue;
            // } else if((instType & BC_MASK) == BC_R1){
            //     int reg=0;

            //     if(instType == BC_JUMP){
            //         if(!CompileInstructionArg(info,reg,ARG_CONST_NUMBER|ARG_REG)){
            //             info.nextLine();
            //             continue;
            //         }
            //     }else if(instType == BC_LOAD_CONST){
            //         if(!CompileInstructionArg(info,reg,ARG_CONST_NUMBER)){
            //             info.nextLine();
            //             continue;
            //         }
            //     }else{
            //         if(!CompileInstructionArg(info,reg,ARG_REG)){
            //             info.nextLine();
            //             continue;
            //         }
            //     }

            //     info.code.add(instType,reg);
            //     log::out << info.code.get(info.code.length()-1) << ("\n");

            //     if(LineEndError(info))
            //         continue;
            // }
        }
    }

    for(auto& addr : info.instructionsToResolve){
        Instruction& inst = info.code.get(addr.instIndex);
        auto find = info.nameOfNumberMap.find(addr.token);
        int index = -1;
        if(find==info.nameOfNumberMap.end()){
            auto find2 = info.nameOfStringMap.find(addr.token);
            if(find2==info.nameOfStringMap.end()){
                // Bytecode::Unresolved* ptr = info.code.getUnresolved(addr.token);
                // if(ptr){
                //     index = ptr->address;
                // }else{
                //     index = info.code.nextUnresolvedAddress++;
                //     info.code.addUnresolved(addr.token,index);
                //     log::out << "Unresolved "<<addr.token << " (first found at "<<addr.token.line<<":"<<addr.token.column<<")\n";
                // }
                log::out << "CompileError at "<<addr.token.line<<":"<<addr.token.column<<" : Constant "<<addr.token<<" could not be resolved\n";
            }else{
                index = find2->second;
            }
        }else{
            index = find->second;
        }
        inst = *(Instruction*)&index;
        // if((inst.type&BC_MASK)==BC_R1){
        //     inst.reg0 = index&0xFF;
        //     inst.reg12 = index>>8;
        //     if(inst.singleReg()!=index){
        //         log::out << "CompileWarning "<<addr.token.line<<":"<<addr.token.column<<", index/address did not fit in 24 bits, expect bugs!\n";
        //     }
        // } else if((inst.type&BC_MASK)==BC_R2){
        //     inst.reg12 = index;
        //     if(inst.reg12!=index){
        //         log::out << "CompileWarning "<<addr.token.line<<":"<<addr.token.column<<", index/address did not fit in 16 bits, expect bugs!\n";
        //     }
        // } else {
        //     log::out << "CompileError at "<<addr.token.line<<":"<<addr.token.column<<" : Instruction must be of type R1 or R2 to resolve constants\n";
        // }
    }

    return info.code;
}

std::string Disassemble(Bytecode code){
    using namespace engone;
    // engone::Memory buffer{1};
    std::string buffer="";
    
    // #define CHECK_MEMORY if(!buffer.resize(buffer.max*2+1000)) {log::out << "Failed allocation\n"; return {};}
    
    for(int i=0;i<code.constNumbers.used;i++){
        Number* num = code.getConstNumber(i);
        buffer += "num_";
        buffer += std::to_string(i);
        buffer += ": ";
        buffer += std::to_string(num->value);
        buffer += "\n";
    }
    for(int i=0;i<code.constStrings.used;i++){
        String* num = code.getConstString(i);
        buffer += "str_";
        buffer += std::to_string(i);
        buffer += ": \"";
        buffer += *num; // careful with new line?
        buffer += "\"\n";
    }
    
    #define REF_NUM 1
    #define REF_STR 2
    int refTypes[256]{0};
    
    int programCounter = 0;
    while(true){
        if(programCounter>=code.length())
            break;
        Instruction& inst = code.get(programCounter++);
        
        // necessary to know which register are of what type
        // that info is needed so load knows which constant it should load
        if(inst.type==BC_NUM){
            refTypes[inst.reg0] = REF_NUM;   
        }else if(inst.type==BC_STR){
            refTypes[inst.reg0] = REF_STR;
        }else if(inst.type==BC_MOV){
            refTypes[inst.reg1] = refTypes[inst.reg0];
        }else if(inst.type==BC_COPY){
            refTypes[inst.reg1] = refTypes[inst.reg0];
        }else if(inst.type==BC_RETURN){
            // refTypes[DEFAULT_REG_RETURN_VALUE] = refTypes[inst.];
        }else if(inst.type==BC_RUN){
            
        }
        
        const char* name = InstToString(inst.type); // Todo: load, add instead of BC_LOAD, BC_ADD
        buffer += "    ";
        buffer += name;
        int regCount = (inst.type&BC_MASK)>>6;
        for(int i=0;i<regCount;i++){
            buffer += " ";
            buffer += "$";
            buffer += std::to_string(inst.regs[i]);
        }
        
        if(inst.type==BC_LOAD){
            uint extraData = *(uint*)&code.get(programCounter++);
            if(refTypes[inst.reg0]==REF_STR)
                buffer += " str_";
            else
                buffer += " num_";
            buffer+=std::to_string(extraData);
        }
        buffer +="\n";
    }   
    return buffer; 
}