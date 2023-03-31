#include <stdio.h>
#include <string.h>

#include "BetBat/Tokenizer.h"
#include "Engone/PlatformLayer.h"

#ifdef TLOG
#define _TLOG(x) x;
#else
#define _TLOG(x) ;
#endif
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
    token.print(TOKEN_PRINT_QUOTES);
    return logger;
}
Token::operator std::string(){
    return std::string(str,length);
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
    
    char* ptr = (char*)tokenData.data+tokenData.used;
    memcpy(ptr,str,length);
    tokenData.used+=length;
    
    Token token{};
    token.str = ptr;
    token.length = length;
    token.line = -1;
    token.column = -1;
    
    *((Token*)tokens.data + tokens.used) = token;
    
    tokens.used++;
    return true;
}
bool Tokens::add(Token token){
    if(tokens.max == tokens.used){
        if(!tokens.resize(tokens.max*2 + 100))
            return false;
    }
    
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
}
void Tokens::printTokens(int tokensPerLine, int flags){
    using namespace engone;
    log::out << "\n####   "<<tokens.used<<" Tokens   ####\n";
    uint i=0;
    for(;i<tokens.used;i++){
        Token* token = ((Token*)tokens.data + i);
        if(flags&TOKEN_PRINT_LN_COL)
            log::out << "["<<token->line << ":"<<token->column<<"] ";
        
        token->print(flags);
        if(flags&TOKEN_PRINT_SUFFIXES){
            if(token->flags&TOKEN_SUFFIX_LINE_FEED)
                log::out << "\\n";
        }
        if((i+1)%tokensPerLine==0)
            log::out << "\n";
        else if((token->flags&TOKEN_SUFFIX_SPACE)==0)
            log::out << " ";
    }
    if(i%tokensPerLine != 0)
        log::out << "\n";
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
Tokens Tokenize(engone::Memory textData){
    using namespace engone;
    if(textData.m_typeSize!=1) {
        log::out << "Tokenize : size of type in textData must be one (was "<<textData.m_typeSize<<")\n";
        return {};
    }
    log::out << log::BLUE<< "\n##   Tokenizer   ##\n";
    // Todo: handle errors like outTokens.add returning false
    
    char* text = (char*)textData.data;
    int length = textData.used;
    Tokens outTokens{};
    outTokens.tokenData.resize(textData.used); // Todo: do not assume token data will be same or less than textData. It's just asking for a bug
    
    engone::Memory& tokenData = outTokens.tokenData;
    memset(tokenData.data,'_',tokenData.max); // Good indicator for issues
    
    const char* specials = "+-*/=<>" "$#{}()[]" ":;.,";
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
    
    bool canBeDot = false; // used to deal with decimals (eg. 255.92) as one token
    
    int index=0;
    while(true){
        if(index==length)
            break;
        char prevChr = 0;
        char nextChr = 0;
        if(index>0)
            prevChr = text[index-1];
        if(index+1<length)
            nextChr = text[index+1];
        char chr = text[index];
        index++;
        
        int col = column;
        int ln = line;
        
        bool isQuotes = chr == '"';
        bool isComment = chr=='/' && (nextChr == '/' || nextChr=='*');
        bool isDelim = chr==' ' || chr=='\t' || chr=='\n' || chr=='\r';
        bool isSpecial = false;
        if(!isQuotes&&!isComment&&!isDelim){
            for(int i=0;i<specialLength;i++){ // No need to run if delim, comment or quote
                if(chr==specials[i]){
                    isSpecial = true;
                    break;
                }
            }
        }
        
        column++;
        if(chr == '\n'){
            line++;
            column=1;
        }
        if(inComment){
            if(inEnclosedComment){
                if(chr=='*'&&nextChr=='/'){
                    inComment=false;
                    index++;
                    inEnclosedComment=false;
                    _TLOG(log::out << "// : End comment\n";)
                }
            }else{
                if(chr=='\n'||index==length){
                    inComment=false;
                    _TLOG(log::out << "// : End comment\n";)
                }
            }
            continue;
        } else if(inQuotes) {
            if(isQuotes && prevChr!='\\'){
                // Stop reading string token
                inQuotes=false;
                
                token.flags = 0;
                if(nextChr=='\r'||nextChr=='\n')
                    token.flags |= TOKEN_SUFFIX_LINE_FEED;
                else if(nextChr==' ')
                    token.flags |= TOKEN_SUFFIX_SPACE;
                token.flags |= TOKEN_QUOTED;
                
                _TLOG(log::out << " : Add " << token << "\n";)
                _TLOG(log::out << "\" : End quote\n";)
                
                outTokens.add(token);
                token = {};
            } else if(!(chr=='\\'&&nextChr=='"')) {
                if(token.length!=0){
                    _TLOG(log::out << "-";)
                }
                _TLOG(log::out << chr;)
                
                char tmp = chr;
                if(chr=='\\'&&nextChr=='n'){
                    tmp = '\n';
                    index++;
                }
                *((char*)tokenData.data+tokenData.used) = tmp;
                tokenData.used++;
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
                token.str = (char*)tokenData.data+tokenData.used;
                token.line = ln;
                token.column = col;
                
                if(chr>='0'&&chr<='9')
                    canBeDot=true;
                else
                    canBeDot=false;
            }
            if(!(chr>='0'&&chr<='9') && chr!='.')
                canBeDot=false;
            *((char*)tokenData.data+tokenData.used) = chr;
            tokenData.used++;
            token.length++;
        }
        
        if(token.length!=0 && (isDelim || isQuotes || isComment || isSpecial || index==length)){
            token.flags = 0;
            // Todo: is checking line feed necessary? line feed flag of last token is set further up.
            if(chr=='\r'||chr=='\n')
                token.flags |= TOKEN_SUFFIX_LINE_FEED;
            else if(chr==' ')
                token.flags |= TOKEN_SUFFIX_SPACE;
                
            _TLOG(log::out << " : Add " << token << "\n";)
            
            canBeDot=false;
            outTokens.add(token);
            token = {};
        }
        if(!inComment&&!inQuotes){
            if(isComment){
                inComment=true;
                if(chr=='/' && nextChr=='*')
                    inEnclosedComment=true;
                index++; // skip the next slash
                _TLOG(log::out << "// : Begin comment\n";)
                continue;
            }else if(isQuotes){
                inQuotes = true;
                token.str = (char*)tokenData.data+tokenData.used;
                token.length = 0;
                token.line = ln;
                token.column = col;
                _TLOG(log::out << "\" : Begin quote\n";)
                continue;
            }
        }
        if(isSpecial){
            // this scope only adds special token
            _TLOG(printf("%c",chr);)
            token = {};
            token.str = (char*)tokenData.data+tokenData.used;
            token.length = 1;
            *((char*)tokenData.data+tokenData.used) = chr;
            tokenData.used++;
            token.line = ln;
            token.column = col;
            if(nextChr=='\r' || nextChr =='\n')
                token.flags = TOKEN_SUFFIX_LINE_FEED;
            else if(nextChr==' ')
                token.flags = TOKEN_SUFFIX_SPACE;
            _TLOG(log::out << " : Add " << token <<"\n";)
            outTokens.add(token);
            token = {};
        }
    }
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
        int64 extraSpace = (int64)tokenData.data + tokenData.max - (int64)lastToken->str - lastToken->length;
        int64 extraSpace2 = tokenData.max-tokenData.used;
        
        if(extraSpace!=extraSpace2)
            log::out << log::RED<<"TokenError: Used memory in tokenData does not match last token's used memory ("<<tokenData.used<<" != "<<((int64)lastToken->str - (int64)tokenData.data + lastToken->length)<<")\n";
            
        if(extraSpace<0||extraSpace2<0)
            log::out << log::RED<<"TokenError: Buffer of tokenData was to small (estimated "<<tokenData.max<<" but needed "<<tokenData.used<<" bytes)\n'";
        
        if(extraSpace!=extraSpace2 || extraSpace<0 || extraSpace2<0)
            goto Tokenize_END;
    }
    
Tokenize_END:
    
    return outTokens;
}