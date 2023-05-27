#include "BetBat/Tokenizer.h"

int IsInteger(Token& token){
    for(int i=0;i<token.length;i++){
        char chr = token.str[i];
        if(chr<'0'||chr>'9'){
            return false;
        }
    }
    return true;
}
double ConvertDecimal(Token& token){
    // Todo: string is not null terminated
    //  temporariy changing character after token
    //  is not safe since it could be a memory violation
    char tmp = *(token.str+token.length);
    *(token.str+token.length) = 0;
    double num = atof(token.str);
    *(token.str+token.length) = tmp;
    return num;
}
bool IsName(Token& token){
    if(token.flags&TOKEN_QUOTED) return false;
    for(int i=0;i<token.length;i++){
        char chr = token.str[i];
        if(i==0){
            if(!((chr>='A'&&chr<='Z')||
            (chr>='a'&&chr<='z')||chr=='_'))
                return false;
        } else {
            if(!((chr>='A'&&chr<='Z')||
            (chr>='a'&&chr<='z')||
            (chr>='0'&&chr<='9')||chr=='_'))
                return false;
        }
    }
    return true;
}
// Can also be an integer
int IsDecimal(Token& token){
    if(token.flags&TOKEN_QUOTED) return false;
    if(token==".") return false;
    int hasDot=false;
    for(int i=0;i<token.length;i++){
        char chr = token.str[i];
        if(hasDot && chr=='.')
            return false; // cannot have 2 dots
        if(chr=='.')
            hasDot = true;
        else if(chr<'0'||chr>'9')
            return false;
    }
    return true;
}
int ConvertInteger(Token& token){
    // Todo: string is not null terminated
    //  temporariy changing character after token
    //  is not safe since it could be a memory violation
    char tmp = *(token.str+token.length);
    *(token.str+token.length) = 0;
    int num = atoi(token.str);
    *(token.str+token.length) = tmp;
    return num;
}
void TokenRange::print(){
    // assert?
    if(!tokenStream) return;
    
    for(int i=startIndex;i<endIndex;i++){
        tokenStream->get(i).print();
    }
}
engone::Logger& operator<<(engone::Logger& logger, TokenRange& tokenRange){
    tokenRange.print();
    return logger;
}
void TokenRange::feed(std::string& outBuffer){
    using namespace engone;
    // assert?
    if(!tokenStream) return;
    for(int i=startIndex;i<endIndex;i++){
        Token& tok = tokenStream->get(i);
        
        if(!tok.str)
            continue;
        
        if(tok.flags&TOKEN_QUOTED)
            outBuffer += "\"";
        
        for(int j=0;j<tok.length;j++){
            char chr = *(tok.str+j);
            if(chr=='\n'){
                outBuffer += "\\n"; // Is this okay?
            }else
                outBuffer += chr;
        }
            
        if(tok.flags&TOKEN_QUOTED)
            outBuffer += "\"";
            
        if(tok.flags&TOKEN_SUFFIX_SPACE){
            outBuffer += " ";
        }
    }
}
void Token::print(int printFlags){
    using namespace engone;
    if(!str) return;
    // if(printFlags&TOKEN_PRINT_QUOTES){
        if(flags&TOKEN_QUOTED)
            log::out << "\"";
    // }
    for(int i=0;i<length;i++){
        char chr = *(str+i);
        if(chr=='\n'){
            log::out << "\\n"; // Is this okay?
        }else
            log::out << chr;
    }
    // if(printFlags&TOKEN_PRINT_QUOTES){
        if(flags&TOKEN_QUOTED)
            log::out << "\"";
    // }
    if(flags&TOKEN_SUFFIX_SPACE){
        log::out << " ";
    }
}
bool Token::operator==(const char* text){
    int len = strlen(text);
    if(len!=length)
        return false;
    for(int i=0;i<len;i++){
        if(str[i]!=text[i])
            return false;
    }
    return true;
}
bool Token::operator!=(const char* text){
    return !(*this == text);
}
engone::Logger& operator<<(engone::Logger& logger, Token& token){
    // token.print(TOKEN_PRINT_QUOTES);
    token.print(0);
    return logger;
}
Token::operator std::string(){
    return std::string(str,length);
}

