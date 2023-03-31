#include "BetBat/Generator.h"

#define ERRAT(L,C) engone::log::out <<engone::log::RED<< "CompileError "<<(L)<<":"<<(C)<<", "
#define ERRT(T) ERRAT(T.line,T.column)
#define ERRTL(T) ERRAT(T.line,T.column+T.length)
#define ERRTOK ERRAT(token.line,token.column)
#define ERRTOKL ERRAT(token.line,token.column+token.length)

#define ERR_GENERIC ERRTOK << "unexpected "<<token<<" "<<__FUNCTION__<<"\n"

#define INST engone::log::out << (info.code.get(info.code.length()-1)) << ", "
// #define INST engone::log::out << (info.code.get(info.code.length()-2).type==BC_LOADC ? info.code.get(info.code.length()-2) : info.code.get(info.code.length()-1)) << ", "

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

// When you refuse to use C++ library
#define GEN_REG_NUM(A,B,X0,X1,X2,X3,X4,X5,X6,X7,X8,X9) #A #B #X0, #A #B #X1,#A #B #X2, #A #B #X3, #A #B #X4, #A #B #X5, #A #B #X6, #A #B #X7, #A #B #X8, #A #B #X9,
static const char* crazyMap[260]{
    GEN_REG_NUM( , ,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM( ,1,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM( ,2,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM( ,3,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM( ,4,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM( ,5,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM( ,6,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM( ,7,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM( ,8,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM( ,9,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(1,0,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(1,1,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(1,2,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(1,3,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(1,4,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(1,5,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(1,6,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(1,7,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(1,8,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(1,9,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(2,0,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(2,1,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(2,2,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(2,3,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(2,4,0,1,2,3,4,5,6,7,8,9)
    GEN_REG_NUM(2,5,0,1,2,3,4,5,6,7,8,9)
};

const char* RegToString(uint8 reg){
    switch(reg){
        case REG_RETURN_VALUE: return REG_RETURN_VALUE_S;
        case REG_ARGUMENT: return REG_ARGUMENT_S;
        case REG_FRAME_POINTER: return REG_FRAME_POINTER_S;
        case REG_STACK_POINTER: return REG_STACK_POINTER_S;
    }
    return crazyMap[reg];
    
    // static std::unordered_map<uint8,std::string> s_regNameMap;
    // auto find = s_regNameMap.find(reg);
    // if(find!=s_regNameMap.end()) return find->second.c_str();
    // std::string& tmp = s_regNameMap[reg]="a"+std::to_string(reg-REG_ACC0);
    // return tmp.c_str();
}
void Instruction::print(){
    using namespace engone;
    if(type==BC_LOADC){
        log::out <<" "<< InstToString(type) << " $"<<RegToString(reg0) << " "<<((uint)reg1|((uint)reg2<<8));
    } else if((type&BC_MASK) == BC_R1){
        log::out <<" "<< InstToString(type);
        if(reg1==0&&reg2==0){
            log::out << " $"<<RegToString(reg0);
        }else{
            log::out << " "<<((uint)reg0|((uint)reg1<<8)|((uint)reg2<<8));
        }
    }else if((type&BC_MASK) == BC_R2){
        log::out <<" "<< InstToString(type)<<" $" <<RegToString(reg0);
        if(reg2!=0||type==BC_LOADV){
            log::out << " "<<((uint)reg1|((uint)reg2<<8));
        }else{
            log::out << " $"<<RegToString(reg1);
        }
    // if((type&BC_MASK) == BC_R3)
    } else
        log::out<<" "<<InstToString(type)<<" $"<<RegToString(reg0)<<" $"<<RegToString(reg1)<<" $"<<RegToString(reg2);
}
engone::Logger& operator<<(engone::Logger& logger, Instruction& instruction){
    instruction.print();
    return logger;
}
engone::Logger& operator<<(engone::Logger& logger, Bytecode::DebugLine& debugLine){
    logger << engone::log::GRAY <<"Line: "<<debugLine.line<<"  ";
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
uint Bytecode::addConstNumber(Decimal number){
    if(constNumbers.max == constNumbers.used){
        if(!constNumbers.resize(constNumbers.max*2 + 100))
            return -1;
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
uint Bytecode::addConstString(Token& token, const char* padding){
    int padlen = 0;
    if(padding)
        padlen = strlen(padding);
    int length = token.length + padlen;
    if(constStrings.max == constStrings.used){
        if(!constStrings.resize(constStrings.max*2 + 100))
            return -1;
    }
    if(constStringText.max < constStringText.used + length){
        if(!constStringText.resize(constStringText.max*2 + length*30))
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
    str->memory.max = str->memory.used = length;
    str->memory.m_typeSize=0;
    constStringText.used+=length;

    memcpy(str->memory.data,token.str,token.length);
    if(padding&&padlen!=0)
        memcpy((char*)str->memory.data + token.length,padding,padlen);

    constStrings.used++;
    return index;
}
bool Bytecode::addLoadC(uint8 reg0, uint constIndex){
    if((constIndex>>16)!=0)
        engone::log::out << engone::log::RED<< "ByteCode::addLoadC, TO SMALL FOR 32 bit integer ("<<constIndex<<")\n";
    if(!add(BC_LOADC,reg0,constIndex))
        return false;
    // if(!add(*(Instruction*)&constIndex)) return false;
    return true;
}

void Bytecode::cleanup(){
    constNumbers.resize(0);
    constStrings.resize(0);
    constStringText.resize(0);
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
    if(token.flags&TOKEN_QUOTED) return false;
    if(token==".") return false;
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
    #ifndef USE_DEBUG_INFO
    return false;
    #endif
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
        if(token.flags&TOKEN_QUOTED)
            length += 2;
        if(token.flags&TOKEN_SUFFIX_SPACE)
            length += 1;
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
        
        if(token.flags&TOKEN_QUOTED)
            *((char*)code.debugLineText.data + code.debugLineText.used++) = '"';
        
        // memcpy((char*)code.debugLineText.data + code.debugLineText.used,token.str,token.length);
        // code.debugLineText.used+=token.length;
        
        for(int j=0;j<token.length;j++){
            char chr = *((char*)token.str+j);
            if(chr=='\n'){
                if(code.debugLineText.used+2>code.debugLineText.max){
                    if(!code.debugLineText.resize(code.debugLineText.max+10)){
                        return false;
                    }
                }
                *((char*)code.debugLineText.data + code.debugLineText.used++) = '\\';
                *((char*)code.debugLineText.data + code.debugLineText.used++) = 'n';
                line->length++; // extra for \n
            }else{
                *((char*)code.debugLineText.data + code.debugLineText.used++) = chr;
            }
        }
        if(token.flags&TOKEN_QUOTED)
            *((char*)code.debugLineText.data + code.debugLineText.used++) = '"';
        if(token.flags&TOKEN_SUFFIX_SPACE)
            *((char*)code.debugLineText.data + code.debugLineText.used++) = ' ';
    }
    return true;
}
void GenerationInfo::nextLine(){
    int extra=index==0;
    if(index==0){
        next();   
    }
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
// Concatenates current token with the next tokens not seperated by space, linefeed or such
// operators and special characters are seen as normal letters
Token CombineTokens(GenerationInfo& info){
    using namespace engone;
        
    Token outToken{};
    while(!info.end()) {
        Token token = info.next();
        // Todo: stop at ( # ) and such 
        if((token.flags&TOKEN_QUOTED) && !outToken.str){
            info.index--;
            break;
        }
        if(!outToken.str)
            outToken.str = token.str;
        outToken.length += token.length;
        if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED))
            break;
    }
    return outToken;
}

int ParseAssignment(GenerationInfo& info, ExpressionInfo& exprInfo);

int ParseExpression(GenerationInfo& info, ExpressionInfo& exprInfo){
    using namespace engone;
    
    log::out << "-- try ParseExpression\n";
    
    if(ParseAssignment(info,exprInfo)){
        log::out << "-- exit ParseExpression\n";
        return 1;   
    }
    if(info.end())
        return 0;
    
    while(true){
        Token token = info.next();
        
        int opType = 0;
        if(token == "null"){
            
        } else if(IsDecimal(token)){
            int constIndex = info.code.addConstNumber(ConvertDecimal(token));
            info.code.add(BC_NUM,exprInfo.acc0Reg+exprInfo.regCount);
            _GLOG(INST << "\n";)
            info.code.addLoadC(exprInfo.acc0Reg+exprInfo.regCount,constIndex);
            _GLOG(INST << "constant "<<token<<"\n";)
            exprInfo.regCount++;
        } else if(token.flags&TOKEN_QUOTED){
            int reg = exprInfo.acc0Reg + exprInfo.regCount;
            int constIndex = info.code.addConstString(token);
            info.code.add(BC_STR,reg);
            _GLOG(INST << "\n";)
            info.code.addLoadC(reg,constIndex);
            _GLOG(INST << "constant "<<token<<"\n";)
            exprInfo.regCount++;
        } else if(IsName(token)){
            // Todo: handle variable or function
            auto varPair = info.variables.find(token);
            auto funcPair = info.internalFunctions.find(token);
            if(varPair!=info.variables.end()){
                int reg = exprInfo.acc0Reg + exprInfo.regCount;
                info.code.add(BC_LOADV,reg,varPair->second.frameIndex);
                _GLOG(INST << "get variable "<<varPair->second.frameIndex<<"\n";)
                info.code.add(BC_COPY,reg,reg);
                _GLOG(INST << "\n";)
                exprInfo.regCount++;
            }else if(funcPair!=info.internalFunctions.end()){
                log::out << log::RED << "Func pair in expr. not implemented!\n";
                // int reg = exprInfo.acc0Reg + exprInfo.regCount;
                // info.code.add(BC_LOADV,reg,varPair->second.frameIndex);
                // _GLOG(INST << "get variable "<<varPair->second.frameIndex<<"\n";)
                // info.code.add(BC_COPY,reg,reg);
                // _GLOG(INST << "\n";)
                // exprInfo.regCount++;
            } else{
                if(exprInfo.regCount==0){
                    // Note: wrong parse functions was chosen OR syntax error but we don't handle that here
                    info.index--;
                }else{
                    ERR_GENERIC;
                    info.nextLine();
                }
                return 0;
            }
        } else if(token=="("){
            ExpressionInfo expr{};
            expr.acc0Reg = exprInfo.acc0Reg+exprInfo.regCount;
            log::out<<"Par ( \n";
            info.parDepth++;
            if(ParseExpression(info,expr)){
                exprInfo.regCount++;
                info.parDepth--;
                log::out<<"Par ) \n";
            }else{
                info.parDepth--;
                ERR_GENERIC;
                info.nextLine();
                return 0;   
            }
        } else if(token ==")"){
            
        }else if((opType = IsOperation(token))){
            exprInfo.operations[exprInfo.opCount++] = opType;
            _GLOG(log::out << "Operator "<<token<<"\n";)
        }else {
            if(exprInfo.regCount==0){
                // Note: wrong parse functions was chosen OR syntax error but we don't handle that here
                info.index--;
            } else{
                // Todo: fails on () should it?
                ERR_GENERIC;
                info.nextLine();
            }
            return 0;
        }
        if(!(exprInfo.regCount>=exprInfo.opCount && exprInfo.regCount<exprInfo.opCount+2)){
            // _GLOG(log::out << "OI\n";)
            // operations[opCount++] = BC_ADD;
            ERRTOK << "uneven numbers and operators, tokens: "<<info.prev()<<" "<<token<<"\n";
            ERRLINE
            info.nextLine();
            return 0;
        }
        bool ending = (token.flags&TOKEN_SUFFIX_LINE_FEED) || info.end() || token == ")";
        
        while(exprInfo.regCount>1){
            if(exprInfo.opCount>=2&&!ending){
                int op1 = exprInfo.operations[exprInfo.opCount-2];
                int op2 = exprInfo.operations[exprInfo.opCount-1];
                if(OpPriority(op1)>=OpPriority(op2)){
                    exprInfo.operations[exprInfo.opCount-2] = op2;
                    exprInfo.opCount--;
                    int reg0 = exprInfo.acc0Reg+exprInfo.regCount-2;
                    int reg1 = exprInfo.acc0Reg+exprInfo.regCount-1;
                    info.code.add(op1,exprInfo.acc0Reg+exprInfo.regCount-2,
                    exprInfo.acc0Reg+exprInfo.regCount-1,exprInfo.acc0Reg+exprInfo.regCount-2);
                    // info.code.add(op1,reg0,reg1,reg0);
                    _GLOG(INST<<"pre\n";)
                    info.code.add(BC_DEL,exprInfo.acc0Reg+exprInfo.regCount-1);
                    // info.code.add(BC_DEL,reg1);
                    
                    _GLOG(INST << "\n";)
                    exprInfo.regCount--;
                }else{
                    // _GLOG(log::out << "break\n";)
                    break;
                }
            }else if(ending){
                int op = exprInfo.operations[exprInfo.opCount-1];
                exprInfo.opCount--;
                info.code.add(op,exprInfo.acc0Reg+exprInfo.regCount-2,
                exprInfo.acc0Reg+exprInfo.regCount-1,exprInfo.acc0Reg+exprInfo.regCount-2);
                _GLOG(INST<<"post\n";)
                info.code.add(BC_DEL,exprInfo.acc0Reg+exprInfo.regCount-1);
                _GLOG(INST << "\n";)
                exprInfo.regCount--;
            }else{
                break;
            }
        }
        
        if(ending){
            log::out << "-- exit ParseExpression\n";
            if(exprInfo.regCount>1){
                log::out<<log::RED<<"CompilerWarning: exprInfo.regCount > 1 at end of expression";             
            }
            return 1;   
        }
    }
}
int ParseBody(GenerationInfo& info);
// returns 0 if syntax is wrong for flow parsing
int ParseFlow(GenerationInfo& info){
    using namespace engone;
    log::out << "-- try ParseFlow\n";
    
    if(info.end()){
        return 0;
    }
    Token token = info.next();
    
    if(token=="if"){
        ExpressionInfo expr{};
        expr.acc0Reg = REG_ACC0;
        ParseExpression(info,expr);
        
        // Todo: add jump instructions to resolve
        
        ParseBody(info);
        
        // Todo: add address to resolve?
        return 1;
    }else{
        // ERR_GENERIC;
        info.index--; // go back, token should not be parsed by this function
        return 0;   
    }
}
int ParseAssignment(GenerationInfo& info, ExpressionInfo& exprInfo){
    using namespace engone;
    
    log::out << "-- try ParseAssignment\n";
    
    if(info.tokens.length() < info.index+2){
        // not enough tokens for assignment
        return 0;
    }
    Token token = info.next();
    Token assign = info.next();
    if((IsName(token) && !(token.flags&TOKEN_QUOTED)) && assign=="="){
        // good
    }else{
        // ERR_GENERIC;
        info.index-=2;
        return 0;
    }
    
    // ExpressionInfo expr{};
    // expr.acc0Reg = REG_ACC0;
    
    if(ParseExpression(info,exprInfo)){
        int finalReg = exprInfo.acc0Reg + exprInfo.regCount-1;
        // Todo: properly deal with regCount==0
        Assert(exprInfo.regCount!=0);
        if(exprInfo.regCount==0){
            log::out << log::RED<<"CompilerWarning: regCount == 0 after expression from assignment\n";   
        }
                
        auto find = info.variables.find(token);
        if(find==info.variables.end()){
            // Note: assign new variable
            GenerationInfo::Variable& var = info.variables[token] = {};
            var.frameIndex = info.frameOffsetIndex++;
            
            info.code.add(BC_PUSH, finalReg);
            _GLOG(INST << "push "<<token<<"\n";)
            
            info.code.add(BC_COPY,finalReg,finalReg);
            _GLOG(INST << "prevent modifications\n";)
        }else{
            // Note: replace old variable
            
            int tempReg = exprInfo.acc0Reg + exprInfo.regCount;
            
            info.code.add(BC_LOADV,tempReg,find->second.frameIndex);
            _GLOG(INST << "load "<<token<<"\n";)
            info.code.add(BC_DEL,tempReg);
            _GLOG(INST << "del "<<token<<"\n";)
            
            GenerationInfo::Variable& var = find->second;
            
            // Todo: cache const number for frameIndex
            if(var.constIndex==-1){
                var.constIndex = info.code.addConstNumber(var.frameIndex);
            }
            
            // load frameIndex into stack pointer while storing current stack pointer
            info.code.add(BC_COPY,REG_STACK_POINTER,tempReg);
            _GLOG(INST << "keep stack pointer\n";)
            info.code.addLoadC(REG_STACK_POINTER,var.constIndex);
            _GLOG(INST << "\n";)
            
            info.code.add(BC_PUSH, finalReg);
            _GLOG(INST << "load new "<<token<<"\n";)
            
            // set stack pointer to earlier value
            info.code.add(BC_DEL,REG_STACK_POINTER);
            _GLOG(INST << "\n";)
            info.code.add(BC_MOV,tempReg,REG_STACK_POINTER);
            _GLOG(INST << "restore stack pointer\n";)
            
            info.code.add(BC_COPY,finalReg,finalReg);
            _GLOG(INST << "prevent modifications\n";)
        }
    }else{
        ERR_GENERIC;
        info.nextLine();
        return 0;
    }
    log::out << "-- exit Assignment\n";
    return 1;
}
int ParseCommand(GenerationInfo& info, ExpressionInfo& exprInfo){
    using namespace engone;
    log::out << "-- try ParseCommand\n";
    
    if(info.end()){
        return 0;
    }
    // Token token = info.next();
    
    Token token = CombineTokens(info);
    if(!token.str){
        ERRTOK << "token was null, probably bug in compilerl\n";
        info.nextLine();
        return 0;
    }
    
    if(token.flags&TOKEN_QUOTED){
        // quoted exe command
    }else if(IsName(token)){
        // internal function
    } else{
        // raw exe command
        //info.index--;
        //return 0;   
    }
    int nameReg = exprInfo.acc0Reg+exprInfo.regCount;
    exprInfo.regCount++;
    int index = info.code.addConstString(token);
    info.code.add(BC_STR,nameReg);
    _GLOG(INST << "name\n";)
    info.code.addLoadC(nameReg, index);
    _GLOG(INST << "constant "<<token<<"\n";)
    
    int argReg = 0;
    
    bool ending = (token.flags&TOKEN_SUFFIX_LINE_FEED) | info.end();
    if(!ending){
        
        int tempReg = exprInfo.acc0Reg+exprInfo.regCount+1;
        info.code.add(BC_STR,tempReg);
        _GLOG(INST << "temp1\n";)
        
        int tempRegN = exprInfo.acc0Reg+exprInfo.regCount+2;
        info.code.add(BC_NUM,tempRegN);
        _GLOG(INST << "tempN\n";)
        
        int tempReg2 = exprInfo.acc0Reg+exprInfo.regCount+3;
        // info.code.add(BC_STR,tempReg2);
        // _GLOG(INST << "temp2\n";)
        
        bool delTemp2=false;
        
        while(!info.end()){
            token = info.next();
            
            int whichArg=0;
            if(token=="("){
                // Todo: parse expression
                ExpressionInfo expr{};
                expr.acc0Reg = tempReg2;
                // log::out<<"Par ( \n";
                info.parDepth++;
                int success = ParseExpression(info,expr);
                if(success){
                    if(expr.regCount!=0){
                       whichArg=1;
                       delTemp2=true;
                    }
                    info.parDepth--;
                }else{
                    info.parDepth--;
                    ERR_GENERIC;
                    info.nextLine();
                    return 0;   
                }
            }else{
                // Todo: find variables
                if(!(token.flags&TOKEN_QUOTED)&&IsName(token)){
                    auto varPair = info.variables.find(token);
                    if(varPair!=info.variables.end()){
                        info.code.add(BC_LOADV,tempRegN,varPair->second.frameIndex);
                        _GLOG(INST << "get variable "<<varPair->second.frameIndex<<"\n";)
                        info.code.add(BC_COPY,tempRegN,tempRegN);
                        _GLOG(INST << "\n";)
                        
                        whichArg = 3;
                        goto CMD_ACC; // Note: careful when changing behaviour, goto can be wierd
                    }
                    // not variable? then continue below
                }
                
                // log::out << "NOW: "<<token<<"\n";
                info.index--;
                Token combinedTokens = CombineTokens(info);
                
                const char* padding=0;
                if(info.now().flags&TOKEN_SUFFIX_SPACE)
                    padding = " ";
                
                // log::out << "tok: "<<combinedTokens<<"\n";
                int constIndex = info.code.addConstString(combinedTokens,padding);
                // printf("eh %d\n",constIndex);
                info.code.addLoadC(tempReg, constIndex);
                _GLOG(INST << "constant "<<combinedTokens<<"\n";)
                whichArg = 2;
            }
        CMD_ACC:
            if(argReg==0&&whichArg!=0){
                argReg = exprInfo.acc0Reg+exprInfo.regCount;
                info.code.add(BC_STR,argReg);
                _GLOG(INST << "arg\n";)
            }
            if(whichArg==1){
                info.code.add(BC_ADD,argReg, tempReg2, argReg);
                _GLOG(INST << "\n";)
            }else if(whichArg==2){
                info.code.add(BC_ADD,argReg, tempReg, argReg);
                _GLOG(INST << "\n";)
            }else if(whichArg==3){
                info.code.add(BC_ADD,argReg, tempRegN, argReg);
                _GLOG(INST << "\n";)
            }
            token = info.now();
            if(token.flags&TOKEN_SUFFIX_LINE_FEED){
                break;
            }
        }
        info.code.add(BC_DEL,tempReg);
        _GLOG(INST << "temp\n";)
        info.code.add(BC_DEL,tempRegN);
        _GLOG(INST << "tempN\n";)
        if(delTemp2){
            info.code.add(BC_DEL,tempReg2);
            _GLOG(INST << "temp2\n";)
        }
    }
    
    info.code.add(BC_RUN,argReg,nameReg);
    _GLOG(INST << "run "<<token<<"\n";)
    
    info.code.add(BC_DEL,nameReg);
    _GLOG(INST << "name\n";)
    exprInfo.regCount--;
    
    if(argReg!=0){
        info.code.add(BC_DEL,argReg);
        _GLOG(INST << "arg\n";)
    }
    
    info.code.add(BC_MOV,REG_RETURN_VALUE,exprInfo.acc0Reg+exprInfo.regCount);
    _GLOG(INST << "\n";)
    exprInfo.regCount++;
    // Note: the parsed instructions add a new value in the accumulation
    //  Don't forget to use and delete it.
    
    log::out << "-- exit ParseCommand\n";
    return 1;
}
int ParseBody(GenerationInfo& info){
    using namespace engone;
    ExpressionInfo expr{};
    expr.acc0Reg = REG_ACC0;
    ExpressionInfo expr2{};
    expr2.acc0Reg = REG_ACC0;
    if (ParseFlow(info)) {
        
    } else if(ParseAssignment(info,expr)) { // Note: cannot be ParseExpression since it collides with ParseCommand
        info.code.add(BC_DEL,expr.acc0Reg);
        expr.regCount--;
    } else if(ParseCommand(info,expr2)) {
        info.code.add(BC_DEL,expr2.acc0Reg);
        expr2.regCount--;
    } else{
        if(!info.end()&&info.index>0){
            ERRT(info.now()) << "unexpected "<<info.now()<<"\n";
        }else {
            ERRT(info.get(0)) << "unexpected "<<info.get(0)<<"\n";
        }
        return 0;
    }
    
    return 1;
}

#define EVAL_FLAG_NO_DEL 1
#define EVAL_FLAG_ASSIGNED 2
int EvaluateExpression(GenerationInfo& info, int accDepth){
    using namespace engone;

    int acc0 = REG_ACC0+accDepth;

    int regCount=0;
    int operations[5]{0};
    int opCount=0;

    bool assignment = false;
    Token variableName{};
    Token functionName{};
    
    bool runDirective=false;
    if(info.parDepth==0)
        runDirective = true;

    // Todo: detect uneven paranthesis.
    // Todo: \n or ; should be seen as ending
    // Todo: use del on temporary values

    int returnFlags=0;

    while(!info.end()){
        // Don't turn token into a reference since the variable is assigned to in some cases.
        // with a reference, Tokens in info would be modified.
        Token token = info.next();

        int opType=0;
        
        if(token == "#"){
            if((token.flags&TOKEN_SUFFIX_SPACE)){
                ERRTOKL << "expected directive name after "<<token<<"\n";
                info.nextLine();
                info.errors++;
                continue;
            }
            token = info.next();
            if(token=="run"){
                runDirective = true;
            }else{
                ERRTOK << "unknown directive #"<<token<<"\n";
                info.nextLine();
                info.errors++;
                continue;
            }
            _GLOG(log::out << "directive. #"<<token<<"\n";)
            continue;
        } else if(IsDecimal(token)){
            if(regCount>opCount){
                // log::out << "deci r>o "<<regCount<<" > "<<opCount<<"\n";
                operations[opCount++] = BC_ADD;
                token = "+";
                info.index--;
            }else{
                int constIndex = info.code.addConstNumber(ConvertDecimal(token));
                info.code.add(BC_NUM,acc0+regCount);
                _GLOG(INST << "\n";)
                info.code.addLoadC(acc0+regCount,constIndex);
                _GLOG(INST << "constant "<<token<<"\n";)
                regCount++;
            }
        } else if((opType = IsOperation(token)) && !runDirective){ // operators only allowed without #run
            if(token=="-" && regCount==0){
                // info.code.add(BC_NUM,acc0+regCount);
                // INST << "\n";
                info.code.add(BC_COPY,REG_ZERO,acc0+regCount);
                _GLOG(INST << " constant 0\n";)
                regCount++;
            }
            operations[opCount++] = opType;
            log::out << "OP "<<opType<<" "<<opCount<<"\n";
        }else if(token=="("){
            if(regCount!=0){
                if(info.at()>1){
                    Token prev = info.prev();
                    if(!IsOperation(prev)){
                        ERRTOK << "expected operation before paranthesis, tokens: "<< info.prev()<<" "<<token<<"\n";
                        ERRLINE
                        info.nextLine();
                        info.errors++;
                        break;
                    }
                }
            }
            // Todo: don't do recurision. It might be easier than you think.
            //  increment and decrement regCount may suffice. myabe...
            log::out<<"Par ( \n";
            info.parDepth++;
            EvaluateExpression(info,acc0+regCount-REG_ACC0);
            info.parDepth--;
            token = info.now();
            log::out<<"Par ) \n";
            regCount++;
        }else if(token==")"){
            // handled later
        } else{
            auto varPair = info.variables.find(token);
            auto funcPair = info.internalFunctions.find(token);
            
            // Note: internal functions must be defined in info.internalFunctions to be recognized. DON'T FORGET!
            //  You can explicitly use the #run directive otherwise.
            
            // Todo: CHECK OUT OF BOUNDS!
            Token nextToken = info.get(info.at()+1);
            if(regCount==0&&nextToken == "="){
                Token nextToken2 = info.get(info.at()+2);
                if(!(nextToken.flags&TOKEN_SUFFIX_SPACE) && nextToken2 == "="){
                    info.index+=2;
                    operations[opCount++] = opType;
                    log::out << "OP "<<nextToken <<nextToken2<<" "<<opCount<<"\n";
                }else{
                    if(assignment){
                        ERRTOK << "cannot assign twice\n";
                        ERRLINE
                        info.nextLine();
                        info.errors++;
                        continue;
                    }
                    runDirective=false; // disable default #run
                    assignment=true;
                    info.index++; // skip = token
                    variableName = token;
                    returnFlags |= EVAL_FLAG_ASSIGNED;
                    _GLOG(log::out << "Assignment, var="<<variableName<<", func=null\n";)
                }
            } else if(runDirective){
                if(((token.flags&TOKEN_QUOTED) || IsName(token))&&!functionName.str){
                    // runDirective=false; // Note: we have to disable it so arguments don't use the directive
                    functionName = CombineTokens(info);
                    
                    _GLOG(log::out << "exe func. "<<functionName<<"\n";)
                    continue;
                }else if(funcPair != info.internalFunctions.end() && !functionName.str){
                    ERRTOK << "unexepected function variable "<<token<<"\n";
                    ERRLINE
                    info.nextLine();
                    info.errors++;
                    continue;
                } else if(varPair != info.variables.end()){
                    info.code.add(BC_LOADV,acc0+regCount,varPair->second.frameIndex);
                    _GLOG(INST << "get variable "<<varPair->second.frameIndex<<"\n";)
                    info.code.add(BC_COPY,acc0+regCount,acc0+regCount);
                    _GLOG(INST << "\n";)
                    regCount++;
                } else {
                     if(regCount>opCount){
                        _GLOG(log::out << "concat add\n");
                        // log::out << "str r>o "<<regCount<<" > "<<opCount<<"\n";
                        operations[opCount++] = BC_ADD;
                        token = "+";
                        info.index--;
                    }else{
                        // Note: we can use the token variable but by combining tokens now
                        //  we decrease the amount of instructions we do later. 
                        Token combinedToken = CombineTokens(info);
                        
                        int constIndex = -1;
                        if(info.now().flags&TOKEN_SUFFIX_SPACE)
                            constIndex = info.code.addConstString(combinedToken," ");
                        else
                            constIndex = info.code.addConstString(combinedToken,"");
                        info.code.add(BC_STR,acc0+regCount);
                        _GLOG(INST << "\n";)
                        info.code.addLoadC(acc0+regCount,constIndex);
                        _GLOG(INST << "constant "<<combinedToken<<"\n";)
                        regCount++;
                        
                        // int constIndex = info.code.addConstString(token);
                        // _GLOG(INST << "constant "<<token<<"\n";)
                    }
                }
            }else{
                if(token=="null"&&(token.flags&TOKEN_QUOTED)==0){
                    info.code.add(BC_MOV,REG_NULL,acc0+regCount);
                    _GLOG(INST << "\n";)
                    regCount++;
                }  else if(funcPair != info.internalFunctions.end() && !functionName.str){
                    functionName = token;
                    _GLOG(log::out << "internal func. "<<token<<"\n";)
                } else if(varPair != info.variables.end()){
                    info.code.add(BC_LOADV,acc0+regCount,varPair->second.frameIndex);
                    _GLOG(INST << "get variable "<<varPair->second.frameIndex<<"\n";)
                    info.code.add(BC_COPY,acc0+regCount,acc0+regCount);
                    _GLOG(INST << "\n";)
                    regCount++;
                } else if((!(token.flags&TOKEN_QUOTED)||IsName(token)) && !variableName.str){
                    variableName = token;
                    _GLOG(log::out << "variable. "<<token<<"\n";)
                } else if((token.flags&TOKEN_QUOTED) || IsName(token)){
                    if(regCount>opCount){
                        // log::out << "str r>o "<<regCount<<" > "<<opCount<<"\n";
                        operations[opCount++] = BC_ADD;
                        token = "+";
                        info.index--;
                    }else{
                        int constIndex = info.code.addConstString(token);
                        info.code.add(BC_STR,acc0+regCount);
                        _GLOG(INST << "\n";)
                        info.code.addLoadC(acc0+regCount,constIndex);
                        _GLOG(INST << "constant "<<token<<"\n";)
                        regCount++;
                    }
                } else {
                    Token prev = info.prev();
                    auto varPair = info.variables.find(prev);
                    auto funcPair = info.internalFunctions.find(prev);
                    
                    if(token == "."){
                        if(varPair!=info.variables.end()){
                            ERRTOK << "property not implemented "<<token<<"\n";
                            ERRLINE;
                            info.nextLine();
                            info.errors++;
                            continue;
                        } else if(funcPair!=info.internalFunctions.end()) {
                            ERRTOK << "property not implemented "<<token<<"\n";
                            ERRLINE;
                            info.nextLine();
                            info.errors++;
                            continue;
                        } else{
                            if(regCount>opCount){
                                // log::out << "str r>o "<<regCount<<" > "<<opCount<<"\n";
                                operations[opCount++] = BC_ADD;
                                token = "+";
                                info.index--;
                            }else{
                                int constIndex = info.code.addConstString(token);
                                info.code.add(BC_STR,acc0+regCount);
                                _GLOG(INST << "\n";)
                                info.code.addLoadC(acc0+regCount,constIndex);
                                _GLOG(INST << "constant "<<token<<"\n";)
                                regCount++;
                            }
                        }
                    }else{
                        ERRTOK << "undefined "<<token<<"\n";
                        ERRLINE;
                        info.nextLine();
                        info.errors++;
                        continue;
                    }
                }
            }
        }
        // DON'T MOVE ENDING CALCULATION! When EvaluateExpression is called above
        //  we need to notify code below that we should finish things up 
        bool ending = false;
        // if(token.str)
        ending = token==")" || info.end() || (token.flags&TOKEN_SUFFIX_LINE_FEED);

        if(!(regCount>=opCount && regCount<opCount+2)){
            _GLOG(log::out << "OI\n";)
            operations[opCount++] = BC_ADD;
            
            // ERRTOK << "uneven numbers and operators, tokens: "<<info.prev()<<" "<<token<<"\n";
            // ERRLINE
            // info.nextLine();
            // break;
        }

        while(regCount>1){
            if(opCount>=2&&!ending){
                int op1 = operations[opCount-2];
                int op2 = operations[opCount-1];
                if(OpPriority(op1)>=OpPriority(op2)){
                    operations[opCount-2] = op2;
                    opCount--;
                    int reg0 = acc0;
                    int reg1 = acc0+1;
                    info.code.add(op1,acc0+regCount-2,acc0+regCount-1,acc0+regCount-2);
                    // info.code.add(op1,reg0,reg1,reg0);
                    _GLOG(INST<<"pre\n";)
                    info.code.add(BC_DEL,acc0+regCount-1);
                    // info.code.add(BC_DEL,reg1);
                    
                    _GLOG(INST << "\n";)
                    regCount--;
                }else{
                    _GLOG(log::out << "break\n";)
                    break;
                }
            }else if(ending){
                int op = operations[opCount-1];
                opCount--;
                info.code.add(op,acc0+regCount-2,acc0+regCount-1,acc0+regCount-2);
                _GLOG(INST<<"post\n";)
                info.code.add(BC_DEL,acc0+regCount-1);
                _GLOG(INST << "\n";)
                regCount--;
            }else{
                break;
            }
        }
        if(ending){
            if(regCount>1){
                ERRTOK << "reg count must be 1 at end (was "<<regCount<<")";
                info.nextLine();
                info.errors++;
                break;
            }
            if(regCount==0){
                returnFlags |= EVAL_FLAG_NO_DEL;
            }
            if(functionName.str){
                // Todo: check if function is defined in bytecode.
                
                int index = info.code.addConstString(functionName);
                info.code.add(BC_STR,acc0+regCount);
                _GLOG(INST << "\n";)
                info.code.addLoadC(acc0+regCount,index);
                _GLOG(INST << "constant "<<functionName<<"\n";)
                int regArg = 0;
                if(regCount!=0)
                    regArg = acc0 + regCount-1;
                info.code.add(BC_RUN,regArg,acc0+regCount);
                _GLOG(INST << "run "<<functionName<<"\n";)
                info.code.add(BC_DEL,acc0+regCount);
                _GLOG(INST << "\n";)
                if(regArg!=-1){
                    info.code.add(BC_DEL,regArg);
                    _GLOG(INST << "\n";)
                    regCount--;   
                }
            }
            if(assignment){
                if(regCount==1||functionName.str){
                    if(variableName.str){
                        int tempReg = acc0+regCount-1;
                        if(functionName.str){
                            tempReg = REG_RETURN_VALUE;
                        }
                        
                        auto find = info.variables.find(variableName);
                        if(find!=info.variables.end()){
                            info.code.add(BC_LOADV,acc0+regCount,find->second.frameIndex);
                            _GLOG(INST << "load old"<<"\n";)
                            info.code.add(BC_DEL,acc0+regCount);
                            _GLOG(INST << "del old"<<"\n";)
                            
                            GenerationInfo::Variable& var = find->second;
                            
                            // Todo: cache const number for frameIndex
                            if(var.constIndex==-1){
                                var.constIndex = info.code.addConstNumber(var.frameIndex);
                            }
                            int tmpReg = acc0+regCount;
                            
                            // load frameIndex into stack pointer while storing current stack pointer
                            info.code.add(BC_COPY,REG_STACK_POINTER,tmpReg);
                            info.code.addLoadC(REG_STACK_POINTER,var.constIndex);
                            
                            info.code.add(BC_PUSH, tempReg);
                            
                            info.code.add(BC_DEL,REG_STACK_POINTER);
                            info.code.add(BC_MOV,tmpReg,REG_STACK_POINTER);
                            
                            // set stack pointer to earlier value
                            _GLOG(INST << "replace "<<variableName<<"\n";)
                            returnFlags |= EVAL_FLAG_NO_DEL;
                        }else{
                            GenerationInfo::Variable& var = info.variables[variableName] = {};
                            var.frameIndex = info.frameOffsetIndex++;
                            
                            info.code.add(BC_PUSH, tempReg);
                            _GLOG(INST << "push "<<variableName<<"\n";)
                        }
                        returnFlags |= EVAL_FLAG_NO_DEL;
                        
                        // Todo: what happens if we push a return value but the function didn't return anything?

                    } else {
                        ERRTOK << "IMPOSSIBLE: variable token was null\n";
                        ERRLINE
                        info.errors++;
                        // break;
                    }
                }else{
                    ERRTOK << log::RED<<"IMPOSSIBLE: bad assignment? regCount: "<<regCount<<"\n";
                    ERRLINE
                    info.errors++;
                }
            }else if(functionName.str){
                info.code.add(BC_MOV,REG_RETURN_VALUE,acc0+regCount);
                // info.code.add(BC_DEL,REG_RETURN_VALUE);
                _GLOG(INST<<"\n";)
            }
            regCount = 0;
            if(opCount!=0){
                ERRTOK<<"opCount should have been zero before ending\n";
                info.errors++;
                opCount = 0;
            }
            variableName={};
            assignment=0;
        }
        if(token.flags&TOKEN_SUFFIX_LINE_FEED){
            if(info.tokens.length()>info.index&&info.parDepth==0){
                info.addDebugLine(info.index);
                #ifdef USE_DEBUG_INFO
                _GLOG(log::out <<"\n"<<*((Bytecode::DebugLine*)info.code.debugLines.data+info.code.debugLines.used-1)<<"\n";)
                #endif
            }
        }
        if(ending){
            break;
        }
    }
    return returnFlags;
}
void Bytecode::printStats(){
    using namespace engone;
    log::out << log::BLUE<< "\n##   Summary (total: "<<getMemoryUsage()<<" bytes)   ##\n";
    // Note: BC_LOADC counts as two instructions
    log::out <<" Instructions: "<<codeSegment.used<<" (max "<<(codeSegment.max*codeSegment.m_typeSize) <<" bytes) \n";
    log::out <<" Numbers: "<<constNumbers.used<<" (max "<<(constNumbers.max*constNumbers.m_typeSize)<<" bytes)\n";
    log::out <<" Strings: "<<constStrings.used<<" (max "<<(constStrings.max*constStrings.m_typeSize)<<" bytes)\n";
    log::out <<" StringText: "<<constStringText.used<<" (max "<<(constStringText.max*constStringText.m_typeSize)<<" bytes)\n";
    log::out <<" DebugLines: "<<debugLines.used<<" (max "<<(debugLines.max*debugLines.m_typeSize)<<" bytes)\n";
    log::out <<" DebugText: "<<debugLineText.used<<" (max "<<(debugLineText.max*debugLineText.m_typeSize)<<" bytes)\n";

}
Bytecode GenerateScript(Tokens tokens, int* outErr){
    using namespace engone;
    log::out <<log::BLUE<<  "\n##   Generator   ##\n";
    
    GenerationInfo info{};
    info.tokens = tokens;
    
    info.internalFunctions["print"]=true;
    
    info.code.add(BC_NUM,REG_ZERO);
    info.code.add(BC_NUM,REG_STACK_POINTER);
    info.code.add(BC_NUM,REG_FRAME_POINTER);
    
    while (!info.end()){
        info.addDebugLine(info.index);
        #ifdef USE_DEBUG_INFO
        _GLOG(log::out <<"\n"<<*((Bytecode::DebugLine*)info.code.debugLines.data+info.code.debugLines.used-1)<<"\n";)
        #endif
        if(!ParseBody(info)){
            info.nextLine();
        }
    }
    
    // int accDepth = 1;
    // info.addDebugLine(0);
    // #ifdef USE_DEBUG_INFO
    // _GLOG(log::out <<*((Bytecode::DebugLine*)info.code.debugLines.data+info.code.debugLines.used-1)<<"\n";)
    // #endif
    // while (!info.end()){
        // int flags = EvaluateExpression(info,accDepth);
        // // log::out << "Done? run it if no assignment?\n";
        // if(0==(flags&EVAL_FLAG_NO_DEL)){
        //     info.code.add(BC_DEL,REG_ACC0+accDepth);
        // }
    // }
    
    
    // Note: code below adds some cleanup instructions which isn't necessary but good practise
    
    info.tokens.get(info.tokens.length()-1).flags |= TOKEN_SUFFIX_LINE_FEED;
    info.tokens.add("(Clean instructions)");
    info.addDebugLine(info.tokens.length()-1);
    
    // #ifdef USE_DEBUG_INFO
    // _GLOG(log::out <<*((Bytecode::DebugLine*)info.code.debugLines.data+info.code.debugLines.used-1);)
    // #endif

    for(auto& pair : info.variables){
        // log::out << "del "<<pair.first<<"\n";
        info.code.add(BC_LOADV,REG_ACC0,pair.second.frameIndex);
        info.code.add(BC_DEL,REG_ACC0);
    }

    info.code.add(BC_DEL,REG_ZERO);
    // _GLOG(INST<<"\n";)
    info.code.add(BC_DEL,REG_STACK_POINTER);
    // _GLOG(INST<<"\n";)
    info.code.add(BC_DEL,REG_FRAME_POINTER);
    // _GLOG(INST<<"\n";)
    
    if(outErr){
        *outErr = info.errors;   
    }
    if(info.errors)
        log::out << log::RED<<"Generator failed with "<<info.errors<<" errors\n";
    
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
                int index=0;
                // info.code.add(BC_LOADC,num);
                // info.code.add(*(Instruction*)&index);
                
                info.code.addLoadC(num,index);

                info.instructionsToResolve.push_back({info.code.length()-1,token});
                log::out << info.code.get(info.code.length()-1) << " (resolve "<<token<<")\n";
                // log::out << info.code.get(info.code.length()-2) << " (resolve "<<token<<")\n";
            }else{
                // resolve bc_load_const directly
                // num = REG_TEMP0 + regIndex;
                // info.instructionsToResolve.push_back({info.code.length()+1,token});
                info.instructionsToResolve.push_back({info.code.length(),token});
            }
            return true;
        }
    }
    if(flags&ARG_NUMBER){
        if(IsDecimal(token)){
            Decimal number = ConvertDecimal(token);
            uint index = info.code.addConstNumber(number);
            _GLOG(log::out<<"Constant ? = "<<number<<" to "<<index<<"\n";)
            num = REG_ACC0 + regIndex;
            info.code.add(BC_NUM,num);
            _GLOG(log::out << info.code.get(info.code.length()-1) << "\n";)
            info.code.addLoadC(num,index);
            _GLOG(log::out << info.code.get(info.code.length()-1) << "\n";)

            // info.code.add(BC_LOADC,num);
            // _GLOG(log::out << info.code.get(info.code.length()-1) << "\n";)
            // info.code.add(*(Instruction*)&index); // address/index
            return true;
        }
    }
    if(flags&ARG_STRING){
        if(token.flags&TOKEN_QUOTED){
            uint index = info.code.addConstString(token);
            _GLOG(log::out<<"Constant ? = "<<token<<" to "<<index<<"\n";)
            num = REG_ACC0 + regIndex;
            info.code.add(BC_STR,num);
            _GLOG(log::out << info.code.get(info.code.length()-1) << "\n";)

            info.code.addLoadC(num,index);
            _GLOG(log::out << info.code.get(info.code.length()-1) << "\n";)

            // info.code.add(BC_LOADC,num);
            // _GLOG(log::out << info.code.get(info.code.length()-1) << "\n";)
            // info.code.add(*(Instruction*)&index); // address/index
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
Bytecode GenerateInstructions(Tokens tokens, int* outErr){
    using namespace engone;
    log::out<<"\n##   Generator (instructions)   ##\n";
    
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
                _GLOG(log::out<<"Address " << name <<" = "<<address<<"\n";)
            }else{
                CHECK_END
                token = info.next();
                if(0==(token.flags&TOKEN_SUFFIX_LINE_FEED)){
                    ERRTOK <<"Expected line feed after "<<token<<"\n";
                    info.nextLine();
                    continue;
                }
                if(IsDecimal(token)){
                    Decimal number = ConvertDecimal(token);
                    int index = info.code.addConstNumber(number);
                    info.nameOfNumberMap[name] = index;
                    _GLOG(log::out<<"Constant " << name<<" = "<<number<<" to "<<index<<"\n";)
                }else if(token.flags&TOKEN_QUOTED){
                    int index = info.code.addConstString(token);
                    info.nameOfStringMap[name] = index;
                    _GLOG(log::out<<"Constant " << name<<" = "<<token<<" to "<<index<<"\n";)
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
            _GLOG(log::out << info.code.get(info.code.length()-1)<<("\n");)
            // if(instType==BC_LOADC){
            //     int zeroInst=0;
            //     info.code.add(*(Instruction*)&zeroInst);
            // }
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
                ERRT(addr.token) <<"Constant "<<addr.token<<" could not be resolved\n";
            }else{
                index = find2->second;
            }
        }else{
            index = find->second;
        }
        if(inst.type==BC_LOADC){
            inst.reg1 = index&0xFF;
            inst.reg2 = (index>>8)&0xFF;
            // inst = *(Instruction*)&index;

        }else{
            ERRT(addr.token)<<"Cannot resolve\n";
        }
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