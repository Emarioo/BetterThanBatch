#include "BetBat/Parser.h"

#undef ERR
#undef ERRLINE
#undef ERRAT

#define ERRAT(L,C) info.errors++;engone::log::out <<engone::log::RED<< "ParseError "<<(L)<<":"<<(C)<<", "
#define ERRT(T) ERRAT(T.line,T.column)
#define ERRTL(T) ERRAT(T.line,T.column+T.length)
#define ERRTOK ERRAT(token.line,token.column)
#define ERRTOKL ERRAT(token.line,token.column+token.length)
#define ERR info.errors++;engone::log::out << engone::log::RED

#define ERR_GENERIC ERRTOK << "unexpected "<<token<<" "<<__FUNCTION__<<"\n"

#define INST engone::log::out << (info.code.length()-1)<<": " <<(info.code.get(info.code.length()-1)) << ", "
// #define INST engone::log::out << (info.code.get(info.code.length()-2).type==BC_LOADC ? info.code.get(info.code.length()-2) : info.code.get(info.code.length()-1)) << ", "

#define ERRLINE engone::log::out <<engone::log::RED<<" [Line "<<info.now().line<<"] ";info.printLine();engone::log::out<<"\n";
#define ERRLINEP engone::log::out <<engone::log::RED<<" [previous line] ";info.printPrevLine();engone::log::out<<"\n";

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
void ParseInfo::printPrevLine(){
    using namespace engone;
    // TODO: bound check
    int nowLine = get(at()).line;
    
    int endIndex = 0;
    int startIndex = at()-1;
    bool setEnd=false;
    while(startIndex>=0){
        Token tok = get(startIndex);
        if(tok.flags&TOKEN_SUFFIX_LINE_FEED){
            if(!setEnd){
                endIndex = startIndex;
                setEnd=true;
            }else {
                break;
            }
        }
        startIndex--;
    }
    if(startIndex==-1)
        startIndex = 0;
    // int endToken = startToken;
    // while(true){
    //     if((uint)endToken>=tokens.length()){
    //         endToken = tokens.length()-1;
    //         break;
    //     }
    //     Token token = get(endToken);
    //     if(token.flags&TOKEN_SUFFIX_LINE_FEED){
    //         break;
    //     }
    //     endToken++;
    // }
    for(int i=startIndex;i<=endIndex;i++){
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
int ParseTypeId(ParseInfo& info, Token& outTypeId){
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
        if(Equal(tok,",") // (a: i32, b: i32)     struct {a: i32, b: i32}
            ||Equal(tok,")") // (a: i32)
            ||Equal(tok,"{") // () -> i32 {
            ||Equal(tok,"}") // struct { x: f32 }
            ||Equal(tok,">") // cast<i32>
        ){
            break;
        }
        endTok++;
        totalLength+=tok.length;
        info.next();
        if(tok.flags&TOKEN_SUFFIX_LINE_FEED)
            break;
    }
    outTypeId = {};
    outTypeId.str = new char[totalLength];
    outTypeId.length = totalLength;

    int sofar=0;
    for (int i=startToken;i!=endTok;i++){
        Token& tok = info.get(i);
        memcpy(outTypeId.str+sofar,tok.str,tok.length);
        sofar+=tok.length;
    }
    // defer {
    //     delete[] dataType.str;
    // };
    return PARSE_SUCCESS;
}
int ParseStruct(ParseInfo& info, ASTStruct*& astStruct,  bool attempt){
    using namespace engone;
    _PLOG(FUNC_ENTER)
    Token structToken = info.get(info.at()+1);
    int startIndex = info.at()+1;
    if(!Equal(structToken,"struct")){
        if(attempt) return PARSE_BAD_ATTEMPT;
        ERRT(structToken)<<"expected struct not "<<structToken<<"\n";
        ERRLINE
        return PARSE_ERROR;   
    }
    
    Token name = info.get(info.at()+2);
    if(!IsName(name)){
        if(attempt) return PARSE_BAD_ATTEMPT;
        ERRT(name)<<"expected a name, "<<name<<" isn't\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next(); // struct
    info.next(); // name
    attempt=false;
    
    Token token = info.get(info.at()+1);
    if(!Equal(token,"{")){
        ERRT(token)<<"expected { not "<<token<<"\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next();
    
    astStruct = info.ast->createStruct(name);
    astStruct->tokenRange.firstToken = structToken;
    astStruct->tokenRange.startIndex = startIndex;
    int offset=0;
    int error = PARSE_SUCCESS;
    bool affectedByAlignment=false;
    // TODO: May want to change the structure of the while loop
    //  struct { a 9 }   <-  : is expected and not 9, function will quit but that's not good because we still have }.
    //  that will cause all sorts of issue outside. leaving this parse function when } is reached even if errors occured
    //  inside is probably better.
    while (true){
        Token name = info.get(info.at()+1);
        
        if(Equal(name,"}")){
            info.next();
            break;
        }
        
        if(!IsName(name)){
            info.next();
            ERRT(name) << "expected a name, "<<name<<" isn't\n";
            ERRLINE
            error = PARSE_ERROR;
            continue;
        }
        info.next();   
        Token token = info.get(info.at()+1);
        if(!Equal(token,":")){
            ERRT(token)<<"expected : not "<<token<<"\n";
            ERRLINE
            error = PARSE_ERROR;
            continue;
        }    
        info.next();
        Token dtToken;
        int result = ParseTypeId(info,dtToken);
        if(result!=PARSE_SUCCESS) {
            ERRT(dtToken) << "failed parsing type "<<dtToken<<"\n";
            ERRLINE;   
            continue;
        }
        
        TypeInfo* typeInfo = info.ast->getTypeInfo(dtToken);
        if(!typeInfo->size){
            ERRT(dtToken) << "Member "<<name<<" in struct has 0 as size\n";
            info.errors--; // it's okay
            ERRLINE
        }
        // TODO: default value
        
        // TODO: what about types which don't have a size yet?
        
        // TODO: auto reallignment to optimize the member layout according to alignment?`
        //   Would this be confusing when debugging?
        // int size = typeInfo->size;
        int size = 8;
        int misalign = (offset%size);
        if(misalign!=0){
            // type isn't alligned
            offset += size - misalign;
            affectedByAlignment=true;
        }
        
        astStruct->members.push_back({(std::string)name,typeInfo->id,offset});
        offset += size;
        
        token = info.get(info.at()+1);
        if(Equal(token,",")){
            info.next();
            continue;
        }else if(Equal(token,"}")){
            info.next();
            break;
        }else{
            ERRT(token)<<"expected } or , not "<<token<<"\n";
            ERRLINE
            error = PARSE_ERROR;
            continue;
        }
    }
    astStruct->tokenRange.endIndex = info.at()+1;
    astStruct->size = offset;
    auto typeInfo = info.ast->getTypeInfo(name);
    typeInfo->astStruct = astStruct;
    typeInfo->size = astStruct->size;
    _GLOG(log::out << "Parsed struct "<<name << " (size: "<<offset<< (affectedByAlignment?", align padding":"")<<") with "<<astStruct->members.size()<<" members\n";)
    return error;
}
int ParseEnum(ParseInfo& info, ASTEnum*& astEnum, bool attempt){
    using namespace engone;
    _PLOG(FUNC_ENTER)
    Token enumToken = info.get(info.at()+1);
    int startIndex = info.at()+1;
    if(!Equal(enumToken,"enum")){
        if(attempt) return PARSE_BAD_ATTEMPT;
        ERRT(enumToken)<<"expected struct not "<<enumToken<<"\n";
        ERRLINE
        return PARSE_ERROR;   
    }
    Token name = info.get(info.at()+2);
    if(!IsName(name)){
        if(attempt) return PARSE_BAD_ATTEMPT;
        ERRT(name)<<"expected a name, "<<name<<" isn't\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next(); // struct
    info.next(); // name
    attempt=false;
    
    Token token = info.get(info.at()+1);
    if(!Equal(token,"{")){
        ERRT(token)<<"expected { not "<<token<<"\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next();
    int nextId=0;
    astEnum = info.ast->createEnum(name);
    astEnum->tokenRange.firstToken = enumToken;
    astEnum->tokenRange.startIndex = startIndex;
    int error = PARSE_SUCCESS;
    while (true){
        Token name = info.get(info.at()+1);
        
        if(Equal(name,"}")){
            info.next();
            break;
        }
        
        if(!IsName(name)){
            info.next(); // ?
            ERRT(name) << "expected a name, "<<name<<" isn't\n";
            ERRLINE
            error = PARSE_ERROR;
            continue;
        }
        info.next();   
        Token token = info.get(info.at()+1);
        if(Equal(token,"=")){
            info.next();
            token = info.get(info.at()+1);
            if(IsInteger(token)){
                info.next();
                nextId = ConvertInteger(token);
                token = info.get(info.at()+1);
            }else{
                ERRT(token) << token<<" is not a number\n";
                ERRLINE
            }
        }
        astEnum->members.push_back({(std::string)name,nextId++});
        
        // token = info.get(info.at()+1);
        if(Equal(token,",")){
            info.next();
            continue;
        }else if(Equal(token,"}")){
            info.next();
            break;
        }else{
            ERRT(token)<<"expected } or , not "<<token<<"\n";
            ERRLINE
            error = PARSE_ERROR;
            continue;
        }
    }
    astEnum->tokenRange.endIndex = info.at()+1;
    auto typeInfo = info.ast->getTypeInfo(name);
    typeInfo->astEnum = astEnum;
    typeInfo->size = 4; // i32
    _GLOG(log::out << "Parsed enum "<<name << " with "<<astEnum->members.size()<<" members\n";)
    return error;
}
// #fex
int ParseExpression(ParseInfo& info, ASTExpression*& expression, bool attempt){
    using namespace engone;
    
    _PLOG(FUNC_ENTER)
    
    // if(ParseAssignment(info,,true)){
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
    // extra info for AST_CAST
    std::vector<TypeId> castTypes;

    bool expectOperator=false;
    while(true){
        Token token = info.get(info.at()+1);
        
        int opType = 0;
        bool ending=false;
        
        if(expectOperator){
            if(Equal(token,".")){
                info.next();
                
                token = info.get(info.at()+1);
                if(!IsName(token)){
                    ERRT(token) << "expected a property name, not "<<token<<"\n";
                    continue;
                }
                info.next();
                
                ASTExpression* tmp = info.ast->createExpression(AST_PROP);
                tmp->name = new std::string(token);
                tmp->left = values.back();
                tmp->tokenRange.firstToken = tmp->left->tokenRange.firstToken;
                tmp->tokenRange.startIndex = tmp->left->tokenRange.startIndex;
                tmp->tokenRange.endIndex = info.at()+1;
                values.pop_back();
                values.push_back(tmp);
                continue;
            } else if((opType = IsOp(token))){
                info.next();

                ops.push_back(opType);

                _PLOG(log::out << "Operator "<<token<<"\n";)
            } else if(Equal(token,";")){
                info.next();
                ending = true;
            } else if(Equal(token,")")){
                // token = info.next();
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
            if(Equal(token,"cast")){
                info.next();
                Token tok = info.get(info.at()+1);
                if(!Equal(tok,"<")){
                    ERRT(tok) << "expected < not "<<tok<<"\n";
                    ERRLINE
                    continue;
                }
                info.next();
                Token tokenTypeId{};
                int result = ParseTypeId(info,tokenTypeId);
                if(result!=PARSE_SUCCESS){
                    ERRT(tokenTypeId) << tokenTypeId << "is not a valid data type\n";
                    ERRLINE
                }
                tok = info.get(info.at()+1);
                if(!Equal(tok,">")){
                    ERRT(tok) << "expected > not "<< tok<<"\n";
                    ERRLINE
                }
                info.next();
                directOps.push_back(AST_CAST);
                TypeId dt = info.ast->getTypeId(tokenTypeId);
                castTypes.push_back(dt);
                attempt=false;
                continue;
            }

            if(IsInteger(token)){
                token = info.next();
                
                ASTExpression* tmp = 0;
                // TODO: handle to large numbers
                // TODO: don't always use tmp->i64Value
                if(token.str[0]=='-'){
                    //-- signed
                    token.str++;
                    token.length--;
                    
                    u64 num=0;
                    for(int i=0;i<token.length;i++){
                        char c = token.str[i];
                        num = num*10 + (c-'0');
                    }
                    // if (num<=pow(2,7))
                    //     tmp = info.ast->createExpression(AST_INT8);
                    // else if (num<=pow(2,15))
                    //     tmp = info.ast->createExpression(AST_INT16);
                    // else if (num<=pow(2,31))
                    //     tmp = info.ast->createExpression(AST_INT32);
                    // else
                    //     tmp = info.ast->createExpression(AST_INT64);
                        
                    tmp = info.ast->createExpression(AST_INT32); // default to this for now
                    tmp->i64Value = -num;
                }else{
                    //-- unsigned   
                    u64 num=0;
                    for(int i=0;i<token.length;i++){
                        char c = token.str[i];
                        num = num*10 + (c-'0');
                    }
                    // if (num<=pow(2,7)-1)
                    //     tmp = info.ast->createExpression(AST_UINT8);
                    // else if (num<=pow(2,15)-1)
                    //     tmp = info.ast->createExpression(AST_UINT16);
                    // else if (num<=pow(2,31)-1)
                    //     tmp = info.ast->createExpression(AST_UINT32);
                    // else
                    //     tmp = info.ast->createExpression(AST_UINT64);
                    tmp = info.ast->createExpression(AST_INT32);
                    tmp->i64Value = num;
                }
                
                values.push_back(tmp);
                tmp->tokenRange.firstToken = token;
                tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
            } else if(IsDecimal(token)){
                token = info.next();
                ASTExpression* tmp = info.ast->createExpression(AST_FLOAT32);
                tmp->f32Value = ConvertDecimal(token);
                values.push_back(tmp);
                tmp->tokenRange.firstToken = token;
                tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
            } else if(Equal(token,"null")){
                token = info.next();
                ASTExpression* tmp = info.ast->createExpression(AST_NULL);
                values.push_back(tmp);
                tmp->tokenRange.firstToken = token;
                tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
            } else if(IsName(token)){
                info.next();
                int startToken=info.at();
                
                Token& tok = info.get(info.at()+1);
                if(Equal(tok,"(")){
                    // function call
                    info.next();
                    ASTExpression* tmp = info.ast->createExpression(AST_FNCALL);
                    tmp->name = new std::string(token);
                    ASTExpression* prev=0;
                    // TODO: sudden end, error handling
                    bool expectComma=false;
                    int count=0;
                    while(true){
                        Token& tok = info.get(info.at()+1);
                        if(Equal(tok,")")){
                            info.next();
                            break;
                        }
                        if(expectComma){
                            if(Equal(tok,",")){
                                info.next();
                                expectComma=false;
                                continue;
                            }
                            ERRT(tok)<<"expected comma not "<<tok<<"\n";
                            return PARSE_ERROR;
                        }
                        ASTExpression* expr=0;
                        int result = ParseExpression(info,expr,false);
                        if(result!=PARSE_SUCCESS){
                            // TODO: error message, parse other arguments instead of returning?
                            return PARSE_ERROR;                         
                        }
                        if(prev){
                            prev->next = expr;
                        }else{
                            tmp->left = expr;
                        }
                        count++;
                        prev = expr;
                        expectComma = true;
                    }
                    log::out << "Parsed call "<<count <<"\n";
                    values.push_back(tmp);
                    tmp->tokenRange.firstToken = token;
                    tmp->tokenRange.startIndex = startToken;
                    tmp->tokenRange.endIndex = info.at()+1;
                }else if(Equal(tok,"{")){
                    // initializer
                    info.next();
                    ASTExpression* tmp = info.ast->createExpression(AST_INITIALIZER);
                    tmp->name = new std::string(token);
                    ASTExpression* prev=0;
                    int count=0;
                    while(true){
                        Token token = info.get(info.at()+1);
                        if(Equal(token,"}")){
                           info.next();
                           break;
                        }
                        ASTExpression* expr=0;
                        int result = ParseExpression(info,expr,false);
                        if(result!=PARSE_SUCCESS){
                            // TODO: error message, parse other arguments instead of returning?
                            return PARSE_ERROR;                         
                        }
                        if(prev){
                            prev->next = expr;
                        }else{
                            tmp->left = expr;
                        }
                        count++;
                        prev = expr;
                        
                        token = info.get(info.at()+1);
                        if(Equal(token,",")){
                            info.next();
                            continue;
                        }else if(Equal(token,"}")){
                           info.next();
                           break;
                        } else {
                            ERRT(token) << "expected , or } not "<<token<<"\n";
                            ERRLINE
                            continue;   
                        }
                    }
                    log::out << "Parse initializer "<<count<<"\n";
                    values.push_back(tmp);
                    tmp->tokenRange.firstToken = token;
                    tmp->tokenRange.startIndex = startToken;
                    tmp->tokenRange.endIndex = info.at()+1;
                }else if(Equal(tok,"::")){
                    info.next();
                    
                    Token tok = info.get(info.at()+1);
                    if(!IsName(tok)){
                        ERRT(tok) << tok<<" is not a name\n";
                        ERRLINE
                        continue;
                    }
                    info.next();
                    
                    // TODO: detect more ::
                    
                    ASTExpression* tmp = info.ast->createExpression(AST_FROM_NAMESPACE);
                    tmp->name = new std::string(token);
                    tmp->member = new std::string(tok);
                    values.push_back(tmp);
                    tmp->tokenRange.firstToken = token;
                    tmp->tokenRange.startIndex = startToken;
                    tmp->tokenRange.endIndex = info.at()+1;
                } else{
                    ASTExpression* tmp = info.ast->createExpression(AST_VAR);
                    tmp->name = new std::string(token);
                    values.push_back(tmp);
                    tmp->tokenRange.firstToken = token;
                    tmp->tokenRange.startIndex = startToken;
                    tmp->tokenRange.endIndex = info.at()+1;
                }
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
                token = info.get(info.at()+1);
                if(!Equal(token,")")){
                    ERRT(token) << "expected ) not "<<token<<"\n";   
                }else
                    info.next();
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
            
            if(op==AST_CAST){
                newVal->castType = castTypes.back();
                castTypes.pop_back();
            }
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
                    
                    val->tokenRange.firstToken = er->tokenRange.firstToken;
                    val->tokenRange.startIndex = er->tokenRange.startIndex;
                    val->tokenRange.endIndex = el->tokenRange.endIndex;

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
                
                val->tokenRange.firstToken = er->tokenRange.firstToken;
                val->tokenRange.startIndex = er->tokenRange.startIndex;
                val->tokenRange.endIndex = el->tokenRange.endIndex;

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
    Token firstToken = info.get(info.at()+1);
    int startIndex = info.at()+1;
    if(Equal(firstToken,"if")){
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
        Token token = info.get(info.at()+1);
        if(Equal(token,"else")){
            info.next();
            result = ParseBody(info,elseBody);
            if(result!=PARSE_SUCCESS){
                return PARSE_ERROR;
            }   
        }

        statement = info.ast->createStatement(ASTStatement::IF);
        statement->rvalue = expr;
        statement->body = body;
        statement->elseBody = elseBody;
        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        return PARSE_SUCCESS;
    }else if(Equal(firstToken,"while")){
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
        statement->rvalue = expr;
        statement->body = body;
        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        return PARSE_SUCCESS;
    } else if(Equal(firstToken,"return")){
        info.next();
        
        ASTExpression* base = 0;        
        ASTExpression* prev = 0;
        while(true){
            ASTExpression* expr=0;
            Token token = info.get(info.at()+1);
            if(Equal(token,";")&&!base){ // return 1,; should not be allowed, that's why we do &&!base
                info.next();
                break;
            }
            int result = ParseExpression(info,expr,true);
            if(result!=PARSE_SUCCESS){
                // TODO: should more stuff be done here?
                return PARSE_ERROR;
            }
            if(result==PARSE_BAD_ATTEMPT){
                break;
            }
            if(!base){
                base = expr;
                prev = expr;
            }else{
                prev->next = expr;
                prev = expr;
            }
            
            Token tok = info.get(info.at()+1);
            if(!Equal(tok,",")){
                if(Equal(tok,";"))
                    info.next();
                break;   
            }
            info.next();
        }
        
        statement = info.ast->createStatement(ASTStatement::RETURN);
        statement->rvalue = base;
        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        return PARSE_SUCCESS;
    }
    if(attempt)
        return PARSE_BAD_ATTEMPT;
    return PARSE_ERROR;
}
// out token contains a newly allocated string. use delete[] on it
int ParseFunction(ParseInfo& info, ASTFunction*& function, bool attempt){
    using namespace engone;
    _PLOG(FUNC_ENTER)
    Token& token = info.get(info.at()+1);
    int startIndex = info.at()+1;
    if(!Equal(token,"fn")){
        if(attempt) return PARSE_BAD_ATTEMPT;
        ERRT(token) << "expected fn for function not "<<token<<"\n";
        return PARSE_ERROR;
    }
    info.next();
    attempt = false;
    
    Token& name = info.next();
    if(!IsName(name)){
        ERRT(name) << "expected a valid name, "<<name<<" isn't\n";
        return PARSE_ERROR;
    }
    
    Token tok = info.next();
    if(!Equal(tok,"(")){
        ERRT(tok) << "expected a (, not "<<tok<<"\n";
        return PARSE_ERROR;
    }

    function = info.ast->createFunction(name);
    function->tokenRange.firstToken = token;
    function->tokenRange.startIndex = startIndex;
    
    while(true){
        Token& arg = info.next();
        if(Equal(arg,")")){
            break;
        }
        // Todo: what if function has no arguments, ) would appear here
        if(!IsName(arg)){
            ERRT(arg) << arg <<" is not a valid argument name\n";
            ERRLINE
            continue;
            // return PARSE_ERROR;
        }
        tok = info.get(info.at()+1);
        if(!Equal(tok,":")){
            ERRT(tok) << "expected : not "<<tok <<"\n";
            ERRLINE
            continue;
            // return PARSE_ERROR;
        }
        info.next();

        Token dataType{};
        int result = ParseTypeId(info,dataType);
        defer {
            delete[] dataType.str;
        };
        auto id = info.ast->getTypeId(dataType);
        function->arguments.push_back({(std::string)arg,id});

        tok = info.next();
        if(Equal(tok,",")){
            continue;
        }else if(Equal(tok,")")){
            break;
        }else{
            ERRT(tok) << "expected , or ) not "<<tok <<"\n";
            ERRLINE
            return PARSE_ERROR;
        }
    }
    // TODO: check token out of bounds
    tok = info.get(info.at()+1);
    Token tok2 = info.get(info.at()+2);
    if(Equal(tok,"-") && !(tok.flags&TOKEN_SUFFIX_SPACE) && Equal(tok2,">")){
        // TODO: return types
        info.next();
        info.next();
        tok = info.get(info.at()+1);
        // TODO: handle (,,,)
        if(!Equal(tok,"(")){
            // multiple args
            while(true){
                Token& tok = info.get(info.at()+1);
                if(Equal(tok,"{")){
                    break;   
                }
                if(Equal(tok,",")){
                    info.next();
                    continue;   
                }
                
                Token dt{};
                int result = ParseTypeId(info,dt);
                if(result!=PARSE_SUCCESS){
                    return PARSE_ERROR;
                }
                auto id = info.ast->getTypeId(dt);
                function->returnTypes.push_back(id);
            }
        }else{
            info.next(); // skip (
            // multiple args
            while(true){
                Token& tok = info.get(info.at()+1);
                if(Equal(tok,")")){
                    info.next();
                    break;   
                }
                if(Equal(tok,",")){
                    info.next();
                    continue;   
                }
                
                Token dt{};
                int result = ParseTypeId(info,dt);
                if(result!=PARSE_SUCCESS){
                    return PARSE_ERROR;
                }
                auto id = info.ast->getTypeId(dt);
                function->returnTypes.push_back(id);
            }
        }
    }

    ASTBody* body;
    int result = ParseBody(info,body);

    function->body = body;
    function->tokenRange.endIndex = info.at()+1;

    return PARSE_SUCCESS;
}
int ParsePropAssignment(ParseInfo& info, ASTStatement*& statement, bool attempt){
    using namespace engone;
    _PLOG(FUNC_ENTER)
    
    int startIndex = info.at()+1;
    
    ASTExpression* lvalue;
    int result = ParseExpression(info,lvalue,attempt);       
    if(result!=PARSE_SUCCESS) return result;
    
    attempt = false;
    
    Token tok = info.get(info.at()+1);
    if(!Equal(tok,"=")){
        statement = info.ast->createStatement(ASTStatement::CALL);
        statement->rvalue = lvalue;   
        statement->tokenRange.firstToken = info.get(startIndex);
        statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        return PARSE_SUCCESS;
    }
    info.next();
    
    ASTExpression* rvalue;
    result = ParseExpression(info,rvalue,attempt);
    if(result!=PARSE_SUCCESS) return result;
    
    statement = info.ast->createStatement(ASTStatement::PROP_ASSIGN);
    statement->lvalue = lvalue;
    statement->rvalue= rvalue;
    
    statement->tokenRange.firstToken = info.get(startIndex);
    statement->tokenRange.startIndex = startIndex;
    statement->tokenRange.endIndex = info.at()+1;
    
    return PARSE_SUCCESS;
}
// normal assignment a = 9
int ParseAssignment(ParseInfo& info, ASTStatement*& statement, bool attempt){
    using namespace engone;
    _PLOG(FUNC_ENTER)
    
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

    int startIndex = info.at()+1;
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
            if(Equal(tok,"=")||Equal(tok,";")){
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

        TypeId dtId = info.ast->getTypeId(dataType);
        assignType = dtId;
        assign = info.get(info.at()+1);

        // if (error==PARSE_SUCCESS)
        //     log::out << "Created datatype "<<dataType<<" "<<dtId<<"\n";
    }
    
    if(Equal(assign,";")){
        info.next();
        statement = info.ast->createStatement(ASTStatement::ASSIGN);

        statement->name = new std::string(name);
        if(assignType!=0)
            statement->typeId = assignType;

        statement->rvalue = 0;
        
        statement->tokenRange.firstToken = info.get(startIndex);
        statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        return PARSE_SUCCESS;
    }else if(!Equal(assign,"=")){
        if(attempt)
            return PARSE_BAD_ATTEMPT;
        ERRT(assign) << "Expected = not "<<assign<<"\n";
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
        statement->typeId = assignType;

    ASTExpression* rvalue=0;
    int result = 0;
    result = ParseExpression(info,rvalue,false);

    if(result!=PARSE_SUCCESS){
        return PARSE_ERROR;
    }

    statement->rvalue = rvalue;
    
    statement->tokenRange.firstToken = info.get(startIndex);
    statement->tokenRange.startIndex = startIndex;
    statement->tokenRange.endIndex = info.at()+1;
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

    bodyLoc->tokenRange.firstToken = info.get(info.at()+1);
    bodyLoc->tokenRange.startIndex = info.at()+1;
    
    bool scoped=false;
    Token token = info.get(info.at()+1);
    if(Equal(token,"{")) {
        forceBrackets = true;
        scoped=true;
        token = info.next();
    }

    ASTStatement* prev=0;
    while(!info.end()){
        Token& token = info.get(info.at()+1);
        if(token=="}" && scoped){
            info.next();
            break;
        }
        ASTStatement* tempStatement=0;
        ASTStruct* tempStruct=0;
        ASTEnum* tempEnum=0;
        ASTFunction* tempFunction=0;
        
        int result=PARSE_BAD_ATTEMPT;

        if(result==PARSE_BAD_ATTEMPT)
            result = ParseFunction(info,tempFunction,true);
        if(result==PARSE_BAD_ATTEMPT)
            result = ParseStruct(info,tempStruct,true);
        if(result==PARSE_BAD_ATTEMPT)
            result = ParseEnum(info,tempEnum,true);
        if(result==PARSE_BAD_ATTEMPT)
            result = ParseAssignment(info,tempStatement,true);
        if(result==PARSE_BAD_ATTEMPT)
            result = ParseFlow(info,tempStatement,true);
        if(result==PARSE_BAD_ATTEMPT){
            // bad name of function? it parses an expression
            // prop assignment or function call
            result = ParsePropAssignment(info,tempStatement,true);
        }

        if(result==PARSE_BAD_ATTEMPT){
            Token& token = info.get(info.at()+1);
            ERRT(token) << "Unexpected '"<<token<<"'\n";
            ERRLINEP
            // log::out << log::RED; info.printPrevLine();log::out << "\n";
            ERRLINE
            // test other parse type
            info.next(); // prevent infinite loop
        }
        if(result==PARSE_ERROR){
            
        }
        if(result==PARSE_SUCCESS){
            if(tempStatement) {
                if(prev){
                    prev->next = tempStatement;
                    prev = tempStatement;
                }else{
                    bodyLoc->statements = tempStatement;
                    prev = tempStatement;
                }
            }
            if(tempFunction) {
                tempFunction->next = bodyLoc->functions;
                bodyLoc->functions = tempFunction;
            }
            if(tempStruct) {
                tempStruct->next = bodyLoc->structs;
                bodyLoc->structs = tempStruct;
            }
            if(tempEnum) {
                tempEnum->next = bodyLoc->enums;
                bodyLoc->enums = tempEnum;
            }
        }
        
        if(!forceBrackets)
            break;
    }
    bodyLoc->tokenRange.endIndex = info.at()+1;
    return PARSE_SUCCESS;
}

AST* ParseTokens(Tokens& tokens, int* outErr){
    using namespace engone;
    _VLOG(log::out <<log::BLUE<<  "##   Parser   ##\n";)
    
    ParseInfo info{tokens};
    
    info.ast = AST::Create();

    int result = ParseBody(info, info.ast->body,true);

    _VLOG(info.ast->print();)
    
    if(outErr){
        *outErr = info.errors;
    }
    if(info.errors)
        log::out << log::RED<<"Parser failed with "<<info.errors<<" errors\n";
    
    return info.ast;
}