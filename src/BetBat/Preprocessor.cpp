#include "BetBat/Preprocessor.h"

// preprocessor needs CompileInfo to handle global caching of include streams
#include "BetBat/Compiler.h"

#undef ERR_HEAD
#undef ERR_LINE
#undef ERR_HEAD2
#undef ERR_HEAD2L
#undef WARN_HEAD
#undef WARN_END
#undef ERR_END

#define ERR_HEAD2(T) info.compileInfo->errors++;engone::log::out << ERR_CUSTOM(info.inTokens->streamName, T.line,T.column,"Preproc. error","E0000")
#define ERR_HEAD2L(T) info.compileInfo->errors++;engone::log::out << ERR_CUSTOM(info.inTokens->streamName, T.line, T.column, "Preproc. error","E0000")
#define WARN_HEAD(T) info.compileInfo->warnings++;engone::log::out << WARN_CUSTOM(info.inTokens->streamName, T.line, T.column, "Preproc. warning","W0000")
#define ERR_END MSG_END
#define WARN_END MSG_END

// #define WARN_HEAD(T, M) info.compileInfo->warnings++;engone::log::out << WARN_CUSTOM(info.tokens->streamName,T.line,T.column,"Parse warning","W0000") << M
#define ERR_HEAD(T, M) info.compileInfo->errors++;engone::log::out << ERR_DEFAULT_T(info.inTokens,T,"Parse error","E0000") << M
#define ERR_LINE(I, M) PrintCode(I, info.inTokens, M)

// #define MLOG_MATCH(X) X
#define MLOG_MATCH(X)