Token& TokenStream::next(){
    return get(readHead++);
}
Token& TokenStream::now(){
    return get(readHead-1);
}
bool TokenStream::end(){
    Assert(readHead<=length());
    return readHead==length();
}
void TokenStream::finish(){
    readHead = length();
}
int TokenStream::at(){
    return readHead-1;
}
bool TokenStream::copyInfo(TokenStream& out){
    out.streamName = streamName;
    out.importList = std::move(importList);
    
    out.lines = lines;
    out.readBytes = readBytes;
    out.enabled = enabled;
    memcpy(out.version,version,VERSION_MAX+1);
    // This is dumb, I have forgotten to copy variables
    // twice and I will keep doing it.
    // three times now.
    
    return true;
}
Token& TokenStream::get(int index){
    return *((Token*)tokens.data + index);
}
bool TokenStream::copy(TokenStream& out){
    out.cleanup();
    if(!out.tokenData.resize(tokenData.max))
        return false;
    out.tokenData.used = tokenData.used;
    memcpy(out.tokenData.data,tokenData.data,tokenData.max);
    if(!out.tokens.resize(tokenData.max))
        return false;
    out.tokens.used = tokens.used;
    memcpy(out.tokens.data,tokens.data,tokens.max);
    
    for(int i=0;i<(int)tokens.used;i++){
        Token& tok = *((Token*)tokens.data+i);
        Token& outTok = *((Token*)out.tokens.data+i);
        uint64 offset = (uint64)tok.str-(uint64)tokenData.data;
        outTok.str = (char*)out.tokenData.data + offset;
    }
    copyInfo(out);
    return true;
}

