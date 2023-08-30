#include "BetBat/Tokenizer.h"

#include "Engone/PlatformLayer.h"

bool IsInteger(const Token& token){
    if(token.flags & TOKEN_MASK_QUOTED) return false;
    for(int i=0;i<token.length;i++){
        char chr = token.str[i];
        if(chr<'0'||chr>'9'){
            return false;
        }
    }
    return true;
}
double ConvertDecimal(const Token& token){
    // TODO: string is not null terminated
    //  temporariy changing character after token
    //  is not safe since it could be a memory violation
    char tmp = *(token.str+token.length);
    *(token.str+token.length) = 0;
    double num = atof(token.str);
    *(token.str+token.length) = tmp;
    return num;
}
bool IsAnnotation(const Token& token){
    if(!token.str || token.length==0) return false;
    return *token.str == '@';
}
bool IsName(const Token& token){
    if(token.flags&TOKEN_MASK_QUOTED) return false;
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
bool IsDecimal(const Token& token){
    if(token.flags&TOKEN_MASK_QUOTED) return false;
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
int ConvertInteger(const Token& token){
    // TODO: string is not null terminated
    //  temporariy changing character after token
    //  is not safe since it could be a memory violation
    char tmp = *(token.str+token.length);
    *(token.str+token.length) = 0;
    int num = atoi(token.str);
    *(token.str+token.length) = tmp;
    return num;
}
bool Equal(const Token& token, const char* str){
    return !(token.flags&TOKEN_MASK_QUOTED) && token == str;
}
bool StartsWith(const Token& token, const char* str){
    if(token.flags&TOKEN_MASK_QUOTED)
        return false;
    int len = strlen(str);
    return len <= token.length && !strncmp(token.str, str, len);
}
bool IsHexadecimal(const Token& token){
    if(token.flags & TOKEN_MASK_QUOTED) return false;
    if(!token.str||token.length<3) return false;
    if(token.str[0] != '0') return false;
    if(token.str[1] != 'x') return false;
    // Assert(token.length <= 2 + 8); // restrict to 32 bit hexidecimals. 64-bit not handled properly
    for(int i=2;i<token.length;i++){
        char chr = token.str[i];
        char al = chr&(~32);
        if((chr<'0' || chr > '9') && (al<'A' || al>'F'))
            return false;
    }
    return true;
}
u64 ConvertHexadecimal(const Token& token){
    if(!token.str||token.length<3) return 0;
    if(token.str[0] != '0') return 0;
    if(token.str[1] != 'x') return 0;
    u64 hex = 0;
    for(int i=2;i<token.length;i++){
        char chr = token.str[i];
        if(chr>='0' && chr <='9'){
            hex = 16*hex + chr-'0';
            continue;
        }
        chr = chr&(~32); // what is this for? chr = chr&0x20 ?
        if(chr>='A' && chr<='F'){
            hex = 16*hex + chr-'A' + 10;
            continue;
        }
    }
    return hex;
}
void TokenRange::print(){
    using namespace engone;
    // assert?
    if(!tokenStream()) return;
    
    for(int i=startIndex();i<endIndex;i++){
        Token& tok = tokenStream()->get(i);
        if(!tok.str)
            continue;
        
        if(tok.flags&TOKEN_DOUBLE_QUOTED)
            log::out << '"';
        else if(tok.flags&TOKEN_SINGLE_QUOTED)
            log::out << '\'';
        
        for(int j=0;j<tok.length;j++){
            char chr = *(tok.str+j);
            // Assert(chr!=0);
            if(chr=='\0'){
                log::out << "\\0"; // Is this okay?
            }else if(chr=='\n'){
                log::out << "\\n"; // Is this okay?
            }else
                log::out << chr;
        }
        if(tok.flags&TOKEN_DOUBLE_QUOTED)
            log::out << '"';
        else if(tok.flags&TOKEN_SINGLE_QUOTED)
            log::out << '\'';
            
        if(((tok.flags&TOKEN_SUFFIX_SPACE) || (tok.flags&TOKEN_SUFFIX_LINE_FEED)) && i+1!=endIndex){
            log::out << " ";
        }
    }
}
engone::Logger& operator<<(engone::Logger& logger, TokenRange& tokenRange){
    tokenRange.print();
    return logger;
}
void TokenStream::addImport(const std::string& name, const std::string& as){
    importList.add({name,as});
    auto& p = importList.last().name;
    ReplaceChar((char*)p.data(),p.length(),'\\','/');
}

std::string Token::getLine(){
    int start = tokenIndex;
    int end = tokenIndex+1;
    // log::out <<"We " <<start << " "<<end<<"\n";
    while(start>0){
        Token& prev = tokenStream->get(start-1);
        if(prev.flags&TOKEN_SUFFIX_LINE_FEED){
            break;
        }
        start--;
        // log::out << "start "<<start<<"\n";
    }
    // NOTE: end is exclusive
    while(end<tokenStream->length()){
        Token& next = tokenStream->get(end-1); // -1 since end is exclusive
        if(next.flags&TOKEN_SUFFIX_LINE_FEED){
            break;
        }
        end++;
        // log::out << "end "<<end<<"\n";
    }
    TokenRange range{};
    range.firstToken.tokenIndex = start;
    range.endIndex = end;
    range.firstToken.tokenStream = tokenStream;
    std::string out = "";
    range.feed(out);
    
    // for(int i=start;i<end;i++){
    //     out += std::string(tokenStream->get(i));
    // }
    return out;
}
u32 TokenRange::feed(char* outBuffer, u32 bufferSize) const {
    using namespace engone;
    // assert?
    if(!tokenStream()) return 0;
    // int written = 0;
    char* start = outBuffer;
    char* end = outBuffer + bufferSize;
    #define CHECK_END if (outBuffer == end) return (u64)outBuffer - (u64)start;
    for(int i=startIndex();i<endIndex;i++){
        Token& tok = tokenStream()->get(i);
        
        if(!tok.str)
            continue;
        
        if(tok.flags&TOKEN_DOUBLE_QUOTED){
            *(outBuffer++) = '"';
            CHECK_END
        }
        
        else if(tok.flags&TOKEN_SINGLE_QUOTED){
            *(outBuffer++) = '\'';
            CHECK_END
        }
        
        for(int j=0;j<tok.length;j++){
            char chr = *(tok.str+j);
            if(chr=='\n'){
                *(outBuffer++) = '\\'; // Is this okay? should it create two characters?
                CHECK_END
                *(outBuffer++) = 'n'; // look at other feed functions
                CHECK_END
            }else{
                *(outBuffer++) = chr;
                CHECK_END
            }
        }
        if(tok.flags&TOKEN_DOUBLE_QUOTED){
            *(outBuffer++) = '"';
            CHECK_END
        } else if(tok.flags&TOKEN_SINGLE_QUOTED){
            *(outBuffer++) = '\'';
            CHECK_END
        }
        if(tok.flags&TOKEN_SUFFIX_SPACE && i!=endIndex){
            *(outBuffer++) = ' ';
            CHECK_END
        }
    }
    #undef CHECK_END
    return (u64)outBuffer - (u64)start;
}
u32 TokenRange::queryFeedSize() const {
    using namespace engone;
    // assert?
    if(!tokenStream()) return 0;
    u32 size = 0;
    for(int i=startIndex();i<endIndex;i++){
        Token& tok = tokenStream()->get(i);
        
        if(!tok.str)
            continue;
        
        if(tok.flags&TOKEN_DOUBLE_QUOTED)
            size++;
        else if(tok.flags&TOKEN_SINGLE_QUOTED)
            size++;

        for(int j=0;j<tok.length;j++){
            char chr = *(tok.str+j);
            if(chr=='\n'){
                size+=2;
            }else
                size+=1;
        }
        if(tok.flags&TOKEN_DOUBLE_QUOTED)
            size+=1;
        else if(tok.flags&TOKEN_SINGLE_QUOTED)
            size+=1;
            
        if(tok.flags&TOKEN_SUFFIX_SPACE && i!=endIndex){
            size+=1;
        }
    }
    return size;
}
void TokenRange::feed(std::string& outBuffer) const {
    using namespace engone;
    // assert?
    if(!tokenStream()) return;
    for(int i=startIndex();i<endIndex;i++){
        Token& tok = tokenStream()->get(i);
        
        if(!tok.str)
            continue;
        
        if(tok.flags&TOKEN_DOUBLE_QUOTED)
            outBuffer += '"';
        else if(tok.flags&TOKEN_SINGLE_QUOTED)
            outBuffer += '\'';
        
        for(int j=0;j<tok.length;j++){
            char chr = *(tok.str+j);
            if(chr=='\n'){
                outBuffer += "\\n"; // Is this okay?
            }else
                outBuffer += chr;
        }
        if(tok.flags&TOKEN_DOUBLE_QUOTED)
            outBuffer += '"';
        else if(tok.flags&TOKEN_SINGLE_QUOTED)
            outBuffer += '\'';
            
        if(tok.flags&TOKEN_SUFFIX_SPACE && i!=endIndex){
            outBuffer += " ";
        }
    }
}
void Token::print(bool skipSuffix) const{
    using namespace engone;
    if(!str) {
        return;
    }
    // if(printFlags&TOKEN_PRINT_QUOTES){
        if(flags&TOKEN_DOUBLE_QUOTED)
            log::out << '"';
        else if(flags&TOKEN_SINGLE_QUOTED)
            log::out << '\'';
    // }
    for(int i=0;i<length;i++){
        char chr = *(str+i);
        // Assert(chr!=0);
        if(chr=='\n'){
            log::out << "\\n"; // Is this okay?
        }else if(chr=='\t'){
            log::out << "\\t"; 
        }else if(chr=='\\'){
            log::out << "\\\\"; 
        }else if(chr=='\0'){
            log::out << "\\0"; 
        }else
            log::out << chr;
    }
    // if(printFlags&TOKEN_PRINT_QUOTES){
        if(flags&TOKEN_DOUBLE_QUOTED)
            log::out << '"';
        else if(flags&TOKEN_SINGLE_QUOTED)
            log::out << '\'';
    // }
    if(flags&TOKEN_SUFFIX_SPACE && !skipSuffix){
        log::out << " ";
    }
}
bool Token::operator==(const std::string& text) const {
    if((int)text.length()!=length)
        return false;
    for(int i=0;i<(int)text.length();i++){
        if(str[i]!=text[i])
            return false;
    }
    return true;
}
bool Token::operator==(const Token& text) const {
    if((int)text.length!=length)
        return false;
    for(int i=0;i<(int)text.length;i++){
        if(str[i]!=text.str[i])
            return false;
    }
    return true;
}
bool Token::operator==(const char* text) const{
    int len = strlen(text);
    if(len!=length)
        return false;
    for(int i=0;i<len;i++){
        if(str[i]!=text[i])
            return false;
    }
    return true;
}
bool Token::operator!=(const char* text) const {
    return !(*this == text);
}
int Token::calcLength() const {
    int len = length;
    if(flags & TOKEN_MASK_QUOTED)
        len += 2;
    // TODO: Take care of all special characters but to it in a
    //   scalable way. Not loads of if statements because you will
    //   need this code else where. Like token.print
    for(int i=0;i<length;i++){
        char chr = str[i];
        if(chr=='\n'||chr=='\t'||chr=='\\'){
            len+=1; // one extra
        }
    }
    return len;
}
engone::Logger& operator<<(engone::Logger& logger, Token& token){
    token.print(true);
    return logger;
}
Token::operator std::string() const{
    return std::string(str,length);
}
Token::operator TokenRange() const{
    TokenRange r{};
    r.firstToken = *this;
    // r.startIndex = tokenIndex;
    r.endIndex = tokenIndex+1;
    // r.tokenStream = tokenStream;
    return r;
}
TokenRange Token::range() const {
    TokenRange r{};
    r.firstToken = *this;
    // r.startIndex = tokenIndex;
    r.endIndex = tokenIndex+1;
    // r.tokenStream = tokenStream;
    return r;
    // return (TokenRange)*this;
}
static Token END_TOKEN{"$END$"};
// Token& TokenStream::next(){
//     if(readHead == length())
//         return END_TOKEN;
//     return get(readHead++);
// }
// Token& TokenStream::now(){
//     return get(readHead-1);
// }
// bool TokenStream::end(){
//     Assert(readHead<=length());
//     return readHead==length();
// }
// void TokenStream::finish(){
//     readHead = length();
// }
// int TokenStream::at(){
//     return readHead-1;
// }
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
Token& TokenStream::get(u32 index) const {
    Assert(this);
    if(index>=(u32)length()) return END_TOKEN;
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
void TokenStream::finalizePointers(){
    Assert(!isFinialized());
    // TODO: Used buckets for tokenData so that this step isn't necessary
    //   for the string pointers. tokenIndex and tokenStream is till
    //   required but you might be abke to do that elsewhere.

    for(int i=0;i<(int)tokens.used;i++){
        Token* tok = (Token*)tokens.data+i;
        tok->str = (char*)tokenData.data + (uint64)tok->str;
        tok->tokenIndex = i;
        tok->tokenStream = this;
    }
}
// TokenRange TokenStream::getLine(int index){
//     if(index<0 || index>= length())
//         return {}; // bad
//     TokenRange range{};
//     range.tokenStream = this;
//     range.startIndex = index - 1;
//     range.endIndex = index;

//     while(range.startIndex>=0){
//         Token& token = get(range.startIndex);
//         if(token.flags&TOKEN_MASK_QUOTED){
//             range.startIndex++;
//             break;
//         }
//         range.startIndex--;
//     }
//     if(range.startIndex<0)
//         range.startIndex = 0;
//     while(range.endIndex<length()){
//         Token& token = get(range.endIndex);
//         if(token.flags&TOKEN_MASK_QUOTED){
//             range.endIndex++; // exclusive
//             break;
//         }
//         range.endIndex++;
//     }
//     if(range.endIndex>=length())
//         range.endIndex = length(); // not -1 because it should be exclusive

//     return range;
// }

bool TokenStream::addTokenAndData(const char* str){
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
    token.tokenIndex = tokens.used;
    token.tokenStream = this;
    
    *((Token*)tokens.data + tokens.used) = token;
    
    tokens.used++;
    tokenData.used+=length;
    return true;
}
bool TokenStream::addToken(Token token){
    if(tokens.max == tokens.used){
        if(!tokens.resize(tokens.max*2 + 100))
            return false;
    }
    token.tokenIndex = tokens.used;
    token.tokenStream = this;
    *((Token*)tokens.data + tokens.used) = token;

    tokens.used++;
    return true;
}
bool TokenStream::addData(char chr){
    if(tokenData.max < tokenData.used + 1){
        // engone::log::out << "resize "<<__FUNCTION__<<"\n";
        if(!tokenData.resize(tokenData.max*2 + 20))
            return false;
    }
    *((char*)tokenData.data+tokenData.used) = chr;
    tokenData.used++;
    return true;
}
bool TokenStream::addData(Token token){
    if(tokenData.max < tokenData.used + token.length){
        // engone::log::out << "resize "<<__FUNCTION__<<"\n";
        if(!tokenData.resize(tokenData.max*2 + 2*token.length))
            return false;
    }
    for(int i=0;i<token.length;i++){
        *((char*)tokenData.data+tokenData.used+i) = token.str[i];
    }
    tokenData.used += token.length;
    return true;
}
bool TokenStream::addData(const char* data){
    int len = strlen(data);
    if(tokenData.max < tokenData.used + len){
        // engone::log::out << "resize "<<__FUNCTION__<<"\n";
        if(!tokenData.resize(tokenData.max*2 + 2*len))
            return false;
    }
    for(int i=0;i<len;i++){
        *((char*)tokenData.data+tokenData.used+i) = data[i];
    }
    tokenData.used += len;
    return true;
}
int TokenStream::length() const {
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
    Assert(isFinialized());
    log::out << "\n####   "<<tokens.used<<" Tokens   ####\n";
    uint i=0;
    for(;i<tokens.used;i++){
        Token* token = ((Token*)tokens.data + i);
        if(showlncol)
            log::out << "["<<token->line << ":"<<token->column<<"] ";
        
        token->print();
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
void TokenStream::writeToFile(const std::string& path){
    using namespace engone;
    auto file = FileOpen(path, nullptr, FILE_ALWAYS_CREATE);
    Assert(file);
    #undef WRITE
    #define WRITE(X, L) FileWrite(file, X, L);
    for(int j=0;j<(int)tokens.used;j++){
        Token& token = *((Token*)tokens.data + j);
        
        if(token.flags&TOKEN_DOUBLE_QUOTED)
            WRITE("\"",1);
            
        if(token.flags&TOKEN_SINGLE_QUOTED)
            WRITE("'",1);
    
        WRITE(token.str, token.length);

        if(token.flags&TOKEN_DOUBLE_QUOTED)
            WRITE("\"",1);
        if(token.flags&TOKEN_SINGLE_QUOTED)
            WRITE("'",1);
        if(token.flags&TOKEN_SUFFIX_LINE_FEED)
            WRITE("\n",1);
        if(token.flags&TOKEN_SUFFIX_SPACE)
            WRITE(" ",1);
    }
    #undef WRITE
    FileClose(file);
}
void TokenStream::print(){
    using namespace engone;
    Assert(isFinialized());

    for(int j=0;j<(int)tokens.used;j++){
        Token& token = *((Token*)tokens.data + j);
        
        if(token.flags&TOKEN_DOUBLE_QUOTED)
            log::out << "\"";
            
        if(token.flags&TOKEN_SINGLE_QUOTED)
            log::out << "'";
    
        for(int i=0;i<token.length;i++){
            char chr = *(token.str+i);
            log::out << chr;
        }
        if(token.flags&TOKEN_DOUBLE_QUOTED)
            log::out << "\"";
        if(token.flags&TOKEN_SINGLE_QUOTED)
            log::out << "'";
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
TokenStream* TokenStream::Tokenize(const std::string& filePath){
    u64 fileSize = 0;
    auto file = engone::FileOpen(filePath, &fileSize, engone::FILE_ONLY_READ);
    if(!file)
        return nullptr;
    TextBuffer textBuffer{};
    // textBuffer.buffer = (char*)engone::Allocate(fileSize);
    // TODO: Reuse buffers, request them from compileInfo?
    textBuffer.buffer = TRACK_ARRAY_ALLOC(char, fileSize);
    textBuffer.size = fileSize;
    bool yes = engone::FileRead(file, textBuffer.buffer, textBuffer.size);
    Assert(yes);
        
    engone::FileClose(file);
    
    textBuffer.origin = filePath;
    textBuffer.startColumn = 1;
    textBuffer.startLine = 1;
    
    auto stream = Tokenize(&textBuffer);

    TRACK_ARRAY_FREE(textBuffer.buffer, char, textBuffer.size);
    // engone::Free(textBuffer.buffer, textBuffer.size);
    return stream;
}
TokenStream* TokenStream::Create(){
    #ifdef LOG_ALLOC
    static bool once = false;
    if(!once){
        once = true;
        engone::TrackType(sizeof(TokenStream),"TokenStream");
    }
    #endif

    // TokenStream* ptr = (TokenStream*)engone::Allocate(sizeof(TokenStream));
    TokenStream* ptr = TRACK_ALLOC(TokenStream);
    new(ptr)TokenStream();
    return ptr;
}
void TokenStream::Destroy(TokenStream* stream){
    // stream->cleanup();
    stream->~TokenStream();
    // engone::Free(stream,sizeof(TokenStream));
    TRACK_FREE(stream,TokenStream);
}
bool specialsTable[256]{0};
bool initializedSpecialsTable = false;
void InitSpecialsTable() {
    initializedSpecialsTable = true;
    for(int i=0;i<256;i++) {
        char chr = i;
        char tmp = chr & ~32;
        specialsTable[i] = (tmp<'A'||tmp>'Z')
            && (chr<'0'||chr>'9')
            && (chr!='_');
    }
}
TokenStream* TokenStream::Tokenize(TextBuffer* textBuffer, TokenStream* optionalIn){
    using namespace engone;
    MEASURE
    // PROFILE_SCOPE
    // _VLOG(log::out << log::BLUE<< "##   Tokenizer   ##\n";)
    // TODO: handle errors like outStream->add returning false
    // if(optionalIn){
    //     log::out << log::RED << "tokenize optional in not implemented\n";   
    // }
    if(!initializedSpecialsTable) InitSpecialsTable();
    // char* text = (char*)textData.data;
    // int length = textData.used;
    TokenStream* outStream = nullptr;
    if(optionalIn){
        outStream = optionalIn;
        outStream->cleanup(true);  // clean except for allocations   
    } else {
        outStream = TokenStream::Create();
    }
    u32 length = textBuffer->size;
    char* text = textBuffer->buffer;
    outStream->streamName = textBuffer->origin;
    // outStream->readBytes = length;
    outStream->readBytes += textBuffer->size;
    outStream->lines = 0;
    outStream->enabled=-1; // enable all layers by default
    // TODO: do not assume token data will be same or less than textData. It's just asking for a bug
    if((int)outStream->tokenData.max<length*5){
        // if(optionalIn)
        //     log::out << "Tokenize : token resize even though optionalIn was used\n";
        outStream->tokenData.resize(length*5);
    }
    if((int)outStream->tokens.max<length/5){
        // if(optionalIn)
        //     log::out << "Tokenize : token resize even though optionalIn was used\n";
        outStream->tokens.resize(length/5+100);
    }

    // NOTE: Turn memset off for speed
    // if(outStream->tokenData.data && outStream->tokenData.max!=0)
    //     memset(outStream->tokenData.data,'_',outStream->tokenData.max); // Good indicator for issues
    
    const char* specials = "+-*/%=<>!&|~" "$@#{}()[]" ":;.,";
    int specialLength = strlen(specials);
    int line=textBuffer->startLine;
    int column=textBuffer->startColumn;
    
    Token token = {};
    // token.str = text;
    token.line = line;
    token.column = column;
    
    bool inQuotes = false;
    bool isSingleQuotes = false;
    bool inComment = false;
    bool inEnclosedComment = false;
    
    bool isNumber = false;
    bool isAlpha = false; // alpha and then alphanumeric

    bool inHexidecimal = false;
    // int commentNestDepth = 0; // C doesn't have this and it's not very good. /*/**/*/
    
    bool canBeDot = false; // used to deal with decimals (eg. 255.92) as one token
    
    // preprocessor stuff
    bool foundHashtag=false;

    bool foundNonSpaceOnLine = false;

    int index=0;
    for(int index=0;index<length;/* increment happens below*/) {
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
            continue;// skip \r and process \n next time
        }
        
        if(inComment){
            if(inEnclosedComment){
                if(chr=='\n'){
                    // Questionable decision to use lines counting code here
                    // AND else where too. A bug is bound to happen by how spread out it is.
                    // Bug counter: 1
                    if(!foundNonSpaceOnLine) {
                        outStream->blankLines++;
                        foundNonSpaceOnLine = false;
                    } else
                        outStream->lines++;
                    if(outStream->length()!=0)
                        outStream->get(outStream->length()-1).flags |= TOKEN_SUFFIX_LINE_FEED;   
                }
                // @optimize TODO: move forward to characters at a time
                //   since */ has two characters. By jumping two characters
                //   we are faster and still guarrranteed to hit either * or /.
                //   Then do some checks to sync back to normal. This won't work.
                //   When counting lines since each character has to be checked.
                //   But you can also consider using a loop here which runs very litte and
                //   predictable code.

                if(chr=='*'&&nextChr=='/'){
                    index++;
                    inComment=false;
                    inEnclosedComment=false;
                    _TLOG(log::out << "// : End comment\n";)
                }
            }else{
                // MEASURE;

                // #define OPTIMIZE_COMMENT
                // Thought this code would be an optimization but it
                // doesn't seem like it. Not very noticeable at least.
                #ifdef OPTIMIZE_COMMENT
                if(chr=='\n'||index==length){
                    if(!foundNonSpaceOnLine)
                        outStream->blankLines++;
                    else
                        outStream->lines++;
                    if(outStream->length()!=0)
                        outStream->get(outStream->length()-1).flags |= TOKEN_SUFFIX_LINE_FEED;
                    inComment=false;
                    _TLOG(log::out << "// : End comment\n";)
                } else {
                    index++;
                    while(true){
                        char chr = text[index];
                        // do it in batches?
                        // not worth because comments have
                        // to few characters?
                        // char ch1 = text[index];
                        // char ch2 = text[index];
                        // char ch3 = text[index];
                        if(chr=='\t')
                            column+=4;
                        else
                            column++;
                        index++;
                        if(chr=='\n'||index==length){
                            line++;
                            column=1;
                            if(!foundNonSpaceOnLine)
                                outStream->blankLines++;
                            else
                                outStream->lines++;
                            foundNonSpaceOnLine=false;
                            if(outStream->length()!=0)
                                outStream->get(outStream->length()-1).flags |= TOKEN_SUFFIX_LINE_FEED;

                            if(index + 1 < length){
                                char chr = text[index];
                                char chr2 = text[index+1];
                                if(chr == '/' && chr2 == '/'){
                                    // log::out << "ha\n";
                                    index+=2;
                                    continue;
                                }
                            }
                            inComment=false;
                            _TLOG(log::out << "// : End comment\n";)
                            break;
                        }
                    }
                }
                #else
                if(chr=='\n'||index==length){
                    if(!foundNonSpaceOnLine)
                        outStream->blankLines++;
                    else
                        outStream->lines++;
                    foundNonSpaceOnLine=false;
                    if(outStream->length()!=0)
                        outStream->get(outStream->length()-1).flags |= TOKEN_SUFFIX_LINE_FEED;
                    inComment=false;
                    _TLOG(log::out << "// : End comment\n";)
                }
                #endif
            }
            continue;
        } else if(inQuotes) {
            if((!isSingleQuotes && chr == '"') || 
                (isSingleQuotes && chr == '\'')){
                // Stop reading string token
                inQuotes=false;
                
                token.flags = 0;
                if(nextChr=='\n')
                    token.flags |= TOKEN_SUFFIX_LINE_FEED;
                else if(nextChr==' ')
                    token.flags |= TOKEN_SUFFIX_SPACE;
                if(isSingleQuotes)
                    token.flags |= TOKEN_SINGLE_QUOTED;
                else
                    token.flags |= TOKEN_DOUBLE_QUOTED;
                
                // _TLOG(log::out << " : Add " << token << "\n";)
                _TLOG(if(token.length!=0) log::out << "\n"; log::out << (isSingleQuotes ? '\'' : '"') <<" : End quote\n";)
                
                outStream->addToken(token);
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
                        _TLOG(log::out << "\\n";)
                    } else if(nextChr=='\\'){
                        tmp = '\\';
                        _TLOG(log::out << "\\\\";)
                    } else if(!isSingleQuotes && nextChr=='"'){
                        tmp = '"';
                        _TLOG(log::out << "\"";)
                    } else if(isSingleQuotes && nextChr=='\''){
                        tmp = '\'';
                        _TLOG(log::out << "'";)
                    } else if(nextChr=='t'){
                        tmp = '\t';
                        _TLOG(log::out << "\\t";)
                    } else if(nextChr=='0'){
                        tmp = '\0';
                        _TLOG(log::out << "\\0";)
                    } else if(nextChr=='r'){
                        tmp = '\r';
                        _TLOG(log::out << "\\r";)
                    } else{
                        tmp = '?';
                        _TLOG(log::out << tmp;)
                    }
                }else{
                    _TLOG(log::out << chr;)
                }
                
                outStream->addData(tmp);
                token.length++;
            }
            continue;
        }
        bool isQuotes = chr == '"' ||chr == '\'';
        bool isComment = chr=='/' && (nextChr == '/' || nextChr=='*');
        bool isDelim = chr==' ' || chr=='\t' || chr=='\n';
        if(!isDelim && !isComment){
            foundNonSpaceOnLine = true;
        }
        if(chr == '\n' || index == length) {
            if(foundNonSpaceOnLine){
                outStream->lines++;
                foundNonSpaceOnLine = false;
            } else
                outStream->blankLines++;
        }
        bool isSpecial = false;
        if(!isQuotes&&!isComment&&!isDelim){ // early check for non-special
            
            // quicker but do we identify a character as special when it shouldn't?
            // the if above checks ensures that we only do this if we char isn't
            // quote, comment or delim but are there other cases?

            isSpecial = specialsTable[chr];
            // is a table faster than below?
            // test with optimized build
            // char tmp = chr & ~32;
            // isSpecial = (tmp<'A'||tmp>'Z')
            //     && (chr<'0'||chr>'9')
            //     && (chr!='_');

            // isSpecial = !(
            //        (chr>='a'&&chr<='z')
            //     || (chr>='A'&&chr<='Z')
            //     || (chr>='0'&&chr<='9')
            //     || (chr=='_')
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
            
            // slow, the versions above may not contain all special characters.
            // for(int i=0;i<specialLength;i++){
            //     if(chr==specials[i]){
            //         isSpecial = true;
            //         break;
            //     }
            // }
        }
        
        if(chr=='\n'){
            if(token.str){
               token.flags = (token.flags&(~TOKEN_SUFFIX_SPACE)) | TOKEN_SUFFIX_LINE_FEED;
            }else if(outStream->length()>0){
                Token& last = outStream->get(outStream->length()-1);
                last.flags = (last.flags&(~TOKEN_SUFFIX_SPACE)) | TOKEN_SUFFIX_LINE_FEED;
                // _TLOG(log::out << ", line feed";)
            }
        }
        if(token.length==0 && isDelim)
            continue;
        
        // Treat decimals with . as one token
        if(canBeDot && chr=='.' && nextChr != '.'){
            isSpecial = false;
            canBeDot=false;
        }
        if(!isDelim&&!isSpecial&&!isQuotes&&!isComment){
            if(token.length!=0){
                _TLOG(log::out << "-";)
            }
            _TLOG(log::out << chr;)
            if(token.length==0){
                token.str = (char*)outStream->tokenData.used;
                token.line = ln;
                token.column = col;
                token.flags = 0;
                
                if(chr>='0'&&chr<='9') {
                    canBeDot=true;
                    isNumber=true;
                    if(chr == '0' && nextChr == 'x') {
                        inHexidecimal = true;
                    }
                } else {
                    canBeDot = false;
                    isAlpha = true;
                }
            }
            if(!(chr>='0'&&chr<='9') && chr!='.') // TODO: how does it work with hexidecimals?
                canBeDot=false;
            if(!isNumber || chr!='_') {
                outStream->addData(chr);    
                token.length++;
            }
        }
        
        bool nextLiteralSuffix = false;
        {
            char tmp = nextChr & ~32;
            if(isNumber && !inHexidecimal) {
                nextLiteralSuffix = !inHexidecimal && isNumber && ((tmp>='A'&&tmp<='Z') || nextChr == '_');
            }
        }
        if(token.length!=0 && (isDelim || isQuotes || isComment || isSpecial || nextLiteralSuffix || index==length)){
            token.flags = 0;
            // TODO: is checking line feed necessary? line feed flag of last token is set further up.
            if(chr=='\n')
                token.flags |= TOKEN_SUFFIX_LINE_FEED;
            else if(chr==' ')
                token.flags |= TOKEN_SUFFIX_SPACE;
            if(isAlpha)
                token.flags |= TOKEN_ALPHANUM;
            if(isNumber)
                token.flags |= TOKEN_NUMERIC;
            // _TLOG(log::out << " : Add " << token << "\n";)
            
            canBeDot=false;
            isNumber=false;
            isAlpha=false;
            inHexidecimal = false;
            
            outStream->addToken(token);
            token = {};
            _TLOG(log::out << "\n";)
        }
        // if(!inComment&&!inQuotes){
        if(isComment) {
            inComment=true;
            if(chr=='/' && nextChr=='*'){
                inEnclosedComment=true;
            }
            token.line = ln;
            token.column = col;
            outStream->commentCount++;
            index++; // skip the next slash
            _TLOG(log::out << "// : Begin comment\n";)
            continue;
        } else if(isQuotes){
            inQuotes = true;
            isSingleQuotes = chr == '\'';
            token.str = (char*)outStream->tokenData.used;
            token.length = 0;
            token.line = ln;
            token.column = col;
            _TLOG(log::out << "\" : Begin quote\n";)
            continue;
        }
        // }
        if(isSpecial){
            if(chr=='#'){
                foundHashtag=true; // if not found then preprocessor isn't necessary
            }
            if(chr=='@'){
                Token anot = {};
                anot.str = (char*)outStream->tokenData.used;
                anot.line = ln;
                anot.column = col;
                anot.length = 1;
                anot.flags = TOKEN_SUFFIX_SPACE;
                
                outStream->addData(chr);
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
                    if(c==' '||c=='\t'||c=='\r'||c=='\n'){
                        if(c=='\n')
                            anot.flags |= TOKEN_SUFFIX_LINE_FEED;
                        break;
                    }
                    outStream->addData(c);
                    anot.length++;
                    if (index==length){
                        break;
                    }
                }
                outStream->addToken(anot);
                continue;
            }
            
            // this scope only adds special token
            _TLOG(log::out << chr;)
            token = {};
            token.str = (char*)outStream->tokenData.used;
            token.length = 1;
            outStream->addData(chr);
            if(chr=='.'&&nextChr=='.'&&nextChr2=='.'){
                index+=2;
                column+=2;
                token.length+=2;
                _TLOG(log::out << nextChr;)
                _TLOG(log::out << nextChr2;)
                outStream->addData(nextChr);
                outStream->addData(nextChr2);
            }else if((chr=='='&&nextChr=='=')||
                (chr=='!'&&nextChr=='=')||
                // (chr=='+'&&nextChr=='=')||
                // (chr=='-'&&nextChr=='=')||
                // (chr=='*'&&nextChr=='=')||
                // (chr=='/'&&nextChr=='=')||
                (chr=='<'&&nextChr=='=')||
                (chr=='>'&&nextChr=='=')||
                (chr=='+'&&nextChr=='+')||
                (chr=='-'&&nextChr=='-')||
                (chr=='&'&&nextChr=='&')||
                // (chr=='>'&&nextChr=='>')|| // Arrows can be combined because they are used with polymorphism.
                // (chr=='<'&&nextChr=='<')|| // It would mess up the parsing.
                (chr==':'&&nextChr==':')||
                (chr=='.'&&nextChr=='.')||
                (chr=='|'&&nextChr=='|')
                ){
                index++;
                column++;
                token.length++;
                _TLOG(log::out << nextChr;)
                outStream->addData(nextChr);
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
            outStream->addToken(token);
            token = {};
        }
    }
    // outStream->addTokenAndData("$");
    if(!foundHashtag){
        // preprocessor not necessary, save time by not running it.
        outStream->enabled &= ~LAYER_PREPROCESSOR;
        _TLOG(log::out<<log::LIME<<"Couldn't find #. Disabling preprocessor.\n";)
    }
    
    #ifdef LOG_IMPORTS
    for(auto& str : outStream->importList){
        log::out << log::LIME<<" @import '"<<str<<"'\n";
    }   
    #endif
    token.str = (char*)outStream->tokenData.data + (uint64)token.str;
    token.tokenIndex = outStream->length()-1;
    token.tokenStream = outStream;
    outStream->finalizePointers();
    
    if(token.length!=0){
        // error happened in a way where we need to add a line feed
        _TLOG(log::out << "\n";)
    }

    // README: Seemingly strange tokens can appear if you miss a quote somewhere.
    //  This is not a bug. It happens since quoted tokens are allowed across lines.
    
    if(inQuotes){
        // TODO: Improve error message for tokens
        log::out<<log::RED<<"TokenError: Missing end quote at "<<outStream->streamName <<":"<< token.line<<":"<<token.column<<"\n";
        outStream->tokens.used--; // last token is corrupted and should be removed
        goto Tokenize_END;
    }
    if(inComment){
        log::out<<log::RED<<"TokenError: Missing end comment for "<<token.line<<":"<<token.column<<"\n";
        outStream->tokens.used--; // last token is corrupted and should be removed
        goto Tokenize_END;
    }
    if(token.length!=0){
        log::out << log::RED<<"TokenError: Token '" << token << "' was left incomplete\n";
        goto Tokenize_END;
    }

    // Last token should have line feed.
    if(outStream->length()>0)
        outStream->get(outStream->length()-1).flags |=TOKEN_SUFFIX_LINE_FEED;

    if(outStream->tokens.used!=0){
        Token* lastToken = ((Token*)outStream->tokens.data+outStream->tokens.used-1);
        int64 extraSpace = (int64)outStream->tokenData.data + outStream->tokenData.max - (int64)lastToken->str - lastToken->length;
        int64 extraSpace2 = outStream->tokenData.max-outStream->tokenData.used;
        
        if(extraSpace!=extraSpace2)
            log::out << log::RED<<"TokenError: Used memory in tokenData does not match last token's used memory ("<<outStream->tokenData.used<<" != "<<((int64)lastToken->str - (int64)outStream->tokenData.data + lastToken->length)<<")\n";
            
        if(extraSpace<0||extraSpace2<0)
            log::out << log::RED<<"TokenError: Buffer of tokenData was to small (estimated "<<outStream->tokenData.max<<" but needed "<<outStream->tokenData.used<<" bytes)\n'";
        
        if(extraSpace!=extraSpace2 || extraSpace<0 || extraSpace2<0)
            goto Tokenize_END;
    }
    
Tokenize_END:
    // log::out << "Last: "<<outStream->get(outStream->length()-1)<<"\n";
    return outStream;
}

// int cmpd(const void* a, const void* b){ return (int)(1000000000*(*(double*)a - *(double*)b));}
struct Round {
    double time;
    u32 lines;
    u32 tokens;
};
int cmp_round(const void* a, const void* b){ return (int)(1000000000*((Round*)a)->time - ((Round*)b)->time);}
void PerfTestTokenize(const engone::Memory<char>& textData, int times){
    using namespace engone;
    log::out << "Data "<<textData.used<<" "<<textData.max<<"\n";
    // log::out <<log::RED<< __FUNCTION__ <<" is broken\n";
    TextBuffer textBuffer{};
    textBuffer.buffer = (char*)textData.data;
    textBuffer.size = textData.max;
    TokenStream* baseStream = TokenStream::Tokenize(&textBuffer); // initial tokenize to get sufficient allocations
    // TokenStream* baseStream = nullptr;
    // log::out << "start\n";
    auto rounds = new Round[times];
    auto startT = engone::StartMeasure();
    for(int i=0;i<times;i++){
        // log::out << "running "<<i<<"\n";
        // heap allocation is included.
        auto mini = engone::StartMeasure();
        // base = Tokenize(textData);
        baseStream = TokenStream::Tokenize(&textBuffer, baseStream);
        rounds[i].lines = baseStream->lines;
        rounds[i].tokens = baseStream->tokens.used;
        // rounds[i].lines = baseStream->lines + baseStream->blankLines;
        rounds[i].time = engone::StopMeasure(mini);
        // log::out << baseStream->blankLines<<"\n";
    }
    double total = engone::StopMeasure(startT);
    // log::out << "end\n";
    
    qsort(rounds,times,sizeof(*rounds),cmp_round);
    
    log::out << __FUNCTION__<<" total: "<<FormatTime(total)<<"\n";
    double sum=0;
    double minT=9999999;
    double maxT=0;
    double median = times==1 ? rounds[0].time : rounds[(times+1)/2].time;
    u32 totalLines = 0;
    u32 totalTokens = 0;
    for(int i=0;i<times;i++){
        sum+=rounds[i].time;
        if(minT>rounds[i].time) minT = rounds[i].time;
        if(maxT<rounds[i].time) maxT = rounds[i].time;
        totalLines += rounds[i].lines;
        totalTokens += rounds[i].tokens;
        // log::out << " "<<FormatTime(measures[i])<<"\n";
    }
    log::out << "   average: "<<FormatTime(sum/times)<<"\n";
    log::out << "   min/max: "<<FormatTime(minT)<<" / "<<FormatTime(maxT)<<"\n";
    log::out << "   median: "<<FormatTime(median)<<"\n";
    log::out << "   total lines: "<<totalLines<<", loc/s "<<(totalLines/total)<<"\n";
    log::out << "   tokens (per file): "<<(totalTokens/times)<<", "<<FormatBytes(textData.max/median)<<"/s\n";
    baseStream->cleanup();
}
void PerfTestTokenize(const char* file, int times){
    auto text = ReadFile(file);
    if (!text.data)
        return;
    PerfTestTokenize(text,times);
    if(text.max != 0)
        text.resize(0);

    #ifdef LOG_MEASURES
    PrintMeasures();
    #endif
}