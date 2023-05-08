#include "BetBat/Parser.h"

#undef ERR
#undef ERRLINE

#define ERRAT(L,C) info.errors++;engone::log::out <<engone::log::RED<< "CompileError "<<(L)<<":"<<(C)<<", "
#define ERRT(T) ERRAT(T.line,T.column)
#define ERRTL(T) ERRAT(T.line,T.column+T.length)
#define ERRTOK ERRAT(token.line,token.column)
#define ERRTOKL ERRAT(token.line,token.column+token.length)
#define ERR info.errors++;engone::log::out << engone::log::RED

#define ERR_GENERIC ERRTOK << "unexpected "<<token<<" "<<__FUNCTION__<<"\n"

#define INST engone::log::out << (info.code.length()-1)<<": " <<(info.code.get(info.code.length()-1)) << ", "
// #define INST engone::log::out << (info.code.get(info.code.length()-2).type==BC_LOADC ? info.code.get(info.code.length()-2) : info.code.get(info.code.length()-1)) << ", "

#define ERRLINE engone::log::out <<engone::log::RED<<" [Line "<<info.now().line<<"] ";info.printLine();engone::log::out<<"\n";

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

void ParseInfo::makeScope(){
    using namespace engone;
    _PLOG(log::out << log::GRAY<< "   Enter scope "<<scopes.size()<<"\n";)
    scopes.push_back({});
}
// int ParseInfo::Scope::getInstsBeforeDelReg(ParseInfo& info){
//     int offset=0;
//     for(Token& token : variableNames){
//         auto var = info.getVariable(token);
//         if(var){
//             offset+=1; // based on drop scope
//         }
//     }
//     return offset;
// }
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
        _PLOG(log::out << log::GRAY<<"   Exit  scope "<<(scopes.size()-1)<<"\n";)
    }
    // Scope& scope = index==-1? scopes.back() : scopes[index];
    // auto& info = *this;
    // for(Token& token : scope.variableNames){
    //     auto var = getVariable(token);
    //     if(var){
    //         // NOTE: don't forget to change offset in variable cleanup  count
    //         //  offset should be however many instruction you add here!
    //         // deletes previous value
    //         // code.add(BC_STOREV,REG_NULL,var->frameIndex);
    //         // _PLOG(INST << "'"<<token<<"'\n";)
    //         if(index==-1)
    //             removeVariable(token);
    //     }
    // }
    // for(Token& token : scope.functionsNames){
    //     auto pair = functions.find(token);
    //     if(pair!=functions.end()){
    //         if(index==-1)
    //             functions.erase(pair);
    //     }
    // }
    // for(int reg : scope.delRegisters){
    //     // code.add(BC_DEL,reg);
    //     // _PLOG(INST << "\n";)
    // }
    // if(index==-1)
    //     scopes.pop_back();
}

// ParseInfo::Variable* ParseInfo::getVariable(const std::string& name){
//     auto pair = globalVariables.find(name);
//     if(pair!=globalVariables.end())
//         return &pair->second;
    
//     if(currentFunction.empty())
//         return 0;
        
//     auto func = getFunction(currentFunction);
//     auto pair2 = func->variables.find(name);
//     if(pair2!=func->variables.end()) 
//         return &pair2->second;
//     return 0;
// }
// void ParseInfo::removeVariable(const std::string& name){
//     auto pair = globalVariables.find(name);
//     if(pair!=globalVariables.end()){
//         globalVariables.erase(pair);
//     }
//     if(currentFunction.empty())
//         return;
        
