#include "BetBat/Preprocessor.h"

// preprocessor needs CompileInfo to handle global caching of include streams
#include "BetBat/Compiler.h"

#ifdef OS_WINDOWS
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#undef WARN_HEAD3
#define WARN_HEAD3(T, M) info.warnings++;engone::log::out << WARN_CUSTOM(info.inTokens->streamName, T.line, T.column, "Preproc. warning","W0000") << M
#undef WARN_LINE2
#define WARN_LINE2(I, M) PrintCode(I, info.inTokens, M)

#undef ERR_SECTION
#define ERR_SECTION(CONTENT) BASE_SECTION2(CONTENT)

#define ERR_DEFAULT(T,STR,STR_LINE) ERR_SECTION(\
                ERR_HEAD2(T)\
                ERR_MSG(STR)\
                ERR_LINE2(T, STR_LINE)\
            )

namespace preproc {
/* #####################
    NEW PREPROCESSOR
#######################*/

SignalIO PreprocContext::parseMacroDefinition() {
    using namespace engone;
    ZoneScopedC(tracy::Color::Gold);

    bool multiline = false;
    
    lexer::Token name_token = gettok();
    
    if(name_token.type != lexer::TOKEN_IDENTIFIER){
        // TODO: Create new error report system (or finalize the started one)?
        //   We should do a complete failure here stopping preprocessing because
        //   everything else could be wrong. All imports which import the current
        //   will not be preprocessed or parsed minimizing meaningless errors.
        ERR_SECTION(
            ERR_HEAD2(name_token)
            ERR_MSG("'"<<lexer->tostring(name_token)<<"' is not a valid name for macros. All characters must be one of a-Z or 0-9 except for the first character which must be a-Z. No special characters like: $@?,:;.")
            ERR_LINE2(name_token, "bad");
        )
        return SIGNAL_COMPLETE_FAILURE;
    }
    
    advance();
    
    // TODO: Can we redefine macros? or should there be some scenario where
    //   we cannot? non-scoped macros perhaps.
    
    bool has_newline = name_token.flags & lexer::TOKEN_FLAG_NEWLINE;
    MacroSpecific localMacro{};
    
    if(name_token.flags&lexer::TOKEN_FLAG_ANY_SUFFIX){

    } else {
        lexer::Token token = gettok();
        if(token.type != '('){
            ERR_SECTION(
                ERR_HEAD2(token)
                ERR_MSG("'"<<lexer->tostring(token)<<"' is not allowed directly after a macro's name. Either use a '(' to specify macro arguments or a space to indicate no arguments and then the content of the macro.")
                ERR_LINE2(token,"bad");
            )
            return SIGNAL_COMPLETE_FAILURE;
        }
        advance();
        int hadError = false;
        while(true){
            lexer::Token token = gettok();
            if(token.type == ')'){
                advance();
                has_newline = token.flags&lexer::TOKEN_FLAG_NEWLINE;
                break;   
            }
            
            if(lexer->equals_identifier(token,"...")) { // TODO: Use token type instead of string ("...")
                if(!localMacro.isVariadic()) {
                    // log::out << " param: "<<token<<"\n";
                    localMacro.addParam(token, true);
                } else {
                    if(!hadError){
                        ERR_SECTION(
                            ERR_HEAD2(token)
                            ERR_MSG("Macros can only have 1 variadic argument.")
                            ERR_LINE2(localMacro.parameters[localMacro.indexOfVariadic], "previous")
                            ERR_LINE2(token, "not allowed")
                        )
                    }
                    
                    hadError=true;
                }
            } else if(token.type != lexer::TOKEN_IDENTIFIER){
                if(!hadError){
                    ERR_SECTION(
                        ERR_HEAD2(token)
                        ERR_MSG("'"<<lexer->tostring(token)<<"' is not a valid name for arguments. The first character must be an alpha (a-Z) letter, the rest can be alpha and numeric (0-9). '...' is also available to specify a variadic argument.")
                        ERR_LINE2(token, "bad");
                    )
                }
                hadError=true;
            } else {
                // log::out << " param: "<<token<<"\n";
                localMacro.addParam(token, false);
            }
            advance();
            token = gettok();
            if(token.type == ')'){
                continue; // logic handled at start of loop
            } else if(token.type == ','){
                advance();
                // cool
            } else{
                if(!hadError){
                    ERR_SECTION(
                        ERR_HEAD2(token)
                        ERR_MSG("'"<<lexer->tostring(token)<< "' is not okay. You use ',' to specify more arguments and ')' to finish parsing of argument.")
                        ERR_LINE2(token, "bad");
                    )
                    return SIGNAL_COMPLETE_FAILURE;
                }
                hadError = true;
            }
        }
        
        _MLOG(log::out << "loaded "<<localMacro.parameters.size()<<" arg names";
        if(!localMacro.isVariadic())
            log::out << "\n";
        else
            log::out << " (variadic)\n";)
    
    }
    if(has_newline) {
        multiline = true;
    }
    bool invalidContent = false;
    int startToken = gethead();
    int endToken = startToken;
    // if(multiline || 0==(suffixFlags&TOKEN_SUFFIX_LINE_FEED)) {
    while(true){
        lexer::Token token = gettok();
        advance();
        if(token.type == '#'){
            if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED)){
                continue;
                // ERR_SECTION(
                // ERR_HEAD2(token, "SPACE AFTER "<<token<<"!\n";
                // )
                // return SignalAttempt::FAILURE;
            }
            token = gettok();
            if(lexer->equals_identifier(token, "endmacro")){
                advance();
                endToken = gethead()-3;
                break;
            }
            if(lexer->equals_identifier(token, "macro")){
                ERR_SECTION(
                    ERR_HEAD2(token)
                    ERR_MSG("Macro definitions inside macros are not allowed.")
                    ERR_LINE2(token,"not allowed")
                )
                invalidContent = true;
            }
            //not end, continue
        }
        if(!multiline){
            if(token.flags&TOKEN_SUFFIX_LINE_FEED){
                endToken = gethead();
                break;
            }
        }
        if(token.type == lexer::TOKEN_EOF&&multiline){
            ERR_SECTION(
                ERR_HEAD2(gettok(-1))
                ERR_MSG("Missing '#endmacro' for macro '"<<lexer->tostring(name_token)<<"' ("<<(localMacro.isVariadic() ? "variadic" : std::to_string(localMacro.parameters.size()))<<" arguments)")
                ERR_LINE2(name_token, "this needs #endmacro somewhere")
                ERR_LINE2(gettok(-1),"macro content ends here!")
            )
        }
    }
    // }
    
    localMacro.content.importId = import_id;
    localMacro.content.token_index_start = startToken;
    if(!invalidContent){
        localMacro.content.token_index_end = endToken;
    } else {
        localMacro.content.token_index_end = startToken;
    }
    int count = endToken-startToken;
    int argc = localMacro.parameters.size();

    if(!evaluateTokens) {
        MacroRoot* rootMacro = preprocessor->create_or_get_macro(import_id, name_token, localMacro.isVariadic() && localMacro.parameters.size() > 1);
        
        preprocessor->insertCertainMacro(import_id, rootMacro, &localMacro);

        std::string name = lexer->getStdStringFromToken(name_token);
        
        _MLOG(log::out << log::LIME<< "#"<<"macro def. '"<<name<<"' ";
        if(argc!=0){
            log::out<< argc;
            if(argc==1) log::out << " arg, ";
            else log::out << " args, ";
        }
        log::out << count;
        // Are you proud of me? - Emarioo, 2023-0?-??
        // Yes - Emarioo, 2024-01-17
        if(count==1) log::out << " token\n";
        else log::out << " tokens\n";)
    }
    return SIGNAL_SUCCESS;
}

