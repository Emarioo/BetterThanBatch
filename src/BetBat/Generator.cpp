#include "BetBat/Generator.h"

#define ERRAT(L,C) engone::log::out <<log::RED<< "CompileError "<<(L)<<":"<<(C)<<", "
#define ERRT(T) ERRAT(T.line,T.column)
#define ERRTL(T) ERRAT(T.line,T.column+T.length)
#define ERRTOK ERRAT(token.line,token.column)
#define ERRTOKL ERRAT(token.line,token.column+token.length)

#define INST engone::log::out << (info.code.get(info.code.length()-2).type==BC_LOADC ? info.code.get(info.code.length()-2) : info.code.get(info.code.length()-1)) << ", "

#define ERRLINE engone::log::out <<log::RED<<" [Line "<<info.now().line<<"] ";info.printLine();engone::log::out<<"\n";

#define CHECK_END if(info.end()){\
        engone::log::out <<log::RED<<  "CompileError: Unexpected end of file\n";\
        break;\
    }
#define CHECK_ENDR if(info.end()){\
        engone::log::out <<log::RED<<  "CompileError: Unexpected end of file\n";\
        return false;\
    }

#ifdef GLOG
#define _GLOG(x) x;
#else
#define _GLOG(x) ;
#endif

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
        INSTCASE(BC_LOADV)

        INSTCASE(BC_NUM)
        INSTCASE(BC_STR)
        INSTCASE(BC_DEL)
        INSTCASE(BC_JUMP)
        INSTCASE(BC_LOADC)
        INSTCASE(BC_PUSH)
        INSTCASE(BC_POP)
        INSTCASE(BC_RETURN)
        INSTCASE(BC_ENTERSCOPE)
        INSTCASE(BC_EXITSCOPE)
    }
    return "BC_?";
}

void Instruction::print(){
    using namespace engone;
    if((type&BC_MASK) == BC_R1){
        log::out <<" "<< InstToString(type);
        if(reg1==0&&reg2==0){
            log::out << " $"<<reg0;
        }else{
            log::out << " "<<((uint)reg0|((uint)reg1<<8)|((uint)reg2<<8));
        }
    }else if((type&BC_MASK) == BC_R2){
        log::out <<" "<< InstToString(type)<<" $" <<reg0;
        if(reg2!=0||type==BC_LOADV){
            log::out << " "<<((uint)reg1|((uint)reg2<<8));
        }else{
            log::out << " $"<<reg1;
        }
    // if((type&BC_MASK) == BC_R3)
    } else
        log::out<<" "<<InstToString(type)<<" $"<<reg0<<" $"<<reg1<<" $"<<reg2;
}
engone::Logger& operator<<(engone::Logger& logger, Instruction& instruction){
    instruction.print();
    return logger;
}
engone::Logger& operator<<(engone::Logger& logger, Bytecode::DebugLine& debugLine){
    logger << "LN:"<<debugLine.line<<" ";
    for(uint i=0;i<debugLine.length;i++){
        logger<<*(debugLine.str+i);
    }
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
    inst.reg1 = reg12&0xFF;
    inst.reg2 = reg12>>8;
    return add(inst);
}
bool Bytecode::add(uint8 type, uint reg012){
    Instruction inst{};
    inst.type = type;
    inst.reg0 = reg012&0xFF;
    inst.reg1 = (reg012>>8)&0xFF;
    inst.reg2 = (reg012>>16)&0xFF;
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
    if(constStringText.max < constStringText.used + token.length){
        if(!constStringText.resize(constStringText.max*2 + token.length*30))
            return -1;
    }
    int index = constStrings.used;
    String* str = ((String*)constStrings.data + index);
    new(str)String();
    
    // if(!str->memory.resize(token.length)){
    //     return -1;   
    // }
    // str->memory.used = token.length;

    str->memory.data = (char*)constStringText.data + constStringText.used;
    str->memory.max = str->memory.used = token.length;
    str->memory.m_typeSize=0;
    constStringText.used+=token.length;

    memcpy(str->memory.data,token.str,token.length);

    constStrings.used++;
    return index;
}
bool Bytecode::addLoadC(uint8 reg0, uint constIndex){
    if(!add(BC_LOADC,reg0)) return false;
    if(!add(*(Instruction*)&constIndex)) return false;
    return true;
}

