#include "BetBat/Parser.h"
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

#undef TEMP_ARRAY
// #define TEMP_ARRAY(TYPE,NAME) QuickArray<TYPE> NAME; NAME.init(&scratch_allocator);
#define TEMP_ARRAY(TYPE,NAME) QuickArray<TYPE> NAME;

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
    switch((u16)token->type) { // cast to u16 to avoid g++ compiler warnings
    case '+':  return AST_ADD;
    case '-':  return AST_SUB;
    case '*':  return AST_MUL;
    case '/':  return AST_DIV;
    case '%':  return AST_MODULO;
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
//     if(!token->str || token->length!=1) return (OperationType)0; // early exit since all assign op only use one character
//     char chr = *token->str;
//     // TODO: Allow % modulus as assignment?
//     if(chr == '+'||chr=='-'||chr=='*'||chr=='/'||
//         chr=='/'||chr=='|'||chr=='&'||chr=='^'){
//         int _=0;
//         return parser::IsOp(token,_);
//     }
//     return (OperationType)0;
// }
OperationType IsAssignOp(lexer::TokenInfo* token){
    switch((u16)token->type) { // cast to u16 to avoid gcc compiler warnings
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
        case AST_MODULO:
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
        case AST_UNARY_SUB:
        case AST_CAST:
            // return 15;
        case AST_INCREMENT:
        case AST_DECREMENT:
            // return 16;
        case AST_REFER:
        case AST_DEREF:
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
    // if(op==AST_MUL||op==AST_DIV||op==AST_MODULO) return 10;
    // if(op==AST_BAND||op==AST_BOR||op==AST_BXOR||
    //     op==AST_BLSHIFT||op==AST_BRSHIFT) return 13;
    // if(op==AST_BNOT || op==AST_NOT || op==AST_CAST) return 15;
    // if(op==AST_REFER || op==AST_DEREF) return 16;
    // if(op==AST_MEMBER || op == AST_FROM_NAMESPACE) return 20;
    log::out << log::RED<<__FILE__<<":"<<__LINE__<<", missing "<<(OperationType)op<<"\n";
    return 0;
}


/*###############################
    Code for parse functions
#################################*/

SignalIO ParseContext::parseTypeId(std::string& outTypeId, int* tokensParsed){
    using namespace engone;
    
    int startToken = info.gethead();
    
    outTypeId = {};
    
    // char[][]
    // Array<char[],Array<char[],char[]>>[]
    // Array < char [
    
    /* a sketch in BNF of type syntax
        type = "(" normal_type ")" | "(" function_type ")"

        normal_type = identifier [ poly_list ] { "*" }

        poly_list = "<" [ type_list ] ">"

        type_list = type { "," type }

        fn < (fn () -> i32) * >

        fn ( fn() -> i32, i32 )

        fn () -> fn () -> i32 , i32

        fn <fn () -> i32, i32>

        var: fn () -> i32, i32

        function_type = "fn" [ convention ] [ poly_list ] "(" [ type_list ] ")" [ "->" return_types ] { "*" }

        return_types = [ "(" type_list ")" ] | type if inside(type_list) else type_list

        convention = "@stdcall" | "@betcall" | "@unixcall"
    */
    
    struct Env {
        std::string buffer="";
        // bool new_type = true;
        // bool type_list = false;
        bool func_params = false;
        bool func_returns = false;
        bool multiple_return_values = false;
        
        bool only_pointer = false;
        bool may_be_name = true;
        
        // bool expect_closing_paren = false;
        // bool consume_closing_paren = false;
    };
    DynamicArray<Env> envs;
    envs.add({});
    
    lexer::TokenInfo* token = nullptr;
    lexer::SourceLocation loc{};
    StringView view{};
    #define CURTOK() { token = info.getinfo(&view); loc = info.getloc(); }
    // if(token->type == lexer::TOKEN_EOF) break; 
    
    // TODO: Cleanup and simplify this code.
    //   It's also not complete, correct syntax will be fine but invalid syntax could get through the parser and
    //   into type checker.
    while(true) {
        CURTOK();
        
        if(envs.last().func_params) {
            if(token->type == ')') {
                info.advance();
                envs.last().buffer += ")";
            } else Assert(false); // TODO: throw error?
            CURTOK();
            auto token2 = info.getinfo(1);
            if (( 0 == (token->flags & lexer::TOKEN_FLAG_ANY_SUFFIX) && token->type == '-' && token2->type == '>')) {
                info.advance(2);
                envs.last().buffer += "->";
                envs.last().func_returns = true;
                envs.last().func_params = false;
                envs.last().may_be_name = false;
                auto token3 = info.getinfo();
                if(token3->type == '(') {
                    info.advance();
                    envs.last().multiple_return_values = true;
                    envs.last().buffer += "(";
                }
                envs.add({});
                continue;
            } else {
                // no return values
                // ERR_SECTION(
                //     ERR_HEAD2(loc)
                //     ERR_MSG("Function ")
                // )
            }
            if(envs.size() > 1) {
                envs[envs.size()-2].buffer += envs.last().buffer;
                envs.pop();
            } else {
                break;
            }
            continue;
        } else if(envs.last().func_returns) {
            if(envs.size() > 1) {
                envs[envs.size()-2].buffer += envs.last().buffer;
                envs.pop();
            } else {
                // envs[envs.size()-2].buffer += envs.last().buffer;
                break;
            }
            continue;
        }
        if(!envs.last().only_pointer) {
            // Skip arg names in function pointer, they don't serve a purpose but it's
            // nice to easily take a function with names and turn it into a func pointer.
            if(envs.size() > 1) {
                if(envs[envs.size()-2].func_params) {
                    auto tok2 = getinfo(1);
                    if (token->type == lexer::TOKEN_IDENTIFIER && tok2->type == ':') {
                        info.advance(2);
                        continue;
                    }
                }
            }
                
            if (token->type == lexer::TOKEN_FUNCTION && envs.last().may_be_name) {
                info.advance();
                envs.last().buffer.append("fn");
                
                while (true) {
                    CURTOK();
                    if (token->type != lexer::TOKEN_ANNOTATION) {
                        break;
                    }
                    info.advance();
                    envs.last().buffer.append("@");
                    envs.last().buffer.append(view);
                }
                
                CURTOK()
                if (token->type != '(') {
                    ERR_SECTION(
                        ERR_HEAD2(loc)
                        ERR_MSG("Invalid syntax for function pointer, expected '('.")
                        ERR_LINE2(loc, "here")
                    )
                    break;
                }
                info.advance();
                envs.last().buffer.append("(");
                envs.last().func_params = true;
                envs.add({});
                // envs.last().expect_closing_paren = true;
                // envs.last().consume_closing_paren = false;
                continue;
            } else if (token->type == lexer::TOKEN_IDENTIFIER && envs.last().may_be_name) {
                info.advance();
                
                envs.last().buffer += view;
                envs.last().may_be_name = false;
                continue;
            } else if (token->type == lexer::TOKEN_NAMESPACE_DELIM) {
                info.advance();
                envs.last().buffer += "::";
                envs.last().may_be_name = true;
                // flawed?
                continue;
            } else if (token->type == '<') {
                info.advance();
                envs.last().buffer += "<";
                envs.last().may_be_name = false;
                envs.add({});
                continue;
            }
            //  else if (token->type == '(' && envs.last().may_be_name) {
            //     if(envs.size() == 1) {
            //         break;
            //     }
            //     info.advance();
            //     envs.last().buffer += "(";
            //     envs.last().may_be_name = false;
            //     envs.add({});
            //     envs.last().expect_closing_paren = true;
            //     // envs.last().consume_closing_paren = false;
            //     continue;
            // }
        }
        if (token->type == '*') {
            if(envs.last().may_be_name) {
                return SIGNAL_FAILURE;
            }
            info.advance();
            envs.last().buffer += "*";
            envs.last().only_pointer = true;
        } else if (token->type == '[') {
            auto token = info.getinfo(&view, 1);
            // char[] { 1, 3 }  could be an array on stack
            // or the end of a function (char[] as return value)
            // auto token2 = info.getinfo(2);
            // if(token->type == ']' && token2->type != '{') {
            if(token->type == ']') {
                if(envs.last().may_be_name) {
                    return SIGNAL_FAILURE;
                }
                info.advance(2);
                std::string tmp = envs.last().buffer;
                envs.last().buffer = "Slice<" + tmp + ">";
                continue;
            } else {
                break;
            }
        } else if (token->type == ',') {
            if(envs.size() == 1) {
                break;
            }
            if(envs[envs.size()-2].multiple_return_values) {
                
            } else if(envs[envs.size()-2].func_returns) {
                // we break here because
                //  func: fn ()->i32, param: i32
                // should be treated as
                //  a function pointer type with one return value
                // and not two return values (i32 and param where ": i32" is left out)
                // User should enclose return values in parenthesis if they need multiple return values
                break;
            }

            info.advance();
            envs[envs.size()-2].buffer += envs.last().buffer;
            envs[envs.size()-2].buffer += ",";
            envs.last().buffer.clear();
            envs.last().may_be_name = true;
            envs.last().only_pointer = false;
        } else if (token->type == '>') {
            if(envs.size() == 1) {
                break;
            }
            if(envs[envs.size()-2].func_returns) {
                break; // in function pointers, we have 2 envs, not just one so we check function pointer too.
            }
            info.advance();
            envs[envs.size()-2].buffer += envs.last().buffer;
            envs[envs.size()-2].buffer += ">";
            envs.pop();
        } else if (token->type == ')') {
            if(envs.size() == 1) {
                break;
            }
            if(envs[envs.size()-2].multiple_return_values) {
                info.advance();
                envs.last().buffer += ")";
            } else if(envs[envs.size()-2].func_returns) {
                break; // in function pointers, we have 2 envs, not just one so we check function pointer too.
            }
            // if(envs.last().expect_closing_paren) {
                envs[envs.size()-2].buffer += envs.last().buffer;
                envs.pop();   
                continue;
            // }
            // info.advance();
            // envs[envs.size()-2].buffer += envs.last().buffer;
            // envs[envs.size()-2].buffer += ")";
            // envs.pop();
        } else {
            break;
        }
    }
    
    outTypeId = "";
    for(int i=0;i<envs.size();i++) {
        outTypeId += envs[i].buffer; 
    }
    if(tokensParsed)
        *tokensParsed = info.gethead() - startToken;
        
    // log::out << "Parsed: '" << outTypeId << "'\n";
    return SIGNAL_SUCCESS;
    #undef NEXT
#ifdef gone
    // TODO: Optimize
    DynamicArray<std::string> strings;
    strings.add({});
    
    // Invalid syntax for types is not properly handled such as "Math::*[]"
    // But such syntax is caught in the type checker even if it is allowed here.
    
    int depth = 0;
    bool wasName = false;
    bool only_pointers = false;
    outTypeId = {};
    StringView view{};
    while(true){
        auto token = info.getinfo(&view);
        if(token->type == lexer::TOKEN_EOF)
            break;

        if(only_pointers) {
            if(token->type == '*') {
                info.advance();
                strings.last().append("*");
                continue;
            } else {
                break;
            }
        }

        if(token->type == lexer::TOKEN_IDENTIFIER || (wasName = false)) {
            if(wasName) {
                break;
            }
            info.advance();
            wasName = true;
            strings.last().append(view.ptr,view.len);
            // outTypeId.append(view.ptr,view.len);
        } else if(token->type == ',') {
            if(depth == 0) {
                break;
            }
            info.advance();
            Assert(strings.size() >= 2);
            strings[strings.size()-2].append(strings.last());
            strings.last().clear();
            strings[strings.size()-2].append(",");
        } else if(token->type == '<') {
            info.advance();
            depth++;
            strings.last().append("<");
            strings.add({});
            // outTypeId.append("<");
         } else if(token->type == '>') {
            if(depth == 0){
                break;
            }
            info.advance();
            depth--;
            Assert(strings.size() >= 2);
            strings[strings.size()-2].append(strings.last());
            strings.pop();
            strings.last().append(">");
            
            if(depth == 0){
                only_pointers = true;
                continue;
            }
        } else if(token->type == '*') {
            info.advance();
            strings.last().append("*");
        } else if(token->type == '[') {
            auto token = info.getinfo(&view, 1);
            if(token->type == ']') {
                info.advance(2);
                std::string tmp = strings.last();
                strings.last() = "Slice<" + tmp + ">";
                // outTypeId.append("[]");
                if(depth == 0)
                    break;
            } else {
                break;
            }
        } else if(token->type == lexer::TOKEN_NAMESPACE_DELIM) {
            info.advance();
            strings.last().append("::");
            // outTypeId.append("::");
        } else {
            // invalid token for types, we stop
            break;
        }
    }
    Assert(strings.size() == 1);
    outTypeId = strings.last();
    // TODO: Pointers should be handled inside the loop
    // while(true) {
    //     auto tok = info.gettok();
    //     if(tok.type == '*') {
    //         info.advance();
    //         outTypeId.append("*");
    //     } else {
    //         break;
    //     }
    // }
    if(tokensParsed)
        *tokensParsed = info.gethead() - startToken;
    return SIGNAL_SUCCESS;
#endif
}
SignalIO ParseContext::parseStruct(ASTStruct*& astStruct){
    using namespace engone;
    // MEASURE;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _PLOG(FUNC_ENTER)

    StringView name_view{};
    auto name_token = info.getinfo(&name_view);
    auto name_tok = info.gettok();
   
    bool hideAnnotation = false;

    bool no_padding = false;

    while (name_token->type == lexer::TOKEN_ANNOTATION){
        info.advance();
        // TODO: Parse annotation parentheses
        if(name_view == "hide"){
            hideAnnotation=true;
        } else if(name_view == "no_padding"){
            no_padding=true;
        } else {
            ERR_SECTION(
                ERR_HEAD2(name_tok)
                ERR_MSG("Unknown annotation for structs.")
                ERR_LINE2(name_tok,"unknown");
            )
            // TODO: List the available annotations
        }
        name_token = info.getinfo(&name_view);
        name_tok = info.gettok();
        continue;
    }
    
    if(name_token->type != lexer::TOKEN_IDENTIFIER){
        auto bad = info.gettok();
        ERR_SECTION(
            ERR_HEAD2(bad)
            ERR_MSG("Expected a name, "<<info.lexer->tostring(bad)<<" isn't.")
            ERR_LINE2(bad,"bad")
        )
        return SIGNAL_COMPLETE_FAILURE;
    }
    info.advance(); // name

    int startIndex = info.gethead();
    astStruct = info.ast->createStruct(name_view);
    astStruct->location = info.srcloc(name_tok);
    // astStruct->tokenRange.firstToken = structToken;
    // astStruct->tokenRange.startIndex = startIndex;
    // astStruct->tokenRange.tokenStream = info.tokens;
    astStruct->name = name_view;
    astStruct->setHidden(hideAnnotation);
    astStruct->no_padding = no_padding;
    StringView view{};
    auto token = info.getinfo(&view);
    if(token->type == '<'){
        info.advance();
        // polymorphic type
        // astStruct->name += "<";
    
        WHILE_TRUE {
            token = info.getinfo(&view);
            if(token->type == lexer::TOKEN_EOF) {
                ERR_SUDDEN_EOF(name_tok,"Sudden end of file when parsing polymorphic types for struct.","this struct")
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
                // astStruct->name += view;
            }
            
            token = info.getinfo();
            if(token->type == ','){
                info.advance();
                // astStruct->name += ",";
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
        // astStruct->name += ">";
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
        auto name_token = info.getinfo(&name_view);
        auto name_tok = info.gettok();

        if(name_token->type == lexer::TOKEN_EOF) {
            ERR_SUDDEN_EOF(name_tok, "Sudden end when parsing struct.", "this struct")
            
            return SIGNAL_COMPLETE_FAILURE;   
        }

        if(name_token->type == ';'){
            info.advance();
            continue;
        } else if(name_token->type == '}'){
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
        if(name_token->type == lexer::TOKEN_FUNCTION) {
            info.advance();
            wasFunction = true;
            ASTFunction* func = 0;

            SignalIO signal = parseFunction(func, astStruct, false);
            switch(signal) {
            case SIGNAL_COMPLETE_FAILURE: return SIGNAL_COMPLETE_FAILURE;
            case SIGNAL_SUCCESS: break;
            default: Assert(false);
            }
            func->parentStruct = astStruct;
            astStruct->add(func);
            // ,func->polyArgs.size()==0?&func->baseImpl:nullptr);
        } else if(name_token->type == lexer::TOKEN_UNION) {
            info.advance();

            log::out << log::GOLD << "Union is an incomplete features\n";
            Assert(false);

            // Assert(union_depth < 1); // TODO: support nested unions and structs
            
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
        } else if(name_token->type == lexer::TOKEN_STRUCT) {
            Assert(("struct in struct not allowed",false)); // error instead
            // ERR_SECTION(
            //     ERR_HEAD2(name)
            //     ERR_MSG("Structs not allowed inside structs.")
            //     ERR_LINE2(name,"bad")
            // )
        } else if(name_token->type == lexer::TOKEN_IDENTIFIER){
            for(auto& mem : astStruct->members) {
                if(mem.name == name_view) {
                    ERR_SECTION(
                        ERR_HEAD2(name_tok)
                        ERR_MSG("The name '"<<info.lexer->tostring(name_tok)<<"' is already used in another member of the struct. You cannot have two members with the same name.")
                        ERR_LINE2(mem.location, "first")
                        ERR_LINE2(name_tok, "last")
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
            auto signal = parseTypeId(typeToken,nullptr);

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
            auto tok0 = info.gettok(0);
            StringView num_data = {};
            auto tok1 = info.gettok(&num_data, 1);
            auto tok2 = info.gettok(2);
            if(tok0.type == '[' && tok2.type == ']') {
                info.advance(3);

                // info.advance();
                // auto tok = info.gettok(&view);
                // bool is_negative = false;
                // if(tok.type == '-') {
                //     is_negative = true;
                //     info.advance();
                //     tok = info.gettok(&view);
                // }
                // i64 value = 0;
                // if(info.lexer->isIntegerLiteral(tok, &value)) {
                //     info.advance();
                //     if(is_negative)
                //         nextValue = -value;
                //     else
                //         nextValue = value;
                // } else {
                //     ERR_SECTION(
                //         ERR_HEAD2(tok)
                //         ERR_MSG("Values for enum members can only be a integer literal. In the future, any constant expression will be allowed.")
                //         ERR_LINE2(tok,"not an integer literal")
                //     )
                // }
                // TODO: Handle hexidecimal use code above
                if(tok1.type == lexer::TOKEN_LITERAL_INTEGER) {
                    // u64 num = 0;
                    // memcpy(&num, num_data.ptr, num_data.len);
                    arrayLength = lexer::ConvertInteger(num_data);
                    
                    if(arrayLength<=0){
                        ERR_SECTION(
                            ERR_HEAD2(tok1)
                            ERR_MSG("Array cannot have negative or zero size.")
                            ERR_LINE2(tok1,"<= 0")
                        )
                        arrayLength = 0;
                    }
                } else {
                    ERR_SECTION(
                        ERR_HEAD2(tok1)
                        ERR_MSG("The length of an array can only be specified with number literals. Use macros to avoid magic numbers. Constants have not been implemented but when they have, they will work too.")
                        ERR_LINE2(tok1, "must be positive integer literal")
                    )
                }
                // the member is not a slice, it's an actual array
                // typeToken = "Slice<" + typeToken +">";
            }
            // Assert(arrayLength==-1); // arrays in structs not implemented yet
            // std::string temps = typeToken;
            
            TypeId typeId = info.ast->getTypeString(typeToken);
            
            signal = SIGNAL_SUCCESS;
            ASTExpression* defaultValue=0;
            token = info.getinfo();
            if(token->type == '='){
                info.advance();

                signal = parseExpression(defaultValue);
            }
            switch(signal) {
            case SIGNAL_SUCCESS: {
                astStruct->members.add({});
                auto& mem = astStruct->members.last();
                mem.name = name_view;
                mem.defaultValue = defaultValue;
                mem.stringType = typeId;
                mem.location = info.srcloc(name_tok);
                if(arrayLength!=-1)
                    mem.array_length = arrayLength;
                
                // auto l = info.lexer->getTokenSource_unsafe(mem.location);
                // log::out << l->line << " " << l->column<<"\n";
                
                break;
            }
            case SIGNAL_COMPLETE_FAILURE: return SIGNAL_COMPLETE_FAILURE;
            default: Assert(false);
            }
        } else {
            info.advance();
            if(!hadRecentError){
                ERR_SECTION(
                    ERR_HEAD2(name_tok)
                    ERR_MSG("Expected a name, "<<info.lexer->tostring(name_tok)<<" isn't.")
                    ERR_LINE2(name_tok,"bad")
                )
                return SIGNAL_COMPLETE_FAILURE;
            }
            hadRecentError=true;
            // errorParsingMembers = SignalAttempt::FAILURE;
            continue;   
        }
        StringView tok_view{};
        auto token = info.getinfo(&tok_view);
        if(token->type == lexer::TOKEN_FUNCTION || token->type == lexer::TOKEN_STRUCT || token->type == lexer::TOKEN_UNION){
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
                //     ERR_LINE2(token->tokenIndex,"bad");
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
SignalIO ParseContext::parseNamespace(ASTScope*& astNamespace){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _PLOG(FUNC_ENTER)
    int startIndex = info.gethead();

    Assert(("parsing namespace, incomplete", false));

    bool hideAnnotation = false;

    StringView name_view{};
    auto token = info.getinfo(&name_view);
    while (token->type == lexer::TOKEN_ANNOTATION){
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
        token = info.getinfo(&name_view);
        continue;
    }

    if(token->type != lexer::TOKEN_IDENTIFIER){
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
            signal = parseFunction(tempFunction, nullptr, false);
        } else if(token->type == lexer::TOKEN_OPERATOR) {
            info.advance();
            signal = parseFunction(tempFunction, nullptr, true);
        } else if(token->type == lexer::TOKEN_STRUCT) {
            info.advance();
            signal = parseStruct(tempStruct);
        } else if(token->type == lexer::TOKEN_ENUM) {
            info.advance();
            signal = parseEnum(tempEnum);
        } else if(token->type == lexer::TOKEN_NAMESPACE) {
            info.advance();
            signal = parseNamespace(tempNamespace);
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

SignalIO ParseContext::parseEnum(ASTEnum*& astEnum){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _PLOG(FUNC_ENTER)
    int startIndex = info.gethead();

    bool hideAnnotation = false;

    ASTEnum::Rules enumRules = ASTEnum::NONE;

    StringView view_name{};
    auto token = info.getinfo(&view_name);
    while (token->type == lexer::TOKEN_ANNOTATION){
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
        } else if(view_name == "lenient_switch") {
            enumRules = (ASTEnum::Rules)(enumRules | ASTEnum::LENIENT_SWITCH);
        } else {
            auto tok = info.gettok();
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("'"<< info.lexer->tostring(tok) << "' is not a known annotation for enums.")
                ERR_LINE2(tok,"unknown")
            )
        }
        info.advance();
        token = info.getinfo(&view_name);
        continue;
    }

    if(token->type != lexer::TOKEN_IDENTIFIER){
        auto tok = info.gettok();
        ERR_SECTION(
            ERR_HEAD2(tok)
            ERR_MSG("You must have a valid name after enum keywords.")
            ERR_LINE2(tok,"not a name")
        )
        return SIGNAL_COMPLETE_FAILURE;
    }
    auto token_name = info.gettok();
    info.advance(); // name

    TypeId colonType = {};

    token = info.getinfo();
    if(token->type == ':') {
        info.advance();

        std::string tokenType{};
        auto signal = parseTypeId(tokenType);
        
        SIGNAL_INVALID_DATATYPE(tokenType)

        colonType = info.ast->getTypeString(tokenType);
    }
    
    token = info.getinfo();
    if(token->type != '{'){
        auto tok = info.gettok();
        ERR_SECTION(
            ERR_HEAD2(tok)
            ERR_MSG("Enum should have a curly brace after it's name (or type if you specified one).")
            ERR_LINE2(tok,"not a '{'")
        )
        return SIGNAL_COMPLETE_FAILURE;
    }
    info.advance();
    i64 nextValue=0;
    if(enumRules & ASTEnum::BITFIELD) {
        nextValue = 1;
    }
    astEnum = info.ast->createEnum(view_name);
    astEnum->location = info.srcloc(token_name);
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
        // name->flags &= ~lexer::TOKEN_FLAG_ANY_SUFFIX;

        // bool semanticError = false;
        // if there was an error you could skip adding the member but that
        // cause a cascading effect of undefined AST_ID which are referring to the member 
        // that had an error. It's probably better to add the number but use a zero or something as value.

        info.advance();
        StringView view{};
        auto token = info.getinfo(&view);
        if(token->type == '='){
            // TODO: Handle expressions, not just literals.

            info.advance();
            auto tok = info.gettok(&view);
            bool is_negative = false;
            if(tok.type == '-') {
                is_negative = true;
                info.advance();
                tok = info.gettok(&view);
            }
            i64 value = 0;
            if(info.lexer->isIntegerLiteral(tok, &value)) {
                info.advance();
                if(is_negative)
                    nextValue = -value;
                else
                    nextValue = value;
            } else {
                ERR_SECTION(
                    ERR_HEAD2(tok)
                    ERR_MSG("Values for enum members can only be a integer literal. In the future, any constant expression will be allowed.")
                    ERR_LINE2(tok,"not an integer literal")
                )
            }

            // TODO: Delete old code
            // if(token->type == lexer::TOKEN_LITERAL_INTEGER){
            //     info.advance();
            //     nextValue = lexer::ConvertInteger(view);
            //     token = info.getinfo();
            // } else if(token->type == lexer::TOKEN_LITERAL_INTEGER){
            //     info.advance();
            //     nextValue = lexer::ConvertHexadecimal(view);
            //     token = info.getinfo();
            // } else if(token->type == lexer::TOKEN_LITERAL_STRING && (token->flags & lexer::TOKEN_FLAG_SINGLE_QUOTED) && view.len == 1) {
            //     info.advance();
            //     nextValue = *view.ptr;
            //     token = info.getinfo();
            // } else {
            //     auto tok = info.gettok();
            //     ERR_SECTION(
            //         ERR_HEAD2(tok)
            //         ERR_MSG("Values for enum members can only be a integer literal. In the future, any constant expression will be allowed.")
            //         ERR_LINE2(tok,"not an integer literal")
            //     )
            // }
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

    _PLOG(log::out << "Parsed enum "<<log::LIME<< view_name <<log::NO_COLOR <<" with "<<astEnum->members.size()<<" members\n";)
    return error;
}

SignalIO ParseContext::parseAnnotationArguments(lexer::TokenRange* out_arguments) {
    // MEASURE;

    if(out_arguments)
        *out_arguments = {};

    // Can the previous token have a suffix? probably not?
    auto tok_prev = info.gettok(-1);
    if(tok_prev.flags & lexer::TOKEN_FLAG_ANY_SUFFIX) {
        return SIGNAL_SUCCESS;
    }

    auto tok = info.gettok();
    if(tok.type != '('){
        return SIGNAL_SUCCESS;
    }
    info.advance(); // skip (

    int depth = 0;

    if(out_arguments) {
        out_arguments->importId = info.import_id;
        out_arguments->token_index_start = info.gethead();
    }

    while(true) {
        auto tok = info.gettok();
        if(tok.type == lexer::TOKEN_EOF) {
            ERR_SUDDEN_EOF(tok_prev, "Sudden end of file when parsing annotation arguments", "this annotation")
        }
        if(tok.type == ')'){
            if(depth == 0) {
                info.advance();
                break;
            }
            depth--;
        }
        if(tok.type == '('){
            depth++;
        }

        // if(out_arguments) {
        //     if(!out_arguments->firstToken.str) {
        //         *out_arguments = tok;
        //     } else {
        //         out_arguments->endIndex = info.at() + 1;
        //     }
        // }

        info.advance();
    }

    if(out_arguments)
        out_arguments->token_index_end = info.gethead();
    
    return SIGNAL_SUCCESS;
}
SignalIO ParseContext::parseAnnotation(StringView* out_annotation_name, lexer::TokenRange* out_arguments) {
    // MEASURE;

    *out_annotation_name = {};
    *out_arguments = {};

    StringView view{};
    auto token = info.getinfo(&view);
    if(token->type != lexer::TOKEN_ANNOTATION) {
        return SIGNAL_SUCCESS;
    }

    *out_annotation_name = view;
    info.advance();

    // ParseAnnotationArguments check for suffix of the previous token
    // Args will not be parsed in cases like this: @hello (sad, arg) but will here: @hello(happy)
    auto signal = parseAnnotationArguments(out_arguments);

    SIGNAL_SWITCH_LAZY()

    return SIGNAL_SUCCESS;
}

SignalIO ParseContext::parseArguments(ASTExpression* fncall, int* count){
    ZoneScopedC(tracy::Color::OrangeRed1);

    // fncall->nonNamedArgs = 0; // can't do this because caller sets it to 1 if expr is a method call

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
                ERR_LINE2(tok, "here")
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
        auto signal = parseExpression(expr);
        
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

SignalIO ParseContext::parseExpression(ASTExpression*& expression){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _PLOG(FUNC_ENTER)
    
    TEMP_ARRAY(ASTExpression*, values);
    TEMP_ARRAY(lexer::Token, extraTokens); // ops, assignOps and castTypes doesn't store the token so you know where it came from. We therefore use this array.
    TEMP_ARRAY(OperationType, ops);
    TEMP_ARRAY(OperationType, assignOps);
    TEMP_ARRAY(TypeId, castTypes);
    TEMP_ARRAY(lexer::SourceLocation, saved_locations);
    DynamicArray<std::string> namespaceNames{};

    bool prev_assign = allow_assignments;

    defer {
        for(auto e : values)
            info.ast->destroy(e);
        allow_assignments = prev_assign;
    };

    bool shouldComputeExpression = false;

    StringView view{};
    // auto token = info.getinfo(&view);
    // if(token->type == lexer::TOKEN_ANNOTATION && view == "run") {
    //     info.advance();
    //     shouldComputeExpression = true;
    // }

    // bool negativeNumber=false;
    bool expectOperator=false;
    WHILE_TRUE_N(10000) {
        auto token = info.getinfo(&view);
        
        OperationType opType = (OperationType)0;
        bool ending=false;
        
        if(expectOperator){
            int extraOpNext=0;
            auto token1 = info.getinfo(1);
            if(token->type == ')'){
                // token = info.advance();
                ending = true;
            } else if(token->type == ';'){
                ending = true;
            } else if(token->type == '.'){
                info.advance();
                auto mem_loc = info.getloc();
                
                StringView member_view{};
                auto member_tok = info.getinfo(&member_view);
                if(member_tok->type != lexer::TOKEN_IDENTIFIER){
                    auto bad = info.gettok();
                    ERR_DEFAULT_LAZY(bad, "Expected a property name, not "<<info.lexer->tostring(bad)<<".")
                    continue;
                }
                info.advance();

                auto token = info.getinfo(&view);
                auto poly_tiny = info.gettok();
                std::string polyTypes="";
                if(token->type == '<' && 0==(token->flags&lexer::TOKEN_FLAG_SPACE)){
                    auto signal = parseTypeId(polyTypes);

                    SIGNAL_INVALID_DATATYPE(polyTypes)
                }

                int startToken=info.gethead(); // start of fncall
                token = info.getinfo();
                auto tok_paren_tiny = info.gettok();
                if(token->type == '('){
                    info.advance();
                    // fncall for methods
                    ASTExpression* tmp = info.ast->createExpression(TypeId(AST_FNCALL));
                    tmp->location = mem_loc; // TODO: Scuffed source location
                    tmp->name = std::string(member_view) + polyTypes;

                    // Create "this" argument in methods
                    // tmp->args->add(values.last());
                    tmp->args.add(values.last());
                    tmp->nonNamedArgs = 0;
                    tmp->nonNamedArgs++;
                    values.pop();
                    
                    // tmp->setImplicitThis(true);
                    
                    tmp->setMemberCall(true);

                    // Parse the other arguments
                    int count = 0;
                    auto signal = parseArguments(tmp, &count);
                    switch(signal){
                        case SIGNAL_SUCCESS: break;
                        case SIGNAL_COMPLETE_FAILURE: {
                            info.ast->destroy(tmp);
                            return SIGNAL_COMPLETE_FAILURE;
                        }
                        default: Assert(false);
                    }

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
                tmp->location = mem_loc;
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
                auto loc = info.getloc();
                saved_locations.add(info.getloc());
                info.advance();

                if(!allow_assignments) {
                    // This error will save the programmer from making many mistakes
                    ERR_SECTION(
                        ERR_HEAD2(loc)
                        ERR_MSG("WOW, slow down there. Did you want an equals operator or assignment here? Use @iknow annotation to remove this error.")
                        ERR_LINE2(loc, "= or ==")
                        ERR_EXAMPLE(1, "if @iknow a = 5 { ... }")
                    )
                    // we can keep parsing, this is just a semantic error
                }

                ops.add(AST_ASSIGN);
                assignOps.add((OperationType)0);

                // allow_inferred_initializers = true;

                _PLOG(log::out << "Operator "<<token<<"\n";)
            } else if((opType = IsAssignOp(token)) && token1->type  == '=') {
                saved_locations.add(info.getloc());
                info.advance(2);

                ops.add(AST_ASSIGN);
                assignOps.add(opType);

                _PLOG(log::out << "Operator "<<token<<"\n";)
            } else if((opType = parser::IsOp(token,token1,view,extraOpNext))){
                saved_locations.add(info.getloc());

                // if(Equal(token,"*") && (info.now().flags&lexer::TOKEN_FLAG_NEWLINE)){
                if(info.getinfo(-1)->flags&lexer::TOKEN_FLAG_NEWLINE){
                    auto tok0 = info.gettok(-1);
                    auto tok1 = info.gettok();
                    
                    ERR_SECTION(
                        ERR_HEAD2(tok1, ERROR_AMBIGUOUS_PARSING)
                        ERR_MSG("Possible ambiguous meaning! '"<<info.lexer->tostring(tok1) << "' is treated as a binary operation (operation with two expressions) but the operator was placed on a new line. "
                        "Was your intention a unary operation where the left expressions is it's own statement and unrelated to the expression on the new line? Please put a semi-colon to solve this ambiguity.")
                        ERR_LINE2(tok0, "semi-colon is recommended after statements");
                        ERR_LINE2(tok1, "currently a binary operation");
                        ERR_EXAMPLE(1, "n := 5\n*ptr = 9\nn := 5 * ptr = 9");
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
                auto signal = parseExpression(indexExpr);
                SIGNAL_SWITCH_LAZY()

                auto tok = info.gettok();
                if(tok.type != ']'){
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
                tmp->location = info.srcloc(token_tiny);
                // tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = startIndex;
                // tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;

                tmp->left = values.last();
                values.pop();
                tmp->right = indexExpr;
                values.add(tmp);
                // tmp->constantValue = tmp->left->constantValue && tmp->right->constantValue; // TODO: constant evaluation
                continue;
            } else if(token->type == lexer::TOKEN_IDENTIFIER && view == "++"){ // TODO: Optimize
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_INCREMENT));
                tmp->location = info.getloc();
                info.advance();

                tmp->setPostAction(true);

                tmp->left = values.last();
                values.pop();
                values.add(tmp);
                // tmp->constantValue = tmp->left->constantValue;
                continue;
            } else if(token->type == lexer::TOKEN_IDENTIFIER && view == "--"){ // TODO: Optimize
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_DECREMENT));
                tmp->location = info.getloc();

                info.advance();
                tmp->setPostAction(true);

                tmp->left = values.last();
                values.pop();
                values.add(tmp);
                // tmp->constantValue = tmp->left->constantValue;// 

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
                    //      && !Equal(token,"}") && token->type != ',' && token->type != '{' && !Equal(token,"]")
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
        } else {
            bool try_inferred_initializer = false;
            // if(allow_inferred_initializers && token->type == '{') {
            //     try_inferred_initializer = true;
            //     allow_inferred_initializers = false;
            // }
            try_inferred_initializer = true; // always try this, we handle error in type checker?

            bool cstring = false;
            if(token->type == lexer::TOKEN_ANNOTATION) {
                if(view == "cstr") {
                    info.advance();

                    // TokenRange args; // TODO: Warn if cstr has errors?
                    // ParseAnnotationArguments(info, );

                    token = info.getinfo(&view);
                    cstring = true;
                    continue;
                } else if(view == "iknow") {
                    info.advance();
                    allow_assignments = true;
                    continue;
                }
            } else if(token->type == '&') {
                saved_locations.add(info.getloc());
                info.advance();
                ops.add(AST_REFER);
                continue;
            }else if(token->type == '*'){
                saved_locations.add(info.getloc());
                info.advance();
                ops.add(AST_DEREF);
                continue;
            } else if(token->type == '!'){
                saved_locations.add(info.getloc());
                info.advance();
                ops.add(AST_NOT);
                continue;
            } else if(token->type == '~'){
                saved_locations.add(info.getloc());
                info.advance();
                ops.add(AST_BNOT);
                continue;
            } else if(token->type == lexer::TOKEN_IDENTIFIER && (view == "cast" || view == "cast_unsafe") ){
                auto token_tiny = info.gettok();
                saved_locations.add(info.getloc());
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
                auto signal = parseTypeId(datatype);

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
                if(tok.type != '>'){
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
                saved_locations.add(info.getloc());
                info.advance();
                ops.add(AST_UNARY_SUB);
                continue;
            } else if(token->type == lexer::TOKEN_IDENTIFIER && view == "++"){
                saved_locations.add(info.getloc());
                info.advance();
                ops.add(AST_INCREMENT);
                continue;
            } else if(token->type == lexer::TOKEN_IDENTIFIER && view == "--"){
                saved_locations.add(info.getloc());
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
                    saved_locations.pop();
                    negativeNumber = true;
                }
                Assert(view.ptr[0]!='-');// ensure that the tokenizer hasn't been changed
                // to clump the - together with the number token
                
                bool printedError = false;
                u64 num=0;
                // log::out << view << "\n";
                bool unsignedSuffix = false;
                bool signedSuffix = false;
                for(int i=0;i<view.len;i++){
                    char c = view.ptr[i];
                    if(c == 'u') {
                        unsignedSuffix = true;
                        continue;
                    } else if(c == 's') {
                        signedSuffix = true;
                        continue;
                    } else if(c == '_') {
                        continue;
                    } else if(c < '0' || c > '9') {
                        printedError = true;
                        ERR_SECTION(
                            ERR_HEAD2(token_tiny)
                            // TODO: Show the limit? pow(2,64)-1
                            ERR_MSG("Invalid suffix on number! Suffixes such as 92u (unsigned), 2s (signed) are allowed but '"<<info.lexer->tostring(token_tiny)<<"' does not contain those.")
                            ERR_LINE2(token_tiny,"remove trailing letters");
                        )
                        break;
                    }
                    if(num * 10 < num && !printedError) {
                        ERR_SECTION(
                            ERR_HEAD2(token_tiny)
                            // TODO: Show the limit? pow(2,64)-1
                            ERR_MSG("Number overflow! '"<<info.lexer->tostring(token_tiny)<<"' is to large for 64-bit integers!")
                            ERR_LINE2(token_tiny,"to large!");
                        )
                        printedError = true;
                        break;
                    }
                    num = num*10 + (c-'0');
                }
                // NOTE: OLD suffix code when suffix wasn't a part of the integer.
                //   The lexer combines integer and trailing letters into an integer token.
                //   Don't remove code below in case we switch back to separating them.
                // StringView suffix{};
                // auto tok_suffix = info.gettok(&suffix);
                // if(tok_suffix.type == lexer::TOKEN_IDENTIFIER && (token->flags&(lexer::TOKEN_FLAG_ANY_SUFFIX))==0) {
                //     if(suffix == "u") {
                //         info.advance();
                //         unsignedSuffix  = true;
                //     } else if(suffix == "s") {
                //         info.advance();
                //         signedSuffix  = true;
                //     } else if((suffix.ptr[0]|32) >= 'a' && (suffix.ptr[0]|32) <= 'z'){
                //         ERR_SECTION(
                //             ERR_HEAD2(token_tiny)
                //             // TODO: Show the limit? pow(2,64)-1
                //             ERR_MSG("Invalid suffix on number! Suffixes such as 92u (unsigned), 2s (signed) are allowed but '"<<info.lexer->tostring(token_tiny)<<"' does not contain those.")
                //             ERR_LINE2(token_tiny,"remove trailing letters");
                //         )
                //     }
                // }
                ASTExpression* tmp = 0;
                if(unsignedSuffix) {
                    if(negativeNumber) {
                        ERR_SECTION(
                            ERR_HEAD2(token_tiny)
                            ERR_MSG("You cannot use an unsigned suffix and a minus at the same time. They collide.")
                            ERR_LINE2(token_tiny,"remove suffix")
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
                tmp->location = info.srcloc(token_tiny);
                tmp->i64Value = negativeNumber ? -num : num;
                
                // tmp->constantValue = true;// 
                values.add(tmp);
            } else if(token->type == lexer::TOKEN_LITERAL_DECIMAL){
                auto loc = info.getloc();

                ASTExpression* tmp = nullptr;
                Assert(view.ptr[0]!='-');// ensure that the tokenizer hasn't been changed
                // to clump the - together with the number token
                bool double_suffix = false;
                double value = lexer::ConvertDecimal(view, &double_suffix);
                if(double_suffix) {
                    info.advance();
                    tmp = info.ast->createExpression(TypeId(AST_FLOAT64));
                    tmp->f64Value = value;
                    if (ops.size()>0 && ops.last() == AST_UNARY_SUB){
                        ops.pop();
                        saved_locations.pop();
                        tmp->f64Value = -tmp->f64Value;
                    }
                } else {
                    info.advance();
                    tmp = info.ast->createExpression(TypeId(AST_FLOAT32));
                    tmp->f32Value = value;
                    if (ops.size()>0 && ops.last() == AST_UNARY_SUB){
                        ops.pop();
                        saved_locations.pop();
                        tmp->f32Value = -tmp->f32Value;
                    }
                }
                tmp->location = loc;
                values.add(tmp);
                // tmp->constantValue = true;// 
            } else if(token->type == lexer::TOKEN_LITERAL_HEXIDECIMAL || token->type == lexer::TOKEN_LITERAL_BINARY || token->type == lexer::TOKEN_LITERAL_OCTAL) {
                auto token_tiny = info.gettok();
                info.advance();
                
                int significant_digits = 0;
                int signedness_suffix = 0;

                i64 num = 0;
                bool yes = lexer->isIntegerLiteral(token_tiny, &num, &significant_digits, &signedness_suffix);
                Assert(yes);

                bool unsignedSuffix = signedness_suffix == lexer::Lexer::UNSIGNED_SUFFIX;
                bool signedSuffix = signedness_suffix == lexer::Lexer::SIGNED_SUFFIX;

                bool negativeNumber = false;
                if (ops.size()>0 && ops.last() == AST_UNARY_SUB){
                    ops.pop();
                    saved_locations.pop();
                    negativeNumber = true;
                }

                ASTExpression* tmp = nullptr;
                if(significant_digits<=8) {
                    if(unsignedSuffix)
                        tmp = info.ast->createExpression(TypeId(AST_UINT32));
                    else
                        tmp = info.ast->createExpression(TypeId(AST_INT32));
                } else if(significant_digits<=16) {
                    if(unsignedSuffix)
                        tmp = info.ast->createExpression(TypeId(AST_UINT64));
                    else
                        tmp = info.ast->createExpression(TypeId(AST_INT64));
                } else {
                    if(unsignedSuffix)
                        tmp = info.ast->createExpression(TypeId(AST_UINT64));
                    else
                        tmp = info.ast->createExpression(TypeId(AST_INT64));
                    ERR_SECTION(
                        ERR_HEAD2(token_tiny)
                        ERR_MSG("Hexidecimal overflow! '"<<info.lexer->tostring(token_tiny)<<"' is to large for 64-bit integers!")
                        ERR_LINE2(token_tiny,"to large!");
                    )
                }
                tmp->location = info.srcloc(token_tiny);
                tmp->i64Value = num;
                if(negativeNumber)
                    tmp->i64Value = -tmp->i64Value;
                values.add(tmp);

            // } else if(token->type == lexer::TOKEN_LITERAL_HEXIDECIMAL){
            //     auto token_tiny = info.gettok();
            //     info.advance();
                
            //     bool negativeNumber = false;
            //     if (ops.size()>0 && ops.last() == AST_UNARY_SUB){
            //         ops.pop();
            //         saved_locations.pop();
            //         negativeNumber = true;
            //     }
            //     // 0x000000001 will be treated as 64 bit value and
            //     // it probably should because you added those 
            //     // zero for that exact reason.
            //     ASTExpression* tmp = nullptr;
            //     int hex_prefix = 0;
            //     if(view.len>=2 && *(u16*)view.ptr == 0x7830) // 0x7830 represents '0x' (backwards because little endian)
            //         hex_prefix = 2;
                    
            //     int significant_digits = 0;
            //     bool unsignedSuffix = false;
            //     bool signedSuffix = false;
            //     for(int i=hex_prefix;i<view.len;i++) {
            //         char c = view.ptr[i];
            //         if(c == 'u') {
            //             unsignedSuffix = true;
            //             continue;
            //         } else if(c == 's') {
            //             signedSuffix = true;
            //             continue;
            //         } else if(c == '_') {
            //             continue;
            //         }
            //         significant_digits++;
            //     }
            //     // NOTE: We use signed integers, not unsigned integers. We don't have to cast to signed which we
            //     //   often want. Later we will infer the type for literals but that's future stuff.
            //     if(significant_digits<=8) {
            //         if(unsignedSuffix)
            //             tmp = info.ast->createExpression(TypeId(AST_UINT32));
            //         else
            //             tmp = info.ast->createExpression(TypeId(AST_INT32));
            //     } else if(significant_digits<=16) {
            //         if(unsignedSuffix)
            //             tmp = info.ast->createExpression(TypeId(AST_UINT64));
            //         else
            //             tmp = info.ast->createExpression(TypeId(AST_INT64));
            //     } else {
            //         if(unsignedSuffix)
            //             tmp = info.ast->createExpression(TypeId(AST_UINT64));
            //         else
            //             tmp = info.ast->createExpression(TypeId(AST_INT64));
            //         ERR_SECTION(
            //             ERR_HEAD2(token_tiny)
            //             ERR_MSG("Hexidecimal overflow! '"<<info.lexer->tostring(token_tiny)<<"' is to large for 64-bit integers!")
            //             ERR_LINE2(token_tiny,"to large!");
            //         )
            //     }
            //     tmp->location = info.srcloc(token_tiny);
            //     tmp->i64Value = lexer::ConvertHexadecimal(view);
            //     if(negativeNumber)
            //         tmp->i64Value = -tmp->i64Value;
            //     values.add(tmp);
            //     // tmp->constantValue = true;// 
            } else if(token->type == lexer::TOKEN_LITERAL_STRING) {
                auto loc = info.getloc();
                info.advance();
                
                ASTExpression* tmp = 0;
                // if(token->length == 1){
                if((token->flags&lexer::TOKEN_FLAG_SINGLE_QUOTED)){
                    tmp = info.ast->createExpression(TypeId(AST_CHAR));
                    tmp->charValue = *view.ptr;
                    // tmp->constantValue = true;// 
                }else if((token->flags&lexer::TOKEN_FLAG_DOUBLE_QUOTED)) {
                    tmp = info.ast->createExpression(TypeId(AST_STRING));
                    tmp->name = view;
                    // tmp->constantValue = true;// 
                    if(cstring)
                        tmp->flags |= ASTNode::NULL_TERMINATED;

                    // A string of zero size can be useful which is why
                    // the code below is commented out.
                    // if(token->length == 0){
                        // ERR_SECTION(
                        // ERR_HEAD2(token, "string should not have a length of 0\n";
                        // This is a semantic error and since the syntax is correct
                        // we don't need to return PARSE_ERROR. We could but things will be fine.
                    // }
                }
                tmp->location = loc;
                values.add(tmp);
                // tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                // tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(token->type == lexer::TOKEN_TRUE || token->type == lexer::TOKEN_FALSE){
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_BOOL));
                tmp->boolValue = token->type == lexer::TOKEN_TRUE;
                tmp->location = info.getloc();
                info.advance();
                // tmp->constantValue = true;// 
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
            //         if(token->type == ','){
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
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_NULL));
                tmp->location = info.getloc();
                info.advance();
                values.add(tmp);
                // tmp->constantValue = true;// 
                // tmp->tokenRange.firstToken = token;
                // tmp->tokenRange.startIndex = info.at();
                // tmp->tokenRange.endIndex = info.at()+1;
                // tmp->tokenRange.tokenStream = info.tokens;
            } else if(token->type == lexer::TOKEN_SIZEOF || token->type == lexer::TOKEN_NAMEOF || token->type == lexer::TOKEN_TYPEID) {
                ASTExpression* tmp = nullptr;
                if(token->type == lexer::TOKEN_SIZEOF)
                    tmp = info.ast->createExpression(TypeId(AST_SIZEOF));
                else if(token->type == lexer::TOKEN_NAMEOF)
                    tmp = info.ast->createExpression(TypeId(AST_NAMEOF));
                else if(token->type == lexer::TOKEN_TYPEID)
                    tmp = info.ast->createExpression(TypeId(AST_TYPEID));
                else Assert(false);
                tmp->location = info.getloc();

                info.advance();

                bool hasParentheses = false;
                auto tok = info.gettok();
                if(tok.type == '(') {
                    // NOTE: We need to handle parentheses here because otherwise
                    //  typeid(Apple).ptr_level would be seen as typeid( (Apple).ptr_level )
                    info.advance();
                    hasParentheses=true;
                }

                int head_before_error = info.gethead();

                bool prev_ignore = ignoreErrors;
                bool prev_show = showErrors;
                ignoreErrors = true;
                showErrors = false;

                ASTExpression* left=nullptr;
                SignalIO signal = SIGNAL_NO_MATCH;
                
                if(hasParentheses)
                    signal = parseExpression(left);

                ignoreErrors = prev_ignore;
                showErrors   = prev_show;

                if (signal == SIGNAL_SUCCESS) {
                    tmp->left = left;
                    Assert(left);
                } else {
                    info.sethead(head_before_error);
                    auto signal = parseTypeId(tmp->name);
                    if (signal != SIGNAL_SUCCESS) {
                        // TODO: Save error messages from parseExpression and print them here.
                        ERR_SECTION(
                            ERR_HEAD2(tmp->location)
                            ERR_MSG("Cannot parse type in sizeof/nameof/typeid. If it was supposed to be an expression, wrap it in parenthesis 'sizeof(*struct.ptr)'. It it was a type, then it has incorrect syntax.")
                            ERR_LINE2(tmp->location, "here")
                        )
                        return SIGNAL_COMPLETE_FAILURE;
                    }
                    // log::out << "parsed " << tmp->name << "\n";
                }

                // tmp->constantValue = true;// 

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
                ASTExpression* tmp = info.ast->createExpression(TypeId(AST_ASM));
                tmp->location = info.getloc();
                info.advance();

                // tmp->tokenRange.firstToken = token;

                // TODO: asm<i32>{...} casting
                
                auto tok = info.gettok();
                if(tok.type == '<') {
                    info.advance();
                    
                    std::string type{};
                    int n=0;
                    auto signal = parseTypeId(type, &n);
                    SIGNAL_INVALID_DATATYPE(type)
                    
                    auto tok = info.gettok();
                    if(tok.type != '>') {
                        ERR_SECTION(
                            ERR_HEAD2(tok)
                            ERR_MSG("ASM keyword (inline assembly) syntax is incorrect. There is a missing '>' or perhaps the syntax of the type is incorrect.")
                            ERR_LINE2(tok,"should be >")
                            ERR_EXAMPLE(1,"asm<i32*> {...}")
                            ERR_EXAMPLE(1,"asm {...} // no type can be done with this")
                        )
                        return SIGNAL_COMPLETE_FAILURE;
                    }
                    info.advance();
                    
                    tmp->asmTypeString = info.ast->getTypeString(type);
                }
                
                tok = info.gettok();
                if(tok.type == '(') {
                    info.advance();
                    
                    while(true) {
                        auto tok = info.gettok();
                        if(tok.type == ')') {
                            info.advance();
                            break;
                        }
                        ASTExpression* arg = nullptr;
                        auto signal = parseExpression(arg);
                        switch(signal) {
                            case SIGNAL_SUCCESS: break;
                            default: return SIGNAL_COMPLETE_FAILURE;
                        }
                        
                        tmp->args.add(arg);
                        
                        tok = info.gettok();
                        if(tok.type == ',') {
                            info.advance();
                            continue;
                        } else if(tok.type == ')') {
                            continue;   
                        } else {
                            ERR_SECTION(
                                ERR_HEAD2(tok)
                                ERR_MSG("Expression list for inline assembly did not end with comma or parenthesis. Perhaps the expression was mistyped?")
                                ERR_LINE2(tok,"expected , or )")
                            )
                            return SIGNAL_COMPLETE_FAILURE;
                        }
                    }
                }
                
                auto curly = info.gettok();
                if(curly.type != '{') {
                    ERR_SECTION(
                        ERR_HEAD2(curly)
                        ERR_MSG("ASM keyword (inline assembly) requires a body with curly braces after it.")
                        ERR_LINE2(curly,"should be }")
                        ERR_EXAMPLE(1,"asm {...}")
                        ERR_EXAMPLE(1,"asm<i32> {...} // or if you want a type")
                    )
                    continue;
                }
                info.advance();
                tmp->asm_range.importId = info.import_id;
                tmp->asm_range.token_index_start = info.gethead();
                
                int depth = 1;
                while(true){
                    auto token = info.getinfo();
                    if(token->type == lexer::TOKEN_EOF){
                        auto tok = info.gettok();
                        ERR_SUDDEN_EOF(tok, "Missing ending curly brace for inline assembly.", "asm block starts here")
                        return SIGNAL_COMPLETE_FAILURE;
                    }
                    if(token->type == '{') {
                        depth++;
                        continue;
                    }
                    if(token->type == '}') {
                        depth--;
                        if(depth==0) {
                            tmp->asm_range.token_index_end = info.gethead(); // exclusive
                            info.advance();
                            break;
                        }
                    }
                    // TODO: We want to search for instructions where variables are used like this
                    //   mov eax, [var] and change the instruction to one that gets the variable.
                    //   For this we need to:
                    //     - Parse the referenced variables
                    //     - Find out where they are and how to access them (rbp-24). (has to happen in bytecode generator)
                    //     - Replace the variable names with [rbp - 16] or whatever you use to access them. (has to happen in x64 gen)
                    info.advance();
                }
                values.add(tmp);
            } else if(try_inferred_initializer && token->type == '{') {
                // nocheckin, FIGURE THIS OUT
                // NOTE: Code is copied from the other AST_INITIALIZER of the form 'Struct{}'
                //   You should refactor this, duplicated code is bad.
                ASTExpression* initExpr = info.ast->createExpression(TypeId(AST_INITIALIZER));
                initExpr->location = info.getloc();
                info.advance();
                // NOTE: Type checker works on TypeIds not AST_FROM_NAMESPACE.
                //  AST_FROM... is used for functions and variables.
                
                initExpr->castType = {};
                
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
                    auto signal = parseExpression(expr);
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
                    if(tok.type == ','){
                        info.advance();
                        continue;
                    }else if(tok.type == '}'){
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
            } else if(token->type == lexer::TOKEN_IDENTIFIER){
                auto loc = info.getloc();
                info.advance();
                // int startToken=info.gethead();
                
                // log::out << view << "\n";
                
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
                    auto signal = parseTypeId(polyTypes);
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
                //   (some time later)
                //   Yes, function parsing is now broken. - Emarioo, 2024-04-05
                // BREAK(pure_name == "name")
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
                            saved_locations.pop();
                        } else {
                            break;
                        }
                    }
                    ns += pure_name;
                    ns += polyTypes;

                    ASTExpression* tmp = info.ast->createExpression(TypeId(AST_FNCALL));
                    tmp->location = loc;
                    // tmp->name = pure_name + polyTypes;
                    tmp->name = ns;

                    int count = 0;
                    auto signal = parseArguments(tmp, &count);
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
                // } else if(tok.type=='{' && 0 == (token->flags & lexer::TOKEN_FLAG_NEWLINE)){
                    // initializer
                    ASTExpression* initExpr = info.ast->createExpression(TypeId(AST_INITIALIZER));
                    initExpr->location = info.getloc();
                    info.advance();
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
                            saved_locations.pop();
                        } else {
                            break;
                        }
                    }
                    ns += pure_name;
                    ns += polyTypes;
                    initExpr->castType = info.ast->getTypeString(ns);
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
                        auto signal = parseExpression(expr);
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
                        if(tok.type == ','){
                            info.advance();
                            continue;
                        }else if(tok.type == '}'){
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
                    //         ERR_LINE2(token->tokenIndex,"bad");
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
                        tmp->location = loc;
                        // info.advance();

                        std::string nsToken = "";
                        int nsOps = 0;
                        while(ops.size()>0){
                            OperationType op = ops.last();
                            if(op == AST_FROM_NAMESPACE) {
                                nsOps++;
                                ops.pop();
                                saved_locations.pop();
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
                                ERR_MSG("Why have you done 'id<type>'? Are you trying to make a polymorphic variable or what? Don't do that.")
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
                // parse again
                info.advance();
                ASTExpression* tmp=0;
                auto signal = parseExpression(tmp);
                SIGNAL_SWITCH_LAZY()
                
                auto tok = info.gettok();
                if(tok.type != ')') {
                    ERR_SECTION(
                        ERR_HEAD2(tok)
                        ERR_MSG("Expected ) not "<<info.lexer->tostring(tok)<<".")
                        ERR_LINE2(tok,"bad")
                    )
                } else
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
            bool is_post = false;
            lexer::SourceLocation loc_op{};
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
                        loc_op = saved_locations[saved_locations.size() - 2];
                        saved_locations.pop();
                    } else {
                        break;
                    }
                } else {
                    // if(IsSingleOp(op2)) {
                    //     nowOp = op2;
                    //     ops.pop();
                        
                    //     loc_op = saved_locations.last();
                    //     saved_locations.pop();
                    // } else 
                    if(OpPrecedence(op1) >= OpPrecedence(op2)){ // this code produces this: 1 = 3-1-1
                    // if(OpPrecedence(op1)>OpPrecedence(op2)){ // this code cause this: 3 = 3-1-1
                        nowOp = op1;
                        ops[ops.size()-2] = op2;
                        ops.pop();
                        
                        loc_op = saved_locations[saved_locations.size() - 2];
                        saved_locations.pop();
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
                
                loc_op = saved_locations.last();
                saved_locations.pop();
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
                // val->tokenRange.firstToken = er->tokenRange.firstToken;
                // val->tokenRange.endIndex = er->tokenRange.endIndex;
                // val->tokenRange = er->tokenRange;
                if(nowOp == AST_FROM_NAMESPACE){
                    auto tok = namespaceNames.last();
                    val->name = tok;
                    // val->tokenRange.firstToken = tok;
                    // val->tokenRange.startIndex -= 2; // -{namespace name} -{::}
                    namespaceNames.pop();
                    
                    // val->location = saved_locations.last();
                    // saved_locations.pop();
                } else if(nowOp == AST_CAST){
                    val->castType = castTypes.last();
                    castTypes.pop();
                    auto extraToken = extraTokens.last();
                    extraTokens.pop();
                    auto str = info.lexer->getStdStringFromToken(extraToken);
                    if(str == "cast_unsafe") {
                        val->setUnsafeCast(true);
                    }
                    // val->location = saved_locations.last();
                    // saved_locations.pop();
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
                if(nowOp == AST_DEREF && (er->typeId == AST_INCREMENT || er->typeId == AST_DECREMENT) && er->postAction) {
                    // We swap deref and post increment so that
                    // *a++ is evaluated as (*a)++ instead of *(a++)
                    val->location = loc_op;
                    val->left = er->left;
                    er->left = val;
                    val = er;
                } else {
                    val->location = loc_op;
                    val->left = er;
                    if(val->typeId != AST_REFER) {
                        // val->constantValue = er->constantValue;// 
                    }
                }
            } else if(values.size()>0){
                val->location = loc_op;

                auto el = values.last();
                values.pop();
                // val->tokenRange.firstToken = el->tokenRange.firstToken;
                // val->tokenRange.startIndex = el->tokenRange.startIndex;
                // val->tokenRange.endIndex = er->tokenRange.endIndex;
                val->left = el;
                val->right = er;

                if(nowOp==AST_ASSIGN){
                    Assert(assignOps.size()>0);
                    val->assignOpType = assignOps.last();
                    assignOps.pop();
                }
                // val->constantValue = val->left->constantValue && val-// >right->constantValue;
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
            // expression->computeWhenPossible = shouldComputeExpression;// 
            if(!info.hasAnyErrors()) {
                Assert(values.size()==1);
            }
            Assert(saved_locations.size() == 0);

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

SignalIO ParseContext::parseFlow(ASTStatement*& statement){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _PLOG(FUNC_ENTER)
    
    // TODO: It would be smart if you could bit mask the token type to
    //   know whether the token is a flow keyword or not.

    // StringView view{};
    // auto token = info.getinfo(&view); // used for "test"
    auto token = info.getinfo();
    
    if(token->type >= lexer::TOKEN_KEYWORD_FLOW_BEGIN && token->type <= lexer::TOKEN_KEYWORD_FLOW_END) {
        if(token->type == lexer::TOKEN_IF){
            auto tok_if = info.gettok();
            info.advance();

            ASTExpression* expr=nullptr;
            auto signal = parseExpression(expr);

            SIGNAL_SWITCH_LAZY()

            // int endIndex = info.at()+1;

            ASTScope* body=nullptr;
            ParseFlags parsed_flags = PARSE_NO_FLAGS;
            signal = parseBody(body, info.currentScopeId, PARSE_NO_FLAGS, &parsed_flags);

            SIGNAL_SWITCH_LAZY()

            ASTScope* elseBody=nullptr;
            auto tok = info.gettok();
            if(tok.type == lexer::TOKEN_ELSE){
                if (0 == (tok.flags & lexer::TOKEN_FLAG_NEWLINE) && info.gettok(1).type != lexer::TOKEN_IF && info.gettok(1).type != '{') {
                    /* We want to catch the mistake below where 'if' is forgotten after else.
                        if true
                            ok
                        else 1 < 5
                            ok
                    However, we still want to allow this:
                        if true
                            ok
                        else Assert(false)
                    */
                    // ERR_SECTION(
                    //     ERR_HEAD2(tok)
                    //     ERR_MSG("Possible mistake! The else has a statement")
                    //     ERR_LINE2(tok, "did you mean else if?")
                    // )
                }
                info.advance();
                signal = parseBody(elseBody, info.currentScopeId);

                SIGNAL_SWITCH_LAZY()
            } else {
                // ERR_LINE2(expr->location, "")
            }

            statement = info.ast->createStatement(ASTStatement::IF);
            statement->firstExpression = expr;
            statement->firstBody = body;
            statement->secondBody = elseBody;
            statement->location = info.srcloc(tok_if);
            // statement->tokenRange.firstToken = firstToken;
            
            // statement->tokenRange.endIndex = info.at()+1;
            // statement->tokenRange.endIndex = endIndex;

            if(!(parsed_flags&PARSE_HAS_CURLIES) && body->statements.size()>0) {
                if (body->statements[0]->type == ASTStatement::IF) {
                    ERR_SECTION(
                        ERR_HEAD2(body->statements[0]->location, ERROR_AMBIGUOUS_IF_ELSE)
                        ERR_MSG("Nested if statements without curly braces may be ambiguous with else statements and is therefore not allowed. Use curly braces on the \"highest\" if statement.")
                        ERR_LINE2(statement->location, "use curly braces here")
                        ERR_LINE2(body->statements[0]->location, "ambiguous when using else")
                    )
                }
            }
            
            return SIGNAL_SUCCESS;
        } else if(token->type == lexer::TOKEN_WHILE){
            info.advance();

            ASTExpression* expr=nullptr;
            auto tok = info.gettok();
            if(tok.type == '{') {
                // no expression, infinite loop
            } else {
                auto signal = parseExpression(expr);
                SIGNAL_SWITCH_LAZY()
            }

            // int endIndex = info.at()+1;

            info.functionScopes.last().loopScopes.add({});
            ASTScope* body=0;
            auto signal = parseBody(body, info.currentScopeId);
            info.functionScopes.last().loopScopes.pop();
            SIGNAL_SWITCH_LAZY()

            statement = info.ast->createStatement(ASTStatement::WHILE);
            statement->firstExpression = expr;
            statement->firstBody = body;
            // statement->tokenRange.firstToken = firstToken;
            
            // statement->tokenRange.endIndex = endIndex;
            
            return SIGNAL_SUCCESS;
        } else if(token->type == lexer::TOKEN_FOR){
            info.advance();
            
            StringView view{};
            token = info.getinfo(&view);
            bool reverseAnnotation = false;
            bool pointerAnnot = false;
            while (token->type == lexer::TOKEN_ANNOTATION){
                if(view == "reverse" ||view == "rev"){
                    reverseAnnotation=true;
                } else if(view == "pointer" || view == "ptr"){
                    pointerAnnot=true;
                } else {
                    auto tok = info.gettok();
                    ERR_SECTION(
                        ERR_HEAD2(tok)
                        ERR_MSG("Unknown annotation for for loops.")
                        ERR_LINE2(tok,"unknown")
                    )
                }
                info.advance();
                token = info.getinfo(&view);
            }

            statement = info.ast->createStatement(ASTStatement::FOR);
            statement->varnames.reserve(2);
            statement->varnames.add({""}); // default names it and nr are set in typechecker
            statement->varnames.add({""}); // varname0 isn't necessarily 'it', with a ranged loop, varname0 is nr
            
            StringView view_nr{};
            
            auto tok1 = info.gettok(1);
            auto tok2 = info.gettok(&view_nr, 2);
            auto tok3 = info.gettok(3);
            
            if(token->type == lexer::TOKEN_IDENTIFIER && tok1.type == ':'){
                info.advance(2);
                statement->varnames[0].name = view;
            } else if(token->type == lexer::TOKEN_IDENTIFIER && tok1.type == ',' && tok1.type == lexer::TOKEN_IDENTIFIER && tok3.type == ':'){
                info.advance(4);
                statement->varnames[0].name = view;
                statement->varnames[1].name = view_nr;
            }
            
            ASTExpression* expr=0;
            auto signal = parseExpression(expr);
            SIGNAL_SWITCH_LAZY()

            ASTScope* body=0;
            info.functionScopes.last().loopScopes.add({});
            signal = parseBody(body, info.currentScopeId);
            info.functionScopes.last().loopScopes.pop();
            SIGNAL_SWITCH_LAZY()

            statement->setReverse(reverseAnnotation);
            statement->setPointer(pointerAnnot);
            statement->firstExpression = expr;
            statement->firstBody = body;
            return SIGNAL_SUCCESS;
        } else if(token->type == lexer::TOKEN_RETURN){
            info.advance();
            
            statement = info.ast->createStatement(ASTStatement::RETURN);
            
            /* Here's the thing, a user may type return with return values on a new line.
                We cannot assume the user wanted to return 0 arguments if we see a newline.
                So we must force the user to type a semi-colon so we know what they want.
                return
                    value0,
                    value2;
                
            */
            // if((token->flags & lexer::TOKEN_FLAG_NEWLINE) == 0) {
            token = info.getinfo();
            if(token->type != '}') {
                // token = info.getinfo();
                WHILE_TRUE {
                    ASTExpression* expr=0;
                    auto tok = info.gettok();
                    if(tok.type == ';' && statement->arrayValues.size()==0){ // return 1,; should not be allowed, that's why we do &&!base
                        info.advance();
                        break;
                    }
                    auto signal = parseExpression(expr);
                    SIGNAL_SWITCH_LAZY()
                    statement->arrayValues.add(expr);
                    
                    tok = info.gettok();
                    if(tok.type != ','){
                        if(tok.type == ';')
                            info.advance();
                        break;   
                    }
                    info.advance();
                }
            }
            return SIGNAL_SUCCESS;
        } else if(token->type == lexer::TOKEN_SWITCH){
            info.advance();
            ASTExpression* switchExpression=0;
            auto signal = parseExpression(switchExpression);
            SIGNAL_SWITCH_LAZY()
            
            auto tok = info.gettok();
            if(tok.type != '{') {
                ERR_DEFAULT(tok, "Switch expect { after the expression.", "not this")
                return SIGNAL_COMPLETE_FAILURE;
            }
            info.advance(); // {
            
            statement = info.ast->createStatement(ASTStatement::SWITCH);
            statement->firstExpression = switchExpression;
            // statement->tokenRange.firstToken = firstToken;
            
            lexer::Token prev_defaultCase{};
            
            bool errorsWasIgnored = info.ignoreErrors;
            defer {
                info.ignoreErrors = errorsWasIgnored;
            };
            
            bool mayRevertError = false;
            
            StringView view{};
            while(true) {
                auto token = info.getinfo(&view);
                if(token->type == lexer::TOKEN_EOF) {
                    Assert(false);
                    break;
                }
                if(token->type == '}') {
                    info.advance();
                    break;
                }
                if(token->type == ';') {
                    info.advance();
                    continue;
                }
                
                if(token->type == lexer::TOKEN_ANNOTATION) {
                    if(view == "TEST_ERROR") {
                        info.ignoreErrors = true;
                        if(mayRevertError)
                            info.errors--;
                        statement->setNoCode(true);
                        info.advance();
                        
                        // TokenRange args;
                        // auto res = ParseAnnotationArguments(info, &args);
                        auto signal = parseAnnotationArguments(nullptr);

                        continue;
                    }else {
                        // TODO: ERR annotation not supported   
                    }
                }
                mayRevertError = false;
                
                bool badDefault = false;
                if(token->type == lexer::TOKEN_IDENTIFIER && view == "default") { // NOTE: This will break if default becomes a keyword
                    auto tok = info.gettok();
                    badDefault = true;
                    mayRevertError = info.ignoreErrors == 0;
                    ERR_SECTION(
                        ERR_HEAD2(tok,ERROR_C_STYLED_DEFAULT_CASE)
                        ERR_MSG("Write 'case: { ... }' to specify default case. Do not use the word 'default'.")
                        ERR_LINE2(tok,"bad")
                    )
                    // info.advance();
                    // return SignalAttempt::FAILURE;
                } else if(!(token->type == lexer::TOKEN_IDENTIFIER && view == "case")) { // TODO: case should perhaps be a keyword
                    auto tok = info.gettok();
                    mayRevertError = info.ignoreErrors == 0;
                    ERR_SECTION(
                        ERR_HEAD2(tok,ERROR_BAD_TOKEN_IN_SWITCH)
                        ERR_MSG("'"<<info.lexer->tostring(tok)<<"' is not allowed in switch statement where 'case' is supposed to be.")
                        ERR_LINE2(tok,"bad")
                    )
                    info.advance();
                    continue;
                    // return SignalAttempt::FAILURE;
                }
                
                info.advance(); // case
                
                bool defaultCase = false;
                auto tok_colon = info.gettok();
                if(tok_colon.type == ':') {
                    auto tok = info.gettok(-1);
                    if(prev_defaultCase.type != 0 && !badDefault){
                        mayRevertError = info.ignoreErrors == 0;
                        ERR_SECTION(
                            ERR_HEAD2(tok,ERROR_DUPLICATE_DEFAULT_CASE)
                            ERR_MSG("You cannot have two default cases in a switch statement.")
                            ERR_LINE2(prev_defaultCase,"previous")
                            ERR_LINE2(tok,"another")
                        )
                    } else {
                        prev_defaultCase = tok;
                    }
                    
                    defaultCase = true;
                    info.advance();
                }
                
                ASTExpression* caseExpression=nullptr;
                if(!defaultCase){
                    auto signal = parseExpression(caseExpression);
                    SIGNAL_SWITCH_LAZY()
                    auto tok_colon = info.gettok();
                    if(tok_colon.type != ':') {
                        ERR_DEFAULT(tok_colon, "Unexepected token in case. ':' was expected.","here")
                        
                        return SIGNAL_COMPLETE_FAILURE;
                    }
                    info.advance(); // :
                }
                
                auto token2 = info.getinfo(&view);
                if(token2->type == lexer::TOKEN_ANNOTATION) {
                    if(view == "TEST_ERROR") {
                        info.ignoreErrors = true;
                        if(mayRevertError)
                            info.errors--;
                        statement->setNoCode(true);
                        info.advance();
                        
                        auto signal = parseAnnotationArguments(nullptr);
                        
                    } else {
                        // TODO: ERR annotation not supported   
                    }
                    //  else if(Equal(token2, "@no_code")) {
                    //     noCode = true;
                    //     info.advance();
                    // }
                }

                auto tok = gettok(&view);

                ParseFlags parsed_flags = PARSE_NO_FLAGS;
                ASTScope* caseBody=nullptr;
                // if (tok.type == '}' || (tok.type == lexer::TOKEN_IDENTIFIER && view == "case")) {
                //     // no body
                // } else {
                    auto signal = parseBody(caseBody, info.currentScopeId, (ParseFlags)(PARSE_INSIDE_SWITCH | PARSE_SKIP_ENTRY_BRACES), &parsed_flags);
                    SIGNAL_SWITCH_LAZY()
                // }
                
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
            
            // statement->tokenRange.endIndex = info.at()+1;
            
            return SIGNAL_SUCCESS;
        } else if(token->type == lexer::TOKEN_DEFER){
            auto loc = info.getloc();
            info.advance();

            if(is_inside_try_block) {
                ERR_SECTION(
                    ERR_HEAD2(loc)
                    ERR_MSG("defer statements are not allowed in try-catch blocks. This is to prevent mistakes, when programming you may expect a defer to be called when it isn't. Altough, if you have an exception such as null pointer dereference, you have more important things to worry about than the thing defer would have cleaned up.")
                    ERR_LINE2(loc, "here")
                )
                return SIGNAL_FAILURE;
            }

            ASTScope* body = nullptr;
            auto signal = parseBody(body, info.currentScopeId);
            SIGNAL_SWITCH_LAZY()

            statement = info.ast->createStatement(ASTStatement::DEFER);
            statement->firstBody = body;

            return SIGNAL_SUCCESS;  
        } else if(token->type == lexer::TOKEN_BREAK) {
            info.advance();
            statement = info.ast->createStatement(ASTStatement::BREAK);
            return SIGNAL_SUCCESS;
        } else if(token->type == lexer::TOKEN_CONTINUE) {
            info.advance();
            statement = info.ast->createStatement(ASTStatement::CONTINUE);
            return SIGNAL_SUCCESS;  
        }
    } else  if(token->type == lexer::TOKEN_USING){
        info.advance();
        Assert(false);
        // variable name
        // namespacing
        // type with polymorphism. not pointers
        // TODO: namespace environemnt, not just using X as Y

        // Token originToken={};
        // SignalDefault result = ParseTypeId(info, originToken, nullptr);
        // Assert(result == SIGNAL_SUCCESS);
        // Token aliasToken = {};
        // Token token = info.gettok();
        // if(Equal(token,"as")) {
        //     info.advance();
            
        //     SignalDefault result = ParseTypeId(info, aliasToken, nullptr);
        // }
        
        // statement = info.ast->createStatement(ASTStatement::USING);
        
        // statement->varnames.add({originToken});
        // if(aliasToken.str){
        //     statement->alias = TRACK_ALLOC(std::string);
        //     new(statement->alias)std::string(aliasToken);
        // }

        // statement->tokenRange.firstToken = firstToken;
        // statement->tokenRange.endIndex = info.at()+1;
        return SIGNAL_SUCCESS;        
    } 
    else if(token->type == lexer::TOKEN_TEST) {
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

        auto signal = parseExpression(statement->testValue);
        SIGNAL_SWITCH_LAZY()

        auto tok = info.gettok();
        if(tok.type == ';')
            info.advance();
        
        signal = parseExpression(statement->firstExpression);
        SIGNAL_SWITCH_LAZY()

        // statement->tokenRange.firstToken = firstToken;
        // statement->tokenRange.endIndex = info.at()+1;

        tok = info.gettok();
        if(tok.type == ';')
            info.advance();
        
        return SIGNAL_SUCCESS;  
    } else if(token->type == lexer::TOKEN_TRY) {
        auto loc = info.getloc();
        info.advance();

        if(compiler->options->target != TARGET_WINDOWS_x64) {
            ERR_SECTION(
                ERR_HEAD2(loc)
                ERR_MSG_COLORED("Try-catch is not supported on "<<log::LIME << ToString(compiler->options->target) <<log::NO_COLOR<<". Use sigaction instead. In the future we will rethink try-catch because Windows has support for it (Structured Exception Handling) while Linux handles exceptions differently with signals. The language will support platform independent exception handling.")
                ERR_LINE2(loc, "here")
            ) 
        }
        
        statement = info.ast->createStatement(ASTStatement::TRY);

        // if(is_inside_try_block) {
        //     ERR_SECTION(
        //         ERR_HEAD2(loc)
        //         ERR_MSG("You cannot have nested try-catch blocks. The exception handler is very simple and cannot determine wheter the inner or outer catch should be executed. (should be fixed in the future)")
        //         ERR_LINE2(loc, "here")
        //     )
        //     return SIGNAL_FAILURE;
        // }

        bool prev_inside_try = is_inside_try_block;
        is_inside_try_block = true;
        auto signal = parseBody(statement->firstBody, currentScopeId);
        is_inside_try_block = prev_inside_try;

        if(signal != SIGNAL_SUCCESS) 
            return signal;

        while(true) {
            auto tok = info.gettok();
            if(tok.type != lexer::TOKEN_CATCH)
                break;
            info.advance();

            ASTStatement::SwitchCase catch_part{};

            StringView varname{};
            auto tok_first = info.gettok(&varname);
            auto tok_second = info.gettok(1);
            if(tok_first.type == lexer::TOKEN_IDENTIFIER && tok_second.type == ':') {
                catch_part.location = info.getloc();
                catch_part.catch_exception_name = varname;
                info.advance(2);
            }

            signal = parseExpression(catch_part.caseExpr);
            if(signal != SIGNAL_SUCCESS)
                return signal;

            signal = parseBody(catch_part.caseBody, currentScopeId);
            if(signal != SIGNAL_SUCCESS)
                return signal;

            statement->switchCases.add(catch_part);
        }

        auto tok = info.gettok();
        if(tok.type == lexer::TOKEN_FINALLY){
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Exception handling (try-catch) does not support 'finally' (termination handler). Might be implemented in  the future.")
                ERR_LINE2(tok, "here")
            )

            info.advance();
            signal = parseBody(statement->secondBody, currentScopeId);
            if(signal != SIGNAL_SUCCESS)
                return signal;
        }

        return SIGNAL_SUCCESS;  
    }
    return SIGNAL_NO_MATCH;
}
SignalIO ParseContext::parseFunction(ASTFunction*& function, ASTStruct* parentStruct, bool is_operator){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _PLOG(FUNC_ENTER)

    function = info.ast->createFunction();
    // function->tokenRange.firstToken = fnToken;
    // function->tokenRange.endIndex = info.at()+1;;

    bool needsExplicitCallConvention = false;
    bool specifiedConvention = false;
    // Token linkToken{};

    bool is_entry_point = false;
    bool is_export = false;

    lexer::Token tok_name{};
    if(!is_operator) {
        StringView view_fn_name{};
        auto token_name = info.getinfo(&view_fn_name);
        tok_name = info.gettok();
        while (token_name->type == lexer::TOKEN_ANNOTATION){
            if(view_fn_name == "hide"){
                function->setHidden(true);
            } else if (view_fn_name == STR_STATIC_IMPORT || view_fn_name == STR_DYNAMIC_IMPORT || view_fn_name == "import" || view_fn_name == "export"){
                // Implicitly specify convention
                if(!specifiedConvention) {
                    if(info.compiler->options->target == TARGET_WINDOWS_x64) {
                        specifiedConvention = true;
                        function->callConvention = CallConvention::STDCALL;
                    } else if(info.compiler->options->target == TARGET_LINUX_x64) {
                        specifiedConvention = true;
                        function->callConvention = CallConvention::UNIXCALL;
                    } else Assert(false);
                }
                
                if (view_fn_name == STR_DYNAMIC_IMPORT) {
                    function->linkConvention = LinkConvention::DYNAMIC_IMPORT;
                } else if (view_fn_name == STR_STATIC_IMPORT){
                    function->linkConvention = LinkConvention::STATIC_IMPORT;
                } else if (view_fn_name == "import"){
                    function->linkConvention = LinkConvention::IMPORT;
                } else if (view_fn_name == "export"){
                    is_export = true;
                }

                info.advance();
                
                StringView token_str{};
                auto token = info.getinfo(&token_str);
                if(token->type == '(') {
                    info.advance();
                    while(true) {
                        token = info.getinfo(&token_str);
                        auto tok = info.gettok();
                        if(token->type == lexer::TOKEN_EOF) {
                            ERR_SECTION(
                                ERR_HEAD2(tok)
                                ERR_MSG("Sudden end of file.")
                                ERR_LINE2(tok,"here")
                            )
                            return SIGNAL_COMPLETE_FAILURE;
                        }
                        if(token->type == ')') {
                            info.advance();
                            break;
                        }
                        auto token2 = info.getinfo(1);
                        if(token->type == lexer::TOKEN_IDENTIFIER && token2->type == '=') {
                            if(token_str == "alias") {
                                info.advance(2);
                                token = info.getinfo(&token_str);
                                
                                if(token->type == lexer::TOKEN_LITERAL_STRING) {
                                    info.advance();
                                    if (view_fn_name == "export") {
                                        function->export_alias = token_str;
                                    } else {
                                        function->linked_alias = token_str;
                                    }
                                } else {
                                    ERR_SECTION(
                                        ERR_HEAD2(tok)
                                        ERR_MSG("Expected a string.")
                                        ERR_LINE2(tok,"here")
                                    )
                                    return SIGNAL_COMPLETE_FAILURE;
                                }
                            } else {
                                if (view_fn_name == "export") {
                                    ERR_SECTION(
                                        ERR_HEAD2(tok)
                                        ERR_MSG("Annotation for export can be supplied with an alias. '"<<token_str<<"' is not an attribute that can be set.")
                                        ERR_LINE2(tok,"here")
                                        ERR_EXAMPLE(1,"@export(alias = \"_verbose_func\")")
                                    )
                                } else {
                                    ERR_SECTION(
                                        ERR_HEAD2(tok)
                                        ERR_MSG("Annotation for import can be supplied with a named library and optionally an alias. '"<<token_str<<"' is not an attribute that can be set.")
                                        ERR_LINE2(tok,"here")
                                        ERR_EXAMPLE(1,"@import(yourlib, alias = \"_verbose_func\")")
                                    )
                                }
                                return SIGNAL_COMPLETE_FAILURE;
                            }
                        } else if(token->type == lexer::TOKEN_IDENTIFIER) {
                            info.advance();
                            if (view_fn_name == "export") {

                            }else {
                                function->linked_library = token_str;
                            }
                        } else {
                            ERR_SECTION(
                                ERR_HEAD2(tok)
                                ERR_MSG("Unexpected token in annotation for @import. Library name and 'alias = \"string\"' is okay.")
                                ERR_LINE2(tok,"here")
                                ERR_EXAMPLE(1,"@import(yourlib, alias = \"_verbose_func\")")
                            )
                            return SIGNAL_COMPLETE_FAILURE;
                        }

                        token = info.getinfo();
                        if(token->type == ',') {
                            info.advance();
                            continue;
                        } else if(token->type == ')') {
                            info.advance();
                            break;
                        } else{
                            ERR_SECTION(
                                ERR_HEAD2(tok)
                                ERR_MSG("Unexpected token.")
                                ERR_LINE2(tok,"here")
                            )
                            info.advance();
                            continue;
                        }
                    }
                }

                info.advance(-1);
                
                needsExplicitCallConvention = true;
            } else if (view_fn_name == "stdcall"){
                function->callConvention = CallConvention::STDCALL;
                specifiedConvention = true;
            // } else if (Equal(name,"@cdecl")){
            // cdecl has not been implemented. It doesn't seem important.
            // default x64 calling convention is used.
            //     Assert(false); // not implemented in type checker and generator. type checker might work as is.
            //     function->callConvention = CallConvention::CDECL_CONVENTION;
            //     specifiedConvention = true;
            } else if (view_fn_name == "betcall"){
                function->callConvention = CallConvention::BETCALL;
                specifiedConvention = true;
            // IMPORTANT: When adding calling convention, do not forget to add it to the "Did you mean" below!
            } else if (view_fn_name == "unixcall"){
                function->callConvention = CallConvention::UNIXCALL;
                specifiedConvention = true;
            } else if (view_fn_name == "oscall"){
                if (compiler->options->target == TARGET_WINDOWS_x64) {
                    function->callConvention = CallConvention::STDCALL;
                } else if (compiler->options->target == TARGET_LINUX_x64) {
                    function->callConvention = CallConvention::UNIXCALL;
                }
                specifiedConvention = true;
            } else if (view_fn_name == "native"){
                function->linkConvention = NATIVE;
            } else if (view_fn_name == "intrinsic"){
                function->callConvention = CallConvention::INTRINSIC;
                // function->linkConvention = NATIVE;
            } else if (view_fn_name == "blank"){
                function->blank_body = true;
            } else if (view_fn_name == "entry"){
                is_entry_point = true;
            } else if (view_fn_name == "compiler"){
                function->is_compiler_func = true;
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
            token_name = info.getinfo(&view_fn_name);
            tok_name = info.gettok();
            continue;
        }
        if(function->linkConvention != LinkConvention::NONE){
            // function->setHidden(true);
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
        info.advance();
    } else {
        StringView view{};
        auto token = info.getinfo(&view);
        while (token->type == lexer::TOKEN_ANNOTATION){
            auto tok = info.gettok();
            info.advance();
            if(view == "hide"){
                function->setHidden(true);
            } else {
                // It should not warn you because it is quite important that you use the right annotations with functions
                // Mispelling external or the calling convention would be very bad.
                ERR_SECTION(
                    ERR_HEAD2(tok)
                    ERR_MSG("Unknown annotation for operators")
                    ERR_LINE2(tok,"unknown")
                )
            }
            token = info.getinfo(&view);
            continue;
        }
        
        OperationType op = (OperationType)0;
        int extraNext = 0;
        function->is_operator = true;
        
        // TODO: Improve parsing here. If the token is an operator but not allowed
        //  then you can continue parsing, it's just that the function won't be
        //  added to the tree and be usable. Parse and then throw away.
        //  Stuff following the operator overload´syntax will then parse fine.
        
        std::string name = "";

        tok_name = info.gettok();
        auto token0 = info.getinfo(&view);
        auto token1 = info.getinfo(nullptr,1);
        // What about +=, -=?
        if(token0->type == lexer::TOKEN_IDENTIFIER && view == "cast") {
            info.advance();
            name = "cast";
        } else if(token0->type == '[' && token1->type == ']') {
            info.advance(2);
            name = "[]";
        } else if(!(op = parser::IsOp(token0, token1, view, extraNext))){
            info.ast->destroy(function);
            function = nullptr;
            ERR_SECTION(
                ERR_HEAD2(tok_name)
                ERR_MSG("Operator overloading requires an operator as the name such as '+' or '[]'.")
                ERR_LINE2(tok_name, "not an operator")
            )
            return SIGNAL_COMPLETE_FAILURE;
        } else {
            info.advance(1 + extraNext); // next is done above
            name = OP_NAME(op);
        }
        if(op == AST_ASSIGN) {
            ERR_DEFAULT(tok_name, "Assign operator is not allowed for operator overloading", "not allowed")
            // for now?
        }

        function->name = name;
    }

    // log::out << "begin " <<function->name<<"\n";

    if (is_export) {
        if(function->export_alias.size() == 0)
            function->export_alias = function->name;
    }

    function->location = info.srcloc(tok_name);

    if (is_entry_point) {
        if (compiler->entry_point != "main") {
            ERR_SECTION(
                ERR_HEAD2(function->location)
                ERR_MSG("You can only specify entry point (@entry) once but it is specified at least twice. Remove the @entry from one of the functions.")
                ERR_LINE2(compiler->location_of_entry_point, "previous entry point")
                ERR_LINE2(function->location, "new entry point")
            )
        }
        compiler->entry_point = function->name;
        compiler->location_of_entry_point = function->location;
    }

    ScopeInfo* funcScope = info.ast->createScope(info.currentScopeId, info.getNextOrder(), nullptr);
    funcScope->is_function_scope = true;
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
            auto signal = parseTypeId(datatype);

            SIGNAL_INVALID_DATATYPE(datatype)

            function->polyArgs.add({datatype});
            function->polyArgs.last().location = info.srcloc(tok);

            tok = info.gettok();
            if (tok.type == ',') {
                info.advance();
                continue;
            } else if(tok.type == '>') {
                info.advance();
                break;
            } else {
                if(is_operator) {
                    ERR_SECTION(
                        ERR_HEAD2(tok)
                        ERR_MSG("Unexpected token for polymorphic function arguments.")
                        ERR_LINE2(tok,"bad token")
                    )
                } else {
                    ERR_SECTION(
                        ERR_HEAD2(tok)
                        ERR_MSG("Unexpected token for polymorphic operator overload arguments.")
                        ERR_LINE2(tok,"bad token")
                    )
                }
                return SIGNAL_COMPLETE_FAILURE;
            }
        }
    }
    
    tok = info.gettok();
    if(tok.type != '('){
        if(is_operator) {
            ERR_SECTION(
                ERR_HEAD2(tok_name)
                ERR_MSG("Operator overload expects a parentheses after the operator type.")
                ERR_LINE2(tok_name,"this function")
            )
        } else {
            ERR_SECTION(
                ERR_HEAD2(tok_name)
                ERR_MSG("Function expects a parentheses after the name.")
                ERR_LINE2(tok_name,"this function")
            )
        }
        return SIGNAL_COMPLETE_FAILURE;
    }

    Assert(!is_operator || !parentStruct); // operator cannot have a parent struct, for now
    if(parentStruct) {
        function->arguments.add({});
        auto& argv = function->arguments[function->arguments.size()-1];
        argv.name = "this";
        // TODO: add source location for this?
        // argv.name.tokenStream = info.tokens; // feed and print won't work if we set these, they think this comes from the stream and tries to print it
        // argv.name.tokenIndex = tok.tokenIndex;
        // argv.name.line = tok.line;
        // argv.name.column = tok.column+1;
        argv.defaultValue = nullptr;
        function->nonDefaults++;

        std::string str = parentStruct->name + "*"; // TODO: doesn't work with polymorhpism?
        argv.stringType = info.ast->getTypeString(str);;
    }
    info.advance(); // skip (
    bool printedErrors=false;
    bool mustHaveDefault=false;
    // TokenRange prevDefault={};
    WHILE_TRUE {
        StringView view_arg{};
        auto arg = info.getinfo(&view_arg);
        auto arg_tok = info.gettok();
        if(arg->type == ')'){
            info.advance();
            break;
        }
        if(arg->type != lexer::TOKEN_IDENTIFIER){
            auto tok = info.gettok();
            info.advance();
            if(!printedErrors) {
                printedErrors=true;
                if(is_operator) {
                    ERR_SECTION(
                        ERR_HEAD2(tok)
                        ERR_MSG("Invalid name for operator overload parameter. A valid name may contain letters and digits ('a-Z', '_', '0-9') where the first character cannot be a digit.")
                        ERR_LINE2(tok,"bad name")
                    )
                } else {
                    ERR_SECTION(
                        ERR_HEAD2(tok)
                        ERR_MSG("Invalid name for function parameter.  A valid name may contain letters and digits ('a-Z', '_', '0-9') where the first character cannot be a digit.")
                        ERR_LINE2(tok,"bad name")
                    )
                }
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
        auto signal = parseTypeId(dataType);

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
        argv.location = info.srcloc(arg_tok);;

        ASTExpression* defaultValue=nullptr;
        tok = info.gettok();
        if(tok.type == '='){
            info.advance();

            if(is_operator) {
                // NOTE: Default arguments may actually be fine. It's just that you can't
                //   specify the value of them unless you use the automatic argument passing
                //   feature which isn't implemented yet.
                // ERR_SECTION(
                //     ERR_HEAD2(tok)
                //     ERR_MSG("Operator overloading cannot have default arguments.")
                //     ERR_LINE2(tok,"not allowed")
                // )
                // This is a semantic error so we can continue parsing just fine.
            }
            
            auto signal = parseExpression(defaultValue);
            SIGNAL_SWITCH_LAZY()

            // prevDefault = defaultValue->tokenRange;

            mustHaveDefault=true;
        } else if(mustHaveDefault){
            // printedErrors doesn't matter here.
            // If we end up here than our parsing is probably correct so far and we might as
            // well log this error since it's probably a "real" error not caused by a cascade.
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Expected a default argument because of previous default argument "<<"? at ?"<<".")
                // ERR_MSG("Expected a default argument because of previous default argument "<<prevDefault<<" at "<<prevDefault.firstToken.line<<":"<<prevDefault.firstToken.column<<".")
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
            if(tok.type == '{' || tok.type == ';'){
                break;   
            }
            
            std::string datatype{};
            auto signal = parseTypeId(datatype);

            SIGNAL_INVALID_DATATYPE(datatype)

            TypeId strId = info.ast->getTypeString(datatype);
            function->returnValues.add({});
            function->returnValues.last().stringType = strId;
            function->returnValues.last().location = info.srcloc(tok);
            // function->returnValues.last().valueToken = datatype;
            // function->returnValues.last().valueToken.endIndex = info.at()+1;
            printedErrors = false;
            
            tok = info.gettok();
            if(tok.type == '{' || tok.type == ';'){
                // info.advance(); { is parsed in ParseBody
                break;   
            } else if(tok.type == ','){
                info.advance();
                continue;
            } else if (!function->needsBody()) {
                break;
            } else {
                if(function->linkConvention != LinkConvention::NONE)
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
        if(is_operator) {
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Operator overloading must have a body!")
                ERR_LINE2(tok,"replace with {}")
            )
        } else {
            if(function->needsBody()){
                ERR_SECTION(
                    ERR_HEAD2(tok)
                    ERR_MSG("Functions must have a body unless they are imported. Forward declarations are not necessary.")
                    ERR_LINE2(tok,"replace with {}")
                )
            }
        }
    } else if(tok.type == '{'){
        if(!function->needsBody()) {
            Assert(!is_operator);
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Native/external functions cannot have a body. Native functions are handled by the language. External functions link to functions outside your source code.")
                ERR_LINE2(tok,"use ; instead")
            )
        }
        
        info.functionScopes.add({});
        info.functionScopes.last().function = function;
        ASTScope* body = 0;
        auto signal = parseBody(body, function->scopeId);
        function->body = body;
        info.functionScopes.pop();
        SIGNAL_SWITCH_LAZY()

    } else {
        if(is_operator) {
            ERR_SECTION(
                ERR_HEAD2(info.gettok(-1))
                ERR_MSG("Operator has no body! Did the return types parse incorrectly? Use curly braces to define the body.")
                ERR_LINE2(info.gettok(-1),"expected '{' afterwards")
            )
        } else {
            if(function->needsBody()) {
                ERR_SECTION(
                    ERR_HEAD2(info.gettok(-1))
                    ERR_MSG("Function has no body! Did the return types parse incorrectly? Use curly braces to define the body.")
                    ERR_LINE2(info.gettok(-1),"expected '{' afterwards")
                )
            }
        }
    }
    // log::out << function->name<<" "<<function->body<<"\n";
    return SIGNAL_SUCCESS;
}

SignalIO ParseContext::parseDeclaration(ASTStatement*& statement){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _PLOG(FUNC_ENTER)

    bool globalDeclaration = false;

    StringView view{};
    auto token0 = info.getinfo(&view);

    if(token0->type == lexer::TOKEN_IDENTIFIER && view == "global") {
        globalDeclaration = true;
        info.advance();
    }

    statement = info.ast->createStatement(ASTStatement::DECLARATION);
    statement->firstExpression = nullptr;
    statement->globalDeclaration = globalDeclaration;

    auto token_name = info.getinfo(&view);
    // tok_name = info.gettok();
    while (token_name->type == lexer::TOKEN_ANNOTATION){
        
        if(view == "notstable") {
            info.advance();
            statement->is_notstable = true;
        } else if (view == "import" || view == STR_DYNAMIC_IMPORT || view == STR_STATIC_IMPORT || view == "export"){
            auto loc = info.getloc();
            info.advance();

            if (view == STR_DYNAMIC_IMPORT) {
                statement->linkConvention = LinkConvention::DYNAMIC_IMPORT;
            } else if (view == STR_STATIC_IMPORT){
                statement->linkConvention = LinkConvention::STATIC_IMPORT;
            } else if (view == "import"){
                statement->linkConvention = LinkConvention::IMPORT;
            } else if (view == "export"){
                // is_export = true;
                ERR_SECTION(
                    ERR_HEAD2(loc)
                    ERR_MSG("@export is not supported yet.")
                    ERR_LINE2(loc, "here")
                )
                return SIGNAL_COMPLETE_FAILURE;
            }
            
            StringView token_str{};
            auto token = info.getinfo(&token_str);
            if(token->type == '(') {
                info.advance();
                while(true) {
                    token = info.getinfo(&token_str);
                    auto tok = info.gettok();
                    if(token->type == lexer::TOKEN_EOF) {
                        ERR_SECTION(
                            ERR_HEAD2(tok)
                            ERR_MSG("Sudden end of file.")
                            ERR_LINE2(tok,"here")
                        )
                        return SIGNAL_COMPLETE_FAILURE;
                    }
                    if(token->type == ')') {
                        info.advance();
                        break;
                    }
                    auto token2 = info.getinfo(1);
                    if(token->type == lexer::TOKEN_IDENTIFIER && token2->type == '=') {
                        if(token_str == "alias") {
                            info.advance(2);
                            token = info.getinfo(&token_str);
                            
                            if(token->type == lexer::TOKEN_LITERAL_STRING) {
                                info.advance();
                                statement->linked_alias = token_str;
                            } else {
                                ERR_SECTION(
                                    ERR_HEAD2(tok)
                                    ERR_MSG("Expected a string.")
                                    ERR_LINE2(tok,"here")
                                )
                                return SIGNAL_COMPLETE_FAILURE;
                            }
                        } else {
                            ERR_SECTION(
                                ERR_HEAD2(tok)
                                ERR_MSG("Annotation for import can be supplied with a named library and optionally an alias. '"<<token_str<<"' is not an attribute that can be set.")
                                ERR_LINE2(tok,"here")
                                ERR_EXAMPLE(1,"@import(yourlib, alias = \"_verbose_func\")")
                            )
                            return SIGNAL_COMPLETE_FAILURE;
                        }
                    } else if(token->type == lexer::TOKEN_IDENTIFIER) {
                        info.advance();
                        statement->linked_library = token_str;
                    } else {
                        ERR_SECTION(
                            ERR_HEAD2(tok)
                            ERR_MSG("Unexpected token in annotation for @import. Library name and 'alias = \"string\"' is okay.")
                            ERR_LINE2(tok,"here")
                            ERR_EXAMPLE(1,"@import(yourlib, alias = \"_verbose_func\")")
                        )
                        return SIGNAL_COMPLETE_FAILURE;
                    }

                    token = info.getinfo();
                    if(token->type == ',') {
                        info.advance();
                        continue;
                    } else if(token->type == ')') {
                        info.advance();
                        break;
                    } else{
                        ERR_SECTION(
                            ERR_HEAD2(tok)
                            ERR_MSG("Unexpected token.")
                            ERR_LINE2(tok,"here")
                        )
                        info.advance();
                        continue;
                    }
                }
            }
        } else {
            auto tok = info.gettok();
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Unknown annotation for variables.")
                ERR_LINE2(tok,"unknown")
            )
            info.advance(); // prevent infinite loop
            return SIGNAL_COMPLETE_FAILURE;
        }
        // info.advance();
        token_name = info.getinfo(&view);
        continue;
    }

    token0 = info.getinfo(&view);
    auto token1 = info.getinfo(1);
    if(token0->type != lexer::TOKEN_IDENTIFIER) {
        // early exit
        return SIGNAL_NO_MATCH;
    } else if (token1->type == ',' || token1->type == ':') {
        // Note that 'a, b: i32;' is a variable declaration.
        // But 'a, 34, 9' may be some form of syntax for a list or something else in the future.
    } else {
        // early exit
        return SIGNAL_NO_MATCH;
    }
    // at this point we can't 100% sure it's a declaration in case ',' represents a list of some sort.
    // It doesn't right now but maybe in the future. - Emarioo, 2024-04-20

    lexer::Token lengthTokenOfLastVar{};

    bool dynamic_array_length = false;

    //-- Evaluate variables on the left side
    // int startIndex = info.at()+1;
    // DynamicArray<ASTStatement::VarName> varnames{};
    while(true){
        StringView view{};
        auto token = info.getinfo(&view);
        auto var_tok = info.gettok();
        if(token->type != lexer::TOKEN_IDENTIFIER){
            auto tok = info.gettok();
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Expected a valid name for assignment. A name may contain letters and digits (and underscore). However, the first character cannot be a digit.")
                ERR_LINE2(tok,"not a valid name");
            )
            return SIGNAL_COMPLETE_FAILURE;
        }
        info.advance();

        statement->varnames.add({view});
        statement->varnames.last().location = info.srcloc(var_tok);
        auto tok = info.gettok();
        if(tok.type == ':'){
            info.advance(); // :
            // statement->varnames.last().declaration = true;

            tok = info.gettok();
            if(tok.type == '=') {
                int index = statement->varnames.size()-1;
                while(index>=0 && !statement->varnames[index].declaration){
                    statement->varnames[index].declaration = true;
                    index--;
                }
            } else {
                std::string typeToken{};
                auto signal = parseTypeId(typeToken, nullptr);
                
                // TODO: Show error of two lines in case user typed:
                /*  hello:
                    }       <- only this line will be seen
                */

                
                SIGNAL_INVALID_DATATYPE(typeToken)

                // Assert(result==SIGNAL_SUCCESS);
                int arrayLength = -1;
                StringView view_int{};
                auto tok0 = info.gettok();
                auto tok1 = info.gettok(&view_int,1);
                auto tok2 = info.gettok(2);
                if(tok0.type == '[' && tok2.type == ']') {
                    info.advance(3);

                    if(statement->varnames.size() > 1) {
                        // TODO: We could allow this if the array is declared last. We don't
                        //   since the type checker and generator doesn't handle it. Look into it?
                        ERR_SECTION(
                            ERR_HEAD2(tok0)
                            ERR_MSG("Array declarations must be standalone. You can specify multiple variables in a declaration.")
                            ERR_LINE2(tok1, "remove array declaration or remove variable declarations")
                            ERR_EXAMPLE(1, "a, b: i32[4] // NOT OKAY")
                            ERR_EXAMPLE(2, "a: i32, b: i32[4] // NOT OKAY")
                            ERR_EXAMPLE(3, "b: i32[4] // Very good!")
                        )
                        return SIGNAL_COMPLETE_FAILURE;
                    }
                    // TODO: Handle hexidecimal
                    if(tok1.type == lexer::TOKEN_LITERAL_INTEGER) {
                        lengthTokenOfLastVar = tok1;
                        arrayLength = lexer::ConvertInteger(view_int);
                        if(arrayLength<0){
                            ERR_SECTION(
                                ERR_HEAD2(tok1)
                                ERR_MSG("Array cannot have negative size.")
                                ERR_LINE2(tok1,"< 0")
                            )
                            arrayLength = 0;
                        }
                    } else if(tok1.type == '.') {
                        lengthTokenOfLastVar = tok1;
                        arrayLength = 0;
                        dynamic_array_length = true;
                    } else {
                        ERR_SECTION(
                            ERR_HEAD2(tok1)
                            ERR_MSG("The length of an array can only be specified with number literals. Use macros to avoid magic numbers. Constants have not been implemented but when they have, they will work too.")
                            ERR_LINE2(tok1, "must be positive integer literal")
                        )
                    }
                    typeToken = "Slice<" + typeToken + ">";
                } else if(typeToken.substr(0,6) == "Slice<" && tok0.type == '{') {
                    // info.advance(); DO NOT ADVANCE '{'
                    arrayLength = 0;
                    dynamic_array_length = true;
                }

                TypeId strId = info.ast->getTypeString(typeToken);

                int index = statement->varnames.size()-1;
                while(index>=0 && !statement->varnames[index].assignString.isString()){
                    statement->varnames[index].declaration = true;
                    statement->varnames[index].assignString = strId;
                    statement->varnames[index].arrayLength = arrayLength;
                    index--;
                }

                if(arrayLength != -1) {
                    tok = info.gettok();
                    if(tok.type == ','){
                        ERR_SECTION(
                            ERR_HEAD2(tok)
                            ERR_MSG("Multiple declarations is not allowed with an array declaration. The comma indicates more variable declarations (or assignments) which isn't supported.")
                            ERR_LINE2(tok, "bad comma!")
                            ERR_EXAMPLE(2, "b: i32[4], c: i32 // NOT OKAY")
                            ERR_EXAMPLE(3, "b: i32[4] // Very good!")
                        )
                        return SIGNAL_COMPLETE_FAILURE;
                    }
                    break;
                }
            }
        } else {
            auto tok = info.gettok();
            if(globalDeclaration) {
                ERR_SECTION(
                    ERR_HEAD2(tok)
                    ERR_MSG("Global variables must use declaration syntax. You must use a ':' (':=' if you want the type to be inferred).")
                    ERR_LINE2(tok, "add a colon before this")
                )
            }
        }
        tok = info.gettok();
        if(tok.type == ','){
            info.advance();
            continue;
        }
        break;
    }
    
    auto prev_tok = info.gettok(-1);
    auto tok = info.gettok();
    if(tok.type == '=') {
        info.advance(); // =

        // Assert(!allow_inferred_initializers);
        // allow_inferred_initializers = true;
        auto signal = parseExpression(statement->firstExpression);
        // allow_inferred_initializers = false;

        SIGNAL_SWITCH_LAZY()
    // } else if(tok.type == '{' && 0 == (prev_tok.flags & lexer::TOKEN_FLAG_ANY_SUFFIX)) {
    } else if(tok.type == '{' && 0 == (prev_tok.flags & lexer::TOKEN_FLAG_NEWLINE)) {
        // array initializer
        info.advance(); // {

        while(true){
            auto tok = info.gettok();
            if(tok.type == '}') {
                info.advance(); // }
                break;
            }
            ASTExpression* expr = nullptr;
            auto signal = parseExpression(expr);
            SIGNAL_SWITCH_LAZY()
            
            Assert(expr);
            statement->arrayValues.add(expr);

            tok = info.gettok();
            if(tok.type == ',') {
                info.advance(); // ,
                // TODO: Error if you see consecutive commas
                // Note that a trailing comma is allowed: { 1, 2, }
                // It's convenient
            } else if(tok.type == '}') {
                info.advance(); // }
                break;
            } else {
                info.advance(); // prevent infinite loop
                ERR_SECTION(
                    ERR_HEAD2(tok)
                    ERR_MSG("Unexpected token '"<<info.lexer->tostring(tok)<<"' at end of array initializer. Use comma for another element or ending curly brace to end initializer.")
                    ERR_LINE2(tok, "expected , or }")
                )
            }
        }
        if(dynamic_array_length){
            // Set array length based on expressions
            statement->varnames.last().arrayLength = statement->arrayValues.size();
        }
        if(statement->arrayValues.size() > statement->varnames.last().arrayLength) {
            ERR_SECTION(
                ERR_HEAD2(tok) // token should be {
                ERR_MSG("You cannot have more expressions in the array initializer than the array length you specified.")
                // TODO: Show which token defined the array length
                ERR_LINE2(lengthTokenOfLastVar, "the maximum length")
                // You could do a token range from the first expression to the last but that could spam the console
                // with 100 expressions which would be annoying so maybe show 5 or 8 values and then do ...
                ERR_LINE2(tok, ""<<statement->arrayValues.size()<<" expressions")
            )
        }
    }
    tok = info.gettok();
    if(tok.type == ';'){
        info.advance(); // parse ';'. won't crash if at end
    }

    // statement->tokenRange.firstToken = info.get(startIndex);
    // statement->tokenRange.endIndex = info.at()+1;
    return SIGNAL_SUCCESS;  
}
SignalIO ParseContext::parseBody(ASTScope*& bodyLoc, ScopeId parentScope, ParseFlags in_flags, ParseFlags* out_flags){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
    // Note: two infos in case ParseDeclaration modifies it and then fails.
    //  without two, ParseCommand would work with a modified info.
    _PLOG(FUNC_ENTER)
    // _PLOG(ENTER)

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

        if ((in_flags & PARSE_SKIP_ENTRY_BRACES) == 0) {
            auto token = info.getinfo();
            if(token->type == '{') {
                info.advance();
                expectEndingCurlyBrace = true;
                if(out_flags)
                    *out_flags = (ParseFlags)(*out_flags|PARSE_HAS_CURLIES);
                inheritScope = false;
            }
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

    StringView view{};
    auto token = info.getinfo(&view);
    while(token->type == lexer::TOKEN_ANNOTATION) {
        if(view == "dumpasm") {
            info.advance();
            bodyLoc->flags |= ASTNode::DUMP_ASM;
        } else if(view ==  "dumpbc") {
            info.advance();
            bodyLoc->flags |= ASTNode::DUMP_BC;
        } else {
            // Annotation may belong to statement so we don't
            // complain about not recognizing it here.
            break;
        }
        token = info.getinfo(&view);
    }
    
    
    bool errorsWasIgnored = info.ignoreErrors;
    defer {
        info.ignoreErrors = errorsWasIgnored;
    };

    DynamicArray<ASTStatement*> nearDefers{};

    // Code to keep track of which defers to insert before return statements
    functionScopes.last().anyScopes.add({});
    functionScopes.last().anyScopes.last().defer_size = functionScopes.last().defers.size();
    defer {
        functionScopes.last().defers.resize(functionScopes.last().anyScopes.last().defer_size);
        functionScopes.last().anyScopes.pop();
    };

    while(true){
        auto token = info.getinfo(&view);
        if(token->type == lexer::TOKEN_EOF) {
            if(expectEndingCurlyBrace) {
                if (functionScopes.size()) {
                    auto func = functionScopes.last().function;
                    auto loc = getloc();
                    ERR_SECTION(
                        ERR_HEAD2(loc)
                        ERR_MSG("Sudden end of file. A curly brace was expected to end the scope. The scope here belongs to function '"<<func->name<<"'. Did you forget a curly brace in this function?")
                        ERR_LINE2(loc, "sudden end here")
                        ERR_LINE2(func->location, "this function")
                    )
                }else {
                    ERR_DEFAULT(info.gettok(), "Sudden end of body. You are missing an ending curly brace '}'.", "here")
                }
                return SIGNAL_COMPLETE_FAILURE;
            }
            break;
        }
        
        if(token->type == '}'){
            if(expectEndingCurlyBrace)
                info.advance();
            break;
        }
        if((in_flags & PARSE_INSIDE_SWITCH)) {
            if(token->type == lexer::TOKEN_IDENTIFIER && (view == "case" || view == "default")) {
                Assert(in_flags & PARSE_SKIP_ENTRY_BRACES);
                break;
            }
            if(token->type == lexer::TOKEN_ANNOTATION && view == "fall") {
                info.advance();
                if(out_flags)
                    *out_flags = (ParseFlags)(*out_flags | PARSE_HAS_CASE_FALL);
                continue;
            }
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
        
        bool noCode = false;
        bool log_nodeid = false;
        while(token->type == lexer::TOKEN_ANNOTATION) {
            if(view == "TEST_ERROR") {
                info.ignoreErrors = true;
                noCode = true;
                info.advance();
                
                auto signal = parseAnnotationArguments(nullptr);
                SIGNAL_SWITCH_LAZY()
            } else if(view == "TEST_CASE") {
                info.advance();

                auto signal = parseAnnotationArguments(nullptr);
                SIGNAL_SWITCH_LAZY()
            } else if(view == "no_code") {
                noCode = true;
                info.advance();
            }  else if(view == "nodeid") {
                log_nodeid = true;
                info.advance();
            } else {
                // error
                break;
            }
            token = info.getinfo(&view);
        }

        lexer::Token stmt_loc = info.gettok();

        ASTNode* astNode = nullptr;
        SignalIO signal=SIGNAL_NO_MATCH;
        if(token->type == '{'){
            ASTScope* body=nullptr;
            signal = parseBody(body, info.currentScopeId);
            
            tempStatement = info.ast->createStatement(ASTStatement::BODY);
            tempStatement->firstBody = body;
            // tempStatement->tokenRange = body->tokenRange;
        } else if(token->type == lexer::TOKEN_STRUCT) {
            info.advance();
            signal = parseStruct(tempStruct);
            astNode = (ASTNode*)tempStruct;
            if(tempStruct)
                bodyLoc->add(info.ast, tempStruct);
            info.nextContentOrder.last()++;
        } else if(token->type == lexer::TOKEN_FUNCTION) {
            info.advance();
            signal = parseFunction(tempFunction, nullptr, false);
            astNode = (ASTNode*)tempFunction;
            if(tempFunction)
                bodyLoc->add(info.ast, tempFunction);
            info.nextContentOrder.last()++;
        } else if(token->type == lexer::TOKEN_OPERATOR) {
            info.advance();
            signal = parseFunction(tempFunction, nullptr, true);
            astNode = (ASTNode*)tempFunction;
            if(tempFunction)
                bodyLoc->add(info.ast, tempFunction);
            info.nextContentOrder.last()++;
        } else if(token->type == lexer::TOKEN_IDENTIFIER && view == "conversion") {
            info.advance();
            signal = parseFunction(tempFunction, nullptr, true);
            astNode = (ASTNode*)tempFunction;
            if(tempFunction)
                bodyLoc->add(info.ast, tempFunction);
            info.nextContentOrder.last()++;
        }  else if(token->type == lexer::TOKEN_ENUM) {
            info.advance();
            signal = parseEnum(tempEnum);
            astNode = (ASTNode*)tempEnum;
            if(tempEnum)
                bodyLoc->add(info.ast, tempEnum);
            info.nextContentOrder.last()++;
        } else if(token->type == lexer::TOKEN_NAMESPACE) {
            info.advance();
            signal = parseNamespace(tempNamespace);
            astNode = (ASTNode*)tempNamespace;
            if(tempNamespace)
                bodyLoc->add(info.ast, tempNamespace);
            info.nextContentOrder.last()++;
        }
        if(signal==SIGNAL_NO_MATCH)
            signal = parseFlow(tempStatement);
        if(signal==SIGNAL_NO_MATCH) {
            signal = parseDeclaration(tempStatement);
        }
        if(signal==SIGNAL_NO_MATCH){
            // bad name of function? it parses an expression
            // prop assignment or function call

            
            bool prev_assign = allow_assignments;
            allow_assignments = true;
            defer {
                allow_assignments = prev_assign;
            };
            
            ASTExpression* expr=nullptr;
            signal = parseExpression(expr);
            if(expr){
                tempStatement = info.ast->createStatement(ASTStatement::EXPRESSION);
                tempStatement->firstExpression = expr;
                // tempStatement->tokenRange = expr->tokenRange;
            }
        }
        // TODO: What about annotations here?
        switch(signal) {
        case SIGNAL_SUCCESS: break;
        case SIGNAL_NO_MATCH: {
            auto tok = info.gettok();
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Did not expect '"<<info.lexer->tostring(tok)<<"' when parsing body. A new statement, struct or function was expected (or enum, namespace, ...).")
                ERR_LINE2(tok,"what")
            )
            // prevent infinite loop. Loop 'only occurs when scoped
            info.advance();
            // fall through and signal complete failure
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
        if(tempStatement) {
            tempStatement->location = info.srcloc(stmt_loc);
            astNode = (ASTNode*)tempStatement;
            // TODO: Optimize by performing the logic for defer, return, break, continue in flow instead of here. That way, all statements don't have to perform these checks.
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
                        // deferCopy->tokenRange = myDefer->tokenRange;
                        deferCopy->firstBody = myDefer->firstBody;
                        deferCopy->sharedContents = true;
                        deferCopy->location = myDefer->location;
                        // TODO: Share flags with deferCopy?

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

        if(log_nodeid) {
            Assert(astNode);
            char buf[20];
            int len = snprintf(buf,sizeof(buf),"nodeid = %d",astNode->nodeId);
            Assert(len);
            log::out << log::YELLOW << "Annotation @nodeid: logging node id\n";
            // TokenStream* prevStream = nullptr;
            // PrintCode(astNode->tokenRange, buf, &prevStream);
            if(tempStatement == astNode && tempStatement->firstExpression) {
                int len = snprintf(buf,sizeof(buf),"nodeid = %d",tempStatement->firstExpression->nodeId);
                Assert(len);
                // PrintCode(tempStatement->firstExpression->tokenRange, buf, &prevStream);
            }
         }

        if(signal==SIGNAL_NO_MATCH){
            // Try again. Important because loop would break after one time if scoped is false.
            // We want to give it another go.
            // Hello! After the 2.1 refactor of the parser I don't know what
            // this continue is for. I mean, SIGNAL_NO_MATCH means token failed matching and we should return complete failure no?
            continue;
        }
        // skip semi-colon if it's there.
        auto tok = info.gettok();
        if(tok.type == ';'){
            info.advance();
        }
        // current body may have the same "parentScope == bodyLoc->scopeId" will make sure break 
        if(!expectEndingCurlyBrace && !(in_flags & (PARSE_TRULY_GLOBAL|PARSE_INSIDE_SWITCH))){
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

    return SIGNAL_SUCCESS;
}
ASTScope* ParseImport(u32 import_id, Compiler* compiler){
    using namespace engone;
    ZoneScopedC(tracy::Color::OrangeRed1);
    _VLOG(log::out <<log::BLUE<<  "##   Parser   ##\n";)
    
    ParseContext info{};
    info.compiler = compiler;
    info.lexer = &compiler->lexer;
    info.ast = compiler->ast;
    info.reporter = &compiler->reporter;
    info.import_id = import_id;

    info.setup_token_iterator();

    ASTScope* body = nullptr;
    // TODO: what to do about 'import path as namespace'
    // if(theNamespace.size()==0){
        body = info.ast->createBody();
        // body->scopeId = info.ast->globalScopeId;
        auto newScope = info.ast->createScope(info.ast->globalScopeId, CONTENT_ORDER_ZERO, body);
        body->scopeId = newScope->id;
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
    auto signal = info.parseBody(body, info.ast->globalScopeId, PARSE_TRULY_GLOBAL);
    info.functionScopes.pop();
    
    info.compiler->options->compileStats.errors += info.errors;
    
    return body;
}
}