SignalIO PreprocContext::parseLink(){
    using namespace engone;
    // ZoneScopeC(tracy::Color::Wheat);

    lexer::Token name_token = gettok();
    if(name_token.type != lexer::TOKEN_LITERAL_STRING){
        ERR_SECTION(
            ERR_HEAD2(name_token)
            ERR_MSG("Expected a string not "<<lexer->tostring(name_token)<<".")
        )
        return SIGNAL_COMPLETE_FAILURE;
    }
    advance();
    
    if(evaluateTokens) {
        std::string name = lexer->getStdStringFromToken(name_token);
        preprocessor->compiler->addLinkDirective(name);
    }
    
    return SIGNAL_SUCCESS;
}
SignalIO PreprocContext::parseLoad(){
    using namespace engone;
    // ZoneScopeC(tracy::Color::Wheat);

    StringView path{};
    lexer::Token name_token = gettok(&path);
    if(name_token.type != lexer::TOKEN_LITERAL_STRING){
        ERR_SECTION(
            ERR_HEAD2(name_token)
            ERR_MSG("Expected a string not "<<lexer->tostring(name_token)<<".")
        )
        return SIGNAL_COMPLETE_FAILURE;
    }
    advance();

    StringView view_as{};
    bool has_as = false;
    lexer::Token tok = gettok(&view_as);
    if(tok.type == lexer::TOKEN_IDENTIFIER && view_as == "as") {
        advance();

        tok = gettok(&view_as);
        if(tok.type != lexer::TOKEN_IDENTIFIER) {
            ERR_SECTION(
                ERR_HEAD2(tok)
                ERR_MSG("Expected a string not "<<lexer->tostring(tok)<<".")
            )
            return SIGNAL_COMPLETE_FAILURE;
        } else {
            has_as = true;
            advance();
        }
    }
    
    if(evaluateTokens) {
        compiler->addLibrary(import_id, path, view_as);
    }
    
    return SIGNAL_SUCCESS;
}