// used for std::vector<int> tokens
#define INT_ENCODE(I) (I<<3)
#define INT_DECODE(I) (I>>3)

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
RootMacro* PreprocInfo::matchMacro(Token& token){
    if(token.flags&TOKEN_QUOTED)
        return 0;
    auto pair = macros.find(token);
    if(pair==macros.end()){
        return 0;
    }
    // if(pair->second.called){
    //     auto& info = *this;
    //     ERR_HEAD2(token) << "Infinite recursion not allowed ("<<token<<")\n";
    //     return 0;
    // }
    _MLOG(MLOG_MATCH(engone::log::out << engone::log::CYAN << "match root "<<token<<"\n";))
    return &pair->second;
}
int CertainMacro::matchArg(Token& token){
    if(token.flags&TOKEN_QUOTED)
        return -1;
    for(int i=0;i<(int)argumentNames.size();i++){
        std::string& str = argumentNames[i];
        if(token == str.c_str()){
            _MLOG(MLOG_MATCH(engone::log::out <<engone::log::MAGENTA<< "match tok with arg "<<token<<"\n";))
            return i;
        }
    }
    return -1;
}
CertainMacro* RootMacro::matchArgCount(int count, bool includeInf){
    auto pair = certainMacros.find(count);
    if(pair==certainMacros.end()){
        if(!includeInf)
            return 0;
        if(hasInfinite){
            if((int)infDefined.argumentNames.size()-1<=count){
                _MLOG(MLOG_MATCH(engone::log::out<<engone::log::MAGENTA << "match argcount "<<count<<" with "<< infDefined.argumentNames.size()<<" (inf)\n";))
                return &infDefined;
            }else{
                return 0;
            }
        }else{
            return 0;   
        }
    }else{
        _MLOG(MLOG_MATCH(engone::log::out <<engone::log::MAGENTA<< "match argcount "<<count<<" with "<< pair->second.argumentNames.size()<<"\n";))
        return &pair->second;
    }
}
void PreprocInfo::nextline(){
    int extra=inTokens->readHead==0;
    if(inTokens->readHead==0){
        next();
    }
    Token nowT = now();
    if(nowT.flags&TOKEN_SUFFIX_LINE_FEED){
        next();
        return;
    }
    while(!end()){
        if(nowT.flags&TOKEN_SUFFIX_LINE_FEED){
            break;
        }
        nowT = next();
    }
}
bool EvalInfo::matchSuperArg(Token& name, CertainMacro*& superMacro, Arguments*& args, int& argIndex){
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
    uint64 offset = outTokens->tokenData.used;
    outTokens->addData(inToken);
    inToken.str = (char*)offset;
    outTokens->addToken(inToken);
}
int ParseDirective(PreprocInfo& info, bool attempt, const char* str){
    using namespace engone;
    Token token = info.get(info.at()+1);
    if(token!=PREPROC_TERM || (token.flags&TOKEN_QUOTED)){
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }else{
            ERR_HEAD2(token) << "Expected " PREPROC_TERM " since this wasn't an attempt found '"<<token<<"'\n";
            ERR_END
            return PARSE_ERROR;
        }
    }
    if(token.flags&TOKEN_SUFFIX_SPACE){
        ERR_HEAD2(token) << "Cannot have space after preprocessor directive\n";
        ERR_END
        info.nextline();
        return PARSE_ERROR;
    }
    token = info.get(info.at()+2);
    if(token != str){
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }else{
            ERR_HEAD2(token) << "Expected "<<str<<" found '"<<token<<"'\n";
            ERR_END
            return PARSE_ERROR;
        }
        return PARSE_ERROR;
    }
    info.next();
    info.next();
    return PARSE_SUCCESS;  
}
int ParseDefine(PreprocInfo& info, bool attempt){
    using namespace engone;
    MEASURE;
    Token token = info.get(info.at()+1);
    if(token!=PREPROC_TERM || (token.flags&TOKEN_QUOTED)){
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }else{
            ERR_HEAD2(token) << "Expected " PREPROC_TERM " since this wasn't an attempt found '"<<token<<"'\n";
            ERR_END
            return PARSE_ERROR;
        }
    }
    if(token.flags&TOKEN_SUFFIX_SPACE){
        ERR_HEAD2(token) << "Cannot have space after preprocessor directive\n";
        ERR_END
        info.nextline();
        return PARSE_ERROR;
    }
    token = info.get(info.at()+2);
    
    // TODO: only allow define on new line?
    
    bool multiline = false;
    if(token=="define"){
        
    }else if(token=="multidefine"){
        multiline=true;
    }else{
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }else{
            ERR_HEAD2(token) << "Expected define found '"<<token<<"'\n";
            ERR_END
            return PARSE_ERROR;
        }
        return PARSE_ERROR;
    }
    token = info.next();
    token = info.next();
    
    Token name = info.next();
    
    if(name.flags&TOKEN_QUOTED){
        ERR_HEAD2(name) << "Define name cannot be quoted "<<name<<"\n";
        ERR_END
        return PARSE_ERROR;
    }
    
    // if(info.matchMacro(name)){
    //     WARN_HEAD(name) << "Intentional redefinition of '"<<name<<"'?\n";   
    // }
    
    int suffixFlags = name.flags;
    CertainMacro defTemp{};
    if((name.flags&TOKEN_SUFFIX_SPACE) || (name.flags&TOKEN_SUFFIX_LINE_FEED)){
    
    }else{
        Token token = info.next();
        if(token!="("){
            ERR_HEAD2(token) << "Unexpected '"<<token<<"' did you forget a space or '(' to indicate arguments?\n";
            ERR_END
            return PARSE_ERROR;
        }
        
        while(!info.end()){
            Token token = info.next();
            if(token==")"){
                suffixFlags = token.flags;
                break;   
            }
            
            if(token=="...") {
                defTemp.infiniteArg = defTemp.argumentNames.size();
            }
            // TODO: token must be valid name
            defTemp.argumentNames.push_back(token);
            token = info.next();
            if(token==")"){
                suffixFlags = token.flags;
                break;   
            } else if(token==","){
                // cool
            } else{
                ERR_HEAD2(token) << "Expected , or ) found '"<<token<<"'\n";   
                ERR_END
            }
        }
        
        _MLOG(log::out << "loaded "<<defTemp.argumentNames.size()<<" arg names";
        if(defTemp.infiniteArg==-1)
            log::out << "\n";
        else
            log::out << " (infinite)\n";)
    
    }
    RootMacro* rootDefined = info.matchMacro(name);
    if(!rootDefined){
        rootDefined = &(info.macros[name] = {});
    }
    
    CertainMacro* defined = 0;
    if(defTemp.infiniteArg==-1){
        defined = rootDefined->matchArgCount(defTemp.argumentNames.size(),false);
        if(defined){
            WARN_HEAD(name) << "Intentional redefinition of '"<<name<<"'?\n";
            WARN_END
            *defined = defTemp;
        }else{
            // move defTemp info appropriate place in rootDefined
            defined = &(rootDefined->certainMacros[(int)defTemp.argumentNames.size()] = defTemp);
        }
    }else{
        if(rootDefined->hasInfinite){
            WARN_HEAD(name) << "Intentional redefinition of '"<<name<<"'?\n";   
            WARN_END
        }
        rootDefined->hasInfinite = true;
        rootDefined->infDefined = defTemp;
        defined = &rootDefined->infDefined;
    }
    
    int startToken = info.at()+1;
    int endToken = startToken;
    if(!multiline&&(suffixFlags&TOKEN_SUFFIX_LINE_FEED))
        goto while_skip_194;
    while(!info.end()){
        Token token = info.next();
        if(token==PREPROC_TERM){
            if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED)){
                ERR_HEAD2(token) << "SPACE AFTER "<<token<<"!\n";
                ERR_END
                return PARSE_ERROR;
            }
            Token token = info.get(info.at()+1);
            if(token=="enddef"){
                info.next();
                endToken = info.at()-1;
                break;
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
            ERR_HEAD2L(token) << "Missing enddef!\n";
            ERR_END
        }
    }
    while_skip_194:
    
    defined->start = startToken;
    defined->end = endToken;
    int count = endToken-startToken;
    int argc = defined->argumentNames.size();
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
    return PARSE_SUCCESS;
}
int ParseUndef(PreprocInfo& info, bool attempt){
    using namespace engone;

    // TODO: check of end

    Token token = info.get(info.at()+1);
    if(token!=PREPROC_TERM || (token.flags&TOKEN_QUOTED)){
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }else{
            ERR_HEAD2(token) << "Expected " PREPROC_TERM " since this wasn't an attempt found '"<<token<<"'\n";
            ERR_END
            return PARSE_ERROR;
        }
    }
    if(token.flags&TOKEN_SUFFIX_SPACE){
        ERR_HEAD2(token) << "Cannot have space after preprocessor directive\n";
        ERR_END
        return PARSE_ERROR;
    }
    token = info.get(info.at()+2);
    if(token!="undef"){
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }
        ERR_HEAD2(token) << "Expected undef (since this wasn't an attempt)\n";
        ERR_END
        return PARSE_ERROR;
    }
    if(token.flags&TOKEN_SUFFIX_LINE_FEED){
        ERR_HEAD2L(token) << "Unexpected line feed (expected a macro name)\n";
        ERR_END
        return PARSE_ERROR;
    }

    info.next();
    info.next();
    
    Token name = info.next();

    auto pair = info.macros.find(name);
    if(pair==info.macros.end()){
        WARN_HEAD(name) << "Cannot undefine '"<<name<<"' (doesn't exist)\n";
        WARN_END
        return PARSE_SUCCESS;
    }

    if(name.flags&TOKEN_SUFFIX_LINE_FEED){
        info.macros.erase(pair);
        return PARSE_SUCCESS;
    }
    token = info.next();

    if(token!="..." && !IsInteger(token)){
        ERR_HEAD2(token) << "Expected an integer (or ...) to indicate argument count not '"<<token<<"'\n";
        ERR_END 
        return PARSE_ERROR;
    }
    if(!(token.flags&TOKEN_SUFFIX_LINE_FEED)){
        ERR_HEAD2(token) << "Expected line feed after '"<<token<<"'\n";
        ERR_END
        return PARSE_ERROR;
    }

    if(token=="..."){
        if(!pair->second.hasInfinite){
            WARN_HEAD(name) << "Cannot undefine '"<<name<<"' [inf] (doesn't exist)\n";
            WARN_END
            return PARSE_SUCCESS;
        }
        pair->second.hasInfinite=false;
        pair->second.infDefined = {};
        return PARSE_SUCCESS;
    }

    int argCount = ConvertInteger(token);
    if(argCount<0){
        ERR_HEAD2(token) << "ArgCount cannot be negative '"<<argCount<<"'\n";
        ERR_END
        return PARSE_ERROR;
    }

    auto cert = pair->second.certainMacros.find(argCount);
    if(cert==pair->second.certainMacros.end()){
        WARN_HEAD(name) << "Cannot undefine '"<<name<<"' ["<<argCount<<" args] (doesn't exist)\n";
        WARN_END
        return PARSE_SUCCESS;
    }
    pair->second.certainMacros.erase(cert);

    return PARSE_SUCCESS;
}

