#include "BetBat/Parser.h"
#include "BetBat/Compiler.h"

#undef WARN_HEAD3
#define WARN_HEAD3(T, M) info.compileInfo->compileOptions->compileStats.warnings++;engone::log::out << WARN_CUSTOM(info.tokens->streamName,T.line,T.column,"Parse warning","W0000") << M

#undef WARN_LINE2
#define WARN_LINE2(I, M) PrintCode(I, info.tokens, M)

#undef ERR_SECTION
#define ERR_SECTION(CONTENT) BASE_SECTION("Parser, ", CONTENT)


#undef WARN_SECTION
#define WARN_SECTION(CONTENT) BASE_WARN_SECTION("Parser, ", CONTENT)

/*
    Declaration of functions
*/
enum ParseFlags : u32 {
    PARSE_NO_FLAGS = 0x0,
    // INPUT
    PARSE_INSIDE_SWITCH = 0x1,
    PARSE_TRULY_GLOBAL = 0x2,
    // OUTPUT
    PARSE_HAS_CURLIES = 0x4,
    PARSE_HAS_CASE_FALL = 0x8, // annotation @fall for switch cases
};
ASTScope* ParseTokenStream(TokenStream* tokens, AST* ast, CompileInfo* compileInfo, std::string theNamespace);
// out token contains a newly allocated string. use delete[] on it
SignalDefault ParseBody(ParseInfo& info, ASTScope*& bodyLoc, ScopeId parentScope, ParseFlags in_flags = PARSE_NO_FLAGS, ParseFlags* out_flags = nullptr);
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
SignalDefault ParseTypeId(ParseInfo& info, Token& outTypeId, int* tokensParsed);