SignalIO PreprocContext::parseIf(){
    using namespace engone;

    bool not_modifier=false,active=false,complete_inactive=false;
    
    // TODO: Parse condition as some expression (not ASTExpressions)
    lexer::Token tok = gettok();
    if(tok.type == '!') {
        not_modifier = true;
        advance();
        tok = gettok();
    }
    
    if(tok.type != lexer::TOKEN_IDENTIFIER) {
        Assert(false); // nocheckin, fix error   
    }
    advance();
    
    
    std::string name = lexer->getStdStringFromToken(tok);
    if(evaluateTokens) {
        active = preprocessor->matchMacro(import_id, name);
        if(not_modifier)
            active = !active;
        not_modifier = false; // reset, we use it later
        complete_inactive = false;
    }
    
    int depth = 0;
    
    _MLOG(log::out << log::GRAY<< "   #if "<<(not_modifier?"!":"")<<name<<"\n";)
    while(true){
        lexer::Token token = gettok();
        
        if(token.type == lexer::TOKEN_EOF) {
            ERR_SECTION(
                ERR_HEAD2(gettok(-1))
                ERR_MSG("Missing endif somewhere for ifdef or ifndef.")
            )
            return SIGNAL_COMPLETE_FAILURE;
        }
        
        if(token.type == '#' && !(token.flags & lexer::TOKEN_FLAG_ANY_SUFFIX)){
            token = gettok(1);
            // If !yes we can skip these tokens, otherwise we will have
            //  to skip them later which is unnecessary computation
            // if(lexer->equals_identifier(token, "if")){
            if(token.type == lexer::TOKEN_IF){
                if(!active){
                    // inactive means we skip current tokens
                    advance(); // hashtag
                    advance(); // if
                    depth++;
                    // log::out << log::GRAY<< "   ifdef - new depth "<<depth<<"\n";
                } else {
                    // evaluate later properly
                }
            } else if(lexer->equals_identifier(token, "elif")) {
                advance();// hashtag
                advance();// elif
                
                token = gettok();
                not_modifier = false;   
                if(token.type == '!') {
                    not_modifier = true;
                    advance();
                    token = gettok();
                }
                if(token.type != lexer::TOKEN_IDENTIFIER) {
                    ERR_SECTION(
                        ERR_HEAD2(token)
                        ERR_MSG("#elif expects a valid macro name after it same as #if.")
                        ERR_LINE2(token,"not a valid macro name")
                    )
                    return SIGNAL_COMPLETE_FAILURE;
                } else {
                    advance();
                    name = lexer->getStdStringFromToken(token);
                    if(!active) {
                        // if a previous if/elif matched we don't want to check this one
                        if(evaluateTokens){
                            active = preprocessor->matchMacro(import_id, name);
                            if(not_modifier)
                                active = !active;
                        }
                    } else {
                        complete_inactive = true;
                        active = false;
                    }
                }
                not_modifier = false; // reset for later
            } else if(lexer->equals_identifier(token,"endif")){
                if(depth==0){
                    advance();
                    advance();
                    _MLOG(log::out << log::GRAY<< "   endif - "<<name<<"\n";)
                    break;
                }
                // log::out << log::GRAY<< "   endif - new depth "<<depth<<"\n";
                depth--;
                continue;
            } else if(token.type == lexer::TOKEN_ELSE){ // we allow multiple elses, they toggle active and inactive sections
            // } else if(lexer->equals_identifier(token,"else")){ // we allow multiple elses, they toggle active and inactive sections
                if(depth==0){
                    advance();
                    advance();
                    if(!complete_inactive)
                        active = !active;
                    _MLOG(log::out << log::GRAY<< "   else - "<<name<<"\n";)
                    continue;
                }
            }
        }
        // TODO: what about errors?
        
        if(active){
            parseOne(); // nocheckin, is this right?
        }else{
            advance();
            // log::out << log::GRAY<<" skip "<<skip << "\n";
        }
    }
    //  log::out << "     exit  ifdef loop\n";
    return SIGNAL_SUCCESS;
}
SignalIO PreprocContext::parseImport() {
    
    StringView path;
    lexer::Token token = gettok(&path);
    if(token.type != lexer::TOKEN_LITERAL_STRING) {
        ERR_SECTION(
            ERR_HEAD2(token)
            ERR_MSG("Expected a string not "<<lexer->tostring(token)<<".")
        )
        return SIGNAL_COMPLETE_FAILURE;
    }
    advance();

    StringView view_as{};
    bool has_as = false;
    token = gettok(&view_as);
    if(token.type == lexer::TOKEN_IDENTIFIER && view_as == "as") {
        advance();

        token = gettok(&view_as);
        if(token.type != lexer::TOKEN_IDENTIFIER) {
            ERR_SECTION(
                ERR_HEAD2(token)
                ERR_MSG("Expected a string not "<<lexer->tostring(token)<<".")
            )
            return SIGNAL_COMPLETE_FAILURE;
        } else {
            has_as = true;
            advance();
        }
    }
    
    if(!evaluateTokens) {
        // only if not in conditional directive
        // std::string path = lexer->getStdStringFromToken(token);
        
        preprocessor->lock_imports.lock();
        auto imp = preprocessor->imports.get(import_id-1);
        preprocessor->lock_imports.unlock();
        
        auto lexer_imp = lexer->getImport_unsafe(import_id);
        std::string orig_dir = TrimLastFile(lexer_imp->path);
        
        u32 dep_id = compiler->addOrFindImport(path, orig_dir);
        
        if(dep_id == 0) {
            Assert(false); // nocheckin, fix error   
        } else {
            // preprocessor->lock_imports.lock();
            // preprocessor->imports.requestSpot(dep_id-1,nullptr);
            // preprocessor->lock_imports.unlock();
            
            imp->import_dependencies.add(dep_id);
            if(has_as)
                compiler->addDependency(import_id, dep_id, view_as);
            else
                compiler->addDependency(import_id, dep_id);
        }
    }
    return SIGNAL_SUCCESS;
}
SignalIO PreprocContext::parseMacroEvaluation() {
    ZoneScopedC(tracy::Color::Goldenrod2);
    
    if(!evaluateTokens) {
        advance(); // skip, is this okay?
        return SIGNAL_NO_MATCH;
    }
    
    lexer::Token macro_token = gettok();
    Assert(macro_token.type == lexer::TOKEN_IDENTIFIER);
    
    std::string macro_name = lexer->getStdStringFromToken(macro_token);
    MacroRoot* root = preprocessor->matchMacro(import_id,macro_name);
    if(!root)
        return SIGNAL_NO_MATCH;
    advance();
    
    typedef DynamicArray<lexer::Token> TokenList;
    
    // TODO: Optimize by not allocating tokens on the heap.
    //  Linear or some form of stack allocator would be better.
    //  Perhaps an array of arrays that you can reuse?
    
    u32 origin_import_id = import_id;
    
    struct Layer {
        Layer(bool eval_content) : eval_content(eval_content) {
            specific = nullptr;
            root = nullptr;
            if(eval_content) {
                new(&input_arguments)DynamicArray<TokenList>();
            } else {
                id = 0;
                caller = nullptr;
                paren_depth = 0;
            }
        }
        ~Layer() {
            if(eval_content) {
                input_arguments.~DynamicArray();
            } else {
                
            }
        }
        u32 _head = 0;
        u32 start_head = 0;
        u32* ref_head = nullptr;
        bool eval_content = true;
        MacroSpecific* specific;
        MacroRoot* root;
        union {
            struct {
                DynamicArray<TokenList> input_arguments;
            };
            struct {
                u32 id;
                Layer* caller;
                Layer* callee; // rename, can easily be confused
                int paren_depth;
            };
        };
        
        lexer::Token get(lexer::Lexer* lexer, int off = 0) {
            Assert(specific || id != 0);
            if(id!=0 && !eval_content) {
                u32 index = *ref_head + off;
                // if(index >= specific->content.token_index_end)
                //     return {lexer::TOKEN_EOF};
                return lexer->getTokenFromImport(id, *ref_head + off);
            } else {
                u32 index = specific->content.token_index_start + *ref_head + off;
                if(index >= specific->content.token_index_end)
                    return {lexer::TOKEN_EOF};
                return lexer->getTokenFromImport(specific->content.importId, specific->content.token_index_start + *ref_head + off);
            }
        }
        void step(int n = 1) {
            *ref_head+=n;
        }
        void sethead(u32 head) {
            _head = head;
            start_head = head;
            ref_head = &_head;
        }
        void sethead(u32* phead) {
            this->ref_head = phead;
            this->start_head = *phead;
        }
        u32 gethead() {
            return *ref_head;
        }
    };
    DynamicArray<Layer*> stack{};
    Layer* first = new Layer(true);
    stack.add(first);
    first->root = root;
    first->sethead((u32)0);
    
    lexer::Token tok = gettok();
    if(!(macro_token.flags & (lexer::TOKEN_FLAG_ANY_SUFFIX)) && tok.type == '(') {
        advance();
        Layer* second = new Layer(false);
        stack.add(second);
        second->callee = first;
        second->id = import_id;
        second->sethead(&head);
    }
    
    while(stack.size() != 0) {
        if(stack.size() >= PREPROC_REC_LIMIT) {
            // TODO: Better error message, maybe provide macro call stack with arguments?
            ERR_SECTION(
                ERR_HEAD2(macro_token)
                ERR_MSG("Recursion limit for macros reached! "<<PREPROC_REC_LIMIT<<" is the maximum.")
                ERR_LINE2(macro_token,"recursion began here")
            )
            return SIGNAL_COMPLETE_FAILURE;
        }
        
        Layer* layer = stack.last();
        if(!layer->eval_content) {
            
            lexer::Token token = layer->get(lexer);
            if(token.type == lexer::TOKEN_EOF) {
                Assert(false);
            }
            
            if(layer->paren_depth==0 && (token.type == ',' || token.type == ')')) {
                if(token.type == ',') {
                    layer->callee->input_arguments.add({});
                }
                
                if(token.type == ',') {
                    layer->step();
                    continue;
                }
                if(token.type == ')') {
                    layer->step();
                    delete layer;
                    stack.pop();
                    continue;
                }
                Assert(false);
            }
            
            if(token.type == '(') {
                layer->paren_depth++;
                layer->step();
                continue;
            } else if(token.type == ')') {
                layer->paren_depth--;
                layer->step();
                continue;
            }
            
            if(token.type == lexer::TOKEN_IDENTIFIER && layer->caller) { // we don't have a caller if it's arguments for the first layer
                MacroSpecific* caller_spec = layer->caller->specific;
                int param_index = caller_spec->matchArg(token,lexer);
                
                if(param_index!=-1) {
                    layer->step();
                    
                    int real_index = param_index;
                    int real_count = 1;
                    if(caller_spec->isVariadic()) {
                        if(param_index == caller_spec->indexOfVariadic) {
                            real_count = layer->caller->input_arguments.size() - caller_spec->nonVariadicArguments();
                        } else if(param_index > caller_spec->indexOfVariadic) {
                            real_index = param_index - 1 + layer->caller->input_arguments.size() - caller_spec->nonVariadicArguments();
                        }
                    }
                    
                    for(int i = real_index; i < real_index + real_count;i++) {
                        auto& list = layer->caller->input_arguments[i];
                        for(int j=0;j<list.size();j++) {
                            auto& token = list[j];
                            if(layer->callee->input_arguments.size() == 0)
                                layer->callee->input_arguments.add({});
                            layer->callee->input_arguments.last().add(token);
                        }
                        if(i != real_index + real_count - 1)
                            layer->callee->input_arguments.add({});
                    }
                    continue;
                }
            }
            
            layer->step();
            if(layer->callee->input_arguments.size() == 0)
                layer->callee->input_arguments.add({});
            layer->callee->input_arguments.last().add(token);
        } else {
            if(!layer->specific) {
                layer->specific = preprocessor->matchArgCount(layer->root, layer->input_arguments.size());
            
                if(!layer->specific) {
                    Assert(false); // nocheckin, fix error
                }
            }
        
            lexer::Token token = layer->get(lexer);
            if(token.type == lexer::TOKEN_EOF) {
                delete layer;
                stack.pop();
                continue;
            }
            
            if(token.type == lexer::TOKEN_IDENTIFIER) {
                int param_index = layer->specific->matchArg(token,lexer);
                if(param_index!=-1) {
                    layer->step();
                    
                    int real_index = param_index;
                    int real_count = 1;
                    if(layer->specific->isVariadic()) {
                        if(param_index == layer->specific->indexOfVariadic) {
                            real_count = layer->input_arguments.size() - layer->specific->nonVariadicArguments();
                        } else if(param_index > layer->specific->indexOfVariadic) {
                            real_index = param_index - 1 + layer->input_arguments.size() - layer->specific->nonVariadicArguments();
                        }
                    }
                    
                    for(int i = real_index; i < real_index + real_count;i++) {
                        auto& list = layer->input_arguments[i];
                        for(int j=0;j<list.size();j++) {
                            lexer->appendToken(new_lexer_import, list[j], true);
                        }
                        if(i+1 != real_index + real_count && list.size() != 0) {
                            // nocheckin, fix line and column.
                            lexer->appendToken_auto_source(new_lexer_import, (lexer::TokenType)',', (u32)lexer::TOKEN_FLAG_SPACE);
                            // lexer->appendToken(new_import_id, list[j]);
                        }
                    }
                    
                    continue;
                }
                std::string name = lexer->getStdStringFromToken(token);
                MacroRoot* macroroot = preprocessor->matchMacro(origin_import_id, name);
                if (macroroot) {
                    layer->step(); // hashtag
                    layer->step(); // name
                    
                    Layer* layer_macro = new Layer(true);
                    layer_macro->root = macroroot;
                    layer_macro->sethead((u32)0);
                    stack.add(layer_macro);
                    
                    token = layer->get(lexer);
                    if(token.type == '(') {
                        layer->step();
                        
                        Layer* layer_arg = new Layer(false);
                        layer_arg->caller = layer;
                        layer_arg->callee = layer_macro;
                        layer_arg->specific = layer->specific;
                        layer_arg->sethead(layer->ref_head);
                        
                        stack.add(layer_arg);
                    }
                    
                    continue;
                }
            }
            layer->step();
            lexer->appendToken(new_lexer_import, token, true);
        }
    }
    #undef gettok
    #undef advance
    return SIGNAL_SUCCESS;
}

