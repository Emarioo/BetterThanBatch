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

SignalIO PreprocContext::parseMacroDefinition() {
    using namespace engone;
    ZoneScopedC(tracy::Color::Gold);

    bool multiline = false;
    
    lexer::Token name_token = gettok();
    
    if(name_token.type != lexer::TOKEN_IDENTIFIER){
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
                        if(!evaluateTokens) { // don't print message twice
                            ERR_SECTION(
                                ERR_HEAD2(token)
                                ERR_MSG("Macros can only have 1 variadic argument.")
                                ERR_LINE2(localMacro.parameters[localMacro.indexOfVariadic], "previous")
                                ERR_LINE2(token, "not allowed")
                            )
                        }
                    }
                    
                    hadError=true;
                }
            } else if(token.type != lexer::TOKEN_IDENTIFIER){
                if(!hadError){
                    if(!evaluateTokens) { // don't print message twice
                        ERR_SECTION(
                            ERR_HEAD2(token)
                            ERR_MSG("'"<<lexer->tostring(token)<<"' is not a valid name for arguments. The first character must be an alpha (a-Z) letter, the rest can be alpha and numeric (0-9). '...' is also available to specify a variadic argument.")
                            ERR_LINE2(token, "bad");
                        )
                    }
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
                    return SIGNAL_COMPLETE_FAILURE; // ensures that we don't preprocess again if we aren't evaluating macros
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
            if((token.flags&lexer::TOKEN_FLAG_SPACE)||(token.flags&lexer::TOKEN_FLAG_NEWLINE)){
                continue;
                // ERR_SECTION(
                // ERR_HEAD2(token, "SPACE AFTER "<<token<<"!\n";
                // )
                // return SignalAttempt::FAILURE;
            }
            lexer::Token mac_tok = gettok();
            if(lexer->equals_identifier(mac_tok, "endmacro")){
                advance();
                endToken = gethead()-2;
                break;
            }
            if(lexer->equals_identifier(mac_tok, "macro")){
                invalidContent = true;
                if(evaluateTokens) { // don't print message twice
                    ERR_SECTION(
                        ERR_HEAD2(mac_tok)
                        ERR_MSG("Macro definitions inside macros are not allowed.")
                        ERR_LINE2(mac_tok,"not allowed")
                        ERR_LINE2(name_token,"inside this macro")
                    )
                    return SIGNAL_COMPLETE_FAILURE;
                }
            }
            //not end, continue
        }
        if(!multiline){
            if(token.flags&lexer::TOKEN_FLAG_NEWLINE){
                endToken = gethead();
                break;
            }
        }
        if(token.type == lexer::TOKEN_EOF&&multiline){
            invalidContent = true;
            // if(!evaluateTokens) { // don't print message twice
            ERR_SECTION(
                ERR_HEAD2(name_token)
                ERR_MSG("Missing '#endmacro' for macro '"<<lexer->tostring(name_token)<<"' ("<<(localMacro.isVariadic() ? "variadic" : std::to_string(localMacro.parameters.size()))<<" arguments). Note that you must specify the end for empty macros. They are otherwise assumed to be multi-line macros.")
                ERR_LINE2(name_token, "this needs #endmacro somewhere")
                ERR_LINE2(gettok(-1),"macro content ends here!")
            )
            // }
            return SIGNAL_COMPLETE_FAILURE;
            break;
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

    // if(!evaluateTokens) {
        // TODO: We define and create macros not inside #if twice.
        //    First with evaluteTokens, then without.
        //    We don't want to do that. Also, what if you got a global macro
        //   evaluated at !evlauateTokens and then
        MacroRoot* rootMacro = preprocessor->create_or_get_macro(import_id, name_token, localMacro.isVariadic() && localMacro.parameters.size() > 1);
        
        localMacro.location = { name_token };
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
    // }
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

SignalIO PreprocContext::parseInclude(){
    using namespace engone;
    ZoneScopedC(tracy::Color::Wheat);
    #ifdef gone
    Token hashtag = info.get(info.at()+1);
    SignalAttempt result = ParseDirective(info, attempt, "include");
    if(result==SignalAttempt::BAD_ATTEMPT)
        return SignalAttempt::BAD_ATTEMPT;

    int tokIndex = info.at()+1;
    // needed for an error, saving it here in case the code changes
    // and info.at changes it output
    Token include_token = info.get(tokIndex);
    if(!(include_token.flags&TOKEN_MASK_QUOTED)){
        ERR_SECTION(
            ERR_HEAD(include_token)
            ERR_MSG("expected a string not "<<include_token<<".")
        )
        return SignalAttempt::FAILURE;
    }
    info.next();

    std::string filepath = include_token;
    Path fullpath = {};

    Path dir = TrimLastFile(info.inTokens->streamName);
    dir = dir.getDirectory();

    // std::string dir = TrimLastFile(info.inTokens->streamName);
    //-- Search directory of current source file
    if(filepath.find("./")==0){
        fullpath = dir.text + filepath.substr(2);
    }
    
    //-- Search cwd or absolute pathP
    else if(FileExist(filepath)){
        fullpath = filepath;
        if(!fullpath.isAbsolute())
            fullpath = fullpath.getAbsolute();
        // fullpath = engone::GetWorkingDirectory() + "/" + filepath;
        // ReplaceChar((char*)fullpath.data(),fullpath.length(),'\\','/');
    }
     else if(fullpath = dir.text + filepath, FileExist(fullpath.text)){
        // fullpath =  dir.text + filePath;
    }
    //-- Search additional import directories.
    // TODO: DO THIS WITH #INCLUDE TOO!
    else {
        // this is thread safe because we importDirectories is read only
        for(int i=0;i<(int)info.compileInfo->importDirectories.size();i++){
            const Path& dir = info.compileInfo->importDirectories[i];
            Assert(dir.isDir() && dir.isAbsolute());
            Path path = dir.text + filepath;
            bool yes = FileExist(path.text);
            if(yes) {
                fullpath = path;
                break;
            }
        }
    }
    
    // TODO: Search additional import directories
    
    if(fullpath.text.empty()){
        ERR_SECTION(
            ERR_HEAD(include_token)
            ERR_MSG("Could not include '"<<filepath<<"' (not found). Format is not appended automatically (while import does append .btb).")
            ERR_LINE(info.get(tokIndex),"not found")
        )
        return SignalAttempt::FAILURE;
    }

    Assert(info.compileInfo);

    if(quoted) {
        u64 fileSize=0;
        auto file = engone::FileOpen(fullpath.text, engone::FILE_READ_ONLY, &fileSize);
        defer { if(file) engone::FileClose(file); };
        if(!file) {
            ERR_SECTION(
                ERR_HEAD(include_token)
                ERR_MSG("Could not include '"<<filepath<<"' (not found). Format is not appended automatically (while import does append .btb).")
                ERR_LINE(info.get(tokIndex),"not found")
            )
        } else {
            char* data = (char*)Allocate(fileSize);
            defer { if(data) Free(data, fileSize); };
            Assert(data);
            engone::FileRead(file, data, fileSize);
            
            // Quotes in data will not confuse the string. Quotes was evaluated to strings in the lexer.
            // Strings are now defined by flags in tokens. It doesn't matter if the string contains quotes.
            
            Token tok{};
            tok.str = data;
            tok.length = fileSize;
            tok.flags = TOKEN_DOUBLE_QUOTED | (include_token.flags & TOKEN_MASK_SUFFIX);
            tok.column = hashtag.column; // TODO: Is this wanted?
            tok.line = hashtag.length;
            info.addToken(tok);
        }
        
    } else {
        info.compileInfo->otherLock.lock();
        auto pair = info.compileInfo->includeStreams.find(fullpath.text);
        TokenStream* includeStream = 0;
        if(pair==info.compileInfo->includeStreams.end()){
            info.compileInfo->otherLock.unlock();
            includeStream = TokenStream::Tokenize(fullpath.text);
            info.compileInfo->otherLock.lock();
            _LOG(LOG_INCLUDES,
                if(includeStream){
                    log::out << log::GRAY <<"Tokenized include: "<< log::LIME<<filepath<<"\n";
                }
            )
            info.compileInfo->includeStreams[fullpath.text] = includeStream;
        }else{
            includeStream = pair->second;
        }
        info.compileInfo->otherLock.unlock();
        
        if(!includeStream){
            ERR_SECTION(
                ERR_HEAD(include_token)
                ERR_MSG("Error with token stream for "<<filepath<<" (bug in the compiler!).")
            )
            return SignalAttempt::FAILURE;
        }
        int tokenIndex=0;
        while(tokenIndex<includeStream->length()){
            Token tok = includeStream->get(tokenIndex++);
            tok.column = hashtag.column; // TODO: Is this wanted?
            tok.line = hashtag.length;
            info.addToken(tok);
        }
        // TokenStream::Destroy(includeStream); destroyed by CompileInfo

        _MLOG(log::out << "Included "<<tokenIndex<<" tokens from "<<filepath<<"\n";)
    }
    #endif
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
    bool had_else = false;
    bool printed_else_error = false;
    bool printed_elif_error = false;
    
    bool prev_cond = inside_conditional;
    inside_conditional = true;
    defer { inside_conditional = prev_cond; };

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
                if(depth == 0) {
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
                        if(had_else && !printed_elif_error) {
                            printed_elif_error = true;
                            if(evaluateTokens) {
                                ERR_SECTION(
                                    ERR_HEAD2(token)
                                    ERR_MSG("#elif is not allowed after #else.")
                                    ERR_LINE2(token, "here")
                                )
                            }
                            complete_inactive = true;
                        }
                        if(!active && !complete_inactive) {
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
                    continue;
                }
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
            } else if(token.type == lexer::TOKEN_ELSE){ 
                if(depth==0){
                    advance();
                    advance();
                    if(!complete_inactive) {
                        active = !active;
                        if(had_else && !printed_else_error) {
                            printed_else_error = true;
                            if(evaluateTokens) {
                                ERR_SECTION(
                                    ERR_HEAD2(token)
                                    ERR_MSG("Multiple #else is not allowed in an #if directive.")
                                    ERR_LINE2(token, "here")
                                )
                            }
                            complete_inactive = true;
                        }
                        had_else = true;
                    }
                    _MLOG(log::out << log::GRAY<< "   else - "<<name<<"\n";)
                    continue;
                }
            } else if(lexer->equals_identifier(token, "import")) {
                if(!evaluateTokens || active) {
                    advance();
                    advance();
                    auto signal = parseImport();
                    if(signal == SIGNAL_COMPLETE_FAILURE) {
                        return SIGNAL_COMPLETE_FAILURE;
                    }
                    continue;
                }
            }
        }
        
        if(active){
            auto signal = parseOne();
            if(signal == SIGNAL_COMPLETE_FAILURE) {
                return SIGNAL_COMPLETE_FAILURE;
            }
        }else{
            advance();
            // log::out << log::GRAY<<" skip "<<skip << "\n";
        }
    }
    //  log::out << "     exit  ifdef loop\n";
    return SIGNAL_SUCCESS;
}
SignalIO PreprocContext::parseImport() {
    using namespace engone;
    StringView path;
    lexer::Token str_token = gettok(&path);
    if(str_token.type != lexer::TOKEN_LITERAL_STRING) {
        ERR_SECTION(
            ERR_HEAD2(str_token)
            ERR_MSG("Expected a string not '"<<lexer->tostring(str_token)<<"'.")
        )
        return SIGNAL_COMPLETE_FAILURE;
    }
    advance();

    StringView view_as{};
    bool has_as = false;
    lexer::Token token = gettok(&view_as);
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
    
    // if((!evaluateTokens && !inside_conditional) || (evaluateTokens && inside_conditional)) {
        // TODO: Macros from imports in #if cannot be preprocessed before they are needed in this import. How to deal with it?
        // Do we process import even if the #if may be false in case it evaluates to true? It would be marked disabled and then enabled if #if was true

        // std::string path = lexer->getStdStringFromToken(token);
        
        preprocessor->lock_imports.lock();
        auto imp = preprocessor->imports.get(import_id-1);
        preprocessor->lock_imports.unlock();
        
        auto lexer_imp = lexer->getImport_unsafe(import_id);
        std::string orig_dir = TrimLastFile(lexer_imp->path);
        
        std::string assumed_path{};
        u32 dep_id = compiler->addOrFindImport(path, orig_dir, &assumed_path);
        
        bool prev_show = info.showErrors;
        if(!evaluateTokens) { // only print errors on the second preprocessing
            info.showErrors = false;
        }
        defer {
            info.showErrors = prev_show;
        };

        bool disabled = !evaluateTokens && inside_conditional;

        if(dep_id == 0) {
            if(assumed_path.size()) {
                ERR_SECTION(
                    ERR_HEAD2(str_token)   
                    ERR_MSG_COLORED("The import '"<<log::GREEN<<path<<log::NO_COLOR<<"' could not be found. It was assumed to exist here '"<<log::GREEN<<assumed_path<<log::NO_COLOR<<"' due to the './' which indicates a relative directory to the current import ('"<<log::GREEN<<TrimLastFile(lexer_import->path)<<log::NO_COLOR<<"' in this case).")
                    ERR_LINE2(str_token,"here")
                )
            } else {
                ERR_SECTION(
                    ERR_HEAD2(str_token)   
                    ERR_MSG_COLORED("The import '"<<log::GREEN<<path<<log::NO_COLOR<<"' could not be found. Did you setup default modules directory correctly? Does the file exist in current working directory of the compiler or from import directories through command line options?")
                    ERR_LINE2(str_token,"here")
                    // TODO: List the import directories?
                )
            }
            return SIGNAL_SUCCESS; // we failed semantically, but not syntactically
        } else {
            if(dep_id == import_id) {
                ERR_SECTION(
                    ERR_HEAD2(str_token)   
                    ERR_MSG_COLORED("A file cannot import itself because there is no point in doing that.")
                    ERR_LINE2(str_token,"here")
                )
            } else {
                // log::out << "depend " << dep_id << " " << path <<" from "<<old_lexer_import->path<< "\n";
                // imp->import_dependencies.add(dep_id);
                if(has_as)
                    compiler->addDependency(import_id, dep_id, view_as, disabled);
                else
                    compiler->addDependency(import_id, dep_id, "", disabled);
            }
        }
    // }
    return SIGNAL_SUCCESS;
}
SignalIO PreprocContext::parseMacroEvaluation() {
    using namespace engone;
    ZoneScopedC(tracy::Color::Goldenrod2);
    
    Assert(evaluateTokens); // We shouldn't call this function unless it's to evaluate macros, it's just waste of CPU time
    
    
    lexer::Token macro_token = gettok();
    lexer::TokenSource* macro_source = getsource();
    Assert(macro_token.type == lexer::TOKEN_IDENTIFIER);
    
    // TODO: Optimize macro matching. One way is to have a table of 256 bools per character which represent
    //   Whether there exists a macro with that character as the first character. This is probably faster
    //   than directly looking into a hash table. The question is the overhead of managing the table.
    //   The table would exist per thread and we need to load macros into the table
    //   based on which files have been imported and which macros should be visible.
    // static bool early_match_table[256];
    // if(!early_match_table[macro_name[0]]) return SIGNAL_NO_MATCH;

    std::string macro_name = lexer->getStdStringFromToken(macro_token);
    MacroRoot* root = preprocessor->matchMacro(import_id,macro_name);
    if(!root)
        return SIGNAL_NO_MATCH;
    advance();
    
    LOG(LOG_PREPROCESSOR, 
        log::LIME << "Eval " << macro_name<<"\n";
    )
    

    typedef DynamicArray<lexer::Token> TokenList;
    
    // TODO: Optimize by not allocating tokens on the heap.
    //  Linear or some form of stack allocator would be better.
    //  Perhaps an array of arrays that you can reuse?
    
    u32 origin_import_id = import_id;
    
    struct Layer {
        Layer(bool eval_content) : eval_content(eval_content) {
            // Assert(eval_content);
            specific = nullptr;
            root = nullptr;
            if(eval_content) {
                new(&input_arguments)DynamicArray<TokenList>();
            } else {
                import_id = 0;
                top_caller = nullptr;
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
        u16 ending_suffix = 0;
        bool eval_content = true;
        bool unwrapped = false;
        bool concat_next_token = false;
        bool quote_next_token = false;
        MacroSpecific* specific;
        MacroRoot* root;
        // union {
        //     struct {
                DynamicArray<TokenList> input_arguments{};
            // };
            // struct {
                u32 import_id = 0;
                Layer* top_caller=nullptr; // the parent macro that called this macro (used by argument parsing or body parsing)
                Layer* adjacent_callee=nullptr; // when this layer is parsing arguments, callee refers to the layer (macro) that uses the arguments
                int paren_depth=0;
            // };
        // };
        
        lexer::Token get(lexer::Lexer* lexer, int off = 0, StringView* string = nullptr) {
            if(import_id!=0 && !eval_content) {
                u32 index = *ref_head + off;
                return lexer->getTokenFromImport(import_id, *ref_head + off, string);
            } else {
                auto spec = specific;
                if(top_caller)
                    spec = top_caller->specific; // for arguments
                Assert(specific);
                u32 index = spec->content.token_index_start + *ref_head + off;
                if(index >= spec->content.token_index_end)
                    return {lexer::TOKEN_EOF};
                return lexer->getTokenFromImport(spec->content.importId, index, string);
            }
        }
        bool is_last(lexer::Lexer* lexer, int off = 0) {
            return get(lexer,off).type == lexer::TOKEN_EOF;
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
    auto createLayer = [&](bool eval_content) {
        auto ptr = TRACK_ALLOC(Layer);
        new(ptr)Layer(eval_content);
        return ptr;
    };
    auto deleteLayer = [&](Layer* layer) {
        layer->~Layer();
        TRACK_FREE(layer,Layer);
    };
    DynamicArray<Layer*> layers{};
    defer {
        for(auto& l : layers)
            deleteLayer(l);
        layers.cleanup();
    };
    Layer* first = createLayer(true);
    layers.add(first);
    first->root = root;
    first->sethead((u32)0);
    first->ending_suffix = macro_token.flags & lexer::TOKEN_FLAG_ANY_SUFFIX;
    
    lexer::Token tok = gettok();
    if(!(macro_token.flags & (lexer::TOKEN_FLAG_ANY_SUFFIX)) && tok.type == '(') {
        advance();
        Layer* second = createLayer(false);
        layers.add(second);
        second->adjacent_callee = first;
        second->import_id = import_id;
        second->sethead(&head); // head is the preprocessors head
    }

    #define SET_SUFFIX(VAR,FROM) VAR = (VAR & ~lexer::TOKEN_FLAG_ANY_SUFFIX) | (FROM & lexer::TOKEN_FLAG_ANY_SUFFIX)
    int appended_tokens = 0;
    
    auto apply_concat=[&](Layer* layer, lexer::Token token, StringView* view = nullptr, bool compute_source = true) {
        std::string new_data="";
        int ln, col;
        
        if(new_lexer_import->chunks.size() == 0)
            layer->concat_next_token = false;
        
        if(layer->concat_next_token) {
            int token_count = new_lexer_import->getTokenCount();
            
            // TODO: Allow binary and hexidecimal literals
            auto tok = lexer->getTokenFromImport(new_import_id, token_count-1);
            auto prev_src = lexer->getTokenSource_unsafe({tok});
            if((tok.type == lexer::TOKEN_LITERAL_OCTAL ||tok.type == lexer::TOKEN_LITERAL_BINARY || tok.type == lexer::TOKEN_LITERAL_HEXIDECIMAL || tok.type == lexer::TOKEN_LITERAL_INTEGER || tok.type == lexer::TOKEN_IDENTIFIER || TOKEN_IS_KEYWORD(tok.type)) &&
            (token.type == lexer::TOKEN_LITERAL_OCTAL || token.type == lexer::TOKEN_LITERAL_BINARY ||token.type == lexer::TOKEN_LITERAL_HEXIDECIMAL ||token.type == lexer::TOKEN_LITERAL_INTEGER || token.type == lexer::TOKEN_IDENTIFIER || TOKEN_IS_KEYWORD(token.type))) {
                // TODO: There is bug if we concat two identifiers which become a keyword
                if(TOKEN_IS_KEYWORD(tok.type)) {
                    new_data += TOK_KEYWORD_NAME(tok.type);
                } else {
                    new_data += lexer->getStdStringFromToken(tok);
                }
                if(view) {
                    new_data += std::string(*view);
                } else if(TOKEN_IS_KEYWORD(token.type)) {
                    new_data += TOK_KEYWORD_NAME(token.type);
                } else {
                    new_data += lexer->getStdStringFromToken(token);
                }
                ln = prev_src->line;
                col = prev_src->column; // prev_src is destroyed by pop so we must save line and column here
                lexer->popTokenFromImport(new_lexer_import);
            } else {
                layer->concat_next_token = false;
            }
        }
        if(layer->quote_next_token) {
            if((token.type == lexer::TOKEN_LITERAL_OCTAL || token.type == lexer::TOKEN_LITERAL_BINARY ||token.type == lexer::TOKEN_LITERAL_HEXIDECIMAL ||token.type == lexer::TOKEN_LITERAL_INTEGER || token.type == lexer::TOKEN_IDENTIFIER || TOKEN_IS_KEYWORD(token.type))) {
                if(view) {
                    new_data += std::string(*view);
                } else if(TOKEN_IS_KEYWORD(token.type)) {
                    new_data += TOK_KEYWORD_NAME(token.type);
                } else {
                    new_data += lexer->getStdStringFromToken(token);
                }
            } else {
                layer->quote_next_token = false;
            }
        }

        if(layer->quote_next_token) {
            StringView new_view = new_data;
            lexer::Token new_token{};
            new_token.type = lexer::TOKEN_LITERAL_STRING;
            new_token.flags = lexer::TOKEN_FLAG_HAS_DATA | lexer::TOKEN_FLAG_DOUBLE_QUOTED | lexer::TOKEN_FLAG_NULL_TERMINATED;
            new_token.flags |= token.flags & lexer::TOKEN_FLAG_ANY_SUFFIX;
            lexer->appendToken(new_lexer_import, new_token, compute_source, &new_view, 0, macro_source->line, macro_source->column);
        } else if(layer->concat_next_token) {
            MacroRoot* macroroot = preprocessor->matchMacro(origin_import_id, new_data);
            if (macroroot) {
                // layer->step(); // name
                
                Layer* layer_macro = createLayer(true);
                layer_macro->root = macroroot;
                layer_macro->sethead((u32)0);
                layer_macro->adjacent_callee = layer->adjacent_callee;
                layer_macro->unwrapped = layer->unwrapped;
                layer->unwrapped = false;
                layers.add(layer_macro);
                // we need to specify that when we do eval_content
                // the tokens should be appended to input_arguments
                layer_macro->ending_suffix = token.flags & lexer::TOKEN_FLAG_ANY_SUFFIX;
            } else {
                StringView new_view = new_data;
                lexer::Token new_token{};
                new_token.type = lexer::TOKEN_IDENTIFIER;
                new_token.flags = lexer::TOKEN_FLAG_HAS_DATA | lexer::TOKEN_FLAG_NULL_TERMINATED;
                new_token.flags |= token.flags & lexer::TOKEN_FLAG_ANY_SUFFIX;
                lexer->appendToken(new_lexer_import, new_token, compute_source, &new_view, 0, ln, col);
            }
        } else {
            return false;
        }

        layer->concat_next_token = false;
        layer->quote_next_token = false;
        return true;
    };

    int counter_directive = 0;

    // rename to 'index_of_first_token' or 'token_count_before_eval'
    int index_of_prev_token = new_lexer_import->getTokenCount();
    // lexer->appendToken(new_lexer_import, list[j], appended_tokens != 0, nullptr, 0, macro_source->line, macro_source->column);
    while(layers.size() != 0) {
        if(layers.size() >= PREPROC_REC_LIMIT) {
            // TODO: Better error message, maybe provide macro call stack with arguments?
            ERR_SECTION(
                ERR_HEAD2(macro_token)
                ERR_MSG("Recursion limit for macros reached! "<<PREPROC_REC_LIMIT<<" is the maximum.")
                ERR_LINE2(macro_token,"recursion began here")
            )
            // macro was incorrectly evaluated so we remove any tokens that was added
            lexer->popMultipleTokensFromImport(new_lexer_import, index_of_prev_token);
            return SIGNAL_COMPLETE_FAILURE;
        }
        
        Layer* layer = layers.last();
        if(!layer->eval_content) { // evaluate argumentsname_token
            Assert(layer->adjacent_callee);
            if(layer->top_caller)
                // the top caller makes it's content visible to arge evaulator
                layer->specific = layer->top_caller->specific;
            lexer::Token token = layer->get(lexer);
            if(token.type == lexer::TOKEN_EOF) {
                // TODO: Assert happens if ) wasn't found
                // this can happen at the end of the file
                Assert(false);
            }
            
            if(layer->paren_depth==0 && (token.type == ',' || token.type == ')')) {
                if(token.type == ',') {
                    layer->step();
                    layer->adjacent_callee->input_arguments.add({});
                    if(layer->unwrapped) {
                        // unused unwrap, would this be a feature? or an error?
                    }
                    layer->unwrapped = false;
                    continue;
                }
                if(token.type == ')') {
                    layer->step();

                    deleteLayer(layer);
                    layers.pop();
                    // nocheckin TODO: Can the last layer be argument layer and
                    //   not macro body? Perhaps with macros within argument layer?
                    layers.last()->ending_suffix = token.flags & lexer::TOKEN_FLAG_ANY_SUFFIX;
                    continue;
                }
                Assert(false); // unreachable
            }
            
            if(token.type == '(') {
                layer->paren_depth++;
            } else if(token.type == ')') {
                layer->paren_depth--;
            }
            
            if(token.type == '#') {
                StringView unwrap_str{};
                lexer::Token unwrap_token = layer->get(lexer,1,&unwrap_str);
                if(unwrap_token.type == lexer::TOKEN_IDENTIFIER && unwrap_str == "unwrap") {
                    layer->unwrapped = true;
                    layer->step(2);
                    continue;   
                }
            }
            
            if(token.type == lexer::TOKEN_IDENTIFIER) {
                // nochecking TODO: Should we auto set the specific like when evaluating content or not?
                Assert(!layer->top_caller || layer->top_caller->specific);
                if(layer->top_caller && layer->top_caller->specific) { // we can't match argument unless we have a top_caller
                    Assert(layer->top_caller->specific);
                    MacroSpecific* top_caller_spec = layer->top_caller->specific;
                    int param_index = top_caller_spec->matchArg(token,lexer);
                    
                    if(param_index!=-1) {
                        if(layer->unwrapped) {
                            // TODO: unwrapped for arguments not implemented. Do we care?
                            //   Do error message?
                            layer->unwrapped = false;
                        }
                        layer->step();
                        
                        int real_index = param_index;
                        int real_count = 1;
                        if(top_caller_spec->isVariadic()) {
                            if(param_index == top_caller_spec->indexOfVariadic) {
                                real_count = layer->top_caller->input_arguments.size() - top_caller_spec->nonVariadicArguments();
                            } else if(param_index > top_caller_spec->indexOfVariadic) {
                                real_index = param_index - 1 + layer->top_caller->input_arguments.size() - top_caller_spec->nonVariadicArguments();
                            }
                        }
                        
                        for(int i = real_index; i < real_index + real_count;i++) {
                            auto& list = layer->top_caller->input_arguments[i];
                            for(int j=0;j<list.size();j++) {
                                auto& token = list[j];
                                if(layer->adjacent_callee->input_arguments.size() == 0)
                                    layer->adjacent_callee->input_arguments.add({});
                                layer->adjacent_callee->input_arguments.last().add(token);
                            }
                            if(i != real_index + real_count - 1)
                                layer->adjacent_callee->input_arguments.add({});
                        }
                        continue;
                    }
                }
                std::string name = lexer->getStdStringFromToken(token);
                MacroRoot* macroroot = preprocessor->matchMacro(origin_import_id, name);
                if (macroroot) {
                    layer->step(); // name
                    
                    Layer* layer_macro = createLayer(true);
                    layer_macro->root = macroroot;
                    layer_macro->sethead((u32)0);
                    layer_macro->adjacent_callee = layer->adjacent_callee;
                    layer_macro->unwrapped = layer->unwrapped;
                    layer->unwrapped = false;
                    layers.add(layer_macro);
                    // we need to specify that when we do eval_content
                    // the tokens should be appended to input_arguments
                    layer_macro->ending_suffix = token.flags & lexer::TOKEN_FLAG_ANY_SUFFIX;

                    token = layer->get(lexer);
                    if(token.type == '(') {
                        layer->step();
                        
                        Layer* layer_arg = createLayer(false);
                        layer_arg->top_caller = layer->top_caller;
                        layer_arg->adjacent_callee = layer_macro;
                        // layer_arg->specific = layer->specific;
                        layer_arg->import_id = layer->import_id;
                        layer_arg->sethead(layer->ref_head);
                        
                        layers.add(layer_arg);
                    }
                    continue;
                }
            }
            
            layer->step();
            if(layer->adjacent_callee->input_arguments.size() == 0)
                layer->adjacent_callee->input_arguments.add({});
            layer->adjacent_callee->input_arguments.last().add(token);
        } else {
            if(!layer->specific) {
                layer->specific = preprocessor->matchArgCount(layer->root, layer->input_arguments.size());
            
                if(!layer->specific) {
                    ERR_SECTION(
                        ERR_HEAD2(macro_token)
                        ERR_MSG_COLORED("While processing the macro '"<<log::GREEN<<macro_name <<log::NO_COLOR<<"', the evaluation of macro '"<<log::GREEN<<layer->root->name<<log::NO_COLOR<<"' with '"<<log::GREEN<<(layer->input_arguments.size())<<log::NO_COLOR<<"' arguments occurred. This is not allowed since '"<<log::GREEN <<layer->root->name<<log::NO_COLOR<<"' is not defined for that amount of arguments. Below are the definitions")
                        ERR_LINE2(macro_token,"macro evaluation started here")
                        for(auto& pair : layer->root->specificMacros) {
                            ERR_LINE2(pair.second->location, "" << (int)pair.second->parameters.size() << " arguments");
                        }
                        if(layer->root->hasVariadic) {
                            ERR_LINE2(layer->root->variadicMacro.location, "variadic");
                        }
                    )
                    break;
                }
            }
        
            lexer::Token token = layer->get(lexer);
            if(token.type == lexer::TOKEN_EOF) {
                // NOTE: Concat can't be handled here because an argument may append more
                //   than one tokens. It's easy to concat two tokens at the end of a token stream
                //   but hard to do in the middle of one. Since an argument may have multiple tokens
                //   and it's the first one we want to concat we must do so directly before or after
                //   the token was added to the token stream.
                deleteLayer(layer);
                layers.pop();
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
                            bool end = i+1 == real_index + real_count && j+1 == list.size();
                            if(end) {
                                list[j].flags = (list[j].flags & ~lexer::TOKEN_FLAG_ANY_SUFFIX) | (token.flags & lexer::TOKEN_FLAG_ANY_SUFFIX);
                            }
                            if(end && layer->is_last(lexer)) {
                                SET_SUFFIX(list[j].flags, layer->ending_suffix);
                            }
                            if(layer->adjacent_callee) {
                                if (layer->unwrapped && list[j].type == ',') {
                                    layer->adjacent_callee->input_arguments.add({});
                                } else {
                                    if(layer->adjacent_callee->input_arguments.size() == 0)
                                        layer->adjacent_callee->input_arguments.add({});
                                    layer->adjacent_callee->input_arguments.last().add(list[j]);
                                }
                            } else {
                                bool concated = false;
                                if((layer->concat_next_token && new_lexer_import->chunks.size() > 0) ||layer->quote_next_token) {
                                    if(apply_concat(layer, list[j], nullptr, appended_tokens != 0)) {
                                        concated = true;
                                    }
                                }
                                if(!concated) {
                                    auto t = lexer->appendToken(new_lexer_import, list[j], appended_tokens != 0, nullptr, 0, macro_source->line, macro_source->column);
                                        // auto t = lexer->appendToken(new_lexer_import, list[j], true);
                                }
                                appended_tokens++;
                                layer->concat_next_token = false;
                                layer->quote_next_token = false;
                            }
                        }
                        if(i+1 != real_index + real_count && list.size() != 0) {
                            if(layer->adjacent_callee) {
                                lexer::Token com{};
                                com.c_type = ',';
                                com.type = (lexer::TokenType)',';
                                com.flags = lexer::TOKEN_FLAG_SPACE;
                                com.origin = 0; // no origin, is this fine?
                                // TODO: What about commas
                                if(layer->adjacent_callee->input_arguments.size() == 0)
                                    layer->adjacent_callee->input_arguments.add({});
                                layer->adjacent_callee->input_arguments.last().add(com);
                            } else {
                                lexer->appendToken_auto_source(new_lexer_import, (lexer::TokenType)',', (u32)lexer::TOKEN_FLAG_SPACE);
                                appended_tokens++;
                                // TODO: Concat logic?
                                layer->concat_next_token = false;
                                layer->quote_next_token = false;
                            }
                        }
                    }
                    
                    continue;
                }
                MacroRoot* macroroot = nullptr;
                if(!layer->quote_next_token) {
                    std::string name = lexer->getStdStringFromToken(token);
                    macroroot = preprocessor->matchMacro(origin_import_id, name);
                }
                if (macroroot) {
                    layer->step(); // name
                    
                    Layer* layer_macro = createLayer(true);
                    layer_macro->root = macroroot;
                    layer_macro->adjacent_callee = layer->adjacent_callee;
                    layer_macro->sethead((u32)0);
                    layer_macro->concat_next_token = layer->concat_next_token;
                    layer->concat_next_token = false; // carry over the concatenation
                    layer_macro->quote_next_token = layer->quote_next_token;
                    layer->quote_next_token = false; // carry over the quoted
                    layers.add(layer_macro);
                    layer_macro->ending_suffix = token.flags & lexer::TOKEN_FLAG_ANY_SUFFIX;
                    
                    // NOTE: Should unwrap be inherited?
                    layer_macro->unwrapped = layer->unwrapped;
                    
                    token = layer->get(lexer);
                    if(token.type == '(') {
                        layer->step();
                        
                        Layer* layer_arg = createLayer(false);
                        layer_arg->top_caller = layer;
                        layer_arg->adjacent_callee = layer_macro;
                        // layer_arg->specific = layer->specific;
                        layer_arg->sethead(layer->ref_head);
                        
                        layers.add(layer_arg);
                    }
                    
                    continue;
                }
            }
            if(token.type == '#') {
                StringView directive_str;
                lexer::Token directive_tok = layer->get(lexer, 1, &directive_str);
                lexer::Token some_tok{};
                std::string some_str{};

                if(directive_tok.type == '#' && (token.flags & lexer::TOKEN_FLAG_ANY_SUFFIX) == 0) {
                    layer->concat_next_token = true;
                    layer->step(2);
                    continue;
                } else {
                    SignalIO signal = SIGNAL_NO_MATCH;
                    if(directive_str == "line") {
                        auto src = macro_source;
                        std::string temp = std::to_string(src->line);

                        lexer::Token tok{};
                        tok.type = lexer::TOKEN_LITERAL_INTEGER;
                        tok.flags = directive_tok.flags&lexer::TOKEN_FLAG_ANY_SUFFIX;
                        tok.flags |= lexer::TOKEN_FLAG_HAS_DATA;
                        
                        some_str = temp;
                        some_tok = tok;
                        signal = SIGNAL_SUCCESS;
                    } else if(directive_str == "column") {
                        auto src = macro_source;
                        Assert(src);
                        std::string temp = std::to_string(src->column);

                        lexer::Token tok{};
                        tok.type = lexer::TOKEN_LITERAL_INTEGER;
                        tok.flags = directive_tok.flags&lexer::TOKEN_FLAG_ANY_SUFFIX;
                        tok.flags |= lexer::TOKEN_FLAG_HAS_DATA;
                        
                        some_str = temp;
                        some_tok = tok;
                        signal = SIGNAL_SUCCESS;
                    } else if(directive_str == "quoted") {
                        layer->quote_next_token = true;
                        layer->step(2);
                        continue;
                    } else if(directive_str == "counter") {
                        std::string temp = std::to_string(counter_directive++);

                        lexer::Token tok{};
                        tok.type = lexer::TOKEN_LITERAL_INTEGER;
                        tok.flags = directive_tok.flags&lexer::TOKEN_FLAG_ANY_SUFFIX;
                        tok.flags |= lexer::TOKEN_FLAG_HAS_DATA;
                        
                        some_str = temp;
                        some_tok = tok;
                        signal = SIGNAL_SUCCESS;
                    }
                    bool is_line_or_column = signal != SIGNAL_NO_MATCH;
                    bool compute_source = signal == SIGNAL_NO_MATCH;
                    if(signal == SIGNAL_NO_MATCH)
                        signal = parseInformational(token, directive_tok, directive_str, &some_tok, &some_str);
                    // SignalIO signal = SIGNAL_SUCCESS;
                    if(signal == SIGNAL_SUCCESS) {
                        layer->step(2); // hashtag + directive name
                        if(layer->is_last(lexer)) {
                            SET_SUFFIX(token.flags, layer->ending_suffix);
                        }
                        StringView tmp = some_str;
                        if(layer->adjacent_callee) {
                            if(is_line_or_column) {
                                if(layer->adjacent_callee->input_arguments.size() == 0)
                                    layer->adjacent_callee->input_arguments.add({});
                                layer->adjacent_callee->input_arguments.last().add(some_tok);
                            } else {
                                // TODO: We can't pass custom text such as file name as input_argument.
                                //    We would need another array to store that information.
                                Assert(("can't use directives like #line as arguments to macros",false));
                                if(layer->adjacent_callee->input_arguments.size() == 0)
                                    layer->adjacent_callee->input_arguments.add({});
                                layer->adjacent_callee->input_arguments.last().add(token);
                            }
                        } else {
                            // lexer->appendToken_auto_source(new_lexer_import, (lexer::TokenType)'_', (u32)lexer::TOKEN_FLAG_SPACE);
                            // lexer->appendToken(new_lexer_import, some_tok, compute_source, &tmp);

                            if((layer->concat_next_token && new_lexer_import->chunks.size() > 0) || layer->quote_next_token) {
                                if(!apply_concat(layer, some_tok, &tmp, compute_source)) {
                                    lexer->appendToken(new_lexer_import, some_tok, compute_source, &tmp);
                                    appended_tokens++;
                                }
                            } else {
                                lexer->appendToken(new_lexer_import, some_tok, compute_source, &tmp);
                                appended_tokens++;
                            }
                            layer->concat_next_token = false;
                            layer->quote_next_token = false;
                        }
                        continue;
                    }
                }
            }
            layer->step();
            if(layer->is_last(lexer)) {
                SET_SUFFIX(token.flags, layer->ending_suffix);
            }
            if(layer->adjacent_callee) {
                if (layer->unwrapped && token.type == ',') {
                    layer->adjacent_callee->input_arguments.add({});
                } else {
                    if(layer->adjacent_callee->input_arguments.size() == 0)
                        layer->adjacent_callee->input_arguments.add({});
                    layer->adjacent_callee->input_arguments.last().add(token);
                }
            } else {
                if((layer->concat_next_token && new_lexer_import->chunks.size() > 0) || layer->quote_next_token) {
                    if(!apply_concat(layer, token)) {
                        lexer->appendToken(new_lexer_import, token, appended_tokens != 0, nullptr, 0, macro_source->line, macro_source->column);
                        appended_tokens++;
                    }
                } else {
                    // NOTE: We use appended_tokens to disable compute_source for the first token that we add.
                    //   That way, we use the first macro call's line and column. Otherwise the content off
                    //   the macro would end up on a different line than where macro actually exist.
                    //   Debugging would be especially scuffed without this when it comes to macros.
                    lexer->appendToken(new_lexer_import, token, appended_tokens != 0, nullptr, 0, macro_source->line, macro_source->column);
                    appended_tokens++;
                }
                layer->concat_next_token = false;
                layer->quote_next_token = false;
            }
        }
    }
    #undef SET_SUFFIX
    return SIGNAL_SUCCESS;
}
SignalIO PreprocContext::parseInformational(lexer::Token hashtag_tok, lexer::Token directive_tok, StringView string, lexer::Token* out_tok, std::string* out_str) {
    #define advance advance_not_allowed
    // StringView string{};
    // auto hashtag_tok = gettok(-1);
    // auto macro_token = getinfo(&string);
    // we expect a hashtag to have been parsed
    Assert(evaluateTokens);
    SignalIO signal = SIGNAL_NO_MATCH;
    if(string == "line") {
        // advance();
        auto src = lexer->getTokenSource_unsafe({hashtag_tok});
        // auto src = getsource(-2);
        Assert(src);
        std::string temp = std::to_string(src->line);

        lexer::Token tok{};
        tok.type = lexer::TOKEN_LITERAL_INTEGER;
        tok.flags = directive_tok.flags&lexer::TOKEN_FLAG_ANY_SUFFIX;
        tok.flags |= lexer::TOKEN_FLAG_HAS_DATA;
        string.ptr = (char*)temp.data();
        string.len = temp.size();
        
        *out_str = temp;
        *out_tok = tok;
        // lexer->appendToken(new_lexer_import, tok, &string);
        signal = SIGNAL_SUCCESS;
    } else if(string == "column") {
        // advance();
        auto src = lexer->getTokenSource_unsafe({hashtag_tok});
        // auto src = getsource(-2);
        Assert(src);
        std::string temp = std::to_string(src->column);

        lexer::Token tok{};
        tok.type = lexer::TOKEN_LITERAL_INTEGER;
        tok.flags = directive_tok.flags&lexer::TOKEN_FLAG_ANY_SUFFIX;
        tok.flags |= lexer::TOKEN_FLAG_HAS_DATA;
        string.ptr = (char*)temp.data();
        string.len = temp.size();
        
        
        *out_str = temp;
        *out_tok = tok;
        // lexer->appendToken(new_lexer_import, tok, &string);
        signal = SIGNAL_SUCCESS;
    } else if(string == "file") {
        // advance();
        std::string temp = new_lexer_import->path;

        lexer::Token tok{};
        tok.type = lexer::TOKEN_LITERAL_STRING;
        tok.flags = directive_tok.flags&lexer::TOKEN_FLAG_ANY_SUFFIX;
        tok.flags |= lexer::TOKEN_FLAG_HAS_DATA | lexer::TOKEN_FLAG_NULL_TERMINATED | lexer::TOKEN_FLAG_DOUBLE_QUOTED;
        string.ptr = (char*)temp.data();
        string.len = temp.size();
        
        
        *out_str = temp;
        *out_tok = tok;
        // lexer->appendToken(new_lexer_import, tok, &string);
        signal = SIGNAL_SUCCESS;
    } else if(string == "filename") {
        // advance();
        std::string temp = TrimDir(new_lexer_import->path);

        lexer::Token tok{};
        tok.type = lexer::TOKEN_LITERAL_STRING;
        tok.flags = directive_tok.flags&lexer::TOKEN_FLAG_ANY_SUFFIX;
        tok.flags |= lexer::TOKEN_FLAG_HAS_DATA | lexer::TOKEN_FLAG_NULL_TERMINATED | lexer::TOKEN_FLAG_DOUBLE_QUOTED;
        string.ptr = (char*)temp.data();
        string.len = temp.size();
        
        *out_str = temp;
        *out_tok = tok;
        // lexer->appendToken(new_lexer_import, tok, &string);
        signal = SIGNAL_SUCCESS;
    } else if(string == "unique") { // rename to counter? unique makes you think aboout UUID
        // advance();
        
        #ifdef OS_WINDOWS
        i32 num = _InterlockedIncrement(&compiler->globalUniqueCounter) - 1;
        #else
        compiler->otherLock.lock();
        i32 num = compiler->globalUniqueCounter++;
        compiler->otherLock.unlock();
        #endif
        // NOTE: This would have been another alternative but x64 inline assembly doesn't work very well
        // i32* ptr = &info.compileInfo->globalUniqueCounter; 
        // i32 num = 0;
        // __asm {
        //     mov rbx, ptr
        //     mov eax, 1
        //     lock xadd DWORD PTR [rbx], eax
        //     mov num, eax
        // }
        
        std::string temp = std::to_string(num);

        lexer::Token tok{};
        tok.type = lexer::TOKEN_LITERAL_INTEGER;
        tok.flags = directive_tok.flags&lexer::TOKEN_FLAG_ANY_SUFFIX;
        tok.flags |= lexer::TOKEN_FLAG_HAS_DATA;
        string.ptr = (char*)temp.data();
        string.len = temp.size();
        
        *out_str = temp;
        *out_tok = tok;
        // lexer->appendToken(new_lexer_import, tok, &string);
        signal = SIGNAL_SUCCESS;
    } else if(string == "date") {
        // advance();
        
        time_t timer;
        time(&timer);
        auto date = localtime(&timer);
        
        std::string temp{};
        temp.resize(200); // TODO: Use a static buffer somewhere instead of allocating on the heap.
        int len = snprintf((char*)temp.data(), temp.size(), "%d-%d-%d",1900+date->tm_year, 1 + date->tm_mon, date->tm_mday);
        Assert(len != 0);
        temp.resize(len);

        lexer::Token tok{};
        tok.type = lexer::TOKEN_LITERAL_STRING;
        tok.flags = directive_tok.flags&lexer::TOKEN_FLAG_ANY_SUFFIX;
        tok.flags |= lexer::TOKEN_FLAG_HAS_DATA | lexer::TOKEN_FLAG_NULL_TERMINATED | lexer::TOKEN_FLAG_DOUBLE_QUOTED;
        string.ptr = (char*)temp.data();
        string.len = temp.size();
        
        *out_str = temp;
        *out_tok = tok;
        // lexer->appendToken(new_lexer_import, tok, &string);
        signal = SIGNAL_SUCCESS;
    }
    #undef advance
    return signal;
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
                if(signal == SIGNAL_SUCCESS)
                    return SIGNAL_SUCCESS;
                if(signal != SIGNAL_NO_MATCH)
                    return signal;
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
        } else if(string == "macro") {
            advance();
            signal = parseMacroDefinition();
        } else if(string == "import") {
            advance();
            signal = parseImport();
        } else if(string == "link") {
            advance();
            signal = parseLink();
        } else if(string == "load") {
            advance();
            signal = parseLoad();
        // } else if(!strcmp(str, "if")) {
        //     advance();
        //     signal = parseIf();
        } else {
            if(evaluateTokens) {
                lexer::Token some_tok{};
                std::string some_str{};
                signal = parseInformational(tok, macro_tok, string, &some_tok, &some_str);
                if(signal == SIGNAL_SUCCESS) {
                    advance();
                    StringView view = some_str;
                    lexer->appendToken(new_lexer_import, some_tok, &view);
                }
            }
            if(signal != SIGNAL_SUCCESS) {
                if(evaluateTokens) {
                    lexer->appendToken(new_lexer_import, tok, nullptr); // hashtag
                    lexer->appendToken(new_lexer_import, macro_tok, &string);
                    signal = SIGNAL_SUCCESS;
                } else {
                    signal = SIGNAL_SUCCESS;
                }
                advance();
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
    context.reporter = &compiler->reporter;
    context.import_id = import_id;
    context.evaluateTokens = phase2;
    
    context.setup_token_iterator();
    
    if(context.evaluateTokens) {
        context.old_lexer_import = lexer->getImport_unsafe(import_id);
        std::string name = context.old_lexer_import->path;
        
        context.new_import_id = lexer->createImport(name, &context.new_lexer_import);
        Assert(context.new_import_id != 0);
        
        lock_imports.lock();
        auto new_imp = imports.requestSpot(context.new_import_id-1, nullptr);
        Assert(new_imp);
        context.current_import = imports.get(import_id-1);
        lock_imports.unlock();
    } else {
        context.old_lexer_import = lexer->getImport_unsafe(import_id);
        lock_imports.lock();
        // log::out << "Preproc imp "<<import_id<<"\n";
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
    
    if(context.evaluateTokens) {
        auto imp = context.new_lexer_import;
        if(imp->chunks.size()) {
            auto last_chunk = imp->chunks.last();
            Assert(last_chunk->tokens.size() == last_chunk->sources.size());
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
        // engone::log::out << "yes\n";160
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
        
        // lock_imports.lock(); // nocheckin, TODO: lock
        CompilerImport* cimp = compiler->imports.get(id-1);
        // lock_imports.unlock();
        // may happen if import wasn't preprocessed and added to 'imports'
        // it was added to Compiler.imports but didn't process it before being
        // here.
        Assert(imp);
        Assert(cimp);
        
        auto pair = imp->rootMacros.find(name);
        if(pair != imp->rootMacros.end()){
            _MMLOG(engone::log::out << engone::log::CYAN << "match root "<<name<<" from '"<<id<<"'\n")
            return pair->second;
        }
        checked_ids.add(id);
        
        // TODO: Optimize
        FOR(cimp->dependencies) {
            if(it.disabled)
                continue;
            bool already_checked = false;
            for(int i=0;i<checked_ids.size();i++) {
                if(checked_ids[i] == it.id) {
                    already_checked=true;
                    break;   
                }
            }
            if(!already_checked) {
                ids_to_check.add(it.id);
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