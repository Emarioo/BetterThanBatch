#include "BetBat/Parser.h"
#include "BetBat/Compiler.h"

#undef ERR
#undef ERR_END
#undef ERR_HEAD2
#undef ERR_HEAD
#undef WARN_HEAD2
#undef WARN_HEAD
#undef ERR_LINE
#undef WARN_LINE
// #undef ERRAT

#define ERR_HEAD2(T) info.compileInfo->errors++;engone::log::out << ERR_DEFAULT_T(info.tokens,T,"Parse error","E0000")
#define WARN_HEAD2(T) info.compileInfo->warnings++; engone::log::out << WARN_CUSTOM(info.tokens->streamName,T.line,T.column,"Parse error","E0000")

#define WARN_HEAD(T, M) info.compileInfo->warnings++;engone::log::out << WARN_CUSTOM(info.tokens->streamName,T.line,T.column,"Parse warning","W0000") << M
#define ERR_HEAD(T, M) info.compileInfo->errors++;engone::log::out << ERR_DEFAULT_T(info.tokens,T,"Parse error","E0000") << M
#define ERR_LINE(I, M) PrintCode(I, info.tokens, M)
#define WARN_LINE(I, M) PrintCode(I, info.tokens, M)

#define ERR info.compileInfo->errors++;engone::log::out << engone::log::RED << "(Parse error E0000): "
#define ERR_END MSG_END

#define INST engone::log::out << (info.code.length()-1)<<": " <<(info.code.get(info.code.length()-1)) << ", "
// #define INST engone::log::out << (info.code.get(info.code.length()-2).type==BC_LOADC ? info.code.get(info.code.length()-2) : info.code.get(info.code.length()-1)) << ", "

#define ERRLINE engone::log::out <<engone::log::RED<<info.now().line<<"|> ";info.printLine();engone::log::out<<"\n";
#define ERRLINEP engone::log::out <<engone::log::RED<<info.get(info.at()-1).line<<"|> ";info.printPrevLine();engone::log::out<<"\n";