SignalIO PreprocContext::parseOne() {
    StringView string{};
    // lexer::Token token = gettok();
    // auto token = getinfo(&string);
    auto tok = gettok(&string);
    auto token = &tok;
    if(token->type == lexer::TOKEN_EOF)
        return SIGNAL_COMPLETE_FAILURE;
    
    if(token->type != '#' || (token->flags & (lexer::TOKEN_FLAG_NEWLINE|lexer::TOKEN_FLAG_SPACE))) {
        if(evaluateTokens) {
            if(tok.type == lexer::TOKEN_IDENTIFIER) {
                auto signal = parseMacroEvaluation();
                if(signal == SIGNAL_SUCCESS) {
                    return SIGNAL_SUCCESS;
                }
            }
            lexer->appendToken(new_lexer_import, tok, &string);
        }
        advance();
        return SIGNAL_SUCCESS;
    }
    // handle directive
    advance(); // skip #
    
    // TODO: Create token types for non-user directives (#import, #include, #macro...)
    auto macro_token = getinfo(&string);
    auto macro_tok = gettok();
    if(macro_token->type == lexer::TOKEN_IDENTIFIER || macro_token->type == lexer::TOKEN_IF) {
        const char* str = string.ptr;
        u32 len = string.len; // lexer->getStringFromToken(token,&str);
        
        SignalIO signal=SIGNAL_NO_MATCH;
        if(macro_token->type == lexer::TOKEN_IF) {
            advance();
            signal = parseIf();
        } else if(!strcmp(str, "macro")) {
            advance();
            signal = parseMacroDefinition();
        } else if(!strcmp(str, "import")) {
            advance();
            signal = parseImport();
        } else if(!strcmp(str, "link")) {
            advance();
            signal = parseLink();
        } else if(!strcmp(str, "load")) {
            advance();
            signal = parseLoad();
        // } else if(!strcmp(str, "if")) {
        //     advance();
        //     signal = parseIf();
        } else {
            signal = parseMacroEvaluation();
            if(signal != SIGNAL_SUCCESS) {
                // the macro was not a macro
                if(evaluateTokens) {
                    lexer->appendToken(new_lexer_import, tok, nullptr); // hashtag
                    lexer->appendToken(new_lexer_import, macro_tok, &string);
                    advance();
                }
            }
        }
        switch(signal) {
        case SIGNAL_SUCCESS: break;
        case SIGNAL_COMPLETE_FAILURE: return SIGNAL_COMPLETE_FAILURE;
        default: Assert(false);
        }
    } else {
        advance();
        if(evaluateTokens) {
            lexer->appendToken(new_lexer_import, tok, &string);
        }
    }
    return SIGNAL_SUCCESS;
}
u32 Preprocessor::process(u32 import_id, bool phase2) {
    using namespace engone;
    ZoneScopedC(tracy::Color::Gold3);
 
    PreprocContext context{};
    context.lexer = lexer;
    context.preprocessor = this;
    context.compiler = compiler;
    context.info.reporter = &compiler->reporter;
    context.import_id = import_id;
    context.evaluateTokens = phase2;
    
    context.setup_token_iterator();
    
    if(context.evaluateTokens) {
        auto old_imp = lexer->getImport_unsafe(import_id);
        std::string name = old_imp->path;
        
        context.new_import_id = lexer->createImport(name, &context.new_lexer_import);
        Assert(context.new_import_id != 0);
        
        lock_imports.lock();
        auto new_imp = imports.requestSpot(context.new_import_id-1, nullptr);
        Assert(new_imp);
        context.current_import = imports.get(import_id-1);
        lock_imports.unlock();
    } else {
        lock_imports.lock();
        context.current_import = imports.requestSpot(import_id-1, nullptr);
        lock_imports.unlock();
        Assert(context.current_import);
    }
    
    while(true) {
        SignalIO signal = context.parseOne();
        if(signal == SIGNAL_COMPLETE_FAILURE) {
            // notify the caller of preprocessor.process that an error occurred.
            break;
        }
        switch(signal){
        case SIGNAL_SUCCESS: break;
        default: Assert(false);
        }
    }

    // if(context.evaluateTokens && context.new_lexer_import->chunks.size() > 0) {
    //     auto c = context.new_lexer_import->chunks.last();
    //     log::out << "preproc toks/srcs " << c->tokens.size() << "/" << c->sources.size()<<"\n";
    // }
    
    return context.new_import_id;
}
/* #region */

