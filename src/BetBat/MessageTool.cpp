#include "BetBat/MessageTool.h"
#include "BetBat/Tokenizer.h"
#include "BetBat/Lexer.h"

void PrintHead(engone::log::Color color, const TokenRange& tokenRange, const StringBuilder& errorCode, TokenStream** prevStream) {
    // , const StringBuilder& stringBuilder) {
    using namespace engone;
    log::out << color;
    if(tokenRange.tokenStream()) {
        // log::out << "./"+ TrimDir(tokenRange.tokenStream()->streamName)<<":"<<(tokenRange.firstToken.line)<<":"<<(tokenRange.firstToken.column);
        // log::out << "."+ tokenRange.tokenStream()->streamName<<":"<<(tokenRange.firstToken.line)<<":"<<(tokenRange.firstToken.column);

        // NOTE: Stream name is an absolute path. If the path contains CWD then skip that part to reduce
        //   the clutter on the screen. The user can probably deduce where the file is even if we drop the CWD part.
        //   Make sure you can still click on the path in your editor/terminal to quick jump to the location. (at least in vscode)
        std::string cwd = engone::GetWorkingDirectory() + "/";
        ReplaceChar((char*)cwd.data(), cwd.length(), '\\','/');
        int index = 0;
        while(tokenRange.tokenStream()->streamName.size() > index && cwd.size() > index) {
            if(tokenRange.tokenStream()->streamName[index] != cwd[index])
                break;
            index++;
        }
        if(index != 0)
            log::out << "./" << tokenRange.tokenStream()->streamName.substr(index)<<":"<<(tokenRange.firstToken.line)<<":"<<(tokenRange.firstToken.column);
        else
            log::out << tokenRange.tokenStream()->streamName<<":"<<(tokenRange.firstToken.line)<<":"<<(tokenRange.firstToken.column);
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
    CASE(ERROR_NONE)
    CASE(ERROR_UNSPECIFIED)
    CASE(ERROR_CASTING_TYPES)
    CASE(ERROR_UNDECLARED)
    CASE(ERROR_TYPE_MISMATCH)
    CASE(ERROR_INVALID_TYPE)
    CASE(ERROR_TOO_MANY_VARIABLES)

    CASE(ERROR_INCOMPLETE_FIELD)

    CASE(ERROR_AMBIGUOUS_PARSING)
    
    CASE(ERROR_DUPLICATE_CASE)
    CASE(ERROR_DUPLICATE_DEFAULT_CASE)
    CASE(ERROR_C_STYLED_DEFAULT_CASE)
    CASE(ERROR_BAD_TOKEN_IN_SWITCH)
    CASE(ERROR_MISSING_ENUM_MEMBERS_IN_SWITCH)
    CASE(ERROR_OVERLOAD_MISMATCH)
    #undef CASE
    
    Assert(false);
    
    return ERROR_UNKNOWN;
}
const char* ToCompileErrorString(temp_compile_error stuff) {
    // if(stuff.err == ERROR_NONE)
    //     return "";
    if(!stuff.shortVersion) {
        #define CASE(ERR) if(stuff.err == ERR) return #ERR;
        CASE(ERROR_NONE)
        CASE(ERROR_UNSPECIFIED)
        
        CASE(ERROR_CASTING_TYPES)
        CASE(ERROR_UNDECLARED)
        CASE(ERROR_TYPE_MISMATCH)
        CASE(ERROR_INVALID_TYPE)
        CASE(ERROR_TOO_MANY_VARIABLES)

        CASE(ERROR_INCOMPLETE_FIELD)

        CASE(ERROR_AMBIGUOUS_PARSING)
    
        CASE(ERROR_DUPLICATE_CASE)
        CASE(ERROR_DUPLICATE_DEFAULT_CASE)
        CASE(ERROR_C_STYLED_DEFAULT_CASE)
        CASE(ERROR_BAD_TOKEN_IN_SWITCH)
        CASE(ERROR_MISSING_ENUM_MEMBERS_IN_SWITCH)
        CASE(ERROR_OVERLOAD_MISMATCH)
        
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
void PrintCode(const Token& token, const StringBuilder& stringBuilder, TokenStream** prevStream, int* base_column){
     PrintCode(token.operator TokenRange(), stringBuilder, prevStream, base_column);
}
void PrintCode(const TokenRange& tokenRange, const StringBuilder& stringBuilder, TokenStream** prevStream, int* base_column){
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
    if(!prevStream || tokenRange.tokenStream() != *prevStream) {
    // if(prevStream && tokenRange.tokenStream() != *prevStream) {
        log::out << log::GRAY << "-- "<<TrimDir(tokenRange.tokenStream()->streamName) <<":"<<tokenRange.firstToken.line << " --\n";
        if(prevStream)
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
    const log::Color codeColor = log::NO_COLOR;
    const log::Color markColor = log::CYAN;

    int lineDigits = 0;
    int baseColumn = base_column ? *base_column : -1;
    for(int i=start;i<end;i++){
        Token& tok = tokenRange.tokenStream()->get(i);
        int numlen = tok.line>0 ? ((int)log10(tok.line)+1) : 1;
        if(numlen>lineDigits)
            lineDigits = numlen;
        if(tok.column<baseColumn || baseColumn == -1)
            baseColumn = tok.column;
    }
    if(base_column)
        *base_column = baseColumn;
    const char* const line_sep_str = " | "; // text that separates the line number and the code
    static const int line_sep_len = strlen(line_sep_str);
    int line_number_width = lineDigits + line_sep_len;
    // log::out << start << " - " <<end<<"\n";
    int currentLine = -1;
    int minPos = -1;
    int maxPos = -1;
    int pos = -1;
    bool added_newline = false;
    for(int i=start;i<end;i++){
        Token& tok = tokenRange.tokenStream()->get(i);
        if(tok.line != currentLine){
            currentLine = tok.line;
            if(i!=start) {
                if(!added_newline)
                    log::out << "\n";
                added_newline = false;
            }
            int numlen = currentLine>0 ? ((int)log10(currentLine)+1) : 1;
            for(int j=0;j<lineDigits-numlen;j++)
                log::out << " ";
            log::out << codeColor << currentLine << line_sep_str;
            pos = line_number_width + tok.column-baseColumn;
            if(pos < minPos || minPos == -1)
                minPos = pos;
            for(int j=0;j<tok.column-baseColumn;j++) log::out << " ";
        }
        // log::out.flush();
        if(i<tokenRange.startIndex()){
            pos += tok.calcLength();
            // if(tok.flags&TOKEN_SUFFIX_SPACE)
            //     pos += 1;
            log::out << codeColor;
            if(pos>minPos || minPos == -1)
                minPos = pos;
        } else if(i>=tokenRange.endIndex){
            pos += tok.calcLength();
            // if(tok.flags&TOKEN_SUFFIX_SPACE)
            //     pos += 1;
            // if(minPos>pos || minPos == -1)
            //     minPos = pos;
            log::out << codeColor;
        } else {
            log::out << markColor;
            if(i==tokenRange.startIndex())
                pos = minPos;
            pos += tok.calcLength();
            if((tok.flags&TOKEN_SUFFIX_SPACE) || (tok.flags&TOKEN_SUFFIX_LINE_FEED))
                pos--;
            if(pos>maxPos || maxPos==-1)
                maxPos = pos;
            if((tok.flags&TOKEN_SUFFIX_SPACE) || (tok.flags&TOKEN_SUFFIX_LINE_FEED))
                pos++;
        }
        tok.print(false);
        if(tok.flags&TOKEN_SUFFIX_LINE_FEED)
            added_newline = true;
        // log::out.flush();
    }
    // log::out << "\n";

    // for(int i=0;i<minPos;i++) log::out << " ";
    // log::out << "<";
    // for(int i=0;i<maxPos - minPos;i++) log::out << " ";
    // log::out << ">\n";
    // for(int i=0;i<maxPos-1;i++)
    //     log::out << " ";
    // log::out << ">\n";

    int msglen = stringBuilder.size();
    // log::out << "len "<<msglen<<"\n";
    log::out << markColor;
    if(msglen+1<minPos){
        // print message on left side of mark
        for(int i=0;i<minPos-(msglen+1);i++)
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

void PrintCode(const TokenRange& tokenRange, const char* message, TokenStream** prevStream){
    StringBuilder temp{};
    temp.borrow(message);   
    PrintCode(tokenRange, temp, prevStream);
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

    const char* str_example = "Example ";
    int str_example_len = strlen(str_example);

    const log::Color codeColor = log::NO_COLOR;
    const log::Color markColor = log::CYAN;

    const char* const linestr = " | ";
    static const int linestrlen = strlen(linestr);

    int lastLine = line;
    for(int i=0;i<stringBuilder.size();i++){
        char chr = stringBuilder.data()[i];
        if(chr == '\n') {
            lastLine++;
        }
    }
    int lineDigits = lastLine>0 ? ((int)log10(lastLine)+1) : 1;
    
    log::out << log::LIME << str_example << log::NO_COLOR;

    int currentLine = line;
    for(int i=0;i<stringBuilder.size();i++){
        char chr = stringBuilder.data()[i];
        if(chr == '\n' || i==0) {
            if(chr=='\n')
                log::out << '\n';
            if(i!=0) {
                for(int j=0;j<str_example_len;j++)
                    log::out << " ";
            }
            int numlen = currentLine>0 ? ((int)log10(currentLine)+1) : 1;
            for(int j=0;j<lineDigits-numlen;j++)
                log::out << " ";
            if(chr=='\n') {
                currentLine++;
            }
            log::out << codeColor << currentLine<<linestr;

        }
        if(chr != '\n')
            log::out << chr;
    }
    log::out << "\n";
}

void Reporter::start_report() {
    // TODO: Mutex
    if(!instant_report) {
        engone::log::out.setInput(&stream);
        engone::log::out.enableConsole(false);
    }
}
void Reporter::end_report() {
    if(!instant_report) {
        engone::log::out.setInput(nullptr);
        engone::log::out.enableConsole(true);
    }
}
void Reporter::err_head(lexer::Token token, CompileError errcode){
    using namespace engone;
    log::out << log::RED;
    
    // NOTE: Stream name is an absolute path. If the path contains CWD then skip that part to reduce
    //   the clutter on the screen. The user can probably deduce where the file is even if we drop the CWD part.
    //   Make sure you can still click on the path in your editor/terminal to quick jump to the location. (at least in vscode)
    std::string cwd = engone::GetWorkingDirectory() + "/";
    ReplaceChar((char*)cwd.data(), cwd.length(), '\\','/');
    int index = 0;
    
    u32 cindex,tindex;
    lexer->decode_origin(token.origin, &cindex, &tindex);

    auto chunk = lexer->getChunk_unsafe(cindex);
    lexer::TokenInfo* info=nullptr;
    lexer::TokenSource* src=nullptr;
    if(token.type == lexer::TOKEN_EOF) {
        info = &chunk->tokens[tindex-1];
        src = &chunk->sources[tindex-1];
    } else {
        info = &chunk->tokens[tindex];
        src = &chunk->sources[tindex];
    }
    auto imp = lexer->getImport_unsafe(chunk->import_id);

    prev_import = imp;
    base_column = -1;

    while(imp->path.size() > index && cwd.size() > index) {
        if(imp->path[index] != cwd[index])
            break;
        index++;
    }
    if(index != 0)
        log::out << "./" << imp->path.substr(index);
    else
        log::out << imp->path;
    log::out <<":"<<(src->line)<<":"<<(src->column);

    log::out << " ("<<ToCompileErrorString({true,errcode})<<")";
    log::out << ": " <<  MESSAGE_COLOR;
}

// void Reporter::err_desc(const StringBuilder& text) {

// }
void Reporter::err_mark(lexer::TokenRange range, const StringBuilder& text) {
    using namespace engone;
    
    auto imp = lexer->getImport_unsafe(range.importId);

    int start = range.token_index_start;
    int end = range.token_index_end;
    // If you call PrintCode multiple times in an error message and the file is the same
    // then we don't need to print this.
    // If the lines we print come from different files then we do need to print this because
    // otherwise we will assume the line we print comes from the location the head of error message
    // displayed.
    if(!prev_import || prev_import != imp) {
        log::out << log::GRAY << "-- "<<TrimDir(imp->path) << " --\n";
        // if(prev_import) should we always set prev_import or not?
        prev_import = imp;
    }
    bool is_eof = false;
    auto possible_eof = lexer->getTokenFromImport(imp->file_id, start);
    if(possible_eof.type == lexer::TOKEN_EOF) {
        start--;
        is_eof = true;
    }

    // log::out <<"We " <<start << " "<<end<<"\n";
    while(start>0){
        auto prev = lexer->getTokenFromImport(imp->file_id, start-1);
        if(prev.flags&lexer::TOKEN_FLAG_NEWLINE){
            break;
        }
        start--;
        // log::out << "start "<<start<<"\n";
    }
    // NOTE: end is exclusive
    while(true){
        auto next = lexer->getTokenFromImport(imp->file_id, end-1); // -1 since end is exclusive
        if(next.type == lexer::TOKEN_EOF||(next.flags&lexer::TOKEN_FLAG_NEWLINE)){
            break;
        }
        end++;
        
        // log::out << "end "<<end<<"\n";
    }
    const log::Color codeColor = log::NO_COLOR;
    const log::Color markColor = log::CYAN;

    int lineDigits = 0;
    int baseColumn = base_column; // if you call err_mark without err_head then and old value for base_column will be used
    for(int i=start;i<end;i++){
        lexer::TokenSource* src;
        auto tok = lexer->getTokenInfoFromImport(imp->file_id,i, &src);
        int numlen = src->line>0 ? ((int)log10(src->line)+1) : 1;
        if(numlen>lineDigits)
            lineDigits = numlen;
        if(src->column<baseColumn || baseColumn == -1)
            baseColumn = src->column;
    }
    base_column = baseColumn;
    const char* const line_sep_str = " | "; // text that separates the line number and the code
    static const int line_sep_len = strlen(line_sep_str);
    int line_number_width = lineDigits + line_sep_len;
    // log::out << start << " - " <<end<<"\n";
    int currentLine = -1;
    int minPos = -1;
    int maxPos = -1;
    int pos = -1;
    bool added_newline = false;
    for(int i=start;i<end;i++){
        lexer::TokenSource* src;
        auto tok = lexer->getTokenInfoFromImport(imp->file_id, i, &src);
        // bool is_eof = tok->type == lexer::TOKEN_EOF;
        if(tok->type == lexer::TOKEN_EOF) {
            minPos+=1; // +1 for the space before <EOF>
            // This code is copied from the mark code below (with a few tweaks)
            log::out << markColor;
            if(i==start)
                pos = minPos;
            
            pos += 5 + 1;
            log::out << " <EOF>\n";
            added_newline = true;

            if(tok->flags&(lexer::TOKEN_FLAG_ANY_SUFFIX))
                pos--;
            if(pos>maxPos || maxPos==-1)
                maxPos = pos;
            if((tok->flags&lexer::TOKEN_FLAG_ANY_SUFFIX))
                pos++;
            break;
        }
        auto tok_tiny = lexer->getTokenFromImport(imp->file_id, i);
        if(src->line != currentLine){
            currentLine = src->line;
            if(i!=start) {
                if(!added_newline)
                    log::out << "\n";
                added_newline = false;
            }
            int numlen = currentLine>0 ? ((int)log10(currentLine)+1) : 1;
            for(int j=0;j<lineDigits-numlen;j++)
                log::out << " ";
            log::out << codeColor << currentLine << line_sep_str;
            pos = line_number_width + src->column-baseColumn;
            if(pos < minPos || minPos == -1)
                minPos = pos;
            for(int j=0;j<src->column-baseColumn;j++) log::out << " ";
        }
        // log::out.flush();
        char buffer[256];
        auto iter = lexer->createFeedIterator(tok_tiny);
        int written=0;
        if(i<range.token_index_start){
            // pos += tok.calcLength();
            log::out << codeColor;
            bool skip_newline = false;
            if(possible_eof.type == lexer::TOKEN_EOF)
                skip_newline = true;
            while((written = lexer->feed(buffer,sizeof(buffer),iter,skip_newline))){
                pos += written;
                log::out.print(buffer,written);
            }
            
            // if(tok.flags&TOKEN_SUFFIX_SPACE)
            //     pos += 1;
            if(pos>minPos || minPos == -1)
                minPos = pos;
        } else if(i>=range.token_index_end){
            // pos += tok.calcLength();
            log::out << codeColor;
            while((written = lexer->feed(buffer,sizeof(buffer),iter))){
                pos += written;
                log::out.print(buffer,written);
            }
            // if(tok.flags&TOKEN_SUFFIX_SPACE)
            //     pos += 1;
            // if(minPos>pos || minPos == -1)
            //     minPos = pos;
        } else {
            log::out << markColor;
            if(i==start)
                pos = minPos;
            // pos += tok.calcLength();
            while((written = lexer->feed(buffer,sizeof(buffer),iter))){
                pos += written;
                log::out.print(buffer,written);
            }
            if(tok->flags&(lexer::TOKEN_FLAG_ANY_SUFFIX))
                pos--;
            if(pos>maxPos || maxPos==-1)
                maxPos = pos;
            if((tok->flags&lexer::TOKEN_FLAG_ANY_SUFFIX))
                pos++;
        }
        // tok.print(false);
        if(tok->flags&lexer::TOKEN_FLAG_NEWLINE)
            added_newline = true;
        // log::out.flush();
    }
    // log::out << "\n";

    // for(int i=0;i<minPos;i++) log::out << " ";
    // log::out << "<";
    // for(int i=0;i<maxPos - minPos;i++) log::out << " ";
    // log::out << ">\n";
    // for(int i=0;i<maxPos-1;i++)
    //     log::out << " ";
    // log::out << ">\n";

    int msglen = text.size();
    // log::out << "len "<<msglen<<"\n";
    log::out << markColor;
    if(msglen+1<minPos){
        // print message on left side of mark
        for(int i=0;i<minPos-(msglen+1);i++)
            log::out << " ";
        if(text.size()!=0)
            log::out << text<<" ";
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
        if(text.size()!=0)
            log::out << " " << text;
    }
    log::out << "\n";
}
void Reporter::err_mark(lexer::Token token, const StringBuilder& text) {
    u32 cindex,tindex;
    lexer->decode_origin(token.origin, &cindex, &tindex);

    auto chunk = lexer->getChunk_unsafe(cindex);
    auto imp = lexer->getImport_unsafe(chunk->import_id);
    
    lexer::TokenRange r{};
    r.importId = chunk->import_id;
    
    for(int i=0;i<imp->chunk_indices.size();i++) {
        u32 ind = imp->chunk_indices[i];
        if(ind == cindex) {
            r.token_index_start = i * TOKEN_ORIGIN_TOKEN_MAX + tindex;
            break;
        }
    }
    r.token_index_end = r.token_index_start + 1;
    err_mark(r, text);
}
