#include "BetBat/Preprocessor.h"

// preprocessor needs CompileInfo to handle global caching of include streams
#include "BetBat/Compiler.h"


#undef WARN_HEAD3
#define WARN_HEAD3(T, M) info.compileInfo->warnings++;engone::log::out << WARN_CUSTOM(info.inTokens->streamName, T.line, T.column, "Preproc. warning","W0000") << M
#undef WARN_LINE2
#define WARN_LINE2(I, M) PrintCode(I, info.inTokens, M)

#undef ERR_HEAD3
#define ERR_HEAD3(T, M) info.compileInfo->errors++;engone::log::out << ERR_DEFAULT_T(info.inTokens,T,"Preproc. error","E0000") << M
#undef ERR_LINE2
#define ERR_LINE2(I, M) PrintCode(I, info.inTokens, M)

// #define MLOG_MATCH(X) X
#define MLOG_MATCH(X)

#undef ERR_SECTION
#define ERR_SECTION(CONTENT) { BASE_SECTION("Preproc. error, E0000"); CONTENT; }


#define _macros compileInfo->_macros

bool PreprocInfo::end(){
    return inTokens->end();
    //  index==(int)inTokens->length();
}
Token& PreprocInfo::next(){
    return inTokens->next();
    // Token& temp = inTokens->get(index++);
    // return temp;
}
int PreprocInfo::at(){
    return inTokens->at();
    // return index-1;
}
int PreprocInfo::length(){
    return inTokens->length();
}
Token& PreprocInfo::get(int index){
    return inTokens->get(index);
}
Token& PreprocInfo::now(){
    return inTokens->now();
    // return inTokens->get(index-1);   
}
RootMacro* PreprocInfo::matchMacro(const Token& token){
    // MEASURE;
// RootMacro* PreprocInfo::matchMacro(Token token){
    if(token.flags&TOKEN_MASK_QUOTED)
        return nullptr;
    auto pair = _macros.find(token);
    if(pair==_macros.end()){
        return nullptr;
    }
    // if(pair->second.called){
    //     auto& info = *this;
    //     ERR_HEAD3(token, "Infinite recursion not allowed ("<<token<<")\n";
    //     return 0;
    // }
    _MLOG(MLOG_MATCH(engone::log::out << engone::log::CYAN << "match root "<<token<<"\n";))
    return &pair->second;
}
void PreprocInfo::removeRootMacro(const Token& name){
    auto pair = _macros.find(name);
    if(pair!=_macros.end()){
        _macros.erase(pair);
    }
}
RootMacro* PreprocInfo::createRootMacro(const Token& name){
    auto ptr = &(_macros[name] = {});
    ptr->name = name;
    return ptr;
}

void CertainMacro::addParam(const Token& name){
    Assert(name.length!=0);
    parameters.add(name);
    // auto pair = parameterMap.find(name);
    // if(pair == parameterMap.end()){
    //     parameterMap[name] = parameters.size()-1;
    // }
    #ifndef SLOW_PREPROCESSOR
    int index=sortedParameters.size()-1;
    for(int i=0;i<sortedParameters.size();i++){
        auto& it = sortedParameters.get(i);
        if(*it.name.str > *name.str) {
            index = i;
            break;
        }
    }
    if(index==sortedParameters.size()-1){
        sortedParameters.add({name,(int)sortedParameters.size()});
    } else {
        sortedParameters._reserve(sortedParameters.size()+1);
        memmove(sortedParameters._ptr + index + 1, sortedParameters._ptr + index, (sortedParameters.size()-index) * sizeof(Parameter));
        *(sortedParameters._ptr + index) = {name,(int)sortedParameters.size()-1};
        sortedParameters.used++;
    }
    #endif
}
int CertainMacro::matchArg(const Token& token){
    // MEASURE;
    if(token.flags&TOKEN_MASK_QUOTED)
        return -1;
    #ifdef SLOW_PREPROCESSOR
    for(int i=0;i<(int)parameters.size();i++){
        std::string& str = parameters[i];
        if(token == str.c_str()){
            _MLOG(MLOG_MATCH(engone::log::out <<engone::log::MAGENTA<< "match tok with arg "<<token<<"\n";))
            return i;
        }
    }
    #else
    for(int i=0;i<(int)sortedParameters.size();i++){
        // std::string& str = parameters[i];
        if(token.length != sortedParameters[i].name.length || *token.str < *sortedParameters[i].name.str)
            continue;
        if(token == sortedParameters[i].name){
            _MLOG(MLOG_MATCH(engone::log::out <<engone::log::MAGENTA<< "match tok with arg "<<token<<"\n";))
            return sortedParameters[i].index;
            // return i;
            // sortedParameters[i].index;
        }
    }
    #endif
    return -1;
}
CertainMacro* RootMacro::matchArgCount(int count, bool includeInf){
    auto pair = certainMacros.find(count);
    if(pair==certainMacros.end()){
        if(!includeInf)
            return nullptr;
        if(hasInfinite){
            if((int)infDefined.parameters.size()-1<=count){
                _MLOG(MLOG_MATCH(engone::log::out<<engone::log::MAGENTA << "match argcount "<<count<<" with "<< infDefined.parameters.size()<<" (inf)\n";))
                return &infDefined;
            }else{
                return nullptr;
            }
        }else{
            return nullptr;
        }
    }else{
        _MLOG(MLOG_MATCH(engone::log::out <<engone::log::MAGENTA<< "match argcount "<<count<<" with "<< pair->second.parameters.size()<<"\n";))
        return &pair->second;
    }
}
// bool EvalInfo::matchSuperArg(Token& name, CertainMacro*& superMacro, Arguments*& args, int& argIndex){
bool EvalInfo::matchSuperArg(const Token& name, CertainMacro*& superMacro, Arguments*& args, int& argIndex){
    for(int i=0;i<(int)superMacros.size();i++){
        CertainMacro* macro = superMacros[i];
        int index=-1;
        if(-1!=(index = macro->matchArg(name))){
            superMacro = macro;
            args = superArgs[i];
            argIndex = index;
            return true;
        }
    }
    return false;
}
void PreprocInfo::addToken(Token inToken){
    // IMPORTANT Changes here may need to apply in ParseToken.
    //  Some special stuff with ## happens there.
    u64 offset = outTokens->tokenData.used; // get offset before adding data
    outTokens->addData(inToken);
    inToken.str = (char*)offset;
    outTokens->addToken(inToken);
}