void MacroSpecific::addParam(lexer::Token name, bool variadic) {
    parameters.add(name);
    if(variadic) {
        Assert(!isVariadic()); // can't add another variadic parameter
        indexOfVariadic = parameters.size()-1;
    }
}
int MacroSpecific::matchArg(lexer::Token token, lexer::Lexer* lexer) {
    Assert(token.type == lexer::TOKEN_IDENTIFIER);
    const char* astr = 0;
    u32 alen = lexer->getStringFromToken(token,&astr);
    for(int i=0;i<(int)parameters.size();i++){
        lexer::Token& tok = parameters[i];
        const char* bstr = 0;
        u32 blen = lexer->getStringFromToken(tok,&bstr);
        if(alen == blen && !strcmp(astr,bstr)){
            _MMLOG(engone::log::out <<engone::log::MAGENTA<< "match tok with arg '"<<astr<<"'\n";)
            return i;
        }
    }
    return -1;
}

MacroRoot* Preprocessor::create_or_get_macro(u32 import_id, lexer::Token name, bool ensure_blank) {
    Assert(name.type == lexer::TOKEN_IDENTIFIER);
    
    lock_imports.lock();
    Import* imp = imports.get(import_id-1);
    lock_imports.unlock();
    Assert(imp);
    
    MacroRoot* macroRoot = nullptr;
    std::string str_name = lexer->getStdStringFromToken(name);
    
    auto pair = imp->rootMacros.find(str_name);
    if(pair != imp->rootMacros.end()){
        macroRoot = pair->second;
    } else {
        macroRoot = TRACK_ALLOC(MacroRoot);
        // engone::log::out << "yes\n";
        new(macroRoot)MacroRoot();
        macroRoot->name = str_name;
        imp->rootMacros[str_name] = macroRoot;
    }
    
    // NOTE: You usually want a base case for recursive macros. ensure_blank
    //   will ensure that an empty zero argument macro is the base case so
    //   you won't have to specify it yourself.
    // IMPORTANT: It should NOT happen for 'macro(...)' because the blank
    //   macro would be matched instead of the variadic one. The caller of
    //   this f unction handles that logic at the moment.
    if (ensure_blank){
        auto pair = macroRoot->specificMacros.find(0);
        if(pair == macroRoot->specificMacros.end()) {
            MacroSpecific* macro = TRACK_ALLOC(MacroSpecific);
            new(macro)MacroSpecific();
            macroRoot->specificMacros[0] = macro;
            macroRoot->hasBlank = true;
        }
    }
    return macroRoot;
}

