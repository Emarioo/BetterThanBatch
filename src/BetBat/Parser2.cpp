#include "BetBat/Parser2.h"
#include "BetBat/Compiler.h"

namespace parser {

/*#################################
    Convenient macros for errors
###################################*/

#undef ERR_SECTION
#define ERR_SECTION(CONTENT) BASE_SECTION2(CONTENT)

#undef WARN_SECTION
#define WARN_SECTION(CONTENT) BASE_WARN_SECTION("Parser, ", CONTENT)

#define ERR_SUDDEN_EOF(T_USEFUL,STR,STR_USEFUL)  ERR_SECTION(\
                ERR_HEAD2(info.gettok(-1))\
                ERR_MSG(STR)\
                ERR_LINE2(T_USEFUL, STR_USEFUL)\
                ERR_LINE2(info.gettok(-1), "the sudden end")\
            )

#define ERR_DEFAULT(T,STR,STR_LINE) ERR_SECTION(\
                ERR_HEAD2(T)\
                ERR_MSG(STR)\
                ERR_LINE2(T, STR_LINE)\
            )
#define ERR_DEFAULT_LAZY(T,STR) ERR_SECTION(\
                ERR_HEAD2(T)\
                ERR_MSG(STR)\
                ERR_LINE2(T, "bad")\
            )

#define SIGNAL_SWITCH_LAZY() switch(signal) { case SIGNAL_SUCCESS: break; case SIGNAL_COMPLETE_FAILURE: return signal; default: Assert(false); }

#define SIGNAL_INVALID_DATATYPE(datatype) if(datatype.empty()) { ERR_DEFAULT(info.gettok(),"Token(s) do not conform to a data type.","bad type") return SIGNAL_COMPLETE_FAILURE; } Assert(signal == SIGNAL_SUCCESS);

/*#################################
    Convenient/utility functions
###################################*/

