#include "BetBat/Parser.h"

#undef ERR
#undef ERRT
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

// returns instruction type
// zero means no operation
OperationType IsOp(Token& token){
    if(token.flags&TOKEN_QUOTED) return (OperationType)0;
    if(Equal(token,"+")) return AST_ADD;
    if(Equal(token,"-")) return AST_SUB;
    if(Equal(token,"*")) return AST_MUL;
    if(Equal(token,"/")) return AST_DIV;
    if(Equal(token,"%")) return AST_MODULUS;
    if(Equal(token,"==")) return AST_EQUAL;
    if(Equal(token,"!=")) return AST_NOT_EQUAL;
    if(Equal(token,"<")) return AST_LESS;
    if(Equal(token,">")) return AST_GREATER;
    if(Equal(token,"<=")) return AST_LESS_EQUAL;
    if(Equal(token,">=")) return AST_GREATER_EQUAL;
    if(Equal(token,"&&")) return AST_AND;
    if(Equal(token,"||")) return AST_OR;
    if(Equal(token,"&")) return AST_BAND;
    if(Equal(token,"|")) return AST_BOR;
    if(Equal(token,"^")) return AST_BXOR;
    if(Equal(token,"~")) return AST_BNOT;
    if(Equal(token,"<<")) return AST_BLSHIFT;
    if(Equal(token,">>")) return AST_BRSHIFT;
    // NOT operation is special

    // if(token=="%") return BC_MOD;
    return (OperationType)0;
}
int OpPrecedence(int op){
    using namespace engone;
    if(op==AST_AND||op==AST_OR) return 1;
    if(op==AST_LESS||op==AST_GREATER||op==AST_LESS_EQUAL||op==AST_GREATER_EQUAL
        ||op==AST_EQUAL||op==AST_NOT_EQUAL) return 5;
    if(op==AST_ADD||op==AST_SUB) return 9;
    if(op==AST_MUL||op==AST_DIV||op==AST_MODULUS) return 10;
    if(op==AST_BAND||op==AST_BOR||op==AST_BXOR||
        op==AST_BLSHIFT||op==AST_BRSHIFT) return 13;
    if(op==AST_BNOT || op==AST_NOT || op==AST_CAST || op==AST_REFER || op==AST_DEREF) return 15;
    if(op==AST_MEMBER || op == AST_FROM_NAMESPACE) return 20;
    log::out << log::RED<<__FILE__<<":"<<__LINE__<<", missing "<<op<<"\n";
    return 0;
}
Token& ParseInfo::next(){
    Token& temp = tokens->get(index++);
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
    return tokens->get(index-2);
}
Token& ParseInfo::now(){
    return tokens->get(index-1);
}
Token& ParseInfo::get(uint _index){
    return tokens->get(_index);
}
bool ParseInfo::end(){
    Assert(index<=tokens->length());
    return index==tokens->length();
}
void ParseInfo::finish(){
    index = tokens->length();
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
        if(endToken>=tokens->length()){
            endToken = tokens->length()-1;
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
        // TODO: stop at ( # ) and such
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
// don't forget to free data in outgoing token
int ParseTypeId(ParseInfo& info, Token& outTypeId){
    using namespace engone;
    int startToken = info.at()+1;
    int endTok = info.at()+1; // exclusive
    int totalLength=0;
    
    // std::string spacename = "";
    // bool testNamespace = true;
    // TODO: check end of tokens
    while(!info.end()){
        Token& tok = info.get(endTok);
        if(tok.flags&TOKEN_QUOTED){
            ERRT(tok) << "Cannot have quotes in data type "<<tok<<"\n";
            ERRLINE
            // error = PARSE_ERROR;
            // NOTE: Don't return in order to stay synchronized.
            //   continue parsing like nothing happen.
        }
        // if(testNamespace){
        //     Token tok2 = info.get(endTok+1);
        //     if(IsName(tok) && Equal(tok2,"::")){
        //         // namespace I be gone?
        //         info.next();
        //         info.next();
                
        //         spacename += tok;
        //         spacename += tok2;
        //         startToken++;
        //         endTok++;
        //         continue;
        //     }else{
        //         testNamespace = false;
        //     }
        // }
        // TODO: Array<T,A> wouldn't work since comma is detected as end of data type.
        //      tuples won't work with paranthesis.
        if(Equal(tok,",") // (a: i32, b: i32)     struct {a: i32, b: i32}
            ||Equal(tok,"=") // a: i32 = 9;
            ||Equal(tok,")") // (a: i32)
            ||Equal(tok,"{") // () -> i32 {
            ||Equal(tok,"}") // struct { x: f32 }
            ||Equal(tok,"<") // ok: Map<i32>   i32 in poly list is handled afterwards
            ||Equal(tok,">") // cast<i32>
            ||Equal(tok,";") // cast<i32>
        ){
            break;
        }
        endTok++;
        totalLength+=tok.length;
        info.next();
        // if(tok.flags&TOKEN_SUFFIX_LINE_FEED)
        //     break;
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
int ParseFunction(ParseInfo& info, ASTFunction*& expression, bool attempt, ASTStruct* parentStruct);
int ParseExpression(ParseInfo& info, ASTExpression*& expression, bool attempt);
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
    info.next(); // struct
    attempt=false;
    
    Token name = info.get(info.at()+1);
    if(!IsName(name)){
        ERRT(name)<<"expected a name, "<<name<<" isn't\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next(); // name

    astStruct = info.ast->createStruct(name);
    astStruct->tokenRange.firstToken = structToken;
    astStruct->tokenRange.startIndex = startIndex;
    astStruct->tokenRange.tokenStream = info.tokens;
    
    Token token = info.get(info.at()+1);
    if(Equal(token,"<")){
        info.next();
        // polymorphic type
    
        while(true){
            token = info.get(info.at()+1);
            if(Equal(token,">")){
                info.next();
                if(astStruct->polyNames.size()==0){
                    ERRT(token) << "empty polymorph list\n";
                    ERRLINE
                }
                break;
            }
            if(!IsName(token)){
                ERRT(token) << token<<" is not a valid name for polymorphism\n";
                ERRLINE
            }else{
                info.next();
                astStruct->polyNames.push_back(token);
            }
            
            token = info.get(info.at()+1);
            if(Equal(token,",")){
                info.next();
                continue;
            }else if(Equal(token,">")){
                info.next();
                break;
            } else{
                ERRT(token) << token<<"is neither , or >\n";
                ERRLINE
                continue;
            }
        }
    }
    
    token = info.get(info.at()+1);
    if(!Equal(token,"{")){
        ERRT(token)<<"expected { not "<<token<<"\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next();
    
    // TODO: Structs need their own scopes for polymorphic types and
    //  internal namespaces, structs, enums...

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

        if (Equal(name,"fn")) {
            // parse function?
            ASTFunction* func = 0;
            int result = ParseFunction(info, func, false, astStruct);
            if(result!=PARSE_SUCCESS){
                continue;
            }
            astStruct->add(func);
        } else {
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
            std::string temps = dtToken;
            
            int polyIndex = -1;
            for(int i=0;i<(int)astStruct->polyNames.size();i++){
                if(temps.find(astStruct->polyNames[i])!=std::string::npos){
                    polyIndex = i;
                    break;
                }
            }
            
            // TypeInfo* typeInfo = info.ast->getTypeInfo(info.currentScopeId,dtToken);
            TypeId typeId = info.ast->addTypeString(dtToken);
            
            result = PARSE_SUCCESS;
            ASTExpression* defaultValue=0;
            token = info.get(info.at()+1);
            if(Equal(token,"=")){
                info.next();
                result = ParseExpression(info,defaultValue,false);
            }
            if(result == PARSE_SUCCESS){
                astStruct->members.push_back({});
                auto& mem = astStruct->members.back();
                mem.name = name;
                // if(!typeInfo)
                //     mem.typeId = 0;
                // else
                    mem.typeId = typeId;
                mem.defaultValue = defaultValue;
                mem.polyIndex = polyIndex;
            }
        }
        Token token = info.get(info.at()+1);
        if(Equal(token,";")){
            info.next();
            continue;
        }else if(Equal(token,"}")){
            info.next();
            break;
        }else{
            ERRT(token)<<"expected } or ; not "<<token<<"\n";
            ERRLINE
            error = PARSE_ERROR;
            continue;
        }
    }
    astStruct->tokenRange.endIndex = info.at()+1;
    // astStruct->size = offset;
    // make sure data type doesn't exist?
    // auto typeInfo = info.ast->getTypeInfo(info.currentScopeId,name,false,true);
    // int strId = info.ast->addTypeString(name);
    // if(!typeInfo){
    // ERRT(name) << name << " is taken\n";
    // info.ast->destroy(astStruct);
    // astStruct = 0;
    // return PARSE_ERROR;
    // }
    // typeInfo->astStruct = astStruct;
    // typeInfo->_size = astStruct->size;
    _GLOG(log::out << "Parsed struct "<<name << " with "<<astStruct->members.size()<<" members\n";)
    return error;
}
// int ParseStruct(ParseInfo& info, ASTStruct*& tempFunction, bool attempt);
int ParseEnum(ParseInfo& info, ASTEnum*& tempFunction, bool attempt);
int ParseNamespace(ParseInfo& info, ASTScope*& astNamespace, bool attempt){
    using namespace engone;
    _PLOG(FUNC_ENTER)
    Token token = info.get(info.at()+1);
    int startIndex = info.at()+1;
    if(!Equal(token,"namespace")){
        if(attempt)
            return PARSE_BAD_ATTEMPT;
        ERRT(token)<<"expected namespace not "<<token<<"\n";
        ERRLINE
        return PARSE_ERROR;
    }
    attempt = false;
    info.next();
    Token name = info.get(info.at()+1);
    if(!IsName(name)){
        ERRT(name)<<"expected a name, "<<name<<" isn't\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next();

    token = info.get(info.at()+1);
    if(!Equal(token,"{")){
        ERRT(token)<<"expected { not "<<token<<"\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next();
    
    int nextId=0;
    astNamespace = info.ast->createNamespace(name);
    astNamespace->tokenRange.firstToken = token;
    astNamespace->tokenRange.startIndex = startIndex;
    astNamespace->tokenRange.tokenStream = info.tokens;

    ScopeId newScopeId = info.ast->addScopeInfo();
    ScopeInfo* newScope = info.ast->getScopeInfo(newScopeId);
    newScope->parent = info.currentScopeId;
    
    astNamespace->scopeId = newScopeId;
    info.ast->getScopeInfo(newScopeId)->name = name;
    
    info.ast->getScopeInfo(info.currentScopeId)->nameScopeMap[name] = newScope;
    
    ScopeId prevScope = info.currentScopeId;
    defer { info.currentScopeId = prevScope; };
    info.currentScopeId = newScopeId;

    int error = PARSE_SUCCESS;
    while(!info.end()){
        Token& token = info.get(info.at()+1);
        if(Equal(token,"}")){
            info.next();
            break;
        }
        ASTStruct* tempStruct=0;
        ASTEnum* tempEnum=0;
        ASTFunction* tempFunction=0;
        ASTScope* tempNamespace=0;
    
        int result=PARSE_BAD_ATTEMPT;

        if(result==PARSE_BAD_ATTEMPT)
            result = ParseFunction(info,tempFunction,true, nullptr);
        if(result==PARSE_BAD_ATTEMPT)
            result = ParseStruct(info,tempStruct,true);
        if(result==PARSE_BAD_ATTEMPT)
            result = ParseEnum(info,tempEnum,true);
        if(result==PARSE_BAD_ATTEMPT)
            result = ParseNamespace(info,tempNamespace,true);

        if(result==PARSE_BAD_ATTEMPT){
            Token& token = info.get(info.at()+1);
            ERRT(token) << "Unexpected '"<<token<<"' (ParseNamespace)\n";
            ERRLINEP
            // log::out << log::RED; info.printPrevLine();log::out << "\n";
            ERRLINE
            // test other parse type
            info.next(); // prevent infinite loop
        }
        if(result==PARSE_ERROR){
            
        }
            
        // We add the AST structures even during error to
        // avoid leaking memory.
        if(tempNamespace) {
            astNamespace->add(tempNamespace, info.ast);
        }
        if(tempFunction) {
            astNamespace->add(tempFunction);
        }
        if(tempStruct) {
            astNamespace->add(tempStruct);
        }
        if(tempEnum) {
            astNamespace->add(tempEnum);
        }
    }

    astNamespace->tokenRange.endIndex = info.at()+1;
    _PLOG(log::out << "Namespace "<<name << "\n";)
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
    attempt=false;
    info.next(); // enum
    Token name = info.get(info.at()+1);
    if(!IsName(name)){
        ERRT(name)<<"expected a name, "<<name<<" isn't\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next(); // name
    
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
    astEnum->tokenRange.tokenStream = info.tokens;
    int error = PARSE_SUCCESS;
    while (true){
        Token name = info.get(info.at()+1);
        
        if(Equal(name,"}")){
            info.next();
            break;
        }
        
        if(!IsName(name)){
            info.next(); // move forward to prevent infinite loop
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
                ERRT(token) << token<<" is not an integer (i32)\n";
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
    // auto typeInfo = info.ast->getTypeInfo(info.currentScopeId, name, false, true);
    // int strId = info.ast->addTypeString(name);
    // if(!typeInfo){
        // ERRT(name) << name << " is taken\n";
        // info.ast->destroy(astEnum);
        // astEnum = 0;
        // return PARSE_ERROR;
    // }
    // typeInfo->astEnum = astEnum;
    // typeInfo->_size = 4; // i32
    _PLOG(log::out << "Parsed enum "<<name << " with "<<astEnum->members.size()<<" members\n";)
    return error;
}
// parses arguments and puts them into fncall->left
int ParseArguments(ParseInfo& info, ASTExpression* fncall, int* count){
    ASTExpression* tail=fncall->left;
    if(tail)
        while(tail->next) {
            tail = tail->next;
        }
    // TODO: sudden end, error handling
    bool expectComma=false;
    bool mustBeNamed=false;
    Token prevNamed = {};
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
            ERRT(tok)<<"expected ',' to supply more arguments or ')' to end fncall (found "<<tok<<" instead)\n";
            return PARSE_ERROR;
        }
        bool named=false;
        Token eq = info.get(info.at()+2);
        if(IsName(tok) && Equal(eq,"=")){
            info.next();
            info.next();
            prevNamed = tok;
            named=true;
            mustBeNamed = true;
        } else if(mustBeNamed){
            ERRT(tok) << "expected named argument because of previous named argument "<<prevNamed << " at "<<prevNamed.line<<":"<<prevNamed.column<<"\n";
            ERRLINE
            // return or continue could desync the parsing so don't do that.
        }

        ASTExpression* expr=0;
        int result = ParseExpression(info,expr,false);
        if(result!=PARSE_SUCCESS){
            // TODO: error message, parse other arguments instead of returning?
            return PARSE_ERROR;                         
        }
        if(named){
            expr->namedValue = new std::string(tok);
        }
        if(tail){
            tail->next = expr;
        }else{
            fncall->left = expr;
        }
        (*count)++;
        tail = expr;
        expectComma = true;
    }
    return PARSE_SUCCESS;
}
// @fex
int ParseExpression(ParseInfo& info, ASTExpression*& expression, bool attempt){
    using namespace engone;
    
    _PLOG(FUNC_ENTER)
    
    if(info.end()){
        if(attempt)
            return PARSE_BAD_ATTEMPT;
        ERR << "Sudden end\n";
        return PARSE_ERROR;
    }

    std::vector<ASTExpression*> values;
    std::vector<OperationType> ops;
    // std::vector<OperationType> directOps; // ! & *
    // extra info for directOps
    std::vector<TypeId> castTypes;
    // extra info for directOps
    std::vector<Token> namespaceNames;
    defer { for(auto e : values) info.ast->destroy(e); };

    bool negativeNumber=false;
    bool expectOperator=false;
    while(true){
        Token token = info.get(info.at()+1);
        
        OperationType opType = (OperationType)0;
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
                int startToken=info.at(); // start of fncall

                Token tok = info.get(info.at()+1);
                if(Equal(tok, "(")){
                    info.next();
                    ASTExpression* tmp = info.ast->createExpression(TypeId(AST_FNCALL));
                    tmp->name = new std::string(token);
                    
                    ASTExpression* refer = info.ast->createExpression(TypeId(AST_REFER));
                    refer->left = values.back();
                    values.pop_back();
                    tmp->left = refer;
                    tmp->boolValue = true; // indicicate fncall to struct method

                    int count = 0;
                    int result = ParseArguments(info, tmp, &count);
                    if(result!=PARSE_SUCCESS)
                        return result;

                    _PLOG(log::out << "Parsed call "<<count <<"\n";)
                    tmp->tokenRange.firstToken = token;
                    tmp->tokenRange.startIndex = startToken;
                    tmp->tokenRange.endIndex = info.at()+1;
                    tmp->tokenRange.tokenStream = info.tokens;


                    values.push_back(tmp);
                    continue;
                }
                
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_MEMBER));
                tmp->name = new std::string(token);
                tmp->left = values.back();
                values.pop_back();
                tmp->tokenRange.firstToken = tmp->left->tokenRange.firstToken;
                tmp->tokenRange.startIndex = tmp->left->tokenRange.startIndex;
                tmp->tokenRange.endIndex = info.at()+1;
                tmp->tokenRange.tokenStream = info.tokens;
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
                ops.push_back(AST_REFER);
                attempt = false;
                continue;
            }else if(Equal(token,"*")){
                info.next();
                ops.push_back(AST_DEREF);
                attempt = false;
                continue;
            } else if(Equal(token,"!")){
                info.next();
                ops.push_back(AST_NOT);
                attempt = false;
                continue;
            } else if(Equal(token,"~")){
                info.next();
                ops.push_back(AST_BNOT);
                attempt = false;
                continue;
            } else if(Equal(token,"cast")){
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
                ops.push_back(AST_CAST);
                // TypeId dt = info.ast->getTypeInfo(info.currentScopeId,tokenTypeId)->id;
                // castTypes.push_back(dt);
                TypeId strId = info.ast->addTypeString(tokenTypeId);
                castTypes.push_back(strId);
                attempt=false;
                continue;
            } else if(Equal(token,"-")){
                info.next();
                negativeNumber=true;
                attempt = false;
                continue;
            }

            if(IsInteger(token)){
                token = info.next();
                
                ASTExpression* tmp = 0;
                // TODO: handle to large numbers
                // TODO: don't always use tmp->i64Value
                if(token.str[0]=='-' || negativeNumber){
                    //-- signed
                    if(!negativeNumber){
                        token.str++;
                        token.length--;
                    }
                    negativeNumber=false;
                    
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
                        
                    tmp = info.ast->createExpression(TypeId(AST_INT32)); // default to this for now
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
                    tmp = info.ast->createExpression(TypeId(AST_INT32));
                    tmp->i64Value = num;
                }
                
                values.push_back(tmp);
                tmp->tokenRange.firstToken = token;
                tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                tmp->tokenRange.tokenStream = info.tokens;
            } else if(IsDecimal(token)){
                token = info.next();
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_FLOAT32));
                tmp->f32Value = ConvertDecimal(token);
                if(negativeNumber)
                    tmp->f32Value = -tmp->f32Value;
                negativeNumber=false;
                values.push_back(tmp);
                tmp->tokenRange.firstToken = token;
                tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                
                tmp->tokenRange.tokenStream = info.tokens;
            } else if(IsHexadecimal(token)){
                info.next();
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_UINT32));
                tmp->i64Value =  ConvertHexadecimal(token); // TODO: Only works with 32 bit or 16,8 I suppose.
                values.push_back(tmp);
                tmp->tokenRange.firstToken = token;
                tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                
                tmp->tokenRange.tokenStream = info.tokens;
            } else if(token.flags&TOKEN_QUOTED) {
                info.next();
                
                ASTExpression* tmp = 0;
                if(token.length == 1){
                    tmp = info.ast->createExpression(TypeId(AST_CHAR));
                    tmp->charValue = *token.str;
                }else {
                    tmp = info.ast->createExpression(TypeId(AST_STRING));
                    tmp->name = new std::string(token);

                    // A string of zero size can be useful which is why
                    // the code below is commented out.
                    // if(token.length == 0){
                        // ERRT(token) << "string should not have a length of 0\n";
                        // This is a semantic error and since the syntax is correct
                        // we don't need to return PARSE_ERROR. We could but things will be fine.
                    // }
                }
                values.push_back(tmp);
                tmp->tokenRange.firstToken = token;
                tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                
                tmp->tokenRange.tokenStream = info.tokens;
            } else if(Equal(token,"[")) {
                info.next();

                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_SLICE_INITIALIZER));

                // TODO: what about slice type. u16[] for example.
                //  you can do just [0,21] and it will auto assign a type in the generator
                //  but we may want to specify one.
 
                while(true){
                    Token token = info.get(info.at()+1);
                    if(Equal(token,"]")){
                        info.next();
                        break;
                    }
                    ASTExpression* expr;
                    int result = ParseExpression(info,expr,false);
                    // TODO: deal with result
                    if(result==PARSE_SUCCESS){
                        // NOTE: The list will be ordered in reverse.
                        //   The generator wants this when pushing values to the stack.
                        expr->next = tmp->left;
                        tmp->left = expr;
                    }
                    
                    token = info.get(info.at()+1);
                    if(Equal(token,",")){
                        info.next();
                        continue;
                    }else if(Equal(token,"]")){
                        info.next();
                        break;
                    }else {
                        ERRT(token) << "expected ] not "<<token<<"\n";
                        continue;
                    }
                }
                
                values.push_back(tmp);
                tmp->tokenRange.firstToken = token;
                tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                tmp->tokenRange.tokenStream = info.tokens;
            } else if(Equal(token,"null")){
                token = info.next();
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_NULL));
                values.push_back(tmp);
                tmp->tokenRange.firstToken = token;
                tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                tmp->tokenRange.tokenStream = info.tokens;
            } else if(Equal(token,"sizeof")) {
                info.next();
                Token token = info.get(info.at()+1);
                if(!IsName(token)){
                    ERRT(token) << "sizeof expects a single name token like 'SomeStruct123'. '"<<token<<"' is not valid.\n";
                    ERRLINE
                    return PARSE_ERROR;
                }
                info.next();
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_SIZEOF));
                tmp->name = new std::string(token);
                tmp->tokenRange.firstToken = token;
                tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                tmp->tokenRange.tokenStream = info.tokens;
                values.push_back(tmp);
            } else if(IsName(token)){
                info.next();
                int startToken=info.at();
                
                // could be a slice if tok[]{}
                // if something is within [] then it's a array access
                Token& tok = info.get(info.at()+1);
                if(Equal(tok,"(")){
                    // function call
                    info.next();
                    
                    std::string ns = "";
                    while(ops.size()>0){
                        OperationType op = ops.back();
                        if(op == AST_FROM_NAMESPACE) {
                            ns += namespaceNames.back();
                            ns += "::";
                            namespaceNames.pop_back();
                            ops.pop_back();
                        } else {
                            break;
                        }
                    }
                    ns += token;

                    ASTExpression* tmp = info.ast->createExpression(TypeId(AST_FNCALL));
                    tmp->name = new std::string(std::move(ns));
                    // tmp->name = new std::string(token); // When not doing AST_FROM_NAMESPACE
                    
                    int count = 0;
                    int result = ParseArguments(info, tmp, &count);
                    if(result!=PARSE_SUCCESS)
                        return result;

                    _PLOG(log::out << "Parsed call "<<count <<"\n";)
                    values.push_back(tmp);
                    tmp->tokenRange.firstToken = token;
                    tmp->tokenRange.startIndex = startToken;
                    tmp->tokenRange.endIndex = info.at()+1;
                    tmp->tokenRange.tokenStream = info.tokens;
                }else if(Equal(tok,"{") && 0 == (token.flags & TOKEN_SUFFIX_SPACE)){ // Struct {} is ignored
                    // initializer
                    info.next();
                    ASTExpression* tmp = info.ast->createExpression(TypeId(AST_INITIALIZER));
                    // NOTE: Type checker works on TypeIds not AST_FROM_NAMESPACE.
                    //  AST_FROM... is used for functions and variables.
                    std::string ns = "";
                    while(ops.size()>0){
                        OperationType op = ops.back();
                        if(op == AST_FROM_NAMESPACE) {
                            ns += namespaceNames.back();
                            ns += "::";
                            namespaceNames.pop_back();
                            ops.pop_back();
                        } else {
                            break;
                        }
                    }
                    ns += token;
                    tmp->castType = info.ast->addTypeString(ns);
                    // TODO: A little odd to use castType. Renaming castType to something more
                    //  generic which works for AST_CAST and AST_INITIALIZER would be better.
                    ASTExpression* tail=0;
                    
                    bool mustBeNamed=false;
                    Token prevNamed = {};
                    int count=0;
                    while(true){
                        Token token = info.get(info.at()+1);
                        if(Equal(token,"}")){
                           info.next();
                           break;
                        }

                        bool named=false;
                        Token eq = info.get(info.at()+2);
                        if(IsName(token) && Equal(eq,"=")){
                            info.next();
                            info.next();
                            named = true;
                            prevNamed = token;
                            mustBeNamed = true;
                        } else if(mustBeNamed){
                            ERRT(token) << "expected named argument because of previous named argument "<<prevNamed << " at "<<prevNamed.line<<":"<<prevNamed.column<<"\n";
                            ERRLINE
                            // return or continue could desync the parsing so don't do that.
                        }

                        ASTExpression* expr=0;
                        int result = ParseExpression(info,expr,false);
                        if(result!=PARSE_SUCCESS){
                            // TODO: error message, parse other arguments instead of returning?
                            // return PARSE_ERROR; Returning would leak arguments so far
                            info.next(); // prevent infinite loop
                            continue;
                        }
                        if(named){
                            expr->namedValue = new std::string(token);
                        }
                        if(tail){
                            tail->next = expr;
                        }else{
                            tmp->left = expr;
                        }
                        count++;
                        tail = expr;
                        
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
                    // log::out << "Parse initializer "<<count<<"\n";
                    values.push_back(tmp);
                    tmp->tokenRange.firstToken = token;
                    tmp->tokenRange.startIndex = startToken;
                    tmp->tokenRange.endIndex = info.at()+1;
                    tmp->tokenRange.tokenStream = info.tokens;
                }else if(Equal(tok,"::")){
                    info.next();
                    
                    // ERRT(tok) << " :: is not implemented\n";
                    // ERRLINE
                    // continue; 
                    
                    Token tok = info.get(info.at()+1);
                    if(!IsName(tok)){
                        ERRT(tok) << tok<<" is not a name\n";
                        ERRLINE
                        continue;
                    }
                    ops.push_back(AST_FROM_NAMESPACE);
                    namespaceNames.push_back(token);
                    // namespaceToken = token;
                    // namespaceName = new std::string(token);
                    continue; // do a second round?
                    
                    // TODO: detect more ::
                    
                    // ASTExpression* tmp = info.ast->createExpression(AST_FROM_NAMESPACE);
                    // tmp->name = new std::string(token);
                    // tmp->member = new std::string(tok);
                    // values.push_back(tmp);
                    // tmp->tokenRange.firstToken = token;
                    // tmp->tokenRange.startIndex = startToken;
                    // tmp->tokenRange.endIndex = info.at()+1;
                    // tmp->tokenRange.tokenStream = info.tokens;
                } else{
                    ASTExpression* tmp = info.ast->createExpression(TypeId(AST_VAR));

                    std::string ns = "";
                    while(ops.size()>0){
                        OperationType op = ops.back();
                        if(op == AST_FROM_NAMESPACE) {
                            ns += namespaceNames.back();
                            ns += "::";
                            namespaceNames.pop_back();
                            ops.pop_back();
                        } else {
                            break;
                        }
                    }
                    ns += token;

                    tmp->name = new std::string(std::move(ns));

                    // tmp->name = new std::string(token); // When not incorporating AST_FROM_NAMESPACE
                    values.push_back(tmp);
                    tmp->tokenRange.firstToken = token;
                    tmp->tokenRange.startIndex = startToken;
                    tmp->tokenRange.endIndex = info.at()+1;
                    tmp->tokenRange.tokenStream = info.tokens;
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
                    // Qutting here is okay because we have a defer which
                    // destroys parsed expressions. No memory leaks!
                }
            }
        }
        if(negativeNumber){
            ERRT(info.get(info.at()-1)) << "Unexpected - before "<<token<<"\n";
            ERRLINE
            return PARSE_ERROR;
            // quitting here is a little unexpected but there is
            // a defer which destroys parsed expresions so no memory leaks at least.
        }
        // while(directOps.size()>0){
        //     OperationType op = directOps.back();
        //     directOps.pop_back();

        //     auto val = values.back();
        //     values.pop_back();
            
        //     auto newVal = info.ast->createExpression(TypeId(op));
        //     // TODO: token range doesn't include the operation token. It should.
        //     newVal->tokenRange = val->tokenRange;
        //     if(op==AST_FROM_NAMESPACE){
        //         Token& tok = namespaceNames.back();
        //         newVal->name = new std::string(tok);
        //         newVal->tokenRange.firstToken = tok;
        //         newVal->tokenRange.startIndex -= 2; // -{namespace name} -{::}
        //         namespaceNames.pop_back();
        //     } else if(op==AST_CAST){
        //         newVal->castType = castTypes.back();
        //         castTypes.pop_back();
        //     }
        //     newVal->left = val;

        //     values.push_back(newVal);
        // }
        
        ending = ending || info.end();

        while(values.size()>0){

            OperationType nowOp = (OperationType)0;
            if(ops.size()>=2&&!ending){
                if(values.size()<2)
                    break;
                OperationType op1 = ops[ops.size()-2];
                OperationType op2 = ops[ops.size()-1];
                if(OpPrecedence(op1)>=OpPrecedence(op2)){
                    nowOp = op1;
                    ops[ops.size()-2] = op2;
                    ops.pop_back();
                }else{
                    // _PLOG(log::out << "break\n";)
                    break;
                }
            }else if(ending){
                if(ops.size()<1){
                    // NOTE: Break on base case when we end with no operators left and with one value. 
                    break;
                }
                nowOp = ops.back();
                ops.pop_back();
            }else{
                break;
            }
            auto er = values.back();
            values.pop_back();

            auto val = info.ast->createExpression(TypeId(nowOp));
            val->tokenRange.tokenStream = info.tokens;
            // FROM_NAMESPACE, CAST, REFER, DEREF, NOT, BNOT
            if(nowOp == AST_REFER || nowOp == AST_DEREF || nowOp == AST_NOT || nowOp == AST_BNOT||
                nowOp == AST_FROM_NAMESPACE || nowOp == AST_CAST){
                
                val->tokenRange = er->tokenRange;
                if(nowOp == AST_FROM_NAMESPACE){
                    Token& tok = namespaceNames.back();
                    val->name = new std::string(tok);
                    val->tokenRange.firstToken = tok;
                    val->tokenRange.startIndex -= 2; // -{namespace name} -{::}
                    namespaceNames.pop_back();
                } else if(nowOp == AST_CAST){
                    val->castType = castTypes.back();
                    castTypes.pop_back();
                }
                
                val->left = er;
            } else if(values.size()>0){
                auto el = values.back();
                values.pop_back();
                val->tokenRange.firstToken = el->tokenRange.firstToken;
                val->tokenRange.startIndex = el->tokenRange.startIndex;
                val->tokenRange.endIndex = er->tokenRange.endIndex;
                val->left = el;
                val->right = er;
            }

            values.push_back(val);
        }
        // Attempt succeeded. Failure is now considered an error.
        attempt = false;
        expectOperator=!expectOperator;
        if(ending){
            expression = values.back();
            Assert(values.size()==1)

            // we have a defer above which destroys expressions which is why we
            // clear the list to prevent it
            values.clear();
            return PARSE_SUCCESS;
        }
    }
}
int ParseBody(ParseInfo& info, ASTScope*& body, bool globalScope=false);
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
        ASTScope* body=0;
        result = ParseBody(info,body);
        if(result!=PARSE_SUCCESS){
            return PARSE_ERROR;
        }
        
        ASTScope* elseBody=0;
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
        statement->tokenRange.tokenStream = info.tokens;
        return PARSE_SUCCESS;
    }else if(Equal(firstToken,"while")){
        info.next();
        ASTExpression* expr=0;
        int result = ParseExpression(info,expr,false);
        if(result!=PARSE_SUCCESS){
            // TODO: should more stuff be done here?
            return PARSE_ERROR;
        }
        ASTScope* body=0;
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
        statement->tokenRange.tokenStream = info.tokens;
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
        statement->tokenRange.tokenStream = info.tokens;
        return PARSE_SUCCESS;
    } else if(Equal(firstToken,"using")){
        info.next();

        // TODO: namespace environemnt, not just using X as Y

        Token name = info.get(info.at()+1);
        if(!IsName(name)){
            ERRT(name) << "expected a name, "<<name <<" isn't one\n";
            ERRLINE;
            return PARSE_ERROR;
        }
        info.next(); 

        Token token = info.get(info.at()+1);
        if(!Equal(token,"as")){
            ERRT(token) << "expected as not "<<token <<"\n";
            ERRLINE;
            return PARSE_ERROR;
        }
        info.next();
        
        Token alias = info.get(info.at()+1);
        if(!IsName(alias)){
            ERRT(alias) << "expected a name, "<<alias <<" isn't one\n";
            ERRLINE;
            return PARSE_ERROR;
        }
        info.next();
        
        statement = info.ast->createStatement(ASTStatement::USING);
        statement->name = new std::string(name);
        statement->alias = new std::string(alias);

        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        statement->tokenRange.tokenStream = info.tokens;
        return PARSE_SUCCESS;        
    } else if(Equal(firstToken,"defer")){
        info.next();

        ASTScope* body = nullptr;
        int result = ParseBody(info, body);
        if(result != PARSE_SUCCESS) {
            return result;
        }

        statement = info.ast->createStatement(ASTStatement::DEFER);
        statement->body = body;

        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        statement->tokenRange.tokenStream = info.tokens;
        return PARSE_SUCCESS;  
    } else if(Equal(firstToken, "break")) {
        info.next();

        statement = info.ast->createStatement(ASTStatement::BREAK);

        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        statement->tokenRange.tokenStream = info.tokens;
        return PARSE_SUCCESS;
    } else if(Equal(firstToken, "continue")) {
        info.next();

        statement = info.ast->createStatement(ASTStatement::CONTINUE);

        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        statement->tokenRange.tokenStream = info.tokens;
        return PARSE_SUCCESS;  
    }
    if(attempt)
        return PARSE_BAD_ATTEMPT;
    return PARSE_ERROR;
}
// out token contains a newly allocated string. use delete[] on it
int ParseFunction(ParseInfo& info, ASTFunction*& function, bool attempt, ASTStruct* parentStruct){
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
    function->tokenRange.tokenStream = info.tokens;

    if(parentStruct) {
        function->arguments.push_back({});
        auto& argv = function->arguments.back();
        argv.name = "this";
        argv.typeId = info.ast->addTypeString(parentStruct->name+"*");;
        argv.defaultValue = nullptr;
    }
    bool mustHaveDefault=false; 
    TokenRange prevDefault={};
    while(true){
        Token& arg = info.next();
        if(Equal(arg,")")){
            break;
        }
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
        // auto id = info.ast->getTypeInfo(info.currentScopeId,dataType)->id;
        TypeId strId = info.ast->addTypeString(dataType);

        ASTExpression* defaultValue=0;
        tok = info.get(info.at()+1);
        if(Equal(tok,"=")){
            info.next();
            
            int result = ParseExpression(info,defaultValue,false);
            if(result!=PARSE_SUCCESS){
                continue;
            }
            prevDefault = defaultValue->tokenRange;

            mustHaveDefault=true;
        } else if(mustHaveDefault){
            ERRT(tok) << "expected a default argument because of previous default argument "<<prevDefault<<" at "<<prevDefault.firstToken.line<<":"<<prevDefault.firstToken.column<<"\n";
            ERRLINE
            continue;
        }

        function->arguments.push_back({});
        auto& argv = function->arguments.back();
        argv.name = arg;
        argv.typeId = strId;
        argv.defaultValue = defaultValue;

        tok = info.get(info.at()+1);
        if(Equal(tok,",")){
            info.next();
            continue;
        }else if(Equal(tok,")")){
            info.next();
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
                // auto id = info.ast->getTypeInfo(info.currentScopeId,dt)->id;
                TypeId strId = info.ast->addTypeString(dt);
                function->returnTypes.push_back(strId);
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
                // auto id = info.ast->getTypeInfo(info.currentScopeId,dt)->id;
                TypeId strId = info.ast->addTypeString(dt);
                function->returnTypes.push_back(strId);
            }
        }
    }

    ASTScope* body = 0;
    int result = ParseBody(info,body);

    function->body = body;
    function->tokenRange.endIndex = info.at()+1;

    return PARSE_SUCCESS;
}
int ParsePropAssignment(ParseInfo& info, ASTStatement*& statement, bool attempt){
    using namespace engone;
    _PLOG(FUNC_ENTER)
    
    int startIndex = info.at()+1;
    
    ASTExpression* lvalue = 0;
    int result = ParseExpression(info,lvalue,attempt);       
    if(result!=PARSE_SUCCESS) return result;
    
    attempt = false;
    
    Token tok = info.get(info.at()+1);
    if(!Equal(tok,"=") && !Equal(tok,"+=") && !Equal(tok,"-=") && !Equal(tok,"*=") && !Equal(tok,"/=")){
        statement = info.ast->createStatement(ASTStatement::CALL);
        statement->rvalue = lvalue;   
        statement->tokenRange.firstToken = info.get(startIndex);
        statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        statement->tokenRange.tokenStream = info.tokens;
        return PARSE_SUCCESS;
    }
    info.next();
    
    ASTExpression* rvalue = 0;
    result = ParseExpression(info,rvalue,attempt);
    if(result!=PARSE_SUCCESS) return result;
    
    statement = info.ast->createStatement(ASTStatement::PROP_ASSIGN);
    
    if(Equal(tok,"+="))
        statement->opType = AST_ADD;
    if(Equal(tok,"-="))
        statement->opType = AST_SUB;
    if(Equal(tok,"*="))
        statement->opType = AST_MUL;
    if(Equal(tok,"/="))
        statement->opType = AST_DIV;
    statement->lvalue = lvalue;
    statement->rvalue= rvalue;
    
    statement->tokenRange.firstToken = info.get(startIndex);
    statement->tokenRange.startIndex = startIndex;
    statement->tokenRange.endIndex = info.at()+1;
    statement->tokenRange.tokenStream = info.tokens;
    
    return PARSE_SUCCESS;
}
// normal assignment a = 9
int ParseAssignment(ParseInfo& info, ASTStatement*& statement, bool attempt){
    using namespace engone;
    _PLOG(FUNC_ENTER)
    
    if(info.tokens->length() < info.index+2){
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
    
    TypeId assignType = {};
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
        
        Token typeToken{};
        int result = ParseTypeId(info,typeToken);
        defer {
            delete[] typeToken.str;
        };
        Token token = info.get(info.at()+1);
        std::vector<TypeId> polyList;
        std::string typeString = typeToken;
        // TypeInfo* typeInfo = info.ast->getTypeInfo(info.currentScopeId,typeString);
        TypeId strId = info.ast->addTypeString(typeString);
        
        // if(Equal(token,"<")){
        //     info.next();
        //     typeString+="<";
        //     while(true){
        //         Token token = info.get(info.at()+1);
        //         if(Equal(token,">")){
        //             info.next();
        //             break;
        //         }
        //         Token polyToken{};
        //         int result = ParseTypeId(info,polyToken);
        //         if(result==PARSE_ERROR){
                    
        //         }
        //         TypeId polyId = info.ast->getTypeInfo(info.currentScopeId,polyToken)->id;
        //         if(polyList.size()!=0){
        //             typeString+=",";
        //         }
        //         polyList.push_back(polyId);
        //         typeString+=std::string(polyToken);
        //         token = info.get(info.at()+1);
        //         if(Equal(token,",")){
        //             info.next();
        //             continue;
        //         } else if(Equal(token,">")){
        //             info.next();
        //             break;
        //         }else{
        //             ERRT(token) << "Unexepcted "<<token<<". Should be , or >\n";
        //         }
        //     }
        //     typeString+=">";

        //     TypeInfo* polyInfo = info.ast->getTypeInfo(info.currentScopeId,typeString);
        //     polyInfo->polyOrigin = typeInfo;
        //     polyInfo->polyTypes = polyList;
        //     polyInfo->astStruct = typeInfo->astStruct;
        //     polyInfo->_size = typeInfo->size(); // TODO: NOT CORRECT!
        // }

        // TypeInfo* ti = info.ast->getTypeInfo(info.currentScopeId,typeString);
        // if(ti){
            // TypeId dtId = ti->id;
            TypeId dtId = strId;
            assignType = dtId;
        // }
        // else assignType = 0;
        assign = info.get(info.at()+1);

        // if (error==PARSE_SUCCESS)
        //     log::out << "Created datatype "<<dataType<<" "<<dtId<<"\n";
    }
    
    if(Equal(assign,";")){
        info.next();
        statement = info.ast->createStatement(ASTStatement::ASSIGN);

        statement->name = new std::string(name);
        if(assignType.isString())
            statement->typeId = assignType;

        statement->rvalue = 0;
        
        statement->tokenRange.firstToken = info.get(startIndex);
        statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        statement->tokenRange.tokenStream = info.tokens;
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
    // if(assignType.getId()==0)
    if(!assignType.isValid())
        info.next(); // next on name if : part didn't do it

    info.next();

    statement = info.ast->createStatement(ASTStatement::ASSIGN);

    // log::out << reloc<<"\n";
    statement->name = new std::string(name);
    // if(assignType.getId()!=0)
    if(assignType.isValid())
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
    statement->tokenRange.tokenStream = info.tokens;
        
    return error;   
}
int ParseBody(ParseInfo& info, ASTScope*& bodyLoc, bool globalScope){
    using namespace engone;
    // Note: two infos in case ParseAssignment modifies it and then fails.
    //  without two, ParseCommand would work with a modified info.
    _PLOG(FUNC_ENTER)
    // _PLOG(ENTER)

    // empty file is fine
    // if(info.end()){
    //     ERR << "Sudden end\n";
    //     return PARSE_ERROR;
    // }
    
    if(!bodyLoc)
        bodyLoc = info.ast->createBody();
    if(!info.end()){
        bodyLoc->tokenRange.firstToken = info.get(info.at()+1);
        bodyLoc->tokenRange.startIndex = info.at()+1;
        bodyLoc->tokenRange.tokenStream = info.tokens;
    }
    bool scoped=false;
    if(!globalScope && !info.end()){
        Token token = info.get(info.at()+1);
        if(Equal(token,"{")) {
            scoped=true;
            token = info.next();
        }
    }
    ScopeId prevScope = info.currentScopeId;
    defer { info.currentScopeId = prevScope; };
    if(scoped){
        info.currentScopeId = info.ast->addScopeInfo();
        ScopeInfo* si = info.ast->getScopeInfo(info.currentScopeId);
        si->parent = prevScope;

    }
    bodyLoc->scopeId = info.currentScopeId;

    ASTStatement* latestDefer = nullptr; // latest parsed defer
    
    while(!info.end()){
        Token& token = info.get(info.at()+1);
        if(scoped && Equal(token,"}")){
            info.next();
            break;
        }
        ASTStatement* tempStatement=0;
        ASTStruct* tempStruct=0;
        ASTEnum* tempEnum=0;
        ASTFunction* tempFunction=0;
        ASTScope* tempNamespace=0;
        
        int result=PARSE_BAD_ATTEMPT;

        if(Equal(token,"{")){
            ASTScope* body=0;
            result = ParseBody(info,body);
            if(result!=PARSE_SUCCESS){
                info.next(); // skip { to avoid infinite loop
                continue;
            }
            tempStatement = info.ast->createStatement(ASTStatement::BODY);
            tempStatement->body = body;
        }
        if(result==PARSE_BAD_ATTEMPT)
            result = ParseFunction(info,tempFunction,true, nullptr);
        if(result==PARSE_BAD_ATTEMPT)
            result = ParseStruct(info,tempStruct,true);
        if(result==PARSE_BAD_ATTEMPT)
            result = ParseEnum(info,tempEnum,true);
        if(result==PARSE_BAD_ATTEMPT)
            result = ParseNamespace(info,tempNamespace,true);
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
            ERRT(token) << "Unexpected '"<<token<<"' (ParseBody)\n";
            ERRLINEP
            // log::out << log::RED; info.printPrevLine();log::out << "\n";
            ERRLINE
            // prevent infinite loop. Loop only occurs when scoped
            info.next();
        }
        if(result==PARSE_ERROR){
            
        }
        // NOTE: ANY CHANGES HERE SHOULD PROBABLY BE ADDED
        //  TO ParseNamespace!
        // We add the AST structures even during error to
        // avoid leaking memory.
        if(tempStatement) {
            if(tempStatement->type == ASTStatement::DEFER) {
                if(!latestDefer){
                    latestDefer = tempStatement;
                } else {
                    tempStatement->next = latestDefer;
                    latestDefer = tempStatement;
                }
            } else {
                bodyLoc->add(tempStatement);
            }
        }
        if(tempFunction) {
            bodyLoc->add(tempFunction);
        }
        if(tempStruct) {
            bodyLoc->add(tempStruct);
        }
        if(tempEnum) {
            bodyLoc->add(tempEnum);
        }
        if(tempNamespace) {
            bodyLoc->add(tempNamespace, info.ast);
        }
        if(result==PARSE_BAD_ATTEMPT){
            if(!scoped) // try again
                continue;
        }
        if(!scoped && !globalScope)
            break;
    }
    if(latestDefer)
        bodyLoc->add(latestDefer);

    if(!info.end()){
        bodyLoc->tokenRange.endIndex = info.at()+1;
    }
    return PARSE_SUCCESS;
}