/*
    Code for the functions
*/
// returns instruction type
// zero means no operation
bool IsSingleOp(OperationType nowOp){
    return nowOp == AST_REFER || nowOp == AST_DEREF || nowOp == AST_NOT || nowOp == AST_BNOT||
        nowOp == AST_FROM_NAMESPACE || nowOp == AST_CAST || nowOp == AST_INCREMENT || nowOp == AST_DECREMENT || nowOp == AST_UNARY_SUB;
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
        // All unary operators have the same precedence
        // It wouldn't make sense for !~0 to be rearranged to ~!0
        case AST_BNOT:
        case AST_NOT:
            // return 15;
        case AST_CAST:
        case AST_REFER:
        case AST_DEREF:
            // return 16;
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
    // return index>=tokens->length();
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
            ERR_SECTION(
                ERR_HEAD(tok)
                ERR_MSG("Cannot have quotes in data type "<<tok<<".")
                ERR_LINE(tok,"bad")
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
                ||Equal(tok,"{") // () -> i32 {   enum A : i32 {
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
    Token tok = info.get(endTok);
    if(Equal(tok,"*")){
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
    if(!Equal(structToken,"struct")){ // TODO: Allow union
        if(attempt) return SignalAttempt::BAD_ATTEMPT;
        ERR_SECTION(
            ERR_HEAD(structToken)
            ERR_MSG("expected struct not "<<structToken<<".")
            ERR_LINE(structToken ,"bad")
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
        ERR_SECTION(
            ERR_HEAD(name)
            ERR_MSG("Expected a name, "<<name<<" isn't.")
            ERR_LINE(name,"bad")
        )
        return SignalAttempt::FAILURE;
    }
    info.next(); // name

    astStruct = info.ast->createStruct(name);
    astStruct->tokenRange.firstToken = structToken;
    // astStruct->tokenRange.startIndex = startIndex;
    // astStruct->tokenRange.tokenStream = info.tokens;
    astStruct->polyName = name;
    astStruct->setHidden(hideAnnotation);
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
                    ERR_SECTION(
                        ERR_HEAD(token)
                        ERR_MSG("empty polymorph list.")
                        ERR_LINE(token,"bad");
                    )
                }
                break;
            }
            info.next(); // must skip name to prevent infinite loop
            if(!IsName(token)){
                ERR_SECTION(
                    ERR_HEAD(token)
                    ERR_MSG("'"<<token<<"' is not a valid name for polymorphism. (TODO: Invalidate the struct somehow?)")
                    ERR_LINE(token,"bad");
                )
            }else{
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
                ERR_SECTION(
                    ERR_HEAD(token)
                    ERR_MSG("'"<<token<<"' is neither , or >.")
                    ERR_LINE(token,"bad")
                )
                continue;
            }
        }
        astStruct->polyName += ">";
    }
    
    token = info.get(info.at()+1);
    if(!Equal(token,"{")){
        ERR_SECTION(
            ERR_HEAD(token)
            ERR_MSG("Expected { not "<<token<<".")
            ERR_LINE(token,"bad");
        )
        return SignalAttempt::FAILURE;
    }
    info.next();
    
    // TODO: Structs need their own scopes for polymorphic types and
    //  internal namespaces, structs, enums...

    // Func
    ScopeInfo* structScope = info.ast->createScope(info.currentScopeId, info.getNextOrder(), nullptr);
    astStruct->scopeId = structScope->id;

    // ensure scope is the same when returning
    auto prevScope = info.currentScopeId;
    defer {info.currentScopeId = prevScope; };
    info.currentScopeId = astStruct->scopeId;

    int offset=0;
    auto errorParsingMembers = SignalAttempt::SUCCESS;
    bool hadRecentError = false;
    bool affectedByAlignment=false;
    int union_depth = 0;
    // TODO: May want to change the structure of the while loop
    //  struct { a 9 }   <-  : is expected and not 9, function will quit but that's not good because we still have }.
    //  that will cause all sorts of issue outside. leaving this parse function when } is reached even if errors occured
    //  inside is probably better.
    WHILE_TRUE {
        Token name = info.get(info.at()+1);

        if(name == END_TOKEN) {
            ERR_SECTION(
                ERR_HEAD(info.get(info.at()))
                ERR_MSG("Sudden end when parsing struct.")
                ERR_LINE(structToken,"this struct")
                ERR_LINE(info.get(info.at()), "should this token be inside the struct?")
            )
            
            return SignalAttempt::COMPLETE_FAILURE;   
        }

        if(Equal(name,";")){
            info.next();
            continue;
        } else if(Equal(name,"}")){
            info.next();
            if(union_depth == 0) {
                break;
            }
            union_depth--;
            continue;
        }
        bool wasFunction = false;
        Token typeToken{};
        int typeEndToken = -1;
        if (Equal(name,"fn")) {
            wasFunction = true;
            // parse function?
            ASTFunction* func = 0;
            SignalAttempt result = ParseFunction(info, func, false, astStruct);
            if(result!=SignalAttempt::SUCCESS){
                continue;
            }
            func->parentStruct = astStruct;
            astStruct->add(func);
            // ,func->polyArgs.size()==0?&func->baseImpl:nullptr);
        } else if(Equal(name, "union")) {
            info.next();
            
            Assert(union_depth < 1); // nocheckin, support nested unions and structs
            
            Token tok = info.get(info.at()+1);
            if(IsName(tok)) {
                ERR_SECTION(
                    ERR_HEAD(tok)
                    ERR_MSG("Named unions not allowed inside structs.")
                    ERR_LINE(tok,"bad")
                )
                info.next(); // skip it, continue as normal
            }
            
            tok = info.get(info.at()+1);
            
            if(Equal(tok,"{")) {
                info.next();
                union_depth++;
                continue;
            } else {
                ERR_SECTION(
                    ERR_HEAD(tok)
                    ERR_MSG("Expected 'union {' but there was no curly brace.")
                    ERR_LINE(tok,"should be a curly brace")
                )
                // what do we do?
            }
        } else if(Equal(name, "struct")) {
            ERR_SECTION(
                ERR_HEAD(name)
                ERR_MSG("Structs not allowed inside structs.")
                ERR_LINE(name,"bad")
            )
            Assert(("struct definitin in struct not allowed",false)); // error instead
        } else if(IsName(name)) {
            for(auto& mem : astStruct->members) {
                if(mem.name == name) {
                    ERR_SECTION(
                        ERR_HEAD(name)
                        ERR_MSG("The name '"<<name<<"' is already used in another member of the struct. You cannot have two members with the same name.")
                        ERR_LINE(mem.name, "last")
                        ERR_LINE(name, "now")
                    )
                    break;
                }
            }
            info.next();   
            Token token = info.get(info.at()+1);
            if(!Equal(token,":")){
                ERR_SECTION(
                    ERR_HEAD(token)
                    ERR_MSG("Expected : not "<<token<<".")
                    ERR_LINE(token,"bad")
                )
                errorParsingMembers = SignalAttempt::FAILURE;
                continue;
            }    
            info.next();
            SignalDefault result = ParseTypeId(info,typeToken,nullptr);
            if(result!=SignalDefault::SUCCESS) {
                if(typeToken.str) {
                    ERR_SECTION(
                        ERR_HEAD(typeToken)
                        ERR_MSG("Failed parsing type '"<<typeToken<<"'.")
                        ERR_LINE(typeToken,"bad")
                    )
                } else {
                    Token tok = info.get(info.at() + 1);
                    ERR_SECTION(
                        ERR_HEAD(tok)
                        ERR_MSG("Failed parsing type '"<<tok<<"'.")
                        ERR_LINE(tok,"bad")
                    )
                }
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
                        ERR_SECTION(
                            ERR_HEAD(token2)
                            ERR_MSG("Array cannot have negative size.")
                            ERR_LINE(info.get(info.at()-1),"< 0")
                        )
                        arrayLength = 0;
                    }
                } else {
                    ERR_SECTION(
                        ERR_HEAD(token2)
                        ERR_MSG("The length of an array can only be specified with number literals. Use macros to avoid magic numbers. Constants have not been implemented but when they have, they will work too.")
                        ERR_LINE(info.get(info.at()-1), "must be positive integer literal")
                    )
                }
                std::string* str = info.ast->createString();
                *str = "Slice<";
                *str += typeToken;
                *str += ">";
                typeToken = *str;
            }
            Assert(arrayLength==-1);
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
        } else {
            info.next();
            if(!hadRecentError){
                ERR_SECTION(
                    ERR_HEAD(name)
                    ERR_MSG("Expected a name, "<<name<<" isn't.")
                    ERR_LINE(name,"bad")
                )
            }
            hadRecentError=true;
            errorParsingMembers = SignalAttempt::FAILURE;
            continue;   
        }
        Token token = info.get(info.at()+1);
        if(Equal(token,"fn") || Equal(token, "struct") || Equal(token, "union")){
            // semi-colon not required
            continue;
        }else if(Equal(token,";")){
            info.next();
            hadRecentError=false;
            continue;
        }else if(Equal(token,"}")){
            continue; // handle break at beginning of loop
        }else{
            if(wasFunction)
                continue;
            if(!hadRecentError){ // no need to print message again, the user already know there are some issues here.
                ERR_SECTION(
                    ERR_HEAD(token)
                    ERR_MSG("Expected a curly brace or semi-colon to mark the end of the member (was: "<<token<<").")
                    ERR_LINE(token,"bad")
                )
                // ERR_SECTION(
                // ERR_HEAD(token,"Expected a curly brace or semi-colon to mark the end of the member (was: "<<token<<").\n\n";
                //     if(typeEndToken!=-1){
                //         TokenRange temp{};
                //         temp.firstToken = typeToken;
                //         temp.endIndex = typeEndToken;
                //         PrintCode(temp,"evaluated type");
                //     }
                //     ERR_LINE(token.tokenIndex,"bad");
                // )
            }
            hadRecentError=true;
            errorParsingMembers = SignalAttempt::FAILURE;
            continue;
        }
    }
    astStruct->tokenRange.endIndex = info.at()+1;
    _GLOG(log::out << "Parsed struct "<<log::LIME<< name <<log::NO_COLOR << " with "<<astStruct->members.size()<<" members\n";)
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
        ERR_SECTION(
            ERR_HEAD(token)
            ERR_MSG("Expected namespace not "<<token<<".")
            ERR_LINE(token,"bad")
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
        ERR_SECTION(
            ERR_HEAD(name)
            ERR_MSG("Expected a name, "<<name<<" isn't.")
            ERR_LINE(name,"bad");
        )
        return SignalAttempt::FAILURE;
    }
    info.next();

    token = info.get(info.at()+1);
    if(!Equal(token,"{")){
        ERR_SECTION(
            ERR_HEAD(token)
            ERR_MSG("Expected { not "<<token<<".")
            ERR_LINE(token,"bad")
        )
        return SignalAttempt::FAILURE;
    }
    info.next();
    
    int nextId=0;
    astNamespace = info.ast->createNamespace(name);
    astNamespace->tokenRange.firstToken = token;
    // astNamespace->tokenRange.startIndex = startIndex;
    // astNamespace->tokenRange.tokenStream = info.tokens;
    astNamespace->setHidden(hideAnnotation);

    ScopeInfo* newScope = info.ast->createScope(info.currentScopeId, info.getNextOrder(), astNamespace);
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
            ERR_SECTION(
                ERR_HEAD(token)
                ERR_MSG("Unexpected '"<<token<<"' (ParseNamespace).")
                ERR_LINE(token,"bad")
            )
            
            // test other parse type
            info.next(); // prevent infinite loop
        }
        // if(result==SignalAttempt::FAILURE){
            
        // }
            
        // We add the AST structures even during error to
        // avoid leaking memory.
        if(tempNamespace) {
            astNamespace->add(info.ast, tempNamespace);
        }
        if(tempFunction) {
            astNamespace->add(info.ast, tempFunction);
        }
        if(tempStruct) {
            astNamespace->add(info.ast, tempStruct);
        }
        if(tempEnum) {
            astNamespace->add(info.ast, tempEnum);
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
        ERR_SECTION(
            ERR_HEAD(enumToken)
            ERR_MSG("Expected struct not "<<enumToken<<".")
            ERR_LINE(enumToken,"bad");
        )
        return SignalAttempt::FAILURE;   
    }
    attempt=false;
    info.next(); // enum

    bool hideAnnotation = false;
    Token name = info.get(info.at()+1);

    ASTEnum::Rules enumRules = ASTEnum::NONE;

    while (IsAnnotation(name)){
        info.next();
        if(Equal(name,"@hide")){
            hideAnnotation=true;
        } else if(Equal(name,"@specified")) {
            enumRules = (ASTEnum::Rules)(enumRules | ASTEnum::SPECIFIED);
        } else if(Equal(name,"@bitfield")) {
            enumRules = (ASTEnum::Rules)(enumRules | ASTEnum::BITFIELD);
        } else if(Equal(name,"@unique")) {
            enumRules = (ASTEnum::Rules)(enumRules | ASTEnum::UNIQUE);
        } else if(Equal(name,"@enclosed")) {
            enumRules = (ASTEnum::Rules)(enumRules | ASTEnum::ENCLOSED);
        } else {
            WARN_SECTION(
                WARN_HEAD(name)
                WARN_MSG("'"<< Token(name.str+1,name.length-1) << "' is not a known annotation for enums.")
                WARN_LINE(name,"unknown")
            )
        }
        name = info.get(info.at()+1);
        continue;
    }

    if(!IsName(name)){
        ERR_SECTION(
            ERR_HEAD(name)
            ERR_MSG("Expected a name, "<<name<<" isn't.")
            ERR_LINE(name,"bad")
        )
        return SignalAttempt::FAILURE;
    }
    info.next(); // name

    TypeId colonType = {};

    Token token = info.get(info.at()+1);
    if(Equal(token,":")) {
        info.next();

        Token tokenType{};
        SignalDefault res = ParseTypeId(info, tokenType, nullptr);
        if(res == SignalDefault::SUCCESS) {
            colonType = info.ast->getTypeString(tokenType);
        }
    }
    
    token = info.get(info.at()+1);
    if(!Equal(token,"{")){
        ERR_SECTION(
            ERR_HEAD(token)
            ERR_MSG("Expected { not "<<token<<":")
            ERR_LINE(token,"bad")
        )
        return SignalAttempt::FAILURE;
    }
    info.next();
    i64 nextValue=0;
    if(enumRules & ASTEnum::BITFIELD) {
        nextValue = 1;
    }
    astEnum = info.ast->createEnum(name);
    astEnum->rules = enumRules;
    if (colonType.isValid()) // use default type in astEnum if type isn't specified
        astEnum->colonType = colonType;
    astEnum->tokenRange.firstToken = enumToken;
    // astEnum->tokenRange.startIndex = startIndex;
    // astEnum->tokenRange.tokenStream = info.tokens;
    astEnum->setHidden(hideAnnotation);
    auto error = SignalAttempt::SUCCESS;
    WHILE_TRUE {
        Token name = info.get(info.at()+1);
        
        if(Equal(name,"}")){
            info.next();
            break;
        }

        bool ignoreRules = false;
        if(Equal(name, "@norules")) {
            info.next();
            ignoreRules = true;
        }

        name = info.get(info.at()+1);
        
        if(!IsName(name)){
            info.next(); // move forward to prevent infinite loop
            ERR_SECTION(
                ERR_HEAD(name)
                ERR_MSG("Expected a name, "<<name<<" isn't.")
                ERR_LINE(name,"bad")
            )
            error = SignalAttempt::FAILURE;
            continue;
        }
        name.flags &= ~TOKEN_MASK_SUFFIX;

        // bool semanticError = false;
        // if there was an error you could skip adding the member but that
        // cause a cascading effect of undefined AST_ID which are referring to the member 
        // that had an error. It's probably better to add the number but use a zero or something as value.

        info.next();   
        Token token = info.get(info.at()+1);
        if(Equal(token,"=")){
            info.next();
            token = info.get(info.at()+1);
            // TODO: Handle expressions
            // TODO: Handle negative values (automatically done if you fix expressions)
            if(IsInteger(token)){
                info.next();
                nextValue = ConvertInteger(token);
                token = info.get(info.at()+1);
            } else if(IsHexadecimal(token)){
                info.next();
                nextValue = ConvertHexadecimal(token);
                token = info.get(info.at()+1);
            } else if(token.length == 1 && (token.flags & TOKEN_SINGLE_QUOTED)) {
                info.next();
                nextValue = *token.str;
                token = info.get(info.at()+1);
            } else {
                ERR_SECTION(
                    ERR_HEAD(token)
                    ERR_MSG(token<<" is not an integer (i32).")
                    ERR_LINE(token,"bad")
                )
            }
        } else {
            if(!ignoreRules && (enumRules & ASTEnum::SPECIFIED)) {
                ERR_SECTION(
                    ERR_HEAD(name)
                    ERR_MSG("Enum uses @specified but the value of the member '"<<name<<"' was not specified. Explicitly give all members a value!")
                    ERR_LINE(name,"specify a value")
                )
            }
        }

        if(!ignoreRules && (enumRules & ASTEnum::UNIQUE)) {
            for(int i=0;i<astEnum->members.size();i++) {
                auto& mem = astEnum->members[i];
                if(mem.enumValue == nextValue) {
                    ERR_SECTION(
                        ERR_HEAD(name)
                        ERR_MSG("Enum uses @unique but the value of member '"<<name<<"' collides with member '"<<mem.name<<"'. There cannot be any duplicates!")
                        ERR_LINE(mem.name, "value "<<mem.enumValue)
                        ERR_LINE(name, "value "<<nextValue)
                    )
                }
            }
        }
        for(int i=0;i<astEnum->members.size();i++) {
            auto& mem = astEnum->members[i];
            if(mem.name == name) {
                ERR_SECTION(
                    ERR_HEAD(name)
                    ERR_MSG("Enum cannot have two members with the same name.")
                    ERR_LINE(mem.name, mem.name)
                    ERR_LINE(name, name)
                )
            }
        }
        Assert(nextValue < 0xFFFF'FFFF);
        astEnum->members.add({name,(int)(nextValue)});
        if(enumRules & ASTEnum::BITFIELD) {
            if(nextValue == 0)
                nextValue++;
            else {
                // we can't just do nextValue <<= 1;
                // if the value was user specified then it may be 7 which would result in 14
                u32 shifts = 0;
                u64 value = nextValue;
                while(value){
                    value >>= 1;
                    shifts++;
                }
                nextValue = (u64)1 << (shifts);
            }
        } else {
            nextValue++;
        }
        
        // token = info.get(info.at()+1);
        if(Equal(token,",")){
            info.next();
            continue;
        }else if(Equal(token,"}")){
            info.next();
            break;
        }else{
            ERR_SECTION(
                ERR_HEAD(token)
                ERR_MSG("Expected } or , not "<<token<<".")
                ERR_LINE(token,"bad");
            )
            error = SignalAttempt::FAILURE;
            continue;
        }
    }
    astEnum->tokenRange.endIndex = info.at()+1;

    // for(auto& mem : astEnum->members) {
    //     log::out << mem.name << " = " << mem.enumValue<<"\n";
    // }

    // auto typeInfo = info.ast->getTypeInfo(info.currentScopeId, name, false, true);
    // int strId = info.ast->getTypeString(name);
    // if(!typeInfo){
        // ERR_SECTION(
        // ERR_HEAD(name, name << " is taken\n";
        // info.ast->destroy(astEnum);
        // astEnum = 0;
        // return PARSE_ERROR;
    // }
    // typeInfo->astEnum = astEnum;
    // typeInfo->_size = 4; // i32
    _PLOG(log::out << "Parsed enum "<<log::LIME<< name <<log::NO_COLOR <<" with "<<astEnum->members.size()<<" members\n";)
    return error;
}
// out_arguments may be null to parse but ignore arguments
SignalDefault ParseAnnotationArguments(ParseInfo& info, TokenRange* out_arguments) {
    // MEASURE;

    if(out_arguments)
        *out_arguments = {};

    // Can the previous token have a suffix? probably not?
    Token& prev = info.get(info.at());
    if(prev.flags & (TOKEN_MASK_SUFFIX)) {
        return SignalDefault::SUCCESS;
    }

    Token& tok = info.get(info.at()+1);
    if(!Equal(tok,"(")){
        return SignalDefault::SUCCESS;
    }
    info.next(); // skip (

    int depth = 0;

    while(!info.end()) {
        Token& tok = info.get(info.at()+1);
        if(Equal(tok,")")){
            if(depth == 0) {
                info.next();
                break;
            }
            depth--;
        }
        if(Equal(tok,"(")){
            depth++;
        }

        if(out_arguments) {
            if(!out_arguments->firstToken.str) {
                *out_arguments = tok;
            } else {
                out_arguments->endIndex = info.at() + 1;
            }
        }

        info.next();
    }
    
    return SignalDefault::SUCCESS;
}
SignalDefault ParseAnnotation(ParseInfo& info, Token* out_annotation_name, TokenRange* out_arguments) {
    // MEASURE;

    *out_annotation_name = {};
    *out_arguments = {};

    Token& tok = info.get(info.at()+1);
    if(!IsAnnotation(tok)) {
        return SignalDefault::SUCCESS;
    }

    *out_annotation_name = info.next();

    // ParseAnnotationArguments check for suffix of the previous token
    // Args will not be parsed in cases like this: @hello (sad, arg) but will here: @hello(happy)
    SignalDefault result = ParseAnnotationArguments(info, out_arguments);

    Assert(result == SignalDefault::SUCCESS); // what do we do if args fail?
    return SignalDefault::SUCCESS;
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
            ERR_SECTION(
                ERR_HEAD(tok)
                ERR_MSG("Expected ',' to supply more arguments or ')' to end fncall (found "<<tok<<" instead).")
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
            ERR_SECTION(
                ERR_HEAD(tok)
                ERR_MSG("Expected named argument because of previous named argument "<<prevNamed << " at "<<prevNamed.line<<":"<<prevNamed.column<<".")
                ERR_LINE(tok,"bad")
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
        } else {
            fncall->nonNamedArgs++;
        }
        // fncall->args->add(expr);
        fncall->args.add(expr);
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

    // std::vector<ASTExpression*> values;
    // std::vector<Token> extraTokens; // ops, assignOps and castTypes doesn't store the token so you know where it came from. We therefore use this array.
    // std::vector<OperationType> ops;
    // std::vector<OperationType> assignOps;
    // std::vector<TypeId> castTypes;
    // std::vector<Token> namespaceNames;
    
    // TODO: This is bad, improved somehow
    TINY_ARRAY(ASTExpression*, values, 5);
    TINY_ARRAY(Token, extraTokens, 5); // ops, assignOps and castTypes doesn't store the token so you know where it came from. We therefore use this array.
    TINY_ARRAY(OperationType, ops, 5);
    TINY_ARRAY(OperationType, assignOps, 5);
    TINY_ARRAY(TypeId, castTypes, 5);
    TINY_ARRAY(Token, namespaceNames, 5);

    defer { for(auto e : values) info.ast->destroy(e); };

    bool shouldComputeExpression = false;

    Token& token = info.get(info.at()+1);
    if(Equal(token,"@run")) {
        info.next();
        shouldComputeExpression = true;
    }

    // bool negativeNumber=false;
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
                    ERR_SECTION(
                        ERR_HEAD(memberName)
                        ERR_MSG("Expected a property name, not "<<memberName<<".")
                        ERR_LINE(memberName,"bad")
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

                    // Create "this" argument in methods
                    // tmp->args->add(values.last());
                    tmp->args.add(values.last());
                    tmp->nonNamedArgs++;
                    values.pop();
                    
                    tmp->setMemberCall(true);

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

                    values.add(tmp);
                    continue;
                }
                if(polyTypes.length()!=0){
                    ERR_SECTION(
                        ERR_HEAD(info.now())
                        ERR_MSG("Polymorphic arguments indicates a method call put the parenthesis for it is missing. '"<<info.get(info.at()+1)<<"' is not '('.")
                    )
                }
                
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_MEMBER));
                tmp->name = memberName;
                tmp->left = values.last();
                values.pop();
                tmp->tokenRange.firstToken = tmp->left->tokenRange.firstToken;
                // tmp->tokenRange.startIndex = tmp->left->tokenRange.startIndex;
                tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
                values.add(tmp);
                continue;
            } else if (Equal(token,"=")) {
                info.next();

                ops.add(AST_ASSIGN);
                assignOps.add((OperationType)0);

                _PLOG(log::out << "Operator "<<token<<"\n";)
            } else if((opType = IsAssignOp(token)) && Equal(info.get(info.at()+2),"=")) {
                info.next();
                info.next();

                ops.add(AST_ASSIGN);
                assignOps.add(opType);

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

                ops.add(opType);

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
            //     tmp->tokenRange.startIndex = values.last()->tokenRange.startIndex;
            //     tmp->tokenRange.endIndex = info.at()+1;
            //     tmp->tokenRange.tokenStream = info.tokens;

            //     tmp->left = values.last();
            //     values.pop();
            //     tmp->right = rightExpr;
            //     values.add(tmp);
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
                    ERR_SECTION(
                        ERR_HEAD(tok)
                        ERR_MSG("bad")
                    )
                } else {
                    info.next();
                }

                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_INDEX));
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = startIndex;
                tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;

                tmp->left = values.last();
                values.pop();
                tmp->right = indexExpr;
                values.add(tmp);
                tmp->constantValue = tmp->left->constantValue && tmp->right->constantValue;
                continue;
            } else if(Equal(token,"++")){
                info.next();
                attempt = false;

                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_INCREMENT));
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
                tmp->setPostAction(true);

                tmp->left = values.last();
                values.pop();
                values.add(tmp);
                tmp->constantValue = tmp->left->constantValue;

                continue;
            } else if(Equal(token,"--")){
                info.next();
                attempt = false;

                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_DECREMENT));
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
                tmp->setPostAction(true);

                tmp->left = values.last();
                values.pop();
                values.add(tmp);
                tmp->constantValue = tmp->left->constantValue;

                continue;
            } else if(Equal(token,")")){
                // token = info.next();
                ending = true;
            } else {
                ending = true;
                if(Equal(token,";")){
                    // info.next();
                } else {
                    Token prev = info.now();
                    // Token next = info.get(info.at()+2);
                    if((prev.flags&TOKEN_SUFFIX_LINE_FEED) == 0 
                         && !Equal(token,"}") && !Equal(token,",") && !Equal(token,"{") && !Equal(token,"]")
                         && !Equal(prev,")"))
                    {
                        // WARN_HEAD3(token, "Did you forget the semi-colon to end the statement or was it intentional? Perhaps you mistyped a character? (put the next statement on a new line to silence this warning)\n\n"; 
                        //     ERR_LINE(tokenIndex-1, "semi-colon here?");
                        //     // ERR_LINE(tokenIndex, "; before this?");
                        //     )
                        // TODO: ERROR instead of warning if special flag is set
                    }
                }
            }
        }else{
            bool cstring = false;
            if(IsAnnotation(token)) {
                if(Equal(token, "@cstr")) {
                    info.next();

                    // TokenRange args; // TODO: Warn if cstr has errors?
                    // ParseAnnotationArguments(info, );

                    token = info.get(info.at() + 1);
                    cstring = true;
                }
            }
            if(Equal(token,"&")){
                info.next();
                ops.add(AST_REFER);
                attempt = false;
                continue;
            }else if(Equal(token,"*")){
                info.next();
                ops.add(AST_DEREF);
                attempt = false;
                continue;
            } else if(Equal(token,"!")){
                info.next();
                ops.add(AST_NOT);
                attempt = false;
                continue;
            } else if(Equal(token,"~")){
                info.next();
                ops.add(AST_BNOT);
                attempt = false;
                continue;
            } else if(Equal(token,"cast") || Equal(token,"cast_unsafe")) {
                info.next();
                Token tok = info.get(info.at()+1);
                if(!Equal(tok,"<")){
                    ERR_SECTION(
                        ERR_HEAD(tok)
                        ERR_MSG("Expected < not "<<tok<<".")
                        ERR_LINE(tok,"bad")
                    )
                    continue;
                }
                info.next();
                Token tokenTypeId{};
                SignalDefault result = ParseTypeId(info,tokenTypeId, nullptr);
                if(result!=SignalDefault::SUCCESS){
                    ERR_SECTION(
                        ERR_HEAD(tokenTypeId)
                        ERR_MSG(tokenTypeId << "is not a valid data type.")
                        ERR_LINE(tokenTypeId,"bad");
                    )
                }
                tok = info.get(info.at()+1);
                if(!Equal(tok,">")){
                    ERR_SECTION(
                        ERR_HEAD(tok)
                        ERR_MSG("expected > not "<< tok<<".")
                        ERR_LINE(tok,"bad");
                    )
                }
                info.next();
                ops.add(AST_CAST);
                // NOTE: unsafe cast is handled further down

                // TypeId dt = info.ast->getTypeInfo(info.currentScopeId,tokenTypeId)->id;
                // castTypes.add(dt);
                TypeId strId = info.ast->getTypeString(tokenTypeId);
                castTypes.add(strId);
                extraTokens.add(token);
                attempt=false;
                continue;
            } else if(Equal(token,"-")){
                info.next();
                ops.add(AST_UNARY_SUB);
                attempt = false;
                continue;
            } else if(Equal(token,"++")){
                info.next();
                ops.add(AST_INCREMENT);
                attempt = false;
                continue;
            } else if(Equal(token,"--")){
                info.next();
                ops.add(AST_DECREMENT);
                attempt = false;
                continue;
            }

            if(IsInteger(token)){
                token = info.next();

                bool negativeNumber = false;
                if (ops.size()>0 && ops.last() == AST_UNARY_SUB){
                    ops.pop();
                    negativeNumber = true;
                }
                Assert(token.str[0]!='-');// ensure that the tokenizer hasn't been changed
                // to clump the - together with the number token
                
                bool printedError = false;
                u64 num=0;
                for(int i=0;i<token.length;i++){
                    char c = token.str[i];
                    if(num * 10 < num && !printedError) {
                        ERR_SECTION(
                            ERR_HEAD(token)
                            // TODO: Show the limit? pow(2,64)-1
                            ERR_MSG("Number overflow! '"<<token<<"' is to large for 64-bit integers!")
                            ERR_LINE(token,"to large!");
                        )
                        printedError = true;
                    }
                    num = num*10 + (c-'0');
                }
                bool unsignedSuffix = false;
                bool signedSuffix = false;
                Token tok = info.get(info.at()+1);
                if((token.flags&TOKEN_SUFFIX_LINE_FEED)==0 && 0==(token.flags&TOKEN_SUFFIX_SPACE)) {
                    if(Equal(tok,"u")) {
                        info.next();
                        unsignedSuffix  = true;
                    } else if(Equal(tok,"s")) {
                        info.next();
                        signedSuffix  = true;
                    } else if(IsName(tok)){
                        ERR_SECTION(
                            ERR_HEAD(tok)
                            ERR_MSG("'"<<tok<<"' is not a known suffix for integers. The available ones are: 92u (unsigned), 31924s (signed).")
                            ERR_LINE(tok,"invalid suffix")
                        )
                    }
                }
                ASTExpression* tmp = 0;
                if(unsignedSuffix) {
                    if(negativeNumber) {
                        ERR_SECTION(
                            ERR_HEAD(tok)
                            ERR_MSG("You cannot use an unsigned suffix and a minus at the same time. They collide.")
                            ERR_LINE(tok,"remove?")
                        )
                    }
                    if ((num&0xFFFFFFFF00000000) == 0) {
                        tmp = info.ast->createExpression(TypeId(AST_UINT32));
                    } else {
                        tmp = info.ast->createExpression(TypeId(AST_UINT64));
                    }
                } else if(signedSuffix || negativeNumber) {
                    if ((num&0xFFFFFFFF80000000) == 0) {
                        tmp = info.ast->createExpression(TypeId(AST_INT32));
                    } else if((num&0x8000000000000000) == 0 || (negativeNumber && (num == 0x8000000000000000))) {
                        tmp = info.ast->createExpression(TypeId(AST_INT64));
                    } else {
                        tmp = info.ast->createExpression(TypeId(AST_INT64));
                        if(!printedError){
                            // This message is different from the one in the loop above.
                            // This complains about to large number for signed integers while the above
                            // complains about unsigned and signed integers. We don't need
                            // to print if the above already did.
                            ERR_SECTION(
                                ERR_HEAD(token)
                                // TODO: Show the limit? pow(2,63)-1 or -pow(2,63) if negative
                                ERR_MSG("'"<<token<<"' is to large for signed 64-bit integers! Larger integers are not supported. Consider using an unsigned 64-bit integer.")
                                ERR_LINE(token,"to large");
                            )
                        }
                    }
                } else {
                    // default will result in signed integers when possible.
                    if ((num&0xFFFFFFFF00000000) == 0) {
                        tmp = info.ast->createExpression(TypeId(AST_INT32));
                    } else if((num&0x8000000000000000) == 0) {
                        tmp = info.ast->createExpression(TypeId(AST_INT64));
                    } else {
                        tmp = info.ast->createExpression(TypeId(AST_UINT64));
                    }
                }
                // if(negativeNumber) {
                //     if(unsignedSuffix) {
                //         ERR_SECTION(
                //             ERR_HEAD(tok)
                //             ERR_MSG("You cannot use an unsigned suffix and a minus at the same time. They collide.")
                //             ERR_LINE(tok,"remove?")
                //         )
                //     }
                //     if ((num&0xFFFFFFFF80000000) == 0) {
                //         tmp = info.ast->createExpression(TypeId(AST_INT32));
                //     } else if((num&0x8000000000000000) == 0) {
                //         tmp = info.ast->createExpression(TypeId(AST_INT64));
                //     } else {
                //         tmp = info.ast->createExpression(TypeId(AST_INT64));
                //         if(!printedError){
                //             ERR_SECTION(
                        // ERR_HEAD(token,"Number overflow! '"<<token<<"' is to large for 64-bit integers!\n\n";
                //                 ERR_LINE(token.tokenIndex,"to large!");
                //             )
                //         }
                //     }
                // }else{
                //     // we default to signed but we use unsigned if the number doesn't fit
                //     if(unsignedSuffix && (num&0x8000000000000000)!=0) {
                //         if(signedSuffix) {
                //             ERR_SECTION(
                //                 ERR_HEAD(tok)
                //                 ERR_MSG("You cannot use an unsigned suffix and a minus at the same time. They collide.")
                //                 ERR_LINE(tok,"remove?")
                //             )
                //         }
                //         if ((num&0xFFFFFFFF00000000) == 0) {
                //             tmp = info.ast->createExpression(TypeId(AST_UINT32));
                //         } else {
                //             tmp = info.ast->createExpression(TypeId(AST_UINT64));
                //         }
                //     } else {
                //         if ((num&0xFFFFFFFF00000000) == 0) {
                //             tmp = info.ast->createExpression(TypeId(AST_INT32));
                //         } else {
                //             tmp = info.ast->createExpression(TypeId(AST_INT64));
                //         }
                //     }
                // }
                tmp->i64Value = negativeNumber ? -num : num;
                
                tmp->constantValue = true;
                values.add(tmp);
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(IsDecimal(token)){
                Token tok = info.get(info.at()+2);
                ASTExpression* tmp = nullptr;
                Assert(token.str[0]!='-');// ensure that the tokenizer hasn't been changed
                // to clump the - together with the number token
                if((token.flags&TOKEN_SUFFIX_LINE_FEED)==0 && 0==(token.flags&TOKEN_SUFFIX_SPACE)
                && Equal(tok,"d")) {
                    info.next();
                    info.next();
                    tmp = info.ast->createExpression(TypeId(AST_FLOAT64));
                    tmp->f64Value = ConvertDecimal(token);
                    if (ops.size()>0 && ops.last() == AST_UNARY_SUB){
                        ops.pop();
                        tmp->f64Value = -tmp->f64Value;
                    }
                } else {
                    info.next();
                    tmp = info.ast->createExpression(TypeId(AST_FLOAT32));
                    tmp->f32Value = ConvertDecimal(token);
                    if (ops.size()>0 && ops.last() == AST_UNARY_SUB){
                        ops.pop();
                        tmp->f32Value = -tmp->f32Value;
                    }
                }

                values.add(tmp);
                tmp->constantValue = true;
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(IsHexadecimal(token)){
                info.next();
                // if (ops.size()>0 && ops.last() == AST_UNARY_SUB){
                //     ops.pop();
                //     ERR_SECTION(
                //         ERR_HEAD(token)
                //         ERR_MSG("Negative hexidecimals is not okay.")
                //         ERR_LINE(token,"bad");
                //     )
                // }
                // 0x000000001 will be treated as 64 bit value and
                // it probably should because you added those 
                // zero for that exact reason.
                ASTExpression* tmp = nullptr;
                if(token.length-2<=8) {
                    tmp = info.ast->createExpression(TypeId(AST_UINT32));
                } else if(token.length-2<=16) {
                    tmp = info.ast->createExpression(TypeId(AST_UINT64));
                } else {
                    tmp = info.ast->createExpression(TypeId(AST_UINT64));
                    ERR_SECTION(
                        ERR_HEAD(token)
                        ERR_MSG("Hexidecimal overflow! '"<<token<<"' is to large for 64-bit integers!")
                        ERR_LINE(token,"to large!");
                    )
                }
                tmp->i64Value =  ConvertHexadecimal(token); // TODO: Only works with 32 bit or 16,8 I suppose.
                values.add(tmp);
                tmp->constantValue = true;
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
                    tmp->constantValue = true;
                }else if((token.flags&TOKEN_DOUBLE_QUOTED)) {
                    tmp = info.ast->createExpression(TypeId(AST_STRING));
                    tmp->name = token;
                    tmp->constantValue = true;
                    if(cstring)
                        tmp->flags |= ASTNode::NULL_TERMINATED;

                    // A string of zero size can be useful which is why
                    // the code below is commented out.
                    // if(token.length == 0){
                        // ERR_SECTION(
                        // ERR_HEAD(token, "string should not have a length of 0\n";
                        // This is a semantic error and since the syntax is correct
                        // we don't need to return PARSE_ERROR. We could but things will be fine.
                    // }
                }
                values.add(tmp);
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(Equal(token,"true") || Equal(token,"false")){
                info.next();
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_BOOL));
                tmp->boolValue = Equal(token,"true");
                tmp->constantValue = true;
                values.add(tmp);
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
            //             ERR_SECTION(
                    // ERR_HEAD(token, "expected ] not "<<token<<"\n";
            //             continue;
            //         }
            //     }
                
            //     values.add(tmp);
            //     tmp->tokenRange.firstToken = token;
            //     tmp->tokenRange.startIndex = info.at();
            //     tmp->tokenRange.endIndex = info.at()+1;
            //     tmp->tokenRange.tokenStream = info.tokens;
            // } 
            else if(Equal(token,"null")){
                token = info.next();
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_NULL));
                values.add(tmp);
                tmp->constantValue = true;
                tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(Equal(token,"sizeof") || Equal(token,"nameof") || Equal(token,"typeid")) {
                info.next();
                ASTExpression* tmp = nullptr;
                if(Equal(token,"sizeof"))
                    tmp = info.ast->createExpression(TypeId(AST_SIZEOF));
                if(Equal(token,"nameof"))
                    tmp = info.ast->createExpression(TypeId(AST_NAMEOF));
                if(Equal(token,"typeid"))
                    tmp = info.ast->createExpression(TypeId(AST_TYPEID));

                // Token token = {}; 
                // int result = ParseTypeId(info,token);
                // tmp->name = token;

                bool hasParentheses = false;
                Token tok = info.get(info.at() + 1);
                if(Equal(tok, "(")) {
                    // NOTE: We need to handle parentheses here because otherwise
                    //  typeid(Apple).ptr_level would be seen as typeid( (Apple).ptr_level )
                    info.next();
                    hasParentheses=true;
                }

                ASTExpression* left=nullptr;
                SignalAttempt result = ParseExpression(info, left, false);
                tmp->left = left;
                tmp->constantValue = true;

                if(hasParentheses) {
                    Token tok = info.get(info.at() + 1);
                    if(Equal(tok, ")")) {
                        info.next();
                    } else {
                        ERR_SECTION(
                            ERR_HEAD(tok)
                            ERR_MSG("Missing closing parentheses.")
                            ERR_LINE(tok,"opening parentheses")
                        )
                    }
                }
                
                tmp->tokenRange.firstToken = token;
                tmp->tokenRange.endIndex = info.at()+1;
                values.add(tmp);
            }  else if(Equal(token, "asm")) {
                info.next();

                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_ASM));
                tmp->tokenRange.firstToken = token;

                // TODO: asm<i32>{...} casting

                Token curly = info.get(info.at()+1);
                if(!Equal(curly, "{")) {
                    ERR_SECTION(
                        ERR_HEAD(curly)
                        ERR_MSG("'asm' keyword (inline assembly) requires a body with curly braces")
                        ERR_LINE(curly,"should be }")
                    )
                    continue;
                }
                info.next();
                int depth = 1;
                Token token{};
                Token& firstToken = info.get(info.at()+1);
                TokenStream* stream = firstToken.tokenStream;
                while(!info.end()){
                    token = info.get(info.at() + 1);
                    if(Equal(token,"{")) {
                        depth ++;
                        continue;
                    }
                    if(Equal(token,"}")) {
                        depth--;
                        if(depth==0) {
                            // info.next(); // Done after loop. it's easier since we might have quit due to end of stream.
                            break;
                        }
                    }
                    if(token.tokenStream != stream) {
                        ERR_SECTION(
                            ERR_HEAD(token)
                            ERR_MSG("Tokens in inlined assembly must come frome the same source file due to implementation restrictions. (it will be fixed later)")
                            ERR_LINE(firstToken,"first file")
                            ERR_LINE(token,"second file")
                        )
                    }
                    // Inline assembly is parsed here. The content of the assembly can be known
                    // with tokenRange in ASTNode. We know the range of tokens for the inline assembly.
                    // If you used a macro from another stream then we have problems.
                    // TODO: We want to search for instructions where variables are used like this
                    //   mov eax, [var] and change the instruction to one that gets the variable.
                    //   Can't do that here though since we need to know the variables first.
                    //   It must be done in type checker.
                    info.next();
                }
                token = info.get(info.at()+1);
                if(!Equal(token, "}")) {
                    ERR_SECTION(
                        ERR_HEAD(curly)
                        ERR_MSG("Missing ending curly brace for inline assembly.")
                        ERR_LINE(curly,"starts here")
                    )
                    continue;
                }
                info.next();

                tmp->tokenRange.endIndex = info.at()+1;
                values.add(tmp);
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
                
                std::string pointers = "";
                int pointer_level = 0;
                while(true) {
                    tok = info.get(info.at() + pointer_level + 1);
                    if(Equal(tok,"*")) {
                        pointer_level++;
                    } else if(Equal(tok,"{")) {
                        // initializer for pointer
                        for(int i=0;i<pointer_level;i++) {
                            pointers+="*";
                            info.next();
                        }
                        break;
                    } else {
                        break;   
                    }
                    
                }
                tok = info.get(info.at()+1);

                if(Equal(tok,"(")){
                    // function call
                    info.next();
                    
                    std::string ns = "";
                    while(ops.size()>0){
                        OperationType op = ops.last();
                        if(op == AST_FROM_NAMESPACE) {
                            ns += namespaceNames.last();
                            ns += "::";
                            namespaceNames.pop();
                            ops.pop();
                        } else {
                            break;
                        }
                    }
                    ns += token;
                    ns += polyTypes;

                    ASTExpression* tmp = info.ast->createExpression(TypeId(AST_FNCALL));
                    tmp->name = polyName;

                    int count = 0;
                    SignalDefault result = ParseArguments(info, tmp, &count);
                    if(result!=SignalDefault::SUCCESS){
                        info.ast->destroy(tmp);
                        return SignalAttempt::FAILURE;
                    }

                    _PLOG(log::out << "Parsed call "<<count <<"\n";)
                    values.add(tmp);
                    tmp->tokenRange.firstToken = token;
                    // tmp->tokenRange.startIndex = startToken;
                    tmp->tokenRange.endIndex = info.at()+1;
                    // tmp->tokenRange.tokenStream = info.tokens;
                } else if(Equal(tok,"{") && 0 == (token.flags & TOKEN_MASK_SUFFIX)){ // Struct {} is ignored
                    // initializer
                    info.next();
                    ASTExpression* initExpr = info.ast->createExpression(TypeId(AST_INITIALIZER));
                    // NOTE: Type checker works on TypeIds not AST_FROM_NAMESPACE.
                    //  AST_FROM... is used for functions and variables.
                    std::string ns = "";
                    while(ops.size()>0){
                        OperationType op = ops.last();
                        if(op == AST_FROM_NAMESPACE) {
                            ns += namespaceNames.last();
                            ns += "::";
                            namespaceNames.pop();
                            ops.pop();
                        } else {
                            break;
                        }
                    }
                    ns += token;
                    ns += polyTypes;
                    ns += pointers;
                    std::string* nsp = info.ast->createString();
                    *nsp = ns;
                    initExpr->castType = info.ast->getTypeString(*nsp);
                    // TODO: A little odd to use castType. Renaming castType to something more
                    //  generic which works for AST_CAST and AST_INITIALIZER would be better.
                    // ASTExpression* tail=0;
                    
                    bool mustBeNamed=false;
                    Token prevNamed = {};
                    int count=0;
                    WHILE_TRUE_N(10000) {
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
                            ERR_SECTION(
                                ERR_HEAD(token)
                                ERR_MSG("Expected named field because of previous named field "<<prevNamed << " at "<<prevNamed.line<<":"<<prevNamed.column<<".")
                                ERR_LINE(token,"bad")
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
                        }
                        // initExpr->args->add(expr);
                        initExpr->args.add(expr);
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
                            ERR_SECTION(
                                ERR_HEAD(token)
                                ERR_MSG("Expected , or } in initializer list not '"<<token<<"'.")
                                ERR_LINE(token,"bad")
                            )
                            continue;
                        }
                    }
                    // log::out << "Parse initializer "<<count<<"\n";
                    values.add(initExpr);
                    initExpr->tokenRange.firstToken = token;
                    // initExpr->tokenRange.startIndex = startToken;
                    initExpr->tokenRange.endIndex = info.at()+1;
                    // initExpr->tokenRange.tokenStream = info.tokens;
                } else {
                    // if(polyTypes.size()!=0){
                    //     This is possible since types are can be parsed in an expression.
                    //     ERR_SECTION(
                    // ERR_HEAD(token, "Polymorphic types not expected with namespace or variable.\n\n";
                    //         ERR_LINE(token.tokenIndex,"bad");
                    //     )
                    //     continue;
                    // }
                    if(Equal(tok,"::")){
                        info.next();
                        
                        // Token tok = info.get(info.at()+1);
                        // if(!IsName(tok)){
                        //     ERR_SECTION(
                        // ERR_HEAD(tok, "'"<<tok<<"' is not a name.\n\n";
                        //         ERR_LINE(tok.tokenIndex,"bad");
                        //     )
                        //     continue;
                        // }
                        ops.add(AST_FROM_NAMESPACE);
                        namespaceNames.add(token);
                        continue; // do a second round here?
                        // TODO: detect more ::
                    } else {
                        ASTExpression* tmp = info.ast->createExpression(TypeId(AST_ID));

                        Token nsToken = polyName;
                        int nsOps = 0;
                        while(ops.size()>0){
                            OperationType op = ops.last();
                            if(op == AST_FROM_NAMESPACE) {
                                nsOps++;
                                ops.pop();
                            } else {
                                break;
                            }
                        }
                        for(int i=0;i<nsOps;i++){
                            Token& tok = namespaceNames.last();
                            if(i==0){
                                nsToken = tok;
                            } else {
                                nsToken.length+=tok.length;
                            }
                            nsToken.length+=2; // colons
                            namespaceNames.pop();
                        }
                        if(nsOps!=0){
                            nsToken.length += polyName.length;
                        }

                        tmp->name = nsToken;

                        values.add(tmp);
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
                    ERR_SECTION(
                        ERR_HEAD(token)
                        ERR_MSG("Got nothing from paranthesis in expression.")
                        ERR_LINE(token,"bad")
                    )
                }
                token = info.get(info.at()+1);
                if(!Equal(token,")")){
                    ERR_SECTION(
                        ERR_HEAD(token)
                        ERR_MSG("Expected ) not "<<token<<".")
                        ERR_LINE(token,"bad")
                    )
                }else
                    info.next();
                values.add(tmp);  
            } else {
                if(attempt){
                    return SignalAttempt::BAD_ATTEMPT;
                }else{
                    ERR_SECTION(
                        ERR_HEAD(token)
                        ERR_MSG("'"<<token << "' is not a value. Values (or expressions) are expected after tokens that calls upon arguments, operations and assignments among other things.")
                        ERR_LINE(info.get(tokenIndex),"should be a value")
                        ERR_LINE(info.get(tokenIndex-1),"expects a value afterwards")
                    );
                    // ERR_LINE()
                    // printLine();
                    return SignalAttempt::FAILURE;
                    // Qutting here is okay because we have a defer which
                    // destroys parsed expressions. No memory leaks!
                }
            }
        }
        // if(negativeNumber){
        //     ERR_SECTION(
            // ERR_HEAD(info.get(info.at()-1), "Unexpected - before "<<token<<"\n";
        //         ERR_LINE(info.at()-1,"bad");
        //     )
        //     return SignalAttempt::FAILURE;
        //     // quitting here is a little unexpected but there is
        //     // a defer which destroys parsed expresions so no memory leaks at least.
        // }
        
        ending = ending || info.end();

        while(values.size()>0){
            OperationType nowOp = (OperationType)0;
            if(ops.size()>=2&&!ending){
                OperationType op1 = ops[ops.size()-2];
                if(!IsSingleOp(op1) && values.size()<2)
                    break;
                OperationType op2 = ops[ops.size()-1];
                if(IsSingleOp(op1)) {
                    if(!IsSingleOp(op2)) {
                        nowOp = op1;
                        ops[ops.size()-2] = op2;
                        ops.pop();
                    } else {
                        break;
                    }
                } else {
                    if(OpPrecedence(op1)>=OpPrecedence(op2)){ // this code produces this: 1 = 3-1-1
                    // if(OpPrecedence(op1)>OpPrecedence(op2)){ // this code cause this: 3 = 3-1-1
                        nowOp = op1;
                        ops[ops.size()-2] = op2;
                        ops.pop();
                    }else{
                        // _PLOG(log::out << "break\n";)
                        break;
                    }
                }
            }else if(ending){
                if(ops.size()==0){
                    // NOTE: Break on base case when we end with no operators left and with one value. 
                    break;
                }
                nowOp = ops.last();
                ops.pop();
            }else{
                break;
            }
            if(expectOperator && opType != 0 && ending) {
                expectOperator = false; // prevent duplicate messages
                ERR_SECTION(
                    ERR_HEAD(info.get(info.at()))
                    ERR_MSG("Operation '"<<info.get(info.at())<<"' needs a left and right expression but the right is missing.")
                    ERR_LINE(info.get(info.at()), "Missing expression to the right")
                )
                continue; // ignore operation
            }

            auto er = values.last();
            values.pop();

            auto val = info.ast->createExpression(TypeId(nowOp));
            // val->tokenRange.tokenStream = info.tokens;
            // FROM_NAMESPACE, CAST, REFER, DEREF, NOT, BNOT
            if(IsSingleOp(nowOp)){
                // TODO: tokenRange is not properly set!
                val->tokenRange.firstToken = er->tokenRange.firstToken;
                val->tokenRange.endIndex = er->tokenRange.endIndex;
                // val->tokenRange = er->tokenRange;
                if(nowOp == AST_FROM_NAMESPACE){
                    Token& tok = namespaceNames.last();
                    val->name = tok;
                    val->tokenRange.firstToken = tok;
                    // val->tokenRange.startIndex -= 2; // -{namespace name} -{::}
                    namespaceNames.pop();
                } else if(nowOp == AST_CAST){
                    val->castType = castTypes.last();
                    castTypes.pop();
                    Token extraToken = extraTokens.last();
                    extraTokens.pop();
                    if(Equal(extraToken,"cast_unsafe")) {
                        val->setUnsafeCast(true);
                    }
                    val->tokenRange.firstToken = extraToken;
                    val->tokenRange.endIndex = er->tokenRange.firstToken.tokenIndex;
                } else if(nowOp == AST_UNARY_SUB){
                    // I had an idea about replacing unary sub with AST_SUB
                    // and creating an integer expression with a zero in it as left node
                    // and the node from unary as the right. The benefit of this is that
                    // you have one less operation (AST_UNARY_SUB) to worry about.
                    // The bad is that you create a new expression and operator overloading
                    // wouldn't work with unary sub.
                } else {
                    // val->tokenRange.startIndex--;
                }
                val->left = er;
                if(val->typeId != AST_REFER) {
                    val->constantValue = er->constantValue;
                }
            } else if(values.size()>0){
                auto el = values.last();
                values.pop();
                val->tokenRange.firstToken = el->tokenRange.firstToken;
                // val->tokenRange.startIndex = el->tokenRange.startIndex;
                val->tokenRange.endIndex = er->tokenRange.endIndex;
                val->left = el;
                val->right = er;

                if(nowOp==AST_ASSIGN){
                    Assert(assignOps.size()>0);
                    val->assignOpType = assignOps.last();
                    assignOps.pop();
                }
                val->constantValue = val->left->constantValue && val->right->constantValue;
            } else {
                Assert(("Bug in compiler",false));
            }

            values.add(val);
        }
        // Attempt succeeded. Failure is now considered an error.
        attempt = false;
        expectOperator=!expectOperator;
        if(ending){
            expression = values.last();
            expression->computeWhenPossible = shouldComputeExpression;
            if(!info.hasErrors()) {
                Assert(values.size()==1);
            }

            // we have a defer above which destroys expressions which is why we
            // clear the list to prevent it
            values.resize(0);
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
        ASTExpression* expr=nullptr;
        SignalAttempt result = ParseExpression(info,expr,false);
        if(result!=SignalAttempt::SUCCESS){
            // TODO: should more stuff be done here?
            return SignalAttempt::FAILURE;
        }

        int endIndex = info.at()+1;

        ASTScope* body=nullptr;
        ParseFlags parsed_flags = PARSE_NO_FLAGS;
        SignalDefault resultBody = ParseBody(info,body, info.currentScopeId, PARSE_NO_FLAGS, &parsed_flags);
        if(resultBody!=SignalDefault::SUCCESS){
            return SignalAttempt::FAILURE;
        }

        ASTScope* elseBody=nullptr;
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
        
        // statement->tokenRange.endIndex = info.at()+1;
        statement->tokenRange.endIndex = endIndex;

        if(!(parsed_flags&PARSE_HAS_CURLIES) && body->statements.size()>0) {
            if (body->statements[0]->type == ASTStatement::IF) {
                ERR_SECTION(
                    ERR_HEAD(body->statements[0]->tokenRange, ERROR_AMBIGUOUS_IF_ELSE)
                    ERR_MSG("Nested if statements without curly braces may be ambiguous with else statements and is therefore not allowed. Use curly braces on the \"highest\" if statement.")
                    ERR_LINE(statement->tokenRange, "use curly braces here")
                    ERR_LINE(body->statements[0]->tokenRange, "ambiguous when using else")
                )
            }
        }
        
        return SignalAttempt::SUCCESS;
    }else if(Equal(firstToken,"while")){
        info.next();

        ASTExpression* expr=nullptr;
        if(Equal(info.get(info.at() + 1), "{")) {
            // no expression, infinite loop
        } else {
            SignalAttempt result = ParseExpression(info,expr,false);
            if(result!=SignalAttempt::SUCCESS){
                // TODO: should more stuff be done here?
                return SignalAttempt::FAILURE;
            }
        }

        int endIndex = info.at()+1;

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
        
        statement->tokenRange.endIndex = endIndex;
        
        return SignalAttempt::SUCCESS;
    } else if(Equal(firstToken,"for")){
        info.next();
        
        Token token = info.get(info.at()+1);
        bool reverseAnnotation = false;
        bool pointerAnnot = false;
        while (IsAnnotation(token)){
            info.next();
            if(Equal(token,"@reverse") || Equal(token,"@rev")){
                reverseAnnotation=true;
            } else if(Equal(token,"@pointer") || Equal(token,"@ptr")){
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
        std::string* strnr = info.ast->createString();
        *strnr = "nr";
        Token nrToken = *strnr;
        statement->varnames.add({nrToken});
        statement->setReverse(reverseAnnotation);
        statement->setPointer(pointerAnnot);
        statement->firstExpression = expr;
        statement->firstBody = body;
        statement->tokenRange.firstToken = firstToken;
        
        statement->tokenRange.endIndex = info.at()+1;
        
        return SignalAttempt::SUCCESS;
    } else if(Equal(firstToken,"return")){
        info.next();
        
        statement = info.ast->createStatement(ASTStatement::RETURN);
        
        if((firstToken.flags & TOKEN_SUFFIX_LINE_FEED) == 0) {
            WHILE_TRUE {
                ASTExpression* expr=0;
                Token token = info.get(info.at()+1);
                if(Equal(token,";")&&statement->arrayValues.size()==0){ // return 1,; should not be allowed, that's why we do &&!base
                    info.next();
                    break;
                }
                SignalAttempt result = ParseExpression(info,expr,true);
                if(result!=SignalAttempt::SUCCESS){
                    break;
                }
                statement->arrayValues.add(expr);
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
        }
        
        // statement->rvalue = base;
        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.endIndex = info.at()+1;
        return SignalAttempt::SUCCESS;
    } else if(Equal(firstToken,"switch")){
        info.next();
        ASTExpression* switchExpression=0;
        SignalAttempt result = ParseExpression(info,switchExpression,false);
        if(result!=SignalAttempt::SUCCESS){
            // TODO: should more stuff be done here?
            return SignalAttempt::FAILURE;
        }
        
        Token tok = info.get(info.at()+1);
        if(!Equal(tok,"{")) {
            return SignalAttempt::FAILURE;
        }
        info.next(); // {
        
        statement = info.ast->createStatement(ASTStatement::SWITCH);
        statement->firstExpression = switchExpression;
        statement->tokenRange.firstToken = firstToken;
        
        Token defaultCaseToken{};
        
        bool errorsWasIgnored = info.ignoreErrors;
        defer {
            info.ignoreErrors = errorsWasIgnored;
        };
        
        bool mayRevertError = false;
        
        while(!info.end()) {
            tok = info.get(info.at()+1);
            if(Equal(tok,"}")) {
                info.next();
                break;   
            }
            if(Equal(tok,";")) {
                info.next();
                continue;
            }
            
            if(IsAnnotation(tok)) {
                if(Equal(tok,"@TEST_ERROR")) {
                    info.ignoreErrors = true;
                    if(mayRevertError)
                        info.errors--;
                    statement->setNoCode(true);
                    info.next();
                    
                    // TokenRange args;
                    // auto res = ParseAnnotationArguments(info, &args);
                    auto res = ParseAnnotationArguments(info, nullptr);

                    continue;
                }else {
                    // TODO: ERR annotation not supported   
                }
            }
            mayRevertError = false;
            
            bool badDefault = false;
            if(Equal(tok,"default")) {
                badDefault = true;
                mayRevertError = info.ignoreErrors == 0;
                ERR_SECTION(
                    ERR_HEAD(tok,ERROR_C_STYLED_DEFAULT_CASE)
                    ERR_MSG("Write 'case: { ... }' to specify default case. 'default' is the C/C++ way.")
                    ERR_LINE(tok,"bad")
                )
                // info.next();
                // return SignalAttempt::FAILURE;
            } else if(!Equal(tok,"case")) {
                mayRevertError = info.ignoreErrors == 0;
                ERR_SECTION(
                    ERR_HEAD(tok,ERROR_BAD_TOKEN_IN_SWITCH)
                    ERR_MSG("'"<<tok<<"' is not allowed in switch statement where 'case' is supposed to be.")
                    ERR_LINE(tok,"bad")
                )
                info.next();
                continue;
                // return SignalAttempt::FAILURE;
            }
            
            info.next(); // case
            
            bool defaultCase = false;
            Token colonTok = info.get(info.at()+1);
            if(Equal(colonTok,":")) {
                if(defaultCaseToken.str && !badDefault){
                    mayRevertError = info.ignoreErrors == 0;
                    ERR_SECTION(
                        ERR_HEAD(tok,ERROR_DUPLICATE_DEFAULT_CASE)
                        ERR_MSG("You cannot have two default cases in a switch statement.")
                        ERR_LINE(defaultCaseToken,"previous")
                        ERR_LINE(tok,"first default case")
                    )
                } else {
                    defaultCaseToken = tok;
                }
                
                defaultCase = true;
                info.next();
                
                // ERR_SECTION(
                //     ERR_HEAD(tok)
                //     ERR_MSG("Default case has not been implemented yet.")
                //     ERR_LINE(tok,"bad")
                // )
                // return SignalAttempt::FAILURE;
            }
            
            ASTExpression* caseExpression=nullptr;
            if(!defaultCase){
                SignalAttempt result = ParseExpression(info,caseExpression,false);
                if(result!=SignalAttempt::SUCCESS){
                    // TODO: should more stuff be done here?
                    return SignalAttempt::FAILURE;
                }
                colonTok = info.get(info.at()+1);
                if(!Equal(colonTok,":")) {
                    // info.next();
                    // continue;
                    return SignalAttempt::FAILURE;
                }
                info.next(); // :
            }
            
            ParseFlags parsed_flags = PARSE_NO_FLAGS;
            ASTScope* caseBody=nullptr;
            SignalDefault resultBody = ParseBody(info,caseBody, info.currentScopeId, PARSE_INSIDE_SWITCH, &parsed_flags);
            if(resultBody!=SignalDefault::SUCCESS){
                return SignalAttempt::FAILURE;
            }
            
            Token& token2 = info.get(info.at()+1);
            if(IsAnnotation(token2)) {
                if(Equal(token2,"@TEST_ERROR")) {
                    info.ignoreErrors = true;
                    if(mayRevertError)
                        info.errors--;
                    statement->setNoCode(true);
                    info.next();
                    
                    auto res = ParseAnnotationArguments(info, nullptr);
                } else {
                    // TODO: ERR annotation not supported   
                }
                //  else if(Equal(token2, "@no-code")) {
                //     noCode = true;
                //     info.next();
                // }
            }
            if(defaultCase) {
                if(statement->firstBody || badDefault) {
                    // Assert(info.errors);
                    info.ast->destroy(caseBody);
                } else {
                    statement->firstBody = caseBody;
                }
            } else {
                ASTStatement::SwitchCase tmp{};
                tmp.caseExpr = caseExpression;
                tmp.caseBody = caseBody;
                tmp.fall = parsed_flags & PARSE_HAS_CASE_FALL;
                statement->switchCases.add(tmp);
            }
        }
        
        statement->tokenRange.endIndex = info.at()+1;
        
        return SignalAttempt::SUCCESS;
    } else  if(Equal(firstToken,"using")){
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
            statement->alias = TRACK_ALLOC(std::string);
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
    // } else if(!(firstToken.flags&TOKEN_MASK_QUOTED) && firstToken.length == 5 && !strncmp(firstToken.str, "test", 4) && firstToken.str[4]>='0' && firstToken.str[4] <= '9') {
    } else if(Equal(firstToken,"test")) {
        info.next();
        
        // test4 {type} {value} {expression}
        // test1 {value} {expression}

        // integer/float or string

        // int byteCount = firstToken.str[4]-'0';
        // if(byteCount!=1 || byteCount!=2||byteCount!=4||byteCount!=8) {
        //     ERR_SECTION(
        //         ERR_HEAD(firstToken)
        //         ERR_MSG("Test statement uses these keyword: test1, test2, test4, test8. The numbers describe the bytes to test. '"<<byteCount<<"' is not one of them.")
        //         ERR_LINE(firstToken, byteCount + " is not allowed")
        //     )
        // } else {
        //     statement->bytesToTest = byteCount;
        // }

        // Token valueToken = info.get(info.at()+1);
        // info.next();

        statement = info.ast->createStatement(ASTStatement::TEST);

        // if(IsInteger(valueToken) || IsDecimal(valueToken) || IsHexadecimal(valueToken) || (valueToken.flags&TOKEN_MASK_QUOTED)) {
        //     statement->testValueToken = valueToken;
        // } else {
        //     ERR_SECTION(
        //         ERR_HEAD(valueToken)
        //         ERR_MSG("Test statement requires a value after the keyword such as an integer, decimal, hexidecimal or string.")
        //         ERR_LINE(valueToken, "not a valid value")
        //     )
        // }

        SignalAttempt result = ParseExpression(info, statement->testValue, false);
        
        if(Equal(info.get(info.at()+1),";")) info.next();
        
        result = ParseExpression(info, statement->firstExpression, false);

        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.endIndex = info.at()+1;

        if(Equal(info.get(info.at()+1),";")) info.next();
        
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
        ERR_SECTION(
            ERR_HEAD(beginToken)
            ERR_MSG("Expected 'operator' for operator overloading not '"<<beginToken<<"'.")
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
            function->setHidden(true);
        } else {
            // It should not warn you because it is quite important that you use the right annotations with functions
            // Mispelling external or the calling convention would be very bad.
            ERR_SECTION(
                ERR_HEAD(name)
                ERR_MSG("'"<< Token(name.str+1,name.length-1) << "' is not a known annotation for operators.")
                ERR_LINE(info.get(info.at()),"unknown")
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
    
    Token name2 = info.get(info.at()+1);
    // What about +=, -=?
    if(Equal(name, "[") && Equal(name2, "]")) {
        info.next();// ]
        name.length++;
    } else if(!(op = IsOp(name, extraNext))){
        info.ast->destroy(function);
        function = nullptr;
        ERR_SECTION(
            ERR_HEAD(name)
            ERR_MSG("Expected a valid operator, "<<name<<" is not.")
        )
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

    ScopeInfo* funcScope = info.ast->createScope(info.currentScopeId, info.getNextOrder(), nullptr);
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
                ERR_SECTION(
                    ERR_HEAD(tok)
                    ERR_MSG("Expected , or > for in poly. arguments for operator "<<name<<".")
                )
                // parse error or what?
                break;
            }
        }
    }
    tok = info.get(info.at()+1);
    if(!Equal(tok,"(")){
        ERR_SECTION(
            ERR_HEAD(tok)
            ERR_MSG("Expected ( not "<<tok<<".")
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
                ERR_SECTION(
                    ERR_HEAD(arg)
                    ERR_MSG("'"<<arg <<"' is not a valid argument name.")
                    ERR_LINE(arg,"bad")
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
                ERR_SECTION(
                    ERR_HEAD(tok)
                    ERR_MSG("Expected : not "<<tok <<".")
                    ERR_LINE(tok,"bad")
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
            ERR_SECTION(
                ERR_HEAD(tok)
                ERR_MSG("Expected , or ) not "<<tok <<".")
                ERR_LINE(tok,"bad")
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
                    ERR_SECTION(
                        ERR_HEAD(tok)
                        ERR_MSG("Expected a comma or curly brace. '"<<tok <<"' is not okay.")
                        ERR_LINE(tok,"bad coder");
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
        ERR_SECTION(
            ERR_HEAD(bodyTok)
            ERR_MSG("Operator overloading must have a body. You have forgotten the body when defining '"<<function->name<<"'.")
            ERR_LINE(bodyTok,"replace with {}")
        )
    } else if(Equal(bodyTok,"{")){
        info.functionScopes.add({});
        ASTScope* body = 0;
        SignalDefault result = ParseBody(info,body, function->scopeId);
        info.functionScopes.pop();
        function->body = body;
    } else {
        ERR_SECTION(
            ERR_HEAD(bodyTok)
            ERR_MSG("Operator has no body! Did the return types parse incorrectly? Use curly braces to define the body.")
            ERR_LINE(bodyTok,"expected {")
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
        ERR_SECTION(
            ERR_HEAD(fnToken)
            ERR_MSG("Expected fn for function not '"<<fnToken<<"'.")
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
            function->setHidden(true);
        } else if (Equal(name,"@dllimport")){
            function->callConvention = CallConventions::STDCALL;
            specifiedConvention = true;
            function->linkConvention = LinkConventions::DLLIMPORT;
            needsExplicitCallConvention = true;
            linkToken = name;
        } else if (Equal(name,"@varimport")){
            function->callConvention = CallConventions::STDCALL;
            specifiedConvention = true;
            function->linkConvention = LinkConventions::VARIMPORT;
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
        // } else if (Equal(name,"@cdecl")){
        // cdecl has not been implemented. It doesn't seem important.
        // default x64 calling convention is used.
        //     Assert(false); // not implemented in type checker and generator. type checker might work as is.
        //     function->callConvention = CallConventions::CDECL_CONVENTION;
        //     specifiedConvention = true;
        } else if (Equal(name,"@betcall")){
            function->callConvention = CallConventions::BETCALL;
            function->linkConvention = LinkConventions::NONE;
            specifiedConvention = true;
        // IMPORTANT: When adding calling convention, do not forget to add it to the "Did you mean" below!
        } else if (Equal(name,"@unixcall")){
            function->callConvention = CallConventions::UNIXCALL;
            specifiedConvention = true;
        } else if (Equal(name,"@native")){
            function->linkConvention = NATIVE;
        } else if (Equal(name,"@intrinsic")){
            function->callConvention = CallConventions::INTRINSIC;
            function->linkConvention = NATIVE;
        } else {
            // It should not warn you because it is quite important that you use the right annotations with functions
            // Mispelling external or the calling convention would be very bad.
            ERR_SECTION(
                ERR_HEAD(name)
                ERR_MSG("'"<< Token(name.str+1,name.length-1) << "' is not a known annotation for functions.")
                ERR_LINE(info.get(info.at()),"unknown")

                // if(StartsWith(name, "@extern")) {
                //     log::out << log::YELLOW << "Did you mean @extern-stdcall or @extern-cdecl\n";
                // }
            )
        }
        name = info.next();
        continue;
    }
    if(function->linkConvention != LinkConventions::NONE){
        function->setHidden(true);
        // native doesn't use a calling convention
        if(needsExplicitCallConvention && !specifiedConvention){
            ERR_SECTION(
                ERR_HEAD(name)
                ERR_MSG("You must specify a calling convention. The default is betcall which you probably don't want. Use @stdcall or @cdecl instead.")
                ERR_LINE(name, "missing call convention")
            )
        }
    }
    
    if(!IsName(name)){
        info.ast->destroy(function);
        function = nullptr;
        ERR_SECTION(
            ERR_HEAD(name)
            ERR_MSG("Expected a valid name, "<<name<<" is not.")
        )
        return SignalAttempt::FAILURE;
    }
    function->name = name;

    ScopeInfo* funcScope = info.ast->createScope(info.currentScopeId, info.getNextOrder(), nullptr);
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
                ERR_SECTION(
                    ERR_HEAD(tok)
                    ERR_MSG("Expected , or > for in poly. arguments for function "<<name<<".")
                )
                // parse error or what?
                break;
            }
        }
    }
    tok = info.next();
    if(!Equal(tok,"(")){
        ERR_SECTION(
            ERR_HEAD(tok)
            ERR_MSG("Expected ( not "<<tok<<".")
        )
        return SignalAttempt::FAILURE;
    }

    if(parentStruct) {
        function->arguments.add({});
        auto& argv = function->arguments[function->arguments.size()-1];
        argv.name = "this";
        // argv.name.tokenStream = info.tokens; // feed and print won't work if we set these, they think this comes from the stream and tries to print it
        // argv.name.tokenIndex = tok.tokenIndex;
        argv.name.line = tok.line;
        argv.name.column = tok.column+1;
        argv.defaultValue = nullptr;
        function->nonDefaults++;

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
                ERR_SECTION(
                    ERR_HEAD(arg)
                    ERR_MSG("'"<<arg <<"' is not a valid argument name.")
                    ERR_LINE(arg,"bad")
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
                ERR_SECTION(
                    ERR_HEAD(tok)
                    ERR_MSG("Expected : not "<<tok <<".")
                    ERR_LINE(tok,"bad")
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
            ERR_SECTION(
                ERR_HEAD(tok)
                ERR_MSG("Expected a default argument because of previous default argument "<<prevDefault<<" at "<<prevDefault.firstToken.line<<":"<<prevDefault.firstToken.column<<".")
                ERR_LINE(tok,"bad")
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
            ERR_SECTION(
                ERR_HEAD(tok)
                ERR_MSG("Expected , or ) not "<<tok <<".")
                ERR_LINE(tok,"bad")
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
                    ERR_SECTION(
                        ERR_HEAD(tok)
                        ERR_MSG("Expected a comma or curly brace. '"<<tok <<"' is not okay.")
                        ERR_LINE(tok,"bad coder")
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
            ERR_SECTION(
                ERR_HEAD(bodyTok)
                ERR_MSG("Functions must have a body. You have forgotten the body when defining '"<<function->name<<"'. Declarations and definitions happen at the same time in this language. This is possible because of out of order compilation.")
                ERR_LINE(bodyTok,"replace with {}")
            )
        }
    } else if(Equal(bodyTok,"{")){
        if(!function->needsBody()) {
            ERR_SECTION(
                ERR_HEAD(bodyTok)
                ERR_MSG("Native/external functions cannot have a body. Native functions are handled by the language. External functions link to functions outside your source code.")
                ERR_LINE(bodyTok,"use ; instead")
            )
        }
        
        info.functionScopes.add({});
        ASTScope* body = 0;
        SignalDefault result = ParseBody(info,body, function->scopeId);
        info.functionScopes.pop();
        function->body = body;
    } else if(function->needsBody()) {
        ERR_SECTION(
            ERR_HEAD(bodyTok)
            ERR_MSG("Function has no body! Did the return types parse incorrectly? Use curly braces to define the body.")
            ERR_LINE(bodyTok,"expected {")
        )
    }

    return SignalAttempt::SUCCESS;
}
SignalAttempt ParseAssignment(ParseInfo& info, ASTStatement*& statement, bool attempt){
    using namespace engone;
    MEASURE;
    _PLOG(FUNC_ENTER)

    bool globalAssignment = false;

    int tokenAt = info.at();
    Token globalToken = info.get(tokenAt+1);
    if(Equal(globalToken,"global")){
        globalAssignment = true;
        tokenAt++;
    }

    // to make things easier attempt is checked here
    if(attempt && !(IsName(info.get(tokenAt+1)) && (Equal(info.get(tokenAt+2),",") || Equal(info.get(tokenAt+2),":") || Equal(info.get(tokenAt+2),"=") ))){
    // if(attempt && !(IsName(info.get(tokenAt+1)) && (Equal(info.get(tokenAt+2),",") || Equal(info.get(tokenAt+2),":")))){
        return SignalAttempt::BAD_ATTEMPT;
    }
    attempt = true;

    if(globalAssignment)
        info.next();

    statement = info.ast->createStatement(ASTStatement::ASSIGN);
    // statement->varnames.stealFrom(varnames);
    statement->firstExpression = nullptr;
    statement->globalAssignment = globalAssignment;

    Token lengthTokenOfLastVar{};

    //-- Evaluate variables on the left side
    int startIndex = info.at()+1;
    // DynamicArray<ASTStatement::VarName> varnames{};
    while(!info.end()){
        Token name = info.get(info.at()+1);
        if(!IsName(name)){
            ERR_SECTION(
                ERR_HEAD(name)
                ERR_MSG("Expected a valid name for assignment ('"<<name<<"' is not).")
                ERR_LINE(info.get(info.at()+1),"cannot be a name");
            )
            return SignalAttempt::FAILURE;
        }
        info.next();

        statement->varnames.add({name});
        Token token = info.get(info.at()+1);
        if(Equal(token,":")){
            info.next(); // :
            attempt=false;


            token = info.get(info.at()+1);
            if(Equal(token,"=")) {
                int index = statement->varnames.size()-1;
                while(index>=0 && !statement->varnames[index].declaration){
                    statement->varnames[index].declaration = true;
                    index--;
                }
            } else {
                Token typeToken{};
                SignalDefault result = ParseTypeId(info,typeToken, nullptr);
                if(result!=SignalDefault::SUCCESS)
                    return CastSignal(result);
                // Assert(result==SignalDefault::SUCCESS);
                int arrayLength = -1;
                Token token1 = info.get(info.at()+1);
                Token token2 = info.get(info.at()+2);
                Token token3 = info.get(info.at()+3);
                if(Equal(token1,"[") && Equal(token3,"]")) {
                    info.next();
                    info.next();
                    info.next();

                    if(IsInteger(token2)) {
                        lengthTokenOfLastVar = token2;
                        arrayLength = ConvertInteger(token2);
                        if(arrayLength<0){
                            ERR_SECTION(
                                ERR_HEAD(token2)
                                ERR_MSG("Array cannot have negative size.")
                                ERR_LINE(info.get(info.at()-1),"< 0")
                            )
                            arrayLength = 0;
                        }
                    } else {
                        ERR_SECTION(
                            ERR_HEAD(token2)
                            ERR_MSG("The length of an array can only be specified with number literals. Use macros to avoid magic numbers. Constants have not been implemented but when they have, they will work too.")
                            ERR_LINE(info.get(info.at()-1), "must be positive integer literal")
                        )
                    }
                    std::string* str = info.ast->createString();
                    *str = "Slice<";
                    *str += typeToken;
                    *str += ">";
                    typeToken = *str;
                }
                TypeId strId = info.ast->getTypeString(typeToken);

                int index = statement->varnames.size()-1;
                while(index>=0 && !statement->varnames[index].declaration){
                    statement->varnames[index].assignString = strId;
                    statement->varnames[index].declaration = true;
                    statement->varnames[index].arrayLength = arrayLength;
                    index--;
                }
            }
        } else {
            if(globalAssignment) {
                ERR_SECTION(
                    ERR_HEAD(token)
                    ERR_MSG("Global variables must use declaration syntax. You must use a ':' (':=' if you want the type to be inferred).")
                    ERR_LINE(token, "add a colon before this")
                )
            }
        }
        token = info.get(info.at()+1);
        if(Equal(token, ",")){
            info.next();
            continue;
        }
        break;
    }

    // statement = info.ast->createStatement(ASTStatement::ASSIGN);
    // // statement->varnames.stealFrom(varnames);
    // statement->firstExpression = nullptr;
    // statement->globalAssignment = globalAssignment;
    
    // We usually don't want the tokenRange to include the expression.
    statement->tokenRange.firstToken = info.get(startIndex);
    statement->tokenRange.endIndex = info.at()+1;

    Token token = info.get(info.at()+1);
    if(Equal(token,"=")) {
        info.next(); // =

        SignalAttempt result = ParseExpression(info,statement->firstExpression,false);

    } else if(Equal(token,"{")) {
        // array initializer
        info.next(); // {

        while(true){
            Token token = info.get(info.at()+1);
            if(Equal(token, "}")) {
                info.next(); // }
                break;
            }
            ASTExpression* expr = nullptr;
            SignalAttempt result = ParseExpression(info, expr, false);
            
            if(result == SignalAttempt::SUCCESS) {
                Assert(expr);
                statement->arrayValues.add(expr);
            }

            token = info.get(info.at()+1);
            if(Equal(token, ",")) {
                info.next(); // ,
                // TODO: Error if you see consecutive commas
                // Note that a trailing comma is allowed: { 1, 2, }
                // It's convenient
            } else if(Equal(token, "}")) {
                info.next(); // }
                break;
            } else {
                info.next(); // prevent infinite loop
                ERR_SECTION(
                    ERR_HEAD(token)
                    ERR_MSG("Unexpected token '"<<token<<"' at end of array initializer. Use comma for another element or ending curly brace to end initializer.")
                    ERR_LINE(token, "expected , or }")
                )
            }
        }
        if(statement->varnames.last().arrayLength<1){
            // Set arrayLength explicitly if it was 0.
            // Otherwise the user set a length for the array and defined less values which is okay.
            // The rest of the values in the array will be zero initialized
            statement->varnames.last().arrayLength = statement->arrayValues.size();
        }
        if(statement->arrayValues.size() > statement->varnames.last().arrayLength) {
            ERR_SECTION(
                ERR_HEAD(token) // token should be {
                ERR_MSG("You cannot have more expressions in the array initializer than the array length you specified.")
                // TODO: Show which token defined the array length
                ERR_LINE(lengthTokenOfLastVar, "the maximum length")
                // You could do a token range from the first expression to the last but that could spam the console
                // with 100 expressions which would be annoying so maybe show 5 or 8 values and then do ...
                ERR_LINE(token, ""<<statement->arrayValues.size()<<" expressions")
            )
        }
    }
    token = info.get(info.at()+1);
    if(Equal(token,";")){
        info.next(); // parse ';'. won't crash if at end
    }

    // statement->tokenRange.firstToken = info.get(startIndex);
    // statement->tokenRange.endIndex = info.at()+1;
    return SignalAttempt::SUCCESS;  
}
SignalDefault ParseBody(ParseInfo& info, ASTScope*& bodyLoc, ScopeId parentScope, ParseFlags in_flags, ParseFlags* out_flags){
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

    if(out_flags)
        *out_flags = PARSE_NO_FLAGS;
    bool expectEndingCurlyBrace = false;
    // bool inheritScope = true;
    bool inheritScope = false; // Read note below as to why this is false (why scopes always are created)
    if(bodyLoc){

    } else {
        bodyLoc = info.ast->createBody();

        Token token = info.get(info.at()+1);
        if(Equal(token,"{")) {
            token = info.next();
            expectEndingCurlyBrace = true;
            if(out_flags)
                *out_flags = (ParseFlags)(*out_flags|PARSE_HAS_CURLIES);
            inheritScope = false;
        }
        // NOTE: Always creating scope even with "if true  print("hey")" with no curly brace
        //  because it makes things more consistent and easier to deal with. If a scope wasn't created
        //  then content order must count the statements of the if body. I don't think not creating a
        //  scope has any benefits. Sure, you could declare a variable inside the if statement and use
        //  it outside since it wouldn't be enclosed in a scope but the complication there is
        //  how it should be initialized if the if statement doesn't run (zero or default values I guess?).
        if(inheritScope) {
            // Code to use the current scope for this body.
            bodyLoc->scopeId = info.currentScopeId;
        } else {
            ScopeInfo* scopeInfo = info.ast->createScope(parentScope, info.getNextOrder(), bodyLoc);
            bodyLoc->scopeId = scopeInfo->id;

            info.currentScopeId = bodyLoc->scopeId;
        }
    }

    if(!info.end()){
        bodyLoc->tokenRange.firstToken = info.get(info.at()+1);
        // endToken is set later
    }

    if(!inheritScope || (in_flags & PARSE_TRULY_GLOBAL)) {
        info.nextContentOrder.add(CONTENT_ORDER_ZERO);
    }
    defer {
        if(!inheritScope || (in_flags & PARSE_TRULY_GLOBAL)) {
            info.nextContentOrder.pop(); 
        }
    };

    while(true) {
        Token tok = info.get(info.at() + 1);
        if(IsAnnotation(tok)) {
            if(Equal(tok, "@dump_asm")) {
                info.next();
                bodyLoc->flags |= ASTNode::DUMP_ASM;
            } else if(Equal(tok, "@dump_bc")) {
                info.next();
                bodyLoc->flags |= ASTNode::DUMP_BC;
            } else {
                // Annotation may belong to statement so we don't
                // complain about not recognizing it here.
                break;
            }
        } else {
            break;
        }
    }
    
    
    bool errorsWasIgnored = info.ignoreErrors;
    defer {
        info.ignoreErrors = errorsWasIgnored;
    };

    DynamicArray<ASTStatement*> nearDefers{};

    while(!info.end()){
        Token& token = info.get(info.at()+1);
        
        if((in_flags & PARSE_INSIDE_SWITCH) && IsAnnotation(token)) {
            if(Equal(token,"@fall")) {
                info.next();
                if(out_flags)
                    *out_flags = (ParseFlags)(*out_flags | PARSE_HAS_CASE_FALL);
                continue;
            }
        }
        if(expectEndingCurlyBrace && Equal(token,"}")){
            info.next();
            break;
        }
        if((in_flags & PARSE_INSIDE_SWITCH) && (Equal(token, "case") || (!expectEndingCurlyBrace && Equal(token,"}")))) {
            // we should not info.next on else
            break;
        }
        
        if(Equal(token,";")){
            info.next();
            continue;
        }
        info.ignoreErrors = errorsWasIgnored;
        
        ASTStatement* tempStatement=0;
        ASTStruct* tempStruct=0;
        ASTEnum* tempEnum=0;
        ASTFunction* tempFunction=0;
        ASTScope* tempNamespace=0;
        
        SignalAttempt result=SignalAttempt::BAD_ATTEMPT;

        if(Equal(token,"{")){
            ASTScope* body=0;
            SignalDefault result2 = ParseBody(info,body, info.currentScopeId);
            if(result2!=SignalDefault::SUCCESS){
                info.next(); // skip { to avoid infinite loop
                continue;
            }
            result = CastSignal(result2);
            tempStatement = info.ast->createStatement(ASTStatement::BODY);
            tempStatement->firstBody = body;
            tempStatement->tokenRange = body->tokenRange;
        }
        bool noCode = false;
        bool log_nodeid = false;
        while(true) {
            Token& token2 = info.get(info.at()+1);
            if(IsAnnotation(token2)) {
                if(Equal(token2,"@TEST_ERROR")) {
                    info.ignoreErrors = true;
                    noCode = true;
                    info.next();
                    
                    auto res = ParseAnnotationArguments(info, nullptr);
                    continue;
                } else if(Equal(token2,"@TEST_CASE")) {
                    info.next();

                    auto res = ParseAnnotationArguments(info, nullptr);
                    // while(true) {
                    //     token2 = info.next(); // skip case name
                    //     if(token2.flags & (TOKEN_SUFFIX_SPACE | TOKEN_SUFFIX_LINE_FEED)) {
                    //         break;   
                    //     }
                    // }
                    continue;
                } else if(Equal(token2, "@no-code")) {
                    noCode = true;
                    info.next();
                    continue;
                }  else if(Equal(token2, "@nodeid")) {
                    log_nodeid = true;
                    info.next();
                    continue;
                }
            }
            break;
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
        if(noCode && tempStatement) {
            if(!tempStatement->isNoCode()) {
                tempStatement->setNoCode(noCode);
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
                ERR_SECTION(
                    ERR_HEAD(token)
                    ERR_MSG("Did not expect '"<<token<<"' when parsing body. A new statement, struct or function was expected (or enum, namespace, ...).")
                    ERR_LINE(token,"what")
                )
                // prevent infinite loop. Loop 'only occurs when scoped
                info.next();
            }
        }
        if(result!=SignalAttempt::SUCCESS){
            // TODO: What should be done?
        }
        // We add the AST structures even during error to
        // avoid leaking memory.
        ASTNode* astNode = nullptr;
        if(tempStatement) {
            astNode = (ASTNode*)tempStatement;
            if(tempStatement->type == ASTStatement::DEFER) {
                nearDefers.add(tempStatement);
                if(info.functionScopes.last().loopScopes.size()==0){
                    info.functionScopes.last().defers.add(tempStatement);
                } else {
                    info.functionScopes.last().loopScopes.last().defers.add(tempStatement);
                }
            } else {
                auto add_defers = [&](DynamicArray<ASTStatement*>& defers) {
                    for(int k=defers.size()-1;k>=0;k--){
                        auto myDefer = defers[k];  // my defer, not yours, get your own
                        ASTStatement* deferCopy = info.ast->createStatement(ASTStatement::DEFER);
                        deferCopy->tokenRange = myDefer->tokenRange;
                        deferCopy->firstBody = myDefer->firstBody;
                        deferCopy->sharedContents = true;

                        bodyLoc->add(info.ast, deferCopy);
                        info.nextContentOrder.last()++;
                    }
                };
                if(tempStatement->type == ASTStatement::RETURN) {
                    for(int j=info.functionScopes.last().loopScopes.size()-1;j>=0;j--){
                        auto& loopScope = info.functionScopes.last().loopScopes[j];
                        add_defers(loopScope.defers);
                        // for(int k=loopScope.defers.size()-1;k>=0;k--){
                        //     auto myDefer = info.functionScopes.last().defers[k];  // my defer, not yours, get your own
                        //     ASTStatement* deferCopy = info.ast->createStatement(ASTStatement::DEFER);
                        //     deferCopy->tokenRange = myDefer->tokenRange;
                        //     deferCopy->firstBody = myDefer->firstBody;
                        //     deferCopy->sharedContents = true;

                        //     bodyLoc->add(info.ast, deferCopy);
                        //     info.nextContentOrder.last()++;
                        // }
                    }
                    add_defers(info.functionScopes.last().defers);
                    // for(int k=info.functionScopes.last().defers.size()-1;k>=0;k--){
                    //     auto myDefer = info.functionScopes.last().defers[k];  // my defer, not yours, get your own
                    //     ASTStatement* deferCopy = info.ast->createStatement(ASTStatement::DEFER);
                    //     deferCopy->tokenRange = myDefer->tokenRange;
                    //     deferCopy->firstBody = myDefer->firstBody;
                    //     deferCopy->sharedContents = true;

                    //     bodyLoc->add(info.ast, deferCopy);
                    //     info.nextContentOrder.last()++;
                    // }
                } else if(tempStatement->type == ASTStatement::CONTINUE || tempStatement->type == ASTStatement::BREAK){
                    if(info.functionScopes.last().loopScopes.size()!=0){
                        auto& loopScope = info.functionScopes.last().loopScopes.last();
                        add_defers(loopScope.defers);
                        // for(int k=loopScope.defers.size()-1;k>=0;k--){
                        //     auto myDefer = info.functionScopes.last().defers[k];  // my defer, not yours, get your own
                        //     ASTStatement* deferCopy = info.ast->createStatement(ASTStatement::DEFER);
                        //     deferCopy->tokenRange = myDefer->tokenRange;
                        //     deferCopy->firstBody = myDefer->firstBody;
                        //     deferCopy->sharedContents = true;

                        //     bodyLoc->add(info.ast, deferCopy);
                        //     info.nextContentOrder.last()++;
                        // }
                    } else {
                        // Error should have been printed already
                        // Assert(false);
                    }
                }
                bodyLoc->add(info.ast, tempStatement);
                info.nextContentOrder.last()++;
            }
        }
        if(tempFunction) {
            astNode = (ASTNode*)tempFunction;
            bodyLoc->add(info.ast, tempFunction);
            info.nextContentOrder.last()++;
        }
        if(tempStruct) {
            astNode = (ASTNode*)tempStruct;
            bodyLoc->add(info.ast, tempStruct);
            info.nextContentOrder.last()++;
        }
        if(tempEnum) {
            astNode = (ASTNode*)tempEnum;
            bodyLoc->add(info.ast, tempEnum);
            info.nextContentOrder.last()++;
        }
        if(tempNamespace) {
            astNode = (ASTNode*)tempNamespace;
            bodyLoc->add(info.ast, tempNamespace);
            info.nextContentOrder.last()++;
        }

        if(log_nodeid) {
            Assert(astNode);
            char buf[20];
            int len = snprintf(buf,sizeof(buf),"nodeid = %d",astNode->nodeId);
            Assert(len);
            log::out << log::YELLOW << "Annotation @nodeid: logging node id\n";
            TokenStream* prevStream = nullptr;
            PrintCode(astNode->tokenRange, buf, &prevStream);
            if(tempStatement == astNode && tempStatement->firstExpression) {
                int len = snprintf(buf,sizeof(buf),"nodeid = %d",tempStatement->firstExpression->nodeId);
                Assert(len);
                PrintCode(tempStatement->firstExpression->tokenRange, buf, &prevStream);
            }
         }

        if(result==SignalAttempt::BAD_ATTEMPT){
            // Try again. Important because loop would break after one time if scoped is false.
            // We want to give it another go.
            continue;
        }
        // skip semi-colon if it's there.
        Token tok = info.get(info.at()+1);
        if(Equal(tok,";")){
            info.next();
        }
        // current body may have the same "parentScope == bodyLoc->scopeId" will make sure break 
        if(!expectEndingCurlyBrace && !(in_flags & PARSE_TRULY_GLOBAL) && !(in_flags & PARSE_INSIDE_SWITCH)){
            break;
        }
    }
    
    for(int i = nearDefers.size()-1;i>=0;i--) {
        if(info.functionScopes.last().loopScopes.size()!=0){
            auto& loopScope = info.functionScopes.last().loopScopes.last();
            for(int j=loopScope.defers.size()-1;j>=0;j--) {
                auto& deferNode = info.functionScopes.last().loopScopes.last().defers[j];
                if(deferNode == nearDefers[i]){
                    loopScope.defers.removeAt(j);
                    // if(j==loopScope.defers.size()-1){
                    //     loopScope.defers.pop();
                    // } else {
                    //     loopScope.defers.remove(j);
                    // }
                }
            }
        }
        bodyLoc->add(info.ast,nearDefers[i]);
        info.nextContentOrder.last()++;
    }
    // nearDefers.cleanup(); // cleaned by destructor

    bodyLoc->tokenRange.endIndex = info.at()+1;
    return SignalDefault::SUCCESS;
}

ASTScope* ParseTokenStream(TokenStream* tokens, AST* ast, CompileInfo* compileInfo, std::string theNamespace){
    using namespace engone;
    MEASURE;
    _VLOG(log::out <<log::BLUE<<  "##   Parser   ##\n";)
    
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

        body = info.ast->createNamespace(theNamespace);
        ScopeInfo* newScope = info.ast->createScope(ast->globalScopeId, CONTENT_ORDER_ZERO, body);

        body->scopeId = newScope->id;
        newScope->name = theNamespace;
        info.ast->getScope(newScope->parent)->nameScopeMap[theNamespace] = newScope->id;
    }
    info.currentScopeId = body->scopeId;

    info.functionScopes.add({});
    SignalDefault result = ParseBody(info, body, ast->globalScopeId, PARSE_TRULY_GLOBAL);
    info.functionScopes.pop();
    
    info.compileInfo->compileOptions->compileStats.errors += info.errors;
    
    return body;
}