void Preprocessor::insertCertainMacro(u32 import_id, MacroRoot* rootMacro, MacroSpecific* localMacro) {
    // nocheckin, we should not be able to insert specific macros into roots from other imports. Otherwise other files that import those files would suddenly have a specific macro available to them even if the import of the root macro didn't specify it.
    lock_imports.lock();
    Import* imp = imports.get(import_id-1);
    lock_imports.unlock();
    Assert(imp);
    
    if(!localMacro->isVariadic()){
        auto pair = rootMacro->specificMacros.find(localMacro->parameters.size());
        MacroSpecific* macro = nullptr;
        if(pair!=rootMacro->specificMacros.end()){
            // if(!certainMacro->isVariadic() && !certainMacro->isBlank()){
            //     WARN_HEAD3(name, "Intentional redefinition of '"<<name<<"'?\n\n";
            //         ERR_LINE2(certainMacro->name, "previous");
            //         ERR_LINE2(name.tokenIndex, "replacement");
            //     )
            // }
            if(localMacro->parameters.size() == 0) {
                rootMacro->hasBlank = false;
            }
            macro = pair->second;
            *macro = *localMacro;
            // if(includeInf && rootMacro->hasVariadic && (int)rootMacro->variadicMacro.parameters.size()-1<=count) {
            //     macro = &rootMacro->variadicMacro;
            //     _MLOG(MLOG_MATCH(engone::log::out<<engone::log::MAGENTA << "match argcount "<<count<<" with "<< rootMacro->variadicMacro.parameters.size()<<" (inf)\n";))
            // }
        }else{
            // _MLOG(MLOG_MATCH(engone::log::out <<engone::log::MAGENTA<< "match argcount "<<count<<" with "<< pair->second.parameters.size()<<"\n";))
            macro = TRACK_ALLOC(MacroSpecific);
            new(macro)MacroSpecific(*localMacro);
            rootMacro->specificMacros[localMacro->parameters.size()] = macro;
        }
    }else{
        // if(rootMacro->hasVariadic){
        //     WARN_HEAD3(name, "Intentional redefinition of '"<<name<<"'?\n\n";
        //         ERR_LINE2(certainMacro->name, "previous");
        //         ERR_LINE2(name.tokenIndex, "replacement");
        //     )
        // }
        rootMacro->hasVariadic = true;
        rootMacro->variadicMacro = *localMacro;
    }
}