SignalAttempt ParseDirective(PreprocInfo& info, bool attempt, const char* str){
    using namespace engone;
    Token token = info.get(info.at()+1);
    if(!Equal(token,PREPROC_TERM)){
        if(attempt){
            return SignalAttempt::BAD_ATTEMPT;
        }else{
            ERR_HEAD3(token, "Expected " PREPROC_TERM " since this wasn't an attempt found '"<<token<<"'\n";
            )
            return SignalAttempt::FAILURE;
        }
    }
    if(token.flags&TOKEN_SUFFIX_SPACE){
        if(attempt) {
            return SignalAttempt::BAD_ATTEMPT;
        } else {
            ERR_HEAD3(token, "Cannot have space after preprocessor directive\n";
            )
            return SignalAttempt::FAILURE;
        }
    }
    token = info.get(info.at()+2);
    if(token != str){
        if(attempt){
            return SignalAttempt::BAD_ATTEMPT;
        }else{
            ERR_HEAD3(token, "Expected "<<str<<" found '"<<token<<"'\n";
            )
            return SignalAttempt::FAILURE;
        }
        return SignalAttempt::FAILURE;
    }
    info.next();
    info.next();
    return SignalAttempt::SUCCESS;  
}
SignalAttempt ParseDefine(PreprocInfo& info, bool attempt){
    using namespace engone;
    MEASURE;
    Token token = info.get(info.at()+1);
    if(!Equal(token,PREPROC_TERM)){
        if(attempt){
            return SignalAttempt::BAD_ATTEMPT;
        }else{
            ERR_HEAD3(token, "Expected " PREPROC_TERM " since this wasn't an attempt found '"<<token<<"'\n";
            )
            return SignalAttempt::FAILURE;
        }
    }
    if(token.flags&TOKEN_SUFFIX_SPACE){
        ERR_HEAD3(token, "Cannot have space after preprocessor directive\n";
        )
        // info.next();
        return SignalAttempt::FAILURE;
    }
    token = info.get(info.at()+2);
    
    // TODO: only allow define on new line?
    
    bool multiline = false;
    if(token=="define"){
        
    }else if(token=="multidefine"){
        multiline=true;
    }else{
        if(attempt){
            return SignalAttempt::BAD_ATTEMPT;
        }else{
            ERR_HEAD3(token, "Expected define found '"<<token<<"'\n";
            )
            return SignalAttempt::FAILURE;
        }
        return SignalAttempt::FAILURE;
    }
    info.next(); // #
    info.next(); // define
    attempt = false;
    
    Token name = info.next();
    
    if(name.flags&TOKEN_MASK_QUOTED){
        ERR_HEAD3(name, "Macro names cannot be quoted ("<<name<<").\n\n";
            ERR_LINE2(name.tokenIndex,"");
        )
        return SignalAttempt::FAILURE;
    }
    if(!IsName(name)){
        ERR_HEAD3(name, "'"<<name<<"' is not a valid name for macros. If the name is valid for variables then it is valid for macros.\n\n";
            ERR_LINE2(name.tokenIndex, "bad");
        )
        return SignalAttempt::FAILURE;
    }
    
    // if(info.matchMacro(name)){
    //     WARN_HEAD3(name, "Intentional redefinition of '"<<name<<"'?\n";   
    // }
    
    int suffixFlags = name.flags;
    CertainMacro defTemp{};
    if(!((name.flags&TOKEN_SUFFIX_SPACE) || (name.flags&TOKEN_SUFFIX_LINE_FEED))){
        Token token = info.next();
        if(token!="("){
            ERR_HEAD3(token, "'"<<token<<"' is not allowed directly after a macro's name. Either use a '(' to indicate arguments or a space for none.\n\n";
                ERR_LINE2(token.tokenIndex,"bad");
            )
            return SignalAttempt::FAILURE;
        }
        int hadError = false;
        while(!info.end()){
            Token token = info.next();
            if(token==")"){
                suffixFlags = token.flags;
                break;   
            }
            
            if(token=="...") {
                if(defTemp.indexOfVariadic == -1) {
                    defTemp.indexOfVariadic = defTemp.parameters.size();
                    defTemp.addParam(token);
                } else {
                    if(!hadError){
                        ERR_SECTION(
                            ERR_HEAD(token)
                            ERR_MSG("Macros can only have 1 variadic argument.")
                            ERR_LINE(defTemp.parameters[defTemp.indexOfVariadic], "previous")
                            ERR_LINE(token, "not allowed")
                        )
                    }
                    
                    hadError=true;
                }
            } else if(!IsName(token)){
                if(!hadError){
                    ERR_SECTION(
                        ERR_HEAD(token)
                        ERR_MSG("'"<<token<<"' is not a valid name for arguments. The first character must be an alpha (a-Z) letter, the rest can be alpha and numeric (0-9). '...' is also available to specify a variadic argument.")
                        ERR_LINE(token, "bad");
                    )
                }
                hadError=true;
            } else {
                defTemp.addParam(token);
            }
            token = info.next();
            if(token==")"){
                suffixFlags = token.flags;
                break;   
            } else if(token==","){
                // cool
            } else{
                if(!hadError){
                    ERR_HEAD3(token, "'"<<token<< "' is not okay. You use ',' to specify more arguments and ')' to finish parsing of argument.\n\n";   
                        ERR_LINE2(token.tokenIndex, "bad");
                    )
                }
                hadError = true;
            }
        }
        
        _MLOG(log::out << "loaded "<<defTemp.parameters.size()<<" arg names";
        if(!defTemp.isVariadic())
            log::out << "\n";
        else
            log::out << " (variadic)\n";)
    
    }
    RootMacro* rootDefined = info.matchMacro(name);
    if(!rootDefined){
        rootDefined = info.createRootMacro(name);
    }
    if(defTemp.isVariadic() && 
    defTemp.nonVariadicArguments() != 0) {
        // READ BEFORE CHANGING! 
        //You usually want a base case for variadic arguments when
        // using it recursively. Instead of having to specify the blank
        // base macro yourself, the compiler does it for you.
        // IMPORTANT: It should NOT happen for macro(...)
        // because the blank macro with zero arguments would be
        // used instead of the variadic one making it impossible to
        // use macro(...).
        
        (rootDefined->certainMacros[0] = {}).blank = true;
    }
    
    CertainMacro* defined = 0;
    if(!defTemp.isVariadic()){
        defined = rootDefined->matchArgCount(defTemp.parameters.size(),false);
        if(defined){
            if(!defined->isVariadic() && !defined->isBlank()){
                WARN_HEAD3(name, "Intentional redefinition of '"<<name<<"'?\n\n";
                    ERR_LINE(defined->name, "previous");
                    ERR_LINE2(name.tokenIndex, "replacement");
                )
            }
            *defined = defTemp;
        }else{
            // move defTemp info appropriate place in rootDefined
            defined = &(rootDefined->certainMacros[(int)defTemp.parameters.size()] = defTemp);
        }
    }else{
        if(rootDefined->hasInfinite){
            WARN_HEAD3(name, "Intentional redefinition of '"<<name<<"'?\n\n";
                ERR_LINE(defined->name, "previous");
                ERR_LINE2(name.tokenIndex, "replacement");
            )
        }
        rootDefined->hasInfinite = true;
        rootDefined->infDefined = defTemp;
        defined = &rootDefined->infDefined;
    }
    bool invalidContent = false;
    int startToken = info.at()+1;
    int endToken = startToken;
    if(!multiline&&(suffixFlags&TOKEN_SUFFIX_LINE_FEED))
        goto while_skip_194;
    while(!info.end()){
        Token token = info.next();
        if(token==PREPROC_TERM){
            if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED)){
                continue;
                // ERR_HEAD3(token, "SPACE AFTER "<<token<<"!\n";
                // )
                // return SignalAttempt::FAILURE;
            }
            Token token = info.get(info.at()+1);
            if(token=="enddef"){
                info.next();
                endToken = info.at()-1;
                break;
            }
            if(token=="define" || token=="multidefine"){
                ERR_HEAD3(token, "Macro definitions inside macros are not allowed.\n\n";
                    ERR_LINE2(info.at()+1,"not allowed");
                )
                invalidContent = true;
            }
            //not end, continue
        }
        if(!multiline){
            if(token.flags&TOKEN_SUFFIX_LINE_FEED){
                endToken = info.at()+1;
                break;
            }
        }
        if(info.end()&&multiline){
            ERR_HEAD3(token, "Missing enddef for macro '"<<name<<"' ("<<(defined->isVariadic() ? "variadic" : std::to_string(defined->parameters.size()))<<" arguments)\n\n";
                ERR_LINE2(name.tokenIndex,"this needs #enddef somewhere");
                ERR_LINE2(info.length()-1,"macro content ends here!");
            )
        }
    }
    while_skip_194:
    defined->name = name;
    defined->start = startToken;
    defined->contentRange.stream = info.inTokens;
    defined->contentRange.start = startToken;
    if(!invalidContent){
        defined->end = endToken;
        defined->contentRange.end = endToken;
    } else {
        defined->end = startToken;
        defined->contentRange.end = startToken;
    }
    int count = endToken-startToken;
    int argc = defined->parameters.size();
    _MLOG(log::out << log::LIME<< PREPROC_TERM<<"define '"<<name<<"' ";
    if(argc!=0){
        log::out<< argc;
        if(argc==1) log::out << " arg, ";
        else log::out << " args, ";
    }
    log::out << count;
    // Are you proud of me?
    if(count==1) log::out << " token\n";
    else log::out << " tokens\n";)
    return SignalAttempt::SUCCESS;
}
SignalAttempt ParseUndef(PreprocInfo& info, bool attempt){
    using namespace engone;

    // TODO: check of end

    Token token = info.get(info.at()+1);
    if(!Equal(token,PREPROC_TERM)){
        if(attempt){
            return SignalAttempt::BAD_ATTEMPT;
        }else{
            ERR_HEAD3(token, "Expected " PREPROC_TERM " since this wasn't an attempt found '"<<token<<"'.\n";
            )
            
            return SignalAttempt::FAILURE;
        }
    }
    if(token.flags&TOKEN_SUFFIX_SPACE){
        ERR_HEAD3(token, "Cannot have space after preprocessor directive\n";
        )
        return SignalAttempt::FAILURE;
    }
    token = info.get(info.at()+2);
    if(token!="undef"){
        if(attempt){
            return SignalAttempt::BAD_ATTEMPT;
        }
        ERR_HEAD3(token, "Expected undef (since this wasn't an attempt)\n";
        )
        return SignalAttempt::FAILURE;
    }
    attempt = false;
    info.next();
    info.next();
    if(token.flags&TOKEN_SUFFIX_LINE_FEED){
        ERR_HEAD3(token, "Unexpected line feed (expected a macro name)\n";
        )
        return SignalAttempt::FAILURE;
    }
    
    Token name = info.next();

    // auto pair = info.macros.find(name);
    RootMacro* rootMacro = info.matchMacro(name);
    if(!rootMacro){
        WARN_HEAD3(name, "Cannot undefine '"<<name<<"' (doesn't exist)\n";
        )
        return SignalAttempt::SUCCESS;
    }

    if(name.flags&TOKEN_SUFFIX_LINE_FEED){
        info.removeRootMacro(name);
        return SignalAttempt::SUCCESS;
    }
    token = info.next();

    if(token!="..." && !IsInteger(token)){
        ERR_HEAD3(token, "Expected an integer (or ...) to indicate argument count not '"<<token<<"'.\n";
        )
        return SignalAttempt::FAILURE;
    }
    if(!(token.flags&TOKEN_SUFFIX_LINE_FEED)){
        ERR_HEAD3(token, "Expected line feed after '"<<token<<"'\n";
        )
        return SignalAttempt::FAILURE;
    }

    if(token=="..."){
        if(!rootMacro->hasInfinite){
            WARN_HEAD3(name, "Cannot undefine '"<<name<<"' [inf] (doesn't exist)\n";
            )
            return SignalAttempt::SUCCESS;
        }
        rootMacro->hasInfinite=false;
        rootMacro->infDefined = {};
        return SignalAttempt::SUCCESS;
    }

    int argCount = ConvertInteger(token);
    if(argCount<0){
        ERR_HEAD3(token, "ArgCount cannot be negative '"<<argCount<<"'\n";
        )
        return SignalAttempt::FAILURE;
    }

    auto cert = rootMacro->certainMacros.find(argCount);
    if(cert==rootMacro->certainMacros.end()){
        WARN_HEAD3(name, "Cannot undefine '"<<name<<"' ["<<argCount<<" args] (doesn't exist)\n";
        )
        return SignalAttempt::SUCCESS;
    }
    rootMacro->certainMacros.erase(cert);

    return SignalAttempt::SUCCESS;
}

