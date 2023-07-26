#include "BetBat/Parser.h"
#include "BetBat/Compiler.h"



// #undef ERRAT

#undef WARN_HEAD3
#define WARN_HEAD3(T, M) info.compileInfo->warnings++;engone::log::out << WARN_CUSTOM(info.tokens->streamName,T.line,T.column,"Parse warning","W0000") << M
#undef ERR_HEAD3
#define ERR_HEAD3(T, M) info.compileInfo->errors++;engone::log::out << ERR_DEFAULT_T(info.tokens,T,"Parse error","E0000") << M
#undef ERR_LINE2
#define ERR_LINE2(I, M) PrintCode(I, info.tokens, M)
#undef WARN_LINE2
#define WARN_LINE2(I, M) PrintCode(I, info.tokens, M)


#undef ERR_SECTION
#define ERR_SECTION(CONTENT) { info.errors++; StringBuilder err_type = "Parse error, E0000"; CONTENT }

/*
    Declaration of functions
*/
ASTScope* ParseTokenStream(TokenStream* tokens, AST* ast, CompileInfo* compileInfo, std::string theNamespace);
// out token contains a newly allocated string. use delete[] on it
SignalDefault ParseBody(ParseInfo& info, ASTScope*& bodyLoc, ScopeId parentScope, bool trulyGlobal = false);
// normal assignment a = 9
SignalAttempt ParseAssignment(ParseInfo& info, ASTStatement*& statement, bool attempt);
SignalAttempt ParseFunction(ParseInfo& info, ASTFunction*& function, bool attempt, ASTStruct* parentStruct);
SignalAttempt ParseOperator(ParseInfo& info, ASTFunction*& function, bool attempt);
// returns 0 if syntax is wrong for flow parsing
SignalAttempt ParseFlow(ParseInfo& info, ASTStatement*& statement, bool attempt);
SignalAttempt ParseExpression(ParseInfo& info, ASTExpression*& expression, bool attempt);
// parses arguments and puts them into fncall->left
SignalDefault ParseArguments(ParseInfo& info, ASTExpression* fncall, int* count);
SignalAttempt ParseEnum(ParseInfo& info, ASTEnum*& astEnum, bool attempt);
SignalAttempt ParseNamespace(ParseInfo& info, ASTScope*& astNamespace, bool attempt);
SignalAttempt ParseStruct(ParseInfo& info, ASTStruct*& astStruct,  bool attempt);
// don't forget to free data in outgoing token
SignalDefault ParseTypeId(ParseInfo& info, Token& outTypeId, int* tokensParsed);

