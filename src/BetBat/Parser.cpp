#include "BetBat/Parser.h"

#define ERRAT(L,C) info.errors++;engone::log::out <<engone::log::RED<< "CompileError "<<(L)<<":"<<(C)<<", "
#define ERRT(T) ERRAT(T.line,T.column)
#define ERRTL(T) ERRAT(T.line,T.column+T.length)
#define ERRTOK ERRAT(token.line,token.column)
#define ERRTOKL ERRAT(token.line,token.column+token.length)

#define ERR_GENERIC ERRTOK << "unexpected "<<token<<" "<<__FUNCTION__<<"\n"

#define INST engone::log::out << (info.code.length()-1)<<": " <<(info.code.get(info.code.length()-1)) << ", "
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

#define ENTER log::out << log::GRAY << "    enter "<< __FUNCTION__<<"\n";
#define TRY  if(attempt){log::out << log::GRAY << "    try   "<< __FUNCTION__<<"\n";}\
    else{ENTER}
#define EXIT log::out << log::GRAY << "    exit  "<< __FUNCTION__<<"\n";

#ifdef GLOG
#define _GLOG(x) x;
#else
#define _GLOG(x) ;
#endif

#define CASE(x) case x: return #x;
const char* ParseResultToString(int result){
    switch(result){
        CASE(PARSE_SUCCESS)
        CASE(PARSE_NO_VALUE)
        CASE(PARSE_ERROR)
        CASE(PARSE_BAD_ATTEMPT)
    }
    return "?";
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
        INSTCASE(BC_NOT_EQUAL)
        INSTCASE(BC_AND)
        INSTCASE(BC_OR)
        
        INSTCASE(BC_JUMPNIF)
        
        INSTCASE(BC_COPY)
        INSTCASE(BC_MOV)
        INSTCASE(BC_RUN)
        INSTCASE(BC_LOADV)
        INSTCASE(BC_STOREV)

        INSTCASE(BC_LEN)
        INSTCASE(BC_SUBSTR)
        INSTCASE(BC_TYPE)
        INSTCASE(BC_NOT)
        
        INSTCASE(BC_WRITE_FILE)
        INSTCASE(BC_APPEND_FILE)
        INSTCASE(BC_READ_FILE)

        INSTCASE(BC_NUM)
        INSTCASE(BC_STR)
        INSTCASE(BC_DEL)
        INSTCASE(BC_JUMP)
        INSTCASE(BC_LOADNC)
        INSTCASE(BC_LOADSC)
        // INSTCASE(BC_PUSH)
        // INSTCASE(BC_POP)
        INSTCASE(BC_RETURN)
        // INSTCASE(BC_ENTERSCOPE)
        // INSTCASE(BC_EXITSCOPE)
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
        case REG_NULL: return REG_NULL_S;
        case REG_ZERO: return REG_ZERO_S;
        case REG_RETURN_VALUE: return REG_RETURN_VALUE_S;
        case REG_ARGUMENT: return REG_ARGUMENT_S;
        case REG_FRAME_POINTER: return REG_FRAME_POINTER_S;
        // case REG_STACK_POINTER: return REG_STACK_POINTER_S;
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
    auto color = log::out.getColor();
    if(type==BC_NUM||type==BC_STR||type==BC_SUBSTR||type==BC_COPY)
        log::out << log::AQUA;
    if(type==BC_DEL)
        log::out << log::PURPLE;
    if(type==BC_LOADNC||type==BC_LOADSC||type==BC_LOADV||type==BC_STOREV){
        log::out << InstToString(type) << " $"<<RegToString(reg0) << " "<<((uint)reg1|((uint)reg2<<8));
    } else if((type&BC_MASK) == BC_R1){
        log::out << InstToString(type);
        if(reg1==0&&reg2==0){
            log::out << " $"<<RegToString(reg0);
        }else{
            log::out << " "<<((uint)reg0|((uint)reg1<<8)|((uint)reg2<<8));
        }
    }else if((type&BC_MASK) == BC_R2){
        log::out << InstToString(type)<<" $" <<RegToString(reg0);
        // if(reg2!=0||type==BC_LOADV){
        //     log::out << " "<<((uint)reg1|((uint)reg2<<8));
        // }else{
            log::out << " $"<<RegToString(reg1);
        // }
    // if((type&BC_MASK) == BC_R3)
    } else
        log::out<<InstToString(type)<<" $"<<RegToString(reg0)<<" $"<<RegToString(reg1)<<" $"<<RegToString(reg2);
        
    log::out << color;
}
engone::Logger& operator<<(engone::Logger& logger, Instruction& instruction){
    instruction.print();
    return logger;
}
engone::Logger& operator<<(engone::Logger& logger, Bytecode::DebugLine& debugLine){
    logger << engone::log::GRAY <<"Line "<<debugLine.line<<": ";
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
void Bytecode::finalizePointers(){
    for(int i=0;i<(int)constStrings.used;i++){
        String* str = (String*)constStrings.data+i;
        str->memory.data = (char*)constStringText.data + (uint64)str->memory.data;
    }
    for(int i=0;i<(int)debugLines.used;i++){
        DebugLine* line = (DebugLine*)debugLines.data+i;
        line->str = (char*)debugLineText.data + (uint64)line->str;
    }
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

    str->memory.data = (char*)constStringText.used;
    // can't use consStringText.data since it may be reallocated
    // str->memory.data = (char*)constStringText.data + constStringText.used;
    str->memory.max = str->memory.used = length;
    str->memory.m_typeSize=0;
    
    char* ptr = (char*)constStringText.data + constStringText.used;
    memcpy(ptr,token.str,token.length);
    if(padding&&padlen!=0)
        memcpy(ptr + token.length,padding,padlen);

    constStrings.used++;
    constStringText.used+=length;
    return index;
}
bool Bytecode::addLoadNC(uint8 reg0, uint constIndex){
    if((constIndex>>16)!=0)
        engone::log::out << engone::log::RED<< "ByteCode::addLoadC, TO SMALL FOR 32 bit integer ("<<constIndex<<")\n";
    if(!add(BC_LOADNC,reg0,constIndex))
        return false;
    // if(!add(*(Instruction*)&constIndex)) return false;
    return true;
}
bool Bytecode::addLoadSC(uint8 reg0, uint constIndex){
    if((constIndex>>16)!=0)
        engone::log::out << engone::log::RED<< "ByteCode::addLoadC, TO SMALL FOR 32 bit integer ("<<constIndex<<")\n";
    if(!add(BC_LOADSC,reg0,constIndex))
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
bool Bytecode::remove(int index){
    if(index<0||index>=(int)codeSegment.used){
        return false;
    }
    Instruction& inst = *((Instruction*)codeSegment.data + index);
    inst = {};
    return true;
}
Bytecode::DebugLine* Bytecode::getDebugLine(int instructionIndex, int* latestIndex){
    if(latestIndex==0)
        return 0;
    int index = *latestIndex;
    DebugLine* line = 0;
    while(true){
        if((int)index>=(int)debugLines.used)
            return 0;
        index++;
        line = (DebugLine*)debugLines.data + index;
        if(line->instructionIndex==instructionIndex)
            break;
    }
    if(latestIndex)
        *latestIndex = index;
    return line;
}
Bytecode::DebugLine* Bytecode::getDebugLine(int instructionIndex){
    for(int i=0;i<(int)debugLines.used;i++){
        DebugLine* line = (DebugLine*)debugLines.data + i;
        
        if(instructionIndex==line->instructionIndex){
            return line;
        }else if(instructionIndex<line->instructionIndex){
            if(i==0)
                return 0;
            return (DebugLine*)debugLines.data + i-1;
        }
    }
    return 0;
}
void ParseInfo::makeScope(){
    using namespace engone;
    _GLOG(log::out << log::GRAY<< "   Enter scope "<<scopes.size()<<"\n";)
    scopes.push_back({});
}
int ParseInfo::Scope::getVariableCleanupCount(ParseInfo& info){
    int offset=0;
    for(Token& token : variableNames){
        auto pair = info.variables.find(token);
        if(pair!=info.variables.end()){
            offset+=2; // based on drop scope
        }
    }
    return offset;
}
void ParseInfo::Scope::removeReg(int reg){
    for(int i = 0;i<(int)delRegisters.size();i++){
        if(delRegisters[i]==reg){
            delRegisters.erase(delRegisters.begin()+i);
            break;
        }
    }   
}
void ParseInfo::dropScope(int index){
    using namespace engone;
    if(index==-1){
        _GLOG(log::out << log::GRAY<<"   Exit  scope "<<(scopes.size()-1)<<"\n";)
    }
    Scope& scope = index==-1? scopes.back() : scopes[index];
    auto& info = *this;
    for(Token& token : scope.variableNames){
        auto pair = variables.find(token);
        if(pair!=variables.end()){
            // code.add(BC_LOADV,REG_ACC0-1,pair->second.frameIndex);
            // _GLOG(INST << "\n";)
            // code.add(BC_DEL,REG_ACC0-1);
            // _GLOG(INST << "'"<<token<<"'\n";)
            
            // deletes previous value
            code.add(BC_STOREV,REG_NULL,pair->second.frameIndex);
            _GLOG(INST << "'"<<token<<"'\n";)
            if(index==-1)
                variables.erase(pair);
        }
    }
    for(int reg : scope.delRegisters){
        code.add(BC_DEL,reg);
        _GLOG(INST << "\n";)
    }
    if(index==-1)
        scopes.pop_back();
}
int Bytecode::length(){
    return (int)codeSegment.used;
}
bool Bytecode::removeLast(){
    if(codeSegment.used>0)
        return false;
    codeSegment.used--;
    return true;
}
Instruction& Bytecode::get(uint index){
    // Assert(index<codeSegment.used);
    return *((Instruction*)codeSegment.data + index);
}

ParseInfo::Variable* ParseInfo::getVariable(const std::string& name){
    auto pair = variables.find(name);
    if(pair==variables.end())
        return 0;
    return &pair->second;
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
bool IsName(Token& token){
    if(token.flags&TOKEN_QUOTED) return false;
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
    if(token=="<") return BC_LESS;
    if(token==">") return BC_GREATER;
    if(token=="==") return BC_EQUAL;
    if(token=="!=") return BC_NOT_EQUAL;
    if(token=="&&") return BC_AND;
    if(token=="||") return BC_OR;
    if(token=="%") return BC_MOD;
    return 0;
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
Token& ParseInfo::next(){
    Token& temp = tokens.get(index++);
    // if(temp.flags&TOKEN_SUFFIX_LINE_FEED||index==1){
        if(code.debugLines.used==0){
            if(addDebugLine(index-1)){
                Bytecode::DebugLine* line = (Bytecode::DebugLine*)code.debugLines.data+code.debugLines.used-1;
                uint64 offset = (uint64)line->str;
                line->str = (char*)code.debugLineText.data + offset;
                // line->str doesn't point to debugLineText yet since it may resize.
                // we do some special stuff to deal with it.
                _GLOG(engone::log::out <<"\n"<<*line<<"\n";)
                line->str = (char*)offset;
            }
        }else{
            Bytecode::DebugLine* lastLine = (Bytecode::DebugLine*)code.debugLines.data+code.debugLines.used - 1;
            if(temp.line != lastLine->line){
                if(addDebugLine(index-1)){
                    Bytecode::DebugLine* line = (Bytecode::DebugLine*)code.debugLines.data+code.debugLines.used-1;
                    uint64 offset = (uint64)line->str;
                    line->str = (char*)code.debugLineText.data + offset;
                    // line->str doesn't point to debugLineText yet since it may resize.
                    // we do some special stuff to deal with it.
                    _GLOG(engone::log::out <<"\n"<<*line<<"\n";)
                    line->str = (char*)offset;
                }
            }
        }
    // }
    return temp;
}
bool ParseInfo::revert(){
    if(index==0)
        return false;
    index--;
    
    // Code is broken. debug line may be printed multiple twice
    // if(code.debugLines.used>0){
    //     Token& tok = tokens.get(index);
    //     Bytecode::DebugLine* lastLine = (Bytecode::DebugLine*)code.debugLines.data+code.debugLines.used - 1;
    //     if(tok.line == lastLine->line){
    //         code.debugLines.used--;   
    //     }
    // }
    return true;
}
Token& ParseInfo::prev(){
    return tokens.get(index-2);
}
Token& ParseInfo::now(){
    return tokens.get(index-1);
}
Token& ParseInfo::get(uint _index){
    return tokens.get(_index);
}
bool ParseInfo::end(){
    Assert(index<=tokens.length());
    return index==tokens.length();
}
void ParseInfo::finish(){
    index = tokens.length();
}
int ParseInfo::at(){
    return index-1;
}
void ParseInfo::printLine(){
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
bool ParseInfo::addDebugLine(uint tokenIndex){
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
    line->str = (char*)code.debugLineText.used;
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
            // uint64 offset = (uint64)token.str;
            char* ptr = token.str;
            char chr = *((char*)ptr+j);
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
bool ParseInfo::addDebugLine(const char* str, int lineIndex){
    #ifndef USE_DEBUG_INFO
    return false;
    #endif
    int length = strlen(str);
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
    line->str = (char*)code.debugLineText.used;
    line->length = length;
    line->instructionIndex = code.length();
    line->line = lineIndex;
        
    for(int j=0;j<length;j++){
        char chr = str[j];
        *((char*)code.debugLineText.data + code.debugLineText.used++) = chr;
    }
    return true;
}
void ParseInfo::nextLine(){
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
    if(op==BC_AND||op==BC_OR) return 1;
    if(op==BC_LESS||op==BC_GREATER||op==BC_EQUAL||op==BC_NOT_EQUAL) return 5;
    if(op==BC_ADD||op==BC_SUB) return 9;
    if(op==BC_MUL||op==BC_DIV||op==BC_MOD) return 10;
    return 0;
}
// Concatenates current token with the next tokens not seperated by space, linefeed or such
// operators and special characters are seen as normal letters
Token CombineTokens(ParseInfo& info){
    using namespace engone;
        
    Token outToken{};
    while(!info.end()) {
        Token token = info.get(info.at()+1);
        // Todo: stop at ( # ) and such
        if(token==";"){
            break;   
        }
        if((token.flags&TOKEN_QUOTED) && outToken.str){
            break;
        }
        token = info.next();
        if(!outToken.str)
            outToken.str = token.str;
        outToken.length += token.length;
        outToken.flags = token.flags;
        if(token.flags&TOKEN_QUOTED)
            break;
        if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED))
            break;
    }
    return outToken;
}

int ParseAssignment(ParseInfo& info, ExpressionInfo& exprInfo, bool attempt, bool requestCopy);
int ParseCommand(ParseInfo& info, ExpressionInfo& exprInfo, bool attempt);

int ParseExpression(ParseInfo& info, ExpressionInfo& exprInfo, bool attempt){
    using namespace engone;
    
    _GLOG(TRY)
    
    // if(ParseAssignment(info,exprInfo,true)){
    //     _GLOG(log::out << "-- exit ParseExpression\n";)
    //     return 1;
    // }
    if(info.end())
        return PARSE_ERROR;
    bool expectOperator=false;
    bool hadNot = false;
    while(true){
        Token token = info.get(info.at()+1);
        // Token token = info.next();
        
        int opType = 0;
        bool ending=false;
        
        if(expectOperator){
            if((opType = IsOperation(token))){
                token = info.next();
                exprInfo.operations[exprInfo.opCount++] = opType;
                _GLOG(log::out << "Operator "<<token<<"\n";)
            } else if(token == ";"){
                token = info.next();
                ending = true;
            } else {
                ending = true;
                // info.revert();
            }
        }else{
            // if(token == "null"){
            //     info.code.add(BC_MOV,REG_NULL,exprInfo.acc0Reg+exprInfo.regCount);
            //     _GLOG(INST << "\n";)
            //     exprInfo.regCount++;
            // } else
            bool no=false;
            if(token=="-"){
                Token tok = info.get(info.at()+2);
                if(IsDecimal(tok)){
                    no = true;
                    token = info.next();
                    token = info.next();
                    // Note the - before ConvertDecimal
                    int constIndex = info.code.addConstNumber(-ConvertDecimal(token));
                    info.code.add(BC_NUM,exprInfo.acc0Reg+exprInfo.regCount);
                    _GLOG(INST << "\n";)
                    info.code.addLoadNC(exprInfo.acc0Reg+exprInfo.regCount,constIndex);
                    _GLOG(INST << "constant "<<token<<"\n";)
                    exprInfo.regCount++;
                }else{
                    // bad
                }
            }
            // if(token=="!"){
            //     token = info.next();
            //     hadNot=true;
            //     continue;   
            // }
            if(no){
                // nothing
            }else if (token == "#"&&!(token.flags&TOKEN_QUOTED)){
                // if(exprInfo.regCount == 0){
                    token = info.next();
                    if(token.flags&TOKEN_SUFFIX_SPACE){
                        // token = info.next();
                        ERRTOK << "expected a directive\n";
                        return PARSE_ERROR;
                    }
                    token = info.next();
                    if(token == "run"){
                        // token = info.next();
                        ExpressionInfo expr{};
                        expr.acc0Reg = exprInfo.acc0Reg + exprInfo.regCount;
                        int success = ParseCommand(info,expr,false);
                        if(success){
                            exprInfo.regCount++;
                        }else{
                            return PARSE_ERROR;
                        }
                    }else if(token == "arg"){
                        int reg = exprInfo.acc0Reg+exprInfo.regCount;
                        info.code.add(BC_COPY,REG_ARGUMENT,reg);
                        _GLOG(INST << "\n";)
                        exprInfo.regCount++;
                    }else if(token == "i"){
                        if(info.loopScopes.size()==0){
                            ERRTOK << "must be in a loop to use '#"<<token<<"'\n";
                            ERRLINE
                            // info.nextLine();
                            return PARSE_ERROR;
                        }else{
                            int reg = exprInfo.acc0Reg+exprInfo.regCount;
                            info.code.add(BC_COPY,info.loopScopes.back().iReg,reg);
                            _GLOG(INST << "\n";)
                            exprInfo.regCount++;
                        }
                    }else if(token == "v"){
                        if(info.loopScopes.size()==0){
                            ERRTOK << "must be in a loop to use '#"<<token<<"'\n";
                            ERRLINE
                            // info.nextLine();
                            return PARSE_ERROR;
                        }else{
                            int reg = exprInfo.acc0Reg+exprInfo.regCount;
                            info.code.add(BC_COPY,info.loopScopes.back().vReg,reg);
                            _GLOG(INST << "\n";)
                            exprInfo.regCount++;
                        }
                    }else {
                        ERRTOK << "undefined directive '"<<token<<"'\n";
                        ERRLINE
                        // info.nextLine();
                        return PARSE_ERROR;
                    }
                // Todo: some directives should not toggle expectOperator.
                //  The toggle is currently forced at the end of this loop.

                // }else{
                //     ERRTOK << "directive not allowed\n";
                //     info.nextLine();
                //     return 0;
                // }
            } else if(IsDecimal(token)){
                token = info.next();
                int constIndex = info.code.addConstNumber(ConvertDecimal(token));
                info.code.add(BC_NUM,exprInfo.acc0Reg+exprInfo.regCount);
                _GLOG(INST << "\n";)
                info.code.addLoadNC(exprInfo.acc0Reg+exprInfo.regCount,constIndex);
                _GLOG(INST << "constant "<<token<<"\n";)
                exprInfo.regCount++;
            } else if(token.flags&TOKEN_QUOTED){
                token = info.next();
                int reg = exprInfo.acc0Reg + exprInfo.regCount;
                int constIndex = info.code.addConstString(token);
                info.code.add(BC_STR,reg);
                _GLOG(INST << "\n";)
                info.code.addLoadSC(reg,constIndex);
                _GLOG(INST << "constant "<<token<<"\n";)
                exprInfo.regCount++;
            } else if(IsName(token)){
                // Todo: handle variable or function
                auto varPair = info.variables.find(token);
                // auto funcPair = info.externalCalls.map.find(token);
                auto func = GetExternalCall(token);
                if(varPair!=info.variables.end()){
                    token = info.next();
                    int reg = exprInfo.acc0Reg + exprInfo.regCount;
                    int tempReg = exprInfo.acc0Reg + exprInfo.regCount+1;

                    // normal variable nothing special
                    info.code.add(BC_LOADV,reg,varPair->second.frameIndex);
                    _GLOG(INST << "get variable "<<varPair->second.frameIndex<<"\n";)
                    info.code.add(BC_COPY,reg,reg);
                    _GLOG(INST << "\n";)
                    exprInfo.regCount++;
                }else{
                    if(attempt){
                        return PARSE_BAD_ATTEMPT;
                    }else{
                        ERRTOK << "undefined variable '"<<token<<"'\n";
                        ERRLINE
                        return PARSE_ERROR;
                    }
                } 
                // if(funcPair!=info.externalCalls.map.end()){
                //     log::out << log::RED << "Func pair in expr. not implemented!\n";
                    
                //     // Todo: ParseCommand?
                    
                //     // int reg = exprInfo.acc0Reg + exprInfo.regCount;
                //     // info.code.add(BC_LOADV,reg,varPair->second.frameIndex);
                //     // _GLOG(INST << "get variable "<<varPair->second.frameIndex<<"\n";)
                //     // info.code.add(BC_COPY,reg,reg);
                //     // _GLOG(INST << "\n";)
                //     // exprInfo.regCount++;
                // } else{
                //     // if(attempt){
                //     //     // Note: wrong parse functions was chosen OR syntax error but we don't handle that here
                //     //     info.revrt();
                //     // }else{
                //     ERR_GENERIC;
                //     info.nextLine();
                //     // }
                //     return 0;
                // }
            } else if(token=="("){
                token = info.next();
                //Note: attempt does not work since we can't simply info.index-- (revert?) if ParseExpression messed around
                // We notify the code with line below 
                attempt=false;
                
                ExpressionInfo expr{};
                expr.acc0Reg = exprInfo.acc0Reg+exprInfo.regCount;
                _GLOG(log::out<<"Par ( \n";)
                info.parDepth++;
                int result = ParseExpression(info,expr,false);
                if(result==PARSE_SUCCESS){
                    exprInfo.regCount++;
                    info.parDepth--;
                    _GLOG(log::out<<"Par ) \n";)
                }else{
                    // info.parDepth--;
                    // ERR_GENERIC;
                    // info.nextLine();
                    return PARSE_ERROR;
                }
            } else{
                if(attempt){
                    // can happen first time
                    return PARSE_BAD_ATTEMPT;
                }else{
                    ERR_GENERIC;
                    ERRLINE
                    return PARSE_ERROR;
                }
                // ending = true;
                // info.revert();
            }
        }


        if(exprInfo.regCount!=0){
            int reg = exprInfo.acc0Reg + exprInfo.regCount-1;
            int tempReg = exprInfo.acc0Reg + exprInfo.regCount;
            Token prop = info.get(info.at()+1);
            if(prop=="."){
                // property of a variable
                info.next();
                
                prop = info.next();
                if(prop == "type"){
                    info.code.add(BC_STR,tempReg);
                    _GLOG(INST << "\n";)
                    info.code.add(BC_TYPE,reg,tempReg);
                    _GLOG(INST << "\n";)
                    info.code.add(BC_DEL,reg);
                    _GLOG(INST << "\n";)
                    info.code.add(BC_MOV,tempReg,reg);
                    _GLOG(INST << "\n";)
                    // exprInfo.regCount++;
                }else if(prop=="length"){
                    info.code.add(BC_NUM,tempReg);
                    _GLOG(INST << "\n";)
                    info.code.add(BC_LEN,reg,tempReg);
                    _GLOG(INST << "\n";)
                    info.code.add(BC_DEL,reg);
                    _GLOG(INST << "\n";)
                    info.code.add(BC_MOV,tempReg,reg);
                    _GLOG(INST << "\n";)
                    // exprInfo.regCount++;
                }else{
                    ERRTOK << "unknown property '"<<prop<<"'\n";
                    ERRLINE
                    return PARSE_ERROR;
                }
            }else if(prop=="["){
                // substring of a string
                prop = info.next(); // prop = "["
                
                ExpressionInfo expr{};
                expr.acc0Reg = tempReg;
                ExpressionInfo expr2{};
                expr2.acc0Reg = tempReg+1;
                int result = ParseExpression(info,expr,false);
                if(result!=PARSE_SUCCESS){
                    return PARSE_ERROR;
                }
                
                prop = info.get(info.at()+1);
                if(prop==":"){
                    prop = info.next();
                    int result = ParseExpression(info,expr2,false);
                    if(result!=PARSE_SUCCESS){
                        return PARSE_ERROR;
                    }
                    prop = info.get(info.at()+1); // next should be ]
                }

                if(prop != "]"){
                    ERRTOK << "exprected ] got '"<<prop<<"'\n";
                    ERRLINE
                    return PARSE_ERROR;
                }
                prop = info.next();

                info.code.add(BC_MOV,reg,tempReg+2);
                _GLOG(INST << "temp 2\n";)
                if(expr2.regCount==0)
                    info.code.add(BC_SUBSTR,reg,tempReg,tempReg); // var[5]
                else
                    info.code.add(BC_SUBSTR,reg,tempReg,tempReg+1); // var[3:4]
                _GLOG(INST << "\n";)

                info.code.add(BC_DEL,tempReg);
                _GLOG(INST << "temp 0\n";)
                if(expr2.regCount!=0){
                    info.code.add(BC_DEL,tempReg+1);
                    _GLOG(INST << "temp 1\n";)
                }
                info.code.add(BC_DEL,tempReg+2);
                _GLOG(INST << "temp 2\n";)
            }
        }
        
        if(hadNot){
            int reg = exprInfo.acc0Reg+exprInfo.regCount-1;
            info.code.add(BC_NOT,reg,reg);
            _GLOG(INST << "\n";)
            hadNot=false;
        }
        
        if(exprInfo.opCount==1&&exprInfo.regCount==0){
            // if(attempt){
            //     // Unexpected token for the expression. Since we did an attempt
            //     //  there may be another parse function who does allow the token.
            //     //  Therefore we go back to prev. token.
            //     info.revert();
            //     return PARSE_ERROR;
            // }else{
            //     ERR_GENERIC;
            //     // info.nextLine();
            // }
            log::out << log::RED<<"DevError: ParseExpression, can this happen?\n";
            info.errors++;
            return PARSE_ERROR;
        }
        if(exprInfo.regCount==exprInfo.opCount&&ending){
            info.errors++;
            log::out << log::RED<<"DevError: ParseExpression, can this happen?\n";
            // ERRTOK << "undefined variable "<<token<<"\n";
            // ERRLINE
            // info.nextLine();
            return PARSE_ERROR;
        }
         
        if(!(exprInfo.regCount==exprInfo.opCount || exprInfo.regCount==exprInfo.opCount+1)){
            info.errors++;
            log::out << log::RED<<"DevError: ParseExpression, can this happen?\n";
            // ERRTOK << "uneven numbers and operators, tokens: "<<info.prev()<<" "<<token<<"\n";
            // ERRLINE
            // info.nextLine();
            return PARSE_ERROR;
        }
        ending = ending || info.end() || token == ")";
        if(token==")")
            info.next();
        
        while(exprInfo.regCount>1){
            if(exprInfo.opCount>=2&&!ending){
                int op1 = exprInfo.operations[exprInfo.opCount-2];
                int op2 = exprInfo.operations[exprInfo.opCount-1];
                if(OpPriority(op1)>=OpPriority(op2)){
                    exprInfo.operations[exprInfo.opCount-2] = op2;
                    exprInfo.opCount--;
                    int reg0 = exprInfo.acc0Reg+exprInfo.regCount-2;
                    int reg1 = exprInfo.acc0Reg+exprInfo.regCount-1;
                    info.code.add(op1,reg0,reg1,reg0);
                    _GLOG(INST<<"pre\n";)
                    info.code.add(BC_DEL,reg1);
                    _GLOG(INST << "\n";)
                    
                    // info.code.add(op1,reg0,reg1,reg0);
                    // info.code.add(BC_DEL,reg1);
                    exprInfo.regCount--;
                }else{
                    // _GLOG(log::out << "break\n";)
                    break;
                }
            }else if(ending){
                int reg0 = exprInfo.acc0Reg+exprInfo.regCount-2;
                int reg1 = exprInfo.acc0Reg+exprInfo.regCount-1;
                int op = exprInfo.operations[exprInfo.opCount-1];
                exprInfo.opCount--;
                info.code.add(op,reg0,reg1,reg0);
                _GLOG(INST<<"post\n";)
                info.code.add(BC_DEL,reg1);
                _GLOG(INST << "\n";)
                exprInfo.regCount--;
            }else{
                break;
            }
        }
        // Attempt succeeded. Failure is now considered an error.
        attempt = false;
        expectOperator=!expectOperator;
        if(ending){
            if(exprInfo.regCount>1){
                log::out<<log::YELLOW<<"CompilerWarning: exprInfo.regCount > 1 at end of expression";             
            }else if(exprInfo.regCount==0){
                log::out<<log::YELLOW<<"CompilerWarning: exprInfo.regCount = 0 at end of expression";             
            }
            _GLOG(EXIT)
            return PARSE_SUCCESS;
        }
    }
}
int ParseBody(ParseInfo& info, int acc0reg);
// returns 0 if syntax is wrong for flow parsing
int ParseFlow(ParseInfo& info, int acc0reg, bool attempt){
    using namespace engone;
    
    _GLOG(TRY)
    
    if(info.end()){
        return PARSE_ERROR;
    }
    Token token = info.get(info.at()+1);
    
    if(token=="_test_"){
        info.next();

        Token name = info.get(info.at()+1);
        if(!IsName(name)){
            ERRT(name) << "Expected a valid variable name '"<<name<<"'\n";
            return PARSE_ERROR;
        }
        info.next();
        
        auto pair = info.variables.find(name);
        if(pair==info.variables.end()){
            ERRT(name) << "Undefined variable '"<<name<<"'\n";
            return PARSE_ERROR;
        }
        int reg = acc0reg;
        info.code.add(BC_LOADV, reg, pair->second.frameIndex);
        _GLOG(INST << "\n";)
        info.code.add(BC_TEST, reg);
        _GLOG(INST << "var '"<<name<<"'\n";)
    }else if(token=="return"){
        token = info.next();
        ExpressionInfo expr{};
        expr.acc0Reg = acc0reg;
        int result = ParseExpression(info,expr,true);
        if(result!=PARSE_SUCCESS&&result!=PARSE_BAD_ATTEMPT){
            return PARSE_ERROR;
        }
        ParseInfo::FuncScope& funcScope = info.funcScopes.back();

        for(int i = info.scopes.size()-1;i>=funcScope.scopeIndex;i--){
            info.dropScope(i);
        }
        if(result==PARSE_SUCCESS)
            info.code.add(BC_RETURN, expr.acc0Reg);
        else
            info.code.add(BC_RETURN, REG_NULL);
        _GLOG(INST << "\n";)
    }else if(token == "break"){
        token = info.next();
        if(info.loopScopes.empty()){
            ERRTOK << token<<" only allowed in a loop\n";
            ERRLINE
            return PARSE_ERROR;
        }
        ParseInfo::LoopScope& loop = info.loopScopes.back();

        for(int i = info.scopes.size()-1;i>loop.scopeIndex;i--){
            info.dropScope(i);
        }

        info.code.addLoadNC(loop.jumpReg,loop.breakConstant);
        _GLOG(INST << "\n";)
        info.code.add(BC_JUMP, loop.jumpReg);
        _GLOG(INST << "\n";)
    }else if(token=="continue"){
        token = info.next();
        if(info.loopScopes.empty()){
            ERRTOK << token<<" only allowed in a loop\n";
            ERRLINE
            return PARSE_ERROR;
        }
        ParseInfo::LoopScope& loop = info.loopScopes.back();
        for(int i = info.scopes.size()-1;i>loop.scopeIndex;i--){
            info.dropScope(i);
        }
        info.code.addLoadNC(loop.jumpReg,loop.continueConstant);
        _GLOG(INST << "\n";)
        info.code.add(BC_JUMP, loop.jumpReg);
        _GLOG(INST << "\n";)
    } else if(token=="if"){
        token = info.next();
        ExpressionInfo expr{};
        expr.acc0Reg = acc0reg;

        int result = ParseExpression(info,expr,false);
        if(result!=PARSE_SUCCESS){
            return PARSE_ERROR;
        }
        
        info.makeScope();
        
        int jumpAddress = info.code.addConstNumber(-1);
        int jumpReg = expr.acc0Reg+1;
        info.code.add(BC_NUM,jumpReg);
        _GLOG(INST << "jump\n";)
        info.code.addLoadNC(jumpReg,jumpAddress);
        _GLOG(INST << "\n";)
        info.code.add(BC_JUMPNIF,expr.acc0Reg,jumpReg);
        _GLOG(INST << "\n";)

        ParseInfo::Scope* scope = &info.scopes.back();
        scope->delRegisters.push_back(expr.acc0Reg);
        scope->delRegisters.push_back(jumpReg);

        result = ParseBody(info,acc0reg+2);
        if(result!=PARSE_SUCCESS){
            return PARSE_ERROR;
        }
        scope = &info.scopes.back();

        token = {};
        if(!info.end())
            token = info.get(info.at()+1);
        if(token=="else"){
            token = info.next();
            
            // we don't want to drop jumpReg here. code in else scope uses it.
            // way may have wanted to drop jumpreg in if body which is why
            // it needs to be in delRegisteers
            scope->removeReg(jumpReg);
            scope->removeReg(expr.acc0Reg);
            info.dropScope();
            info.makeScope();
            scope->delRegisters.push_back(jumpReg);
            scope->delRegisters.push_back(expr.acc0Reg);
            
            int elseAddr = info.code.addConstNumber(-1);
            info.code.addLoadNC(jumpReg,elseAddr);
            _GLOG(INST << "\n";)
            info.code.add(BC_JUMP,jumpReg);
            _GLOG(INST << "\n";)
            
            _GLOG(log::out<<log::GREEN<<"  : JUMP ADDRESS ["<<jumpAddress<<"]\n";)
            info.code.getConstNumber(jumpAddress)->value = info.code.length();
            
            int result = ParseBody(info,acc0reg+2);
            if(result!=PARSE_SUCCESS){
                return PARSE_ERROR;
            }
            // info.dropScope();
            
            _GLOG(log::out<<log::GREEN<<"  : ELSE ADDRESS ["<<elseAddr<<"]\n";)
            info.code.getConstNumber(elseAddr)->value = info.code.length()
                + scope->getVariableCleanupCount(info);
        }else{
            info.code.getConstNumber(jumpAddress)->value = info.code.length()
                + scope->getVariableCleanupCount(info);
            _GLOG(log::out<<log::GREEN<<"  : JUMP ADDRESS (roughly) ["<<jumpAddress<<"]\n";)
                
        }   
        info.dropScope();
    } else if(token=="while"){
        token = info.next();
        info.makeScope();

        int jumpReg = acc0reg;
        info.code.add(BC_NUM,jumpReg);
        _GLOG(INST << "jump\n";)
        ParseInfo::Scope& scope = info.scopes.back();
        scope.delRegisters.push_back(jumpReg);
        int startAddress = info.code.addConstNumber(info.code.length());
        _GLOG(log::out<<log::GREEN<<"  : START ADDRESS ["<<startAddress<<"]\n";)
        int endAddress = info.code.addConstNumber(-1);
        int breakAddress = info.code.addConstNumber(-1);

        ExpressionInfo expr{};
        expr.acc0Reg = acc0reg+1;
        // scope.delRegisters.push_back(expr.acc0Reg);
        int result = ParseExpression(info,expr,false);
        if(result!=PARSE_SUCCESS){
            return PARSE_ERROR;
        }


        info.code.addLoadNC(jumpReg,endAddress);
        _GLOG(INST << "\n";)
        info.code.add(BC_JUMPNIF,expr.acc0Reg,jumpReg);
        _GLOG(INST << "\n";)
        info.code.add(BC_DEL,expr.acc0Reg);
        _GLOG(INST << "\n";)
        
        info.loopScopes.push_back({});
        ParseInfo::LoopScope& loop = info.loopScopes.back();
        loop.iReg = 0; // Todo: add value for #i
        loop.jumpReg = jumpReg;
        loop.continueConstant = startAddress;
        loop.breakConstant = breakAddress;
        loop.scopeIndex = info.scopes.size()-1;

        result = ParseBody(info,expr.acc0Reg+1);
        if(result!=PARSE_SUCCESS){
            return PARSE_ERROR;
        }

        info.loopScopes.erase(info.loopScopes.begin()+info.loopScopes.size()-1);

        
        info.code.addLoadNC(jumpReg,startAddress);
        _GLOG(INST << "\n";)
        info.code.add(BC_JUMP,jumpReg);
        _GLOG(INST << "\n";)
        
        info.code.getConstNumber(endAddress)->value = info.code.length();
        _GLOG(log::out << log::GREEN<<"  : END ADDRESS ["<<endAddress<<"]\n")

        info.code.add(BC_DEL,expr.acc0Reg);
        _GLOG(INST << "\n";)

        info.code.getConstNumber(breakAddress)->value = info.code.length();
        _GLOG(log::out << log::GREEN<<"  : END BREAK ADDRESS ["<<breakAddress<<"]\n")

        info.dropScope();
    } else if(token=="for") {
        token = info.next();
        
        bool createdVar=false;
        bool createdMax=false;
        int iReg = acc0reg;
        int maxReg = acc0reg+1;
        int tempReg = acc0reg+2;
        int jumpReg = acc0reg+3;
        int unusedReg = acc0reg+4; // passed to parse functions

        info.makeScope();

        info.code.add(BC_NUM,tempReg);
        _GLOG(INST << "temp\n";)
        info.code.add(BC_NUM,jumpReg);
        _GLOG(INST << "jump\n";)

        ParseInfo::Scope& scope = info.scopes.back();
        scope.delRegisters.push_back(tempReg);
        scope.delRegisters.push_back(jumpReg);

        int startAddress = info.code.addConstNumber(-1);
        
        ExpressionInfo expr{};
        expr.acc0Reg = unusedReg;

        int stumpIndex = info.code.length();
        info.code.add(Instruction{0,(uint8)iReg}); // add stump
        _GLOG(INST << "May become BC_NUM\n";)

        bool wasAssigned=0;
        int result = ParseAssignment(info,expr,true,false);
        if(result==PARSE_SUCCESS)
            wasAssigned=true;
        if(result==PARSE_BAD_ATTEMPT){
            result = ParseExpression(info,expr,true);
        }

        if(result==PARSE_ERROR){
            return PARSE_ERROR;
        } else if(result==PARSE_NO_VALUE){
            ERRT(info.now()) << "Expected value from assignment '"<<info.now()<<"'\n";
            ERRLINE
            return PARSE_ERROR;
        } else if(result==PARSE_BAD_ATTEMPT){
            // undefined variable "for i : 5"
            token = info.get(info.at()+1);
            Token next = info.get(info.at()+2);
            if(IsName(token) && next == ":"){
                info.next();
                auto varPair = info.variables.find(token);
                if(varPair==info.variables.end()){
                    // Turn token into variable?
                    ParseInfo::Variable& var = info.variables[token] = {};
                    var.frameIndex = info.frameOffsetIndex++;

                    scope.variableNames.push_back(token);
                    
                    // createdVar=true;
                    info.code.add(BC_NUM, iReg);
                    _GLOG(INST << "var reg\n";)
                    // info.code.add(BC_PUSH, varReg);
                    // _GLOG(INST << "push "<<token<<"\n";)

                    info.code.add(BC_STOREV, iReg,var.frameIndex);
                    _GLOG(INST << "store "<<token<<"\n";)
                }else{
                    ERRT(token) << "for cannot loop with a defined variable ("<<token<<") \n";
                    // info.nextLine();
                    return PARSE_ERROR;
                }
                token = info.next();
            }else{
                ERR_GENERIC;
                // ERRLINE
                // info.nextLine();
                return PARSE_ERROR;
            }
        } else if(result==PARSE_SUCCESS){
            // max or var
            // we got start in acc0reg or max.
            token = info.get(info.at()+1);
            if(token==":"){
                token = info.next();

                // createdVar=true;
                info.code.add(BC_MOV,expr.acc0Reg,iReg);
                _GLOG(INST << "acc -> var\n";)
            } else {
                // "for i = 5"
                createdVar=true;
                Instruction& inst = info.code.get(stumpIndex);
                inst.type = BC_NUM;
                inst.reg0 = iReg;
              
                scope.delRegisters.push_back(iReg);

                info.code.getConstNumber(startAddress)->value = stumpIndex+1;
                
                // createdMax=true;
                if(!wasAssigned){
                    createdMax=true;
                }
                info.code.add(BC_MOV,expr.acc0Reg,maxReg);
                _GLOG(INST << "acc -> max\n";)
            }
        }
            
        if(token==":"){
            ExpressionInfo expr{};
            expr.acc0Reg = unusedReg;
            
            info.code.getConstNumber(startAddress)->value = info.code.length();
            // log::out << "For Start Address\n";
            bool wasAssigned=false;
            int result = ParseAssignment(info,expr,true,true);
            if(result==PARSE_SUCCESS)
                wasAssigned=true;
            if(result==PARSE_BAD_ATTEMPT){
                result = ParseExpression(info,expr,true);
            }
            if(result==PARSE_SUCCESS){
                if(!wasAssigned)
                    createdMax=true;
                info.code.add(BC_MOV,expr.acc0Reg,maxReg);
                _GLOG(INST << "acc -> max\n";)
            }else if(result==PARSE_NO_VALUE){
                ERRT(info.now()) << "Expected value from assignment '"<<info.now()<<"'\n";
                ERRLINE;
                return PARSE_ERROR;
            }else if(result ==PARSE_BAD_ATTEMPT){
                ERRTL(info.now()) << "Bad attempt '"<<info.now()<<"'\n";
                ERRLINE;
                return PARSE_ERROR;
            }else{
                // Todo: detect type of failure in parser. If the attempt failed
                //  then we log here. If other type of error we don't print here.
                // ERRTL(token) << "Parsing failed after '"<<token<<"'\n";
                // ERRLINE
                // info.nextLine();
                return PARSE_ERROR;
            }
        }
        // _GLOG(log::out << "    Start Address: "<<info.code.getConstNumber(startAddress)->value<<"\n";)
        _GLOG(log::out<<log::GREEN<<"  : START ADDRESS ["<<startAddress<<"]\n";)

        int endAddress = info.code.addConstNumber(-1);

        info.loopScopes.push_back({});
        ParseInfo::LoopScope& loop = info.loopScopes.back();
        loop.iReg = iReg;
        loop.jumpReg = jumpReg;
        loop.continueConstant = startAddress;
        loop.breakConstant = endAddress;
        loop.scopeIndex = info.scopes.size()-1;
        
        info.code.add(BC_LESS, iReg, maxReg, tempReg);
        _GLOG(INST << "\n";)
        if(createdMax){ 
            info.code.add(BC_DEL,maxReg);
            _GLOG(INST << "\n";)
        }

        info.code.addLoadNC(jumpReg,endAddress);
        _GLOG(INST << "\n";)
        info.code.add(BC_JUMPNIF, tempReg, jumpReg);
        _GLOG(INST << "\n";)
        
        result = ParseBody(info,unusedReg);
        if(result!=PARSE_SUCCESS){
            return PARSE_ERROR;
        }
        
        int incrIndex = info.code.addConstNumber(1);
        info.code.addLoadNC(tempReg,incrIndex);
        _GLOG(INST << "\n";)
        info.code.add(BC_ADD,iReg,tempReg,iReg);
        _GLOG(INST << "increment var\n";)
        
        info.code.addLoadNC(jumpReg,startAddress);
        _GLOG(INST << "\n";)
        info.code.add(BC_JUMP, jumpReg);
        _GLOG(INST << "\n";)
        
        info.code.getConstNumber(endAddress)->value = info.code.length();
        _GLOG(log::out << "    End Address: "<<info.code.getConstNumber(endAddress)->value<<"\n";)
        
        info.loopScopes.erase(info.loopScopes.begin()+info.loopScopes.size()-1);

        // If the loop didn't run once we would delete a potential variable in the
        // scope even though it wasn't made.
        info.dropScope();
    } else if(token=="each"){
        info.next();

        info.makeScope();

        int jumpReg = acc0reg;
        int iReg = acc0reg+1;
        int tempReg = acc0reg+2;
        int beginReg = acc0reg+3;
        int endReg = acc0reg+4;
        int chrReg = acc0reg+5;
        int tempStrReg = acc0reg+6;
        int varReg  = acc0reg+7;
        int unusedReg  = acc0reg+8;

        info.code.add(BC_NUM,jumpReg);
        _GLOG(INST << "jump\n";)
        info.code.add(BC_NUM,iReg);
        _GLOG(INST << "i\n";)
        info.code.add(BC_NUM,tempReg);
        _GLOG(INST << "temp\n";)
        info.code.add(BC_NUM,beginReg);
        _GLOG(INST << "begin\n";)
        info.code.add(BC_NUM,endReg);
        _GLOG(INST << "end\n";)
        info.code.add(BC_STR,tempStrReg);
        _GLOG(INST << "tempstr\n";)

        ParseInfo::Scope& scope = info.scopes.back();
        scope.delRegisters.push_back(jumpReg);
        scope.delRegisters.push_back(iReg);
        scope.delRegisters.push_back(tempReg);
        scope.delRegisters.push_back(beginReg);
        scope.delRegisters.push_back(endReg);
        scope.delRegisters.push_back(tempStrReg);

        Token name = info.get(info.at()+1);
        Token colon = info.get(info.at()+2);

        bool createdVar=false;
        if(colon == ":"){
            info.next();
            info.next();
            if(!IsName(name)){
                ERRT(name) << "Expected a variable name ('"<<name<<"' is not valid)\n";
                return PARSE_ERROR;
            }
            auto pair = info.variables.find(name);
            if(pair!=info.variables.end()){
                ERRT(name) << "Expected a non-defined varialbe ('"<<name<<"' is defined)\n";
                return PARSE_ERROR;
            }

            ParseInfo::Variable& var = info.variables[name] = {};
            var.frameIndex = info.frameOffsetIndex++;
            scope.variableNames.push_back(name);
            createdVar=true;
        }
        
        int startAddress = info.code.addConstNumber(info.code.length());
        _GLOG(log::out<<log::GREEN<<"  : START ADDRESS ["<<startAddress<<"]\n";)
        
        // Todo: ParseAssignment here? 
        //  each str : tmp = "okay" {}   <- would be possible. Is it useful?

        Token listName = info.next();
        auto pair = info.variables.find(listName);
        if(pair==info.variables.end()){
            ERRT(listName) << "Must be a defined variable ('"<<listName<<"' is not)\n";
            return PARSE_ERROR;
        }
        info.code.add(BC_LOADV,varReg,pair->second.frameIndex);
        _GLOG(INST << "\n";)

        int endAddress = info.code.addConstNumber(-1);
        int elseAddress = info.code.addConstNumber(-1);
        int bodyIfAddress = info.code.addConstNumber(-1);
        int lastAddress = info.code.addConstNumber(-1);
        int continueAddress = info.code.addConstNumber(-1);
        
        info.code.add(BC_LEN, varReg,tempReg);
        _GLOG(INST << "\n";)
        info.code.add(BC_LESS,iReg,tempReg,tempReg);
        _GLOG(INST << "\n";)

        info.code.addLoadNC(jumpReg,endAddress);
        _GLOG(INST << "\n";)
        info.code.add(BC_JUMPNIF,tempReg,jumpReg);
        _GLOG(INST << "\n";)
        
        info.code.add(BC_MOV,varReg,chrReg);
        _GLOG(INST << "\n";)
        info.code.add(BC_SUBSTR,chrReg,iReg,iReg);
        _GLOG(INST << "\n";)

        int oneConstant = info.code.addConstNumber(1);
        info.code.addLoadNC(tempReg,oneConstant);
        _GLOG(INST << "\n";)
        info.code.add(BC_ADD,iReg,tempReg,iReg);
        _GLOG(INST << "\n";)

        Token spaceToken = " ";
        int spaceConstant = info.code.addConstString(spaceToken);
        
        info.code.addLoadSC(tempStrReg,spaceConstant);
        _GLOG(INST << "\n";)
        info.code.add(BC_EQUAL,chrReg,tempStrReg,tempStrReg);
        _GLOG(INST << "\n";)
        info.code.add(BC_DEL,chrReg);
        _GLOG(INST << "\n";)
        
        info.code.addLoadNC(jumpReg,elseAddress);
        _GLOG(INST << "\n";)
        info.code.add(BC_JUMPNIF,tempStrReg,jumpReg);
        _GLOG(INST << "\n";)

        // end = i-2
        int twoConstant = info.code.addConstNumber(2);
        info.code.addLoadNC(tempReg,twoConstant);
        _GLOG(INST << "\n";)
        info.code.add(BC_SUB,iReg,tempReg,endReg);
        _GLOG(INST << "\n";)

        info.code.addLoadNC(jumpReg,bodyIfAddress);
        _GLOG(INST << "\n";)
        info.code.add(BC_JUMP,jumpReg);
        _GLOG(INST << "\n";)

        info.code.getConstNumber(elseAddress)->value = info.code.length();
        _GLOG(log::out << log::GREEN<<"  : ELSE ADDRESS ["<<elseAddress<<"]\n")

        info.code.add(BC_LEN, varReg, tempReg);
        _GLOG(INST << "\n";)
        info.code.add(BC_EQUAL,iReg,tempReg,tempReg);
        _GLOG(INST << "\n";)

        info.code.addLoadNC(jumpReg,startAddress);
        _GLOG(INST << "\n";)
        info.code.add(BC_JUMPNIF,tempReg,jumpReg);
        _GLOG(INST << "\n";)

        info.code.addLoadNC(tempReg,oneConstant);
        _GLOG(INST << "\n";)
        info.code.add(BC_SUB,iReg,tempReg,endReg);
        _GLOG(INST << "\n";)

        info.code.getConstNumber(bodyIfAddress)->value = info.code.length();
        _GLOG(log::out << log::GREEN<<"  : BODY IF ADDRESS ["<<elseAddress<<"]\n")

        info.code.addLoadNC(tempReg,oneConstant);
        _GLOG(INST << "\n";)
        info.code.add(BC_ADD,tempReg,endReg,tempReg);
        _GLOG(INST << "\n";)
        info.code.add(BC_SUB,tempReg,beginReg,tempReg);
        _GLOG(INST << "\n";)
        info.code.add(BC_GREATER,tempReg,REG_ZERO,tempReg);
        _GLOG(INST << "\n";)

        info.code.addLoadNC(jumpReg,lastAddress);
        _GLOG(INST << "\n";)
        info.code.add(BC_JUMPNIF,tempReg,jumpReg);
        _GLOG(INST << "\n";)
        
        info.code.add(BC_MOV,varReg,chrReg);
        _GLOG(INST << "\n";)
        info.code.add(BC_SUBSTR,chrReg,beginReg,endReg);
        _GLOG(INST << "\n";)

        if(createdVar){
            ParseInfo::Variable& var = info.variables[name];
            info.code.add(BC_STOREV, chrReg, var.frameIndex);
            _GLOG(INST << "store "<<name<<"\n";)
        }

        info.loopScopes.push_back({});
        ParseInfo::LoopScope& loop = info.loopScopes.back();
        loop.iReg = iReg; // Todo: add value for #i
        loop.vReg = chrReg; // Todo: add value for #i
        loop.jumpReg = jumpReg;
        loop.continueConstant = continueAddress;
        loop.breakConstant = continueAddress;
        loop.scopeIndex = info.scopes.size()-1;

        int result2 = ParseBody(info,unusedReg);
        if(result2!=PARSE_SUCCESS){
            return PARSE_ERROR;
        }
        
        info.code.getConstNumber(continueAddress)->value = info.code.length();
        _GLOG(log::out << log::GREEN<<"  : CONTINUE/BREAK ADDRESS ["<<continueAddress<<"]\n")
        
        info.code.add(BC_DEL,chrReg);
        _GLOG(INST << "\n";)

        info.loopScopes.erase(info.loopScopes.begin()+info.loopScopes.size()-1);
        
        info.code.getConstNumber(lastAddress)->value = info.code.length();
        _GLOG(log::out << log::GREEN<<"  : BEGIN ADDRESS ["<<lastAddress<<"]\n")

        info.code.add(BC_ADD,iReg,REG_ZERO,beginReg);
        _GLOG(INST << "\n";)

        info.code.addLoadNC(jumpReg,startAddress);
        _GLOG(INST << "\n";)
        info.code.add(BC_JUMP,jumpReg);
        _GLOG(INST << "\n";)
        
        info.code.getConstNumber(endAddress)->value = info.code.length();
        _GLOG(log::out << log::GREEN<<"  : END ADDRESS ["<<endAddress<<"]\n")

        info.dropScope();
    } else {
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }else{
            ERR_GENERIC;
            ERRLINE
            // info.nextLine();
            return PARSE_ERROR;
        }
    }
    _GLOG(EXIT)
    return PARSE_SUCCESS;
}
int ParseAssignment(ParseInfo& info, ExpressionInfo& exprInfo, bool attempt, bool requestCopy){
    using namespace engone;
    
    _GLOG(TRY)
    
    if(info.tokens.length() < info.index+2){
        // not enough tokens for assignment
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }
        _GLOG(log::out <<log::RED<< "CompilerError: to few tokens for assignment\n");
        // info.nextLine();
        return PARSE_ERROR;
    }
    Token name = info.get(info.at()+1);
    Token assign = info.get(info.at()+2);
    Token token = info.get(info.at()+3);
    
    // Todo: handle ++a
    
    if(assign=="++"||assign=="--"){
        // a++
        auto find = info.variables.find(name);
        if(find==info.variables.end()){
            if(attempt){
                return PARSE_BAD_ATTEMPT;
            }
            ERRT(name) << "variable '"<<name<<"' must be defined when using '"<<assign<<"'\n";
            return PARSE_ERROR;   
        }
        ParseInfo::Variable& var = find->second;
        info.next();
        info.next();
        // Todo: a=9;b = a++ <- b would be 10 with current code. Change it to
        //  make a copy which b is set to then do addition on variable.
        //  how does it work with requestCopy?
        int finalReg = exprInfo.acc0Reg;
        info.code.add(BC_LOADV,finalReg,var.frameIndex);
        _GLOG(INST << "load var '"<<name<<"'\n";)
        int constone = info.code.addConstNumber(1);
        info.code.add(BC_NUM,finalReg+1);
        info.code.addLoadNC(finalReg+1,constone);
        if(assign == "++")
            info.code.add(BC_ADD,finalReg,finalReg+1,finalReg);
        if(assign == "--")
            info.code.add(BC_SUB,finalReg,finalReg+1,finalReg);
        _GLOG(INST <<"\n";)
        info.code.add(BC_DEL,finalReg+1);
        _GLOG(INST <<"\n";)
        
        if(requestCopy){
            info.code.add(BC_COPY,finalReg,finalReg);
            _GLOG(INST << "copy requested\n";)
            _GLOG(EXIT)
            return PARSE_SUCCESS;
        }
        
        return PARSE_SUCCESS;
    }
    
    if(!IsName(name)){
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }else{
            info.next();
            ERRT(name) << "unexpected "<<name<<", wanted a variable for assignment\n";
            // info.nextLine();
            return PARSE_ERROR;
        }
    }
    if(assign=="="&&token=="{"){
        info.next();
        info.next();
        info.next();
        auto funcPair = info.functions.find(token);
        auto varPair = info.variables.find(token);
        if(varPair != info.variables.end()){
            ERRT(token) << "'"<<token<<"' already defined as variable\n";
            // info.nextLine();
            return PARSE_ERROR;
        }else if(funcPair==info.functions.end()){
            // Note: assign new variable
            ParseInfo::Function& func = info.functions[token] = {};
            int constIndex = info.code.addConstNumber(-1);
            int jumpReg = exprInfo.acc0Reg;
            info.code.add(BC_NUM,jumpReg);
            _GLOG(INST << "jump\n";)
            info.code.addLoadNC(jumpReg,constIndex);
            _GLOG(INST << "\n";)
            info.code.add(BC_JUMP, jumpReg);
            _GLOG(INST << "Skip function body '"<<token<<"'\n";)
            
            func.jumpAddress = info.code.length();

            info.makeScope();
            info.funcScopes.push_back({(int)info.scopes.size()-1});

            // token = info.next will be {
            int result = ParseBody(info,exprInfo.acc0Reg+1);

            info.dropScope();
            // Todo: consider not returning when failed since the
            //  final code would have a higher chance of executing
            //  correctly with the "tailing" instructions below.
            // Todo: remove the function from the map on failure.
            //  It shouldn't exist since we failed.
            if(result!=PARSE_SUCCESS){
                return PARSE_ERROR;
            }

            info.code.add(BC_RETURN,REG_NULL);
            _GLOG(INST << "\n";)

            info.code.getConstNumber(constIndex)->value = info.code.length();
            info.code.add(BC_DEL,jumpReg);
            _GLOG(INST << "skipped to here\n";)

            _GLOG(EXIT)
            return PARSE_NO_VALUE;
        }else{
            ERRT(token) << "function '"<<token<<"' already defined\n";
            // info.nextLine();
            return PARSE_ERROR;
        }
        _GLOG(EXIT)
        return PARSE_SUCCESS;
    }
    
    if(!(assign=="=" ||assign=="+=" ||assign=="-=" ||assign=="*=" ||assign=="/=")){
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }else{
            info.next(); // move forward to prevent infinite loop? altough nextline would fix it?
            ERRT(name) << "unexpected "<<name<<", wanted a variable for assignment\n";
            // info.nextLine();
            return PARSE_ERROR;
        }
    }
    info.next();
    info.next();
    // ExpressionInfo expr{};
    // expr.acc0Reg = REG_ACC0;
    int result=0;

    result = ParseExpression(info,exprInfo,false);
    if(result!=PARSE_SUCCESS){
        return PARSE_ERROR;
    }
    int finalReg = exprInfo.acc0Reg;
    if(exprInfo.regCount==0){
        log::out << log::RED<<"CompilerWarning: regCount == 0 after expression from assignment\n";   
    }
            
    auto find = info.variables.find(name);
    ParseInfo::Variable* var = 0;
    if(find==info.variables.end()){
        if(assign != "="){
            ERRT(name) << "variable '"<<name<<"' must be defined when using '"<<assign<<"'\n";
            return PARSE_ERROR;   
        }
        var = &(info.variables[name] = {});
        // Note: assign new variable
        var->frameIndex = info.frameOffsetIndex++;
        // Todo: bound check on scopes, global scope should exist though
        auto& scope = info.scopes.back();
        scope.variableNames.push_back(name);
    }else{
        var = &find->second;
    }
    if(assign=="="){
        info.code.add(BC_STOREV, finalReg, var->frameIndex);
        _GLOG(INST << "store "<<name<<"\n";)
    } else {
        info.code.add(BC_LOADV,finalReg+1,var->frameIndex);
        _GLOG(INST << "load var '"<<name<<"'\n";)
        if(assign == "+=")
            info.code.add(BC_ADD,finalReg+1,finalReg,finalReg+1);
        if(assign == "-=")
            info.code.add(BC_SUB,finalReg+1,finalReg,finalReg+1);
        if(assign == "*=")
            info.code.add(BC_MUL,finalReg+1,finalReg,finalReg+1);
        if(assign == "/=")
            info.code.add(BC_DIV,finalReg+1,finalReg,finalReg+1);
        _GLOG(INST <<"\n";)
        info.code.add(BC_DEL,finalReg);
        _GLOG(INST <<"\n";)
        info.code.add(BC_MOV,finalReg+1,finalReg);
        _GLOG(INST <<"\n";)
    }
        
    if(requestCopy){
        info.code.add(BC_COPY,finalReg,finalReg);
        _GLOG(INST << "copy requested\n";)
        _GLOG(EXIT)
        return PARSE_SUCCESS;
    }
    _GLOG(EXIT)
    return PARSE_SUCCESS;   
}
int ParseCommand(ParseInfo& info, ExpressionInfo& exprInfo, bool attempt){
    using namespace engone;
    _GLOG(TRY)
    
    if(info.end()){
        log::out <<log::RED<< "Sudden end in ParseCommand?";
        return PARSE_ERROR;
    }
    // Token token = info.next();
    
    Token varName = info.get(info.at()+1);
    auto var = info.getVariable(varName);
    if(var){
        info.next();
        Token token = info.get(info.at()+1);
        int instruction = 0;
        if(token=="<") instruction = BC_READ_FILE;
        else if(token==">") instruction = BC_WRITE_FILE;
        else if(token==">>") instruction = BC_APPEND_FILE;
        if(instruction){
            info.next();
            
            int varReg = exprInfo.acc0Reg;
            int fileReg = exprInfo.acc0Reg+1;
            
            info.code.add(BC_LOADV,varReg,var->frameIndex);
            _GLOG(INST << varName<<"\n";)
            
            token = info.get(info.at()+1);
            auto fileVar = info.getVariable(token);
            if(fileVar){
                info.next();
                info.code.add(BC_LOADV,fileReg,fileVar->frameIndex);
                _GLOG(INST << varName<<"\n";)
                info.code.add(instruction,varReg,fileReg);
                _GLOG(INST << "\n";)
            }else{
                token = CombineTokens(info); // read file
                // we assume it's valid?
            
                info.code.add(BC_STR,fileReg);
                _GLOG(INST << "\n";)
                int constIndex = info.code.addConstString(token);
                info.code.addLoadSC(fileReg,constIndex);
                _GLOG(INST << token<<"\n";)
                
                info.code.add(instruction,varReg,fileReg);
                _GLOG(INST << "\n";)
                info.code.add(BC_DEL,fileReg);
                _GLOG(INST << "\n";)
            }
            // Note: PARSE_SUCCESS would indicated varReg (eg, variable value) to be "returned".
            //   That's maybe not quite right since we would need to copy the value otherwise
            //   we would have two variables pointing to the same value.
            return PARSE_NO_VALUE;
        }
        else{
            ERRT(token) << "Expected piping (not '"<<token<<"', | not supported yet) after variable '"<<varName<<"'\n";
            return PARSE_ERROR;   
        }
           
    }
    
    Token token = CombineTokens(info);
    Token cmdName = token;
    if(!token.str){
        // info.errors++;
        // log::out <<log::RED <<"DevError ParseCommand, token.str was null\n";
        // return PARSE_ERROR;
        token = info.next(); 
        // we didn't get a token from CombineTokens but we have to move forward
        // to prevent infinite loop.
        return PARSE_NO_VALUE;
        // succeeded with doing nothing
        // happens with a single ; in body parsing.

    }
    
    if(token.flags&TOKEN_QUOTED){
        // quoted exe command
    }else if(IsName(token)){
        // internal function
    } else{
        // raw exe command
    }
    int funcReg = exprInfo.acc0Reg+exprInfo.regCount;
    exprInfo.regCount++;
    auto funcPair = info.functions.find(token);
    // bool codeFunc=false;
    if(funcPair!=info.functions.end()){
        // func in bytecode
        ParseInfo::Function& func = funcPair->second;
        if(func.constIndex==-1){
            func.constIndex = info.code.addConstNumber(func.jumpAddress);
        }
        info.code.add(BC_NUM,funcReg);
        _GLOG(INST << "\n";)
        info.code.addLoadNC(funcReg,func.constIndex);
        _GLOG(INST << "index to '"<<token<<"'\n";)
        // codeFunc=true;

    }else{
        // executable or internal func
        int index = info.code.addConstString(token);
        info.code.add(BC_STR,funcReg);
        _GLOG(INST << "name\n";)
        info.code.addLoadSC(funcReg, index);
        _GLOG(INST << "constant "<<token<<"\n";)
    }
    
    int argReg = 0;
    
    bool ending = (token.flags&TOKEN_SUFFIX_LINE_FEED) | info.end();
    if(!ending){
        
        int tempStrReg = exprInfo.acc0Reg+exprInfo.regCount+1;
        info.code.add(BC_STR,tempStrReg);
        _GLOG(INST << "temp str\n";)
        
        int tempReg = exprInfo.acc0Reg+exprInfo.regCount+2;
        
        bool addSpace=false;
        while(!info.end()){
            Token prev = info.now();
            token = info.next();
            // log::out << "TOK: '"<<token<<"'\n";
            #define ARG_STR 1
            #define ARG_TEMP 2
            #define ARG_DEL 4
            int whichArg=0;
            if(token=="#"&&!(token.flags&TOKEN_QUOTED)){
                if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED)){
                    ERRTOK << "expected a directive\n";
                    return PARSE_ERROR;
                }
                token = info.next();
                if(token == "run"){
                    ExpressionInfo expr{};
                    expr.acc0Reg = tempReg;
                    int result = ParseCommand(info,expr,false);
                    if(result==PARSE_SUCCESS){
                        if(expr.regCount!=0){
                            whichArg=ARG_TEMP|ARG_DEL;
                        }
                    }else{
                        return PARSE_ERROR;
                    }
                }else if(token=="arg"){
                    info.code.add(BC_MOV,REG_ARGUMENT,tempReg);
                    _GLOG(INST << "\n";)
                    whichArg=ARG_TEMP;
                }else if(token == "i"){
                    if(info.loopScopes.size()==0){
                        ERRTOK << "must be in a loop to use '#"<<token<<"'\n";
                        // info.nextLine();
                        return PARSE_ERROR;
                    }else{
                        info.code.add(BC_MOV,info.loopScopes.back().iReg,tempReg);
                        _GLOG(INST << "\n";)
                        whichArg=ARG_TEMP;
                    }
                }else if(token == "v"){
                    if(info.loopScopes.size()==0){
                        ERRTOK << "must be in a loop to use '#"<<token<<"'\n";
                        // info.nextLine();
                        return PARSE_ERROR;
                    }else{
                        info.code.add(BC_MOV,info.loopScopes.back().vReg,tempReg);
                        _GLOG(INST << "\n";)
                        whichArg=ARG_TEMP;
                    }
                }else{
                    ERRTOK << "undefined directive '"<<token<<"'\n";
                    // info.nextLine();
                    return PARSE_ERROR;
                }
            } else if(token=="("){
                // Todo: parse expression
                ExpressionInfo expr{};
                expr.acc0Reg = tempReg;
                // log::out<<"Par ( \n";
                info.parDepth++;
                int result = ParseExpression(info,expr,false);
                info.parDepth--;
                if(result==PARSE_SUCCESS){
                    if(expr.regCount!=0){
                       whichArg=ARG_TEMP|ARG_DEL;
                    }
                }else{
                    return PARSE_ERROR;
                }
            } else if(token==";"||token==")") {
                // argReg = exprInfo.acc0Reg+exprInfo.regCount;
                // info.code.add(BC_STR,argReg);
                // _GLOG(INST << "arg\n";)
                // if(info.spaceConstIndex==-1){
                //     Token temp{};
                //     char tempStr[]=" ";
                //     temp.str = tempStr;
                //     temp.length = 1;
                //     info.spaceConstIndex = info.code.addConstString(temp);
                // }
                // int spaceReg = tempReg2+1;
                // // Todo: This is flawed. Space is added before when it should be added afterwards.
                // if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED)){
                //     info.code.add(BC_STR,spaceReg);
                //     _GLOG(INST << "\n";)
                //     info.code.addLoadC(spaceReg,info.spaceConstIndex);
                //     _GLOG(INST << "constant ' '\n";)
                //     info.code.add(BC_ADD,argReg,spaceReg,argReg);
                //     _GLOG(INST << "extra space\n";)
                //     info.code.add(BC_DEL,spaceReg);
                //     _GLOG(INST << "\n";)
                // }
                break;
            } else{
                // Todo: find variables
                if(!(token.flags&TOKEN_QUOTED)&&IsName(token)){
                    auto varPair = info.variables.find(token);
                    if(varPair!=info.variables.end()){
                        info.code.add(BC_LOADV,tempReg,varPair->second.frameIndex);
                        _GLOG(INST << "get variable "<<varPair->second.frameIndex<<"\n";)

                        // . for property, eg. type, length
                        // [] for substring [2] single character, [2:3] an inclusive range of character
                        

                        
                        whichArg = ARG_TEMP;
                        goto CMD_ACC; // Note: careful when changing behaviour, goto can be wierd
                    }
                    // not variable? then continue below
                }
                
                info.revert(); // is revert okay here? will it mess up debug lines?
                token = CombineTokens(info);
                const char* padding=0;
                // if(info.now().flags&TOKEN_SUFFIX_SPACE)
                //     padding = " ";
                
                // log::out << "tok: "<<combinedTokens<<"\n";
                int constIndex = info.code.addConstString(token,padding);
                // printf("eh %d\n",constIndex);
                info.code.addLoadSC(tempStrReg, constIndex);
                _GLOG(INST << "constant '"<<token<<"'\n";)
                whichArg = ARG_STR;
            }
            CMD_ACC:
            // Todo: if there is just one arg you can skip some ADD, STR/NUM and DEL instructions
            //  and directly use the number or string since there's no need for accumulation/concatenation
            if(argReg!=0){
                if(info.spaceConstIndex==-1){
                    Token temp{};
                    char tempStr[]=" ";
                    temp.str = tempStr;
                    temp.length = 1;
                    info.spaceConstIndex = info.code.addConstString(temp);
                }
                int spaceReg = tempReg+1;
                // Todo: This is flawed. Space is added before when it should be added afterwards.
                
                addSpace = (prev.flags&TOKEN_SUFFIX_SPACE) || (prev.flags&TOKEN_SUFFIX_LINE_FEED);
                if(addSpace){
                    info.code.add(BC_STR,spaceReg);
                    _GLOG(INST << "\n";)
                    info.code.addLoadSC(spaceReg,info.spaceConstIndex);
                    _GLOG(INST << "constant ' '\n";)
                    info.code.add(BC_ADD,argReg,spaceReg,argReg);
                    _GLOG(INST << "extra space\n";)
                    info.code.add(BC_DEL,spaceReg);
                    _GLOG(INST << "\n";)
                }
                // if((token.flags&TOKEN_SUFFIX_SPACE) || (token.flags&TOKEN_SUFFIX_LINE_FEED)){
                //     info.code.add(BC_STR,spaceReg);
                //     _GLOG(INST << "'\n";)
                //     info.code.addLoadC(spaceReg,info.spaceConstIndex);
                //     _GLOG(INST << "constant ' '\n";)
                //     info.code.add(BC_ADD,argReg,spaceReg,argReg);
                //     _GLOG(INST << "'\n";)
                //     info.code.add(BC_DEL,spaceReg);
                //     _GLOG(INST << "'\n";)
                // }
            }
            
            // Todo: handle properties and substring here too. Not only in ParseExpression.
            //  It's not just a copy and paste though, tempReg and ARG_TEMP, ARG_DEL needs to
            //  be taken into account. If more properties are added then you would need to change
            //  it in both places so making a function out of it would be a good idea.
            
            if(argReg==0&&whichArg!=0){
                argReg = exprInfo.acc0Reg+exprInfo.regCount;
                info.code.add(BC_STR,argReg);
                _GLOG(INST << "arg\n";)
            }
            if(whichArg&ARG_STR){
                info.code.add(BC_ADD,argReg, tempStrReg, argReg);
                _GLOG(INST << "\n";)
            }
            if(whichArg&ARG_TEMP){
                info.code.add(BC_ADD,argReg, tempReg, argReg);
                _GLOG(INST << "\n";)
            }
            if(whichArg&ARG_DEL){
                info.code.add(BC_DEL,tempReg);
                _GLOG(INST << "\n";)
            }
            token = info.now();
            if(token.flags&TOKEN_SUFFIX_LINE_FEED){
                break;
            }
        }
        info.code.add(BC_DEL,tempStrReg);
        _GLOG(INST << "temp\n";)
    }
    
    // if(codeFunc){
    //     info.makeScope();
    //     info.funcScopes.push_back({(int)info.scopes.size()-1});
    // }
    info.code.add(BC_RUN,argReg,funcReg);
    _GLOG(INST << "run "<<cmdName<<"\n";)
    // if(codeFunc){
    //     info.dropScope();
    // }

    info.code.add(BC_DEL,funcReg);
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
    
    _GLOG(EXIT)
    return PARSE_SUCCESS;
}
int ParseBody(ParseInfo& info, int acc0reg){
    using namespace engone;
    // Note: two infos in case ParseAssignment modifies it and then fails.
    //  without two, ParseCommand would work with a modified info.
    _GLOG(ENTER)

    if(info.end()){
        log::out << log::YELLOW<<"ParseBody: sudden end?\n";
        return PARSE_ERROR;
    }
    Token token = info.get(info.at()+1);
    if(token=="{"){
        token = info.next();
        while(!info.end()){
            Token& token = info.get(info.at()+1);
            if(token=="}"){
                info.next();
                break;
            }
            
            ExpressionInfo expr{};
            expr.acc0Reg = acc0reg;
            
            int result = ParseFlow(info,acc0reg,true);
            if(result==PARSE_BAD_ATTEMPT)
                result = ParseAssignment(info,expr,true, false);
            if(result==PARSE_SUCCESS)
                continue;
            if(result==PARSE_BAD_ATTEMPT)
                result = ParseCommand(info,expr,true);

            if(result==PARSE_SUCCESS){
                info.code.add(BC_DEL,expr.acc0Reg);
                _GLOG(INST << "\n";)
                expr.regCount--;
            } else if(result==PARSE_NO_VALUE){
                
            } else if(result==PARSE_BAD_ATTEMPT){
                if(!info.end()&&info.index>0){
                    ERRT(info.now()) << "unexpected "<<info.now()<<"\n";
                }else {
                    ERRT(info.get(0)) << "unexpected "<<info.get(0)<<"\n";
                }
                info.nextLine();
                return PARSE_ERROR;
            }else {
                log::out << log::RED<<"Fail\n";
                info.nextLine();
                // Error ? do waht?
                // Todo: we may fail because current token is unexpected or we may fail because
                //  some other reason deep within the recursive parsing. Somehow we need to know
                //  what happened so don't print a message if another function already has.
            }
        }
    }else {
        ExpressionInfo expr{};
        expr.acc0Reg = acc0reg;
        
        int result = ParseFlow(info,acc0reg,true);
        if(result==PARSE_BAD_ATTEMPT){
            result = ParseAssignment(info,expr,true, false);
        }
        if(result==PARSE_SUCCESS){
            // ParseFlow and ParseAssignment will not return a value
            // and we don't want to delete any potential ones so we skip
            // the BC_DEL below.
            goto Scope_123; // Forgive me programmer god for I have sinned
        }
        if(result==PARSE_BAD_ATTEMPT)
            result = ParseCommand(info,expr,true);
        
        if(result==PARSE_BAD_ATTEMPT){
            ERRT(info.now()) << "Bad attempt '"<<info.now()<<"'\n";
            return PARSE_ERROR;
        }else if(result==PARSE_NO_VALUE){

        }else if(result==PARSE_SUCCESS){
            info.code.add(BC_DEL,expr.acc0Reg);
            _GLOG(INST << "\n";)
            expr.regCount--;
        }else {
            // if(!info.end()&&info.index>0){
            //     ERRT(info.now()) << "unexpected "<<info.now()<<"\n";
            // }else {
            //     ERRT(info.get(0)) << "bad something?\n";
            // }
            return PARSE_ERROR;
        }
    }
    Scope_123:
    _GLOG(EXIT)
    return PARSE_SUCCESS;
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
Bytecode GenerateScript(Tokens& tokens, int* outErr){
    using namespace engone;
    _SILENT(log::out <<log::BLUE<<  "\n##   Parser   ##\n";)
    
    ParseInfo info{tokens};
    
    // Todo: some better way to define this since they have to be defined here and in context.
    //  Better to define in one place.
    // ProvideDefaultCalls(info.externalCalls);
    
    info.makeScope();
    while (!info.end()){
        // info.addDebugLine(info.index);
        // #ifdef USE_DEBUG_INFO
        // _GLOG(log::out <<"\n"<<*((Bytecode::DebugLine*)info.code.debugLines.data+info.code.debugLines.used-1)<<"\n";)
        // #endif
        int result = ParseBody(info, REG_ACC0);
        if(result!=PARSE_SUCCESS){
            info.nextLine();
            // skip line or token until successful
        }
    }
    info.addDebugLine("(End of global scope)");
    info.dropScope();
    
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
    
    
    
    if(info.variables.size()!=0){
        log::out << log::YELLOW<<"ParseWarning: "<<info.variables.size()<<" variables remain?\n";

    }
    // we drop scope so we don't need this anymore
    // for(auto& pair : info.variables){
    //     // log::out << "del "<<pair.first<<"\n";
    //     info.code.add(BC_LOADV,REG_ACC0,pair.second.frameIndex);
    //     info.code.add(BC_DEL,REG_ACC0);
    // }

    if(outErr){
        *outErr = info.errors;
    }
    if(info.errors)
        log::out << log::RED<<"Generator failed with "<<info.errors<<" errors\n";
    
    info.code.finalizePointers();
    #ifdef USE_DEBUG_INFO
    _GLOG(log::out <<*((Bytecode::DebugLine*)info.code.debugLines.data+info.code.debugLines.used-1);)
    #endif
    return info.code;
}
#define ARG_REG 1
#define ARG_CONST 2
#define ARG_NUMBER 4
#define ARG_STRING 8
// regIndex: index of the register in the instruction
bool GenInstructionArg(ParseInfo& info, int instType, int& num, int flags, uint8 regIndex){
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
                // else if(token == REG_STACK_POINTER_S)
                //     num = REG_STACK_POINTER;
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
            if(instType!=BC_LOADNC){
                // resolve other instruction like jump by using load const
                num = REG_ACC0 + regIndex;
                int index=0;
                // info.code.add(BC_LOADC,num);
                // info.code.add(*(Instruction*)&index);
                
                info.code.addLoadNC(num,index);

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
            info.code.addLoadNC(num,index);
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

            info.code.addLoadSC(num,index);
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
bool LineEndError(ParseInfo& info){
    using namespace engone;
    Token now = info.now();
    if(0==(now.flags&TOKEN_SUFFIX_LINE_FEED) && !info.end()){
        ERRTL(now) << "Expected line feed found "<<info.now()<<"\n";
        info.nextLine();
        return true;
    }
    return false;
}
Bytecode GenerateInstructions(Tokens& tokens, int* outErr){
    using namespace engone;
    _SILENT(log::out<<"\n##   Generator (instructions)   ##\n";)
    
    std::unordered_map<std::string,int> instructionMap;
    #define MAP(K,V) instructionMap[K] = V;
    
    MAP("add",BC_ADD)
    MAP("sub",BC_SUB)
    MAP("mul",BC_MUL)
    MAP("div",BC_DIV)
    MAP("less",BC_LESS)
    MAP("greater",BC_GREATER)
    MAP("equal",BC_EQUAL)
    MAP("nequal",BC_NOT_EQUAL)
    MAP("and",BC_AND)
    MAP("or",BC_OR)
    
    MAP("jumpnif",BC_JUMPNIF)

    MAP("mov",BC_MOV)
    MAP("copy",BC_COPY)
    MAP("run",BC_RUN)
    MAP("loadv",BC_LOADV)
    MAP("storev",BC_STOREV)

    MAP("num",BC_NUM)
    MAP("str",BC_STR)
    MAP("del",BC_DEL)
    MAP("jump",BC_JUMP)
    MAP("loadc",BC_LOADNC)
    // MAP("push",BC_PUSH)
    // MAP("pop",BC_POP)
    MAP("return",BC_RETURN)
    // MAP("enter",BC_ENTERSCOPE)
    // MAP("exit",BC_EXITSCOPE)

    ParseInfo info{tokens};

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
            info.revert();
            
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
            
            if(instType == BC_LOADNC||instType==BC_LOADSC){
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
        if(inst.type==BC_LOADNC||inst.type==BC_LOADSC){
            inst.reg1 = index&0xFF;
            inst.reg2 = (index>>8)&0xFF;
            // inst = *(Instruction*)&index;

        }else{
            ERRT(addr.token)<<"Cannot resolve\n";
        }
    }

    return info.code;
}

std::string Disassemble(Bytecode& code){
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

    // auto printNumConst
    
    #define REF_NUM 1
    #define REF_STR 2
    int refTypes[256]{0};

    // struct Location{
    //     int pc = 0;

    // };
    // std::vector<Location> locations;

    // int consts
    
    // int programCounter = 0;
    // while(true){
    //     if(programCounter>=code.length())
    //         break;
    //     Instruction& inst = code.get(programCounter++);
        
    //     if(inst.type==BC_LOADC){
    //         // uint extraData = *(uint*)&code.get(programCounter);
    //         uint extraData = (uint)inst.reg1|((uint)inst.reg2<<8);
    //         locations.push_back({pc-1,extraData});
    //         // if(refTypes[inst.reg0]==REF_STR)
    //         //     buffer += " str_";
    //         // else
    //         //     buffer += " num_";
    //         // buffer+=std::to_string(extraData);
    //     }
    // }

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
            // refTypes[REG_RETURN_VALUE] = refTypes[inst.];
        }else if(inst.type==BC_RUN){
            
        }
        
        const char* name = InstToString(inst.type); // Todo: load, add instead of BC_LOAD, BC_ADD
        buffer += "    ";
        buffer += name;
        int regCount = (inst.type&BC_MASK)>>6;
        // if(inst.type==BC_JUMP){

        // }
        for(int i=0;i<regCount;i++){
            buffer += " ";
            buffer += "$";
            buffer += std::to_string(inst.regs[i]);
        }

        if(inst.type==BC_LOADNC){
            // uint extraData = *(uint*)&code.get(programCounter);
            uint extraData = (uint)inst.reg1|((uint)inst.reg2<<8);
            buffer += " num_";
            buffer+=std::to_string(extraData);
        }
        if(inst.type==BC_LOADSC){
            // uint extraData = *(uint*)&code.get(programCounter);
            uint extraData = (uint)inst.reg1|((uint)inst.reg2<<8);
            buffer += " str_";
            buffer+=std::to_string(extraData);
        }
        buffer +="\n";
    }   
    return buffer; 
}