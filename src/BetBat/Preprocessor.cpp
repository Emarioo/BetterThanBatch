#include "BetBat/Preprocessor.h"

#define PERR info.errors++; engone::log::out << engone::log::RED << "PreProcError: "
#define PERRT(token) info.errors++;engone::log::out << engone::log::RED << "PreProcError "<< token.line <<":"<<token.column<<": "
#define PERRTL(token) info.errors++;engone::log::out << engone::log::RED << "PreProcError "<< token.line <<":"<<(token.column+token.length)<<": "
#define PWARNT(token) engone::log::out << engone::log::YELLOW << "PreProcWarning "<< token.line <<":"<<token.column<<": "

#ifdef PLOG
#define _PLOG(X) X
#else
#define _PLOG(X) ;
#endif

// #define PLOG_MATCH(X) X
#define PLOG_MATCH(X)

// used for std::vector<int> tokens
#define INT_ENCODE(I) (I<<3)
#define INT_DECODE(I) (I>>3)

bool PreprocInfo::end(){
    return index==(int)inTokens.length();
}
Token& PreprocInfo::next(){
    Token& temp = inTokens.get(index++);
    return temp;
}
int PreprocInfo::at(){
    return index-1;
}
int PreprocInfo::length(){
    return inTokens.length();
}
Token& PreprocInfo::get(int index){
    return inTokens.get(index);
}
Token& PreprocInfo::now(){
    return inTokens.get(index-1);   
}
PreprocInfo::Defined* PreprocInfo::matchDefine(Token& token){
    if(token.flags&TOKEN_QUOTED)
        return 0;
    auto pair = defines.find(token);
    if(pair==defines.end()){
        return 0;
    }
    // if(pair->second.called){
    //     auto& info = *this;
    //     PERRT(token) << "Infinite recursion not allowed ("<<token<<")\n";
    //     return 0;
    // }
    _PLOG(PLOG_MATCH(engone::log::out << engone::log::CYAN << "match root "<<token<<"\n";))
    return &pair->second;
}
int PreprocInfo::CertainDefined::matchArg(Token& token){
    if(token.flags&TOKEN_QUOTED)
        return -1;
    for(int i=0;i<(int)argumentNames.size();i++){
        std::string& str = argumentNames[i];
        if(token == str.c_str()){
            _PLOG(PLOG_MATCH(engone::log::out <<engone::log::MAGENTA<< "match tok with arg "<<token<<"\n";))
            return i;
        }
    }
    return -1;
}
PreprocInfo::CertainDefined* PreprocInfo::Defined::matchArgCount(int count, bool includeInf){
    auto pair = argDefines.find(count);
    if(pair==argDefines.end()){
        if(!includeInf)
            return 0;
        if(hasInfinite){
            if((int)infDefined.argumentNames.size()-1<=count){
                _PLOG(PLOG_MATCH(engone::log::out<<engone::log::MAGENTA << "match argcount "<<count<<" with "<< infDefined.argumentNames.size()<<" (inf)\n";))
                return &infDefined;
            }else{
                return 0;
            }
        }else{
            return 0;   
        }
    }else{
        _PLOG(PLOG_MATCH(engone::log::out <<engone::log::MAGENTA<< "match argcount "<<count<<" with "<< infDefined.argumentNames.size()<<"\n";))
        return &pair->second;
    }
}
void PreprocInfo::nextline(){
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
void PreprocInfo::addToken(Token inToken){
    int offset = tokens.tokenData.used;
    tokens.append(inToken);
    inToken.str = (char*)tokens.tokenData.data + offset;
    tokens.add(inToken);
}
int ParseDirective(PreprocInfo& info, bool attempt, const char* str){
    using namespace engone;
    Token token = info.get(info.at()+1);
    if(token!="@" || (token.flags&TOKEN_QUOTED)){
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }else{
            PERRT(token) << "Expected @ since this wasn't an attempt found '"<<token<<"'\n";
            return PARSE_ERROR;
        }
    }
    if(token.flags&TOKEN_SUFFIX_SPACE){
        PERRT(token) << "Cannot have space after preprocessor directive\n";
        info.nextline();
        return PARSE_ERROR;
    }
    token = info.get(info.at()+2);
    if(token != str){
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }else{
            PERRT(token) << "Expected "<<str<<" found '"<<token<<"'\n";
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
    Token token = info.get(info.at()+1);
    if(token!="@" || (token.flags&TOKEN_QUOTED)){
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }else{
            PERRT(token) << "Expected @ since this wasn't an attempt found '"<<token<<"'\n";
            return PARSE_ERROR;
        }
    }
    if(token.flags&TOKEN_SUFFIX_SPACE){
        PERRT(token) << "Cannot have space after preprocessor directive\n";
        info.nextline();
        return PARSE_ERROR;
    }
    token = info.get(info.at()+2);
    
    // Todo: only allow define on new line?
    
    bool multiline = false;
    if(token=="define"){
        
    }else if(token=="multidefine"){
        multiline=true;
    }else{
        if(attempt){
            return PARSE_BAD_ATTEMPT;
        }else{
            PERRT(token) << "Expected define found '"<<token<<"'\n";
            return PARSE_ERROR;
        }
        return PARSE_ERROR;
    }
    token = info.next();
    token = info.next();
    
    Token name = info.next();
    
    if(name.flags&TOKEN_QUOTED){
        PERRT(name) << "Define name cannot be quoted "<<name<<"\n";
        return PARSE_ERROR;
    }
    
    // if(info.matchDefine(name)){
    //     PWARNT(name) << "Intentional redefinition of '"<<name<<"'?\n";   
    // }
    
    PreprocInfo::Defined* rootDefined = info.matchDefine(name);
    if(!rootDefined){
        rootDefined = &(info.defines[name] = {});
    }
    int suffixFlags = name.flags;
    PreprocInfo::CertainDefined defTemp{};
    if((name.flags&TOKEN_SUFFIX_SPACE) || (name.flags&TOKEN_SUFFIX_LINE_FEED)){
    
    }else{
        Token token = info.next();
        if(token!="("){
            PERRT(token) << "Expected ( found '"<<token<<"'\n";
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
            // Todo: token must be valid name
            defTemp.argumentNames.push_back(token);
            token = info.next();
            if(token==")"){
                suffixFlags = token.flags;
                break;   
            } else if(token==","){
                // cool
            } else{
                PERRT(token) << "Expected , or ) found '"<<token<<"'\n";   
            }
        }
        
        _PLOG(log::out << "loaded "<<defTemp.argumentNames.size()<<" arg names";
        if(defTemp.infiniteArg==-1)
            log::out << "\n";
        else
            log::out << " (infinite)\n";)
    
    }
    
    PreprocInfo::CertainDefined* defined = 0;
    if(defTemp.infiniteArg==-1){
        defined = rootDefined->matchArgCount(defTemp.argumentNames.size(),false);
        if(defined){
            PWARNT(name) << "Intentional redefinition of '"<<name<<"'?\n";
            *defined = defTemp;
        }else{
            // move defTemp info appropriate place in rootDefined
            defined = &(rootDefined->argDefines[(int)defTemp.argumentNames.size()] = defTemp);
        }
    }else{
        if(rootDefined->hasInfinite){
            PWARNT(name) << "Intentional redefinition of '"<<name<<"'?\n";   
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
        if(token=="@"){
            if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED)){
                PERRT(token) << "SPACE AFTER @!\n";
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
            PERRTL(token) << "Missing enddef!\n";
        }
    }
    while_skip_194:
    
    defined->start = startToken;
    defined->end = endToken;
    int count = endToken-startToken;
    int argc = defined->argumentNames.size();
    _PLOG(log::out << log::LIME<< "@define '"<<name<<"' ";
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
int ParseToken(PreprocInfo& info);
// both @ifdef and @ifndef
int ParseIfdef(PreprocInfo& info, bool attempt){
    using namespace engone;
    bool notDefined=false;
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
    Token token = info.next();
    
    PreprocInfo::Defined* defined = 0;
    bool yes = nullptr != info.matchDefine(token);
    if(notDefined)
        yes = !yes;
    
    int depth = 0;
    int error = PARSE_SUCCESS;
    bool hadElse=false;
    // log::out << "     enter ifdef loop\n";
    while(!info.end()){
        Token token = info.get(info.at()+1);
        if(token=="@"){
            if((token.flags&TOKEN_SUFFIX_SPACE)||(token.flags&TOKEN_SUFFIX_LINE_FEED)){
                PERRT(token) << "SPACE AFTER @!\n";
                return PARSE_ERROR;
            }
            Token token = info.get(info.at()+2);
            // If !yes we can skip these tokens, otherwise we will have
            //  to skip them later which is unnecessary computation
            if(token=="ifdef" || token=="ifndef"){
                if(!yes){
                    info.next();
                    info.next();
                    depth++;
                    // log::out << log::GRAY<< " depth "<<depth<<"\n";
                }
            }
            if(token=="endif"){
                if(depth==0){
                    info.next();
                    info.next();
                    // log::out << log::GRAY<< " break\n";
                    break;
                }
                depth--;
                // log::out << log::GRAY<< " depth "<<depth<<"\n";
            }
            if(token == "else"){
                if(depth==0){
                    info.next();
                    info.next();
                    if(hadElse){
                        PERRT(info.get(info.at()-1)) << "Already had @else\n";
                        error = PARSE_ERROR;
                        // best option is to continue
                        // things will break more if we return suddenly
                    }else{
                        // log::out << log::GRAY<< " flip\n";
                        yes = !yes;
                        hadElse=true;
                    }
                }
            }
        }
        // Todo: what about errors?
        
        if(yes){
            result = ParseToken(info);
        }else{
            Token skip = info.next();
            // log::out << log::GRAY<<" skip "<<skip << "\n";
        }
        if(info.end()){
            PERRT(info.get(info.length()-1)) << "Missing @endif somewhere for ifdef or ifndef\n";
            return PARSE_ERROR;   
        }
    }
    //  log::out << "     exit  ifdef loop\n";
    return error;
}
void EvaluateRange(PreprocInfo& info, Token& name, PreprocInfo::Defined* defined, std::vector<std::vector<int>>& argValues);

int EvaluateArgValues(PreprocInfo& info, Token& name, PreprocInfo::CertainDefined* topDefined,
    std::vector<std::vector<int>>& argValues,std::vector<std::vector<int>>& outValues, int& outExtraFlags, int& index, std::vector<int>& tokens){
    using namespace engone;
    
    if(index>=(int)tokens.size() || info.length() < INT_DECODE(tokens[index])){
        outExtraFlags |= name.flags&(TOKEN_SUFFIX_SPACE|TOKEN_SUFFIX_LINE_FEED);   
        return PARSE_SUCCESS; // no arguments to read
    }
    Token par0 = info.get(INT_DECODE(tokens[index]));
    if(par0!="(" || (name.flags&TOKEN_SUFFIX_SPACE)||(name.flags&TOKEN_SUFFIX_LINE_FEED)){
        outExtraFlags |= name.flags&(TOKEN_SUFFIX_SPACE|TOKEN_SUFFIX_LINE_FEED);   
        return PARSE_SUCCESS; // no paranthesis, no args to read
    }
    index++;
    if(index>=(int)tokens.size() || info.length() < INT_DECODE(tokens[index])){
        PERRTL(par0) << "Unexpected end\n";
        return PARSE_ERROR;
    }
    Token par1 = info.get(INT_DECODE(tokens[index]));
    outExtraFlags |= par1.flags&(TOKEN_SUFFIX_SPACE|TOKEN_SUFFIX_LINE_FEED);   
    if(par1==")"){
        index++;
        return PARSE_SUCCESS; // once again, no args
    }
    
    int parDepth = 0;
    int valueIndex=outValues.size();
    // bool unwrapNext=false;
    while(index<(int)tokens.size()){
        Token token = info.get(INT_DECODE(tokens[index]));
        index++;
        if(token==","){
            if(parDepth==0){
                valueIndex++;
                continue;
            }
        }else if(token=="("){
            parDepth++;
        }else if(token==")"){
            if(parDepth==0){
                outExtraFlags |= token.flags&(TOKEN_SUFFIX_SPACE|TOKEN_SUFFIX_LINE_FEED);
                break;
            }
            parDepth--;
        }
        // _PLOG(log::out <<log::GRAY << " range "<<outValues.size()<<" "<< token<<"\n";)
        // if(token=="@"){
        //     Token nextTok = info.get(INT_DECODE(tokens[index]));
        //     if(nextTok=="unwrap"){
        //         unwrapNext=true;
        //         index++;
        //         continue;   
        //     }
        // }
        int argIndex = -1;
        if(topDefined!=0){
            if(-1!=(argIndex = topDefined->matchArg(token))){
                // _PLOG(log::out << log::GRAY<<" match arg "<<token <<", index: "<<argIndex<<"\n";)
                int argStart = argIndex;
                int argEnd = argIndex+1;
                
                if(argIndex==topDefined->infiniteArg){
                    // one from argValues is guarranteed. Therfore -1.
                    // argumentNames has ... therefore another -1
                    int extraArgs = argValues.size() - 1 - (topDefined->argumentNames.size() - 1);
                    argEnd += extraArgs;
                }
                
                for(int i=argStart;i<argEnd;i++){
                    std::vector<int> list = argValues[i];
                    // log::out << log::GRAY<< "range"<<i<<" "<<argRange.start << " "<<argRange.end<<"\n";
                    outValues.push_back({});
                    valueIndex++;
                    int ind=0;
                    while(ind<(int)list.size()){
                        _PLOG(log::out <<log::GRAY << " argv["<<(outValues.size()-1)<<"] += "<<info.get(INT_DECODE(list[ind]))<<" (from arg)\n";)
                        outValues.back().push_back(list[ind]);
                        ind++;
                    }
                }
            }
        }
        
        if(argIndex==-1){
            if((int)outValues.size()==valueIndex)
                outValues.push_back({});
            _PLOG(log::out <<log::GRAY << " argv["<<(outValues.size()-1)<<"] += "<<token<<"\n";)
            outValues.back().push_back(tokens[index-1]);
        }
    }
    _PLOG(log::out << "Eval "<<outValues.size()<<" arguments\n";)
    return PARSE_SUCCESS;
}
void EvaluateRange(PreprocInfo& info, Token& name, PreprocInfo::CertainDefined* defined,
    std::vector<std::vector<int>>& argValues, int extraFlags, bool unwrapping){
    using namespace engone;
    if(info.evaluationDepth>=PREPROC_REC_LIMIT){
        PERR << "Hit recursion limit of "<<PREPROC_REC_LIMIT<<"\n";
        return;
    }
    info.evaluationDepth++;
    defined->called=true;
    std::vector<int> tokens;
    for(int i=defined->start;i<defined->end;i++){
        tokens.push_back(INT_ENCODE(i));
    }
    _PLOG(log::out <<"Eval "<<name<<" "<<tokens.size()<<" toks\n";)
    int index=0;
    bool unwrapNext = false;
    while(index<(int)tokens.size()){
        Token token = info.get(INT_DECODE(tokens[index]));
        index++;
        
        if(token=="@"){
            Token nextTok = info.get(INT_DECODE(tokens[index]));
            if(nextTok=="unwrap"){
                unwrapNext=true; // Not handled on arguments  Base(A) @unwrap A <- nothing happens
                index++;
                continue;
            }
        }
        
        PreprocInfo::Defined* rootDefined=0;
        int argIndex = -1;
        if((rootDefined = info.matchDefine(token))){
            
            std::vector<std::vector<int>> ranges;
            int newExtraFlags=0;
            
            EvaluateArgValues(info,token,defined,argValues, ranges, newExtraFlags, index,tokens);
            
            if(index==(int)tokens.size())
                newExtraFlags = extraFlags;
            
            PreprocInfo::CertainDefined* defined = rootDefined->matchArgCount(ranges.size());
            if(defined){
                EvaluateRange(info,token, defined, ranges, newExtraFlags,unwrapNext);
            }else{
                PERRT(token)<<"Define '"<<token<<"' with "<<ranges.size()<<" args does not exist\n";
                //  return PARSE_ERROR;
            }
        } else if(-1!=(argIndex = defined->matchArg(token))){
            _PLOG(log::out << log::GRAY<<" match arg "<<token <<", index: "<<argIndex<<"\n";)
            int argStart = argIndex;
            int argEnd = argIndex+1;
            
            if(defined->infiniteArg!=-1){
                // one from argValues is guarranteed. Therfore -1.
                // argumentNames has ... therefore another -1
                int extraArgs = argValues.size() - 1 - (defined->argumentNames.size() - 1);
                if(argIndex>defined->infiniteArg){
                    argStart += extraArgs;
                    argEnd += extraArgs;
                } else if(argIndex==defined->infiniteArg){
                    argEnd += extraArgs;
                }
            }
            _PLOG(log::out << " argValues "<<argValues.size()<<"\n";)
            for(int i=argStart;i<argEnd;i++){
                std::vector<int> args = argValues[i];
                // log::out << log::GRAY<< "range"<<i<<" "<<argRange.start << " "<<argRange.end<<"\n";
                int indexArg=0;
                while(indexArg<(int)args.size()){
                    Token argTok = info.get(INT_DECODE(args[indexArg]));
                    indexArg++;
                    if((rootDefined=info.matchDefine(argTok))){
                        // calculate arguments?
                        std::vector<std::vector<int>> ranges;
                        int newExtraFlags=0;
                        EvaluateArgValues(info,token,0,argValues,ranges,newExtraFlags,indexArg,args);
                        
                        PreprocInfo::CertainDefined* defined = rootDefined->matchArgCount(ranges.size());
                        if(defined){
                            EvaluateRange(info,argTok,defined,ranges,newExtraFlags,false);
                        }else{
                            PERRT(token)<<"Define '"<<token<<"' with "<<ranges.size()<<" args does not exist\n";
                            //  return PARSE_ERROR;
                        }
                    }else{
                        if(i+1==argEnd&&indexArg==(int)args.size())
                            argTok.flags |= token.flags & (TOKEN_SUFFIX_SPACE|TOKEN_SUFFIX_LINE_FEED);
                        if(index==(int)tokens.size()&&i+1==argEnd&&indexArg==(int)args.size())
                            argTok.flags = extraFlags;
                        info.addToken(argTok);
                        _PLOG(log::out << log::GRAY <<"  out << "<<argTok<<"\n");
                    }
                }
            }
            // log::out << "End here\n";
        }else{
            if(index==(int)tokens.size())
                token.flags = extraFlags;
            info.addToken(token);
            _PLOG(log::out << log::GRAY <<" out << "<<token<<"\n");
        }
        unwrapNext=false;
    }
    defined->called=false;
    info.evaluationDepth--;
}
// Default parse function. Simular to ParseBody
int ParseToken(PreprocInfo& info){
    using namespace engone;
    int result = ParseDefine(info,true);
            
    if(result == PARSE_BAD_ATTEMPT)
        result = ParseIfdef(info,true);
    
    if(result==PARSE_SUCCESS)
        return result;
    if(result == PARSE_ERROR){
        info.nextline();
        return result;
    }
    
    Token token = info.next();
    PreprocInfo::Defined* rootDefined=0;
    if((rootDefined = info.matchDefine(token))){
        std::vector<std::vector<int>> argValues;
        
        std::vector<int> tokens;
        // better solution here. pushing to many tokens
        for(int i=info.at()+1;i<(int)info.length();i++){
            tokens.push_back(INT_ENCODE(i));
        }
        int index=0;
        
        std::vector<std::vector<int>> empty;
        int newExtraFlags=0;
        EvaluateArgValues(info,token,0,empty,argValues,newExtraFlags,index,tokens);
        for(int i=0;i<index;i++)
            info.next();
            
        PreprocInfo::CertainDefined* defined = rootDefined->matchArgCount(argValues.size());
        if(defined){
            EvaluateRange(info,token,defined,argValues,newExtraFlags,false);
        }else{
            PERRT(token)<<"Define '"<<token<<"' with "<<argValues.size()<<" args does not exist\n";
            return PARSE_ERROR;
        }
    }else{
        _PLOG(log::out << "Append "<<token<<"\n";)
        info.addToken(token);
    }
    return PARSE_SUCCESS;
}
void Preprocess(Tokens& inTokens, int* error){
    using namespace engone;
    log::out <<log::BLUE<<  "\n##   Preprocessor   ##\n";
    
    PreprocInfo info{};
    info.tokens.tokenData.resize(inTokens.tokenData.max*10); // hopeful estimation
    info.inTokens = inTokens; // Note: don't modify inTokens
    
    while(!info.end()){
        int result = ParseToken(info);
    }
    
    if(info.errors)
        log::out << log::RED << "Preprocessor failed with "<<info.errors<<" errors\n";
    
    log::out << log::BLUE<<"### # # #  #  #  #    #    #\n";
    inTokens.cleanup();
    inTokens = info.tokens;
    if(error)
        *error = info.errors;
}