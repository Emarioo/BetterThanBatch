#include <stdio.h>
#include <string.h>

#include "BetBat/Tokenizer.h"
#include "Engone/PlatformLayer.h"

#ifdef TLOG
#define _TLOG(x) x;
#else
#define _TLOG(x) ;
#endif

int IsInteger(Token& token){
    for(int i=0;i<token.length;i++){
        char chr = token.str[i];
        if(chr<'0'||chr>'9'){
            return false;
        }
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

void Token::print(int printFlags){
    using namespace engone;
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
bool Tokens::copyInfo(Tokens& out){
    out.lines = lines;
    out.enabled = enabled;
    return true;
}
bool Tokens::copy(Tokens& out){
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
bool Tokens::add(const char* str){
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
void Tokens::finalizePointers(){
    for(int i=0;i<(int)tokens.used;i++){
        Token* tok = (Token*)tokens.data+i;
        tok->str = (char*)tokenData.data + (uint64)tok->str;
    }
}
bool Tokens::add(Token token){
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
Token& Tokens::get(uint index){
    return *((Token*)tokens.data + index);
}
uint32 Tokens::length(){
    return tokens.used;
}
void Tokens::cleanup(){
    tokens.resize(0);
    tokenData.resize(0);
    lines=0;
}
void Tokens::printTokens(int tokensPerLine, int flags){
    using namespace engone;
    log::out << "\n####   "<<tokens.used<<" Tokens   ####\n";
    uint i=0;
    for(;i<tokens.used;i++){
        Token* token = ((Token*)tokens.data + i);
        // if(flags&TOKEN_PRINT_LN_COL)
        //     log::out << "["<<token->line << ":"<<token->column<<"] ";
        
        token->print(flags);
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
void Tokens::print(){
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
void Tokens::printData(int charsPerLine){
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
bool Tokens::append(char chr){
    if(tokenData.max < tokenData.used + 1){
        if(!tokenData.resize(tokenData.max*2 + 20))
            return false;
    }
    *((char*)tokenData.data+tokenData.used) = chr;
    tokenData.used++;
    return true;
}
bool Tokens::append(Token& tok){
    if(tokenData.max < tokenData.used + tok.length){
        if(!tokenData.resize(tokenData.max*2 + 2*tok.length))
            return false;
    }
    for(int i=0;i<tok.length;i++){
        *((char*)tokenData.data+tokenData.used+i) = tok.str[i];
    }
    tokenData.used += tok.length;
    return true;
}
Tokens Tokenize(engone::Memory& textData){
    using namespace engone;
    if(textData.m_typeSize!=1) {
        log::out << "Tokenize : size of type in textData must be one (was "<<textData.m_typeSize<<")\n";
        return {};
    }
    _SILENT(log::out << log::BLUE<< "\n##   Tokenizer   ##\n";)
    // Todo: handle errors like outTokens.add returning false
    
    char* text = (char*)textData.data;
    int length = textData.used;
    Tokens outTokens{};
    outTokens.enabled=-1; // enable all layers by default
    // Todo: do not assume token data will be same or less than textData. It's just asking for a bug
    outTokens.tokenData.resize(textData.used*5);
    
    memset(outTokens.tokenData.data,'_',outTokens.tokenData.max); // Good indicator for issues
    
    const char* specials = "+-*/%=<>!&|" "$@#{}()[]" ":;.,";
    int specialLength = strlen(specials);
    int line=1;
    int column=1;
    
    Token token = {};
    token.str = text;
    token.line = line;
    token.column = column;
    
    bool inQuotes = false;
    bool inComment = false;
    bool inEnclosedComment = false;

    // int commentNestDepth = 0; // C doesn't have this and it's not very good. /*/**/*/
    
    bool canBeDot = false; // used to deal with decimals (eg. 255.92) as one token
    
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
        column++;
        if(chr == '\n'){
            line++;
            column=1;
        }
        
        if(chr=='\r'&&(nextChr=='\n'||prevChr=='\n')){
            continue;
        }
        
        bool isQuotes = chr == '"';
        bool isComment = chr=='/' && (nextChr == '/' || nextChr=='*');
        bool isDelim = chr==' ' || chr=='\t' || chr=='\n';
        bool isSpecial = false;
        if(!isQuotes&&!isComment&&!isDelim){
            for(int i=0;i<specialLength;i++){ // No need to run if delim, comment or quote
                if(chr==specials[i]){
                    isSpecial = true;
                    break;
                }
            }
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
            if(isQuotes){
                // Stop reading string token
                inQuotes=false;
                
                token.flags = 0;
                if(nextChr=='\n')
                    token.flags |= TOKEN_SUFFIX_LINE_FEED;
                else if(nextChr==' ')
                    token.flags |= TOKEN_SUFFIX_SPACE;
                token.flags |= TOKEN_QUOTED;
                
                // _TLOG(log::out << " : Add " << token << "\n";)
                _TLOG(log::out << "\" : End quote\n";)
                
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
            if(chr=='@'){
                const char* str_enable = "enable";
                const char* str_disable = "disable";
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
                if(type!=0){
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
                                        outTokens.enabled &= ~(LAYER_PARSER|LAYER_INTERPRETER); // interpreter doesn't work without parser
                                }else if(!strcmp(word,"optimizer")){
                                    if(type==1)
                                        outTokens.enabled |= LAYER_OPTIMIZER | LAYER_PARSER; // optimizer requires parser
                                    else if(type==2)
                                        outTokens.enabled &= ~LAYER_OPTIMIZER;
                                }else if(!strcmp(word,"interpreter")){
                                    if(type==1)
                                        outTokens.enabled |= LAYER_INTERPRETER | LAYER_PARSER; // intepreter requires parser
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
            _TLOG(printf("%c",chr);)
            token = {};
            token.str = (char*)outTokens.tokenData.used;
            token.length = 1;
            outTokens.append(chr);
            if((chr=='='&&nextChr=='=')||
                (chr=='!'&&nextChr=='=')||
                (chr=='+'&&nextChr=='=')||
                (chr=='-'&&nextChr=='=')||
                (chr=='*'&&nextChr=='=')||
                (chr=='/'&&nextChr=='=')||
                (chr=='+'&&nextChr=='+')||
                (chr=='-'&&nextChr=='-')||
                (chr=='&'&&nextChr=='&')||
                (chr=='>'&&nextChr=='>')||
                (chr=='|'&&nextChr=='|')
                ){
                index++;
                column++;
                token.length++;
                _TLOG(printf("%c",nextChr);)
                outTokens.append(nextChr);
            }else if(chr=='.'&&nextChr=='.'&&nextChr2=='.'){
                index+=2;
                column+=2;
                token.length+=2;
                _TLOG(printf("%c",nextChr);)
                _TLOG(printf("%c",nextChr2);)
                outTokens.append(nextChr);
                outTokens.append(nextChr2);
            }else if(chr=='.'&&nextChr=='.'){
                index+=1;
                column+=1;
                token.length+=1;
                _TLOG(printf("%c",nextChr);)
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
            outTokens.add(token);
            token = {};
        }
    }
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
    
    return outTokens;
}