int ParseImport(PreprocInfo& info, bool attempt){
    using namespace engone;
    MEASURE;
    Token token = info.get(info.at()+1);
    int result = ParseDirective(info, attempt, "import");
    if(result==PARSE_BAD_ATTEMPT)
        return PARSE_BAD_ATTEMPT;

    Token name = info.get(info.at()+1);
    if(!(name.flags&TOKEN_QUOTED)){
        ERR_HEAD2(name) << "expected a string not "<<name<<"\n";
        ERR_END
        return PARSE_ERROR;
    }
    info.next();
    
    token = info.get(info.at()+1);
    if(Equal(token,"as")){
        info.next();
        token = info.get(info.at()+1);
        if(token.flags & TOKEN_QUOTED){
            ERR_HEAD2(token) << "don't use quotes with "<<log::YELLOW<<"as\n";
            ERR_END
            return PARSE_ERROR;
        }
        info.next();
        info.inTokens->addImport(name,token);
    } else {
        info.inTokens->addImport(name,"");
    }
    
    return PARSE_SUCCESS;
}
int ParseInclude(PreprocInfo& info, bool attempt){
    using namespace engone;
    MEASURE;
    Token hashtag = info.get(info.at()+1);
    int result = ParseDirective(info, attempt, "include");
    if(result==PARSE_BAD_ATTEMPT)
        return PARSE_BAD_ATTEMPT;

    int tokIndex = info.at()+1;
    // needed for an error, saving it here in case the code changes
    // and info.at changes it output
    Token token = info.get(tokIndex);
    if(!(token.flags&TOKEN_QUOTED)){
        ERR_HEAD2(token) << "expected a string not "<<token<<"\n";
        ERR_END
        return PARSE_ERROR;
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
    
    //-- Search cwd or absolute path
    else if(FileExist(filepath)){
        fullpath = filepath;
        if(!fullpath.isAbsolute())
            fullpath = fullpath.getAbsolute();
        // fullpath = engone::GetWorkingDirectory() + "/" + filepath;
        // ReplaceChar((char*)fullpath.data(),fullpath.length(),'\\','/');
    }
     
        //-- Search cwd or absolute path
        // else if(FileExist(importName)){
        //     fullPath = importName;
        //     if(!fullPath.isAbsolute())
        //         fullPath = fullPath.getAbsolute();
        // }
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
        ERR_HEAD(token, "Could not include '"<<filepath<<"' (not found). Format is not appended automatically (while import does append .btb).\n\n";
            ERR_LINE(tokIndex,"not found");
        )
        return PARSE_ERROR;
    }

    Assert(info.compileInfo)
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
        ERR_HEAD2(token) << "Error with token stream for "<<filepath<<" (bug in the compiler!)\n";
        ERR_END
        return PARSE_ERROR;
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
    
    return PARSE_SUCCESS;
}

