#include "BetBat/MessageTool.h"
#include "BetBat/Tokenizer.h"


void PrintHead(engone::log::Color color, const TokenRange& tokenRange, const StringBuilder& errorCode, TokenStream** prevStream) {
    // , const StringBuilder& stringBuilder) {
    using namespace engone;
    log::out << color;
    if(tokenRange.tokenStream()) {
        log::out << TrimDir(tokenRange.tokenStream()->streamName)<<":"<<(tokenRange.firstToken.line)<<":"<<(tokenRange.firstToken.column);
    } else {
        log::out << "?"<<":"<<(tokenRange.firstToken.line)<<":"<<(tokenRange.firstToken.column);
    }
    log::out << " ("<<errorCode<<")";
    log::out << ": " <<  MESSAGE_COLOR;
    Assert(prevStream);
    if(prevStream)
        *prevStream = tokenRange.tokenStream();
    //  << stringBuilder;
}
CompileError ToCompileError(const char* str){
    Assert(str);
    int len = strlen(str);
    Assert(len > 1);
    
    if(*str == 'E' && str[1] >= '0' && str[1] <= '9') {
        int num = atoi(str + 1);
        // TODO: Hnadle potential error with atoi
        return (CompileError)num;   
    }
    #define CASE(ERR) if(!strcmp(str, #ERR)) return ERR;
    CASE(ERROR_CASTING_TYPES)
    CASE(ERROR_UNDECLARED)
    CASE(ERROR_DUPLICATE_CASE)
    CASE(ERROR_DUPLICATE_DEFAULT_CASE)
    CASE(ERROR_C_STYLED_DEFAULT_CASE)
    CASE(ERROR_BAD_TOKEN_IN_SWITCH)
    CASE(ERROR_MISSING_ENUM_MEMBERS_IN_SWITCH)
    CASE(ERROR_OVERLOAD_MISMATCH)
    CASE(ERROR_NONE)
    CASE(ERROR_UNSPECIFIED)
    #undef CASE
    
    Assert(false);
    
    return ERROR_UNKNOWN;
}
const char* ToCompileErrorString(temp_compile_error stuff) {
    // if(stuff.err == ERROR_NONE)
    //     return "";
    
    if(!stuff.shortVersion) {
        #define CASE(ERR) if(stuff.err == ERR) return #ERR;
        CASE(ERROR_CASTING_TYPES)
        CASE(ERROR_UNDECLARED)
        CASE(ERROR_DUPLICATE_CASE)
        CASE(ERROR_DUPLICATE_DEFAULT_CASE)
        CASE(ERROR_C_STYLED_DEFAULT_CASE)
        CASE(ERROR_BAD_TOKEN_IN_SWITCH)
        CASE(ERROR_MISSING_ENUM_MEMBERS_IN_SWITCH)
        CASE(ERROR_OVERLOAD_MISMATCH)
        CASE(ERROR_NONE)
        CASE(ERROR_UNSPECIFIED)
        CASE(ERROR_UNKNOWN)
        #undef CASE
    }
    
    static std::unordered_map<CompileError,std::string*> errorStrings;
    auto pair = errorStrings.find(stuff.err);
    if(pair != errorStrings.end())
        return pair->second->c_str();
        
    auto str = new std::string();
    errorStrings[stuff.err] = str;
    str->append("E");
    str->append(std::to_string((u32)stuff.err));
    return str->c_str();
}
void PrintHead(engone::log::Color color, const Token& token, const StringBuilder& errorCode, TokenStream** prevStream) {
    PrintHead(color, token.operator TokenRange(), errorCode, prevStream);
}
void PrintCode(const Token& token, const StringBuilder& stringBuilder, TokenStream** prevStream){
     PrintCode(token.operator TokenRange(), stringBuilder, prevStream);
}
void PrintCode(const TokenRange& tokenRange, const StringBuilder& stringBuilder, TokenStream** prevStream){
    using namespace engone;
    if(!tokenRange.tokenStream())
        return;
    int start = tokenRange.startIndex();
    int end = tokenRange.endIndex;
    // If you call PrintCode multiple times in an error message and the file is the same
    // then we don't need to print this.
    // If the lines we print come from different files then we do need to print this because
    // otherwise we will assume the line we print comes from the location the head of error message
    // displayed.
    // Assert(prevStream);
    // if(!prevStream || tokenRange.tokenStream() != *prevStream) {
    if(prevStream && tokenRange.tokenStream() != *prevStream) {
        log::out << log::GRAY << "-- "<<TrimDir(tokenRange.tokenStream()->streamName) <<":"<<tokenRange.firstToken.line << " --\n";
        *prevStream = tokenRange.tokenStream();
    }

    // log::out <<"We " <<start << " "<<end<<"\n";
    while(start>0){
        Token& prev = tokenRange.tokenStream()->get(start-1);
        if(prev.flags&TOKEN_SUFFIX_LINE_FEED){
            break;
        }
        start--;
        // log::out << "start "<<start<<"\n";
    }
    // NOTE: end is exclusive
    while(end<tokenRange.tokenStream()->length()){
        Token& next = tokenRange.tokenStream()->get(end-1); // -1 since end is exclusive
        if(next.flags&TOKEN_SUFFIX_LINE_FEED){
            break;
        }
        end++;
        
        // log::out << "end "<<end<<"\n";
    }
    const log::Color codeColor = log::SILVER;
    const log::Color markColor = log::CYAN;

    int lineDigits = 0;
    int baseColumn = 999999;
    for(int i=start;i<end;i++){
        Token& tok = tokenRange.tokenStream()->get(i);
        int numlen = tok.line>0 ? ((int)log10(tok.line)+1) : 1;
        if(numlen>lineDigits)
            lineDigits = numlen;
        if(tok.column<baseColumn)
            baseColumn = tok.column;
    }
    // log::out << start << " - " <<end<<"\n";
    int currentLine = -1;
    int minPos = -1;
    int maxPos = -1;
    int pos = -1;
    for(int i=start;i<end;i++){
        Token& tok = tokenRange.tokenStream()->get(i);
        if(tok.line != currentLine){
            currentLine = tok.line;
            if(i!=start)
                log::out << "\n";
            int numlen = currentLine>0 ? ((int)log10(currentLine)+1) : 1;
            for(int j=0;j<lineDigits-numlen;j++)
                log::out << " ";
            const char* const linestr = " | ";
            static const int linestrlen = strlen(linestr);
            log::out << codeColor << currentLine<<linestr;
            pos = lineDigits + linestrlen + tok.column-baseColumn;
            if(minPos>pos || minPos == -1)
                minPos = pos;
            for(int j=0;j<tok.column-baseColumn;j++) log::out << " ";
        }
        if(i<tokenRange.startIndex()){
            pos += tok.calcLength();
            if(tok.flags&TOKEN_SUFFIX_SPACE)
                pos += 1;
            log::out << codeColor;
            if(pos>minPos || minPos == -1)
                minPos = pos;
        } else if(i>=tokenRange.endIndex){
            pos += tok.calcLength();
            if(tok.flags&TOKEN_SUFFIX_SPACE)
                pos += 1;
            // if(minPos>pos || minPos == -1)
            //     minPos = pos;
            log::out << codeColor;
        } else {
            log::out << markColor;
            if(i==tokenRange.startIndex())
                pos = minPos;
            pos += tok.calcLength();
            if(tok.flags&TOKEN_SUFFIX_SPACE && 0==(tok.flags&TOKEN_SUFFIX_LINE_FEED) && i+1 != tokenRange.endIndex)
                pos += 1;
            if(pos>maxPos || maxPos==-1)
                maxPos = pos;
        }
        tok.print(false);
    }
    log::out << "\n";
    int msglen = stringBuilder.size();
    // log::out << "len "<<msglen<<"\n";
    log::out << markColor;
    if(msglen+4<minPos){ // +4 for some extra space
        // print message on left side of mark
        for(int i=0;i<minPos-msglen - 1;i++)
            log::out << " ";
        if(stringBuilder.size()!=0)
            log::out << stringBuilder<<" ";
        if(maxPos-minPos>0){
            log::out << "^";
        }   
        for(int i=0;i<maxPos-minPos - 1;i++)
            log::out << "~";
    } else {
        // print msg on right side of mark
        for(int i=0;i<minPos;i++)
            log::out << " ";
        if(maxPos-minPos>0){
            log::out << "^";
        }   
        for(int i=0;i<maxPos-minPos - 1;i++)
            log::out << "~";
        if(stringBuilder.size()!=0)
            log::out << " " << stringBuilder;
    }
    log::out << "\n";
}