void Bytecode::cleanup(){
    constNumbers.resize(0);
    constStrings.resize(0);
    codeSegment.resize(0);
    debugLines.resize(0);
    debugLineText.resize(0);
}
String* Bytecode::getConstString(uint index){
    if(index>=constStrings.used) return 0;
    return (String*)constStrings.data + index;
}
Bytecode::DebugLine* Bytecode::getDebugLine(uint instructionIndex, uint* latestIndex){
    uint index = 0;
    if(latestIndex)
        index = *latestIndex;
    DebugLine* line = 0;
    while(true){
        if(index>=debugLines.used)
            return 0;
        line = (DebugLine*)debugLines.data + index;
        index++;
        if(line->instructionIndex==instructionIndex)
            break;
    }
    if(latestIndex)
        *latestIndex = index;
    return line;
}
uint Bytecode::length(){
    return codeSegment.used;   
}
Instruction& Bytecode::get(uint index){
    Assert(index<codeSegment.used);
    return *((Instruction*)codeSegment.data + index);
}

uint32 Bytecode::getMemoryUsage(){
    uint32 sum = codeSegment.max*codeSegment.m_typeSize+
        constNumbers.max*constNumbers.m_typeSize+
        constStrings.max*constStrings.m_typeSize+
        constStringText.max*constStringText.m_typeSize+
        debugLines.max*debugLines.m_typeSize+
        debugLineText.max*debugLineText.m_typeSize;
    return sum;
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
        if(i==0){
            if(!((chr>='A'&&chr<='Z')||
            (chr>='a'&&chr<='z')||chr=='_'))
                return false;
        } else {
            if(!((chr>='A'&&chr<='Z')||
            (chr>='a'&&chr<='z')||
            (chr>='0'&&chr<='9')||chr=='_'))
                return false;
        }
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
// returns instruction type
// zero means no operation
int IsOperation(Token& token){
    if(token=="+") return BC_ADD;
    if(token=="-") return BC_SUB;
    if(token=="*") return BC_MUL;
    if(token=="/") return BC_DIV;
    return 0;
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
Token& GenerationInfo::next(){
    return tokens.get(index++);
}
Token& GenerationInfo::prev(){
    return tokens.get(index-2);
}
Token& GenerationInfo::now(){
    return tokens.get(index-1);
}
Token& GenerationInfo::get(uint _index){
    return tokens.get(_index);
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
void GenerationInfo::printLine(){
    using namespace engone;
    int startToken = at()-1;
    while(true){
        if(startToken<0){
            startToken = 0;
            break;
        }
        Token token = get(startToken);
        if(token.flags&TOKEN_SUFFIX_LINE_FEED){
            startToken++;
            break;
        }
        startToken--;
    }
    int endToken = startToken;
    while(true){
        if((uint)endToken>=tokens.length()){
            endToken = tokens.length()-1;
            break;
        }
        Token token = get(endToken);
        if(token.flags&TOKEN_SUFFIX_LINE_FEED){
            break;
        }
        endToken++;
    }
    for(int i=startToken;i<=endToken;i++){
        log::out << get(i);
    }
}
bool GenerationInfo::addDebugLine(uint tokenIndex){
    int startToken = tokenIndex-1;
    while(true){
        if(startToken<0){
            startToken = 0;
            break;
        }
        Token& token = get(startToken);
        if(token.flags&TOKEN_SUFFIX_LINE_FEED){
            startToken++;
            break;
        }
        startToken--;
    }
    int endToken = startToken;
    while(true){
        if((uint)endToken>=tokens.length()){
            endToken = tokens.length()-1;
            break;
        }
        Token& token = get(endToken);
        if(token.flags&TOKEN_SUFFIX_LINE_FEED){
            break;
        }
        endToken++;
    }
    int length=0;
    for(int i=startToken;i<=endToken;i++){
        Token& token = get(i);
        length+=token.length;
    }
    if(code.debugLineText.used+length>code.debugLineText.max){
        if(!code.debugLineText.resize(code.debugLineText.max*2+length*30)){
            return false;
        }
    }
    if(code.debugLines.used==code.debugLines.max){
        if(!code.debugLines.resize(code.debugLines.max*2+10)){
            return false;
        }
    }
    Bytecode::DebugLine* line = (Bytecode::DebugLine*)code.debugLines.data+code.debugLines.used;
    code.debugLines.used++;
    line->str = (char*)code.debugLineText.data+code.debugLineText.used;
    line->length = length;
    line->instructionIndex = code.length();
    line->line = get(tokenIndex).line;
    for(int i=startToken;i<=endToken;i++){
        Token& token = get(i);
        memcpy((char*)code.debugLineText.data + code.debugLineText.used,token.str,token.length);
        code.debugLineText.used+=token.length;
    }
    return true;
}
void GenerationInfo::nextLine(){
    while(!end()){
        Token nowT = now();
        if(nowT.flags&TOKEN_SUFFIX_LINE_FEED){
            break;
        }
        Token token = next();
        // if(token.flags&TOKEN_SUFFIX_LINE_FEED){
        //     break;
        // }
    }
}
int OpPriority(int op){
    if(op==BC_ADD||op==BC_SUB) return 1;
    if(op==BC_MUL||op==BC_DIV) return 2;
    return 0;
}
#define EVAL_FLAG_NO_DEL 1 
int EvaluateExpression(GenerationInfo& info, int accDepth){
    using namespace engone;

    int acc0 = REG_ACC0+accDepth;

    int regCount=0;
    int operations[5]{0};
    int opCount=0;

    bool assignment = false;
    Token variableName{};

    // Todo: values of tokens are stored and so when we actually use them and realize
    //  syntax errors we don't know the line and column from the token. The values don't hold it.
    //  easy fix, just store tokens instead of values but think about it later.

    // Todo: detect uneven paranthesis.
    // Todo: detect '29*(5+)' and '*(21 51)' as errors while '1+(-9)' is valid.
    // Todo: \n or ; should be seen as ending
    // Todo: use del on temporary values

    int returnFlags=0;

    while(!info.end()){
        Token token = info.next();

        int opType=0;
        if(IsDecimal(token)){
            int constIndex = info.code.addConstNumber(ConvertDecimal(token));
            info.code.add(BC_NUM,acc0+regCount);
            _GLOG(INST << "\n";)
            info.code.addLoadC(acc0+regCount,constIndex);
            INST << "constant "<<token<<"\n";
            regCount++;
        }else if((opType = IsOperation(token))){
            if(token=="-" && regCount==0){
                // info.code.add(BC_NUM,acc0+regCount);
                // INST << "\n";
                info.code.add(BC_COPY,REG_ZERO,acc0+regCount);
                INST << " constant 0\n";
                regCount++;
            }
            operations[opCount++] = opType;
        }else if(token=="("){
            if(info.at()>1){
                Token prev = info.prev();
                if(!IsOperation(prev)){
                    ERRTOK << "expected operation before paranthesis, tokens: "<< info.prev()<<" "<<token<<"\n";
                    ERRLINE
                    info.nextLine();
                    break;
                }
            }
            // Todo: don't do recurision. It might be easier than you think.
            //  increment and decrement regCount may suffice. myabe...
            EvaluateExpression(info,acc0+regCount-REG_ACC0);
            regCount++;
        }else if(token==")"){
            // handled later
        }else if(token=="="){
            if(assignment){
                ERRTOK << "cannot assign twice\n";
                ERRLINE
                info.nextLine();
                break;
            }
            if(regCount!=0){
                ERRTOK << "cannot assign non variable\n";
                ERRLINE
                info.nextLine();
                break;
            }
            if(!variableName.str){
                ERRTOK << "cannot assign to nothing\n";
                ERRLINE
                info.nextLine();
                break;
            }
            assignment=true;
            log::out << "Assignment\n";
        }else{
            auto find = info.variables.find(token);
            if(find==info.variables.end()){
                if(regCount==0 && !assignment){
                    variableName = token;
                    log::out << "varname "<<token<<"\n";
                }else{
                    ERRTOK << "undefined "<<token<<"\n";
                    ERRLINE;
                    info.nextLine();
                    break;
                }
            }else{
                info.code.add(BC_LOADV,acc0+regCount,find->second.frameIndex);
                INST << "get variable "<<find->second.frameIndex<<"\n";
                info.code.add(BC_COPY,acc0+regCount,acc0+regCount);
                INST << "\n";
                regCount++;
            }
        }
        // DON'T MOVE ENDING CALCULATION! When EvaluateExpression is called above
        //  we need to notify code below that we should finish things up 
        bool ending = token==")" || info.end() || (token.flags&TOKEN_SUFFIX_LINE_FEED);

        if(!(regCount>=opCount && regCount<opCount+2)){
            ERRTOK << "uneven numbers and operators, tokens: "<<info.prev()<<" "<<token<<"\n";
            ERRLINE
            info.nextLine();
            break;
        }

        while(regCount>1){
            if(opCount>=2&&!ending){
                int op1 = operations[opCount-2];
                int op2 = operations[opCount-1];
                if(OpPriority(op1)>=OpPriority(op2)){
                    operations[opCount-2] = op2;
                    opCount--;
                    info.code.add(op1,acc0+regCount-2,acc0+regCount-1,acc0+regCount-2);
                    INST<<"pre\n";
                    info.code.add(BC_DEL,acc0+regCount-1);
                    INST << "\n";
                    regCount--;
                }else{
                    log::out << "break\n";
                    break;
                }
            }else if(ending){
                int op = operations[opCount-1];
                opCount--;
                info.code.add(op,acc0+regCount-2,acc0+regCount-1,acc0+regCount-2);
                INST<<"post\n";
                info.code.add(BC_DEL,acc0+regCount-1);
                INST << "\n";
                regCount--;
            }else{
                break;
            }
        }
        if(ending){
            if(regCount>1){
                ERRTOK << "reg count must be 1 at end (was "<<regCount<<")";
                info.nextLine();
                break;
            }
            if(assignment){
                if(regCount==1){
                    if(variableName.str){
                        GenerationInfo::Variable& var = info.variables[variableName] = {};
                        var.frameIndex = info.frameOffsetIndex++;
                        
                        info.code.add(BC_PUSH, acc0+regCount-1);
                        INST << "push variable\n";
                        returnFlags |= EVAL_FLAG_NO_DEL;
                        // info.code.add(BC_ADD, REG_STACK_POINTER, REG_ONE, REG_STACK_POINTER);
                        INST << "stack increment\n";

                    } else {
                        ERRTOK << "variable token was null (should be impossible)\n";
                        ERRLINE
                        info.nextLine();
                        break;
                    }
                }else{
                    log::out << log::RED<<"oops? "<<token<<"\n";
                    // assign null?
                }
            }
            regCount = 0;
            if(opCount!=0){
                ERRTOK<<"opCount should have been zero before ending\n";
                opCount = 0;
            }
            variableName={};
            assignment=0;
        }
        if(token.flags&TOKEN_SUFFIX_LINE_FEED){
            if(info.tokens.length()>info.index){
                info.addDebugLine(info.index);
                log::out <<"\n   ---  "<<*((Bytecode::DebugLine*)info.code.debugLines.data+info.code.debugLines.used-1)<<"   ---  \n";
            }
        }
        if(ending){
            break;
        }
        if(token==")"){
            break;
        }
    }
    return returnFlags;
}
Bytecode GenerateScript(Tokens tokens){
    using namespace engone;
    log::out << "\n####  Compile Script  ####\n";
    
    GenerationInfo info{};
    info.tokens = tokens;
    
    info.code.add(BC_NUM,REG_ZERO);
    info.code.add(BC_NUM,REG_STACK_POINTER);
    info.code.add(BC_NUM,REG_FRAME_POINTER);

    int accDepth = 1;
    
    info.addDebugLine(0);
    log::out <<"\n   ---  "<<*((Bytecode::DebugLine*)info.code.debugLines.data+info.code.debugLines.used-1)<<"   ---  \n";
    while (!info.end()){
        int flags = EvaluateExpression(info,accDepth);
        if(0==(flags&EVAL_FLAG_NO_DEL)){
            info.code.add(BC_DEL,REG_ACC0+accDepth);
        }
    }

    for(auto& pair : info.variables){
        // log::out << "del "<<pair.first<<"\n";
        info.code.add(BC_LOADV,REG_ACC0,pair.second.frameIndex);
        info.code.add(BC_DEL,REG_ACC0);
    }

    info.code.add(BC_DEL,REG_ZERO);
    INST<<"\n";
    info.code.add(BC_DEL,REG_STACK_POINTER);
    INST<<"\n";
    info.code.add(BC_DEL,REG_FRAME_POINTER);
    INST<<"\n";

    log::out << "\n####  Bytecode Summay (total: "<<info.code.getMemoryUsage()<<" bytes)  ####\n";
    // Note: BC_LOADC counts as two instructions
    log::out <<" Instructions: "<<info.code.length()<<"\n";
    log::out <<" Numbers: "<<info.code.constNumbers.used<<"\n";
    log::out <<" Strings: "<<info.code.constStrings.used<<"\n";

    return info.code;
}
#define ARG_REG 1
#define ARG_CONST 2
#define ARG_NUMBER 4
#define ARG_STRING 8
// regIndex: index of the register in the instruction
bool GenInstructionArg(GenerationInfo& info, int instType, int& num, int flags, uint8 regIndex){
    using namespace engone;
    Assert(("CompileArg... flags must be ARG_REG or so ...",flags!=0));
    
    Token now = info.now(); // make sure prev exists
    if(now.flags&TOKEN_SUFFIX_LINE_FEED){
        ERRTL(now) << "Expected arguments\n";
        return false;
    }
    if(0==(now.flags&TOKEN_SUFFIX_SPACE)){
        ERRTL(now) << "Expected a space after "<< now <<"\n";
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
                if(token == REG_FRAME_POINTER_S)
                    num = REG_FRAME_POINTER;
                else if(token == REG_STACK_POINTER_S)
                    num = REG_STACK_POINTER;
                else if(token == REG_RETURN_ADDR_S)
                    num = REG_RETURN_ADDR;
                else if(token == REG_RETURN_VALUE_S)
                    num = REG_RETURN_VALUE;
                else if(token == REG_ARGUMENT_S)
                    num = REG_ARGUMENT;
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
            if(instType!=BC_LOADC){
                // resolve other instruction like jump by using load const
                num = REG_ACC0 + regIndex;
                info.code.add(BC_LOADC,num);
                int index=0;
                info.code.add(*(Instruction*)&index);
                info.instructionsToResolve.push_back({info.code.length()-1,token});
                log::out << info.code.get(info.code.length()-2) << " (resolve "<<token<<")\n";
            }else{
                // resolve bc_load_const directly
                // num = REG_TEMP0 + regIndex;
                info.instructionsToResolve.push_back({info.code.length()+1,token});
            }
            return true;
        }
    }
    if(flags&ARG_NUMBER){
        if(IsDecimal(token)){
            double number = ConvertDecimal(token);
            uint index = info.code.addConstNumber(number);
            log::out<<"Constant ? = "<<number<<" to "<<index<<"\n";
            num = REG_ACC0 + regIndex;
            info.code.add(BC_NUM,num);
            log::out << info.code.get(info.code.length()-1) << "\n";
            info.code.add(BC_LOADC,num);
            log::out << info.code.get(info.code.length()-1) << "\n";
            info.code.add(*(Instruction*)&index); // address/index
            return true;
        }
    }
    if(flags&ARG_STRING){
        if(token.flags&TOKEN_QUOTED){
            uint index = info.code.addConstString(token);
            log::out<<"Constant ? = "<<token<<" to "<<index<<"\n";
            num = REG_ACC0 + regIndex;
            info.code.add(BC_STR,num);
            log::out << info.code.get(info.code.length()-1) << "\n";
            info.code.add(BC_LOADC,num);
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
        if(flags&ARG_REG){
            if((flags&ARG_NUMBER)||(flags&ARG_STRING))
                log::out << ", ";
            else
                log::out << " or ";
        }
        log::out <<"constant";
    }
    if(flags&ARG_NUMBER){
        if((flags&ARG_REG)||(flags&ARG_CONST)){
            if((flags&ARG_NUMBER)||(flags&ARG_STRING))
                log::out << ", ";
            else
                log::out << " or ";
        }
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
    Token now = info.now();
    if(0==(now.flags&TOKEN_SUFFIX_LINE_FEED) && !info.end()){
        ERRTL(now) << "Expected line feed found "<<info.now()<<"\n";
        info.nextLine();
        return true;
    }
    return false;
}
Bytecode GenerateInstructions(Tokens tokens){
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
    MAP("loadv",BC_LOADV)

    MAP("num",BC_NUM)
    MAP("str",BC_STR)
    MAP("del",BC_DEL)
    MAP("jump",BC_JUMP)
    MAP("loadc",BC_LOADC)
    MAP("push",BC_PUSH)
    MAP("pop",BC_POP)
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
                if(!GenInstructionArg(info,instType,tmp,ARG_REG|ARG_CONST|ARG_NUMBER|ARG_STRING,i)) {
                    error=true;
                    break;
                }
                inst.regs[i] = tmp;
            }
            if(error){
                info.nextLine();
                continue;
            }
            
            if(instType == BC_LOADC){
                if(!GenInstructionArg(info,instType,tmp,ARG_CONST,1)) {
                    info.nextLine();
                    continue;
                }
            }
            
            info.code.add(inst);
            log::out << info.code.get(info.code.length()-1)<<("\n");
            if(instType==BC_LOADC){
                int zeroInst=0;
                info.code.add(*(Instruction*)&zeroInst);
            }
            if(LineEndError(info))
                continue;
        }
    }

    for(auto& addr : info.instructionsToResolve){
        Instruction& inst = info.code.get(addr.instIndex);
        auto find = info.nameOfNumberMap.find(addr.token);
        int index = -1;
        if(find==info.nameOfNumberMap.end()){
            auto find2 = info.nameOfStringMap.find(addr.token);
            if(find2==info.nameOfStringMap.end()){
                log::out << "CompileError at "<<addr.token.line<<":"<<addr.token.column<<" : Constant "<<addr.token<<" could not be resolved\n";
            }else{
                index = find2->second;
            }
        }else{
            index = find->second;
        }
        inst = *(Instruction*)&index;
    }

    return info.code;
}

std::string Disassemble(Bytecode code){
    using namespace engone;
    // engone::Memory buffer{1};
    std::string buffer="";
    
    // #define CHECK_MEMORY if(!buffer.resize(buffer.max*2+1000)) {log::out << "Failed allocation\n"; return {};}
    
    for(uint i=0;i<code.constNumbers.used;i++){
        Number* num = code.getConstNumber(i);
        buffer += "num_";
        buffer += std::to_string(i);
        buffer += ": ";
        buffer += std::to_string(num->value);
        buffer += "\n";
    }
    for(uint i=0;i<code.constStrings.used;i++){
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
    
    uint programCounter = 0;
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
            // refTypes[REG_RETURN_VALUE] = refTypes[inst.];
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
        
        if(inst.type==BC_LOADC){
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