int ParseToken(PreprocInfo& info);
// both ifdef and ifndef
int ParseIfdef(PreprocInfo& info, bool attempt){
    using namespace engone;
    bool notDefined=false;

    // Token ch = info.get(info.at()+1);
    // Token ch2 = info.get(info.at()+2);
    // if(ch2=="#"){
    //     log::out << "maybe stop\n";

    // }

    int result = ParseDirective(info,attempt,"ifdef");
    if(result==PARSE_BAD_ATTEMPT){
        result = ParseDirective(info,attempt,"ifndef");
        notDefined=true;
    }
    if(result==PARSE_ERROR)
        return PARSE_ERROR;
    if(result==PARSE_BAD_ATTEMPT)
        return PARSE_BAD_ATTEMPT;
    // otherwise success!
    
    attempt = false;
    Token name = info.next();
    
    RootMacro* defined = 0;
    bool active = info.matchMacro(name);
    if(notDefined)
        active = !active;
    
    int depth = 0;
    int error = PARSE_SUCCESS;
    // bool hadElse=false;
    _MLOG(log::out << log::GRAY<< "   ifdef - "<<name<<"\n";)
    while(!info.end()){
        Token token = info.get(info.at()+1);
        if(token==PREPROC_TERM){
            if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED)){
                ERR_HEAD2(token) << "SPACE AFTER "<<token<<"!\n";
                ERR_END
                return PARSE_ERROR;
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
            result = ParseToken(info);
        }else{
            Token skip = info.next();
            // log::out << log::GRAY<<" skip "<<skip << "\n";
        }
        if(info.end()){
            ERR_HEAD2L(info.get(info.length()-1)) << "Missing endif somewhere for ifdef or ifndef\n";
            ERR_END
            return PARSE_ERROR;   
        }
    }
    //  log::out << "     exit  ifdef loop\n";
    return error;
}
int ParsePredefinedMacro(PreprocInfo& info, bool attempt){
    using namespace engone;
    MEASURE;
    Token token = info.get(info.at()+1);
    // early exit
    if(!token.str || token.length==0 || *token.str != '_') {
        if(attempt)
            return PARSE_BAD_ATTEMPT;
        else
            return PARSE_ERROR;
    }
    // NOTE: One idea is to allow __LINE__ (double underscore) too
    //   but this would slow down the compiler a tiny bit so I am not going to.
    //   This mindset will be used elsewhere to and will propably provide more
    //   performance and less complexity.
    if(Equal(token,"_LINE_")){
        info.next();

        std::string temp = std::to_string(token.line);

        token.str = (char*)temp.data();
        token.length = temp.length();
        
        _MLOG(log::out << "Append "<<token<<"\n";)
        info.addToken(token);
        return PARSE_SUCCESS;
    } else if(Equal(token,"_COLUMN_")) {
        info.next();

        std::string temp = std::to_string(token.column);

        token.str = (char*)temp.data();
        token.length = temp.length();
        
        _MLOG(log::out << "Append "<<token<<"\n";)
        info.addToken(token);
        return PARSE_SUCCESS;
    } else if(Equal(token,"_FILE_")) {
        info.next();

        token.str = (char*)info.inTokens->streamName.data();
        token.length = info.inTokens->streamName.length();
        
        _MLOG(log::out << "Append "<<token<<"\n";)
        info.addToken(token);
        return PARSE_SUCCESS;
    } else if(Equal(token,"_FILENAME_")) {
        info.next();

        int lastSlash = -1;
        for(int i=0;i<(int)info.inTokens->streamName.length();i++){
            if(info.inTokens->streamName[i] == '/') {
                lastSlash = i;
                break;
            }
        }
        token.str = (char*)info.inTokens->streamName.data();
        token.length = info.inTokens->streamName.length();
        if(lastSlash != -1 && ( info.inTokens->streamName.size() == 0
             || info.inTokens->streamName.back() != '/'))
        {
             token.str += lastSlash + 1;
             token.length -= lastSlash + 1;
        }
        
        _MLOG(log::out << "Append "<<token<<"\n";)
        info.addToken(token);
        return PARSE_SUCCESS;
    } else if(Equal(token,"_UNIQUE_")) {
        info.next();

        i32 num = info.compileInfo->globalUniqueCounter++;

        std::string temp = std::to_string(num);
        token.str = (char*)temp.data();
        token.length = temp.length();
        
        _MLOG(log::out << "Append "<<token<<"\n";)
        info.addToken(token);
        return PARSE_SUCCESS;
    } else {
        if(attempt)
            return PARSE_BAD_ATTEMPT;
        else
            return PARSE_ERROR;
    }
}
void Transfer(PreprocInfo& info, TokenList& from, TokenList& to, bool quoted, bool unwrap=false,Arguments* args=0,int* argIndex=0){
    // TODO: optimize with some resize and memcpy?
    for(int i=0;i<(int)from.size();i++){
        if(!unwrap){
            auto tmp = from[i];
            if(quoted)
                tmp.flags |= TOKEN_QUOTED;
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
                tmp.flags |= TOKEN_QUOTED;
            args->back().push_back(tmp);
        }
    }
}
int EvalMacro(PreprocInfo& info, EvalInfo& evalInfo);
// Called by EvalMacro
// Example: MACRO(Hello + 2, Cool(stuf, hey))
int EvalArguments(PreprocInfo& info, EvalInfo& evalInfo){
    using namespace engone;
    TokenList& tokens = evalInfo.workingRange;
    if(evalInfo.macroName.flags&(TOKEN_SUFFIX_LINE_FEED|TOKEN_SUFFIX_SPACE)){
        evalInfo.finalFlags = evalInfo.macroName.flags&(TOKEN_SUFFIX_LINE_FEED|TOKEN_SUFFIX_SPACE);
        return PARSE_SUCCESS;
    }

    int& index=evalInfo.workIndex;
    Token token = info.get(tokens[index]);
    if(token != "("){
        evalInfo.finalFlags = evalInfo.macroName.flags&(TOKEN_SUFFIX_LINE_FEED|TOKEN_SUFFIX_SPACE);
        return PARSE_SUCCESS;
    }
    index++;
    Token token2 = info.get(tokens[index]);
    if(token2 == ")"){
        index++;
        evalInfo.finalFlags = token2.flags&(TOKEN_SUFFIX_LINE_FEED|TOKEN_SUFFIX_SPACE);
        return PARSE_SUCCESS; // no arguments
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
            
            if(superMacro->infiniteArg!=-1){
                // one from argValues is guarranteed. Therfore -1.
                // argumentNames has ... therefore another -1
                int extraArgs = superArgs->size() - 1 - (superMacro->argumentNames.size() - 1);
                if(argIndex>superMacro->infiniteArg){
                    argStart += extraArgs;
                    argEnd += extraArgs;
                } else if(argIndex==superMacro->infiniteArg){
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
                        int result = EvalArguments(info, newEvalInfo);
                        indexArg += newEvalInfo.workIndex;

                        CertainMacro* macro = rootMacro->matchArgCount(newEvalInfo.arguments.size());
                        if(!macro){
                            ERR_HEAD2(argTok)<<"Macro '"<<argTok<<"' cannot "<<newEvalInfo.arguments.size()<<" arguments\n";
                            ERR_END
                            // return PARSE_ERROR;
                        }else{
                            newEvalInfo.macro = macro;
                            EvalMacro(info,newEvalInfo);
                            Transfer(info,newEvalInfo.output,evalInfo.arguments.back(),wasQuoted);
                        }
                    }else{
                        auto tmp = argTokens[indexArg-1];
                        if(wasQuoted)
                            tmp.flags|=TOKEN_QUOTED;
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
            int result = EvalArguments(info, newEvalInfo);
            index += newEvalInfo.workIndex;
            
            CertainMacro* macro = rootMacro->matchArgCount(newEvalInfo.arguments.size());
            if(!macro){
                ERR_HEAD2(token)<<"Macro '"<<token<<"' cannot "<<newEvalInfo.arguments.size()<<" arguments\n";
                ERR_END
                // return PARSE_ERROR;
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
                tokenRef.flags|=TOKEN_QUOTED;
            evalInfo.arguments.back().push_back(tokenRef);
            _MLOG(log::out <<log::GRAY << " argv["<<(evalInfo.arguments.size()-1)<<"] += "<<token<<"\n";)
        }
        unwrapNext=false;
    }
    _MLOG(log::out << "Eval "<<evalInfo.arguments.size()<<" arguments\n";)
    return PARSE_SUCCESS;
}
int EvalMacro(PreprocInfo& info, EvalInfo& evalInfo){
    using namespace engone;

    if(info.evaluationDepth>=PREPROC_REC_LIMIT){
        ERR_HEAD2(info.now()) << "Reached recursion limit of "<<PREPROC_REC_LIMIT<<" for macros\n";
        ERR_END
        return PARSE_ERROR;
    }
    info.evaluationDepth++;
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
            
            if(evalInfo.macro->infiniteArg!=-1){
                // one from argValues is guarranteed. Therfore -1.
                // argumentNames has ... therefore another -1
                int extraArgs = evalInfo.arguments.size() - 1 - (evalInfo.macro->argumentNames.size() - 1);
                if(argIndex>evalInfo.macro->infiniteArg){
                    argStart += extraArgs;
                    argEnd += extraArgs;
                } else if(argIndex==evalInfo.macro->infiniteArg){
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
                        ref.flags |= TOKEN_QUOTED;
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
            int result = EvalArguments(info, newEvalInfo);
            index += newEvalInfo.workIndex;

            newEvalInfo.superMacros.pop_back();
            newEvalInfo.superArgs.pop_back();
            _MLOG(log::out <<log::GRAY<<"pop super\n";)

            if(index==(int)tokens.size())
                newEvalInfo.finalFlags = evalInfo.finalFlags;
            

            CertainMacro* macro = rootMacro->matchArgCount(newEvalInfo.arguments.size());
            if(!macro){
                ERR_HEAD2(token)<<"Macro '"<<token<<"' cannot have "<<newEvalInfo.arguments.size()<<" arguments\n";
                ERR_END
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
                tokens[index-1].flags |= TOKEN_QUOTED;
                wasQuoted=false;
            }

            evalInfo.output.push_back(tokens[index-1]);
            _MLOG(log::out << log::GRAY <<" eval.out << "<<token<<"\n";);
        }
    }
    info.evaluationDepth--;
    return PARSE_SUCCESS;
}
int ParseMacro(PreprocInfo& info, int attempt){
    using namespace engone;
    MEASURE;
    Token name = info.get(info.at()+1);
    RootMacro* rootMacro=0;
    if(!(rootMacro = info.matchMacro(name))){
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }
        ERR_HEAD2(name) << "Undefined macro '"<<name<<"'\n";
        ERR_END
        return PARSE_ERROR;
    }

    info.next();
    
    EvalInfo evalInfo{};
    evalInfo.rootMacro = rootMacro;
    evalInfo.macroName = name;

    // TODO: do not add remaining tokens to workingRange
    for(int i=info.at()+1;i<(int)info.length();i++){
        evalInfo.workingRange.push_back({(uint16)i,(uint16)info.get(i).flags});
    }
    int result = EvalArguments(info, evalInfo);
    for(int i=0;i<evalInfo.workIndex;i++)
        info.next();
    // TODO: handle result

    CertainMacro* macro = rootMacro->matchArgCount(evalInfo.arguments.size());
    if(!macro){
        ERR_HEAD2(name)<<"Macro '"<<name<<"' cannot have "<<evalInfo.arguments.size()<<" arguments\n";
        ERR_END
        return PARSE_ERROR;
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

        while(true){
            Token nextToken{};
            if(i+1<(int)evalInfo.output.size()){
                nextToken=info.get(evalInfo.output[i+1]);
            }
            if(nextToken==".."){
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
        int result = ParseToken(info);
    }
    info.inTokens = originalStream;
    info.usingTempStream = false;

    return PARSE_SUCCESS;
}

// Default parse function. Simular to ParseBody
int ParseToken(PreprocInfo& info){
    using namespace engone;
    int result = ParseDefine(info,true);
    if(result == PARSE_BAD_ATTEMPT)
        result = ParsePredefinedMacro(info,true);
    if(result == PARSE_BAD_ATTEMPT)
        result = ParseUndef(info,true);
    if(result == PARSE_BAD_ATTEMPT)
        result = ParseIfdef(info,true);
    if(result == PARSE_BAD_ATTEMPT)
        result = ParseMacro(info,true);
    if(result == PARSE_BAD_ATTEMPT)
        result = ParseInclude(info,true);
    if(result == PARSE_BAD_ATTEMPT)
        result = ParseImport(info,true);
    
    if(result==PARSE_SUCCESS)
        return result;
    if(result == PARSE_ERROR){
        // info.nextline(); // it is up to the parse functions to skip
        return result;
    }

    Token token = info.next();
    _MLOG(log::out << "Append "<<token<<"\n";)
    info.addToken(token);
    return PARSE_SUCCESS;
}
void Preprocess(CompileInfo* compileInfo, TokenStream* inTokens){
    using namespace engone;
    MEASURE;
    // _VLOG(log::out <<log::BLUE<<  "##   Preprocessor   ##\n";)
    
    PreprocInfo info{};
    info.compileInfo = compileInfo;
    info.outTokens = TokenStream::Create(); 
    info.outTokens->tokenData.resize(inTokens->tokenData.max*10); // hopeful estimation
    info.inTokens = inTokens; // Note: don't modify inTokens
    
    while(!info.end()){
        int result = ParseToken(info);
    }
    // info.tokens->lines = info->inTokens.lines;
    // info.inTokens.
    info.outTokens->finalizePointers();
    
    inTokens->tokens.resize(0);
    inTokens->tokenData.resize(0);
    inTokens->tokens = info.outTokens->tokens;
    inTokens->tokenData = info.outTokens->tokenData;
    info.outTokens->tokens = {0};
    info.outTokens->tokenData = {0};
    TokenStream::Destroy(info.outTokens);
    
    if(info.tempStream)
        TokenStream::Destroy(info.tempStream);
}