TokenStream* TokenStream::copy(){
    auto out = TokenStream::Create();
    out->tokenData.resize(tokenData.max);
    out->tokenData.used = tokenData.used;
    
    BROKEN;
    // out.cleanup();
    // if(!out.tokenData.resize(tokenData.max))
    //     return false;
    // out.tokenData.used = tokenData.used;
    // memcpy(out.tokenData.data,tokenData.data,tokenData.max);
    // if(!out.tokens.resize(tokenData.max))
    //     return false;
    // out.tokens.used = tokens.used;
    // memcpy(out.tokens.data,tokens.data,tokens.max);
    
    // for(int i=0;i<(int)tokens.used;i++){
    //     Token& tok = *((Token*)tokens.data+i);
    //     Token& outTok = *((Token*)out.tokens.data+i);
    //     uint64 offset = (uint64)tok.str-(uint64)tokenData.data;
    //     outTok.str = (char*)out.tokenData.data + offset;
    // }
    // copyInfo(out);
    return 0;
}
bool TokenStream::add(const char* str){
    if(tokens.max == tokens.used){
        if(!tokens.resize(tokens.max*2 + 100))
            return false;
    }
    int length = strlen(str);
    if(tokenData.max < tokenData.used + length){
        if(!tokenData.resize(tokenData.max*2 + 2*length))
            return false;
    }
    uint64 offset = tokenData.used;
    char* ptr = (char*)tokenData.data+tokenData.used;
    memcpy(ptr,str,length);
    
    Token token{};
    token.str = (char*)offset;
    token.length = length;
    token.line = -1;
    token.column = -1;
    
    *((Token*)tokens.data + tokens.used) = token;
    
    tokens.used++;
    tokenData.used+=length;
    return true;
}
void TokenStream::finalizePointers(){
    for(int i=0;i<(int)tokens.used;i++){
        Token* tok = (Token*)tokens.data+i;
        tok->str = (char*)tokenData.data + (uint64)tok->str;
    }
}
TokenRange TokenStream::getLine(int index){
    if(index<0 || index>= length())
        return {}; // bad
    TokenRange range{};
    range.tokenStream = this;
    range.startIndex = index - 1;
    range.endIndex = index;

    while(range.startIndex>=0){
        Token& token = get(range.startIndex);
        if(token.flags&TOKEN_QUOTED){
            range.startIndex++;
            break;
        }
        range.startIndex--;
    }
    if(range.startIndex<0)
        range.startIndex = 0;
    while(range.endIndex<length()){
        Token& token = get(range.endIndex);
        if(token.flags&TOKEN_QUOTED){
            range.endIndex++; // exclusive
            break;
        }
        range.endIndex++;
    }
    if(range.endIndex>=length())
        range.endIndex = length(); // not -1 because it should be exclusive

    return range;
}
bool TokenStream::add(Token token){
    if(tokens.max == tokens.used){
        if(!tokens.resize(tokens.max*2 + 100))
            return false;
    }
    // if(token.flags&TOKEN_SUFFIX_LINE_FEED)
    //     lines++;
    *((Token*)tokens.data + tokens.used) = token;

    tokens.used++;
    return true;
}
int TokenStream::length(){
    return tokens.used;
}
void TokenStream::cleanup(bool leaveAllocations){
    if(leaveAllocations){
        auto t0 = tokens;
        t0.used = 0;
        auto t1 = tokenData;
        t1.used = 0;
        *this = {};
        tokens = t0;
        tokenData = t1;
    }else{
        tokens.resize(0);
        tokenData.resize(0);
        *this = {};
    }
}
void TokenStream::printTokens(int tokensPerLine, bool showlncol){
    using namespace engone;
    log::out << "\n####   "<<tokens.used<<" Tokens   ####\n";
    uint i=0;
    for(;i<tokens.used;i++){
        Token* token = ((Token*)tokens.data + i);
        if(showlncol)
            log::out << "["<<token->line << ":"<<token->column<<"] ";
        
        token->print(0);
        // if(flags&TOKEN_PRINT_SUFFIXES){
            if(token->flags&TOKEN_SUFFIX_LINE_FEED)
                log::out << "\\n";
        // }
        if((i+1)%tokensPerLine==0)
            log::out << "\n";
        else if((token->flags&TOKEN_SUFFIX_SPACE)==0)
            log::out << " ";
    }
    if(i%tokensPerLine != 0)
        log::out << "\n";
    // log::out << "$END$";
}
void TokenStream::print(){
    using namespace engone;
    for(int j=0;j<(int)tokens.used;j++){
        Token& token = *((Token*)tokens.data + j);
        
        if(token.flags&TOKEN_QUOTED)
            log::out << "\"";
    
        for(int i=0;i<token.length;i++){
            char chr = *(token.str+i);
            log::out << chr;
        }
        if(token.flags&TOKEN_QUOTED)
            log::out << "\"";
        if(token.flags&TOKEN_SUFFIX_LINE_FEED)
            log::out << "\n";
        if(token.flags&TOKEN_SUFFIX_SPACE)
            log::out << " ";
    }
}
void TokenStream::printData(int charsPerLine){
    using namespace engone;
    log::out << "\n####   Token Data   ####\n";
    uint i=0;
    for(;i<tokenData.used;i++){
        char chr = *((char*)tokenData.data+i);
        log::out << chr;
        if((i+1)%charsPerLine==0)
            log::out << "\n";
    }
    if((i)%charsPerLine!=0)
        log::out << "\n";
}
bool TokenStream::append(char chr){
    if(tokenData.max < tokenData.used + 1){
        // engone::log::out << "resize "<<__FUNCTION__<<"\n";
        if(!tokenData.resize(tokenData.max*2 + 20))
            return false;
    }
    *((char*)tokenData.data+tokenData.used) = chr;
    tokenData.used++;
    return true;
}
bool TokenStream::append(Token& tok){
    if(tokenData.max < tokenData.used + tok.length){
        // engone::log::out << "resize "<<__FUNCTION__<<"\n";
        if(!tokenData.resize(tokenData.max*2 + 2*tok.length))
            return false;
    }
    for(int i=0;i<tok.length;i++){
        *((char*)tokenData.data+tokenData.used+i) = tok.str[i];
    }
    tokenData.used += tok.length;
    return true;
}
TokenStream* TokenStream::Tokenize(const std::string& filePath){
    engone::Memory memory = ReadFile(filePath.c_str());
    if(!memory.data)
        return 0;
    auto stream = Tokenize((char*)memory.data,memory.used);
    stream->streamName = filePath;
    memory.resize(0);
    return stream;
}
TokenStream* TokenStream::Create(){
    return new TokenStream();
}
void TokenStream::Destroy(TokenStream* stream){
    // stream->cleanup() not needed
    delete stream;   
}
TokenStream* TokenStream::Tokenize(const char* text, int length, TokenStream* optionalIn){
    using namespace engone;
    // _VLOG(log::out << log::BLUE<< "##   Tokenizer   ##\n";)
    // Todo: handle errors like outTokens.add returning false
    if(optionalIn){
        log::out << log::RED << "tokenize optional in not implemented\n";   
    }
    // char* text = (char*)textData.data;
    // int length = textData.used;
    TokenStream* outStream = TokenStream::Create();
    TokenStream& outTokens = *outStream;
    outStream->readBytes += length;
    // TokenStream outTokens{};
    // if(optionalIn){
    //     outTokens = *optionalIn;
    //     outTokens.cleanup(true); // clean except for allocations   
    // }
    outTokens.enabled=-1; // enable all layers by default
    // Todo: do not assume token data will be same or less than textData. It's just asking for a bug
    if((int)outTokens.tokenData.max<length*5){
        if(optionalIn)
            log::out << "Tokenize : token resize even though optionalIn was used\n";
        outTokens.tokenData.resize(length*5);
    }
    
    memset(outTokens.tokenData.data,'_',outTokens.tokenData.max); // Good indicator for issues
    
    const char* specials = "+-*/%=<>!&|~" "$@#{}()[]" ":;.,";
    int specialLength = strlen(specials);
    int line=1;
    int column=1;
    
    Token token = {};
    // token.str = text;
    token.line = line;
    token.column = column;
    
    bool inQuotes = false;
    bool inComment = false;
    bool inEnclosedComment = false;

    // int commentNestDepth = 0; // C doesn't have this and it's not very good. /*/**/*/
    
    bool canBeDot = false; // used to deal with decimals (eg. 255.92) as one token
    
    bool foundHashtag=false;

    int index=0;
    while(true){
        if(index==length)
            break;
        char prevChr = 0;
        char nextChr = 0;
        char nextChr2 = 0;
        if(index>0)
            prevChr = text[index-1];
        if(index+1<length)
            nextChr = text[index+1];
        if(index+2<length)
            nextChr2 = text[index+2];
        char chr = text[index];
        index++;
        
        int col = column;
        int ln = line;
        if(chr=='\t')
            column+=4;
        else
            column++;
        if(chr == '\n'){
            line++;
            column=1;
        }
        
        if(chr=='\r'&&(nextChr=='\n'||prevChr=='\n')){
            continue;
        }
        
        if(inComment){
            if(inEnclosedComment){
                if(chr=='\n'){
                    if(outTokens.length()!=0)
                        outTokens.get(outTokens.length()-1).flags |= TOKEN_SUFFIX_LINE_FEED;   
                }
                if(chr=='*'&&nextChr=='/'){
                    index++;
                    inComment=false;
                    inEnclosedComment=false;
                    _TLOG(log::out << "// : End comment\n";)
                }
            }else{
                if(chr=='\n'||index==length){
                    if(outTokens.length()!=0)
                        outTokens.get(outTokens.length()-1).flags |= TOKEN_SUFFIX_LINE_FEED;
                    inComment=false;
                    _TLOG(log::out << "// : End comment\n";)
                }
            }
            continue;
        } else if(inQuotes) {
            if(chr == '"'){
                // Stop reading string token
                inQuotes=false;
                
                token.flags = 0;
                if(nextChr=='\n')
                    token.flags |= TOKEN_SUFFIX_LINE_FEED;
                else if(nextChr==' ')
                    token.flags |= TOKEN_SUFFIX_SPACE;
                token.flags |= TOKEN_QUOTED;
                
                // _TLOG(log::out << " : Add " << token << "\n";)
                _TLOG(if(token.length!=0) log::out << "\n"; log::out << "\" : End quote\n";)
                
                outTokens.add(token);
                token = {};
            } else {
                if(token.length!=0){
                    _TLOG(log::out << "-";)
                }
                
                char tmp = chr;
                if(chr=='\\'){
                    index++;
                    if(nextChr=='n'){
                        tmp = '\n';
                        _TLOG(log::out << "\n";)
                    }else if(nextChr=='\\'){
                        tmp = '\\';
                        _TLOG(log::out << "\\";)
                    }else if(nextChr=='"'){
                        tmp = '"';
                        _TLOG(log::out << "\"";)
                    }else if(nextChr=='t'){
                        tmp = '\t';
                        _TLOG(log::out << "\\t";)
                    }else{
                        tmp = '?';
                        _TLOG(log::out << tmp;)
                    }
                }else{
                    _TLOG(log::out << chr;)
                }
                
                outTokens.append(tmp);
                token.length++;
            }
            continue;
        }
        bool isQuotes = chr == '"';
        bool isComment = chr=='/' && (nextChr == '/' || nextChr=='*');
        bool isDelim = chr==' ' || chr=='\t' || chr=='\n';
        bool isSpecial = false;
        if(!isQuotes&&!isComment&&!isDelim){ // early check for non-special
            
            // quicker but do we identify a character as special when it shouldn't?
            // isSpecial = !(false
            //     || (chr>='a'&&chr<='z')
            //     || (chr>='A'&&chr<='Z')
            //     || (chr>='0'&&chr<='9')
            //     || (chr!='_')
            // );
            
            // faster than for loop slower than a-z, 0-9
            // isSpecial = false
            //     ||chr=='+'||chr=='-'||chr=='*'||chr=='/'
            //     ||chr=='%'||chr=='='||chr=='<'||chr=='>'
            //     ||chr=='!'||chr=='&'||chr=='|'||chr=='$'
            //     ||chr=='@'||chr=='#'||chr=='{'||chr=='}'
            //     ||chr=='('||chr==')'||chr=='['||chr==']'
            //     ||chr==':'||chr==';'||chr=='.'||chr==','
            //     ||chr=='~'
            // ;
            // "+-*/%=<>!&|" "$@#{}()[]" ":;.,"
            
            // slow
            for(int i=0;i<specialLength;i++){
                if(chr==specials[i]){
                    isSpecial = true;
                    break;
                }
            }
        }
        
        if(chr=='\n'){
            if(token.str){
               token.flags = (token.flags&(~TOKEN_SUFFIX_SPACE)) | TOKEN_SUFFIX_LINE_FEED;
            }else if(outTokens.length()>0){
                Token& last = outTokens.get(outTokens.length()-1);
                last.flags = (last.flags&(~TOKEN_SUFFIX_SPACE)) | TOKEN_SUFFIX_LINE_FEED;
                // _TLOG(log::out << ", line feed";)
            }
        }
        if(token.length==0 && isDelim)
            continue;
        
        // Treat decimals with . as one token
        if(canBeDot && chr=='.'){
            isSpecial = false;
            canBeDot=false;
        }
        if(!isDelim&&!isSpecial&&!isQuotes&&!isComment){
            if(token.length!=0){
                _TLOG(log::out << "-";)
            }
            _TLOG(log::out << chr;)
            if(token.length==0){
                token.str = (char*)outTokens.tokenData.used;
                token.line = ln;
                token.column = col;
                
                if(chr>='0'&&chr<='9')
                    canBeDot=true;
                else
                    canBeDot=false;
            }
            if(!(chr>='0'&&chr<='9') && chr!='.')
                canBeDot=false;
            outTokens.append(chr);
            token.length++;
        }
        
        if(token.length!=0 && (isDelim || isQuotes || isComment || isSpecial || index==length)){
            token.flags = 0;
            // Todo: is checking line feed necessary? line feed flag of last token is set further up.
            if(chr=='\n')
                token.flags |= TOKEN_SUFFIX_LINE_FEED;
            else if(chr==' ')
                token.flags |= TOKEN_SUFFIX_SPACE;
                
            // _TLOG(log::out << " : Add " << token << "\n";)
            
            canBeDot=false;
            outTokens.add(token);
            token = {};
            _TLOG(log::out << "\n";)
        }
        if(!inComment&&!inQuotes){
            if(isComment){
                inComment=true;
                if(chr=='/' && nextChr=='*'){
                    inEnclosedComment=true;
                }
                index++; // skip the next slash
                _TLOG(log::out << "// : Begin comment\n";)
                continue;
            }else if(isQuotes){
                inQuotes = true;
                token.str = (char*)outTokens.tokenData.used;
                token.length = 0;
                token.line = ln;
                token.column = col;
                _TLOG(log::out << "\" : Begin quote\n";)
                continue;
            }
        }
        if(isSpecial){
            if(chr=='#'){
                foundHashtag=true; // if not found then preprocessor isn't necessary
            }
            if(chr=='@'){
                const char* str_enable = "enable";
                const char* str_disable = "disable";
                const char* str_version = "version";
                const char* str_import = "import";
                int type=0;
                if(length-index>=(int)strlen(str_enable)){
                    if(0==strncmp(text+index,str_enable,strlen(str_enable))){
                        type = 1;
                        index += strlen(str_enable);
                        ln = column += strlen(str_enable);
                    }   
                }
                if(type==0){
                    if(length-index>=(int)strlen(str_disable)){
                        if(0==strncmp(text+index,str_disable,strlen(str_disable))){
                            type = 2;
                            index += strlen(str_disable);
                            ln = column += strlen(str_disable);
                        }
                    }
                }
                if(type==0){
                    if(length-index>=(int)strlen(str_version)){
                        if(0==strncmp(text+index,str_version,strlen(str_version))){
                            type = 3;
                            index += strlen(str_version);
                            ln = column += strlen(str_version);
                        }
                    }
                }
                if(type==0){
                    if(length-index>=(int)strlen(str_import)){
                        if(0==strncmp(text+index,str_import,strlen(str_import))){
                            type = 4;
                            index += strlen(str_import);
                            ln = column += strlen(str_import);
                        }
                    }
                }
                if(type==4){
                    // @import anything\n
                    int startIndex=-1;
                    // TODO: Code below WILL have some edge cases not accounted for. Check it out.
                    while(index<length){
                        char c = text[index];
                        char nc = 0;
                        char pc = 0;

                        if(index>0)
                            pc = text[index-1];
                        if(index+1<length)
                            nc = text[index+1];
                        index++;
                        if(c=='\t')
                            column+=4;
                        else
                            column++;
                        if(c == '\n'){
                            line++;
                            column=1;
                        }
                        if(startIndex!=-1){
                            if(c=='\r'||c=='\n'||index==length){
                                int len = index-startIndex-1;
                                Token temp{};
                                temp.length = len;
                                temp.str = (char*)text + startIndex;
                                // if(outTokeimportList){
                                outStream->importList.push_back(temp);
                                ReplaceChar((char*)outStream->importList.back().data(),outStream->importList.back().length(),'\\','/');
                                // }else{
                                //     log::out << log::RED << "Import list was not available. Cannot import '"<<temp<<"'\n";   
                                // }
                                break;
                            }
                        }
                        if(c=='\r'&&(nc=='\n'||pc=='\n')){
                            break;
                        }

                        if(c==' '||c=='\t'){
                            
                        }else{
                            if(startIndex==-1){
                                startIndex = index-1;
                            }
                        }
                    }
                    continue;
                }
                if(type==3){
                    int startIndex=-1;
                    bool bad=false;
                    // TODO: Code below WILL have some edge cases not accounted for. Check it out.
                    while(index<length){
                        char c = text[index];
                        char nc = 0;
                        char pc = 0;

                        if(index>0)
                            pc = text[index-1];
                        if(index+1<length)
                            nc = text[index+1];
                        index++;
                        if(c=='\t')
                            column+=4;
                        else
                            column++;
                        if(c == '\n'){
                            line++;
                            column=1;
                        }
                        if(startIndex!=-1){
                            if(c==' '||c=='\t'||c=='\r'||c=='\n'||index==length){
                                int len = index-startIndex-1;
                                if(len>TokenStream::VERSION_MAX){
                                    log::out << log::RED << "TokenError "<<ln<<":"<<col<<": version to big for buffer (was "<<len<<",max "<<TokenStream::VERSION_MAX<<")\n"; 
                                    bad=true;
                                }else{
                                    memcpy(outTokens.version,text+startIndex,len);
                                    outTokens.version[len] = 0;
                                }
                                break;
                            }
                        }
                        if(c=='\r'&&(nc=='\n'||pc=='\n')){
                            break;
                        }

                        if(c==' '||c=='\t'){
                            
                        }else{
                            if(startIndex==-1){
                                startIndex = index-1;
                            }
                        }
                    }
                    if(!bad){
                        _TLOG(log::out <<log::GREEN<< "@version '"<<outTokens.version<<"'\n";)
                    }
                    continue;
                }else if(type!=0){
                    const int WORDMAX = 100;
                    char word[WORDMAX];
                    int len = 0;
                    while(index<length){
                        char c = text[index];
                        char nc = 0;
                        char pc = 0;
                        if(index>0)
                            pc = text[index-1];
                        if(index+1<length)
                            nc = text[index+1];
                        index++;
                        if(c=='\t')
                            column+=4;
                        else
                            column++;
                        if(c == '\n'){
                            line++;
                            column=1;
                        }
                        
                        if(c=='\r'&&(nc=='\n'||pc=='\n')){
                            continue;
                        }
                        
                        
                        if(c!=' '&&c!='\n'){
                            if(len+1<WORDMAX){
                                word[len++] = c;
                            }else{
                                log::out << log::RED << "TokenError "<<ln<<":"<<col<<": word buffer to small for @enable, @disable\n"; 
                            }
                        }
                        if(c==' '||index==length||c=='\n'){
                            if(len!=0){
                                word[len] = 0;
                                bool bad=false;
                                if(!strcmp(word,"all")){
                                    if(type==1)
                                        outTokens.enabled = -1;
                                    else if(type==2)
                                        outTokens.enabled = 0;
                                }else if(!strcmp(word,"preprocessor")){
                                    if(type==1)
                                        outTokens.enabled |= LAYER_PREPROCESSOR;
                                    else if(type==2)
                                        outTokens.enabled &= ~LAYER_PREPROCESSOR;
                                }else if(!strcmp(word,"parser")){
                                    if(type==1)
                                        outTokens.enabled |= LAYER_PARSER;
                                    else if(type==2)
                                        outTokens.enabled &= ~(LAYER_PARSER|LAYER_GENERATOR|LAYER_OPTIMIZER|LAYER_INTERPRETER); // interpreter doesn't work without parser
                                }else if(!strcmp(word,"generator")){
                                    if(type==1)
                                        outTokens.enabled |= LAYER_GENERATOR|LAYER_PARSER;
                                    else if(type==2)
                                        outTokens.enabled &= ~(LAYER_GENERATOR|LAYER_OPTIMIZER|LAYER_INTERPRETER); // interpreter doesn't work without parser
                                }else if(!strcmp(word,"optimizer")){
                                    if(type==1)
                                        outTokens.enabled |= LAYER_OPTIMIZER | LAYER_PARSER | LAYER_GENERATOR; // optimizer requires parser
                                    else if(type==2)
                                        outTokens.enabled &= ~LAYER_OPTIMIZER;
                                }else if(!strcmp(word,"interpreter")){
                                    if(type==1)
                                        outTokens.enabled |= LAYER_INTERPRETER | LAYER_GENERATOR | LAYER_PARSER; // intepreter requires parser
                                    else if(type==2)
                                        outTokens.enabled &= ~LAYER_INTERPRETER;
                                }else{
                                    log::out << log::RED << "TokenError "<<ln<<":"<<col<<": unknown layer '"<<word<<"'\n"; 
                                    bad=true;
                                }
                                if(!bad){
                                    if(type==1)
                                        log::out <<log::GREEN<< "@enable '"<<word<<"'\n";
                                    else if(type==2)
                                        log::out <<log::GREEN<< "@disable '"<<word<<"'\n";
                                }
                                
                                len=0;
                            }
                            if(c=='\n')
                                break;
                            col = column;
                            ln = line;
                        }
                    }
                    continue;
                }
            }
            
            // this scope only adds special token
            _TLOG(log::out << chr;)
            token = {};
            token.str = (char*)outTokens.tokenData.used;
            token.length = 1;
            outTokens.append(chr);
            if(chr=='.'&&nextChr=='.'&&nextChr2=='.'){
                index+=2;
                column+=2;
                token.length+=2;
                _TLOG(log::out << nextChr;)
                _TLOG(log::out << nextChr2;)
                outTokens.append(nextChr);
                outTokens.append(nextChr2);
            }else if((chr=='='&&nextChr=='=')||
                (chr=='!'&&nextChr=='=')||
                (chr=='+'&&nextChr=='=')||
                (chr=='-'&&nextChr=='=')||
                (chr=='*'&&nextChr=='=')||
                (chr=='/'&&nextChr=='=')||
                (chr=='<'&&nextChr=='=')||
                (chr=='>'&&nextChr=='=')||
                (chr=='+'&&nextChr=='+')||
                (chr=='-'&&nextChr=='-')||
                (chr=='&'&&nextChr=='&')||
                (chr=='>'&&nextChr=='>')||
                (chr=='<'&&nextChr=='<')||
                (chr==':'&&nextChr==':')||
                (chr=='.'&&nextChr=='.')||
                (chr=='|'&&nextChr=='|')
                ){
                index++;
                column++;
                token.length++;
                _TLOG(log::out << nextChr;)
                outTokens.append(nextChr);
            }
            if(index<length)
                nextChr = text[index]; // code below needs the updated nextChr
            else
                nextChr = 0;
            
            token.line = ln;
            token.column = col;
            if(nextChr =='\n')
                token.flags = TOKEN_SUFFIX_LINE_FEED;
            else if(nextChr==' ')
                token.flags = TOKEN_SUFFIX_SPACE;
            // _TLOG(log::out << " : Add " << token <<"\n";)
            _TLOG(log::out << " : special\n";)
            outTokens.add(token);
            token = {};
        }
    }
    if(!foundHashtag){
        // preprocessor not necessary, save time by not running it.
        outTokens.enabled &= ~LAYER_PREPROCESSOR;
        _TLOG(log::out<<log::LIME<<"Couldn't find #. Disabling preprocessor.\n";)
    }
    
    #ifdef LOG_IMPORTS
    for(auto& str : outStream->importList){
        log::out << log::LIME<<" @import '"<<str<<"'\n";
    }   
    #endif

    outTokens.lines = line;
    outTokens.finalizePointers();
    // Last token should have line feed.
    if(outTokens.length()>0)
        outTokens.get(outTokens.length()-1).flags |=TOKEN_SUFFIX_LINE_FEED;
    
    if(token.length!=0){
        // error happened in a way where we need to add a line feed
        _TLOG(log::out << "\n";)
    }
    
    if(inQuotes){
        log::out<<log::RED<<"TokenError: Missing end quote for '"<<token << "' at "<< token.line<<":"<<token.column<<"\n";
        goto Tokenize_END;
    }
    if(inComment){
        log::out<<log::RED<<"TokenError: Missing end comment for "<<token.line<<":"<<token.column<<"\n";
        goto Tokenize_END;
    }
    if(token.length!=0){
        log::out << log::RED<<"TokenError: Token '" << token << "' was left incomplete\n";
        goto Tokenize_END;
    }
    if(outTokens.tokens.used!=0){
        Token* lastToken = ((Token*)outTokens.tokens.data+outTokens.tokens.used-1);
        int64 extraSpace = (int64)outTokens.tokenData.data + outTokens.tokenData.max - (int64)lastToken->str - lastToken->length;
        int64 extraSpace2 = outTokens.tokenData.max-outTokens.tokenData.used;
        
        if(extraSpace!=extraSpace2)
            log::out << log::RED<<"TokenError: Used memory in tokenData does not match last token's used memory ("<<outTokens.tokenData.used<<" != "<<((int64)lastToken->str - (int64)outTokens.tokenData.data + lastToken->length)<<")\n";
            
        if(extraSpace<0||extraSpace2<0)
            log::out << log::RED<<"TokenError: Buffer of tokenData was to small (estimated "<<outTokens.tokenData.max<<" but needed "<<outTokens.tokenData.used<<" bytes)\n'";
        
        if(extraSpace!=extraSpace2 || extraSpace<0 || extraSpace2<0)
            goto Tokenize_END;
    }
    
    
Tokenize_END:
    
    return outStream;
}