// returns instruction type
// zero means no operation
bool IsSingleOp(OperationType nowOp){
    return nowOp == AST_REFER || nowOp == AST_DEREF || nowOp == AST_NOT || nowOp == AST_BNOT||
        nowOp == AST_FROM_NAMESPACE || nowOp == AST_CAST || nowOp == AST_INCREMENT || nowOp == AST_DECREMENT || nowOp == AST_UNARY_SUB;
}
// info is required to perform next when encountering
// two arrow tokens
OperationType IsOp(lexer::TokenInfo* token, lexer::TokenInfo* token1, const StringView& view, int& extraNext){
    // TODO: Optimize by reordering the cases based on the operations frequencies in the average codebase.
    switch(token->type) {
    case '+':  return AST_ADD;
    case '-':  return AST_SUB;
    case '*':  return AST_MUL;
    case '/':  return AST_DIV;
    case '%':  return AST_MODULUS;
    case '&':  return AST_BAND;
    case '|':  return AST_BOR;
    case '^':  return AST_BXOR;
    case '~':  return AST_BNOT;
    case '<': {
        if((token->flags&lexer::TOKEN_FLAG_SPACE)==0 && token1->type == '<') {
            extraNext=1;
            return AST_BLSHIFT;
        }
        return AST_LESS;
    }
    case '>': {
        if((token->flags&lexer::TOKEN_FLAG_SPACE)==0 && token1->type == '>') {
            extraNext=1;
            return AST_BRSHIFT;
        }
        return AST_GREATER;
    }
    case lexer::TOKEN_IDENTIFIER: {
        // TODO: These should not be identifiers but tokens.
        if(view == "==") return AST_EQUAL;
        if(view == "!=") return AST_NOT_EQUAL;
        if(view == "<=") return AST_LESS_EQUAL;
        if(view == ">=") return AST_GREATER_EQUAL;
        if(view == "&&") return AST_AND;
        if(view == "||") return AST_OR;
        if(view == "..") return AST_RANGE;
    }
    // NOT operation is special
    default: break;
    }

    return (OperationType)0;
}
// OperationType IsAssignOp(Token& token){
//     if(!token.str || token.length!=1) return (OperationType)0; // early exit since all assign op only use one character
//     char chr = *token.str;
//     // TODO: Allow % modulus as assignment?
//     if(chr == '+'||chr=='-'||chr=='*'||chr=='/'||
//         chr=='/'||chr=='|'||chr=='&'||chr=='^'){
//         int _=0;
//         return parser::IsOp(token,_);
//     }
//     return (OperationType)0;
// }
OperationType IsAssignOp(lexer::TokenInfo* token){
    switch(token->type) {
    case '+': return AST_ADD;
    case '-': return AST_SUB;
    case '*': return AST_MUL;
    case '/': return AST_DIV;
    case '|': return AST_BOR;
    case '&': return AST_BAND;
    case '^': return AST_BXOR;
    default: break;
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

enum ParseFlags : u32 {
    PARSE_NO_FLAGS = 0x0,
    // INPUT
    PARSE_INSIDE_SWITCH = 0x1,
    PARSE_TRULY_GLOBAL = 0x2,
    // OUTPUT
    PARSE_HAS_CURLIES = 0x4,
    PARSE_HAS_CASE_FALL = 0x8, // annotation @fall for switch cases
};

/*#####################################
    Declaration of parse functions
######################################*/
// The function will not parse any tokens if they didn't conform to a type. If outTypeId is empty, no tokens were parsed.
SignalIO ParseTypeId(ParseInfo& info, std::string& outTypeId, int* tokensParsed = nullptr);
SignalIO ParseStruct(ParseInfo& info, ASTStruct*& astStruct);
SignalIO ParseNamespace(ParseInfo& info, ASTScope*& astNamespace);
SignalIO ParseEnum(ParseInfo& info, ASTEnum*& astEnum);
// out_arguments may be null to parse but ignore arguments
SignalIO ParseAnnotationArguments(ParseInfo& info, TokenRange* out_arguments);
SignalIO ParseAnnotation(ParseInfo& info, Token* out_annotation_name, TokenRange* out_arguments);
// parses arguments and puts them into fncall->left
SignalIO ParseArguments(ParseInfo& info, ASTExpression* fncall, int* count);
SignalIO ParseExpression(ParseInfo& info, ASTExpression*& expression);
SignalIO ParseFlow(ParseInfo& info, ASTStatement*& statement);
SignalIO ParseOperator(ParseInfo& info, ASTFunction*& function);
// returns 0 if syntax is wrong for flow parsing
SignalIO ParseFunction(ParseInfo& info, ASTFunction*& function, ASTStruct* parentStruct);
SignalIO ParseAssignment(ParseInfo& info, ASTStatement*& statement);
// out token contains a newly allocated string. use delete[] on it
SignalIO ParseBody(ParseInfo& info, ASTScope*& bodyLoc, ScopeId parentScope, ParseFlags in_flags = PARSE_NO_FLAGS, ParseFlags* out_flags = nullptr);

/*###############################
    Code for parse functions
#################################*/
// #define PARSER_OFF
#ifndef PARSER_OFF

SignalIO ParseTypeId(ParseInfo& info, std::string& outTypeId, int* tokensParsed){
    using namespace engone;
    
    int startToken = info.gethead();
    
    int depth = 0;
    bool wasName = false;
    outTypeId = {};
    StringView view{};
    while(true){
        auto token = info.getinfo(&view);
        if(token->type == lexer::TOKEN_EOF)
            break;

        if(token->type == lexer::TOKEN_IDENTIFIER || (wasName = false)) {
            if(wasName) {
                break;
            }
            info.advance();
            wasName = true;
            outTypeId.append(view.ptr,view.len);
        } else if(token->type == '<') {
            info.advance();
            depth++;
            outTypeId.append("<");
         }else if(token->type == '>') {
            if(depth == 0){
                break;
            }
            info.advance();
            depth--;
            outTypeId.append(">");
            
            if(depth == 0){
                break;
            }
        } else if(token->type == '[') {
            auto tok = info.getinfo(&view, 1);
            if(tok->type == ']') {
                info.advance(2);
                if(depth == 0)
                    break;
            } else {
                break;
            }
        } else if(token->type == lexer::TOKEN_NAMESPACE_DELIM) {
            info.advance();
            outTypeId.append("::");
        } else {
            // invalid token for types, we stop
            break;
        }
    }
    while(true) {
        auto tok = info.getinfo();
        if(tok->type == '*') {
            info.advance();
            outTypeId.append("*");
        } else {
            break;
        }

    }
    if(tokensParsed)
        *tokensParsed = info.gethead() - startToken;
    return SIGNAL_SUCCESS;
}
SignalIO ParseStruct(ParseInfo& info, ASTStruct*& astStruct){
    using namespace engone;
    // MEASURE;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _PLOG(FUNC_ENTER)

    StringView name_view{};
    auto name_token = info.getinfo(&name_view);
    auto name_token_tiny = info.gettok();
   
    bool hideAnnotation = false;

    while (name_token->type == lexer::TOKEN_ANNOTATION){
        info.advance();
        // TODO: Parse annotation parentheses
        if(name_view == "hide"){
            hideAnnotation=true;
        // } else if(Equal(name,"@export")) {
        } else {
            Assert(false); // nocheckin, fix error
            // WARN_SECTION(
            //     WARN_HEAD(name)
            //     WARN_MSG("'"<< Token(name.str+1,name.length-1) << "' is not a known annotation for functions.")
            //     WARN_LINE(info.get(info.at()),"unknown");
            // )
        }
        name_token = info.getinfo(&name_view);
        continue;
    }
    
    if(name_token->type != lexer::TOKEN_IDENTIFIER){
        Assert(false); // nocheckin, fix error
        auto bad = info.gettok();
        ERR_SECTION(
            ERR_HEAD2(bad)
            ERR_MSG("Expected a name, "<<name_view<<" isn't.")
            ERR_LINE2(bad,"bad")
        )
        return SIGNAL_COMPLETE_FAILURE;
    }
    info.advance(); // name

    int startIndex = info.gethead();
    astStruct = info.ast->createStruct(name_view);
    astStruct->location = info.srcloc(name_token_tiny);
    // astStruct->tokenRange.firstToken = structToken;
    // astStruct->tokenRange.startIndex = startIndex;
    // astStruct->tokenRange.tokenStream = info.tokens;
    astStruct->polyName = name_view;
    astStruct->setHidden(hideAnnotation);
    StringView view{};
    auto token = info.getinfo(&view);
    if(token->type == '<'){
        info.advance();
        // polymorphic type
        astStruct->polyName += "<";
    
        WHILE_TRUE {
            token = info.getinfo(&view);
            if(token->type == lexer::TOKEN_EOF) {
                ERR_SUDDEN_EOF(name_token_tiny,"Sudden end of file when parsing polymorphic types for struct.","this struct")
                return SIGNAL_COMPLETE_FAILURE;
            }
            if(token->type == '>'){
                if(astStruct->polyArgs.size()==0){
                    auto bad = info.gettok();
                    ERR_DEFAULT_LAZY(bad, "empty polymorphic list for struct")
                }
                info.advance();
                break;
            }
            info.advance(); // must skip name to prevent infinite loop
            if(token->type != lexer::TOKEN_IDENTIFIER){
                auto bad = info.gettok(-1);
                ERR_DEFAULT_LAZY(bad, "'"<<info.lexer->tostring(bad)<<"' is not a valid name for polymorphism. (TODO: Invalidate the struct somehow?)")
            }else{
                ASTStruct::PolyArg arg={};
                arg.name = view;
                astStruct->polyArgs.add(arg);
                astStruct->polyName += view;
            }
            
            token = info.getinfo();
            if(token->type == ','){
                info.advance();
                astStruct->polyName += ",";
                continue;
            }else if(token->type == '>'){
                info.advance();
                break;
            } else{
                auto bad = info.gettok();
                ERR_DEFAULT_LAZY(bad,"'"<<info.lexer->tostring(bad)<<"' is neither , or >.")
                continue;
            }
        }
        astStruct->polyName += ">";
    }
    
    token = info.getinfo(&view);
    if(token->type != '{'){
        auto bad = info.gettok();
        ERR_DEFAULT_LAZY(bad,"Expected { not '"<<info.lexer->tostring(bad)<<"'.")
        return SIGNAL_COMPLETE_FAILURE;
    }
    info.advance();
    
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
    auto errorParsingMembers = SIGNAL_SUCCESS;
    bool hadRecentError = false;
    bool affectedByAlignment=false;
    int union_depth = 0;
    // TODO: May want to change the structure of the while loop
    //  struct { a 9 }   <-  : is expected and not 9, function will quit but that's not good because we still have }.
    //  that will cause all sorts of issue outside. leaving this parse function when } is reached even if errors occured
    //  inside is probably better.
    WHILE_TRUE {
        StringView name_view{};
        auto name = info.getinfo(&name_view);
        auto name_tiny = info.gettok();

        if(name->type == lexer::TOKEN_EOF) {
            ERR_SUDDEN_EOF(name_token_tiny, "Sudden end when parsing struct.", "this struct")
            
            return SIGNAL_COMPLETE_FAILURE;   
        }

        if(name->type == ';'){
            info.advance();
            continue;
        } else if(name->type == '}'){
            info.advance();
            if(union_depth == 0) {
                break;
            }
            union_depth--;
            continue;
        }
        bool wasFunction = false;
        std::string typeToken{};
        int typeEndToken = -1;
        if(name->type == lexer::TOKEN_IDENTIFIER) {
            if (view.equals("fn")) {
                info.advance();
                wasFunction = true;
                // parse function?
                ASTFunction* func = 0;
                Assert(false); // nocheckin
                // SignalIO signal = ParseFunction(info, func, astStruct);
                // switch(signal) {
                // case SIGNAL_COMPLETE_FAILURE: return SIGNAL_COMPLETE_FAILURE;
                // case SIGNAL_SUCCESS: break;
                // default: Assert(false);
                // }
                func->parentStruct = astStruct;
                astStruct->add(func);
                // ,func->polyArgs.size()==0?&func->baseImpl:nullptr);
            } else if(view.equals("union")) {
                info.advance();
                Assert(false);
                
                // Assert(union_depth < 1); // nocheckin, support nested unions and structs
                
                // Token tok = info.gettok();
                // if(IsName(tok)) {
                //     ERR_SECTION(
                //         ERR_HEAD2(tok)
                //         ERR_MSG("Named unions not allowed inside structs.")
                //         ERR_LINE2(tok,"bad")
                //     )
                //     info.advance(); // skip it, continue as normal
                // }
                
                // tok = info.gettok();
                
                // if(tok.type == '{') {
                //     info.advance();
                //     union_depth++;
                //     continue;
                // } else {
                //     ERR_SECTION(
                //         ERR_HEAD2(tok)
                //         ERR_MSG("Expected 'union {' but there was no curly brace.")
                //         ERR_LINE2(tok,"should be a curly brace")
                //     )
                //     // what do we do?
                // }
            } else if(view.equals("struct")) {
                Assert(("struct in struct not allowed",false)); // error instead
                // ERR_SECTION(
                //     ERR_HEAD2(name)
                //     ERR_MSG("Structs not allowed inside structs.")
                //     ERR_LINE2(name,"bad")
                // )
            } else {
                for(auto& mem : astStruct->members) {
                    if(mem.name == view) {
                        ERR_SECTION(
                            ERR_HEAD2(name_tiny)
                            ERR_MSG("The name '"<<info.lexer->tostring(name_tiny)<<"' is already used in another member of the struct. You cannot have two members with the same name.")
                            ERR_LINE2(mem.location, "last")
                            ERR_LINE2(name_tiny, "now")
                        )
                        break;
                    }
                }
                info.advance();
                auto token = info.getinfo();
                if(token->type != ':'){
                    auto bad = info.gettok();
                    ERR_DEFAULT_LAZY(bad,"Expected : not '"<<info.lexer->tostring(bad)<<"'.")
                    return SIGNAL_COMPLETE_FAILURE;
                }
                info.advance();
                auto type_tok = info.gettok();
                auto signal = ParseTypeId(info,typeToken,nullptr);

                SIGNAL_INVALID_DATATYPE(typeToken)
                // switch(signal){
                //     case SIGNAL_SUCCESS: break;
                //     case SIGNAL_COMPLETE_FAILURE: {
                //         if(typeToken.size() != 0) {
                //             ERR_SECTION(
                //                 ERR_HEAD2(type_tok)
                //                 ERR_MSG("Failed parsing type '"<<typeToken<<"'.")
                //                 ERR_LINE2(type_tok,"bad")
                //             )
                //         } else {
                //             ERR_SECTION(
                //                 ERR_HEAD2(type_tok)
                //                 ERR_MSG("Failed parsing type '"<<info.lexer->tostring(type_tok)<<"'.")
                //                 ERR_LINE2(type_tok,"bad")
                //             )
                //         }
                //         return SIGNAL_COMPLETE_FAILURE;
                //     }
                //     default: Assert(false);
                // }

                // typeEndToken = info.at()+1;
                int arrayLength = -1;
                auto token1 = info.gettok(0);
                StringView num_data = {};
                auto token2_info = info.getinfo(&num_data, 1);
                auto token2 = info.gettok(1);
                auto token3 = info.gettok(2);
                if(token1.type == '[' && token3.type == ']') {
                    info.advance(3);

                    if(token2.type == lexer::TOKEN_LITERAL_INTEGER) {
                        // u64 num = 0;
                        // memcpy(&num, num_data.ptr, num_data.len);
                        arrayLength = lexer::ConvertInteger(num_data);
                        if(arrayLength<0){
                            ERR_SECTION(
                                ERR_HEAD2(token2)
                                ERR_MSG("Array cannot have negative size.")
                                ERR_LINE2(token2,"< 0")
                            )
                            arrayLength = 0;
                        }
                    } else {
                        ERR_SECTION(
                            ERR_HEAD2(token2)
                            ERR_MSG("The length of an array can only be specified with number literals. Use macros to avoid magic numbers. Constants have not been implemented but when they have, they will work too.")
                            ERR_LINE2(token2, "must be positive integer literal")
                        )
                    }
                    std::string* str = info.ast->createString();
                    *str = "Slice<";
                    *str += typeToken;
                    *str += ">";
                    typeToken = *str;
                }
                Assert(arrayLength==-1); // arrays in structs not implemented yet
                // std::string temps = typeToken;
                
                TypeId typeId = info.ast->getTypeString(typeToken);
                
                signal = SIGNAL_SUCCESS;
                ASTExpression* defaultValue=0;
                token = info.getinfo();
                if(token->type == '='){
                    info.advance();
                    signal = ParseExpression(info,defaultValue);
                }
                switch(signal) {
                case SIGNAL_SUCCESS: {
                    astStruct->members.add({});
                    auto& mem = astStruct->members.last();
                    mem.name = name_view;
                    mem.defaultValue = defaultValue;
                    mem.stringType = typeId;
                    break;
                }
                case SIGNAL_COMPLETE_FAILURE: return SIGNAL_COMPLETE_FAILURE;
                default: Assert(false);
                }
            }
        } else {
            info.advance();
            if(!hadRecentError){
                ERR_SECTION(
                    ERR_HEAD2(name_tiny)
                    ERR_MSG("Expected a name, "<<info.lexer->tostring(name_tiny)<<" isn't.")
                    ERR_LINE2(name_tiny,"bad")
                )
                return SIGNAL_COMPLETE_FAILURE;
            }
            hadRecentError=true;
            // errorParsingMembers = SignalAttempt::FAILURE;
            continue;   
        }
        StringView tok_view{};
        auto token = info.getinfo(&tok_view);
        if(tok_view == "fn" || tok_view == "struct" || tok_view == "union"){
            // semi-colon not required
            continue;
        }else if(token->type == ';'){
            info.advance();
            hadRecentError=false;
            continue;
        }else if(token->type == '}'){
            continue; // handle break at beginning of loop
        }else{
            if(wasFunction)
                continue;
            auto token_tiny = info.gettok();
            if(!hadRecentError){ // no need to print message again, the user already know there are some issues here.
                ERR_SECTION(
                    ERR_HEAD2(token_tiny)
                    ERR_MSG("Expected a curly brace or semi-colon to mark the end of the member (was: "<<info.lexer->tostring(token_tiny)<<").")
                    ERR_LINE2(token_tiny,"bad")
                )
                // ERR_SECTION(
                // ERR_HEAD2(token,"Expected a curly brace or semi-colon to mark the end of the member (was: "<<token<<").\n\n";
                //     if(typeEndToken!=-1){
                //         TokenRange temp{};
                //         temp.firstToken = typeToken;
                //         temp.endIndex = typeEndToken;
                //         PrintCode(temp,"evaluated type");
                //     }
                //     ERR_LINE2(token.tokenIndex,"bad");
                // )
            }
            hadRecentError=true;
            // errorParsingMembers = SignalAttempt::FAILURE;
            continue;
        }
    }
    // astStruct->tokenRange.endIndex = info.at()+1;
    _GLOG(log::out << "Parsed struct "<<log::LIME<< name_view <<log::NO_COLOR << " with "<<astStruct->members.size()<<" members\n";)
    return SIGNAL_SUCCESS;
}
SignalIO ParseNamespace(ParseInfo& info, ASTScope*& astNamespace){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _PLOG(FUNC_ENTER)
    int startIndex = info.gethead();

    bool hideAnnotation = false;

    StringView name_view{};
    auto name = info.getinfo(&name_view);
    while (name->type == lexer::TOKEN_ANNOTATION){
        auto tok = info.gettok();
        info.advance();
        if(name_view == "hide"){
            hideAnnotation=true;
        } else {
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("'"<< info.lexer->tostring(tok) << "' is not a known annotation for functions.")
                ERR_LINE2(tok,"unknown")
            )
        }
        name = info.getinfo(&name_view);
        continue;
    }

    if(name->type != lexer::TOKEN_IDENTIFIER){
        auto tok = info.gettok();
        ERR_SECTION(
            ERR_HEAD2(tok)
            ERR_MSG("Expected a name, "<<info.lexer->tostring(tok)<<" isn't.")
            ERR_LINE2(tok,"bad");
        )
        return SIGNAL_COMPLETE_FAILURE;
    }
    info.advance();

    auto tok = info.gettok();
    if(tok.type != '{'){
        ERR_SECTION(
            ERR_HEAD2(tok)
            ERR_MSG("Namespaces expects a '{' after the name of the namespace.")
            ERR_LINE2(tok,"this is not a '{'")
        )
        return SIGNAL_COMPLETE_FAILURE;
    }
    info.advance();
    
    int nextId=0;
    astNamespace = info.ast->createNamespace(name_view);
    // astNamespace->tokenRange.firstToken = token;
    // astNamespace->tokenRange.startIndex = startIndex;
    // astNamespace->tokenRange.tokenStream = info.tokens;
    astNamespace->setHidden(hideAnnotation);

    ScopeInfo* newScope = info.ast->createScope(info.currentScopeId, info.getNextOrder(), astNamespace);
    astNamespace->scopeId = newScope->id;

    newScope->name = name_view;
    info.ast->getScope(newScope->parent)->nameScopeMap[name_view] = newScope->id;
    
    ScopeId prevScope = info.currentScopeId;
    defer { info.currentScopeId = prevScope; };

    info.currentScopeId = newScope->id;

    StringView view{};
    auto error = SIGNAL_SUCCESS;

    while(true){
        auto token = info.getinfo(&view);
        if(token->type == lexer::TOKEN_EOF)
            break;
        // Token& token = info.gettok();
        if(token->type == '}'){
            info.advance();
            break;
        }
        ASTStruct* tempStruct=0;
        ASTEnum* tempEnum=0;
        ASTFunction* tempFunction=0;
        ASTScope* tempNamespace=0;
    
        SignalIO signal;

        if(token->type == lexer::TOKEN_FUNCTION) {
            info.advance();
            signal = ParseFunction(info,tempFunction, nullptr);
        } else if(token->type == lexer::TOKEN_STRUCT) {
            info.advance();
            signal = ParseStruct(info,tempStruct);
        } else if(token->type == lexer::TOKEN_ENUM) {
            info.advance();
            signal = ParseEnum(info,tempEnum);
        } else if(token->type == lexer::TOKEN_NAMESPACE) {
            info.advance();
            signal = ParseNamespace(info,tempNamespace);
        } else {
            auto token = info.gettok();
            ERR_SECTION(
                ERR_HEAD2(token)
                ERR_MSG("Unexpected '"<<info.lexer->tostring(token)<<"' (ParseNamespace).")
                ERR_LINE2(token,"bad")
            )
            
            info.advance();
        }
        switch(signal) {
        case SIGNAL_SUCCESS: break;
        case SIGNAL_COMPLETE_FAILURE: return SIGNAL_COMPLETE_FAILURE;
        default: Assert(false);
        }
            
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

    // astNamespace->tokenRange.endIndex = info.at()+1;
    _PLOG(log::out << "Namespace "<<name_view << "\n";)
    return error;
}

SignalIO ParseEnum(ParseInfo& info, ASTEnum*& astEnum){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _PLOG(FUNC_ENTER)
    int startIndex = info.gethead();

    bool hideAnnotation = false;

    ASTEnum::Rules enumRules = ASTEnum::NONE;

    StringView view_name{};
    auto token_name = info.getinfo(&view_name);
    while (token_name->type == lexer::TOKEN_ANNOTATION){
        if(view_name == "hide") {
            hideAnnotation=true;
        } else if(view_name == "specified") {
            enumRules = (ASTEnum::Rules)(enumRules | ASTEnum::SPECIFIED);
        } else if(view_name == "bitfield") {
            enumRules = (ASTEnum::Rules)(enumRules | ASTEnum::BITFIELD);
        } else if(view_name == "unique") {
            enumRules = (ASTEnum::Rules)(enumRules | ASTEnum::UNIQUE);
        } else if(view_name == "enclosed") {
            enumRules = (ASTEnum::Rules)(enumRules | ASTEnum::ENCLOSED);
        } else {
            auto tok = info.gettok();
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("'"<< info.lexer->tostring(tok) << "' is not a known annotation for enums.")
                ERR_LINE2(tok,"unknown")
            )
        }
        info.advance();
        token_name = info.getinfo();
        continue;
    }

    if(token_name->type != lexer::TOKEN_IDENTIFIER){
        auto tok = info.gettok();
        ERR_SECTION(
            ERR_HEAD2(tok)
            ERR_MSG("You must have a valid name after enum keywords.")
            ERR_LINE2(tok,"not a name")
        )
        return SIGNAL_COMPLETE_FAILURE;
    }
    info.advance(); // name

    TypeId colonType = {};

    auto token = info.gettok();
    if(token.type == ':') {
        info.advance();

        std::string tokenType{};
        auto signal = ParseTypeId(info, tokenType);
        
        SIGNAL_INVALID_DATATYPE(tokenType)

        colonType = info.ast->getTypeString(tokenType);
    }
    
    token = info.gettok();
    if(token.type != '{'){
        ERR_SECTION(
            ERR_HEAD2(token)
            ERR_MSG("Enum should have a curly brace after it's name (or type if you specified one).")
            ERR_LINE2(token,"not a '{'")
        )
        return SIGNAL_COMPLETE_FAILURE;
    }
    info.advance();
    i64 nextValue=0;
    if(enumRules & ASTEnum::BITFIELD) {
        nextValue = 1;
    }
    astEnum = info.ast->createEnum(view_name);
    astEnum->rules = enumRules;
    if (colonType.isValid()) // use default type in astEnum if type isn't specified
        astEnum->colonType = colonType;
    // astEnum->tokenRange.firstToken = enumToken;
    // astEnum->tokenRange.startIndex = startIndex;
    // astEnum->tokenRange.tokenStream = info.tokens;
    astEnum->setHidden(hideAnnotation);
    auto error = SIGNAL_SUCCESS;
    WHILE_TRUE {
        StringView mem_name_view{};
        auto name_tok = info.gettok();
        auto name = info.getinfo(&mem_name_view);
        
        if(name->type == '}'){
            info.advance();
            break;
        }

        bool ignoreRules = false;
        if(name->type == lexer::TOKEN_ANNOTATION && mem_name_view == "norules") {
            info.advance();
            ignoreRules = true;
            name_tok = info.gettok();
            name = info.getinfo(&mem_name_view);
        }

        
        if(name->type != lexer::TOKEN_IDENTIFIER){
            auto tok = info.gettok();
            info.advance(); // move forward to prevent infinite loop
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Expected a valid name for enum member.")
                ERR_LINE2(tok,"not a valid name")
            )
            return SIGNAL_COMPLETE_FAILURE;
        }
        // name->flags &= ~TOKEN_MASK_SUFFIX;

        // bool semanticError = false;
        // if there was an error you could skip adding the member but that
        // cause a cascading effect of undefined AST_ID which are referring to the member 
        // that had an error. It's probably better to add the number but use a zero or something as value.

        info.advance();
        StringView view{};
        auto token = info.getinfo(&view);
        if(token->type == '='){
            info.advance();
            token = info.getinfo(&view);
            // TODO: Handle expressions
            // TODO: Handle negative values (automatically done if you fix expressions)
            if(token->type == lexer::TOKEN_LITERAL_INTEGER){
                info.advance();
                nextValue = lexer::ConvertInteger(view);
                token = info.getinfo();
            } else if(token->type == lexer::TOKEN_LITERAL_INTEGER){
                info.advance();
                nextValue = lexer::ConvertHexadecimal(view);
                token = info.getinfo();
            } else if(token->type == lexer::TOKEN_LITERAL_STRING && (token->flags & lexer::TOKEN_FLAG_SINGLE_QUOTED) && view.len == 1) {
                info.advance();
                nextValue = *view.ptr;
                token = info.getinfo();
            } else {
                auto tok = info.gettok();
                ERR_SECTION(
                    ERR_HEAD2(tok)
                    ERR_MSG("Values for enum members can only be a integer literal. In the future, any constant expression will be allowed.")
                    ERR_LINE2(tok,"not an integer literal")
                )
            }
        } else {
            if(!ignoreRules && (enumRules & ASTEnum::SPECIFIED)) {
                auto tok = info.gettok(-1);
                ERR_SECTION(
                    ERR_HEAD2(tok)
                    ERR_MSG("Enum uses @specified but the value of the member was not specified. Explicitly give all members a value!")
                    ERR_LINE2(tok,"this member")
                )
            }
        }

        if(!ignoreRules && (enumRules & ASTEnum::UNIQUE)) {
            for(int i=0;i<astEnum->members.size();i++) {
                auto& mem = astEnum->members[i];
                
                if(mem.enumValue == nextValue) {
                    ERR_SECTION(
                        ERR_HEAD2(name_tok)
                        ERR_MSG("Enum uses @unique but the value of member '"<<mem_name_view<<"' collides with member '"<<mem.name<<"'. There cannot be any duplicates!")
                        ERR_LINE2(mem.location, "value "<<mem.enumValue)
                        ERR_LINE2(name_tok, "value "<<nextValue)
                    )
                }
            }
        }
        for(int i=0;i<astEnum->members.size();i++) {
            auto& mem = astEnum->members[i];
            if(mem.name == mem_name_view) {
                ERR_SECTION(
                    ERR_HEAD2(name_tok)
                    ERR_MSG("Enum cannot have two members with the same name.")
                    ERR_LINE2(mem.location, mem.name)
                    ERR_LINE2(name_tok, mem_name_view)
                )
            }
        }
        Assert(nextValue < 0xFFFF'FFFF);
        astEnum->members.add({mem_name_view,(int)(nextValue)});
        astEnum->members.last().ignoreRules = ignoreRules;
        astEnum->members.last().location = info.srcloc(name_tok);
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
        
        token = info.getinfo();
        if(token->type == ','){
            info.advance();
            continue;
        }else if(token->type == '}'){
            info.advance();
            break;
        }else{
            auto tok = info.gettok();
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Unexpected token in enum body.")
                ERR_LINE2(tok,"why is it here?");
            )
            return SIGNAL_COMPLETE_FAILURE;
            // error = SignalAttempt::FAILURE;
            // continue;
        }
    }
    // astEnum->tokenRange.endIndex = info.at()+1;

    // for(auto& mem : astEnum->members) {
    //     log::out << mem.name << " = " << mem.enumValue<<"\n";
    // }

    // auto typeInfo = info.ast->getTypeInfo(info.currentScopeId, name, false, true);
    // int strId = info.ast->getTypeString(name);
    // if(!typeInfo){
        // ERR_SECTION(
        // ERR_HEAD2(name, name << " is taken\n";
        // info.ast->destroy(astEnum);
        // astEnum = 0;
        // return PARSE_ERROR;
    // }
    // typeInfo->astEnum = astEnum;
    // typeInfo->_size = 4; // i32
    _PLOG(log::out << "Parsed enum "<<log::LIME<< view_name <<log::NO_COLOR <<" with "<<astEnum->members.size()<<" members\n";)
    return error;
}
#ifdef gone
SignalIO ParseAnnotationArguments(ParseInfo& info, TokenRange* out_arguments) {
    // MEASURE;

    if(out_arguments)
        *out_arguments = {};

    // Can the previous token have a suffix? probably not?
    Token& prev = info.get(info.at());
    if(prev.flags & (TOKEN_MASK_SUFFIX)) {
        return SignalDefault::SIGNAL_SUCCESS;
    }

    Token& tok = info.gettok();
    if(!Equal(tok,"(")){
        return SignalDefault::SIGNAL_SUCCESS;
    }
    info.advance(); // skip (

    int depth = 0;

    while(!info.end()) {
        Token& tok = info.gettok();
        if(Equal(tok,")")){
            if(depth == 0) {
                info.advance();
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

        info.advance();
    }
    
    return SignalDefault::SIGNAL_SUCCESS;
}
SignalIO ParseAnnotation(ParseInfo& info, Token* out_annotation_name, TokenRange* out_arguments) {
    // MEASURE;

    *out_annotation_name = {};
    *out_arguments = {};

    Token& tok = info.gettok();
    if(!IsAnnotation(tok)) {
        return SignalDefault::SIGNAL_SUCCESS;
    }

    *out_annotation_name = info.advance();

    // ParseAnnotationArguments check for suffix of the previous token
    // Args will not be parsed in cases like this: @hello (sad, arg) but will here: @hello(happy)
    SignalDefault result = ParseAnnotationArguments(info, out_arguments);

    Assert(result == SignalDefault::SIGNAL_SUCCESS); // what do we do if args fail?
    return SignalDefault::SIGNAL_SUCCESS;
}
#endif
SignalIO ParseArguments(ParseInfo& info, ASTExpression* fncall, int* count){
    ZoneScopedC(tracy::Color::OrangeRed1);

    // TODO: sudden end, error handling
    bool expectComma=false;
    bool mustBeNamed=false;
    lexer::Token prevNamed = {};
    StringView view{};
    WHILE_TRUE {
        auto token = info.getinfo(&view);
        auto tok = info.gettok();
        if(tok.type == ')'){
            info.advance();
            break;
        }
        if(expectComma){
            if(tok.type == ','){
                info.advance();
                expectComma=false;
                continue;
            }
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Expected ',' to supply more arguments or ')' to end fncall (found "<<info.lexer->tostring(tok)<<" instead).")
            )
            return SIGNAL_COMPLETE_FAILURE;
        }
        bool named=false;
        auto tok_eq = info.gettok(1);
        if(tok.type == lexer::TOKEN_IDENTIFIER && tok_eq.type == '='){
            info.advance(2);
            prevNamed = tok;
            named=true;
            mustBeNamed = true;
        } else if(mustBeNamed){
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Arguments after a named argument must also be named in a function call.")
                ERR_LINE2(tok,"this argument should be named..")
                ERR_LINE2(prevNamed,"...because of this")
            )
            // return or continue could desync the parsing so don't do that.
        }

        ASTExpression* expr=0;
        auto signal = ParseExpression(info,expr);
        
        SIGNAL_SWITCH_LAZY()

        if(named){
            expr->namedValue = view;
        } else {
            fncall->nonNamedArgs++;
        }
        fncall->args.add(expr);
        (*count)++;
        expectComma = true;
    }
    return SIGNAL_SUCCESS;
}