SignalAttempt ParseImport(PreprocInfo& info, bool attempt){
    using namespace engone;
    MEASURE;
    Token token = info.get(info.at()+1);
    SignalAttempt result = ParseDirective(info, attempt, "import");
    if(result==SignalAttempt::BAD_ATTEMPT)
        return SignalAttempt::BAD_ATTEMPT;

    Token name = info.get(info.at()+1);
    if(!(name.flags&TOKEN_MASK_QUOTED)){
        ERR_HEAD3(name, "expected a string not "<<name<<"\n";
        )
        return SignalAttempt::FAILURE;
    }
    info.next();
    
    token = info.get(info.at()+1);
    if(Equal(token,"as")){
        info.next();
        token = info.get(info.at()+1);
        if(token.flags & TOKEN_MASK_QUOTED){
            ERR_HEAD3(token, "don't use quotes with "<<log::YELLOW<<"as\n";
            )
            return SignalAttempt::FAILURE;
        }
        info.next();
        // handled by PreprocessImports
        // info.inTokens->addImport(name,token);
    } else {
        // info.inTokens->addImport(name,"");
    }
    
    return SignalAttempt::SUCCESS;
}
SignalAttempt ParseLink(PreprocInfo& info, bool attempt){
    using namespace engone;
    MEASURE;
    Token token = info.get(info.at()+1);
    SignalAttempt result = ParseDirective(info, attempt, "link");
    if(result==SignalAttempt::BAD_ATTEMPT)
        return SignalAttempt::BAD_ATTEMPT;

    Token name = info.get(info.at()+1);
    if(!(name.flags&TOKEN_MASK_QUOTED)){
        ERR_HEAD3(name, "expected a string not "<<name<<"\n";
        )
        return SignalAttempt::FAILURE;
    }
    info.next();
    
    info.compileInfo->addLinkDirective(name);
    
    return SignalAttempt::SUCCESS;
}
SignalAttempt ParseInclude(PreprocInfo& info, bool attempt){
    using namespace engone;
    MEASURE;
    Token hashtag = info.get(info.at()+1);
    SignalAttempt result = ParseDirective(info, attempt, "include");
    if(result==SignalAttempt::BAD_ATTEMPT)
        return SignalAttempt::BAD_ATTEMPT;

    int tokIndex = info.at()+1;
    // needed for an error, saving it here in case the code changes
    // and info.at changes it output
    Token token = info.get(tokIndex);
    if(!(token.flags&TOKEN_MASK_QUOTED)){
        ERR_HEAD3(token, "expected a string not "<<token<<"\n";
        )
        return SignalAttempt::FAILURE;
    }
    info.next();

    std::string filepath = token;
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
        ERR_HEAD3(token, "Could not include '"<<filepath<<"' (not found). Format is not appended automatically (while import does append .btb).\n\n";
            ERR_LINE2(tokIndex,"not found");
        )
        return SignalAttempt::FAILURE;
    }

    Assert(info.compileInfo);
    auto pair = info.compileInfo->includeStreams.find(fullpath.text);
    TokenStream* includeStream = 0;
    if(pair==info.compileInfo->includeStreams.end()){
        includeStream = TokenStream::Tokenize(fullpath.text);
        #ifdef LOG_INCLUDES
        if(includeStream){
            log::out << log::GRAY <<"Tokenized include: "<< log::LIME<<filepath<<"\n";
        }
        #endif
        info.compileInfo->includeStreams[fullpath.text] = includeStream;
    }else{
        includeStream = pair->second;
    }
    
    if(!includeStream){
        ERR_HEAD3(token, "Error with token stream for "<<filepath<<" (bug in the compiler!)\n";
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
    
    return SignalAttempt::SUCCESS;
}

SignalDefault ParseToken(PreprocInfo& info);
// both ifdef and ifndef
SignalAttempt ParseIfdef(PreprocInfo& info, bool attempt){
    using namespace engone;
    bool notDefined=false;

    // Token ch = info.get(info.at()+1);
    // Token ch2 = info.get(info.at()+2);
    // if(ch2=="#"){
    //     log::out << "maybe stop\n";

    // }

    SignalAttempt resultDirective = ParseDirective(info,attempt,"ifdef");
    if(resultDirective==SignalAttempt::BAD_ATTEMPT){
        resultDirective = ParseDirective(info,attempt,"ifndef");
        notDefined=true;
    }
    if(resultDirective==SignalAttempt::BAD_ATTEMPT)
        return SignalAttempt::BAD_ATTEMPT;
    if(resultDirective!=SignalAttempt::SUCCESS)
        return resultDirective;
    // otherwise success!
    
    attempt = false;
    Token name = info.next();
    
    RootMacro* defined = 0;
    bool active = info.matchMacro(name);
    if(notDefined)
        active = !active;
    
    int depth = 0;
    SignalAttempt error = SignalAttempt::SUCCESS;
    // bool hadElse=false;
    _MLOG(log::out << log::GRAY<< "   ifdef - "<<name<<"\n";)
    while(!info.end()){
        Token token = info.get(info.at()+1);
        if(token==PREPROC_TERM){
            if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED)){
                ERR_HEAD3(token, "SPACE AFTER "<<token<<"!\n";
                )
                return SignalAttempt::FAILURE;
            }
            Token token = info.get(info.at()+2);
            // If !yes we can skip these tokens, otherwise we will have
            //  to skip them later which is unnecessary computation
            if(token=="ifdef" || token=="ifndef"){
                if(!active){
                    info.next();
                    info.next();
                    depth++;
                    // log::out << log::GRAY<< "   ifdef - new depth "<<depth<<"\n";
                }
                // continue;
            }else if(token=="endif"){
                if(depth==0){
                    info.next();
                    info.next();
                    _MLOG(log::out << log::GRAY<< "   endif - "<<name<<"\n";)
                    break;
                }
                // log::out << log::GRAY<< "   endif - new depth "<<depth<<"\n";
                depth--;
                continue;
            }else if(token == "else"){ // we allow multiple elses, they toggle active and inactive sections
                if(depth==0){
                    info.next();
                    info.next();
                    active = !active;
                    _MLOG(log::out << log::GRAY<< "   else - "<<name<<"\n";)
                    continue;
                }
            }
        }
        // TODO: what about errors?
        
        if(active){
            SignalDefault result = ParseToken(info);
        }else{
            Token skip = info.next();
            // log::out << log::GRAY<<" skip "<<skip << "\n";
        }
        if(info.end()){
            ERR_HEAD3(info.get(info.length()-1), "Missing endif somewhere for ifdef or ifndef\n";
            )
            return SignalAttempt::FAILURE;
        }
    }
    //  log::out << "     exit  ifdef loop\n";
    return error;
}
// does not support multiple tokens
// this function is different because macro need this interface when
// doing concatenation. interacting with inTokens and outTokens is not an option.
// YOU MUST PARSE HASHTAG BEFORE CALLING THIS
SignalAttempt ParsePredefinedMacro(PreprocInfo& info, const Token& parseToken, Token& outToken, char buffer[256]){
    using namespace engone;
    MEASURE;
    // Token token = info.get(info.at()+1);
    // Token token = parseToken;
    // early exit
    if(!parseToken.str || parseToken.length==0) {
        // if(attempt)
            return SignalAttempt::BAD_ATTEMPT;
        // else
        //     return SignalAttempt::FAILURE;
    }

    defer {
        Assert(outToken.length < 255);
    };

    // Previous token must be hashtag, should work with macros too.
    // MACRO line where MACRO is replaced with # will assert and should
    // #line which will never trigger assert
    Assert(parseToken.tokenStream);
    Assert(Equal(parseToken.tokenStream->get(parseToken.tokenIndex-1),"#"));

    // NOTE: One idea is to allow __LINE__ (double underscore) too
    //   but this would slow down the compiler a tiny bit so I am not going to.
    //   This mindset will be used elsewhere to and will propably provide more
    //   performance and less complexity.
    //   I will warn about double underscore though. (done below)
    if(Equal(parseToken,"line")){
        // info.next();

        std::string temp = std::to_string(parseToken.line);
        Assert(temp.size()-1 < sizeof(buffer));
        strcpy(buffer, temp.data());
        outToken = parseToken;
        outToken.str = (char*)buffer;
        outToken.length = temp.length();
        
        // _MLOG(log::out << "Append "<<outToken<<"\n";)
        // info.addToken(token);
        return SignalAttempt::SUCCESS;
    } else if(Equal(parseToken,"column")) {
        // info.next();

        std::string temp = std::to_string(parseToken.column);
        Assert(temp.size()-1 < sizeof(buffer));
        strcpy(buffer, temp.data());
        outToken = parseToken;
        outToken.str = (char*)buffer;
        outToken.length = temp.length();
        
        // _MLOG(log::out << "Append "<<outToken<<"\n";)
        // info.addToken(token);
        return SignalAttempt::SUCCESS;
    } else if(Equal(parseToken,"file")) {
        // info.next();

        // token.str = (char*)info.inTokens->streamName.data();
        // token.length = info.inTokens->streamName.length();
        auto& temp = info.inTokens->streamName;
        Assert(temp.size()-1 < sizeof(buffer));
        strcpy(buffer, temp.data());
        outToken = parseToken;
        outToken.str = (char*)buffer;
        outToken.length = temp.length();
        outToken.flags = TOKEN_DOUBLE_QUOTED;
        
        // _MLOG(log::out << "Append "<<outToken<<"\n";)
        // info.addToken(token);
        return SignalAttempt::SUCCESS;
    } else if(Equal(parseToken,"filename")) {
        // info.next();

        // int lastSlash = -1;
        // for(int i=0;i<(int)info.inTokens->streamName.length();i--){
        //     if(info.inTokens->streamName[i] == '/') {
        //         lastSlash = i;
        //         break;
        //     }
        // }
        
        auto temp = TrimDir(info.inTokens->streamName);
        Assert(temp.size()-1 < sizeof(buffer));
        strcpy(buffer, temp.data());
        outToken = parseToken;
        outToken.str = (char*)buffer;
        outToken.length = temp.length();
        outToken.flags = TOKEN_DOUBLE_QUOTED;
        
        // if(lastSlash != -1 && ( info.inTokens->streamName.size() == 0
        //      || info.inTokens->streamName.back() != '/'))
        // {
        //      outToken.str += lastSlash + 1;
        //      outToken.length -= lastSlash + 1;
        // }
        
        // _MLOG(log::out << "Append "<<outToken<<"\n";)
        // info.addToken(token);
        return SignalAttempt::SUCCESS;
    } else if(Equal(parseToken,"unique")) { // rename to counter? unique makes you think aboout UUID
        // info.next();

        i32 num = info.compileInfo->globalUniqueCounter++;

        std::string temp = std::to_string(num);
        Assert(temp.size()-1 < sizeof(buffer));
        strcpy(buffer, temp.data());
        outToken = parseToken;
        outToken.str = (char*)buffer;
        outToken.length = temp.length();
        
        // _MLOG(log::out << "Append "<<outToken<<"\n";)
        // info.addToken(token);
        return SignalAttempt::SUCCESS;
    } else {
        #define BAD_DOUBLE_UNDERSCORE(X) if(Equal(parseToken,"__" #X "__")) { WARN_HEAD3(parseToken,"Did you mean '_" #X "_' with two underscores?\n\n";WARN_LINE2(parseToken.tokenIndex,"_" #X "_ instead?");)}
        BAD_DOUBLE_UNDERSCORE(LINE)
        else BAD_DOUBLE_UNDERSCORE(COLUMN)
        else BAD_DOUBLE_UNDERSCORE(FILE)
        else BAD_DOUBLE_UNDERSCORE(FILENAME)
        else BAD_DOUBLE_UNDERSCORE(UNIQUE)
        #undef BAD_DOUBLE_UNDERSCORE
        // if(attempt)
        return SignalAttempt::BAD_ATTEMPT;
        // else
        //     return SignalAttempt::FAILURE;
    }
}
void Transfer(PreprocInfo& info, TokenList& from, TokenList& to, bool quoted, bool unwrap=false,Arguments* args=0,int* argIndex=0){
    // TODO: optimize with some resize and memcpy?
    for(int i=0;i<(int)from.size();i++){
        if(!unwrap){
            auto tmp = from[i];
            if(quoted)
                tmp.flags |= TOKEN_DOUBLE_QUOTED;
            to.push_back(tmp);
        }else{
            Token tok = info.get(from[i]);
            if(tok==","){
                (*argIndex)++;
                continue;
            }
            if((int)args->size()==*argIndex)
                args->push_back({});
            auto tmp = from[i];
            if(quoted)
                tmp.flags |= TOKEN_DOUBLE_QUOTED;
            args->back().push_back(tmp);
        }
    }
}
SignalDefault EvalMacro(PreprocInfo& info, EvalInfo& evalInfo);
// Called by EvalMacro
// Example: MACRO(Hello + 2, Cool(stuf, hey))
SignalDefault EvalArguments(PreprocInfo& info, EvalInfo& evalInfo){
    using namespace engone;
    TokenList& tokens = evalInfo.workingRange;
    if(evalInfo.macroName.flags&(TOKEN_SUFFIX_LINE_FEED|TOKEN_SUFFIX_SPACE)){
        evalInfo.finalFlags = evalInfo.macroName.flags&(TOKEN_SUFFIX_LINE_FEED|TOKEN_SUFFIX_SPACE);
        return SignalDefault::SUCCESS;
    }

    int& index=evalInfo.workIndex;
    Token token = info.get(tokens[index]);
    if(token != "("){
        evalInfo.finalFlags = evalInfo.macroName.flags&(TOKEN_SUFFIX_LINE_FEED|TOKEN_SUFFIX_SPACE);
        return SignalDefault::SUCCESS;
    }
    index++;
    Token token2 = info.get(tokens[index]);
    if(token2 == ")"){
        index++;
        evalInfo.finalFlags = token2.flags&(TOKEN_SUFFIX_LINE_FEED|TOKEN_SUFFIX_SPACE);
        return SignalDefault::SUCCESS; // no arguments
    }
    bool wasQuoted=false;
    int parDepth = 0;
    bool unwrapNext=false;
    while(index<(int)tokens.size()){
        TokenRef tokenRef = tokens[index];
        Token token = info.get(tokenRef);
        index++;
        if(token==","){
            unwrapNext=false;
            if(parDepth==0){
                evalInfo.argIndex++;
                continue;
            }
        }else if(token=="("){
            parDepth++;
        }else if(token==")"){
            if(parDepth==0){
                evalInfo.finalFlags = token.flags&(TOKEN_SUFFIX_LINE_FEED|TOKEN_SUFFIX_SPACE);
                break;
            }
            parDepth--;
        }

        if(token==PREPROC_TERM){
            Token nextTok = info.get(tokens[index]);
            if(nextTok=="unwrap"){
                _MLOG(log::out << log::MAGENTA<<"unwrap\n";)
                unwrapNext=true;
                index++;
                continue;
            }
        }

        RootMacro* rootMacro=0;
        CertainMacro* superMacro = 0;
        Arguments* superArgs = 0;
        int argIndex=-1;
        if(evalInfo.matchSuperArg(token,superMacro,superArgs,argIndex)){
            _MLOG(log::out << log::GRAY<<" match arg "<<token <<", index: "<<argIndex<<"\n";)
            int argStart = argIndex;
            int argEnd = argIndex+1;
            
            if(superMacro->isVariadic()){
                // one from argValues is guarranteed. Therfore -1.
                // parameters has ... therefore another -1
                int extraArgs = superArgs->size() - 1 - (superMacro->parameters.size() - 1);
                if(argIndex>superMacro->indexOfVariadic){
                    argStart += extraArgs;
                    argEnd += extraArgs;
                } else if(argIndex==superMacro->indexOfVariadic){
                    argEnd += extraArgs;
                }
            }
            // _MLOG(log::out << " argValues "<<superArgs->size()<<"\n";)
            for(int i=argStart;i<argEnd;i++){
                TokenList argTokens = (*superArgs)[i];
                evalInfo.arguments.push_back({});
                int indexArg=0;
                while(indexArg<(int)argTokens.size()){
                    Token argTok = info.get(argTokens[indexArg]);
                    indexArg++;
                    RootMacro* rootMacro=0;
                    if((rootMacro=info.matchMacro(argTok))){
                        // calculate arguments?

                        EvalInfo newEvalInfo{};
                        newEvalInfo.rootMacro = rootMacro;
                        newEvalInfo.macroName = argTok;
                        newEvalInfo.superMacros = evalInfo.superMacros;
                        newEvalInfo.superArgs = evalInfo.superArgs;

                        for(int i=indexArg;i<(int)argTokens.size();i++){
                            newEvalInfo.workingRange.push_back(argTokens[i]);
                        }
                        SignalDefault result = EvalArguments(info, newEvalInfo);
                        indexArg += newEvalInfo.workIndex;

                        CertainMacro* macro = rootMacro->matchArgCount(newEvalInfo.arguments.size());
                        if(!macro){
                            ERR_HEAD3(argTok, "Macro '"<<argTok<<"' cannot "<<newEvalInfo.arguments.size()<<" arguments\n";
                            )
                            // return SignalAttempt::FAILURE;
                        }else{
                            newEvalInfo.macro = macro;
                            EvalMacro(info,newEvalInfo);
                            Transfer(info,newEvalInfo.output,evalInfo.arguments.back(),wasQuoted);
                        }
                    }else{
                        auto tmp = argTokens[indexArg-1];
                        if(wasQuoted)
                            tmp.flags|=TOKEN_DOUBLE_QUOTED;
                        evalInfo.arguments.back().push_back(tmp);
                        _MLOG(log::out <<log::GRAY << " argv["<<(evalInfo.arguments.size()-1)<<"] += "<<argTok<<"\n";)
                    }
                }
            }
        } else if((rootMacro = info.matchMacro(token))){
            
            EvalInfo newEvalInfo{};
            newEvalInfo.rootMacro = rootMacro;
            newEvalInfo.macroName = token;
            newEvalInfo.superMacros = evalInfo.superMacros;
            newEvalInfo.superArgs = evalInfo.superArgs;

            for(int i=index;i<(int)tokens.size();i++){
                newEvalInfo.workingRange.push_back(tokens[i]);
            }
            SignalDefault result = EvalArguments(info, newEvalInfo);
            index += newEvalInfo.workIndex;
            
            CertainMacro* macro = rootMacro->matchArgCount(newEvalInfo.arguments.size());
            if(!macro){
                ERR_HEAD3(token,"Macro '"<<token<<"' cannot "<<newEvalInfo.arguments.size()<<" arguments\n";
                )
                // return SignalAttempt::FAILURE;
            }else{
                newEvalInfo.macro = macro;
                EvalMacro(info,newEvalInfo);
                
                if((int)evalInfo.arguments.size()==evalInfo.argIndex)
                        evalInfo.arguments.push_back({});
                if(unwrapNext){
                    _MLOG(log::out << log::MAGENTA<<"actual unwrap\n";)
                }
                Transfer(info,newEvalInfo.output,evalInfo.arguments.back(),wasQuoted,unwrapNext,&evalInfo.arguments,&evalInfo.argIndex);
            }
        }else {
            if((int)evalInfo.arguments.size()==evalInfo.argIndex)
                evalInfo.arguments.push_back({});
            if(wasQuoted)
                tokenRef.flags|=TOKEN_DOUBLE_QUOTED;
            evalInfo.arguments.back().push_back(tokenRef);
            _MLOG(log::out <<log::GRAY << " argv["<<(evalInfo.arguments.size()-1)<<"] += "<<token<<"\n";)
        }
        unwrapNext=false;
    }
    _MLOG(log::out << "Eval "<<evalInfo.arguments.size()<<" arguments\n";)
    return SignalDefault::SUCCESS;
}
SignalDefault EvalMacro(PreprocInfo& info, EvalInfo& evalInfo){
    using namespace engone;

    if(info.macroRecursionDepth>=PREPROC_REC_LIMIT){
        ERR_HEAD3(info.now(), "Reached recursion limit of "<<PREPROC_REC_LIMIT<<" for macros\n";
        )
        return SignalDefault::FAILURE;
    }
    info.macroRecursionDepth++;
    TokenList tokens{};
    for(int i=evalInfo.macro->start;i<evalInfo.macro->end;i++){
        tokens.push_back({(uint16)i,(uint16)info.get(i).flags});
    }
    _MLOG(log::out <<"Eval "<<evalInfo.macroName<<", "<<tokens.size()<<" tokens\n";)
    int index=0;
    bool wasQuoted=false;
    while(index<(int)tokens.size()){
        Token token = info.get(tokens[index]);
        index++;

        if(token==PREPROC_TERM&&!(token.flags&TOKEN_SUFFIX_SPACE)){
            // TODO: bound check
            Token token2 = info.get(tokens[index]);
            if(token2=="quoted"){
                index++;
                wasQuoted=true;
                continue;
            }   
        }

        RootMacro* rootMacro=0;
        int argIndex = -1;
        if(evalInfo.macro && -1!=(argIndex = evalInfo.macro->matchArg(token))){
            _MLOG(log::out << log::GRAY<<" match arg "<<token <<", index: "<<argIndex<<"\n";)
            int argStart = argIndex;
            int argEnd = argIndex+1;
            
            if(evalInfo.macro->isVariadic()){
                // one from argValues is guarranteed. Therfore -1.
                // parameters has ... therefore another -1
                int extraArgs = evalInfo.arguments.size() - 1 - (evalInfo.macro->parameters.size() - 1);
                if(argIndex>evalInfo.macro->indexOfVariadic){
                    argStart += extraArgs;
                    argEnd += extraArgs;
                } else if(argIndex==evalInfo.macro->indexOfVariadic){
                    argEnd += extraArgs;
                }
            }
            for(int i=argStart;i<argEnd;i++){
                TokenList& args = evalInfo.arguments[i];
                int indexArg=0;
                while(indexArg<(int)args.size()){
                    Token argTok = info.get(args[indexArg]);
                    auto ref = args[indexArg];
                    indexArg++;
                    
                    if(i+1==argEnd&&indexArg==(int)args.size())
                        ref.flags |= token.flags & (TOKEN_SUFFIX_SPACE|TOKEN_SUFFIX_LINE_FEED);
                    if(index==(int)tokens.size()&&i+1==argEnd&&indexArg==(int)args.size())
                        ref.flags = evalInfo.finalFlags;
                    if(wasQuoted){
                        ref.flags |= TOKEN_DOUBLE_QUOTED;
                    }
                    evalInfo.output.push_back(ref);
                    _MLOG(log::out << log::GRAY <<"  eval.out << "<<argTok<<"\n";);
                }
            }
            wasQuoted=false;
        }else if((rootMacro = info.matchMacro(token))){
            EvalInfo newEvalInfo{};
            newEvalInfo.rootMacro = rootMacro;
            newEvalInfo.macroName = token;

            newEvalInfo.superMacros.push_back(evalInfo.macro);
            newEvalInfo.superArgs.push_back(&evalInfo.arguments);
            _MLOG(log::out <<log::GRAY<<"push super\n";)

            for(int i=index;i<(int)tokens.size();i++){
                newEvalInfo.workingRange.push_back(tokens[i]);
            }
            SignalDefault result = EvalArguments(info, newEvalInfo);
            index += newEvalInfo.workIndex;

            newEvalInfo.superMacros.pop_back();
            newEvalInfo.superArgs.pop_back();
            _MLOG(log::out <<log::GRAY<<"pop super\n";)

            if(index==(int)tokens.size())
                newEvalInfo.finalFlags = evalInfo.finalFlags;
            

            CertainMacro* macro = rootMacro->matchArgCount(newEvalInfo.arguments.size());
            if(!macro){
                ERR_HEAD3(token, "Macro '"<<token<<"' cannot have "<<newEvalInfo.arguments.size()<<" arguments\n";
                )
            }else{
                newEvalInfo.macro = macro;
                EvalMacro(info,newEvalInfo);
                Transfer(info,newEvalInfo.output,evalInfo.output,wasQuoted);
                wasQuoted=false;
            }
        } else{
            if(index==(int)tokens.size())
                tokens[index-1].flags = evalInfo.finalFlags;
            
            if(wasQuoted){
                tokens[index-1].flags |= TOKEN_DOUBLE_QUOTED;
                wasQuoted=false;
            }

            evalInfo.output.push_back(tokens[index-1]);
            _MLOG(log::out << log::GRAY <<" eval.out << "<<token<<"\n";);
        }
    }
    info.macroRecursionDepth--;
    return SignalDefault::SUCCESS;
}
SignalAttempt ParseMacro(PreprocInfo& info, int attempt){
    using namespace engone;
    MEASURE;
    Token name = info.get(info.at()+1);
    RootMacro* rootMacro=0;
    if(!(rootMacro = info.matchMacro(name))){
        if(attempt){
            return SignalAttempt::BAD_ATTEMPT;
        }
        ERR_HEAD3(name, "Undefined macro '"<<name<<"'\n";
        )
        return SignalAttempt::FAILURE;
    }

    info.next();
    
    EvalInfo evalInfo{};
    evalInfo.rootMacro = rootMacro;
    evalInfo.macroName = name;

    // TODO: do not add remaining tokens to workingRange
    for(int i=info.at()+1;i<(int)info.length();i++){
        evalInfo.workingRange.push_back({(uint16)i,(uint16)info.get(i).flags});
    }
    SignalDefault result = EvalArguments(info, evalInfo);
    for(int i=0;i<evalInfo.workIndex;i++)
        info.next();
    // TODO: handle result

    CertainMacro* macro = rootMacro->matchArgCount(evalInfo.arguments.size());
    if(!macro){
        ERR_HEAD3(name, "Macro '"<<name<<"' cannot have "<<evalInfo.arguments.size()<<" arguments.\n\n";
            ERR_LINE2(name.tokenIndex,"bad");
        )
        return SignalAttempt::FAILURE;
    }

    evalInfo.macro = macro;

    // evaluate the real deal
    EvalMacro(info,evalInfo);

    // Can't process a macro when doing final parsing of the macro.
    // The recursive macro handling should've happened when using arrays of tokens.
    Assert(!info.usingTempStream);

    // Create a temporary stream to act as inTokens when
    // parsing ifdef and predefined tokens.
    if(!info.tempStream) {
        info.tempStream = TokenStream::Create();
    }
    info.tempStream->tokenData.used = 0;
    info.tempStream->tokens.used = 0;
    info.tempStream->streamName = info.inTokens->streamName;
    info.tempStream->readHead = 0;
    info.usingTempStream = true;

    // Time to output the stuff
    for(int i=0;i<(int)evalInfo.output.size();i++){
        Token baseToken = info.get(evalInfo.output[i]);
        baseToken.flags = evalInfo.output[i].flags;
        uint64 offset = info.tempStream->tokenData.used;
        info.tempStream->addData(baseToken);
        baseToken.str = (char*)offset;
        // baseToken.str = (char*)info.outTokens->tokenData.data + offset;

        WHILE_TRUE {
            Token nextToken{};
            if(i+1<(int)evalInfo.output.size()){
                nextToken=info.get(evalInfo.output[i+1]);
            }
            if(nextToken=="##"){
                if(i+2<(int)evalInfo.output.size()){
                    Token token2 = info.get(evalInfo.output[i+2]);
                    info.tempStream->addData(token2);
                    baseToken.length += token2.length;
                    i+=2;
                    continue;
                }
                i++;
            }
            info.tempStream->addToken(baseToken);
            break;
        }
    }
    info.tempStream->finalizePointers();
   
    TokenStream* originalStream = info.inTokens;
    info.inTokens = info.tempStream; // Note: don't modify inTokens
    while(!info.end()){
        // #ifndef SLOW_PREPROCESSOR
        // // Quick check;
        // Token& token = info.get(info.at()+1);
        // if(*token.str != '#' && *token.str != '_'){
        //     info.next();
        //     info.addToken(token);
        //     continue;
        // }
        // #endif
        SignalDefault result = ParseToken(info);
    }
    info.inTokens = originalStream;
    info.usingTempStream = false;

    return SignalAttempt::SUCCESS;
}
// TODO: Move the struct elsewhere? Like a header?
struct MacroCall {
    RootMacro* rootMacro = nullptr;
    CertainMacro* certainMacro = nullptr;
    DynamicArray<TokenSpan> argumentRanges{}; // input to current range
    bool unwrapped=false;
    bool useDetailedArgs=false;
    DynamicArray<DynamicArray<const Token*>> detailedArguments{};
};
engone::Logger& operator<<(engone::Logger& logger, const TokenSpan& span){
    TokenRange tokRange = span.stream->get(span.start);
    tokRange.endIndex = span.end;
    return logger << tokRange;
}
SignalDefault FetchArguments(PreprocInfo& info, TokenSpan& tokenRange, MacroCall* call, MacroCall* newCall, DynamicArray<bool>& unwrappedArgs){
    using namespace engone;
    int& index = tokenRange.start;
    MEASURE;
    
    _MLOG(log::out << "Fetch args:\n";)

    Token token = tokenRange.stream->get(index);
    if(token != "("){
        // TODO: DON'T IGNORE FLAGS!
        // evalInfo.finalFlags = evalInfo.macroName.flags&(TOKEN_SUFFIX_LINE_FEED|TOKEN_SUFFIX_SPACE);
        return SignalDefault::SUCCESS;
    }
    index++;
    Token token2 = tokenRange.stream->get(index);
    if(token2 == ")"){
        index++;
        // TODO: DON'T IGNORE FLAGS!
        // evalInfo.finalFlags = token2.flags&(TOKEN_SUFFIX_LINE_FEED|TOKEN_SUFFIX_SPACE);
        return SignalDefault::SUCCESS; // no arguments
    }

    bool wasQuoted=false; // TODO: Fix wasQuoted and unwrap
    int parDepth = 0;
    TokenSpan argRange{};
    argRange.stream = tokenRange.stream;
    argRange.start = index;

    if(call){
        newCall->useDetailedArgs = call->useDetailedArgs;
    }

    auto add_arg = [&](){
        if(argRange.start != index-1){
            if(call && call->useDetailedArgs){
                newCall->detailedArguments.add({});
                for(int i=argRange.start;i<argRange.end;i++){
                    newCall->detailedArguments.last().add((const Token*)&argRange.stream->get(i));
                }
                _MLOG(log::out << " arg["<<(newCall->detailedArguments.size()-1)<<"] "<<argRange<<"\n";)
            } else {
                newCall->argumentRanges.add(argRange);
                _MLOG(log::out << " arg["<<(newCall->argumentRanges.size()-1)<<"] "<<argRange<<"\n";)
            }
        }
        argRange.start = index;
    };

    while(index < tokenRange.end){
        argRange.end = index;
        const Token& token = tokenRange.stream->get(index++);
        if(Equal(token,",")){
            // unwrapNext=false;
            if(parDepth==0){
                add_arg();
                argRange.start = index;
                continue;
            }
        }else if(Equal(token,"(")){
            parDepth++;
        }else if(Equal(token,")")){
            if(parDepth==0){
                add_arg();
                // TODO: DON'T IGNORE THESE FLAGS!
                // evalInfo.finalFlags = token.flags&(TOKEN_SUFFIX_LINE_FEED|TOKEN_SUFFIX_SPACE);
                break;
            }
            parDepth--;
        }

        if(call && Equal(token,"...")){
            if(argRange.start != index-1){
                ERR_HEAD3(token,"Infinite argument should be it's own argument. You cannot combine it with other tokens.\n\n";
                    ERR_LINE2(token.tokenIndex,"bad");
                )
            }
            add_arg();
            int argCount = call->useDetailedArgs ? call->detailedArguments.size() : call->argumentRanges.size();
            int infArgs = argCount - (call->certainMacro->parameters.size() - 1);
            if(call->useDetailedArgs){
                for(int i=call->certainMacro->indexOfVariadic;i<call->certainMacro->indexOfVariadic+infArgs;i++){
                    newCall->detailedArguments.add({});
                    for(int j=0;j<call->detailedArguments[i].size();j++){
                        newCall->detailedArguments.last().add(call->detailedArguments[i][j]);
                    }
                }
            } else {
                for(int i=call->certainMacro->indexOfVariadic;i<call->certainMacro->indexOfVariadic+infArgs;i++){
                    newCall->argumentRanges.add(call->argumentRanges[i]);
                }
            }
            if(!Equal(tokenRange.stream->get(index),",")&&!Equal(tokenRange.stream->get(index),")")){
                ERR_HEAD3(tokenRange.stream->get(index),"Infinite argument should be it's own argument. You cannot combine it with other tokens.\n\n";
                    ERR_LINE2(index,"bad");
                )
            }
        } else if(Equal(token,PREPROC_TERM)){ // annotation instead of directive?
            const Token& nextTok = tokenRange.stream->get(index);
            if(Equal(nextTok,"unwrap")){
                add_arg();
                newCall->unwrapped = true;
                int argCount = 1 + (newCall->useDetailedArgs ? newCall->detailedArguments.size() : newCall->argumentRanges.size());
                unwrappedArgs.resize(argCount);
                unwrappedArgs[argCount-1] = true;
                // Assert(("unwrap is broken with new macro calculations",false));
                _MLOG(log::out << log::MAGENTA<<"unwrap\n";)
                // unwrapNext=true;
                index++;
                argRange.start = index;
                continue;
            }
        }
    }
    return SignalDefault::SUCCESS;
}
SignalAttempt ParseMacro_fast(PreprocInfo& info, int attempt){
    using namespace engone;
    MEASURE;

    Token name = info.get(info.at()+1);
    RootMacro* rootMacro=0;
    if(!(rootMacro = info.matchMacro(name))){
        if(attempt){
            return SignalAttempt::BAD_ATTEMPT;
        }
        ERR_HEAD3(name, "Undefined macro '"<<name<<"'\n";)
        return SignalAttempt::FAILURE;
    }
    info.next();
    attempt=false;
    
    // TODO: Use struct TokenSpan { int start; int end; TokenStream* stream } instead of TokenRange.
    //  TokenRange contains the first token which isn't necessary.
    struct Env {
        TokenSpan range{};
        int callIndex=0;
        int ownsCall=-1; // env will pop the call when popped

        bool firstTime=true;
        bool unwrapOutput=false;
        int outputToCall=-1; // index of call
        // int calculationArgIndex=-1; // used with unwrap

        int finalFlags = 0;
    };
    // TODO: Use bucket arrays instead
    DynamicArray<const Token*> outputTokens{}; // TODO: Reuse a temporary array instead of doing a new allocation
    DynamicArray<i16> outputTokensFlags{}; // TODO: Reuse a temporary array instead of doing a new allocation
    // outputTokens._reserve(15000);
    DynamicArray<MacroCall> calls{};
    // calls._reserve(30);
    DynamicArray<Env> environments{};
    // environments._reserve(50);
    calls.add({});
    environments.add({});
    {
        MacroCall& initialCall = calls.last();
        Env& initialEnv = environments.last();
        initialEnv.outputToCall = -1;
        initialEnv.unwrapOutput = false;
        initialCall.rootMacro = rootMacro;
        initialEnv.callIndex = 0;
        initialEnv.ownsCall = 0;

        int argCount=0;
        DynamicArray<bool> unwrappedArgs{};
        if(!info.end()){
            TokenSpan argRange{};
            argRange.stream = info.inTokens;
            argRange.start = info.at()+1;
            argRange.end = info.length();

            FetchArguments(info,argRange, nullptr, &initialCall, unwrappedArgs);
            while(info.at() + 1 < argRange.start){
                info.next();
            }
            argCount = initialCall.useDetailedArgs ? initialCall.detailedArguments.size() : initialCall.argumentRanges.size();
            initialEnv.finalFlags = argRange.stream->get(argRange.start - 1).flags;
        }
        if(!initialCall.unwrapped || initialCall.useDetailedArgs){
            Assert(!initialCall.unwrapped); // how does unwrap work here?
            initialCall.certainMacro = rootMacro->matchArgCount(argCount);
            if(!initialCall.certainMacro){
                ERR_HEAD3(name, "Macro '"<<name<<"' cannot have "<<(argCount)<<" arguments.\n\n";
                    ERR_LINE2(name.tokenIndex,"bad");
                )
                return SignalAttempt::FAILURE;
            }
            _MLOG(log::out << "CertainMacro "<<argCount<<"\n";)
            initialEnv.range = initialCall.certainMacro->contentRange;
        } else {
            initialCall.useDetailedArgs = true;
            _MLOG(log::out << "Unwrapped args\n";)
            for(int i = initialCall.argumentRanges.size()-1; i >= 0;i--){
                environments.add({});
                Env& argEnv = environments.last();
                argEnv.range = initialCall.argumentRanges[i];
                argEnv.callIndex = calls.size()-1;
                if(i<unwrappedArgs.size())
                    argEnv.unwrapOutput = unwrappedArgs[i];
                argEnv.outputToCall = calls.size()-1;
            }
        }
    }
    // TODO: Add some extra logic to transfer flags from tokens, parameters and macro calls.
    //  It looks a little nicer. I have started but not finished it because it's not very important.

    // int limit = 40;
    // while (limit-->0){
    while (calls.size()<PREPROC_REC_LIMIT){
        Env& env = environments.last();
        MacroCall* call = nullptr;
        if(env.callIndex!=-1) {
            call = &calls.get(env.callIndex);
            if(!call->certainMacro && env.ownsCall==env.callIndex){
                Assert(call->rootMacro && call->unwrapped);
                int argCount = call->detailedArguments.size();
                // if(call->unwrapped){
                    call->certainMacro = call->rootMacro->matchArgCount(argCount);
                // }else{
                    // call->certainMacro = call->rootMacro->matchArgCount(call->argumentRanges.size());
                // }
                if(!call->certainMacro){
                    ERR_HEAD3(call->rootMacro->name, "Macro '"<<"?"<<"' cannot have "<<(argCount)<<" arguments.\n\n";
                        ERR_LINE2(call->rootMacro->name.tokenIndex,"bad");
                    )
                    continue;
                }
                _MLOG(log::out << "CertainMacro special "<<argCount<<"\n";)
                env.range = call->certainMacro->contentRange; 
            }
        }
        if(env.range.end == env.range.start) {
            _MLOG(log::out << "Env["<<(environments.size()-1)<<"] pop "<<(env.callIndex==-1 ? "-1" : calls[env.callIndex].rootMacro->name)<<"\n";)
            environments.pop();
            if(env.ownsCall!=-1){
                Assert(env.ownsCall==calls.size()-1);
                calls.pop();
            }

            if(environments.size()==0)
                break;
            continue;
        }
        _MLOG(log::out << "Env["<<(environments.size()-1)<<"] callref: "<<(env.callIndex==-1 ? "-1" : calls[env.callIndex].rootMacro->name) << " own: "<<(env.ownsCall==-1 ? "-1" : calls[env.ownsCall].rootMacro->name)<<"\n";)
        bool initialEnv = env.firstTime;
        if(env.firstTime){
            env.firstTime=false;
        }
        Assert(env.range.start < env.range.stream->length());
        const Token& token = env.range.stream->get(env.range.start++);

        int parameterIndex = -1;
        RootMacro* deeperMacro = nullptr;

        if(env.callIndex!=-1) {
            if(call->certainMacro && -1!=(parameterIndex = call->certainMacro->matchArg(token))){ // TOOD: rename matchArg to matchParameter
                // @optimize TODO: Parameters are evaluated in full each time they occur.
                //   This is inefficient with many parameters but a parameter will
                //   usually only occur once or twice. Even so it could be worth thinking
                //   of a way to optimize it.
                int argCount = call->useDetailedArgs ? call->detailedArguments.size() : call->argumentRanges.size();
                int infArgs = argCount - (call->certainMacro->parameters.size() - 1);
                int paramStart = parameterIndex;
                int paramCount = 1;
                if(call->certainMacro->isVariadic()){
                    if(call->certainMacro->indexOfVariadic < parameterIndex) {
                        paramStart += infArgs-1;
                    }
                }
                if(parameterIndex==call->certainMacro->indexOfVariadic)
                    paramCount = infArgs;

                _MLOG(log::out << "Param "<<token<<" Inf["<<paramStart<<"-"<<(paramStart+paramCount)<<"]\n";)

                if(call->useDetailedArgs){
                    for(int i = paramStart + paramCount -1;i>=paramStart;i--){
                        for(int j=0;j<call->detailedArguments[i].size();j++){
                            const Token* argToken = call->detailedArguments[i][j];
                            if(env.outputToCall==-1){
                                _MLOG(log::out << " output " << *argToken<<"\n";)
                                outputTokens.add(argToken);
                                outputTokensFlags.add(argToken->flags);
                            } else {
                                Assert(env.outputToCall>=0&&env.outputToCall<calls.size());
                                MacroCall& outputCall = calls[env.outputToCall];
                                if(initialEnv)
                                    outputCall.detailedArguments.add({});
                                if(env.unwrapOutput && Equal(*argToken,",")){
                                    _MLOG(log::out << " output arg comma\n";)
                                    outputCall.detailedArguments.add({});
                                } else {
                                    _MLOG(log::out << " output arg["<<(outputCall.detailedArguments.size()-1)<<"] "<<*argToken<<"\n";)
                                    outputCall.detailedArguments.last().add(argToken);
                                }
                                // TOOD: What about final flags?
                            }
                        }
                    }
                } else {
                    for(int i = paramStart + paramCount -1;i>=paramStart;i--){
                        Assert(i<call->argumentRanges.size());
                        Env argEnv{};
                        argEnv.range = call->argumentRanges[i];
                        if(env.callIndex==0){
                            // log::out << "Saw param, global env\n";
                            argEnv.callIndex = -1;
                        } else {
                            // log::out << "Saw param, local env\n";
                            argEnv.callIndex = env.callIndex-1;
                        }
                        if(i == paramStart + paramCount - 1)
                            argEnv.finalFlags = token.flags;
                        if(env.range.start == env.range.end)
                            argEnv.finalFlags &= ~(TOKEN_SUFFIX_LINE_FEED); 
                        environments.add(argEnv);
                    }
                }
                continue;
            }
        }
        if((deeperMacro = info.matchMacro(token))){
            _MLOG(log::out << "Macro "<<token<<"\n";)

            environments.add({});
            calls.add({});
            Env& innerEnv = environments.last();
            MacroCall& innerCall = calls.last();
            innerEnv.outputToCall = env.outputToCall;
            innerEnv.unwrapOutput = env.unwrapOutput;
            innerEnv.callIndex = calls.size()-1;
            innerEnv.ownsCall = calls.size()-1;
            innerCall.rootMacro = deeperMacro;

            int argCount=0;
            DynamicArray<bool> unwrappedArgs{};
            if(env.range.start < env.range.end){
                FetchArguments(info,env.range, call, &innerCall, unwrappedArgs);
                argCount = innerCall.useDetailedArgs ? innerCall.detailedArguments.size() : innerCall.argumentRanges.size();
            }
            innerEnv.finalFlags = env.range.stream->get(env.range.start - 1).flags;

            if(!innerCall.unwrapped || innerCall.useDetailedArgs){
                Assert(!innerCall.unwrapped); // how does unwrap work here?
                innerCall.certainMacro = deeperMacro->matchArgCount(argCount);
                if(!innerCall.certainMacro){
                    ERR_HEAD3(token, "Macro '"<<token<<"' cannot have "<<(argCount)<<" arguments.\n\n";
                        ERR_LINE2(token.tokenIndex,"bad");
                    )
                    continue;
                }
                _MLOG(log::out << "CertainMacro "<<argCount<<"\n";)
                innerEnv.range = innerCall.certainMacro->contentRange;
            } else {
                innerCall.useDetailedArgs = true;
                _MLOG(log::out << "Unwrapped args\n";)
                for(int i = innerCall.argumentRanges.size()-1; i >= 0;i--){
                    environments.add({});
                    Env& argEnv = environments.last();
                    argEnv.range = innerCall.argumentRanges[i];
                    argEnv.callIndex = calls.size()-1;
                    if(i<unwrappedArgs.size())
                        argEnv.unwrapOutput = unwrappedArgs[i];
                    argEnv.outputToCall = calls.size()-1;
                }
            }
            continue;
        }
        if(env.outputToCall==-1){
            _MLOG(log::out << " output " << token<<"\n";)
            outputTokens.add(&token);
            if(env.range.start == env.range.end){
                outputTokensFlags.add(env.finalFlags|(token.flags&(TOKEN_MASK_QUOTED)));
            } else
                outputTokensFlags.add(token.flags);
        } else {
            Assert(env.outputToCall>=0&&env.outputToCall<calls.size());
            MacroCall& outputCall = calls[env.outputToCall];
            Assert(outputCall.useDetailedArgs);
            if(initialEnv)
                outputCall.detailedArguments.add({});
            if(env.unwrapOutput && Equal(token,",")){
                _MLOG(log::out << " output arg comma\n";)
                outputCall.detailedArguments.add({});
            } else {
                _MLOG(log::out << " output arg["<<(outputCall.detailedArguments.size()-1)<<"] "<<token<<"\n";)
                outputCall.detailedArguments.last().add(&token);
            }
        }
    }
    if(calls.size()>=PREPROC_REC_LIMIT){
        Env& env = environments.last();
        MacroCall* call = env.callIndex!=-1 ? &calls[env.callIndex] : nullptr;
        Token& tok = call ? call->certainMacro->name : info.now();
        ERR_HEAD3(tok, "Reached recursion limit of "<<PREPROC_REC_LIMIT<<" for macros\n";
            ERR_LINE2(tok.tokenIndex,"bad");
        )
        return SignalAttempt::FAILURE;
    }
    // log::out << "output tokens: "<<outputTokens.size()<<"\n";

    if(!info.tempStream) {
        info.tempStream = TokenStream::Create();
    }

    // This shouldn't happen.
    // Maybe caused by multithreading or recursion due to ParseToken below.
    Assert(!info.usingTempStream);

    info.tempStream->tokenData.used = 0;
    info.tempStream->tokens.used = 0;
    info.tempStream->streamName = info.inTokens->streamName;
    info.tempStream->readHead = 0;
    info.usingTempStream = true;

    // TODO: This can be done in the main loop instead.
    // No need for outputTokens. Concatenation can be done there too.
    char buffer[256];
    for(int i=0;i<(int)outputTokens.size();i++){
        Token baseToken = *outputTokens[i];
        SignalAttempt result = SignalAttempt::BAD_ATTEMPT;
        if(Equal(baseToken,"#") && outputTokens.size()>i+1){
            result = ParsePredefinedMacro(info,*outputTokens[i+1], baseToken,buffer);
            if(result == SignalAttempt::SUCCESS){
                i++;
                baseToken.flags &= (outputTokensFlags[i]&(~TOKEN_MASK_QUOTED))|(TOKEN_MASK_QUOTED);
            }
        }
        if(result == SignalAttempt::BAD_ATTEMPT){
            baseToken.flags &= (outputTokensFlags[i]&(~TOKEN_MASK_QUOTED))|(TOKEN_MASK_QUOTED);
        }
        // baseToken.flags = evalInfo.output[i].flags; // TODO: WHAT ABOUT EXTRA FLAGS? NEW LINES DISAPPEAR WHEN REPLACING MACRO NAME!
        uint64 offset = info.tempStream->tokenData.used;
        info.tempStream->addData(baseToken);
        // info.outTokens->addData(baseToken);
        baseToken.str = (char*)offset;
        WHILE_TRUE {
            
            if(!Equal(*outputTokens[i],"#") && 0==(baseToken.flags&TOKEN_MASK_QUOTED) && i+3<(int)outputTokens.size() && Equal(*outputTokens[i+1],"#") && Equal(*outputTokens[i+2],"#")){
                // Assert(false);
                Token token2 = *outputTokens[i+3];
                if(Equal(token2,"#")){
                    SignalAttempt result = ParsePredefinedMacro(info,*outputTokens[i+4], token2,buffer);
                    if(result==SignalAttempt::SUCCESS){
                        i++;
                    }
                } 
                if(0==(outputTokens[i+3]->flags&TOKEN_MASK_QUOTED)) {
                    if(result == SignalAttempt::BAD_ATTEMPT){
                        token2.flags = outputTokensFlags[i];
                    }
                    info.tempStream->addData(token2);
                    // info.outTokens->addData(token2);
                    baseToken.length += token2.length;
                    i+=3;
                    continue;
                }
            }
            info.tempStream->addToken(baseToken);
            // info.outTokens->addToken(baseToken);
            
            break;
        }
    }
    info.tempStream->finalizePointers();

    // log::out << "####\n";
    // info.tempStream->print();
    // log::out<<"\n";
   
    TokenStream* originalStream = info.inTokens;
    info.inTokens = info.tempStream; // Note: don't modify inTokens
    // TODO: Don't parse defines in macro evaluation. A flag in PreprocInfo would do.
    // TODO: If you define a new macro and then evaluate it you might be able to achieve infinite recursion.
    // There is an assert above which "locks" tempStream so it will be fine right now.
    while(!info.end()){
        // #ifndef SLOW_PREPROCESSOR
        // // Quick check, doesn't matter if it's quoted.
        // // ParseToken will not need to parse macros which is why we can do these
        // // checks
        // // This broke because of concatenation with ##, revisit this code
        // Token& token = info.get(info.at()+1);
        // if(*token.str != '#' && *token.str != '_'){
        //     info.next();
        //     info.addToken(token);
        //     continue;
        // }
        // if(*token.str == '#' && !(token.flags & TOKEN_MASK_QUOTED) &&
        //     !(token.flags & TOKEN_SUFFIX_SPACE) && !(token.flags & TOKEN_SUFFIX_LINE_FEED)){
        //     // ParseDefine also makes sure that #define doesn't occur but we have to 
        //     // do it here to in case tokens were merged resulting in #define.
        //     if(Equal(info.get(info.at()+2),"define") || Equal(info.get(info.at()+2),"multidefine")) {
        //         ERR_HEAD3(info.get(info.at()+2), "Macro definitions inside macros are not allowed.\n\n";
        //             ERR_LINE2(info.at()+2,"not allowed");
        //         )
        //         info.next();
        //         info.addToken(token);
        //         continue;
        //     }
        // }
        // #endif
        SignalDefault result = ParseToken(info);
    }
    info.inTokens = originalStream;
    info.usingTempStream = false;

    // log::out << "####\n";
    // info.outTokens->finalizePointers();
    // info.outTokens->print();

    return SignalAttempt::SUCCESS;
}