bool Preprocessor::removeCertainMacro(u32 import_id, MacroRoot* rootMacro, int argumentAmount, bool variadic){
    Assert(false); // nocheckin, only for local macros
    lock_imports.lock();
    Import* imp = imports.get(import_id-1);
    lock_imports.unlock();
    Assert(imp);
    
    bool removed=false;
    if(variadic) {
        if(rootMacro->hasVariadic) {
            rootMacro->hasVariadic = false;
            removed = true;
            // rootMacro->variadicMacro.cleanup();
            rootMacro->variadicMacro = {};
            if(rootMacro->hasBlank){
                auto blank = rootMacro->specificMacros[0];
                blank->~MacroSpecific();
                TRACK_FREE(blank,MacroSpecific);
                rootMacro->specificMacros.erase(0);
                rootMacro->hasBlank = false;
            }
        }
    } else {
        auto pair = rootMacro->specificMacros.find(argumentAmount);
        if(pair != rootMacro->specificMacros.end()){
            removed = true;
            pair->second->~MacroSpecific();
            TRACK_FREE(pair->second,MacroSpecific);
            rootMacro->specificMacros.erase(pair);
        }
    }
    return removed;
}
MacroRoot* Preprocessor::matchMacro(u32 import_id, const std::string& name) {
    DynamicArray<u32> checked_ids{}; // necessary to prevent infinite loop with circular dependencies
    DynamicArray<u32> ids_to_check{};
    ids_to_check.add(import_id);
    while(ids_to_check.size()>0) {
        u32 id = ids_to_check.last();
        ids_to_check.pop();
        
        lock_imports.lock();
        Import* imp = imports.get(id-1);
        lock_imports.unlock();
        Assert(imp);
        
        auto pair = imp->rootMacros.find(name);
        if(pair != imp->rootMacros.end()){
            _MMLOG(engone::log::out << engone::log::CYAN << "match root "<<name<<" from '"<<id<<"'\n")
            return pair->second;
        }
        checked_ids.add(id);
        
        // TODO: Optimize
        FOR(imp->import_dependencies) {
            bool already_checked = false;
            for(int i=0;i<checked_ids.size();i++) {
                if(checked_ids[i] == it) {
                    already_checked=true;
                    break;   
                }
            }
            if(!already_checked) {
                ids_to_check.add(it);
            }
        }
    }
    return nullptr;
}

MacroSpecific* Preprocessor::matchArgCount(MacroRoot* root, int count){
    auto pair = root->specificMacros.find(count);
    MacroSpecific* macro = nullptr;
    if(pair==root->specificMacros.end()){
        if(root->hasVariadic && (int)root->variadicMacro.parameters.size()-1<=count) {
            macro = &root->variadicMacro;
            _MMLOG(engone::log::out<<engone::log::MAGENTA << "match argcount "<<count<<" with "<< root->variadicMacro.parameters.size()<<" (inf)\n")
        }
    }else{
        macro = pair->second;
        _MMLOG(engone::log::out <<engone::log::MAGENTA<< "match argcount "<<count<<" with "<< macro->parameters.size()<<"\n")
    }
    return macro;
}
/* #endregion */
}