/*
    Code for the functions
*/
// returns instruction type
// zero means no operation
bool IsSingleOp(OperationType nowOp){
    return nowOp == AST_REFER || nowOp == AST_DEREF || nowOp == AST_NOT || nowOp == AST_BNOT||
        nowOp == AST_FROM_NAMESPACE || nowOp == AST_CAST || nowOp == AST_INCREMENT || nowOp == AST_DECREMENT;
}
// info is required to perform next when encountering
// two arrow tokens
OperationType IsOp(Token& token, int& extraNext){
    if(token.flags&TOKEN_MASK_QUOTED) return (OperationType)0;
    if(Equal(token,"+")) return AST_ADD;
    if(Equal(token,"-")) return AST_SUB;
    if(Equal(token,"*")) return AST_MUL;
    if(Equal(token,"/")) return AST_DIV;
    if(Equal(token,"%")) return AST_MODULUS;
    if(Equal(token,"==")) return AST_EQUAL;
    if(Equal(token,"!=")) return AST_NOT_EQUAL;
    if(Equal(token,"<")) {
        if((token.flags&TOKEN_SUFFIX_SPACE)==0&& Equal(token.tokenStream->get(token.tokenIndex+1),"<")) {
            extraNext=1;
            return AST_BLSHIFT;
        }
         return AST_LESS;
    }
    if(Equal(token,">")) {
        if((token.flags&TOKEN_SUFFIX_SPACE)==0&&Equal(token.tokenStream->get(token.tokenIndex+1),">")) {
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
            return 1;
        case AST_OR:
            return 2;
        case AST_AND:
            return 3;
        case AST_LESS:
        case AST_GREATER:
        case AST_LESS_EQUAL:
        case AST_GREATER_EQUAL:
        case AST_EQUAL:
        case AST_NOT_EQUAL:
            return 5;
        case AST_RANGE:
            return 6;
        case AST_ADD:
            return 8;
        case AST_SUB:
            return 9;
        case AST_MODULUS:
            return 10;
        case AST_MUL:
            return 11;
        case AST_DIV:
            return 12;
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
// Concatenates current token with the next tokens not seperated by space, linefeed or such
// operators and special characters are seen as normal letters
Token CombineTokens(ParseInfo& info){
    using namespace engone;
        
    Token outToken{};
    while(!info.end()) {
        Token token = info.get(info.at()+1);
        // TODO: stop at ( # ) and such
        if(token==";"&& !(token.flags&TOKEN_MASK_QUOTED)){
            break;   
        }
        if((token.flags&TOKEN_MASK_QUOTED) && outToken.str){
            break;
        }
        token = info.next();
        if(!outToken.str)
            outToken.str = token.str;
        outToken.length += token.length;
        outToken.flags = token.flags;
        if(token.flags&TOKEN_MASK_QUOTED)
            break;
        if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED))
            break;
    }
    return outToken;
}
SignalDefault ParseTypeId(ParseInfo& info, Token& outTypeId, int* tokensParsed){
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
        if(tok.flags&TOKEN_MASK_QUOTED){
            ERR_HEAD3(tok, "Cannot have quotes in data type "<<tok<<".\n\n";
                ERR_LINE2(tok.tokenIndex,"bad");
            )
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
    if(tokensParsed)
        *tokensParsed = endTok - startToken;
    if(outTypeId.length == 0)
        return SignalDefault::FAILURE;
    return SignalDefault::SUCCESS;
}
SignalAttempt ParseStruct(ParseInfo& info, ASTStruct*& astStruct,  bool attempt){
    using namespace engone;
    MEASURE;
    _PLOG(FUNC_ENTER)
    Token structToken = info.get(info.at()+1);
    int startIndex = info.at()+1;
    if(!Equal(structToken,"struct")){
        if(attempt) return SignalAttempt::BAD_ATTEMPT;
        ERR_HEAD3(structToken,"expected struct not "<<structToken<<"\n";
            ERR_LINE2(structToken.tokenIndex,"bad");
        )
        return SignalAttempt::FAILURE;
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
            WARN_HEAD3(name, "'"<< Token(name.str+1,name.length-1) << "' is not a known annotation for functions.\n\n";
                WARN_LINE2(info.at(),"unknown");
            )
        }
        name = info.get(info.at()+1);
        continue;
    }
    
    if(!IsName(name)){
        ERR_HEAD3(name,"expected a name, "<<name<<" isn't\n";
            ERR_LINE2(name.tokenIndex,"bad");
        )
        return SignalAttempt::FAILURE;
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
    
        WHILE_TRUE {
            token = info.get(info.at()+1);
            if(Equal(token,">")){
                info.next();
                
                if(astStruct->polyArgs.size()==0){
                    ERR_HEAD3(token, "empty polymorph list\n";
                        ERR_LINE2(token.tokenIndex,"bad");
                    )
                }
                break;
            }
            if(!IsName(token)){
                ERR_HEAD3(token, token<<" is not a valid name for polymorphism\n";
                    ERR_LINE2(token.tokenIndex,"bad");
                )
            }else{
                info.next();
                ASTStruct::PolyArg arg={};
                arg.name = token;
                astStruct->polyArgs.add(arg);
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
                ERR_HEAD3(token, token<<"is neither , or >\n";
                    ERR_LINE2(token.tokenIndex,"bad");
                )
                continue;
            }
        }
        astStruct->polyName += ">";
    }
    
    token = info.get(info.at()+1);
    if(!Equal(token,"{")){
        ERR_HEAD3(token,"expected { not "<<token<<"\n";
            ERR_LINE2(token.tokenIndex,"bad");
        )
        return SignalAttempt::FAILURE;
    }
    info.next();
    
    // TODO: Structs need their own scopes for polymorphic types and
    //  internal namespaces, structs, enums...

    // Func
    ScopeInfo* structScope = info.ast->createScope(info.currentScopeId);
    astStruct->scopeId = structScope->id;

    // ensure scope is the same when returning
    auto prevScope = info.currentScopeId;
    defer {info.currentScopeId = prevScope; };
    
    info.currentScopeId = astStruct->scopeId;

    int offset=0;
    auto errorParsingMembers = SignalAttempt::SUCCESS;
    bool hadRecentError = false;
    bool affectedByAlignment=false;
    // TODO: May want to change the structure of the while loop
    //  struct { a 9 }   <-  : is expected and not 9, function will quit but that's not good because we still have }.
    //  that will cause all sorts of issue outside. leaving this parse function when } is reached even if errors occured
    //  inside is probably better.
    WHILE_TRUE {
        Token name = info.get(info.at()+1);

        if(Equal(name,";")){
            info.next();
            continue;
        } else if(Equal(name,"}")){
            info.next();
            break;
        }

        Token typeToken{};
        int typeEndToken = -1;
        if (Equal(name,"fn")) {
            // parse function?
            ASTFunction* func = 0;
            SignalAttempt result = ParseFunction(info, func, false, astStruct);
            if(result!=SignalAttempt::SUCCESS){
                continue;
            }
            func->parentStruct = astStruct;
            astStruct->add(func);
            // ,func->polyArgs.size()==0?&func->baseImpl:nullptr);
        } else {
            if(!IsName(name)){
                info.next();
                if(!hadRecentError){
                    ERR_HEAD3(name, "expected a name, "<<name<<" isn't\n";
                        ERR_LINE2(name.tokenIndex,"bad");
                    )
                }
                hadRecentError=true;
                errorParsingMembers = SignalAttempt::FAILURE;
                continue;
            }
            info.next();   
            Token token = info.get(info.at()+1);
            if(!Equal(token,":")){
                ERR_HEAD3(token,"expected : not "<<token<<"\n";
                    ERR_LINE2(token.tokenIndex,"bad");
                )
                errorParsingMembers = SignalAttempt::FAILURE;
                continue;
            }    
            info.next();
            SignalDefault result = ParseTypeId(info,typeToken,nullptr);
            if(result!=SignalDefault::SUCCESS) {
                ERR_HEAD3(typeToken, "failed parsing type "<<typeToken<<"\n";
                    ERR_LINE2(typeToken.tokenIndex,"bad");
                )
                continue;
            }
            typeEndToken = info.at()+1;
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
                        ERR_HEAD3(token2, "Array cannot have negative size.\n\n";
                            ERR_LINE2(info.at()-1,"< 0");
                        )
                        arrayLength = 0;
                    }
                } else {
                    ERR_HEAD3(token2, "The length of an array can only be specified with number literals. Use macros to avoid magic numbers. Constants have not been implemented but when they have, they will work too.\n\n";
                        ERR_LINE2(info.at()-1, "must be positive integer literal");
                    )
                }
                std::string* str = info.ast->createString();
                *str = "Slice<";
                *str += typeToken;
                *str += ">";
                typeToken = *str;
            }

            // std::string temps = typeToken;
            
            TypeId typeId = info.ast->getTypeString(typeToken);
            
            SignalAttempt resultFromExpr = SignalAttempt::SUCCESS;
            ASTExpression* defaultValue=0;
            token = info.get(info.at()+1);
            if(Equal(token,"=")){
                info.next();
                resultFromExpr = ParseExpression(info,defaultValue,false);
            }
            if(resultFromExpr == SignalAttempt::SUCCESS){
                astStruct->members.add({});
                auto& mem = astStruct->members.last();
                mem.name = name;
                mem.defaultValue = defaultValue;
                mem.stringType = typeId;
            }
        }
        Token token = info.get(info.at()+1);
        if(Equal(token,"fn")){
            // semi-colon not needed for functions
            continue;
        }else if(Equal(token,";")){
            info.next();
            hadRecentError=false;
            continue;
        }else if(Equal(token,"}")){
            info.next();
            break;
        }else{
            if(!hadRecentError){ // no need to print message again, the user already know there are some issues here.
                ERR_HEAD3(token,"Expected a curly brace or semi-colon to mark the end of the member (was: "<<token<<").\n\n";
                    if(typeEndToken!=-1){
                        TokenRange temp{};
                        temp.firstToken = typeToken;
                        temp.endIndex = typeEndToken;
                        PrintCode(temp,"evaluated type");
                    }
                    ERR_LINE2(token.tokenIndex,"bad");
                )
            }
            hadRecentError=true;
            errorParsingMembers = SignalAttempt::FAILURE;
            continue;
        }
    }
    astStruct->tokenRange.endIndex = info.at()+1;
    _GLOG(log::out << "Parsed struct "<<log::LIME<< name <<log::SILVER << " with "<<astStruct->members.size()<<" members\n";)
    return errorParsingMembers;
}
SignalAttempt ParseNamespace(ParseInfo& info, ASTScope*& astNamespace, bool attempt){
    using namespace engone;
    MEASURE;
    _PLOG(FUNC_ENTER)
    Token token = info.get(info.at()+1);
    int startIndex = info.at()+1;
    if(!Equal(token,"namespace")){
        if(attempt)
            return SignalAttempt::BAD_ATTEMPT;
        ERR_HEAD3(token,"expected namespace not "<<token<<"\n";
            ERR_LINE2(token.tokenIndex,"bad");
        )
        return SignalAttempt::FAILURE;
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
            WARN_HEAD3(name, "'"<< Token(name.str+1,name.length-1) << "' is not a known annotation for functions.\n\n";
                WARN_LINE2(info.at(),"unknown");
            )
        }
        name = info.get(info.at()+1);
        continue;
    }

    if(!IsName(name)){
        ERR_HEAD3(name,"expected a name, "<<name<<" isn't\n";
            ERR_LINE2(name.tokenIndex,"bad");
        )
        return SignalAttempt::FAILURE;
    }
    info.next();

    token = info.get(info.at()+1);
    if(!Equal(token,"{")){
        ERR_HEAD3(token,"expected { not "<<token<<"\n";
            ERR_LINE2(token.tokenIndex,"bad");
        )
        return SignalAttempt::FAILURE;
    }
    info.next();
    
    int nextId=0;
    astNamespace = info.ast->createNamespace(name);
    astNamespace->tokenRange.firstToken = token;
    // astNamespace->tokenRange.startIndex = startIndex;
    // astNamespace->tokenRange.tokenStream = info.tokens;
    astNamespace->hidden = hideAnnotation;

    ScopeInfo* newScope = info.ast->createScope(info.currentScopeId);
    astNamespace->scopeId = newScope->id;

    newScope->name = name;
    info.ast->getScope(newScope->parent)->nameScopeMap[name] = newScope->id;
    
    ScopeId prevScope = info.currentScopeId;
    defer { info.currentScopeId = prevScope; };

    info.currentScopeId = newScope->id;

    auto error = SignalAttempt::SUCCESS;
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
    
        auto result=SignalAttempt::BAD_ATTEMPT;

        if(result==SignalAttempt::BAD_ATTEMPT)
            result = ParseFunction(info,tempFunction,true, nullptr);
        if(result==SignalAttempt::BAD_ATTEMPT)
            result = ParseStruct(info,tempStruct,true);
        if(result==SignalAttempt::BAD_ATTEMPT)
            result = ParseEnum(info,tempEnum,true);
        if(result==SignalAttempt::BAD_ATTEMPT)
            result = ParseNamespace(info,tempNamespace,true);

        if(result==SignalAttempt::BAD_ATTEMPT){
            Token& token = info.get(info.at()+1);
            ERR_HEAD3(token, "Unexpected '"<<token<<"' (ParseNamespace)\n";
                ERR_LINE2(token.tokenIndex,"bad");
            )
            
            // test other parse type
            info.next(); // prevent infinite loop
        }
        if(result==SignalAttempt::FAILURE){
            
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
SignalAttempt ParseEnum(ParseInfo& info, ASTEnum*& astEnum, bool attempt){
    using namespace engone;
    MEASURE;
    _PLOG(FUNC_ENTER)
    Token enumToken = info.get(info.at()+1);
    int startIndex = info.at()+1;
    if(!Equal(enumToken,"enum")){
        if(attempt) return SignalAttempt::BAD_ATTEMPT;
        ERR_HEAD3(enumToken,"expected struct not "<<enumToken<<"\n";
            ERR_LINE2(enumToken.tokenIndex,"bad");
        )
        return SignalAttempt::FAILURE;   
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
            WARN_HEAD3(name, "'"<< Token(name.str+1,name.length-1) << "' is not a known annotation for functions.\n\n";
                WARN_LINE2(info.at(),"unknown");
            )
        }
        name = info.get(info.at()+1);
        continue;
    }

    if(!IsName(name)){
        ERR_HEAD3(name,"expected a name, "<<name<<" isn't\n";
            ERR_LINE2(name.tokenIndex,"bad");
        )
        return SignalAttempt::FAILURE;
    }
    info.next(); // name
    
    Token token = info.get(info.at()+1);
    if(!Equal(token,"{")){
        ERR_HEAD3(token,"expected { not "<<token<<"\n";
            ERR_LINE2(token.tokenIndex,"bad");
        )
        return SignalAttempt::FAILURE;
    }
    info.next();
    int nextId=0;
    astEnum = info.ast->createEnum(name);
    astEnum->tokenRange.firstToken = enumToken;
    // astEnum->tokenRange.startIndex = startIndex;
    // astEnum->tokenRange.tokenStream = info.tokens;
    astEnum->hidden = hideAnnotation;
    auto error = SignalAttempt::SUCCESS;
    WHILE_TRUE {
        Token name = info.get(info.at()+1);
        
        if(Equal(name,"}")){
            info.next();
            break;
        }
        
        if(!IsName(name)){
            info.next(); // move forward to prevent infinite loop
            ERR_HEAD3(name, "expected a name, "<<name<<" isn't\n";
                ERR_LINE2(name.tokenIndex,"bad");
            )
            error = SignalAttempt::FAILURE;
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
                ERR_HEAD3(token, token<<" is not an integer (i32)\n";
                    ERR_LINE2(token.tokenIndex,"bad");
                )
            }
        }
        astEnum->members.add({name,nextId++});
        
        // token = info.get(info.at()+1);
        if(Equal(token,",")){
            info.next();
            continue;
        }else if(Equal(token,"}")){
            info.next();
            break;
        }else{
            ERR_HEAD3(token,"expected } or , not "<<token<<"\n";
                ERR_LINE2(token.tokenIndex,"bad");
            )
            error = SignalAttempt::FAILURE;
            continue;
        }
    }
    astEnum->tokenRange.endIndex = info.at()+1;
    // auto typeInfo = info.ast->getTypeInfo(info.currentScopeId, name, false, true);
    // int strId = info.ast->getTypeString(name);
    // if(!typeInfo){
        // ERR_HEAD3(name, name << " is taken\n";
        // info.ast->destroy(astEnum);
        // astEnum = 0;
        // return PARSE_ERROR;
    // }
    // typeInfo->astEnum = astEnum;
    // typeInfo->_size = 4; // i32
    _PLOG(log::out << "Parsed enum "<<log::LIME<< name <<log::SILVER <<" with "<<astEnum->members.size()<<" members\n";)
    return error;
}
SignalDefault ParseArguments(ParseInfo& info, ASTExpression* fncall, int* count){
    MEASURE;

    // TODO: sudden end, error handling
    bool expectComma=false;
    bool mustBeNamed=false;
    Token prevNamed = {};
    WHILE_TRUE {
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
            ERR_HEAD3(tok,"Expected ',' to supply more arguments or ')' to end fncall (found "<<tok<<" instead)\n";
            )
            return SignalDefault::FAILURE;
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
            ERR_HEAD3(tok, "expected named argument because of previous named argument "<<prevNamed << " at "<<prevNamed.line<<":"<<prevNamed.column<<"\n";
                ERR_LINE2(tok.tokenIndex,"bad");
            )
            // return or continue could desync the parsing so don't do that.
        }

        ASTExpression* expr=0;
        SignalAttempt result = ParseExpression(info,expr,false);
        if(result!=SignalAttempt::SUCCESS){
            // TODO: error message, parse other arguments instead of returning?
            return SignalDefault::FAILURE;
        }
        if(named){
            expr->namedValue = tok;
            // expr->namedValue = (std::string*)engone::Allocate(sizeof(std::string));
            // new(expr->namedValue)std::string(tok);
        } else {
            fncall->nonNamedArgs++;
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
    return SignalDefault::SUCCESS;
}
SignalAttempt ParseExpression(ParseInfo& info, ASTExpression*& expression, bool attempt){
    using namespace engone;
    MEASURE;
    _PLOG(FUNC_ENTER)
    
    if(info.end()){
        if(attempt)
            return SignalAttempt::BAD_ATTEMPT;
        return SignalAttempt::FAILURE;
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
    WHILE_TRUE {
        int tokenIndex = info.at()+1;
        Token token = info.get(tokenIndex);
        
        OperationType opType = (OperationType)0;
        bool ending=false;
        
        if(expectOperator){
            int extraOpNext=0;
            if(Equal(token,".")){
                info.next();
                
                Token memberName = info.get(info.at()+1);
                if(!IsName(memberName)){
                    ERR_HEAD3(memberName, "Expected a property name, not "<<memberName<<".\n\n";
                        ERR_LINE2(memberName.tokenIndex,"bad");
                    )
                    continue;
                }
                info.next();

                Token tok = info.get(info.at()+1);
                std::string polyTypes="";
                if(Equal(tok,"<") && 0==(tok.flags&TOKEN_SUFFIX_SPACE)){
                    info.next();
                    memberName.length++;
                    // polymorphic type or function
                    polyTypes += "<";
                    int depth = 1;
                    while(!info.end()){
                        tok = info.next();
                        polyTypes+=tok;
                        memberName.length+=tok.length;
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
                    tmp->name = memberName;
                    // tmp->name = (std::string*)engone::Allocate(sizeof(std::string));
                    // new(tmp->name)std::string(std::string(token)+polyTypes);
                    
                    tmp->args = (DynamicArray<ASTExpression*>*)engone::Allocate(sizeof(DynamicArray<ASTExpression*>));
                    new(tmp->args)DynamicArray<ASTExpression*>();

                    // Create "this" argument in methods
                    tmp->args->add(values.back());
                    tmp->nonNamedArgs++;
                    values.pop_back();
                    

                    tmp->boolValue = true; // indicicate fncall to struct method

                    // Parse the other arguments
                    int count = 0;
                    SignalDefault result = ParseArguments(info, tmp, &count);
                    if(result!=SignalDefault::SUCCESS){
                        info.ast->destroy(tmp);
                        return SignalAttempt::FAILURE;
                    }

                    _PLOG(log::out << "Parsed call "<<count <<"\n";)
                    tmp->tokenRange.firstToken = memberName;
                    // tmp->tokenRange.startIndex = startToken;
                    tmp->tokenRange.endIndex = info.at()+1;
                    // tmp->tokenRange.tokenStream = info.tokens;

                    values.push_back(tmp);
                    continue;
                }
                if(polyTypes.length()!=0){
                    ERR_HEAD3(info.now(),
                    "Polymorphic arguments indicates a method call put the parenthesis for it is missing. '"<<info.get(info.at()+1)<<"' is not '('.";
                    )
                }
                
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_MEMBER));
                tmp->name = memberName;
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
                    WARN_HEAD3(token, "'"<<token << "' is treated as a multiplication but perhaps you meant to do a dereference since the operation exists on a new line. "
                        "Separate with a semi-colon for dereference or put the multiplication on the same line to silence this message.\n\n";
                        WARN_LINE2(info.at(), "semi-colon is recommended after statements");
                        WARN_LINE2(info.at()+1, "should this be left and right operation");
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
                SignalAttempt result = ParseExpression(info, indexExpr, false);
                if(result != SignalAttempt::SUCCESS){
                    
                }
                Token tok = info.get(info.at()+1);
                if(!Equal(tok,"]")){
                    ERR_HEAD3(tok,"bad\n\n";)
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
                        WARN_HEAD3(token, "Did you forget the semi-colon to end the statement or was it intentional? Perhaps you mistyped a character? (put the next statement on a new line to silence this warning)\n\n"; 
                            ERR_LINE2(tokenIndex-1, "semi-colon here?");
                            // ERR_LINE2(tokenIndex, "; before this?");
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
                    ERR_HEAD3(tok, "expected < not "<<tok<<"\n";
                        ERR_LINE2(tok.tokenIndex,"bad");
                    )
                    continue;
                }
                info.next();
                Token tokenTypeId{};
                SignalDefault result = ParseTypeId(info,tokenTypeId, nullptr);
                if(result!=SignalDefault::SUCCESS){
                    ERR_HEAD3(tokenTypeId, tokenTypeId << "is not a valid data type\n";
                        ERR_LINE2(tokenTypeId.tokenIndex,"bad");
                    )
                }
                tok = info.get(info.at()+1);
                if(!Equal(tok,">")){
                    ERR_HEAD3(tok, "expected > not "<< tok<<"\n";
                        ERR_LINE2(tok.tokenIndex,"bad");
                    )
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
                negativeNumber=!negativeNumber;
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
                
                // TODO: handle to large numbers
                Assert(token.str[0]!='-');
                bool printedError = false;
                u64 num=0;
                for(int i=0;i<token.length;i++){
                    char c = token.str[i];
                    if(num * 10 < num && !printedError) {
                        ERR_HEAD3(token,"Number overflow! '"<<token<<"' is to large for 64-bit integers!\n\n";
                            ERR_LINE2(token.tokenIndex,"to large!");
                        )
                        printedError = true;
                    }
                    num = num*10 + (c-'0');
                }

                ASTExpression* tmp = 0;
                if(negativeNumber) {
                    if ((num&0xFFFFFFFF80000000) == 0) {
                        tmp = info.ast->createExpression(TypeId(AST_INT32));
                    } else if((num&0x8000000000000000) == 0) {
                        tmp = info.ast->createExpression(TypeId(AST_INT64));
                    } else {
                        tmp = info.ast->createExpression(TypeId(AST_INT64));
                        if(!printedError){
                            ERR_HEAD3(token,"Number overflow! '"<<token<<"' is to large for 64-bit integers!\n\n";
                                ERR_LINE2(token.tokenIndex,"to large!");
                            )
                        }
                    }
                }else{
                    if ((num&0xFFFFFFFF00000000) == 0) {
                        tmp = info.ast->createExpression(TypeId(AST_INT32));
                    } else {
                        tmp = info.ast->createExpression(TypeId(AST_INT64));
                    }
                }
                tmp->i64Value = negativeNumber ? -num : num;
                negativeNumber = false;
                
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
                if(negativeNumber){
                    ERR_HEAD3(token,"Negative hexidecimals is not okay.\n\n";
                        ERR_LINE2(token.tokenIndex,"bad");
                    )
                    negativeNumber=false;
                }
                // 0x000000001 will be treated as 64 bit value and
                // it probably should because you wouldn't use so many 
                // zero for no reason.
                ASTExpression* tmp = nullptr;
                if(token.length-2<=8) {
                    tmp = info.ast->createExpression(TypeId(AST_UINT32));
                } else if(token.length-2<=16) {
                    tmp = info.ast->createExpression(TypeId(AST_UINT64));
                } else {
                    tmp = info.ast->createExpression(TypeId(AST_UINT64));
                    ERR_HEAD3(token,"Hexidecimal overflow! '"<<token<<"' is to large for 64-bit integers!\n\n";
                        ERR_LINE2(token.tokenIndex,"to large!");
                    )
                }
                tmp->i64Value =  ConvertHexadecimal(token); // TODO: Only works with 32 bit or 16,8 I suppose.
                values.push_back(tmp);
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(token.flags&TOKEN_MASK_QUOTED) {
                info.next();
                
                ASTExpression* tmp = 0;
                // if(token.length == 1){
                if((token.flags&TOKEN_SINGLE_QUOTED)){
                    tmp = info.ast->createExpression(TypeId(AST_CHAR));
                    tmp->charValue = *token.str;
                }else if((token.flags&TOKEN_DOUBLE_QUOTED)) {
                    tmp = info.ast->createExpression(TypeId(AST_STRING));
                    tmp->name = token;
                    // tmp->name = (std::string*)engone::Allocate(sizeof(std::string));
                    // new(tmp->name)std::string(token);

                    // A string of zero size can be useful which is why
                    // the code below is commented out.
                    // if(token.length == 0){
                        // ERR_HEAD3(token, "string should not have a length of 0\n";
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
 
            // WHILE_TRUE {
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
            //             ERR_HEAD3(token, "expected ] not "<<token<<"\n";
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
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_SIZEOF));

                // Token token = {}; 
                // int result = ParseTypeId(info,token);
                // tmp->name = token;

                ASTExpression* left=nullptr;
                SignalAttempt result = ParseExpression(info, left, false);
                tmp->left = left;
                
                tmp->tokenRange.firstToken = token;
                tmp->tokenRange.endIndex = info.at()+1;
                values.push_back(tmp);
            } else if(Equal(token,"nameof")) {
                info.next();
                Token token = {};
                SignalDefault result = ParseTypeId(info,token, nullptr);

                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_NAMEOF));
                tmp->name = token;
                tmp->tokenRange.firstToken = token;
                tmp->tokenRange.endIndex = info.at()+1;
                values.push_back(tmp);
            } else if(IsName(token)){
                info.next();
                int startToken=info.at();
                
                Token polyName = token;
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
                if(Equal(tok,"<") && !(token.flags&TOKEN_SUFFIX_SPACE)){
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
                Token tok2 = info.get(info.at()+2);
                Assert(!(Equal(tok,"[") && Equal(tok2,"]"))); // HANDLE SLICE TYPES!

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
                    SignalDefault result = ParseArguments(info, tmp, &count);
                    if(result!=SignalDefault::SUCCESS){
                        info.ast->destroy(tmp);
                        return SignalAttempt::FAILURE;
                    }

                    _PLOG(log::out << "Parsed call "<<count <<"\n";)
                    values.push_back(tmp);
                    tmp->tokenRange.firstToken = token;
                    // tmp->tokenRange.startIndex = startToken;
                    tmp->tokenRange.endIndex = info.at()+1;
                    // tmp->tokenRange.tokenStream = info.tokens;
                } else if(Equal(tok,"{") && 0 == (token.flags & TOKEN_SUFFIX_SPACE)){ // Struct {} is ignored
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
                    WHILE_TRUE {
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
                            ERR_HEAD3(token, "expected named argument because of previous named argument "<<prevNamed << " at "<<prevNamed.line<<":"<<prevNamed.column<<"\n";
                                ERR_LINE2(token.tokenIndex,"bad");
                            )
                            // return or continue could desync the parsing so don't do that.
                        }

                        ASTExpression* expr=0;
                        SignalAttempt result = ParseExpression(info,expr,false);
                        if(result!=SignalAttempt::SUCCESS){
                            // TODO: error message, parse other arguments instead of returning?
                            // return PARSE_ERROR; Returning would leak arguments so far
                            info.next(); // prevent infinite loop
                            continue;
                        }
                        if(named){
                            expr->namedValue = token;
                            // expr->namedValue = (std::string*)engone::Allocate(sizeof(std::string));
                            // new(expr->namedValue)std::string(token);
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
                            ERR_HEAD3(token, "expected , or } not "<<token<<"\n";
                                ERR_LINE2(token.tokenIndex,"bad");
                            )
                            continue;
                        }
                    }
                    // log::out << "Parse initializer "<<count<<"\n";
                    values.push_back(initExpr);
                    initExpr->tokenRange.firstToken = token;
                    // initExpr->tokenRange.startIndex = startToken;
                    initExpr->tokenRange.endIndex = info.at()+1;
                    // initExpr->tokenRange.tokenStream = info.tokens;
                } else {
                    // if(polyTypes.size()!=0){
                    //     This is possible since types are can be parsed in an expression.
                    //     ERR_HEAD3(token, "Polymorphic types not expected with namespace or variable.\n\n";
                    //         ERR_LINE2(token.tokenIndex,"bad");
                    //     )
                    //     continue;
                    // }
                    if(Equal(tok,"::")){
                        info.next();
                        
                        // Token tok = info.get(info.at()+1);
                        // if(!IsName(tok)){
                        //     ERR_HEAD3(tok, "'"<<tok<<"' is not a name.\n\n";
                        //         ERR_LINE2(tok.tokenIndex,"bad");
                        //     )
                        //     continue;
                        // }
                        ops.push_back(AST_FROM_NAMESPACE);
                        namespaceNames.push_back(token);
                        continue; // do a second round here?
                        // TODO: detect more ::
                    } else {
                        ASTExpression* tmp = info.ast->createExpression(TypeId(AST_ID));

                        Token nsToken = polyName;
                        int nsOps = 0;
                        while(ops.size()>0){
                            OperationType op = ops.back();
                            if(op == AST_FROM_NAMESPACE) {
                                nsOps++;
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
                            nsToken.length += polyName.length;
                        }

                        tmp->name = nsToken;

                        values.push_back(tmp);
                        tmp->tokenRange.firstToken = nsToken;
                        tmp->tokenRange.endIndex = info.at()+1;
                    }
                }
            } else if(Equal(token,"(")){
                // parse again
                info.next();
                ASTExpression* tmp=0;
                SignalAttempt result = ParseExpression(info,tmp,false);
                if(result!=SignalAttempt::SUCCESS)
                    return SignalAttempt::FAILURE;
                if(!tmp){
                    ERR_HEAD3(token, "got nothing from paranthesis in expression\n";
                        ERR_LINE2(token.tokenIndex,"bad");
                    )
                }
                token = info.get(info.at()+1);
                if(!Equal(token,")")){
                    ERR_HEAD3(token, "expected ) not "<<token<<"\n";   
                        ERR_LINE2(token.tokenIndex,"bad");
                    )
                }else
                    info.next();
                values.push_back(tmp);  
            } else {
                if(attempt){
                    return SignalAttempt::BAD_ATTEMPT;
                }else{
                    ERR_HEAD3(token, "'"<<token << "' is not a value. Values (or expressions) are expected after tokens that calls upon arguments, operations and assignments among other things.\n";
                        ERR_LINE2(tokenIndex,"should be a value");
                        ERR_LINE2(tokenIndex-1,"expects a value afterwards")
                    );
                    // ERR_LINE2()
                    // printLine();
                    return SignalAttempt::FAILURE;
                    // Qutting here is okay because we have a defer which
                    // destroys parsed expressions. No memory leaks!
                }
            }
        }
        if(negativeNumber){
            ERR_HEAD3(info.get(info.at()-1), "Unexpected - before "<<token<<"\n";
                ERR_LINE2(info.at()-1,"bad");
            )
            return SignalAttempt::FAILURE;
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
                    val->assignOpType = assignOps.back();
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
            Assert(values.size()==1);

            // we have a defer above which destroys expressions which is why we
            // clear the list to prevent it
            values.clear();
            return SignalAttempt::SUCCESS;
        }
    }
    // shouldn't happen
    return SignalAttempt::FAILURE;
}
SignalAttempt ParseFlow(ParseInfo& info, ASTStatement*& statement, bool attempt){
    using namespace engone;
    MEASURE;
    _PLOG(FUNC_ENTER)
    
    if(info.end()){
        return SignalAttempt::FAILURE;
    }
    Token firstToken = info.get(info.at()+1);
    int startIndex = info.at()+1;
    if(Equal(firstToken,"if")){
        info.next();
        ASTExpression* expr=0;
        SignalAttempt result = ParseExpression(info,expr,false);
        if(result!=SignalAttempt::SUCCESS){
            // TODO: should more stuff be done here?
            return SignalAttempt::FAILURE;
        }
        ASTScope* body=0;
        SignalDefault resultBody = ParseBody(info,body, info.currentScopeId);
        if(resultBody!=SignalDefault::SUCCESS){
            return SignalAttempt::FAILURE;
        }
        
        ASTScope* elseBody=0;
        Token token = info.get(info.at()+1);
        if(Equal(token,"else")){
            info.next();
            resultBody = ParseBody(info,elseBody, info.currentScopeId);
            if(resultBody!=SignalDefault::SUCCESS){
                return SignalAttempt::FAILURE;
            }   
        }

        statement = info.ast->createStatement(ASTStatement::IF);
        statement->firstExpression = expr;
        statement->firstBody = body;
        statement->secondBody = elseBody;
        statement->tokenRange.firstToken = firstToken;
        
        statement->tokenRange.endIndex = info.at()+1;
        
        return SignalAttempt::SUCCESS;
    }else if(Equal(firstToken,"while")){
        info.next();
        ASTExpression* expr=0;
        SignalAttempt result = ParseExpression(info,expr,false);
        if(result!=SignalAttempt::SUCCESS){
            // TODO: should more stuff be done here?
            return SignalAttempt::FAILURE;
        }

        info.functionScopes.last().loopScopes.add({});
        ASTScope* body=0;
        SignalDefault resultBody = ParseBody(info,body, info.currentScopeId);
        info.functionScopes.last().loopScopes.pop();
        if(resultBody!=SignalDefault::SUCCESS){
            return SignalAttempt::FAILURE;
        }

        statement = info.ast->createStatement(ASTStatement::WHILE);
        statement->firstExpression = expr;
        statement->firstBody = body;
        statement->tokenRange.firstToken = firstToken;
        
        statement->tokenRange.endIndex = info.at()+1;
        
        return SignalAttempt::SUCCESS;
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
                WARN_HEAD3(token, "'"<< Token(token.str+1,token.length-1) << "' is not a known annotation for functions.\n\n";
                    WARN_LINE2(info.at(),"unknown");
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
        SignalAttempt result = ParseExpression(info,expr,false);
        if(result!=SignalAttempt::SUCCESS){
            // TODO: should more stuff be done here?
            return SignalAttempt::FAILURE;
        }
        ASTScope* body=0;
        info.functionScopes.last().loopScopes.add({});
        SignalDefault resultBody = ParseBody(info,body, info.currentScopeId);
        info.functionScopes.last().loopScopes.pop();
        if(resultBody!=SignalDefault::SUCCESS){
            return SignalAttempt::FAILURE;
        }
        statement = info.ast->createStatement(ASTStatement::FOR);
        statement->varnames.add({varname});
        statement->reverse = reverseAnnotation;
        statement->pointer = pointerAnnot;
        statement->firstExpression = expr;
        statement->firstBody = body;
        statement->tokenRange.firstToken = firstToken;
        
        statement->tokenRange.endIndex = info.at()+1;
        
        return SignalAttempt::SUCCESS;
    } else if(Equal(firstToken,"return")){
        info.next();
        
        statement = info.ast->createStatement(ASTStatement::RETURN);
        // statement->returnValues = (std::vector<ASTExpression*>*)engone::Allocate(sizeof(std::vector<ASTExpression*>));
        // new(statement->returnValues)std::vector<ASTExpression*>();

        WHILE_TRUE {
            ASTExpression* expr=0;
            Token token = info.get(info.at()+1);
            if(Equal(token,";")&&statement->returnValues.size()==0){ // return 1,; should not be allowed, that's why we do &&!base
                info.next();
                break;
            }
            SignalAttempt result = ParseExpression(info,expr,true);
            if(result!=SignalAttempt::SUCCESS){
                break;
            }
            statement->returnValues.add(expr);
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
        statement->tokenRange.endIndex = info.at()+1;
        return SignalAttempt::SUCCESS;
    } else if(Equal(firstToken,"using")){
        info.next();

        // variable name
        // namespacing
        // type with polymorphism. not pointers
        // TODO: namespace environemnt, not just using X as Y

        Token originToken={};
        SignalDefault result = ParseTypeId(info, originToken, nullptr);
        Assert(result == SignalDefault::SUCCESS);
        Token aliasToken = {};
        Token token = info.get(info.at()+1);
        if(Equal(token,"as")) {
            info.next();
            
            SignalDefault result = ParseTypeId(info, aliasToken, nullptr);
        }
        
        statement = info.ast->createStatement(ASTStatement::USING);
        
        statement->varnames.add({originToken});
        if(aliasToken.str){
            statement->alias = (std::string*)engone::Allocate(sizeof(std::string));
            new(statement->alias)std::string(aliasToken);
        }

        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.endIndex = info.at()+1;
        return SignalAttempt::SUCCESS;        
    } else if(Equal(firstToken,"defer")){
        info.next();

        ASTScope* body = nullptr;
        SignalDefault result = ParseBody(info, body, info.currentScopeId);
        if(result != SignalDefault::SUCCESS) {
            return SignalAttempt::FAILURE;
        }

        statement = info.ast->createStatement(ASTStatement::DEFER);
        statement->firstBody = body;

        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.endIndex = info.at()+1;
        return SignalAttempt::SUCCESS;  
    } else if(Equal(firstToken, "break")) {
        info.next();

        // TODO: We must be in loop scope!

        statement = info.ast->createStatement(ASTStatement::BREAK);

        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.endIndex = info.at()+1;
        return SignalAttempt::SUCCESS;
    } else if(Equal(firstToken, "continue")) {
        info.next();

        // TODO: We must be in loop scope!

        statement = info.ast->createStatement(ASTStatement::CONTINUE);

        statement->tokenRange.firstToken = firstToken;
        
        statement->tokenRange.endIndex = info.at()+1;
        
        return SignalAttempt::SUCCESS;  
    }
    if(attempt)
        return SignalAttempt::BAD_ATTEMPT;
    return SignalAttempt::FAILURE;
}
SignalAttempt ParseOperator(ParseInfo& info, ASTFunction*& function, bool attempt) {
    using namespace engone;
    MEASURE;
    _PLOG(FUNC_ENTER)
    Token beginToken = info.get(info.at()+1);
    // int startIndex = info.at()+1;
    if(!Equal(beginToken,"operator")){
        if(attempt) return SignalAttempt::BAD_ATTEMPT;
        ERR_HEAD3(beginToken, "Expected 'operator' for operator overloading not '"<<beginToken<<"'.\n";

        )
            
        return SignalAttempt::FAILURE;
    }
    info.next();
    attempt = false;

    function = info.ast->createFunction();
    function->tokenRange.firstToken = beginToken;
    function->tokenRange.endIndex = info.at()+1;;

    Token name = info.next();
    while (IsAnnotation(name)){
        if(Equal(name,"@hide")){
            function->hidden=true;
        } else {
            // It should not warn you because it is quite important that you use the right annotations with functions
            // Mispelling external or the calling convention would be very bad.
            ERR_HEAD3(name, "'"<< Token(name.str+1,name.length-1) << "' is not a known annotation for operators.\n\n";
                ERR_LINE2(info.at(),"unknown");
            )
        }
        name = info.next();
        continue;
    }

    OperationType op = (OperationType)0;
    int extraNext = 0;
    
    // TODO: Improve parsing here. If the token is an operator but not allowed
    //  then you can continue parsing, it's just that the function won't be
    //  added to the tree and be usable. Parse and then throw away.
    //  Stuff following the operator overload´syntax will then parse fine.
    
    // What about +=, -=?
    if(!(op = IsOp(name, extraNext))){
        info.ast->destroy(function);
        function = nullptr;
        ERR_HEAD3(name,"Expected a valid operator, "<<name<<" is not.\n";)
        return SignalAttempt::FAILURE;
    }
    // info.next(); // next is done above
    while(extraNext--){
        Token t = info.next();
        name.length += t.length;
        // used for doing next on the extra arrows for bit shifts
    }
    Assert(info.get(info.at()+1) != "=");

    function->name = name;

    ScopeInfo* funcScope = info.ast->createScope(info.currentScopeId);
    function->scopeId = funcScope->id;

    // ensure we leave this parse function with the same scope we entered with
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
            SignalDefault result = ParseTypeId(info, tok, nullptr);
            if(result == SignalDefault::SUCCESS) {
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
                ERR_HEAD3(tok, "expected , or > for in poly. arguments for operator "<<name<<"\n";
            )
                // parse error or what?
                break;
            }
        }
    }
    tok = info.get(info.at()+1);
    if(!Equal(tok,"(")){
        ERR_HEAD3(tok, "expected ( not "<<tok<<"\n";
        )
        return SignalAttempt::FAILURE;
    }
    info.next();

    bool printedErrors=false;
    TokenRange prevDefault={};
    WHILE_TRUE {
        Token& arg = info.get(info.at()+1);
        if(Equal(arg,")")){
            info.next();
            break;
        }
        if(!IsName(arg)){
            info.next();
            if(!printedErrors) {
                printedErrors=true;
                ERR_HEAD3(arg, "'"<<arg <<"' is not a valid argument name.\n\n";
                    ERR_LINE2(arg.tokenIndex,"bad");
                )
            }
            continue;
            // return PARSE_ERROR;
        }
        info.next();
        tok = info.get(info.at()+1);
        if(!Equal(tok,":")){
            if(!printedErrors) {
                printedErrors=true;
                ERR_HEAD3(tok, "Expected : not "<<tok <<".\n\n";
                    ERR_LINE2(tok.tokenIndex,"bad");
                )
            }
            continue;
            // return PARSE_ERROR;
        }
        info.next();

        Token dataType{};
        SignalDefault result = ParseTypeId(info,dataType, nullptr);
        Assert(result == SignalDefault::SUCCESS);
        // auto id = info.ast->getTypeInfo(info.currentScopeId,dataType)->id;
        TypeId strId = info.ast->getTypeString(dataType);

        function->arguments.add({});
        auto& argv = function->arguments[function->arguments.size()-1];
        argv.name = arg; // add the argument even if default argument fails
        argv.stringType = strId;

        function->nonDefaults++; // We don't have defaults at all
        // this is important for function overloading

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
            ERR_HEAD3(tok, "Expected , or ) not "<<tok <<".\n\n";
                ERR_LINE2(tok.tokenIndex,"bad");
            )
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
        
        WHILE_TRUE {
            
            Token tok = info.get(info.at()+1);
            if(Equal(tok,"{") || Equal(tok,";")){
                break;   
            }
            
            Token dt{};
            SignalDefault result = ParseTypeId(info,dt, nullptr);
            if(result!=SignalDefault::SUCCESS){
                break; // prevent infinite loop
                // info.next();
                // continue as we have
                // continue;
                // return PARSE_ERROR;
                // printedErrors = false;
            } else {
                TypeId strId = info.ast->getTypeString(dt);
                function->returnValues.add({});
                function->returnValues.last().stringType = strId;
                function->returnValues.last().valueToken = dt;
                function->returnValues.last().valueToken.endIndex = info.at()+1;
                printedErrors = false;
            }
            tok = info.get(info.at()+1);
            if(Equal(tok,"{") || Equal(tok,";")){
                // info.next(); { is parsed in ParseBody
                break;   
            } else if(Equal(tok,",")){
                info.next();
                continue;
            } else {
                if(function->linkConvention != LinkConventions::NONE)
                    break;
                if(!printedErrors){
                    printedErrors=true;
                    ERR_HEAD3(tok, "Expected a comma or curly brace. '"<<tok <<"' is not okay.\n";
                        ERR_LINE2(tok.tokenIndex,"bad coder");
                    )
                }
                continue;
                // Continuing since we are inside of return values and expect
                // something to end it
            }
        }
    }
    function->tokenRange.endIndex = info.at()+1; // don't include body in function's token range
    // the body's tokenRange can be accessed with function->body->tokenRange
    
    Token& bodyTok = info.get(info.at()+1);
    
    if(Equal(bodyTok,";")){
        info.next();
        ERR_HEAD3(bodyTok,"Operator overloading must have a body. You have forgotten the body when defining '"<<function->name<<"'.\n\n";
            ERR_LINE2(bodyTok.tokenIndex,"replace with {}");
        )
    } else if(Equal(bodyTok,"{")){
        info.functionScopes.add({});
        ASTScope* body = 0;
        SignalDefault result = ParseBody(info,body, function->scopeId);
        info.functionScopes.pop();
        function->body = body;
    } else {
        ERR_HEAD3(bodyTok,"Operator has no body! Did the return types parse incorrectly? Use curly braces to define the body.\n\n";
            ERR_LINE2(bodyTok.tokenIndex,"expected {");
        )
    }

    return SignalAttempt::SUCCESS;
}
SignalAttempt ParseFunction(ParseInfo& info, ASTFunction*& function, bool attempt, ASTStruct* parentStruct){
    using namespace engone;
    MEASURE;
    _PLOG(FUNC_ENTER)
    Token fnToken = info.get(info.at()+1);
    // int startIndex = info.at()+1;
    if(!Equal(fnToken,"fn")){
        if(attempt) return SignalAttempt::BAD_ATTEMPT;
        ERR_HEAD3(fnToken, "Expected fn for function not '"<<fnToken<<"'.\n";

        )
            
        return SignalAttempt::FAILURE;
    }
    info.next();
    attempt = false;

    function = info.ast->createFunction();
    function->tokenRange.firstToken = fnToken;
    function->tokenRange.endIndex = info.at()+1;;

    bool needsExplicitCallConvention = false;
    bool specifiedConvention = false;
    Token linkToken{};

    Token name = info.next();
    while (IsAnnotation(name)){
        // NOTE: The reason that @extern-stdcall is used instead of @extern @stdcall is to prevent
        //   the programmer from making mistakes. With two annotations, they may forget to specify the calling convention.
        //   Calling convention is very important. If extern and call convention is combined then they won't forget.
        if(Equal(name,"@hide")){
            function->hidden=true;
        } else if (Equal(name,"@dllimport")){
            function->callConvention = CallConventions::STDCALL;
            specifiedConvention = true;
            function->linkConvention = LinkConventions::DLLIMPORT;
            needsExplicitCallConvention = true;
            linkToken = name;
        } else if (Equal(name,"@import")){
            function->callConvention = CallConventions::STDCALL;
            specifiedConvention = true;
            function->linkConvention = LinkConventions::IMPORT;
            needsExplicitCallConvention = true;
            linkToken = name;
        } else if (Equal(name,"@stdcall")){
            function->callConvention = CallConventions::STDCALL;
            specifiedConvention = true;
        // cdecl has not been implemented. It doesn't seem important.
        // default x64 calling convention is used.
        // } else if (Equal(name,"@cdecl")){
        //     Assert(false); // not implemented in type checker and generator. type checker might work as is.
        //     function->callConvention = CallConventions::CDECL_CONVENTION;
        //     specifiedConvention = true;
        } else if (Equal(name,"@betcall")){
            function->callConvention = CallConventions::BETCALL;
            function->linkConvention = LinkConventions::NONE;
            specifiedConvention = true;
        // IMPORTANT: When adding calling convention, do not forget to add it to the "Did you mean" below!
        } else if (Equal(name,"@native")){
            function->linkConvention = NATIVE;
        } else if (Equal(name,"@intrinsic")){
            function->callConvention = CallConventions::INTRINSIC;
            function->linkConvention = NATIVE;
        } else {
            // It should not warn you because it is quite important that you use the right annotations with functions
            // Mispelling external or the calling convention would be very bad.
            ERR_HEAD3(name, "'"<< Token(name.str+1,name.length-1) << "' is not a known annotation for functions.\n\n";
                ERR_LINE2(info.at(),"unknown");

                // if(StartsWith(name, "@extern")) {
                //     log::out << log::YELLOW << "Did you mean @extern-stdcall or @extern-cdecl\n";
                // }
            )
        }
        name = info.next();
        continue;
    }
    if(function->linkConvention != LinkConventions::NONE){
        function->hidden = true;
        // native doesn't use a calling convention
        if(needsExplicitCallConvention && !specifiedConvention){
            ERR_HEAD3(name, "You must specify a calling convention. The default is betcall which you probably don't want. Use @stdcall or @cdecl instead.\n\n";
                ERR_LINE2(name.tokenIndex, "missing call convention");
            )
        }
    }
    
    if(!IsName(name)){
        info.ast->destroy(function);
        function = nullptr;
        ERR_HEAD3(name,"expected a valid name, "<<name<<" is not.\n";)
        return SignalAttempt::FAILURE;
    }
    function->name = name;

    ScopeInfo* funcScope = info.ast->createScope(info.currentScopeId);
    function->scopeId = funcScope->id;

    // ensure we leave this parse function with the same scope we entered with
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
            SignalDefault result = ParseTypeId(info, tok, nullptr);
            if(result == SignalDefault::SUCCESS) {
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
                ERR_HEAD3(tok, "expected , or > for in poly. arguments for function "<<name<<"\n";
            )
                // parse error or what?
                break;
            }
        }
    }
    tok = info.next();
    if(!Equal(tok,"(")){
        ERR_HEAD3(tok, "expected ( not "<<tok<<"\n";
            )
        return SignalAttempt::FAILURE;
    }

    if(parentStruct) {
        function->arguments.add({});
        auto& argv = function->arguments[function->arguments.size()-1];
        argv.name = "this";
        argv.name.tokenStream = info.tokens;
        argv.name.tokenIndex = tok.tokenIndex;
        argv.name.line = tok.line;
        argv.name.column = tok.column+1;
        argv.defaultValue = nullptr;

        std::string* str = info.ast->createString();
        *str = parentStruct->polyName + "*"; // TODO: doesn't work with polymorhpism
        argv.stringType = info.ast->getTypeString(*str);;
    }
    bool printedErrors=false;
    bool mustHaveDefault=false;
    TokenRange prevDefault={};
    WHILE_TRUE {
        Token& arg = info.get(info.at()+1);
        if(Equal(arg,")")){
            info.next();
            break;
        }
        if(!IsName(arg)){
            info.next();
            if(!printedErrors) {
                printedErrors=true;
                ERR_HEAD3(arg, "'"<<arg <<"' is not a valid argument name.\n\n";
                    ERR_LINE2(arg.tokenIndex,"bad");
                )
            }
            continue;
            // return PARSE_ERROR;
        }
        info.next();
        tok = info.get(info.at()+1);
        if(!Equal(tok,":")){
            if(!printedErrors) {
                printedErrors=true;
                ERR_HEAD3(tok, "Expected : not "<<tok <<".\n\n";
                    ERR_LINE2(tok.tokenIndex,"bad");
                )
            }
            continue;
            // return PARSE_ERROR;
        }
        info.next();

        Token dataType{};
        SignalDefault result = ParseTypeId(info,dataType, nullptr);
        Assert(result == SignalDefault::SUCCESS);
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
            
            SignalAttempt result = ParseExpression(info,defaultValue,false);
            if(result!=SignalAttempt::SUCCESS){
                continue;
            }
            prevDefault = defaultValue->tokenRange;

            mustHaveDefault=true;
        } else if(mustHaveDefault){
            // printedErrors doesn't matter here.
            // If we end up here than our parsing is probably correct so far and we might as
            // well log this error since it's probably a "real" error not caused by a cascade.
            ERR_HEAD3(tok, "Expected a default argument because of previous default argument "<<prevDefault<<" at "<<prevDefault.firstToken.line<<":"<<prevDefault.firstToken.column<<".\n\n";
                ERR_LINE2(tok.tokenIndex,"bad");
            )
            // continue; we don't continue since we want to parse comma if it exists
        }
        if(!defaultValue)
            function->nonDefaults++;

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
            ERR_HEAD3(tok, "Expected , or ) not "<<tok <<".\n\n";
                ERR_LINE2(tok.tokenIndex,"bad");
            )
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
        
        WHILE_TRUE {
            
            Token tok = info.get(info.at()+1);
            if(Equal(tok,"{") || Equal(tok,";")){
                break;   
            }
            
            Token dt{};
            SignalDefault result = ParseTypeId(info,dt, nullptr);
            if(result!=SignalDefault::SUCCESS){
                break; // prevent infinite loop
                // info.next();
                // continue as we have
                // continue;
                // return PARSE_ERROR;
                // printedErrors = false;
            } else {
                TypeId strId = info.ast->getTypeString(dt);
                function->returnValues.add({});
                function->returnValues.last().stringType = strId;
                function->returnValues.last().valueToken = dt;
                function->returnValues.last().valueToken.endIndex = info.at()+1;
                printedErrors = false;
            }
            tok = info.get(info.at()+1);
            if(Equal(tok,"{") || Equal(tok,";")){
                // info.next(); { is parsed in ParseBody
                break;   
            } else if(Equal(tok,",")){
                info.next();
                continue;
            } else {
                if(function->linkConvention != LinkConventions::NONE)
                    break;
                if(!printedErrors){
                    printedErrors=true;
                    ERR_HEAD3(tok, "Expected a comma or curly brace. '"<<tok <<"' is not okay.\n";
                        ERR_LINE2(tok.tokenIndex,"bad coder");
                    )
                }
                continue;
                // Continuing since we are inside of return values and expect
                // something to end it
            }
        }
    }
    function->tokenRange.endIndex = info.at()+1; // don't include body in function's token range
    // the body's tokenRange can be accessed with function->body->tokenRange
    
    Token& bodyTok = info.get(info.at()+1);
    
    if(Equal(bodyTok,";")){
        info.next();
        if(function->needsBody()){
            ERR_HEAD3(bodyTok,"Functions must have a body. You have forgotten the body when defining '"<<function->name<<"'. Declarations and definitions happen at the same time in this language. This is possible because of out of order compilation.\n\n";
                ERR_LINE2(bodyTok.tokenIndex,"replace with {}");
            )
        }
    } else if(Equal(bodyTok,"{")){
        if(!function->needsBody()) {
            ERR_HEAD3(bodyTok,"Native/external functions cannot have a body. Native functions are handled by the language. External functions link to functions outside your source code.\n\n";
                ERR_LINE2(bodyTok.tokenIndex,"use ; instead");
            )
        }
        
        info.functionScopes.add({});
        ASTScope* body = 0;
        SignalDefault result = ParseBody(info,body, function->scopeId);
        info.functionScopes.pop();
        function->body = body;
    } else if(function->needsBody()) {
        ERR_HEAD3(bodyTok,"Function has no body! Did the return types parse incorrectly? Use curly braces to define the body.\n\n";
            ERR_LINE2(bodyTok.tokenIndex,"expected {");
        )
    }

    return SignalAttempt::SUCCESS;
}
SignalAttempt ParseAssignment(ParseInfo& info, ASTStatement*& statement, bool attempt){
    using namespace engone;
    MEASURE;
    _PLOG(FUNC_ENTER)
    
    // if(info.tokens->length() < info.index+2){
    //     // not enough tokens for assignment
    //     if(attempt){
    //         return PARSE_BAD_ATTEMPT;
    //     }
    //     ERR_HEAD3()
    //     _PLOG(log::out <<log::RED<< "CompilerError: to few tokens for assignment\n";)
    //     // info.nextLine();
    //     return PARSE_ERROR;
    // }
    // int error=PARSE_SUCCESS;

    // to make things easier attempt is checked here
    if(attempt && !(IsName(info.get(info.at()+1)) && (Equal(info.get(info.at()+2),",") || Equal(info.get(info.at()+2),":") || Equal(info.get(info.at()+2),"=") ))){
        return SignalAttempt::BAD_ATTEMPT;
    }
    attempt = true;

    //-- Evaluate variables on the left side
    int startIndex = info.at()+1;
    DynamicArray<ASTStatement::VarName> varnames{};
    while(!info.end()){
        Token name = info.get(info.at()+1);
        if(!IsName(name)){
            ERR_HEAD3(name, "Expected a valid name for assignment ('"<<name<<"' is not).\n\n";
                ERR_LINE2(info.at()+1,"cannot be a name");
            )
            return SignalAttempt::FAILURE;
        }
        info.next();

        varnames.add({name});
        Token token = info.get(info.at()+1);
        if(Equal(token,":")){
            info.next(); // :
            attempt=false;
            
            Token typeToken{};
            SignalDefault result = ParseTypeId(info,typeToken, nullptr);
            Assert(result==SignalDefault::SUCCESS);
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
                        ERR_HEAD3(token2, "Array cannot have negative size.\n\n";
                            ERR_LINE2(info.at()-1,"< 0");
                        )
                        arrayLength = 0;
                    }
                } else {
                    ERR_HEAD3(token2, "The length of an array can only be specified with number literals. Use macros to avoid magic numbers. Constants have not been implemented but when they have, they will work too.\n\n";
                        ERR_LINE2(info.at()-1, "must be positive integer literal");
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
            while(index>=0 && !varnames[index].assignString.isValid()){
                varnames[index].assignString = strId;
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

    statement = info.ast->createStatement(ASTStatement::ASSIGN);
    statement->varnames.stealFrom(varnames);
    statement->firstExpression = nullptr;

    Token token = info.get(info.at()+1);
    if(Equal(token,"=")) {
        info.next(); // =

        SignalAttempt result = ParseExpression(info,statement->firstExpression,false);

    } else if(Equal(token,";")){
        info.next(); // parse ';'. won't crash if at end
    }

    statement->tokenRange.firstToken = info.get(startIndex);
    statement->tokenRange.endIndex = info.at()+1;
    return SignalAttempt::SUCCESS;  
}
SignalDefault ParseBody(ParseInfo& info, ASTScope*& bodyLoc, ScopeId parentScope, bool trulyGlobal){
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
    
    ScopeId prevScope = info.currentScopeId;
    defer { info.currentScopeId = prevScope; }; // ensure that we exit with the same scope we came in with

    bool scoped=false;
    if(bodyLoc){

    } else {
        bodyLoc = info.ast->createBody();

        if(!info.end()){
            Token token = info.get(info.at()+1);
            if(Equal(token,"{")) {
                token = info.next();
                scoped=true;
                ScopeInfo* scopeInfo = info.ast->createScope(parentScope);
                bodyLoc->scopeId = scopeInfo->id;

                info.currentScopeId = bodyLoc->scopeId;
            }
        }
        if(!scoped){
            bodyLoc->scopeId = info.currentScopeId;
        }
    }

    if(!info.end()){
        bodyLoc->tokenRange.firstToken = info.get(info.at()+1);
        // endToken is set later
    }

    DynamicArray<ASTStatement*> nearDefers{};

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
        
        SignalAttempt result=SignalAttempt::BAD_ATTEMPT;

        if(Equal(token,"{")){
            ASTScope* body=0;
            SignalDefault result = ParseBody(info,body, info.currentScopeId);
            if(result!=SignalDefault::SUCCESS){
                info.next(); // skip { to avoid infinite loop
                continue;
            }
            tempStatement = info.ast->createStatement(ASTStatement::BODY);
            tempStatement->firstBody = body;
            tempStatement->tokenRange = body->tokenRange;
        }
        if(result==SignalAttempt::BAD_ATTEMPT)
            result = ParseFunction(info,tempFunction,true, nullptr);
        if(result==SignalAttempt::BAD_ATTEMPT)
            result = ParseOperator(info,tempFunction,true);
        if(result==SignalAttempt::BAD_ATTEMPT)
            result = ParseStruct(info,tempStruct,true);
        if(result==SignalAttempt::BAD_ATTEMPT)
            result = ParseEnum(info,tempEnum,true);
        if(result==SignalAttempt::BAD_ATTEMPT)
            result = ParseNamespace(info,tempNamespace,true);
        if(result==SignalAttempt::BAD_ATTEMPT)
            result = ParseAssignment(info,tempStatement,true);
        if(result==SignalAttempt::BAD_ATTEMPT)
            result = ParseFlow(info,tempStatement,true);
        if(result==SignalAttempt::BAD_ATTEMPT){
            // bad name of function? it parses an expression
            // prop assignment or function call
            ASTExpression* expr=nullptr;
            result = ParseExpression(info,expr,true);
            if(expr){
                tempStatement = info.ast->createStatement(ASTStatement::EXPRESSION);
                tempStatement->firstExpression = expr;
                tempStatement->tokenRange = expr->tokenRange;
            }
        }

        if(result==SignalAttempt::BAD_ATTEMPT){
            if(IsAnnotation(token)){
                 {
                    WARN_HEAD3(token, "'"<< Token(token.str+1,token.length-1) << "' is not a known annotation for bodies.\n\n";
                        WARN_LINE2(info.at()+1,"unknown");
                    )
                }
                info.next();
            } else {
                Token& token = info.get(info.at()+1);
                ERR_HEAD3(token, "Unexpected '"<<token<<"' (ParseBody).\n\n";
                    ERR_LINE2(info.at()+1,"what");
                )
                // prevent infinite loop. Loop only occurs when scoped
                info.next();
            }
        }
        if(result!=SignalAttempt::SUCCESS){
            // TODO: What should be done?
        }
        // We add the AST structures even during error to
        // avoid leaking memory.
        if(tempStatement) {
            if(tempStatement->type == ASTStatement::DEFER) {
                nearDefers.add(tempStatement);
                if(info.functionScopes.last().loopScopes.size()==0){
                    info.functionScopes.last().defers.add(tempStatement);
                } else {
                    info.functionScopes.last().loopScopes.last().defers.add(tempStatement);
                }
            } else {
                if(tempStatement->type == ASTStatement::RETURN) {
                    for(int j=info.functionScopes.last().loopScopes.size()-1;j>=0;j--){
                        auto& loopScope = info.functionScopes.last().loopScopes[j];
                        for(int k=0;k<loopScope.defers.size();k++){
                            auto myDefer = loopScope.defers[k];  // my defer, not yours, get your own
                            ASTStatement* deferCopy = info.ast->createStatement(ASTStatement::DEFER);
                            deferCopy->tokenRange = myDefer->tokenRange;
                            deferCopy->firstBody = myDefer->firstBody;
                            deferCopy->sharedContents = true;

                            bodyLoc->add(deferCopy);
                        }
                    }
                    for(int j=info.functionScopes.last().defers.size()-1;j>=0;j--){
                        auto myDefer = info.functionScopes.last().defers[j];  // my defer, not yours, get your own
                        ASTStatement* deferCopy = info.ast->createStatement(ASTStatement::DEFER);
                        deferCopy->tokenRange = myDefer->tokenRange;
                        deferCopy->firstBody = myDefer->firstBody;
                        deferCopy->sharedContents = true;

                        bodyLoc->add(deferCopy);
                    }
                } else if(tempStatement->type == ASTStatement::CONTINUE || tempStatement->type == ASTStatement::BREAK){
                    if(info.functionScopes.last().loopScopes.size()!=0){
                        auto& loopScope = info.functionScopes.last().loopScopes.last();
                        for(int k=0;k<loopScope.defers.size();k++){
                            auto myDefer = loopScope.defers[k];  // my defer, not yours, get your own
                            ASTStatement* deferCopy = info.ast->createStatement(ASTStatement::DEFER);
                            deferCopy->tokenRange = myDefer->tokenRange;
                            deferCopy->firstBody = myDefer->firstBody;
                            deferCopy->sharedContents = true;

                            bodyLoc->add(deferCopy);
                        }
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
        if(result==SignalAttempt::BAD_ATTEMPT){
            // Try again. Important because loop would break after one time if scoped is false.
            // We want to give it another go.
            continue;
        }
        // current body may have the same "parentScope == bodyLoc->scopeId" will make sure break 
        if(!scoped && !trulyGlobal){
            break;
        }
    }
    
    for(int i = nearDefers.size()-1;i>=0;i--) {
        if(info.functionScopes.last().loopScopes.size()!=0){
            auto& loopScope = info.functionScopes.last().loopScopes.last();
            for(int j=loopScope.defers.size()-1;j>=0;j--) {
                auto& deferNode = info.functionScopes.last().loopScopes.last().defers[j];
                if(deferNode == nearDefers[i]){
                    loopScope.defers.remove(j);
                    // if(j==loopScope.defers.size()-1){
                    //     loopScope.defers.pop();
                    // } else {
                    //     loopScope.defers.remove(j);
                    // }
                }
            }
        }
        bodyLoc->add(nearDefers[i]);
    }
    // nearDefers.cleanup(); // cleaned by destructor



    bodyLoc->tokenRange.endIndex = info.at()+1;
    return SignalDefault::SUCCESS;
}

ASTScope* ParseTokenStream(TokenStream* tokens, AST* ast, CompileInfo* compileInfo, std::string theNamespace){
    using namespace engone;
    MEASURE;
    // _VLOG(log::out <<log::BLUE<<  "##   Parser   ##\n";)
    
    ParseInfo info{tokens};
    info.tokens = tokens;
    info.ast = ast;
    info.compileInfo = compileInfo;

    ASTScope* body = nullptr;
    if(theNamespace.size()==0){
        body = info.ast->createBody();
        body->scopeId = ast->globalScopeId;
    } else {
        // TODO: Should namespaced imports from a source file you import as a namespace
        //  have global has their parent? It does now, ast->globalScopeId is used.
        ScopeInfo* newScope = info.ast->createScope(ast->globalScopeId);

        newScope->name = theNamespace;
        info.ast->getScope(newScope->parent)->nameScopeMap[theNamespace] = newScope->id;

        body = info.ast->createNamespace(theNamespace);
        body->scopeId = newScope->id;
        
    }
    info.currentScopeId = body->scopeId;

    info.functionScopes.add({});
    SignalDefault result = ParseBody(info, body, ast->globalScopeId, true);
    info.functionScopes.pop();
    
    return body;
}