// Default parse function.
SignalDefault ParseToken(PreprocInfo& info){
    using namespace engone;
    // MEASURE;
    // if(info.usingTempStream /*<- true when doing macros*/ && (info.get(info.at()+1).flags&TOKEN_MASK_QUOTED)==0 && 
    //     Equal(info.get(info.at()+2),"#")&&Equal(info.get(info.at()+3),"#") && 
    //     (info.get(info.at()+3).flags&TOKEN_MASK_QUOTED)==0)
    // {
    //     int result = ParseToken(info);
    //     Assert(result);
    //     info.next(); info.next(); // skip ##
    //     result = ParseToken(info);
    //     Assert(result);

    //     Token token = info.next();
    //     Token second = info.next();

    //     u64 offset = info.outTokens->tokenData.used; // get offset before adding data
    //     info.outTokens->addData(token);
    //     info.outTokens->addData(second);
    //     token.str = (char*)offset;
    //     token.length = token.length + second.length;
    //     info.outTokens->addToken(token);

    //     _MLOG(log::out << "Append "<<token<<second<<"\n";)
    //     return SignalAttempt::SUCCESS;
    // }
    SignalAttempt result = ParseDefine(info,true);
    if(result == SignalAttempt::BAD_ATTEMPT) {
        if(Equal(info.get(info.at()+1),"#")){
            Token token = info.get(info.at()+2);
            Token outToken{};
            char buffer[256];
            result = ParsePredefinedMacro(info,token,outToken,buffer);
            if(result==SignalAttempt::SUCCESS){
                info.next();
                info.next();
                info.addToken(outToken);
                _MLOG(log::out << "Append "<<outToken<<"\n";)
                return SignalDefault::SUCCESS;
            }
        }
    }
    if(result == SignalAttempt::BAD_ATTEMPT)
        result = ParseUndef(info,true);
    if(result == SignalAttempt::BAD_ATTEMPT)
        result = ParseIfdef(info,true);
    if(result == SignalAttempt::BAD_ATTEMPT)
        #ifdef SLOW_PREPROCESSOR
            result = ParseMacro(info,true);
        #else
            result = ParseMacro_fast(info,true);
        #endif
    if(result == SignalAttempt::BAD_ATTEMPT)
        result = ParseInclude(info,true);
    if(result == SignalAttempt::BAD_ATTEMPT)
        result = ParseImport(info,true);
    if(result == SignalAttempt::BAD_ATTEMPT)
        result = ParseLink(info,true);
    
    if(result==SignalAttempt::SUCCESS)
        return SignalDefault::SUCCESS;
    if(result != SignalAttempt::BAD_ATTEMPT){
        return CastSignal(result);
    }

    Token token = info.next();
    _MLOG(log::out << "Append "<<token<<"\n";)
    info.addToken(token);
    return SignalDefault::SUCCESS;
}
TokenStream* Preprocess(CompileInfo* compileInfo, TokenStream* inTokens){
    using namespace engone;
    MEASURE;
    
    PreprocInfo info{};
    info.compileInfo = compileInfo;
    info.outTokens = TokenStream::Create();
    info.outTokens->tokenData.resize(100+inTokens->tokenData.max*3); // hopeful estimation
    info.inTokens = inTokens; // Note: don't modify inTokens
    info.inTokens->readHead = 0;

    while(!info.end()){
        // Since # and _ doesn't account for macros we do an extra check for alpha character.
        // If so, it might a macro and we have to ParseToken
        // Token& token = info.get(info.at()+1);
        // if(*token.str != '#' && *token.str != '_' && !((*token.str > 'a' && *token.str < 'z') || (*token.str > 'A' && *token.str < 'Z'))){
        //     info.next();
        //     info.addToken(token);
        //     continue;
        // }
        SignalDefault result = ParseToken(info);
    }
    
    // inTokens->tokens.resize(0);
    // inTokens->tokenData.resize(0);
    // inTokens->tokens = info.outTokens->tokens;
    // inTokens->tokenData = info.outTokens->tokenData;
    // info.inTokens->finalizePointers();
    // info.outTokens->tokens = {0};
    // info.outTokens->tokenData = {0};
    // TokenStream::Destroy(info.outTokens);
    if(info.tempStream)
        TokenStream::Destroy(info.tempStream);

    info.compileInfo->errors += info.errors;
    
    info.outTokens->streamName = info.inTokens->streamName;
    info.outTokens->finalizePointers();

    return info.outTokens;
}
void PreprocessImports(CompileInfo* compileInfo, TokenStream* inTokens){
    using namespace engone;
    MEASURE;
    
    PreprocInfo info{};
    info.compileInfo = compileInfo;
    info.inTokens = inTokens; // Note: don't modify inTokens
    info.inTokens->readHead = 0;

    while(!info.end()){
        Token token = info.get(info.at()+1);
        SignalAttempt result = ParseDirective(info, true, "import");
        if(result!=SignalAttempt::SUCCESS){
            info.next();
            continue;
        }

        Token name = info.get(info.at()+1);
        if(!(name.flags&TOKEN_MASK_QUOTED)){
            ERR_HEAD3(name, "expected a string not "<<name<<"\n";
            )
            continue;
        }
        info.next();
        
        token = info.get(info.at()+1);
        if(Equal(token,"as")){
            info.next();
            token = info.get(info.at()+1);
            if(token.flags & TOKEN_MASK_QUOTED){
                ERR_HEAD3(token, "don't use quotes with "<<log::YELLOW<<"as\n";
                )
                continue;
            }
            info.next();
            info.inTokens->addImport(name,token);
        } else {
            info.inTokens->addImport(name,"");
        }
    }
    
    info.compileInfo->errors += info.errors;
}
#undef _macros