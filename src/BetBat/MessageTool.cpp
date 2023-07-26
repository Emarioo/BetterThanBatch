#include "BetBat/MessageTool.h"
#include "BetBat/Tokenizer.h"


void PrintHead(engone::log::Color color, const TokenRange& tokenRange, const StringBuilder& errorCode) {
    // , const StringBuilder& stringBuilder) {
    using namespace engone;
    log::out << color;
    log::out << TrimDir(tokenRange.tokenStream()->streamName)<<":"<<(tokenRange.firstToken.line)<<":"<<(tokenRange.firstToken.column);
    log::out << " ("<<errorCode<<")";
    log::out << ": " <<  MESSAGE_COLOR;
    //  << stringBuilder;
}
void PrintHead(engone::log::Color color, const Token& token, const StringBuilder& errorCode) {
    // , const StringBuilder& stringBuilder) {
    PrintHead(color, token.operator TokenRange(), errorCode);
    // , stringBuilder);
}

void PrintCode(const TokenRange& tokenRange, const StringBuilder& stringBuilder){
    using namespace engone;
    if(!tokenRange.tokenStream())
        return;
    int start = tokenRange.startIndex();
    int end = tokenRange.endIndex;
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
void PrintCode(const Token& token, const StringBuilder& stringBuilder){
     PrintCode(token.operator TokenRange(), stringBuilder);
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