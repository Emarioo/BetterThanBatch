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

// #define CASE(x) case x: return #x;
// const char* ParseResultToString(int result){
//     switch(result){
//         CASE(PARSE_SUCCESS)
//         CASE(PARSE_NO_VALUE)
//         CASE(PARSE_ERROR)
//         CASE(PARSE_BAD_ATTEMPT)
//     }
//     return "?";
// }
void ParseInfo::makeScope(){
    using namespace engone;
    _GLOG(log::out << log::GRAY<< "   Enter scope "<<scopes.size()<<"\n";)
    scopes.push_back({});
}
int ParseInfo::Scope::getInstsBeforeDelReg(ParseInfo& info){
    int offset=0;
    for(Token& token : variableNames){
        auto var = info.getVariable(token);
        if(var){
            offset+=1; // based on drop scope
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
        auto var = getVariable(token);
        if(var){
            // NOTE: don't forget to change offset in variable cleanup  count
            //  offset should be however many instruction you add here!
            // deletes previous value
            code.add(BC_STOREV,REG_NULL,var->frameIndex);
            _GLOG(INST << "'"<<token<<"'\n";)
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
        _GLOG(INST << "\n";)
    }
    if(index==-1)
        scopes.pop_back();
}

ParseInfo::Variable* ParseInfo::getVariable(const std::string& name){
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
void ParseInfo::removeVariable(const std::string& name){
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
ParseInfo::Variable* ParseInfo::addVariable(const std::string& name){
    if(currentFunction.empty()){
        return &(globalVariables[name] = {});   
    }
    auto func = getFunction(currentFunction);
    return &(func->variables[name] = {});
}
ParseInfo::Function* ParseInfo::getFunction(const std::string& name){
    auto pair = functions.find(name);
    if(pair==functions.end())
        return 0;
    return &pair->second;
}
ParseInfo::Function* ParseInfo::addFunction(const std::string& name){
    return &(functions[name] = {});
}
// non-quoted
bool Equal(Token& token, const char* str){
    return !(token.flags&TOKEN_QUOTED) && token == str;
}
// returns instruction type
// zero means no operation
int IsOp(Token& token){
    if(token.flags&TOKEN_QUOTED) return 0;
    if(token=="+") return ASTExpression::ADD;
    if(token=="-") return ASTExpression::SUB;
    if(token=="*") return ASTExpression::MUL;
    if(token=="/") return ASTExpression::DIV;
    // if(token=="<") return BC_LESS;
    // if(token==">") return BC_GREATER;
    // if(token=="<=") return BC_LESS_EQ;
    // if(token==">=") return BC_GREATER_EQ;
    // if(token=="==") return BC_EQUAL;
    // if(token=="!=") return BC_NOT_EQUAL;
    // if(token=="&&") return BC_AND;
    // if(token=="||") return BC_OR;
    // if(token=="%") return BC_MOD;
    return 0;
}
int OpPrecedence(int op){
    if(op==ASTExpression::ADD||op==ASTExpression::SUB) return 9;
    // if(op==BC_AND||op==BC_OR) return 1;
    // if(op==BC_LESS||op==BC_GREATER||op==BC_LESS_EQ||op==BC_GREATER_EQ
    //     ||op==BC_EQUAL||op==BC_NOT_EQUAL) return 5;
    // if(op==BC_MUL||op==BC_DIV||op==BC_MOD) return 10;
    return 0;
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
// Concatenates current token with the next tokens not seperated by space, linefeed or such
// operators and special characters are seen as normal letters
Token CombineTokens(ParseInfo& info){
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

int ParseAssignment(ParseInfo& info, ExpressionInfo& exprInfo, bool attempt, bool requestCopy);
int ParseCommand(ParseInfo& info, ExpressionInfo& exprInfo, bool attempt);

// #fex
int ParseExpression(ParseInfo& info, ASTExpression** expression, bool attempt){
    using namespace engone;
    
    _GLOG(TRY)
    
    // if(ParseAssignment(info,exprInfo,true)){
    //     _GLOG(log::out << "-- exit ParseExpression\n";)
    //     return 1;
    // }
    if(info.end())
        return PARSE_ERROR;

    std::vector<ASTExpression*> values;
    std::vector<int> ops;

    bool expectOperator=false;
    while(true){
        Token token = info.get(info.at()+1);
        
        int opType = 0;
        bool ending=false;
        
        if(expectOperator){
            if((opType = IsOp(token))){
                token = info.next();

                ops.push_back(opType);

                _GLOG(log::out << "Operator "<<token<<"\n";)
            } else if(Equal(token,";")){
                token = info.next();
                ending = true;
            } else {
                ending = true;
            }
        }else{
            if(IsDecimal(token)){
                token = info.next();
                ASTExpression* tmp = info.ast->createExpression(ASTExpression::F32);
                info.ast->relocate(tmp)->f32Value = ConvertDecimal(token);
                values.push_back(tmp);
            } else{
                if(attempt){
                    return PARSE_BAD_ATTEMPT;
                }else{
                    ERR_GENERIC;
                    ERRLINE
                    return PARSE_ERROR;
                }
            }
        }
        
        ending = ending || info.end();

        while(values.size()>1){
            if(ops.size()>=2&&!ending){
                int op1 = ops[ops.size()-2];
                int op2 = ops[ops.size()-1];
                if(OpPrecedence(op1)>=OpPrecedence(op2)){
                    ops[ops.size()-2] = op2;
                    ops.pop_back();

                    auto er = values.back();
                    values.pop_back();
                    auto el = values.back();
                    values.pop_back();

                    auto val = info.ast->createExpression(op1);
                    info.ast->relocate(val)->left = el;
                    info.ast->relocate(val)->right = er;

                    values.push_back(val);
                }else{
                    // _GLOG(log::out << "break\n";)
                    break;
                }
            }else if(ending){
                // there is at least two values and one op
                auto er = values.back();
                values.pop_back();
                auto el = values.back();
                values.pop_back();
                int op = ops.back();
                ops.pop_back();

                auto val = info.ast->createExpression(op);
                info.ast->relocate(val)->left = el;
                info.ast->relocate(val)->right = er;

                values.push_back(val);
            }else{
                break;
            }
        }
        // Attempt succeeded. Failure is now considered an error.
        attempt = false;
        expectOperator=!expectOperator;
        if(ending){
            *expression = values.back();
            _GLOG(EXIT)
            return PARSE_SUCCESS;
        }
    }
}
int ParseBody(ParseInfo& info, ASTBody** body, bool multiple);
// returns 0 if syntax is wrong for flow parsing
int ParseFlow(ParseInfo& info, int acc0reg, bool attempt){
    using namespace engone;
    
    _GLOG(TRY)
    
    if(info.end()){
        return PARSE_ERROR;
    }
    Token token = info.get(info.at()+1);
    
    // if(token=="_test_"){
    //     info.next();

    //     Token name = info.get(info.at()+1);
    //     if(!IsName(name)){
    //         ERRT(name) << "Expected a valid variable name '"<<name<<"'\n";
    //         return PARSE_ERROR;
    //     }
    //     info.next();
        
    //     auto var = info.getVariable(name);
    //     if(!var){
    //         ERRT(name) << "Undefined variable '"<<name<<"'\n";
    //         return PARSE_ERROR;
    //     }
    //     int reg = acc0reg;
    //     info.code.add(BC_LOADV, reg, var->frameIndex);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_TEST, reg);
    //     _GLOG(INST << "var '"<<name<<"'\n";)
    // }else if(token=="return"){
    //     token = info.next();
    //     ExpressionInfo expr{};
    //     expr.acc0Reg = acc0reg;
    //     int result = ParseExpression(info,expr,true);
    //     if(result!=PARSE_SUCCESS&&result!=PARSE_BAD_ATTEMPT){
    //         return PARSE_ERROR;
    //     }
    //     ParseInfo::FuncScope& funcScope = info.funcScopes.back();

    //     for(int i = info.scopes.size()-1;i>=funcScope.scopeIndex;i--){
    //         info.dropScope(i);
    //     }
    //     if(result==PARSE_SUCCESS)
    //         info.code.add(BC_RETURN, expr.acc0Reg);
    //     else
    //         info.code.add(BC_RETURN, REG_NULL);
    //     _GLOG(INST << "\n";)
    // }else if(token == "break"){
    //     token = info.next();
    //     if(info.loopScopes.empty()){
    //         ERRTOK << token<<" only allowed in a loop\n";
    //         ERRLINE
    //         return PARSE_ERROR;
    //     }
    //     ParseInfo::LoopScope& loop = info.loopScopes.back();

    //     for(int i = info.scopes.size()-1;i>loop.scopeIndex;i--){
    //         info.dropScope(i);
    //     }

    //     info.code.addLoadNC(loop.jumpReg,loop.breakConstant);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_JUMP, loop.jumpReg);
    //     _GLOG(INST << "\n";)
    // }else if(token=="continue"){
    //     token = info.next();
    //     if(info.loopScopes.empty()){
    //         ERRTOK << token<<" only allowed in a loop\n";
    //         ERRLINE
    //         return PARSE_ERROR;
    //     }
    //     ParseInfo::LoopScope& loop = info.loopScopes.back();
    //     for(int i = info.scopes.size()-1;i>loop.scopeIndex;i--){
    //         info.dropScope(i);
    //     }
    //     info.code.addLoadNC(loop.jumpReg,loop.continueConstant);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_JUMP, loop.jumpReg);
    //     _GLOG(INST << "\n";)
    // } else if(token=="if"){
    //     token = info.next();
    //     ExpressionInfo expr{};
    //     expr.acc0Reg = acc0reg;

    //     int result = ParseExpression(info,expr,false);
    //     if(result!=PARSE_SUCCESS){
    //         return PARSE_ERROR;
    //     }
        
    //     info.makeScope();
        
    //     int jumpAddress = info.code.addConstNumber(-1);
    //     int jumpReg = expr.acc0Reg+1;
    //     info.code.add(BC_NUM,jumpReg);
    //     _GLOG(INST << "jump\n";)
    //     info.code.addLoadNC(jumpReg,jumpAddress);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_JUMPNIF,expr.acc0Reg,jumpReg);
    //     _GLOG(INST << "\n";)

    //     ParseInfo::Scope* scope = &info.scopes.back();
    //     scope->delRegisters.push_back(expr.acc0Reg);
    //     scope->delRegisters.push_back(jumpReg);

    //     result = ParseBody(info,acc0reg+2);
    //     if(result!=PARSE_SUCCESS){
    //         return PARSE_ERROR;
    //     }
    //     scope = &info.scopes.back();

    //     token = {};
    //     if(!info.end())
    //         token = info.get(info.at()+1);
    //     if(token=="else"){
    //         token = info.next();
            
    //         // we don't want to drop jumpReg here. code in else scope uses it.
    //         // way may have wanted to drop jumpreg in if body which is why
    //         // it needs to be in delRegisteers
    //         scope->removeReg(jumpReg);
    //         scope->removeReg(expr.acc0Reg);
    //         info.dropScope();
    //         info.makeScope();
    //         scope->delRegisters.push_back(jumpReg);
    //         scope->delRegisters.push_back(expr.acc0Reg);
            
    //         int elseAddr = info.code.addConstNumber(-1);
    //         info.code.addLoadNC(jumpReg,elseAddr);
    //         _GLOG(INST << "\n";)
    //         info.code.add(BC_JUMP,jumpReg);
    //         _GLOG(INST << "\n";)
            
    //         _GLOG(log::out<<log::GREEN<<"  : JUMP ADDRESS scope: "<<(info.scopes.size()-1)<<"\n";)
    //         info.code.getConstNumber(jumpAddress)->value = info.code.length();
            
    //         int result = ParseBody(info,acc0reg+2);
    //         if(result!=PARSE_SUCCESS){
    //             return PARSE_ERROR;
    //         }
    //         // info.dropScope();
            
    //         _GLOG(log::out<<log::GREEN<<"  : ELSE ADDRESS scope: "<<(info.scopes.size()-1)<<"\n";)
    //         info.code.getConstNumber(elseAddr)->value = info.code.length()
    //             + scope->getInstsBeforeDelReg(info);
    //     }else{
    //         info.code.getConstNumber(jumpAddress)->value = info.code.length()
    //             + scope->getInstsBeforeDelReg(info);
    //         _GLOG(log::out<<log::GREEN<<"  : JUMP ADDRESS scope: "<<(info.scopes.size()-1)<<"\n";)
                
    //     }   
    //     info.dropScope();
    // } else if(token=="while"){
    //     token = info.next();
    //     info.makeScope();

    //     int jumpReg = acc0reg;
    //     info.code.add(BC_NUM,jumpReg);
    //     _GLOG(INST << "jump\n";)
    //     ParseInfo::Scope& scope = info.scopes.back();
    //     scope.delRegisters.push_back(jumpReg);
    //     int startAddress = info.code.addConstNumber(info.code.length());
    //     _GLOG(log::out<<log::GREEN<<"  : START ADDRESS scope: "<<(info.scopes.size()-1)<<"\n";)
    //     int endAddress = info.code.addConstNumber(-1);
    //     int breakAddress = info.code.addConstNumber(-1);

    //     ExpressionInfo expr{};
    //     expr.acc0Reg = acc0reg+1;
    //     // scope.delRegisters.push_back(expr.acc0Reg);
    //     int result = ParseExpression(info,expr,false);
    //     if(result!=PARSE_SUCCESS){
    //         return PARSE_ERROR;
    //     }


    //     info.code.addLoadNC(jumpReg,endAddress);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_JUMPNIF,expr.acc0Reg,jumpReg);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_DEL,expr.acc0Reg);
    //     _GLOG(INST << "\n";)
        
    //     info.loopScopes.push_back({});
    //     ParseInfo::LoopScope& loop = info.loopScopes.back();
    //     loop.iReg = 0; // Todo: add value for #i
    //     loop.jumpReg = jumpReg;
    //     loop.continueConstant = startAddress;
    //     loop.breakConstant = breakAddress;
    //     loop.scopeIndex = info.scopes.size()-1;

    //     result = ParseBody(info,expr.acc0Reg+1);
    //     if(result!=PARSE_SUCCESS){
    //         return PARSE_ERROR;
    //     }

    //     info.loopScopes.erase(info.loopScopes.begin()+info.loopScopes.size()-1);

        
    //     info.code.addLoadNC(jumpReg,startAddress);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_JUMP,jumpReg);
    //     _GLOG(INST << "\n";)
        
    //     info.code.getConstNumber(endAddress)->value = info.code.length();
    //     _GLOG(log::out << log::GREEN<<"  : END ADDRESS scope: "<<(info.scopes.size()-1)<<"\n";)

    //     info.code.add(BC_DEL,expr.acc0Reg);
    //     _GLOG(INST << "\n";)

    //     info.code.getConstNumber(breakAddress)->value = info.code.length();
    //     _GLOG(log::out << log::GREEN<<"  : END BREAK ADDRESS scope: "<<(info.scopes.size()-1)<<"\n";)

    //     info.dropScope();
    // } else if(token=="for") {
    //     token = info.next();
        
    //     bool createdVar=false;
    //     bool createdMax=false;
    //     int iReg = acc0reg;
    //     int maxReg = acc0reg+1;
    //     int tempReg = acc0reg+2;
    //     int jumpReg = acc0reg+3;
    //     int unusedReg = acc0reg+4; // passed to parse functions

    //     info.makeScope();

    //     info.code.add(BC_NUM,tempReg);
    //     _GLOG(INST << "temp\n";)
    //     info.code.add(BC_NUM,jumpReg);
    //     _GLOG(INST << "jump\n";)

    //     ParseInfo::Scope& scope = info.scopes.back();
    //     scope.delRegisters.push_back(tempReg);
    //     scope.delRegisters.push_back(jumpReg);

    //     int startAddress = info.code.addConstNumber(-1);
        
    //     ExpressionInfo expr{};
    //     expr.acc0Reg = unusedReg;

    //     int stumpIndex = info.code.length();
    //     info.code.add(Instruction{0,(uint8)iReg}); // add stump
    //     _GLOG(INST << "May become BC_NUM\n";)

    //     bool wasAssigned=0;
    //     int result = ParseAssignment(info,expr,true,false);
    //     if(result==PARSE_SUCCESS)
    //         wasAssigned=true;
    //     if(result==PARSE_BAD_ATTEMPT){
    //         result = ParseExpression(info,expr,true);
    //     }
    //     Token varName{};
    //     if(result==PARSE_ERROR){
    //         return PARSE_ERROR;
    //     } else if(result==PARSE_NO_VALUE){
    //         ERRT(info.now()) << "Expected value from assignment '"<<info.now()<<"'\n";
    //         ERRLINE
    //         return PARSE_ERROR;
    //     } else if(result==PARSE_BAD_ATTEMPT){
    //         // undefined variable "for i : 5"
    //         token = info.get(info.at()+1);
    //         Token next = info.get(info.at()+2);
    //         if(IsName(token) && next == ":" && !(next.flags&TOKEN_QUOTED)){
    //             info.next();
    //             auto var = info.getVariable(token);
    //             if(!var){
    //                 // Turn token into variable?
                    
    //                 auto var = info.addVariable(token);
    //                 var->frameIndex = info.frameOffsetIndex++;
    //                 varName = token;
    //                 scope.variableNames.push_back(token);
                    
    //                 // createdVar=true;
    //                 info.code.add(BC_NUM, iReg);
    //                 _GLOG(INST << "var reg\n";)

    //                 info.code.add(BC_STOREV, iReg,var->frameIndex);
    //                 _GLOG(INST << "store "<<token<<"\n";)
    //             }else{
    //                 ERRT(token) << "for cannot loop with a defined variable ("<<token<<") \n";
    //                 // info.nextLine();
    //                 return PARSE_ERROR;
    //             }
    //             token = info.next();
    //         }else{
    //             ERR_GENERIC;
    //             // ERRLINE
    //             // info.nextLine();
    //             return PARSE_ERROR;
    //         }
    //     } else if(result==PARSE_SUCCESS){
    //         // max or var
    //         // we got start in acc0reg or max.
    //         token = info.get(info.at()+1);
    //         if(token==":"){
    //             token = info.next();

    //             // createdVar=true;
                
    //             scope.delRegisters.push_back(iReg);
    //             info.code.add(BC_MOV,expr.acc0Reg,iReg);
    //             _GLOG(INST << "acc -> var\n";)
    //         } else {
    //             // "for i = 5"
    //             createdVar=true;
    //             Instruction& inst = info.code.get(stumpIndex);
    //             inst.type = BC_NUM;
    //             inst.reg0 = iReg;
              
    //             scope.delRegisters.push_back(iReg);

    //             info.code.getConstNumber(startAddress)->value = stumpIndex+1;
                
    //             // createdMax=true;
    //             if(!wasAssigned){
    //                 createdMax=true;
    //             }
    //             info.code.add(BC_MOV,expr.acc0Reg,maxReg);
    //             _GLOG(INST << "acc -> max\n";)
    //         }
    //     }
            
    //     if(token==":"){
    //         ExpressionInfo expr{};
    //         expr.acc0Reg = unusedReg;
            
    //         info.code.getConstNumber(startAddress)->value = info.code.length();
    //         // log::out << "For Start Address\n";
    //         bool wasAssigned=false;
    //         int result = ParseAssignment(info,expr,true,true);
    //         if(result==PARSE_SUCCESS)
    //             wasAssigned=true;
    //         if(result==PARSE_BAD_ATTEMPT){
    //             result = ParseExpression(info,expr,true);
    //         }
    //         if(result==PARSE_SUCCESS){
    //             if(!wasAssigned)
    //                 createdMax=true;
    //             info.code.add(BC_MOV,expr.acc0Reg,maxReg);
    //             _GLOG(INST << "acc -> max\n";)
    //         }else if(result==PARSE_NO_VALUE){
    //             ERRT(info.now()) << "Expected value from assignment '"<<info.now()<<"'\n";
    //             ERRLINE;
    //             return PARSE_ERROR;
    //         }else if(result ==PARSE_BAD_ATTEMPT){
    //             ERRTL(info.now()) << "Bad attempt '"<<info.now()<<"'\n";
    //             ERRLINE;
    //             return PARSE_ERROR;
    //         }else{
    //             // Todo: detect type of failure in parser. If the attempt failed
    //             //  then we log here. If other type of error we don't print here.
    //             // ERRTL(token) << "Parsing failed after '"<<token<<"'\n";
    //             // ERRLINE
    //             // info.nextLine();
    //             return PARSE_ERROR;
    //         }
    //     }
    //     // _GLOG(log::out << "    Start Address: "<<info.code.getConstNumber(startAddress)->value<<"\n";)
    //     _GLOG(log::out<<log::GREEN<<"  : START ADDRESS scope: "<<(info.scopes.size()-1)<<"\n";)

    //     int endAddress = info.code.addConstNumber(-1);

    //     info.loopScopes.push_back({});
    //     ParseInfo::LoopScope& loop = info.loopScopes.back();
    //     loop.iReg = iReg;
    //     loop.jumpReg = jumpReg;
    //     loop.continueConstant = startAddress;
    //     loop.breakConstant = endAddress;
    //     loop.scopeIndex = info.scopes.size()-1;
        
    //     // if(varName.str){
    //     // only necessary after ParseBody
    //     //     auto varptr = info.getVariable(varName);
    //     //     info.code.add(BC_LOADV, iReg,varptr->frameIndex);
    //     //     _GLOG(INST << "load "<<varName<<"\n";)
    //     // }
    //     info.code.add(BC_LESS, iReg, maxReg, tempReg);
    //     _GLOG(INST << "\n";)
    //     if(createdMax){ 
    //         info.code.add(BC_DEL,maxReg);
    //         _GLOG(INST << "\n";)
    //     }

    //     info.code.addLoadNC(jumpReg,endAddress);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_JUMPNIF, tempReg, jumpReg);
    //     _GLOG(INST << "\n";)
        
    //     result = ParseBody(info,unusedReg);
    //     if(result!=PARSE_SUCCESS){
    //         return PARSE_ERROR;
    //     }
        
    //     int incrIndex = info.code.addConstNumber(1);
    //     info.code.addLoadNC(tempReg,incrIndex);
    //     _GLOG(INST << "\n";)
    //     if(varName.str){
    //         // if body changed value we need to get the new one
    //         auto varptr = info.getVariable(varName);
    //         info.code.add(BC_LOADV, iReg,varptr->frameIndex);
    //         _GLOG(INST << "load "<<varName<<"\n";)
    //     }
    //     info.code.add(BC_ADD,iReg,tempReg,iReg);
    //     _GLOG(INST << "increment var\n";)
        
    //     info.code.addLoadNC(jumpReg,startAddress);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_JUMP, jumpReg);
    //     _GLOG(INST << "\n";)
        
    //     info.code.getConstNumber(endAddress)->value = info.code.length();
    //     _GLOG(log::out << "    End Address: "<<info.code.getConstNumber(endAddress)->value<<"\n";)
        
    //     info.loopScopes.erase(info.loopScopes.begin()+info.loopScopes.size()-1);

    //     // If the loop didn't run once we would delete a potential variable in the
    //     // scope even though it wasn't made.
    //     info.dropScope();
    // } else if(token=="each"){
    //     info.next();

    //     info.makeScope();

    //     int jumpReg = acc0reg;
    //     int iReg = acc0reg+1;
    //     int tempReg = acc0reg+2;
    //     int beginReg = acc0reg+3;
    //     int endReg = acc0reg+4;
    //     int chrReg = acc0reg+5;
    //     int tempStrReg = acc0reg+6;
    //     int tempStrReg2= acc0reg+7;
    //     int varReg  = acc0reg+8;
    //     int unusedReg  = acc0reg+9;

    //     info.code.add(BC_NUM,jumpReg);
    //     _GLOG(INST << "jump\n";)
    //     info.code.add(BC_NUM,iReg);
    //     _GLOG(INST << "i\n";)
    //     info.code.add(BC_NUM,tempReg);
    //     _GLOG(INST << "temp\n";)
    //     info.code.add(BC_NUM,beginReg);
    //     _GLOG(INST << "begin\n";)
    //     info.code.add(BC_NUM,endReg);
    //     _GLOG(INST << "end\n";)
    //     info.code.add(BC_STR,tempStrReg);
    //     _GLOG(INST << "tempstr\n";)
    //     info.code.add(BC_STR,tempStrReg2);
    //     _GLOG(INST << "tempstr\n";)

    //     ParseInfo::Scope& scope = info.scopes.back();
    //     scope.delRegisters.push_back(jumpReg);
    //     scope.delRegisters.push_back(iReg);
    //     scope.delRegisters.push_back(tempReg);
    //     scope.delRegisters.push_back(beginReg);
    //     scope.delRegisters.push_back(endReg);
    //     scope.delRegisters.push_back(tempStrReg);
    //     scope.delRegisters.push_back(tempStrReg2);

    //     Token name = info.get(info.at()+1);
    //     Token colon = info.get(info.at()+2);

    //     bool createdVar=false;
    //     if(colon == ":"){
    //         info.next();
    //         info.next();
    //         if(!IsName(name)){
    //             ERRT(name) << "Expected a variable name ('"<<name<<"' is not valid)\n";
    //             return PARSE_ERROR;
    //         }
    //         auto var = info.getVariable(name);
    //         if(var){
    //             ERRT(name) << "Expected a non-defined variable ('"<<name<<"' is defined)\n";
    //             return PARSE_ERROR;
    //         }

    //         var = info.addVariable(name);
    //         var->frameIndex = info.frameOffsetIndex++;
    //         // scope.variableNames.push_back(name);
    //         // Note: chrReg which is what var will refer to is added to
    //         //  delRegisters when we don't specify a variable.
    //         //  There is no need to add variable to scope so it
    //         //  is deleted because it already will be deleted.
    //         //  there is also some special cases with break and return
    //         //  which is handled. We don't want to handle it
    //         //  twice with variable and delRegister.
    //         createdVar=true;
    //     }
        
    //     // only allow variable not expression (each tmp : onlyvar)
    //     // Token listName = info.next();
    //     // auto pair = info.var.find(listName);
    //     // if(pair==info.variables.end()){
    //     //     ERRT(listName) << "Must be a defined variable ('"<<listName<<"' is not)\n";
    //     //     return PARSE_ERROR;
    //     // }
    //     // info.code.add(BC_LOADV,varReg,pair->second.frameIndex);
    //     // _GLOG(INST << "\n";)

    //     ExpressionInfo expr{};
    //     expr.acc0Reg = varReg;
    //     ExpressionInfo expr2{};
    //     expr2.acc0Reg = varReg;

    //     // Todo: ParseAssignment here? 
    //     //  each str : tmp = "okay" {}   <- would be possible. Is it useful?
    //     int result = ParseAssignment(info,expr,true,false);
    //     if(result==PARSE_BAD_ATTEMPT){
    //         result = ParseExpression(info,expr2,false);
    //         if(result==PARSE_SUCCESS){
    //             scope.delRegisters.push_back(varReg);
    //         }
    //     }
    //     if(result!=PARSE_SUCCESS){
    //         return PARSE_ERROR;
    //     }


    //     int startAddress = info.code.addConstNumber(info.code.length());
    //     _GLOG(log::out<<log::GREEN<<"  : START ADDRESS ["<<startAddress<<"]\n";)

    //     int endAddress = info.code.addConstNumber(-1);
    //     int elseAddress = info.code.addConstNumber(-1);
    //     int bodyIfAddress = info.code.addConstNumber(-1);
    //     int lastAddress = info.code.addConstNumber(-1);
    //     int continueAddress = info.code.addConstNumber(-1);
        
    //     info.code.add(BC_LEN, varReg,tempReg);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_LESS,iReg,tempReg,tempReg);
    //     _GLOG(INST << "\n";)

    //     info.code.addLoadNC(jumpReg,endAddress);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_JUMPNIF,tempReg,jumpReg);
    //     _GLOG(INST << "\n";)
        
    //     info.code.add(BC_MOV,varReg,chrReg);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_SUBSTR,chrReg,iReg,iReg);
    //     _GLOG(INST << "\n";)

    //     int oneConstant = info.code.addConstNumber(1);
    //     info.code.addLoadNC(tempReg,oneConstant);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_ADD,iReg,tempReg,iReg);
    //     _GLOG(INST << "\n";)

    //     Token spaceToken = " ";
    //     int spaceConstant = info.code.addConstString(spaceToken);
    //     Token lineToken = "\n";
    //     int lineConstant = info.code.addConstString(lineToken);
        
    //     info.code.addLoadSC(tempStrReg,spaceConstant);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_EQUAL,chrReg,tempStrReg,tempStrReg);
    //     _GLOG(INST << "\n";)

    //     info.code.addLoadSC(tempStrReg2,lineConstant);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_EQUAL,chrReg,tempStrReg2,tempStrReg2);
    //     _GLOG(INST << "\n";)

    //     info.code.add(BC_OR,tempStrReg,tempStrReg2,tempStrReg);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_DEL,chrReg);
    //     _GLOG(INST << "\n";)
        
    //     info.code.addLoadNC(jumpReg,elseAddress);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_JUMPNIF,tempStrReg,jumpReg);
    //     _GLOG(INST << "\n";)

    //     // end = i-2
    //     int twoConstant = info.code.addConstNumber(2);
    //     info.code.addLoadNC(tempReg,twoConstant);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_SUB,iReg,tempReg,endReg);
    //     _GLOG(INST << "\n";)

    //     info.code.addLoadNC(jumpReg,bodyIfAddress);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_JUMP,jumpReg);
    //     _GLOG(INST << "\n";)

    //     info.code.getConstNumber(elseAddress)->value = info.code.length();
    //     _GLOG(log::out << log::GREEN<<"  : ELSE ADDRESS ["<<elseAddress<<"]\n";)

    //     info.code.add(BC_LEN, varReg, tempReg);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_EQUAL,iReg,tempReg,tempReg);
    //     _GLOG(INST << "\n";)

    //     info.code.addLoadNC(jumpReg,startAddress);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_JUMPNIF,tempReg,jumpReg);
    //     _GLOG(INST << "\n";)

    //     info.code.addLoadNC(tempReg,oneConstant);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_SUB,iReg,tempReg,endReg);
    //     _GLOG(INST << "\n";)

    //     info.code.getConstNumber(bodyIfAddress)->value = info.code.length();
    //     _GLOG(log::out << log::GREEN<<"  : BODY IF ADDRESS ["<<elseAddress<<"]\n";)

    //     info.code.addLoadNC(tempReg,oneConstant);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_ADD,tempReg,endReg,tempReg);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_SUB,tempReg,beginReg,tempReg);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_GREATER,tempReg,REG_ZERO,tempReg);
    //     _GLOG(INST << "\n";)

    //     info.code.addLoadNC(jumpReg,lastAddress);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_JUMPNIF,tempReg,jumpReg);
    //     _GLOG(INST << "\n";)
        
    //     info.code.add(BC_MOV,varReg,chrReg); // resusing the free chrReg
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_SUBSTR,chrReg,beginReg,endReg);
    //     _GLOG(INST << "\n";)

    //     if(createdVar){
    //         auto var = info.getVariable(name);
    //         info.code.add(BC_STOREV, chrReg, var->frameIndex);
    //         _GLOG(INST << "store "<<name<<"\n";)
    //     }

    //     /*
    //     each "1 2" {
    //         print late
    //         late = "WOW"
    //     }
    //     is an extra scope needed here since the above code would print
    //     "late" and then "WOW" since the variable isn't deleted since we
    //     don't clear the scope?
    //     HAHA! actually,the parser evaluates late as a constant which
    //     the instructions uses.
    //     */
    //     info.loopScopes.push_back({});
    //     ParseInfo::LoopScope& loop = info.loopScopes.back();
    //     loop.iReg = iReg; // Todo: add value for #i
    //     loop.vReg = chrReg; // Todo: add value for #i
    //     loop.jumpReg = jumpReg;
    //     loop.continueConstant = continueAddress;
    //     loop.breakConstant = continueAddress;
    //     loop.scopeIndex = info.scopes.size()-1;

    //     if(!createdVar){
    //         scope.delRegisters.push_back(chrReg);
    //     }

    //     int result2 = ParseBody(info,unusedReg);
    //     if(result2!=PARSE_SUCCESS){
    //         return PARSE_ERROR;
    //     }
    //     // reallocation of scopes may have occured so the scope
    //     // reference we had may be invalid. We need to get it again.
    //     ParseInfo::Scope& scope2 = info.scopes.back();
    //     if(!createdVar){
    //         scope2.removeReg(chrReg);
    //     }
        
    //     info.code.getConstNumber(continueAddress)->value = info.code.length();
    //     _GLOG(log::out << log::GREEN<<"  : CONTINUE/BREAK ADDRESS ["<<continueAddress<<"]\n";)
        
    //     info.code.add(BC_DEL,chrReg);
    //     _GLOG(INST << "\n";)

    //     info.loopScopes.erase(info.loopScopes.begin()+info.loopScopes.size()-1);
        
    //     info.code.getConstNumber(lastAddress)->value = info.code.length();
    //     _GLOG(log::out << log::GREEN<<"  : BEGIN ADDRESS ["<<lastAddress<<"]\n";)

    //     info.code.add(BC_ADD,iReg,REG_ZERO,beginReg);
    //     _GLOG(INST << "\n";)

    //     info.code.addLoadNC(jumpReg,startAddress);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_JUMP,jumpReg);
    //     _GLOG(INST << "\n";)
        
    //     info.code.getConstNumber(endAddress)->value = info.code.length();
    //     _GLOG(log::out << log::GREEN<<"  : END ADDRESS ["<<endAddress<<"]\n";)

    //     info.dropScope();
    // } else {
    //     if(attempt){
    //         return PARSE_BAD_ATTEMPT;
    //     }else{
    //         ERR_GENERIC;
    //         ERRLINE
    //         // info.nextLine();
    //         return PARSE_ERROR;
    //     }
    // }
    _GLOG(EXIT)
    return PARSE_SUCCESS;
}
// #fass
int ParseAssignment(ParseInfo& info, ASTStatement** statement, bool attempt){
    using namespace engone;
    
    _GLOG(TRY)
    
    if(info.tokens.length() < info.index+2){
        // not enough tokens for assignment
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }
        _GLOG(log::out <<log::RED<< "CompilerError: to few tokens for assignment\n";)
        // info.nextLine();
        return PARSE_ERROR;
    }
    Token name = info.get(info.at()+1);
    Token assign = info.get(info.at()+2);
    Token token = info.get(info.at()+3);
    
    if(Equal(assign,"=")){
        return PARSE_BAD_ATTEMPT;
    }
    if(IsName(name)){
        info.next(); // prevent loop
        ERRT(name) << "expected a valid name for assignment ('"<<name<<"' isn't)\n";
        return PARSE_ERROR;
    }

    info.next();
    info.next();

    *statement = info.ast->createStatement(ASTStatement::ASSIGN);

    info.ast->relocate(*statement)->name = new std::string(name);


    ASTExpression* expression=0;

    int result = 0;
    result = ParseExpression(info,&expression,false);

    if(result!=PARSE_SUCCESS){
        return PARSE_ERROR;
    }

    info.ast->relocate(*statement)->expression = expression;

    // memcpy(info.ast->relocate(*statement)->name.str);

    // Todo: handle ++a
    
    // if(assign=="++"||assign=="--"){
    //     // a++
    //     auto var = info.getVariable(name);
    //     if(!var){
    //         if(attempt){
    //             return PARSE_BAD_ATTEMPT;
    //         }
    //         ERRT(name) << "variable '"<<name<<"' must be defined when using '"<<assign<<"'\n";
    //         return PARSE_ERROR;   
    //     }
    //     info.next();
    //     info.next();
    //     // Todo: a=9;b = a++ <- b would be 10 with current code. Change it to
    //     //  make a copy which b is set to then do addition on variable.
    //     //  how does it work with requestCopy?
    //     int finalReg = exprInfo.acc0Reg;
    //     info.code.add(BC_LOADV,finalReg,var->frameIndex);
    //     _GLOG(INST << "load var '"<<name<<"'\n";)
    //     int constone = info.code.addConstNumber(1);
    //     info.code.add(BC_NUM,finalReg+1);
    //     _GLOG(INST << "\n";)
    //     info.code.addLoadNC(finalReg+1,constone);
    //     if(assign == "++")
    //         info.code.add(BC_ADD,finalReg,finalReg+1,finalReg);
    //     if(assign == "--")
    //         info.code.add(BC_SUB,finalReg,finalReg+1,finalReg);
    //     _GLOG(INST <<"\n";)
    //     info.code.add(BC_DEL,finalReg+1);
    //     _GLOG(INST <<"\n";)
        
    //     if(requestCopy){
    //         info.code.add(BC_COPY,finalReg,finalReg);
    //         _GLOG(INST << "copy requested\n";)
    //         _GLOG(EXIT)
    //         return PARSE_SUCCESS;
    //     }
    //     return PARSE_SUCCESS;
    // }
    
    // if(!IsName(name)){
    //     if(attempt){
    //         return PARSE_BAD_ATTEMPT;
    //     }else{
    //         info.next();
    //         ERRT(name) << "unexpected "<<name<<", wanted a variable for assignment\n";
    //         // info.nextLine();
    //         return PARSE_ERROR;
    //     }
    // }
    // if(assign=="="&&token=="{"){
    //     info.next();
    //     info.next();
    //     // don't next on {, ParseBody needs it.
    //     auto func = info.getFunction(token);
    //     auto var = info.getVariable(token);
    //     if(var){
    //         ERRT(token) << "'"<<token<<"' already defined as variable\n";
    //         // info.nextLine();
    //         return PARSE_ERROR;
    //     }else if(!func){
    //         // Note: assign new function
    //         func = info.addFunction(name);
    //         info.scopes.back().functionsNames.push_back(name);
    //         int constIndex = info.code.addConstNumber(-1);
    //         int jumpReg = exprInfo.acc0Reg;
    //         info.code.add(BC_NUM,jumpReg);
    //         _GLOG(INST << "jump\n";)
    //         info.code.addLoadNC(jumpReg,constIndex);
    //         _GLOG(INST << "\n";)
    //         info.code.add(BC_JUMP, jumpReg);
    //         _GLOG(INST << "Skip function body '"<<name<<"'\n";)
            
    //         func->jumpAddress = info.code.length();

    //         info.makeScope();
    //         auto& scope = info.scopes.back();
    //         scope.delRegisters.push_back(REG_ARGUMENT);
    //         info.funcScopes.push_back({(int)info.scopes.size()-1});
            
    //         // token = info.next will be {
    //         int result = ParseBody(info,exprInfo.acc0Reg+1);

    //         info.dropScope();
    //         // Todo: consider not returning when failed since the
    //         //  final code would have a higher chance of executing
    //         //  correctly with the "tailing" instructions below.
    //         // Todo: remove the function from the map on failure.
    //         //  It shouldn't exist since we failed.
    //         if(result!=PARSE_SUCCESS){
    //             return PARSE_ERROR;
    //         }

    //         info.code.add(BC_RETURN,REG_NULL);
    //         _GLOG(INST << "\n";)

    //         info.code.getConstNumber(constIndex)->value = info.code.length();
    //         info.code.add(BC_DEL,jumpReg);
    //         _GLOG(INST << "skipped to here\n";)

    //         _GLOG(EXIT)
    //         return PARSE_NO_VALUE;
    //     }else{
    //         ERRT(token) << "function '"<<token<<"' already defined\n";
    //         // info.nextLine();
    //         return PARSE_ERROR;
    //     }
    //     _GLOG(EXIT)
    //     return PARSE_SUCCESS;
    // }
    // int unusedReg = exprInfo.acc0Reg;
    // int indexReg = 0;
    // if (assign=="["){
    //     auto varName = info.getVariable(name);
    //     if(!varName){
    //         ERRT(name) << "variable must be defined to use [x] ('"<<varName<<"' wasn't)\n";
    //         ERRLINE
    //         return PARSE_ERROR;
    //     }
    //     info.next();
    //     info.next();
    //     int result = ParseExpression(info,exprInfo,false);
    //     if(result!=PARSE_SUCCESS){
    //         return PARSE_ERROR;
    //     }
    //     indexReg = exprInfo.acc0Reg;
    //     unusedReg = indexReg+1;
    //     Token token = info.get(info.at()+1);
    //     if (token!="]"){
    //         ERRT(token) << "expected ], not "<<token<<"\n";
    //         return PARSE_ERROR;
    //     }
    //     info.next();
    //     assign = info.get(info.at()+1);

    //     if(assign!="="){
    //         ERRT(assign) << "char assignment only works for = (not '"<<assign<<"')\n";
    //         ERRLINE
    //         return PARSE_ERROR;
    //     }
    // } 
    
    // if(!(assign=="=" ||assign=="+=" ||assign=="-=" ||assign=="*=" ||assign=="/=")){
    //     if(attempt){
    //         return PARSE_BAD_ATTEMPT;
    //     }else{
    //         info.next(); // move forward to prevent infinite loop? altough nextline would fix it?
    //         ERRT(assign) << "unexpected "<<name<<", wanted token for assignment\n";
    //         // info.nextLine();
    //         return PARSE_ERROR;
    //     }
    // }
    // if (indexReg==0) {
    //     info.next();
    // }
    // info.next();
    // ExpressionInfo expr{};
    // expr.acc0Reg = unusedReg;
    // int result=0;

    // result = ParseExpression(info,expr,false);
    // if(result!=PARSE_SUCCESS){
    //     return PARSE_ERROR;
    // }
    // int finalReg = expr.acc0Reg;
    // if(expr.regCount==0){
    //     log::out << log::RED<<"CompilerWarning: regCount == 0 after expression from assignment\n";   
    // }
            
    // auto var = info.getVariable(name);
    // if(!var){
    //     if(assign != "="){
    //         ERRT(name) << "variable '"<<name<<"' must be defined when using '"<<assign<<"'\n";
    //         return PARSE_ERROR;   
    //     }
    //     var = info.addVariable(name);
    //     // Note: assign new variable
    //     var->frameIndex = info.frameOffsetIndex++;
    //     // Todo: bound check on scopes, global scope should exist though
    //     auto& scope = info.scopes.back();
    //     scope.variableNames.push_back(name);
    // }

    // if(indexReg!=0){
    //     info.code.add(BC_LOADV,finalReg+1,var->frameIndex);
    //     _GLOG(INST << "load var '"<<name<<"'\n";)

    //     if (assign=="="){
    //         info.code.add(BC_SETCHAR,finalReg,indexReg,finalReg+1);
    //         _GLOG(INST <<"\n";)
    //         info.code.add(BC_DEL,indexReg);
    //         _GLOG(INST <<"\n";)
    //         info.code.add(BC_DEL,finalReg);
    //         _GLOG(INST <<"\n";)
    //         info.code.add(BC_MOV,finalReg+1,finalReg);
    //         _GLOG(INST <<"\n";)
    //     }else {
    //         // handle above
    //     }
    // }else{
    //     if(assign=="="){
    //         info.code.add(BC_STOREV, finalReg, var->frameIndex);
    //         _GLOG(INST << "store "<<name<<"\n";)
    //     } else {
    //         info.code.add(BC_LOADV,finalReg+1,var->frameIndex);
    //         _GLOG(INST << "load var '"<<name<<"'\n";)
    //         if(assign == "+=")
    //             info.code.add(BC_ADD,finalReg+1,finalReg,finalReg+1);
    //         if(assign == "-=")
    //             info.code.add(BC_SUB,finalReg+1,finalReg,finalReg+1);
    //         if(assign == "*=")
    //             info.code.add(BC_MUL,finalReg+1,finalReg,finalReg+1);
    //         if(assign == "/=")
    //             info.code.add(BC_DIV,finalReg+1,finalReg,finalReg+1);
    //         _GLOG(INST <<"\n";)
    //         info.code.add(BC_DEL,finalReg);
    //         _GLOG(INST <<"\n";)
    //         info.code.add(BC_MOV,finalReg+1,finalReg);
    //         _GLOG(INST <<"\n";)
    //     }
    // }
    _GLOG(EXIT)
    return PARSE_SUCCESS;   
}
int ParseCommand(ParseInfo& info, ExpressionInfo& exprInfo, bool attempt){
    using namespace engone;
    _GLOG(TRY)
    
    // if(info.end()){
    //     log::out <<log::RED<< "Sudden end in ParseCommand?";
    //     return PARSE_ERROR;
    // }
    // // Token token = info.next();
    // Token varName = info.get(info.at()+1);
    // bool isAsync=false;

    // if (varName == "#" && !(varName.flags&TOKEN_SUFFIX_SPACE)){
    //     Token token = info.get(info.at()+2);
    //     if (token=="async"){
    //         isAsync = true;
    //         varName = info.get(info.at()+3);
    //         info.next();
    //         info.next();
    //     }
    // }
    // // TODO: don't allow async when piping variables

    // auto var = info.getVariable(varName);
    // if(var){
    //     info.next();
    //     Token token = info.get(info.at()+1);
    //     int instruction = 0;
    //     if(token=="<") instruction = BC_READ_FILE;
    //     else if(token==">") instruction = BC_WRITE_FILE;
    //     else if(token==">>") instruction = BC_APPEND_FILE;
    //     if(instruction){
    //         info.next();
            
    //         int varReg = exprInfo.acc0Reg;
    //         int fileReg = exprInfo.acc0Reg+1;
            
    //         info.code.add(BC_LOADV,varReg,var->frameIndex);
    //         _GLOG(INST << varName<<"\n";)

    //         ExpressionInfo expr{};
    //         expr.acc0Reg = fileReg;
    //         int result = ParseExpression(info,expr,true);
    //         if(result==PARSE_SUCCESS){
    //             // info.next();
    //             // info.code.add(BC_LOADV,fileReg,fileVar->frameIndex);
    //             // _GLOG(INST << varName<<"\n";)
    //             info.code.add(instruction,varReg,fileReg);
    //             _GLOG(INST << "\n";)
    //             info.code.add(BC_DEL,fileReg);
    //             _GLOG(INST << "\n";)
    //         } else if (result==PARSE_BAD_ATTEMPT){
    //             token = CombineTokens(info); // read file
    //             // we assume it's valid?
            
    //             info.code.add(BC_STR,fileReg);
    //             _GLOG(INST << "\n";)
    //             int constIndex = info.code.addConstString(token);
    //             info.code.addLoadSC(fileReg,constIndex);
    //             _GLOG(INST << token<<"\n";)
                
    //             info.code.add(instruction,varReg,fileReg);
    //             _GLOG(INST << "\n";)
    //             info.code.add(BC_DEL,fileReg);
    //             _GLOG(INST << "\n";)
    //         }else
    //             return PARSE_ERROR;
    //         // TODO: check for regcount == 0?
            
    //         // Note: PARSE_SUCCESS would indicated varReg (eg, variable value) to be "returned".
    //         //   That's maybe not quite right since we would need to copy the value otherwise
    //         //   we would have two variables pointing to the same value.
    //         return PARSE_NO_VALUE;
    //     }
    //     else{
    //         ERRT(token) << "Expected piping (not '"<<token<<"', | not supported yet) after variable '"<<varName<<"'\n";
    //         return PARSE_ERROR;   
    //     }
    // }
    
    // Token token = CombineTokens(info);
    // Token cmdName = token;
    // if(!token.str){
    //     // info.errors++;
    //     // log::out <<log::RED <<"DevError ParseCommand, token.str was null\n";
    //     // return PARSE_ERROR;
    //     token = info.next(); 
    //     // we didn't get a token from CombineTokens but we have to move forward
    //     // to prevent infinite loop.
    //     return PARSE_NO_VALUE;
    //     // succeeded with doing nothing
    //     // happens with a single ; in body parsing.

    // }
    
    // // if(token.flags&TOKEN_QUOTED){
    // // }else if(IsName(token)){
    // //     // quoted exe command
    // //     // internal function
    // // } else{
    // //     // raw exe command
    // // }
    // int funcReg = exprInfo.acc0Reg+exprInfo.regCount;
    // exprInfo.regCount++;
    // auto func = info.getFunction(token);
    // // bool codeFunc=false;
    // if(func){
    //     // func in bytecode
    //     if(func->constIndex==-1){
    //         func->constIndex = info.code.addConstNumber(func->jumpAddress);
    //     }
    //     info.code.add(BC_NUM,funcReg);
    //     _GLOG(INST << "\n";)
    //     info.code.addLoadNC(funcReg,func->constIndex);
    //     _GLOG(INST << "func index to '"<<token<<"'\n";)
    //     // codeFunc=true;

    // }else{
    //     // executable or internal func
    //     int index = info.code.addConstString(token);
    //     info.code.add(BC_STR,funcReg);
    //     _GLOG(INST << "name\n";)
    //     info.code.addLoadSC(funcReg, index);
    //     _GLOG(INST << "cmd constant "<<token<<"\n";)
    // }
    
    // int argReg = REG_NULL;
    
    // bool ending = (token.flags&TOKEN_SUFFIX_LINE_FEED) | info.end();
    // if(!ending){
        
    //     int tempStrReg = exprInfo.acc0Reg+exprInfo.regCount+1;
    //     info.code.add(BC_STR,tempStrReg);
    //     _GLOG(INST << "temp str\n";)
        
    //     int tempReg = exprInfo.acc0Reg+exprInfo.regCount+2;
        
    //     bool addSpace=false;
    //     while(!info.end()){
    //         Token prev = info.now();
    //         token = info.next();
    //         // log::out << "TOK: '"<<token<<"'\n";
    //         #define ARG_STR 1
    //         #define ARG_TEMP 2
    //         #define ARG_DEL 4
    //         int whichArg=0;
    //         if(token=="#"&&!(token.flags&TOKEN_QUOTED)){
    //             if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED)){
    //                 ERRTOK << "expected a directive\n";
    //                 return PARSE_ERROR;
    //             }
    //             token = info.next();
    //             if(token == "run"){
    //                 ExpressionInfo expr{};
    //                 expr.acc0Reg = tempReg;
    //                 int result = ParseCommand(info,expr,false);
    //                 if(result==PARSE_SUCCESS){
    //                     if(expr.regCount!=0){
    //                         whichArg=ARG_TEMP|ARG_DEL;
    //                     }
    //                 }else{
    //                     return PARSE_ERROR;
    //                 }
    //             }else if(token=="arg"){
    //                 info.code.add(BC_MOV,REG_ARGUMENT,tempReg);
    //                 _GLOG(INST << "\n";)
    //                 whichArg=ARG_TEMP;
    //             }else if(token == "i"){
    //                 if(info.loopScopes.size()==0){
    //                     ERRTOK << "must be in a loop to use '#"<<token<<"'\n";
    //                     // info.nextLine();
    //                     return PARSE_ERROR;
    //                 }else{
    //                     info.code.add(BC_MOV,info.loopScopes.back().iReg,tempReg);
    //                     _GLOG(INST << "\n";)
    //                     whichArg=ARG_TEMP;
    //                 }
    //             }else if(token == "v"){
    //                 if(info.loopScopes.size()==0){
    //                     ERRTOK << "must be in a loop to use '#"<<token<<"'\n";
    //                     // info.nextLine();
    //                     return PARSE_ERROR;
    //                 }else{
    //                     info.code.add(BC_MOV,info.loopScopes.back().vReg,tempReg);
    //                     _GLOG(INST << "\n";)
    //                     whichArg=ARG_TEMP;
    //                 }
    //             }else{
    //                 ERRTOK << "undefined directive '"<<token<<"'\n";
    //                 // info.nextLine();
    //                 return PARSE_ERROR;
    //             }
    //         } else if(token=="("){
    //             // Todo: parse expression
    //             ExpressionInfo expr{};
    //             expr.acc0Reg = tempReg;
    //             // log::out<<"Par ( \n";
    //             info.parDepth++;
    //             int result = ParseExpression(info,expr,false);
    //             info.parDepth--;
    //             if(result==PARSE_SUCCESS){
    //                 if(expr.regCount!=0){
    //                    whichArg=ARG_TEMP|ARG_DEL;
    //                 }
    //             }else{
    //                 return PARSE_ERROR;
    //             }
    //         } else if(token==";"||token==")") {
    //             // argReg = exprInfo.acc0Reg+exprInfo.regCount;
    //             // info.code.add(BC_STR,argReg);
    //             // _GLOG(INST << "arg\n";)
    //             // if(info.spaceConstIndex==-1){
    //             //     Token temp{};
    //             //     char tempStr[]=" ";
    //             //     temp.str = tempStr;
    //             //     temp.length = 1;
    //             //     info.spaceConstIndex = info.code.addConstString(temp);
    //             // }
    //             // int spaceReg = tempReg2+1;
    //             // // Todo: This is flawed. Space is added before when it should be added afterwards.
    //             // if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED)){
    //             //     info.code.add(BC_STR,spaceReg);
    //             //     _GLOG(INST << "\n";)
    //             //     info.code.addLoadC(spaceReg,info.spaceConstIndex);
    //             //     _GLOG(INST << "constant ' '\n";)
    //             //     info.code.add(BC_ADD,argReg,spaceReg,argReg);
    //             //     _GLOG(INST << "extra space\n";)
    //             //     info.code.add(BC_DEL,spaceReg);
    //             //     _GLOG(INST << "\n";)
    //             // }
    //             break;
    //         } else{
    //             // Todo: find variables
    //             if(!(token.flags&TOKEN_QUOTED)&&IsName(token)){
    //                 auto var = info.getVariable(token);
    //                 if(var){
    //                     info.code.add(BC_LOADV,tempReg,var->frameIndex);
    //                     _GLOG(INST << "get variable "<<var->frameIndex<<"\n";)

    //                     // . for property, eg. type, length
    //                     // [] for substring [2] single character, [2:3] an inclusive range of character
                        

                        
    //                     whichArg = ARG_TEMP;
    //                     goto CMD_ACC; // Note: careful when changing behaviour, goto can be wierd
    //                 }
    //                 // not variable? then continue below
    //             }
                
    //             info.revert(); // is revert okay here? will it mess up debug lines?
    //             token = CombineTokens(info);
    //             const char* padding=0;
    //             // if(info.now().flags&TOKEN_SUFFIX_SPACE)
    //             //     padding = " ";
                
    //             // log::out << "tok: "<<combinedTokens<<"\n";
    //             int constIndex = info.code.addConstString(token,padding);
    //             // printf("eh %d\n",constIndex);
    //             info.code.addLoadSC(tempStrReg, constIndex);
    //             _GLOG(INST << "constant '"<<token<<"'\n";)
    //             whichArg = ARG_STR;
    //         }
    //         CMD_ACC:
    //         // Todo: if there is just one arg you can skip some ADD, STR/NUM and DEL instructions
    //         //  and directly use the number or string since there's no need for accumulation/concatenation
    //         if(argReg!=0){
    //             if(info.spaceConstIndex==-1){
    //                 Token temp{};
    //                 char tempStr[]=" ";
    //                 temp.str = tempStr;
    //                 temp.length = 1;
    //                 info.spaceConstIndex = info.code.addConstString(temp);
    //             }
    //             int spaceReg = tempReg+1;
    //             // Todo: This is flawed. Space is added before when it should be added afterwards.
                
    //             addSpace = (prev.flags&TOKEN_SUFFIX_SPACE) || (prev.flags&TOKEN_SUFFIX_LINE_FEED);
    //             if(addSpace){
    //                 info.code.add(BC_STR,spaceReg);
    //                 _GLOG(INST << "\n";)
    //                 info.code.addLoadSC(spaceReg,info.spaceConstIndex);
    //                 _GLOG(INST << "constant ' '\n";)
    //                 info.code.add(BC_ADD,argReg,spaceReg,argReg);
    //                 _GLOG(INST << "extra space\n";)
    //                 info.code.add(BC_DEL,spaceReg);
    //                 _GLOG(INST << "\n";)
    //             }
    //             // if((token.flags&TOKEN_SUFFIX_SPACE) || (token.flags&TOKEN_SUFFIX_LINE_FEED)){
    //             //     info.code.add(BC_STR,spaceReg);
    //             //     _GLOG(INST << "'\n";)
    //             //     info.code.addLoadC(spaceReg,info.spaceConstIndex);
    //             //     _GLOG(INST << "constant ' '\n";)
    //             //     info.code.add(BC_ADD,argReg,spaceReg,argReg);
    //             //     _GLOG(INST << "'\n";)
    //             //     info.code.add(BC_DEL,spaceReg);
    //             //     _GLOG(INST << "'\n";)
    //             // }
    //         }
            
    //         // Todo: handle properties and substring here too. Not only in ParseExpression.
    //         //  It's not just a copy and paste though, tempReg and ARG_TEMP, ARG_DEL needs to
    //         //  be taken into account. If more properties are added then you would need to change
    //         //  it in both places so making a function out of it would be a good idea.
            
    //         if(argReg==0&&whichArg!=0){
    //             argReg = exprInfo.acc0Reg+exprInfo.regCount;
    //             info.code.add(BC_STR,argReg);
    //             _GLOG(INST << "arg\n";)
    //         }
    //         if(whichArg&ARG_STR){
    //             info.code.add(BC_ADD,argReg, tempStrReg, argReg);
    //             _GLOG(INST << "\n";)
    //         }
    //         if(whichArg&ARG_TEMP){
    //             info.code.add(BC_ADD,argReg, tempReg, argReg);
    //             _GLOG(INST << "\n";)
    //         }
    //         if(whichArg&ARG_DEL){
    //             info.code.add(BC_DEL,tempReg);
    //             _GLOG(INST << "\n";)
    //         }
    //         token = info.now();
    //         if(token.flags&TOKEN_SUFFIX_LINE_FEED){
    //             break;
    //         }
    //     }
    //     info.code.add(BC_DEL,tempStrReg);
    //     _GLOG(INST << "temp\n";)
    // }
    
    // if(isAsync){
    //     int treg = REG_RETURN_VALUE;
    //     info.code.add(BC_NUM,treg);
    //     _GLOG(INST << "\n";)
    //     info.code.add(BC_THREAD,argReg,funcReg,treg);
    //     _GLOG(INST << "thread on "<<cmdName<<"\n";)
    // }else{
    //     info.code.add(BC_RUN,argReg,funcReg);
    //     _GLOG(INST << "run "<<cmdName<<"\n";)
    // }

    // info.code.add(BC_DEL,funcReg);
    // _GLOG(INST << "name\n";)
    // exprInfo.regCount--;
    
    // info.code.add(BC_MOV,REG_RETURN_VALUE,exprInfo.acc0Reg+exprInfo.regCount);
    // _GLOG(INST << "\n";)
    // exprInfo.regCount++;

    // // Note: the parsed instructions add a new value in the accumulation
    // //  Don't forget to use and delete it.
    
    _GLOG(EXIT)
    return PARSE_SUCCESS;
}
int ParseBody(ParseInfo& info, ASTBody** bodyLoc, bool multiple=false){
    using namespace engone;
    // Note: two infos in case ParseAssignment modifies it and then fails.
    //  without two, ParseCommand would work with a modified info.
    _GLOG(ENTER)

    if(info.end()){
        log::out << log::YELLOW<<"ParseBody: sudden end?\n";
        return PARSE_ERROR;
    }
    
    *bodyLoc = info.ast->createBody();

    bool scoped=false;
    Token token = info.get(info.at()+1);
    if(Equal(token,"{")) {
        multiple = true;
        scoped=true;
        token = info.next();
    } 

    info.makeScope();
    
    while(!info.end()){
        Token& token = info.get(info.at()+1);
        if(token=="}" && scoped){
            info.next();
            break;
        }
        ASTStatement* temp=0;
        
        int result=0;

        result = ParseAssignment(info,&temp,true);
        // info.ast->relocate(temp)
        
        // result = ParseFlow(info,acc0reg,true);
        // if(result==PARSE_BAD_ATTEMPT)
        // if(result==PARSE_SUCCESS)
        //     continue;
        // if(result==PARSE_BAD_ATTEMPT)
        //     result = ParseCommand(info,expr,true);

        // if(result==PARSE_SUCCESS){
        //     info.code.add(BC_DEL,expr.acc0Reg);
        //     _GLOG(INST << "\n";)
        //     expr.regCount--;
        // } else if(result==PARSE_NO_VALUE){
            
        // } else if(result==PARSE_BAD_ATTEMPT){
        //     if(!info.end()&&info.index>0){
        //         ERRT(info.now()) << "unexpected "<<info.now()<<"\n";
        //     }else {
        //         ERRT(info.get(0)) << "unexpected "<<info.get(0)<<"\n";
        //     }
        //     info.nextLine();
        //     // return PARSE_ERROR;
        // }else {
        //     // log::out << log::RED<<"Fail\n";
        //     info.nextLine();
        //     // Error ? do waht?
        //     // Todo: we may fail because current token is unexpected or we may fail because
        //     //  some other reason deep within the recursive parsing. Somehow we need to know
        //     //  what happened so don't print a message if another function already has.
        // }

        if(!multiple)
            break;
    }
    info.dropScope();
        
    _GLOG(EXIT)
    return PARSE_SUCCESS;
}

AST* ParseTokens(Tokens& tokens, int* outErr){
    using namespace engone;
    _VLOG(log::out <<log::BLUE<<  "##   Parser   ##\n";)
    
    ParseInfo info{tokens};
    
    // Todo: some better way to define this since they have to be defined here and in context.
    //  Better to define in one place.
    // ProvideDefaultCalls(info.externalCalls);
    
    info.ast = AST::Create();

    // info.makeScope();
    int result = ParseBody(info, &info.ast->body,true);

    // while (!info.end()){
    //     // info.addDebugLine(info.index);
    //     // #ifdef USE_DEBUG_INFO
    //     // _GLOG(log::out <<"\n"<<*((Bytecode::DebugLine*)info.code.debugLines.data+info.code.debugLines.used-1)<<"\n";)
    //     // #endif
    //     if(result!=PARSE_SUCCESS){
    //         info.nextLine();
    //         // skip line or token until successful
    //     }
    // }
    info.addDebugLine("(End of global scope)");
    // info.dropScope();
    
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
    
    // info.code.finalizePointers();
    // #ifdef USE_DEBUG_INFO
    // _GLOG(log::out <<*((Bytecode::DebugLine*)info.code.debugLines.data+info.code.debugLines.used-1);)
    // #endif
    return info.ast;
}