//     auto func = getFunction(currentFunction);
//     auto pair2 = func->variables.find(name);
//     if(pair2!=func->variables.end()) 
//         func->variables.erase(pair2);
// }
// ParseInfo::Variable* ParseInfo::addVariable(const std::string& name){
//     if(currentFunction.empty()){
//         return &(globalVariables[name] = {});   
//     }
//     auto func = getFunction(currentFunction);
//     return &(func->variables[name] = {});
// }
// ParseInfo::Function* ParseInfo::getFunction(const std::string& name){
//     auto pair = functions.find(name);
//     if(pair==functions.end())
//         return 0;
//     return &pair->second;
// }
// ParseInfo::Function* ParseInfo::addFunction(const std::string& name){
//     return &(functions[name] = {});
// }
// non-quoted
bool Equal(Token& token, const char* str){
    return !(token.flags&TOKEN_QUOTED) && token == str;
}
// returns instruction type
// zero means no operation
int IsOp(Token& token){
    if(token.flags&TOKEN_QUOTED) return 0;
    if(token=="+") return AST_ADD;
    if(token=="-") return AST_SUB;
    if(token=="*") return AST_MUL;
    if(token=="/") return AST_DIV;
    if(token=="==") return AST_EQUAL;
    if(token=="!=") return AST_NOT_EQUAL;
    if(token=="<") return AST_LESS;
    if(token==">") return AST_GREATER;
    if(token=="<=") return AST_LESS_EQUAL;
    if(token==">=") return AST_GREATER_EQUAL;
    if(token=="&&") return AST_AND;
    if(token=="||") return AST_OR;
    // NOT operation is special

    // if(token=="%") return BC_MOD;
    return 0;
}
int OpPrecedence(int op){
    using namespace engone;
    if(op==AST_AND||op==AST_OR) return 1;
    if(op==AST_LESS||op==AST_GREATER||op==AST_LESS_EQUAL||op==AST_GREATER_EQUAL
        ||op==AST_EQUAL||op==AST_NOT_EQUAL) return 5;
    if(op==AST_ADD||op==AST_SUB) return 9;
    if(op==AST_MUL||op==AST_DIV) return 10;
    log::out << log::RED<<__FILE__<<":"<<__LINE__<<", missing "<<op<<"\n";
    return 0;
}
Token& ParseInfo::next(){
    Token& temp = tokens.get(index++);
    // if(temp.flags&TOKEN_SUFFIX_LINE_FEED||index==1){
        // if(code.debugLines.used==0){
        //     if(addDebugLine(index-1)){
        //         Bytecode::DebugLine* line = (Bytecode::DebugLine*)code.debugLines.data+code.debugLines.used-1;
        //         uint64 offset = (uint64)line->str;
        //         line->str = (char*)code.debugLineText.data + offset;
        //         // line->str doesn't point to debugLineText yet since it may resize.
        //         // we do some special stuff to deal with it.
        //         _PLOG(engone::log::out <<"\n"<<*line<<"\n";)
        //         line->str = (char*)offset;
        //     }
        // }else{
        //     Bytecode::DebugLine* lastLine = (Bytecode::DebugLine*)code.debugLines.data+code.debugLines.used - 1;
        //     if(temp.line != lastLine->line){
        //         if(addDebugLine(index-1)){
        //             Bytecode::DebugLine* line = (Bytecode::DebugLine*)code.debugLines.data+code.debugLines.used-1;
        //             uint64 offset = (uint64)line->str;
        //             line->str = (char*)code.debugLineText.data + offset;
        //             // line->str doesn't point to debugLineText yet since it may resize.
        //             // we do some special stuff to deal with it.
        //             _PLOG(engone::log::out <<"\n"<<*line<<"\n";)
        //             line->str = (char*)offset;
        //         }
        //     }
        // }
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
    // if(code.debugLineText.used+length>code.debugLineText.max){
    //     if(!code.debugLineText.resize(code.debugLineText.max*2+length*30)){
    //         return false;
    //     }
    // }
    // if(code.debugLines.used==code.debugLines.max){
    //     if(!code.debugLines.resize(code.debugLines.max*2+10)){
    //         return false;
    //     }
    // }
    // Bytecode::DebugLine* line = (Bytecode::DebugLine*)code.debugLines.data+code.debugLines.used;
    // code.debugLines.used++;
    // line->str = (char*)code.debugLineText.used;
    // line->length = length;
    // line->instructionIndex = code.length();
    // line->line = get(tokenIndex).line;
    // for(int i=startToken;i<=endToken;i++){
    //     Token& token = get(i);
        
    //     if(token.flags&TOKEN_QUOTED)
    //         *((char*)code.debugLineText.data + code.debugLineText.used++) = '"';
        
    //     // memcpy((char*)code.debugLineText.data + code.debugLineText.used,token.str,token.length);
    //     // code.debugLineText.used+=token.length;
    //     for(int j=0;j<token.length;j++){
    //         // uint64 offset = (uint64)token.str;
    //         char* ptr = token.str;
    //         char chr = *((char*)ptr+j);
    //         if(chr=='\n'){
    //             if(code.debugLineText.used+2>code.debugLineText.max){
    //                 if(!code.debugLineText.resize(code.debugLineText.max+10)){
    //                     return false;
    //                 }
    //             }
    //             *((char*)code.debugLineText.data + code.debugLineText.used++) = '\\';
    //             *((char*)code.debugLineText.data + code.debugLineText.used++) = 'n';
    //             line->length++; // extra for \n
    //         }else{
    //             *((char*)code.debugLineText.data + code.debugLineText.used++) = chr;
    //         }
    //     }
    //     if(token.flags&TOKEN_QUOTED)
    //         *((char*)code.debugLineText.data + code.debugLineText.used++) = '"';
    //     if(token.flags&TOKEN_SUFFIX_SPACE)
    //         *((char*)code.debugLineText.data + code.debugLineText.used++) = ' ';
    // }
    return true;
}
bool ParseInfo::addDebugLine(const char* str, int lineIndex){
    #ifndef USE_DEBUG_INFO
    return false;
    #endif
    int length = strlen(str);
    // if(code.debugLineText.used+length>code.debugLineText.max){
    //     if(!code.debugLineText.resize(code.debugLineText.max*2+length*30)){
    //         return false;
    //     }
    // }
    // if(code.debugLines.used==code.debugLines.max){
    //     if(!code.debugLines.resize(code.debugLines.max*2+10)){
    //         return false;
    //     }
    // }
    // Bytecode::DebugLine* line = (Bytecode::DebugLine*)code.debugLines.data+code.debugLines.used;
    // code.debugLines.used++;
    // line->str = (char*)code.debugLineText.used;
    // line->length = length;
    // line->instructionIndex = code.length();
    // line->line = lineIndex;
        
    // for(int j=0;j<length;j++){
    //     char chr = str[j];
    //     *((char*)code.debugLineText.data + code.debugLineText.used++) = chr;
    // }
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

// int ParseAssignment(ParseInfo& info, ExpressionInfo& exprInfo, bool attempt, bool requestCopy);
// int ParseCommand(ParseInfo& info, ExpressionInfo& exprInfo, bool attempt);

// #fex
int ParseExpression(ParseInfo& info, ASTExpression*& expression, bool attempt){
    using namespace engone;
    
    _PLOG(FUNC_ENTER)
    // _PLOG(TRY)
    
    // if(ParseAssignment(info,exprInfo,true)){
    //     _PLOG(log::out << "-- exit ParseExpression\n";)
    //     return 1;
    // }
    if(info.end()){
        if(attempt)
            return PARSE_BAD_ATTEMPT;
        ERR << "Sudden end\n";
        return PARSE_ERROR;
    }

    std::vector<ASTExpression*> values;
    std::vector<int> ops;
    std::vector<int> directOps; // ! & *

    bool expectOperator=false;
    while(true){
        Token token = info.get(info.at()+1);
        
        int opType = 0;
        bool ending=false;
        
        if(expectOperator){
            if((opType = IsOp(token))){
                token = info.next();

                ops.push_back(opType);

                _PLOG(log::out << "Operator "<<token<<"\n";)
            } else if(Equal(token,";")){
                token = info.next();
                ending = true;
            } else if(Equal(token,")")){
                token = info.next();
                ending = true;
            } else {
                ending = true;
            }
        }else{
            if(Equal(token,"&")){
                info.next();
                directOps.push_back(AST_REFER);
                attempt = false;
                continue;
            }
            if(Equal(token,"*")){
                info.next();
                directOps.push_back(AST_DEREF);
                attempt = false;
                continue;
            }
            if(Equal(token,"!")){
                info.next();
                directOps.push_back(AST_NOT);
                attempt = false;
                continue;
            }

            if(IsInteger(token)){
                token = info.next();
                ASTExpression* tmp = info.ast->createExpression(AST_INT32);
                tmp->i32Value = ConvertInteger(token);
                values.push_back(tmp);
                tmp->token = token;
            } else if(IsDecimal(token)){
                token = info.next();
                ASTExpression* tmp = info.ast->createExpression(AST_FLOAT32);
                tmp->f32Value = ConvertDecimal(token);
                values.push_back(tmp);
                tmp->token = token;
            } else if(IsName(token)){
                token = info.next();
                ASTExpression* tmp = info.ast->createExpression(AST_VAR);
                tmp->varName = new std::string(token);
                values.push_back(tmp);
                tmp->token = token;
            } else if(Equal(token,"(")){
                // parse again
                info.next();
                ASTExpression* tmp=0;
                int result = ParseExpression(info,tmp,false);
                if(result!=PARSE_SUCCESS)
                    return PARSE_ERROR;
                if(!tmp){
                    ERRT(token) << "got nothing from paranthesis in expression\n";
                }
                values.push_back(tmp);
            } else {
                if(attempt){
                    return PARSE_BAD_ATTEMPT;
                }else{
                    ERR_GENERIC;
                    ERRLINE
                    return PARSE_ERROR;
                }
            }
        }

        while(directOps.size()>0){
            int op = directOps.back();
            directOps.pop_back();

            auto val = values.back();
            values.pop_back();
            
            auto newVal = info.ast->createExpression(op);
            newVal->left = val;

            values.push_back(newVal);
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
                    val->left = el;
                    val->right = er;
                    // val->token = operator +?

                    values.push_back(val);
                }else{
                    // _PLOG(log::out << "break\n";)
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
                val->left = el;
                val->right = er;
                // val->token = ...

                values.push_back(val);
            }else{
                break;
            }
        }
        // Attempt succeeded. Failure is now considered an error.
        attempt = false;
        expectOperator=!expectOperator;
        if(ending){
            expression = values.back();
            // _PLOG(EXIT)
            return PARSE_SUCCESS;
        }
    }
}
int ParseBody(ParseInfo& info, ASTBody*& body, bool forceBrackets=false);
// returns 0 if syntax is wrong for flow parsing
int ParseFlow(ParseInfo& info, ASTStatement*& statement, bool attempt){
    using namespace engone;
    _PLOG(FUNC_ENTER)
    
    if(info.end()){
        return PARSE_ERROR;
    }
    Token token = info.get(info.at()+1);

    if(Equal(token,"if")){
        info.next();
        ASTExpression* expr=0;
        int result = ParseExpression(info,expr,false);
        if(result!=PARSE_SUCCESS){
            // TODO: should more stuff be done here?
            return PARSE_ERROR;
        }
        ASTBody* body=0;
        result = ParseBody(info,body);
        if(result!=PARSE_SUCCESS){
            return PARSE_ERROR;
        }
        
        ASTBody* elseBody=0;
        token = info.get(info.at()+1);
        if(Equal(token,"else")){
            info.next();
            result = ParseBody(info,elseBody);
            if(result!=PARSE_SUCCESS){
                return PARSE_ERROR;
            }   
        }

        statement = info.ast->createStatement(ASTStatement::IF);
        statement->expression = expr;
        statement->body = body;
        statement->elseBody = elseBody;
        return PARSE_SUCCESS;
    }else if(Equal(token,"while")){
        info.next();
        ASTExpression* expr=0;
        int result = ParseExpression(info,expr,false);
        if(result!=PARSE_SUCCESS){
            // TODO: should more stuff be done here?
            return PARSE_ERROR;
        }
        ASTBody* body=0;
        result = ParseBody(info,body);
        if(result!=PARSE_SUCCESS){
            return PARSE_ERROR;
        }
        statement = info.ast->createStatement(ASTStatement::WHILE);
        statement->expression = expr;
        statement->body = body;
        return PARSE_SUCCESS;
    }
    if(attempt)
        return PARSE_BAD_ATTEMPT;
    return PARSE_ERROR;
}
int GetDataType(Token& token){
    if(token=="i32") return AST_INT32;
    if(token=="f32") return AST_FLOAT32;
    if(token=="bool") return AST_BOOL8;
    return AST_NONETYPE;
}
// out token contains a newly allocated string. use delete[] on it
int ParseDataType(ParseInfo& info, Token& outDataType){
    using namespace engone;
    int startToken = info.at()+1;
    int endTok = info.at()+1; // exclusive
    int totalLength=0;
    
    while(true){
        Token& tok = info.get(endTok);
        if(tok.flags&TOKEN_QUOTED){
            ERRT(tok) << "Cannot have quotes in data type "<<tok<<"\n";
            ERRLINE
            // error = PARSE_ERROR;
            // NOTE: Don't return in order to stay synchronized.
            //   continue parsing like nothing happen.
        }
        // if(Equal(tok,"=")){
        // Todo: Array<T,A> wouldn't work since comma is detected as end of data type.
        //      tuples won't work with paranthesis.
        if(Equal(tok,",")
            ||Equal(tok,")")
        ){
            break;
        }
        endTok++;
        totalLength+=tok.length;
        info.next();
        if(tok.flags&TOKEN_SUFFIX_LINE_FEED)
            break;
    }
    outDataType = {};
    outDataType.str = new char[totalLength];
    outDataType.length = totalLength;

    int sofar=0;
    for (int i=startToken;i!=endTok;i++){
        Token& tok = info.get(i);
        memcpy(outDataType.str+sofar,tok.str,tok.length);
        sofar+=tok.length;
    }

    // defer {
    //     delete[] dataType.str;
    // };
    return PARSE_SUCCESS;
}
int ParseFunction(ParseInfo& info, ASTFunction*& function, bool attempt){
    using namespace engone;
    _PLOG(FUNC_ENTER)
    Token& token = info.get(info.at()+1);
    if(!Equal(token,"fn")){
        if(attempt) return PARSE_BAD_ATTEMPT;
        ERRT(token) << "expected fn for function not "<<token<<"\n";
        return PARSE_ERROR;
    }
    info.next();
    attempt = false;
    
    Token& name = info.next();
    if(!IsName(name)){
        ERRT(token) << "expected a valid name, "<<token<<" isn't\n";
        return PARSE_ERROR;
    }
    
    Token tok = info.next();
    if(!Equal(tok,"(")){
        ERRT(token) << "expected a (, not "<<token<<"\n";
        return PARSE_ERROR;
    }

    function = info.ast->createFunction(name);
    
    while(true){
        Token& arg = info.next();
        if(!IsName(arg)){
            return PARSE_ERROR;
        }
        tok = info.next();
        if(!Equal(tok,":")){
            return PARSE_ERROR;
        }

        Token dataType;
        int result = ParseDataType(info,dataType);
        defer {
            delete[] dataType.str;
        };
        auto id = info.ast->getOrAddDataType(dataType);
        function->arguments.push_back({(std::string)arg,id});

        tok = info.next();
        if(Equal(tok,",")){
            continue;
        }else if(Equal(tok,")")){
            break;
        }else{
            return PARSE_ERROR;
        }
    }
    // TODO: check token out of bounds
    tok = info.get(info.at()+1);
    Token tok2 = info.get(info.at()+2);
    if(Equal(tok,"-") && !(tok.flags&TOKEN_SUFFIX_SPACE) && Equal(tok2,">")){
        // TODO: return types
    }

    ASTBody* body;
    int result = ParseBody(info,body);

    function->body = body;

    return PARSE_SUCCESS;
}
// #fass
int ParseAssignment(ParseInfo& info, ASTStatement*& statement, bool attempt){
    using namespace engone;
    _PLOG(FUNC_ENTER)
    // _PLOG(TRY)
    
    if(info.tokens.length() < info.index+2){
        // not enough tokens for assignment
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }
        _PLOG(log::out <<log::RED<< "CompilerError: to few tokens for assignment\n";)
        // info.nextLine();
        return PARSE_ERROR;
    }
    int error=PARSE_SUCCESS;

    Token name = info.get(info.at()+1);
    Token assign = info.get(info.at()+2);
    
    int assignType = 0;
    if(Equal(assign,":")){
        // we expect a data type which can be comprised of multiple tokens.
        
        /*
        {name} : {data type}\n   <-  line feed marks end of data type
        {name} :     <-  line feed is okay here
             {data type}\n

        nums : i32[      <-  data type is cut off.
            ]
        */

        info.next();
        info.next();
        attempt=false;

        // i32 *
        // i32 []*
        // Array<i32>
        // (i32, f32)
        // TODO: quoted tokens should not be allowed, var : "i32"*  <-  BAD
        // TODO: is var := ...  allowed. : without data type? I guess it's fine?
        // TODO: what about sudden end?
        int startToken = info.at()+1;
        int endTok = info.at()+1; // exclusive
        int totalLength=0;
        
        while(true){
            Token& tok = info.get(endTok);
            if(tok.flags&TOKEN_QUOTED){
                ERRT(tok) << "Cannot have quotes in data type "<<tok<<"\n";
                ERRLINE
                error = PARSE_ERROR;
                // NOTE: Don't return in order to stay synchronized.
                //   continue parsing like nothing happen.
            }
            if(Equal(tok,"=")){
                break;
            }
            endTok++;
            totalLength+=tok.length;
            info.next();
            if(tok.flags&TOKEN_SUFFIX_LINE_FEED)
                break;
        }
        Token dataType{};
        dataType.str = new char[totalLength];
        dataType.length = totalLength;
        defer {
            delete[] dataType.str;
        };
        
        // std::function<void()> shit{};
        // DeferStruct de(shit);
        // shit = [&](){};
        // defer {};

        int sofar=0;
        for (int i=startToken;i!=endTok;i++){
            Token& tok = info.get(i);
            memcpy(dataType.str+sofar,tok.str,tok.length);
            sofar+=tok.length;
        }

        DataType dtId = info.ast->getDataType(dataType);
        if(dtId==AST_NONETYPE){
            // create data type, whether it's defined doesn't matter.
            // we handle that later
            dtId = info.ast->addDataType(dataType);
        }
        assignType = dtId;
        assign = info.get(info.at()+1);

        if (error==PARSE_SUCCESS)
            log::out << "Created datatype "<<dataType<<" "<<dtId<<"\n";
    }
    
    if(!Equal(assign,"=")){
        if(attempt)
            return PARSE_BAD_ATTEMPT;
        ERRT(assign) << "Expected =\n";
        ERRLINE;
        return PARSE_ERROR;
    }
    if(!IsName(name)){
        info.next(); // prevent loop
        ERRT(name) << "expected a valid name for assignment ('"<<name<<"' isn't)\n";
        return PARSE_ERROR;
    }
    if(assignType==0)
        info.next(); // next on name if : part didn't do it

    info.next();

    statement = info.ast->createStatement(ASTStatement::ASSIGN);

    // log::out << reloc<<"\n";
    statement->name = new std::string(name);
    if(assignType!=0)
        statement->dataType = assignType;

    ASTExpression* expression=0;
    int result = 0;
    result = ParseExpression(info,expression,false);

    if(result!=PARSE_SUCCESS){
        return PARSE_ERROR;
    }

    statement->expression = expression;

    // _PLOG(EXIT)
    return error;   
}
int ParseBody(ParseInfo& info, ASTBody*& bodyLoc, bool forceBrackets){
    using namespace engone;
    // Note: two infos in case ParseAssignment modifies it and then fails.
    //  without two, ParseCommand would work with a modified info.
    _PLOG(FUNC_ENTER)
    // _PLOG(ENTER)

    if(info.end()){
        ERR << "Sudden end\n";
        return PARSE_ERROR;
    }
    
    bodyLoc = info.ast->createBody();

    bool scoped=false;
    Token token = info.get(info.at()+1);
    if(Equal(token,"{")) {
        forceBrackets = true;
        scoped=true;
        token = info.next();
    } 

    info.makeScope();

    ASTStatement* prev=0;
    ASTFunction* prevf=0;
    
    while(!info.end()){
        Token& token = info.get(info.at()+1);
        if(token=="}" && scoped){
            info.next();
            break;
        }
        ASTStatement* temp=0;
        ASTFunction* func=0;
        
        int result=PARSE_BAD_ATTEMPT;

        if(result==PARSE_BAD_ATTEMPT)
            result = ParseFunction(info,func,true);
        if(result==PARSE_BAD_ATTEMPT)
            result = ParseAssignment(info,temp,true);
        if(result==PARSE_BAD_ATTEMPT)
            result = ParseFlow(info,temp,true);

        if(result==PARSE_BAD_ATTEMPT){
            Token& token = info.get(info.at()+1);
            ERRT(token) << "Unexpected '"<<token<<"'\n";
            // test other parse type
            info.next(); // prevent infinite loop
        }
        if(result==PARSE_ERROR){
            
        }
        if(result==PARSE_SUCCESS){
            if(temp){
                if(prev){
                    prev->next = temp;
                    prev = temp;
                }else{
                    bodyLoc->statement = temp;
                    prev = temp;
                }
            }
            if(func){
                if(prevf){
                    prevf->next = func;
                    prevf = func;
                }else{
                    bodyLoc->function = func;
                    prevf = func;
                }
            }
        }
        
        if(!forceBrackets)
            break;
    }
    info.dropScope();
        
    // _PLOG(EXIT)
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
    int result = ParseBody(info, info.ast->body,true);

    info.ast->print();
    // info.dropScope();

    if(outErr){
        *outErr = info.errors;
    }
    if(info.errors)
        log::out << log::RED<<"Parser failed with "<<info.errors<<" errors\n";
    
    // info.code.finalizePointers();
    // #ifdef USE_DEBUG_INFO
    // _PLOG(log::out <<*((Bytecode::DebugLine*)info.code.debugLines.data+info.code.debugLines.used-1);)
    // #endif
    return info.ast;
}