#include <stdio.h>
#include <string.h>

#include "BetBat/Tokenizer.h"
#include "Engone/PlatformLayer.h"

#ifdef TOKENIZER_LOG
#define _TOKENIZER_LOG(x) x;
#else
#define _TOKENIZER_LOG(x)
#endif
void Token::print(int printFlags){
    using namespace engone;
    if(printFlags&TOKEN_PRINT_QUOTES){
        if(flags&TOKEN_QUOTED)
            log::out << "\"";
    }
    for(int i=0;i<length;i++){
        log::out << *(str+i);
    }
    if(printFlags&TOKEN_PRINT_QUOTES){
        if(flags&TOKEN_QUOTED)
            log::out << "\"";
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
Token::operator std::string(){
    return std::string(std::string_view(str,length));
}
bool Tokens::add(Token token){
    if(tokens.max == tokens.used){
        if(!tokens.resize(tokens.max*2 + 100))
            return false;
    }
    // if(tokenData.max < tokenData.used + token.length){
    //     if(!tokenData.resize(tokenData.max*2 + 100 + token.length))
    //         return false;
    // }
    // char* ptr = (char*)tokenData.data+tokenData.used;
    // memcpy(ptr,token.str,token.length);
    // token.str = ptr;
    // tokenData.used+=token.length;
    
    *((Token*)tokens.data + tokens.used) = token;
    
    tokens.used++;
    return true;
}
Token& Tokens::get(int index){
    return *((Token*)tokens.data + index);
}
int Tokens::length(){
    return tokens.used;
}
void Tokens::printTokens(int tokensPerLine, int flags){
    using namespace engone;
    log::out << "\n####   "<<tokens.used<<" Tokens   ####\n";
    int i=0;
    for(;i<tokens.used;i++){
        Token* token = ((Token*)tokens.data + i);
        if(flags&TOKEN_PRINT_LN_COL)
            log::out << "["<<token->line << ":"<<token->column<<"] ";
            // printf("[%d:%d] ",token->line,token->column);
        
        token->print(flags);
        if(flags&TOKEN_PRINT_SUFFIXES){
            if(token->flags&TOKEN_SUFFIX_LINE_FEED)
                log::out << "\\n";
                // printf("\\n");
        }
        if((i+1)%tokensPerLine==0)
            log::out << "\n";
            // printf("\n");
        else
            log::out << " ";
            // printf(" ");
    }
    if(i%tokensPerLine != 0)
        log::out << "\n";
}
void Tokens::printData(int charsPerLine){
    using namespace engone;
    log::out << "\n####   Token Data   ####\n";
    int i=0;
    for(;i<tokenData.used;i++){
        char chr = *((char*)tokenData.data+i);
        // printf("%c",chr);
        log::out << chr;
        // printf("%c:%d ",chr,(int)chr);
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
        // printf("Tokenize : size of type in textData must be one (was "<<textData.m_typeSize<<")\n";
        return {};
    }
    log::out << "\n####   Tokenizer   ####\n";
    // Todo: handle errors like outTokens.add returning false
    
    char* text = (char*)textData.data;
    int length = textData.used;
    Tokens outTokens{};
    outTokens.sourceText = textData;
    outTokens.tokenData.resize(textData.used); // tokenData will probably never be larger than the original text
    
    engone::Memory& tokenData = outTokens.tokenData;
    memset(tokenData.data,'_',tokenData.max); // Good indicator for issues
    
    const char* specials = "+-*/=<>" "$#{}()[]" ":;";
    int specialLength = strlen(specials);
    Token token = {text,0};
    
    int line=1;
    int column=1;
    
    token.line = line;
    token.column = column;
    
    bool inQuotes = false;
    
    int index=-1;
    while(true){
        index++;
        if(index==length)
            break;
        char prevChr = 0;
        char nextChr = 0;
        if(index>0)
            prevChr = text[index-1];
        if(index+1<length)
            nextChr = text[index+1];
        char chr = text[index];
        
        int col = column;
        int ln = line;
        
        bool isQuotes = chr == '"';
        bool isDelim = chr==' ' || chr=='\t' || chr=='\n' || chr=='\r';
        bool isSpecial = false;
        for(int i=0;i<specialLength;i++){
            if(chr==specials[i]){
                isSpecial = true;
                break;
            }
        }
        
        column++;
        if(chr == '\n'){
            line++;
            column=1;
        }
        if(!inQuotes){
            if(token.length==0&&isQuotes){
                // Start reading string token
                inQuotes = true;
                token.str = (char*)tokenData.data+tokenData.used;
                token.length = 0;
                token.line = ln;
                token.column = col;
                _TOKENIZER_LOG(log::out << "\" : Begin quote\n";)
                continue;
            }
        } else {
            if(isQuotes && prevChr!='\\'){
                // Stop reading string token
                inQuotes=false;
                
                token.flags = 0;
                if(nextChr=='\r'||nextChr=='\n')
                    token.flags |= TOKEN_SUFFIX_LINE_FEED;
                else if(nextChr==' ')
                    token.flags |= TOKEN_SUFFIX_SPACE;
                token.flags |= TOKEN_QUOTED;
                
                _TOKENIZER_LOG(log::out << " : Add ";token.print();log::out << "\n";)
                _TOKENIZER_LOG(log::out << "\" : End quote\n";)
                
                outTokens.add(token);
                token = {};
            } else if(!(chr=='\\'&&nextChr=='"')) {
                if(token.length!=0){
                    _TOKENIZER_LOG(log::out << "-";)
                }
                _TOKENIZER_LOG(log::out << chr;)
                
                *((char*)tokenData.data+tokenData.used) = chr;
                tokenData.used++;
                token.length++;
            }
            continue;
        }
        
        if(token.length==0 && isDelim){
            continue;
        }
        
        if(!isDelim&&!isSpecial&&!isQuotes){
            if(token.length!=0){
                _TOKENIZER_LOG(log::out << "-";)
            }
            _TOKENIZER_LOG(log::out << chr;)
            if(token.length==0){
                token.str = (char*)tokenData.data+tokenData.used;
                token.line = ln;
                token.column = col;
            }
            *((char*)tokenData.data+tokenData.used) = chr;
            tokenData.used++;
            token.length++;
        }
        
        if(token.length!=0 && (isDelim || isQuotes || isSpecial || index+1==length)){
            token.flags = 0;
            if(chr=='\r'||chr=='\n')
                token.flags |= TOKEN_SUFFIX_LINE_FEED;
            else if(chr==' ')
                token.flags |= TOKEN_SUFFIX_SPACE;
                
            _TOKENIZER_LOG(log::out << " : Add ";token.print();log::out << "\n";)
                
            outTokens.add(token);
            token = {};
            
            if(isQuotes){
                inQuotes = true;
                token.str = (char*)tokenData.data+tokenData.used;
                token.length = 0;
                token.line = ln;
                token.column = col;
                _TOKENIZER_LOG(log::out << "\" : Begin quote\n";)   
            }
        }
        if(isSpecial){
            _TOKENIZER_LOG(printf("%c",chr);)
            token = {(char*)tokenData.data+tokenData.used,1};
            *((char*)tokenData.data+tokenData.used) = chr;
            tokenData.used++;
            token.line = ln;
            token.column = col;
            if(index+1<length){
                char chrTemp = *(text+index+1); 
                if(chrTemp=='\r' || chrTemp =='\n')
                    token.flags = TOKEN_SUFFIX_LINE_FEED;
                else if(chrTemp==' ')
                    token.flags = TOKEN_SUFFIX_SPACE;
            }
            _TOKENIZER_LOG(log::out << " : Add ";token.print();log::out <<"\n";)
            outTokens.add(token);
            token = {};
        }
    }
    if(token.length!=0){
        // error happened in a way where we need to add a line feed
        _TOKENIZER_LOG(printf("\n");)
    }
    
    if(inQuotes){
        log::out<<"TokenError: Missing end quote for '";
        token.print();
        log::out << "' at "<< token.line<<":"<<token.column<<"\n";
        goto Tokenize_END;
    }
    if(token.length!=0){
        log::out << "TokenError: Token '";
        token.print();
        log::out << "' was left incomplete\n";
        goto Tokenize_END;
    }
    if(outTokens.tokens.used!=0){
        Token* lastToken = ((Token*)outTokens.tokens.data+outTokens.tokens.used-1);
        int64 extraSpace = (int64)tokenData.data + tokenData.max - (int64)lastToken->str - lastToken->length;
        int64 extraSpace2 = tokenData.max-tokenData.used;
        
        if(extraSpace!=extraSpace2)
            log::out << "TokenError: Used memory in tokenData does not match last token's used memory ("<<tokenData.used<<" != "<<((int64)lastToken->str - (int64)tokenData.data + lastToken->length)<<")\n";
            
        if(extraSpace<0||extraSpace2<0)
            log::out << "TokenError: Buffer of tokenData was to small (estimated "<<tokenData.max<<" but needed "<<tokenData.used<<" bytes)\n'";
        
        if(extraSpace!=extraSpace2 || extraSpace<0 || extraSpace2<0)
            goto Tokenize_END;
    }
    
Tokenize_END:
    
    return outTokens;
}