int cmpd(const void* a, const void* b){ return (int)(1000000000*(*(double*)a - *(double*)b));}
void PerfTestTokenize(const engone::Memory& textData, int times){
    using namespace engone;
    log::out <<log::RED<< __FUNCTION__ <<" is broken\n";
    // TokenStream base = TokenStream::Tokenize(textData); // initial tokenize to get sufficient allocations
    
    // log::out << "start\n";
    // double* measures = new double[times];
    // auto startT = engone::MeasureSeconds();
    // for(int i=0;i<times;i++){
    //     // heap allocation is included.
    //     auto mini = engone::MeasureSeconds();
    //     // base = Tokenize(textData);
    //     base = TokenStream::Tokenize(textData,0,&base);
    //     measures[i] = engone::StopMeasure(mini);
    // }
    // double total = engone::StopMeasure(startT);
    // log::out << "end\n";
    
    // qsort(measures,times,sizeof(double),cmpd);
    
    // log::out << __FUNCTION__<<" total: "<<FormatTime(total)<<"\n";
    // double sum=0;
    // double minT=9999999;
    // double maxT=0;
    // double median = measures[(times+1)/2];
    // for(int i=0;i<times;i++){
    //     sum+=measures[i];
    //     if(minT>measures[i]) minT = measures[i];
    //     if(maxT<measures[i]) maxT = measures[i];
    //     // log::out << " "<<FormatTime(measures[i])<<"\n";
    // }
    // log::out << "   average on individuals: "<<FormatTime(sum/times)<<"\n";
    // log::out << "   min/max: "<<FormatTime(minT)<<" / "<<FormatTime(maxT)<<"\n";
    // log::out << "   median: "<<FormatTime(median)<<"\n";
    // base.cleanup();
}
void PerfTestTokenize(const char* file, int times){
    auto text = ReadFile(file);
    if (!text.data)
        return;
    PerfTestTokenize(text,times);    
    text.resize(0);
}