SignalIO ParseExpression(ParseInfo& info, ASTExpression*& expression){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _PLOG(FUNC_ENTER)
    
    // if(info.end()){
    //     if(attempt)
    //         return SignalAttempt::BAD_ATTEMPT;
    //     return SignalAttempt::FAILURE;
    // }

    // std::vector<ASTExpression*> values;
    // std::vector<Token> extraTokens; // ops, assignOps and castTypes doesn't store the token so you know where it came from. We therefore use this array.
    // std::vector<OperationType> ops;
    // std::vector<OperationType> assignOps;
    // std::vector<TypeId> castTypes;
    // std::vector<Token> namespaceNames;
    
    // TODO: This is bad, improved somehow
    TINY_ARRAY(ASTExpression*, values, 5);
    TINY_ARRAY(lexer::Token, extraTokens, 5); // ops, assignOps and castTypes doesn't store the token so you know where it came from. We therefore use this array.
    TINY_ARRAY(OperationType, ops, 5);
    TINY_ARRAY(OperationType, assignOps, 5);
    TINY_ARRAY(TypeId, castTypes, 5);
    TINY_ARRAY(std::string, namespaceNames, 5);

    defer { for(auto e : values) info.ast->destroy(e); };

    bool shouldComputeExpression = false;

    StringView view{};
    auto token = info.getinfo(&view);
    if(token->type == lexer::TOKEN_ANNOTATION && view == "run") {
        info.advance();
        shouldComputeExpression = true;
    }

    // bool negativeNumber=false;
    bool expectOperator=false;
    WHILE_TRUE_N(10000) {
        int tokenIndex = info.gethead();
        auto token = info.getinfo(&view);
        
        OperationType opType = (OperationType)0;
        bool ending=false;
        
        if(expectOperator){
            int extraOpNext=0;
            auto token1 = info.getinfo();
            if(token->type == '.'){
                info.advance();
                
                StringView member_view{};
                auto member_tok = info.getinfo(&member_view);
                if(member_tok->type != lexer::TOKEN_IDENTIFIER){
                    auto bad = info.gettok();
                    ERR_DEFAULT_LAZY(bad, "Expected a property name, not "<<info.lexer->tostring(bad)<<".")
                    continue;
                }
                info.advance();

                auto tok = info.getinfo(&view);
                auto poly_tiny = info.gettok();
                std::string polyTypes="";
                if(tok->type == '<' && 0==(tok->flags&lexer::TOKEN_FLAG_SPACE)){
                    auto signal = ParseTypeId(info, polyTypes);

                    SIGNAL_INVALID_DATATYPE(polyTypes)
                }

                int startToken=info.gethead(); // start of fncall
                tok = info.getinfo();
                auto tok_paren_tiny = info.gettok();
                if(tok->type == '('){
                    info.advance();
                    // fncall for strucy methods
                    ASTExpression* tmp = info.ast->createExpression(TypeId(AST_FNCALL));
                    tmp->name = std::string(member_view) + polyTypes;

                    // Create "this" argument in methods
                    // tmp->args->add(values.last());
                    tmp->args.add(values.last());
                    tmp->nonNamedArgs++;
                    values.pop();
                    
                    tmp->setMemberCall(true);

                    // Parse the other arguments
                    int count = 0;
                    Assert(false); // nocheckin
                    // auto signal = ParseArguments(info, tmp, &count);
                    // switch(signal){
                    //     case SIGNAL_SUCCESS: break;
                    //     case SIGNAL_COMPLETE_FAILURE: {
                    //         info.ast->destroy(tmp);
                    //         return SIGNAL_COMPLETE_FAILURE;
                    //     }
                    //     default: Assert(false);
                    // }

                    _PLOG(log::out << "Parsed call "<<count <<"\n";)
                    // tmp->tokenRange.firstToken = memberName;
                    // tmp->tokenRange.startIndex = startToken;
                    // tmp->tokenRange.endIndex = info.at()+1;
                    // tmp->tokenRange.tokenStream = info.tokens;

                    values.add(tmp);
                    continue;
                }
                if(polyTypes.length()!=0){
                    ERR_SECTION(
                        ERR_HEAD2(poly_tiny)
                        ERR_MSG("Polymorphic arguments like this: 'var.member<int>()' is only allowed with function calls. 'var.member<int>' is NOT allowed because it has no meaning.")
                        ERR_LINE2(poly_tiny,"should not be polymorphic")
                    )
                }
                
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_MEMBER));
                tmp->name = member_view;
                tmp->left = values.last();
                values.pop();
                // tmp->tokenRange.firstToken = tmp->left->tokenRange.firstToken;
                // tmp->tokenRange.startIndex = tmp->left->tokenRange.startIndex;
                // tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
                values.add(tmp);
                continue;
            } else if (token->type == '=') {
                info.advance();

                ops.add(AST_ASSIGN);
                assignOps.add((OperationType)0);

                _PLOG(log::out << "Operator "<<token<<"\n";)
            } else if((opType = IsAssignOp(token)) && info.gettok(1).type  == '=') {
                info.advance(2);

                ops.add(AST_ASSIGN);
                assignOps.add(opType);

                _PLOG(log::out << "Operator "<<token<<"\n";)
            } else if((opType = parser::IsOp(token,token1,view,extraOpNext))){

                // if(Equal(token,"*") && (info.now().flags&lexer::TOKEN_FLAG_NEWLINE)){
                if(info.getinfo(nullptr,-1)->flags&lexer::TOKEN_FLAG_NEWLINE){
                    auto tok0 = info.gettok(-1);
                    auto tok1 = info.gettok();
                    ERR_SECTION(
                        ERR_HEAD2(tok1, ERROR_AMBIGUOUS_PARSING)
                        ERR_MSG("Possible ambiguous meaning! '"<<info.lexer->tostring(tok1) << "' is treated as a binary operation (operation with two expressions) but the operator was placed on a new line. "
                        "Was your intention a unary operation where the left expressions is it's own statement and unrelated to the expression on the new line? Please put a semi-colon to solve this ambiguity.")
                        ERR_LINE2(tok0, "semi-colon is recommended after statements");
                        ERR_LINE2(tok1, "currently a binary operation");
                        ERR_EXAMPLE(1, "5\n*ptr = 9\n5 * ptr = 9");
                    )
                }
                info.advance(extraOpNext + 1);
                // bit shifts will use a second token, hence extraOpNext

                ops.add(opType);

                _PLOG(log::out << "Operator "<<token<<"\n";)
            // } else if (Equal(token,"..")){
            //     info.advance();
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
            } else if(token->type == '['){
                auto token_tiny = info.gettok();
                // int startIndex = info.at()+1;
                info.advance();

                ASTExpression* indexExpr=nullptr;
                auto signal = ParseExpression(info, indexExpr);
                switch(signal) {
                case SIGNAL_SUCCESS: break;
                case SIGNAL_COMPLETE_FAILURE: return SIGNAL_COMPLETE_FAILURE;
                default: Assert(false);
                }

                auto tok = info.gettok();
                if(tok.type == ']'){
                    ERR_SECTION(
                        ERR_HEAD2(tok)
                        ERR_MSG("Expected an ']' to end the index operator.")
                        ERR_LINE2(tok,"should be ']'")
                        ERR_LINE2(token_tiny,"index operator starts here")
                    )
                    return SIGNAL_COMPLETE_FAILURE;
                } else {
                    info.advance();
                }

                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_INDEX));
                // tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = startIndex;
                // tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;

                tmp->left = values.last();
                values.pop();
                tmp->right = indexExpr;
                values.add(tmp);
                tmp->constantValue = tmp->left->constantValue && tmp->right->constantValue;
                continue;
            } else if(token->type == lexer::TOKEN_IDENTIFIER && view == "++"){ // TODO: Optimize
                info.advance();

                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_INCREMENT));
                // tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                // tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
                tmp->setPostAction(true);

                tmp->left = values.last();
                values.pop();
                values.add(tmp);
                tmp->constantValue = tmp->left->constantValue;

                continue;
            } else if(token->type == lexer::TOKEN_IDENTIFIER && view == "++"){ // TODO: Optimize
                info.advance();
                // attempt = false;

                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_DECREMENT));
                // tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                // tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
                tmp->setPostAction(true);

                tmp->left = values.last();
                values.pop();
                values.add(tmp);
                tmp->constantValue = tmp->left->constantValue;

                continue;
            } else if(token->type == ')'){
                // token = info.advance();
                ending = true;
            } else {
                ending = true;
                if(token->type == ';'){
                    // info.advance();
                } else {
                    // Token prev = info.now();
                    // // Token next = info.get(info.at()+2);
                    // if((prev.flags&lexer::TOKEN_FLAG_NEWLINE) == 0 
                    //      && !Equal(token,"}") && !token.type == ',' && !token.type == '{' && !Equal(token,"]")
                    //      && !Equal(prev,")"))
                    // {
                    //     // WARN_HEAD3(token, "Did you forget the semi-colon to end the statement or was it intentional? Perhaps you mistyped a character? (put the next statement on a new line to silence this warning)\n\n"; 
                    //     //     ERR_LINE2(tokenIndex-1, "semi-colon here?");
                    //     //     // ERR_LINE2(tokenIndex, "; before this?");
                    //     //     )
                    //     // TODO: ERROR instead of warning if special flag is set
                    // }
                }
            }
        }else{
            bool cstring = false;
            if(token->type == lexer::TOKEN_ANNOTATION) {
                if(view == "cstr") {
                    info.advance();

                    // TokenRange args; // TODO: Warn if cstr has errors?
                    // ParseAnnotationArguments(info, );

                    token = info.getinfo(&view);
                    cstring = true;
                }
            }
            if(token->type == '&') {
                info.advance();
                ops.add(AST_REFER);
                continue;
            }else if(token->type == '*'){
                info.advance();
                ops.add(AST_DEREF);
                continue;
            } else if(token->type == '!'){
                info.advance();
                ops.add(AST_NOT);
                continue;
            } else if(token->type == '~'){
                info.advance();
                ops.add(AST_BNOT);
                continue;
            } else if(token->type == lexer::TOKEN_IDENTIFIER && (view == "cast" || view == "cast_unsafe") ){
                auto token_tiny = info.gettok();
                info.advance();
                auto tok = info.gettok();
                if(tok.type != '<'){
                    ERR_SECTION(
                        ERR_HEAD2(tok)
                        ERR_MSG("Expected < not "<<info.lexer->tostring(tok)<<".")
                        ERR_LINE2(tok,"bad")
                    )
                    continue;
                }
                info.advance();
                tok = info.gettok();
                std::string datatype{};
                auto signal = ParseTypeId(info,datatype);

                SIGNAL_INVALID_DATATYPE(datatype)

                // switch(signal) {
                // case SIGNAL_SUCCESS: break;
                // case SIGNAL_COMPLETE_FAILURE: return SIGNAL_COMPLETE_FAILURE;
                // default: Assert(false);
                // }

                // if(tokenTypeId.empty()){
                //     ERR_SECTION(
                //         ERR_HEAD2(tok)
                //         ERR_MSG(info.lexer->tostring(tok) << "is not a valid data type.")
                //         ERR_LINE2(tok,"bad");
                //     )
                //     return SIGNAL_COMPLETE_FAILURE;
                // }
                tok = info.gettok();
                if(tok.type == '>'){
                    ERR_SECTION(
                        ERR_HEAD2(tok)
                        ERR_MSG("expected > not "<< info.lexer->tostring(tok)<<".")
                        ERR_LINE2(tok,"bad");
                    )
                }
                info.advance();
                ops.add(AST_CAST);
                // NOTE: unsafe cast is handled further down

                // TypeId dt = info.ast->getTypeInfo(info.currentScopeId,tokenTypeId)->id;
                // castTypes.add(dt);
                TypeId strId = info.ast->getTypeString(datatype);
                castTypes.add(strId);
                extraTokens.add(token_tiny);
                continue;
            } else if(token->type == '-'){
                info.advance();
                ops.add(AST_UNARY_SUB);
                continue;
            } else if(token->type == lexer::TOKEN_IDENTIFIER && view == "++"){
                info.advance();
                ops.add(AST_INCREMENT);
                continue;
            } else if(token->type == lexer::TOKEN_IDENTIFIER && view == "--"){
                info.advance();
                ops.add(AST_DECREMENT);
                continue;
            }

            if(token->type == lexer::TOKEN_LITERAL_INTEGER){
                auto  token_tiny = info.gettok();
                info.advance();

                bool negativeNumber = false;
                if (ops.size()>0 && ops.last() == AST_UNARY_SUB){
                    ops.pop();
                    negativeNumber = true;
                }
                Assert(view.ptr[0]!='-');// ensure that the tokenizer hasn't been changed
                // to clump the - together with the number token
                
                bool printedError = false;
                u64 num=0;
                for(int i=0;i<view.len;i++){
                    char c = view.ptr[i];
                    if(num * 10 < num && !printedError) {
                        ERR_SECTION(
                            ERR_HEAD2(token_tiny)
                            // TODO: Show the limit? pow(2,64)-1
                            ERR_MSG("Number overflow! '"<<info.lexer->tostring(token_tiny)<<"' is to large for 64-bit integers!")
                            ERR_LINE2(token_tiny,"to large!");
                        )
                        printedError = true;
                    }
                    num = num*10 + (c-'0');
                }
                bool unsignedSuffix = false;
                bool signedSuffix = false;
                auto tok_suffix = info.gettok();
                if((token->flags&(lexer::TOKEN_FLAG_ANY_SUFFIX))==0) {
                    if(tok_suffix.type == 'u') {
                        info.advance();
                        unsignedSuffix  = true;
                    } else if(tok_suffix.type == 's') {
                        info.advance();
                        signedSuffix  = true;
                    } else if(tok_suffix.type == lexer::TOKEN_IDENTIFIER || (tok_suffix.type|32) >= 'a' && (tok_suffix.type|32) <= 'z'){
                        ERR_SECTION(
                            ERR_HEAD2(tok_suffix)
                            ERR_MSG("'"<<info.lexer->tostring(tok_suffix)<<"' is not a known suffix for integers. The available ones are: 92u (unsigned), 31924s (signed).")
                            ERR_LINE2(tok_suffix,"invalid suffix")
                        )
                    }
                }
                ASTExpression* tmp = 0;
                if(unsignedSuffix) {
                    if(negativeNumber) {
                        ERR_SECTION(
                            ERR_HEAD2(tok_suffix)
                            ERR_MSG("You cannot use an unsigned suffix and a minus at the same time. They collide.")
                            ERR_LINE2(tok_suffix,"remove?")
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
                                ERR_HEAD2(token_tiny)
                                // TODO: Show the limit? pow(2,63)-1 or -pow(2,63) if negative
                                ERR_MSG("'"<<info.lexer->tostring(token_tiny)<<"' is to large for signed 64-bit integers! Larger integers are not supported. Consider using an unsigned 64-bit integer.")
                                ERR_LINE2(token_tiny,"to large");
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
                //             ERR_HEAD2(tok)
                //             ERR_MSG("You cannot use an unsigned suffix and a minus at the same time. They collide.")
                //             ERR_LINE2(tok,"remove?")
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
                        // ERR_HEAD2(token,"Number overflow! '"<<token<<"' is to large for 64-bit integers!\n\n";
                //                 ERR_LINE2(token.tokenIndex,"to large!");
                //             )
                //         }
                //     }
                // }else{
                //     // we default to signed but we use unsigned if the number doesn't fit
                //     if(unsignedSuffix && (num&0x8000000000000000)!=0) {
                //         if(signedSuffix) {
                //             ERR_SECTION(
                //                 ERR_HEAD2(tok)
                //                 ERR_MSG("You cannot use an unsigned suffix and a minus at the same time. They collide.")
                //                 ERR_LINE2(tok,"remove?")
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
                // tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                // tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(token->type == lexer::TOKEN_LITERAL_DECIMAL){
                auto tok_suffix = info.gettok();
                ASTExpression* tmp = nullptr;
                Assert(view.ptr[0]!='-');// ensure that the tokenizer hasn't been changed
                // to clump the - together with the number token
                if(token->flags&(lexer::TOKEN_FLAG_ANY_SUFFIX)==0 && tok_suffix.type == 'd') {
                    info.advance(2);
                    tmp = info.ast->createExpression(TypeId(AST_FLOAT64));
                    tmp->f64Value = lexer::ConvertDecimal(view);
                    if (ops.size()>0 && ops.last() == AST_UNARY_SUB){
                        ops.pop();
                        tmp->f64Value = -tmp->f64Value;
                    }
                } else {
                    info.advance();
                    tmp = info.ast->createExpression(TypeId(AST_FLOAT32));
                    tmp->f32Value = lexer::ConvertDecimal(view);
                    if (ops.size()>0 && ops.last() == AST_UNARY_SUB){
                        ops.pop();
                        tmp->f32Value = -tmp->f32Value;
                    }
                }

                values.add(tmp);
                tmp->constantValue = true;
                // tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                // tmp->tokenRange.endIndex = info.at()+1;
                
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(token->type == lexer::TOKEN_LITERAL_HEXIDECIMAL){
                auto token_tiny = info.gettok();
                info.advance();
                // if (ops.size()>0 && ops.last() == AST_UNARY_SUB){
                //     ops.pop();
                //     ERR_SECTION(
                //         ERR_HEAD2(token)
                //         ERR_MSG("Negative hexidecimals is not okay.")
                //         ERR_LINE2(token,"bad");
                //     )
                // }
                // 0x000000001 will be treated as 64 bit value and
                // it probably should because you added those 
                // zero for that exact reason.
                ASTExpression* tmp = nullptr;
                int hex_prefix = 0;
                if(view.len>=2 && *(u16*)view.ptr == '0x')
                    hex_prefix = 2;
                if(view.len-hex_prefix<=8) {
                    tmp = info.ast->createExpression(TypeId(AST_UINT32));
                } else if(view.len-hex_prefix<=16) {
                    tmp = info.ast->createExpression(TypeId(AST_UINT64));
                } else {
                    tmp = info.ast->createExpression(TypeId(AST_UINT64));
                    ERR_SECTION(
                        ERR_HEAD2(token_tiny)
                        ERR_MSG("Hexidecimal overflow! '"<<info.lexer->tostring(token_tiny)<<"' is to large for 64-bit integers!")
                        ERR_LINE2(token_tiny,"to large!");
                    )
                }
                tmp->i64Value =  lexer::ConvertHexadecimal(view);
                values.add(tmp);
                tmp->constantValue = true;
                // tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                // tmp->tokenRange.endIndex = info.at()+1;
                
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(token->type == lexer::TOKEN_LITERAL_STRING) {
                info.advance();
                
                ASTExpression* tmp = 0;
                // if(token.length == 1){
                if((token->flags&lexer::TOKEN_FLAG_SINGLE_QUOTED)){
                    tmp = info.ast->createExpression(TypeId(AST_CHAR));
                    tmp->charValue = *view.ptr;
                    tmp->constantValue = true;
                }else if((token->flags&lexer::TOKEN_FLAG_DOUBLE_QUOTED)) {
                    tmp = info.ast->createExpression(TypeId(AST_STRING));
                    tmp->name = view;
                    tmp->constantValue = true;
                    if(cstring)
                        tmp->flags |= ASTNode::NULL_TERMINATED;

                    // A string of zero size can be useful which is why
                    // the code below is commented out.
                    // if(token.length == 0){
                        // ERR_SECTION(
                        // ERR_HEAD2(token, "string should not have a length of 0\n";
                        // This is a semantic error and since the syntax is correct
                        // we don't need to return PARSE_ERROR. We could but things will be fine.
                    // }
                }
                values.add(tmp);
                // tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                // tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(token->type == lexer::TOKEN_TRUE || token->type == lexer::TOKEN_FALSE){
                info.advance();
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_BOOL));
                tmp->boolValue = token->type == lexer::TOKEN_TRUE;
                tmp->constantValue = true;
                values.add(tmp);
                // tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                // tmp->tokenRange.endIndex = info.at()+1;
                
                // tmp->tokenRange.tokenStream = info.tokens;
            } 
            // else if(Equal(token,"[")) {
            //     info.advance();

            //     ASTExpression* tmp = info.ast->createExpression(TypeId(AST_SLICE_INITIALIZER));

            //     // TODO: what about slice type. u16[] for example.
            //     //  you can do just [0,21] and it will auto assign a type in the generator
            //     //  but we may want to specify one.
 
            // WHILE_TRUE {
            //         Token token = info.gettok();
            //         if(Equal(token,"]")){
            //             info.advance();
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
                    
            //         token = info.gettok();
            //         if(token.type == ','){
            //             info.advance();
            //             continue;
            //         }else if(Equal(token,"]")){
            //             info.advance();
            //             break;
            //         }else {
            //             ERR_SECTION(
                    // ERR_HEAD2(token, "expected ] not "<<token<<"\n";
            //             continue;
            //         }
            //     }
                
            //     values.add(tmp);
            //     tmp->tokenRange.firstToken = token;
            //     tmp->tokenRange.startIndex = info.at();
            //     tmp->tokenRange.endIndex = info.at()+1;
            //     tmp->tokenRange.tokenStream = info.tokens;
            // } 
            else if(token->type == lexer::TOKEN_NULL){
                info.advance();
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_NULL));
                values.add(tmp);
                tmp->constantValue = true;
                // tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                // tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(token->type == lexer::TOKEN_SIZEOF || token->type == lexer::TOKEN_NAMEOF || token->type == lexer::TOKEN_TYPEID) {
                info.advance();
                ASTExpression* tmp = nullptr;
                if(token->type == lexer::TOKEN_SIZEOF)
                    tmp = info.ast->createExpression(TypeId(AST_SIZEOF));
                if(token->type == lexer::TOKEN_NAMEOF)
                    tmp = info.ast->createExpression(TypeId(AST_NAMEOF));
                if(token->type == lexer::TOKEN_TYPEID)
                    tmp = info.ast->createExpression(TypeId(AST_TYPEID));

                // Token token = {}; 
                // int result = ParseTypeId(info,token);
                // tmp->name = token;

                bool hasParentheses = false;
                auto tok = info.gettok();
                if(tok.type == '(') {
                    // NOTE: We need to handle parentheses here because otherwise
                    //  typeid(Apple).ptr_level would be seen as typeid( (Apple).ptr_level )
                    info.advance();
                    hasParentheses=true;
                }

                ASTExpression* left=nullptr;
                auto signal = ParseExpression(info, left);
                tmp->left = left;
                tmp->constantValue = true;

                if(hasParentheses) {
                    auto tok = info.gettok();
                    if(tok.type == ')') {
                        info.advance();
                    } else {
                        ERR_SECTION(
                            ERR_HEAD2(tok)
                            ERR_MSG("Missing closing parentheses.")
                            ERR_LINE2(tok,"opening parentheses")
                        )
                    }
                }
                
                // tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.endIndex = info.at()+1;
                values.add(tmp);
            }  else if(token->type == lexer::TOKEN_ASM) {
                auto token_tiny = info.gettok();
                info.advance();

                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_ASM));
                // tmp->tokenRange.firstToken = token;

                // TODO: asm<i32>{...} casting

                auto curly = info.gettok();
                if(curly.type != '{') {
                    ERR_SECTION(
                        ERR_HEAD2(curly)
                        ERR_MSG("'asm' keyword (inline assembly) requires a body with curly braces")
                        ERR_LINE2(curly,"should be }")
                    )
                    continue;
                }
                info.advance();
                int depth = 1;
                //  token{};
                auto firstToken = info.gettok();
                // TokenStream* stream = firstToken.tokenStream;
                while(true){
                    auto token = info.gettok();
                    if(token.type == lexer::TOKEN_EOF){
                        ERR_SUDDEN_EOF(token_tiny, "Missing ending curly brace for inline assembly.", "asm block starts here")
                        return SIGNAL_COMPLETE_FAILURE;
                    }
                    if(token.type == '{') {
                        depth++;
                        continue;
                    }
                    if(token.type == '}') {
                        depth--;
                        if(depth==0) {
                            info.advance();
                            break;
                        }
                    }
                    // if(token.tokenStream != stream) {
                    //     ERR_SECTION(
                    //         ERR_HEAD2(token)
                    //         ERR_MSG("Tokens in inlined assembly must come frome the same source file due to implementation restrictions. (spam the developer to fix this. Oh wait, that's me)")
                    //         ERR_LINE2(firstToken,"first file")
                    //         ERR_LINE2(token,"second file")
                    //     )
                    // }
                    // Inline assembly is parsed here. The content of the assembly can be known
                    // with tokenRange in ASTNode. We know the range of tokens for the inline assembly.
                    // If you used a macro from another stream then we have problems.
                    // TODO: We want to search for instructions where variables are used like this
                    //   mov eax, [var] and change the instruction to one that gets the variable.
                    //   Can't do that here though since we need to know the variables first.
                    //   It must be done in type checker.
                    info.advance();
                }

                // nocheckin, we must set the range of the asm block
                Assert(("asm incomplete after rewrite V2.1",false));

                // tmp->tokenRange.endIndex = info.at()+1;
                values.add(tmp);
            } else if(token->type == lexer::TOKEN_IDENTIFIER){
                info.advance();
                int startToken=info.gethead();
                
                std::string pure_name = view;
                // could be a slice if tok[]{}
                // if something is within [] then it's a array access
                // polytypes exist for struct initializer and function calls
                auto tok = info.gettok();
                auto poly_tok = info.gettok();
                std::string polyTypes="";
                // TODO: func<i32>() and i < 5 has ambiguity when
                //  parsing. Currently using space to differentiate them.
                //  Checking for <, > and ( for functions could work but
                //  i < 5 may have something similar in some cases.
                //  Checking for that could take a long time since
                //  it could be hard to know when to stop.
                //  ParseTypeId has some logic for it so things are possible
                //  it's just hard.
                if(tok.type == '<' && !(token->flags&lexer::TOKEN_FLAG_SPACE)){
                    auto signal = ParseTypeId(info, polyTypes);
                    SIGNAL_INVALID_DATATYPE(polyTypes)
                    // switch(signal){
                    // case SIGNAL_SUCCESS: break;
                    // case SIGNAL_COMPLETE_FAILURE: return SIGNAL_COMPLETE_FAILURE;
                    // default: Assert(false);
                    // }
                    tok = info.gettok();
                }

                // NOTE: I removed code that parsed poly types and replaced it with ParseTypeId.
                //   The code was scuffed anyway so even if it broke things you should rewrite it.
                
                if(tok.type == '('){
                    // function call
                    info.advance();
                    
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
                    ns += pure_name;
                    ns += polyTypes;

                    ASTExpression* tmp = info.ast->createExpression(TypeId(AST_FNCALL));
                    // tmp->name = pure_name + polyTypes;
                    tmp->name = ns;

                    int count = 0;
                    auto signal = ParseArguments(info, tmp, &count);
                    switch(signal){
                        case SIGNAL_SUCCESS: break;
                        case SIGNAL_COMPLETE_FAILURE: {
                            info.ast->destroy(tmp);
                            return SIGNAL_COMPLETE_FAILURE;
                        }
                        default: Assert(false);
                    }

                    _PLOG(log::out << "Parsed call "<<count <<"\n";)
                    values.add(tmp);
                    // tmp->tokenRange.firstToken = token;
                    // tmp->tokenRange.startIndex = startToken;
                    // tmp->tokenRange.endIndex = info.at()+1;
                    // tmp->tokenRange.tokenStream = info.tokens;
                } else if(tok.type=='{' && 0 == (token->flags & lexer::TOKEN_FLAG_ANY_SUFFIX)){
                    // initializer
                    info.advance();
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
                    ns += pure_name;
                    ns += polyTypes;
                    std::string* nsp = info.ast->createString();
                    *nsp = ns;
                    initExpr->castType = info.ast->getTypeString(*nsp);
                    // TODO: A little odd to use castType. Renaming castType to something more
                    //  generic which works for AST_CAST and AST_INITIALIZER would be better.
                    // ASTExpression* tail=0;
                    
                    bool mustBeNamed=false;
                    lexer::Token prevNamed = {};
                    int count=0;
                    WHILE_TRUE_N(10000) {
                        auto token = info.getinfo(&view);
                        auto tok = info.gettok();
                        if(token->type == '}'){
                           info.advance();
                           break;
                        }

                        bool named=false;
                        auto tok_eq = info.gettok(1);
                        if(token->type == lexer::TOKEN_IDENTIFIER && tok_eq.type == '='){
                            info.advance(2);
                            named = true;
                            prevNamed = tok;
                            mustBeNamed = true;
                        } else if(mustBeNamed){
                            ERR_SECTION(
                                ERR_HEAD2(tok)
                                ERR_MSG("Arguments after a named argument must also be named in a struct initializer.")
                                ERR_LINE2(tok,"this argument should be named..")
                                ERR_LINE2(prevNamed,"...because of this")
                            )
                            // return or continue could desync the parsing so don't do that.
                        }

                        ASTExpression* expr=0;
                        auto signal = ParseExpression(info,expr);
                        switch(signal) {
                        case SIGNAL_SUCCESS: break;
                        case SIGNAL_COMPLETE_FAILURE: return SIGNAL_COMPLETE_FAILURE;
                        default: Assert(false);
                        }
                        if(named){
                            expr->namedValue = view;
                        }
                        initExpr->args.add(expr);
                        count++;
                        
                        tok = info.gettok();
                        if(token->type == ','){
                            info.advance();
                            continue;
                        }else if(token->type == '}'){
                           info.advance();
                           break;
                        } else {
                            ERR_SECTION(
                                ERR_HEAD2(tok)
                                ERR_MSG("Expected , or } in initializer list not '"<<info.lexer->tostring(tok)<<"'.")
                                ERR_LINE2(tok,"bad")
                            )
                            continue;
                        }
                    }
                    // log::out << "Parse initializer "<<count<<"\n";
                    values.add(initExpr);
                    // initExpr->tokenRange.firstToken = token;
                    // initExpr->tokenRange.startIndex = startToken;
                    // initExpr->tokenRange.endIndex = info.at()+1;
                    // initExpr->tokenRange.tokenStream = info.tokens;
                } else {
                    // if(polyTypes.size()!=0){
                    //     This is possible since types are can be parsed in an expression.
                    //     ERR_SECTION(
                    // ERR_HEAD2(token, "Polymorphic types not expected with namespace or variable.\n\n";
                    //         ERR_LINE2(token.tokenIndex,"bad");
                    //     )
                    //     continue;
                    // }
                    if(tok.type == lexer::TOKEN_NAMESPACE_DELIM){
                        info.advance();
                        
                        // Token tok = info.gettok();
                        // if(!IsName(tok)){
                        //     ERR_SECTION(
                        // ERR_HEAD2(tok, "'"<<tok<<"' is not a name.\n\n";
                        //         ERR_LINE2(tok.tokenIndex,"bad");
                        //     )
                        //     continue;
                        // }
                        ops.add(AST_FROM_NAMESPACE);
                        namespaceNames.add(view);
                        continue; // do a second round here?
                        // TODO: detect more ::
                    } else {
                        ASTExpression* tmp = info.ast->createExpression(TypeId(AST_ID));

                        std::string nsToken = "";
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
                        for(int i=nsOps-1;i>=0;i--){
                            auto tok = namespaceNames[namespaceNames.size() - i - 1];
                            nsToken += tok;
                            nsToken += "::";
                            namespaceNames.pop();
                        }
                        nsToken += pure_name;

                        if(polyTypes.size() != 0) {
                            ERR_SECTION(
                                ERR_HEAD2(poly_tok)
                                ERR_MSG("Why have you done 'id<i32>' in an expression? Don't do that.")
                                ERR_LINE2(poly_tok,"bad programmer")
                            )
                        }

                        tmp->name = nsToken;

                        values.add(tmp);
                        // tmp->tokenRange.firstToken = nsToken;
                        // tmp->tokenRange.endIndex = info.at()+1;
                    }
                }
            } else if(token->type == '('){
                auto token_tiny = info.gettok();
                // parse again
                info.advance();
                ASTExpression* tmp=0;
                auto signal = ParseExpression(info,tmp);
                switch(signal) {
                case SIGNAL_SUCCESS: break;
                case SIGNAL_COMPLETE_FAILURE: return signal;
                default: Assert(false);
                }
                
                if(!tmp){
                    ERR_SECTION(
                        ERR_HEAD2(token_tiny)
                        ERR_MSG("Got nothing from parenthesis in expression.")
                        ERR_LINE2(token_tiny,"bad")
                    )
                }
                auto tok = info.gettok();
                if(tok.type != ')'){
                    ERR_SECTION(
                        ERR_HEAD2(tok)
                        ERR_MSG("Expected ) not "<<info.lexer->tostring(tok)<<".")
                        ERR_LINE2(tok,"bad")
                    )
                }else
                    info.advance();
                values.add(tmp);  
            } else {
                auto token_tiny = info.gettok();
                // if(attempt){
                //     return SignalAttempt::BAD_ATTEMPT;
                // }else{
                    ERR_SECTION(
                        ERR_HEAD2(token_tiny)
                        ERR_MSG("'"<<info.lexer->tostring(token_tiny) << "' is not a value. Values (or expressions) are expected after tokens that calls upon arguments, operations and assignments among other things.")
                        ERR_LINE2(token_tiny,"should be a value")
                        // ERR_LINE2(info.gettok(tokenIndex-1),"expects a value afterwards")
                    )
                    // ERR_LINE2()
                    // printLine();
                    return SIGNAL_COMPLETE_FAILURE;
                    // Qutting here is okay because we have a defer which
                    // destroys parsed expressions. No memory leaks!
                // }
            }
        }
        // if(negativeNumber){
        //     ERR_SECTION(
            // ERR_HEAD2(info.get(info.at()-1), "Unexpected - before "<<token<<"\n";
        //         ERR_LINE2(info.at()-1,"bad");
        //     )
        //     return SignalAttempt::FAILURE;
        //     // quitting here is a little unexpected but there is
        //     // a defer which destroys parsed expresions so no memory leaks at least.
        // }
        
        ending = ending || info.gettok().type == lexer::TOKEN_EOF;

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
                auto bad = info.gettok(-1);
                expectOperator = false; // prevent duplicate messages
                ERR_SECTION(
                    ERR_HEAD2(bad)
                    ERR_MSG("Operation '"<<info.lexer->tostring(bad)<<"' needs a left and right expression but the right is missing.")
                    ERR_LINE2(bad, "Missing expression to the right")
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
                    auto tok = namespaceNames.last();
                    val->name = tok;
                    val->tokenRange.firstToken = tok;
                    // val->tokenRange.startIndex -= 2; // -{namespace name} -{::}
                    namespaceNames.pop();
                } else if(nowOp == AST_CAST){
                    val->castType = castTypes.last();
                    castTypes.pop();
                    auto extraToken = extraTokens.last();
                    extraTokens.pop();
                    auto str = info.lexer->getStdStringFromToken(extraToken);
                    if(str == "cast_unsafe") {
                        val->setUnsafeCast(true);
                    }
                    // val->tokenRange.firstToken = extraToken;
                    // val->tokenRange.endIndex = er->tokenRange.firstToken.tokenIndex;
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
        // attempt = false;
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
            return SIGNAL_SUCCESS;
        }
    }
    Assert(false);
    // shouldn't happen
    return SIGNAL_COMPLETE_FAILURE;
}
#ifdef gone
SignalIO ParseFlow(ParseInfo& info, ASTStatement*& statement){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _PLOG(FUNC_ENTER)
    
    if(info.end()){
        return SignalAttempt::FAILURE;
    }
    Token firstToken = info.gettok();
    int startIndex = info.at()+1;
    if(Equal(firstToken,"if")){
        info.advance();
        ASTExpression* expr=nullptr;
        SignalAttempt result = ParseExpression(info,expr,false);
        if(result!=SignalAttempt::SIGNAL_SUCCESS){
            // TODO: should more stuff be done here?
            return SignalAttempt::FAILURE;
        }

        int endIndex = info.at()+1;

        ASTScope* body=nullptr;
        ParseFlags parsed_flags = PARSE_NO_FLAGS;
        SignalDefault resultBody = ParseBody(info,body, info.currentScopeId, PARSE_NO_FLAGS, &parsed_flags);
        if(resultBody!=SignalDefault::SIGNAL_SUCCESS){
            return SignalAttempt::FAILURE;
        }

        ASTScope* elseBody=nullptr;
        Token token = info.gettok();
        if(Equal(token,"else")){
            info.advance();
            resultBody = ParseBody(info,elseBody, info.currentScopeId);
            if(resultBody!=SignalDefault::SIGNAL_SUCCESS){
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
                    ERR_HEAD2(body->statements[0]->tokenRange, ERROR_AMBIGUOUS_IF_ELSE)
                    ERR_MSG("Nested if statements without curly braces may be ambiguous with else statements and is therefore not allowed. Use curly braces on the \"highest\" if statement.")
                    ERR_LINE2(statement->tokenRange, "use curly braces here")
                    ERR_LINE2(body->statements[0]->tokenRange, "ambiguous when using else")
                )
            }
        }
        
        return SignalAttempt::SIGNAL_SUCCESS;
    }else if(Equal(firstToken,"while")){
        info.advance();

        ASTExpression* expr=nullptr;
        if(Equal(info.gettok(), "{")) {
            // no expression, infinite loop
        } else {
            SignalAttempt result = ParseExpression(info,expr,false);
            if(result!=SignalAttempt::SIGNAL_SUCCESS){
                // TODO: should more stuff be done here?
                return SignalAttempt::FAILURE;
            }
        }

        int endIndex = info.at()+1;

        info.functionScopes.last().loopScopes.add({});
        ASTScope* body=0;
        SignalDefault resultBody = ParseBody(info,body, info.currentScopeId);
        info.functionScopes.last().loopScopes.pop();
        if(resultBody!=SignalDefault::SIGNAL_SUCCESS){
            return SignalAttempt::FAILURE;
        }

        statement = info.ast->createStatement(ASTStatement::WHILE);
        statement->firstExpression = expr;
        statement->firstBody = body;
        statement->tokenRange.firstToken = firstToken;
        
        statement->tokenRange.endIndex = endIndex;
        
        return SignalAttempt::SIGNAL_SUCCESS;
    } else if(Equal(firstToken,"for")){
        info.advance();
        
        Token token = info.gettok();
        bool reverseAnnotation = false;
        bool pointerAnnot = false;
        while (IsAnnotation(token)){
            info.advance();
            if(Equal(token,"@reverse") || Equal(token,"@rev")){
                reverseAnnotation=true;
            } else if(Equal(token,"@pointer") || Equal(token,"@ptr")){
                pointerAnnot=true;
            } else {
                WARN_SECTION(
                    WARN_HEAD(token)
                    WARN_MSG("'"<< Token(token.str+1,token.length-1) << "' is not a known annotation for functions.")
                    WARN_LINE(info.get(info.at()),"unknown")
                )
            }
            token = info.gettok();
            continue;
        }

        // NOTE: assuming array iteration
        Token varname = info.gettok();
        Token colon = info.get(info.at()+2);
        if(Equal(colon,":") && IsName(varname)){
            info.advance();
            info.advance();
        } else {
            std::string* str = info.ast->createString();
            *str = "it";
            varname = *str;
        }
        
        ASTExpression* expr=0;
        SignalAttempt result = ParseExpression(info,expr,false);
        if(result!=SignalAttempt::SIGNAL_SUCCESS){
            // TODO: should more stuff be done here?
            return SignalAttempt::FAILURE;
        }
        ASTScope* body=0;
        info.functionScopes.last().loopScopes.add({});
        SignalDefault resultBody = ParseBody(info,body, info.currentScopeId);
        info.functionScopes.last().loopScopes.pop();
        if(resultBody!=SignalDefault::SIGNAL_SUCCESS){
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
        
        return SignalAttempt::SIGNAL_SUCCESS;
    } else if(Equal(firstToken,"return")){
        info.advance();
        
        statement = info.ast->createStatement(ASTStatement::RETURN);
        
        if((firstToken.flags & lexer::TOKEN_FLAG_NEWLINE) == 0) {
            WHILE_TRUE {
                ASTExpression* expr=0;
                Token token = info.gettok();
                if(Equal(token,";")&&statement->arrayValues.size()==0){ // return 1,; should not be allowed, that's why we do &&!base
                    info.advance();
                    break;
                }
                SignalAttempt result = ParseExpression(info,expr,true);
                if(result!=SignalAttempt::SIGNAL_SUCCESS){
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
                
                Token tok = info.gettok();
                if(!tok.type == ','){
                    if(tok.type == '{')
                        info.advance();
                    break;   
                }
                info.advance();
            }
        }
        
        // statement->rvalue = base;
        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.endIndex = info.at()+1;
        return SignalAttempt::SIGNAL_SUCCESS;
    } else if(Equal(firstToken,"switch")){
        info.advance();
        ASTExpression* switchExpression=0;
        SignalAttempt result = ParseExpression(info,switchExpression,false);
        if(result!=SignalAttempt::SIGNAL_SUCCESS){
            // TODO: should more stuff be done here?
            return SignalAttempt::FAILURE;
        }
        
        Token tok = info.gettok();
        if(!tok.type == '{') {
            return SignalAttempt::FAILURE;
        }
        info.advance(); // {
        
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
            tok = info.gettok();
            if(Equal(tok,"}")) {
                info.advance();
                break;   
            }
            if(tok.type == '{') {
                info.advance();
                continue;
            }
            
            if(IsAnnotation(tok)) {
                if(Equal(tok,"@TEST_ERROR")) {
                    info.ignoreErrors = true;
                    if(mayRevertError)
                        info.errors--;
                    statement->setNoCode(true);
                    info.advance();
                    
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
                    ERR_HEAD2(tok,ERROR_C_STYLED_DEFAULT_CASE)
                    ERR_MSG("Write 'case: { ... }' to specify default case. 'default' is the C/C++ way.")
                    ERR_LINE2(tok,"bad")
                )
                // info.advance();
                // return SignalAttempt::FAILURE;
            } else if(!Equal(tok,"case")) {
                mayRevertError = info.ignoreErrors == 0;
                ERR_SECTION(
                    ERR_HEAD2(tok,ERROR_BAD_TOKEN_IN_SWITCH)
                    ERR_MSG("'"<<tok<<"' is not allowed in switch statement where 'case' is supposed to be.")
                    ERR_LINE2(tok,"bad")
                )
                info.advance();
                continue;
                // return SignalAttempt::FAILURE;
            }
            
            info.advance(); // case
            
            bool defaultCase = false;
            Token colonTok = info.gettok();
            if(Equal(colonTok,":")) {
                if(defaultCaseToken.str && !badDefault){
                    mayRevertError = info.ignoreErrors == 0;
                    ERR_SECTION(
                        ERR_HEAD2(tok,ERROR_DUPLICATE_DEFAULT_CASE)
                        ERR_MSG("You cannot have two default cases in a switch statement.")
                        ERR_LINE2(defaultCaseToken,"previous")
                        ERR_LINE2(tok,"first default case")
                    )
                } else {
                    defaultCaseToken = tok;
                }
                
                defaultCase = true;
                info.advance();
                
                // ERR_SECTION(
                //     ERR_HEAD2(tok)
                //     ERR_MSG("Default case has not been implemented yet.")
                //     ERR_LINE2(tok,"bad")
                // )
                // return SignalAttempt::FAILURE;
            }
            
            ASTExpression* caseExpression=nullptr;
            if(!defaultCase){
                SignalAttempt result = ParseExpression(info,caseExpression,false);
                if(result!=SignalAttempt::SIGNAL_SUCCESS){
                    // TODO: should more stuff be done here?
                    return SignalAttempt::FAILURE;
                }
                colonTok = info.gettok();
                if(!Equal(colonTok,":")) {
                    // info.advance();
                    // continue;
                    return SignalAttempt::FAILURE;
                }
                info.advance(); // :
            }
            
            ParseFlags parsed_flags = PARSE_NO_FLAGS;
            ASTScope* caseBody=nullptr;
            SignalDefault resultBody = ParseBody(info,caseBody, info.currentScopeId, PARSE_INSIDE_SWITCH, &parsed_flags);
            if(resultBody!=SignalDefault::SIGNAL_SUCCESS){
                return SignalAttempt::FAILURE;
            }
            
            Token& token2 = info.gettok();
            if(IsAnnotation(token2)) {
                if(Equal(token2,"@TEST_ERROR")) {
                    info.ignoreErrors = true;
                    if(mayRevertError)
                        info.errors--;
                    statement->setNoCode(true);
                    info.advance();
                    
                    auto res = ParseAnnotationArguments(info, nullptr);
                } else {
                    // TODO: ERR annotation not supported   
                }
                //  else if(Equal(token2, "@no_code")) {
                //     noCode = true;
                //     info.advance();
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
        
        return SignalAttempt::SIGNAL_SUCCESS;
    } else  if(Equal(firstToken,"using")){
        info.advance();

        // variable name
        // namespacing
        // type with polymorphism. not pointers
        // TODO: namespace environemnt, not just using X as Y

        Token originToken={};
        SignalDefault result = ParseTypeId(info, originToken, nullptr);
        Assert(result == SignalDefault::SIGNAL_SUCCESS);
        Token aliasToken = {};
        Token token = info.gettok();
        if(Equal(token,"as")) {
            info.advance();
            
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
        return SignalAttempt::SIGNAL_SUCCESS;        
    } else if(Equal(firstToken,"defer")){
        info.advance();

        ASTScope* body = nullptr;
        SignalDefault result = ParseBody(info, body, info.currentScopeId);
        if(result != SignalDefault::SIGNAL_SUCCESS) {
            return SignalAttempt::FAILURE;
        }

        statement = info.ast->createStatement(ASTStatement::DEFER);
        statement->firstBody = body;

        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.endIndex = info.at()+1;
        return SignalAttempt::SIGNAL_SUCCESS;  
    } else if(Equal(firstToken, "break")) {
        info.advance();

        // TODO: We must be in loop scope!

        statement = info.ast->createStatement(ASTStatement::BREAK);

        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.endIndex = info.at()+1;
        return SignalAttempt::SIGNAL_SUCCESS;
    } else if(Equal(firstToken, "continue")) {
        info.advance();

        // TODO: We must be in loop scope!

        statement = info.ast->createStatement(ASTStatement::CONTINUE);

        statement->tokenRange.firstToken = firstToken;
        
        statement->tokenRange.endIndex = info.at()+1;
        
        return SignalAttempt::SIGNAL_SUCCESS;  
    // } else if(!(firstToken.flags&TOKEN_MASK_QUOTED) && firstToken.length == 5 && !strncmp(firstToken.str, "test", 4) && firstToken.str[4]>='0' && firstToken.str[4] <= '9') {
    } else if(Equal(firstToken,"test")) {
        info.advance();
        
        // test4 {type} {value} {expression}
        // test1 {value} {expression}

        // integer/float or string

        // int byteCount = firstToken.str[4]-'0';
        // if(byteCount!=1 || byteCount!=2||byteCount!=4||byteCount!=8) {
        //     ERR_SECTION(
        //         ERR_HEAD2(firstToken)
        //         ERR_MSG("Test statement uses these keyword: test1, test2, test4, test8. The numbers describe the bytes to test. '"<<byteCount<<"' is not one of them.")
        //         ERR_LINE2(firstToken, byteCount + " is not allowed")
        //     )
        // } else {
        //     statement->bytesToTest = byteCount;
        // }

        // Token valueToken = info.gettok();
        // info.advance();

        statement = info.ast->createStatement(ASTStatement::TEST);

        // if(IsInteger(valueToken) || IsDecimal(valueToken) || IsHexadecimal(valueToken) || (valueToken.flags&TOKEN_MASK_QUOTED)) {
        //     statement->testValueToken = valueToken;
        // } else {
        //     ERR_SECTION(
        //         ERR_HEAD2(valueToken)
        //         ERR_MSG("Test statement requires a value after the keyword such as an integer, decimal, hexidecimal or string.")
        //         ERR_LINE2(valueToken, "not a valid value")
        //     )
        // }

        SignalAttempt result = ParseExpression(info, statement->testValue, false);
        
        if(Equal(info.gettok(),";")) info.advance();
        
        result = ParseExpression(info, statement->firstExpression, false);

        statement->tokenRange.firstToken = firstToken;
        statement->tokenRange.endIndex = info.at()+1;

        if(Equal(info.gettok(),";")) info.advance();
        
        return SignalAttempt::SIGNAL_SUCCESS;  
    }
    if(attempt)
        return SignalAttempt::BAD_ATTEMPT;
    return SignalAttempt::FAILURE;
}
SignalIO ParseOperator(ParseInfo& info, ASTFunction*& function) {
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _PLOG(FUNC_ENTER)
    Token beginToken = info.gettok();
    // int startIndex = info.at()+1;
    if(!Equal(beginToken,"operator")){
        if(attempt) return SignalAttempt::BAD_ATTEMPT;
        ERR_SECTION(
            ERR_HEAD2(beginToken)
            ERR_MSG("Expected 'operator' for operator overloading not '"<<beginToken<<"'.")
        )
            
        return SignalAttempt::FAILURE;
    }
    info.advance();
    attempt = false;

    function = info.ast->createFunction();
    function->tokenRange.firstToken = beginToken;
    function->tokenRange.endIndex = info.at()+1;;

    Token name = info.advance();
    while (IsAnnotation(name)){
        if(Equal(name,"@hide")){
            function->setHidden(true);
        } else {
            // It should not warn you because it is quite important that you use the right annotations with functions
            // Mispelling external or the calling convention would be very bad.
            ERR_SECTION(
                ERR_HEAD2(name)
                ERR_MSG("'"<< Token(name.str+1,name.length-1) << "' is not a known annotation for operators.")
                ERR_LINE2(info.get(info.at()),"unknown")
            )
        }
        name = info.advance();
        continue;
    }

    OperationType op = (OperationType)0;
    int extraNext = 0;
    
    // TODO: Improve parsing here. If the token is an operator but not allowed
    //  then you can continue parsing, it's just that the function won't be
    //  added to the tree and be usable. Parse and then throw away.
    //  Stuff following the operator overload´syntax will then parse fine.
    
    Token name2 = info.gettok();
    // What about +=, -=?
    if(Equal(name, "[") && Equal(name2, "]")) {
        info.advance();// ]
        name.length++;
    } else if(!(op = parser::IsOp(name, extraNext))){
        info.ast->destroy(function);
        function = nullptr;
        ERR_SECTION(
            ERR_HEAD2(name)
            ERR_MSG("Expected a valid operator, "<<name<<" is not.")
        )
        return SignalAttempt::FAILURE;
    }
    // info.advance(); // next is done above
    while(extraNext--){
        Token t = info.advance();
        name.length += t.length;
        // used for doing next on the extra arrows for bit shifts
    }
    Assert(info.gettok() != "=");

    function->name = name;

    ScopeInfo* funcScope = info.ast->createScope(info.currentScopeId, info.getNextOrder(), nullptr);
    function->scopeId = funcScope->id;

    // ensure we leave this parse function with the same scope we entered with
    auto prevScope = info.currentScopeId;
    defer { info.currentScopeId = prevScope; };

    info.currentScopeId = function->scopeId;

    Token tok = info.gettok();
    if(tok.type == '<'){
        info.advance();
        while(!info.end()){
            tok = info.gettok();
            if(tok.type == '>'){
                info.advance();
                break;
            }
            SignalDefault result = ParseTypeId(info, tok, nullptr);
            if(result == SignalDefault::SIGNAL_SUCCESS) {
                function->polyArgs.add({tok});
            }

            tok = info.gettok();
            if (tok.type == ',') {
                info.advance();
                continue;
            } else if(tok.type == '>') {
                info.advance();
                break;
            } else {
                ERR_SECTION(
                    ERR_HEAD2(tok)
                    ERR_MSG("Expected , or > for in poly. arguments for operator "<<name<<".")
                )
                // parse error or what?
                break;
            }
        }
    }
    tok = info.gettok();
    if(!Equal(tok,"(")){
        ERR_SECTION(
            ERR_HEAD2(tok)
            ERR_MSG("Expected ( not "<<tok<<".")
        )
        return SignalAttempt::FAILURE;
    }
    info.advance();

    bool printedErrors=false;
    TokenRange prevDefault={};
    WHILE_TRUE {
        Token& arg = info.gettok();
        if(Equal(arg,")")){
            info.advance();
            break;
        }
        if(!IsName(arg)){
            info.advance();
            if(!printedErrors) {
                printedErrors=true;
                ERR_SECTION(
                    ERR_HEAD2(arg)
                    ERR_MSG("'"<<arg <<"' is not a valid argument name.")
                    ERR_LINE2(arg,"bad")
                )
            }
            continue;
            // return PARSE_ERROR;
        }
        info.advance();
        tok = info.gettok();
        if(!Equal(tok,":")){
            if(!printedErrors) {
                printedErrors=true;
                ERR_SECTION(
                    ERR_HEAD2(tok)
                    ERR_MSG("Expected : not "<<tok <<".")
                    ERR_LINE2(tok,"bad")
                )
            }
            continue;
            // return PARSE_ERROR;
        }
        info.advance();

        Token dataType{};
        SignalDefault result = ParseTypeId(info,dataType, nullptr);
        Assert(result == SignalDefault::SIGNAL_SUCCESS);
        // auto id = info.ast->getTypeInfo(info.currentScopeId,dataType)->id;
        TypeId strId = info.ast->getTypeString(dataType);

        function->arguments.add({});
        auto& argv = function->arguments[function->arguments.size()-1];
        argv.name = arg; // add the argument even if default argument fails
        argv.stringType = strId;

        function->nonDefaults++; // We don't have defaults at all
        // this is important for function overloading

        printedErrors = false; // we succesfully parsed this so we good?

        tok = info.gettok();
        if(tok.type == ','){
            info.advance();
            continue;
        }else if(Equal(tok,")")){
            info.advance();
            break;
        }else{
            printedErrors = true; // we bad and might keep being bad.
            // don't do printed errors?
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Expected , or ) not "<<tok <<".")
                ERR_LINE2(tok,"bad")
            )
            continue;
            // Continuing since we saw ( and are inside of arguments.
            // we must find ) to leave.
            // return PARSE_ERROR;
        }
    }
    // TODO: check token out of bounds
    printedErrors=false;
    tok = info.gettok();
    Token tok2 = info.get(info.at()+2);
    if(Equal(tok,"-") && !(tok.flags&lexer::TOKEN_FLAG_SPACE) && Equal(tok2,">")){
        info.advance();
        info.advance();
        tok = info.gettok();
        
        WHILE_TRUE {
            
            Token tok = info.gettok();
            if(tok.type == '{' || tok.type == '{'){
                break;   
            }
            
            Token dt{};
            SignalDefault result = ParseTypeId(info,dt, nullptr);
            if(result!=SignalDefault::SIGNAL_SUCCESS){
                break; // prevent infinite loop
                // info.advance();
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
            tok = info.gettok();
            if(tok.type == '{' || tok.type == '{'){
                // info.advance(); { is parsed in ParseBody
                break;   
            } else if(tok.type == ','){
                info.advance();
                continue;
            } else {
                if(function->linkConvention != LinkConventions::NONE)
                    break;
                if(!printedErrors){
                    printedErrors=true;
                    ERR_SECTION(
                        ERR_HEAD2(tok)
                        ERR_MSG("Expected a comma or curly brace. '"<<tok <<"' is not okay.")
                        ERR_LINE2(tok,"bad coder");
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
    
    Token& bodyTok = info.gettok();
    
    if(Equal(bodyTok,";")){
        info.advance();
        ERR_SECTION(
            ERR_HEAD2(bodyTok)
            ERR_MSG("Operator overloading must have a body. You have forgotten the body when defining '"<<function->name<<"'.")
            ERR_LINE2(bodyTok,"replace with {}")
        )
    } else if(Equal(bodyTok,"{")){
        info.functionScopes.add({});
        ASTScope* body = 0;
        SignalDefault result = ParseBody(info,body, function->scopeId);
        info.functionScopes.pop();
        function->body = body;
    } else {
        ERR_SECTION(
            ERR_HEAD2(bodyTok)
            ERR_MSG("Operator has no body! Did the return types parse incorrectly? Use curly braces to define the body.")
            ERR_LINE2(bodyTok,"expected {")
        )
    }

    return SignalAttempt::SIGNAL_SUCCESS;
}
#endif
SignalIO ParseFunction(ParseInfo& info, ASTFunction*& function, ASTStruct* parentStruct){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _PLOG(FUNC_ENTER)

    function = info.ast->createFunction();
    // function->tokenRange.firstToken = fnToken;
    // function->tokenRange.endIndex = info.at()+1;;

    bool needsExplicitCallConvention = false;
    bool specifiedConvention = false;
    // Token linkToken{};

    StringView view_fn_name{};
    auto token_name = info.getinfo(&view_fn_name);
    auto tok_name = info.gettok();
    while (token_name->type == lexer::TOKEN_ANNOTATION){
        // NOTE: The reason that @extern-stdcall is used instead of @extern @stdcall is to prevent
        //   the programmer from making mistakes. With two annotations, they may forget to specify the calling convention.
        //   Calling convention is very important. If extern and call convention is combined then they won't forget.
        if(view_fn_name == "hide"){
            function->setHidden(true);
        } else if (view_fn_name == "dllimport"){
            function->callConvention = CallConventions::STDCALL;
            specifiedConvention = true;
            function->linkConvention = LinkConventions::DLLIMPORT;
            needsExplicitCallConvention = true;
            // linkToken = name;
        } else if (view_fn_name == "varimport"){
            function->callConvention = CallConventions::STDCALL;
            specifiedConvention = true;
            function->linkConvention = LinkConventions::VARIMPORT;
            needsExplicitCallConvention = true;
            // linkToken = name;
        } else if (view_fn_name == "import"){
            function->callConvention = CallConventions::STDCALL;
            specifiedConvention = true;
            function->linkConvention = LinkConventions::IMPORT;
            needsExplicitCallConvention = true;
            // linkToken = name;
        } else if (view_fn_name == "stdcall"){
            function->callConvention = CallConventions::STDCALL;
            specifiedConvention = true;
        // } else if (Equal(name,"@cdecl")){
        // cdecl has not been implemented. It doesn't seem important.
        // default x64 calling convention is used.
        //     Assert(false); // not implemented in type checker and generator. type checker might work as is.
        //     function->callConvention = CallConventions::CDECL_CONVENTION;
        //     specifiedConvention = true;
        } else if (view_fn_name == "betcall"){
            function->callConvention = CallConventions::BETCALL;
            function->linkConvention = LinkConventions::NONE;
            specifiedConvention = true;
        // IMPORTANT: When adding calling convention, do not forget to add it to the "Did you mean" below!
        } else if (view_fn_name == "unixcall"){
            function->callConvention = CallConventions::UNIXCALL;
            specifiedConvention = true;
        } else if (view_fn_name == "native"){
            function->linkConvention = NATIVE;
        } else if (view_fn_name == "intrinsic"){
            function->callConvention = CallConventions::INTRINSIC;
            function->linkConvention = NATIVE;
        } else {
            auto tok = info.gettok();
            // It should not warn you because it is quite important that you use the right annotations with functions
            // Mispelling external or the calling convention would be very bad.
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Unknown annotation for functions.")
                ERR_LINE2(tok,"unknown")

                // if(StartsWith(name, "@extern")) {
                //     log::out << log::YELLOW << "Did you mean @extern-stdcall or @extern-cdecl\n";
                // }
            )
        }
        info.advance();
        token_name = info.getinfo();
        tok_name = info.gettok();
        continue;
    }
    if(function->linkConvention != LinkConventions::NONE){
        function->setHidden(true);
        // native doesn't use a calling convention
        if(needsExplicitCallConvention && !specifiedConvention){
            ERR_SECTION(
                ERR_HEAD2(tok_name)
                ERR_MSG("You must specify a calling convention. The default is betcall which you probably don't want. Use @stdcall or @unixcall.")
                ERR_LINE2(tok_name, "missing call convention")
            )
        }
    }
    
    if(token_name->type != lexer::TOKEN_IDENTIFIER){
        info.ast->destroy(function);
        function = nullptr;
        ERR_SECTION(
            ERR_HEAD2(tok_name)
            ERR_MSG("Invalid name for function. Name must be alphabetical.")
            ERR_LINE2(tok_name,"not valid")
        )
        return SIGNAL_COMPLETE_FAILURE;
    }
    function->name = view_fn_name;

    ScopeInfo* funcScope = info.ast->createScope(info.currentScopeId, info.getNextOrder(), nullptr);
    function->scopeId = funcScope->id;

    // ensure we leave this parse function with the same scope we entered with
    auto prevScope = info.currentScopeId;
    defer { info.currentScopeId = prevScope; };

    info.currentScopeId = function->scopeId;

    auto tok = info.gettok();
    if(tok.type == '<'){
        info.advance();
        while(true){
            tok = info.gettok();
            if(tok.type == '>'){
                info.advance();
                break;
            }
            std::string datatype{};
            auto signal = ParseTypeId(info, datatype);

            SIGNAL_INVALID_DATATYPE(datatype)

            function->polyArgs.add({datatype});

            tok = info.gettok();
            if (tok.type == ',') {
                info.advance();
                continue;
            } else if(tok.type == '>') {
                info.advance();
                break;
            } else {
                ERR_SECTION(
                    ERR_HEAD2(tok)
                    ERR_MSG("Unexpected token for polymorphic function arguments.")
                    ERR_LINE2(tok,"bad token")
                )
                return SIGNAL_COMPLETE_FAILURE;
            }
        }
    }
    info.advance();
    tok = info.gettok();
    if(tok.type != '('){
        ERR_SECTION(
            ERR_HEAD2(tok_name)
            ERR_MSG("Function expects a parentheses after the name.")
            ERR_LINE2(tok_name,"this function")
        )
        return SIGNAL_COMPLETE_FAILURE;
    }

    if(parentStruct) {
        function->arguments.add({});
        auto& argv = function->arguments[function->arguments.size()-1];
        argv.name = "this";
        // nocheckin, add source location for this?
        // argv.name.tokenStream = info.tokens; // feed and print won't work if we set these, they think this comes from the stream and tries to print it
        // argv.name.tokenIndex = tok.tokenIndex;
        // argv.name.line = tok.line;
        // argv.name.column = tok.column+1;
        argv.defaultValue = nullptr;
        function->nonDefaults++;

        std::string* str = info.ast->createString();
        *str = parentStruct->polyName + "*"; // TODO: doesn't work with polymorhpism
        argv.stringType = info.ast->getTypeString(*str);;
    }
    info.advance(); // skip (
    bool printedErrors=false;
    bool mustHaveDefault=false;
    TokenRange prevDefault={};
    WHILE_TRUE {
        StringView view_arg{};
        auto arg = info.getinfo(&view_arg);
        if(arg->type == ')'){
            info.advance();
            break;
        }
        if(arg->type != lexer::TOKEN_IDENTIFIER){
            auto tok = info.gettok();
            info.advance();
            if(!printedErrors) {
                printedErrors=true;
                ERR_SECTION(
                    ERR_HEAD2(tok)
                    ERR_MSG("Invalid name for function parameter.")
                    ERR_LINE2(tok,"bad name")
                )
                return SIGNAL_COMPLETE_FAILURE;
            }
            continue;
            // return PARSE_ERROR;
        }
        info.advance();
        tok = info.gettok();
        if(tok.type != ':'){
            if(!printedErrors) {
                printedErrors=true;
                ERR_SECTION(
                    ERR_HEAD2(tok)
                    ERR_MSG("Function parameters use this format: '(name: type, name: type)'.")
                    ERR_LINE2(tok,"should be ':'")
                )
                return SIGNAL_COMPLETE_FAILURE;
            }
            continue;
            // return PARSE_ERROR;
        }
        info.advance();

        std::string dataType{};
        auto bad = info.gettok();
        auto signal = ParseTypeId(info,dataType);

        if(dataType.empty()) {
            // TODO: Better error message
            ERR_DEFAULT_LAZY(bad, "Invalid type.")
            return SIGNAL_COMPLETE_FAILURE;
        }
        Assert(signal == SIGNAL_SUCCESS);

        // auto id = info.ast->getTypeInfo(info.currentScopeId,dataType)->id;
        TypeId strId = info.ast->getTypeString(dataType);

        function->arguments.add({});
        auto& argv = function->arguments[function->arguments.size()-1];
        argv.name = view_arg; // add the argument even if default argument fails
        argv.stringType = strId;

        ASTExpression* defaultValue=nullptr;
        tok = info.gettok();
        if(tok.type == '='){
            info.advance();
            
            auto signal = ParseExpression(info,defaultValue);
            SIGNAL_SWITCH_LAZY()

            prevDefault = defaultValue->tokenRange;

            mustHaveDefault=true;
        } else if(mustHaveDefault){
            // printedErrors doesn't matter here.
            // If we end up here than our parsing is probably correct so far and we might as
            // well log this error since it's probably a "real" error not caused by a cascade.
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Expected a default argument because of previous default argument "<<prevDefault<<" at "<<prevDefault.firstToken.line<<":"<<prevDefault.firstToken.column<<".")
                ERR_LINE2(tok,"bad")
            )
            // continue; we don't continue since we want to parse comma if it exists
        }
        if(!defaultValue)
            function->nonDefaults++;

        argv.defaultValue = defaultValue;
        printedErrors = false; // we succesfully parsed this so we good?

        tok = info.gettok();
        if(tok.type == ','){
            info.advance();
            continue;
        }else if(tok.type == ')'){
            info.advance();
            break;
        }else{
            printedErrors = true; // we bad and might keep being bad.
            // don't do printed errors?
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Unexpected token in function parameters.")
                ERR_LINE2(tok,"why is this here?")
            )
            return SIGNAL_COMPLETE_FAILURE;
            // continue;
            // Continuing since we saw ( and are inside of arguments.
            // we must find ) to leave.
            // return PARSE_ERROR;
        }
    }
    // TODO: check token out of bounds
    printedErrors=false;
    tok = info.gettok();
    auto tok2 = info.gettok(1);
    if(tok.type == '-' && !(tok.flags&lexer::TOKEN_FLAG_ANY_SUFFIX) && tok2.type == '>'){
        info.advance(2);
        tok = info.gettok();
        
        WHILE_TRUE {
            auto tok = info.gettok();
            if(tok.type == '{' || tok.type == '{'){
                break;   
            }
            
            std::string datatype{};
            auto signal = ParseTypeId(info,datatype);

            SIGNAL_INVALID_DATATYPE(datatype)

            TypeId strId = info.ast->getTypeString(datatype);
            function->returnValues.add({});
            function->returnValues.last().stringType = strId;
            // function->returnValues.last().valueToken = datatype;
            // function->returnValues.last().valueToken.endIndex = info.at()+1;
            printedErrors = false;
            
            tok = info.gettok();
            if(tok.type == '{' || tok.type == '{'){
                // info.advance(); { is parsed in ParseBody
                break;   
            } else if(tok.type == ','){
                info.advance();
                continue;
            } else {
                if(function->linkConvention != LinkConventions::NONE)
                    break;
                if(!printedErrors){
                    printedErrors=true;
                    ERR_SECTION(
                        ERR_HEAD2(tok)
                        ERR_MSG("Unexpected token in function return values.")
                        ERR_LINE2(tok,"bad programmer")
                    )
                    return SIGNAL_COMPLETE_FAILURE;
                }
                continue;
                // Continuing since we are inside of return values and expect
                // something to end it
            }
        }
    }
    // function->tokenRange.endIndex = info.at()+1; // don't include body in function's token range
    // the body's tokenRange can be accessed with function->body->tokenRange
    
    tok = info.gettok();
    if(tok.type == ';'){
        info.advance();
        if(function->needsBody()){
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Functions must have a body unless they are imported. Forward declarations are not necessary.")
                ERR_LINE2(tok,"replace with {}")
            )
        }
    } else if(tok.type == '{'){
        if(!function->needsBody()) {
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Native/external functions cannot have a body. Native functions are handled by the language. External functions link to functions outside your source code.")
                ERR_LINE2(tok,"use ; instead")
            )
        }
        
        info.functionScopes.add({});
        ASTScope* body = 0;
        auto signal = ParseBody(info,body, function->scopeId);
        SIGNAL_SWITCH_LAZY()

        info.functionScopes.pop();
        function->body = body;
    } else if(function->needsBody()) {
        ERR_SECTION(
            ERR_HEAD2(info.gettok(-1))
            ERR_MSG("Function has no body! Did the return types parse incorrectly? Use curly braces to define the body.")
            ERR_LINE2(info.gettok(-1),"expected '{' afterwards")
        )
    }

    return SIGNAL_SUCCESS;
}
#ifdef gone
SignalIO ParseAssignment(ParseInfo& info, ASTStatement*& statement){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
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
        info.advance();

    statement = info.ast->createStatement(ASTStatement::ASSIGN);
    // statement->varnames.stealFrom(varnames);
    statement->firstExpression = nullptr;
    statement->globalAssignment = globalAssignment;

    Token lengthTokenOfLastVar{};

    //-- Evaluate variables on the left side
    int startIndex = info.at()+1;
    // DynamicArray<ASTStatement::VarName> varnames{};
    while(!info.end()){
        Token name = info.gettok();
        if(!IsName(name)){
            ERR_SECTION(
                ERR_HEAD2(name)
                ERR_MSG("Expected a valid name for assignment ('"<<name<<"' is not).")
                ERR_LINE2(info.gettok(),"cannot be a name");
            )
            return SignalAttempt::FAILURE;
        }
        info.advance();

        statement->varnames.add({name});
        Token token = info.gettok();
        if(token.type == ':'){
            info.advance(); // :
            attempt=false;


            token = info.gettok();
            if(token.type == '=') {
                int index = statement->varnames.size()-1;
                while(index>=0 && !statement->varnames[index].declaration){
                    statement->varnames[index].declaration = true;
                    index--;
                }
            } else {
                Token typeToken{};
                SignalDefault result = ParseTypeId(info,typeToken, nullptr);
                if(result!=SignalDefault::SIGNAL_SUCCESS)
                    return CastSignal(result);
                // Assert(result==SignalDefault::SIGNAL_SUCCESS);
                int arrayLength = -1;
                Token token1 = info.gettok();
                Token token2 = info.get(info.at()+2);
                Token token3 = info.get(info.at()+3);
                if(Equal(token1,"[") && Equal(token3,"]")) {
                    info.advance();
                    info.advance();
                    info.advance();

                    if(IsInteger(token2)) {
                        lengthTokenOfLastVar = token2;
                        arrayLength = ConvertInteger(token2);
                        if(arrayLength<0){
                            ERR_SECTION(
                                ERR_HEAD2(token2)
                                ERR_MSG("Array cannot have negative size.")
                                ERR_LINE2(info.get(info.at()-1),"< 0")
                            )
                            arrayLength = 0;
                        }
                    } else {
                        ERR_SECTION(
                            ERR_HEAD2(token2)
                            ERR_MSG("The length of an array can only be specified with number literals. Use macros to avoid magic numbers. Constants have not been implemented but when they have, they will work too.")
                            ERR_LINE2(info.get(info.at()-1), "must be positive integer literal")
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
                    ERR_HEAD2(token)
                    ERR_MSG("Global variables must use declaration syntax. You must use a ':' (':=' if you want the type to be inferred).")
                    ERR_LINE2(token, "add a colon before this")
                )
            }
        }
        token = info.gettok();
        if(token.type == ','){
            info.advance();
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

    Token token = info.gettok();
    if(token.type == '=') {
        info.advance(); // =

        SignalAttempt result = ParseExpression(info,statement->firstExpression,false);

    } else if(token.type == '{') {
        // array initializer
        info.advance(); // {

        while(true){
            Token token = info.gettok();
            if(token.type == '}') {
                info.advance(); // }
                break;
            }
            ASTExpression* expr = nullptr;
            SignalAttempt result = ParseExpression(info, expr, false);
            
            if(result == SignalAttempt::SIGNAL_SUCCESS) {
                Assert(expr);
                statement->arrayValues.add(expr);
            }

            token = info.gettok();
            if(token.type == ',') {
                info.advance(); // ,
                // TODO: Error if you see consecutive commas
                // Note that a trailing comma is allowed: { 1, 2, }
                // It's convenient
            } else if(token.type == '}') {
                info.advance(); // }
                break;
            } else {
                info.advance(); // prevent infinite loop
                ERR_SECTION(
                    ERR_HEAD2(token)
                    ERR_MSG("Unexpected token '"<<token<<"' at end of array initializer. Use comma for another element or ending curly brace to end initializer.")
                    ERR_LINE2(token, "expected , or }")
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
                ERR_HEAD2(token) // token should be {
                ERR_MSG("You cannot have more expressions in the array initializer than the array length you specified.")
                // TODO: Show which token defined the array length
                ERR_LINE2(lengthTokenOfLastVar, "the maximum length")
                // You could do a token range from the first expression to the last but that could spam the console
                // with 100 expressions which would be annoying so maybe show 5 or 8 values and then do ...
                ERR_LINE2(token, ""<<statement->arrayValues.size()<<" expressions")
            )
        }
    }
    token = info.gettok();
    if(Equal(token,";")){
        info.advance(); // parse ';'. won't crash if at end
    }

    // statement->tokenRange.firstToken = info.get(startIndex);
    // statement->tokenRange.endIndex = info.at()+1;
    return SignalAttempt::SIGNAL_SUCCESS;  
}
#endif
SignalIO ParseBody(ParseInfo& info, ASTScope*& bodyLoc, ScopeId parentScope, ParseFlags in_flags, ParseFlags* out_flags){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
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

        auto token = info.gettok();
        if(token.type == '{') {
            info.advance();
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

    // if(!info.end()){
        // bodyLoc->tokenRange.firstToken = info.gettok();
        // endToken is set later
    // }

    if(!inheritScope || (in_flags & PARSE_TRULY_GLOBAL)) {
        info.nextContentOrder.add(CONTENT_ORDER_ZERO);
    }
    defer {
        if(!inheritScope || (in_flags & PARSE_TRULY_GLOBAL)) {
            info.nextContentOrder.pop(); 
        }
    };

    while(true) {
        StringView view{};
        auto tok = info.getinfo(&view);
        if(tok->type == lexer::TOKEN_ANNOTATION) {
            if(view == "@dump_asm") {
                info.advance();
                bodyLoc->flags |= ASTNode::DUMP_ASM;
            } else if(view ==  "@dump_bc") {
                info.advance();
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

    StringView view{};
    while(true){
        auto token = info.getinfo(&view);
        if(token->type == lexer::TOKEN_EOF) {
            if(expectEndingCurlyBrace) {
                ERR_DEFAULT(info.gettok(), "Sudden end of body. You are missing an ending curly brace '}'.", "here")
                return SIGNAL_COMPLETE_FAILURE;
            }
            break;
        }
        
        if((in_flags & PARSE_INSIDE_SWITCH) && token->type == lexer::TOKEN_ANNOTATION) {
            if(view == "@fall") {
                info.advance();
                if(out_flags)
                    *out_flags = (ParseFlags)(*out_flags | PARSE_HAS_CASE_FALL);
                continue;
            }
        }
        if(expectEndingCurlyBrace && token->type == '}'){
            info.advance();
            break;
        }
        if((in_flags & PARSE_INSIDE_SWITCH) && (token->type == lexer::TOKEN_IDENTIFIER && view == "case") || (!expectEndingCurlyBrace && token->type == '}')) {
            // we should not info.advance on else
            break;
        }
        
        if(token->type == ';'){
            info.advance();
            continue;
        }
        info.ignoreErrors = errorsWasIgnored;
        
        ASTStatement* tempStatement=0;
        ASTStruct* tempStruct=0;
        ASTEnum* tempEnum=0;
        ASTFunction* tempFunction=0;
        ASTScope* tempNamespace=0;
        
        SignalIO result = SIGNAL_NO_MATCH;

        if(token->type == '{'){
            ASTScope* body=0;
            auto signal = ParseBody(info, body, info.currentScopeId);
            switch(signal){
            case SIGNAL_SUCCESS: break;
            case SIGNAL_COMPLETE_FAILURE: return signal;
            default: Assert(false);
            }
            // if(result2!=SignalDefault::SIGNAL_SUCCESS){
            //     info.advance(); // skip { to avoid infinite loop
            //     continue;
            // }
            // result = CastSignal(result2);
            tempStatement = info.ast->createStatement(ASTStatement::BODY);
            tempStatement->firstBody = body;
            tempStatement->tokenRange = body->tokenRange;
        }
        bool noCode = false;
        bool log_nodeid = false;
        while(true) {
            auto token2 = info.getinfo(&view);
            if(token2->type == lexer::TOKEN_ANNOTATION) {
                if(view == "@TEST_ERROR") {
                    info.ignoreErrors = true;
                    noCode = true;
                    info.advance();
                    
                    Assert(false); // nocheckin
                    // auto res = ParseAnnotationArguments(info, nullptr);
                    continue;
                } else if(view == "@TEST_CASE") {
                    info.advance();

                    Assert(false); // nocheckin
                    // auto res = ParseAnnotationArguments(info, nullptr);
                    // while(true) {
                    //     token2 = info.advance(); // skip case name
                    //     if(token2.flags & (lexer::TOKEN_FLAG_SPACE | lexer::TOKEN_FLAG_NEWLINE)) {
                    //         break;   
                    //     }
                    // }
                    continue;
                } else if(view == "@no_code") {
                    noCode = true;
                    info.advance();
                    continue;
                }  else if(view == "@nodeid") {
                    log_nodeid = true;
                    info.advance();
                    continue;
                }
            }
            break;
        }

        StringView view{};
        token = info.getinfo(&view);
        auto token_tiny = info.gettok();

        SignalIO signal=SIGNAL_NO_MATCH;
        if(token->type == lexer::TOKEN_STRUCT) {
            info.advance();
            signal = ParseStruct(info,tempStruct);
        } else if(token->type == lexer::TOKEN_FUNCTION) {
            info.advance();
            signal = ParseFunction(info,tempFunction, nullptr);
        } else if(token->type == lexer::TOKEN_ENUM) {
            info.advance();
            signal = ParseEnum(info,tempEnum);
        } else if(token->type == lexer::TOKEN_NAMESPACE) {
            info.advance();
            signal = ParseNamespace(info,tempNamespace);
        }
        // nocheckin
        // if(signal==SIGNAL_NO_MATCH)
        //     signal = ParseOperator(info,tempFunction);
        // if(signal==SIGNAL_NO_MATCH)
        //     signal = ParseAssignment(info,tempStatement);
        // if(signal==SIGNAL_NO_MATCH)
        //     signal = ParseFlow(info,tempStatement);
        if(signal==SIGNAL_NO_MATCH){
            // bad name of function? it parses an expression
            // prop assignment or function call
            ASTExpression* expr=nullptr;
            signal = ParseExpression(info,expr);
            if(expr){
                tempStatement = info.ast->createStatement(ASTStatement::EXPRESSION);
                tempStatement->firstExpression = expr;
                tempStatement->tokenRange = expr->tokenRange;
            }
        }
        // TODO: What about annotations here?
        switch(signal) {
        case SIGNAL_SUCCESS: break;
        case SIGNAL_NO_MATCH: {
            Assert(false); // nocheckin, fix error
            auto token = info.gettok();
            ERR_SECTION(
                ERR_HEAD2(token_tiny)
                ERR_MSG("Did not expect '"<<info.lexer->tostring(token_tiny)<<"' when parsing body. A new statement, struct or function was expected (or enum, namespace, ...).")
                ERR_LINE2(token_tiny,"what")
            )
            // prevent infinite loop. Loop 'only occurs when scoped
            info.advance();
        }
        case SIGNAL_COMPLETE_FAILURE: return SIGNAL_COMPLETE_FAILURE;
        default: Assert(false);
        }

        if(noCode && tempStatement) {
            if(!tempStatement->isNoCode()) {
                tempStatement->setNoCode(noCode);
            }
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

        if(signal==SIGNAL_NO_MATCH){
            // Try again. Important because loop would break after one time if scoped is false.
            // We want to give it another go.
            continue;
        }
        // skip semi-colon if it's there.
        auto tok = info.gettok();
        if(tok.type == ';'){
            info.advance();
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

    // bodyLoc->tokenRange.endIndex = info.at()+1;
    return SIGNAL_SUCCESS;
}
#endif // PARSER_OFF
/*
    Public function
*/
ASTScope* ParseImport(u32 import_id, Compiler* compiler){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _VLOG(log::out <<log::BLUE<<  "##   Parser   ##\n";)
    
    ParseInfo info{};
    info.compiler = compiler;
    info.lexer = &compiler->lexer;
    info.ast = compiler->ast;
    info.reporter = &compiler->reporter;
    info.import_id = import_id;

    info.setup_token_iterator();

    ASTScope* body = nullptr;
    // nocheckin, what to do about 'import path as namespace'
    // if(theNamespace.size()==0){
        body = info.ast->createBody();
        body->scopeId = info.ast->globalScopeId;
    // } else {
    //     // TODO: Should namespaced imports from a source file you import as a namespace
    //     //  have global has their parent? It does now, ast->globalScopeId is used.

    //     body = info.ast->createNamespace(theNamespace);
    //     ScopeInfo* newScope = info.ast->createScope(info.ast->globalScopeId, CONTENT_ORDER_ZERO, body);

    //     body->scopeId = newScope->id;
    //     newScope->name = theNamespace;
    //     info.ast->getScope(newScope->parent)->nameScopeMap[theNamespace] = newScope->id;
    // }
    info.currentScopeId = body->scopeId;

    info.functionScopes.add({});
    #ifndef PARSER_OFF
    auto signal = ParseBody(info, body, info.ast->globalScopeId, PARSE_TRULY_GLOBAL);
    #endif
    info.functionScopes.pop();
    
    // nocheckin
    // info.compileInfo->compileOptions->compileStats.errors += info.errors;
    
    return body;
}
}