void PrintCode(const TokenRange& tokenRange, const char* message){
    StringBuilder temp{};
    temp.borrow(message);   
    PrintCode(tokenRange, temp);
}
void PrintCode(int index, TokenStream* stream, const char* message){
    if(!stream) return;
    TokenRange range{};
    range.firstToken = stream->get(index);
    // range.startIndex = index;
    range.endIndex = index+1;
    // range.tokenStream = stream;
    PrintCode(range, message);
}
void PrintExample(int line, const StringBuilder& stringBuilder){
    using namespace engone;

    const log::Color codeColor = log::SILVER;
    const log::Color markColor = log::CYAN;


    int lastLine = line;
    for(int i=0;i<stringBuilder.size();i++){
        char chr = stringBuilder.data()[i];
        if(chr == '\n') {
            lastLine++;
        }
    }
    int lineDigits = lastLine>0 ? ((int)log10(lastLine)+1) : 1;
    
    int currentLine = line;
    for(int i=0;i<stringBuilder.size();i++){
        char chr = stringBuilder.data()[i];
        if(chr == '\n' || i==0) {
            int numlen = currentLine>0 ? ((int)log10(currentLine)+1) : 1;
            for(int j=0;j<lineDigits-numlen;j++)
                log::out << " ";
            const char* const linestr = " | ";
            static const int linestrlen = strlen(linestr);
            log::out << codeColor << currentLine<<linestr;

            if(chr=='\n') {
                currentLine++;
            }
        }
        log::out << chr;
    }
    log::out << "\n";
}