ASTScope* ParseTokens(TokenStream* tokens, AST* ast, int* outErr, std::string theNamespace){
    using namespace engone;
    // _VLOG(log::out <<log::BLUE<<  "##   Parser   ##\n";)
    
    ParseInfo info{tokens};
    info.tokens = tokens;
    info.ast = ast;
    info.currentScopeId = ast->globalScopeId;
    
    // if(optionalAST)
    //     info.ast = optionalAST;
    // else{
    //     info.ast = AST::Create();
    // }
    ASTScope* body = 0;
    ScopeId prevScope = info.currentScopeId;
    if(!theNamespace.empty()){
        body = info.ast->createNamespace(theNamespace);
        
        ScopeId newScopeId = info.ast->addScopeInfo();
        ScopeInfo* newScope = info.ast->getScopeInfo(newScopeId);
        
        newScope->parent = info.currentScopeId;
        
        body->scopeId = newScopeId;
        info.ast->getScopeInfo(newScopeId)->name = theNamespace;
        info.ast->getScopeInfo(info.currentScopeId)->nameScopeMap[theNamespace] = newScope;
        
        info.currentScopeId = newScopeId;
    }
    
    int result = ParseBody(info, body, true);
    
    info.currentScopeId = prevScope;
    
    if(outErr){
        *outErr += info.errors;
    }
    
    // compiler logs the error count, dont need it here too
    // if(info.errors)
    //     log::out << log::RED<<"Parser failed with "<<info.errors<<" error(s)\n";
    
    return body;}