// returns instruction type
// zero means no operation
bool IsSingleOp(OperationType nowOp){
    return nowOp == AST_REFER || nowOp == AST_DEREF || nowOp == AST_NOT || nowOp == AST_BNOT||
        nowOp == AST_FROM_NAMESPACE || nowOp == AST_CAST || nowOp == AST_INCREMENT || nowOp == AST_DECREMENT;
}
// info is required to perform next when encountering
// two arrow tokens
OperationType IsOp(Token& token, int& extraNext){
    if(token.flags&TOKEN_QUOTED) return (OperationType)0;
    if(Equal(token,"+")) return AST_ADD;
    if(Equal(token,"-")) return AST_SUB;
    if(Equal(token,"*")) return AST_MUL;
    if(Equal(token,"/")) return AST_DIV;
    if(Equal(token,"%")) return AST_MODULUS;
    if(Equal(token,"==")) return AST_EQUAL;
    if(Equal(token,"!=")) return AST_NOT_EQUAL;
    if(Equal(token,"<")) {
        if(Equal(token.tokenStream->get(token.tokenIndex+1),"<")) {
            extraNext=1;
            return AST_BLSHIFT;
        }
         return AST_LESS;
    }
    if(Equal(token,">")) {
        if(Equal(token.tokenStream->get(token.tokenIndex+1),">")) {
            extraNext=1;
            return AST_BRSHIFT;
        }
        return AST_GREATER;
    }
    if(Equal(token,"<=")) return AST_LESS_EQUAL;
    if(Equal(token,">=")) return AST_GREATER_EQUAL;
    if(Equal(token,"&&")) return AST_AND;
    if(Equal(token,"||")) return AST_OR;
    if(Equal(token,"&")) return AST_BAND;
    if(Equal(token,"|")) return AST_BOR;
    if(Equal(token,"^")) return AST_BXOR;
    if(Equal(token,"~")) return AST_BNOT;
    if(Equal(token,"..")) return AST_RANGE;
    // NOT operation is special

    return (OperationType)0;
}
OperationType IsAssignOp(Token& token){
    if(!token.str || token.length!=1) return (OperationType)0; // early exit since all assign op only use one character
    char chr = *token.str;
    // TODO: Allow % modulus as assignment?
    if(chr == '+'||chr=='-'||chr=='*'||chr=='/'||
        chr=='/'||chr=='|'||chr=='&'||chr=='^'){
        int _=0;
        return IsOp(token,_);
    }
    return (OperationType)0;
}
int OpPrecedence(int op){
    using namespace engone;
    // use a map instead?
    switch (op){
        case AST_ASSIGN:
            return 2;
        case AST_AND:
        case AST_OR:
            return 3;
        case AST_LESS:
        case AST_GREATER:
        case AST_LESS_EQUAL:
        case AST_GREATER_EQUAL:
        case AST_EQUAL:
        case AST_NOT_EQUAL:
            return 5;
        case AST_RANGE:
            return 8;
        case AST_ADD:
        case AST_SUB:
            return 9;
        case AST_MUL:
        case AST_DIV:
        case AST_MODULUS:
            return 10;
        case AST_BAND:
        case AST_BOR:
        case AST_BXOR:
        case AST_BLSHIFT:
        case AST_BRSHIFT:
            return 13;
        case AST_BNOT:
        case AST_NOT:
        case AST_CAST:
            return 15;
        case AST_REFER:
        case AST_DEREF:
            return 16;
        case AST_MEMBER:
        case AST_FROM_NAMESPACE:
            return 20;
    }
    // if(op==AST_ASSIGN) return 2;
    // if(op==AST_AND||op==AST_OR) return 3;
    // if(op==AST_LESS||op==AST_GREATER||op==AST_LESS_EQUAL||op==AST_GREATER_EQUAL
    //     ||op==AST_EQUAL||op==AST_NOT_EQUAL) return 5;
    // if(op==AST_RANGE) return 8;
    // if(op==AST_ADD||op==AST_SUB) return 9;
    // if(op==AST_MUL||op==AST_DIV||op==AST_MODULUS) return 10;
    // if(op==AST_BAND||op==AST_BOR||op==AST_BXOR||
    //     op==AST_BLSHIFT||op==AST_BRSHIFT) return 13;
    // if(op==AST_BNOT || op==AST_NOT || op==AST_CAST) return 15;
    // if(op==AST_REFER || op==AST_DEREF) return 16;
    // if(op==AST_MEMBER || op == AST_FROM_NAMESPACE) return 20;
    log::out << log::RED<<__FILE__<<":"<<__LINE__<<", missing "<<op<<"\n";
    return 0;
}
Token& ParseInfo::next(){
    // return tokens->next();
    Token& temp = tokens->get(index++);
    return temp;
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
    // Assert(index<=tokens->length());
    return index>=tokens->length();
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
        Token& tok = get(i); 
        log::out << tok;
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
    MEASURE;
    int startToken = info.at()+1;
    int endTok = info.at()+1; // exclusive
    int totalLength=0;
    
    // TODO: check end of tokens

    int depth = 0;
    bool wasName = false;
    outTypeId = {};
    while(!info.end()){
        Token& tok = info.get(endTok);
        if(tok.flags&TOKEN_QUOTED){
            ERR_HEAD2(tok) << "Cannot have quotes in data type "<<tok<<"\n";
            ERRLINE
            // NOTE: Don't return in order to stay synchronized.
            //   continue parsing like nothing happen.
        }
        if(Equal(tok,"<")) {
            depth++;
        }else if(Equal(tok,">")) {
            if(depth == 0){
                break;
            }
            depth--;
            if(depth == 0){
                endTok++;
                outTypeId.length += tok.length;
                info.next();
                break;
            }
        }
        // TODO: Array<T,A> wouldn't work since comma is detected as end of data type.
        //      tuples won't work with paranthesis.
        if(depth == 0) {
            if(Equal(tok,",") // (a: i32, b: i32)     struct {a: i32, b: i32}
                ||Equal(tok,"=") // a: i32 = 9;
                ||Equal(tok,")") // (a: i32)
                ||Equal(tok,"{") // () -> i32 {
                ||Equal(tok,"}") // struct { x: f32 }
                ||Equal(tok,";") // cast<i32>
            ){
                break;
            } else if(Equal(tok,"[")) {
                Token& token = info.get(endTok+1);
                if(!Equal(token,"]"))
                    break;
            }
            if(IsName(tok)){
                if(wasName) {
                    break;
                }
            }
        }
        if(!IsName(tok)){
            wasName = false;
        } else {
            wasName = true;
        }
        if(endTok == startToken) {
            outTypeId.str = tok.str;
            outTypeId.tokenIndex = startToken;
            outTypeId.column = tok.column;
            outTypeId.line = tok.line;
            outTypeId.tokenStream = tok.tokenStream;
        }
        endTok++;
        outTypeId.length += tok.length;
        info.next();
    }

    return PARSE_SUCCESS;
}
int ParseFunction(ParseInfo& info, ASTFunction*& expression, bool attempt, ASTStruct* parentStruct);
int ParseExpression(ParseInfo& info, ASTExpression*& expression, bool attempt);
int ParseStruct(ParseInfo& info, ASTStruct*& astStruct,  bool attempt){
    using namespace engone;
    MEASURE;
    _PLOG(FUNC_ENTER)
    Token structToken = info.get(info.at()+1);
    int startIndex = info.at()+1;
    if(!Equal(structToken,"struct")){
        if(attempt) return PARSE_BAD_ATTEMPT;
        ERR_HEAD2(structToken)<<"expected struct not "<<structToken<<"\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next(); // struct
    attempt=false;

    bool hideAnnotation = false;

    Token name = info.get(info.at()+1);
    while (IsAnnotation(name)){
        info.next();
        if(Equal(name,"@hide")){
            hideAnnotation=true;
        // } else if(Equal(name,"@export")) {
        } else {
            WARN_HEAD(name, "'"<< Token(name.str+1,name.length-1) << "' is not a known annotation for functions.\n\n";
                WARN_LINE(info.at(),"unknown");
            )
        }
        name = info.get(info.at()+1);
        continue;
    }
    
    if(!IsName(name)){
        ERR_HEAD2(name)<<"expected a name, "<<name<<" isn't\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next(); // name

    astStruct = info.ast->createStruct(name);
    astStruct->tokenRange.firstToken = structToken;
    // astStruct->tokenRange.startIndex = startIndex;
    // astStruct->tokenRange.tokenStream = info.tokens;
    astStruct->polyName = name;
    astStruct->hidden = hideAnnotation;
    Token token = info.get(info.at()+1);
    if(Equal(token,"<")){
        info.next();
        // polymorphic type
        astStruct->polyName += "<";
    
        while(true){
            token = info.get(info.at()+1);
            if(Equal(token,">")){
                info.next();
                
                if(astStruct->polyArgs.size()==0){
                    ERR_HEAD2(token) << "empty polymorph list\n";
                    ERRLINE
                }
                break;
            }
            if(!IsName(token)){
                ERR_HEAD2(token) << token<<" is not a valid name for polymorphism\n";
                ERRLINE
            }else{
                info.next();
                ASTStruct::PolyArg arg={};
                arg.name = token;
                astStruct->polyArgs.push_back(arg);
                astStruct->polyName += token;
            }
            
            token = info.get(info.at()+1);
            if(Equal(token,",")){
                info.next();
                astStruct->polyName += ",";
                continue;
            }else if(Equal(token,">")){
                info.next();
                break;
            } else{
                ERR_HEAD2(token) << token<<"is neither , or >\n";
                ERRLINE
                continue;
            }
        }
        astStruct->polyName += ">";
    }
    
    token = info.get(info.at()+1);
    if(!Equal(token,"{")){
        ERR_HEAD2(token)<<"expected { not "<<token<<"\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next();
    
    // TODO: Structs need their own scopes for polymorphic types and
    //  internal namespaces, structs, enums...

    astStruct->scopeId = info.ast->createScope()->id;
    auto prevScope = info.currentScopeId;
    defer {info.currentScopeId = prevScope; };
    info.currentScopeId = astStruct->scopeId;

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
            // ,func->polyArgs.size()==0?&func->baseImpl:nullptr);
        } else {
            if(!IsName(name)){
                info.next();
                ERR_HEAD2(name) << "expected a name, "<<name<<" isn't\n";
                ERRLINE
                error = PARSE_ERROR;
                continue;
            }
            info.next();   
            Token token = info.get(info.at()+1);
            if(!Equal(token,":")){
                ERR_HEAD2(token)<<"expected : not "<<token<<"\n";
                ERRLINE
                error = PARSE_ERROR;
                continue;
            }    
            info.next();
            Token dtToken;
            int result = ParseTypeId(info,dtToken);
            if(result!=PARSE_SUCCESS) {
                ERR_HEAD2(dtToken) << "failed parsing type "<<dtToken<<"\n";
                ERRLINE;   
                continue;
            }
            int arrayLength = -1;
            Token token1 = info.get(info.at()+1);
            Token token2 = info.get(info.at()+2);
            Token token3 = info.get(info.at()+3);
            if(Equal(token1,"[") && Equal(token3,"]")) {
                info.next();
                info.next();
                info.next();

                if(IsInteger(token2)) {
                    arrayLength = ConvertInteger(token2);
                    if(arrayLength<0){
                        ERR_HEAD(token2, "Array cannot have negative size.\n\n";
                            ERR_LINE(info.at()-1,"< 0");
                        )
                        arrayLength = 0;
                    }
                } else {
                    ERR_HEAD(token2, "The length of an array can only be specified with number literals. Use macros to avoid magic numbers. Constants have not been implemented but when they have, they will work too.\n\n";
                        ERR_LINE(info.at()-1, "must be positive integer literal");
                    )
                }
                std::string* str = info.ast->createString();
                *str = "Slice<";
                *str += dtToken;
                *str += ">";
                dtToken = *str;
            }

            std::string temps = dtToken;
            
            TypeId typeId = info.ast->getTypeString(dtToken);
            
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
                mem.defaultValue = defaultValue;
                astStruct->baseImpl.members.push_back({});
                auto& mem2 = astStruct->baseImpl.members.back();
                mem2.typeId = typeId;
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
            ERR_HEAD2(token)<<"expected } or ; not "<<token<<"\n";
            ERRLINE
            error = PARSE_ERROR;
            continue;
        }
    }
    astStruct->tokenRange.endIndex = info.at()+1;
    _GLOG(log::out << "Parsed struct "<<log::LIME<< name <<log::SILVER << " with "<<astStruct->members.size()<<" members\n";)
    return error;
}
// int ParseStruct(ParseInfo& info, ASTStruct*& tempFunction, bool attempt);
int ParseEnum(ParseInfo& info, ASTEnum*& tempFunction, bool attempt);
int ParseNamespace(ParseInfo& info, ASTScope*& astNamespace, bool attempt){
    using namespace engone;
    MEASURE;
    _PLOG(FUNC_ENTER)
    Token token = info.get(info.at()+1);
    int startIndex = info.at()+1;
    if(!Equal(token,"namespace")){
        if(attempt)
            return PARSE_BAD_ATTEMPT;
        ERR_HEAD2(token)<<"expected namespace not "<<token<<"\n";
        ERRLINE
        return PARSE_ERROR;
    }
    attempt = false;
    info.next();
    bool hideAnnotation = false;

    Token name = info.get(info.at()+1);
    while (IsAnnotation(name)){
        info.next();
        if(Equal(name,"@hide")){
            hideAnnotation=true;
        } else {
            WARN_HEAD(name, "'"<< Token(name.str+1,name.length-1) << "' is not a known annotation for functions.\n\n";
                WARN_LINE(info.at(),"unknown");
            )
        }
        name = info.get(info.at()+1);
        continue;
    }

    if(!IsName(name)){
        ERR_HEAD2(name)<<"expected a name, "<<name<<" isn't\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next();

    token = info.get(info.at()+1);
    if(!Equal(token,"{")){
        ERR_HEAD2(token)<<"expected { not "<<token<<"\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next();
    
    int nextId=0;
    astNamespace = info.ast->createNamespace(name);
    astNamespace->tokenRange.firstToken = token;
    // astNamespace->tokenRange.startIndex = startIndex;
    // astNamespace->tokenRange.tokenStream = info.tokens;
    astNamespace->hidden = hideAnnotation;

    ScopeInfo* newScope = info.ast->createScope();
    ScopeId newScopeId = newScope->id;
    newScope->parent = info.currentScopeId;
    
    astNamespace->scopeId = newScopeId;
    info.ast->getScope(newScopeId)->name = name;
    
    info.ast->getScope(info.currentScopeId)->nameScopeMap[name] = newScope->id;
    
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
            ERR_HEAD2(token) << "Unexpected '"<<token<<"' (ParseNamespace)\n";
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
    MEASURE;
    _PLOG(FUNC_ENTER)
    Token enumToken = info.get(info.at()+1);
    int startIndex = info.at()+1;
    if(!Equal(enumToken,"enum")){
        if(attempt) return PARSE_BAD_ATTEMPT;
        ERR_HEAD2(enumToken)<<"expected struct not "<<enumToken<<"\n";
        ERRLINE
        return PARSE_ERROR;   
    }
    attempt=false;
    info.next(); // enum

    bool hideAnnotation = false;
    Token name = info.get(info.at()+1);

    while (IsAnnotation(name)){
        info.next();
        if(Equal(name,"@hide")){
            hideAnnotation=true;
        } else {
            WARN_HEAD(name, "'"<< Token(name.str+1,name.length-1) << "' is not a known annotation for functions.\n\n";
                WARN_LINE(info.at(),"unknown");
            )
        }
        name = info.get(info.at()+1);
        continue;
    }

    if(!IsName(name)){
        ERR_HEAD2(name)<<"expected a name, "<<name<<" isn't\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next(); // name
    
    Token token = info.get(info.at()+1);
    if(!Equal(token,"{")){
        ERR_HEAD2(token)<<"expected { not "<<token<<"\n";
        ERRLINE
        return PARSE_ERROR;
    }
    info.next();
    int nextId=0;
    astEnum = info.ast->createEnum(name);
    astEnum->tokenRange.firstToken = enumToken;
    // astEnum->tokenRange.startIndex = startIndex;
    // astEnum->tokenRange.tokenStream = info.tokens;
    astEnum->hidden = hideAnnotation;
    int error = PARSE_SUCCESS;
    while (true){
        Token name = info.get(info.at()+1);
        
        if(Equal(name,"}")){
            info.next();
            break;
        }
        
        if(!IsName(name)){
            info.next(); // move forward to prevent infinite loop
            ERR_HEAD2(name) << "expected a name, "<<name<<" isn't\n";
            ERRLINE
            error = PARSE_ERROR;
            continue;
        }
        info.next();   
        Token token = info.get(info.at()+1);
        if(Equal(token,"=")){
            info.next();
            token = info.get(info.at()+1);
            // TODO: Handle expressions
            if(IsInteger(token)){
                info.next();
                nextId = ConvertInteger(token);
                token = info.get(info.at()+1);
            } else if(IsHexadecimal(token)){
                info.next();
                nextId = ConvertHexadecimal(token);
                token = info.get(info.at()+1);
            } else{
                ERR_HEAD2(token) << token<<" is not an integer (i32)\n";
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
            ERR_HEAD2(token)<<"expected } or , not "<<token<<"\n";
            ERRLINE
            error = PARSE_ERROR;
            continue;
        }
    }
    astEnum->tokenRange.endIndex = info.at()+1;
    // auto typeInfo = info.ast->getTypeInfo(info.currentScopeId, name, false, true);
    // int strId = info.ast->getTypeString(name);
    // if(!typeInfo){
        // ERR_HEAD2(name) << name << " is taken\n";
        // info.ast->destroy(astEnum);
        // astEnum = 0;
        // return PARSE_ERROR;
    // }
    // typeInfo->astEnum = astEnum;
    // typeInfo->_size = 4; // i32
    _PLOG(log::out << "Parsed enum "<<log::LIME<< name <<log::SILVER <<" with "<<astEnum->members.size()<<" members\n";)
    return error;
}
// parses arguments and puts them into fncall->left
int ParseArguments(ParseInfo& info, ASTExpression* fncall, int* count){
    MEASURE;
    // ASTExpression* tail=fncall->left;
    // if(tail)
    //     while(tail->next) {
    //         tail = tail->next;
    //     }
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
            ERR_HEAD2(tok)<<"expected ',' to supply more arguments or ')' to end fncall (found "<<tok<<" instead)\n";
            ERR_END
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
            ERR_HEAD2(tok) << "expected named argument because of previous named argument "<<prevNamed << " at "<<prevNamed.line<<":"<<prevNamed.column<<"\n";
            ERRLINE
            ERR_END
            // return or continue could desync the parsing so don't do that.
        }

        ASTExpression* expr=0;
        int result = ParseExpression(info,expr,false);
        if(result!=PARSE_SUCCESS){
            // TODO: error message, parse other arguments instead of returning?
            return PARSE_ERROR;                         
        }
        if(named){
            expr->namedValue = (std::string*)engone::Allocate(sizeof(std::string));
            new(expr->namedValue)std::string(tok);
        }
        fncall->args->add(expr);
        // if(tail){
        //     tail->next = expr;
        // }else{
        //     fncall->left = expr;
        // }
        (*count)++;
        // tail = expr;
        expectComma = true;
    }
    return PARSE_SUCCESS;
}
int ParseExpression(ParseInfo& info, ASTExpression*& expression, bool attempt){
    using namespace engone;
    MEASURE;
    _PLOG(FUNC_ENTER)
    
    if(info.end()){
        if(attempt)
            return PARSE_BAD_ATTEMPT;
        ERR << "Sudden end\n";
        return PARSE_ERROR;
    }

    std::vector<ASTExpression*> values;
    std::vector<OperationType> ops;
    std::vector<OperationType> assignOps;
    // std::vector<OperationType> directOps; // ! & *
    // extra info for directOps
    std::vector<TypeId> castTypes;
    // extra info for directOps
    std::vector<Token> namespaceNames;
    defer { for(auto e : values) info.ast->destroy(e); };

    bool negativeNumber=false;
    bool expectOperator=false;
    while(true){
        int tokenIndex = info.at()+1;
        Token token = info.get(tokenIndex);
        
        OperationType opType = (OperationType)0;
        bool ending=false;
        
        if(expectOperator){
            int extraOpNext=0;
            if(Equal(token,".")){
                info.next();
                
                token = info.get(info.at()+1);
                if(!IsName(token)){
                    ERR_HEAD(token, "Expected a property name, not "<<token<<".\n\n";
                        ERR_LINE(token.tokenIndex,"bad");
                    )
                    continue;
                }
                info.next();

                Token tok = info.get(info.at()+1);
                std::string polyTypes="";
                if(Equal(tok,"<")){
                    info.next();
                    token.length++;
                    // polymorphic type or function
                    polyTypes += "<";
                    int depth = 1;
                    while(!info.end()){
                        tok = info.next();
                        polyTypes+=tok;
                        token.length+=tok.length;
                        if(Equal(tok,"<")){
                            depth++;
                        }else if(Equal(tok,">")){
                            depth--;
                        }
                        if(depth==0){
                            break;
                        }
                    }
                }

                int startToken=info.at(); // start of fncall
                tok = info.get(info.at()+1);
                if(Equal(tok, "(")){
                    info.next();
                    // fncall for strucy methods
                    ASTExpression* tmp = info.ast->createExpression(TypeId(AST_FNCALL));
                    tmp->name = token;
                    // tmp->name = (std::string*)engone::Allocate(sizeof(std::string));
                    // new(tmp->name)std::string(std::string(token)+polyTypes);
                    
                    tmp->args = (DynamicArray<ASTExpression*>*)engone::Allocate(sizeof(DynamicArray<ASTExpression*>));
                    new(tmp->args)DynamicArray<ASTExpression*>();

                    // Create "this" argument in methods
                    tmp->args->add(values.back());
                    values.pop_back();

                    tmp->boolValue = true; // indicicate fncall to struct method

                    // Parse the other arguments
                    int count = 0;
                    int result = ParseArguments(info, tmp, &count);
                    if(result!=PARSE_SUCCESS){
                        info.ast->destroy(tmp);
                        return result;
                    }

                    _PLOG(log::out << "Parsed call "<<count <<"\n";)
                    tmp->tokenRange.firstToken = token;
                    // tmp->tokenRange.startIndex = startToken;
                    tmp->tokenRange.endIndex = info.at()+1;
                    // tmp->tokenRange.tokenStream = info.tokens;

                    values.push_back(tmp);
                    continue;
                }
                if(polyTypes.length()!=0){
                    ERR_HEAD(info.now(),
                    "Polymorphic arguments indicates a method call put the parenthesis for it is missing. '"<<info.get(info.at()+1)<<"' is not '('.";
                    )
                }
                
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_MEMBER));
                tmp->name = token;
                // tmp->name = (std::string*)engone::Allocate(sizeof(std::string));
                // new(tmp->name)std::string(token);
                tmp->left = values.back();
                values.pop_back();
                tmp->tokenRange.firstToken = tmp->left->tokenRange.firstToken;
                // tmp->tokenRange.startIndex = tmp->left->tokenRange.startIndex;
                tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
                values.push_back(tmp);
                continue;
            } else if (Equal(token,"=")) {
                info.next();

                ops.push_back(AST_ASSIGN);
                assignOps.push_back((OperationType)0);

                _PLOG(log::out << "Operator "<<token<<"\n";)
            } else if((opType = IsAssignOp(token)) && Equal(info.get(info.at()+2),"=")) {
                info.next();
                info.next();

                ops.push_back(AST_ASSIGN);
                assignOps.push_back(opType);

                _PLOG(log::out << "Operator "<<token<<"\n";)
            } else if((opType = IsOp(token,extraOpNext))){

                // if(Equal(token,"*") && (info.now().flags&TOKEN_SUFFIX_LINE_FEED)){
                if(info.now().flags&TOKEN_SUFFIX_LINE_FEED){
                    WARN_HEAD(token, "'"<<token << "' is treated as a multiplication but perhaps you meant to do a dereference since the operation exists on a new line. "
                        "Separate with a semi-colon for dereference or put the multiplication on the same line to silence this message.\n\n";
                        WARN_LINE(info.at(), "semi-colon is recommended after statements");
                        WARN_LINE(info.at()+1, "should this be left and right operation");
                    )
                }
                info.next();
                while(extraOpNext--){
                    info.next();
                    // used for doing next on the extra arrows for bit shifts
                }

                ops.push_back(opType);

                _PLOG(log::out << "Operator "<<token<<"\n";)
            // } else if (Equal(token,"..")){
            //     info.next();
            //     attempt=false;

            //     ASTExpression* rightExpr=nullptr;
            //     int result = ParseExpression(info, rightExpr, false);
            //     if(result != PARSE_SUCCESS){
                    
            //     }

            //     ASTExpression* tmp = info.ast->createExpression(TypeId(AST_RANGE));
            //     tmp->tokenRange.firstToken = token;
            //     tmp->tokenRange.startIndex = values.back()->tokenRange.startIndex;
            //     tmp->tokenRange.endIndex = info.at()+1;
            //     tmp->tokenRange.tokenStream = info.tokens;

            //     tmp->left = values.back();
            //     values.pop_back();
            //     tmp->right = rightExpr;
            //     values.push_back(tmp);
            //     continue;
            } else if(Equal(token,"[")){
                int startIndex = info.at()+1;
                info.next();
                attempt=false;
                ASTExpression* indexExpr=nullptr;
                int result = ParseExpression(info, indexExpr, false);
                if(result != PARSE_SUCCESS){
                    
                }
                Token tok = info.get(info.at()+1);
                if(!Equal(tok,"]")){
                    ERR_HEAD(tok,"bad\n\n";)
                } else {
                    info.next();
                }

                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_INDEX));
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = startIndex;
                tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;

                tmp->left = values.back();
                values.pop_back();
                tmp->right = indexExpr;
                values.push_back(tmp);
                continue;
            } else if(Equal(token,"++")){
                info.next();
                attempt = false;

                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_INCREMENT));
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
                tmp->boolValue = true;

                tmp->left = values.back();
                values.pop_back();
                values.push_back(tmp);

                continue;
            } else if(Equal(token,"--")){
                info.next();
                attempt = false;

                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_DECREMENT));
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
                tmp->boolValue = true;

                tmp->left = values.back();
                values.pop_back();
                values.push_back(tmp);

                continue;
            } else if(Equal(token,")")){
                // token = info.next();
                ending = true;
            } else {
                ending = true;
                if(Equal(token,";")){
                    info.next();
                } else {
                    Token prev = info.now();
                    // Token next = info.get(info.at()+2);
                    if((prev.flags&TOKEN_SUFFIX_LINE_FEED) == 0 
                         && !Equal(token,"}") && !Equal(token,",") && !Equal(token,"{") && !Equal(token,"]")
                         && !Equal(prev,")")){
                        WARN_HEAD(token, "Did you forget the semi-colon to end the statement or was it intentional? Perhaps you mistyped a character? (put the next statement on a new line to silence this warning)\n\n"; 
                            ERR_LINE(tokenIndex-1, "semi-colon here?");
                            // ERR_LINE(tokenIndex, "; before this?");
                            )
                        // TODO: ERROR instead of warning if special flag is set
                    }
                }
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
                    ERR_HEAD2(tok) << "expected < not "<<tok<<"\n";
                    ERRLINE
                    continue;
                }
                info.next();
                Token tokenTypeId{};
                int result = ParseTypeId(info,tokenTypeId);
                if(result!=PARSE_SUCCESS){
                    ERR_HEAD2(tokenTypeId) << tokenTypeId << "is not a valid data type\n";
                    ERRLINE
                }
                tok = info.get(info.at()+1);
                if(!Equal(tok,">")){
                    ERR_HEAD2(tok) << "expected > not "<< tok<<"\n";
                    ERRLINE
                }
                info.next();
                ops.push_back(AST_CAST);
                // TypeId dt = info.ast->getTypeInfo(info.currentScopeId,tokenTypeId)->id;
                // castTypes.push_back(dt);
                TypeId strId = info.ast->getTypeString(tokenTypeId);
                castTypes.push_back(strId);
                attempt=false;
                continue;
            } else if(Equal(token,"-")){
                info.next();
                negativeNumber=true;
                attempt = false;
                continue;
            } else if(Equal(token,"++")){
                info.next();
                ops.push_back(AST_INCREMENT);

                attempt = false;
                continue;
            } else if(Equal(token,"--")){
                info.next();
                ops.push_back(AST_DECREMENT);

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
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(IsDecimal(token)){
                token = info.next();
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_FLOAT32));
                tmp->f32Value = ConvertDecimal(token);
                if(negativeNumber)
                    tmp->f32Value = -tmp->f32Value;
                negativeNumber=false;
                values.push_back(tmp);
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(IsHexadecimal(token)){
                info.next();
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_UINT32));
                tmp->i64Value =  ConvertHexadecimal(token); // TODO: Only works with 32 bit or 16,8 I suppose.
                values.push_back(tmp);
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(token.flags&TOKEN_QUOTED) {
                info.next();
                
                ASTExpression* tmp = 0;
                // if(token.length == 1){
                if(0==(token.flags&TOKEN_DOUBLE_QUOTED)){
                    tmp = info.ast->createExpression(TypeId(AST_CHAR));
                    tmp->charValue = *token.str;
                }else {
                    tmp = info.ast->createExpression(TypeId(AST_STRING));
                    tmp->name = token;
                    // tmp->name = (std::string*)engone::Allocate(sizeof(std::string));
                    // new(tmp->name)std::string(token);

                    // A string of zero size can be useful which is why
                    // the code below is commented out.
                    // if(token.length == 0){
                        // ERR_HEAD2(token) << "string should not have a length of 0\n";
                        // This is a semantic error and since the syntax is correct
                        // we don't need to return PARSE_ERROR. We could but things will be fine.
                    // }
                }
                values.push_back(tmp);
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(Equal(token,"true") || Equal(token,"false")){
                info.next();
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_BOOL));
                tmp->boolValue = Equal(token,"true");
                values.push_back(tmp);
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                
                // tmp->tokenRange.tokenStream = info.tokens;
            } 
            // else if(Equal(token,"[")) {
            //     info.next();

            //     ASTExpression* tmp = info.ast->createExpression(TypeId(AST_SLICE_INITIALIZER));

            //     // TODO: what about slice type. u16[] for example.
            //     //  you can do just [0,21] and it will auto assign a type in the generator
            //     //  but we may want to specify one.
 
            //     while(true){
            //         Token token = info.get(info.at()+1);
            //         if(Equal(token,"]")){
            //             info.next();
            //             break;
            //         }
            //         ASTExpression* expr;
            //         int result = ParseExpression(info,expr,false);
            //         // TODO: deal with result
            //         if(result==PARSE_SUCCESS){
            //             // NOTE: The list will be ordered in reverse.
            //             //   The generator wants this when pushing values to the stack.
            //             expr->next = tmp->left;
            //             tmp->left = expr;
            //         }
                    
            //         token = info.get(info.at()+1);
            //         if(Equal(token,",")){
            //             info.next();
            //             continue;
            //         }else if(Equal(token,"]")){
            //             info.next();
            //             break;
            //         }else {
            //             ERR_HEAD2(token) << "expected ] not "<<token<<"\n";
            //             continue;
            //         }
            //     }
                
            //     values.push_back(tmp);
            //     tmp->tokenRange.firstToken = token;
            //     tmp->tokenRange.startIndex = info.at();
            //     tmp->tokenRange.endIndex = info.at()+1;
            //     tmp->tokenRange.tokenStream = info.tokens;
            // } 
            else if(Equal(token,"null")){
                token = info.next();
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_NULL));
                values.push_back(tmp);
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(Equal(token,"sizeof")) {
                info.next();
                Token token = {}; 
                int result = ParseTypeId(info,token);
                // Token token = info.get(info.at()+1);
                // if(!IsName(token)){
                //     ERR_HEAD2(token) << "sizeof expects a single name token like 'SomeStruct123'. '"<<token<<"' is not valid.\n";
                //     ERRLINE
                //     return PARSE_ERROR;
                // }
                // info.next();
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_SIZEOF));
                // tmp->name = (std::string*)engone::Allocate(sizeof(std::string));
                // new(tmp->name)std::string(token);
                tmp->name = token;
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
                values.push_back(tmp);
            } else if(IsName(token)){
                info.next();
                int startToken=info.at();
                
                Token polyName{};
                polyName = token;
                // could be a slice if tok[]{}
                // if something is within [] then it's a array access
                // polytypes exist for struct initializer and function calls
                Token tok = info.get(info.at()+1);
                std::string polyTypes="";
                // TODO: func<i32>() and i < 5 has ambiguity when
                //  parsing. Currently using space to differentiate them.
                //  Checking for <, > and ( for functions could work but
                //  i < 5 may have something similar in some cases.
                //  Checking for that could take a long time since
                //  it could be hard to know when to stop.
                //  ParseTypeId has some logic for it so things are possible
                //  it's just hard.
                if(Equal(tok,"<") && !(tok.flags&TOKEN_SUFFIX_SPACE)){
                    info.next();
                    // polymorphic type or function
                    polyTypes += "<";
                    polyName.length++;
                    int depth = 1;
                    while(!info.end()){
                        tok = info.next();
                        polyTypes+=tok;
                        polyName.length+=tok.length;
                        if(Equal(tok,"<")){
                            depth++;
                        }else if(Equal(tok,">")){
                            depth--;
                        }
                        if(depth==0){
                            break;
                        }
                    }
                }
                tok = info.get(info.at()+1);
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
                    ns += polyTypes;

                    ASTExpression* tmp = info.ast->createExpression(TypeId(AST_FNCALL));
                    // tmp->name = (std::string*)engone::Allocate(sizeof(std::string));
                    // new(tmp->name)std::string(std::move(ns));
                    tmp->name = polyName;
                    tmp->args = (DynamicArray<ASTExpression*>*)engone::Allocate(sizeof(DynamicArray<ASTExpression*>));
                    new(tmp->args)DynamicArray<ASTExpression*>();

                    int count = 0;
                    int result = ParseArguments(info, tmp, &count);
                    if(result!=PARSE_SUCCESS){
                        info.ast->destroy(tmp);
                        return result;
                    }

                    _PLOG(log::out << "Parsed call "<<count <<"\n";)
                    values.push_back(tmp);
                    tmp->tokenRange.firstToken = token;
                    // tmp->tokenRange.startIndex = startToken;
                    tmp->tokenRange.endIndex = info.at()+1;
                    // tmp->tokenRange.tokenStream = info.tokens;
                }else if(Equal(tok,"{") && 0 == (token.flags & TOKEN_SUFFIX_SPACE)){ // Struct {} is ignored
                    // initializer
                    info.next();
                    ASTExpression* initExpr = info.ast->createExpression(TypeId(AST_INITIALIZER));
                    initExpr->args = (DynamicArray<ASTExpression*>*)engone::Allocate(sizeof(DynamicArray<ASTExpression*>));
                    new(initExpr->args)DynamicArray<ASTExpression*>();
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
                    ns += polyTypes;
                    std::string* nsp = info.ast->createString();
                    *nsp = ns;
                    initExpr->castType = info.ast->getTypeString(*nsp);
                    // TODO: A little odd to use castType. Renaming castType to something more
                    //  generic which works for AST_CAST and AST_INITIALIZER would be better.
                    // ASTExpression* tail=0;
                    
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
                            ERR_HEAD2(token) << "expected named argument because of previous named argument "<<prevNamed << " at "<<prevNamed.line<<":"<<prevNamed.column<<"\n";
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
                            expr->namedValue = (std::string*)engone::Allocate(sizeof(std::string));
                            new(expr->namedValue)std::string(token);
                        }
                        initExpr->args->add(expr);
                        // if(tail){
                        //     tail->next = expr;
                        // }else{
                        //     tmp->left = expr;
                        // }
                        count++;
                        // tail = expr;
                        
                        token = info.get(info.at()+1);
                        if(Equal(token,",")){
                            info.next();
                            continue;
                        }else if(Equal(token,"}")){
                           info.next();
                           break;
                        } else {
                            ERR_HEAD2(token) << "expected , or } not "<<token<<"\n";
                            ERRLINE
                            continue;
                        }
                    }
                    // log::out << "Parse initializer "<<count<<"\n";
                    values.push_back(initExpr);
                    initExpr->tokenRange.firstToken = token;
                    // initExpr->tokenRange.startIndex = startToken;
                    initExpr->tokenRange.endIndex = info.at()+1;
                    // initExpr->tokenRange.tokenStream = info.tokens;
                }else {
                    if(polyTypes.size()!=0){
                        ERR_HEAD(token, "Polymorphic types not expected with namespace or variable.\n\n";
                            ERR_LINE(token.tokenIndex,"bad");
                        )
                        continue;
                    }
                    if(Equal(tok,"::")){
                        info.next();
                        
                        // ERR_HEAD2(tok) << " :: is not implemented\n";
                        // ERRLINE
                        // continue; 
                        
                        Token tok = info.get(info.at()+1);
                        if(!IsName(tok)){
                            ERR_HEAD2(tok) << tok<<" is not a name\n";
                            ERRLINE
                            continue;
                        }
                        ops.push_back(AST_FROM_NAMESPACE);
                        namespaceNames.push_back(token);
                        // namespaceToken = token;
                        continue; // do a second round?
                        
                        // TODO: detect more ::
                        
                        // ASTExpression* tmp = info.ast->createExpression(AST_FROM_NAMESPACE);
                        
                        // values.push_back(tmp);
                        // tmp->tokenRange.firstToken = token;
                        // tmp->tokenRange.startIndex = startToken;
                        // tmp->tokenRange.endIndex = info.at()+1;
                        // tmp->tokenRange.tokenStream = info.tokens;
                    } else{
                        ASTExpression* tmp = info.ast->createExpression(TypeId(AST_VAR));

                        Token nsToken = token;
                        int nsOps = 0;
                        while(ops.size()>0){
                            OperationType op = ops.back();
                            if(op == AST_FROM_NAMESPACE) {
                                nsOps++;
                                // ns += namespaceNames.back();
                                // ns += "::";
                                // namespaceNames.pop_back();
                                ops.pop_back();
                            } else {
                                break;
                            }
                        }
                        for(int i=0;i<nsOps;i++){
                            Token& tok = namespaceNames.back();
                            if(i==0){
                                nsToken = tok;
                            } else {
                                nsToken.length+=tok.length;
                            }
                            nsToken.length+=2; // colons
                            namespaceNames.pop_back();
                        }
                        if(nsOps!=0){
                            nsToken.length += token.length;
                        }
                        // std::string ns = "";
                        // while(ops.size()>0){
                        //     OperationType op = ops.back();
                        //     if(op == AST_FROM_NAMESPACE) {
                        //         ns += namespaceNames.back();
                        //         ns += "::";
                        //         namespaceNames.pop_back();
                        //         ops.pop_back();
                        //     } else {
                        //         break;
                        //     }
                        // }
                        // ns += token;

                        tmp->name = nsToken;

                        // tmp->name = (std::string*)engone::Allocate(sizeof(std::string));
                        // new(tmp->name)std::string(std::move(ns));

                        values.push_back(tmp);
                        tmp->tokenRange.firstToken = token;
                        // tmp->tokenRange.startIndex = startToken;
                        tmp->tokenRange.endIndex = info.at()+1;
                        // tmp->tokenRange.tokenStream = info.tokens;
                    }
                }
            } else if(Equal(token,"(")){
                // parse again
                info.next();
                ASTExpression* tmp=0;
                int result = ParseExpression(info,tmp,false);
                if(result!=PARSE_SUCCESS)
                    return PARSE_ERROR;
                if(!tmp){
                    ERR_HEAD2(token) << "got nothing from paranthesis in expression\n";
                }
                token = info.get(info.at()+1);
                if(!Equal(token,")")){
                    ERR_HEAD2(token) << "expected ) not "<<token<<"\n";   
                }else
                    info.next();
                values.push_back(tmp);  
            } else {
                if(attempt){
                    return PARSE_BAD_ATTEMPT;
                }else{
                    ERR_HEAD(token, "'"<<token << "' is not a value. Values (or expressions) are expected after tokens that calls upon arguments, operations and assignments among other things.\n";
                    ERR_LINE(tokenIndex,"should be a value");
                    ERR_LINE(tokenIndex-1,"expects a value afterwards")
                    );
                    // ERR_LINE()
                    // printLine();
                    return PARSE_ERROR;
                    // Qutting here is okay because we have a defer which
                    // destroys parsed expressions. No memory leaks!
                }
            }
        }
        if(negativeNumber){
            ERR_HEAD2(info.get(info.at()-1)) << "Unexpected - before "<<token<<"\n";
            ERRLINE
            return PARSE_ERROR;
            // quitting here is a little unexpected but there is
            // a defer which destroys parsed expresions so no memory leaks at least.
        }
        
        ending = ending || info.end();

        while(values.size()>0){
            OperationType nowOp = (OperationType)0;
            if(ops.size()>=2&&!ending){
                OperationType op1 = ops[ops.size()-2];
                if(!IsSingleOp(op1) && values.size()<2)
                    break;
                OperationType op2 = ops[ops.size()-1];
                // if(OpPrecedence(op1)>=OpPrecedence(op2)){
                if(OpPrecedence(op1)>OpPrecedence(op2)){
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
            // val->tokenRange.tokenStream = info.tokens;
            // FROM_NAMESPACE, CAST, REFER, DEREF, NOT, BNOT
            if(IsSingleOp(nowOp)){
                // TODO: tokenRange is not properly set!
                val->tokenRange.firstToken = er->tokenRange.firstToken;
                // val->tokenRange = er->tokenRange;
                if(nowOp == AST_FROM_NAMESPACE){
                    Token& tok = namespaceNames.back();
                    val->name = tok;
                    // val->name = (std::string*)engone::Allocate(sizeof(std::string));
                    // new(val->name)std::string(tok);
                    val->tokenRange.firstToken = tok;
                    // val->tokenRange.startIndex -= 2; // -{namespace name} -{::}
                    namespaceNames.pop_back();
                } else if(nowOp == AST_CAST){
                    val->castType = castTypes.back();
                    castTypes.pop_back();
                } else {
                    // val->tokenRange.startIndex--;
                }
                val->left = er;
            } else if(values.size()>0){
                auto el = values.back();
                values.pop_back();
                val->tokenRange.firstToken = el->tokenRange.firstToken;
                // val->tokenRange.startIndex = el->tokenRange.startIndex;
                val->tokenRange.endIndex = er->tokenRange.endIndex;
                val->left = el;
                val->right = er;

                if(nowOp==AST_ASSIGN){
                    Assert(assignOps.size()>0);
                    val->castType = assignOps.back();
                    assignOps.pop_back();
                }
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
    MEASURE;
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
        // statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        // statement->tokenRange.tokenStream = info.tokens;
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
        // statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        // statement->tokenRange.tokenStream = info.tokens;
        return PARSE_SUCCESS;
    } else if(Equal(firstToken,"for")){
        info.next();
        
        Token token = info.get(info.at()+1);
        bool reverseAnnotation = false;
        bool pointerAnnot = false;
        while (IsAnnotation(token)){
            info.next();
            if(Equal(token,"@reverse")){
                reverseAnnotation=true;
            } else if(Equal(token,"@pointer")){
                pointerAnnot=true;
            } else {
                WARN_HEAD(token, "'"<< Token(token.str+1,token.length-1) << "' is not a known annotation for functions.\n\n";
                    WARN_LINE(info.at(),"unknown");
                )
            }
            token = info.get(info.at()+1);
            continue;
        }

        // NOTE: assuming array iteration
        Token varname = info.get(info.at()+1);
        Token colon = info.get(info.at()+2);
        if(Equal(colon,":") && IsName(varname)){
            info.next();
            info.next();
        } else {
            std::string* str = info.ast->createString();
            *str = "it";
            varname = *str;
        }
        
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
        statement = info.ast->createStatement(ASTStatement::FOR);
        statement->varnames.push_back({varname});
        statement->reverse = reverseAnnotation;
        statement->pointer = pointerAnnot;
        statement->rvalue = expr;
        statement->body = body;
        statement->tokenRange.firstToken = firstToken;
        // statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        // statement->tokenRange.tokenStream = info.tokens;
        return PARSE_SUCCESS;
    } else if(Equal(firstToken,"return")){
        info.next();
        
        ASTStatement* statement = info.ast->createStatement(ASTStatement::RETURN);
        statement->returnValues = (std::vector<ASTExpression*>*)engone::Allocate(sizeof(std::vector<ASTExpression*>));
        new(statement->returnValues)std::vector<ASTExpression*>();

        while(true){
            ASTExpression* expr=0;
            Token token = info.get(info.at()+1);
            if(Equal(token,";")&&statement->returnValues->size()==0){ // return 1,; should not be allowed, that's why we do &&!base
                info.next();
                break;
            }
            int result = ParseExpression(info,expr,true);
            if(result==PARSE_BAD_ATTEMPT){
                break;
            }
            if(result!=PARSE_SUCCESS){
                break;
            }
            statement->returnValues->push_back(expr);
            // if(!base){
            //     base = expr;
            //     prev = expr;
            // }else{
            //     prev->next = expr;
            //     prev = expr;
            // }
            
            Token tok = info.get(info.at()+1);
            if(!Equal(tok,",")){
                if(Equal(tok,";"))
                    info.next();
                break;   
            }
            info.next();
        }
        
        // statement->rvalue = base;
        statement->tokenRange.firstToken = firstToken;
        // statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        // statement->tokenRange.tokenStream = info.tokens;
        return PARSE_SUCCESS;
    } else if(Equal(firstToken,"using")){
        info.next();

        // variable name
        // namespacing
        // type with polymorphism. not pointers
        // TODO: namespace environemnt, not just using X as Y

        Token originToken={};
        int result = ParseTypeId(info, originToken);
        Token aliasToken = {};
        Token token = info.get(info.at()+1);
        if(Equal(token,"as")) {
            info.next();
            
            int result = ParseTypeId(info, aliasToken);
        }
        
        statement = info.ast->createStatement(ASTStatement::USING);
        
        statement->varnames.push_back({originToken});
        if(aliasToken.str){
            statement->alias = (std::string*)engone::Allocate(sizeof(std::string));
            new(statement->alias)std::string(aliasToken);
        }

        statement->tokenRange.firstToken = firstToken;
        // statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        // statement->tokenRange.tokenStream = info.tokens;
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
        // statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        // statement->tokenRange.tokenStream = info.tokens;
        return PARSE_SUCCESS;  
    } else if(Equal(firstToken, "break")) {
        info.next();

        statement = info.ast->createStatement(ASTStatement::BREAK);

        statement->tokenRange.firstToken = firstToken;
        // statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        // statement->tokenRange.tokenStream = info.tokens;
        return PARSE_SUCCESS;
    } else if(Equal(firstToken, "continue")) {
        info.next();

        statement = info.ast->createStatement(ASTStatement::CONTINUE);

        statement->tokenRange.firstToken = firstToken;
        // statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        // statement->tokenRange.tokenStream = info.tokens;
        return PARSE_SUCCESS;  
    }
    if(attempt)
        return PARSE_BAD_ATTEMPT;
    return PARSE_ERROR;
}
// out token contains a newly allocated string. use delete[] on it
int ParseFunction(ParseInfo& info, ASTFunction*& function, bool attempt, ASTStruct* parentStruct){
    using namespace engone;
    MEASURE;
    _PLOG(FUNC_ENTER)
    Token& token = info.get(info.at()+1);
    int startIndex = info.at()+1;
    if(!Equal(token,"fn")){
        if(attempt) return PARSE_BAD_ATTEMPT;
        ERR_HEAD2(token) << "expected fn for function not "<<token<<"\n";
            ERR_END
        return PARSE_ERROR;
    }
    info.next();
    attempt = false;

    bool hideAnnotation = false;

    Token name = info.next();
    while (IsAnnotation(name)){
        if(Equal(name,"@hide")){
                hideAnnotation=true;
        // } else if(Equal(name,"@export")) {
        } else {
            WARN_HEAD(name, "'"<< Token(name.str+1,name.length-1) << "' is not a known annotation for functions.\n\n";
                WARN_LINE(info.at(),"unknown");
            )
        }
        name = info.next();
        continue;
    }
    
    if(!IsName(name)){
        ERR_HEAD(name,"expected a valid name, "<<name<<" is not.\n";)
        return PARSE_ERROR;
    }
    
    function = info.ast->createFunction(name);
    function->tokenRange.firstToken = token;
    // function->tokenRange.startIndex = startIndex;
    // function->tokenRange.tokenStream = info.tokens;
    // function->baseImpl.name = name;
    function->hidden = hideAnnotation;

    ScopeInfo* funcScope = info.ast->createScope();
    function->scopeId = funcScope->id;
    funcScope->parent = info.currentScopeId;
    auto prevScope = info.currentScopeId;
    defer { info.currentScopeId = prevScope; };
    info.currentScopeId = function->scopeId;

    Token tok = info.get(info.at()+1);
    if(Equal(tok,"<")){
        info.next();
        while(!info.end()){
            tok = info.get(info.at()+1);
            if(Equal(tok,">")){
                info.next();
                break;
            }
            int result = ParseTypeId(info, tok);
            if(result == PARSE_SUCCESS) {
                function->polyArgs.add({tok});
            }

            tok = info.get(info.at()+1);
            if (Equal(tok,",")) {
                info.next();
                continue;
            } else if(Equal(tok,">")) {
                info.next();
                break;
            } else {
                ERR_HEAD2(tok) << "expected , or > for in poly. arguments for function "<<name<<"\n";
            ERR_END
                // parse error or what?
                break;
            }
        }
    }
    tok = info.next();
    if(!Equal(tok,"(")){
        ERR_HEAD2(tok) << "expected ( not "<<tok<<"\n";
            ERR_END
        return PARSE_ERROR;
    }

    if(parentStruct) {
        function->arguments.add({});
        auto& argv = function->arguments[function->arguments.size()-1];
        argv.name = "this";
        argv.defaultValue = nullptr;

        std::string* str = info.ast->createString();
        *str = parentStruct->polyName + "*"; // TODO: doesn't work with polymorhpism
        argv.stringType = info.ast->getTypeString(*str);;
    }
    bool printedErrors=false;
    bool mustHaveDefault=false; 
    TokenRange prevDefault={};
    while(true){
        Token& arg = info.get(info.at()+1);
        if(Equal(arg,")")){
            info.next();
            break;
        }
        if(!IsName(arg)){
            info.next();
            if(!printedErrors) {
                printedErrors=true;
                ERR_HEAD2(arg) << arg <<" is not a valid argument name\n";
                ERRLINE
                ERR_END
            }
            continue;
            // return PARSE_ERROR;
        }
        info.next();
        tok = info.get(info.at()+1);
        if(!Equal(tok,":")){
            if(!printedErrors) {
                printedErrors=true;
                ERR_HEAD2(tok) << "expected : not "<<tok <<"\n";
                ERRLINE
                ERR_END
            }
            continue;
            // return PARSE_ERROR;
        }
        info.next();

        Token dataType{};
        int result = ParseTypeId(info,dataType);
        
        // auto id = info.ast->getTypeInfo(info.currentScopeId,dataType)->id;
        TypeId strId = info.ast->getTypeString(dataType);

        function->arguments.add({});
        auto& argv = function->arguments[function->arguments.size()-1];
        argv.name = arg; // add the argument even if default argument fails
        argv.stringType = strId;

        ASTExpression* defaultValue=nullptr;
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
            // printedErrors doesn't matter here.
            // If we end up here than our parsing is probably correct so far and we might as
            // well log this error since it's probably a "real" error not caused by a cascade.
            ERR_HEAD2(tok) << "expected a default argument because of previous default argument "<<prevDefault<<" at "<<prevDefault.firstToken.line<<":"<<prevDefault.firstToken.column<<"\n";
            ERRLINE
            ERR_END
            // continue; we don't continue since we want to parse comma if it exists
        }

        argv.defaultValue = defaultValue;
        printedErrors = false; // we succesfully parsed this so we good?

        tok = info.get(info.at()+1);
        if(Equal(tok,",")){
            info.next();
            continue;
        }else if(Equal(tok,")")){
            info.next();
            break;
        }else{
            printedErrors = true; // we bad and might keep being bad.
            // don't do printed errors?
            ERR_HEAD2(tok) << "expected , or ) not "<<tok <<"\n";
            ERRLINE
            ERR_END
            continue;
            // Continuing since we saw ( and are inside of arguments.
            // we must find ) to leave.
            // return PARSE_ERROR;
        }
    }
    // TODO: check token out of bounds
    printedErrors=false;
    tok = info.get(info.at()+1);
    Token tok2 = info.get(info.at()+2);
    if(Equal(tok,"-") && !(tok.flags&TOKEN_SUFFIX_SPACE) && Equal(tok2,">")){
        info.next();
        info.next();
        tok = info.get(info.at()+1);
        while(true){
            Token tok = info.get(info.at()+1);
            if(Equal(tok,"{")){
                break;   
            }
            
            Token dt{};
            int result = ParseTypeId(info,dt);
            if(result!=PARSE_SUCCESS){
                // continue as we have
                // continue;
                // return PARSE_ERROR;
                // printedErrors = false;
            } else {
                TypeId strId = info.ast->getTypeString(dt);
                function->returnTypes.add(strId);
                printedErrors = true;
            }
            tok = info.get(info.at()+1);
            if(Equal(tok,"{")){
                // info.next(); { is parsed in ParseBody
                break;   
            } else if(Equal(tok,",")){
                info.next();
                continue;
            } else {
                if(!printedErrors){
                    printedErrors=false;
                    ERR_HEAD2(tok) << "expected , or { not "<<tok <<"\n";
                    ERRLINE
                    ERR_END
                }
                continue;
                // Continuing since we are inside of return values and expect
                // something to end it
            }
        }
    }
    
    info.functionScopes.push_back({});
    ASTScope* body = 0;
    int result = ParseBody(info,body);
    
    info.functionScopes.pop_back();

    function->body = body;
    function->tokenRange.endIndex = info.at()+1;

    return PARSE_SUCCESS;
}
// normal assignment a = 9
int ParseAssignment(ParseInfo& info, ASTStatement*& statement, bool attempt){
    using namespace engone;
    MEASURE;
    _PLOG(FUNC_ENTER)
    
    // if(info.tokens->length() < info.index+2){
    //     // not enough tokens for assignment
    //     if(attempt){
    //         return PARSE_BAD_ATTEMPT;
    //     }
    //     ERR_HEAD2()
    //     _PLOG(log::out <<log::RED<< "CompilerError: to few tokens for assignment\n";)
    //     // info.nextLine();
    //     return PARSE_ERROR;
    // }
    int error=PARSE_SUCCESS;

    // to make things easier attempt is checked here
    if(attempt && !(IsName(info.get(info.at()+1)) && (Equal(info.get(info.at()+2),",") || Equal(info.get(info.at()+2),":") || Equal(info.get(info.at()+2),"=") ))){
        return PARSE_BAD_ATTEMPT;
    }
    attempt = true;

    int startIndex = info.at()+1;
    std::vector<ASTStatement::VarName> varnames;
    bool assigned = false;
    TypeId lastType = {};
    while(!info.end()){
        Token name = info.get(info.at()+1);
        if(!IsName(name)){
            ERR_HEAD(name, "Expected a valid name for assignment ('"<<name<<"' is not).\n\n";
                ERR_LINE(info.at()+1,"cannot be a name");
            )
            return PARSE_ERROR;
        }
        info.next();

        varnames.push_back({name});
        Token token = info.get(info.at()+1);
        if(Equal(token,":")){
            assigned = true;
            info.next(); // :
            attempt=false;
            
            Token typeToken{};
            int result = ParseTypeId(info,typeToken);
            // if(result!=PARSE_SUCCESS)
            //     return PARSE_ERROR;
            int arrayLength = -1;
            Token token1 = info.get(info.at()+1);
            Token token2 = info.get(info.at()+2);
            Token token3 = info.get(info.at()+3);
            if(Equal(token1,"[") && Equal(token3,"]")) {
                info.next();
                info.next();
                info.next();

                if(IsInteger(token2)) {
                    arrayLength = ConvertInteger(token2);
                    if(arrayLength<0){
                        ERR_HEAD(token2, "Array cannot have negative size.\n\n";
                            ERR_LINE(info.at()-1,"< 0");
                        )
                        arrayLength = 0;
                    }
                } else {
                    ERR_HEAD(token2, "The length of an array can only be specified with number literals. Use macros to avoid magic numbers. Constants have not been implemented but when they have, they will work too.\n\n";
                        ERR_LINE(info.at()-1, "must be positive integer literal");
                    )
                }
                std::string* str = info.ast->createString();
                *str = "Slice<";
                *str += typeToken;
                *str += ">";
                typeToken = *str;
            }
            
            
            TypeId strId = info.ast->getTypeString(typeToken);

            int index = varnames.size()-1;
            while(index>=0 && !varnames[index].assignType.isValid()){
                varnames[index].assignType = strId;
                varnames[index].arrayLength = arrayLength;
                index--;
            }
        }
        token = info.get(info.at()+1);
        if(Equal(token, ",")){
            info.next();
            continue;
        }
        break;
    }

    Token token = info.get(info.at()+1);
    if(Equal(token,";") || info.end()){
        info.next(); // ;
        statement = info.ast->createStatement(ASTStatement::ASSIGN);

        statement->varnames = std::move(varnames);

        statement->rvalue = nullptr;
        
        statement->tokenRange.firstToken = info.get(startIndex);
        // statement->tokenRange.startIndex = startIndex;
        statement->tokenRange.endIndex = info.at()+1;
        // statement->tokenRange.tokenStream = info.tokens;
        return PARSE_SUCCESS;
    }else if(!Equal(token,"=")){
        ERR_HEAD2(token) << "Expected = not "<<token<<"\n";
        ERRLINE;
        return PARSE_ERROR;
    }
    info.next(); // =

    statement = info.ast->createStatement(ASTStatement::ASSIGN);
    statement->varnames = std::move(varnames);
    
    ASTExpression* rvalue=nullptr;
    int result = 0;
    result = ParseExpression(info,rvalue,false);

    // if(result!=PARSE_SUCCESS){
    //     return PARSE_ERROR;
    // }

    statement->rvalue = rvalue;
    
    statement->tokenRange.firstToken = info.get(startIndex);
    // statement->tokenRange.startIndex = startIndex;
    statement->tokenRange.endIndex = info.at()+1;
    // statement->tokenRange.tokenStream = info.tokens;
        
    return error;   
}
int ParseBody(ParseInfo& info, ASTScope*& bodyLoc, bool globalScope){
    using namespace engone;
    MEASURE;
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
        // bodyLoc->tokenRange.startIndex = info.at()+1;
        // bodyLoc->tokenRange.tokenStream = info.tokens;
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
        info.currentScopeId = info.ast->createScope()->id;
        ScopeInfo* si = info.ast->getScope(info.currentScopeId);
        si->parent = prevScope;

    }
    bodyLoc->scopeId = info.currentScopeId;

    // ASTStatement* latestDefer = nullptr; // latest parsed defer
    std::vector<ASTStatement*> nearDefers;
    
    while(!info.end()){
        Token& token = info.get(info.at()+1);
        if(scoped && Equal(token,"}")){
            info.next();
            break;
        }
        if(Equal(token,";")){
            info.next();
            continue;
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
            tempStatement->tokenRange = body->tokenRange;
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
            ASTExpression* expr=nullptr;
            result = ParseExpression(info,expr,true);
            if(expr){
                tempStatement = info.ast->createStatement(ASTStatement::EXPRESSION);
                tempStatement->rvalue = expr;
                tempStatement->tokenRange = expr->tokenRange;
            }
        }

        if(result==PARSE_BAD_ATTEMPT){
            if(IsAnnotation(token)){
                if(Equal(token,"@native-code")){
                    bodyLoc->nativeCode = true;
                } else {
                    WARN_HEAD(token, "'"<< Token(token.str+1,token.length-1) << "' is not a known annotation for bodies.\n\n";
                        WARN_LINE(info.at()+1,"unknown");
                    )
                }
                info.next();
            } else {
                Token& token = info.get(info.at()+1);
                ERR_HEAD(token, "Unexpected '"<<token<<"' (ParseBody).\n\n";
                    ERR_LINE(info.at()+1,"what");
                )
                // prevent infinite loop. Loop only occurs when scoped
                info.next();
            }
        }
        if(result==PARSE_ERROR){
            
        }
        // We add the AST structures even during error to
        // avoid leaking memory.
        if(tempStatement) {
            if(tempStatement->type == ASTStatement::DEFER) {
                nearDefers.push_back(tempStatement);
                info.functionScopes.back().defers.push_back(tempStatement);
            } else {
                if(tempStatement->type == ASTStatement::CONTINUE || tempStatement->type == ASTStatement::BREAK
                    || tempStatement->type == ASTStatement::RETURN)
                {
                    for(int j=info.functionScopes.back().defers.size()-1;j>=0;j--){
                        auto myDefer = info.functionScopes.back().defers[j];  // my defer, not yours, get your own
                        ASTStatement* deferCopy = info.ast->createStatement(ASTStatement::DEFER);
                        deferCopy->tokenRange = myDefer->tokenRange;
                        deferCopy->body = myDefer->body;
                        deferCopy->sharedContents = true;

                        bodyLoc->add(deferCopy);
                    }
                }
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
    
    for(int i = nearDefers.size()-1;i>=0;i--)
        bodyLoc->add(nearDefers[i]);

    bodyLoc->tokenRange.endIndex = info.at()+1;
    return PARSE_SUCCESS;
}

ASTScope* ParseTokens(TokenStream* tokens, AST* ast, CompileInfo* compileInfo, std::string theNamespace){
    using namespace engone;
    MEASURE;
    // _VLOG(log::out <<log::BLUE<<  "##   Parser   ##\n";)
    
    ParseInfo info{tokens};
    info.tokens = tokens;
    info.ast = ast;
    info.compileInfo = compileInfo;
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
        
        ScopeInfo* newScope = info.ast->createScope();
        ScopeId newScopeId = newScope->id;
        
        newScope->parent = info.currentScopeId;
        
        body->scopeId = newScopeId;
        info.ast->getScope(newScopeId)->name = theNamespace;
        info.ast->getScope(info.currentScopeId)->nameScopeMap[theNamespace] = newScope->id;
        
        info.currentScopeId = newScopeId;
    }
    info.functionScopes.push_back({});
    int result = ParseBody(info, body, true);
    info.functionScopes.pop_back();
    
    info.currentScopeId = prevScope;
    
    return body;
}