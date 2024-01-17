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
#define ERR_SECTION(CONTENT) BASE_SECTION("Preproc. error, ", CONTENT)

bool PreprocInfo::end(){
    return index>=length();
}
Token& PreprocInfo::next(){
    return inTokens->get(index++);
}
void PreprocInfo::reverse() {
    index--;
}
int PreprocInfo::at(){
    return index-1;
}
int PreprocInfo::length(){
    return inTokens->length();
}
Token& PreprocInfo::get(int index){
    return inTokens->get(index);
}
Token& PreprocInfo::now(){
    return inTokens->get(index-1);
}
void CertainMacro::addParam(const Token& name){
    Assert(name.length!=0);
    parameters.add(name);
    // auto pair = parameterMap.find(name);
    // if(pair == parameterMap.end()){
    //     parameterMap[name] = parameters.size()-1;
    // }
    // #ifndef SLOW_PREPROCESSOR
    // int index=sortedParameters.size()-1;
    // for(int i=0;i<sortedParameters.size();i++){
    //     auto& it = sortedParameters.get(i);
    //     if(*it.name.str > *name.str) {
    //         index = i;
    //         break;
    //     }
    // }
    // if(index==sortedParameters.size()-1){
    //     sortedParameters.add({name,(int)sortedParameters.size()});
    // } else {
    //     sortedParameters._reserve(sortedParameters.size()+1);
    //     memmove(sortedParameters._ptr + index + 1, sortedParameters._ptr + index, (sortedParameters.size()-index) * sizeof(Parameter));
    //     *(sortedParameters._ptr + index) = {name,(int)sortedParameters.size()-1};
    //     sortedParameters.used++;
    // }
    // #endif
}
int CertainMacro::matchArg(const Token& token){
    // MEASURE;
    if(token.flags&TOKEN_MASK_QUOTED)
        return -1;
    // #ifdef SLOW_PREPROCESSOR
    for(int i=0;i<(int)parameters.size();i++){
        Token& tok = parameters[i];
        if(token == tok){
            _MMLOG(engone::log::out <<engone::log::MAGENTA<< "match tok with arg "<<token<<"\n";)
            return i;
        }
    }
    // #else
    // for(int i=0;i<(int)sortedParameters.size();i++){
    //     // std::string& str = parameters[i];
    //     if(token.length != sortedParameters[i].name.length || *token.str < *sortedParameters[i].name.str)
    //         continue;
    //     this code has a bug with: #define A(a,b,c) c    new line:  A(1,2,3)
    //     some arguments swap place
    //     if(token == sortedParameters[i].name){
    //         _MLOG(MLOG_MATCH(engone::log::out <<engone::log::MAGENTA<< "match tok with arg "<<token<<"\n";))
    //         return sortedParameters[i].index;
    //         // return i;
    //         // sortedParameters[i].index;
    //     }
    // }
    // #endif
    return -1;
}
// CertainMacro* RootMacro::matchArgCount(int count, bool includeInf){
//     auto pair = certainMacros.find(count);
//     if(pair==certainMacros.end()){
//         if(!includeInf)
//             return nullptr;
//         if(hasVariadic){
//             if((int)variadicMacro.parameters.size()-1<=count){
//                 _MLOG(MLOG_MATCH(engone::log::out<<engone::log::MAGENTA << "match argcount "<<count<<" with "<< variadicMacro.parameters.size()<<" (inf)\n";))
//                 return &variadicMacro;
//             }else{
//                 return nullptr;
//             }
//         }else{
//             return nullptr;
//         }
//     }else{
//         _MLOG(MLOG_MATCH(engone::log::out <<engone::log::MAGENTA<< "match argcount "<<count<<" with "<< pair->second.parameters.size()<<"\n";))
//         return &pair->second;
//     }
// }
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
    if(!Equal(token,"#")){
        if(attempt){
            return SignalAttempt::BAD_ATTEMPT;
        }else{
            ERR_SECTION(
                ERR_HEAD(token)
                ERR_MSG("Expected " << "#" << " since this wasn't an attempt found '"<<token<<"'")
            )
            return SignalAttempt::FAILURE;
        }
    }
    if(token.flags&TOKEN_SUFFIX_SPACE){
        if(attempt) {
            return SignalAttempt::BAD_ATTEMPT;
        } else {
            ERR_SECTION(
                ERR_HEAD(token)
                ERR_MSG("Cannot have space after preprocessor directive")
            )
            return SignalAttempt::FAILURE;
        }
    }
    token = info.get(info.at()+2);
    if(token != str){
        if(attempt){
            return SignalAttempt::BAD_ATTEMPT;
        }else{
            ERR_SECTION(
                ERR_HEAD(token)
                ERR_MSG("Expected "<<str<<" found '"<<token<<"'")
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
    ZoneScopedC(tracy::Color::Wheat);
    Token token = info.get(info.at()+1);
    if(!Equal(token,"#")){
        if(attempt){
            return SignalAttempt::BAD_ATTEMPT;
        }else{
            ERR_SECTION(
                ERR_HEAD(token)
                ERR_MSG("Expected " << "#" << " since this wasn't an attempt found '"<<token<<"'")
            )
            return SignalAttempt::FAILURE;
        }
    }
    if(token.flags&TOKEN_SUFFIX_SPACE){
        ERR_SECTION(
            ERR_HEAD(token)
            ERR_MSG("Cannot have space after preprocessor directive")
        )
        // info.next();
        return SignalAttempt::FAILURE;
    }
    token = info.get(info.at()+2);
    
    // WARN_HEAD3(token,"Macros are actually broken. See document in Obsidian.\n");
    // TODO: only allow define on new line?
    
    bool multiline = false;
    if(token=="macro"){
        
    // }else if(token=="multidefine"){
    //     multiline=true;
    }else{
        if(attempt){
            return SignalAttempt::BAD_ATTEMPT;
        }else{
            ERR_SECTION(
                ERR_HEAD(token)
                ERR_MSG("Expected define found '"<<token<<"'")
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
        ERR_SECTION(
            ERR_HEAD(name)
            ERR_MSG("Macro names cannot be quoted ("<<name<<").")
            ERR_LINE(name,"");
        )
        return SignalAttempt::FAILURE;
    }
    if(!IsName(name)){
        ERR_SECTION(
            ERR_HEAD(name)
            ERR_MSG("'"<<name<<"' is not a valid name for macros. If the name is valid for variables then it is valid for macros.")
            ERR_LINE(name, "bad");
        )
        return SignalAttempt::FAILURE;
    }
    
    // if(info.matchMacro(name)){
    //     WARN_HEAD3(name, "Intentional redefinition of '"<<name<<"'?\n";   
    // }
    
    int suffixFlags = name.flags;
    CertainMacro localMacro{};
    
    if(name.flags&TOKEN_MASK_SUFFIX){
        suffixFlags = name.flags;

    } else {
        Token token = info.next();
        if(token!="("){
            ERR_SECTION(
                ERR_HEAD(token)
                ERR_MSG("'"<<token<<"' is not allowed directly after a macro's name. Either use a '(' to indicate arguments or a space for none.")
                ERR_LINE(token,"bad");
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
                if(localMacro.indexOfVariadic == -1) {
                    // log::out << " param: "<<token<<"\n";
                    localMacro.indexOfVariadic = localMacro.parameters.size();
                    localMacro.addParam(token);
                } else {
                    if(!hadError){
                        ERR_SECTION(
                            ERR_HEAD(token)
                            ERR_MSG("Macros can only have 1 variadic argument.")
                            ERR_LINE(localMacro.parameters[localMacro.indexOfVariadic], "previous")
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
                // log::out << " param: "<<token<<"\n";
                localMacro.addParam(token);
            }
            token = info.next();
            if(token==")"){
                suffixFlags = token.flags;
                break;   
            } else if(token==","){
                // cool
            } else{
                if(!hadError){
                    ERR_SECTION(
                        ERR_HEAD(token)
                        ERR_MSG("'"<<token<< "' is not okay. You use ',' to specify more arguments and ')' to finish parsing of argument.")
                        ERR_LINE(token, "bad");
                    )
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
    if(suffixFlags & TOKEN_SUFFIX_LINE_FEED) {
        multiline = true;
    }
    // RootMacro* rootMacro = info.compileInfo->ensureRootMacro(name, localMacro.isVariadic());
    
    // CertainMacro* certainMacro = 0;
    // if(!localMacro.isVariadic()){
    //     certainMacro = info.compileInfo->matchArgCount(rootMacro, localMacro.parameters.size(),false);
    //     if(certainMacro){
    //         // if(!certainMacro->isVariadic() && !certainMacro->isBlank()){
    //         //     WARN_HEAD3(name, "Intentional redefinition of '"<<name<<"'?\n\n";
    //         //         ERR_LINE(certainMacro->name, "previous");
    //         //         ERR_LINE2(name.tokenIndex, "replacement");
    //         //     )
    //         // }
    //         *certainMacro = localMacro;
    //     }else{
    //         // move localMacro info appropriate place in rootMacro
    //         certainMacro = &(rootMacro->certainMacros[(int)localMacro.parameters.size()] = localMacro);
    //     }
    // }else{
    //     // if(rootMacro->hasVariadic){
    //     //     WARN_HEAD3(name, "Intentional redefinition of '"<<name<<"'?\n\n";
    //     //         ERR_LINE(certainMacro->name, "previous");
    //     //         ERR_LINE2(name.tokenIndex, "replacement");
    //     //     )
    //     // }
    //     rootMacro->hasVariadic = true;
    //     rootMacro->variadicMacro = localMacro;
    //     certainMacro = &rootMacro->variadicMacro;
    // }
    bool invalidContent = false;
    int startToken = info.at()+1;
    int endToken = startToken;
    // if(multiline || 0==(suffixFlags&TOKEN_SUFFIX_LINE_FEED)) {
    while(!info.end()){
        Token token = info.next();
        if(token=="#"){
            if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED)){
                continue;
                // ERR_SECTION(
                // ERR_HEAD(token, "SPACE AFTER "<<token<<"!\n";
                // )
                // return SignalAttempt::FAILURE;
            }
            Token token = info.get(info.at()+1);
            if(token=="endmacro"){
                info.next();
                endToken = info.at()-1;
                break;
            }
            if(token=="macro"){
            // if(token=="define" || token=="multidefine"){
                ERR_SECTION(
                    ERR_HEAD(token)
                    ERR_MSG("Macro definitions inside macros are not allowed.")
                    ERR_LINE(token,"not allowed")
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
            ERR_SECTION(
                ERR_HEAD(token)
                ERR_MSG("Missing '#endmacro' for macro '"<<name<<"' ("<<(localMacro.isVariadic() ? "variadic" : std::to_string(localMacro.parameters.size()))<<" arguments)")
                ERR_LINE(name, "this needs #endmacro somewhere")
                ERR_LINE(info.get(info.length()-1),"macro content ends here!")
            )
        }
    }
    // }
    localMacro.name = name;
    localMacro.start = startToken;
    localMacro.contentRange.stream = info.inTokens;
    localMacro.contentRange.start = startToken;
    if(!invalidContent){
        localMacro.end = endToken;
        localMacro.contentRange.end = endToken;
    } else {
        localMacro.end = startToken;
        localMacro.contentRange.end = startToken;
    }
    int count = endToken-startToken;
    int argc = localMacro.parameters.size();

    RootMacro* rootMacro = info.compileInfo->ensureRootMacro(name, localMacro.isVariadic() && localMacro.parameters.size() > 1);
    
    info.compileInfo->insertCertainMacro(rootMacro, &localMacro);

    _MLOG(log::out << log::LIME<< "#"<<"macro def. '"<<name<<"' ";
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

    // Assert(("Broken due to multithreading",false));
    // defining and undefining won't work with multithreading
    // because two imports may define the same macro at almost the same time one will be lost
    // The imports will then use macros with the same name from other imports.
    // 
    // and when those are used within the separate imports one one import may defineone thing is parsed, you undefine some macro
    // TODO: check of end

    Token token = info.get(info.at()+1);
    if(!Equal(token,"#")){
        if(attempt){
            return SignalAttempt::BAD_ATTEMPT;
        }else{
            ERR_SECTION(
                ERR_HEAD(token)
                ERR_MSG("Expected " "#" " since this wasn't an attempt found '"<<token<<"'.")
            )
            
            return SignalAttempt::FAILURE;
        }
    }
    if(token.flags&TOKEN_SUFFIX_SPACE){
        ERR_SECTION(
            ERR_HEAD(token)
            ERR_MSG("Cannot have space after preprocessor directive.")
        )
        return SignalAttempt::FAILURE;
    }
    token = info.get(info.at()+2);
    if(token!="undef"){
        if(attempt){
            return SignalAttempt::BAD_ATTEMPT;
        }
        ERR_SECTION(
            ERR_HEAD(token)
            ERR_MSG("Expected undef (since this wasn't an attempt)")
        )
        return SignalAttempt::FAILURE;
    }
    attempt = false;
    info.next();
    info.next();
    if(token.flags&TOKEN_SUFFIX_LINE_FEED){
        ERR_SECTION(
            ERR_HEAD(token)
            ERR_MSG("Unexpected line feed (expected a macro name).")
        )
        return SignalAttempt::FAILURE;
    }
    
    Token name = info.next();

    RootMacro* rootMacro = info.compileInfo->matchMacro(name);
    if(!rootMacro){
        WARN_HEAD3(name, "Cannot undefine '"<<name<<"' (doesn't exist)\n";
        )
        return SignalAttempt::SUCCESS;
    }

    if(name.flags&TOKEN_SUFFIX_LINE_FEED){
        info.compileInfo->removeRootMacro(name);
        return SignalAttempt::SUCCESS;
    }
    token = info.next();

    if(token=="..." && (token.flags&TOKEN_SUFFIX_LINE_FEED)){
        bool yes = info.compileInfo->removeCertainMacro(rootMacro, 0, true);
        if(!yes){
            WARN_HEAD3(name, "Cannot undefine variadic '"<<name<<"' (doesn't exist)\n";
            )
            return SignalAttempt::FAILURE;
        }
    } else if (IsInteger(token) && (token.flags&TOKEN_SUFFIX_LINE_FEED)){
        int argCount = ConvertInteger(token);
        if(argCount<0){
            ERR_SECTION(
                ERR_HEAD(token)
                ERR_MSG("ArgCount cannot be negative '"<<argCount<<"'")
            )
            return SignalAttempt::FAILURE;
        }
        bool yes = info.compileInfo->removeCertainMacro(rootMacro, argCount, false);
        if(!yes) {
            WARN_HEAD3(name, "Cannot undefine '"<<name<<"' ["<<argCount<<" args] (doesn't exist)\n";
            )
            return SignalAttempt::SUCCESS;
        }
    } else {
        ERR_SECTION(
            ERR_HEAD(token)
            ERR_MSG("'"<<token<<"' is not allowed after '#undef SomeName'. Either use nothing (new line) to remove the whole macro name space or an integer to remove a macro with the specified argument amount or ... to remove the variadic macro.")
            ERR_LINE(token,"should be nothing, integer or ...")
            ERR_EXAMPLE(1,"#undef X\n#undef X 3\n#undef X ...");
        )
        return SignalAttempt::FAILURE;
    }
    return SignalAttempt::SUCCESS;
}

SignalAttempt ParseLink(PreprocInfo& info, bool attempt){
    using namespace engone;
    // ZoneScopeC(tracy::Color::Wheat);
    Token token = info.get(info.at()+1);
    SignalAttempt result = ParseDirective(info, attempt, "link");
    if(result==SignalAttempt::BAD_ATTEMPT)
        return SignalAttempt::BAD_ATTEMPT;

    Token name = info.get(info.at()+1);
    if(!(name.flags&TOKEN_MASK_QUOTED)){
        ERR_SECTION(
            ERR_HEAD(name)
            ERR_MSG("Expected a string not "<<name<<".")
        )
        return SignalAttempt::FAILURE;
    }
    info.next();
    
    info.compileInfo->addLinkDirective(name);
    
    return SignalAttempt::SUCCESS;
}
SignalAttempt ParseInclude(PreprocInfo& info, bool attempt, bool quoted = false){
    using namespace engone;
    ZoneScopedC(tracy::Color::Wheat);
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
    
    bool active = info.compileInfo->matchMacro(name);
    if(notDefined)
        active = !active;
    notDefined = false; // reset, we use it later
    
    bool complete_inactive = false;
    
    int depth = 0;
    SignalAttempt error = SignalAttempt::SUCCESS;
    // bool hadElse=false;
    _MLOG(log::out << log::GRAY<< "   ifdef - "<<name<<"\n";)
    while(!info.end()){
        Token token = info.get(info.at()+1);
        if(token=="#"){
            if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED)){
                ERR_SECTION(
                    ERR_HEAD(token)
                    ERR_MSG("SPACE AFTER "<<token<<"!")
                )
                return SignalAttempt::FAILURE;
            }
            Token token = info.get(info.at()+2);
            // If !yes we can skip these tokens, otherwise we will have
            //  to skip them later which is unnecessary computation
            if(token=="ifdef" || token=="ifndef"){
                if(!active){
                    info.next(); // hashtag
                    info.next();
                    depth++;
                    // log::out << log::GRAY<< "   ifdef - new depth "<<depth<<"\n";
                }
                //  else {
                //     info.next(); // hashtag
                //     info.next(); // ifdef/ifndef
                    
                // }
                // continue;
            } else if(token == "elif" || (token == "elnif" && (notDefined = true))) {
                info.next(); // hashtag
                info.next(); // elif
                
                name = info.next(); // name, hopefully
                if(!IsName(name)) {
                    ERR_SECTION(
                        ERR_HEAD(name)
                        ERR_MSG("#elif/elnif expects a valid macro name after it same as #ifdef/ifndef.")
                        ERR_LINE(name,"not a valid macro name")
                    )
                } else {
                    if(!active) {
                        // if a previous ifdef/elif matched we don't want to check this one
                        active = info.compileInfo->matchMacro(name);
                        if(notDefined)
                            active = !active;
                    } else {
                        complete_inactive = true;
                        active = false;
                    }
                }
                notDefined = false; // reset for later
            } else if(token=="endif"){
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
                    if(!complete_inactive)
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
            ERR_SECTION(
                ERR_HEAD(info.get(info.length()-1))
                ERR_MSG("Missing endif somewhere for ifdef or ifndef.")
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
SignalAttempt ParsePredefinedMacro(PreprocInfo& info, const Token& parseToken, Token& outToken, char* buffer, u32 bufferLen){
    using namespace engone;
    ZoneScopedC(tracy::Color::Wheat);
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
        Assert(outToken.length+1 < (int)bufferLen);
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
        if(info.parsedMacroName.str)
            outToken = info.parsedMacroName;
        else
            outToken = parseToken;

        std::string temp = std::to_string(outToken.line);
        Assert(temp.size()-1 < bufferLen);
        strcpy(buffer, temp.data());
        outToken.str = (char*)buffer;
        outToken.length = temp.length();
        
        // _MLOG(log::out << "Append "<<outToken<<"\n";)
        // info.addToken(token);
        return SignalAttempt::SUCCESS;
    } else if(Equal(parseToken,"column")) {
        // info.next();

        if(info.parsedMacroName.str)
            outToken = info.parsedMacroName;
        else
            outToken = parseToken;

        std::string temp = std::to_string(outToken.column);
        Assert(temp.size()-1 < bufferLen);
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
        Assert(temp.size()-1 < bufferLen);
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
        Assert(temp.size()-1 < bufferLen);
        strcpy(buffer, temp.data());
        outToken = parseToken;
        outToken.str = (char*)buffer;
        outToken.length = temp.length();
        outToken.flags = TOKEN_DOUBLE_QUOTED;
        
        // if(lastSlash != -1 && ( info.inTokens->streamName.size() == 0
        //      || info.inTokens->streamName.last() != '/'))
        // {
        //      outToken.str += lastSlash + 1;
        //      outToken.length -= lastSlash + 1;
        // }
        
        // _MLOG(log::out << "Append "<<outToken<<"\n";)
        // info.addToken(token);
        return SignalAttempt::SUCCESS;
    } else if(Equal(parseToken,"unique")) { // rename to counter? unique makes you think aboout UUID
        // info.next();
        
        // TODO: This is one alternative.
        #ifdef OS_WINDOWS
        i32 num = _InterlockedIncrement(&info.compileInfo->globalUniqueCounter) - 1;
        #else
        // TODO: How bad is it for performance when using a mutex lock below? Is it better to use
        //   something else?
        info.compileInfo->otherLock.lock();
        i32 num = info.compileInfo->globalUniqueCounter++;
        info.compileInfo->otherLock.unlock();
        #endif
        // NOTE: This would have been another alternative but x64 inline assembly isn't supported so it doesn't work.
        // i32* ptr = &info.compileInfo->globalUniqueCounter; 
        // i32 num = 0;
        // __asm {
        //     mov rbx, ptr
        //     mov eax, 1
        //     lock xadd DWORD PTR [rbx], eax
        //     mov num, eax
        // }

        std::string temp = std::to_string(num);
        Assert(temp.size()-1 < bufferLen);
        strcpy(buffer, temp.data());
        outToken = parseToken;
        outToken.str = (char*)buffer;
        outToken.length = temp.length();
        
        // _MLOG(log::out << "Append "<<outToken<<"\n";)
        // info.addToken(token);
        return SignalAttempt::SUCCESS;
    } else if(Equal(parseToken,"date")) {
        // info.next();
        time_t timer;
        time(&timer);
        auto date = localtime(&timer);
        
        // int len = snprintf(buffer, bufferLen, "%d.%d.%d",date->tm_mday, 1 + date->tm_mon, 1900 + date->tm_year);
        int len = snprintf(buffer, bufferLen, "%d.%d.%d",1900+date->tm_year, 1 + date->tm_mon, date->tm_mday);
        Assert(len != 0);
        outToken = parseToken;
        outToken.str = (char*)buffer;
        outToken.length = len;
        
        // _MLOG(log::out << "Append "<<outToken<<"\n";)
        // info.addToken(token);
        return SignalAttempt::SUCCESS;
    } else {
        // this shouldn't be here.
        // it will warn about #__LINE__ but we want to warn about __LINE__
        // you can do a quick check of underscore as first character if you are worried about speed.
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
engone::Logger& operator<<(engone::Logger& logger, const TokenSpan& span){
    TokenRange tokRange = span.stream->get(span.start);
    tokRange.endIndex = span.end;
    return logger << tokRange;
}
SignalDefault FetchArguments(PreprocInfo& info, TokenSpan& tokenRange, MacroCall* call, MacroCall* newCall, DynamicArray<bool>& unwrappedArgs){
    using namespace engone;
    int& index = tokenRange.start;
    ZoneScopedC(tracy::Color::Wheat);
    
    _MLOG(log::out << "Pre fetch input args:\n";)

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
                newCall->realArgumentCount++;
                newCall->detailedArguments.add({});
                newCall->realArgs_per_range.add(1);
                for(int i=argRange.start;i<argRange.end;i++){
                    newCall->detailedArguments.last().add((const Token*)&argRange.stream->get(i));
                }
                _MLOG(log::out << " arg["<<(newCall->detailedArguments.size()-1)<<"] "<<argRange<<"\n";)
            } else {
                // if(Equal(argRange.stream->get(argRange.start), "...") {
                    
                // }
                newCall->realArgumentCount++;
                newCall->inputArgumentRanges.add(argRange);
                newCall->realArgs_per_range.add(1);
                _MLOG(log::out << " arg["<<(newCall->inputArgumentRanges.size()-1)<<"] "<<argRange<<"\n";)
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
                ERR_SECTION(
                    ERR_HEAD(token)
                    ERR_MSG("Infinite argument should be it's own argument. You cannot combine it with other tokens.")
                    ERR_LINE(token,"bad")
                )
                add_arg();
                argRange.start = index - 1;
            }
            
            argRange.end = index;
            
            // Assert(!call->useDetailedArgs); // not implemented
            int argCount = call->realArgumentCount;
            int infArgs = argCount - (call->certainMacro->parameters.size() - 1);
            
            if(call && call->useDetailedArgs){
                newCall->detailedArguments.add({});
                newCall->realArgumentCount+=infArgs;
                newCall->detailedArguments.last().add(&argRange.stream->get(argRange.start));
                newCall->realArgs_per_range.add(infArgs);
            } else {
                newCall->realArgumentCount+=infArgs;
                newCall->inputArgumentRanges.add(argRange);
                newCall->realArgs_per_range.add(infArgs);
            }
            argRange.start = index;
            
            if(!Equal(tokenRange.stream->get(index),",")&&!Equal(tokenRange.stream->get(index),")")){
                ERR_SECTION(
                    ERR_HEAD(tokenRange.stream->get(index))
                    ERR_MSG("Infinite argument should be it's own argument. You cannot combine it with other tokens.")
                    ERR_LINE(tokenRange.stream->get(index),"bad")
                )
            }
        } else
        if(Equal(token,"#")){ // annotation instead of directive?
            const Token& nextTok = tokenRange.stream->get(index);
            if(Equal(nextTok,"unwrap")){
                // mac(hey #unwrap LIST) is allowed with add_arg() below but we should throw error if user forgets comma
                add_arg();
                newCall->unwrapped = true;
                int argCount = 1 + (newCall->realArgumentCount);
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
    _MLOG(
    log::out << " arg range: ";
    if(newCall && newCall->useDetailedArgs){
         for(int i=0;i<newCall->detailedArguments.size();i++){
            for(int j=0;j<newCall->detailedArguments.size();j++){
                if(i!=0&&j!=0) log::out << ", ";
                log::out << *newCall->detailedArguments.get(i).get(j);
            }
        }
        log::out << "\n";
    } else {
        for(int i=0;i<newCall->inputArgumentRanges.size();i++) {
            if(i!=0) log::out << ", ";
            log::out << newCall->inputArgumentRanges[i];
        }
        log::out << "\n";
    }
    )
    return SignalDefault::SUCCESS;
}
// A warning: the code in this function is complex and hard to understand.
SignalAttempt ParseMacro_fast(PreprocInfo& info, int attempt){
    using namespace engone;
    ZoneScopedC(tracy::Color::Wheat);
    
    bool quote_tokens = false;
    int extra_off = 0;
    if(Equal(info.get(info.at() + 1), "#") && Equal(info.get(info.at() + 2), "quote")) {
        extra_off += 2;
        quote_tokens = true;
    }

    Token macroName = info.get(info.at() + extra_off + 1);
    RootMacro* rootMacro = info.compileInfo->matchMacro(macroName);
    if(!rootMacro){
        if(attempt){
            return SignalAttempt::BAD_ATTEMPT;
        }
        ERR_SECTION(
            ERR_HEAD(macroName)
            ERR_MSG("Macro '"<<macroName<<"' is undefined. Cannot evaluate. (this error message shouldn't happen).")
        )
        return SignalAttempt::FAILURE;
    }
    for(int i=0;i<extra_off;i++) info.next();
    info.next();
    attempt=false;
    
    // TODO: Use struct TokenSpan { int start; int end; TokenStream* stream } instead of TokenRange.
    //  TokenRange contains the first token which isn't necessary.
    
    Assert(!info.macroArraysInUse);
    info.macroArraysInUse = true;
    defer { info.macroArraysInUse=false; };

    info.outputTokens.resize(0);
    info.outputTokensFlags.resize(0);
    info.calls.resize(0);
    info.environments.resize(0);
    // info.unwrappedArgs.resize(0);
    
    defer {
        FOR(info.calls) delete it;
        FOR(info.environments) delete it;
    };

    // TODO: Use bucket arrays instead
    // DynamicArray<const Token*> outputTokens{}; // TODO: Reuse a temporary array instead of doing a new allocation
    // DynamicArray<i16> outputTokensFlags{}; // TODO: Reuse a temporary array instead of doing a new allocation
    // // outputTokens._reserve(15000);
    // DynamicArray<MacroCall> calls{};
    // // calls._reserve(30);
    // DynamicArray<Env> environments{};
    // // environments._reserve(50);
    // DynamicArray<bool> unwrappedArgs{};
    
    // TODO: Use a bucket array when allocating macros calls and environments.
    //   Perhaps you could do MacroCall calls[100]; with a fixed size since we will have a limit for the macro recursion.
    //   Or maybe don't since the limit might be really big? like 1000 is a lot to have.
    info.calls.add(new MacroCall());
    info.environments.add(new Env());
    u32 finalFlags = 0;
    {
        MacroCall* initialCall = info.calls.last();
        Env* initialEnv = info.environments.last();
        initialEnv->outputToCall = -1;
        initialEnv->unwrapOutput = false;
        initialCall->rootMacro = rootMacro;
        initialEnv->callIndex = 0;
        initialEnv->ownsCall = 0;

        int argCount=0;
        if(!info.end()){
            TokenSpan argRange{};
            argRange.stream = info.inTokens;
            argRange.start = info.at()+1;
            argRange.end = info.length();

            info.unwrappedArgs.resize(0);
            FetchArguments(info,argRange, nullptr, initialCall, info.unwrappedArgs);
            while(info.at() + 1 < argRange.start){
                info.next();
            }
            argCount = initialCall->realArgumentCount;
            initialEnv->finalFlags = argRange.stream->get(argRange.start - 1).flags;
            finalFlags = initialEnv->finalFlags;
        } else {
            finalFlags = macroName.flags & (TOKEN_SUFFIX_LINE_FEED | TOKEN_SUFFIX_SPACE);
        }
        if(!initialCall->unwrapped || initialCall->useDetailedArgs){
            Assert(!initialCall->unwrapped); // how does unwrap work here?
            // initialCall->certainMacro = rootMacro->matchArgCount(argCount);
            initialCall->certainMacro = info.compileInfo->matchArgCount(rootMacro, argCount, true);
            if(!initialCall->certainMacro){
                ERR_SECTION(
                    ERR_HEAD(macroName)
                    ERR_MSG("Macro '"<<macroName<<"' cannot have "<<(argCount)<<" arguments.")
                    ERR_LINE(macroName,"bad")
                )
                // TODO: Show the macros that do exist.
                return SignalAttempt::FAILURE;
            }
            
            _MLOG(log::out << "Matched CertainMacro with "<<argCount<<" args\n";)
            initialEnv->range = initialCall->certainMacro->contentRange;
        } else {
            initialCall->useDetailedArgs = true;
            _MLOG(log::out << "Unwrapped args\n";)
            for(int i = (int)initialCall->inputArgumentRanges.size()-1; i >= 0;i--){
                info.environments.add(new Env());
                Env* argEnv = info.environments.last();
                argEnv->range = initialCall->inputArgumentRanges[i];
                argEnv->callIndex = info.calls.size()-1;
                if(i<(int)info.unwrappedArgs.size())
                    argEnv->unwrapOutput = info.unwrappedArgs[i];
                argEnv->outputToCall = info.calls.size()-1;
            }
        }
    }
    struct TempEnv {
        int indexOfInputArg, argStart, argCount;
        bool include_comma, is_last_arg;
    };
    DynamicArray<TempEnv> tempEnvs{};
    // TODO: Add some extra logic to transfer flags from tokens, parameters and macro calls.
    //  It looks a little nicer. I have started but not finished it because it's not very important.

    // TODO: Not thread safe here. Implement global macros and we might be fine.

    // int limit = 40;
    // while (limit-->0){
    while (info.calls.size()<PREPROC_REC_LIMIT){
        Env* env = info.environments.last();
        MacroCall* call = nullptr;
        if(env->callIndex!=-1) {
            call = info.calls.get(env->callIndex);
            if(!call->certainMacro && env->ownsCall==env->callIndex){
                Assert(call->rootMacro && call->unwrapped);
                int argCount = call->realArgumentCount;
                // if(call->unwrapped){ 
                    call->certainMacro = info.compileInfo->matchArgCount(call->rootMacro, argCount, true);
                    
                // }else{
                    // call->certainMacro = call->rootMacro->matchArgCount(call->inputArgumentRanges.size());
                // }
                if(!call->certainMacro){
                    ERR_SECTION(
                        ERR_HEAD(call->rootMacro->name)
                        ERR_MSG("Macro '"<<"?"<<"' cannot have "<<(argCount)<<" arguments.")
                        ERR_LINE(call->rootMacro->name,"bad")
                    )
                    continue;
                }
                _MLOG(log::out << "CertainMacro special "<<argCount<<"\n";)
                env->range = call->certainMacro->contentRange; 
            }
        }
        if(env->range.end == env->range.start) {
            _MLOG(log::out << log::GOLD << "Env["<<(info.environments.size()-1)<<"] pop callIndex: "<<(env->callIndex==-1 ? "-1" : info.calls[env->callIndex]->rootMacro->name)<<"\n";)
            if(env->ownsCall!=-1){
                Assert(env->ownsCall==(int)info.calls.size()-1);
                delete info.calls.last();
                info.calls.pop();
            }
            Assert(env == info.environments.last());
            delete info.environments.last();
            info.environments.pop();

            if(info.environments.size()==0)
                break;
            continue;
        }
        _MLOG(log::out << log::GOLD << "Env["<<(info.environments.size()-1)<<"] callIndex: "<<(env->callIndex==-1 ? "-1" : info.calls[env->callIndex]->rootMacro->name) << ", ownsCall: "<<(env->ownsCall==-1 ? "-1" : info.calls[env->ownsCall]->rootMacro->name)<<"\n";)
        bool initialEnv = env->firstTime;
        if(env->firstTime){
            env->firstTime=false;
        }
        Assert(env->range.start < env->range.stream->length());
        const Token& token = env->range.stream->get(env->range.start++);

        int parameterIndex = -1;
        RootMacro* deeperMacro = nullptr;

        if(env->callIndex!=-1) {
            // MATCH PARAMTER
            if(call->certainMacro && -1!=(parameterIndex = call->certainMacro->matchArg(token))){ // TOOD: rename matchArg to matchParameter
                // @optimize TODO: Parameters are evaluated in full each time they occur.
                //   This is inefficient with many parameters but a parameter will
                //   usually only occur once or twice. Even so it could be worth thinking
                //   of a way to optimize it.

                _MLOG(log::out << "Param "<<token<<" [param:"<<parameterIndex<<"]\n")

                if(call->useDetailedArgs){
                    Assert(false);
                    #ifdef gone
                    // Yeah this is complicated.
                    int start = paramStart;
                    int end = paramStart + paramCount;
                    // if(env->arg_start != 0 || env->arg_end != 0) {
                    //     start += env->arg_start;
                    //     end = start + env->arg_end;
                    // }
                    _MLOG(log::out << " Used range (detailed) ["<<start<<"-"<<(end)<<"]\n";)
                    // Assert(false);
                    for(int i = end - 1;i>=start;i--){
                        for(int j=0;j<(int)call->detailedArguments[i].size();j++){
                            const Token* argToken = call->detailedArguments[i][j];
                            if(env->outputToCall==-1){
                                _MLOG(log::out << " output " << *argToken<<"\n";)
                                info.outputTokens.add(argToken);
                                info.outputTokensFlags.add(argToken->flags);
                            } else {
                                Assert(env->outputToCall>=0&&env->outputToCall<(int)info.calls.size());
                                MacroCall* outputCall = info.calls[env->outputToCall];
                                if(initialEnv) {
                                    outputCall->detailedArguments.add({});
                                    outputCall->realArgumentCount++;
                                }
                                if(env->unwrapOutput && Equal(*argToken,",")){
                                    _MLOG(log::out << " output arg comma\n";)
                                    outputCall->detailedArguments.add({});
                                    outputCall->realArgumentCount++;
                                } else {
                                    _MLOG(log::out << " output arg["<<(outputCall->realArgumentCount-1)<<"] "<<*argToken<<"\n";)
                                    outputCall->detailedArguments.last().add(argToken);
                                }
                                // TOOD: What about final flags?
                            }
                        }
                    }
                    #endif
                } else {
                    auto add_arg_env=[&](int indexOfInputArg, int argStart, int argCount, bool include_comma, bool is_last_arg) {
                        _MLOG(log::out << log::LIME << "Arg env "<<indexOfInputArg << ", start:"<<argStart << ",count:"<<argCount<<"\n")
                        Env* argEnv = new Env();
                        argEnv->range = call->inputArgumentRanges[indexOfInputArg];
                        argEnv->arg_start = argStart;
                        argEnv->arg_count = argCount;
                        
                        // The code that decrements the range makes this happen:
                        // #define Hey(...) PrintMessage(...)
                        // Hey(a,b,c) -> PrintMessage(a,b,c)
                        // Otherwise you would get PrintMessage(abc) which is kind of useless
                        // perhaps do something with unwrap, spread or something to tweak when
                        // this code is off but it's quite nice for the most part.
                        if(include_comma) {
                            argEnv->range.start--; // include comma
                        }

                        if(env->callIndex==0){
                            // log::out << "Saw param, global env\n";
                            argEnv->callIndex = -1;
                        } else {
                            // log::out << "Saw param, local env\n";
                            argEnv->callIndex = env->callIndex-1;
                        }
                        if(is_last_arg)
                            argEnv->finalFlags = token.flags;
                        if(env->range.start == env->range.end)
                            argEnv->finalFlags &= ~(TOKEN_SUFFIX_LINE_FEED); 
                        info.environments.add(argEnv);
                    };

                    /* Whats going on?
                    We found a parameter (token) in the environment (either macro content or argument environment).
                    We need to evaluate the paramater based on our input arguments.
                    We do this by adding a new environment that parses the argument input.

                    The difficult part is figuring out which input argument we should parse based on the parameter we found.
                    One input argument may be variadic and consist of multiple arguments. We need
                    too count and add each input argument (also count inner arguments if variadic).

                    Sometimes we want a subpart of an input argument: env->arg_start, env->arg_count
                    */

                    int inf_args = parameterIndex == call->certainMacro->indexOfVariadic ? call->realArgumentCount - (call->certainMacro->parameters.size() - 1) : 0;
                    int remaining_args = parameterIndex == call->certainMacro->indexOfVariadic ? call->realArgumentCount - (call->certainMacro->parameters.size() - 1) : 1;
                    if(env->arg_count != 0) {
                        Assert(env->arg_count <= remaining_args);
                        if(env->arg_count < remaining_args) {
                            remaining_args = env->arg_count;
                        }
                    }

                    int real_index = 0;
                    int input_index = 0;
                    int inner_index = 0;
                    int added_envs = 0;
                    tempEnvs.resize(0);
                    while(true) {
                        if(input_index >= call->realArgs_per_range.size()) {
                            // end of arguments
                            // we couldn't match the parameter
                            // this will only happen with variadic arguments
                            // because they may contain zero arguments.
                            Assert(parameterIndex == call->certainMacro->indexOfVariadic);
                            break;

                        }

                        if(parameterIndex == real_index - env->arg_start - added_envs) {
                            // we have found our match
                            // more if parameters include variadic arg
                            // if our param is variadic then we found the start of the variadic args
                            int available_args = call->realArgs_per_range[input_index] - inner_index;
                            int args_to_use = 0;
                            if(available_args >= remaining_args) {
                                args_to_use = remaining_args;
                            } else /* available_args < remaining_args */ {
                                args_to_use = available_args;
                            }
                            remaining_args -= args_to_use;
                            tempEnvs.add({input_index, inner_index, args_to_use, added_envs != 0, remaining_args == 0});
                            // add_arg_env(input_index, inner_index, args_to_use, added_envs != 0, remaining_args == 0);
                            added_envs++;
                            if(remaining_args == 0) {
                                // we done
                                break;
                            }
                            Assert(remaining_args >= 0);
                        }

                        real_index++;
                        inner_index++;
                        if(inner_index >= call->realArgs_per_range[input_index]) {
                            inner_index = 0;
                            input_index++;
                        }
                    }
                    // reverse
                    for(int i=tempEnvs.size()-1;i>=0;i--) {
                        auto& e = tempEnvs[i];
                        add_arg_env(e.indexOfInputArg, e.argStart, e.argCount, e.include_comma, e.is_last_arg);
                    }
                }
                continue;
            }
        }
        // MATCH MACRO
        if((deeperMacro = info.compileInfo->matchMacro(token))){
            _MLOG(log::out << log::CYAN << "Found macro call "<<token<<"\n";)
            
            info.environments.add(new Env());
            info.calls.add(new MacroCall());
            Env* innerEnv = info.environments.last();
            MacroCall* innerCall = info.calls.last();
            innerEnv->outputToCall = env->outputToCall;
            innerEnv->unwrapOutput = env->unwrapOutput;
            innerEnv->callIndex = info.calls.size()-1;
            innerEnv->ownsCall = info.calls.size()-1;
            innerCall->rootMacro = deeperMacro;

            int argCount=0;
            // DynamicArray<bool> unwrappedArgs{};
            info.unwrappedArgs.resize(0);
            if(env->range.start < env->range.end) {
                FetchArguments(info,env->range, call, innerCall, info.unwrappedArgs);
                argCount = innerCall->realArgumentCount;
            }
            innerEnv->finalFlags = env->range.stream->get(env->range.start - 1).flags;

            if(!innerCall->unwrapped || innerCall->useDetailedArgs){
                Assert(!innerCall->unwrapped); // how does unwrap work here?
                innerCall->certainMacro = info.compileInfo->matchArgCount(deeperMacro, argCount, true);
                if(!innerCall->certainMacro){
                    ERR_SECTION(
                        ERR_HEAD(token)
                        ERR_MSG("Macro '"<<token<<"' cannot have "<<(argCount)<<" arguments.")
                        ERR_LINE(token,"bad")
                    )
                    continue;
                }
                _MLOG(log::out << "Matched CertainMacro with "<<argCount<<" args\n";)
                innerEnv->range = innerCall->certainMacro->contentRange;
            } else {
                innerCall->useDetailedArgs = true;
                _MLOG(log::out << "Unwrapped args\n";)
                for(u32 i = innerCall->inputArgumentRanges.size()-1; i >= 0;i--){
                    info.environments.add(new Env());
                    Env* argEnv = info.environments.last();
                    argEnv->range = innerCall->inputArgumentRanges[i];
                    argEnv->callIndex = info.calls.size()-1;
                    if(i<info.unwrappedArgs.size())
                        argEnv->unwrapOutput = info.unwrappedArgs[i];
                    argEnv->outputToCall = info.calls.size()-1;
                }
            }
            continue;
        }
        if(env->outputToCall==-1){
            _MLOG(log::out << " output " << token<<"\n";)
            info.outputTokens.add(&token);
            if(env->range.start == env->range.end){
                info.outputTokensFlags.add(env->finalFlags|((token.flags & ~(TOKEN_SUFFIX_LINE_FEED|TOKEN_SUFFIX_SPACE)) &(TOKEN_MASK_QUOTED)));
            } else {
                info.outputTokensFlags.add(token.flags);
            }
        } else {
            Assert(env->outputToCall>=0&&env->outputToCall<(int)info.calls.size());
            MacroCall* outputCall = info.calls[env->outputToCall];
            Assert(outputCall->useDetailedArgs);
            if(initialEnv)
                outputCall->detailedArguments.add({});
            if(env->unwrapOutput && Equal(token,",")){
                _MLOG(log::out << " output arg comma\n";)
                outputCall->detailedArguments.add({});
                outputCall->realArgumentCount++;
            } else {
                _MLOG(log::out << " output arg["<<(outputCall->detailedArguments.size()-1)<<"] "<<token<<"\n";)
                outputCall->detailedArguments.last().add(&token);
                outputCall->realArgumentCount++;
            }
        }
    }
    if(info.calls.size()>=PREPROC_REC_LIMIT){
        Env* env = info.environments.last();
        MacroCall* call = env->callIndex!=-1 ? info.calls[env->callIndex] : nullptr;
        // Token& tok = call ? call->certainMacro->name : info.now();
        ERR_SECTION(
            ERR_HEAD(macroName)
            ERR_MSG("Reached recursion limit of "<<PREPROC_REC_LIMIT<<" for macros.")
            ERR_LINE(macroName,"bad")
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
    info.usingTempStream = true;
    
    info.parsedMacroName = macroName;

    // TODO: This can be done in the main loop instead.
    // No need for outputTokens. Concatenation can be done there too.
    const int bufferLen = 256;
    char buffer[bufferLen];
    for(int i=0;i<(int)info.outputTokens.size();i++){
        Token baseToken = *info.outputTokens[i];
        SignalAttempt result = SignalAttempt::BAD_ATTEMPT;
        if(Equal(baseToken,"#") && (int)info.outputTokens.size()>i+1){
            result = ParsePredefinedMacro(info,*info.outputTokens[i+1], baseToken,buffer, bufferLen);
            if(result == SignalAttempt::SUCCESS){
                i++;
                // baseToken.flags &= (info.outputTokensFlags[i]&(~TOKEN_MASK_QUOTED))|(TOKEN_MASK_QUOTED);
                // baseToken.flags = (info.outputTokensFlags[i] & (~TOKEN_MASK_QUOTED)) | (baseToken.flags & TOKEN_MASK_QUOTED);
            }
        }
        if(result == SignalAttempt::BAD_ATTEMPT){
            baseToken.flags = (info.outputTokensFlags[i] & (~TOKEN_MASK_QUOTED)) | (baseToken.flags & TOKEN_MASK_QUOTED);
        }
        u64 offset = info.tempStream->tokenData.used;
        info.tempStream->addData(baseToken);
        baseToken.str = (char*)offset;
        WHILE_TRUE {
            
            if(!Equal(*info.outputTokens[i],"#") && 0==(baseToken.flags&TOKEN_MASK_QUOTED) && i+3<(int)info.outputTokens.size()
            && Equal(*info.outputTokens[i+1],"#") && Equal(*info.outputTokens[i+2],"#")){
                // Assert(false);
                Token token2 = *info.outputTokens[i+3];
                if(Equal(token2,"#") && i+4 < info.outputTokens.size()){
                    SignalAttempt result = ParsePredefinedMacro(info,*info.outputTokens[i+4], token2,buffer,bufferLen);
                    if(result==SignalAttempt::SUCCESS){
                        i++;
                    }
                } 
                if(0==(info.outputTokens[i+3]->flags&TOKEN_MASK_QUOTED)) {
                    if(result == SignalAttempt::BAD_ATTEMPT){
                        token2.flags = info.outputTokensFlags[i];
                    }
                    info.tempStream->addData(token2);
                    // info.outTokens->addData(token2);
                    baseToken.length += token2.length;
                    i+=3;
                    continue;
                }
            }
            
            // if(0 == (baseToken.flags & TOKEN_MASK_QUOTED)) {
            // } else {
            //     if(info.tempStream->length() > 0) {
            //         Token& prev = info.tempStream->get(info.tempStream->length()-1);
            //     }
            // }

            // IMPORTANT: Setting the column, line, and flags is necessary because
            //  the when looking at the tokens you would otherwise see them at the original
            //  location at the definition of the macro. This messes with the lines in the
            //  error messages and the debugger would show the location of the macro definition 
            //  instead of where the macro was used.
            baseToken.column = macroName.column;
            baseToken.line = macroName.line;
            if(i == (int)info.outputTokens.size()-1) {
                baseToken.flags &= ~(TOKEN_SUFFIX_LINE_FEED | TOKEN_SUFFIX_SPACE);
                baseToken.flags |= finalFlags & (TOKEN_SUFFIX_LINE_FEED | TOKEN_SUFFIX_SPACE);
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

    if(quote_tokens) {
        Assert(info.tempStream->length() > 0);
        
        
        TokenRange range{};
        range.firstToken = info.tempStream->get(0);
        range.endIndex = info.tempStream->length();
        
        // TODO: This could use a nice refactor. We do a lot of tricky stuff which is easy to do wrong.
        //   Making some functions which does the tricky stuff so we can just use them without issues would be nice.
        
        int size = range.queryFeedSize(true);
        
        u64 offset = info.outTokens->tokenData.used;
        char* buf = info.outTokens->addData_late(size);
        int readSize = range.feed(buf, size, true);
        Assert(readSize == size);
        
        Token quotedToken{};
        quotedToken.flags |= TOKEN_DOUBLE_QUOTED;
        Token last = info.tempStream->get(range.endIndex - 1);
        quotedToken.flags |= info.tempStream->get(range.endIndex-1).flags & (TOKEN_SUFFIX_LINE_FEED | TOKEN_SUFFIX_SPACE);
        quotedToken.column = range.firstToken.column;
        quotedToken.line = range.firstToken.line;
        quotedToken.length = size;
        quotedToken.str = (char*)offset;
        
        info.outTokens->addToken(quotedToken);
    } else {
        // TODO: A function to load and unload token stream. The index
        //  must be saved and restored which you may forget when swithing token stream.
        TokenStream* originalStream = info.inTokens;
        info.inTokens = info.tempStream; // Note: don't modify inTokens
        
        int savedIndex = info.index;
        info.index = 0;
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
            //         ERR_SECTION(
        // ERR_HEAD(info.get(info.at()+2), "Macro definitions inside macros are not allowed.\n\n";
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
        info.parsedMacroName = {};
        info.index = savedIndex;
        info.inTokens = originalStream;
        info.usingTempStream = false;
    }

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
    SignalAttempt result = SignalAttempt::BAD_ATTEMPT;
    const Token& hashtag = info.get(info.at()+1);
    if(Equal(hashtag,"#") && 0 == (hashtag.flags & (TOKEN_SUFFIX_LINE_FEED|TOKEN_SUFFIX_SPACE))){
        // if(result == SignalAttempt::BAD_ATTEMPT) {
            // if(Equal(info.get(info.at()+1),"#")){
        Token token = info.get(info.at()+2);
        Token outToken{};
        const int bufferLen = 256;
        char buffer[bufferLen]; // not ideal
        
        if(Equal(token,"quote")) {
            if(Equal(info.get(info.at() + 3), "#")) {
                token = info.get(info.at()+4);
                result = ParsePredefinedMacro(info,token,outToken,buffer,bufferLen);
                if(result==SignalAttempt::SUCCESS){
                    info.next();
                    info.next();
                    info.next();
                    info.next();
                    outToken.flags |= TOKEN_DOUBLE_QUOTED;   
                    info.addToken(outToken);
                    _MLOG(log::out << "Append "<<outToken<<"\n";)
                    return SignalDefault::SUCCESS;
                }
                info.next();
                info.next();
                
                result = ParseInclude(info,true, true);
                if(result!=SignalAttempt::BAD_ATTEMPT) return CastSignal(result);
                
                info.reverse();
                info.reverse();
            }
            result = ParseMacro_fast(info,true);
            if(result!=SignalAttempt::BAD_ATTEMPT) return CastSignal(result);
            
            info.next(); // hashtag
            info.next(); // quote
            token = info.next();
            token.flags |= TOKEN_DOUBLE_QUOTED;
            
            // _MLOG(log::out << "Append "<<token<<"\n";)
            info.addToken(token);
            return SignalDefault::SUCCESS;
        } else {
            result = ParsePredefinedMacro(info,token,outToken,buffer,bufferLen);
            if(result==SignalAttempt::SUCCESS){
                info.next();
                info.next();
                info.addToken(outToken);
                // _MLOG(log::out << "Append "<<outToken<<"\n";)
                return SignalDefault::SUCCESS;
            }
        }
        
        // if(result == SignalAttempt::BAD_ATTEMPT)
            result = ParseDefine(info,true);
        if(result!=SignalAttempt::BAD_ATTEMPT) return CastSignal(result);
        // if(result == SignalAttempt::BAD_ATTEMPT)
            result = ParseUndef(info,true);
        if(result!=SignalAttempt::BAD_ATTEMPT) return CastSignal(result);
        // if(result == SignalAttempt::BAD_ATTEMPT)
            result = ParseIfdef(info,true);
        if(result!=SignalAttempt::BAD_ATTEMPT) return CastSignal(result);
        // if(result == SignalAttempt::BAD_ATTEMPT)
            result = ParseInclude(info,true);
        if(result!=SignalAttempt::BAD_ATTEMPT) return CastSignal(result);
        // if(result == SignalAttempt::BAD_ATTEMPT)
        //     result = ParseImport(info,true);
        // if(result!=SignalAttempt::BAD_ATTEMPT) return CastSignal(result);
        // if(result == SignalAttempt::BAD_ATTEMPT)
            result = ParseLink(info,true);
        if(result!=SignalAttempt::BAD_ATTEMPT) return CastSignal(result);
        
        // if(Equal(token,"quote")) {
        //     // we do info.next in the function, it needs to know if we #quote or not
        //     result = ParseMacro_fast(info,true);
        //     if(result!=SignalAttempt::BAD_ATTEMPT) return CastSignal(result);
            
        //     info.next(); // hashtag
        //     info.next(); // quote
        //     Token token = info.next();
        //     token.flags |= TOKEN_DOUBLE_QUOTED;
            
        //     _MLOG(log::out << "Append "<<token<<"\n";)
        //     info.addToken(token);
        //     return SignalDefault::SUCCESS;
        // }
    }

    if(info.get(info.at()+1).flags&TOKEN_ALPHANUM || Equal(info.get(info.at()+1),"#"))
        #ifdef SLOW_PREPROCESSOR
            result = ParseMacro(info,true);
        #else
            result = ParseMacro_fast(info,true);
        #endif
    if(result != SignalAttempt::BAD_ATTEMPT){
        return CastSignal(result);
    }

    Token token = info.next();
    // _MLOG(log::out << "Append "<<token<<"\n";)
    info.addToken(token);
    return SignalDefault::SUCCESS;
}
TokenStream* Preprocess(CompileInfo* compileInfo, TokenStream* inTokens){
    using namespace engone;
    Assert(compileInfo);
    Assert(inTokens);
    ZoneScopedC(tracy::Color::Wheat);
    
    PreprocInfo info{};
    info.compileInfo = compileInfo;
    info.outTokens = TokenStream::Create();
    info.outTokens->tokenData.resize(100+inTokens->tokenData.max*3); // hopeful estimation
    info.inTokens = inTokens; // Note: don't modify inTokens
    // info.inTokens->readHead = 0;

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

    info.compileInfo->addStats(info.errors, info.warnings);
    
    info.outTokens->streamName = info.inTokens->streamName;
    info.outTokens->finalizePointers();

    return info.outTokens;
}
/* #####################
    NEW PREPROCESSOR
#######################*/

void PreprocContext::parseMacroDefinition() {
    using namespace engone;
    ZoneScopedC(tracy::Color::Wheat);

    bool multiline = false;
    
    lexer::Token name_token = gettok();
    
    if(name_token.type == lexer::TOKEN_IDENTIFIER){
        // TODO: Create new error report system (or finalize the started one)?
        //   We should do a complete failure here stopping preprocessing because
        //   everything else could be wrong. All imports which import the current
        //   will not be preprocessed or parsed minimizing meaningless errors.
        Assert(false);
        // ERR_SECTION(
        //     ERR_HEAD(name)
        //     ERR_MSG("'"<<name<<"' is not a valid name for macros. If the name is valid for variables then it is valid for macros.")
        //     ERR_LINE(name, "bad");
        // )
        return;
    }
    
    advance();
    
    // TODO: Can we redefine macros? or should there be some scenario where
    //   we cannot? non-scoped macros perhaps.
    
    bool has_newline = name_token.flags & lexer::TOKEN_FLAG_NEWLINE;
    CertainMacro localMacro{};
    
    if(name_token.flags&lexer::TOKEN_FLAG_ANY_SUFFIX){

    } else {
        lexer::Token token = gettok();
        if(token.type != '('){
            Assert(false); // nocheckin, report error
            // ERR_SECTION(
            //     ERR_HEAD(token)
            //     ERR_MSG("'"<<token<<"' is not allowed directly after a macro's name. Either use a '(' to indicate arguments or a space for none.")
            //     ERR_LINE(token,"bad");
            // )
            return;
            // return SignalAttempt::FAILURE;
        }
        advance();
        int hadError = false;
        while(true){
            lexer::Token token = gettok();
            if(token.type == ')'){
                has_newline = token.flags&lexer::TOKEN_FLAG_NEWLINE;
                break;   
            }
            
            if(lexer->equals(token,"...")) { // TODO: Use token type instead of string ("...")
                if(localMacro.indexOfVariadic == -1) {
                    // log::out << " param: "<<token<<"\n";
                    localMacro.indexOfVariadic = localMacro.parameters.size();
                    localMacro.addParam(token);
                } else {
                    if(!hadError){
                        Assert(false); // nocheckin
                        // ERR_SECTION(
                        //     ERR_HEAD(token)
                        //     ERR_MSG("Macros can only have 1 variadic argument.")
                        //     ERR_LINE(localMacro.parameters[localMacro.indexOfVariadic], "previous")
                        //     ERR_LINE(token, "not allowed")
                        // )
                    }
                    
                    hadError=true;
                }
            } else if(token.type != lexer::TOKEN_IDENTIFIER){
                if(!hadError){
                    Assert(false); // nocheckin
                    // ERR_SECTION(
                    //     ERR_HEAD(token)
                    //     ERR_MSG("'"<<token<<"' is not a valid name for arguments. The first character must be an alpha (a-Z) letter, the rest can be alpha and numeric (0-9). '...' is also available to specify a variadic argument.")
                    //     ERR_LINE(token, "bad");
                    // )
                }
                hadError=true;
            } else {
                // log::out << " param: "<<token<<"\n";
                localMacro.addParam(token);
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
                        ERR_HEAD(token)
                        ERR_MSG("'"<<token<< "' is not okay. You use ',' to specify more arguments and ')' to finish parsing of argument.")
                        ERR_LINE(token, "bad");
                    )
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
    // RootMacro* rootMacro = info.compileInfo->ensureRootMacro(name, localMacro.isVariadic());
    
    // CertainMacro* certainMacro = 0;
    // if(!localMacro.isVariadic()){
    //     certainMacro = info.compileInfo->matchArgCount(rootMacro, localMacro.parameters.size(),false);
    //     if(certainMacro){
    //         // if(!certainMacro->isVariadic() && !certainMacro->isBlank()){
    //         //     WARN_HEAD3(name, "Intentional redefinition of '"<<name<<"'?\n\n";
    //         //         ERR_LINE(certainMacro->name, "previous");
    //         //         ERR_LINE2(name.tokenIndex, "replacement");
    //         //     )
    //         // }
    //         *certainMacro = localMacro;
    //     }else{
    //         // move localMacro info appropriate place in rootMacro
    //         certainMacro = &(rootMacro->certainMacros[(int)localMacro.parameters.size()] = localMacro);
    //     }
    // }else{
    //     // if(rootMacro->hasVariadic){
    //     //     WARN_HEAD3(name, "Intentional redefinition of '"<<name<<"'?\n\n";
    //     //         ERR_LINE(certainMacro->name, "previous");
    //     //         ERR_LINE2(name.tokenIndex, "replacement");
    //     //     )
    //     // }
    //     rootMacro->hasVariadic = true;
    //     rootMacro->variadicMacro = localMacro;
    //     certainMacro = &rootMacro->variadicMacro;
    // }
    bool invalidContent = false;
    int startToken = info.at()+1;
    int endToken = startToken;
    // if(multiline || 0==(suffixFlags&TOKEN_SUFFIX_LINE_FEED)) {
    while(!info.end()){
        Token token = info.next();
        if(token=="#"){
            if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED)){
                continue;
                // ERR_SECTION(
                // ERR_HEAD(token, "SPACE AFTER "<<token<<"!\n";
                // )
                // return SignalAttempt::FAILURE;
            }
            Token token = info.get(info.at()+1);
            if(token=="endmacro"){
                info.next();
                endToken = info.at()-1;
                break;
            }
            if(token=="macro"){
            // if(token=="define" || token=="multidefine"){
                ERR_SECTION(
                    ERR_HEAD(token)
                    ERR_MSG("Macro definitions inside macros are not allowed.")
                    ERR_LINE(token,"not allowed")
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
            ERR_SECTION(
                ERR_HEAD(token)
                ERR_MSG("Missing '#endmacro' for macro '"<<name<<"' ("<<(localMacro.isVariadic() ? "variadic" : std::to_string(localMacro.parameters.size()))<<" arguments)")
                ERR_LINE(name, "this needs #endmacro somewhere")
                ERR_LINE(info.get(info.length()-1),"macro content ends here!")
            )
        }
    }
    // }
    localMacro.name = name;
    localMacro.start = startToken;
    localMacro.contentRange.stream = info.inTokens;
    localMacro.contentRange.start = startToken;
    if(!invalidContent){
        localMacro.end = endToken;
        localMacro.contentRange.end = endToken;
    } else {
        localMacro.end = startToken;
        localMacro.contentRange.end = startToken;
    }
    int count = endToken-startToken;
    int argc = localMacro.parameters.size();

    RootMacro* rootMacro = info.compileInfo->ensureRootMacro(name, localMacro.isVariadic() && localMacro.parameters.size() > 1);
    
    info.compileInfo->insertCertainMacro(rootMacro, &localMacro);

    _MLOG(log::out << log::LIME<< "#"<<"macro def. '"<<name<<"' ";
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
void PreprocContext::parseAll() {
    while(true) {
        lexer::Token token = gettok();
        if(token.type == lexer::TOKEN_EOF)
            break;
        
        if(token.type != '#' || (token.flags & (lexer::TOKEN_FLAG_NEWLINE|lexer::TOKEN_FLAG_SPACE))) {
            advance();
            lexer->appendToken(new_import_id, token);
            continue;
        }
        // handle directive
        advance(); // skip #
        
        // TODO: Create token types for non-user directives (#import, #include, #macro...)
        token = gettok();
        if(token.type == lexer::TOKEN_IDENTIFIER) {
            const char* str = nullptr;
            u32 len = lexer->getStringFromToken(token,&str);
            
            
            if(!strcmp(str, "macro")) {
                advance();
                parseMacroDefinition();
            } else {
                // user defined macro?   
            }
        }
        advance(); // skip identifier
    }
}
u32 Preprocessor::process(u32 import_id) {
    ZoneScopedC(tracy::Color::Wheat);
 
    PreprocContext context{};
    context.lexer = lexer;
    context.import_id = import_id;
    
    context.new_import_id = lexer->createImport(nullptr);
    Assert(context.new_import_id != 0);
    
    lock_imports.lock();
    context.current_import = imports.requestSpot(import_id-1, nullptr);
    lock_imports.unlock();
    Assert(context.current_import);
    
    context.parseAll();
    
    return context.new_import_id;
}