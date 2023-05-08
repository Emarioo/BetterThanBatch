#include "BetBat/GenParser.h"

#undef ERRT
#undef ERRLINE

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

// #define CASE(x) case x: return #x;
// const char* ParseResultToString2(int result){
//     switch(result){
//         CASE(PARSE_SUCCESS)
//         CASE(PARSE_NO_VALUE)
//         CASE(PARSE_ERROR)
//         CASE(PARSE_BAD_ATTEMPT)
//     }
//     return "?";
// }
void GenParseInfo::makeScope(){
    using namespace engone;
    _PLOG(log::out << log::GRAY<< "   Enter scope "<<scopes.size()<<"\n";)
    scopes.push_back({});
}
int GenParseInfo::Scope::getInstsBeforeDelReg(GenParseInfo& info){
    int offset=0;
    for(Token& token : variableNames){
        auto var = info.getVariable(token);
        if(var){
            offset+=1; // based on drop scope
        }
    }
    return offset;
}
void GenParseInfo::Scope::removeReg(int reg){
    for(int i = 0;i<(int)delRegisters.size();i++){
        if(delRegisters[i]==reg){
            delRegisters.erase(delRegisters.begin()+i);
            break;
        }
    }   
}
void GenParseInfo::dropScope(int index){
    using namespace engone;
    if(index==-1){
        _PLOG(log::out << log::GRAY<<"   Exit  scope "<<(scopes.size()-1)<<"\n";)
    }
    Scope& scope = index==-1? scopes.back() : scopes[index];
    auto& info = *this;
    for(Token& token : scope.variableNames){
        auto var = getVariable(token);
        if(var){
            // NOTE: don't forget to change offset in variable cleanup  count
            //  offset should be however many instruction you add here!
            // deletes previous value
            code.add(BC_STOREV,REG_NULL,var->frameIndex);
            _PLOG(INST << "'"<<token<<"'\n";)
            if(index==-1)
                removeVariable(token);
        }
    }
    for(Token& token : scope.functionsNames){
        auto pair = functions.find(token);
        if(pair!=functions.end()){
            if(index==-1)
                functions.erase(pair);
        }
    }
    for(int reg : scope.delRegisters){
        code.add(BC_DEL,reg);
        _PLOG(INST << "\n";)
    }
    if(index==-1)
        scopes.pop_back();
}
GenParseInfo::Variable* GenParseInfo::getVariable(const std::string& name){
    auto pair = globalVariables.find(name);
    if(pair!=globalVariables.end())
        return &pair->second;
    
    if(currentFunction.empty())
        return 0;
        
    auto func = getFunction(currentFunction);
    auto pair2 = func->variables.find(name);
    if(pair2!=func->variables.end()) 
        return &pair2->second;
    return 0;
}
void GenParseInfo::removeVariable(const std::string& name){
    auto pair = globalVariables.find(name);
    if(pair!=globalVariables.end()){
        globalVariables.erase(pair);
    }
    if(currentFunction.empty())
        return;
        
    auto func = getFunction(currentFunction);
    auto pair2 = func->variables.find(name);
    if(pair2!=func->variables.end()) 
        func->variables.erase(pair2);
}
GenParseInfo::Variable* GenParseInfo::addVariable(const std::string& name){
    if(currentFunction.empty()){
        return &(globalVariables[name] = {});   
    }
    auto func = getFunction(currentFunction);
    return &(func->variables[name] = {});
}
GenParseInfo::Function* GenParseInfo::getFunction(const std::string& name){
    auto pair = functions.find(name);
    if(pair==functions.end())
        return 0;
    return &pair->second;
}
GenParseInfo::Function* GenParseInfo::addFunction(const std::string& name){
    return &(functions[name] = {});
}
// returns instruction type
// zero means no operation
int IsOperation(Token& token){
    if(token.flags&TOKEN_QUOTED) return 0;
    if(token=="+") return BC_ADD;
    if(token=="-") return BC_SUB;
    if(token=="*") return BC_MUL;
    if(token=="/") return BC_DIV;
    if(token=="<") return BC_LESS;
    if(token==">") return BC_GREATER;
    if(token=="<=") return BC_LESS_EQ;
    if(token==">=") return BC_GREATER_EQ;
    if(token=="==") return BC_EQUAL;
    if(token=="!=") return BC_NOT_EQUAL;
    if(token=="&&") return BC_AND;
    if(token=="||") return BC_OR;
    if(token=="%") return BC_MOD;
    return 0;
}
Token& GenParseInfo::next(){
    Token& temp = tokens.get(index++);
    // if(temp.flags&TOKEN_SUFFIX_LINE_FEED||index==1){
        if(code.debugLines.used==0){
            if(addDebugLine(index-1)){
                Bytecode::DebugLine* line = (Bytecode::DebugLine*)code.debugLines.data+code.debugLines.used-1;
                uint64 offset = (uint64)line->str;
                line->str = (char*)code.debugLineText.data + offset;
                // line->str doesn't point to debugLineText yet since it may resize.
                // we do some special stuff to deal with it.
                _PLOG(engone::log::out <<"\n"<<*line<<"\n";)
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
                    _PLOG(engone::log::out <<"\n"<<*line<<"\n";)
                    line->str = (char*)offset;
                }
            }
        }
    // }
    return temp;
}
bool GenParseInfo::revert(){
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
Token& GenParseInfo::prev(){
    return tokens.get(index-2);
}
Token& GenParseInfo::now(){
    return tokens.get(index-1);
}
Token& GenParseInfo::get(uint _index){
    return tokens.get(_index);
}
bool GenParseInfo::end(){
    Assert(index<=tokens.length());
    return index==tokens.length();
}
void GenParseInfo::finish(){
    index = tokens.length();
}
int GenParseInfo::at(){
    return index-1;
}
void GenParseInfo::printLine(){
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
bool GenParseInfo::addDebugLine(uint tokenIndex){
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
bool GenParseInfo::addDebugLine(const char* str, int lineIndex){
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
void GenParseInfo::nextLine(){
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
    if(op==BC_LESS||op==BC_GREATER||op==BC_LESS_EQ||op==BC_GREATER_EQ
        ||op==BC_EQUAL||op==BC_NOT_EQUAL) return 5;
    if(op==BC_ADD||op==BC_SUB) return 9;
    if(op==BC_MUL||op==BC_DIV||op==BC_MOD) return 10;
    return 0;
}
// Concatenates current token with the next tokens not seperated by space, linefeed or such
// operators and special characters are seen as normal letters
Token CombineTokens(GenParseInfo& info){
    using namespace engone;
        
    Token outToken{};
    while(!info.end()) {
        Token token = info.get(info.at()+1);
        // Todo: stop at ( # ) and such
        if(token==";"&& !(token.flags&TOKEN_QUOTED)){
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

int ParseAssignment(GenParseInfo& info, ExpressionInfo& exprInfo, bool attempt, bool requestCopy);
int ParseCommand(GenParseInfo& info, ExpressionInfo& exprInfo, bool attempt);

// #fex
int ParseExpression(GenParseInfo& info, ExpressionInfo& exprInfo, bool attempt){
    using namespace engone;
    
    _PLOG(TRY)
    
    // if(ParseAssignment(info,exprInfo,true)){
    //     _PLOG(log::out << "-- exit ParseExpression\n";)
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
                _PLOG(log::out << "Operator "<<token<<"\n";)
            } else if(token == ";"&& !(token.flags&TOKEN_QUOTED)){
                token = info.next();
                ending = true;
            } else {
                ending = true;
                // info.revert();
            }
        }else{
            // if(token == "null"){
            //     info.code.add(BC_MOV,REG_NULL,exprInfo.acc0Reg+exprInfo.regCount);
            //     _PLOG(INST << "\n";)
            //     exprInfo.regCount++;
            // } else
            bool no=false;
            if(token=="-"&& !(token.flags&TOKEN_QUOTED)){
                Token tok = info.get(info.at()+2);
                if(IsDecimal(tok)){
                    no = true;
                    token = info.next();
                    token = info.next();
                    // Note the - before ConvertDecimal
                    int constIndex = info.code.addConstNumber(-ConvertDecimal(token));
                    info.code.add(BC_NUM,exprInfo.acc0Reg+exprInfo.regCount);
                    _PLOG(INST << "\n";)
                    info.code.addLoadNC(exprInfo.acc0Reg+exprInfo.regCount,constIndex);
                    _PLOG(INST << "constant "<<token<<"\n";)
                    exprInfo.regCount++;
                }else{
                    // bad
                }
            }
            if(token=="!"&& !(token.flags&TOKEN_QUOTED)){
                token = info.next();
                hadNot=true;
                continue;   
            }
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
                    if(token == "run"&& !(token.flags&TOKEN_QUOTED)){
                        // token = info.next();
                        ExpressionInfo expr{};
                        expr.acc0Reg = exprInfo.acc0Reg + exprInfo.regCount;
                        int success = ParseCommand(info,expr,false);
                        if(success){
                            exprInfo.regCount++;
                        }else{
                            return PARSE_ERROR;
                        }
                    }else if(token == "async"&& !(token.flags&TOKEN_QUOTED)){
                        // token = info.next();
                        info.revert();
                        info.revert();
                        ExpressionInfo expr{};
                        expr.acc0Reg = exprInfo.acc0Reg + exprInfo.regCount;
                        int success = ParseCommand(info,expr,false);
                        if(success){
                            exprInfo.regCount++;
                        }else{
                            return PARSE_ERROR;
                        }
                    }else if(token == "arg"&& !(token.flags&TOKEN_QUOTED)){
                        int reg = exprInfo.acc0Reg+exprInfo.regCount;
                        info.code.add(BC_COPY,REG_ARGUMENT,reg);
                        _PLOG(INST << "\n";)
                        exprInfo.regCount++;
                    }else if(token == "i"&& !(token.flags&TOKEN_QUOTED)){
                        if(info.loopScopes.size()==0){
                            ERRTOK << "must be in a loop to use '#"<<token<<"'\n";
                            ERRLINE
                            // info.nextLine();
                            return PARSE_ERROR;
                        }else{
                            int reg = exprInfo.acc0Reg+exprInfo.regCount;
                            info.code.add(BC_COPY,info.loopScopes.back().iReg,reg);
                            _PLOG(INST << "\n";)
                            exprInfo.regCount++;
                        }
                    }else if(token == "v"&& !(token.flags&TOKEN_QUOTED)){
                        if(info.loopScopes.size()==0){
                            ERRTOK << "must be in a loop to use '#"<<token<<"'\n";
                            ERRLINE
                            // info.nextLine();
                            return PARSE_ERROR;
                        }else{
                            int reg = exprInfo.acc0Reg+exprInfo.regCount;
                            info.code.add(BC_COPY,info.loopScopes.back().vReg,reg);
                            _PLOG(INST << "\n";)
                            exprInfo.regCount++;
                        }
                    }else if(token == "join"&& !(token.flags&TOKEN_QUOTED)){
                        token = info.next();
                        if(!IsName(token)){
                            ERRT(token) << "Expected a valid variable name (not '"<<token<<"')\n";
                            return PARSE_ERROR;   
                        }
                        auto var = info.getVariable(token);
                        if(!var){
                            ERRT(token) << "Expected a defined variable ('"<<token<<"' isn't)\n";
                            return PARSE_ERROR;
                        }
                        int reg = exprInfo.acc0Reg+exprInfo.regCount;
                        int tempReg = exprInfo.acc0Reg+exprInfo.regCount+1;
                        info.code.add(BC_LOADV, tempReg,var->frameIndex);
                        _PLOG(INST << "var '"<<token<<"'\n";)
                        info.code.add(BC_JOIN, tempReg, reg);
                        _PLOG(INST << "\n";)
                        exprInfo.regCount++;
                    }else {
                        ERRTOK << "undefined directive '"<<token<<"'\n";
                        ERRLINE
                        // info.nextLine();
                        return PARSE_ERROR;
                    }
    //                 if(token=="#" && !(token.flags&TOKEN_SUFFIX_SPACE)){
    //     Token token = info.get(info.get()+2);
    //     if(token=="join"){
    //         info.next();   
    //         info.next();
            
    //         token = info.next();
    //         if(!IsName(token)){
    //             ERRT(token) << "Expected a valid variable name (not '"<<token<<"')\n";
    //             return PARSE_ERROR;   
    //         }
    //         auto var = info.getVariable(token);
    //         if(!var){
    //             ERRT(token) << "Expected a defined variable ('"<<token<<"' isn't)\n";
    //             return PARSE_ERROR;
    //         }
            
    //         info.code.add(BC_JOIN,);
    //     }
    // }
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
                _PLOG(INST << "\n";)
                info.code.addLoadNC(exprInfo.acc0Reg+exprInfo.regCount,constIndex);
                _PLOG(INST << "constant "<<token<<"\n";)
                exprInfo.regCount++;
            } else if(token.flags&TOKEN_QUOTED){
                token = info.next();
                int reg = exprInfo.acc0Reg + exprInfo.regCount;
                int constIndex = info.code.addConstString(token);
                info.code.add(BC_STR,reg);
                _PLOG(INST << "\n";)
                info.code.addLoadSC(reg,constIndex);
                _PLOG(INST << "constant "<<token<<"\n";)
                exprInfo.regCount++;
            } else if(IsName(token)){
                // Todo: handle variable or function
                auto var = info.getVariable(token);
                // auto func = GetExternalCall(token);
                if(var){
                    token = info.next();
                    int reg = exprInfo.acc0Reg + exprInfo.regCount;
                    int tempReg = exprInfo.acc0Reg + exprInfo.regCount+1;

                    // normal variable nothing special
                    info.code.add(BC_LOADV,reg,var->frameIndex);
                    _PLOG(INST << "get variable "<<var->frameIndex<<"\n";)
                    info.code.add(BC_COPY,reg,reg);
                    _PLOG(INST << "\n";)
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
                //     // _PLOG(INST << "get variable "<<varPair->second.frameIndex<<"\n";)
                //     // info.code.add(BC_COPY,reg,reg);
                //     // _PLOG(INST << "\n";)
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
            } else if(token=="("&& !(token.flags&TOKEN_QUOTED)){
                token = info.next();
                //Note: attempt does not work since we can't simply info.index-- (revert?) if ParseExpression messed around
                // We notify the code with line below 
                attempt=false;
                
                ExpressionInfo expr{};
                expr.acc0Reg = exprInfo.acc0Reg+exprInfo.regCount;
                _PLOG(log::out<<"Par ( \n";)
                info.parDepth++;
                int result = ParseExpression(info,expr,false);
                if(result==PARSE_SUCCESS){
                    exprInfo.regCount++;
                    info.parDepth--;
                    _PLOG(log::out<<"Par ) \n";)
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
            if(prop=="."&& !(prop.flags&TOKEN_QUOTED)){
                // property of a variable
                info.next();
                
                prop = info.next();
                if(prop == "type"&& !(prop.flags&TOKEN_QUOTED)){
                    info.code.add(BC_STR,tempReg);
                    _PLOG(INST << "\n";)
                    info.code.add(BC_TYPE,reg,tempReg);
                    _PLOG(INST << "\n";)
                    info.code.add(BC_DEL,reg);
                    _PLOG(INST << "\n";)
                    info.code.add(BC_MOV,tempReg,reg);
                    _PLOG(INST << "\n";)
                    // exprInfo.regCount++;
                }else if(prop=="length"&& !(prop.flags&TOKEN_QUOTED)){
                    info.code.add(BC_NUM,tempReg);
                    _PLOG(INST << "\n";)
                    info.code.add(BC_LEN,reg,tempReg);
                    _PLOG(INST << "\n";)
                    info.code.add(BC_DEL,reg);
                    _PLOG(INST << "\n";)
                    info.code.add(BC_MOV,tempReg,reg);
                    _PLOG(INST << "\n";)
                    // exprInfo.regCount++;
                }else{
                    ERRTOK << "unknown property '"<<prop<<"'\n";
                    ERRLINE
                    return PARSE_ERROR;
                }
            }else if(prop=="[" && !(prop.flags&TOKEN_QUOTED)){
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
                if(prop==":"&& !(prop.flags&TOKEN_QUOTED)){
                    prop = info.next();
                    int result = ParseExpression(info,expr2,false);
                    if(result!=PARSE_SUCCESS){
                        return PARSE_ERROR;
                    }
                    prop = info.get(info.at()+1); // next should be ]
                }

                if(prop != "]"&& !(prop.flags&TOKEN_QUOTED)){
                    ERRTOK << "exprected ] got '"<<prop<<"'\n";
                    ERRLINE
                    return PARSE_ERROR;
                }
                prop = info.next();

                info.code.add(BC_MOV,reg,tempReg+2);
                _PLOG(INST << "temp 2\n";)
                if(expr2.regCount==0)
                    info.code.add(BC_SUBSTR,reg,tempReg,tempReg); // var[5]
                else
                    info.code.add(BC_SUBSTR,reg,tempReg,tempReg+1); // var[3:4]
                _PLOG(INST << "\n";)

                info.code.add(BC_DEL,tempReg);
                _PLOG(INST << "temp 0\n";)
                if(expr2.regCount!=0){
                    info.code.add(BC_DEL,tempReg+1);
                    _PLOG(INST << "temp 1\n";)
                }
                info.code.add(BC_DEL,tempReg+2);
                _PLOG(INST << "temp 2\n";)
            }
        }
        
        if(hadNot){
            int reg = exprInfo.acc0Reg+exprInfo.regCount-1;
            info.code.add(BC_NOT,reg,reg);
            _PLOG(INST << "\n";)
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
            ERRLINE
            info.errors++;
            return PARSE_ERROR;
        }
        if(exprInfo.regCount==exprInfo.opCount&&ending){
            info.errors++;
            log::out << log::RED<<"DevError: ParseExpression, can this happen?\n";
            // ERRTOK << "undefined variable "<<token<<"\n";
            ERRLINE
            // info.nextLine();
            return PARSE_ERROR;
        }
         
        if(!(exprInfo.regCount==exprInfo.opCount || exprInfo.regCount==exprInfo.opCount+1)){
            info.errors++;
            log::out << log::RED<<"DevError: ParseExpression, can this happen?\n";
            ERRLINE
            // ERRTOK << "uneven numbers and operators, tokens: "<<info.prev()<<" "<<token<<"\n";
            // ERRLINE
            // info.nextLine();
            return PARSE_ERROR;
        }
        ending = ending || info.end() || (token == ")"&&!(token.flags&TOKEN_QUOTED));
        if(token==")" && !(token.flags&TOKEN_QUOTED)) // make sure we don't detect ")" as )
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
                    _PLOG(INST<<"pre\n";)
                    info.code.add(BC_DEL,reg1);
                    _PLOG(INST << "\n";)
                    
                    // info.code.add(op1,reg0,reg1,reg0);
                    // info.code.add(BC_DEL,reg1);
                    exprInfo.regCount--;
                }else{
                    // _PLOG(log::out << "break\n";)
                    break;
                }
            }else if(ending){
                int reg0 = exprInfo.acc0Reg+exprInfo.regCount-2;
                int reg1 = exprInfo.acc0Reg+exprInfo.regCount-1;
                int op = exprInfo.operations[exprInfo.opCount-1];
                exprInfo.opCount--;
                info.code.add(op,reg0,reg1,reg0);
                _PLOG(INST<<"post\n";)
                info.code.add(BC_DEL,reg1);
                _PLOG(INST << "\n";)
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
            _PLOG(EXIT)
            return PARSE_SUCCESS;
        }
    }
}
int ParseBody(GenParseInfo& info, int acc0reg);
// returns 0 if syntax is wrong for flow parsing
int ParseFlow(GenParseInfo& info, int acc0reg, bool attempt){
    using namespace engone;
    
    _PLOG(TRY)
    
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
        
        auto var = info.getVariable(name);
        if(!var){
            ERRT(name) << "Undefined variable '"<<name<<"'\n";
            return PARSE_ERROR;
        }
        int reg = acc0reg;
        info.code.add(BC_LOADV, reg, var->frameIndex);
        _PLOG(INST << "\n";)
        info.code.add(BC_TEST, reg);
        _PLOG(INST << "var '"<<name<<"'\n";)
    }else if(token=="return"){
        token = info.next();
        ExpressionInfo expr{};
        expr.acc0Reg = acc0reg;
        int result = ParseExpression(info,expr,true);
        if(result!=PARSE_SUCCESS&&result!=PARSE_BAD_ATTEMPT){
            return PARSE_ERROR;
        }
        GenParseInfo::FuncScope& funcScope = info.funcScopes.back();

        for(int i = info.scopes.size()-1;i>=funcScope.scopeIndex;i--){
            info.dropScope(i);
        }
        if(result==PARSE_SUCCESS)
            info.code.add(BC_RETURN, expr.acc0Reg);
        else
            info.code.add(BC_RETURN, REG_NULL);
        _PLOG(INST << "\n";)
    }else if(token == "break"){
        token = info.next();
        if(info.loopScopes.empty()){
            ERRTOK << token<<" only allowed in a loop\n";
            ERRLINE
            return PARSE_ERROR;
        }
        GenParseInfo::LoopScope& loop = info.loopScopes.back();

        for(int i = info.scopes.size()-1;i>loop.scopeIndex;i--){
            info.dropScope(i);
        }

        info.code.addLoadNC(loop.jumpReg,loop.breakConstant);
        _PLOG(INST << "\n";)
        info.code.add(BC_JUMP, loop.jumpReg);
        _PLOG(INST << "\n";)
    }else if(token=="continue"){
        token = info.next();
        if(info.loopScopes.empty()){
            ERRTOK << token<<" only allowed in a loop\n";
            ERRLINE
            return PARSE_ERROR;
        }
        GenParseInfo::LoopScope& loop = info.loopScopes.back();
        for(int i = info.scopes.size()-1;i>loop.scopeIndex;i--){
            info.dropScope(i);
        }
        info.code.addLoadNC(loop.jumpReg,loop.continueConstant);
        _PLOG(INST << "\n";)
        info.code.add(BC_JUMP, loop.jumpReg);
        _PLOG(INST << "\n";)
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
        _PLOG(INST << "jump\n";)
        info.code.addLoadNC(jumpReg,jumpAddress);
        _PLOG(INST << "\n";)
        info.code.add(BC_JUMPNIF,expr.acc0Reg,jumpReg);
        _PLOG(INST << "\n";)

        GenParseInfo::Scope* scope = &info.scopes.back();
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
            _PLOG(INST << "\n";)
            info.code.add(BC_JUMP,jumpReg);
            _PLOG(INST << "\n";)
            
            _PLOG(log::out<<log::GREEN<<"  : JUMP ADDRESS scope: "<<(info.scopes.size()-1)<<"\n";)
            info.code.getConstNumber(jumpAddress)->value = info.code.length();
            
            int result = ParseBody(info,acc0reg+2);
            if(result!=PARSE_SUCCESS){
                return PARSE_ERROR;
            }
            // info.dropScope();
            
            _PLOG(log::out<<log::GREEN<<"  : ELSE ADDRESS scope: "<<(info.scopes.size()-1)<<"\n";)
            info.code.getConstNumber(elseAddr)->value = info.code.length()
                + scope->getInstsBeforeDelReg(info);
        }else{
            info.code.getConstNumber(jumpAddress)->value = info.code.length()
                + scope->getInstsBeforeDelReg(info);
            _PLOG(log::out<<log::GREEN<<"  : JUMP ADDRESS scope: "<<(info.scopes.size()-1)<<"\n";)
                
        }   
        info.dropScope();
    } else if(token=="while"){
        token = info.next();
        info.makeScope();

        int jumpReg = acc0reg;
        info.code.add(BC_NUM,jumpReg);
        _PLOG(INST << "jump\n";)
        GenParseInfo::Scope& scope = info.scopes.back();
        scope.delRegisters.push_back(jumpReg);
        int startAddress = info.code.addConstNumber(info.code.length());
        _PLOG(log::out<<log::GREEN<<"  : START ADDRESS scope: "<<(info.scopes.size()-1)<<"\n";)
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
        _PLOG(INST << "\n";)
        info.code.add(BC_JUMPNIF,expr.acc0Reg,jumpReg);
        _PLOG(INST << "\n";)
        info.code.add(BC_DEL,expr.acc0Reg);
        _PLOG(INST << "\n";)
        
        info.loopScopes.push_back({});
        GenParseInfo::LoopScope& loop = info.loopScopes.back();
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
        _PLOG(INST << "\n";)
        info.code.add(BC_JUMP,jumpReg);
        _PLOG(INST << "\n";)
        
        info.code.getConstNumber(endAddress)->value = info.code.length();
        _PLOG(log::out << log::GREEN<<"  : END ADDRESS scope: "<<(info.scopes.size()-1)<<"\n";)

        info.code.add(BC_DEL,expr.acc0Reg);
        _PLOG(INST << "\n";)

        info.code.getConstNumber(breakAddress)->value = info.code.length();
        _PLOG(log::out << log::GREEN<<"  : END BREAK ADDRESS scope: "<<(info.scopes.size()-1)<<"\n";)

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
        _PLOG(INST << "temp\n";)
        info.code.add(BC_NUM,jumpReg);
        _PLOG(INST << "jump\n";)

        GenParseInfo::Scope& scope = info.scopes.back();
        scope.delRegisters.push_back(tempReg);
        scope.delRegisters.push_back(jumpReg);

        int startAddress = info.code.addConstNumber(-1);
        
        ExpressionInfo expr{};
        expr.acc0Reg = unusedReg;

        int stumpIndex = info.code.length();
        info.code.add(Instruction{0,(uint8)iReg}); // add stump
        _PLOG(INST << "May become BC_NUM\n";)

        bool wasAssigned=0;
        int result = ParseAssignment(info,expr,true,false);
        if(result==PARSE_SUCCESS)
            wasAssigned=true;
        if(result==PARSE_BAD_ATTEMPT){
            result = ParseExpression(info,expr,true);
        }
        Token varName{};
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
            if(IsName(token) && next == ":" && !(next.flags&TOKEN_QUOTED)){
                info.next();
                auto var = info.getVariable(token);
                if(!var){
                    // Turn token into variable?
                    
                    auto var = info.addVariable(token);
                    var->frameIndex = info.frameOffsetIndex++;
                    varName = token;
                    scope.variableNames.push_back(token);
                    
                    // createdVar=true;
                    info.code.add(BC_NUM, iReg);
                    _PLOG(INST << "var reg\n";)

                    info.code.add(BC_STOREV, iReg,var->frameIndex);
                    _PLOG(INST << "store "<<token<<"\n";)
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
                
                scope.delRegisters.push_back(iReg);
                info.code.add(BC_MOV,expr.acc0Reg,iReg);
                _PLOG(INST << "acc -> var\n";)
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
                _PLOG(INST << "acc -> max\n";)
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
                _PLOG(INST << "acc -> max\n";)
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
        // _PLOG(log::out << "    Start Address: "<<info.code.getConstNumber(startAddress)->value<<"\n";)
        _PLOG(log::out<<log::GREEN<<"  : START ADDRESS scope: "<<(info.scopes.size()-1)<<"\n";)

        int endAddress = info.code.addConstNumber(-1);

        info.loopScopes.push_back({});
        GenParseInfo::LoopScope& loop = info.loopScopes.back();
        loop.iReg = iReg;
        loop.jumpReg = jumpReg;
        loop.continueConstant = startAddress;
        loop.breakConstant = endAddress;
        loop.scopeIndex = info.scopes.size()-1;
        
        // if(varName.str){
        // only necessary after ParseBody
        //     auto varptr = info.getVariable(varName);
        //     info.code.add(BC_LOADV, iReg,varptr->frameIndex);
        //     _PLOG(INST << "load "<<varName<<"\n";)
        // }
        info.code.add(BC_LESS, iReg, maxReg, tempReg);
        _PLOG(INST << "\n";)
        if(createdMax){ 
            info.code.add(BC_DEL,maxReg);
            _PLOG(INST << "\n";)
        }

        info.code.addLoadNC(jumpReg,endAddress);
        _PLOG(INST << "\n";)
        info.code.add(BC_JUMPNIF, tempReg, jumpReg);
        _PLOG(INST << "\n";)
        
        result = ParseBody(info,unusedReg);
        if(result!=PARSE_SUCCESS){
            return PARSE_ERROR;
        }
        
        int incrIndex = info.code.addConstNumber(1);
        info.code.addLoadNC(tempReg,incrIndex);
        _PLOG(INST << "\n";)
        if(varName.str){
            // if body changed value we need to get the new one
            auto varptr = info.getVariable(varName);
            info.code.add(BC_LOADV, iReg,varptr->frameIndex);
            _PLOG(INST << "load "<<varName<<"\n";)
        }
        info.code.add(BC_ADD,iReg,tempReg,iReg);
        _PLOG(INST << "increment var\n";)
        
        info.code.addLoadNC(jumpReg,startAddress);
        _PLOG(INST << "\n";)
        info.code.add(BC_JUMP, jumpReg);
        _PLOG(INST << "\n";)
        
        info.code.getConstNumber(endAddress)->value = info.code.length();
        _PLOG(log::out << "    End Address: "<<info.code.getConstNumber(endAddress)->value<<"\n";)
        
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
        int tempStrReg2= acc0reg+7;
        int varReg  = acc0reg+8;
        int unusedReg  = acc0reg+9;

        info.code.add(BC_NUM,jumpReg);
        _PLOG(INST << "jump\n";)
        info.code.add(BC_NUM,iReg);
        _PLOG(INST << "i\n";)
        info.code.add(BC_NUM,tempReg);
        _PLOG(INST << "temp\n";)
        info.code.add(BC_NUM,beginReg);
        _PLOG(INST << "begin\n";)
        info.code.add(BC_NUM,endReg);
        _PLOG(INST << "end\n";)
        info.code.add(BC_STR,tempStrReg);
        _PLOG(INST << "tempstr\n";)
        info.code.add(BC_STR,tempStrReg2);
        _PLOG(INST << "tempstr\n";)

        GenParseInfo::Scope& scope = info.scopes.back();
        scope.delRegisters.push_back(jumpReg);
        scope.delRegisters.push_back(iReg);
        scope.delRegisters.push_back(tempReg);
        scope.delRegisters.push_back(beginReg);
        scope.delRegisters.push_back(endReg);
        scope.delRegisters.push_back(tempStrReg);
        scope.delRegisters.push_back(tempStrReg2);

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
            auto var = info.getVariable(name);
            if(var){
                ERRT(name) << "Expected a non-defined variable ('"<<name<<"' is defined)\n";
                return PARSE_ERROR;
            }

            var = info.addVariable(name);
            var->frameIndex = info.frameOffsetIndex++;
            // scope.variableNames.push_back(name);
            // Note: chrReg which is what var will refer to is added to
            //  delRegisters when we don't specify a variable.
            //  There is no need to add variable to scope so it
            //  is deleted because it already will be deleted.
            //  there is also some special cases with break and return
            //  which is handled. We don't want to handle it
            //  twice with variable and delRegister.
            createdVar=true;
        }
        
        // only allow variable not expression (each tmp : onlyvar)
        // Token listName = info.next();
        // auto pair = info.var.find(listName);
        // if(pair==info.variables.end()){
        //     ERRT(listName) << "Must be a defined variable ('"<<listName<<"' is not)\n";
        //     return PARSE_ERROR;
        // }
        // info.code.add(BC_LOADV,varReg,pair->second.frameIndex);
        // _PLOG(INST << "\n";)

        ExpressionInfo expr{};
        expr.acc0Reg = varReg;
        ExpressionInfo expr2{};
        expr2.acc0Reg = varReg;

        // Todo: ParseAssignment here? 
        //  each str : tmp = "okay" {}   <- would be possible. Is it useful?
        int result = ParseAssignment(info,expr,true,false);
        if(result==PARSE_BAD_ATTEMPT){
            result = ParseExpression(info,expr2,false);
            if(result==PARSE_SUCCESS){
                scope.delRegisters.push_back(varReg);
            }
        }
        if(result!=PARSE_SUCCESS){
            return PARSE_ERROR;
        }


        int startAddress = info.code.addConstNumber(info.code.length());
        _PLOG(log::out<<log::GREEN<<"  : START ADDRESS ["<<startAddress<<"]\n";)

        int endAddress = info.code.addConstNumber(-1);
        int elseAddress = info.code.addConstNumber(-1);
        int bodyIfAddress = info.code.addConstNumber(-1);
        int lastAddress = info.code.addConstNumber(-1);
        int continueAddress = info.code.addConstNumber(-1);
        
        info.code.add(BC_LEN, varReg,tempReg);
        _PLOG(INST << "\n";)
        info.code.add(BC_LESS,iReg,tempReg,tempReg);
        _PLOG(INST << "\n";)

        info.code.addLoadNC(jumpReg,endAddress);
        _PLOG(INST << "\n";)
        info.code.add(BC_JUMPNIF,tempReg,jumpReg);
        _PLOG(INST << "\n";)
        
        info.code.add(BC_MOV,varReg,chrReg);
        _PLOG(INST << "\n";)
        info.code.add(BC_SUBSTR,chrReg,iReg,iReg);
        _PLOG(INST << "\n";)

        int oneConstant = info.code.addConstNumber(1);
        info.code.addLoadNC(tempReg,oneConstant);
        _PLOG(INST << "\n";)
        info.code.add(BC_ADD,iReg,tempReg,iReg);
        _PLOG(INST << "\n";)

        Token spaceToken = " ";
        int spaceConstant = info.code.addConstString(spaceToken);
        Token lineToken = "\n";
        int lineConstant = info.code.addConstString(lineToken);
        
        info.code.addLoadSC(tempStrReg,spaceConstant);
        _PLOG(INST << "\n";)
        info.code.add(BC_EQUAL,chrReg,tempStrReg,tempStrReg);
        _PLOG(INST << "\n";)

        info.code.addLoadSC(tempStrReg2,lineConstant);
        _PLOG(INST << "\n";)
        info.code.add(BC_EQUAL,chrReg,tempStrReg2,tempStrReg2);
        _PLOG(INST << "\n";)

        info.code.add(BC_OR,tempStrReg,tempStrReg2,tempStrReg);
        _PLOG(INST << "\n";)
        info.code.add(BC_DEL,chrReg);
        _PLOG(INST << "\n";)
        
        info.code.addLoadNC(jumpReg,elseAddress);
        _PLOG(INST << "\n";)
        info.code.add(BC_JUMPNIF,tempStrReg,jumpReg);
        _PLOG(INST << "\n";)

        // end = i-2
        int twoConstant = info.code.addConstNumber(2);
        info.code.addLoadNC(tempReg,twoConstant);
        _PLOG(INST << "\n";)
        info.code.add(BC_SUB,iReg,tempReg,endReg);
        _PLOG(INST << "\n";)

        info.code.addLoadNC(jumpReg,bodyIfAddress);
        _PLOG(INST << "\n";)
        info.code.add(BC_JUMP,jumpReg);
        _PLOG(INST << "\n";)

        info.code.getConstNumber(elseAddress)->value = info.code.length();
        _PLOG(log::out << log::GREEN<<"  : ELSE ADDRESS ["<<elseAddress<<"]\n";)

        info.code.add(BC_LEN, varReg, tempReg);
        _PLOG(INST << "\n";)
        info.code.add(BC_EQUAL,iReg,tempReg,tempReg);
        _PLOG(INST << "\n";)

        info.code.addLoadNC(jumpReg,startAddress);
        _PLOG(INST << "\n";)
        info.code.add(BC_JUMPNIF,tempReg,jumpReg);
        _PLOG(INST << "\n";)

        info.code.addLoadNC(tempReg,oneConstant);
        _PLOG(INST << "\n";)
        info.code.add(BC_SUB,iReg,tempReg,endReg);
        _PLOG(INST << "\n";)

        info.code.getConstNumber(bodyIfAddress)->value = info.code.length();
        _PLOG(log::out << log::GREEN<<"  : BODY IF ADDRESS ["<<elseAddress<<"]\n";)

        info.code.addLoadNC(tempReg,oneConstant);
        _PLOG(INST << "\n";)
        info.code.add(BC_ADD,tempReg,endReg,tempReg);
        _PLOG(INST << "\n";)
        info.code.add(BC_SUB,tempReg,beginReg,tempReg);
        _PLOG(INST << "\n";)
        info.code.add(BC_GREATER,tempReg,REG_ZERO,tempReg);
        _PLOG(INST << "\n";)

        info.code.addLoadNC(jumpReg,lastAddress);
        _PLOG(INST << "\n";)
        info.code.add(BC_JUMPNIF,tempReg,jumpReg);
        _PLOG(INST << "\n";)
        
        info.code.add(BC_MOV,varReg,chrReg); // resusing the free chrReg
        _PLOG(INST << "\n";)
        info.code.add(BC_SUBSTR,chrReg,beginReg,endReg);
        _PLOG(INST << "\n";)

        if(createdVar){
            auto var = info.getVariable(name);
            info.code.add(BC_STOREV, chrReg, var->frameIndex);
            _PLOG(INST << "store "<<name<<"\n";)
        }

        /*
        each "1 2" {
            print late
            late = "WOW"
        }
        is an extra scope needed here since the above code would print
        "late" and then "WOW" since the variable isn't deleted since we
        don't clear the scope?
        HAHA! actually,the parser evaluates late as a constant which
        the instructions uses.
        */
        info.loopScopes.push_back({});
        GenParseInfo::LoopScope& loop = info.loopScopes.back();
        loop.iReg = iReg; // Todo: add value for #i
        loop.vReg = chrReg; // Todo: add value for #i
        loop.jumpReg = jumpReg;
        loop.continueConstant = continueAddress;
        loop.breakConstant = continueAddress;
        loop.scopeIndex = info.scopes.size()-1;

        if(!createdVar){
            scope.delRegisters.push_back(chrReg);
        }

        int result2 = ParseBody(info,unusedReg);
        if(result2!=PARSE_SUCCESS){
            return PARSE_ERROR;
        }
        // reallocation of scopes may have occured so the scope
        // reference we had may be invalid. We need to get it again.
        GenParseInfo::Scope& scope2 = info.scopes.back();
        if(!createdVar){
            scope2.removeReg(chrReg);
        }
        
        info.code.getConstNumber(continueAddress)->value = info.code.length();
        _PLOG(log::out << log::GREEN<<"  : CONTINUE/BREAK ADDRESS ["<<continueAddress<<"]\n";)
        
        info.code.add(BC_DEL,chrReg);
        _PLOG(INST << "\n";)

        info.loopScopes.erase(info.loopScopes.begin()+info.loopScopes.size()-1);
        
        info.code.getConstNumber(lastAddress)->value = info.code.length();
        _PLOG(log::out << log::GREEN<<"  : BEGIN ADDRESS ["<<lastAddress<<"]\n";)

        info.code.add(BC_ADD,iReg,REG_ZERO,beginReg);
        _PLOG(INST << "\n";)

        info.code.addLoadNC(jumpReg,startAddress);
        _PLOG(INST << "\n";)
        info.code.add(BC_JUMP,jumpReg);
        _PLOG(INST << "\n";)
        
        info.code.getConstNumber(endAddress)->value = info.code.length();
        _PLOG(log::out << log::GREEN<<"  : END ADDRESS ["<<endAddress<<"]\n";)

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
    _PLOG(EXIT)
    return PARSE_SUCCESS;
}
// #fass
int ParseAssignment(GenParseInfo& info, ExpressionInfo& exprInfo, bool attempt, bool requestCopy){
    using namespace engone;
    
    _PLOG(TRY)
    
    if(info.tokens.length() < info.index+2){
        // not enough tokens for assignment
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }
        _PLOG(log::out <<log::RED<< "CompilerError: to few tokens for assignment\n";)
        // info.nextLine();
        return PARSE_ERROR;
    }
    Token name = info.get(info.at()+1);
    Token assign = info.get(info.at()+2);
    Token token = info.get(info.at()+3);
    
    // Todo: handle ++a
    
    if(assign=="++"||assign=="--"){
        // a++
        auto var = info.getVariable(name);
        if(!var){
            if(attempt){
                return PARSE_BAD_ATTEMPT;
            }
            ERRT(name) << "variable '"<<name<<"' must be defined when using '"<<assign<<"'\n";
            return PARSE_ERROR;   
        }
        info.next();
        info.next();
        // Todo: a=9;b = a++ <- b would be 10 with current code. Change it to
        //  make a copy which b is set to then do addition on variable.
        //  how does it work with requestCopy?
        int finalReg = exprInfo.acc0Reg;
        info.code.add(BC_LOADV,finalReg,var->frameIndex);
        _PLOG(INST << "load var '"<<name<<"'\n";)
        int constone = info.code.addConstNumber(1);
        info.code.add(BC_NUM,finalReg+1);
        _PLOG(INST << "\n";)
        info.code.addLoadNC(finalReg+1,constone);
        if(assign == "++")
            info.code.add(BC_ADD,finalReg,finalReg+1,finalReg);
        if(assign == "--")
            info.code.add(BC_SUB,finalReg,finalReg+1,finalReg);
        _PLOG(INST <<"\n";)
        info.code.add(BC_DEL,finalReg+1);
        _PLOG(INST <<"\n";)
        
        if(requestCopy){
            info.code.add(BC_COPY,finalReg,finalReg);
            _PLOG(INST << "copy requested\n";)
            _PLOG(EXIT)
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
        // don't next on {, ParseBody needs it.
        auto func = info.getFunction(token);
        auto var = info.getVariable(token);
        if(var){
            ERRT(token) << "'"<<token<<"' already defined as variable\n";
            // info.nextLine();
            return PARSE_ERROR;
        }else if(!func){
            // Note: assign new function
            func = info.addFunction(name);
            info.scopes.back().functionsNames.push_back(name);
            int constIndex = info.code.addConstNumber(-1);
            int jumpReg = exprInfo.acc0Reg;
            info.code.add(BC_NUM,jumpReg);
            _PLOG(INST << "jump\n";)
            info.code.addLoadNC(jumpReg,constIndex);
            _PLOG(INST << "\n";)
            info.code.add(BC_JUMP, jumpReg);
            _PLOG(INST << "Skip function body '"<<name<<"'\n";)
            
            func->jumpAddress = info.code.length();

            info.makeScope();
            auto& scope = info.scopes.back();
            scope.delRegisters.push_back(REG_ARGUMENT);
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
            _PLOG(INST << "\n";)

            info.code.getConstNumber(constIndex)->value = info.code.length();
            info.code.add(BC_DEL,jumpReg);
            _PLOG(INST << "skipped to here\n";)

            _PLOG(EXIT)
            return PARSE_NO_VALUE;
        }else{
            ERRT(token) << "function '"<<token<<"' already defined\n";
            // info.nextLine();
            return PARSE_ERROR;
        }
        _PLOG(EXIT)
        return PARSE_SUCCESS;
    }
    int unusedReg = exprInfo.acc0Reg;
    int indexReg = 0;
    if (assign=="["){
        auto varName = info.getVariable(name);
        if(!varName){
            ERRT(name) << "variable must be defined to use [x] ('"<<varName<<"' wasn't)\n";
            ERRLINE
            return PARSE_ERROR;
        }
        info.next();
        info.next();
        int result = ParseExpression(info,exprInfo,false);
        if(result!=PARSE_SUCCESS){
            return PARSE_ERROR;
        }
        indexReg = exprInfo.acc0Reg;
        unusedReg = indexReg+1;
        Token token = info.get(info.at()+1);
        if (token!="]"){
            ERRT(token) << "expected ], not "<<token<<"\n";
            return PARSE_ERROR;
        }
        info.next();
        assign = info.get(info.at()+1);

        if(assign!="="){
            ERRT(assign) << "char assignment only works for = (not '"<<assign<<"')\n";
            ERRLINE
            return PARSE_ERROR;
        }
    } 
    
    if(!(assign=="=" ||assign=="+=" ||assign=="-=" ||assign=="*=" ||assign=="/=")){
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }else{
            info.next(); // move forward to prevent infinite loop? altough nextline would fix it?
            ERRT(assign) << "unexpected "<<name<<", wanted token for assignment\n";
            // info.nextLine();
            return PARSE_ERROR;
        }
    }
    if (indexReg==0) {
        info.next();
    }
    info.next();
    ExpressionInfo expr{};
    expr.acc0Reg = unusedReg;
    int result=0;

    result = ParseExpression(info,expr,false);
    if(result!=PARSE_SUCCESS){
        return PARSE_ERROR;
    }
    int finalReg = expr.acc0Reg;
    if(expr.regCount==0){
        log::out << log::RED<<"CompilerWarning: regCount == 0 after expression from assignment\n";   
    }
            
    auto var = info.getVariable(name);
    if(!var){
        if(assign != "="){
            ERRT(name) << "variable '"<<name<<"' must be defined when using '"<<assign<<"'\n";
            return PARSE_ERROR;   
        }
        var = info.addVariable(name);
        // Note: assign new variable
        var->frameIndex = info.frameOffsetIndex++;
        // Todo: bound check on scopes, global scope should exist though
        auto& scope = info.scopes.back();
        scope.variableNames.push_back(name);
    }

    if(indexReg!=0){
        info.code.add(BC_LOADV,finalReg+1,var->frameIndex);
        _PLOG(INST << "load var '"<<name<<"'\n";)

        if (assign=="="){
            info.code.add(BC_SETCHAR,finalReg,indexReg,finalReg+1);
            _PLOG(INST <<"\n";)
            info.code.add(BC_DEL,indexReg);
            _PLOG(INST <<"\n";)
            info.code.add(BC_DEL,finalReg);
            _PLOG(INST <<"\n";)
            info.code.add(BC_MOV,finalReg+1,finalReg);
            _PLOG(INST <<"\n";)
        }else {
            // handle above
        }
    }else{
        if(assign=="="){
            info.code.add(BC_STOREV, finalReg, var->frameIndex);
            _PLOG(INST << "store "<<name<<"\n";)
        } else {
            info.code.add(BC_LOADV,finalReg+1,var->frameIndex);
            _PLOG(INST << "load var '"<<name<<"'\n";)
            if(assign == "+=")
                info.code.add(BC_ADD,finalReg+1,finalReg,finalReg+1);
            if(assign == "-=")
                info.code.add(BC_SUB,finalReg+1,finalReg,finalReg+1);
            if(assign == "*=")
                info.code.add(BC_MUL,finalReg+1,finalReg,finalReg+1);
            if(assign == "/=")
                info.code.add(BC_DIV,finalReg+1,finalReg,finalReg+1);
            _PLOG(INST <<"\n";)
            info.code.add(BC_DEL,finalReg);
            _PLOG(INST <<"\n";)
            info.code.add(BC_MOV,finalReg+1,finalReg);
            _PLOG(INST <<"\n";)
        }
    }
        
    if(requestCopy){
        info.code.add(BC_COPY,finalReg,finalReg);
        _PLOG(INST << "copy requested\n";)
        _PLOG(EXIT)
        return PARSE_SUCCESS;
    }
    _PLOG(EXIT)
    return PARSE_SUCCESS;   
}
int ParseCommand(GenParseInfo& info, ExpressionInfo& exprInfo, bool attempt){
    using namespace engone;
    _PLOG(TRY)
    
    if(info.end()){
        log::out <<log::RED<< "Sudden end in ParseCommand?";
        return PARSE_ERROR;
    }
    // Token token = info.next();
    Token varName = info.get(info.at()+1);
    bool isAsync=false;

    if (varName == "#" && !(varName.flags&TOKEN_SUFFIX_SPACE)){
        Token token = info.get(info.at()+2);
        if (token=="async"){
            isAsync = true;
            varName = info.get(info.at()+3);
            info.next();
            info.next();
        }
    }
    // TODO: don't allow async when piping variables

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
            _PLOG(INST << varName<<"\n";)

            ExpressionInfo expr{};
            expr.acc0Reg = fileReg;
            int result = ParseExpression(info,expr,true);
            if(result==PARSE_SUCCESS){
                // info.next();
                // info.code.add(BC_LOADV,fileReg,fileVar->frameIndex);
                // _PLOG(INST << varName<<"\n";)
                info.code.add(instruction,varReg,fileReg);
                _PLOG(INST << "\n";)
                info.code.add(BC_DEL,fileReg);
                _PLOG(INST << "\n";)
            } else if (result==PARSE_BAD_ATTEMPT){
                token = CombineTokens(info); // read file
                // we assume it's valid?
            
                info.code.add(BC_STR,fileReg);
                _PLOG(INST << "\n";)
                int constIndex = info.code.addConstString(token);
                info.code.addLoadSC(fileReg,constIndex);
                _PLOG(INST << token<<"\n";)
                
                info.code.add(instruction,varReg,fileReg);
                _PLOG(INST << "\n";)
                info.code.add(BC_DEL,fileReg);
                _PLOG(INST << "\n";)
            }else
                return PARSE_ERROR;
            // TODO: check for regcount == 0?
            
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
    
    // if(token.flags&TOKEN_QUOTED){
    // }else if(IsName(token)){
    //     // quoted exe command
    //     // internal function
    // } else{
    //     // raw exe command
    // }
    int funcReg = exprInfo.acc0Reg+exprInfo.regCount;
    exprInfo.regCount++;
    auto func = info.getFunction(token);
    // bool codeFunc=false;
    if(func){
        // func in bytecode
        if(func->constIndex==-1){
            func->constIndex = info.code.addConstNumber(func->jumpAddress);
        }
        info.code.add(BC_NUM,funcReg);
        _PLOG(INST << "\n";)
        info.code.addLoadNC(funcReg,func->constIndex);
        _PLOG(INST << "func index to '"<<token<<"'\n";)
        // codeFunc=true;

    }else{
        // executable or internal func
        int index = info.code.addConstString(token);
        info.code.add(BC_STR,funcReg);
        _PLOG(INST << "name\n";)
        info.code.addLoadSC(funcReg, index);
        _PLOG(INST << "cmd constant "<<token<<"\n";)
    }
    
    int argReg = REG_NULL;
    
    bool ending = (token.flags&TOKEN_SUFFIX_LINE_FEED) | info.end();
    if(!ending){
        
        int tempStrReg = exprInfo.acc0Reg+exprInfo.regCount+1;
        info.code.add(BC_STR,tempStrReg);
        _PLOG(INST << "temp str\n";)
        
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
                    _PLOG(INST << "\n";)
                    whichArg=ARG_TEMP;
                }else if(token == "i"){
                    if(info.loopScopes.size()==0){
                        ERRTOK << "must be in a loop to use '#"<<token<<"'\n";
                        // info.nextLine();
                        return PARSE_ERROR;
                    }else{
                        info.code.add(BC_MOV,info.loopScopes.back().iReg,tempReg);
                        _PLOG(INST << "\n";)
                        whichArg=ARG_TEMP;
                    }
                }else if(token == "v"){
                    if(info.loopScopes.size()==0){
                        ERRTOK << "must be in a loop to use '#"<<token<<"'\n";
                        // info.nextLine();
                        return PARSE_ERROR;
                    }else{
                        info.code.add(BC_MOV,info.loopScopes.back().vReg,tempReg);
                        _PLOG(INST << "\n";)
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
                // _PLOG(INST << "arg\n";)
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
                //     _PLOG(INST << "\n";)
                //     info.code.addLoadC(spaceReg,info.spaceConstIndex);
                //     _PLOG(INST << "constant ' '\n";)
                //     info.code.add(BC_ADD,argReg,spaceReg,argReg);
                //     _PLOG(INST << "extra space\n";)
                //     info.code.add(BC_DEL,spaceReg);
                //     _PLOG(INST << "\n";)
                // }
                break;
            } else{
                // Todo: find variables
                if(!(token.flags&TOKEN_QUOTED)&&IsName(token)){
                    auto var = info.getVariable(token);
                    if(var){
                        info.code.add(BC_LOADV,tempReg,var->frameIndex);
                        _PLOG(INST << "get variable "<<var->frameIndex<<"\n";)

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
                _PLOG(INST << "constant '"<<token<<"'\n";)
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
                    _PLOG(INST << "\n";)
                    info.code.addLoadSC(spaceReg,info.spaceConstIndex);
                    _PLOG(INST << "constant ' '\n";)
                    info.code.add(BC_ADD,argReg,spaceReg,argReg);
                    _PLOG(INST << "extra space\n";)
                    info.code.add(BC_DEL,spaceReg);
                    _PLOG(INST << "\n";)
                }
                // if((token.flags&TOKEN_SUFFIX_SPACE) || (token.flags&TOKEN_SUFFIX_LINE_FEED)){
                //     info.code.add(BC_STR,spaceReg);
                //     _PLOG(INST << "'\n";)
                //     info.code.addLoadC(spaceReg,info.spaceConstIndex);
                //     _PLOG(INST << "constant ' '\n";)
                //     info.code.add(BC_ADD,argReg,spaceReg,argReg);
                //     _PLOG(INST << "'\n";)
                //     info.code.add(BC_DEL,spaceReg);
                //     _PLOG(INST << "'\n";)
                // }
            }
            
            // Todo: handle properties and substring here too. Not only in ParseExpression.
            //  It's not just a copy and paste though, tempReg and ARG_TEMP, ARG_DEL needs to
            //  be taken into account. If more properties are added then you would need to change
            //  it in both places so making a function out of it would be a good idea.
            
            if(argReg==0&&whichArg!=0){
                argReg = exprInfo.acc0Reg+exprInfo.regCount;
                info.code.add(BC_STR,argReg);
                _PLOG(INST << "arg\n";)
            }
            if(whichArg&ARG_STR){
                info.code.add(BC_ADD,argReg, tempStrReg, argReg);
                _PLOG(INST << "\n";)
            }
            if(whichArg&ARG_TEMP){
                info.code.add(BC_ADD,argReg, tempReg, argReg);
                _PLOG(INST << "\n";)
            }
            if(whichArg&ARG_DEL){
                info.code.add(BC_DEL,tempReg);
                _PLOG(INST << "\n";)
            }
            token = info.now();
            if(token.flags&TOKEN_SUFFIX_LINE_FEED){
                break;
            }
        }
        info.code.add(BC_DEL,tempStrReg);
        _PLOG(INST << "temp\n";)
    }
    
    if(isAsync){
        int treg = REG_RETURN_VALUE;
        info.code.add(BC_NUM,treg);
        _PLOG(INST << "\n";)
        info.code.add(BC_THREAD,argReg,funcReg,treg);
        _PLOG(INST << "thread on "<<cmdName<<"\n";)
    }else{
        info.code.add(BC_RUN,argReg,funcReg);
        _PLOG(INST << "run "<<cmdName<<"\n";)
    }

    info.code.add(BC_DEL,funcReg);
    _PLOG(INST << "name\n";)
    exprInfo.regCount--;
    
    info.code.add(BC_MOV,REG_RETURN_VALUE,exprInfo.acc0Reg+exprInfo.regCount);
    _PLOG(INST << "\n";)
    exprInfo.regCount++;

    // Note: the parsed instructions add a new value in the accumulation
    //  Don't forget to use and delete it.
    
    _PLOG(EXIT)
    return PARSE_SUCCESS;
}
int ParseBody(GenParseInfo& info, int acc0reg){
    using namespace engone;
    // Note: two infos in case ParseAssignment modifies it and then fails.
    //  without two, ParseCommand would work with a modified info.
    _PLOG(ENTER)

    if(info.end()){
        log::out << log::YELLOW<<"ParseBody: sudden end?\n";
        return PARSE_ERROR;
    }
    Token token = info.get(info.at()+1);
    if(token=="{"){
        info.makeScope();
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
                _PLOG(INST << "\n";)
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
        
        info.dropScope();
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
            _PLOG(INST << "\n";)
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
    _PLOG(EXIT)
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
    _VLOG(log::out <<log::BLUE<<  "##   Parser   ##\n";)
    
    GenParseInfo info{tokens};
    
    // Todo: some better way to define this since they have to be defined here and in context.
    //  Better to define in one place.
    // ProvideDefaultCalls(info.externalCalls);
    
    info.makeScope();
    while (!info.end()){
        // info.addDebugLine(info.index);
        // #ifdef USE_DEBUG_INFO
        // _PLOG(log::out <<"\n"<<*((Bytecode::DebugLine*)info.code.debugLines.data+info.code.debugLines.used-1)<<"\n";)
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
    // _PLOG(log::out <<*((Bytecode::DebugLine*)info.code.debugLines.data+info.code.debugLines.used-1)<<"\n";)
    // #endif
    // while (!info.end()){
        // int flags = EvaluateExpression(info,accDepth);
        // // log::out << "Done? run it if no assignment?\n";
        // if(0==(flags&EVAL_FLAG_NO_DEL)){
        //     info.code.add(BC_DEL,REG_ACC0+accDepth);
        // }
    // }
    
    
    
    // if(info.variables.size()!=0){
    //     log::out << log::YELLOW<<"ParseWarning: "<<info.variables.size()<<" variables remain?\n";
    // }
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
    _PLOG(log::out <<*((Bytecode::DebugLine*)info.code.debugLines.data+info.code.debugLines.used-1)<<"\n";)
    #endif
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