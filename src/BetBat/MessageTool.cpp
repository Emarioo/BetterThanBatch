#include "BetBat/MessageTool.h"
#include "BetBat/Tokenizer.h"


void PrintCode(TokenRange* tokenRange, const char* message){
    using namespace engone;
    int start = tokenRange->startIndex;
    int end = tokenRange->endIndex;
    while(start>0){
        Token& prev = tokenRange->tokenStream->get(start-1);
        if(prev.flags&TOKEN_SUFFIX_LINE_FEED){
            break;
        }
        start--;
    }
    // NOTE: end is exclusive
    while(end<tokenRange->tokenStream->length()){
        Token& next = tokenRange->tokenStream->get(end-1); // -1 since end is exclusive
        if(next.flags&TOKEN_SUFFIX_LINE_FEED){
            break;
        }
        end++;
    }

    const log::Color codeColor = log::SILVER;
    const log::Color markColor = log::CYAN;

    int lineDigits = 0;
    for(int i=start;i<end;i++){
        Token& tok = tokenRange->tokenStream->get(i);
        int numlen = ((int)log10(tok.line)+1); 
        if(numlen>lineDigits)
            lineDigits = numlen;
    }

    int currentLine = -1;
    int minColumn = 0;
    int maxColumn = -1;
    int column = -1;
    for(int i=start;i<end;i++){
        Token& tok = tokenRange->tokenStream->get(i);
        if(tok.line != currentLine){
            currentLine = tok.line;
            if(i!=start)
                log::out << "\n";
            int numlen = ((int)log10(currentLine)+1); 
            for(int j=0;j<lineDigits-numlen;j++)
                log::out << " ";
            log::out << codeColor << currentLine<<" |> ";
            static const int somelen = strlen(" |> ");
            minColumn = lineDigits + somelen;
            column = minColumn;
        }
        if(i >= tokenRange->startIndex && i< tokenRange->endIndex) {
            log::out << markColor;
            if(i==tokenRange->startIndex)
                column = minColumn;
            column += tok.length;
            if(tok.flags&TOKEN_SUFFIX_SPACE && 0==(tok.flags&TOKEN_SUFFIX_LINE_FEED) && i+1 != tokenRange->endIndex)
                column += 1;
        } else {
            if(i<tokenRange->startIndex){
                minColumn += tok.length;
                if(tok.flags&TOKEN_SUFFIX_SPACE)
                    minColumn += 1;
            }
            log::out << codeColor;
        }
        if(column>maxColumn || maxColumn==-1)
            maxColumn = column;
        tok.print(false);
    }
    log::out << "\n";
    for(int i=0;i<minColumn;i++)
        log::out << " ";
    log::out << markColor;
    for(int i=0;i<maxColumn-minColumn;i++)
        log::out << "~";
    if(message)
        log::out << " " << message;
    log::out << "\n";
}
void PrintCode(int index, TokenStream* stream, const char* message){
    TokenRange range{};
    range.startIndex = index;
    range.endIndex = index+1;
    range.tokenStream = stream;
    PrintCode(&range, message);
}